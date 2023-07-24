// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

#include "dynsub.h"

void ppc_init (struct ppc *ppc)
{
  ppc->bol = true;
  ppc->indent = 0;
  ppc->lineno = 1;
}

static int ppc_lineno (struct ppc *ppc) { return ppc->lineno; }
static void ppc_indent (struct ppc *ppc) { ppc->indent += 2; }
static void ppc_outdent (struct ppc *ppc) { ppc->indent -= 2; }

static void ppc_print (struct ppc *ppc, const char *fmt, ...)
  ddsrt_attribute_format_printf (2, 3);

static void ppc_print (struct ppc *ppc, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  if (ppc->bol)
    printf ("%3d %*s", ppc->lineno, ppc->indent, "");
  vprintf (fmt, ap);
  va_end (ap);
  ppc->bol = (fmt[0] == 0) ? 0 : (fmt[strlen (fmt) - 1] == '\n');
  if (ppc->bol)
    ++ppc->lineno;
}

static void ppc_print_equivhash (struct ppc *ppc, const DDS_XTypes_EquivalenceHash id)
{
  ppc_print (ppc, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             (unsigned) id[0], (unsigned) id[1], (unsigned) id[2], (unsigned) id[3],
             (unsigned) id[4], (unsigned) id[5], (unsigned) id[6], (unsigned) id[7],
             (unsigned) id[8], (unsigned) id[9], (unsigned) id[10], (unsigned) id[11],
             (unsigned) id[12], (unsigned) id[13]);
}

static void ppc_print_memberflags (struct ppc *ppc, DDS_XTypes_MemberFlag flag)
{
  const char *sep = "";
  ppc_print (ppc, "memberflags={");
#define PF(flagname) do { \
    if (flag & DDS_XTypes_##flagname) { \
      ppc_print (ppc, "%s%s", sep, #flagname); \
      flag &= (uint16_t)~DDS_XTypes_##flagname; \
      sep = ","; \
    } \
  } while (0)
  PF(TRY_CONSTRUCT1);
  PF(TRY_CONSTRUCT2);
  PF(IS_EXTERNAL);
  PF(IS_OPTIONAL);
  PF(IS_MUST_UNDERSTAND);
  PF(IS_KEY);
  PF(IS_DEFAULT);
  if (flag)
    ppc_print (ppc, "%s0x%"PRIx16, sep, flag);
  ppc_print (ppc, "}");
}

static void ppc_print_typeflags (struct ppc *ppc, DDS_XTypes_TypeFlag flag)
{
  const char *sep = "";
  ppc_print (ppc, "typeflags={");
#define PF(flagname) do { \
    if (flag & DDS_XTypes_##flagname) { \
      ppc_print (ppc, "%s%s", sep, #flagname); \
      flag &= (uint16_t)~DDS_XTypes_##flagname; \
      sep = ","; \
    } \
  } while (0)
  PF(IS_FINAL);
  PF(IS_APPENDABLE);
  PF(IS_MUTABLE);
  PF(IS_NESTED);
  PF(IS_AUTOID_HASH);
  if (flag)
    ppc_print (ppc, "%s0x%"PRIx16, sep, flag);
  ppc_print (ppc, "}");
}

