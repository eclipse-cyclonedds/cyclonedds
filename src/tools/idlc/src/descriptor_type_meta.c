// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "dds/ddsrt/md5.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_opcodes.h"

#include "idl/heap.h"
#include "idl/print.h"
#include "idl/processor.h"
#include "idl/stream.h"
#include "idl/string.h"
#include "idl/misc.h"
#include "generator.h"
#include "descriptor_type_meta.h"

static struct dds_cdrstream_allocator idlc_cdrstream_default_allocator = { idl_malloc, idl_realloc, idl_free };

static idl_retcode_t
push_type (struct descriptor_type_meta *dtm, const void *node)
{
  struct type_meta *tm, *tmp;

  // Check if node exist in admin, if not create new node and add
  tm = dtm->admin;
  while (tm && tm->node != node)
    tm = tm->admin_next;
  if (!tm) {
    tm = idl_calloc (1, sizeof (*tm));
    if (!tm)
      return IDL_RETCODE_NO_MEMORY;
    tm->node = node;
    if (!(tm->ti_minimal = idl_calloc (1, sizeof (*tm->ti_minimal))) ||
        !(tm->to_minimal = idl_calloc (1, sizeof (*tm->to_minimal))) ||
        !(tm->ti_complete = idl_calloc (1, sizeof (*tm->ti_complete))) ||
        !(tm->to_complete = idl_calloc (1, sizeof (*tm->to_complete))))
    {
      if (tm->ti_minimal) idl_free (tm->ti_minimal);
      if (tm->to_minimal) idl_free (tm->to_minimal);
      if (tm->ti_complete) idl_free (tm->ti_complete);
      if (tm->to_complete) idl_free (tm->to_complete);
      idl_free (tm);
      return IDL_RETCODE_NO_MEMORY;
    }

    if (dtm->admin == NULL)
      dtm->admin = tm;
    else {
      tmp = dtm->admin;
      while (tmp->admin_next)
        tmp = tmp->admin_next;
      tmp->admin_next = tm;
    }
  }

  // Add node to stack
  tmp = dtm->stack;
  dtm->stack = tm;
  tm->stack_prev = tmp;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
pop_type (struct descriptor_type_meta *dtm, const void *node)
{
  assert (dtm->stack);
  assert (dtm->stack->node == node);
  (void) node;
  dtm->stack = dtm->stack->stack_prev;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
xcdr2_ser (
  const void *obj,
  const struct dds_cdrstream_desc *desc,
  dds_ostream_t *os)
{
  // serialize as XCDR2 LE
  os->m_buffer = NULL;
  os->m_index = 0;
  os->m_size = 0;
  os->m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;
  dds_return_t ret = dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, &idlc_cdrstream_default_allocator, obj, desc) ? IDL_RETCODE_OK : IDL_RETCODE_BAD_PARAMETER;
  return ret;
}

static idl_retcode_t add_to_seq_generic(dds_sequence_t *seq, const void *elem, size_t elem_size)
{
  assert (seq->_length < UINT32_MAX);
  assert ((seq->_length + 1) <= SIZE_MAX / elem_size);
  unsigned char *buf = idl_realloc (seq->_buffer, (seq->_length + 1) * elem_size);
  if (buf == NULL) {
#if __GNUC__ >= 12
    IDL_WARNING_GNUC_OFF(analyzer-malloc-leak)
    IDL_WARNING_GNUC_OFF(analyzer-double-free)
#endif
    idl_free (seq->_buffer);
    seq->_buffer = NULL;
    seq->_release = false;
    return IDL_RETCODE_NO_MEMORY;
#if __GNUC__ >= 12
    IDL_WARNING_GNUC_ON(analyzer-double-free)
    IDL_WARNING_GNUC_ON(analyzer-malloc-leak)
#endif
  }
  seq->_buffer = buf;
  memcpy (seq->_buffer + seq->_length * elem_size, elem, elem_size);
  seq->_length++;
  seq->_maximum++;
  seq->_release = true;
  return IDL_RETCODE_OK;
}

#define ADD_TO_SEQ_GENERIC(struct_, seq_prefix_, elem_type_, seq_suffix_) \
  static idl_retcode_t \
  add_to_seq_##elem_type_ (struct_ seq_prefix_##elem_type_##seq_suffix_ *seq, const struct_ elem_type_ *obj) \
  { \
    return add_to_seq_generic ((dds_sequence_t *) seq, obj, sizeof (*obj)); \
  }
#define ADD_TO_SEQ(elem_type_) ADD_TO_SEQ_GENERIC(/**/, /**/, elem_type_, Seq)
#define ADD_TO_SEQ_STRUCT(elem_type_) ADD_TO_SEQ_GENERIC(struct, dds_sequence_, elem_type_, /**/)

static bool
has_non_plain_annotation (const idl_type_spec_t *type_spec)
{
  /* XTypes spec section 7.3.4.1: anonymous collection types (array, sequence,
     and map) that have no annotations beyond @external and @try_construct */
  for (idl_annotation_appl_t *a = ((idl_node_t *) type_spec)->annotations; a; a = idl_next (a)) {
    if (strcmp (a->annotation->name->identifier, "external") && strcmp (a->annotation->name->identifier, "try_construct"))
      return true;
  }
  return false;
}

static bool
has_plain_collection_typeid_impl (const idl_type_spec_t *type_spec, bool alias_related_type)
{
  /* An alias declarator can be an array, but can't have a plain-collection type id. An array
     type related type of an alias can be a plain collection type (in that case, type_spec,
     which is the alias declarator, is an array type) */
  if (idl_is_alias (type_spec) && !alias_related_type)
    return false;
  if (idl_is_array (type_spec) || idl_is_sequence (type_spec))
    return !has_non_plain_annotation (type_spec);
  return false;
}

static bool
has_plain_collection_typeid (const idl_type_spec_t *type_spec)
{
  return has_plain_collection_typeid_impl (type_spec, false);
}

static bool
has_fully_descriptive_typeid_impl (const idl_type_spec_t *type_spec, bool array_type_spec, bool alias_related_type)
{
  if (idl_is_alias (type_spec) && !alias_related_type)
    return false;
  if (idl_is_array (type_spec)) {
    if (array_type_spec) {
      type_spec = idl_type_spec (type_spec);
    } else {
      // re-check for the array element type
      return has_fully_descriptive_typeid_impl (type_spec, true, alias_related_type) && !has_non_plain_annotation (type_spec);
    }
  }
  if (idl_is_sequence (type_spec)) {
    idl_type_spec_t *element_type_spec = idl_type_spec (type_spec);
    return has_fully_descriptive_typeid_impl (element_type_spec, false, false)
      && !has_non_plain_annotation (type_spec)
      && !has_non_plain_annotation (element_type_spec);
  }
  if (idl_is_string (type_spec) || idl_is_base_type (type_spec))
    return !has_non_plain_annotation (type_spec);
  return false;
}

static bool
has_fully_descriptive_typeid (const idl_type_spec_t *type_spec)
{
  return has_fully_descriptive_typeid_impl (type_spec, false, false);
}

static idl_retcode_t
get_typeid(const idl_pstate_t *pstate, struct descriptor_type_meta *dtm, const idl_type_spec_t *type_spec, bool alias_related_type, DDS_XTypes_TypeIdentifier *ti, DDS_XTypes_TypeKind kind, bool array_element);

static DDS_XTypes_CollectionElementFlag
get_sequence_element_flags(const idl_sequence_t *seq);

static DDS_XTypes_CollectionElementFlag
get_array_element_flags(const idl_node_t *node);

ADD_TO_SEQ(DDS_XTypes_SBound)
ADD_TO_SEQ(DDS_XTypes_LBound)

static idl_retcode_t
get_plain_typeid (const idl_pstate_t *pstate, struct descriptor_type_meta *dtm, const idl_type_spec_t *type_spec, bool alias_related_type, DDS_XTypes_TypeIdentifier *ti, DDS_XTypes_TypeKind kind)
{
  idl_retcode_t ret;
  assert (ti);
  assert (has_fully_descriptive_typeid_impl (type_spec, false, alias_related_type) || has_plain_collection_typeid_impl (type_spec, alias_related_type));
  (void) alias_related_type;

  if (idl_is_array (type_spec)) {
    const idl_literal_t *literal = ((const idl_declarator_t *) type_spec)->const_expr;
    assert (literal);
    if (idl_array_size (type_spec) <= UINT8_MAX) {
      ti->_d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL;
      for (; literal; literal = idl_next (literal)) {
        assert (literal->value.uint32 < UINT8_MAX);
        uint8_t val = literal->value.uint8;
        if ((ret = add_to_seq_DDS_XTypes_SBound (&ti->_u.array_sdefn.array_bound_seq, &val)) < 0)
          return ret;
      }
      ti->_u.array_sdefn.element_identifier = idl_calloc (1, sizeof (*ti->_u.array_sdefn.element_identifier));
      if ((ret = get_typeid (pstate, dtm, type_spec, false, ti->_u.array_sdefn.element_identifier, kind, true)) < 0)
        return ret;
      ti->_u.array_sdefn.header.element_flags = get_array_element_flags (type_spec);
      if (has_fully_descriptive_typeid_impl (type_spec, true, alias_related_type))
        ti->_u.array_sdefn.header.equiv_kind = DDS_XTypes_EK_BOTH;
      else
        ti->_u.array_sdefn.header.equiv_kind = kind;
    } else {
      ti->_d = DDS_XTypes_TI_PLAIN_ARRAY_LARGE;
      ti->_u.array_ldefn.element_identifier = idl_calloc (1, sizeof (*ti->_u.array_ldefn.element_identifier));
      ti->_u.array_ldefn.header.element_flags = get_array_element_flags (type_spec);
      for (; literal; literal = idl_next (literal)) {
        assert (literal->value.uint64 < UINT32_MAX);
        uint32_t val = literal->value.uint32;
        if ((ret = add_to_seq_DDS_XTypes_LBound (&ti->_u.array_ldefn.array_bound_seq, &val)) < 0)
          return ret;
      }
      if ((ret = get_typeid (pstate, dtm, type_spec, false, ti->_u.array_ldefn.element_identifier, kind, true)) < 0)
        return ret;
      if (has_fully_descriptive_typeid_impl (type_spec, true, alias_related_type))
        ti->_u.array_ldefn.header.equiv_kind = DDS_XTypes_EK_BOTH;
      else
        ti->_u.array_ldefn.header.equiv_kind = kind;
    }
  } else {
    switch (idl_type (type_spec))
    {
      case IDL_BOOL: ti->_d = DDS_XTypes_TK_BOOLEAN; break;
      case IDL_CHAR: ti->_d = DDS_XTypes_TK_CHAR8; break;
      case IDL_OCTET: ti->_d = DDS_XTypes_TK_BYTE; break;
      case IDL_INT8: ti->_d = DDS_XTypes_TK_INT8; break;
      case IDL_INT16: case IDL_SHORT: ti->_d = DDS_XTypes_TK_INT16; break;
      case IDL_INT32: case IDL_LONG: ti->_d = DDS_XTypes_TK_INT32; break;
      case IDL_INT64: case IDL_LLONG: ti->_d = DDS_XTypes_TK_INT64; break;
      case IDL_UINT8: ti->_d = DDS_XTypes_TK_UINT8; break;
      case IDL_UINT16: case IDL_USHORT: ti->_d = DDS_XTypes_TK_UINT16; break;
      case IDL_UINT32: case IDL_ULONG: ti->_d = DDS_XTypes_TK_UINT32; break;
      case IDL_UINT64: case IDL_ULLONG: ti->_d = DDS_XTypes_TK_UINT64; break;
      case IDL_FLOAT: ti->_d = DDS_XTypes_TK_FLOAT32; break;
      case IDL_DOUBLE: ti->_d = DDS_XTypes_TK_FLOAT64; break;
      case IDL_LDOUBLE: ti->_d = DDS_XTypes_TK_FLOAT128; break;
      case IDL_STRING:
      {
        if (!idl_is_bounded(type_spec)) {
          ti->_d = DDS_XTypes_TI_STRING8_SMALL;
          ti->_u.string_sdefn.bound = 0;
        } else if (idl_bound (type_spec) < 256) {
          ti->_d = DDS_XTypes_TI_STRING8_SMALL;
          ti->_u.string_sdefn.bound = (uint8_t) idl_bound (type_spec);
        } else {
          ti->_d = DDS_XTypes_TI_STRING8_LARGE;
          ti->_u.string_ldefn.bound = idl_bound (type_spec);
        }
        break;
      }
      case IDL_SEQUENCE:
      {
        const idl_sequence_t *seq = (const idl_sequence_t *) type_spec;
        if (!idl_is_bounded (seq) || seq->maximum <= UINT8_MAX) {
          ti->_d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL;
          ti->_u.seq_sdefn.bound = (uint8_t) seq->maximum;
          ti->_u.seq_sdefn.header.element_flags = get_sequence_element_flags (seq);
          ti->_u.seq_sdefn.element_identifier = idl_calloc (1, sizeof (*ti->_u.seq_sdefn.element_identifier));
          if ((ret = get_typeid (pstate, dtm, idl_type_spec (type_spec), false, ti->_u.seq_sdefn.element_identifier, kind, false)) < 0)
            return ret;
          if (has_fully_descriptive_typeid (type_spec))
            ti->_u.seq_sdefn.header.equiv_kind = DDS_XTypes_EK_BOTH;
          else
            ti->_u.seq_sdefn.header.equiv_kind = kind;
        } else {
          ti->_d = DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE;
          ti->_u.seq_ldefn.bound = seq->maximum;
          ti->_u.seq_ldefn.header.element_flags = get_sequence_element_flags (seq);
          ti->_u.seq_ldefn.element_identifier = idl_calloc (1, sizeof (*ti->_u.seq_ldefn.element_identifier));
          if ((ret = get_typeid (pstate, dtm, idl_type_spec (type_spec), false, ti->_u.seq_ldefn.element_identifier, kind, false)) < 0)
            return ret;
          if (has_fully_descriptive_typeid (type_spec))
            ti->_u.seq_ldefn.header.equiv_kind = DDS_XTypes_EK_BOTH;
          else
            ti->_u.seq_ldefn.header.equiv_kind = kind;
        }
        break;
      }
      default:
        abort ();
    }
  }
  return IDL_RETCODE_OK;
}

idl_retcode_t
get_type_hash (DDS_XTypes_EquivalenceHash hash, const DDS_XTypes_TypeObject *to)
{
  dds_ostream_t os;
  idl_retcode_t ret;
  if ((ret = xcdr2_ser (to, &DDS_XTypes_TypeObject_cdrstream_desc, &os)) < 0)
    return ret;

  // get md5 of serialized cdr and store first 14 bytes in equivalence hash parameter
  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) os.m_buffer, os.m_index);
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  memcpy (hash, buf, sizeof(DDS_XTypes_EquivalenceHash));
  dds_ostream_fini (&os, &idlc_cdrstream_default_allocator);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
