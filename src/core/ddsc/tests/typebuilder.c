// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/dds.h"
#include "config_env.h"

#include "dds/ddsc/dds_opcodes.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_typebuilder.h"
#include "ddsi__xt_impl.h"
#include "dds__types.h"
#include "dds__topic.h"
#include "TypeBuilderTypes.h"
#include "CUnit/Test.h"
#include "test_common.h"

static dds_entity_t g_participant = 0;

static void typebuilder_init (void)
{
  g_participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GT_FATAL (g_participant, 0);
}

static void typebuilder_fini (void)
{
  dds_delete (g_participant);
}

static void topic_type_ref (dds_entity_t topic, struct ddsi_type **type)
{
  dds_topic *t;
  dds_return_t ret = dds_topic_pin (topic, &t);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  struct ddsi_sertype *sertype = t->m_stype;
  ret = ddsi_type_ref_local (&t->m_entity.m_domain->gv, type, sertype, DDSI_TYPEID_KIND_COMPLETE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL (type, NULL);
  CU_ASSERT_NEQ_FATAL (*type, NULL);
  dds_topic_unpin (t);
}

static void topic_type_unref (dds_entity_t topic, struct ddsi_type *type)
{
  dds_topic *t;
  dds_return_t ret = dds_topic_pin (topic, &t);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ddsi_type_unref (&t->m_entity.m_domain->gv, type);
  dds_topic_unpin (t);
}

static struct ddsi_domaingv *gv_from_topic (dds_entity_t topic)
{
  dds_topic *t;
  dds_return_t ret = dds_topic_pin (topic, &t);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  struct ddsi_domaingv *gv = &t->m_entity.m_domain->gv;
  dds_topic_unpin (t);
  return gv;
}

static bool ti_to_pairs_equal (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *a, dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *b)
{
  if (a->_length != b->_length)
    return false;
  for (uint32_t n = 0; n < a->_length; n++)
  {
    struct DDS_XTypes_TypeObject *to_b = NULL;
    uint32_t m;
    for (m = 0; !to_b && m < b->_length; m++)
    {
      if (!ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier, &b->_buffer[m].type_identifier))
        to_b = &b->_buffer[m].type_object;
    }
    if (!to_b)
      return false;

    dds_ostreamLE_t to_a_ser = { .x = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 } };
    xcdr2_ser (&a->_buffer[n].type_object, &DDS_XTypes_TypeObject_desc, &to_a_ser);
    dds_ostreamLE_t to_b_ser = { .x = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 } };
    xcdr2_ser (to_b, &DDS_XTypes_TypeObject_desc, &to_b_ser);

    if (to_a_ser.x.m_index != to_b_ser.x.m_index)
      return false;
    if (memcmp (to_a_ser.x.m_buffer, to_b_ser.x.m_buffer, to_a_ser.x.m_index))
      return false;

    dds_ostreamLE_fini (&to_a_ser, &dds_cdrstream_default_allocator);
    dds_ostreamLE_fini (&to_b_ser, &dds_cdrstream_default_allocator);
  }
  return true;
}

static bool ti_pairs_equal (dds_sequence_DDS_XTypes_TypeIdentifierPair *a, dds_sequence_DDS_XTypes_TypeIdentifierPair *b)
{
    if (a->_length != b->_length)
    return false;
  for (uint32_t n = 0; n < a->_length; n++)
  {
    bool found = false;
    for (uint32_t m = 0; !found && m < b->_length; m++)
    {
      if (!ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier1, &b->_buffer[m].type_identifier1))
      {
        if (ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier2, &b->_buffer[m].type_identifier2))
          return false;
        found = true;
      }
    }
    if (!found)
      return false;
  }
  return true;
}

static bool tmap_equal (ddsi_typemap_t *a, ddsi_typemap_t *b)
{
  return ti_to_pairs_equal (&a->x.identifier_object_pair_minimal, &b->x.identifier_object_pair_minimal)
      && ti_to_pairs_equal (&a->x.identifier_object_pair_complete, &b->x.identifier_object_pair_complete)
      && ti_pairs_equal (&a->x.identifier_complete_minimal, &b->x.identifier_complete_minimal);
}

