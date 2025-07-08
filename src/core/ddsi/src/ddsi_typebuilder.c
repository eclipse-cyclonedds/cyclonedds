// Copyright(c) 2022 ZettaScale Technology
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <stdlib.h>
#include "dds/features.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "dds/ddsi/ddsi_typebuilder.h"
#include "ddsi__xt_impl.h"
#include "ddsi__list_tmpl.h"
#include "ddsi__typelib.h"
#include "dds/cdr/dds_cdrstream.h"

#define OPS_CHUNK_SZ 100u
#define XCDR1_MAX_ALIGN 8
#define XCDR2_MAX_ALIGN 4
#define STRUCT_BASE_MEMBER_NAME "parent"
#define KEY_NAME_SEP "."

#define _PUSH(fn,val) \
  if ((ret = fn (ops, (val))) != DDS_RETCODE_OK) \
    return ret;
#define PUSH_OP(val) _PUSH(push_op,val)
#define PUSH_ARG(val) _PUSH(push_op_arg,val)
#define OR_OP(idx,val) or_op(ops, (idx), (val))
#define SET_OP(idx,val) \
  if ((ret = set_op (ops, (idx), (val))) != DDS_RETCODE_OK) \
    return ret;

struct typebuilder_ops
{
  uint32_t *ops;
  uint32_t index;
  uint32_t maximum;
  uint32_t n_ops;
};

struct typebuilder_type;
struct typebuilder_aggregated_type;
struct typebuilder_struct;

struct typebuilder_aggregated_type_ref
{
  struct typebuilder_aggregated_type *type;
  uint32_t ref_insn;
  uint32_t ref_base;
};

struct typebuilder_type_ref
{
  struct typebuilder_type *type;
};

struct typebuilder_type
{
  enum dds_stream_typecode type_code;
  uint32_t size;
  uint32_t align;
  union {
    struct {
      bool is_signed;
      bool is_fp;
    } prim_args;
    struct {
      uint32_t bound;
      uint32_t elem_sz;
      uint32_t elem_align;
      struct typebuilder_type_ref element_type;
    } collection_args;
    struct {
      uint32_t bit_bound;
      uint32_t max;
    } enum_args;
    struct {
      uint32_t bit_bound;
      uint32_t bits_h;
      uint32_t bits_l;
    } bitmask_args;
    struct {
      uint32_t max_size;
    } string_args;
    struct
    {
      struct typebuilder_aggregated_type_ref external_type;
    } external_type_args;
  } args;
};

struct typebuilder_struct_member
{
  struct typebuilder_type type;
  struct typebuilder_aggregated_type *parent;
  char *member_name;
  uint32_t member_index;            // index of member in the struct
  uint32_t member_id;               // id assigned by annotation or hash id
  uint32_t member_offset;           // in-memory offset of the member in its parent struct
  uint32_t insn_offs;               // offset of the ADR instruction for the member within its parent aggregated type
  uint32_t plm_insn_offs;           // offset of the PLM instruction, if applicable
  bool is_key;
  bool is_must_understand;
  bool is_external;
  bool is_optional;
};

struct typebuilder_struct
{
  uint32_t n_members;
  struct typebuilder_struct_member *members;
};

struct typebuilder_union_member
{
  struct typebuilder_type type;
  uint32_t disc_value;
  bool is_external;
  bool is_default;
  bool is_last_label;
};

struct typebuilder_union
{
  struct typebuilder_type disc_type;
  uint32_t disc_size;
  bool disc_is_key;
  uint32_t member_offs;
  uint32_t n_cases;
  struct typebuilder_union_member *cases;
};

struct typebuilder_aggregated_type
{
  char *type_name;
  ddsi_typeid_t id;
  struct typebuilder_type *base_type;
  uint16_t extensibility;     // DDS_XTypes_IS_FINAL / DDS_XTypes_IS_APPENDABLE / DDS_XTypes_IS_MUTABLE
  DDS_XTypes_TypeKind kind;   // DDS_XTypes_TK_STRUCTURE / DDS_XTypes_TK_UNION
  uint32_t size;              // size of this aggregated type, including padding at the end
  uint32_t align;             // max alignment for this aggregated type
  uint32_t insn_offs;         // index in ops array of first instruction for this type
  bool has_explicit_key;      // has the @key annotation set on one or more members
  union {
    struct typebuilder_struct _struct;
    struct typebuilder_union _union;
  } detail;
};

typedef enum key_path_part_kind {
  KEY_PATH_PART_REGULAR,
  KEY_PATH_PART_INHERIT,
  KEY_PATH_PART_INHERIT_MUTABLE
} key_path_part_kind_t;

struct typebuilder_key_path_part {
  key_path_part_kind_t kind;
  const struct typebuilder_struct_member *member;
};

struct typebuilder_key_path {
  uint32_t n_parts;
  struct typebuilder_key_path_part *parts;
  size_t name_len;
};

struct typebuilder_key
{
  uint32_t key_index;  // index of this key field in definition order;
  uint32_t kof_idx;    // key offset in type ops
  struct typebuilder_key_path *path;
};

#define NOARG
DDSI_LIST_TYPES_TMPL(typebuilder_dep_types, struct typebuilder_aggregated_type *, NOARG, 32)
#undef NOARG

struct typebuilder_data
{
  struct ddsi_domaingv *gv;
  const struct ddsi_type *type;
  struct typebuilder_aggregated_type toplevel_type;
  struct typebuilder_dep_types dep_types;
  uint32_t n_keys;
  struct typebuilder_key *keys;
  bool fixed_size;
};

DDSI_LIST_DECLS_TMPL(static, typebuilder_dep_types, struct typebuilder_aggregated_type *, ddsrt_attribute_unused)
DDSI_LIST_CODE_TMPL(static, typebuilder_dep_types, struct typebuilder_aggregated_type *, NULL, ddsrt_malloc, ddsrt_free)

static dds_return_t typebuilder_add_aggrtype (struct typebuilder_data *tbd, struct typebuilder_aggregated_type *tb_aggrtype, const struct ddsi_type *type);
static dds_return_t typebuilder_add_type (struct typebuilder_data *tbd, uint32_t *size, uint32_t *align, struct typebuilder_type *tb_type, const struct ddsi_type *type, bool is_ext, bool use_ext_type);
static dds_return_t resolve_ops_offsets_aggrtype (const struct typebuilder_aggregated_type *tb_aggrtype, struct typebuilder_ops *ops);
static dds_return_t get_keys_aggrtype (struct typebuilder_data *tbd, struct typebuilder_key_path *path, const struct typebuilder_aggregated_type *tb_aggrtype, bool parent_key);
static dds_return_t set_implicit_keys_aggrtype (struct typebuilder_aggregated_type *tb_aggrtype, bool is_toplevel, bool parent_is_key);

static struct typebuilder_data *typebuilder_data_new (struct ddsi_domaingv *gv, const struct ddsi_type *type) ddsrt_nonnull_all;
static struct typebuilder_data *typebuilder_data_new (struct ddsi_domaingv *gv, const struct ddsi_type *type)
{
  struct typebuilder_data *tbd;
  if (!(tbd = ddsrt_calloc (1, sizeof (*tbd))))
    return NULL;
  tbd->gv = gv;
  tbd->type = type;
  typebuilder_dep_types_init (&tbd->dep_types);
  tbd->fixed_size = true;
  return tbd;
}

static void typebuilder_type_fini (struct typebuilder_type *tb_type)
{
  switch (tb_type->type_code)
  {
    case DDS_OP_VAL_ARR:
    case DDS_OP_VAL_BSQ:
    case DDS_OP_VAL_SEQ:
      if (tb_type->args.collection_args.element_type.type)
      {
        typebuilder_type_fini (tb_type->args.collection_args.element_type.type);
        ddsrt_free (tb_type->args.collection_args.element_type.type);
      }
      break;
    default:
      break;
  }
}

static void typebuilder_struct_fini (struct typebuilder_struct *tb_struct)
{
  if (tb_struct->members)
  {
    for (uint32_t n = 0; n < tb_struct->n_members; n++)
    {
      ddsrt_free (tb_struct->members[n].member_name);
      typebuilder_type_fini (&tb_struct->members[n].type);
    }
    ddsrt_free (tb_struct->members);
  }
}

static void typebuilder_union_fini (struct typebuilder_union *tb_union)
{
  if (tb_union->cases)
  {
    for (uint32_t n = 0; n < tb_union->n_cases; n++)
      typebuilder_type_fini (&tb_union->cases[n].type);
    ddsrt_free (tb_union->cases);
  }
}

static void typebuilder_aggrtype_fini (struct typebuilder_aggregated_type *tb_aggrtype)
{
  ddsrt_free (tb_aggrtype->type_name);
  if (tb_aggrtype->base_type)
  {
    typebuilder_type_fini (tb_aggrtype->base_type);
    ddsrt_free (tb_aggrtype->base_type);
  }
  switch (tb_aggrtype->kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
      typebuilder_struct_fini (&tb_aggrtype->detail._struct);
      break;
    case DDS_XTypes_TK_UNION:
      typebuilder_union_fini (&tb_aggrtype->detail._union);
      break;
  }
}

static void typebuilder_ops_fini (struct typebuilder_ops *ops)
{
  ddsrt_free (ops->ops);
}

static void typebuilder_data_free (struct typebuilder_data *tbd)
{
  if (!ddsi_typeid_is_none (&tbd->toplevel_type.id))
    typebuilder_aggrtype_fini (&tbd->toplevel_type);

  struct typebuilder_dep_types_iter_d it;
  for (struct typebuilder_aggregated_type *tb_aggrtype = typebuilder_dep_types_iter_d_first (&tbd->dep_types, &it); tb_aggrtype; tb_aggrtype = typebuilder_dep_types_iter_d_next (&it))
  {
    typebuilder_aggrtype_fini (tb_aggrtype);
    typebuilder_dep_types_iter_d_remove (&it);
    ddsrt_free (tb_aggrtype);
  }
  typebuilder_dep_types_free (&tbd->dep_types);

  for (uint32_t n = 0; n < tbd->n_keys; n++)
  {
    assert (tbd->keys[n].path && tbd->keys[n].path->parts && tbd->keys[n].path->n_parts);
    ddsrt_free (tbd->keys[n].path->parts);
    ddsrt_free (tbd->keys[n].path);
  }
  ddsrt_free (tbd->keys);

  ddsrt_free (tbd);
}

