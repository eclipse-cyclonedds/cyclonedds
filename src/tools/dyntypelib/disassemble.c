/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_opcodes.h"

#include "dyntypelib.h"

static const char *opcode_name (uint32_t op)
{
  switch (op)
  {
    case DDS_SOP_RTS: return "RTS";
    case DDS_SOP_ADR: return "ADR";
    case DDS_SOP_JSR: return "JSR";
    case DDS_SOP_JEQ: return "JEQ";
    case DDS_SOP_DLC: return "DLC";
    case DDS_SOP_PLC: return "PLC";
    case DDS_SOP_PLM: return "PLM";
    case DDS_SOP_KOF: return "KOF";
    case DDS_SOP_JEQ4: return "JEQ4";
    case DDS_SOP_MID: return "MID";
    case DDS_OP_EVS: return "EVS";
    case DDS_OP_EVM: return "EVM";
  }
  return "?";
}

static const char *typecode_name (enum dds_stream_typecode type)
{
  switch (type)
  {
    case DDS_SOP_VAL_1BY: return "1BY";
    case DDS_SOP_VAL_2BY: return "2BY";
    case DDS_SOP_VAL_4BY: return "4BY";
    case DDS_SOP_VAL_8BY: return "8BY";
    case DDS_SOP_VAL_STR: return "STR";
    case DDS_SOP_VAL_BST: return "BST";
    case DDS_SOP_VAL_SEQ: return "SEQ";
    case DDS_SOP_VAL_ARR: return "ARR";
    case DDS_SOP_VAL_UNI: return "UNI";
    case DDS_SOP_VAL_STU: return "STU";
    case DDS_SOP_VAL_BSQ: return "BSQ";
    case DDS_SOP_VAL_ENU: return "ENU";
    case DDS_SOP_VAL_EXT: return "EXT";
    case DDS_SOP_VAL_BLN: return "BLN";
    case DDS_SOP_VAL_BMK: return "BMK";
    case DDS_SOP_VAL_WSTR: return "WSTR";
    case DDS_SOP_VAL_BWSTR: return "BWSTR";
    case DDS_SOP_VAL_WCHAR: return "WCHAR";
    case DDS_SOP_VAL_16BY: return "16BY";
  }
  return "?";
}

static void print_sep (bool *need_sep)
{
  if (*need_sep)
    printf (",");
  *need_sep = true;
}

static void print_one_flag (bool *need_sep, const char *name)
{
  print_sep (need_sep);
  printf ("%s", name);
}

static void print_common_flags (uint32_t insn, bool union_default_flag)
{
  bool need_sep = false;
  const uint32_t flags = DDS_OP_FLAGS (insn);
  printf (" flags={");
  if (insn & DDS_OP_FLAG_EXT)
    print_one_flag (&need_sep, "EXT");
  if (insn & DDS_OP_FLAG_TYPE_TC_DEF)
    print_one_flag (&need_sep, "TYPE_TC_DEF");
  if (insn & DDS_OP_FLAG_TYPE_TC_TRIM)
    print_one_flag (&need_sep, "TYPE_TC_TRIM");
  if (insn & DDS_OP_FLAG_SUBTYPE_TC_DEF)
    print_one_flag (&need_sep, "SUBTYPE_TC_DEF");
  if (insn & DDS_OP_FLAG_SUBTYPE_TC_TRIM)
    print_one_flag (&need_sep, "SUBTYPE_TC_TRIM");
  if (flags & DDS_OP_FLAG_KEY)
    print_one_flag (&need_sep, "KEY");
  if (flags & DDS_OP_FLAG_FP)
    print_one_flag (&need_sep, union_default_flag ? "DEF" : "FP");
  if (flags & DDS_OP_FLAG_SGN)
    print_one_flag (&need_sep, "SGN");
  if (flags & DDS_OP_FLAG_MU)
    print_one_flag (&need_sep, "MU");
  if (flags & DDS_OP_FLAG_BASE)
    print_one_flag (&need_sep, "BASE");
  if (flags & DDS_OP_FLAG_OPT)
    print_one_flag (&need_sep, "OPT");
  if (flags & DDS_OP_FLAG_SZ_MASK)
  {
    print_sep (&need_sep);
    printf ("SZ=%u", DDS_OP_FLAGS_SZ (flags));
  }
  printf ("}");
}

