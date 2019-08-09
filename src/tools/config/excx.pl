: # -*- perl -*-
eval 'exec perl -w -S $0 "$@"'
if 0;

use strict;

# NOTES:
# - very fragile - and very sensitive to input formatting
# - default value may not contain a semicolon
#
# UGLINESSES:
# - knowledge of conversion functions in here
# - hard definitions of enums in here
# - negated_boolean is A BIT WEIRD and special-cased
# - some other hard-coded knowledge of the top level nodes
# - some hard-coded overrides for defaults
$|=1;

my %typehint2xmltype = ("____" => "____",
                        "networkAddress" => "String",
                        "partitionAddress" => "String",
                        "networkAddresses" => "String",
                        "ipv4" => "String",
                        "boolean" => "Boolean",
                        "negated_boolean" => "Boolean",
                        "boolean_default" => "Enum",
                        "string" => "String",
                        "tracingOutputFileName" => "String",
                        "verbosity" => "Enum",
                        "tracemask" => "String",
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
                        "allow_multicast" => "String",
                        "transport_selector" => "String",
                        "many_sockets_mode" => "Enum",
                        "xcheck" => "String",
                        "min_tls_version" => "String");

my %typehint2unit = ("duration_inf" => "duration_inf",
                     "duration_ms_1hr" => "duration",
                     "duration_100ms_1hr" => "duration",
                     "duration_ms_1s" => "duration",
                     "duration_us_1s" => "duration",
                     "bandwidth" => "bandwidth",
                     "memsize" => "memsize",
                     "maybe_memsize" => "memsize",
                     "maybe_duration_inf" => "duration_inf");

my %enum_values = ("locators" => "local;none",
                   "standards_conformance" => "lax;strict;pedantic",
                   "verbosity" => "finest;finer;fine;config;info;warning;severe;none",
                   "besmode" => "full;writers;minimal",
                   "retransmit_merging" => "never;adaptive;always",
                   "sched_prio_class" => "relative;absolute",
                   "sched_class" => "realtime;timeshare;default",
                   "cipher" => "null;blowfish;aes128;aes192;aes256",
                   "boolean_default" => "false;true;default",
                   "many_sockets_mode" => "false;true;single;none;many");

my %range = ("port" => "1;65535",
             "dyn_port" => "-1;65535",
             "general_cfgelems/startupmodeduration" => "0;60000",
             "natint_255" => "0;255",
             "duration_ms_1hr" => "0;1hr",
             "duration_100ms_1hr" => "100ms;1hr",
             "duration_ms_1s" => "0;1s",
             "duration_us_1s" => "0;1s");

my %unit_blurb = ("bandwidth" => "\n<p>The unit must be specified explicitly. Recognised units: <i>X</i>b/s, <i>X</i>bps for bits/s or <i>X</i>B/s, <i>X</i>Bps for bytes/s; where <i>X</i> is an optional prefix: k for 10<sup>3</sup>, Ki for 2<sup>10</sup>, M for 10<sup>6</sup>, Mi for 2<sup>20</sup>, G for 10<sup>9</sup>, Gi for 2<sup>30</sup>.</p>",
                  "memsize" => "\n<p>The unit must be specified explicitly. Recognised units: B (bytes), kB & KiB (2<sup>10</sup> bytes), MB & MiB (2<sup>20</sup> bytes), GB & GiB (2<sup>30</sup> bytes).</p>",
                  "duration" => "\n<p>The unit must be specified explicitly. Recognised units: ns, us, ms, s, min, hr, day.</p>",
                  "duration_inf" => "\n<p>Valid values are finite durations with an explicit unit or the keyword 'inf' for infinity. Recognised units: ns, us, ms, s, min, hr, day.</p>");

while (my ($k, $v) = each %typehint2xmltype) {
  die "script error: values of enum type $k unknown\n" if $v eq "Enum" && $enum_values{$k} eq "";
}

# Configurator can't handle UINT32_MAX ... instead of fixing it, just use a different
# default for the rare ones that have a problem (that works as long as there is no
# practical difference between the two)
my %default_overrides = ("multiple_recv_threads_attrs/maxretries" => 2000000000);

my ($name, $table, $kind, $subtable, $multiplicity, $defaultvalue, $typehint, $description);

my %tab2elems;
my %elem;
my %desc;
my %typehint_seen;
my $gobbling_description;
my $skip_lite;
my $in_table;
my $rest;
my $deprecated;

############################

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

