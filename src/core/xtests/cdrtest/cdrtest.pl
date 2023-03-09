#
# Copyright(c) 2019 to 2021 ZettaScale Technology and others
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

my $outfn = "cdrtest";
local $nextident = "a0000";

my @types = qw(u0 u1 u2 u3 u4 bstr seq ary str uni);
my @idltype = ("octet", "unsigned short", "unsigned long", "unsigned long long", "string");
# unions cannot have an octet as a discriminator ...
my @idltype_unidisc = ("char", "unsigned short", "unsigned long", "unsigned long long", "string");
my @ctype = ("uint8_t", "uint16_t", "uint32_t", "uint64_t", "char *");
my @probs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 0 1 1 1 1);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};
my @noseqprobs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 0 1 1 1 1);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};
my @noaryprobs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 0 1 0 1 1);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};
my @unicaseprobs = do {
  my @ps = qw(0.3 0.3 0.3 0.3 0.3 0 1 0 1 0);
  my (@xs, $sum);
  for (@ps) { $sum += $_; push @xs, $sum; }
  @xs;
};

open IDL, ">${outfn}.idl" or die "can't open ${outfn}.idl";
open CYC, ">${outfn}-main.c" or die "can't open ${outfn}-main.c";
print CYC <<EOF;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

// Cyclone includes
#include "dds/dds.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/sockets.h"
#include "dds/cdr/dds_cdrstream.h"
#include "ddsi__serdata_cdr.h"

// OpenSplice includes
#include "c_base.h"
#include "sd_cdr.h"
#include "sd_serializerXMLTypeinfo.h"
#include "v_copyIn.h"
#include "sac_genericCopyIn.h"

// OpenSplice data types (modified)
#include "cdrtestSplDcps_s.h"

// Cyclone data types
#include "cdrtest.h"

int main()
{
  unsigned char garbage[1000];
  struct ddsi_sertype_cdr ddd;
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
  my $runs = 10;
  print CYC <<EOF;
{
  dds_entity_t tp = dds_create_topic (dp, &$t->[1]_desc, \"$t->[1]\", NULL, NULL);
  if (tp < 0) abort ();
  dds_entity_t rd = dds_create_reader (dp, tp, NULL, NULL);
  if (rd < 0) abort ();
  dds_qos_t *wqos = dds_create_qos();
  dds_qset_data_representation (wqos, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR1 } );
  dds_entity_t wr = dds_create_writer (dp, tp, wqos, NULL);
  if (wr < 0) abort ();
  dds_delete_qos (wqos);
  for (int run = 0; run < $runs; run++)
  {
EOF
;
    print CYC geninit ($t, $runs);
    print CYC <<EOF;

    $t->[1] v$t->[1] = a$t->[1]\[run\];

    /* write a sample and take it */
    if (dds_write (wr, &v$t->[1]) < 0) abort ();
    void *msg = NULL;
    dds_sample_info_t info;
    if (dds_take (rd, &msg, &info, 1, 1) != 1) abort ();
    const $t->[1] *b = msg;
EOF
;
    print CYC gencmp ($t);
    print CYC <<EOF;
    uint32_t actual_sz = 0;
    ddd.type = (struct dds_cdrstream_desc) {
      .size = $t->[1]_desc.m_size,
      .align = $t->[1]_desc.m_align,
      .flagset = $t->[1]_desc.m_flagset,
      .keys.nkeys = 0,
      .keys.keys = NULL,
      // .keys.key_index = NULL,
      .ops.nops = dds_stream_countops ($t->[1]_desc.m_ops, $t->[1]_desc.m_nkeys, $t->[1]_desc.m_keys),
      .ops.ops = (uint32_t *) $t->[1]_desc.m_ops
    };
    for (uint32_t i = 0; i < 1000; i++) {
      for (size_t j = 0; j < sizeof (garbage); j++)
        garbage[j] = (unsigned char) ddsrt_random ();
      if (dds_stream_normalize (garbage, (uint32_t) sizeof (garbage), false, DDSI_RTPS_CDR_ENC_VERSION_1, &ddd.type, false, &actual_sz)) {
        is.m_buffer = garbage;
        is.m_size = 1000;
        is.m_index = 0;
        dds_stream_read_sample (&is, msg, &dds_cdrstream_default_allocator, &ddd.type);
        deser_garbage++;
      }
    }
    char *tmp = malloc ($t->[1]_metaDescriptorLength), *xml_desc = tmp;
    for (int n = 0; n < $t->[1]_metaDescriptorArrLength; n++)
      tmp = stpcpy (tmp, $t->[1]_metaDescriptor[n]);
    sd_serializer serializer = sd_serializerXMLTypeinfoNew (base, 0);
    sd_serializedData meta_data = sd_serializerFromString (serializer, xml_desc);
    if (sd_serializerDeserialize (serializer, meta_data) == NULL) abort ();
    c_type type = c_resolve (base, "$t->[1]"); if (!type) abort ();
    sd_serializedDataFree (meta_data);
    sd_serializerFree (serializer);
    free (xml_desc);
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
    if (!dds_stream_normalize ((void *) blob, blobsz, true, DDSI_RTPS_CDR_ENC_VERSION_1, &ddd.type, false, &actual_sz)) abort ();
    is.m_buffer = blob;
    is.m_size = blobsz;
    is.m_index = 0;
    dds_stream_read_sample (&is, msg, &dds_cdrstream_default_allocator, &ddd.type);
    sd_cdrSerdataFree (sd);
    sd = sd_cdrSerialize (ci, samplecopy);
    blobsz = sd_cdrSerdataBlob (&blob, sd);
    if (!dds_stream_normalize ((void *) blob, blobsz, false, DDSI_RTPS_CDR_ENC_VERSION_1, &ddd.type, false, &actual_sz)) abort ();
    for (uint32_t i = 1; i < blobsz && i <= 16; i++) {
      if (dds_stream_normalize ((void *) blob, blobsz - i, false, DDSI_RTPS_CDR_ENC_VERSION_1, &ddd.type, false, &actual_sz)) abort ();
    }
    sd_cdrSerdataFree (sd);
EOF
;
    print CYC gencmp ($t);
    print CYC <<EOF;
    sd_cdrInfoFree (ci);
    c_free (samplecopy);
    dds_return_loan (rd, &msg, 1);
  } /* for run 0..9 */
  dds_delete (rd);
  dds_delete (wr);
  dds_delete (tp);
}

EOF
  ;
}

