: # -*- perl -*-
eval 'exec perl -w -S $0 "$@"'
if 0;

use strict;
use Data::Dumper;

if (@ARGV != 3) {
  print STDERR "usage: $0 input output.md output.rnc\n";
  exit 2;
}

my $input = $ARGV[0];
my $output_md = $ARGV[1];
my $output_rnc = $ARGV[2];

# This "perl" script extracts the configuration elements and their types and descriptions
# from the source and generates a RELAX NG Compact Form (RNC) and a MarkDown version of
# it.  The scare quotes are necessary because it really is just a translation to perl
# syntax of an old gawk script, originally used for generating input to a half-baked
# Java-based configuration editor.
#
# There are tools out there to convert RNC to, e.g., XSD.  RNC has the advantage of being
# understood by Emacs' nXML mode, but more importantly, of being fairly straightforward to
# generate.
#
# In an ideal world it would be a bit less fragile in its parsing of the input, and
# besides one should generate the C code for the configuration tables from a sensible
# source format, rather than try to extract it from the C source.
#
# Other issues:
# - knowledge of conversion functions in here
# - hard definitions of enums in here
# - some other hard-coded knowledge of the top level nodes
#
# INCANTATION
#
# trang is a tool for converting (among other things) RNC to XSD
#
# perl -w ../docs/makernc.pl ../src/core/ddsi/src/q_config.c ../docs/manual/options.md ../etc/cyclonedds.rnc \
#   && java -jar trang-20091111/trang.jar -I rnc -O xsd ../etc/cyclonedds.rnc ../etc/cyclonedds.xsd
$|=1;
my $debug = 0;

my %typehint2xmltype =
  ("____" => "____",
   "nop" => "____",
   "networkAddress" => "String",
   "partitionAddress" => "String",
   "networkAddresses" => "String",
   "ipv4" => "String",
   "boolean" => "Boolean",
   "boolean_default" => "Enum",
   "string" => "String",
   "tracingOutputFileName" => "String",
   "verbosity" => "Enum",
   "tracemask" => "Comma",
   "peer" => "String",
   "float" => "Float",
   "int" => "Int",
   "int32" => "Int",
   "uint" => "Int",
   "uint32" => "Int",
   "natint" => "Int",
   "natint_255" => "Int",
   "domainId" => "String",
   "participantIndex" => "String",
   "port" => "Int",
   "dyn_port" => "Int",
   "duration_inf" => "String",
   "duration_ms_1hr" => "String",
   "duration_ms_1s" => "String",
   "duration_100ms_1hr" => "String",
   "duration_us_1s" => "String",
   "memsize" => "String",
   "bandwidth" => "String",
   "standards_conformance" => "Enum",
   "locators" => "Enum",
   "service_name" => "String",
   "sched_class" => "Enum",
   "cipher" => "Enum",
   "besmode" => "Enum",
   "retransmit_merging" => "Enum",
   "sched_prio_class" => "Enum",
   "sched_class" => "Enum",
   "maybe_int32" => "String",
   "maybe_memsize" => "String",
   "maybe_duration_inf" => "String",
   "allow_multicast" => "Comma",
   "transport_selector" => "Enum",
   "many_sockets_mode" => "Enum",
   "xcheck" => "Comma",
   "min_tls_version" => "String");

my %typehint2unit =
  ("duration_inf" => "duration_inf",
   "duration_ms_1hr" => "duration",
   "duration_100ms_1hr" => "duration",
   "duration_ms_1s" => "duration",
   "duration_us_1s" => "duration",
   "bandwidth" => "bandwidth",
   "memsize" => "memsize",
   "maybe_memsize" => "memsize",
   "maybe_duration_inf" => "duration_inf");