static void print_plm_flags (uint32_t insn)
{
  bool need_sep = false;
  uint32_t flags = (insn & DDS_PLM_FLAGS_MASK) >> 16;
  printf (" flags={");
  if (flags & DDS_OP_FLAG_BASE)
    print_one_flag (&need_sep, "BASE");
  flags &= ~DDS_OP_FLAG_BASE;
  if (flags)
  {
    print_sep (&need_sep);
    printf ("0x%02"PRIx32, flags);
  }
  printf ("}");
}

static void print_topic_flags (uint32_t flags)
{
  bool need_sep = false;
  printf ("flags={");
#define PF(name) do { \
    if (flags & DDS_TOPIC_##name) { \
      print_one_flag (&need_sep, #name); \
      flags &= ~DDS_TOPIC_##name; \
    } \
  } while (0)
  PF(NO_OPTIMIZE);
  PF(FIXED_KEY);
  PF(CONTAINS_UNION);
  PF(FIXED_SIZE);
  PF(FIXED_KEY_XCDR2);
  PF(XTYPES_METADATA);
  PF(RESTRICT_DATA_REPRESENTATION);
  PF(KEY_MUTABLE);
  PF(KEY_APPENDABLE);
  PF(FIXED_KEY_XCDR2_KEYHASH);
  PF(KEY_SEQUENCE);
  PF(KEY_ARRAY_NONPRIM);
  PF(KEY_UNION);
#undef PF
  if (flags)
  {
    print_sep (&need_sep);
    printf ("0x%08"PRIx32, flags);
  }
  printf ("}");
}

static uint32_t adr_width (uint32_t insn)
{
  const enum dds_stream_typecode type = DDS_OP_TYPE (insn);
  switch (type)
  {
    case DDS_SOP_VAL_1BY:
    case DDS_SOP_VAL_2BY:
    case DDS_SOP_VAL_4BY:
    case DDS_SOP_VAL_8BY:
    case DDS_SOP_VAL_STR:
    case DDS_SOP_VAL_WSTR:
    case DDS_SOP_VAL_BLN:
    case DDS_SOP_VAL_WCHAR:
    case DDS_SOP_VAL_16BY:
      return 2;
    case DDS_SOP_VAL_BST:
    case DDS_SOP_VAL_BWSTR:
    case DDS_SOP_VAL_ENU:
      return 3;
    case DDS_SOP_VAL_BMK:
      return 4;
    case DDS_SOP_VAL_SEQ:
    case DDS_SOP_VAL_BSQ:
    case DDS_SOP_VAL_ARR: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      uint32_t width = (type == DDS_SOP_VAL_SEQ) ? 2 : 3;
      switch (subtype)
      {
        case DDS_SOP_VAL_ENU:
        case DDS_SOP_VAL_BST:
        case DDS_SOP_VAL_BWSTR:
          width++;
          if (type == DDS_SOP_VAL_ARR && (subtype == DDS_SOP_VAL_BST || subtype == DDS_SOP_VAL_BWSTR))
            width++;
          break;
        case DDS_SOP_VAL_BMK:
        case DDS_SOP_VAL_SEQ:
        case DDS_SOP_VAL_ARR:
        case DDS_SOP_VAL_UNI:
        case DDS_SOP_VAL_STU:
        case DDS_SOP_VAL_BSQ:
          width += 2;
          break;
        default:
          break;
      }
      return width;
    }
    case DDS_SOP_VAL_UNI: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
      if (subtype == DDS_SOP_VAL_ENU)
        return 5;
      if (subtype == DDS_SOP_VAL_BMK)
        return 6;
      return 4;
    }
    case DDS_SOP_VAL_EXT:
      return 3 + ((insn & (DDS_OP_FLAG_EXT | DDS_OP_FLAG_OPT)) != 0);
    case DDS_SOP_VAL_STU:
      return 1;
  }
  return 1;
}

static uint32_t op_width (const uint32_t *ops, uint32_t nops, uint32_t idx)
{
  const uint32_t insn = ops[idx];
  switch (insn & DDS_OP_MASK)
  {
    case DDS_SOP_RTS:
    case DDS_SOP_JSR:
    case DDS_SOP_DLC:
    case DDS_SOP_PLC:
      return 1;
    case DDS_SOP_ADR:
      return adr_width (insn);
    case DDS_SOP_JEQ:
      return 3;
    case DDS_SOP_JEQ4:
      return 4;
    case DDS_SOP_PLM:
    case DDS_SOP_MID:
    case DDS_OP_EVM:
      return 2;
    case DDS_SOP_KOF:
      return 1 + DDS_OP_LENGTH (insn);
    case DDS_OP_EVS:
      return (idx + 2 < nops) ? 3 + ops[idx + 2] : 3;
  }
  return 1;
}