#define D(n) TypeBuilderTypes_ ## n ## _desc
CU_TheoryDataPoints (ddsc_typebuilder, topic_desc) = {
  CU_DataPoints (const dds_topic_descriptor_t *,
                 &D(t1), &D(t2), &D(t3), &D(t4), &D(t5), &D(t6), &D(t7), &D(t8),
                 &D(t9), &D(t10), &D(t11), &D(t12), &D(t13), &D(t14), &D(t15), &D(t16),
                 &D(t17), &D(t18), &D(t19), &D(t20), &D(t21), &D(t22), &D(t23), &D(t24),
                 &D(t25), &D(t26), &D(t27), &D(t28), &D(t29), &D(t30), &D(t31), &D(t32),
                 &D(t33), &D(t34), &D(t35), &D(t36), &D(t37), &D(t38), /* TODO &D(t39), */
                 &D(t40), &D(t41), &D(t42), &D(t43), &D(t44), &D(t45), &D(t46), &D(t47),
                 &D(t48), &D(t49), &D(t50), &D(t51), &D(t52), &D(t53), &D(t54), &D(t55),
                 &D(t56), &D(t57), &D(t58), &D(t59), &D(t60), &D(t61), &D(t62),
                 &D(t63), &D(t64), &D(t65), &D(t66), &D(t67), &D(t68), &D(t69),
                 &D(t70), &D(t71), &D(t72), &D(t73) ),
};
#undef D

static void assert_key_names (const dds_topic_descriptor_t *desc, uint32_t nkeys, const char * const *names)
{
  CU_ASSERT_EQ_FATAL (desc->m_nkeys, nkeys);
  for (uint32_t n = 0; n < nkeys; n++)
    CU_ASSERT_STREQ_FATAL (desc->m_keys[n].m_name, names[n]);
}

CU_Test(ddsc_typebuilder, union_key_rule_matrix)
{
  static const char * const t63_keys[] = { "_d" };
  static const char * const t64_keys[] = { "u._d" };
  static const char * const t65_keys[] = { "t1.s1._d" };
  static const char * const t67_keys[] = { "t1.s1._d" };
  static const char * const t68_keys[] = { "t1.s1._d", "t1.s2" };
  static const char * const t69_keys[] = { "t1.s2" };
  static const char * const t73_keys[] = { "u._d" };

  assert_key_names (&TypeBuilderTypes_t63_desc, 1, t63_keys);
  assert_key_names (&TypeBuilderTypes_t64_desc, 1, t64_keys);
  assert_key_names (&TypeBuilderTypes_t65_desc, 1, t65_keys);
  assert_key_names (&TypeBuilderTypes_t66_desc, 0, NULL);
  assert_key_names (&TypeBuilderTypes_t67_desc, 1, t67_keys);
  assert_key_names (&TypeBuilderTypes_t68_desc, 2, t68_keys);
  assert_key_names (&TypeBuilderTypes_t69_desc, 1, t69_keys);
  assert_key_names (&TypeBuilderTypes_t70_desc, 0, NULL);
  assert_key_names (&TypeBuilderTypes_t71_desc, 0, NULL);
  assert_key_names (&TypeBuilderTypes_t72_desc, 0, NULL);
  assert_key_names (&TypeBuilderTypes_t73_desc, 1, t73_keys);
}