sub store_entry {
  $name =~ s/\|.*//; # aliases are not visible in osplconf
  my $ltable = lc $table;
  my $lname = lc $name;
  if (not exists $tab2elems{$ltable}) {
    $tab2elems{$ltable} = $name;
  } else {
    $tab2elems{$ltable} .= ";$name";
  }
  $elem{"$ltable/$lname"} = "$kind;$subtable;$multiplicity;$defaultvalue;$typehint";
  my $ub = exists $typehint2unit{$typehint} && exists $unit_blurb{$typehint2unit{$typehint}} ? $unit_blurb{$typehint2unit{$typehint}} : "";
  $desc{"$ltable/$lname"} = clean_description($description).$ub;
  die "error: no mapping defined for type $typehint\n" if $typehint2xmltype{$typehint} eq "";
  $typehint_seen{$typehint} = 1;
  #printf "%s - $s\n", "$ltable/$lname", $elem{"$ltable/lname"};
  #$typehint = "";
}

sub print_description {
  my ($desc, $indent) = @_;
  print "$indent  <comment><![CDATA[\n";
  print "$desc\n";
  print "$indent    ]]></comment>\n";
}

sub kind_to_kstr {
  my ($kind, $typehint, $table, $name, $isroot) = @_;
  if ($isroot) {
    die unless $kind eq "GROUP";
    return "rootElement";
  } elsif ($kind eq "GROUP" || $kind eq "MGROUP") {
    return "element";
  } elsif ($kind eq "ATTR") {
    return "attribute$typehint2xmltype{$typehint}";
  } elsif ($kind eq "LEAF") {
    return "leaf$typehint2xmltype{$typehint}";
  } else {
    die "error: $kind unrecognized kind ($table/$name)\n";
  }
}

sub transform_default {
  my (@fs) = @_;
  (my $tmp = $fs[3]) =~ s/^"(.*)"$/$1/;
  if ($fs[4] ne "negated_boolean") {
    return $tmp;
  } else {
    my %map = ("true" => "false", "false" => "true");
    return $map{lc $tmp};
  }
}

sub conv_to_xml {
  my ($table, $name, $indent, $prefix, $isroot, $force_min_occ_1) = @_;
  #, fs,vs,vsn,kstr,ts,tsi,tsn,i,min_occ,max_occ,rr,req) { # fs,vs,kstr,... are locals
  my $lctn = lc "$table/$name";
  my @fs = split /;/, $elem{$lctn};
  #print "$table/$name - \n"; for (my $i = 0; $i < @fs; $i++) { print "  - $i $fs[$i]\n" }
  my $kstr = kind_to_kstr($fs[0], $fs[4], $table, $name, $isroot);
  my ($min_occ, $max_occ);
  if ($fs[2] =~ /MAX/) {
    $min_occ = $max_occ = 0;
  } elsif ($fs[2] == 0 || $fs[2] == 1) {
    # multiplicity = 0 => special case, treat as-if 1
    # multiplicity = 1 => required if no default
    # force_min_occ_1 so we can mark "Domain" as required and have it
    # show up in the config editor when creating a new file
    if ($force_min_occ_1) {
      $min_occ = 1;
    } elsif ($fs[0] eq "GROUP" || $fs[0] eq "MGROUP") {
      $min_occ = 0;
    } elsif ($fs[3] eq "" || $fs[3] eq "NULL") {
      $min_occ = 1;
    } else {
      $min_occ = 0;
    }
    $max_occ = 1;
  } else {
    $min_occ = 0; $max_occ = $fs[2];
  }
  if ($fs[0] eq "ATTR") {
    my $req = ($min_occ == 0) ? "false" : "true";
    print "$indent<$kstr name=\"$name\" required=\"$req\">\n";
  } else {
    print "$indent<$kstr name=\"$name\" minOccurrences=\"$min_occ\" maxOccurrences=\"$max_occ\">\n";
  }
  print_description ("$prefix$desc{$lctn}", $indent);
  # enum, int ranges
  if (exists $enum_values{$fs[4]}) {
    my @vs = split /;/, $enum_values{$fs[4]};
    print "$indent  <value>$_</value>\n" for @vs;
  }
  my $rr = exists $range{$lctn} ? $range{$lctn} : "";
  if ($rr eq "" && exists $range{$fs[4]}) { $rr = $range{$fs[4]}; }
  if ($rr ne "") {
    my @vs = split /;/, $rr;
    print "$indent  <minimum>$vs[0]</minimum>\n";
    print "$indent  <maximum>$vs[1]</maximum>\n";
  }
  # remarkably, osplconf can't deal with strings for which no maximum
  # length is specified, even though it accepts unlimited length
  # strings ...
  if ($typehint2xmltype{$fs[4]} eq "String") {
    print "$indent  <maxLength>0</maxLength>\n";
  }
  # default not applicable to GROUPs
  if ($fs[0] ne "GROUP" && $fs[0] ne "MGROUP") {
    my $defover = exists $default_overrides{$lctn} ? $default_overrides{$lctn} : "";
    if ($defover ne "") {
      print "$indent  <default>$defover</default>\n";
    } elsif ($fs[3] eq "" || $fs[3] eq "NULL") {
      print "$indent  <default></default>\n";
    } else {
      print "$indent  <default>".transform_default(@fs)."</default>\n";
    }
  }
  # recurse into subtables if any (except when it is the root: rootElement needs
  # special treatment
  if (!$isroot && $fs[1] ne "") {
    my @ts = sort (split /,/, $fs[1]);
    conv_table_to_xml($_, "$indent  ", $prefix, 0, 0) for @ts;
  }
  print "$indent</$kstr>\n";
}