static uint16_t get_extensibility (DDS_XTypes_TypeFlag flags)
{
  if (flags & DDS_XTypes_IS_MUTABLE)
    return DDS_XTypes_IS_MUTABLE;
  if (flags & DDS_XTypes_IS_APPENDABLE)
    return DDS_XTypes_IS_APPENDABLE;
  assert (flags & DDS_XTypes_IS_FINAL);
  return DDS_XTypes_IS_FINAL;
}

static uint32_t get_bitbound_flags (uint32_t bit_bound)
{
  uint32_t flags = 0;
  if (bit_bound > 32)
    flags |= 3 << DDS_OP_FLAG_SZ_SHIFT;
  else if (bit_bound > 16)
    flags |= 2 << DDS_OP_FLAG_SZ_SHIFT;
  else if (bit_bound > 8)
    flags |= 1 << DDS_OP_FLAG_SZ_SHIFT;
  return flags;
}

static void align_to (uint32_t *offs, uint32_t align)
{
  *offs = (*offs + align - 1) & ~(align - 1);
}

static struct typebuilder_aggregated_type *typebuilder_find_aggrtype (struct typebuilder_data *tbd, const struct ddsi_type *type)
{
  struct typebuilder_aggregated_type *tb_aggrtype = NULL;

  if (!ddsi_typeid_compare (&type->xt.id, &tbd->toplevel_type.id))
    tb_aggrtype = &tbd->toplevel_type;
  else
  {
    struct typebuilder_dep_types_iter it;
    for (tb_aggrtype = typebuilder_dep_types_iter_first (&tbd->dep_types, &it); tb_aggrtype; tb_aggrtype = typebuilder_dep_types_iter_next (&it))
    {
      if (!ddsi_typeid_compare (&type->xt.id, &tb_aggrtype->id))
        break;
    }
  }

  return tb_aggrtype;
}

#define ALGN(type,ext) (uint32_t) ((ext) ? dds_alignof (void *) : dds_alignof (type))
#define SZ(type,ext) (uint32_t) ((ext) ? sizeof (type *) : sizeof (type))

static const struct ddsi_type *type_unalias (const struct ddsi_type *t)
{
  return t->xt._d == DDS_XTypes_TK_ALIAS ? type_unalias (t->xt._u.alias.related_type) : t;
}

static dds_return_t typebuilder_add_type (struct typebuilder_data *tbd, uint32_t *size, uint32_t *align, struct typebuilder_type *tb_type, const struct ddsi_type *type, bool is_ext, bool use_ext_type)
{
  assert (tbd);
  dds_return_t ret = DDS_RETCODE_OK;
  switch (type->xt._d)
  {
    case DDS_XTypes_TK_BOOLEAN:
      tb_type->type_code = DDS_OP_VAL_BLN;
      *align = ALGN (uint8_t, is_ext);
      *size = SZ (uint8_t, is_ext);
      break;
    case DDS_XTypes_TK_INT8:
    case DDS_XTypes_TK_UINT8:
    case DDS_XTypes_TK_CHAR8:
    case DDS_XTypes_TK_BYTE:
      tb_type->type_code = DDS_OP_VAL_1BY;
      tb_type->args.prim_args.is_signed = (type->xt._d == DDS_XTypes_TK_CHAR8 || type->xt._d == DDS_XTypes_TK_INT8);
      *align = ALGN (uint8_t, is_ext);
      *size = SZ (uint8_t, is_ext);
      break;
    case DDS_XTypes_TK_INT16:
    case DDS_XTypes_TK_UINT16:
      tb_type->type_code = DDS_OP_VAL_2BY;
      tb_type->args.prim_args.is_signed = (type->xt._d == DDS_XTypes_TK_INT16);
      *align = ALGN (uint16_t, is_ext);
      *size = SZ (uint16_t, is_ext);
      break;
    case DDS_XTypes_TK_CHAR16:
      tb_type->type_code = DDS_OP_VAL_WCHAR;
      tb_type->args.prim_args.is_signed = false;
      *align = ALGN (uint16_t, is_ext);
      *size = SZ (uint16_t, is_ext);
      break;
    case DDS_XTypes_TK_INT32:
    case DDS_XTypes_TK_UINT32:
    case DDS_XTypes_TK_FLOAT32:
      tb_type->type_code = DDS_OP_VAL_4BY;
      tb_type->args.prim_args.is_signed = (type->xt._d == DDS_XTypes_TK_INT32);
      tb_type->args.prim_args.is_fp = (type->xt._d == DDS_XTypes_TK_FLOAT32);
      *align = type->xt._d == DDS_XTypes_TK_FLOAT32 ? ALGN (float, is_ext) : ALGN (uint32_t, is_ext);
      *size = SZ (uint32_t, is_ext);
      break;
    case DDS_XTypes_TK_INT64:
    case DDS_XTypes_TK_UINT64:
    case DDS_XTypes_TK_FLOAT64:
      tb_type->type_code = DDS_OP_VAL_8BY;
      tb_type->args.prim_args.is_signed = (type->xt._d == DDS_XTypes_TK_INT64);
      tb_type->args.prim_args.is_fp = (type->xt._d == DDS_XTypes_TK_FLOAT64);
      *align = type->xt._d == DDS_XTypes_TK_FLOAT64 ? ALGN (double, is_ext) : ALGN (uint64_t, is_ext);
      *size = SZ (uint64_t, is_ext);
      break;
    case DDS_XTypes_TK_STRING8: {
      bool bounded = (type->xt._u.str8.bound > 0);
      tb_type->type_code = bounded ? DDS_OP_VAL_BST : DDS_OP_VAL_STR;
      tb_type->args.string_args.max_size = type->xt._u.str8.bound + 1; // +1 for terminating '\0'
      *align = ALGN (uint8_t, !bounded || is_ext);
      if (bounded && !is_ext)
        *size = tb_type->args.string_args.max_size * (uint32_t) sizeof (char);
      else
        *size = sizeof (char *);
      tbd->fixed_size = false;
      break;
    }
    case DDS_XTypes_TK_STRING16: {
      bool bounded = (type->xt._u.str16.bound > 0);
      tb_type->type_code = bounded ? DDS_OP_VAL_BWSTR : DDS_OP_VAL_WSTR;
      tb_type->args.string_args.max_size = type->xt._u.str16.bound + 1; // +1 for terminating L'\0'
      *align = ALGN (wchar_t, !bounded || is_ext);
      if (bounded && !is_ext)
        *size = tb_type->args.string_args.max_size * (uint32_t) sizeof (wchar_t);
      else
        *size = sizeof (wchar_t *);
      tbd->fixed_size = false;
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      uint32_t max = 0;
      for (uint32_t n = 0; n < type->xt._u.enum_type.literals.length; n++)
      {
        assert (type->xt._u.enum_type.literals.seq[n].value >= 0);
        if ((uint32_t) type->xt._u.enum_type.literals.seq[n].value > max)
          max = (uint32_t) type->xt._u.enum_type.literals.seq[n].value;
      }
      tb_type->type_code = DDS_OP_VAL_ENU;
      tb_type->args.enum_args.max = max;
      tb_type->args.enum_args.bit_bound = type->xt._u.enum_type.bit_bound;
      *align = ALGN (uint32_t, is_ext);
      *size = SZ (uint32_t, is_ext);
      break;
    }
    case DDS_XTypes_TK_BITMASK: {
      uint64_t bits = 0;
      for (uint32_t n = 0; n < type->xt._u.bitmask.bitflags.length; n++)
        bits |= 1llu << type->xt._u.bitmask.bitflags.seq[n].position;
      tb_type->type_code = DDS_OP_VAL_BMK;
      tb_type->args.bitmask_args.bits_l = (uint32_t) (bits & 0xffffffffu);
      tb_type->args.bitmask_args.bits_h = (uint32_t) (bits >> 32);
      tb_type->args.bitmask_args.bit_bound = type->xt._u.bitmask.bit_bound;
      if (type->xt._u.bitmask.bit_bound > 32)
      {
        *align = ALGN (uint64_t, is_ext);
        *size = SZ (uint64_t, is_ext);
      }
      else if (type->xt._u.bitmask.bit_bound > 16)
      {
        *align = ALGN (uint32_t, is_ext);
        *size = SZ (uint32_t, is_ext);
      }
      else if (type->xt._u.bitmask.bit_bound > 8)
      {
        *align = ALGN (uint16_t, is_ext);
        *size = SZ (uint16_t, is_ext);
      }
      else
      {
        *align = ALGN (uint8_t, is_ext);
        *size = SZ (uint8_t, is_ext);
      }
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      bool bounded = type->xt._u.seq.bound > 0;
      tb_type->type_code = bounded ? DDS_OP_VAL_BSQ : DDS_OP_VAL_SEQ;
      if (bounded)
        tb_type->args.collection_args.bound = type->xt._u.seq.bound;
      if (!(tb_type->args.collection_args.element_type.type = ddsrt_calloc (1, sizeof (*tb_type->args.collection_args.element_type.type))))
      {
        ret = DDS_RETCODE_OUT_OF_RESOURCES;
        goto err;
      }
      if ((ret = typebuilder_add_type (tbd, &tb_type->args.collection_args.elem_sz,
              &tb_type->args.collection_args.elem_align,
              tb_type->args.collection_args.element_type.type, type->xt._u.seq.c.element_type, false, false)) != DDS_RETCODE_OK)
      {
        goto err;
      }
      *align = ALGN (dds_sequence_t, is_ext);
      *size = SZ (dds_sequence_t, is_ext);
      tbd->fixed_size = false;
      break;
    }
    case DDS_XTypes_TK_ARRAY: {
      uint32_t bound = 1;
      for (uint32_t n = 0; n < type->xt._u.array.bounds._length; n++)
        bound *= type->xt._u.array.bounds._buffer[n];

      const struct ddsi_type *el_type = type_unalias (type->xt._u.array.c.element_type);
      while (el_type->xt._d == DDS_XTypes_TK_ARRAY)
      {
        for (uint32_t n = 0; n < el_type->xt._u.array.bounds._length; n++)
          bound *= el_type->xt._u.array.bounds._buffer[n];
        el_type = type_unalias (el_type->xt._u.array.c.element_type);
      }

      tb_type->type_code = DDS_OP_VAL_ARR;
      tb_type->args.collection_args.bound = bound;
      if (!(tb_type->args.collection_args.element_type.type = ddsrt_calloc (1, sizeof (*tb_type->args.collection_args.element_type.type))))
      {
        ret = DDS_RETCODE_OUT_OF_RESOURCES;
        goto err;
      }
      if ((ret = typebuilder_add_type (tbd, &tb_type->args.collection_args.elem_sz,
              &tb_type->args.collection_args.elem_align,
              tb_type->args.collection_args.element_type.type, el_type, false, false)) != DDS_RETCODE_OK)
      {
        goto err;
      }
      *align = is_ext ? dds_alignof (void *) : tb_type->args.collection_args.elem_align;
      *size = is_ext ? sizeof (void *) : bound * tb_type->args.collection_args.elem_sz;
      break;
    }
    case DDS_XTypes_TK_ALIAS:
      if ((ret = typebuilder_add_type (tbd, size, align, tb_type, type->xt._u.alias.related_type, is_ext, use_ext_type)) != DDS_RETCODE_OK)
        goto err;
      break;
    case DDS_XTypes_TK_STRUCTURE:
    case DDS_XTypes_TK_UNION: {
      if (use_ext_type)
        tb_type->type_code = DDS_OP_VAL_EXT;
      else
        tb_type->type_code = type->xt._d == DDS_XTypes_TK_STRUCTURE ? DDS_OP_VAL_STU : DDS_OP_VAL_UNI;

      struct typebuilder_aggregated_type *aggrtype;
      if ((aggrtype = typebuilder_find_aggrtype (tbd, type)) == NULL)
      {
        if (!(aggrtype = ddsrt_calloc (1, sizeof (*aggrtype))))
        {
          ret = DDS_RETCODE_OUT_OF_RESOURCES;
          goto err;
        }
        typebuilder_dep_types_append (&tbd->dep_types, aggrtype);
        if ((ret = typebuilder_add_aggrtype (tbd, aggrtype, type)) != DDS_RETCODE_OK)
          return ret;
      }
      tb_type->args.external_type_args.external_type.type = aggrtype;
      *align = is_ext ? dds_alignof (void *) : aggrtype->align;
      *size = is_ext ? sizeof (void *) : aggrtype->size;
      break;
    }
    case DDS_XTypes_TK_FLOAT128:
    case DDS_XTypes_TK_ANNOTATION:
    case DDS_XTypes_TK_MAP:
    case DDS_XTypes_TK_BITSET:
      ret = DDS_RETCODE_UNSUPPORTED;
      break;
    case DDS_XTypes_TK_NONE:
      ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }

  tb_type->align = *align;
  tb_type->size = *size;

err:
  return ret;
}
#undef SZ
#undef ALGN

