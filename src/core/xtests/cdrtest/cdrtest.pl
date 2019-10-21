#
# Copyright(c) 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

#use strict;

use Data::Dumper;
$Data::Dumper::Terse = 1;
$Data::Dumper::Useqq = 1;

my $outfn = "xxx";
local $nextident = "a0000";

my @types = qw(u0 u1 u2 u3 u4 seq ary str uni);
my @idltype = ("octet", "unsigned short", "unsigned long", "unsigned long long", "string");
# unions cannot have an octet as a discriminator ...
my @idltype_unidisc = ("char", "unsigned short", "unsigned long", "unsigned long long", "string");
my @ctype = ("uint8_t", "uint16_t", "uint32_t", "uint64_t", "char *");
my @probs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 1 1 1 1);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};
my @noaryprobs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 1 0 1 1);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};
my @unicaseprobs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 1 0 1 0);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};

open IDL, ">${outfn}.idl" or die "can't open ${outfn}.idl";
open CYC, ">${outfn}-cyc.c" or die "can't open ${outfn}-cyc.c";
print CYC <<EOF;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "cyclonedds/dds.h"
#include "cyclonedds/ddsrt/random.h"
#include "cyclonedds/ddsrt/sockets.h"
#include "cyclonedds/ddsi/ddsi_serdata_default.h"
#include "dds__stream.h"

#include "c_base.h"
#include "sd_cdr.h"
#include "sd_serializerXMLTypeinfo.h"
#include "v_copyIn.h"
#include "sac_genericCopyIn.h"

#include "xxx.h"
int main()
{
  unsigned char garbage[1000];
  struct ddsi_sertopic_default ddd;
  uint32_t deser_garbage = 0;
  memset (&ddd, 0, sizeof (ddd));
  dds_istream_t is;
  c_base base = c_create ("X", NULL, 0, 0);
  dds_entity_t dp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (dp < 0) abort ();
EOF
;
for (1 .. 300) {
  my $t = genstr (0);
  my $idl = genidltd ($t);
  print IDL $idl;
  (my $idlcmt = $idl) =~ s,^,//,mg;
  print CYC $idlcmt;
  gencyc ($t);
}
print CYC <<EOF;
  dds_delete (dp);
  printf ("deserialized %"PRIu32" pieces of garbage\\n", deser_garbage);
  return 0;
}
EOF
;
close CYC;
close IDL;