static bool rel_target_idx (uint32_t idx, int32_t rel, uint32_t *target_idx)
{
  const int64_t target = (int64_t) idx + rel;
  if (target < 0 || target > UINT32_MAX)
    return false;
  *target_idx = (uint32_t) target;
  return true;
}

static bool union_default_case_idx (const uint32_t *ops, uint32_t nops, uint32_t idx, uint32_t *default_idx)
{
  const uint32_t expected = op_width (ops, nops, idx);
  if (expected > nops - idx)
    return false;
  const uint32_t insn = ops[idx];
  if ((insn & DDS_OP_MASK) != DDS_OP_ADR || DDS_OP_TYPE (insn) != DDS_SOP_VAL_UNI || (insn & DDS_OP_FLAG_DEF) == 0)
    return false;
  if (expected < 4 || ops[idx + 2] == 0)
    return false;

  uint32_t case_idx;
  const int16_t rel = (int16_t) (uint16_t) ops[idx + 3];
  if (!rel_target_idx (idx, rel, &case_idx) || case_idx >= nops)
    return false;

  for (uint32_t n = 0; n < ops[idx + 2]; n++)
  {
    const uint32_t width = op_width (ops, nops, case_idx);
    if (width > nops - case_idx)
      return false;
    if (n == ops[idx + 2] - 1)
    {
      *default_idx = case_idx;
      return true;
    }
    case_idx += width;
    if (case_idx >= nops)
      return false;
  }
  return false;
}

static bool is_union_default_case (const uint32_t *ops, uint32_t nops, uint32_t idx)
{
  uint32_t op_idx = 0;
  while (op_idx < nops)
  {
    const uint32_t expected = op_width (ops, nops, op_idx);
    if (expected > nops - op_idx)
      return false;
    uint32_t default_idx;
    if (union_default_case_idx (ops, nops, op_idx, &default_idx) && default_idx == idx)
      return true;
    op_idx += expected;
  }
  return false;
}

static void print_abs_target (uint32_t idx, int32_t rel)
{
  uint32_t target;
  printf (" (=");
  if (!rel_target_idx (idx, rel, &target))
    printf ("?");
  else
    printf ("%"PRIu32, target);
  printf (")");
}

static void print_rel_offset (int32_t rel)
{
  printf ("%+"PRId32, rel);
}

static void print_couple (const char *name, uint32_t idx, uint32_t word, const char *hi_name, const char *lo_name)
{
  const uint32_t hi = word >> 16;
  const int32_t lo = (int16_t) (uint16_t) word;
  printf (" %s={%s=", name, hi_name);
  print_rel_offset ((int32_t) hi);
  print_abs_target (idx, (int32_t) hi);
  printf (",%s=", lo_name);
  print_rel_offset (lo);
  print_abs_target (idx, lo);
  printf ("}");
}

static void print_disc_value (uint32_t value)
{
  printf (" disc=0x%08"PRIx32"(%"PRId32")", value, (int32_t) value);
}

