// Copyright (c) 2022 ZettaScale Technology and others
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
#include "dds/features.h"

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/bits.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/xmlparser.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsrt/sockets.h"
#if DDSRT_HAVE_FILESYSTEM
#include "dds/ddsrt/filesystem.h"
#endif
#if DDSRT_HAVE_NETSTAT
#include "dds/ddsrt/netstat.h"
#endif
#if DDSRT_HAVE_RUSAGE
#include "dds/ddsrt/rusage.h"
#endif

#include "dds/ddsi/ddsi_config.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_gc.h"
#ifdef DDS_HAS_TYPELIB
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/ddsi_typebuilder.h"
#endif
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_proxy_endpoint.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_xmsg.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_transmit.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_addrset.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_portmapping.h"
#include "ddsi__ipaddr.h"
#include "ddsi__discovery_addrset.h"

#ifdef DDS_HAS_SECURITY
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_timed_cb.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_shared_secret.h"
#endif

#ifdef DDS_HAS_QOS_PROVIDER
#include "dds/ddsc/dds_public_qos_provider.h"
#endif

#include "dds/ddsc/dds_internal_api.h"
#include "dds/ddsc/dds_loaned_sample.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsc/dds_statistics.h"
#include "dds/ddsc/dds_psmx.h"

#include "dds/cdr/dds_cdrstream.h"

#include "dds__write.h"
#include "dds__writer.h"
#include "dds__entity.h"
#include "dds__sysdef_parser.h"

DDSRT_WARNING_DEPRECATED_OFF
DDSRT_WARNING_GNUC_OFF (unused-result)
DDSRT_WARNING_CLANG_OFF (unused-result)

DDSRT_WARNING_CLANG_OFF(unused-result)
DDSRT_WARNING_GNUC_OFF(unused-result)

#ifdef DDS_HAS_SECURITY
static void test_DDS_Security_Exception_vset (void *ptr, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);
  DDS_Security_Exception_vset (ptr, ptr, 0, 0, msg, ap);
  va_end (ap);
}
#endif

static void test_ddsrt_vasprintf (char **buf, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  ddsrt_vasprintf (buf, fmt, ap);
  va_end (ap);
}

static dds_return_t test_collect_sample (void *arg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *serdata)
{
  (void)arg; (void)si; (void)st; (void)serdata;
  return 0;
}

/* Check that all exported functions are actually exported
   in case of a build that has testing disabled. All newly added
   functions that are exported (including DDSI and DDSRT) should
   be added in this test. */