CU_Theory((const dds_topic_descriptor_t *desc), ddsc_typebuilder, topic_desc, .init = typebuilder_init, .fini = typebuilder_fini)
{
  char topic_name[100];
  dds_return_t ret;
  dds_entity_t topic;
  struct ddsi_type *type;
  dds_topic_descriptor_t *generated_desc;

  tprintf ("Testing %s [TYPEBUILDER (IDLC)]\n", desc->m_typename);

  create_unique_topic_name ("ddsc_typebuilder", topic_name, sizeof (topic_name));
  topic = dds_create_topic (g_participant, desc, topic_name, NULL, NULL);
  CU_ASSERT_GT_FATAL (topic, 0);

  // generate a topic descriptor
  topic_type_ref (topic, &type);
  generated_desc = dds_alloc (sizeof (*generated_desc));
  ret = ddsi_topic_descriptor_from_type (gv_from_topic (topic), generated_desc, type);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  // check
  tprintf ("size: %u (%u)\n", generated_desc->m_size, desc->m_size);
  CU_ASSERT_EQ_FATAL (desc->m_size, generated_desc->m_size);
  tprintf ("align: %u (%u)\n", generated_desc->m_align, desc->m_align);
  CU_ASSERT_EQ_FATAL (desc->m_align, generated_desc->m_align);
  tprintf ("flagset: %x (%x)\n", generated_desc->m_flagset, desc->m_flagset);
  CU_ASSERT_EQ_FATAL (desc->m_flagset, generated_desc->m_flagset);
  tprintf ("nkeys: %u (%u)\n", generated_desc->m_nkeys, desc->m_nkeys);
  CU_ASSERT_EQ_FATAL (desc->m_nkeys, generated_desc->m_nkeys);
  for (uint32_t n = 0; n < desc->m_nkeys; n++)
  {
    tprintf("key[%u] name: %s (%s)\n", n, generated_desc->m_keys[n].m_name, desc->m_keys[n].m_name);
    CU_ASSERT_STREQ_FATAL (desc->m_keys[n].m_name, generated_desc->m_keys[n].m_name);
    tprintf("  offset: %u (%u)\n", generated_desc->m_keys[n].m_offset, desc->m_keys[n].m_offset);
    CU_ASSERT_EQ_FATAL (desc->m_keys[n].m_offset, generated_desc->m_keys[n].m_offset);
    tprintf("  index: %u (%u)\n", generated_desc->m_keys[n].m_idx, desc->m_keys[n].m_idx);
    CU_ASSERT_EQ_FATAL (desc->m_keys[n].m_idx, generated_desc->m_keys[n].m_idx);
  }
  tprintf ("typename: %s (%s)\n", generated_desc->m_typename, desc->m_typename);
  CU_ASSERT_STREQ_FATAL (desc->m_typename, generated_desc->m_typename);
  tprintf ("nops: %u (%u)\n", generated_desc->m_nops, desc->m_nops);
  CU_ASSERT_EQ_FATAL (desc->m_nops, generated_desc->m_nops);

  uint32_t ops_cnt_gen = dds_stream_countops (generated_desc->m_ops, generated_desc->m_nkeys, generated_desc->m_keys);
  uint32_t ops_cnt = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys);
  tprintf ("ops count: %u (%u)\n", ops_cnt_gen, ops_cnt);
  CU_ASSERT_EQ_FATAL (ops_cnt_gen, ops_cnt);
  for (uint32_t n = 0; n < desc->m_nops; n++)
  {
    if (desc->m_ops[n] != generated_desc->m_ops[n])
    {
      tprintf ("incorrect op at index %u: 0x%08x (0x%08x)\n", n, generated_desc->m_ops[n], desc->m_ops[n]);
      CU_FAIL_FATAL ("different ops");
    }
  }

  tprintf ("typeinfo: %u (%u)\n", generated_desc->type_information.sz, desc->type_information.sz);
  ddsi_typeinfo_t *tinfo = ddsi_typeinfo_deser (desc->type_information.data, desc->type_information.sz);
  ddsi_typeinfo_t *gen_tinfo = ddsi_typeinfo_deser (generated_desc->type_information.data, generated_desc->type_information.sz);
  CU_ASSERT_NEQ_FATAL (ddsi_typeinfo_equal (tinfo, gen_tinfo, DDSI_TYPE_INCLUDE_DEPS), 0);
  ddsi_typeinfo_fini (tinfo);
  ddsrt_free (tinfo);
  ddsi_typeinfo_fini (gen_tinfo);
  ddsrt_free (gen_tinfo);

  tprintf ("typemap: %u (%u)\n", generated_desc->type_mapping.sz, desc->type_mapping.sz);
  ddsi_typemap_t *tmap = ddsi_typemap_deser (desc->type_mapping.data, desc->type_mapping.sz);
  ddsi_typemap_t *gen_tmap = ddsi_typemap_deser (generated_desc->type_mapping.data, generated_desc->type_mapping.sz);
  CU_ASSERT_FATAL (tmap_equal (tmap, gen_tmap));
  ddsi_typemap_fini (tmap);
  ddsrt_free (tmap);
  ddsi_typemap_fini (gen_tmap);
  ddsrt_free (gen_tmap);

  // we don't check restrict_data_representation, this information is not in the type meta-data

  // cleanup
  ddsi_topic_descriptor_fini (generated_desc);
  ddsrt_free (generated_desc);
  topic_type_unref (topic, type);
  tprintf ("\n");
}

