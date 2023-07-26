// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_xt_typelookup.h"
#include "ddsi__xt_impl.h"
#include "ddsi__addrset.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__typelookup.h"
#include "ddsi__typewrap.h"
#include "ddsi__vendor.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds__entity.h"
#include "config_env.h"
#include "test_common.h"
#include "xtypes_common.h"

#include "XSpace.h"
#include "XSpaceEnum.h"
#include "XSpaceMustUnderstand.h"
#include "XSpaceTypeConsistencyEnforcement.h"
#include "XSpaceNoTypeInfo.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_publisher1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;
static dds_entity_t g_subscriber2 = 0;

typedef void (*sample_init) (void *s);
typedef void (*sample_check) (void *s1, void *s2);

static void xtypes_typeinfo_init (void)
{
  /* Domains for pub and sub use a different internal domain id, but the external
   * domain id in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  char *conf1 = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID_PUB);
  char *conf2 = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID_SUB);
  g_domain1 = dds_create_domain (DDS_DOMAINID_PUB, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID_SUB, conf2);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
  g_participant2 = dds_create_participant (DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL (g_participant2 > 0);

  g_publisher1 = dds_create_publisher (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_publisher1 > 0);
  g_subscriber2 = dds_create_subscriber (g_participant2, NULL, NULL);
  CU_ASSERT_FATAL (g_subscriber2 > 0);
}

static void xtypes_typeinfo_fini (void)
{
  dds_delete (g_domain2);
  dds_delete (g_domain1);
}

/* Invalid hashed type (with valid hash type id) as top-level type */
CU_Test (ddsc_xtypes_typeinfo, invalid_top_level_local_hash, .init = xtypes_typeinfo_init, .fini = xtypes_typeinfo_fini)
{
  char topic_name[100];
  dds_topic_descriptor_t desc;
  DDS_XTypes_TypeInformation *ti;

  for (uint32_t n = 0; n < 6; n++)
  {
    // coverity[store_writes_const_field]
    memcpy (&desc, &XSpace_to_toplevel_desc, sizeof (desc));
    typeinfo_deser (&ti, &desc.type_information);
    if (n % 2)
    {
      ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
      ddsi_typeid_copy_impl (&ti->minimal.typeid_with_size.type_id, &ti->minimal.dependent_typeids._buffer[n / 2].type_id);
    }
    else
    {
      ddsi_typeid_fini_impl (&ti->complete.typeid_with_size.type_id);
      ddsi_typeid_copy_impl (&ti->complete.typeid_with_size.type_id, &ti->complete.dependent_typeids._buffer[n / 2].type_id);
    }
    typeinfo_ser (&desc.type_information, ti);

    create_unique_topic_name ("ddsc_xtypes_typeinfo", topic_name, sizeof (topic_name));
    dds_entity_t topic = dds_create_topic (g_participant1, &desc, topic_name, NULL, NULL);
    CU_ASSERT_FATAL (topic < 0);

    ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
    ddsrt_free (ti);
    ddsrt_free ((void *) desc.type_information.data);
  }
}

/* Non-hashed type (with valid hash type id) as top-level type */
CU_Test (ddsc_xtypes_typeinfo, invalid_top_level_local_non_hash, .init = xtypes_typeinfo_init, .fini = xtypes_typeinfo_fini)
{
  char topic_name[100];

  dds_topic_descriptor_t desc;
  // coverity[store_writes_const_field]
  memcpy (&desc, &XSpace_to_toplevel_desc, sizeof (desc));

  DDS_XTypes_TypeInformation *ti;
  typeinfo_deser (&ti, &desc.type_information);

  ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
  ti->minimal.typeid_with_size.type_id._d = DDS_XTypes_TK_UINT32;
  typeinfo_ser (&desc.type_information, ti);

  create_unique_topic_name ("ddsc_xtypes_typeinfo", topic_name, sizeof (topic_name));
  dds_entity_t topic = dds_create_topic (g_participant1, &desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic < 0);

  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
  ddsrt_free ((void *) desc.type_information.data);
}

static void mod_toplevel (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_flags = 0x7f;
}

static void mod_inherit (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  ddsi_typeid_fini_impl (&type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.header.base_type);
  ddsi_typeid_copy_impl (&type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.header.base_type,
      &type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id);
}

static void mod_uniondisc (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_UNION);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.union_type.discriminator.common.type_id._d = DDS_XTypes_TK_FLOAT32;
}

static void mod_unionmembers (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_UNION);
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._u.union_type.member_seq._length == 2);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.union_type.member_seq._buffer[0].common.member_flags |= DDS_XTypes_IS_DEFAULT;
}

static void mod_arraybound (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *type_id_obj_seq, uint32_t kind)
{
  assert (kind == DDS_XTypes_EK_MINIMAL);
  (void) kind;
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._d == DDS_XTypes_TK_STRUCTURE);
  assert (type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id._d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL);
  type_id_obj_seq->_buffer[0].type_object._u.minimal._u.struct_type.member_seq._buffer[0].common.member_type_id._u.array_sdefn.array_bound_seq._buffer[0] = 5;
}