static void ppc_print_paramval (struct ppc *ppc, const DDS_XTypes_AnnotationParameterValue *v)
{
  ppc_print (ppc, "{_d=%02x,_u=", (unsigned)v->_d);
  switch (v->_d)
  {
#define CASE(disc, fmt, member) \
  case DDS_XTypes_TK_##disc: \
    ppc_print (ppc, fmt, v->_u.member##_value); \
    break
      CASE(BOOLEAN, "%d", boolean);
      CASE(BYTE, "%"PRIu8, byte);
      CASE(INT8, "%"PRId8, int8);
      CASE(UINT8, "%"PRIu8, uint8);
      CASE(INT16, "%"PRId16, int16);
      CASE(UINT16, "%"PRIu16, uint_16);
      CASE(INT32, "%"PRId32, int32);
      CASE(UINT32, "%"PRIu32, uint32);
      CASE(INT64, "%"PRId64, int64);
      CASE(UINT64, "%"PRIu64, uint64);
      CASE(FLOAT32, "%f", float32);
      CASE(FLOAT64, "%f", float64);
      CASE(CHAR8, "'%c'", char);
      CASE(ENUM, "%"PRIu32, enumerated);
      CASE(STRING8, "\"%s\"", string8);
#undef CASE
  }
  ppc_print (ppc, "}\n");
}

static void ppc_print_paramseq (struct ppc *ppc, const DDS_XTypes_AppliedAnnotationParameterSeq *ps)
{
  if (ps == NULL)
    return;
  for (uint32_t j = 0; j < ps->_length; j++)
  {
    DDS_XTypes_AppliedAnnotationParameter *p = &ps->_buffer[j];
    ppc_print (ppc, "namehash=%02x%02x%02x%02x value=",
               (unsigned)p->paramname_hash[0], (unsigned)p->paramname_hash[1],
               (unsigned)p->paramname_hash[2], (unsigned)p->paramname_hash[3]);
    ppc_print_paramval (ppc, &p->value);
  }
}

static void ppc_print_annots (struct ppc *ppc, const DDS_XTypes_AppliedAnnotationSeq *as)
{
  if (as == NULL)
    return;
  ppc_print (ppc, "annots={\n");
  ppc_indent (ppc);
  for (uint32_t i = 0; i < as->_length; i++)
  {
    const DDS_XTypes_AppliedAnnotation *a = &as->_buffer[i];
    ppc_print (ppc, "typeid={_d=0x%"PRIx8"} params={", a->annotation_typeid._d);
    ppc_indent (ppc);
    ppc_print_paramseq (ppc, a->param_seq);
    ppc_outdent (ppc);
    ppc_print (ppc, "}\n");
  }
  ppc_outdent (ppc);
  ppc_print (ppc, "}\n");
}

static void ppc_print_builtin_type_annots (struct ppc *ppc, const struct DDS_XTypes_AppliedBuiltinTypeAnnotations *as)
{
  if (as == NULL)
    return;
  if (as->verbatim)
  {
    const DDS_XTypes_AppliedVerbatimAnnotation *v = as->verbatim;
    ppc_print (ppc, "verbatim={placement=%s,language=%s,text=%s}\n", v->placement, v->language, v->text);
  }
}

static void ppc_print_typedetail_sans_name (struct ppc *ppc, const DDS_XTypes_CompleteTypeDetail *detail)
{
  ppc_print_builtin_type_annots (ppc, detail->ann_builtin);
  ppc_print_annots (ppc, detail->ann_custom);
}

static void ppc_print_typedetail (struct ppc *ppc, const DDS_XTypes_CompleteTypeDetail *detail)
{
  if (detail == NULL)
    return;
  ppc_print (ppc, "typename=%s ", detail->type_name);
  ppc_print_typedetail_sans_name (ppc, detail);
}

static void ppc_print_builtin_member_annots (struct ppc *ppc, const DDS_XTypes_AppliedBuiltinMemberAnnotations *a)
{
  if (a == NULL)
    return;
  if (a->unit) {
    ppc_print (ppc, "unit=%s\n", a->unit);
  }
  if (a->min) {
    ppc_print (ppc, "min=");
    ppc_print_paramval (ppc, a->min);
    ppc_print (ppc, "\n");
  }
  if (a->max) {
    ppc_print (ppc, "max=");
    ppc_print_paramval (ppc, a->max);
    ppc_print (ppc, "\n");
  }
  if (a->hash_id) {
    ppc_print (ppc, "hash_id=%s\n", a->hash_id);
  }
}

static void ppc_print_elementdetail (struct ppc *ppc, const DDS_XTypes_CompleteElementDetail *detail)
{
  ppc_print_builtin_member_annots (ppc, detail->ann_builtin);
  ppc_print_annots (ppc, detail->ann_custom);
}

static void ppc_print_memberdetail_sans_name (struct ppc *ppc, const DDS_XTypes_CompleteMemberDetail *detail)
{
  ppc_print_builtin_member_annots (ppc, detail->ann_builtin);
  ppc_print_annots (ppc, detail->ann_custom);
}

static bool ppc_print_simple (struct ppc *ppc, const uint8_t disc)
{
  switch (disc)
  {
#define CASE(disc) DDS_XTypes_TK_##disc: ppc_print (ppc, "%s\n", #disc); return true
    case CASE(NONE);
    case CASE(BOOLEAN);
    case CASE(BYTE);
    case CASE(INT16);
    case CASE(INT32);
    case CASE(INT64);
    case CASE(UINT16);
    case CASE(UINT32);
    case CASE(UINT64);
    case CASE(FLOAT32);
    case CASE(FLOAT64);
    case CASE(FLOAT128);
    case CASE(INT8);
    case CASE(UINT8);
    case CASE(CHAR8);
    case CASE(CHAR16);
    case CASE(STRING8);
    case CASE(STRING16);
#undef CASE
  }
  return false;
}

static void ppc_print_ti (struct ppc *ppc, const DDS_XTypes_TypeIdentifier *typeid)
{
  if (ppc_print_simple (ppc, typeid->_d))
    return;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      const uint32_t bound = (typeid->_d == DDS_XTypes_TI_STRING8_SMALL) ? typeid->_u.string_sdefn.bound : typeid->_u.string_ldefn.bound;
      ppc_print (ppc, "STRING8_%s bound=%"PRIu32"\n", (typeid->_d == DDS_XTypes_TI_STRING8_SMALL) ? "SMALL" : "LARGE", bound);
      break;
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE: {
      const DDS_XTypes_PlainCollectionHeader *header;
      const DDS_XTypes_TypeIdentifier *et;
      uint32_t bound;
      if (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) {
        header = &typeid->_u.seq_sdefn.header;
        et = typeid->_u.seq_sdefn.element_identifier;
        bound = typeid->_u.seq_sdefn.bound;
      } else {
        header = &typeid->_u.seq_ldefn.header;
        et = typeid->_u.seq_ldefn.element_identifier;
        bound = typeid->_u.seq_ldefn.bound;
      }
      ppc_print (ppc, "PLAIN_SEQUENCE_%s bound=%"PRIu32" equiv_kind=", (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) ? "SMALL" : "LARGE", bound);
      switch (header->equiv_kind)
      {
        case DDS_XTypes_EK_MINIMAL: ppc_print (ppc, "MINIMAL"); break;
        case DDS_XTypes_EK_COMPLETE: ppc_print (ppc, "COMPLETE"); break;
        case DDS_XTypes_EK_BOTH: ppc_print (ppc, "BOTH"); break;
        default: ppc_print (ppc, "%02"PRIx8, header->equiv_kind);
      }
      ppc_print (ppc, " ");
      ppc_print_memberflags (ppc, header->element_flags);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      ppc_print_ti (ppc, et);
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE: {
      const DDS_XTypes_PlainCollectionHeader *header;
      const DDS_XTypes_TypeIdentifier *et;
      ppc_print (ppc, "PLAIN_ARRAY_%s bound={", (typeid->_d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL) ? "SMALL" : "LARGE");
      if (typeid->_d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL) {
        header = &typeid->_u.array_sdefn.header;
        et = typeid->_u.array_sdefn.element_identifier;
        for (uint32_t i = 0; i < typeid->_u.array_sdefn.array_bound_seq._length; i++)
          ppc_print (ppc, "%s%"PRIu8, (i == 0) ? "" : ",", typeid->_u.array_sdefn.array_bound_seq._buffer[i]);
      } else {
        header = &typeid->_u.array_ldefn.header;
        et = typeid->_u.array_ldefn.element_identifier;
        for (uint32_t i = 0; i < typeid->_u.array_ldefn.array_bound_seq._length; i++)
          ppc_print (ppc, "%s%"PRIu32, (i == 0) ? "" : ",", typeid->_u.array_ldefn.array_bound_seq._buffer[i]);
      }
      ppc_print (ppc, "} equiv_kind=");
      switch (header->equiv_kind)
      {
        case DDS_XTypes_EK_MINIMAL: ppc_print (ppc, "MINIMAL"); break;
        case DDS_XTypes_EK_COMPLETE: ppc_print (ppc, "COMPLETE"); break;
        case DDS_XTypes_EK_BOTH: ppc_print (ppc, "BOTH"); break;
        default: ppc_print (ppc, "%02"PRIx8, header->equiv_kind);
      }
      ppc_print (ppc, " ");
      ppc_print_memberflags (ppc, header->element_flags);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      ppc_print_ti (ppc, et);
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_EK_COMPLETE: {
      ppc_print (ppc, "COMPLETE id=");
      ppc_print_equivhash (ppc, typeid->_u.equivalence_hash);
      ppc_indent (ppc);
      struct type_hashid_map *info = lookup_hashid (typeid->_u.equivalence_hash);
      if (info->lineno)
        ppc_print (ppc, ": See line %d\n", info->lineno);
      else
      {
        ppc_print (ppc, "\n");
        info->lineno = ppc_lineno (ppc);
        ppc_print_to (ppc, get_complete_typeobj_for_hashid (typeid->_u.equivalence_hash));
      }
      ppc_outdent (ppc);
      break;
    }
    default: {
      printf ("type id discriminant %u encountered, sorry\n", (unsigned) typeid->_d);
      abort ();
    }
  }
}

void ppc_print_to (struct ppc *ppc, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  if (ppc_print_simple (ppc, typeobj->_d))
    return;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS: {
      const DDS_XTypes_CompleteAliasType *x = &typeobj->_u.alias_type;
      ppc_print (ppc, "ALIAS typename=%s ", x->header.detail.type_name);
      ppc_print_typeflags (ppc, x->alias_flags);
      ppc_print_typedetail_sans_name (ppc, &x->header.detail);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      ppc_print_builtin_member_annots (ppc, x->body.ann_builtin);
      ppc_print_annots (ppc, x->body.ann_custom);
      ppc_print_memberflags (ppc, x->body.common.related_flags);
      ppc_print (ppc, "\n");
      ppc_print_ti (ppc, &x->body.common.related_type);
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      const DDS_XTypes_CompleteEnumeratedType *x = &typeobj->_u.enumerated_type;
      ppc_print (ppc, "ENUM typename=%s bit_bound=%"PRIu32" ", x->header.detail.type_name, x->header.common.bit_bound);
      ppc_print_typeflags (ppc, x->enum_flags);
      ppc_print_typedetail_sans_name (ppc, &x->header.detail);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      for (uint32_t i = 0; i < x->literal_seq._length; i++)
      {
        const DDS_XTypes_CompleteEnumeratedLiteral *l = &x->literal_seq._buffer[i];
        ppc_print (ppc, "%s = %"PRId32" ", l->detail.name, l->common.value);
        ppc_print_memberflags (ppc, l->common.flags);
        ppc_print_memberdetail_sans_name (ppc, &l->detail);
        ppc_print (ppc, "\n");
      }
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_TK_BITMASK: {
      const DDS_XTypes_CompleteBitmaskType *x = &typeobj->_u.bitmask_type;
      ppc_print (ppc, "ENUM typename=%s bit_bound=%"PRIu32" ", x->header.detail.type_name, x->header.common.bit_bound);
      ppc_print_typeflags (ppc, x->bitmask_flags);
      ppc_print_typedetail_sans_name (ppc, &x->header.detail);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      for (uint32_t i = 0; i < x->flag_seq._length; i++)
      {
        const DDS_XTypes_CompleteBitflag *l = &x->flag_seq._buffer[i];
        ppc_print (ppc, "%s = %"PRIu32" ", l->detail.name, l->common.position);
        ppc_print_memberflags (ppc, l->common.flags);
        ppc_print_memberdetail_sans_name (ppc, &l->detail);
        ppc_print (ppc, "\n");
      }
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      const DDS_XTypes_CompleteSequenceType *x = &typeobj->_u.sequence_type;
      ppc_print (ppc, "SEQUENCE bound=%"PRIu32" ", x->header.common.bound);
      ppc_print_typeflags (ppc, x->collection_flag);
      ppc_print_memberflags (ppc, x->element.common.element_flags);
      ppc_print_typedetail (ppc, x->header.detail);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      ppc_print_elementdetail (ppc, &x->element.detail);
      ppc_print_ti (ppc, &x->element.common.type);
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      ppc_print (ppc, "STRUCTURE typename=%s ", t->header.detail.type_name);
      ppc_print_typeflags (ppc, t->struct_flags);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      ppc_print_typedetail_sans_name (ppc, &t->header.detail);
      if (t->header.base_type._d != DDS_XTypes_TK_NONE)
      {
        ppc_print (ppc, "basetype=\n");
        ppc_indent (ppc);
        ppc_print_ti (ppc, &t->header.base_type);
        ppc_outdent (ppc);
      }
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
        ppc_print (ppc, "name=%s ", m->detail.name);
        ppc_print (ppc, "memberid=0x%"PRIx32" ", m->common.member_id);
        ppc_print_memberflags (ppc, m->common.member_flags);
        ppc_print (ppc, "\n");
        ppc_indent (ppc);
        ppc_print_memberdetail_sans_name (ppc, &m->detail);
        ppc_print_ti (ppc, &m->common.member_type_id);
        ppc_outdent (ppc);
      }
      ppc_outdent (ppc);
      break;
    }
    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      ppc_print (ppc, "UNION ");
      ppc_print (ppc, "typename=%s ", t->header.detail.type_name);
      ppc_print_typeflags (ppc, t->union_flags);
      ppc_print (ppc, "\n");
      ppc_indent (ppc);
      ppc_print_typedetail_sans_name (ppc, &t->header.detail);
      ppc_print (ppc, "discriminator=");
      ppc_indent (ppc);
      const DDS_XTypes_CompleteDiscriminatorMember *disc = &t->discriminator;
      ppc_print_memberflags (ppc, disc->common.member_flags);
      ppc_print (ppc, " ");
      ppc_print_ti (ppc, &disc->common.type_id);
      ppc_print_builtin_type_annots (ppc, disc->ann_builtin);
      ppc_print_annots (ppc, disc->ann_custom);
      ppc_outdent (ppc);
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteUnionMember *m = &t->member_seq._buffer[i];
        if (m->common.label_seq._length == 0)
          ppc_print (ppc, "default:\n");
        for (uint32_t j = 0; j < m->common.label_seq._length; j++)
          ppc_print (ppc, "case %"PRIu32":\n", m->common.label_seq._buffer[j]);
        ppc_indent (ppc);
        ppc_print (ppc, "name=%s ", m->detail.name);
        ppc_print (ppc, "memberid=0x%"PRIx32" ", m->common.member_id);
        ppc_print_memberflags (ppc, m->common.member_flags);
        ppc_print (ppc, "\n");
        ppc_indent (ppc);
        ppc_print_memberdetail_sans_name (ppc, &m->detail);
        ppc_print_ti (ppc, &m->common.type_id);
        ppc_outdent (ppc);
        ppc_outdent (ppc);
      }
      ppc_outdent (ppc);
      break;
    }
    default: {
      printf ("type object discriminant %u encountered, sorry\n", (unsigned) typeobj->_d);
      abort ();
    }
  }
}
