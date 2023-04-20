// Copyright(c) 2022 ZettaScale Technology and others
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

#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/dds.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds__qos.h"
#include "dds__listener.h"

#include "test_common.h"

#ifdef DDS_HAS_TYPE_DISCOVERY
#include "ddsi__xt_impl.h"
#endif

/* Calling API functions on an uninitialized library should fail with
   PRECONDITION_NOT_MET, BAD_PARAMETER or UNSUPPORTED.

   A few exceptions exist that initialize the library:
   - dds_create_domain
   - dds create_participant
   - dds_lookup_participant
   - dds_write_set_batch dds_create_waitset
   - dds_create_guardcondition

   So those we don't bother checking here.  Another bunch of functions doesn't do anything
   interesting, like dds_free_typeinfo, and those we also skip.

   The arguments passed are all arguments that are not trivially invalid, like a null
   pointer where a non-null pointer is expected. */

static void check (dds_return_t res)
{
  CU_ASSERT (res == DDS_RETCODE_PRECONDITION_NOT_MET || res == DDS_RETCODE_BAD_PARAMETER || res == DDS_RETCODE_UNSUPPORTED);
}

static void check_0 (void *ptr)
{
  CU_ASSERT (ptr == 0);
}

static void check_ih (dds_instance_handle_t ih)
{
  CU_ASSERT (ih == 0);
}

static bool filter_fn (const void *sample) { (void) sample; return true; }
static bool filter_arg_fn (const void *sample, void *arg) { (void) sample; (void) arg; return true; }

DDSRT_WARNING_DEPRECATED_OFF