#define D(n) XSpace_ ## n ## _desc
CU_TheoryDataPoints (ddsc_xtypes_typeinfo, invalid_type_object_local) = {
  CU_DataPoints (const char *,                    "invalid flag, non-matching typeid",
  /*                                              |               */"invalid flag, matching typeid",
  /*                                              |                |               */"invalid inheritance",
  /*                                              |                |                |              */"invalid union discr",
  /*                                              |                |                |               |                */"union multiple default",
  /*                                              |                |                |               |                 |                   */"array bound overflow"),
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(to_toplevel), &D(to_toplevel), &D(to_inherit), &D(to_uniondisc), &D(to_unionmembers), &D(to_arraybound) ),
  CU_DataPoints (typeobj_modify,                  mod_toplevel,    mod_toplevel,    mod_inherit,    mod_uniondisc,    mod_unionmembers,    mod_arraybound    ),
  CU_DataPoints (bool,                            false,           true,            true,           true,             true,                true              ),
};
#undef D

CU_Theory ((const char *test_descr, const dds_topic_descriptor_t *topic_desc, typeobj_modify mod, bool matching_typeinfo), ddsc_xtypes_typeinfo, invalid_type_object_local, .init = xtypes_typeinfo_init, .fini = xtypes_typeinfo_fini)
{
  char topic_name[100];
  printf("Test invalid_type_object_local: %s\n", test_descr);

  dds_topic_descriptor_t desc;
  xtypes_util_modify_type_meta (&desc, topic_desc, mod, matching_typeinfo, DDS_XTypes_EK_MINIMAL);

  // test that topic creation fails
  create_unique_topic_name ("ddsc_xtypes_typeinfo", topic_name, sizeof (topic_name));
  dds_entity_t topic = dds_create_topic (g_participant1, &desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic < 0);

  if (matching_typeinfo)
    ddsrt_free ((void *) desc.type_information.data);
  ddsrt_free ((void *) desc.type_mapping.data);
}

