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

#include <string.h>

#include "CUnit/Test.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dyntypelib.h"

CU_Test (dyntypelib, inherited_key)
{
  dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);
  struct dyntypelib *dtl = dtl_new (participant);
  struct dyntypelib_error err;
  CU_ASSERT_EQ_FATAL (dtl_add_xml_type_library (dtl, DYNTYPELIB_TEST_DATA_DIR "/inheritance.xml", &err), DDS_RETCODE_OK);

  struct dyntype *derived = dtl_lookup_typename (dtl, "::Model::Inner::Derived");
  CU_ASSERT_NEQ_FATAL (derived, NULL);
  const DDS_XTypes_CompleteTypeObject *type = &derived->typeobj->_u.complete;
  const DDS_XTypes_CompleteStructType *st = &type->_u.struct_type;
  CU_ASSERT_NEQ_FATAL (st->header.base_type._d, DDS_XTypes_TK_NONE);
  CU_ASSERT_EQ_FATAL (st->member_seq._length, 1u);
  CU_ASSERT_EQ (st->member_seq._buffer[0].common.member_id, 4u);

  dds_topic_descriptor_t *desc;
  CU_ASSERT_EQ_FATAL (dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, participant, derived->typeinfo, 0, &desc), DDS_RETCODE_OK);
  CU_ASSERT_EQ (desc->m_nkeys, 1u);
  struct dds_cdrstream_desc cdr;
  CU_ASSERT_EQ_FATAL (dds_cdrstream_desc_from_topic_desc (&cdr, desc), DDS_RETCODE_OK);
  struct elem *input = domtree_from_file (DYNTYPELIB_TEST_DATA_DIR "/derived_sample.xml");
  CU_ASSERT_NEQ_FATAL (input, NULL);
  void *sample = dtl_scan_sample (dtl, input, type, false, &err);
  CU_ASSERT_NEQ_FATAL (sample, NULL);

  const uint8_t expected_key[] = { 1, 2, 3, 4 };
  const uint8_t expected_data[] = { 1, 2, 3, 4, 0xaa, 0xcc, 0xbb };
  dds_ostreamBE_t os;
  dds_ostreamBE_init (&os, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
  CU_ASSERT_FATAL (dds_stream_write_sampleBE (&os, &dds_cdrstream_default_allocator, sample, &cdr));
  CU_ASSERT_MEMEQ (os.x.m_buffer, os.x.m_index, expected_data, sizeof (expected_data));
  os.x.m_index = 0;
  CU_ASSERT_FATAL (dds_stream_write_keyBE (&os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, sample, &cdr));
  CU_ASSERT_MEMEQ (os.x.m_buffer, os.x.m_index, expected_key, sizeof (expected_key));

  dds_ostreamBE_fini (&os, &dds_cdrstream_default_allocator);
  ddsrt_free (sample);
  domtree_free (input);
  dds_cdrstream_desc_fini (&cdr, &dds_cdrstream_default_allocator);
  dds_delete_topic_descriptor (desc);
  dtl_free (dtl);
  CU_ASSERT_EQ (dds_delete (participant), DDS_RETCODE_OK);
}