my %enum_values =
  ("locators" => "local;none",
   "standards_conformance" => "lax;strict;pedantic",
   "verbosity" => "finest;finer;fine;config;info;warning;severe;none",
   "besmode" => "full;writers;minimal",
   "retransmit_merging" => "never;adaptive;always",
   "sched_prio_class" => "relative;absolute",
   "sched_class" => "realtime;timeshare;default",
   "cipher" => "null;blowfish;aes128;aes192;aes256",
   "boolean_default" => "false;true;default",
   "many_sockets_mode" => "false;true;single;none;many",
   "transport_selector" => "default;udp;udp6;tcp;tcp6;raweth");

# should extrace these from the source ...
my %comma_values =
  ("tracemask" => "|fatal;error;warning;info;config;discovery;data;radmin;timing;traffic;topic;tcp;plist;whc;throttle;rhc;content;trace",
   "allow_multicast" => "default|false;spdp;asm;ssm;true",
   "xcheck" => "|whc;rhc;all");

my %range =
  ("port" => "1;65535",
   "dyn_port" => "-1;65535",
   "general_cfgelems/startupmodeduration" => "0;60000",
   "natint_255" => "0;255",
   "duration_ms_1hr" => "0;1hr",
   "duration_100ms_1hr" => "100ms;1hr",
   "duration_ms_1s" => "0;1s",
   "duration_us_1s" => "0;1s");

my %unit_blurb =
  ("bandwidth" => "\n<p>The unit must be specified explicitly. Recognised units: <i>X</i>b/s, <i>X</i>bps for bits/s or <i>X</i>B/s, <i>X</i>Bps for bytes/s; where <i>X</i> is an optional prefix: k for 10<sup>3</sup>, Ki for 2<sup>10</sup>, M for 10<sup>6</sup>, Mi for 2<sup>20</sup>, G for 10<sup>9</sup>, Gi for 2<sup>30</sup>.</p>",
   "memsize" => "\n<p>The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2<sup>10</sup> bytes), MB & MiB (2<sup>20</sup> bytes), GB & GiB (2<sup>30</sup> bytes).</p>",
   "duration" => "\n<p>The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.</p>",
   "duration_inf" => "\n<p>Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.</p>");

my %unit_patterns =
  ("memsize" => '0|(\d+(\.\d*)?([Ee][\-+]?\d+)?|\.\d+([Ee][\-+]?\d+)?) *([kMG]i?)?B',
   "bandwidth" => '0|(\d+(\.\d*)?([Ee][\-+]?\d+)?|\.\d+([Ee][\-+]?\d+)?) *([kMG]i?)?[Bb][p/]s',
   "duration" => '0|(\d+(\.\d*)?([Ee][\-+]?\d+)?|\.\d+([Ee][\-+]?\d+)?) *([num]?s|min|hr|day)',
   "duration_inf" => 'inf|0|(\d+(\.\d*)?([Ee][\-+]?\d+)?|\.\d+([Ee][\-+]?\d+)?) *([num]?s|min|hr|day)');

while (my ($k, $v) = each %typehint2xmltype) {
  die "script error: values of enum type $k unknown\n" if $v eq "Enum" && $enum_values{$k} eq "";
}

my %elem;
my %typehint_seen;

my %tables;

my @root = read_config ($input);

{
  open my $fh, ">:unix", "$output_rnc" or die "can't open $output_rnc";
  print $fh "default namespace = \"https://cdds.io/config\"\n";
  print $fh "namespace a = \"http://relaxng.org/ns/compatibility/annotations/1.0\"\n";
  print $fh "grammar {\n";
  print $fh "  start =\n";
  my $isfirst = 1;
  conv_table($fh, \&conv_to_rnc, \@root, "/", "  ", "", \$isfirst);
  for (sort keys %unit_patterns) {
    printf $fh "  %s = xsd:token { pattern = \"%s\" }\n", $_, $unit_patterns{$_};
  }
  print $fh "}\n";
  close $fh;
}