static bool supported_key_type (const struct typebuilder_type *tb_type)
{
  if (tb_type->type_code == DDS_OP_VAL_EXT || tb_type->type_code == DDS_OP_VAL_STU || tb_type->type_code == DDS_OP_VAL_STR || tb_type->type_code == DDS_OP_VAL_BST)
    return true;
  if (tb_type->type_code <= DDS_OP_VAL_8BY || tb_type->type_code == DDS_OP_VAL_BLN || tb_type->type_code == DDS_OP_VAL_ENU || tb_type->type_code == DDS_OP_VAL_BMK)
    return true;
  if (tb_type->type_code == DDS_OP_VAL_ARR || tb_type->type_code == DDS_OP_VAL_SEQ || tb_type->type_code == DDS_OP_VAL_BSQ)
     return supported_key_type (tb_type->args.collection_args.element_type.type);
  return false;
}

static dds_return_t typebuilder_add_struct (struct typebuilder_data *tbd, struct typebuilder_aggregated_type *tb_aggrtype, const struct ddsi_type *type)
{
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t offs = 0, sz, align;

  assert (tbd);
  if (!(tb_aggrtype->type_name = ddsrt_strdup (type->xt._u.structure.detail.type_name)))
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }
  tb_aggrtype->extensibility = get_extensibility (type->xt._u.structure.flags);

  if (type->xt._u.structure.base_type)
  {
    if (!(tb_aggrtype->base_type = ddsrt_calloc (1, sizeof (*tb_aggrtype->base_type))))
    {
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto err;
    }
    if ((ret = typebuilder_add_type (tbd, &sz, &align, tb_aggrtype->base_type, type->xt._u.structure.base_type, false, true)) != DDS_RETCODE_OK)
    {
      goto err;
    }

    // add member size and align of base type members to current aggrtype
    struct typebuilder_aggregated_type *base = tb_aggrtype;
    while (base->base_type && (base = base->base_type->args.external_type_args.external_type.type))
    {
      for (uint32_t n = 0; n < base->detail._struct.n_members; n++)
      {
        struct typebuilder_struct_member *member = &base->detail._struct.members[n];
        if (member->type.align > tb_aggrtype->align)
          tb_aggrtype->align = member->type.align;

        align_to (&offs, member->type.align);
        assert (member->type.size <= UINT32_MAX - offs);
        offs += member->type.size;
      }
    }
  }

  // add padding for base type (in-memory represented as a nested struct)
  align_to (&offs, tb_aggrtype->align);

  tb_aggrtype->detail._struct.n_members = type->xt._u.structure.members.length;
  if (!(tb_aggrtype->detail._struct.members = ddsrt_calloc (tb_aggrtype->detail._struct.n_members, sizeof (*tb_aggrtype->detail._struct.members))))
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }

  for (uint32_t n = 0; n < type->xt._u.structure.members.length; n++)
  {
    bool is_ext = type->xt._u.structure.members.seq[n].flags & DDS_XTypes_IS_EXTERNAL;
    bool is_key = type->xt._u.structure.members.seq[n].flags & DDS_XTypes_IS_KEY;
    bool is_mu = type->xt._u.structure.members.seq[n].flags & DDS_XTypes_IS_MUST_UNDERSTAND;
    bool is_opt = type->xt._u.structure.members.seq[n].flags & DDS_XTypes_IS_OPTIONAL;
    if (is_key)
      tb_aggrtype->has_explicit_key = true;
    tb_aggrtype->detail._struct.members[n] = (struct typebuilder_struct_member) {
      .member_name = ddsrt_strdup (type->xt._u.structure.members.seq[n].detail.name),
      .member_index = n,
      .member_id = type->xt._u.structure.members.seq[n].id,
      .is_external = is_ext,
      .is_key = is_key,
      .is_must_understand = is_mu,
      .is_optional = is_opt,
      .parent = tb_aggrtype
    };
    if (tb_aggrtype->detail._struct.members[n].member_name == NULL)
    {
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto err;
    }

    if ((ret = typebuilder_add_type (tbd, &sz, &align, &tb_aggrtype->detail._struct.members[n].type, type->xt._u.structure.members.seq[n].type, is_ext || is_opt, true)) != DDS_RETCODE_OK)
      goto err;

    if (is_key && !supported_key_type (&tb_aggrtype->detail._struct.members[n].type))
    {
      ret = DDS_RETCODE_UNSUPPORTED;
      goto err;
    }

    if (align > tb_aggrtype->align)
      tb_aggrtype->align = align;

    align_to (&offs, align);
    tb_aggrtype->detail._struct.members[n].member_offset = offs;
    assert (sz <= UINT32_MAX - offs);
    offs += sz;
  }

  // add padding at end of struct
  align_to (&offs, tb_aggrtype->align);
  tb_aggrtype->size = offs;
err:
  return ret;
}

static dds_return_t typebuilder_add_union (struct typebuilder_data *tbd, struct typebuilder_aggregated_type *tb_aggrtype, const struct ddsi_type *type)
{
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t disc_sz, disc_align, member_sz = 0, member_align = 0;

  assert (tbd);
  if (!(tb_aggrtype->type_name = ddsrt_strdup (type->xt._u.union_type.detail.type_name)))
    return DDS_RETCODE_OUT_OF_RESOURCES;
  tb_aggrtype->extensibility = get_extensibility (type->xt._u.union_type.flags);

  if ((ret = typebuilder_add_type (tbd, &disc_sz, &disc_align, &tb_aggrtype->detail._union.disc_type, type->xt._u.union_type.disc_type, false, false)) != DDS_RETCODE_OK)
    goto err;
  tb_aggrtype->detail._union.disc_size = disc_sz;
  tb_aggrtype->detail._union.disc_is_key = type->xt._u.union_type.disc_flags & DDS_XTypes_IS_KEY;
  // TODO: support for union (discriminator) as part of a type's key
  if (tb_aggrtype->detail._union.disc_is_key)
  {
    ret = DDS_RETCODE_UNSUPPORTED;
    goto err;
  }

  uint32_t n_cases = 0;
  for (uint32_t n = 0; n < type->xt._u.union_type.members.length; n++)
  {
    n_cases += type->xt._u.union_type.members.seq[n].label_seq._length;
    if (type->xt._u.union_type.members.seq[n].flags & DDS_XTypes_IS_DEFAULT)
      n_cases++;
  }

  tb_aggrtype->detail._union.n_cases = n_cases;
  if (!(tb_aggrtype->detail._union.cases = ddsrt_calloc (n_cases, sizeof (*tb_aggrtype->detail._union.cases))))
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }
  for (uint32_t n = 0, c = 0; n < type->xt._u.union_type.members.length; n++)
  {
    uint32_t sz = 0, align = 0;
    bool is_ext = type->xt._u.union_type.members.seq[n].flags & DDS_XTypes_IS_EXTERNAL;
    bool is_default = type->xt._u.union_type.members.seq[n].flags & DDS_XTypes_IS_DEFAULT;
    for (uint32_t l = 0; l < type->xt._u.union_type.members.seq[n].label_seq._length; l++)
    {
      bool is_last = !is_default && (l == type->xt._u.union_type.members.seq[n].label_seq._length - 1);
      tb_aggrtype->detail._union.cases[c].is_external = is_ext;
      tb_aggrtype->detail._union.cases[c].is_last_label = is_last;
      tb_aggrtype->detail._union.cases[c].disc_value = (uint32_t) type->xt._u.union_type.members.seq[n].label_seq._buffer[l];
      if ((ret = typebuilder_add_type (tbd, &sz, &align, &tb_aggrtype->detail._union.cases[c].type, type->xt._u.union_type.members.seq[n].type, is_ext, false)) != DDS_RETCODE_OK)
        goto err;
      c++;
    }
    if (is_default)
    {
      tb_aggrtype->detail._union.cases[c].is_external = is_ext;
      tb_aggrtype->detail._union.cases[c].is_default = true;
      tb_aggrtype->detail._union.cases[c].is_last_label = true;
      tb_aggrtype->detail._union.cases[c].disc_value = 0;
      if ((ret = typebuilder_add_type (tbd, &sz, &align, &tb_aggrtype->detail._union.cases[c].type, type->xt._u.union_type.members.seq[n].type, is_ext, false)) != DDS_RETCODE_OK)
        goto err;
      c++;
    }
    if (align > member_align)
      member_align = align;
    if (sz > member_sz)
      member_sz = sz;
  }

  // union size (size of c struct that has discriminator and c union)
  tb_aggrtype->size = disc_sz;
  align_to (&tb_aggrtype->size, member_align);
  tb_aggrtype->size += member_sz;

  // padding at end of union
  uint32_t max_align = member_align > disc_align ? member_align : disc_align;
  tb_aggrtype->align = max_align;
  align_to (&tb_aggrtype->size, max_align);

  // offset for union members
  tb_aggrtype->detail._union.member_offs = disc_sz;
  align_to (&tb_aggrtype->detail._union.member_offs, member_align);