sub gencyc {
  my ($t) = @_;
  print CYC <<EOF;
{
  dds_entity_t tp = dds_create_topic (dp, &$t->[1]_desc, \"$t->[1]\", NULL, NULL);
  if (tp < 0) abort ();
  dds_entity_t rd = dds_create_reader (dp, tp, NULL, NULL);
  if (rd < 0) abort ();
  dds_entity_t wr = dds_create_writer (dp, tp, NULL, NULL);
  if (wr < 0) abort ();
EOF
  ;
  print CYC geninit ($t);
  print CYC <<EOF;
  if (dds_write (wr, &v$t->[1]) < 0) abort ();
  void *msg = NULL;
  dds_sample_info_t info;
  if (dds_take (rd, &msg, &info, 1, 1) != 1) abort ();
  const $t->[1] *b = msg;
EOF
  ;
  print CYC gencmp ($t);
  print CYC <<EOF;
  ddd.type = (struct dds_topic_descriptor *) &$t->[1]_desc;
  for (uint32_t i = 0; i < 1000; i++) {
    for (size_t j = 0; j < sizeof (garbage); j++)
      garbage[j] = (unsigned char) ddsrt_random ();
    if (dds_stream_normalize (garbage, (uint32_t) sizeof (garbage), false, &ddd, false)) {
      is.m_buffer = garbage;
      is.m_size = 1000;
      is.m_index = 0;
      dds_stream_read_sample (&is, msg, &ddd);
      deser_garbage++;
    }
  }
  sd_serializer serializer = sd_serializerXMLTypeinfoNew (base, 0);
  sd_serializedData meta_data = sd_serializerFromString (serializer, $t->[1]_desc.m_meta);
  if (sd_serializerDeserialize (serializer, meta_data) == NULL) abort ();
  c_type type = c_resolve (base, "$t->[1]"); if (!type) abort ();
  sd_serializedDataFree (meta_data);
  sd_serializerFree (serializer);
  struct sd_cdrInfo *ci = sd_cdrInfoNew (type);
  if (sd_cdrCompile (ci) < 0) abort ();
  DDS_copyCache cc = DDS_copyCacheNew ((c_metaObject) type);
  struct DDS_srcInfo_s src = { .src = &v$t->[1], cc };
  void *samplecopy = c_new (type);
  DDS_copyInStruct (base, &src, samplecopy);
  struct sd_cdrSerdata *sd = sd_cdrSerializeBSwap (ci, samplecopy);
  const void *blob;
  uint32_t blobsz = sd_cdrSerdataBlob (&blob, sd);
  /* hack alert: modifying read-only blob ...*/
  if (!dds_stream_normalize ((void *) blob, blobsz, true, &ddd, false)) abort ();
  is.m_buffer = blob;
  is.m_size = blobsz;
  is.m_index = 0;
  dds_stream_read_sample (&is, msg, &ddd);
  sd_cdrSerdataFree (sd);
  sd = sd_cdrSerialize (ci, samplecopy);
  blobsz = sd_cdrSerdataBlob (&blob, sd);
  if (!dds_stream_normalize ((void *) blob, blobsz, false, &ddd, false)) abort ();
  for (uint32_t i = 1; i < blobsz && i <= 16; i++) {
    if (dds_stream_normalize ((void *) blob, blobsz - i, false, &ddd, false)) abort ();
  }
  sd_cdrSerdataFree (sd);
EOF
;
  print CYC gencmp ($t);
  print CYC <<EOF;
  sd_cdrInfoFree (ci);
  dds_return_loan (rd, &msg, 1);
  dds_delete (rd);
  dds_delete (wr);
  dds_delete (tp);
}
EOF
  ;
}

sub geninit {
  my ($t) = @_;
  my @out;
  my $res = geninit1 ("  ", \@out, $t, "");
  return (join "", @out) . "  $t->[1] v$t->[1] = $res;\n";
}

sub gencmp {
  my ($t) = @_;
  my $res = gencmp1 ($t, "v$t->[1]", "");
  return $res;
}

sub geninit1 {
  my ($ind, $out, $t, $idxsuf) = @_;
  if ($t->[0] =~ /^u([0-3])$/) {
    return int (rand (10));
  } elsif ($t->[0] eq "u4") {
    return "\"".("x"x(int (rand (8))))."\"";
  } elsif ($t->[0] eq "seq") {
    my $len = int (rand (10));
    my $bufref;
    if ($len == 0) {
      $bufref = "0";
    } else {
      my $buf = "vb$t->[1]_$idxsuf";
      $bufref = "$buf";
      my $ctype = ($t->[2]->[0] =~ /^u(\d+)$/) ? $ctype[$1] : $t->[2]->[1];
      my $tmp = "  $ctype $buf\[\] = {";
      for (1..$len) {
        $tmp .= geninit1 ("$ind", $out, $t->[2], "${idxsuf}_$_");
        $tmp .= "," if $_ < $len;
      }
      $tmp .= "};\n";
      push @$out, $tmp;
    }
    return "{$len,$len,$bufref,0}";
  } elsif ($t->[0] eq "ary") {
    my $len = $t->[3]; die unless $len > 0;
    my $tmp = "{";
    for (1..$len) {
      $tmp .= geninit1 ("$ind", $out, $t->[2], "${idxsuf}_$_");
      $tmp .= "," if $_ < $len;
    }
    $tmp .= "}";
    return $tmp;
  } elsif ($t->[0] eq "str") {
    my $tmp = "{";
    for (my $i = 2; $i < @$t; $i++) {
      my ($name, $st) = @{$t->[$i]};
      $tmp .= geninit1 ("", $out, $st, "${idxsuf}_");
      $tmp .= "," if $i + 1 < @$t;
    }
    $tmp .= "}";
    return $tmp;
  } elsif ($t->[0] eq "uni") { # uni name disctype hasdef case...
    my $discval = int(rand(@$t - 3)); # -3 so we generate values outside label range as well
    my $hasdef = $t->[3];
    my $case = (4 + $discval < @$t) ? $discval : $hasdef ? @$t-1 : 0;
    $discval = ("'".chr ($discval + ord ("A"))."'") if $t->[2] eq "u0";
    # $case matches have a label or default; if no default generate an initializer for the
    # first case to avoid compiler warnings
    my ($name, $st) = @{$t->[4+$case]};
    my $tmp = "{$discval,{.$name=";
    $tmp .= geninit1 ("", $out, $st, "${idxsuf}_");
    $tmp .= "}}";
    return $tmp;
  } else {
    die;
  }
}

sub gencmp1 {
  my ($t, $toplevel, $path) = @_;
  if ($t->[0] =~ /^u([0-3])$/) {
    return "  if ($toplevel.$path != b->$path) abort ();\n";
  } elsif ($t->[0] eq "u4") {
    return "  if (strcmp ($toplevel.$path, b->$path) != 0) abort ();\n";
  } elsif ($t->[0] eq "seq") {
    my $idx = "i".length $path;
    return ("if ($toplevel.$path._length != b->$path._length) abort ();\n" .
            "for (uint32_t $idx = 0; $idx < $toplevel.$path._length; $idx++) {\n" .
            gencmp1 ($t->[2], $toplevel, "$path._buffer[$idx]") .
            "}\n");
  } elsif ($t->[0] eq "ary") {
    my $len = $t->[3]; die unless $len > 0;
    my $idx = "i".length $path;
    return ("for (uint32_t $idx = 0; $idx < $len; $idx++) {\n" .
            gencmp1 ($t->[2], $toplevel, "$path\[$idx]") .
            "}\n");
  } elsif ($t->[0] eq "str") {
    my $sep = length $path == 0 ? "" : ".";
    my $tmp = "";
    for (my $i = 2; $i < @$t; $i++) {
      my ($name, $st) = @{$t->[$i]};
      $tmp .= gencmp1 ($st, $toplevel, "$path$sep$name");
    }
    return $tmp;
  } elsif ($t->[0] eq "uni") { # uni name disctype hasdef case...
    my $tmp = "if ($toplevel.$path._d != b->$path._d) abort ();\n";
    my $hasdef = $t->[3];
    $tmp .= "switch ($toplevel.$path._d) {\n";
    for (my $i = 4; $i < @$t; $i++) {
      my ($name, $st) = @{$t->[$i]};
      my $discval = $i - 4;
      $discval = "'".chr ($discval + ord ("A"))."'" if $t->[2] eq "u0";
      $tmp .= ($i == @$t && $hasdef) ? "  default:\n" : "  case $discval:\n";
      $tmp .= gencmp1 ($st, $toplevel, "$path._u.$name");
      $tmp .= "break;\n";
    }
    $tmp .= "}\n";
    return $tmp;
  } else {
    die;
  }
}

sub genidltd {
  my ($t) = @_;
  my @out = ();
  my $res = genidl1td ("", \@out, $t);
  return (join "", @out) . $res . "#pragma keylist $t->[1]\n//------------\n";
};

sub genidl1 {
  my ($ind, $out, $name, $t) = @_;
  my $res = "";
  if ($t->[0] =~ /^u(\d+)$/) {
    $res = "${ind}$idltype[$1] $name;\n";
  } elsif ($t->[0] eq "seq") {
    push @$out, genidl1td ("", $out, $t);
    $res = "${ind}$t->[1] $name;\n";
  } elsif ($t->[0] eq "ary") {
    if ($t->[2]->[0] =~ /^u(\d+)$/) {
      $res = "${ind}$idltype[$1] ${name}[$t->[3]];\n";
    } else {
      push @$out, genidl1td ("", $out, $t->[2]);
      $res = "${ind}$t->[2]->[1] ${name}[$t->[3]];\n";
    }
  } elsif ($t->[0] eq "str") {
    push @$out, genidl1td ("", $out, $t);
    $res = "${ind}$t->[1] $name;\n";
  } elsif ($t->[0] eq "uni") {
    push @$out, genidl1td ("", $out, $t);
    $res = "${ind}$t->[1] $name;\n";
  } else {
    die;
  }
  return $res;
}

sub genidl1td {
  my ($ind, $out, $t) = @_;
  if ($t->[0] eq "seq") {
    if ($t->[2]->[0] =~ /^u(\d+)$/) {
      return "${ind}typedef sequence<$idltype[$1]> $t->[1];\n";
    } else {
      push @$out, genidl1td ("", $out, $t->[2]);
      return "${ind}typedef sequence<$t->[2]->[1]> $t->[1];\n";
    }
  } elsif ($t->[0] eq "ary") {
    if ($t->[2]->[0] =~ /^u(\d+)$/) {
      return "${ind}typedef ${idltype[$1]} $t->[1]"."[$t->[3]];\n";
    } else {
      push @$out, genidl1td ("", $out, $t->[2]);
      return "${ind}typedef $t->[2]->[1] $t->[1]"."[$t->[3]];\n";
    }
  } elsif ($t->[0] eq "str") {
    my $res = "struct $t->[1] {\n";
    for (my $i = 2; $i < @$t; $i++) {
      $res .= genidl1 ($ind."  ", $out, @{$t->[$i]});
    }
    $res .= "};\n";
    return $res;
  } elsif ($t->[0] eq "uni") {
    my $hasdef = $t->[3];
    die unless $t->[2] =~ /^u([0-2])$/;
    my $res = "${ind}union $t->[1] switch ($idltype_unidisc[$1]) {\n";
    for (my $i = 4; $i < @$t; $i++) {
      my $discval = $i - 4;
      $discval = "'".(chr ($discval + ord ("A")))."'" if $t->[2] eq "u0";
      $res .= ($i == @$t && $hasdef) ? "$ind  default: " : "$ind  case $discval: ";
      $res .= genidl1 ($ind."    ", $out, @{$t->[$i]});
    }
    $res .= "};\n";
    return $res;
  } else {
    die;
  }
};

sub genu0 { return ["u0"]; }
sub genu1 { return ["u1"]; }
sub genu2 { return ["u2"]; }
sub genu3 { return ["u3"]; }
sub genu4 { return ["u4"]; }
sub genseq { return ["seq", nextident (), gentype ($_[0] + 1, @probs)]; }
sub genary { return ["ary", nextident (), gentype ($_[0] + 1, @noaryprobs), 1 + int (rand (4))]; }
sub genstr {
  my @ts = ("str", nextident ());
  my $n = 1 + int (rand (4));
  push @ts, [ nextident (), gentype ($_[0] + 1, @probs) ] while $n--;
  return \@ts;
}
sub genuni {
  my @ts = ("uni", nextident (), "u".(int (rand (2))), int (rand (1))); # uni name disctype hasdef case...
  my $ncases = 1 + int (rand (4));
  push @ts, [ nextident (), gentype ($_[0] + 1, @unicaseprobs) ] while $ncases--;
  return \@ts;
}

sub gentype {
  my $t = choosetype (@_);
  my $f = "gen$t";
  return &$f (@_);
}

sub choosetype {
  my ($lev, @probs) = @_;
  my $r = rand ($_[0] == 4 ? $probs[3] : $probs[$#probs]);
  my $i;
  for ($i = 0; $i < $#probs; $i++) {
    last if $r < $probs[$i];
  }
  return $types[$i];
}

sub nextident {
  return $nextident++;
}
