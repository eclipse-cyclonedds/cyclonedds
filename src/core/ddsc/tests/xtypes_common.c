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

#include "dds/dds.h"
#include "dds/version.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_xt_typelookup.h"
#include "dds/ddsi/ddsi_thread.h"
#include "ddsi__xt_impl.h"
#include "ddsi__typelookup.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__typewrap.h"
#include "ddsi__addrset.h"
#include "ddsi__vendor.h"
#include "test_common.h"
#include "xtypes_common.h"

void typeinfo_ser (struct dds_type_meta_ser *ser, DDS_XTypes_TypeInformation *ti)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  xcdr2_ser (ti, &DDS_XTypes_TypeInformation_desc, &os);
  ser->data = os.m_buffer;
  ser->sz = os.m_index;
}

void typeinfo_deser (DDS_XTypes_TypeInformation **ti, const struct dds_type_meta_ser *ser)
{
  xcdr2_deser (ser->data, ser->sz, (void **) ti, &DDS_XTypes_TypeInformation_desc);
}

void typemap_ser (struct dds_type_meta_ser *ser, DDS_XTypes_TypeMapping *tmap)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  xcdr2_ser (tmap, &DDS_XTypes_TypeMapping_desc, &os);
  ser->data = os.m_buffer;
  ser->sz = os.m_index;
}

void typemap_deser (DDS_XTypes_TypeMapping **tmap, const struct dds_type_meta_ser *ser)
{
  xcdr2_deser (ser->data, ser->sz, (void **) tmap, &DDS_XTypes_TypeMapping_desc);
}

void test_proxy_rd_create (struct ddsi_domaingv *gv, const char *topic_name, DDS_XTypes_TypeInformation *ti, dds_return_t exp_ret, const ddsi_guid_t *pp_guid, const ddsi_guid_t *rd_guid)
{
  ddsi_plist_t *plist = ddsrt_calloc (1, sizeof (*plist));
  plist->qos.present |= DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME | DDSI_QP_TYPE_INFORMATION | DDSI_QP_DATA_REPRESENTATION | DDSI_QP_LIVELINESS;
  plist->qos.topic_name = ddsrt_strdup (topic_name);
  plist->qos.type_name = ddsrt_strdup ("dummy");
  plist->qos.type_information = ddsi_typeinfo_dup ((struct ddsi_typeinfo *) ti);
  plist->qos.data_representation.value.n = 1;
  plist->qos.data_representation.value.ids = ddsrt_calloc (1, sizeof (*plist->qos.data_representation.value.ids));
  plist->qos.data_representation.value.ids[0] = DDS_DATA_REPRESENTATION_XCDR2;
  plist->qos.liveliness.kind = DDS_LIVELINESS_AUTOMATIC;
  plist->qos.liveliness.lease_duration = DDS_INFINITY;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, gv);
  struct ddsi_addrset *as = ddsi_new_addrset ();
  ddsi_add_locator_to_addrset (gv, as, &gv->loc_default_uc);
  ddsi_ref_addrset (as); // increase refc to 2, new_proxy_participant does not add a ref
  int rc = ddsi_new_proxy_participant (gv, pp_guid, 0, NULL, as, as, plist, DDS_INFINITY, DDSI_VENDORID_ECLIPSE, 0, ddsrt_time_wallclock (), 1);
  CU_ASSERT_FATAL (rc);

  ddsi_xqos_mergein_missing (&plist->qos, &ddsi_default_qos_reader, ~(uint64_t)0);
#ifdef DDS_HAS_SSM
  rc = ddsi_new_proxy_reader (gv, pp_guid, rd_guid, as, plist, ddsrt_time_wallclock (), 1, 0);
#else
  rc = ddsi_new_proxy_reader (gv, pp_guid, rd_guid, as, plist, ddsrt_time_wallclock (), 1);
#endif
  CU_ASSERT_EQUAL_FATAL (rc, exp_ret);
  ddsi_plist_fini (plist);
  ddsrt_free (plist);
  ddsi_thread_state_asleep (thrst);
}

void test_proxy_rd_fini (struct ddsi_domaingv *gv, const ddsi_guid_t *pp_guid, const ddsi_guid_t *rd_guid)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, gv);
  ddsi_delete_proxy_reader (gv, rd_guid, ddsrt_time_wallclock (), false);
  ddsi_delete_proxy_participant_by_guid (gv, pp_guid, ddsrt_time_wallclock (), false);
  ddsi_thread_state_asleep (thrst);
}