err:
  return ret;
}

static dds_return_t typebuilder_add_aggrtype (struct typebuilder_data *tbd, struct typebuilder_aggregated_type *tb_aggrtype, const struct ddsi_type *type)
{
  assert (tbd);
  dds_return_t ret = DDS_RETCODE_OK;
  ddsi_typeid_copy (&tb_aggrtype->id, &type->xt.id);
  tb_aggrtype->kind = type->xt._d;
  switch (type->xt._d)
  {
    case DDS_XTypes_TK_STRUCTURE:
      ret = typebuilder_add_struct (tbd, tb_aggrtype, type);
      break;
    case DDS_XTypes_TK_UNION:
      ret = typebuilder_add_union (tbd, tb_aggrtype, type);
      break;
    default:
      ret = DDS_RETCODE_BAD_PARAMETER;
      break;
  }
  return ret;
}

static dds_return_t push_op_impl (struct typebuilder_ops *ops, uint32_t op, uint32_t index)
{
  assert (ops);
  while (index >= ops->maximum)
  {
    assert (UINT16_MAX - OPS_CHUNK_SZ > ops->maximum);
    ops->maximum += OPS_CHUNK_SZ;
    uint32_t *tmp = ddsrt_realloc (ops->ops, sizeof (*tmp) * ops->maximum);
    if (!tmp)
    {
      typebuilder_ops_fini (ops);
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
    ops->ops = tmp;
  }
  ops->ops[index] = op;
  ops->n_ops++;
  return DDS_RETCODE_OK;
}

static dds_return_t set_op (struct typebuilder_ops *ops, uint32_t index, uint32_t op)
{
  return push_op_impl (ops, op, index);
}

static dds_return_t push_op (struct typebuilder_ops *ops, uint32_t op)
{
  return push_op_impl (ops, op, ops->index++);
}

static dds_return_t push_op_arg (struct typebuilder_ops *ops, uint32_t op)
{
  return push_op_impl (ops, op, ops->index++);
}

static void or_op (struct typebuilder_ops *ops, uint32_t index, uint32_t value)
{
  assert (ops);
  assert (index <= ops->index);
  ops->ops[index] |= value;
}

static uint32_t get_type_flags (const struct typebuilder_type *tb_type)
{
  uint32_t flags = 0;
  switch (tb_type->type_code)
  {
    case DDS_OP_VAL_1BY:
    case DDS_OP_VAL_2BY:
    case DDS_OP_VAL_4BY:
    case DDS_OP_VAL_8BY:
      flags |= tb_type->args.prim_args.is_fp ? DDS_OP_FLAG_FP : 0u;
      flags |= tb_type->args.prim_args.is_signed ? DDS_OP_FLAG_SGN : 0u;
      break;
    case DDS_OP_VAL_ENU:
      flags |= get_bitbound_flags (tb_type->args.enum_args.bit_bound);
      break;
    case DDS_OP_VAL_BMK:
      flags |= get_bitbound_flags (tb_type->args.bitmask_args.bit_bound);
      break;
    default:
      break;
  }
  return flags;
}

static dds_return_t get_ops_type (struct typebuilder_type *tb_type, uint32_t flags, uint32_t member_offset, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_OK;
  switch (tb_type->type_code)
  {
    case DDS_OP_VAL_1BY:
    case DDS_OP_VAL_2BY:
    case DDS_OP_VAL_4BY:
    case DDS_OP_VAL_8BY:
      flags |= get_type_flags (tb_type);
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) ((DDS_OP_VAL_1BY + (tb_type->type_code - DDS_OP_VAL_1BY)) << 16) | flags);
      PUSH_ARG (member_offset);
      break;
    case DDS_OP_VAL_BLN:
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_BLN | flags);
      PUSH_ARG (member_offset);
      break;
    case DDS_OP_VAL_ENU:
      flags |= get_type_flags (tb_type);
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_ENU | flags);
      PUSH_ARG (member_offset);
      PUSH_ARG (tb_type->args.enum_args.max);
      break;
    case DDS_OP_VAL_BMK:
      flags |= get_type_flags (tb_type);
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_BMK | flags);
      PUSH_ARG (member_offset);
      PUSH_ARG (tb_type->args.bitmask_args.bits_h);
      PUSH_ARG (tb_type->args.bitmask_args.bits_l);
      break;
    case DDS_OP_VAL_STR:
      flags &= ~DDS_OP_FLAG_EXT;
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_STR | flags);
      PUSH_ARG (member_offset);
      break;
    case DDS_OP_VAL_WSTR:
      flags &= ~DDS_OP_FLAG_EXT;
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_WSTR | flags);
      PUSH_ARG (member_offset);
      break;
    case DDS_OP_VAL_BST:
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_BST | flags);
      PUSH_ARG (member_offset);
      PUSH_ARG (tb_type->args.string_args.max_size);
      break;
    case DDS_OP_VAL_BWSTR:
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_BWSTR | flags);
      PUSH_ARG (member_offset);
      PUSH_ARG (tb_type->args.string_args.max_size);
      break;
    case DDS_OP_VAL_WCHAR:
      flags |= get_type_flags (tb_type);
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_WCHAR | flags);
      PUSH_ARG (member_offset);
      break;
    case DDS_OP_VAL_BSQ:
    case DDS_OP_VAL_SEQ: {
      bool bounded = tb_type->type_code == DDS_OP_VAL_BSQ;
      struct typebuilder_type *element_type = tb_type->args.collection_args.element_type.type;
      assert (element_type);
      flags |= get_type_flags (element_type);
      uint32_t adr_index = ops->index;
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) (bounded ? DDS_OP_TYPE_BSQ : DDS_OP_TYPE_SEQ) | (element_type->type_code << 8u) | flags);
      PUSH_ARG (member_offset);
      if (bounded)
        PUSH_ARG (tb_type->args.collection_args.bound);
      switch (element_type->type_code)
      {
        case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR:
          break;
        case DDS_OP_VAL_ENU:
          PUSH_ARG (element_type->args.enum_args.max);
          break;
        case DDS_OP_VAL_BMK:
          PUSH_ARG (element_type->args.bitmask_args.bits_h);
          PUSH_ARG (element_type->args.bitmask_args.bits_l);
          break;
        case DDS_OP_VAL_BST:
        case DDS_OP_VAL_BWSTR:
          PUSH_ARG (element_type->args.string_args.max_size);
          break;
        case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI:
          element_type->args.external_type_args.external_type.ref_base = adr_index;
          PUSH_ARG (element_type->args.external_type_args.external_type.type->size);
          element_type->args.external_type_args.external_type.ref_insn = ops->index;
          PUSH_ARG ((4 + (bounded ? 1u : 0u)) << 16u);  // set next_insn, elem_insn is set after emitting external type
          break;
        case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BSQ: {
          PUSH_ARG (tb_type->args.collection_args.elem_sz);
          uint32_t next_insn_idx = ops->index;
          PUSH_ARG (4 + (bounded ? 1u : 0u));  // set elem_insn, next_insn is set after element
          if ((ret = get_ops_type (element_type, flags & (DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU), 0u, ops)) != DDS_RETCODE_OK)
            goto err;
          PUSH_OP (DDS_OP_RTS);
          OR_OP (next_insn_idx, (uint32_t) (ops->index - adr_index) << 16u);
          break;
        }
        case DDS_OP_VAL_EXT:
          ret = DDS_RETCODE_UNSUPPORTED;
          goto err;
      }
      break;
    }
    case DDS_OP_VAL_EXT: {
      bool ext = flags & DDS_OP_FLAG_EXT;
      tb_type->args.external_type_args.external_type.ref_base = ops->index;
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_EXT | flags);
      PUSH_ARG (member_offset);
      tb_type->args.external_type_args.external_type.ref_insn = ops->index;
      PUSH_ARG ((3 + (ext ? 1u : 0u)) << 16u);  // set next_insn, elem_insn is set after emitting external type
      if (ext)
        PUSH_ARG (tb_type->args.external_type_args.external_type.type->size);
      break;
    }
    case DDS_OP_VAL_ARR: {
      struct typebuilder_type *element_type = tb_type->args.collection_args.element_type.type;
      assert (element_type);
      flags |= get_type_flags (element_type);
      uint32_t adr_index = ops->index;
      PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_ARR | (element_type->type_code << 8u) | flags);
      PUSH_ARG (member_offset);
      PUSH_ARG (tb_type->args.collection_args.bound);
      switch (element_type->type_code)
      {
        case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR:
          break;
        case DDS_OP_VAL_ENU:
          PUSH_ARG (element_type->args.enum_args.max);
          break;
        case DDS_OP_VAL_BMK:
          PUSH_ARG (element_type->args.bitmask_args.bits_h);
          PUSH_ARG (element_type->args.bitmask_args.bits_l);
          break;
        case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR:
          PUSH_ARG (0);
          PUSH_ARG (element_type->args.string_args.max_size);
          break;
        case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI:
          element_type->args.external_type_args.external_type.ref_base = adr_index;
          element_type->args.external_type_args.external_type.ref_insn = ops->index;
          PUSH_ARG (5 << 16);  // set next_insn, elem_insn is set after emitting external type
          PUSH_ARG (element_type->args.external_type_args.external_type.type->size);
          break;
        case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BSQ: {
          uint32_t next_insn_idx = ops->index;
          PUSH_ARG (5);  // set elem_insn, next_insn is set after element
          PUSH_ARG (tb_type->args.collection_args.elem_sz);
          if ((ret = get_ops_type (element_type, flags & (DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU), 0u, ops)) != DDS_RETCODE_OK)
            goto err;
          PUSH_OP (DDS_OP_RTS);
          OR_OP (next_insn_idx, (uint32_t) (ops->index - adr_index) << 16u);
          break;
        }
        case DDS_OP_VAL_EXT:
          ret = DDS_RETCODE_UNSUPPORTED;
          goto err;
      }
      break;
    }
    case DDS_OP_VAL_UNI:
    case DDS_OP_VAL_STU:
      ret = DDS_RETCODE_UNSUPPORTED;
      break;
  }