get_hashed_typeid (const idl_pstate_t *pstate, struct descriptor_type_meta *dtm, const idl_type_spec_t *type_spec, DDS_XTypes_TypeIdentifier *ti, DDS_XTypes_TypeKind kind)
{
  idl_retcode_t ret;

  assert (ti);
  assert (!has_fully_descriptive_typeid (type_spec) && !has_plain_collection_typeid (type_spec));

  type_spec = idl_strip (type_spec, IDL_STRIP_FORWARD);

  struct type_meta *tm = dtm->admin;
  while (tm && tm->node != type_spec)
    tm = tm->admin_next;
  if (!tm || !tm->finalized) {
    idl_error (pstate, idl_location (type_spec), "Type id not found for type %s", idl_identifier (type_spec));
    return IDL_RETCODE_BAD_PARAMETER;
  }
  if (kind == DDS_XTypes_EK_COMPLETE) {
    ti->_d = DDS_XTypes_EK_COMPLETE;
    if ((ret = get_type_hash (ti->_u.equivalence_hash, tm->to_complete)) < 0)
      return ret;
  } else {
    assert (kind == DDS_XTypes_EK_MINIMAL);
    ti->_d = DDS_XTypes_EK_MINIMAL;
    if ((ret = get_type_hash (ti->_u.equivalence_hash, tm->to_minimal)) < 0)
      return ret;
  }
  return IDL_RETCODE_OK;
}

static void
get_namehash (DDS_XTypes_NameHash name_hash, const char *name)
{
  /* FIXME: multi byte utf8 chars? */
  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) name, (uint32_t) strlen (name));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  memcpy (name_hash, buf, sizeof (DDS_XTypes_NameHash));
}

static DDS_XTypes_StructTypeFlag
get_struct_flags(const idl_struct_t *_struct)
{
  DDS_XTypes_StructTypeFlag flags = 0u;
  if (_struct->extensibility.value == IDL_MUTABLE)
    flags |= DDS_XTypes_IS_MUTABLE;
  else if (_struct->extensibility.value == IDL_APPENDABLE)
    flags |= DDS_XTypes_IS_APPENDABLE;
  else {
    assert (_struct->extensibility.value == IDL_FINAL);
    flags |= DDS_XTypes_IS_FINAL;
  }
  if (_struct->nested.value)
    flags |= DDS_XTypes_IS_NESTED;
  if (_struct->autoid.value == IDL_HASH)
    flags |= DDS_XTypes_IS_AUTOID_HASH;
  return flags;
}

static DDS_XTypes_StructMemberFlag
get_struct_member_flags(const idl_member_t *member)
{
  DDS_XTypes_StructMemberFlag flags = 0u;
  switch (member->try_construct.value) {
    case IDL_DISCARD:
      flags |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;
      break;
    case IDL_USE_DEFAULT:
      flags |= DDS_XTypes_TRY_CONSTRUCT_USE_DEFAULT;
      break;
    case IDL_TRIM:
      flags |= DDS_XTypes_TRY_CONSTRUCT_TRIM;
      break;
  }

  if (member->external.value)
    flags |= DDS_XTypes_IS_EXTERNAL;
  if (member->key.value)
    flags |= DDS_XTypes_IS_KEY;
  if (member->optional.value)
    flags |= DDS_XTypes_IS_OPTIONAL;
  // XTypes spec 7.2.2.4.4.4.8: Key members shall always have their 'must understand' attribute set to true
  if (member->must_understand.value || member->key.value)
    flags |= DDS_XTypes_IS_MUST_UNDERSTAND;
  return flags;
}