CU_Test(ddsc_typebuilder, invalid_toplevel, .init = typebuilder_init, .fini = typebuilder_fini)
{
  char topic_name[100];
  dds_return_t ret;
  dds_entity_t topic;
  struct ddsi_type *type;
  dds_topic_descriptor_t *generated_desc;

  create_unique_topic_name ("ddsc_typebuilder", topic_name, sizeof (topic_name));
  topic = dds_create_topic (g_participant, &TypeBuilderTypes_t2_desc, topic_name, NULL, NULL);
  CU_ASSERT_GT_FATAL (topic, 0);

  // generate a topic descriptor
  topic_type_ref (topic, &type);
  generated_desc = dds_alloc (sizeof (*generated_desc));
  assert (generated_desc);
  for (uint32_t n = 0; n < type->xt._u.structure.members.length; n++)
  {
    ret = ddsi_topic_descriptor_from_type (gv_from_topic (topic), generated_desc, type->xt._u.structure.members.seq[n].type);
    CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  }

  // cleanup
  ddsrt_free (generated_desc);
  topic_type_unref (topic, type);
}

CU_Test(ddsc_typebuilder, alias_toplevel, .init = typebuilder_init, .fini = typebuilder_fini)
{
  char topic_name[100];
  dds_return_t ret;
  dds_entity_t topic;
  struct ddsi_type *type;
  dds_topic_descriptor_t *generated_desc;

  create_unique_topic_name ("ddsc_typebuilder", topic_name, sizeof (topic_name));
  topic = dds_create_topic (g_participant, &TypeBuilderTypes_t48_desc, topic_name, NULL, NULL);
  CU_ASSERT_GT_FATAL (topic, 0);

  // generate a topic descriptor
  topic_type_ref (topic, &type);
  generated_desc = dds_alloc (sizeof (*generated_desc));
  assert (generated_desc);
  assert (type->xt._u.structure.members.length == 1);
  assert (type->xt._u.structure.members.seq[0].type->xt._d == DDS_XTypes_TK_ALIAS);
  ret = ddsi_topic_descriptor_from_type (gv_from_topic (topic), generated_desc, type->xt._u.structure.members.seq[0].type);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  // should be able to create a topic
  char topic_name2[100];
  create_unique_topic_name ("ddsc_typebuilder", topic_name2, sizeof (topic_name2));
  const dds_entity_t topic2 = dds_create_topic (g_participant, generated_desc, topic_name2, NULL, NULL);
  CU_ASSERT_GT_FATAL (topic2, 0);

  // verify its type really is the alias
  struct ddsi_type *type2;
  topic_type_ref (topic2, &type2);
  CU_ASSERT_EQ (type2->xt._d, DDS_XTypes_TK_ALIAS);
  topic_type_unref (topic2, type2);

#if 0
  const dds_entity_t wr = dds_create_writer (g_participant, topic2, NULL, NULL);
  CU_ASSERT_GT_FATAL (wr, 0);
  while (true)
  {
    dds_write (wr, &(TypeBuilderTypes_t48){ .t1 = { .n1 = 33 } });
    dds_sleepfor (DDS_SECS (1));
  }
#endif

  // cleanup
  ddsi_topic_descriptor_fini (generated_desc);
  ddsrt_free (generated_desc);
  topic_type_unref (topic, type);
}