void xtypes_util_modify_type_meta (dds_topic_descriptor_t *dst_desc, const dds_topic_descriptor_t *src_desc, typeobj_modify mod, bool update_typeinfo, uint32_t kind)
{
  // coverity[store_writes_const_field]
  memcpy (dst_desc, src_desc, sizeof (*dst_desc));

  DDS_XTypes_TypeInformation *ti = NULL;
  if (update_typeinfo)
    typeinfo_deser (&ti, &dst_desc->type_information);

  DDS_XTypes_TypeMapping *tmap = NULL;
  typemap_deser (&tmap, &dst_desc->type_mapping);

  if (update_typeinfo)
  {
    assert (ti);
    // confirm that top-level type is the first in type map
    if (kind == DDS_XTypes_EK_MINIMAL || kind == DDS_XTypes_EK_BOTH)
    {
      assert (!ddsi_typeid_compare_impl (&ti->minimal.typeid_with_size.type_id, &tmap->identifier_object_pair_minimal._buffer[0].type_identifier));
      ddsi_typeid_fini_impl (&ti->minimal.typeid_with_size.type_id);
      for (uint32_t n = 0; n < tmap->identifier_object_pair_minimal._length; n++)
        ddsi_typeid_fini_impl (&tmap->identifier_object_pair_minimal._buffer[n].type_identifier);
    }
    if (kind == DDS_XTypes_EK_COMPLETE || kind == DDS_XTypes_EK_BOTH)
    {
      assert (!ddsi_typeid_compare_impl (&ti->complete.typeid_with_size.type_id, &tmap->identifier_object_pair_complete._buffer[0].type_identifier));
      ddsi_typeid_fini_impl (&ti->complete.typeid_with_size.type_id);
      for (uint32_t n = 0; n < tmap->identifier_object_pair_complete._length; n++)
        ddsi_typeid_fini_impl (&tmap->identifier_object_pair_complete._buffer[n].type_identifier);
    }
  }

  // modify the specified object in the type mapping
  if (kind == DDS_XTypes_EK_MINIMAL || kind == DDS_XTypes_EK_BOTH)
    mod (&tmap->identifier_object_pair_minimal, DDS_XTypes_EK_MINIMAL);
  if (kind == DDS_XTypes_EK_COMPLETE || kind == DDS_XTypes_EK_BOTH)
    mod (&tmap->identifier_object_pair_complete, DDS_XTypes_EK_COMPLETE);

  if (update_typeinfo)
  {
    // get hash-id for modified type and store in type map and replace top-level type id
    if (kind == DDS_XTypes_EK_MINIMAL || kind == DDS_XTypes_EK_BOTH)
    {
      for (uint32_t n = 0; n < tmap->identifier_object_pair_minimal._length; n++)
      {
        ddsi_typeid_t type_id;
        ddsi_typeobj_get_hash_id (&tmap->identifier_object_pair_minimal._buffer[n].type_object, &type_id);
        ddsi_typeid_copy_impl (&tmap->identifier_object_pair_minimal._buffer[n].type_identifier, &type_id.x);
        ddsi_typeid_fini (&type_id);
      }
      ddsi_typeid_copy_impl (&ti->minimal.typeid_with_size.type_id, &tmap->identifier_object_pair_minimal._buffer[0].type_identifier);
    }
    if (kind == DDS_XTypes_EK_COMPLETE || kind == DDS_XTypes_EK_BOTH)
    {
      for (uint32_t n = 0; n < tmap->identifier_object_pair_complete._length; n++)
      {
        ddsi_typeid_t type_id;
        ddsi_typeobj_get_hash_id (&tmap->identifier_object_pair_complete._buffer[n].type_object, &type_id);
        ddsi_typeid_copy_impl (&tmap->identifier_object_pair_complete._buffer[n].type_identifier, &type_id.x);
        ddsi_typeid_fini (&type_id);
      }
      ddsi_typeid_copy_impl (&ti->complete.typeid_with_size.type_id, &tmap->identifier_object_pair_complete._buffer[0].type_identifier);
    }
  }

  // replace the type map and type info in the topic descriptor with updated ones
  if (update_typeinfo)
    typeinfo_ser (&dst_desc->type_information, ti);
  typemap_ser (&dst_desc->type_mapping, tmap);

  // clean up
  ddsi_typemap_fini ((ddsi_typemap_t *) tmap);
  ddsrt_free (tmap);

  if (update_typeinfo)
  {
    ddsi_typeinfo_fini ((ddsi_typeinfo_t *) ti);
    ddsrt_free (ti);
  }
}