static DDS_XTypes_UnionTypeFlag
get_union_flags(const idl_union_t *_union)
{
  DDS_XTypes_UnionTypeFlag flags = 0u;
  if (_union->extensibility.value == IDL_MUTABLE)
    flags |= DDS_XTypes_IS_MUTABLE;
  else if (_union->extensibility.value == IDL_APPENDABLE)
    flags |= DDS_XTypes_IS_APPENDABLE;
  else {
    assert (_union->extensibility.value == IDL_FINAL);
    flags |= DDS_XTypes_IS_FINAL;
  }
  if (_union->nested.value)
    flags |= DDS_XTypes_IS_NESTED;
  if (_union->autoid.value == IDL_HASH)
    flags |= DDS_XTypes_IS_AUTOID_HASH;
  return flags;
}

static DDS_XTypes_UnionDiscriminatorFlag
get_union_discriminator_flags(const idl_switch_type_spec_t *switch_type_spec)
{
  DDS_XTypes_UnionDiscriminatorFlag flags = 0u;

  // FIXME: support non-default try-construct
  flags |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;

  // XTypes spec 7.2.2.4.4.4.6: In a union type, the discriminator member shall always have the 'must understand' attribute set to true
  flags |= DDS_XTypes_IS_MUST_UNDERSTAND;
  if (switch_type_spec->key.value)
    flags |= DDS_XTypes_IS_KEY;
  // optional and external not allowed
  return flags;
}

static DDS_XTypes_UnionMemberFlag
get_union_case_flags(const idl_case_t *_case)
{
  DDS_XTypes_UnionMemberFlag flags = 0u;
  switch (_case->try_construct.value) {
    case IDL_DISCARD:
      flags |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;
      break;
    case IDL_USE_DEFAULT:
      flags |= DDS_XTypes_TRY_CONSTRUCT_USE_DEFAULT;
      break;
    case IDL_TRIM:
      flags |= DDS_XTypes_TRY_CONSTRUCT_TRIM;
      break;
  }
  if (_case->external.value)
    flags |= DDS_XTypes_IS_EXTERNAL;
  if (idl_is_default_case (_case))
    flags |= DDS_XTypes_IS_DEFAULT;
  // optional, key and must-understand not allowed
  return flags;
}

static DDS_XTypes_CollectionElementFlag
get_sequence_element_flags(const idl_sequence_t *seq)
{
  DDS_XTypes_CollectionElementFlag flags = 0u;

  // FIXME: support non-default try-construct
  flags |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;

  (void) seq;
  // FIXME: support @external for sequence element type
  // if (seq->external)
  //   flags |= DDS_XTypes_IS_EXTERNAL;
  return flags;
}

static DDS_XTypes_CollectionElementFlag
get_array_element_flags(const idl_node_t *node)
{
  DDS_XTypes_CollectionElementFlag flags = 0u;

  // FIXME: support non-default try-construct
  flags |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;

  (void) node;
  // FIXME: support @external for array element type
  // if (arr->external)
  //   flags |= DDS_XTypes_IS_EXTERNAL;
  return flags;
}

static DDS_XTypes_BitmaskTypeFlag
get_bitmask_flags(const idl_bitmask_t *_bitmask)
{
  DDS_XTypes_BitmaskTypeFlag flags = 0u;
  if (_bitmask->extensibility.value == IDL_APPENDABLE)
    flags |= DDS_XTypes_IS_APPENDABLE;
  else {
    assert (_bitmask->extensibility.value == IDL_FINAL);
    flags |= DDS_XTypes_IS_FINAL;
  }
  return flags;
}

static DDS_XTypes_EnumTypeFlag
get_enum_flags(const idl_enum_t *_enum)
{
  DDS_XTypes_EnumTypeFlag flags = 0u;
  if (_enum->extensibility.value == IDL_APPENDABLE)
    flags |= DDS_XTypes_IS_APPENDABLE;
  else {
    assert (_enum->extensibility.value == IDL_FINAL);
    flags |= DDS_XTypes_IS_FINAL;
  }
  return flags;
}

static DDS_XTypes_EnumeratedLiteralFlag
get_enum_literal_flags(const idl_enum_t *_enum, const idl_enumerator_t *_enumerator)
{
  DDS_XTypes_EnumeratedLiteralFlag flags = 0u;
  if (_enum->default_enumerator == _enumerator)
    flags |= DDS_XTypes_IS_DEFAULT;
  return flags;
}