CU_Test (ddsc_uninitialized, various)
{
  check (dds_enable (1));
  check (dds_delete (1));
  check (dds_get_publisher (1));
  check (dds_get_subscriber (1));
  check (dds_get_datareader (1));
  check (dds_get_parent (1));
  check (dds_get_participant (1));
  check (dds_set_status_mask (1, 1));
  uint32_t mask;
  check (dds_get_mask (1, &mask));
  check (dds_get_status_changes (1, &mask));
  check (dds_get_status_mask (1, &mask));
  check (dds_read_status (1, &mask, 1));
  check (dds_take_status (1, &mask, 1));
  dds_instance_handle_t ih;
  check (dds_get_instance_handle (1, &ih));
  dds_domainid_t domainid;
  check (dds_get_domainid (1, &domainid));
  dds_guid_t guid;
  check (dds_get_guid (1, &guid));
  dds_qos_t qos = { 0 };
  check (dds_get_qos (1, &qos));
  check (dds_set_qos (1, &qos));
  dds_listener_t listener = { 0 };
  check (dds_get_listener (1, &listener));
  check (dds_set_listener (1, &listener));
  dds_entity_t es[3];
  check (dds_get_children (1, es, sizeof (es) / sizeof (*es)));

  check (dds_create_topic (1, &Space_Type1_desc, "aap", NULL, NULL));

  struct ddsi_sertype sertype = { 0 }, *psertype = &sertype;
  check (dds_create_topic_sertype (1, "aap", &psertype, NULL, NULL, NULL));

  check (dds_find_topic (DDS_FIND_SCOPE_LOCAL_DOMAIN, 1, "aap", NULL, 1));
  check (dds_find_topic_scoped (DDS_FIND_SCOPE_LOCAL_DOMAIN, 1, "aap", 1));

  dds_topic_descriptor_t *desc;
  check (dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, 1, NULL, 1, &desc));

  char buf[20];
  check (dds_get_name (1, buf, sizeof (buf)));
  check (dds_get_type_name (1, buf, sizeof (buf)));

  struct dds_topic_filter filter = {
    .mode = DDS_TOPIC_FILTER_SAMPLE,
    .f = { .sample = filter_fn },
    .arg = NULL
  };
  check (dds_set_topic_filter_and_arg (1, filter_arg_fn, NULL));
  check (dds_set_topic_filter_extended (1, &filter));

  check (dds_get_topic_filter_and_arg (1, NULL, NULL));
  check (dds_get_topic_filter_extended (1, &filter));

  check (dds_create_subscriber (1, NULL, NULL));
  check (dds_create_publisher (1, NULL, NULL));
  check (dds_suspend (1));
  check (dds_resume (1));
  check (dds_wait_for_acks (1, 0));

  check (dds_create_reader (1, 2, NULL, NULL));
  struct dds_rhc rhc = { 0 };
  check (dds_create_reader_rhc (1, 2, NULL, NULL, &rhc));
  check (dds_reader_wait_for_historical_data (1, 0));

  check (dds_create_writer (1, 2, NULL, NULL));
  char data = 0;
  check (dds_register_instance (1, &ih, &data));
  check (dds_unregister_instance (1, &data));
  check (dds_unregister_instance_ih (1, 1));
  check (dds_unregister_instance_ts (1, &data, 1));
  check (dds_unregister_instance_ih_ts (1, 1, 1));
  check (dds_writedispose (1, &data));
  check (dds_writedispose_ts (1, &data, 1));
  check (dds_dispose (1, &data));
  check (dds_dispose_ts (1, &data, 1));
  check (dds_dispose_ih (1, 1));
  check (dds_dispose_ih_ts (1, 1, 1));
  check (dds_write (1, &data));
  dds_write_flush (1);
  struct ddsi_serdata serdata = { 0 };
  check (dds_writecdr (1, &serdata));
  check (dds_forwardcdr (1, &serdata));
  check (dds_write_ts (1, &data, 1));

  check (dds_create_readcondition (1, DDS_ANY_STATE));
  check (dds_create_querycondition (1, DDS_ANY_STATE, filter_fn));

  check (dds_set_guardcondition (1, true));
  bool triggered;
  check (dds_read_guardcondition (1, &triggered));
  check (dds_take_guardcondition (1, &triggered));

  check (dds_waitset_get_entities (1, es, sizeof (es) / sizeof (*es)));
  check (dds_waitset_attach (1, 2, 3));
  check (dds_waitset_detach (1, 2));
  check (dds_waitset_set_trigger (1, true));
  check (dds_waitset_wait (1, NULL, 0, 0));
  check (dds_waitset_wait_until (1, NULL, 0, DDS_NEVER));

  void *raw = NULL;
  dds_sample_info_t si;
  check (dds_read (1, &raw, &si, 1, 1));
  check (dds_read_wl (1, &raw, &si, 1));
  check (dds_read_mask (1, &raw, &si, 1, 1, DDS_ANY_STATE));
  check (dds_read_mask_wl (1, &raw, &si, 1, DDS_ANY_STATE));
  check (dds_read_instance (1, &raw, &si, 1, 1, 1));
  check (dds_read_instance_wl (1, &raw, &si, 1, 1));
  check (dds_read_instance_mask (1, &raw, &si, 1, 1, 1, DDS_ANY_STATE));
  check (dds_read_instance_mask_wl (1, &raw, &si, 1, 1, DDS_ANY_STATE));
  check (dds_read_next (1, &raw, &si));
  check (dds_read_next_wl (1, &raw, &si));
  check (dds_take (1, &raw, &si, 1, 1));
  check (dds_take_wl (1, &raw, &si, 1));
  check (dds_take_mask (1, &raw, &si, 1, 1, DDS_ANY_STATE));
  check (dds_take_mask_wl (1, &raw, &si, 1, DDS_ANY_STATE));
  check (dds_take_instance (1, &raw, &si, 1, 1, 1));
  check (dds_take_instance_wl (1, &raw, &si, 1, 1));
  check (dds_take_instance_mask (1, &raw, &si, 1, 1, 1, DDS_ANY_STATE));
  check (dds_take_instance_mask_wl (1, &raw, &si, 1, 1, DDS_ANY_STATE));
  check (dds_take_next (1, &raw, &si));
  check (dds_take_next_wl (1, &raw, &si));
  struct ddsi_serdata *pserdata = NULL;
  check (dds_readcdr (1, &pserdata, 1, &si, DDS_ANY_STATE));
  check (dds_readcdr_instance (1, &pserdata, 1, &si, 1, DDS_ANY_STATE));
  check (dds_takecdr (1, &pserdata, 1, &si, DDS_ANY_STATE));
  check (dds_takecdr_instance (1, &pserdata, 1, &si, 1, DDS_ANY_STATE));

  check (dds_return_loan (1, &raw, 1));
  check_ih (dds_lookup_instance (1, &data));
  check (dds_instance_get_key (1, 1, &data));

  check (dds_begin_coherent (1));
  check (dds_end_coherent (1));
  check (dds_notify_readers (1));
  check (dds_triggered (1));
  check (dds_get_topic (1));
  check (dds_get_matched_subscriptions (1, &ih, 1));
  check_0 (dds_get_matched_subscription_data (1, ih));
  check (dds_get_matched_publications (1, &ih, 1));
  check_0 (dds_get_matched_publication_data (1, ih));

  check (dds_assert_liveliness (1));
  check (dds_domain_set_deafmute (1, true, true, 1));

#ifdef DDS_HAS_TYPE_DISCOVERY
  const dds_typeid_t typeid = { 0 }; // FIXME: get a real one
  dds_typeobj_t *typeobj;
  check (dds_get_typeobj (1, &typeid, 1, &typeobj));
  dds_typeinfo_t *typeinfo;
  check (dds_get_typeinfo (1, &typeinfo));
#endif
}