CU_Test(ddsc_typebuilder, union_float128_case, .init = typebuilder_init, .fini = typebuilder_fini)
{
  // IDLC doesn't support long double, so create the FLOAT128 case dynamically.
  dds_dynamic_type_t dtype = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "float128_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_PRIM (DDS_DYNAMIC_FLOAT128, "u1", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  bool found_case = false;
  for (uint32_t n = 0; n + 3 < desc->m_nops; n++)
  {
    if (DDS_OP (desc->m_ops[n]) == DDS_OP_JEQ4)
    {
      found_case = true;
      CU_ASSERT_EQ_FATAL (DDS_OP_TYPE (desc->m_ops[n]), DDS_OP_VAL_16BY);
      CU_ASSERT_EQ_FATAL (desc->m_ops[n + 1], 1u);
      break;
    }
  }
  CU_ASSERT_FATAL (found_case);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dtype);
}

CU_Test(ddsc_typebuilder, union_discriminator_key, .init = typebuilder_init, .fini = typebuilder_fini)
{
  dds_dynamic_type_t dtype = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "disc_key_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_PRIM (DDS_DYNAMIC_INT32, "u1", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_key (&dtype, 0, true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  CU_ASSERT_EQ_FATAL (desc->m_nkeys, 1u);
  CU_ASSERT_STREQ_FATAL (desc->m_keys[0].m_name, "_d");
  const uint32_t kof_offs = desc->m_keys[0].m_offset;
  CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[kof_offs]), DDS_OP_KOF);
  CU_ASSERT_EQ_FATAL (DDS_OP_LENGTH (desc->m_ops[kof_offs]), 1u);

  const uint32_t disc_offs = desc->m_ops[kof_offs + 1];
  CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[disc_offs]), DDS_OP_ADR);
  CU_ASSERT_EQ_FATAL (DDS_OP_TYPE (desc->m_ops[disc_offs]), DDS_OP_VAL_UNI);
  CU_ASSERT_FATAL (desc->m_ops[disc_offs] & DDS_OP_FLAG_KEY);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dtype);
}

static uint32_t find_union_desc_offs (const dds_topic_descriptor_t *desc)
{
  for (uint32_t n = 0; n + 3 < desc->m_nops; n++)
  {
    const uint32_t op = desc->m_ops[n];
    if (DDS_OP (op) == DDS_OP_ADR && DDS_OP_TYPE (op) == DDS_OP_VAL_UNI)
      return n;
  }
  return UINT32_MAX;
}

static void assert_union_case_labels (const dds_topic_descriptor_t *desc, uint32_t nlabels, const uint32_t *labels)
{
  const uint32_t union_offs = find_union_desc_offs (desc);
  CU_ASSERT_NEQ_FATAL (union_offs, UINT32_MAX);
  CU_ASSERT_FATAL (desc->m_ops[union_offs] & DDS_OP_FLAG_DEF);
  CU_ASSERT_EQ_FATAL (desc->m_ops[union_offs + 2], nlabels);

  const int16_t case_jsr = DDS_OP_ADR_JSR (desc->m_ops[union_offs + 3]);
  CU_ASSERT_FATAL (case_jsr > 0);
  uint32_t case_offs = union_offs + (uint32_t) (uint16_t) case_jsr;
  for (uint32_t n = 0; n < nlabels; n++)
  {
    CU_ASSERT_LT_FATAL (case_offs + 1, desc->m_nops);
    CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[case_offs]), DDS_OP_JEQ4);
    CU_ASSERT_EQ_FATAL (desc->m_ops[case_offs + 1], labels[n]);
    case_offs += 4;
  }
}