CU_Test (dyntypelib, recursive_inheritance)
{
  dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);
  struct dyntypelib *dtl = dtl_new (participant);
  struct dyntypelib_error err;
  CU_ASSERT_EQ_FATAL (dtl_add_xml_type_library (dtl, DYNTYPELIB_TEST_DATA_DIR "/recursive_inheritance.xml", &err), DDS_RETCODE_OK);

  const struct dyntype *base = dtl_lookup_typename (dtl, "::Branch");
  const struct dyntype *derived = dtl_lookup_typename (dtl, "::Leaf");
  CU_ASSERT_NEQ_FATAL (base, NULL);
  CU_ASSERT_NEQ_FATAL (derived, NULL);
  const DDS_XTypes_CompleteStructType *bst = &base->typeobj->_u.complete._u.struct_type;
  const DDS_XTypes_CompleteStructType *dst = &derived->typeobj->_u.complete._u.struct_type;
  CU_ASSERT_EQ_FATAL (bst->member_seq._length, 1u);
  CU_ASSERT_EQ_FATAL (dst->member_seq._length, 1u);
  CU_ASSERT_STREQ (bst->member_seq._buffer[0].detail.name, "children");
  CU_ASSERT_STREQ (dst->member_seq._buffer[0].detail.name, "value");
  CU_ASSERT_EQ (bst->member_seq._buffer[0].common.member_id, 0u);
  CU_ASSERT_EQ (dst->member_seq._buffer[0].common.member_id, 1u);
  CU_ASSERT_NEQ (dst->header.base_type._d, DDS_XTypes_TK_NONE);

  /* Building the descriptor starting at the base must still give sequence
     elements the complete derived layout, including their base. */
  dds_topic_descriptor_t *desc;
  CU_ASSERT_EQ_FATAL (dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, participant, base->typeinfo, 0, &desc), DDS_RETCODE_OK);
  struct dds_cdrstream_desc cdr;
  CU_ASSERT_EQ_FATAL (dds_cdrstream_desc_from_topic_desc (&cdr, desc), DDS_RETCODE_OK);
  struct elem *input = domtree_from_file (DYNTYPELIB_TEST_DATA_DIR "/recursive_sample.xml");
  CU_ASSERT_NEQ_FATAL (input, NULL);
  void *sample = dtl_scan_sample (dtl, input, &base->typeobj->_u.complete, false, &err);
  CU_ASSERT_NEQ_FATAL (sample, NULL);
  const uint8_t expected[] = {
    0, 0, 0, 24, 0, 0, 0, 20, 0, 0, 0, 1, /* branch, sequence, length */
    0, 0, 0, 12, 0, 0, 0, 4, 0, 0, 0, 0,  /* leaf and its empty sequence */
    0, 0, 0, 42
  };
  dds_ostreamBE_t os;
  dds_ostreamBE_init (&os, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
  CU_ASSERT_FATAL (dds_stream_write_sampleBE (&os, &dds_cdrstream_default_allocator, sample, &cdr));
  CU_ASSERT_MEMEQ (os.x.m_buffer, os.x.m_index, expected, sizeof (expected));
  dds_ostreamBE_fini (&os, &dds_cdrstream_default_allocator);
  dds_stream_free_sample (sample, &dds_cdrstream_default_allocator, desc->m_ops);
  ddsrt_free (sample);
  domtree_free (input);
  dds_cdrstream_desc_fini (&cdr, &dds_cdrstream_default_allocator);
  dds_delete_topic_descriptor (desc);
  dtl_free (dtl);
  CU_ASSERT_EQ (dds_delete (participant), DDS_RETCODE_OK);
}

CU_Test (dyntypelib, invalid_base_type)
{
  static const struct {
    const char *path;
    const char *diagnostic;
  } cases[] = {
    { DYNTYPELIB_TEST_DATA_DIR "/unknown_base.xml", "Missing" },
    { DYNTYPELIB_TEST_DATA_DIR "/nonstruct_base.xml", "Alias" },
    { DYNTYPELIB_TEST_DATA_DIR "/cyclic_base.xml", "cyclic" }
  };
  dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);
  struct dyntypelib *dtl = dtl_new (participant);
  struct dyntypelib_error err = { 0 };
  CU_ASSERT_EQ_FATAL (dtl_add_xml_type_library (dtl, DYNTYPELIB_TEST_DATA_DIR "/inheritance.xml", &err), DDS_RETCODE_OK);
  const struct dyntype *derived = dtl_lookup_typename (dtl, "::Model::Inner::Derived");
  CU_ASSERT_NEQ_FATAL (derived, NULL);
  for (size_t i = 0; i < sizeof (cases) / sizeof (cases[0]); i++)
  {
    CU_ASSERT_NEQ (dtl_add_xml_type_library (dtl, cases[i].path, &err), DDS_RETCODE_OK);
    CU_ASSERT_NEQ (strstr (err.errmsg, cases[i].diagnostic), NULL);
  }
  CU_ASSERT_EQ (dtl_lookup_typename (dtl, "::Derived"), NULL);
  CU_ASSERT_EQ (dtl_lookup_typename (dtl, "::Alias"), NULL);
  CU_ASSERT_EQ (dtl_lookup_typename (dtl, "::First"), NULL);
  CU_ASSERT_EQ (dtl_lookup_typename (dtl, "::Second"), NULL);
  CU_ASSERT_EQ_FATAL (dtl_add_xml_type_library (dtl, DYNTYPELIB_TEST_DATA_DIR "/recursive_inheritance.xml", &err), DDS_RETCODE_OK);
  CU_ASSERT_EQ (dtl_lookup_typename (dtl, "::Model::Inner::Derived"), derived);
  CU_ASSERT_NEQ (dtl_lookup_typename (dtl, "::Leaf"), NULL);
  dtl_free (dtl);
  CU_ASSERT_EQ (dds_delete (participant), DDS_RETCODE_OK);
}