static void print_adr (const uint32_t *ops, uint32_t idx)
{
  const uint32_t insn = ops[idx];
  const enum dds_stream_typecode type = DDS_OP_TYPE (insn);
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  printf ("/%s", typecode_name (type));
  if (type == DDS_SOP_VAL_SEQ || type == DDS_SOP_VAL_BSQ || type == DDS_SOP_VAL_ARR || type == DDS_SOP_VAL_UNI)
    printf ("/%s", typecode_name (subtype));
  print_common_flags (insn, type == DDS_SOP_VAL_UNI);

  switch (type)
  {
    case DDS_SOP_VAL_1BY:
    case DDS_SOP_VAL_2BY:
    case DDS_SOP_VAL_4BY:
    case DDS_SOP_VAL_8BY:
    case DDS_SOP_VAL_STR:
    case DDS_SOP_VAL_WSTR:
    case DDS_SOP_VAL_BLN:
    case DDS_SOP_VAL_WCHAR:
    case DDS_SOP_VAL_16BY:
      printf (" offset=%"PRIu32, ops[idx + 1]);
      break;
    case DDS_SOP_VAL_BST:
    case DDS_SOP_VAL_BWSTR:
      printf (" offset=%"PRIu32" bound=%"PRIu32, ops[idx + 1], ops[idx + 2]);
      break;
    case DDS_SOP_VAL_ENU:
      printf (" offset=%"PRIu32" max=%"PRIu32, ops[idx + 1], ops[idx + 2]);
      break;
    case DDS_SOP_VAL_BMK:
      printf (" offset=%"PRIu32" bits=0x%08"PRIx32"%08"PRIx32, ops[idx + 1], ops[idx + 2], ops[idx + 3]);
      break;
    case DDS_SOP_VAL_SEQ:
    case DDS_SOP_VAL_BSQ:
    case DDS_SOP_VAL_ARR: {
      uint32_t arg = idx + 1;
      printf (" offset=%"PRIu32, ops[arg++]);
      if (type == DDS_SOP_VAL_BSQ)
        printf (" bound=%"PRIu32, ops[arg++]);
      else if (type == DDS_SOP_VAL_ARR)
        printf (" length=%"PRIu32, ops[arg++]);
      switch (subtype)
      {
        case DDS_SOP_VAL_ENU:
          printf (" max=%"PRIu32, ops[arg]);
          break;
        case DDS_SOP_VAL_BMK:
          printf (" bits=0x%08"PRIx32"%08"PRIx32, ops[arg], ops[arg + 1]);
          break;
        case DDS_SOP_VAL_BST:
        case DDS_SOP_VAL_BWSTR:
          if (type == DDS_SOP_VAL_ARR)
            printf (" reserved=%"PRIu32" bound=%"PRIu32, ops[arg], ops[arg + 1]);
          else
            printf (" bound=%"PRIu32, ops[arg]);
          break;
        case DDS_SOP_VAL_SEQ:
        case DDS_SOP_VAL_ARR:
        case DDS_SOP_VAL_UNI:
        case DDS_SOP_VAL_STU:
        case DDS_SOP_VAL_BSQ:
          if (type == DDS_SOP_VAL_ARR)
          {
            print_couple ("jump", idx, ops[arg], "next", "elem");
            printf (" elem-size=%"PRIu32, ops[arg + 1]);
          }
          else
          {
            printf (" elem-size=%"PRIu32, ops[arg]);
            print_couple ("jump", idx, ops[arg + 1], "next", "elem");
          }
          break;
        default:
          break;
      }
      break;
    }
    case DDS_SOP_VAL_UNI:
      printf (" offset=%"PRIu32" cases=%"PRIu32, ops[idx + 1], ops[idx + 2]);
      print_couple ("jump", idx, ops[idx + 3], "next", "cases");
      if (subtype == DDS_SOP_VAL_ENU)
        printf (" max=%"PRIu32, ops[idx + 4]);
      else if (subtype == DDS_SOP_VAL_BMK)
        printf (" bits=0x%08"PRIx32"%08"PRIx32, ops[idx + 4], ops[idx + 5]);
      break;
    case DDS_SOP_VAL_EXT:
      printf (" offset=%"PRIu32, ops[idx + 1]);
      print_couple ("jump", idx, ops[idx + 2], "next", "elem");
      if (insn & (DDS_OP_FLAG_EXT | DDS_OP_FLAG_OPT))
        printf (" elem-size=%"PRIu32, ops[idx + 3]);
      break;
    case DDS_SOP_VAL_STU:
      break;
  }
}