sub conv_table_to_xml {
  my ($table, $indent, $prefix, $isroot, $force_min_occ_1) = @_;
  return unless exists $tab2elems{$table};
  my @ns = sort (split /;/, $tab2elems{$table});
  conv_to_xml($table, $_, $indent, ($table eq "unsupp_cfgelems") ? "<b>Internal</b>" : $prefix, $isroot, $force_min_occ_1) for @ns;
}

while (<>) {
  if ($gobbling_description) {
    $description .= $_;
    #print "  .. $_\n";
  }

  if ($gobbling_description && /(^|")(\s*\)) *\} *, *$/) {
    $gobbling_description = 0;
    store_entry() unless $deprecated;
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
    #print "END_MARKER $table\n";
    next;
  }

  if (/^static +const +struct +cfgelem +([A-Za-z_0-9]+)\s*\[/) {
    $in_table = 1;
    $table = $1;
    #print "TABLE $table\n";
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
    # extract name + reference to subtable
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
    $subtable = "";
    if ($subelems ne "") { $subtable = $subelems; }
    if ($subattrs ne "") {
      if ($subtable ne "") { $subtable = "$subtable,$subattrs"; }
      else { $subtable = $subattrs; }
    }
    $rest =~ s/ *\) *, *//;
    #print "  kind $kind name $name subtable $subtable -- $rest\n";

    # don't care about the distinction between GROUP/LEAF and
    # GROUP/LEAF_W_ATTRS in the remainer of the code: we simply
    # rely on subtable.
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
    $subtable = "";
    if ($subelems ne "NULL") { $subtable = $subelems; }
    if ($subattrs ne "NULL") {
      if ($subtable ne "") { $subtable = "$subtable,$subattrs"; }
      else { $subtable = $subattrs; }
    }
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
    if ($defaultvalue eq "0") { $defaultvalue = "NULL"; }
    # skip reference to internal name (either ABSOFF(field),
    # RELOFF(field,field) or <int>,<int> (the latter being used by
    # "verbosity")
    $rest =~ s/(ABSOFF *\( *[A-Za-z_0-9.]+ *\)|RELOFF *\( *[A-Za-z_0-9.]+ *, *[A-Za-z_0-9]+ *\)|[0-9]+ *, *[0-9]+) *, *//;
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
      store_entry() unless $deprecated;
    } else {
      # strip the quotes &c. once the full text has been gathered
      $description = $rest;
      $gobbling_description = 1;
    }
    #print "  .. gobbling $gobbling_description";
    next;
  }
}

#print "$tab2elems{cyclonedds_root_cfgelems}\n";
my @rootnames = split /;/, $tab2elems{cyclonedds_root_cfgelems};
die "error: cyclonedds_root_cfgelems has no or multiple entries\n" if @rootnames != 1;
die "error: root_cfgelems doesn't exist\n" unless exists $tab2elems{root_cfgelems};

# Override the type for ControlTopic/Deaf, .../Mute so that an empty
# string is allowed by the configuration validation in spliced
# (easier than adding a boolean_or_empty to DDSI2 for a quick hack)
#if (elem["control_topic_cfgelems/deaf"] == "" || elem["control_topic_cfgelems/mute"] == "") {
#  print FILENAME": error: control_topic_cfgelems/{deaf,mute} missing" > "/dev/stderr";
#  exit 1;
#}
#elem["control_topic_cfgelems/deaf"] = "LEAF;;1;\"false\";string";
#elem["control_topic_cfgelems/mute"] = "LEAF;;1;\"false\";string";

print << 'EOT';
<?xml version="1.0" encoding="UTF-8"?>
<!--
  Copyright(c) 2006 to 2019 ADLINK Technology Limited and others

  This program and the accompanying materials are made available under the
  terms of the Eclipse Public License v. 2.0 which is available at
  http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
  v. 1.0 which is available at
  http://www.eclipse.org/org/documents/edl-v10.php.

  SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

  ===
  generated from src/core/ddsi/src/q_config.c using excx.pl
-->
<splice_meta_config version="1.0">
  <!--xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://www.splice-dds.org/splice_metaconfig.xsd"-->
EOT
conv_table_to_xml("cyclonedds_root_cfgelems", "  ", "", 1, 0);
conv_table_to_xml("root_cfgelems", "  ", "", 0, 1);
print << 'EOT';
</splice_meta_config>
EOT

while (my ($k, $v) = each %typehint_seen) {
  warn "script warning: type mapping defined for $k but not used" if $v == 0;
}