err:
  return ret;
}

static bool aggrtype_has_key (struct typebuilder_aggregated_type *tb_aggrtype)
{
  return tb_aggrtype && (tb_aggrtype->has_explicit_key || (tb_aggrtype->base_type && aggrtype_has_key (tb_aggrtype->base_type->args.external_type_args.external_type.type)));
}

static dds_return_t get_ops_struct (const struct typebuilder_struct *tb_struct, struct typebuilder_type *tb_base_type, uint16_t extensibility, uint32_t parent_insn_offs, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (extensibility == DDS_XTypes_IS_MUTABLE)
  {
    PUSH_OP (DDS_OP_PLC);
  }
  else if (extensibility == DDS_XTypes_IS_APPENDABLE)
  {
    PUSH_OP (DDS_OP_DLC);
  }
  else
    assert (extensibility == DDS_XTypes_IS_FINAL);

  if (extensibility == DDS_XTypes_IS_MUTABLE)
  {
    if (tb_base_type)
    {
      PUSH_ARG (DDS_OP_PLM | (DDS_OP_FLAG_BASE << 16));  // offset of basetype is added after adding members
      PUSH_ARG (0);
    }
    for (uint32_t m = 0; m < tb_struct->n_members; m++)
    {
      assert (parent_insn_offs < ops->index);
      tb_struct->members[m].plm_insn_offs = ops->index - parent_insn_offs;
      PUSH_ARG (DDS_OP_PLM);  // offset to ADR instruction is added after adding members
      PUSH_ARG (tb_struct->members[m].member_id);
    }
    PUSH_OP (DDS_OP_RTS);
  }
  else if (tb_base_type)
  {
    uint32_t flags = DDS_OP_FLAG_BASE | (aggrtype_has_key (tb_base_type->args.external_type_args.external_type.type) ? DDS_OP_FLAG_KEY : 0u);
    if ((ret = get_ops_type (tb_base_type, flags, 0, ops)) != DDS_RETCODE_OK)
      return ret;
  }

  for (uint32_t m = 0; m < tb_struct->n_members; m++)
  {
    uint32_t flags = 0u;
    flags |= tb_struct->members[m].is_external ? DDS_OP_FLAG_EXT : 0u;
    flags |= tb_struct->members[m].is_optional ? (DDS_OP_FLAG_OPT | DDS_OP_FLAG_EXT) : 0u;
    flags |= tb_struct->members[m].is_key ? DDS_OP_FLAG_KEY : 0u;
    flags |= tb_struct->members[m].is_must_understand ? DDS_OP_FLAG_MU : 0u;
    tb_struct->members[m].insn_offs = ops->index - parent_insn_offs;
    if ((ret = get_ops_type (&tb_struct->members[m].type, flags, tb_struct->members[m].member_offset, ops)) != DDS_RETCODE_OK)
      return ret;
    if (extensibility == DDS_XTypes_IS_MUTABLE)
      PUSH_OP (DDS_OP_RTS);
  }

  return ret;
}

static dds_return_t get_ops_union_case (struct typebuilder_type *tb_type, uint32_t flags, uint32_t disc_value, uint32_t offset, bool include_inline_type, uint32_t *inline_types_offs, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_OK;
  switch (tb_type->type_code)
  {
    case DDS_OP_VAL_1BY:
    case DDS_OP_VAL_2BY:
    case DDS_OP_VAL_4BY:
    case DDS_OP_VAL_8BY:
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | (uint32_t) ((DDS_OP_VAL_1BY + (tb_type->type_code - DDS_OP_VAL_1BY)) << 16) | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (0);
      break;
    case DDS_OP_VAL_BLN:
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | (uint32_t) DDS_OP_TYPE_BLN | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (0);
      break;
    case DDS_OP_VAL_ENU:
      flags |= get_type_flags (tb_type);
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | (uint32_t) DDS_OP_TYPE_ENU | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (tb_type->args.enum_args.max);
      break;
    case DDS_OP_VAL_STR:
      flags &= ~DDS_OP_FLAG_EXT;
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | (uint32_t) DDS_OP_TYPE_STR | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (0);
      break;
    case DDS_OP_VAL_WSTR:
      flags &= ~DDS_OP_FLAG_EXT;
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | (uint32_t) DDS_OP_TYPE_WSTR | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (0);
      break;
    case DDS_OP_VAL_WCHAR:
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | (uint32_t) DDS_OP_TYPE_WCHAR | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (0);
      break;
    case DDS_OP_VAL_UNI:
    case DDS_OP_VAL_STU: {
      flags |= get_type_flags (tb_type);
      tb_type->args.external_type_args.external_type.ref_base = ops->index;
      tb_type->args.external_type_args.external_type.ref_insn = ops->index;
      PUSH_OP ((uint32_t) DDS_OP_JEQ4 | ((uint32_t) tb_type->type_code << 16u) | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      PUSH_ARG (flags & DDS_OP_FLAG_EXT ? tb_type->args.external_type_args.external_type.type->size : 0);
      break;
    }
    case DDS_OP_VAL_BMK:
    case DDS_OP_VAL_BST:
    case DDS_OP_VAL_BWSTR:
    case DDS_OP_VAL_BSQ:
    case DDS_OP_VAL_SEQ:
    case DDS_OP_VAL_ARR: {
      uint32_t inst_offs_idx = ops->index;
      /* don't add type flags here, because the offset of the (in-union) type ops
         is included here, which includes the member type flags */
      PUSH_OP (DDS_OP_JEQ4 | (tb_type->type_code << 16u) | flags);
      PUSH_ARG (disc_value);
      PUSH_ARG (offset);
      bool push_elem_sz = flags & DDS_OP_FLAG_EXT && (tb_type->type_code != DDS_OP_VAL_SEQ || tb_type->type_code == DDS_OP_VAL_BSQ || tb_type->type_code == DDS_OP_VAL_ARR);
      PUSH_ARG (push_elem_sz ? tb_type->args.collection_args.elem_sz : 0);

      // set offset to inline type
      assert (inst_offs_idx < *inline_types_offs);
      OR_OP (inst_offs_idx, *inline_types_offs - inst_offs_idx);

      if (include_inline_type)
      {
        // temporarily replace ops->index with index for inline types
        uint32_t ops_idx = ops->index;
        ops->index = *inline_types_offs;
        if ((ret = get_ops_type (tb_type, 0u, 0u, ops)) != DDS_RETCODE_OK)
          return ret;
        *inline_types_offs = ops->index;
        ops->index = ops_idx;
        SET_OP ((*inline_types_offs)++, DDS_OP_RTS);
      }
      break;
    }
    case DDS_OP_VAL_EXT:
      ret = DDS_RETCODE_UNSUPPORTED;
      break;
  }
  return ret;
}

static dds_return_t get_ops_union (const struct typebuilder_union *tb_union, uint16_t extensibility, struct typebuilder_ops *ops)
{
  dds_return_t ret;
  if (extensibility == DDS_XTypes_IS_MUTABLE)
    return DDS_RETCODE_UNSUPPORTED;
  else if (extensibility == DDS_XTypes_IS_APPENDABLE)
  {
    PUSH_OP (DDS_OP_DLC);
  }
  else
    assert (extensibility == DDS_XTypes_IS_FINAL);

  uint32_t flags = DDS_OP_FLAG_MU;
  switch (tb_union->disc_type.type_code)
  {
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      if (tb_union->disc_type.args.prim_args.is_signed)
        flags |= DDS_OP_FLAG_SGN;
      break;
    case DDS_OP_VAL_ENU:
      flags |= get_bitbound_flags (tb_union->disc_type.args.enum_args.bit_bound);
      break;
    case DDS_OP_VAL_BMK:
      flags |= get_bitbound_flags (tb_union->disc_type.args.bitmask_args.bit_bound);
      break;
    default:
      break;
  }
  for (uint32_t c = 0; c < tb_union->n_cases && !(flags & DDS_OP_FLAG_DEF); c++)
    flags |= tb_union->cases[c].is_default ? DDS_OP_FLAG_DEF : 0u;

  uint32_t next_insn_offs = ops->index;
  PUSH_OP ((uint32_t) DDS_OP_ADR | (uint32_t) DDS_OP_TYPE_UNI | (uint32_t) (tb_union->disc_type.type_code << 8) | flags);
  PUSH_ARG (0u);
  PUSH_ARG (tb_union->n_cases);
  uint32_t next_insn_idx = ops->index;
  PUSH_ARG (4u + (tb_union->disc_type.type_code == DDS_OP_VAL_ENU ? 1 : 0));
  if (tb_union->disc_type.type_code == DDS_OP_VAL_ENU)
    PUSH_ARG (tb_union->disc_type.args.enum_args.max);

  uint32_t inline_types_offs = ops->index + (uint32_t) (4u * tb_union->n_cases);
  for (uint32_t c = 0; c < tb_union->n_cases; c++)
  {
    uint32_t case_flags = 0u;
    case_flags |= tb_union->cases[c].is_external ? DDS_OP_FLAG_EXT : 0u;
    if ((ret = get_ops_union_case (&tb_union->cases[c].type, case_flags, tb_union->cases[c].disc_value, tb_union->member_offs, tb_union->cases[c].is_last_label, &inline_types_offs, ops)) != DDS_RETCODE_OK)
      return ret;
  }

  // move ops index forward to end of inline ops
  ops->index = inline_types_offs;
  OR_OP (next_insn_idx, (uint32_t) (ops->index - next_insn_offs) << 16u);
  return ret;
}

static dds_return_t get_ops_aggrtype (struct typebuilder_aggregated_type *tb_aggrtype, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_UNSUPPORTED;
  tb_aggrtype->insn_offs = ops->index;
  switch (tb_aggrtype->kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
      if ((ret = get_ops_struct (&tb_aggrtype->detail._struct, tb_aggrtype->base_type, tb_aggrtype->extensibility, tb_aggrtype->insn_offs, ops)) != DDS_RETCODE_OK)
      {
        typebuilder_ops_fini (ops);
        return ret;
      }
      break;
    case DDS_XTypes_TK_UNION:
      if ((ret = get_ops_union (&tb_aggrtype->detail._union, tb_aggrtype->extensibility, ops)) != DDS_RETCODE_OK)
      {
        typebuilder_ops_fini (ops);
        return ret;
      }
      break;
    default:
      abort ();
  }

  // mutable types have an RTS instruction per member
  if (tb_aggrtype->extensibility != DDS_XTypes_IS_MUTABLE)
    PUSH_OP (DDS_OP_RTS);

  return ret;
}

static dds_return_t typebuilder_get_ops (struct typebuilder_data *tbd, struct typebuilder_ops *ops)
{
  dds_return_t ret;
  if ((ret = get_ops_aggrtype (&tbd->toplevel_type, ops)) != DDS_RETCODE_OK)
    return ret;

  struct typebuilder_dep_types_iter it;
  for (struct typebuilder_aggregated_type *tb_aggrtype = typebuilder_dep_types_iter_first (&tbd->dep_types, &it); !ret && tb_aggrtype; tb_aggrtype = typebuilder_dep_types_iter_next (&it))
    ret = get_ops_aggrtype (tb_aggrtype, ops);

  return ret;
}

static dds_return_t resolve_ops_offsets_type (struct typebuilder_type *tb_type, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t ref_op = 0, offs_base = 0, offs_target = 0;
  bool update_offs = false;
  switch (tb_type->type_code)
  {
    case DDS_OP_VAL_ARR:
    case DDS_OP_VAL_BSQ:
    case DDS_OP_VAL_SEQ: {
      struct typebuilder_type *element_type = tb_type->args.collection_args.element_type.type;
      assert (element_type);
      ret = resolve_ops_offsets_type (element_type, ops);
      break;
    }
    case DDS_OP_VAL_UNI:
    case DDS_OP_VAL_STU:
    case DDS_OP_VAL_EXT:
      ref_op = tb_type->args.external_type_args.external_type.ref_insn;
      offs_base = tb_type->args.external_type_args.external_type.ref_base;
      offs_target = tb_type->args.external_type_args.external_type.type->insn_offs;
      ret = resolve_ops_offsets_aggrtype (tb_type->args.external_type_args.external_type.type, ops);
      update_offs = true;
      break;
    default:
      // no offset updates required for other member types
      break;
  }

  if (update_offs)
  {
    assert (ref_op <= INT16_MAX);
    assert (offs_base <= INT16_MAX);
    assert (offs_target <= INT16_MAX);
    int16_t offs = (int16_t) (offs_target - offs_base);
    OR_OP (ref_op, (uint16_t) offs);
  }

  return ret;
}

static dds_return_t resolve_ops_offsets_struct (const struct typebuilder_struct *tb_struct, struct typebuilder_type *tb_base_type, uint16_t extensibility, uint32_t parent_insn_offs, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (extensibility == DDS_XTypes_IS_MUTABLE)
  {
    if (tb_base_type)
    {
      uint32_t plm_insn_idx = parent_insn_offs + 1;
      assert (DDS_OP (ops->ops[plm_insn_idx]) == DDS_OP_PLM);
      assert (DDS_PLM_FLAGS (ops->ops[plm_insn_idx]) == DDS_OP_FLAG_BASE);
      OR_OP (parent_insn_offs + 1, (uint16_t) (tb_base_type->args.external_type_args.external_type.type->insn_offs - plm_insn_idx));
      struct typebuilder_aggregated_type *base = tb_base_type->args.external_type_args.external_type.type;
      if ((ret = resolve_ops_offsets_struct (&base->detail._struct, base->base_type, base->extensibility, base->insn_offs, ops)) != DDS_RETCODE_OK)
        return ret;
    }
    for (uint32_t m = 0; m < tb_struct->n_members; m++)
    {
      struct typebuilder_struct_member *mem = &tb_struct->members[m];
      uint32_t plm_insn_idx = parent_insn_offs + mem->plm_insn_offs;
      assert (DDS_OP (ops->ops[plm_insn_idx]) == DDS_OP_PLM);
      assert (ops->ops[plm_insn_idx + 1] == mem->member_id);
      OR_OP (plm_insn_idx, (uint16_t) (mem->insn_offs - mem->plm_insn_offs));
    }
  }
  for (uint32_t m = 0; m < tb_struct->n_members; m++)
  {
    if ((ret = resolve_ops_offsets_type (&tb_struct->members[m].type, ops)) != DDS_RETCODE_OK)
      return ret;
  }
  return ret;
}

static dds_return_t resolve_ops_offsets_union (const struct typebuilder_union *tb_union, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t m = 0; m < tb_union->n_cases; m++)
  {
    /* In case its not the last label and the member type is not an aggregated
       type defined outside the current union, the offset to the in-union type
       ops is already set (or not required, for primitive types which are inline
       in the current JEQ4) */
    if (!tb_union->cases[m].is_last_label && tb_union->cases[m].type.type_code != DDS_OP_VAL_STU && tb_union->cases[m].type.type_code != DDS_OP_VAL_UNI)
      continue;
    if ((ret = resolve_ops_offsets_type (&tb_union->cases[m].type, ops)) != DDS_RETCODE_OK)
      return ret;
  }
  return ret;
}