static void print_decoded_op (const uint32_t *ops, uint32_t nops, uint32_t idx, uint32_t width, bool is_default_case)
{
  const uint32_t insn = ops[idx];
  const uint32_t op = insn & DDS_OP_MASK;
  printf (" %s", opcode_name (op));
  switch (op)
  {
    case DDS_SOP_RTS:
    case DDS_SOP_PLC:
      break;
    case DDS_SOP_DLC: {
      const uint16_t required_prefix = DDS_OP_DLC_REQUIRED_PREFIX (insn);
      if (required_prefix != 0)
        printf (" required-prefix=%"PRIu16, required_prefix);
      break;
    }
    case DDS_SOP_ADR:
      print_adr (ops, idx);
      break;
    case DDS_SOP_JSR: {
      const int16_t rel = DDS_OP_JUMP (insn);
      printf (" rel=");
      print_rel_offset (rel);
      print_abs_target (idx, rel);
      break;
    }
    case DDS_SOP_JEQ:
    case DDS_SOP_JEQ4: {
      const enum dds_stream_typecode type = DDS_JEQ_TYPE (insn);
      printf ("/%s", typecode_name (type));
      if (is_default_case)
        printf (" default");
      print_common_flags (insn, false);
      if (DDS_OP_JUMP (insn) != 0)
      {
        printf (" case-insn=");
        print_rel_offset (DDS_OP_JUMP (insn));
        print_abs_target (idx, DDS_OP_JUMP (insn));
      }
      print_disc_value (ops[idx + 1]);
      printf (" offset=%"PRIu32, ops[idx + 2]);
      if (op == DDS_SOP_JEQ4)
        printf (" extra=%"PRIu32, ops[idx + 3]);
      break;
    }
    case DDS_SOP_PLM:
    {
      const int16_t rel = DDS_OP_ADR_PLM (insn);
      print_plm_flags (insn);
      printf (" elem-insn=");
      print_rel_offset (rel);
      if (rel != 0)
        print_abs_target (idx, rel);
      printf (" member-id=%"PRIu32, ops[idx + 1]);
      break;
    }
    case DDS_SOP_KOF:
      printf (" n=%"PRIu32" offsets=[", DDS_OP_LENGTH (insn));
      for (uint32_t n = 1; n < width; n++)
        printf ("%s%"PRIu32, n == 1 ? "" : ",", ops[idx + n]);
      printf ("]");
      break;
    case DDS_SOP_MID:
      printf (" op-index=%"PRIu32" member-id=%"PRIu32, DDS_OP_LENGTH (insn), ops[idx + 1]);
      break;
    case DDS_OP_EVS:
      printf (" set-id=%"PRIu32" default=0x%08"PRIx32"(%"PRId32") nvalues=%"PRIu32" values=[",
          DDS_OP_LENGTH (insn), ops[idx + 1], (int32_t) ops[idx + 1], ops[idx + 2]);
      for (uint32_t n = 0; n < ops[idx + 2] && idx + 3 + n < nops; n++)
        printf ("%s0x%08"PRIx32, n == 0 ? "" : ",", ops[idx + 3 + n]);
      printf ("]");
      break;
    case DDS_OP_EVM:
      printf (" op-index=%"PRIu32" set-id=%"PRIu32, DDS_OP_LENGTH (insn), ops[idx + 1]);
      break;
  }
}

static void print_ops (const struct dds_cdrstream_desc *desc)
{
  const uint32_t *ops = desc->ops.ops;
  uint32_t idx = 0;
  while (idx < desc->ops.nops)
  {
    const uint32_t expected = op_width (ops, desc->ops.nops, idx);
    const uint32_t available = (expected <= desc->ops.nops - idx) ? expected : desc->ops.nops - idx;
    printf ("  %04"PRIu32":", idx);
    for (uint32_t n = 0; n < available; n++)
      printf (" %08"PRIx32, ops[idx + n]);
    if (available < expected)
    {
      printf (" <truncated: expected %"PRIu32" words>\n", expected);
      break;
    }
    print_decoded_op (ops, desc->ops.nops, idx, expected, is_union_default_case (ops, desc->ops.nops, idx));
    printf ("\n");
    idx += expected;
  }
}

void dtl_print_cdrstream_descriptor (const struct dds_cdrstream_desc *desc)
{
  printf ("size=%"PRIu32" align=%"PRIu32" nkeys=%"PRIu32" nops=%"PRIu32" ",
      desc->size, desc->align, desc->keys.nkeys, desc->ops.nops);
  print_topic_flags (desc->flagset);
  printf ("\n");
  if (desc->keys.nkeys != 0)
  {
    printf ("keys:\n");
    for (uint32_t n = 0; n < desc->keys.nkeys; n++)
    {
      printf ("  [%"PRIu32"] op-offset=%"PRIu32" index=%"PRIu32"\n",
          n, desc->keys.keys[n].ops_offs, desc->keys.keys[n].idx);
    }
  }
  printf ("ops:\n");
  print_ops (desc);
}