sub geninit {
  my ($t, $runs) = @_;
  my $ind = "    ";
  my $tmp = $ind . "/* init samples */\n";
  my @res;
  for (1..$runs) {
    my @out;
    push (@res, geninit1 ($ind . "  ", \@out, $t, "_r$_"));
    $tmp .= (join "", @out);
  }
  $tmp .= $ind . "$t->[1] a$t->[1]\[$runs\] = {";
  for (1..$runs) {
    $tmp .= $ind . "  " . pop (@res) . ", \n"
  }
  $tmp .= $ind . "};\n";
  return $tmp;
}

sub gencmp {
  my ($t) = @_;
  my $ind = "    ";
  my $res = gencmp1 ($ind, $t, "v$t->[1]", "");
  return "\n" . $ind . "/* compare b with original sample */\n" . $res;
}

sub geninit1 {
  my ($ind, $out, $t, $idxsuf) = @_;
  if ($t->[0] =~ /^u([0-3])$/) {
    return int (rand (10));
  } elsif ($t->[0] eq "u4") {
    return "\"".("x"x(int (rand (8))))."\"";
  } elsif ($t->[0] eq "bstr") {
    return "\"".("x"x(int ($t->[1] - 1)))."\"";
  } elsif ($t->[0] eq "seq") {
    my $len = int (rand (10));
    my $bufref;
    if ($len == 0) {
      $bufref = "0";
    } else {
      my $buf = "vb$t->[1]_$idxsuf";
      $bufref = "$buf";
      my $ctype;
      if ($t->[2]->[0] =~ /^u(\d+)$/) {
        $ctype = $ctype[$1];
      } elsif ($t->[2]->[0] eq "bstr") {
        $ctype = "char *";
      } else {
        $ctype = $t->[2]->[1];
      }
      my $tmp = $ind . "$ctype $buf\[\] = {";
      for (1..$len) {
        $tmp .= geninit1 ($ind, $out, $t->[2], "${idxsuf}_$_");
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
      $tmp .= geninit1 ($ind, $out, $t->[2], "${idxsuf}_$_");
      $tmp .= "," if $_ < $len;
    }
    $tmp .= "}";
    return $tmp;
  } elsif ($t->[0] eq "str") {
    my $tmp = "{";
    for (my $i = 2; $i < @$t; $i++) {
      my ($name, $st) = @{$t->[$i]};
      $tmp .= geninit1 ($ind, $out, $st, "${idxsuf}_");
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
    $tmp .= geninit1 ($ind, $out, $st, "${idxsuf}_");
    $tmp .= "}}";
    return $tmp;
  } else {
    die;
  }
}

sub gencmp1 {
  my ($ind, $t, $toplevel, $path) = @_;
  if ($t->[0] =~ /^u([0-3])$/) {
    return $ind . "if ($toplevel.$path != b->$path) abort ();\n";
  } elsif ($t->[0] eq "u4") {
    return $ind . "if (strcmp ($toplevel.$path, b->$path) != 0) abort ();\n";
  } elsif ($t->[0] eq "bstr") {
    return $ind . "if (strcmp ($toplevel.$path, b->$path) != 0) abort ();\n";
  } elsif ($t->[0] eq "seq") {
    my $idx = "i".length $path;
    return ($ind . "if ($toplevel.$path._length != b->$path._length) abort ();\n" .
            $ind . "for (uint32_t $idx = 0; $idx < $toplevel.$path._length; $idx++) {\n" .
            $ind . gencmp1 ("  ", $t->[2], $toplevel, "$path._buffer[$idx]") .
            $ind . "}\n");
  } elsif ($t->[0] eq "ary") {
    my $len = $t->[3]; die unless $len > 0;
    my $idx = "i".length $path;
    return ($ind . "for (uint32_t $idx = 0; $idx < $len; $idx++) {\n" .
            $ind . gencmp1 ("  ", $t->[2], $toplevel, "$path\[$idx]") .
            $ind . "}\n");
  } elsif ($t->[0] eq "str") {
    my $sep = length $path == 0 ? "" : ".";
    my $tmp = $ind;
    for (my $i = 2; $i < @$t; $i++) {
      my ($name, $st) = @{$t->[$i]};
      $tmp .= gencmp1 ("  ", $st, $toplevel, "$path$sep$name");
    }
    return $tmp;
  } elsif ($t->[0] eq "uni") { # uni name disctype hasdef case...
    my $tmp = $ind . "if ($toplevel.$path._d != b->$path._d) abort ();\n";
    my $hasdef = $t->[3];
    $tmp .= $ind . "switch ($toplevel.$path._d) {\n";
    for (my $i = 4; $i < @$t; $i++) {
      my ($name, $st) = @{$t->[$i]};
      my $discval = $i - 4;
      $discval = "'".chr ($discval + ord ("A"))."'" if $t->[2] eq "u0";
      $tmp .= ($i == @$t && $hasdef) ? $ind . "  default:\n" : $ind . "  case $discval:\n";
      $tmp .= gencmp1 ($ind . "  ", $st, $toplevel, "$path._u.$name");
      $tmp .= $ind . "break;\n";
    }
    $tmp .= $ind . "  }\n";
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
  } elsif ($t->[0] eq "bstr") {
    $res = "${ind}string<$t->[1]> $name;\n";
  } elsif ($t->[0] eq "seq") {
    push @$out, genidl1td ("", $out, $t);
    $res = "${ind}$t->[1] $name;\n";
  } elsif ($t->[0] eq "ary") {
    if ($t->[2]->[0] =~ /^u(\d+)$/) {
      $res = "${ind}$idltype[$1] ${name}[$t->[3]];\n";
    } elsif ($t->[2]->[0] eq "bstr") {
      $res = "${ind}string<$t->[2]->[1]> ${name}[$t->[3]];\n";
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
    } elsif ($t->[2]->[0] eq "bstr") {
      return "${ind}typedef sequence<string<$t->[2]->[1]>> $t->[1];\n";
    } else {
      push @$out, genidl1td ("", $out, $t->[2]);
      return "${ind}typedef sequence<$t->[2]->[1]> $t->[1];\n";
    }
  } elsif ($t->[0] eq "ary") {
    if ($t->[2]->[0] =~ /^u(\d+)$/) {
      return "${ind}typedef ${idltype[$1]} $t->[1]"."[$t->[3]];\n";
    } elsif ($t->[2]->[0] eq "bstr") {
      return "${ind}typedef string<$t->[2]->[1]> $t->[1]"."[$t->[3]];\n";
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
sub genbstr { return ["bstr", 2 + int (rand (20))]; }
sub genseq { return ["seq", nextident (), gentype ($_[0] + 1, @noseqprobs)]; }
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