static dds_return_t resolve_ops_offsets_aggrtype (const struct typebuilder_aggregated_type *tb_aggrtype, struct typebuilder_ops *ops)
{
  dds_return_t ret = DDS_RETCODE_UNSUPPORTED;
  if (tb_aggrtype->base_type && tb_aggrtype->extensibility != DDS_XTypes_IS_MUTABLE) // for mutable types, offset to base is set in PLM list item
  {
    if ((ret = resolve_ops_offsets_type (tb_aggrtype->base_type, ops)) != DDS_RETCODE_OK)
      return ret;
  }

  switch (tb_aggrtype->kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
      ret = resolve_ops_offsets_struct (&tb_aggrtype->detail._struct, tb_aggrtype->base_type, tb_aggrtype->extensibility, tb_aggrtype->insn_offs, ops);
      break;
    case DDS_XTypes_TK_UNION:
      ret = resolve_ops_offsets_union (&tb_aggrtype->detail._union, ops);
      break;
    default:
      abort ();
  }
  return ret;
}

static dds_return_t typebuilder_resolve_ops_offsets (const struct typebuilder_data *tbd, struct typebuilder_ops *ops)
{
  return resolve_ops_offsets_aggrtype (&tbd->toplevel_type, ops);
}

static void path_free (struct typebuilder_key_path *path)
{
  ddsrt_free (path->parts);
  ddsrt_free (path);
}

static dds_return_t extend_path (struct typebuilder_key_path **dst, const struct typebuilder_key_path *path, const char *name, const struct typebuilder_struct_member *member, key_path_part_kind_t part_kind)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (!(*dst = ddsrt_calloc (1, sizeof (**dst))))
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }
  (*dst)->n_parts = 1 + (path ? path->n_parts : 0);
  if (!((*dst)->parts = ddsrt_calloc ((*dst)->n_parts, sizeof (*(*dst)->parts))))
  {
    ddsrt_free (*dst);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }
  if (path)
  {
    for (uint32_t n = 0; n < path->n_parts; n++)
    {
      (*dst)->parts[n].kind = path->parts[n].kind;
      (*dst)->parts[n].member = path->parts[n].member;
    }
    (*dst)->name_len = path->name_len;
  }
  if (name)
    (*dst)->name_len += strlen (name) + 1; // +1 for separator (parts 0..n-1) and \0 (part n)
  (*dst)->parts[(*dst)->n_parts - 1].member = member;
  (*dst)->parts[(*dst)->n_parts - 1].kind = part_kind;

err:
  return ret;
}

static dds_return_t get_keys_struct (struct typebuilder_data *tbd, struct typebuilder_key_path *path, const struct typebuilder_struct *tb_struct, bool has_explicit_keys, bool parent_is_key)
{
  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t n = 0; n < tb_struct->n_members; n++)
  {
    struct typebuilder_struct_member *member = &tb_struct->members[n];
    if (member->is_key || (parent_is_key && !has_explicit_keys))
    {
      struct typebuilder_key_path *member_path;
      if ((ret = extend_path (&member_path, path, member->member_name, member, false)) != DDS_RETCODE_OK)
        goto err;

      if (member->type.type_code == DDS_OP_VAL_EXT)
      {
        ret = get_keys_aggrtype (tbd, member_path, member->type.args.external_type_args.external_type.type, true);
        path_free (member_path);
        if (ret != DDS_RETCODE_OK)
          goto err;
      }
      else
      {
        struct typebuilder_key *tmp;
        if (!(tmp = ddsrt_realloc (tbd->keys, (tbd->n_keys + 1) * sizeof (*tbd->keys))))
        {
          path_free (member_path);
          ret = DDS_RETCODE_OUT_OF_RESOURCES;
          goto err;
        }
        tbd->n_keys++;
        tbd->keys = tmp;
        tbd->keys[tbd->n_keys - 1].path = member_path;
      }
    }
  }
err:
  return ret;
}