static void assert_dynamic_type_union_case_labels (dds_dynamic_type_t *dtype, uint32_t nlabels, const uint32_t *labels)
{
  dds_typeinfo_t *type_info;
  dds_return_t ret = dds_dynamic_type_register (dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  assert_union_case_labels (desc, nlabels, labels);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
}

CU_Test(ddsc_typebuilder, union_default_case_last, .init = typebuilder_init, .fini = typebuilder_fini)
{
  dds_dynamic_type_t dtype = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "default_case_last_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM (DDS_DYNAMIC_INT32, "d"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_PRIM (DDS_DYNAMIC_INT32, "u1", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  assert_dynamic_type_union_case_labels (&dtype, 2, (uint32_t[]) { 1, 0 });
  dds_dynamic_type_unref (&dtype);

  dtype = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "default_explicit_case_last_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);

  ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_DEFAULT_LABELS_PRIM (DDS_DYNAMIC_INT32, "d", 1, ((int32_t[]) { 10 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_PRIM (DDS_DYNAMIC_INT32, "u1", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  assert_dynamic_type_union_case_labels (&dtype, 3, (uint32_t[]) { 10, 1, 0 });
  dds_dynamic_type_unref (&dtype);
}

static void assert_mutable_union_case_member_ids (const dds_topic_descriptor_t *desc, uint32_t n_cases, const uint32_t *member_ids)
{
  uint32_t union_offs = UINT32_MAX;
  for (uint32_t n = 0; n + 3 < desc->m_nops; n++)
  {
    const uint32_t op = desc->m_ops[n];
    if (DDS_OP (op) == DDS_OP_PLC &&
        DDS_OP (desc->m_ops[n + 1]) == DDS_OP_ADR &&
        DDS_OP_TYPE (desc->m_ops[n + 1]) == DDS_OP_VAL_UNI)
    {
      union_offs = n + 1;
      break;
    }
  }
  CU_ASSERT_NEQ_FATAL (union_offs, UINT32_MAX);
  CU_ASSERT_EQ_FATAL (desc->m_ops[union_offs + 2], n_cases);

  const int16_t case_jsr = DDS_OP_ADR_JSR (desc->m_ops[union_offs + 3]);
  CU_ASSERT_FATAL (case_jsr > 0);
  const uint32_t case_jsr_offs = (uint32_t) (uint16_t) case_jsr;
  const uint32_t first_case_offs = union_offs + case_jsr_offs;
  for (uint32_t c = 0; c < n_cases; c++)
  {
    const uint32_t case_offs = first_case_offs + 4u * c;
    CU_ASSERT_LT_FATAL (case_offs, desc->m_nops);
    CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[case_offs]), DDS_OP_JEQ4);

    bool found_mid = false;
    for (uint32_t n = 0; n + 1 < desc->m_nops; n++)
    {
      const uint32_t op = desc->m_ops[n];
      if (DDS_OP (op) == DDS_OP_MID && (op & DDS_MID_OFFSET_MASK) == case_offs)
      {
        CU_ASSERT_EQ_FATAL (desc->m_ops[n + 1], member_ids[c]);
        found_mid = true;
        break;
      }
    }
    CU_ASSERT_FATAL (found_mid);
  }
}

CU_Test(ddsc_typebuilder, mutable_union_descriptor, .init = typebuilder_init, .fini = typebuilder_fini)
{
  dds_dynamic_type_t dtype = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "mutable_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_set_extensibility (&dtype, DDS_DYNAMIC_TYPE_EXT_MUTABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_ID_PRIM (DDS_DYNAMIC_INT32, "u1", 77u, 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_key (&dtype, DDS_DYNAMIC_MEMBER_ID_DISCRIMINATOR, true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  enum dds_cdr_type_extensibility ext;
  CU_ASSERT_FATAL (dds_stream_extensibility (desc->m_ops, &ext));
  CU_ASSERT_EQ_FATAL (ext, DDS_CDR_TYPE_EXT_MUTABLE);
  CU_ASSERT_FATAL (dds_stream_data_types (desc->m_ops) & DDS_DATA_TYPE_DEFAULTS_TO_XCDR2);
  CU_ASSERT_FATAL (dds_stream_data_types (desc->m_ops) & DDS_DATA_TYPE_CONTAINS_KEY);

  const uint32_t member_ids[] = { 77u };
  assert_mutable_union_case_member_ids (desc, 1u, member_ids);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dtype);
}

CU_Test(ddsc_typebuilder, mutable_union_multi_label_member_ids, .init = typebuilder_init, .fini = typebuilder_fini)
{
  dds_dynamic_type_t dtype = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "mutable_union_multi_label",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_set_extensibility (&dtype, DDS_DYNAMIC_TYPE_EXT_MUTABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_ID_PRIM (DDS_DYNAMIC_INT32, "u1", 77u, 2, ((int32_t[]) { 1, 2 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dtype,
      DDS_DYNAMIC_UNION_MEMBER_ID_PRIM (DDS_DYNAMIC_INT16, "u2", 88u, 1, ((int32_t[]) { 3 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  const uint32_t member_ids[] = { 77u, 77u, 88u };
  assert_mutable_union_case_member_ids (desc, 3u, member_ids);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dtype);
}

CU_Test(ddsc_typebuilder, nested_mutable_union_descriptor, .init = typebuilder_init, .fini = typebuilder_fini)
{
  dds_dynamic_type_t dunion = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "nested_mutable_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dunion.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_set_extensibility (&dunion, DDS_DYNAMIC_TYPE_EXT_MUTABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dunion,
      DDS_DYNAMIC_UNION_MEMBER_ID_PRIM (DDS_DYNAMIC_INT32, "u1", 77u, 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dstruct = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_STRUCTURE,
    .name = "nested_mutable_union_struct"
  });
  CU_ASSERT_EQ_FATAL (dstruct.ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER (dunion, "u"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dstruct, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  const uint32_t member_ids[] = { 77u };
  assert_mutable_union_case_member_ids (desc, 1u, member_ids);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dstruct);
}

CU_Test(ddsc_typebuilder, union_member_key_uses_discriminator, .init = typebuilder_init, .fini = typebuilder_fini)
{
  dds_dynamic_type_t dunion = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "implicit_disc_key_union",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32)
  });
  CU_ASSERT_EQ_FATAL (dunion.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&dunion,
      DDS_DYNAMIC_UNION_MEMBER_PRIM (DDS_DYNAMIC_INT32, "u1", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dstruct = dds_dynamic_type_create (g_participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_STRUCTURE,
    .name = "implicit_disc_key_struct"
  });
  CU_ASSERT_EQ_FATAL (dstruct.ret, DDS_RETCODE_OK);

  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER (dunion, "u"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_key (&dstruct, 0, true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dstruct, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, g_participant, type_info, 0, &desc);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  CU_ASSERT_EQ_FATAL (desc->m_nkeys, 1u);
  CU_ASSERT_STREQ_FATAL (desc->m_keys[0].m_name, "u._d");
  const uint32_t kof_offs = desc->m_keys[0].m_offset;
  CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[kof_offs]), DDS_OP_KOF);
  CU_ASSERT_EQ_FATAL (DDS_OP_LENGTH (desc->m_ops[kof_offs]), 2u);

  const uint32_t member_offs = desc->m_ops[kof_offs + 1];
  CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[member_offs]), DDS_OP_ADR);
  CU_ASSERT_EQ_FATAL (DDS_OP_TYPE (desc->m_ops[member_offs]), DDS_OP_VAL_EXT);
  CU_ASSERT_FATAL (desc->m_ops[member_offs] & DDS_OP_FLAG_KEY);

  const int16_t member_jsr = DDS_OP_ADR_JSR (desc->m_ops[member_offs + 2]);
  CU_ASSERT_FATAL (member_jsr >= 0);
  const uint32_t member_jsr_offs = (uint32_t) (uint16_t) member_jsr;
  const uint32_t disc_offs = member_offs + member_jsr_offs + desc->m_ops[kof_offs + 2];
  CU_ASSERT_EQ_FATAL (DDS_OP (desc->m_ops[disc_offs]), DDS_OP_ADR);
  CU_ASSERT_EQ_FATAL (DDS_OP_TYPE (desc->m_ops[disc_offs]), DDS_OP_VAL_UNI);
  CU_ASSERT_FATAL (desc->m_ops[disc_offs] & DDS_OP_FLAG_KEY);

  dds_delete_topic_descriptor (desc);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dstruct);
}