static idl_retcode_t
get_typeid(
  const idl_pstate_t *pstate,
  struct descriptor_type_meta *dtm,
  const idl_type_spec_t *type_spec,
  bool alias_related_type, /* FIXME: refactor so that this parameter is not required */
  DDS_XTypes_TypeIdentifier *ti,
  DDS_XTypes_TypeKind kind,
  bool array_element)
{
  idl_retcode_t ret;

  // array element type is parent of the array type-spec (which is the declarator)
  if (idl_is_array (type_spec) && array_element)
    type_spec = idl_type_spec (type_spec);

  if (has_fully_descriptive_typeid_impl (type_spec, false, alias_related_type) || has_plain_collection_typeid_impl (type_spec, alias_related_type)) {
    if ((ret = get_plain_typeid (pstate, dtm, type_spec, alias_related_type, ti, kind)) < 0)
      return ret;
  } else {
    if ((ret = get_hashed_typeid (pstate, dtm, type_spec, ti, kind)) < 0)
      return ret;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
get_check_type_spec_typeid(
  const idl_pstate_t *pstate,
  struct descriptor_type_meta *dtm,
  const idl_type_spec_t *type_spec,
  bool alias_related_type, /* FIXME: refactor so that this parameter is not required */
  DDS_XTypes_TypeIdentifier *ti,
  DDS_XTypes_TypeKind kind)
{
  idl_retcode_t ret;
  /* Only set the type id in case it is not set yet, to avoid leaking memory
     by overwriting an existing one */
  if (ti->_d == DDS_XTypes_TK_NONE) {
    if ((ret = get_typeid (pstate, dtm, type_spec, alias_related_type, ti, kind, false)) < 0)
      return ret;
  }
#ifndef NDEBUG
  else {
    DDS_XTypes_TypeIdentifier ti_tmp;
    memset (&ti_tmp, 0, sizeof (ti_tmp));
    ret = get_typeid (pstate, dtm, type_spec, alias_related_type, &ti_tmp, kind, false);
    assert (ret == IDL_RETCODE_OK);
    assert (!ddsi_typeid_compare ((struct ddsi_typeid *) ti, (struct ddsi_typeid *) &ti_tmp));
    dds_stream_free_sample ((struct ddsi_typeid *) &ti_tmp, &idlc_cdrstream_default_allocator, DDS_XTypes_TypeIdentifier_desc.m_ops);
  }
#endif
  return IDL_RETCODE_OK;
}

static idl_retcode_t
get_type_spec_typeids(
  const idl_pstate_t *pstate,
  struct descriptor_type_meta *dtm,
  const idl_type_spec_t *type_spec,
  bool alias_related_type, /* FIXME: refactor so that this parameter is not required */
  DDS_XTypes_TypeIdentifier *ti_minimal,
  DDS_XTypes_TypeIdentifier *ti_complete)
{
  idl_retcode_t ret;
  if ((ret = get_check_type_spec_typeid (pstate, dtm, type_spec, alias_related_type, ti_minimal, DDS_XTypes_EK_MINIMAL)) < 0)
    return ret;
  if ((ret = get_check_type_spec_typeid (pstate, dtm, type_spec, alias_related_type, ti_complete, DDS_XTypes_EK_COMPLETE)) < 0)
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
set_xtypes_annotation_parameter_value(
  struct DDS_XTypes_AnnotationParameterValue *val,
  const idl_literal_t *lit)
{
  switch (idl_type(lit)) {
    case IDL_BOOL:
      val->_d = DDS_XTypes_TK_BOOLEAN;
      val->_u.boolean_value = lit->value.bln;
      break;
    case IDL_CHAR:
      val->_d = DDS_XTypes_TK_CHAR8;
      val->_u.char_value = lit->value.chr;
      break;
    case IDL_OCTET:
      val->_d = DDS_XTypes_TK_BYTE;
      val->_u.byte_value = lit->value.uint8;
      break;
    case IDL_INT8:
      val->_d = DDS_XTypes_TK_INT8;
      val->_u.int8_value = lit->value.int8;
      break;
    case IDL_UINT8:
      val->_d = DDS_XTypes_TK_UINT8;
      val->_u.uint8_value = lit->value.uint8;
      break;
    case IDL_SHORT:
    case IDL_INT16:
      val->_d = DDS_XTypes_TK_INT16;
      val->_u.int16_value = lit->value.int16;
      break;
    case IDL_USHORT:
    case IDL_UINT16:
      val->_d = DDS_XTypes_TK_UINT16;
      val->_u.uint_16_value = lit->value.uint16;
      break;
    case IDL_LONG:
    case IDL_INT32:
      val->_d = DDS_XTypes_TK_INT32;
      val->_u.int32_value = lit->value.int32;
      break;
    case IDL_ULONG:
    case IDL_UINT32:
      val->_d = DDS_XTypes_TK_UINT32;
      val->_u.uint32_value = lit->value.uint32;
      break;
    case IDL_LLONG:
    case IDL_INT64:
      val->_d = DDS_XTypes_TK_INT64;
      val->_u.int64_value = lit->value.int64;
      break;
    case IDL_ULLONG:
    case IDL_UINT64:
      val->_d = DDS_XTypes_TK_UINT64;
      val->_u.uint64_value = lit->value.uint64;
      break;
    case IDL_FLOAT:
      val->_d = DDS_XTypes_TK_FLOAT32;
      val->_u.float32_value = lit->value.flt;
      break;
    case IDL_DOUBLE:
      val->_d = DDS_XTypes_TK_FLOAT64;
      val->_u.float64_value = lit->value.dbl;
      break;
    // enums are currently not allowed in min/max/range annotations
    // case IDL_ENUM:
    //   val->_d = DDS_XTypes_TK_ENUM;
    //   val->_u.enumerated_value = ???;
    //   break;
    default:
      return IDL_RETCODE_BAD_PARAMETER;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
get_builtin_member_ann(
  const idl_node_t *node,
  DDS_XTypes_AppliedBuiltinMemberAnnotations **ann_builtin)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  *ann_builtin = NULL;
  bool set = false;
  DDS_XTypes_AppliedBuiltinMemberAnnotations *ptr = idl_calloc (1, sizeof (**ann_builtin));
  if (ptr == NULL) {
    ret = IDL_RETCODE_NO_MEMORY;
    goto cleanup;
  }

  if (idl_is_member (node) || idl_is_case (node)) {
    const idl_member_t *mem = (const idl_member_t*)node;
    const idl_case_t *cs = (const idl_case_t*)node;
    const idl_literal_t *min = idl_is_member(node) ? mem->min.value : cs->min.value,
                        *max = idl_is_member(node) ? mem->max.value : cs->max.value;
    const char *unit = idl_is_member(node) ? mem->unit.value : cs->unit.value;

    for (idl_annotation_appl_t *a = ((idl_node_t *) node)->annotations; a; a = idl_next (a)) {
      if (!strcmp (a->annotation->name->identifier, "hashid")) {
        if (a->parameters) {
          assert (idl_type(a->parameters->const_expr) == IDL_STRING);
          assert (idl_is_literal(a->parameters->const_expr));
          ptr->hash_id = idl_strdup (((const idl_literal_t *)a->parameters->const_expr)->value.str);
        } else {
          ptr->hash_id = idl_strdup ("");
        }
        if (ptr->hash_id == NULL) {
          ret = IDL_RETCODE_NO_MEMORY;
          goto cleanup;
        }
        set = true;
        break;
      }
    }

    if (unit) {
      if (!(ptr->unit = idl_strdup(unit))) {
        ret = IDL_RETCODE_NO_MEMORY;
        goto cleanup;
      }
      set = true;
    }
    if (min) {
      if (!(ptr->min = idl_calloc(1, sizeof(*(ptr->min))))) {
        ret = IDL_RETCODE_NO_MEMORY;
        goto cleanup;
      }
      if ((ret = set_xtypes_annotation_parameter_value(ptr->min, min)))
        goto cleanup;
      set = true;
    }
    if (max) {
      if (!(ptr->max = idl_calloc(1, sizeof(*(ptr->max))))) {
        ret = IDL_RETCODE_NO_MEMORY;
        goto cleanup;
      }
      if ((ret = set_xtypes_annotation_parameter_value(ptr->max, max)))
        goto cleanup;
      set = true;
    }
  }

  if (set) {
    *ann_builtin = ptr;
    return ret;
  }

cleanup:
  if (ptr) {
    if (ptr->hash_id)
      idl_free (ptr->hash_id);
    if (ptr->unit)
      idl_free (ptr->unit);
    if (ptr->min)
      idl_free (ptr->min);
    if (ptr->max)
      idl_free (ptr->max);
    idl_free (ptr);
  }
  return ret;
}

static void
get_builtin_type_ann(
  const idl_node_t *node,
  DDS_XTypes_AppliedBuiltinTypeAnnotations **ann_builtin)
{
  *ann_builtin = NULL;
  (void) node;
  // FIXME: verbatim annotation not supported
}

static void
get_custom_ann(
  const idl_node_t *node,
  DDS_XTypes_AppliedAnnotationSeq **ann_custom)
{
  *ann_custom = NULL;
  (void) node;
  // FIXME: custom annotations not supported
}

static idl_retcode_t
get_complete_type_detail(
  const idl_node_t *node,
  DDS_XTypes_CompleteTypeDetail *detail)
{
  char *type;
  if (IDL_PRINTA(&type, print_scoped_name, node) < 0)
    return IDL_RETCODE_NO_MEMORY;
  idl_strlcpy (detail->type_name, type, sizeof (detail->type_name));

  get_builtin_type_ann (node, &detail->ann_builtin);
  get_custom_ann (node, &detail->ann_custom);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
get_complete_member_detail(
  const idl_node_t *node,
  DDS_XTypes_CompleteMemberDetail *detail)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_strlcpy (detail->name, idl_identifier (node), sizeof (detail->name));
  if ((ret = get_builtin_member_ann (idl_parent (node), &detail->ann_builtin)) != IDL_RETCODE_OK)
    return ret;
  get_custom_ann (node, &detail->ann_custom);
  return ret;
}

ADD_TO_SEQ(DDS_XTypes_MinimalStructMember)
ADD_TO_SEQ(DDS_XTypes_CompleteStructMember)

static idl_retcode_t
add_struct_member (const idl_pstate_t *pstate, struct descriptor_type_meta *dtm, DDS_XTypes_TypeObject *to_minimal, DDS_XTypes_TypeObject *to_complete, const void *node, const idl_type_spec_t *type_spec)
{
  idl_retcode_t ret;

  assert (to_minimal->_u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  assert (to_complete->_u.complete._d == DDS_XTypes_TK_STRUCTURE);
  assert (idl_is_member (idl_parent (node)));

  DDS_XTypes_MinimalStructMember m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteStructMember c;
  memset (&c, 0, sizeof (c));

  const idl_member_t *member = (const idl_member_t *) idl_parent (node);
  const idl_declarator_t *decl = (const idl_declarator_t *) node;
  if ((ret = get_type_spec_typeids (pstate, dtm, type_spec, false, &m.common.member_type_id, &c.common.member_type_id)) < 0)
    return ret;
  m.common.member_id = c.common.member_id = decl->id.value;
  m.common.member_flags = c.common.member_flags = get_struct_member_flags (member);
  get_namehash (m.detail.name_hash, idl_identifier (node));
  if ((ret = get_complete_member_detail (node, &c.detail) < 0))
    return ret;

  if ((ret = add_to_seq_DDS_XTypes_MinimalStructMember (&to_minimal->_u.minimal._u.struct_type.member_seq, &m)) < 0)
    return ret;
  if ((ret = add_to_seq_DDS_XTypes_CompleteStructMember (&to_complete->_u.complete._u.struct_type.member_seq, &c)) < 0)
    return ret;

  return IDL_RETCODE_OK;
}

ADD_TO_SEQ(DDS_XTypes_MinimalUnionMember)
ADD_TO_SEQ(DDS_XTypes_CompleteUnionMember)

static idl_retcode_t
add_union_case(const idl_pstate_t *pstate, struct descriptor_type_meta *dtm, DDS_XTypes_TypeObject *to_minimal, DDS_XTypes_TypeObject *to_complete, const void *node, const idl_type_spec_t *type_spec)
{
  idl_retcode_t ret;

  assert (to_minimal->_u.complete._d == DDS_XTypes_TK_UNION);
  assert (to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
  assert (idl_is_case (idl_parent (node)));

  DDS_XTypes_MinimalUnionMember m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteUnionMember c;
  memset (&c, 0, sizeof (c));

  const idl_case_t *_case = (const idl_case_t *) idl_parent (node);
  if (get_type_spec_typeids (pstate, dtm, type_spec, false, &m.common.type_id, &c.common.type_id) < 0)
    return -1;
  m.common.member_id = c.common.member_id = _case->declarator->id.value;
  m.common.member_flags = c.common.member_flags = get_union_case_flags (_case);
  get_namehash (m.detail.name_hash, idl_identifier (node));
  if ((ret = get_complete_member_detail (node, &c.detail)) < 0)
    return ret;

  /* case labels */
  idl_case_t *case_node = (idl_case_t *) idl_parent (node);
  const idl_case_label_t *cl;
  uint32_t cnt, n;
  static const idl_mask_t mask = IDL_DEFAULT_CASE_LABEL;
  for (cl = case_node->labels, cnt = 0; cl; cl = idl_next (cl)) {
    if ((idl_mask(cl) & mask) != mask)
      cnt++;
  }
  m.common.label_seq._length = c.common.label_seq._length = cnt;
  m.common.label_seq._release = c.common.label_seq._release = true;
  if (cnt) {
    m.common.label_seq._buffer = idl_calloc (cnt, sizeof (*m.common.label_seq._buffer));
    c.common.label_seq._buffer = idl_calloc (cnt, sizeof (*c.common.label_seq._buffer));
    for (cl = case_node->labels, n = 0; cl; cl = idl_next (cl)) {
      if ((idl_mask(cl) & mask) != mask) {
        int64_t val = idl_case_label_intvalue (cl);
        /* A type object has an int32 field for case label value, so the value
          must be in that range when generating xtypes meta-data for a type. */
        if (val < INT32_MIN || val > INT32_MAX) {
          idl_error (pstate, idl_location (cl), "Case label value must be in range INT32_MIN..INT32_MAX (inclusive) when generating type meta-data");
          ret = IDL_RETCODE_OUT_OF_RANGE;
          goto err;
        }
        m.common.label_seq._buffer[n] = c.common.label_seq._buffer[n] = (int32_t) val;
        n++;
      }
    }
  }

  if ((ret = add_to_seq_DDS_XTypes_MinimalUnionMember (&to_minimal->_u.minimal._u.union_type.member_seq, &m)) < 0)
    goto err;
  if ((ret = add_to_seq_DDS_XTypes_CompleteUnionMember (&to_complete->_u.complete._u.union_type.member_seq, &c)) < 0)
    goto err;
  return IDL_RETCODE_OK;

err:
  if (c.detail.ann_builtin)
    idl_free (c.detail.ann_builtin);
  if (c.detail.ann_custom)
    idl_free (c.detail.ann_custom);
  if (m.common.label_seq._buffer)
    idl_free (m.common.label_seq._buffer);
  if (c.common.label_seq._buffer)
    idl_free (c.common.label_seq._buffer);
  if (to_minimal->_u.minimal._u.union_type.member_seq._buffer)
  {
    idl_free (to_minimal->_u.minimal._u.union_type.member_seq._buffer);
    to_minimal->_u.minimal._u.union_type.member_seq._buffer = NULL;
  }
  if (to_complete->_u.complete._u.union_type.member_seq._buffer)
  {
    idl_free (to_complete->_u.complete._u.union_type.member_seq._buffer);
    to_complete->_u.complete._u.union_type.member_seq._buffer = NULL;
  }
  return ret;
}

static idl_retcode_t
emit_hashed_type(
  uint8_t type_kind,
  const void *node,
  bool revisit,
  struct descriptor_type_meta *dtm)
{
  idl_retcode_t ret;
  assert (!has_fully_descriptive_typeid (node) && !has_plain_collection_typeid (node));
  if (revisit) {
    if (!dtm->stack->finalized) {
      if ((ret = get_type_hash (dtm->stack->ti_minimal->_u.equivalence_hash, dtm->stack->to_minimal)) < 0
          || (ret = get_type_hash (dtm->stack->ti_complete->_u.equivalence_hash, dtm->stack->to_complete)) < 0)
        return ret;
      dtm->stack->finalized = true;
    }
    pop_type (dtm, node);
    return IDL_RETCODE_OK;
  } else {
    if ((ret = push_type (dtm, node)) < 0)
      return ret;
    if (!dtm->stack->finalized) {
      dtm->stack->ti_minimal->_d = DDS_XTypes_EK_MINIMAL;
      dtm->stack->to_minimal->_d = DDS_XTypes_EK_MINIMAL;
      dtm->stack->to_minimal->_u.minimal._d = type_kind;
      dtm->stack->ti_complete->_d = DDS_XTypes_EK_COMPLETE;
      dtm->stack->to_complete->_d = DDS_XTypes_EK_COMPLETE;
      dtm->stack->to_complete->_u.complete._d = type_kind;
    }
    return IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
add_typedef (
  const idl_pstate_t *pstate,
  bool revisit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  const idl_type_spec_t *type_spec = idl_is_array (node) ? node : idl_type_spec (node);

  // don't visit fully descriptive type-spec, but visit plain-collection type-spec
  bool visit_type_spec = idl_is_array (node) || !has_fully_descriptive_typeid_impl (type_spec, false, false);

  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_ALIAS);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_ALIAS);
    /* alias_flags and related_flags unused, header empty */
    if ((ret = get_type_spec_typeids (pstate, dtm, type_spec, idl_is_array (node),
          &dtm->stack->to_minimal->_u.minimal._u.alias_type.body.common.related_type,
          &dtm->stack->to_complete->_u.complete._u.alias_type.body.common.related_type)) < 0)
      return ret;
  }

  if ((ret = emit_hashed_type (DDS_XTypes_TK_ALIAS, node, revisit, dtm)) < 0)
    return ret;
  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_DONT_RECURSE | (visit_type_spec ? IDL_VISIT_TYPE_SPEC : 0);

  if (!revisit) {
    if ((ret = get_complete_type_detail (node, &dtm->stack->to_complete->_u.complete._u.alias_type.header.detail)) < 0)
      return ret;
    if ((ret = get_builtin_member_ann (node, &dtm->stack->to_complete->_u.complete._u.alias_type.body.ann_builtin)) < 0)
      return ret;
    return IDL_VISIT_REVISIT | (visit_type_spec ? IDL_VISIT_TYPE_SPEC : 0);
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
add_array (
  const idl_pstate_t *pstate,
  bool revisit,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  const idl_type_spec_t *type_spec = idl_type_spec (node);

  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_ARRAY);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_ARRAY);
    if ((ret = get_type_spec_typeids (pstate, dtm, type_spec, false,
        &dtm->stack->to_minimal->_u.minimal._u.array_type.element.common.type,
        &dtm->stack->to_complete->_u.complete._u.array_type.element.common.type)) < 0)
      return ret;
  }

  if ((ret = emit_hashed_type (DDS_XTypes_TK_ARRAY, node, revisit, dtm)) < 0)
    return ret;
  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_DONT_RECURSE;

  if (!revisit) {
    dtm->stack->to_minimal->_u.minimal._u.array_type.element.common.element_flags =
      dtm->stack->to_complete->_u.complete._u.array_type.element.common.element_flags = get_sequence_element_flags (node);

    if ((ret = get_complete_type_detail (type_spec, &dtm->stack->to_complete->_u.complete._u.array_type.header.detail)) < 0)
      return ret;

    const idl_literal_t *literal = ((const idl_declarator_t *)node)->const_expr;
    if (!literal)
      return 0u;
    for (; literal; literal = idl_next (literal)) {
      if ((ret = add_to_seq_DDS_XTypes_LBound (&dtm->stack->to_minimal->_u.minimal._u.array_type.header.common.bound_seq,
          &literal->value.uint32)) < 0)
        return ret;
      if ((ret = add_to_seq_DDS_XTypes_LBound (&dtm->stack->to_complete->_u.complete._u.array_type.header.common.bound_seq,
          &literal->value.uint32)) < 0)
        return ret;
    }
    return IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta * dtm = (struct descriptor_type_meta *) user_data;
  const idl_struct_t * _struct = (const idl_struct_t *) node;
  idl_retcode_t ret;

  (void) pstate;
  (void) path;
  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_STRUCTURE);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_STRUCTURE);
    if (_struct->inherit_spec) {
      if ((ret = get_type_spec_typeids (pstate, dtm, _struct->inherit_spec->base, false, &dtm->stack->to_minimal->_u.minimal._u.struct_type.header.base_type, &dtm->stack->to_complete->_u.complete._u.struct_type.header.base_type)) < 0)
        return ret;
    }
    if ((ret = get_complete_type_detail (node, &dtm->stack->to_complete->_u.complete._u.struct_type.header.detail)) < 0)
      return ret;

    /* xtypes spec 7.3.4.5: The elements in CompleteStructMemberSeq shall be ordered in increasing
       values of the member_index, so no need to do any custom ordering here because the members
       are visited in the same order */
  }

  if ((ret = emit_hashed_type (DDS_XTypes_TK_STRUCTURE, node, revisit, dtm)) < 0)
    return ret;
  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_DONT_RECURSE | (node == dtm->root ? IDL_VISIT_DONT_ITERATE : 0);

  if (!revisit) {
    dtm->stack->to_minimal->_u.minimal._u.struct_type.struct_flags =
      dtm->stack->to_complete->_u.complete._u.struct_type.struct_flags = get_struct_flags (_struct);
    /* For a topic, only its top-level type should be visited, not the other (non-related) types in the idl */
    return IDL_VISIT_REVISIT | (node == dtm->root ? IDL_VISIT_DONT_ITERATE : 0);
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta * dtm = (struct descriptor_type_meta *) user_data;
  idl_retcode_t ret;

  (void) pstate;
  (void) path;
  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_UNION);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
  }
  if ((ret = emit_hashed_type (DDS_XTypes_TK_UNION, node, revisit, dtm)) < 0)
    return ret;

  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_DONT_RECURSE | (node == dtm->root ? IDL_VISIT_DONT_ITERATE : 0);

  if (!revisit) {
    dtm->stack->to_minimal->_u.minimal._u.union_type.union_flags =
      dtm->stack->to_complete->_u.complete._u.union_type.union_flags = get_union_flags ((const idl_union_t *) node);
    if ((ret = get_complete_type_detail (node, &dtm->stack->to_complete->_u.complete._u.union_type.header.detail)) < 0)
      return ret;
    /* For a topic, only its top-level type should be visited, not the other (non-related) types in the idl */
    return IDL_VISIT_REVISIT | (node == dtm->root ? IDL_VISIT_DONT_ITERATE : 0);
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_switch_type_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) revisit;
  (void) path;

  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;

  assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
  assert (idl_is_union (idl_parent (node)));
  const idl_union_t *_union = (const idl_union_t *) idl_parent (node);
  idl_type_spec_t *switch_type_spec = idl_type_spec (_union->switch_type_spec);

  if (revisit) {
    DDS_XTypes_CommonDiscriminatorMember
      *m_cdm = &tm->to_minimal->_u.minimal._u.union_type.discriminator.common,
      *c_cdm = &tm->to_complete->_u.complete._u.union_type.discriminator.common;
    m_cdm->member_flags = c_cdm->member_flags = get_union_discriminator_flags (_union->switch_type_spec);
    if (get_type_spec_typeids (pstate, dtm, switch_type_spec, false, &m_cdm->type_id, &c_cdm->type_id) < 0)
      return -1;
    get_builtin_type_ann (node, &tm->to_complete->_u.complete._u.union_type.discriminator.ann_builtin);
    get_custom_ann (node, &tm->to_complete->_u.complete._u.union_type.discriminator.ann_custom);
    return IDL_RETCODE_OK;
  } else {
    if (has_fully_descriptive_typeid (switch_type_spec))
      return IDL_VISIT_REVISIT;
    else
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_inherit_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) revisit;
  (void) path;
  (void) node;
  (void) user_data;

  return IDL_VISIT_TYPE_SPEC;
}

static idl_retcode_t
emit_declarator (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;

  (void) pstate;
  (void) path;
  if (idl_is_typedef (idl_parent (node)))
    return add_typedef (pstate, revisit, node, user_data);
  if (idl_is_array (node)) {
    if (!has_fully_descriptive_typeid (node) && !has_plain_collection_typeid (node)) {
      if ((ret = add_array (pstate, revisit, node, user_data)) < 0)
        return ret;
    }
  }

  if (revisit) {
    struct type_meta *tm = dtm->stack;
    assert(tm);
    assert(!tm->finalized);
    if (tm->to_minimal->_u.minimal._d == DDS_XTypes_TK_STRUCTURE) {
      assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_STRUCTURE);
      if ((ret = add_struct_member (pstate, dtm, tm->to_minimal, tm->to_complete, node, idl_is_array (node) ? node : idl_type_spec (node))) < 0)
        return ret;
    } else if (tm->to_minimal->_u.minimal._d == DDS_XTypes_TK_UNION) {
      assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_UNION);
      if ((ret = add_union_case (pstate, dtm, tm->to_minimal, tm->to_complete, node, idl_is_array (node) ? node : idl_type_spec (node))) < 0)
        return ret;
    } else {
      abort ();
    }
  } else {
    const idl_type_spec_t *type_spec = idl_is_array (node) ? node : idl_type_spec (node);
    if (has_fully_descriptive_typeid (type_spec))
      return IDL_VISIT_REVISIT;
    else
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static int enum_literal_minimal_compare (const void *va, const void *vb)
{
  const struct DDS_XTypes_MinimalEnumeratedLiteral *a = va;
  const struct DDS_XTypes_MinimalEnumeratedLiteral *b = vb;
  return (a->common.value == b->common.value) ? 0 : (a->common.value < b->common.value) ? -1 : 1;
}

static int enum_literal_complete_compare (const void *va, const void *vb)
{
  const struct DDS_XTypes_CompleteEnumeratedLiteral *a = va;
  const struct DDS_XTypes_CompleteEnumeratedLiteral *b = vb;
  return (a->common.value == b->common.value) ? 0 : (a->common.value < b->common.value) ? -1 : 1;
}

static idl_retcode_t
emit_enum (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  const idl_enum_t *_enum = (const idl_enum_t *) node;
  idl_retcode_t ret;

  (void)pstate;
  (void)path;
  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_ENUM);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_ENUM);

    /* XTypes spec 7.3.4.5: The elements in (Complete|Minimal)EnumeratedLiteralSeq
       shall be ordered in increasing values of their numeric value. */
    DDS_XTypes_MinimalEnumeratedLiteralSeq *seqm = &dtm->stack->to_minimal->_u.minimal._u.enumerated_type.literal_seq;
    DDS_XTypes_CompleteEnumeratedLiteralSeq *seqc = &dtm->stack->to_complete->_u.complete._u.enumerated_type.literal_seq;
    qsort (seqm->_buffer, seqm->_length, sizeof (*seqm->_buffer), enum_literal_minimal_compare);
    qsort (seqc->_buffer, seqc->_length, sizeof (*seqc->_buffer), enum_literal_complete_compare);
  }
  if ((ret = emit_hashed_type (DDS_XTypes_TK_ENUM, node, revisit, (struct descriptor_type_meta *) user_data)) < 0)
    return ret;
  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_DONT_RECURSE;
  if (!revisit) {
    dtm->stack->to_minimal->_u.minimal._u.enumerated_type.header.common.bit_bound =
      dtm->stack->to_complete->_u.complete._u.enumerated_type.header.common.bit_bound = _enum->bit_bound.value;
    dtm->stack->to_minimal->_u.minimal._u.enumerated_type.enum_flags =
      dtm->stack->to_complete->_u.complete._u.enumerated_type.enum_flags = get_enum_flags (_enum);
    if ((ret = get_complete_type_detail (node, &dtm->stack->to_complete->_u.complete._u.enumerated_type.header.detail)) < 0)
      return ret;
    return IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

ADD_TO_SEQ(DDS_XTypes_MinimalEnumeratedLiteral)
ADD_TO_SEQ(DDS_XTypes_CompleteEnumeratedLiteral)

static idl_retcode_t
emit_enumerator (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)pstate;
  (void)revisit;
  (void)path;

  idl_retcode_t ret;
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;

  assert (idl_is_enum (idl_parent (node)));
  idl_enum_t *_enum = (idl_enum_t *) idl_parent (node);
  assert (tm->to_minimal->_u.minimal._d == DDS_XTypes_TK_ENUM && tm->to_complete->_u.complete._d == DDS_XTypes_TK_ENUM);

  DDS_XTypes_MinimalEnumeratedLiteral m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteEnumeratedLiteral c;
  memset (&c, 0, sizeof (c));

  const idl_enumerator_t *enumerator = (idl_enumerator_t *) node;
  assert (enumerator->value.value <= INT32_MAX);
  m.common.value = c.common.value = (int32_t) enumerator->value.value;
  get_namehash (m.detail.name_hash, idl_identifier (enumerator));
  m.common.flags = c.common.flags = get_enum_literal_flags (_enum, enumerator);
  if ((ret = get_complete_member_detail (node, &c.detail)) < 0)
    return ret;

  if ((ret = add_to_seq_DDS_XTypes_MinimalEnumeratedLiteral (&tm->to_minimal->_u.minimal._u.enumerated_type.literal_seq, &m)) < 0)
    return ret;
  if ((ret = add_to_seq_DDS_XTypes_CompleteEnumeratedLiteral (&tm->to_complete->_u.complete._u.enumerated_type.literal_seq, &c)) < 0)
    return ret;

  return IDL_RETCODE_OK;
}