static dds_return_t get_keys_aggrtype (struct typebuilder_data *tbd, struct typebuilder_key_path *path, const struct typebuilder_aggregated_type *tb_aggrtype, bool parent_is_key)
{
  dds_return_t ret = DDS_RETCODE_UNSUPPORTED;

  if (tb_aggrtype->base_type)
  {
    struct typebuilder_key_path *base_path;
    bool mut = tb_aggrtype->extensibility == DDS_XTypes_IS_MUTABLE;
    if ((ret = extend_path (&base_path, path, mut ? NULL : STRUCT_BASE_MEMBER_NAME, NULL, mut ? KEY_PATH_PART_INHERIT_MUTABLE : KEY_PATH_PART_INHERIT)) != DDS_RETCODE_OK)
      return ret;
    get_keys_aggrtype (tbd, base_path, tb_aggrtype->base_type->args.external_type_args.external_type.type, parent_is_key);
    path_free (base_path);
  }

  switch (tb_aggrtype->kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
      if ((ret = get_keys_struct (tbd, path, &tb_aggrtype->detail._struct, tb_aggrtype->has_explicit_key, parent_is_key)) != DDS_RETCODE_OK)
        return ret;
      break;
    case DDS_XTypes_TK_UNION:
      /* TODO: Support union types as key. The discriminator is the key in that case, and currently
         this is rejected in typebuilder_add_union, so at this point a union has no key attribute set */
      ret = DDS_RETCODE_OK;
      break;
    default:
      abort ();
  }
  return ret;
}

static int key_id_cmp (const void *va, const void *vb)
{
  const struct typebuilder_key * const *a = va;
  const struct typebuilder_key * const *b = vb;

  assert ((*a)->path->n_parts && (*a)->path->parts);
  for (uint32_t n = 0; n < (*a)->path->n_parts; n++)
  {
    assert (n < (*b)->path->n_parts);
    switch ((*a)->path->parts[n].kind)
    {
      case KEY_PATH_PART_INHERIT:
      case KEY_PATH_PART_INHERIT_MUTABLE:
        /* a derived type cannot add keys, so all keys must have an INHERIT_MUTABLE
           kind part at this index */
        assert ((*b)->path->parts[n].kind == (*a)->path->parts[n].kind);
        break;
      case KEY_PATH_PART_REGULAR:
        if ((*a)->path->parts[n].member->member_id != (*b)->path->parts[n].member->member_id)
          return (*a)->path->parts[n].member->member_id < (*b)->path->parts[n].member->member_id ? -1 : 1;
        break;
    }
  }
  assert ((*a)->path->n_parts == (*b)->path->n_parts);
  return 0;
}

static dds_return_t typebuilder_get_keys_push_ops (struct typebuilder_data *tbd, struct typebuilder_ops *ops, struct typebuilder_key ***p_keys_by_id)
{
  dds_return_t ret;
  assert (tbd->n_keys > 0);

  struct typebuilder_key **keys_by_id;
  if (!(keys_by_id = ddsrt_malloc (tbd->n_keys * sizeof (*keys_by_id))))
    return DDS_RETCODE_OUT_OF_RESOURCES;
  *p_keys_by_id = keys_by_id;

  for (uint32_t k = 0; k < tbd->n_keys; k++)
    keys_by_id[k] = &tbd->keys[k];
  qsort ((struct typebuilder_key **) keys_by_id, tbd->n_keys, sizeof (*keys_by_id), key_id_cmp);

  // key ops (sorted by definition order)
  for (uint32_t k = 0; k < tbd->n_keys; k++)
  {
    struct typebuilder_key *key = &tbd->keys[k];
    assert (key->path && key->path->parts && key->path->n_parts);
    key->key_index = k;
    key->kof_idx = ops->index;
    if ((ret = push_op_arg (ops, DDS_OP_KOF)) != DDS_RETCODE_OK)
      goto err;

    uint32_t n_key_offs = 0;
    bool inherit_mutable = false;
    for (uint32_t n = 0; n < key->path->n_parts; n++)
    {
      switch (key->path->parts[n].kind)
      {
        case KEY_PATH_PART_REGULAR:
          if ((ret = push_op_arg (ops, (inherit_mutable ? key->path->parts[n].member->parent->insn_offs : 0u) + key->path->parts[n].member->insn_offs)) != DDS_RETCODE_OK)
            goto err;
          inherit_mutable = false;
          n_key_offs++;
          break;
        case KEY_PATH_PART_INHERIT:
          // we get for @final and @appendable types, for appendable we need to skip the DLC instruction
          if ((ret = push_op_arg (ops, (tbd->toplevel_type.extensibility == DDS_XTypes_IS_FINAL) ? 0u : 1u)) != DDS_RETCODE_OK)
            goto err;
          inherit_mutable = false;
          n_key_offs++;
          break;
        case KEY_PATH_PART_INHERIT_MUTABLE:
          inherit_mutable = true;
          break;
      }
    }
    OR_OP (key->kof_idx, n_key_offs);
  }
  return DDS_RETCODE_OK;

err:
  ddsrt_free (keys_by_id);
  return ret;
}

static char *typebuilder_get_keys_make_name (const struct typebuilder_key *key)
{
  char *name = ddsrt_malloc (key->path->name_len + 1);
  if (!name)
    return NULL;
  size_t name_csr = 0;
  for (uint32_t p = 0; p < key->path->n_parts; p++)
  {
    if (name_csr > 0 && key->path->parts[p].kind != KEY_PATH_PART_INHERIT_MUTABLE)
    {
      (void) ddsrt_strlcpy (name + name_csr, KEY_NAME_SEP, (key->path->name_len + 1) - name_csr);
      name_csr += strlen (KEY_NAME_SEP);
    }
    if (key->path->parts[p].kind == KEY_PATH_PART_INHERIT)
    {
      (void) ddsrt_strlcpy (name + name_csr, STRUCT_BASE_MEMBER_NAME, (key->path->name_len + 1) - name_csr);
      name_csr += strlen (STRUCT_BASE_MEMBER_NAME);
    }
    else if (key->path->parts[p].kind == KEY_PATH_PART_REGULAR)
    {
      (void) ddsrt_strlcpy (name + name_csr, key->path->parts[p].member->member_name, (key->path->name_len + 1) - name_csr);
      name_csr += strlen (key->path->parts[p].member->member_name);
    }
  }
  return name;
}

static dds_return_t typebuilder_get_keys_build_descriptor (const struct typebuilder_data *tbd, struct typebuilder_key **keys_by_id, struct dds_key_descriptor **key_desc)
{
  // build key descriptor list (keys sorted by member id)
  if (!(*key_desc = ddsrt_malloc (tbd->n_keys * sizeof (**key_desc))))
    return DDS_RETCODE_OUT_OF_RESOURCES;
  for (uint32_t k = 0; k < tbd->n_keys; k++)
  {
    struct typebuilder_key const * key = keys_by_id[k];
    (*key_desc)[k] = (struct dds_key_descriptor) {
      .m_name = typebuilder_get_keys_make_name (key),
      .m_offset = key->kof_idx,
      .m_idx = key->key_index
    };
    if (!(*key_desc)[k].m_name)
    {
      for (uint32_t i = 0; i < k; i++)
        ddsrt_free ((char *) (*key_desc)[k].m_name); // cast const away, inherited from ye olden days
      ddsrt_free (*key_desc);
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t typebuilder_get_keys (struct typebuilder_data *tbd, struct typebuilder_ops *ops, struct dds_key_descriptor **key_desc)
{
  dds_return_t ret;
  if ((ret = get_keys_aggrtype (tbd, NULL, &tbd->toplevel_type, false)) != DDS_RETCODE_OK)
    return ret;
  if (tbd->n_keys == 0)
    return ret;

  struct typebuilder_key **keys_by_id;
  if ((ret = typebuilder_get_keys_push_ops (tbd, ops, &keys_by_id)) != DDS_RETCODE_OK)
    return ret;

  // build key descriptor list
  ret = typebuilder_get_keys_build_descriptor (tbd, keys_by_id, key_desc);
  ddsrt_free (keys_by_id);
  return ret;
}

static void set_implicit_keys_collection (struct typebuilder_type *tb_collection, bool is_toplevel, bool parent_is_key)
{
  struct typebuilder_type *element_type = tb_collection->args.collection_args.element_type.type;
  switch (element_type->type_code)
  {
    case DDS_OP_VAL_STU:
      set_implicit_keys_aggrtype (element_type->args.external_type_args.external_type.type, false, parent_is_key);
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR:
      set_implicit_keys_collection (element_type, is_toplevel, parent_is_key);
      break;
    default:
      break;
  }
}

static void set_implicit_keys_struct (struct typebuilder_struct *tb_struct, bool has_explicit_key, bool is_toplevel, bool parent_is_key)
{
  for (uint32_t n = 0; n < tb_struct->n_members; n++)
  {
    if (parent_is_key && !has_explicit_key)
      tb_struct->members[n].is_key = true;

    struct typebuilder_type *tb_type = &tb_struct->members[n].type;
    if (tb_type->type_code == DDS_OP_VAL_EXT)
      set_implicit_keys_aggrtype (tb_type->args.external_type_args.external_type.type, false, (parent_is_key || is_toplevel) && tb_struct->members[n].is_key);
    else if (tb_type->type_code == DDS_OP_VAL_ARR || tb_type->type_code == DDS_OP_VAL_SEQ || tb_type->type_code == DDS_OP_VAL_BSQ)
      set_implicit_keys_collection (tb_type, false, (parent_is_key || is_toplevel) && tb_struct->members[n].is_key);
  }
}

static dds_return_t set_implicit_keys_aggrtype (struct typebuilder_aggregated_type *tb_aggrtype, bool is_toplevel, bool parent_is_key)
{
  dds_return_t ret = DDS_RETCODE_UNSUPPORTED;
  if (tb_aggrtype->base_type)
  {
    if ((ret = set_implicit_keys_aggrtype (tb_aggrtype->base_type->args.external_type_args.external_type.type, is_toplevel, false)) != DDS_RETCODE_OK)
      return ret;
  }

  switch (tb_aggrtype->kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
      set_implicit_keys_struct (&tb_aggrtype->detail._struct, tb_aggrtype->has_explicit_key, is_toplevel, parent_is_key);
      break;
    case DDS_XTypes_TK_UNION:
      // TODO: union discriminator can be implicit key
      ret = DDS_RETCODE_OK;
      break;
    default:
      abort ();
  }
  return ret;
}

static uint32_t get_descriptor_flagset (const struct typebuilder_data *tbd)
{
  uint32_t flags = 0u;
  if (tbd->fixed_size)
    flags |= DDS_TOPIC_FIXED_SIZE;
  flags |= DDS_TOPIC_XTYPES_METADATA;
  /* Flags for key characteristics are calculated in cdrstream */
  return flags;
}


struct visited_aggrtype {
  const struct typebuilder_aggregated_type *aggrtype;
  struct visited_aggrtype *next;
};

static dds_return_t add_memberids_aggrtype (struct typebuilder_data *tbd, struct typebuilder_ops *ops, const struct typebuilder_aggregated_type *tb_aggrtype, struct visited_aggrtype *visited_aggrtypes);
static dds_return_t add_memberids_collection (struct typebuilder_data *tbd, struct typebuilder_ops *ops, const struct typebuilder_type *tb_collection, struct visited_aggrtype *visited_aggrtypes);

static dds_return_t add_memberids_collection (struct typebuilder_data *tbd, struct typebuilder_ops *ops, const struct typebuilder_type *tb_collection, struct visited_aggrtype *visited_aggrtypes)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct typebuilder_type *elem_type = tb_collection->args.collection_args.element_type.type;
  switch (elem_type->type_code)
  {
    case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI:
      if ((ret = add_memberids_aggrtype (tbd, ops, elem_type->args.external_type_args.external_type.type, visited_aggrtypes)) != DDS_RETCODE_OK)
        goto err;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BSQ:
      if ((ret = add_memberids_collection (tbd, ops, elem_type, visited_aggrtypes)) != DDS_RETCODE_OK)
        goto err;
      break;
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR:
    case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BWSTR:
      break;
    case DDS_OP_VAL_EXT:
      abort ();
  }
err:
  return ret;
}

static dds_return_t add_memberids_struct (struct typebuilder_data *tbd, struct typebuilder_ops *ops, const struct typebuilder_struct *tb_struct, struct visited_aggrtype *visited_aggrtypes)
{
  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t n = 0; n < tb_struct->n_members; n++)
  {
    struct typebuilder_struct_member *member = &tb_struct->members[n];
    if (member->is_optional)
    {
      PUSH_OP (DDS_OP_MID);
      PUSH_ARG (member->insn_offs);
    }
    switch (member->type.type_code)
    {
      case  DDS_OP_VAL_EXT:
        if ((ret = add_memberids_aggrtype (tbd, ops, member->type.args.external_type_args.external_type.type, visited_aggrtypes)) != DDS_RETCODE_OK)
          goto err;
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BSQ:
        if ((ret = add_memberids_collection (tbd, ops, &member->type, visited_aggrtypes)) != DDS_RETCODE_OK)
          goto err;
        break;
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR:
      case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BWSTR:
        break;
      case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI:
        abort ();
    }
  }
err:
  return ret;
}

static dds_return_t add_memberids_union (struct typebuilder_data *tbd, struct typebuilder_ops *ops, const struct typebuilder_union *tb_union, struct visited_aggrtype *visited_aggrtypes)
{
  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t n = 0; n < tb_union->n_cases; n++)
  {
    struct typebuilder_union_member *_case = &tb_union->cases[n];
    switch (_case->type.type_code)
    {
      case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI:
        if ((ret = add_memberids_aggrtype (tbd, ops, _case->type.args.external_type_args.external_type.type, visited_aggrtypes)) != DDS_RETCODE_OK)
          goto err;
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_BSQ:
        if ((ret = add_memberids_collection (tbd, ops, &_case->type, visited_aggrtypes)) != DDS_RETCODE_OK)
          goto err;
        break;
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
      case DDS_OP_VAL_BLN: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR:
      case DDS_OP_VAL_WSTR: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_ENU: case DDS_OP_VAL_BWSTR:
        break;
      case DDS_OP_VAL_EXT:
        abort ();
    }
  }
err:
  return ret;
}