{
  open my $fh, ">:unix", "$output_md" or die "can't open $output_md";
  my $sep_blurb = "";
  conv_table($fh, \&conv_to_md, \@root, "/", "  ", "", \$sep_blurb);
  close $fh;
}

exit 0;

sub clean_description {
  my ($desc) = @_;
  $desc =~ s/^\s*BLURB\s*\(\s*//s;
  $desc =~ s/^\s*"//s;
  $desc =~ s/\s*"(\s*\))? *(\}\s*,\s*$)?$//s;
  $desc =~ s/\\"/"/g;
  $desc =~ s/\\n\s*\\/\n/g;
  $desc =~ s/\\\\/\\/g;
  $desc =~ s/\n\n/\n/g;
  # should fix the source ...
  $desc =~ s/DDSI2E?/Cyclone DDS/g;
  return $desc;
}

sub html_to_md {
  my ($desc) = @_;
  $desc =~ s/^\<\/p\>//gs;
  $desc =~ s/\<\/p\>//gs;
  $desc =~ s/<sup>/^/s;
  $desc =~ s/<\/sup>//s;
  $desc =~ s/\<p\>/\n\n/gs;
  $desc =~ s/\<\/?(i|ul)\>//gs;
  $desc =~ s/\<li\>(.*?)\<\/li\>\n*/* $1\n/gs;
  $desc =~ s/&quot;/"/gs;
  $desc =~ s/\n+/\n/gs;
  $desc =~ s/^\n*//s;
  return $desc;
}

sub kind_to_kstr {
  my ($kind, $typehint, $table, $name) = @_;
  if ($kind eq "GROUP" || $kind eq "MGROUP") {
    return "element";
  } elsif ($kind eq "ATTR") {
    return "$typehint2xmltype{$typehint}";
  } elsif ($kind eq "LEAF") {
    return "$typehint2xmltype{$typehint}";
  } else {
    die "error: $kind unrecognized kind ($table/$name)\n";
  }
}

sub store_entry {
  my ($name, $table, $kind, $subtables, $multiplicity, $defaultvalue, $typehint, $description) = @_;
  $name =~ s/\|.*//; # aliases are not visible in osplconf
  my $ltable = lc $table;
  my $lname = lc $name;
  die "error: no mapping defined for type $typehint\n" if $typehint2xmltype{$typehint} eq "";
  my $ub = exists $typehint2unit{$typehint} && exists $unit_blurb{$typehint2unit{$typehint}} ? $unit_blurb{$typehint2unit{$typehint}} : "";
  if ($kind eq "GROUP" || $kind eq "MGROUP") {
    # GROUP and MGROUP have no data, so also no default value
    $defaultvalue = undef;
  } elsif ($defaultvalue eq "" || $defaultvalue eq "NULL") {
    $defaultvalue = undef;
  } else {
    $defaultvalue =~ s/^"(.*)"$/$1/;
  }

  my ($min_occ, $max_occ);
  if ($multiplicity =~ /MAX/) {
    $min_occ = $max_occ = 0;
  } elsif ($multiplicity == 0 || $multiplicity == 1) {
    # multiplicity = 0 => special case, treat as-if 1
    # multiplicity = 1 => required if no default
    if ($kind eq "GROUP" || $kind eq "MGROUP") {
      $min_occ = 0;
    } elsif (not defined $defaultvalue) {
      $min_occ = 1;
    } else {
      $min_occ = 0;
    }
    $max_occ = 1;
  } else {
    $min_occ = 0; $max_occ = $multiplicity;
  }

  my $kstr = kind_to_kstr($kind, $typehint, $table, $name);

  my $desc = clean_description($description).$ub;
  $desc .= "<p>The default value is: &quot;$defaultvalue&quot;.</p>" if defined $defaultvalue;
  my $fs;
  push @{$tables{$table}}, { table => $table, name => $name,
                             kind => $kind, kstr => $kstr,
                             subtables => $subtables, multiplicity => $multiplicity,
                             min_occ => $min_occ, max_occ => $max_occ, root => 0,
                             defaultvalue => $defaultvalue, typehint => $typehint,
                             description => $desc };
  # typehint_seen is for verifying no bogus type hints are defined in this script
  $typehint_seen{$typehint} = 1;
}

sub fmtblurb {
  my ($blurb) = @_;
  my $isbullet = ($blurb =~ s/^\* //);
  my $maxlen = $isbullet ? 78 : 74;
  my @words = split ' ', $blurb;
  my @lines = ();
  while (@words > 0) {
    my $x = shift @words;
    while (@words > 0 && (length $x) + 1 + (length $words[0]) < $maxlen) {
      $x .= " " . shift @words;
    }
    push @lines, "$x";
  }
  my $sep = "\n" . ($isbullet ? "  " : "");
  return ($isbullet ? "* " : "") . join $sep, @lines;
}

sub print_description_rnc {
  my ($fh, $desc, $indent) = @_;
  my $x = $desc;
  my @xs = split /\n+/, $x;
  $_ = fmtblurb ($_) for @xs;
  $x = join "\n\n", @xs;
  return 0 if $x =~ /^\s$/s;
  print $fh "[ a:documentation [ xml:lang=\"en\" \"\"\"\n$x\"\"\" ] ]\n";
  return 1;
}

sub print_description_md {
  my ($fh, $desc, $indent) = @_;
  my $x = html_to_md ($desc);
  my @xs = split /\n+/, $x;
  $_ = fmtblurb ($_) for @xs;
  $x = join "\n\n", @xs;
  return 0 if $x =~ /^\s$/s;
  print $fh "$x\n";
  return 1;
}

sub conv_to_rnc {
  my ($fh, $fs, $name, $fqname, $indent, $prefix, $isfirstref) = @_;

  printf $fh "${indent}%s", ($$isfirstref ? "" : "& ");
  print_description_rnc ($fh, $fs->{description}, $indent);
  printf $fh "${indent}%s %s {\n", ($fs->{kind} eq "ATTR" ? "attribute" : "element"), $name;

  my $sub_isfirst = 1;
  conv_table($fh, \&conv_to_rnc, $fs->{subtables}, $fqname, "${indent}  ", $prefix, \$sub_isfirst);
  my $sep = $sub_isfirst ? "" : "& ";

  if ($fs->{kind} eq "GROUP" || $fs->{kind} eq "MGROUP") {
    printf $fh "${indent}  ${sep}empty\n" if $sub_isfirst;
  } elsif ($fs->{kstr} eq "Boolean") {
    printf $fh "${indent}  ${sep}xsd:boolean\n";
  } elsif ($fs->{kstr} eq "Comma") {
    die unless exists $comma_values{$fs->{typehint}};
    my $pat = "";
    my @xs = split /\|/, $comma_values{$fs->{typehint}};
    my $allowempty = 0;
    for (@xs) {
      if ($_ eq "") { $allowempty = 1; next; }
      (my $vs = $_) =~ s/;/|/g;
      $pat .= "|" unless $pat eq "";
      if ($vs =~ /\|/) {
        $pat .= "(($vs)(,($vs))*)";
      } else {
        $pat .= $vs;
      }
    }
    $pat .= "|" if $allowempty;
    printf $fh "${indent}  ${sep}xsd:token { pattern = \"%s\" }\n", $pat;
  } elsif ($fs->{kstr} eq "Enum") {
    die unless exists $enum_values{$fs->{typehint}};
    my @vs = split /;/, $enum_values{$fs->{typehint}};
    printf $fh "${indent}  ${sep}%s\n", (join '|', map { "\"$_\"" } @vs);
  } elsif ($fs->{kstr} eq "Int") {
    printf $fh "${indent}  ${sep}xsd:integer\n";
    #if (exists $range{$lctn} || exists $range{$fs->{typehint}}) {
    #  # integer with range
    #  my $rr = exists $range{$lctn} ? $range{$lctn} : $range{$fs->{typehint}};
    #  my @vs = split /;/, $range{$lctn};
    #}
  } elsif ($typehint2unit{$fs->{typehint}}) {
    # number with unit
    printf $fh "${indent}  ${sep}$typehint2unit{$fs->{typehint}}\n";
  } elsif ($typehint2xmltype{$fs->{typehint}} =~ /String$/) {
    printf $fh "${indent}  ${sep}text\n";
  } else {
    die;
  }

  my $suffix;
  if ($fs->{min_occ} == 0) {
    $suffix = ($fs->{max_occ} == 1) ? "?" : "*";
  } else {
    $suffix = ($fs->{max_occ} == 1) ? "" : "+";
  }
  printf $fh "${indent}}%s\n", $suffix;
  $$isfirstref = 0;
}

sub list_children_md {
  my ($fh, $fs, $name, $fqname, $indent, $prefix, $children) = @_;
  if ($fs->{kind} eq "ATTR") {
    push @{$children->{attributes}}, $name;
  } else {
    push @{$children->{elements}}, $name;
  }
}

sub conv_to_md {
  my ($fh, $fs, $name, $fqname, $indent, $prefix, $separator_blurb_ref) = @_;

  print $fh $$separator_blurb_ref;
  $$separator_blurb_ref = "\n\n";

  # Print fully-qualified element/attribute name as a heading, with the heading level
  # determined by the nesting level.  The nesting level can be computed from the number of
  # slashes :)
  (my $slashes = $fqname) =~ s/[^\/]//g;
  die unless length $slashes >= 2;
  my $level = (length $slashes) - 1;
  printf $fh "%s $fqname\n", ("#"x$level);

  # Describe type (boolean, integer, &c.); for a group list its attributes and children as
  # links to their descriptions
  {
    my %children = ("attributes" => [], "elements" => []);
    conv_table($fh, \&list_children_md, $fs->{subtables}, "", "${indent}  ", $prefix, \%children);
    if (@{$children{attributes}} > 0) {
      my @xs = sort @{$children{attributes}};
      my @ys = map { my $lt = lc "$fqname\[\@$_]"; $lt =~ s/[^a-z0-9]//g; "[$_](#$lt)" } @xs;
      printf $fh "Attributes: %s\n\n", (join ', ', @ys);
    }
    if (@{$children{elements}} > 0) {
      my @xs = sort @{$children{elements}};
      my @ys = map { my $lt = lc "$fqname\[\@$_]"; $lt =~ s/[^a-z0-9]//g; "[$_](#$lt)" } @xs;
      printf $fh "Children: %s\n\n", (join ', ', @ys);
    }
  }

  if ($fs->{kind} eq "GROUP" || $fs->{kind} eq "MGROUP") {
    # nothing to see here
  } elsif ($fs->{kstr} eq "Boolean") {
    printf $fh "Boolean\n";
  } elsif ($fs->{kstr} eq "Comma") {
    die unless exists $comma_values{$fs->{typehint}};
    my $pat = "";
    my @xs = split /\|/, $comma_values{$fs->{typehint}};
    if (@xs > 1) {
      printf $fh "One of:\n";
    }
    my $allowempty = 0;
    for (@xs) {
      if ($_ eq "") { $allowempty = 1; next; }
      my @vs = split /;/, $_;
      if (@vs > 1) {
        printf $fh "* Comma-separated list of: %s\n", (join ', ', @vs);
      } else {
        printf $fh "* Keyword: %s\n", $vs[0];
      }
    }
    printf $fh "* Or empty\n" if $allowempty;
  } elsif ($fs->{kstr} eq "Enum") {
    die unless exists $enum_values{$fs->{typehint}};
    my @vs = split /;/, $enum_values{$fs->{typehint}};
    printf $fh "One of: %s\n", (join ', ', @vs);
  } elsif ($fs->{kstr} eq "Int") {
    printf $fh "Integer\n";
    #if (exists $range{$lctn} || exists $range{$fs->{typehint}}) {
    #  # integer with range
    #  my $rr = exists $range{$lctn} ? $range{$lctn} : $range{$fs->{typehint}};
    #  my @vs = split /;/, $range{$lctn};
    #}
  } elsif ($typehint2unit{$fs->{typehint}}) {
    # number with unit
    printf $fh "Number-with-unit\n";
  } elsif ($typehint2xmltype{$fs->{typehint}} =~ /String$/) {
    printf $fh "Text\n";
  } else {
    die;
  }

  # Descriptive text
  printf $fh "\n";
  print_description_md ($fh, $fs->{description}, $indent);

  # Generate attributes & children
  conv_table($fh, \&conv_to_md, $fs->{subtables}, $fqname, "${indent}  ", $prefix, $separator_blurb_ref);
}

sub conv_table {
  my ($fh, $convsub, $tablenames, $fqname, $indent, $prefix, $closure) = @_;
  my $elems = 0;
  for (@$tablenames) {
    next unless exists $tables{$_};
    for my $fs (sort { $a->{name} cmp $b->{name} } @{$tables{$_}}) {
      my $fqname1;
      if ($fs->{kind} eq "ATTR") {
        die unless $elems == 0;
        $fqname1 = "${fqname}[\@$fs->{name}]";
      } else {
        $fqname1 = "$fqname/$fs->{name}";
        $elems++;
      }
      my $prefix1 = ($fs->{table} eq "unsupp_cfgelems") ? "<b>Internal</b>" : $prefix;
      &$convsub ($fh, $fs, $fs->{name}, $fqname1, $indent, $prefix1, $closure);
    }
  }
}

sub read_config {
  my %incl = (# included options
              'DDSI_INCLUDE_SSL' => 1,
              'DDSI_INCLUDE_NETWORK_PARTITIONS' => 1,
              'DDSI_INCLUDE_SSM' => 1,
              'DDSI_INCLUDE_SECURITY' => 1,
              # excluded options
              'DDSI_INCLUDE_NETWORK_CHANNELS' => 0,
              'DDSI_INCLUDE_BANDWIDTH_LIMITING' => 0);
  my ($input) = @_;
  my ($name, $table, $kind, @subtables, $multiplicity, $defaultvalue, $typehint, $description);
  my ($gobbling_description, $in_table, $rest, $deprecated);
  my @stk = (); # stack of conditional nesting, for each: copy/discard/ignore
  open FH, "<", $input or die "can't open $input\n";
  while (<FH>) {
    s/[\r\n]+$//s;

    # ignore parts guarded by #if/#ifdef/#if!/#ifndef if $incl says so
    if (/^\s*\#\s*if(n?def|\s*!)?\s*([A-Za-z_][A-Za-z_0-9]*)\s*(?:\/(?:\/.*|\*.*?\*\/)\s*)?$/) {
      my $x = (not defined $1 || $1 eq "def") ? -1 : 1; my $var = $2;
      die if $var =~ /^DDSI_INCLUDE_/ && !exists $incl{$var};
      push @stk, (not defined $incl{$var}) ? 0 : $incl{$var} ? $x : -$x;
    } elsif (/^\s*\#\s*if/) { # ignore any other conditional
      push @stk, 0;
    } elsif (/^\s*\#\s*else/) {
      $stk[$#stk] = -$stk[$#stk];
    } elsif (/^\s*\#\s*endif/) {
      pop @stk;
    }
    next if grep {$_ < 0} @stk;

    if ($gobbling_description) {
      $description .= $_;
      #print "  .. $_\n";
    }

    if ($gobbling_description && /(^|")(\s*\)) *\} *, *$/) {
      $gobbling_description = 0;
      my @st = @subtables;
      store_entry ($name, $table, $kind, \@st, $multiplicity, $defaultvalue, $typehint, $description)
        unless $deprecated;
      next;
    }

    if ($gobbling_description) {
      next;
    }

    if (/^[ \t]*(#[ \t]*(if|ifdef|ifndef|else|endif).*)?$/) { # skip empty lines, preproc
      next;
    }

    if (/^ *END_MARKER *$/) {
      if (!$in_table) {
        warn "END_MARKER seen while not in a table";
      }
      $in_table = 0;
      print "END_MARKER $table\n" if $debug;
      next;
    }

    if (/^static +const +struct +cfgelem +([A-Za-z_0-9]+)\s*\[/) {
      $in_table = 1;
      $table = $1;
      print "TABLE $table\n" if $debug;
      next;
    }

    if ($in_table && /^ *WILDCARD *, *$|^ *\{ *(MOVED) *\(/) {
      next;
    }

    # Recognise all "normal" entries: attributes, groups, leaves and
    # leaves with attributes. This doesn't recognise the ones used for the
    # root groups: those are dealt with by the next pattern
    if ($in_table && /^ *\{ *((?:DEPRECATED_)?(?:ATTR|GROUP|GROUP_W_ATTRS|MGROUP|LEAF|LEAF_W_ATTRS)) *\(/) {
      $rest = $_;
      # extract kind
      $rest =~ s/^ *\{ *((?:DEPRECATED_)?(?:ATTR|GROUP|GROUP_W_ATTRS|MGROUP|LEAF|LEAF_W_ATTRS)) *\( *(.*)/$2/;
      $kind = $1;
      $deprecated = ($kind =~ s/^DEPRECATED_//);
      # extract name + reference to subtables
      $rest =~ s/\"([A-Za-z_0-9|]+)\" *(.*)/$2/;
      $name = $1;
      my ($subelems, $subattrs) = ("", "");
      if ($kind eq "GROUP" || $kind eq "GROUP_W_ATTRS" || $kind eq "MGROUP") {
        $rest =~ s/, *([A-Za-z_0-9]+) *(.*)/$2/;
        $subelems = $1;
      }
      if ($kind eq "LEAF_W_ATTRS" || $kind eq "GROUP_W_ATTRS" || $kind eq "MGROUP") {
        $rest =~ s/, *([A-Za-z_0-9]+) *(.*)/$2/;
        $subattrs = $1;
      }
      @subtables = ();
      push @subtables, $subattrs if $subattrs ne "";
      push @subtables, $subelems if $subelems ne "";
      $rest =~ s/ *\) *, *//;
      print "  kind $kind name $name subtables @subtables -- $rest\n" if $debug;

      # don't care about the distinction between GROUP/LEAF and
      # GROUP/LEAF_W_ATTRS in the remainer of the code: we simply
      # rely on subtables.
      $kind =~ s/_W_ATTRS//;
    }

    # Root groups: use a special trick, which allows them to do groups
    # with attributes. Which the DDSI2 proper doesn't use, but which the
    # service configuration stuff does rely on.
    if ($in_table && /^ *\{ *"([A-Za-z_0-9|]+)" *, */) {
      $rest = $_;
      # root elements are all groups, formatted as: <name>, <subelems>,
      # <attrs>, NODATA, description. They're therefore pretty easy to
      # parse.
      $kind = "GROUP";
      $rest =~ s/^ *\{ *\"([A-Za-z_0-9|]+)\" *, *(.*)/$2/;
      $name = $1;
      # then follow the sub-elements and the attributes
      $rest =~ s/([A-Za-z_0-9]+) *, *(.*)/$2/;
      my $subelems = $1;
      $rest =~ s/([A-Za-z_0-9]+) *, *(.*)/$2/;
      my $subattrs = $1;
      # then we require NODATA (could do this in the pattern also)
      die "error: NODATA expected" unless $rest =~ /^NODATA *,/;
      # multiplicity is hard coded: we want to allow multiple ddsi2 services
      $multiplicity = 0;
      @subtables = ();
      push @subtables, $subattrs if $subattrs ne "";
      push @subtables, $subelems if $subelems ne "";
      $rest =~ s/([A-Za-z_0-9]+) *, *(.*)/$2/;
    }

    # Extract stuff specific to ATTRs, LEAFs and MGROUPs
    if ($in_table && ($kind eq "ATTR" || $kind eq "LEAF" || $kind eq "MGROUP")) {
      # extract multiplicity
      $rest =~ s/([0-9]+|U?INT(?:16|32|64)?_MAX) *, *(.*)/$2/;
      $multiplicity = $1;
      # extract default value
      $rest =~ s/(\"(?:[^\"]*)\"|NULL|0) *, *(.*)/$2/;
      $defaultvalue = $1;
      if ($defaultvalue eq "0") {
        $defaultvalue = "NULL";
      }
      # skip reference to internal name (either ABSOFF(field),
      # RELOFF(field,field) or <int>,<int> (the latter being used by
      # "verbosity")
      $rest =~ s/(ABSOFF *\( *[A-Za-z_0-9.]+ *\)|RELOFF *\( *[A-Za-z_0-9.]+ *, *[A-Za-z_0-9. ]+\)|[0-9]+ *, *[0-9]+) *, *//;
      # skip init function
      $rest =~ s/([A-Za-z_0-9]+|0) *, *//;
      # type hint from conversion function
      $rest =~ s/(uf_(?:[A-Za-z_0-9]+)|NULL|0) *, *(.*)/$2/;
      $typehint = $1;
      $typehint =~ s/^uf_//;
      # accept typehint = NULL for a LEAF_WITH_ATTRS: there is no defined
      # "syntax" for groups that have only attributes, pretending it is a
      # group because that causes us to emit an "element" and not a
      # "leaf".
      if ($typehint eq "0" || $typehint eq "NULL") {
        $kind = "GROUP";
        $typehint = "____";
      }
      # skip free, print functions
      $rest =~ s/([A-Za-z_0-9]+|0) *, *([A-Za-z_0-9]+|0) *, *//;
      #print "  .. multiplicity $multiplicity default $defaultvalue typehint $typehint\n";
    }

    # Extract description (or NULL, if not to be included in the configurator XML)
    if ($in_table) {
      #print "  .. $rest\n";
      # description or NULL
      if ($rest =~ /NULL *\} *, *$/) {
        # no description - discard this one/simply continue with next one
      } elsif ($rest =~ /(?:BLURB\s*\(\s*)?(".*")(?:\s*\))? *\} *, *$/) {
        # description ending on same line
        $description = $1;
        my @st = @subtables;
        store_entry ($name, $table, $kind, \@st, $multiplicity, $defaultvalue, $typehint, $description)
          unless $deprecated;
      } else {
        # strip the quotes &c. once the full text has been gathered
        $description = $rest;
        $gobbling_description = 1;
      }
      #print "  .. gobbling $gobbling_description";
      next;
    }
  }
  close FH;

  my @roots = @{$tables{cyclonedds_root_cfgelems}};
  die "error: cyclonedds_root_cfgelems has no or multiple entries\n" if @roots != 1;
  die "error: root_cfgelems doesn't exist\n" unless exists $tables{root_cfgelems};
  my $root = $roots[0];
  die "error: root_cfgelems doesn't exist\n" unless defined $root;
  $root->{min_occ} = $root->{max_occ} = $root->{isroot} = 1;
  while (my ($k, $v) = each %typehint_seen) {
    warn "script warning: type mapping defined for $k but not used" if $v == 0;
  }
  return ("cyclonedds_root_cfgelems");
}