/* Invalid hashed type (with valid hash type id) as top-level type for proxy endpoint */
CU_Test (ddsc_xtypes_typeinfo, invalid_top_level_remote_hash, .init = xtypes_typeinfo_init, .fini = xtypes_typeinfo_fini)
{
  dds_topic_descriptor_t desc;
  DDS_XTypes_TypeInformation *ti;
  struct ddsi_domaingv *gv = get_domaingv (g_participant1);
  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes_typeinfo", topic_name, sizeof (topic_name));

  // create local topic so that types are in type lib and resolved
  dds_entity_t topic = dds_create_topic (g_participant1, &XSpace_to_toplevel_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  // create type id with invalid top-level
  // coverity[store_writes_const_field]
  memcpy (&desc, &XSpace_to_toplevel_desc, sizeof (desc));
  typeinfo_deser (&ti, &desc.type_information);
  ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
  ddsi_typeid_copy_impl (&ti->minimal.typeid_with_size.type_id, &ti->minimal.dependent_typeids._buffer[0].type_id);

  // create proxy reader with modified type
  struct ddsi_guid pp_guid, rd_guid;
  gen_test_guid (gv, &pp_guid, DDSI_ENTITYID_PARTICIPANT);
  gen_test_guid (gv, &rd_guid, DDSI_ENTITYID_KIND_READER_NO_KEY);
  test_proxy_rd_create (gv, topic_name, ti, DDS_RETCODE_BAD_PARAMETER, &pp_guid, &rd_guid);

  // clean up
  test_proxy_rd_fini (gv, &pp_guid, &rd_guid);
  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
}

/* Invalid type object for proxy endpoint */
#define D(n) XSpace_ ## n ## _desc
CU_TheoryDataPoints (ddsc_typelookup, invalid_type_object_remote) = {
  CU_DataPoints (const char *,
  /*                                             */"invalid flag",
  /*                                              |               */"invalid inheritance",
  /*                                              |                |              */"invalid union discr"),
  CU_DataPoints (const dds_topic_descriptor_t *,  &D(to_toplevel), &D(to_inherit), &D(to_uniondisc) ),
  CU_DataPoints (typeobj_modify,                  mod_toplevel,    mod_inherit,    mod_uniondisc    )
};
#undef D

#ifdef DDS_HAS_TYPE_DISCOVERY
static void test_proxy_rd_matches (dds_entity_t wr, bool exp_match)
{
  struct dds_entity *x;
  dds_return_t rc = dds_entity_pin (wr, &x);
  CU_ASSERT_EQUAL_FATAL (rc, DDS_RETCODE_OK);
  struct dds_writer *dds_wr = (struct dds_writer *) x;
  CU_ASSERT_EQUAL_FATAL (dds_wr->m_wr->num_readers, exp_match ? 1 : 0);
  dds_entity_unpin (x);
}
#endif

CU_Theory ((const char *test_descr, const dds_topic_descriptor_t *topic_desc, typeobj_modify mod), ddsc_typelookup, invalid_type_object_remote, .init = xtypes_typeinfo_init, .fini = xtypes_typeinfo_fini)
{
#ifdef DDS_HAS_TYPE_DISCOVERY
  struct ddsi_domaingv *gv = get_domaingv (g_participant1);
  printf("Test invalid_type_object_remote: %s\n", test_descr);

  char topic_name[100];
  create_unique_topic_name ("ddsc_typelookup", topic_name, sizeof (topic_name));

  // local writer
  dds_entity_t topic = dds_create_topic (g_participant1, topic_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);
  dds_entity_t wr = dds_create_writer (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  dds_topic_descriptor_t desc;
  xtypes_util_modify_type_meta (&desc, topic_desc, mod, true, DDS_XTypes_EK_MINIMAL);

  DDS_XTypes_TypeInformation *ti;
  typeinfo_deser (&ti, &desc.type_information);
  struct ddsi_guid pp_guid, rd_guid;
  gen_test_guid (gv, &pp_guid, DDSI_ENTITYID_PARTICIPANT);
  gen_test_guid (gv, &rd_guid, DDSI_ENTITYID_KIND_READER_NO_KEY);
  test_proxy_rd_create (gv, topic_name, ti, DDS_RETCODE_OK, &pp_guid, &rd_guid);
  test_proxy_rd_matches (wr, false);

  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;

  DDS_XTypes_TypeMapping *tmap;
  typemap_deser (&tmap, &desc.type_mapping);
  DDS_Builtin_TypeLookup_Reply reply = {
    .header = { .remoteEx = DDS_RPC_REMOTE_EX_OK, .relatedRequestId = { .sequence_number = { .low = 1, .high = 0 }, .writer_guid = { .guidPrefix = { 0 }, .entityId = { .entityKind = DDSI_EK_WRITER, .entityKey = { 0 } } } } },
    .return_data = { ._d = DDS_Builtin_TypeLookup_getTypes_HashId, ._u = { .getType = { ._d = DDS_RETCODE_OK, ._u = { .result =
      { .types = { ._length = tmap->identifier_object_pair_minimal._length, ._maximum = tmap->identifier_object_pair_minimal._maximum, ._release = false, ._buffer = tmap->identifier_object_pair_minimal._buffer } } } } } }
    };
  ddsi_tl_add_types (gv, &reply, &gpe_match_upd, &n_match_upd);

  // expect no match because of invalid types
  CU_ASSERT_EQUAL_FATAL (n_match_upd, 0);
  ddsrt_free (gpe_match_upd);

  struct ddsi_type *type = ddsi_type_lookup (gv, (ddsi_typeid_t *) &tmap->identifier_object_pair_minimal._buffer[0].type_identifier);
  CU_ASSERT_PTR_NOT_NULL_FATAL (type);
  CU_ASSERT_EQUAL_FATAL (type->state, DDSI_TYPE_INVALID);

  // clean up
  test_proxy_rd_fini (gv, &pp_guid, &rd_guid);
  ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
  ddsrt_free (ti);
  ddsi_typemap_fini ((ddsi_typemap_t *) tmap);
  ddsrt_free (tmap);
  ddsrt_free ((void *) desc.type_information.data);
  ddsrt_free ((void *) desc.type_mapping.data);
#else
  (void) test_descr;
  (void) topic_desc;
  (void) mod;
#endif /* DDS_HAS_TYPE_DISCOVERY */
}

CU_Test (ddsc_xtypes_typeinfo, get_type_info, .init = xtypes_typeinfo_init, .fini = xtypes_typeinfo_fini)
{
  char topic_name[100];
  create_unique_topic_name ("ddsc_xtypes_typeinfo", topic_name, sizeof (topic_name));

  dds_entity_t topic = dds_create_topic (g_participant1, &XSpace_XType1_desc, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);
  dds_entity_t wr = dds_create_writer (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  dds_entity_t rd = dds_create_reader (g_participant1, topic, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  dds_typeinfo_t *type_info_tp, *type_info_wr, *type_info_rd;
  dds_return_t ret;
  ret = dds_get_typeinfo (topic, &type_info_tp);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (wr, &type_info_wr);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_get_typeinfo (rd, &type_info_rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ddsi_typeinfo_equal (type_info_tp, type_info_wr, DDSI_TYPE_INCLUDE_DEPS));
  CU_ASSERT_FATAL (ddsi_typeinfo_equal (type_info_tp, type_info_rd, DDSI_TYPE_INCLUDE_DEPS));

  dds_free_typeinfo (type_info_tp);
  dds_free_typeinfo (type_info_wr);
  dds_free_typeinfo (type_info_rd);
}