static dds_return_t add_memberids_aggrtype (struct typebuilder_data *tbd, struct typebuilder_ops *ops, const struct typebuilder_aggregated_type *tb_aggrtype, struct visited_aggrtype *visited_aggrtypes)
{
  dds_return_t ret = DDS_RETCODE_OK;

  if (tb_aggrtype->extensibility == DDS_XTypes_IS_MUTABLE)
    return DDS_RETCODE_OK;

  struct visited_aggrtype *va = visited_aggrtypes;
  while (true)
  {
    if (va->aggrtype == tb_aggrtype)
      return DDS_RETCODE_OK;
    else if (va->next != NULL)
      va = va->next;
    else
    {
      va->next = ddsrt_calloc (1, sizeof (*va->next));
      va->next->aggrtype = tb_aggrtype;
      break;
    }
  }

  if (tb_aggrtype->base_type)
  {
    if ((ret = add_memberids_aggrtype (tbd, ops, tb_aggrtype->base_type->args.external_type_args.external_type.type, visited_aggrtypes)) != DDS_RETCODE_OK)
      goto err;
  }

  switch (tb_aggrtype->kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
      if ((ret = add_memberids_struct (tbd, ops, &tb_aggrtype->detail._struct, visited_aggrtypes)) != DDS_RETCODE_OK)
        goto err;
      break;
    case DDS_XTypes_TK_UNION:
      if ((ret = add_memberids_union (tbd, ops, &tb_aggrtype->detail._union, visited_aggrtypes)) != DDS_RETCODE_OK)
        goto err;
      break;
    default:
      abort ();
  }

err:
  return ret;
}

static dds_return_t typebuilder_add_mid_table (struct typebuilder_data *tbd, struct typebuilder_ops *ops)
{
  dds_return_t ret;
  uint32_t old_idx = ops->index;
  struct visited_aggrtype visited_aggrtypes = { NULL, NULL };

  if ((ret = add_memberids_aggrtype (tbd, ops, &tbd->toplevel_type, &visited_aggrtypes)) != DDS_RETCODE_OK)
    return ret;

  if (ops->index > old_idx)
    PUSH_OP (DDS_OP_RTS);

  struct visited_aggrtype *va = visited_aggrtypes.next;
  while (va != NULL)
  {
    struct visited_aggrtype *van = va->next;
    ddsrt_free (va);
    va = van;
  }

  return ret;
}


static dds_return_t get_topic_descriptor (dds_topic_descriptor_t *desc, struct typebuilder_data *tbd)
{
  dds_return_t ret;
  unsigned char *typeinfo_data = NULL, *typemap_data = NULL;
  uint32_t typeinfo_sz, typemap_sz;
  struct typebuilder_ops ops = { NULL, 0, 0, 0 };

  if ((ret = ddsi_type_get_typeinfo_ser (tbd->gv, tbd->type, &typeinfo_data, &typeinfo_sz)) != DDS_RETCODE_OK)
    goto err;

  if ((ret = ddsi_type_get_typemap_ser (tbd->gv, tbd->type, &typemap_data, &typemap_sz)) != DDS_RETCODE_OK)
    goto err;

  if ((ret = typebuilder_get_ops (tbd, &ops)) != DDS_RETCODE_OK
      || (ret = typebuilder_resolve_ops_offsets (tbd, &ops)) != DDS_RETCODE_OK)
    goto err;

  struct dds_key_descriptor *key_desc = NULL;
  if ((ret = typebuilder_get_keys (tbd, &ops, &key_desc)) != DDS_RETCODE_OK)
    goto err;

  if ((ret = typebuilder_add_mid_table (tbd, &ops)) != DDS_RETCODE_OK)
    goto err;

  const dds_topic_descriptor_t d =
  {
    .m_size = (uint32_t) tbd->toplevel_type.size,
    .m_align = (uint32_t) tbd->toplevel_type.align,
    .m_flagset = get_descriptor_flagset (tbd),
    .m_typename = ddsrt_strdup (tbd->toplevel_type.type_name),
    .m_nkeys = tbd->n_keys,
    .m_keys = key_desc,
    .m_nops = ops.n_ops,
    .m_ops = ops.ops,
    .m_meta = "",
    .type_information.data = typeinfo_data,
    .type_information.sz = typeinfo_sz,
    .type_mapping.data = typemap_data,
    .type_mapping.sz = typemap_sz,
    .restrict_data_representation = 0
  };
  if (d.m_typename == NULL)
  {
    ddsrt_free ((void *) d.m_typename);
    ddsrt_free ((void *) d.type_information.data);
    ddsrt_free ((void *) d.type_mapping.data);
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }
  // coverity[store_writes_const_field]
  memcpy (desc, &d, sizeof (*desc));
  return DDS_RETCODE_OK;

err:
  typebuilder_ops_fini (&ops);
  ddsrt_free (typeinfo_data);
  ddsrt_free (typemap_data);
  return ret;
}

dds_return_t ddsi_topic_descriptor_from_type (struct ddsi_domaingv *gv, dds_topic_descriptor_t *desc, const struct ddsi_type *type)
{
  assert (gv);
  assert (desc);
  assert (type);

  dds_return_t ret;
  struct typebuilder_data *tbd;
  if (!(tbd = typebuilder_data_new (gv, type)))
    return DDS_RETCODE_OUT_OF_RESOURCES;

  /* Because the top-level type and all its dependencies are resolved, and the caller
     of this function should have a reference to the top-level ddsi_type, we can access
     the type and its dependencies without taking the typelib lock */
  if (!ddsi_type_resolved_locked (tbd->gv, type, DDSI_TYPE_INCLUDE_DEPS)
      || type->xt.kind != DDSI_TYPEID_KIND_COMPLETE)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err;
  }

  const struct ddsi_type * unaliased_type = type_unalias (type);
  if ((ret = typebuilder_add_aggrtype (tbd, &tbd->toplevel_type, unaliased_type)) != DDS_RETCODE_OK)
    goto err;
  set_implicit_keys_aggrtype (&tbd->toplevel_type, true, false);
  if ((ret = get_topic_descriptor (desc, tbd)) != DDS_RETCODE_OK)
    goto err;

err:
  typebuilder_data_free (tbd);
  return ret;
}

void ddsi_topic_descriptor_fini (dds_topic_descriptor_t *desc)
{
  ddsrt_free ((char *) desc->m_typename);
  ddsrt_free ((void *) desc->m_ops);
  if (desc->m_nkeys)
  {
    for (uint32_t n = 0; n < desc->m_nkeys; n++)
      ddsrt_free ((void *) desc->m_keys[n].m_name);
    ddsrt_free ((void *) desc->m_keys);
  }
  ddsrt_free ((void *) desc->type_information.data);
  ddsrt_free ((void *) desc->type_mapping.data);
}
