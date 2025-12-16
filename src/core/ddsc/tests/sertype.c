// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/dds.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_typewrap.h"
#include "test_common.h"
#include "test_util.h"
#include "SertypeData.h"

CU_Test (ddsc_sertype_default, compare)
{
  dds_return_t ret;
  dds_entity_t domain = dds_create_domain (0, NULL);
  CU_ASSERT_GEQ_FATAL (domain, 0);
  dds_entity_t participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (participant, 0);

  char topic_name[100];
  create_unique_topic_name ("ddsc_dynamic_type", topic_name, sizeof (topic_name));

  dds_topic_descriptor_t SertypeDefaultCompare1a_desc = SertypeDefaultCompare2_desc;
  SertypeDefaultCompare1a_desc.m_typename = "SertypeDefaultCompare1";
  dds_entity_t topic1 = dds_create_topic (participant, &SertypeDefaultCompare1_desc, topic_name, NULL, NULL);
  dds_entity_t topic2 = dds_create_topic (participant, &SertypeDefaultCompare1a_desc, topic_name, NULL, NULL);

  dds_entity_t rd = dds_create_reader (participant, topic1, NULL, NULL);
  dds_entity_t wr = dds_create_writer (participant, topic2, NULL, NULL);
  sync_reader_writer (participant, rd, participant, wr);

  dds_typeinfo_t *rd_type_info, *wr_type_info;
  const struct ddsi_sertype *rd_sertype, *wr_sertype;

  ret = dds_get_entity_sertype (rd, &rd_sertype);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  ret = dds_get_entity_sertype (wr, &wr_sertype);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);

#ifdef DDS_HAS_TYPELIB
  ret = dds_get_typeinfo (rd, &rd_type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (wr, &wr_type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  // Minimal types should be equal, but complete types different because of annotation on type 1a
  const ddsi_typeid_t * rd_type_min = ddsi_typeinfo_minimal_typeid (rd_type_info);
  const ddsi_typeid_t * wr_type_min = ddsi_typeinfo_minimal_typeid (wr_type_info);
  const ddsi_typeid_t * rd_type_compl = ddsi_typeinfo_complete_typeid (rd_type_info);
  const ddsi_typeid_t * wr_type_compl = ddsi_typeinfo_complete_typeid (wr_type_info);

  CU_ASSERT_EQ (ddsi_typeid_compare (rd_type_min, wr_type_min), 0);
  CU_ASSERT_NEQ (ddsi_typeid_compare (rd_type_compl, wr_type_compl), 0);

  // Sertypes should be different, because of different complete types
  CU_ASSERT_NEQ (rd_sertype, wr_sertype);
#else
  ret = dds_get_typeinfo (rd, &rd_type_info);
  CU_ASSERT_NEQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (rd, &wr_type_info);
  CU_ASSERT_NEQ_FATAL (ret, DDS_RETCODE_OK);

  // Sertypes should be the same, because other than type-info, types are equal
  CU_ASSERT_EQ (rd_sertype, wr_sertype);
#endif

  dds_free_typeinfo (rd_type_info);
  dds_free_typeinfo (wr_type_info);
}