static int bitflag_minimal_compare (const void *va, const void *vb)
{
  const struct DDS_XTypes_MinimalBitflag *a = va;
  const struct DDS_XTypes_MinimalBitflag *b = vb;
  return (a->common.position == b->common.position) ? 0 : (a->common.position < b->common.position) ? -1 : 1;
}

static int bitflag_complete_compare (const void *va, const void *vb)
{
  const struct DDS_XTypes_CompleteBitflag *a = va;
  const struct DDS_XTypes_CompleteBitflag *b = vb;
  return (a->common.position == b->common.position) ? 0 : (a->common.position < b->common.position) ? -1 : 1;
}

static idl_retcode_t
emit_bitmask(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  const idl_bitmask_t *_bitmask = (const idl_bitmask_t *) node;
  idl_retcode_t ret;

  (void) pstate;
  (void) path;
  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_BITMASK);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_BITMASK);
    /* XTypes spec 7.3.4.5: The elements in (Complete|Minimal)BitflagSeq shall be
       ordered in increasing values of their position. */
    DDS_XTypes_MinimalBitflagSeq *seqm = &dtm->stack->to_minimal->_u.minimal._u.bitmask_type.flag_seq;
    DDS_XTypes_CompleteBitflagSeq *seqc = &dtm->stack->to_complete->_u.complete._u.bitmask_type.flag_seq;
    qsort (seqm->_buffer, seqm->_length, sizeof (*seqm->_buffer), bitflag_minimal_compare);
    qsort (seqc->_buffer, seqc->_length, sizeof (*seqc->_buffer), bitflag_complete_compare);
  }
  if ((ret = emit_hashed_type (DDS_XTypes_TK_BITMASK, node, revisit, (struct descriptor_type_meta *) user_data)) < 0)
    return ret;
  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_DONT_RECURSE;
  if (!revisit) {
    dtm->stack->to_minimal->_u.minimal._u.bitmask_type.header.common.bit_bound =
      dtm->stack->to_complete->_u.complete._u.bitmask_type.header.common.bit_bound = _bitmask->bit_bound.value;
    dtm->stack->to_minimal->_u.minimal._u.bitmask_type.bitmask_flags =
      dtm->stack->to_complete->_u.complete._u.bitmask_type.bitmask_flags = get_bitmask_flags (_bitmask);
    if ((ret = get_complete_type_detail (node, &dtm->stack->to_complete->_u.complete._u.bitmask_type.header.detail)) < 0)
      return ret;
    return IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

ADD_TO_SEQ(DDS_XTypes_MinimalBitflag)
ADD_TO_SEQ(DDS_XTypes_CompleteBitflag)

static idl_retcode_t
emit_bit_value (
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void) pstate;
  (void) revisit;
  (void) path;

  idl_retcode_t ret = IDL_RETCODE_OK;
  struct descriptor_type_meta *dtm = (struct descriptor_type_meta *) user_data;
  struct type_meta *tm = dtm->stack;

  assert (tm->to_complete->_u.complete._d == DDS_XTypes_TK_BITMASK);
  assert (idl_is_bitmask (idl_parent (node)));
  assert (!dtm->stack->finalized);

  DDS_XTypes_MinimalBitflag m;
  memset (&m, 0, sizeof (m));
  DDS_XTypes_CompleteBitflag c;
  memset (&c, 0, sizeof (c));

  const idl_bit_value_t *bit_value = (idl_bit_value_t *) node;
  m.common.position = c.common.position = bit_value->position.value;
  get_namehash (m.detail.name_hash, idl_identifier (bit_value));
  if ((ret = get_complete_member_detail (node, &c.detail)) < 0)
    goto err;
  if ((ret = add_to_seq_DDS_XTypes_MinimalBitflag (&tm->to_minimal->_u.minimal._u.bitmask_type.flag_seq, &m)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = add_to_seq_DDS_XTypes_CompleteBitflag (&tm->to_complete->_u.complete._u.bitmask_type.flag_seq, &c)) != IDL_RETCODE_OK)
    goto err;
  return ret;

err:
  if (c.detail.ann_builtin != NULL) {
    if (c.detail.ann_builtin->hash_id != NULL)
      idl_free (c.detail.ann_builtin->hash_id);
    idl_free (c.detail.ann_builtin);
  }
  if (tm->to_minimal->_u.minimal._u.bitmask_type.flag_seq._release)
  {
    idl_free (tm->to_minimal->_u.minimal._u.bitmask_type.flag_seq._buffer);
    tm->to_minimal->_u.minimal._u.bitmask_type.flag_seq._buffer = NULL;
  }
  if (tm->to_complete->_u.complete._u.bitmask_type.flag_seq._release)
  {
    idl_free (tm->to_complete->_u.complete._u.bitmask_type.flag_seq._buffer);
    tm->to_complete->_u.complete._u.bitmask_type.flag_seq._buffer = NULL;
  }
  return ret;
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  struct descriptor_type_meta * dtm = (struct descriptor_type_meta *) user_data;
  const idl_sequence_t * seq = (const idl_sequence_t *) node;
  idl_retcode_t ret;

  (void) path;
  /* In case the sequence is not a plain collection type identifier, which means that the
     sequence itself (not the element type) cannot be expressed by a (non-hash) type
     identifier but also needs a type object, a hashed type is added for this sequence */
  if (has_plain_collection_typeid (node))
    return IDL_VISIT_TYPE_SPEC;

  /* Sequence node should not be a fully descriptive type (which does not need a hashed
     type identifier. A fully descriptive sequence is handled in emit_declarator */
  assert (!has_fully_descriptive_typeid (node));

  if (revisit) {
    assert (dtm->stack->to_minimal->_u.minimal._d == DDS_XTypes_TK_SEQUENCE);
    assert (dtm->stack->to_complete->_u.complete._d == DDS_XTypes_TK_SEQUENCE);
    if ((ret = get_type_spec_typeids (pstate, dtm, idl_type_spec(node), false,
        &dtm->stack->to_minimal->_u.minimal._u.sequence_type.element.common.type,
        &dtm->stack->to_complete->_u.complete._u.sequence_type.element.common.type)) < 0)
    {
      return ret;
    }
  }

  if ((ret = emit_hashed_type (DDS_XTypes_TK_SEQUENCE, node, revisit, dtm)) < 0)
    return ret;
  if (!revisit && dtm->stack->finalized)
    return IDL_VISIT_REVISIT | IDL_VISIT_TYPE_SPEC;

  if (!revisit) {
    /* Add the sequence flags (not the element flags) and bound (maximum). The element type
       will be set on revisit for this node, after visiting the type-spec (= element type).
       No need to include sequence_type.header.detail here, because sequences are anonymous
       types and there is no type name to store */
    dtm->stack->to_minimal->_u.minimal._u.sequence_type.element.common.element_flags =
      dtm->stack->to_complete->_u.complete._u.sequence_type.element.common.element_flags = get_sequence_element_flags (node);
    dtm->stack->to_minimal->_u.minimal._u.sequence_type.header.common.bound =
      dtm->stack->to_complete->_u.complete._u.sequence_type.header.common.bound = seq->maximum;
    return IDL_VISIT_REVISIT | IDL_VISIT_TYPE_SPEC;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_forward(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)pstate;
  (void)revisit;
  (void)path;
  (void)node;
  (void)user_data;
  return IDL_VISIT_TYPE_SPEC;
}

static idl_retcode_t
print_ser_data(FILE *fp, const char *kind, const char *type, unsigned char *data, uint32_t sz)
{
  char *sep = ", ", *lsep = "\\\n  ", *fmt;

  fmt = "#define %1$s_%2$s (const unsigned char []){ ";
  if (idl_fprintf(fp, fmt, kind, type) < 0)
    return IDL_RETCODE_NO_MEMORY;

  fmt = "%1$s%2$s0x%3$02"PRIx8;
  for (uint32_t n = 0; n < sz; n++)
    if (idl_fprintf(fp, fmt, n > 0 ? sep : "", !(n % 16) ? lsep : "", data[n]) < 0)
      return IDL_RETCODE_NO_MEMORY;

  fmt = "\\\n}\n"
        "#define %1$s_SZ_%2$s %3$"PRIu32"u\n";
  if (idl_fprintf(fp, fmt, kind, type, sz) < 0)
    return IDL_RETCODE_NO_MEMORY;

  return 0;
}

static idl_retcode_t
get_typeid_with_size (
    DDS_XTypes_TypeIdentifierWithSize *typeid_with_size,
    DDS_XTypes_TypeIdentifier *ti,
    DDS_XTypes_TypeObject *to)
{
  idl_retcode_t ret;
  assert (ti);
  assert (to);
  memcpy (&typeid_with_size->type_id, ti, sizeof (typeid_with_size->type_id));
  dds_ostream_t os;
  if ((ret = xcdr2_ser (to, &DDS_XTypes_TypeObject_cdrstream_desc, &os)) < 0)
    return ret;
  typeid_with_size->typeobject_serialized_size = os.m_index;
  dds_ostream_fini (&os, &idlc_cdrstream_default_allocator);
  return IDL_RETCODE_OK;
}

static void
type_id_fini (DDS_XTypes_TypeIdentifier *ti)
{
  dds_stream_free_sample (ti, &idlc_cdrstream_default_allocator, DDS_XTypes_TypeIdentifier_desc.m_ops);
}

static void
type_obj_fini (DDS_XTypes_TypeObject *to)
{
  dds_stream_free_sample (to, &idlc_cdrstream_default_allocator, DDS_XTypes_TypeObject_desc.m_ops);
}

static idl_retcode_t
print_typeid_with_deps (
  FILE *fp,
  const struct DDS_XTypes_TypeIdentifierWithDependencies *typeid_with_deps)
{
  struct ddsi_typeid_str tidstr;
  const char *fmt = "  %s (#deps: %d)\n";
  if (idl_fprintf (fp, fmt, ddsi_make_typeid_str (&tidstr, (struct ddsi_typeid *) &typeid_with_deps->typeid_with_size.type_id), typeid_with_deps->dependent_typeid_count) < 0)
    return IDL_RETCODE_NO_MEMORY;
  fmt = "   - %s\n";
  for (uint32_t n = 0; n < typeid_with_deps->dependent_typeids._length; n++)
  {
    if (idl_fprintf (fp, fmt, ddsi_make_typeid_str (&tidstr, (struct ddsi_typeid *) &typeid_with_deps->dependent_typeids._buffer[n].type_id)) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
print_typeinformation_comment (
  FILE *fp,
  const struct DDS_XTypes_TypeInformation *type_information)
{
  if (idl_fprintf(fp, "/* Type Information:\n") < 0)
    return IDL_RETCODE_NO_MEMORY;

  if (print_typeid_with_deps (fp, &type_information->minimal) != IDL_RETCODE_OK)
    return IDL_RETCODE_NO_MEMORY;

  if (print_typeid_with_deps (fp, &type_information->complete) != IDL_RETCODE_OK)
    return IDL_RETCODE_NO_MEMORY;

  if (idl_fprintf (fp, "*/\n") < 0)
    return IDL_RETCODE_NO_MEMORY;

  return IDL_RETCODE_OK;
}

idl_retcode_t
generate_descriptor_type_meta (
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  struct descriptor_type_meta *dtm)
{
  idl_retcode_t ret;
  idl_visitor_t visitor;

  memset (dtm, 0, sizeof (*dtm));
  memset (&visitor, 0, sizeof (visitor));

  visitor.visit = IDL_STRUCT | IDL_UNION | IDL_DECLARATOR | IDL_BITMASK | IDL_BIT_VALUE | IDL_ENUM | IDL_ENUMERATOR | IDL_SWITCH_TYPE_SPEC | IDL_INHERIT_SPEC | IDL_SEQUENCE;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;
  visitor.accept[IDL_ACCEPT_BITMASK] = &emit_bitmask;
  visitor.accept[IDL_ACCEPT_BIT_VALUE] = &emit_bit_value;
  visitor.accept[IDL_ACCEPT_ENUM] = &emit_enum;
  visitor.accept[IDL_ACCEPT_ENUMERATOR] = &emit_enumerator;
  visitor.accept[IDL_ACCEPT_INHERIT_SPEC] = &emit_inherit_spec;
  visitor.accept[IDL_ACCEPT_SEQUENCE] = &emit_sequence;
  visitor.accept[IDL_ACCEPT_FORWARD] = &emit_forward;

  /* must be invoked for topics only, so structs and unions */
  assert (idl_is_struct (node) || idl_is_union (node));

  dtm->root = node;
  if ((ret = idl_visit (pstate, node, &visitor, dtm)))
    return ret;
  return IDL_RETCODE_OK;
}

void
descriptor_type_meta_fini (struct descriptor_type_meta *dtm)
{
  struct type_meta *tm = dtm->admin;
  while (tm) {
    type_id_fini (tm->ti_minimal);
    idl_free (tm->ti_minimal);
    type_obj_fini (tm->to_minimal);
    idl_free (tm->to_minimal);

    type_id_fini (tm->ti_complete);
    idl_free (tm->ti_complete);
    type_obj_fini (tm->to_complete);
    idl_free (tm->to_complete);

    struct type_meta *tmp = tm;
    tm = tm->admin_next;
    idl_free (tmp);
  }
}

static void
xtypes_typeinfo_fini (struct DDS_XTypes_TypeInformation *type_information)
{
  if (type_information->minimal.dependent_typeids._buffer)
    idl_free (type_information->minimal.dependent_typeids._buffer);
  if (type_information->complete.dependent_typeids._buffer)
    idl_free (type_information->complete.dependent_typeids._buffer);
}

static void
generate_type_meta_ser_impl_free_mapping_buffers (
  DDS_XTypes_TypeMapping *mapping)
{
  if (mapping->identifier_complete_minimal._buffer)
    idl_free (mapping->identifier_complete_minimal._buffer);
  if (mapping->identifier_object_pair_complete._buffer)
    idl_free (mapping->identifier_object_pair_complete._buffer);
  if (mapping->identifier_object_pair_minimal._buffer)
    idl_free (mapping->identifier_object_pair_minimal._buffer);
}

ADD_TO_SEQ_STRUCT(DDS_XTypes_TypeIdentifierWithSize)
ADD_TO_SEQ_STRUCT(DDS_XTypes_TypeIdentifierTypeObjectPair)
ADD_TO_SEQ_STRUCT(DDS_XTypes_TypeIdentifierPair)

static idl_retcode_t
generate_type_meta_ser_impl (
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  struct DDS_XTypes_TypeInformation *type_information,
  dds_ostream_t *os_typeinfo,
  dds_ostream_t *os_typemap)
{
  idl_retcode_t ret;
  struct descriptor_type_meta dtm;

  if ((ret = generate_descriptor_type_meta (pstate, node, &dtm)) != IDL_RETCODE_OK)
    goto err_gen;

  memset (type_information, 0, sizeof (*type_information));

  /* typeidwithsize for top-level type */
  if ((ret = get_typeid_with_size (&type_information->minimal.typeid_with_size, dtm.admin->ti_minimal, dtm.admin->to_minimal)) < 0
      || (ret = get_typeid_with_size (&type_information->complete.typeid_with_size, dtm.admin->ti_complete, dtm.admin->to_complete)) < 0)
    goto err_dep;

  /* dependent type ids, skip first (top-level) */
  for (struct type_meta *tm = dtm.admin->admin_next; tm; tm = tm->admin_next) {
    DDS_XTypes_TypeIdentifierWithSize tidws;

    if ((ret = get_typeid_with_size (&tidws, tm->ti_minimal, tm->to_minimal)) < 0)
      goto err_dep;

    /* Minimal type ids can be equal for different types (e.g. only type name differs for an typedef),
       so check if the type id is already in the list and don't add duplicates */
    bool found = false;
    for (uint32_t n = 0; !found && n < type_information->minimal.dependent_typeids._length; n++)
      found = !ddsi_typeid_compare ((struct ddsi_typeid *) &type_information->minimal.dependent_typeids._buffer[n].type_id, (struct ddsi_typeid *) &tidws.type_id);
    if (!found)
    {
      if ((ret = add_to_seq_DDS_XTypes_TypeIdentifierWithSize (&type_information->minimal.dependent_typeids, &tidws)) < 0)
        goto err_dep;
      type_information->minimal.dependent_typeid_count++;
    }

    if ((ret = get_typeid_with_size (&tidws, tm->ti_complete, tm->to_complete)) < 0)
      goto err_dep;
    if ((ret = add_to_seq_DDS_XTypes_TypeIdentifierWithSize (&type_information->complete.dependent_typeids, &tidws)) < 0)
      goto err_dep;
    type_information->complete.dependent_typeid_count++;
  }

  if ((ret = xcdr2_ser (type_information, &DDS_XTypes_TypeInformation_cdrstream_desc, os_typeinfo)) < 0)
    goto err_dep_ser;

  /* type id/obj seq for min and complete */
  DDS_XTypes_TypeMapping mapping;
  memset (&mapping, 0, sizeof (mapping));
  for (struct type_meta *tm = dtm.admin; tm; tm = tm->admin_next) {
    DDS_XTypes_TypeIdentifierTypeObjectPair mp, cp;
    DDS_XTypes_TypeIdentifierPair ip;

    memcpy (&mp.type_identifier, tm->ti_minimal, sizeof (mp.type_identifier));
    memcpy (&mp.type_object, tm->to_minimal, sizeof (mp.type_object));
    if ((ret = add_to_seq_DDS_XTypes_TypeIdentifierTypeObjectPair (&mapping.identifier_object_pair_minimal, &mp)) < 0)
      goto err_map;

    memcpy (&cp.type_identifier, tm->ti_complete, sizeof (cp.type_identifier));
    memcpy (&cp.type_object, tm->to_complete, sizeof (cp.type_object));
    if ((ret = add_to_seq_DDS_XTypes_TypeIdentifierTypeObjectPair (&mapping.identifier_object_pair_complete, &cp)) < 0)
      goto err_map;

    memcpy (&ip.type_identifier1, tm->ti_complete, sizeof (ip.type_identifier1));
    memcpy (&ip.type_identifier2, tm->ti_minimal, sizeof (ip.type_identifier2));
    if ((ret = add_to_seq_DDS_XTypes_TypeIdentifierPair (&mapping.identifier_complete_minimal, &ip)) < 0)
      goto err_map;
  }

  if ((ret = xcdr2_ser (&mapping, &DDS_XTypes_TypeMapping_cdrstream_desc, os_typemap)) < 0)
    goto err_map_ser;

  generate_type_meta_ser_impl_free_mapping_buffers (&mapping);
  descriptor_type_meta_fini (&dtm);
  return IDL_RETCODE_OK;

err_map_ser:
err_map:
  generate_type_meta_ser_impl_free_mapping_buffers (&mapping);
err_dep_ser:
err_dep:
  xtypes_typeinfo_fini (type_information);
err_gen:
  descriptor_type_meta_fini (&dtm);
  return ret;
}

idl_retcode_t
print_type_meta_ser (
  FILE *fp,
  const idl_pstate_t *pstate,
  const idl_node_t *node)
{
  struct DDS_XTypes_TypeInformation type_information;
  dds_ostream_t os_typeinfo;
  dds_ostream_t os_typemap;
  char *type_name;
  idl_retcode_t rc;

  if (IDL_PRINTA(&type_name, print_type, node) < 0)
    return IDL_RETCODE_NO_MEMORY;

  if ((rc = generate_type_meta_ser_impl (pstate, node, &type_information, &os_typeinfo, &os_typemap)))
    return rc;

  if ((rc = print_typeinformation_comment (fp, &type_information)) != IDL_RETCODE_OK)
    goto err_print;
  print_ser_data (fp, "TYPE_INFO_CDR", type_name, os_typeinfo.m_buffer, os_typeinfo.m_index);
  print_ser_data (fp, "TYPE_MAP_CDR", type_name, os_typemap.m_buffer, os_typemap.m_index);

err_print:
  xtypes_typeinfo_fini (&type_information);
  dds_ostream_fini (&os_typeinfo, &idlc_cdrstream_default_allocator);
  dds_ostream_fini (&os_typemap, &idlc_cdrstream_default_allocator);
  return rc;
}

idl_retcode_t
generate_type_meta_ser (
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  idl_typeinfo_typemap_t *result)
{
  struct DDS_XTypes_TypeInformation type_information;
  dds_ostream_t os_typeinfo;
  dds_ostream_t os_typemap;
  idl_retcode_t rc;

  if ((rc = generate_type_meta_ser_impl (pstate, node, &type_information, &os_typeinfo, &os_typemap)))
    return rc;

  result->typeinfo = NULL;
  result->typemap = NULL;
  result->typeinfo_size = os_typeinfo.m_index;
  result->typemap_size = os_typemap.m_index;
  if ((result->typeinfo = idl_malloc (result->typeinfo_size)) == NULL) {
    rc = IDL_RETCODE_NO_MEMORY;
    goto err_nomem;
  }
  memcpy (result->typeinfo, os_typeinfo.m_buffer, result->typeinfo_size);
  if ((result->typemap = idl_malloc (result->typemap_size)) == NULL) {
    idl_free (result->typeinfo);
    rc = IDL_RETCODE_NO_MEMORY;
    goto err_nomem;
  }
  memcpy (result->typemap, os_typemap.m_buffer, result->typemap_size);

err_nomem:
  xtypes_typeinfo_fini (&type_information);
  dds_ostream_fini (&os_typeinfo, &idlc_cdrstream_default_allocator);
  dds_ostream_fini (&os_typemap, &idlc_cdrstream_default_allocator);
  return rc;
}