int main (int argc, char **argv)
{
  (void) argc;
  (void) argv;
  void *ptr = &ptr, *ptr2 = &ptr2, *ptr3 = &ptr3, *ptr4 = &ptr4, *ptr5 = &ptr5, *ptr6 = &ptr6, *ptr7 = &ptr7, *ptr8 = &ptr8;

  /* The functions shouldn't actually be called, just included here so that
     the linker resolves the symbols. An early return (with unreachable code
     warnings disabled) does not work here, because in that case the compiler
     optimizes the code and strips out all function calls. */
  if (dds_time () > 0)
    return 1;

  // dds.h
  dds_enable (1);
  dds_delete (1);
  dds_get_publisher (1);
  dds_get_subscriber (1);
  dds_get_datareader (1);
  dds_get_mask (1, ptr);
  dds_get_instance_handle (1, ptr);
  dds_get_guid (1, ptr);
  dds_read_status (1, ptr, 0);
  dds_take_status (1, ptr, 0);
  dds_get_status_changes (1, ptr);
  dds_get_status_mask (1, ptr);
  dds_set_status_mask (1, 0);
  dds_get_qos (1, ptr);
  dds_set_qos (1, ptr);
  dds_get_listener (1, ptr);
  dds_set_listener (1, ptr);
  dds_create_participant (0, ptr, ptr);
  dds_create_participant_guid (1, ptr, ptr2, 0, ptr3);
  dds_create_domain (0, ptr);
  dds_create_domain_with_rawconfig (0, ptr);
  dds_get_parent (1);
  dds_get_participant (1);
  dds_get_children (1, ptr, 0);
  dds_get_domainid (1, ptr);
  dds_lookup_participant (0, ptr, 0);
  dds_create_topic (1, ptr, ptr, ptr, ptr);
  dds_create_topic_sertype (1, ptr, ptr, ptr, ptr, ptr);
  dds_find_topic (0, 1, ptr, ptr, 0);
  dds_find_topic_scoped (0, 1, ptr, 0);
  dds_create_topic_descriptor (0, 1, ptr, 0, ptr);
  dds_delete_topic_descriptor (ptr);
  dds_get_name (1, ptr, 0);
  dds_get_type_name (1, ptr, 0);
  dds_set_topic_filter_and_arg (1, 0, ptr);
  dds_set_topic_filter_extended (1, ptr);
  dds_get_topic_filter_and_arg (1, ptr, ptr);
  dds_get_topic_filter_extended (1, ptr);
  dds_create_subscriber (1, ptr, ptr);
  dds_create_publisher (1, ptr, ptr);
  dds_suspend (1);
  dds_resume (1);
  dds_wait_for_acks (1, 0);
  dds_create_reader (1, 1, ptr, ptr);
  dds_create_reader_guid (1, 1, ptr, ptr2, ptr3);
  dds_create_reader_rhc (1, 1, ptr, ptr, ptr);
  dds_reader_wait_for_historical_data (1, 0);
  dds_create_writer (1, 1, ptr, ptr);
  dds_create_writer_guid (1, 1, ptr, ptr2, ptr3);
  dds_register_instance (1, ptr, ptr);
  dds_unregister_instance (1, ptr);
  dds_unregister_instance_ih (1, 1);
  dds_unregister_instance_ts (1, ptr, 0);
  dds_unregister_instance_ih_ts (1, 1, 0);
  dds_writedispose (1, ptr);
  dds_writedispose_ts (1, ptr, 0);
  dds_dispose (1, ptr);
  dds_dispose_ts (1, ptr, 0);
  dds_dispose_ih (1, 1);
  dds_dispose_ih_ts (1, 1, 0);
  dds_write (1, ptr);
  dds_write_flush (1);
  dds_writecdr (1, ptr);
  dds_forwardcdr (1, ptr);
  dds_write_ts (1, ptr, 0);
  dds_create_readcondition (1, 0);
  dds_create_querycondition (1, 0, 0);
  dds_create_guardcondition (1);
  dds_set_guardcondition (1, 0);
  dds_read_guardcondition (1, ptr);
  dds_take_guardcondition (1, ptr);
  dds_create_waitset (1);
  dds_waitset_get_entities (1, ptr, 0);
  dds_waitset_attach (1, 1, 0);
  dds_waitset_detach (1, 1);
  dds_waitset_set_trigger (1, 0);
  dds_waitset_wait (1, ptr, 0, 0);
  dds_waitset_wait_until (1, ptr, 0, 0);
  dds_peek (1, ptr, ptr, 0, 0);
  dds_peek_mask (1, ptr, ptr, 0, 0, 0);
  dds_peek_instance (1, ptr, ptr, 0, 0, 1);
  dds_peek_instance_mask (1, ptr, ptr, 0, 0, 1, 0);
  dds_peek_next (1, ptr, ptr);
  dds_read (1, ptr, ptr, 0, 0);
  dds_read_wl (1, ptr, ptr, 0);
  dds_read_mask (1, ptr, ptr, 0, 0, 0);
  dds_read_mask_wl (1, ptr, ptr, 0, 0);
  dds_read_instance (1, ptr, ptr, 0, 0, 1);
  dds_read_instance_wl (1, ptr, ptr, 0, 1);
  dds_read_instance_mask (1, ptr, ptr, 0, 0, 1, 0);
  dds_read_instance_mask_wl (1, ptr, ptr, 0, 1, 0);
  dds_read_next (1, ptr, ptr);
  dds_read_next_wl (1, ptr, ptr);
  dds_take (1, ptr, ptr, 0, 0);
  dds_take_wl (1, ptr, ptr, 0);
  dds_take_mask (1, ptr, ptr, 0, 0, 0);
  dds_take_mask_wl (1, ptr, ptr, 0, 0);
  dds_take_instance (1, ptr, ptr, 0, 0, 1);
  dds_take_instance_wl (1, ptr, ptr, 0, 1);
  dds_take_instance_mask (1, ptr, ptr, 0, 0, 1, 0);
  dds_take_instance_mask_wl (1, ptr, ptr, 0, 1, 0);
  dds_take_next (1, ptr, ptr);
  dds_take_next_wl (1, ptr, ptr);
  dds_peekcdr (1, ptr, 0, ptr, 0);
  dds_peekcdr_instance (1, ptr, 0, ptr, 1, 0);
  dds_readcdr (1, ptr, 0, ptr, 0);
  dds_readcdr_instance (1, ptr, 0, ptr, 1, 0);
  dds_takecdr (1, ptr, 0, ptr, 0);
  dds_takecdr_instance (1, ptr, 0, ptr, 1, 0);
  dds_peek_with_collector (1, 0, 1, 0, test_collect_sample, ptr);
  dds_read_with_collector (1, 0, 1, 0, test_collect_sample, ptr);
  dds_take_with_collector (1, 0, 1, 0, test_collect_sample, ptr);
  dds_lookup_instance (1, ptr);
  dds_instance_get_key (1, 1, ptr);
  dds_begin_coherent (1);
  dds_end_coherent (1);
  dds_notify_readers (1);
  dds_triggered (1);
  dds_get_topic (1);
  dds_get_matched_subscriptions (1, ptr, 0);
  dds_get_matched_subscription_data (1, 1);
  dds_get_matched_publications (1, ptr, 0);
  dds_get_matched_publication_data (1, 1);
#ifdef DDS_HAS_TYPELIB
  dds_builtintopic_get_endpoint_type_info (ptr, ptr);
#endif
  dds_builtintopic_free_endpoint (ptr);
  dds_builtintopic_free_topic (ptr);
  dds_builtintopic_free_participant (ptr);
  dds_assert_liveliness (1);
  dds_domain_set_deafmute (1, 0, 0, 0);
  dds_get_typeobj (1, ptr, 0, ptr);
  dds_free_typeobj (ptr);
  dds_get_typeinfo (1, ptr);
  dds_free_typeinfo (ptr);
  dds_get_entity_sertype (1, ptr);

  // dds_public_loan_api.h
  dds_request_loan (1, ptr);
  dds_return_loan (1, ptr, 0);
  dds_is_shared_memory_available (1);
  dds_request_loan_of_size (1, 0, ptr);
  dds_is_loan_available (1); // deprecated
  dds_loan_sample (1, ptr); // deprecated

  // dds_public_alloc.h
  dds_alloc (0);
  dds_realloc (ptr, 0);
  dds_realloc_zero (ptr, 0);
  dds_free (ptr);
  dds_string_alloc (0);
  dds_string_dup (ptr);
  dds_string_free (ptr);
  dds_sample_free (ptr, ptr, DDS_FREE_ALL);

  // dds_public_impl.h
  dds_write_set_batch (0);

  // dds_public_listener.h
  dds_create_listener (ptr);
  dds_delete_listener (ptr);
  dds_reset_listener (ptr);
  dds_copy_listener (ptr, ptr2);
  dds_merge_listener (ptr, ptr2);
  dds_lset_data_available_arg (ptr, 0, ptr2, 0);
  dds_lset_data_on_readers_arg (ptr, 0, ptr2, 0);
  dds_lset_inconsistent_topic_arg (ptr, 0, ptr2, 0);
  dds_lset_liveliness_changed_arg (ptr, 0, ptr2, 0);
  dds_lset_liveliness_lost_arg (ptr, 0, ptr2, 0);
  dds_lset_offered_deadline_missed_arg (ptr, 0, ptr2, 0);
  dds_lset_offered_incompatible_qos_arg (ptr, 0, ptr2, 0);
  dds_lset_publication_matched_arg (ptr, 0, ptr2, 0);
  dds_lset_requested_deadline_missed_arg (ptr, 0, ptr2, 0);
  dds_lset_requested_incompatible_qos_arg (ptr, 0, ptr2, 0);
  dds_lset_sample_lost_arg (ptr, 0, ptr2, 0);
  dds_lset_sample_rejected_arg (ptr, 0, ptr2, 0);
  dds_lset_subscription_matched_arg (ptr, 0, ptr2, 0);
  dds_lset_inconsistent_topic (ptr, 0);
  dds_lset_liveliness_lost (ptr, 0);
  dds_lset_offered_deadline_missed (ptr, 0);
  dds_lset_offered_incompatible_qos (ptr, 0);
  dds_lset_data_on_readers (ptr, 0);
  dds_lset_sample_lost (ptr, 0);
  dds_lset_data_available (ptr, 0);
  dds_lset_sample_rejected (ptr, 0);
  dds_lset_liveliness_changed (ptr, 0);
  dds_lset_requested_deadline_missed (ptr, 0);
  dds_lset_requested_incompatible_qos (ptr, 0);
  dds_lset_publication_matched (ptr, 0);
  dds_lset_subscription_matched (ptr, 0);
  dds_lget_data_available_arg (ptr, 0, ptr, ptr);
  dds_lget_data_on_readers_arg (ptr, 0, ptr, ptr);
  dds_lget_inconsistent_topic_arg (ptr, 0, ptr, ptr);
  dds_lget_liveliness_changed_arg (ptr, 0, ptr, ptr);
  dds_lget_liveliness_lost_arg (ptr, 0, ptr, ptr);
  dds_lget_offered_deadline_missed_arg (ptr, 0, ptr, ptr);
  dds_lget_offered_incompatible_qos_arg (ptr, 0, ptr, ptr);
  dds_lget_publication_matched_arg (ptr, 0, ptr, ptr);
  dds_lget_requested_deadline_missed_arg (ptr, 0, ptr, ptr);
  dds_lget_requested_incompatible_qos_arg (ptr, 0, ptr, ptr);
  dds_lget_sample_lost_arg (ptr, 0, ptr, ptr);
  dds_lget_sample_rejected_arg (ptr, 0, ptr, ptr);
  dds_lget_subscription_matched_arg (ptr, 0, ptr, ptr);
  dds_lget_inconsistent_topic (ptr, 0);
  dds_lget_liveliness_lost (ptr, 0);
  dds_lget_offered_deadline_missed (ptr, 0);
  dds_lget_offered_incompatible_qos (ptr, 0);
  dds_lget_data_on_readers (ptr, 0);
  dds_lget_sample_lost (ptr, 0);
  dds_lget_data_available (ptr, 0);
  dds_lget_sample_rejected (ptr, 0);
  dds_lget_liveliness_changed (ptr, 0);
  dds_lget_requested_deadline_missed (ptr, 0);
  dds_lget_requested_incompatible_qos (ptr, 0);
  dds_lget_publication_matched (ptr, 0);
  dds_lget_subscription_matched (ptr, 0);

  // dds_public_qos
  dds_create_qos ();
  dds_delete_qos (ptr);
  dds_reset_qos (ptr);
  dds_copy_qos (ptr, ptr2);
  dds_merge_qos (ptr, ptr2);
  dds_qos_equal (ptr, ptr2);
  dds_qset_userdata (ptr, ptr2, 0);
  dds_qset_topicdata (ptr, ptr2, 0);
  dds_qset_groupdata (ptr, ptr2, 0);
  dds_qset_durability (ptr, 0);
  dds_qset_history (ptr, 0, 0);
  dds_qset_resource_limits (ptr, 0, 0, 0);
  dds_qset_presentation (ptr, 0, 0, 0);
  dds_qset_lifespan (ptr, 0);
  dds_qset_deadline (ptr, 0);
  dds_qset_latency_budget (ptr, 0);
  dds_qset_ownership (ptr, 0);
  dds_qset_ownership_strength (ptr, 0);
  dds_qset_liveliness (ptr, 0, 0);
  dds_qset_time_based_filter (ptr, 0);
  dds_qset_partition (ptr, 0, ptr2);
  dds_qset_partition1 (ptr, ptr2);
  dds_qset_reliability (ptr, 0, 0);
  dds_qset_transport_priority (ptr, 0);
  dds_qset_destination_order (ptr, 0);
  dds_qset_writer_data_lifecycle (ptr, 0);
  dds_qset_reader_data_lifecycle (ptr, 0, 0);
  dds_qset_writer_batching (ptr, 0);
  dds_qset_durability_service (ptr, 0, 0, 0, 0, 0, 0);
  dds_qset_ignorelocal (ptr, 0);
  dds_qset_prop (ptr, ptr2, ptr3);
  dds_qunset_prop (ptr, ptr2);
  dds_qset_bprop (ptr, ptr2, ptr3, 0);
  dds_qunset_bprop (ptr, ptr2);
  dds_qset_type_consistency (ptr, 0, 0, 0, 0, 0, 0);
  dds_qset_data_representation (ptr, 0, ptr2);
  dds_qset_entity_name (ptr, ptr2);
  dds_qset_psmx_instances (ptr, 0, ptr2);
  dds_qget_userdata (ptr, ptr2, ptr);
  dds_qget_topicdata (ptr, ptr2, ptr);
  dds_qget_groupdata (ptr, ptr2, ptr);
  dds_qget_durability (ptr, 0);
  dds_qget_history (ptr, 0, ptr);
  dds_qget_resource_limits (ptr, ptr, ptr, ptr);
  dds_qget_presentation (ptr, 0, ptr, ptr);
  dds_qget_lifespan (ptr, ptr);
  dds_qget_deadline (ptr, ptr);
  dds_qget_latency_budget (ptr, ptr);
  dds_qget_ownership (ptr, 0);
  dds_qget_ownership_strength (ptr, ptr);
  dds_qget_liveliness (ptr, 0, ptr);
  dds_qget_time_based_filter (ptr, ptr);
  dds_qget_partition (ptr, ptr, ptr);
  dds_qget_reliability (ptr, 0, ptr);
  dds_qget_transport_priority (ptr, ptr);
  dds_qget_destination_order (ptr, 0);
  dds_qget_writer_data_lifecycle (ptr, ptr);
  dds_qget_reader_data_lifecycle (ptr, ptr, ptr);
  dds_qget_writer_batching (ptr, ptr);
  dds_qget_durability_service (ptr, ptr, 0, ptr, ptr, ptr, ptr);
  dds_qget_ignorelocal (ptr, 0);
  dds_qget_propnames (ptr, ptr, ptr);
  dds_qget_prop (ptr, ptr, ptr);
  dds_qget_bpropnames (ptr, ptr, ptr);
  dds_qget_bprop (ptr, ptr, ptr, ptr);
  dds_qget_type_consistency (ptr, 0, ptr, ptr, ptr, ptr, ptr);
  dds_qget_data_representation (ptr, ptr, ptr);
  dds_qget_entity_name (ptr, ptr);
  dds_qget_psmx_instances (ptr, ptr2, ptr3);

  // dds_public_status.h
  dds_get_inconsistent_topic_status (1, ptr);
  dds_get_publication_matched_status (1, ptr);
  dds_get_liveliness_lost_status (1, ptr);
  dds_get_offered_deadline_missed_status (1, ptr);
  dds_get_offered_incompatible_qos_status (1, ptr);
  dds_get_subscription_matched_status (1, ptr);
  dds_get_liveliness_changed_status (1, ptr);
  dds_get_sample_rejected_status (1, ptr);
  dds_get_sample_lost_status (1, ptr);
  dds_get_requested_deadline_missed_status (1, ptr);
  dds_get_requested_incompatible_qos_status (1, ptr);

#ifdef DDS_HAS_TYPELIB
  // dds_public_dynamic_type.h
  dds_dynamic_member_set_external (ptr, 0, 0);
  dds_dynamic_member_set_hashid (ptr, 0, ptr);
  dds_dynamic_member_set_key (ptr, 0, 0);
  dds_dynamic_member_set_must_understand (ptr, 0, 0);
  dds_dynamic_member_set_optional (ptr, 0, 0);
  dds_dynamic_type_add_bitmask_field (ptr, ptr, 0);
  dds_dynamic_type_add_enum_literal (ptr, ptr, (dds_dynamic_enum_literal_value_t) { .value_kind = 0 }, 0);
  dds_dynamic_type_add_member (ptr, (dds_dynamic_member_descriptor_t) { .name = "dummy" });
  dds_dynamic_type_create (0, (dds_dynamic_type_descriptor_t) { .name = "dummy" });
  dds_dynamic_type_dup (ptr);
  dds_dynamic_type_ref (ptr);
  dds_dynamic_type_register (ptr, ptr);
  dds_dynamic_type_set_autoid (ptr, 0);
  dds_dynamic_type_set_bit_bound (ptr, 0);
  dds_dynamic_type_set_extensibility (ptr, 0);
  dds_dynamic_type_set_nested (ptr, 0);
  dds_dynamic_type_unref (ptr);
#endif

  // dds_rhs.h
  dds_rhc_associate (ptr, NULL, NULL, NULL);
  dds_rhc_store (ptr, NULL, NULL, NULL);
  dds_rhc_unregister_wr (ptr, NULL);
  dds_rhc_relinquish_ownership (ptr, 1);
  dds_rhc_set_qos (ptr, ptr);
  dds_rhc_free (ptr);
  dds_rhc_peek (ptr, 0, 0, 1, ptr, 0, 0);
  dds_rhc_read (ptr, 0, 0, 1, ptr, 0, 0);
  dds_rhc_take (ptr, 0, 0, 1, ptr, 0, 0);
  dds_rhc_add_readcondition (ptr, ptr);
  dds_rhc_remove_readcondition (ptr, ptr);
  dds_reader_data_available_cb (ptr);

  // dds_statistics.h
  dds_create_statistics (1);
  dds_refresh_statistics (ptr);
  dds_delete_statistics (ptr);
  dds_lookup_statistic (ptr, ptr);

  // dds_cdrstream.h
  bool ret_cdrs;
  dds_istream_init (ptr, 0, ptr2, 0);
  dds_istream_fini (ptr);
  dds_ostream_init (ptr, ptr2, 0, 0);
  dds_ostream_fini (ptr, ptr2);
  dds_ostreamLE_init (ptr, ptr2, 0, 0);
  dds_ostreamLE_fini (ptr, ptr2);
  dds_ostreamBE_init (ptr, ptr2, 0, 0);
  dds_ostreamBE_fini (ptr, ptr2);

  ret_cdrs = dds_stream_normalize (ptr, 0, 0, 0, ptr2, 0, ptr3);
  (void) ret_cdrs;
  ret_cdrs = dds_stream_normalize_xcdr2_data (ptr, ptr2, 0, 0, ptr3);
  (void) ret_cdrs;

  dds_stream_write (ptr, ptr2, ptr3, ptr4);
  dds_stream_writeLE (ptr, ptr2, ptr3, ptr4);
  dds_stream_writeBE (ptr, ptr2, ptr3, ptr4);
  dds_stream_write_with_mid (ptr, ptr2, ptr3, ptr4, ptr5);
  dds_stream_write_with_midLE (ptr, ptr2, ptr3, ptr4, ptr5);
  dds_stream_write_with_midBE (ptr, ptr2, ptr3, ptr4, ptr5);
  dds_stream_write_with_byte_order (ptr, ptr2, ptr3, ptr4, ptr5, 0);
  dds_stream_write_sample (ptr, ptr2, ptr3, ptr4);
  dds_stream_write_sampleLE (ptr, ptr2, ptr3, ptr4);
  dds_stream_write_sampleBE (ptr, ptr2, ptr3, ptr4);

  dds_stream_getsize_sample (ptr, ptr, 0);
  dds_stream_getsize_key (ptr, ptr, 0);

  dds_stream_read (ptr, ptr2, ptr3, ptr4);
  dds_stream_read_key (ptr, ptr2, ptr3, ptr4);

  dds_stream_read_sample (ptr, ptr2, ptr3, ptr4);
  dds_stream_free_sample (ptr, ptr2, ptr3);
  dds_stream_countops (ptr, 0, ptr2);
  dds_stream_print_key (ptr, ptr2, ptr3, 0);
  dds_stream_print_sample (ptr, ptr2, ptr3, 0);

  dds_stream_extract_key_from_data (ptr, ptr2, ptr3, ptr4);
  dds_stream_extract_key_from_key (ptr, ptr2, 0, ptr3, ptr4);
  dds_stream_extract_keyBE_from_data (ptr, ptr2, ptr3, ptr4);
  dds_stream_extract_keyBE_from_key (ptr, ptr2, 0, ptr3, ptr4);
  dds_cdrstream_desc_from_topic_desc (ptr, ptr2);
  dds_cdrstream_desc_init_with_nops (ptr, ptr2, 0, 0, 0, ptr3, 0, ptr4, 0);
  dds_cdrstream_desc_init (ptr, ptr2, 0, 0, 0, ptr3, ptr4, 0);
  dds_cdrstream_desc_fini (ptr, ptr2);

  // dds_psmx.h
  dds_add_psmx_endpoint_to_list (ptr, ptr2);
  dds_add_psmx_topic_to_list (ptr, ptr2);
  dds_remove_psmx_endpoint_from_list (ptr, ptr2);
  dds_remove_psmx_topic_from_list (ptr, ptr2);
  dds_psmx_init_generic (ptr);
  dds_psmx_cleanup_generic (ptr);
  dds_psmx_topic_init_generic (ptr, ptr2, ptr3, ptr4, ptr, 0);
  dds_psmx_topic_cleanup_generic (ptr);
  dds_psmx_get_config_option_value (ptr, ptr2);

  // dds_loaned_sample.h
  dds_loaned_sample_ref (ptr);
  dds_loaned_sample_unref (ptr);
  dds_reader_store_loaned_sample (1, ptr);
  dds_reader_store_loaned_sample_wr_metadata (0, ptr, 0, 0, 0);

#ifdef DDS_HAS_SECURITY
  // dds_security_timed_cb.h
  dds_security_timed_dispatcher_new (ptr);
  dds_security_timed_dispatcher_free (ptr);
  dds_security_timed_dispatcher_enable (ptr);
  dds_security_timed_dispatcher_disable (ptr);
  dds_security_timed_dispatcher_add (ptr, 0, 0, ptr);
  dds_security_timed_dispatcher_remove (ptr, 0);

  // dds_security_utils.h
  DDS_Security_BinaryProperty_alloc ();
  DDS_Security_BinaryProperty_deinit (ptr);
  DDS_Security_BinaryProperty_free (ptr);
  DDS_Security_BinaryProperty_copy (ptr, ptr);
  DDS_Security_BinaryProperty_equal (ptr, ptr);
  DDS_Security_BinaryProperty_set_by_value (ptr, ptr, ptr, 0);
  DDS_Security_BinaryProperty_set_by_string (ptr, ptr, ptr);
  DDS_Security_BinaryProperty_set_by_ref (ptr, ptr, ptr, 0);
  DDS_Security_BinaryPropertySeq_alloc ();
  DDS_Security_BinaryPropertySeq_allocbuf (0);
  DDS_Security_BinaryPropertySeq_deinit (ptr);
  DDS_Security_BinaryPropertySeq_free (ptr);
  DDS_Security_Property_alloc ();
  DDS_Security_Property_free (ptr);
  DDS_Security_Property_deinit (ptr);
  DDS_Security_Property_copy (ptr, ptr);
  DDS_Security_Property_equal (ptr, ptr);
  DDS_Security_Property_get_value (ptr, ptr);
  DDS_Security_PropertySeq_alloc ();
  DDS_Security_PropertySeq_allocbuf (0);
  DDS_Security_PropertySeq_freebuf (ptr);
  DDS_Security_PropertySeq_free (ptr);
  DDS_Security_PropertySeq_deinit (ptr);
  DDS_Security_PropertySeq_find_property (ptr, ptr);
  DDS_Security_DataHolder_alloc ();
  DDS_Security_DataHolder_free (ptr);
  DDS_Security_DataHolder_deinit (ptr);
  DDS_Security_DataHolder_copy (ptr, ptr);
  DDS_Security_DataHolder_equal (ptr, ptr);
  DDS_Security_DataHolder_find_property (ptr, ptr);
  DDS_Security_DataHolder_find_binary_property (ptr, ptr);
  DDS_Security_DataHolderSeq_alloc ();
  DDS_Security_DataHolderSeq_allocbuf (0);
  DDS_Security_DataHolderSeq_freebuf (ptr);
  DDS_Security_DataHolderSeq_free (ptr);
  DDS_Security_DataHolderSeq_deinit (ptr);
  DDS_Security_DataHolderSeq_copy (ptr, ptr);
  DDS_Security_ParticipantBuiltinTopicData_alloc ();
  DDS_Security_ParticipantBuiltinTopicData_free (ptr);
  DDS_Security_ParticipantBuiltinTopicData_deinit (ptr);
  DDS_Security_OctetSeq_alloc ();
  DDS_Security_OctetSeq_allocbuf (0);
  DDS_Security_OctetSeq_freebuf (ptr);
  DDS_Security_OctetSeq_free (ptr);
  DDS_Security_OctetSeq_deinit (ptr);
  DDS_Security_OctetSeq_copy (ptr, ptr);
  DDS_Security_HandleSeq_alloc ();
  DDS_Security_HandleSeq_allocbuf (0);
  DDS_Security_HandleSeq_freebuf (ptr);
  DDS_Security_HandleSeq_free (ptr);
  DDS_Security_HandleSeq_deinit (ptr);
  test_DDS_Security_Exception_vset (ptr, " ");
  DDS_Security_Exception_set (ptr, ptr, 0, 0, " ");
  DDS_Security_Exception_reset (ptr);
  DDS_Security_Exception_clean (ptr);
  DDS_Security_PropertyQosPolicy_deinit (ptr);
  DDS_Security_PropertyQosPolicy_free (ptr);
  DDS_Security_set_token_nil (ptr);
  DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit (ptr);
  DDS_Security_basicprotectionkind2transformationkind (ptr, DDS_SECURITY_BASICPROTECTION_KIND_NONE);
  DDS_Security_protectionkind2transformationkind (ptr, DDS_SECURITY_PROTECTION_KIND_NONE);
  DDS_Security_get_conf_item_type (ptr, ptr);
  DDS_Security_normalize_file (ptr);
  DDS_Security_parse_xml_date (ptr);

  // dds_security_serializer.h
  DDS_Security_Serializer_new (0, 0);
  DDS_Security_Serializer_free (ptr);
  DDS_Security_Serializer_buffer (ptr, ptr, ptr);
  DDS_Security_Serialize_PropertySeq (ptr, ptr);
  DDS_Security_Serialize_BinaryPropertyArray (ptr, ptr, 0);
  DDS_Security_Serialize_BinaryPropertySeq (ptr, ptr);
  DDS_Security_Serialize_ParticipantBuiltinTopicData (ptr, ptr);
  DDS_Security_Serialize_KeyMaterial_AES_GCM_GMAC (ptr, ptr);
  DDS_Security_Deserializer_new (ptr, 0);
  DDS_Security_Deserializer_free (ptr);
  DDS_Security_Deserialize_ParticipantBuiltinTopicData (ptr, ptr, ptr);
  DDS_Security_BuiltinTopicKeyBE (ptr, ptr);
  DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC (ptr, ptr);

  // shared_secret.h
  DDS_Security_get_challenge1_from_secret_handle (1);
  DDS_Security_get_challenge2_from_secret_handle (1);
  DDS_Security_get_secret_from_secret_handle (1);
  DDS_Security_get_secret_size_from_secret_handle (1);
#endif

#ifdef DDS_HAS_QOS_PROVIDER
  dds_create_qos_provider(ptr,ptr);
  dds_create_qos_provider_scope(ptr,ptr,ptr);
  dds_qos_provider_get_qos(ptr,0,ptr,ptr);
  dds_delete_qos_provider(ptr);
#endif

  // ddsi_sertype.h
  struct dds_type_consistency_enforcement_qospolicy tce = { 0, false, false, false, false, false };
  ddsi_sertype_v0 (ptr);
  ddsi_sertype_init_props (ptr, ptr, ptr, ptr, 0, 0, 0, 0);
  ddsi_sertype_init_flags (ptr, ptr, ptr, ptr, 0);
  ddsi_sertype_init (ptr, ptr, ptr, ptr, 0);
  ddsi_sertype_fini (ptr);
  ddsi_sertype_ref (ptr);
  ddsi_sertype_unref (ptr);
  ddsi_sertype_compute_serdata_basehash (ptr);
  ddsi_sertype_equal (ptr, ptr);
  ddsi_sertype_hash (ptr);
  ddsi_sertype_extensibility_enc_format (0);
  ddsi_sertype_get_native_enc_identifier (0, 0);
  ddsi_sertype_enc_id_xcdr_version (0);
  ddsi_sertype_enc_id_enc_format (0);
  ddsi_sertype_free (ptr);
  ddsi_sertype_zero_samples (ptr, ptr, 0);
  ddsi_sertype_realloc_samples (ptr, ptr, ptr, 0, 0);
  ddsi_sertype_free_samples (ptr, ptr, 0, DDS_FREE_ALL);
  ddsi_sertype_zero_sample (ptr, ptr);
  ddsi_sertype_alloc_sample (ptr);
  ddsi_sertype_free_sample (ptr, ptr, DDS_FREE_ALL);
  ddsi_sertype_typeid (ptr, 0);
  ddsi_sertype_typemap (ptr);
  ddsi_sertype_typeinfo (ptr);
  ddsi_sertype_derive_sertype (ptr, 0, tce);
  ddsi_sertype_get_serialized_size (ptr, SDK_DATA, ptr, ptr, ptr);
  ddsi_sertype_serialize_into (ptr, SDK_DATA, ptr, ptr, 0);

  // ddsi_serdata.h
  ddsi_serdata_init (ptr, ptr, 0);
  ddsi_serdata_ref (ptr);
  ddsi_serdata_unref (ptr);
  ddsi_serdata_size (ptr);
  ddsi_serdata_from_ser (ptr, 0, ptr, 0);
  ddsi_serdata_from_ser_iov (ptr, 0, 0, ptr, 0);
  ddsi_serdata_from_keyhash (ptr, ptr);
  ddsi_serdata_from_sample (ptr, 0, ptr);
  ddsi_serdata_to_untyped (ptr);
  ddsi_serdata_to_ser (ptr, 0, 0, ptr);
  ddsi_serdata_to_ser_ref (ptr, 0, 0, ptr);
  ddsi_serdata_to_ser_unref (ptr, ptr);
  ddsi_serdata_to_sample (ptr, ptr, ptr, ptr);
  ddsi_serdata_untyped_to_sample (ptr, ptr, ptr, ptr, ptr);
  ddsi_serdata_eqkey (ptr, ptr);
  char buf[10];
  ddsi_serdata_print (ptr, buf, 0);
  ddsi_serdata_print_untyped (ptr, ptr, buf, 0);
  ddsi_serdata_get_keyhash (ptr, ptr, 0);
  ddsi_serdata_from_loaned_sample (ptr, 0, ptr2, ptr3, 0);
  ddsi_serdata_from_psmx (ptr, ptr2);

#ifdef DDS_HAS_TYPELIB
  // ddsi_typewrap.h
  ddsi_typeid_compare (ptr, ptr);
  ddsi_typeid_copy (ptr, ptr);
  ddsi_typeid_dup (ptr);
  ddsi_typeid_ser (ptr, ptr, ptr);
  ddsi_typeid_is_none (ptr);
  ddsi_typeid_is_hash (ptr);
  ddsi_typeid_is_minimal (ptr);
  ddsi_typeid_is_complete (ptr);
  ddsi_typeid_is_fully_descriptive (ptr);
  ddsi_typeid_get_equivalence_hash (ptr, ptr);
  ddsi_typeid_kind (ptr);
  ddsi_typeid_fini (ptr);

  // ddsi_typelib.h
  ddsi_typeinfo_equal (ptr, ptr, 0);
  ddsi_typeinfo_typeid (ptr, 0);
  ddsi_typeinfo_deser (ptr, 0);
  ddsi_typeinfo_fini (ptr);
  ddsi_typeinfo_free (ptr);
  ddsi_typeinfo_dup (ptr);
  ddsi_typeinfo_minimal_typeid (ptr);
  ddsi_typeinfo_complete_typeid (ptr);
  ddsi_typemap_deser (ptr, 0);
  ddsi_typemap_fini (ptr);
  ddsi_typemap_equal (ptr, ptr);
  ddsi_typemap_get_type_name (ptr, ptr2);
  ddsi_type_lookup (ptr, ptr);
  ddsi_type_compare (ptr, ptr);
  ddsi_type_ref (ptr, ptr2, ptr3);
  ddsi_type_unref (ptr, ptr2);
  ddsi_type_add (ptr, ptr2, ptr3, ptr4, ptr5);

  ddsi_make_typeid_str (ptr, ptr);
#endif

  // ddsi_config.h
  ddsi_config_init_default (ptr);

  // ddsi_config_impl.h
  ddsi_config_fini (ptr);

  // ddsi/ddsi_thread.h
  ddsi_lookup_thread_state ();
  ddsi_lookup_thread_state_real ();
  ddsi_thread_state_asleep (ptr);
  ddsi_thread_state_awake (ptr, ptr2);

  // ddsi/ddsi_gc.h
  ddsi_gcreq_new (ptr, ptr);
  ddsi_gcreq_free (ptr);
  ddsi_gcreq_enqueue (ptr);
  ddsi_gcreq_get_arg (ptr);
  ddsi_gcreq_set_arg (ptr, ptr2);

#ifdef DDS_HAS_TYPELIB
  // ddsi/ddsi_typebuilder.h
  ddsi_topic_descriptor_from_type (ptr, ptr2, ptr3);
  ddsi_topic_descriptor_fini (ptr);
#endif

  // ddsi/ddsi_proxy_endpoint.h
  ddsi_new_proxy_writer (ptr, ptr2, ptr3, ptr4, ptr5, ptr6, ptr7, ptr8, ddsrt_time_wallclock(), 0);
#ifdef DDSRT_HAVE_SSM
  ddsi_new_proxy_reader (ptr, ptr2, ptr3, ptr4, ptr5, ptr6, ddsrt_time_wallclock(), 0, 0);
#else
  ddsi_new_proxy_reader (ptr, ptr2, ptr3, ptr4, ptr5, ptr6, ddsrt_time_wallclock(), 0);
#endif

  // ddsi/ddsi_proxy_participant.h
  ddsi_new_proxy_participant (ptr, ptr2, ptr3, 0, ptr5, ptr6, ptr7, 0, (ddsi_vendorid_t) { 0 }, ddsrt_time_wallclock(), 0);

  // ddsi/ddsi_plist.h
  ddsi_plist_init_empty (ptr);
  ddsi_plist_fini (ptr);

  // ddsi/ddsi_xqos.h
  ddsi_xqos_delta (ptr, ptr2, 0);

  // ddsi/ddsi_xmsg.h
  (void) ddsi_xpack_new (ptr, false);
  ddsi_xpack_free (ptr);
  ddsi_xpack_send (ptr, false);

  // ddsi/ddsi_guid.h
  ddsi_hton_guid ((ddsi_guid_t) { 0 });
  ddsi_ntoh_guid ((ddsi_guid_t) { 0 });

  // ddsi/ddsi_tkmap.h
  (void) ddsi_tkmap_lookup_instance_ref (ptr, ptr2);
  ddsi_tkmap_instance_unref (ptr, ptr2);

  // ddsi/ddsi_transmit.h
  ddsi_write_sample_gc (ptr, ptr2, ptr3, ptr4, ptr5);

  // ddsi/ddsi_entity_index.h
  (void) ddsi_entidx_lookup_writer_guid (ptr, ptr2);

  // ddsi/ddsi_addrset.h
  (void) ddsi_ref_addrset (ptr);
  ddsi_unref_addrset (ptr);
  (void) ddsi_new_addrset ();
  ddsi_add_locator_to_addrset (ptr, ptr2, ptr3);

  // ddsi/ddsi_tran.h
  (void) ddsi_locator_to_string (ptr, 0, ptr2);

  // ddsi/ddsi_portmapping.h
  ddsi_get_port_int (ptr, ptr2, 0, 0, 0, ptr3, 0);

  // ddsi__ipaddr.h
  ddsi_ipaddr_to_loc (ptr, ptr2, 0);

  // ddsi__discovery_addrset.h
  ddsi_include_multicast_locator_in_discovery (ptr);


  // ddsrt/atomics.h
  ddsrt_atomic_ld32 (ptr);
  ddsrt_atomic_ldptr (ptr);
  ddsrt_atomic_ldvoidp (ptr);
  ddsrt_atomic_st32 (ptr, 0);
  ddsrt_atomic_stptr (ptr, 0);
  ddsrt_atomic_stvoidp (ptr, ptr);
  ddsrt_atomic_inc32 (ptr);
  ddsrt_atomic_incptr (ptr);
  ddsrt_atomic_inc32_ov (ptr);
  ddsrt_atomic_incptr_ov (ptr);
  ddsrt_atomic_inc32_nv (ptr);
  ddsrt_atomic_incptr_nv (ptr);
  ddsrt_atomic_dec32 (ptr);
  ddsrt_atomic_decptr (ptr);
  ddsrt_atomic_dec32_nv (ptr);
  ddsrt_atomic_decptr_nv (ptr);
  ddsrt_atomic_dec32_ov (ptr);
  ddsrt_atomic_decptr_ov (ptr);
  ddsrt_atomic_add32 (ptr, 0);
  ddsrt_atomic_addptr (ptr, 0);
  ddsrt_atomic_add32_ov (ptr, 0);
  ddsrt_atomic_addptr_ov (ptr, 0);
  ddsrt_atomic_add32_nv (ptr, 0);
  ddsrt_atomic_addptr_nv (ptr, 0);
  ddsrt_atomic_sub32 (ptr, 0);
  ddsrt_atomic_subptr (ptr, 0);
  ddsrt_atomic_sub32_ov (ptr, 0);
  ddsrt_atomic_subptr_ov (ptr, 0);
  ddsrt_atomic_sub32_nv (ptr, 0);
  ddsrt_atomic_subptr_nv (ptr, 0);
  ddsrt_atomic_and32 (ptr, 0);
  ddsrt_atomic_andptr (ptr, 0);
  ddsrt_atomic_and32_ov (ptr, 0);
  ddsrt_atomic_andptr_ov (ptr, 0);
  ddsrt_atomic_and32_nv (ptr, 0);
  ddsrt_atomic_andptr_nv (ptr, 0);
  ddsrt_atomic_or32 (ptr, 0);
  ddsrt_atomic_orptr (ptr, 0);
  ddsrt_atomic_or32_ov (ptr, 0);
  ddsrt_atomic_orptr_ov (ptr, 0);
  ddsrt_atomic_or32_nv (ptr, 0);
  ddsrt_atomic_orptr_nv (ptr, 0);
  ddsrt_atomic_cas32 (ptr, 0, 0);
  ddsrt_atomic_casptr (ptr, 0, 0);
  ddsrt_atomic_casvoidp (ptr, ptr, ptr);
  ddsrt_atomic_fence ();
  ddsrt_atomic_fence_ldld ();
  ddsrt_atomic_fence_stst ();
  ddsrt_atomic_fence_acq ();
  ddsrt_atomic_fence_rel ();
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_ld64 (ptr);
  ddsrt_atomic_st64 (ptr, 0);
  ddsrt_atomic_inc64 (ptr);
  ddsrt_atomic_inc64_ov (ptr);
  ddsrt_atomic_inc64_nv (ptr);
  ddsrt_atomic_dec64 (ptr);
  ddsrt_atomic_dec64_ov (ptr);
  ddsrt_atomic_dec64_nv (ptr);
  ddsrt_atomic_add64 (ptr, 0);
  ddsrt_atomic_add64_ov (ptr, 0);
  ddsrt_atomic_add64_nv (ptr, 0);
  ddsrt_atomic_sub64 (ptr, 0);
  ddsrt_atomic_sub64_ov (ptr, 0);
  ddsrt_atomic_sub64_nv (ptr, 0);
  ddsrt_atomic_and64 (ptr, 0);
  ddsrt_atomic_and64_ov (ptr, 0);
  ddsrt_atomic_and64_nv (ptr, 0);
  ddsrt_atomic_or64 (ptr, 0);
  ddsrt_atomic_or64_ov (ptr, 0);
  ddsrt_atomic_or64_nv (ptr, 0);
  ddsrt_atomic_cas64 (ptr, 0, 0);
#endif

  // ddsrt/bits.h
  ddsrt_ffs32u (0);

  // ddsrt/md5.h
  ddsrt_md5_init (ptr);
  ddsrt_md5_append (ptr, ptr, 0);
  ddsrt_md5_finish (ptr, ptr);

  // ddsrt/mh3.h
  ddsrt_mh3 (ptr, 0, 0);

  // ddsrt/hopscotch.h
  ddsrt_hh_new (0, ptr, ptr);
  ddsrt_hh_free (ptr);
  ddsrt_hh_lookup (ptr, ptr);
  ddsrt_hh_add (ptr, ptr2);
  ddsrt_hh_remove (ptr, ptr2);
  ddsrt_hh_add_absent (ptr, ptr2);
  ddsrt_hh_remove_present (ptr, ptr2);
  ddsrt_hh_enum (ptr, ptr2, ptr3);
  ddsrt_hh_iter_first (ptr, ptr2);
  ddsrt_hh_iter_next (ptr);

  // ddsrt/sync.h
  bool ret_sync;
  ddsrt_mutex_init (ptr);
  ddsrt_mutex_destroy (ptr);
  ddsrt_mutex_lock (ptr);
  ret_sync = ddsrt_mutex_trylock (ptr);
  (void) ret_sync;
  ddsrt_mutex_unlock (ptr);
  ddsrt_cond_init (ptr);
  ddsrt_cond_destroy (ptr);
  ddsrt_cond_wait (ptr,ptr);
  ddsrt_cond_waituntil (ptr, ptr, 0);
  ddsrt_cond_waitfor (ptr, ptr, 0);
  ddsrt_cond_signal (ptr);
  ddsrt_cond_broadcast (ptr);
  ddsrt_rwlock_init (ptr);
  ddsrt_rwlock_destroy (ptr);
  ddsrt_rwlock_read (ptr);
  ddsrt_rwlock_write (ptr);
  ret_sync = ddsrt_rwlock_tryread (ptr);
  (void) ret_sync;
  ret_sync = ddsrt_rwlock_trywrite (ptr);
  (void) ret_sync;
  ddsrt_rwlock_unlock (ptr);
  ddsrt_once (ptr, 0);

  // ddsrt/threads.h
  ddsrt_thread_t t;
  ddsrt_threadattr_init (ptr);
  ddsrt_thread_create (&t, ptr, ptr, ptr, ptr);
  ddsrt_gettid ();
  ddsrt_gettid_for_thread (t);
  ddsrt_thread_self ();
  ddsrt_thread_equal (t, t);
  ddsrt_thread_join (t, ptr);
  ddsrt_thread_getname (ptr, 0);
#if DDSRT_HAVE_THREAD_SETNAME
  ddsrt_thread_setname (ptr);
#endif
#if DDSRT_HAVE_THREAD_LIST
  ddsrt_thread_list (ptr, 0);
  ddsrt_thread_getname_anythread (0, ptr, 0);
#endif
  ddsrt_thread_cleanup_push (0, ptr);
  ddsrt_thread_cleanup_pop (0);
  ddsrt_thread_init (0);
  ddsrt_thread_fini (0);

  // ddsrt/process.h
  ddsrt_getpid ();
  ddsrt_getprocessname ();

  // ddsrt/time.h
  ddsrt_mtime_t mt = { .v = 0};
  ddsrt_wctime_t wct = { .v = 0};
  ddsrt_etime_t et = { .v = 0};
  dds_time ();
  dds_sleepfor (0);
  ddsrt_time_wallclock ();
  ddsrt_time_monotonic ();
  ddsrt_time_elapsed ();
  ddsrt_ctime (0, ptr, 0);
  ddsrt_time_add_duration (0, 0);
  ddsrt_mtime_add_duration (mt, 0);
  ddsrt_wctime_add_duration (wct, 0);
  ddsrt_etime_add_duration (et, 0);
  ddsrt_mtime_to_sec_usec (ptr, ptr2, mt);
  ddsrt_wctime_to_sec_usec (ptr, ptr2, wct);
  ddsrt_etime_to_sec_usec (ptr, ptr2, et);

  // ddsrt/bswap.h
  ddsrt_bswap2u (0);
  ddsrt_bswap2 (0);
  ddsrt_bswap4u (0);
  ddsrt_bswap4 (0);
  ddsrt_bswap8u (0);
  ddsrt_bswap8 (0);

  // ddsrt/random.h
  ddsrt_random ();

  // ddsrt/avl.h
  ddsrt_avl_treedef_init (ptr, 0, 0, ptr, ptr, 0);
  ddsrt_avl_treedef_init_r (ptr, 0, 0, ptr, ptr, ptr, 0);
  ddsrt_avl_init (ptr, ptr);
  ddsrt_avl_free (ptr, ptr, 0);
  ddsrt_avl_free_arg (ptr, ptr, 0, ptr);
  ddsrt_avl_root (ptr, ptr);
  ddsrt_avl_root_non_empty (ptr, ptr);
  ddsrt_avl_lookup (ptr, ptr, ptr);
  ddsrt_avl_lookup_ipath (ptr, ptr, ptr, ptr);
  ddsrt_avl_lookup_dpath (ptr, ptr, ptr, ptr);
  ddsrt_avl_lookup_pred_eq (ptr, ptr, ptr);
  ddsrt_avl_lookup_succ_eq (ptr, ptr, ptr);
  ddsrt_avl_lookup_pred (ptr, ptr, ptr);
  ddsrt_avl_lookup_succ (ptr, ptr, ptr);
  ddsrt_avl_insert (ptr, ptr, ptr);
  ddsrt_avl_delete (ptr, ptr, ptr);
  ddsrt_avl_insert_ipath (ptr, ptr, ptr, ptr);
  ddsrt_avl_delete_dpath (ptr, ptr, ptr, ptr);
  ddsrt_avl_swap_node (ptr, ptr, ptr, ptr);
  ddsrt_avl_augment_update (ptr, ptr);
  ddsrt_avl_is_empty (ptr);
  ddsrt_avl_is_singleton (ptr);
  ddsrt_avl_find_min (ptr, ptr);
  ddsrt_avl_find_max (ptr, ptr);
  ddsrt_avl_find_pred (ptr, ptr, ptr);
  ddsrt_avl_find_succ (ptr, ptr, ptr);
  ddsrt_avl_walk (ptr, ptr, ptr, ptr);
  ddsrt_avl_const_walk (ptr, ptr, ptr, ptr);
  ddsrt_avl_walk_range (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_const_walk_range (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_walk_range_reverse (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_const_walk_range_reverse (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_iter_first (ptr, ptr, ptr);
  ddsrt_avl_iter_succ_eq (ptr, ptr, ptr, ptr);
  ddsrt_avl_iter_succ (ptr, ptr, ptr, ptr);
  ddsrt_avl_iter_next (ptr);

  ddsrt_avl_ctreedef_init (ptr, 0, 0, ptr, ptr, 0);
  ddsrt_avl_ctreedef_init_r (ptr, 0, 0, ptr, ptr, ptr, 0);
  ddsrt_avl_cinit (ptr, ptr);
  ddsrt_avl_cfree (ptr, ptr, ptr);
  ddsrt_avl_cfree_arg (ptr, ptr, ptr, ptr);
  ddsrt_avl_croot (ptr, ptr);
  ddsrt_avl_croot_non_empty (ptr, ptr);
  ddsrt_avl_clookup (ptr, ptr, ptr);
  ddsrt_avl_clookup_ipath (ptr, ptr, ptr, ptr);
  ddsrt_avl_clookup_dpath (ptr, ptr, ptr, ptr);
  ddsrt_avl_clookup_pred_eq (ptr, ptr, ptr);
  ddsrt_avl_clookup_succ_eq (ptr, ptr, ptr);
  ddsrt_avl_clookup_pred (ptr, ptr, ptr);
  ddsrt_avl_clookup_succ (ptr, ptr, ptr);
  ddsrt_avl_cinsert (ptr, ptr, ptr);
  ddsrt_avl_cdelete (ptr, ptr, ptr);
  ddsrt_avl_cinsert_ipath (ptr, ptr, ptr, ptr);
  ddsrt_avl_cdelete_dpath (ptr, ptr, ptr, ptr);
  ddsrt_avl_cswap_node (ptr, ptr, ptr, ptr);
  ddsrt_avl_caugment_update (ptr, ptr);

  ddsrt_avl_cis_empty (ptr);
  ddsrt_avl_cis_singleton (ptr);
  ddsrt_avl_ccount (ptr);

  ddsrt_avl_cfind_min (ptr, ptr);
  ddsrt_avl_cfind_max (ptr, ptr);
  ddsrt_avl_cfind_pred (ptr, ptr, ptr);
  ddsrt_avl_cfind_succ (ptr, ptr, ptr);
  ddsrt_avl_cwalk (ptr, ptr, ptr, ptr);
  ddsrt_avl_cconst_walk (ptr, ptr, ptr, ptr);
  ddsrt_avl_cwalk_range (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_cconst_walk_range (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_cwalk_range_reverse (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_cconst_walk_range_reverse (ptr, ptr, ptr, ptr, ptr, ptr);
  ddsrt_avl_citer_first (ptr, ptr, ptr);
  ddsrt_avl_citer_succ_eq (ptr, ptr, ptr, ptr);
  ddsrt_avl_citer_succ (ptr, ptr, ptr, ptr);
  ddsrt_avl_citer_next (ptr);

  // ddsrt/fibheap.h
  ddsrt_fibheap_def_init (ptr, 0, 0);
  ddsrt_fibheap_init (ptr, ptr);
  ddsrt_fibheap_min (ptr, ptr);
  ddsrt_fibheap_merge (ptr, ptr, ptr);
  ddsrt_fibheap_insert (ptr, ptr, ptr);
  ddsrt_fibheap_delete (ptr, ptr, ptr);
  ddsrt_fibheap_extract_min (ptr, ptr);
  ddsrt_fibheap_decrease_key (ptr, ptr, ptr);

#if DDSRT_HAVE_NETSTAT
  // ddsrt/netstat.h
  ddsrt_netstat_new (ptr, ptr);
  ddsrt_netstat_free (ptr);
  ddsrt_netstat_get (ptr, ptr);
#endif

#if DDSRT_HAVE_RUSAGE && DDSRT_HAVE_THREAD_LIST
  // ddsrt/rusage.h
  ddsrt_thread_list_id_t tids[1];
  ddsrt_thread_list (tids, 1);
  ddsrt_getrusage (0, ptr);
  ddsrt_getrusage_anythread (tids[0], (ddsrt_rusage_t *) ptr);
#endif

  // ddsrt/environ.h
  ddsrt_getenv (ptr, ptr);
  ddsrt_setenv (ptr, ptr);
  ddsrt_unsetenv (ptr);
  ddsrt_expand_envvars (ptr, 0);
  ddsrt_expand_envvars_sh (ptr, 0);

  // ddsrt/retcode.h
  dds_strretcode (0);

  // ddsrt/log.h
  dds_log (0, ptr, 0, ptr, " ");
  dds_set_log_mask (0); // ROS 2 rmw_cyclonedds_cpp needs this, probably erroneously
  dds_get_log_mask ();

  // ddsrt/sockets.h
#if DDSRT_HAVE_GETHOSTNAME
  ddsrt_gethostname (ptr, 0);
#endif

  // ddsrt/heap.h
  ddsrt_allocation_ops_t ao = {0,0,0,0};
  ddsrt_malloc (1);
  ddsrt_malloc_s (1);
  ddsrt_calloc (1, 1);
  ddsrt_calloc_s (1, 1);
  ddsrt_realloc (ptr, 1);
  ddsrt_realloc_s (ptr, 1);
  ddsrt_free (ptr);
  ddsrt_set_allocator(ao);

  // ddsrt/string.h
  ddsrt_strcasecmp (ptr, ptr);
  ddsrt_strncasecmp (ptr, ptr, 1);
  ddsrt_strsep (ptr, ptr);
  ddsrt_memdup (ptr, 1);
  ddsrt_strdup (ptr);
  ddsrt_strndup (ptr, 1);
  ddsrt_strlcpy (ptr, ptr2, 1);
  ddsrt_strlcat (ptr, ptr2, 1);
  ddsrt_str_replace (ptr, ptr2, ptr3, 1);
  ddsrt_str_trim_ord_space (ptr);

  // ddsrt/strtol.h
  ddsrt_todigit (0);
  ddsrt_strtoll (ptr, ptr, 0, ptr);
  ddsrt_strtoull (ptr, ptr, 0, ptr);
  ddsrt_atoll (ptr, ptr);
  ddsrt_atoull (ptr, ptr);
  ddsrt_lltostr (0, ptr, 0, ptr);
  ddsrt_ulltostr (0, ptr, 0, ptr);

  // ddsrt/xmlparser.h
  ddsrt_xmlp_new_file (ptr, ptr, ptr);
  ddsrt_xmlp_new_string (ptr, ptr, ptr);
  ddsrt_xmlp_set_options (ptr, 0);
  ddsrt_xmlp_get_bufpos (ptr);
  ddsrt_xmlp_free (ptr);
  ddsrt_xmlp_parse (ptr);

#if DDSRT_HAVE_FILESYSTEM
  // ddsrt/filesystem.h
  ddsrt_opendir (ptr, ptr);
  ddsrt_closedir (ptr);
  ddsrt_readdir (ptr, ptr);
  ddsrt_stat (ptr, ptr);
  ddsrt_file_normalize (ptr);
  ddsrt_file_sep ();
#endif

  // ddsrt/io.h
  test_ddsrt_vasprintf (ptr, " ");
  ddsrt_asprintf (ptr, " ");

  // ddsrt/ifaddrs.h
  ddsrt_getifaddrs (ptr, ptr);
  ddsrt_freeifaddrs (ptr);

  // ddsrt/sockets.h
  ddsrt_sockaddrfromstr (0, ptr, ptr2);

  // dds__write.h
  dds_write_impl (ptr, ptr, 0, (dds_write_action) 0);
  dds_writecdr_impl (ptr, ptr, ptr, false);

  // dds__writer.h
  dds_writer_psmx_loan_raw (ptr, ptr2, 0, 0, 0);
  dds_writer_psmx_loan_from_serdata (ptr, ptr2);

  // dds__entity.h
  dds_entity_pin (0, ptr);
  dds_entity_unpin (ptr);
  dds_entity_lock (0, 0, ptr);
  dds_entity_unlock (ptr);

  // dds__sysdef_parser.h
  dds_sysdef_init_sysdef (ptr, ptr2, 0);
  dds_sysdef_init_sysdef_str (ptr, ptr2, 0);
  dds_sysdef_fini_sysdef (ptr);
  dds_sysdef_init_data_types (ptr, ptr2);
  dds_sysdef_init_data_types_str (ptr, ptr2);
  dds_sysdef_fini_data_types (ptr);

  return 0;
}

#undef EXP
