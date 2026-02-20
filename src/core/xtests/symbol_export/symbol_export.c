// Copyright (c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>

#include "dds/config.h"     // Exports values for #if sections in test
#include "dds/ddsrt/arch.h" // Exports values for DDSRT_64BIT
#include "dds/features.h"   // Exports values for #if sections in test

#include "symbol_export/ddsc_loader.h"

/* Check that all exported functions are actually exported
   in case of a build that has testing disabled. All newly added
   functions that are exported (including DDSI and DDSRT) should
   be added in this test. */
int main() {
  void *ddsc_library_handle = DDSC_LIBRARY_HANDLE();
  if (ddsc_library_handle == NULL) {
    printf("Unable to load %s library\n", DDSC_LIBRARY_NAME);
    return EXIT_FAILURE;
  }

  int result = EXIT_SUCCESS;

  // dds.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_enable");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_delete");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_publisher");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_subscriber");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_datareader");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_instance_handle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_guid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_status_changes");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_status_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_set_status_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_set_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_set_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_participant");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_create_participant_guid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_domain");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_create_domain_with_rawconfig");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_parent");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_participant");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_children");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_domainid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lookup_participant");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_topic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_topic_sertype");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_find_topic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_find_topic_scoped");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_create_topic_descriptor");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_delete_topic_descriptor");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_name");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_type_name");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_set_topic_filter_and_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_set_topic_filter_extended");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_topic_filter_and_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_topic_filter_extended");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_subscriber");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_publisher");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_suspend");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_resume");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_wait_for_acks");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_reader");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_reader_guid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_reader_rhc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_reader_wait_for_historical_data");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_writer");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_writer_guid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_register_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_unregister_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_unregister_instance_ih");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_unregister_instance_ts");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_unregister_instance_ih_ts");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_writedispose");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_writedispose_ts");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dispose");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dispose_ts");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dispose_ih");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dispose_ih_ts");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_write");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_write_flush");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_writecdr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_forwardcdr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_write_ts");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_readcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_create_querycondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_create_guardcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_set_guardcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_guardcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_guardcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_waitset");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_waitset_get_entities");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_waitset_attach");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_waitset_detach");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_waitset_set_trigger");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_waitset_wait");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_waitset_wait_until");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peek");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peek_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peek_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peek_instance_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peek_next");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_mask_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_instance_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_instance_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_read_instance_mask_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_next");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_next_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_mask_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_instance_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_instance_mask");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_take_instance_mask_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_next");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_next_wl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peekcdr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peekcdr_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_readcdr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_readcdr_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_takecdr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_takecdr_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_peek_with_collector");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_read_with_collector");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_take_with_collector");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lookup_instance");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_instance_get_key");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_begin_coherent");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_end_coherent");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_notify_readers");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_triggered");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_topic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_matched_subscriptions");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_matched_subscription_data");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_matched_publications");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_matched_publication_data");
#ifdef DDS_HAS_TYPELIB
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_builtintopic_get_endpoint_type_info");
#endif
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_builtintopic_free_endpoint");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_builtintopic_free_topic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_builtintopic_free_participant");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_assert_liveliness");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_domain_set_deafmute");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_typeobj");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_free_typeobj");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_typeinfo");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_free_typeinfo");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_entity_sertype");

  // dds_public_loan_api.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_request_loan");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_return_loan");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_is_shared_memory_available");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_request_loan_of_size");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_is_loan_available"); // deprecated
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_loan_sample"); // deprecated

  // dds_public_alloc.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_realloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_realloc_zero");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_string_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_string_dup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_string_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_sample_free");

  // dds_public_impl.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_write_set_batch");

  // dds_public_listener.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_delete_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_reset_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_copy_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_merge_listener");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_data_available_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_data_on_readers_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_inconsistent_topic_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_liveliness_changed_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_liveliness_lost_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_offered_deadline_missed_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_offered_incompatible_qos_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_publication_matched_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_requested_deadline_missed_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_requested_incompatible_qos_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lset_sample_lost_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_sample_rejected_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_subscription_matched_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_inconsistent_topic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lset_liveliness_lost");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_offered_deadline_missed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_offered_incompatible_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lset_data_on_readers");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lset_sample_lost");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lset_data_available");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lset_sample_rejected");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_liveliness_changed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_requested_deadline_missed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_requested_incompatible_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_publication_matched");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lset_subscription_matched");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_data_available_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_data_on_readers_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_inconsistent_topic_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_liveliness_changed_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_liveliness_lost_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_offered_deadline_missed_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_offered_incompatible_qos_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_publication_matched_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_requested_deadline_missed_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_requested_incompatible_qos_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lget_sample_lost_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_sample_rejected_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_subscription_matched_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_inconsistent_topic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lget_liveliness_lost");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_offered_deadline_missed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_offered_incompatible_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lget_data_on_readers");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lget_sample_lost");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lget_data_available");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lget_sample_rejected");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_liveliness_changed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_requested_deadline_missed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_requested_incompatible_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_publication_matched");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_lget_subscription_matched");

  // dds_public_qos
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_delete_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_reset_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_copy_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_merge_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qos_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_userdata");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_topicdata");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_groupdata");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_durability");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_history");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_resource_limits");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_presentation");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_lifespan");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_deadline");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_latency_budget");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_ownership");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_ownership_strength");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_liveliness");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_time_based_filter");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_partition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_partition1");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_reliability");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_transport_priority");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_destination_order");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_writer_data_lifecycle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_reader_data_lifecycle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_writer_batching");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_durability_service");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_ignorelocal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_prop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_prop_propagate");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qunset_prop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_bprop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_bprop_propagate");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qunset_bprop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_type_consistency");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qset_data_representation");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_entity_name");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qset_psmx_instances");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_userdata");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_topicdata");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_groupdata");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_durability");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_history");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_resource_limits");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_presentation");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_lifespan");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_deadline");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_latency_budget");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_ownership");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_ownership_strength");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_liveliness");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_time_based_filter");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_partition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_reliability");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_transport_priority");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_destination_order");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_writer_data_lifecycle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_reader_data_lifecycle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_writer_batching");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_durability_service");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_ignorelocal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_propnames");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_prop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_prop_propagate");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_bpropnames");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_bprop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_bprop_propagate");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_type_consistency");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_qget_data_representation");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_entity_name");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qget_psmx_instances");

  // dds_public_status.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_inconsistent_topic_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_publication_matched_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_liveliness_lost_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_offered_deadline_missed_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_offered_incompatible_qos_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_subscription_matched_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_liveliness_changed_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_sample_rejected_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_sample_lost_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_requested_deadline_missed_status");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_get_requested_incompatible_qos_status");

#if DDS_HAS_TYPELIB // In dds/ddsrt/features.h
  // dds_public_dynamic_type.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_external");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_hashid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_key");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_must_understand");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_optional");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_add_bitmask_field");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_add_enum_literal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_add_member");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dynamic_type_create");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dynamic_type_dup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dynamic_type_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_register");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_autoid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_bit_bound");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_extensibility");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_nested");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_dynamic_type_unref");
#endif

  // dds_rhs.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_associate");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_store");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_unregister_wr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_rhc_relinquish_ownership");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_peek");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_read");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_rhc_take");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_rhc_add_readcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_rhc_remove_readcondition");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_reader_data_available_cb");

  // dds_statistics.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_statistics");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_refresh_statistics");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_delete_statistics");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_lookup_statistic");

  // dds_cdrstream.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_istream_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_istream_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_ostream_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_ostream_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_ostreamLE_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_ostreamLE_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_ostreamBE_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_ostreamBE_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_normalize");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_normalize_xcdr2_data");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_write");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_writeLE");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_writeBE");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_write_with_mid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_write_with_midLE");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_write_with_midBE");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_write_with_byte_order");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_write_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_write_sampleLE");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_write_sampleBE");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_getsize_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_getsize_key");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_read");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_read_key");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_read_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_free_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_countops");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_print_key");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_stream_print_sample");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_extract_key_from_data");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_extract_key_from_key");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_extract_keyBE_from_data");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_stream_extract_keyBE_from_key");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_cdrstream_desc_from_topic_desc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_cdrstream_desc_init_with_nops");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_cdrstream_desc_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_cdrstream_desc_fini");

  // dds_psmx.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_add_psmx_endpoint_to_list");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_add_psmx_topic_to_list");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_remove_psmx_endpoint_from_list");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_remove_psmx_topic_from_list");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_psmx_init_generic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_psmx_cleanup_generic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_psmx_topic_init_generic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_psmx_topic_cleanup_generic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_psmx_get_config_option_value");

  // dds_loaned_sample.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_loaned_sample_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_loaned_sample_unref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_reader_store_loaned_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_reader_store_loaned_sample_wr_metadata");

#ifdef DDS_HAS_SECURITY
  // dds_security_timed_cb.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_enable");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_disable");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_add");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_remove");

  // dds_security_utils.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_copy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_set_by_value");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_set_by_string");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_set_by_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_allocbuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Property_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Property_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Property_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Property_copy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Property_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Property_get_value");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_allocbuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_freebuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_find_property");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_copy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_find_property");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_find_binary_property");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_allocbuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_freebuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_copy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_ParticipantBuiltinTopicData_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_ParticipantBuiltinTopicData_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_ParticipantBuiltinTopicData_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_allocbuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_freebuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_copy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_alloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_allocbuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_freebuf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Exception_vset");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Exception_set");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Exception_reset");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Exception_clean");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertyQosPolicy_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_PropertyQosPolicy_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_set_token_nil");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_basicprotectionkind2transformationkind");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_protectionkind2transformationkind");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_get_conf_item_type");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_normalize_file");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_parse_xml_date");

  // dds_security_serializer.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serializer_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serializer_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serializer_buffer");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_PropertySeq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_BinaryPropertyArray");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_BinaryPropertySeq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_ParticipantBuiltinTopicData");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_KeyMaterial_AES_GCM_GMAC");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Deserializer_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Deserializer_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Deserialize_ParticipantBuiltinTopicData");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_BuiltinTopicKeyBE");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC");

  // shared_secret.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_get_challenge1_from_secret_handle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_get_challenge2_from_secret_handle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_get_secret_from_secret_handle");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "DDS_Security_get_secret_size_from_secret_handle");
#endif

#ifdef DDS_HAS_QOS_PROVIDER
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_create_qos_provider");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_create_qos_provider_scope");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_qos_provider_get_qos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_delete_qos_provider");
#endif

// ddsi_sertype.h
#ifndef _WIN32
  // ddsi_sertype_v0 is a function only when not built for Windows, otherwise is
  // a struct
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_v0");
#endif
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_init_props");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_init_flags");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_unref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_compute_serdata_basehash");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_hash");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_extensibility_enc_format");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_get_native_enc_identifier");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_enc_id_xcdr_version");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_enc_id_enc_format");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_zero_samples");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_realloc_samples");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_free_samples");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_zero_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_alloc_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_free_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_typeid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_typemap");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_sertype_typeinfo");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_derive_sertype");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_get_serialized_size");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_sertype_serialize_into");

  // ddsi_serdata.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_unref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_size");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_from_ser");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_serdata_from_ser_iov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_serdata_from_keyhash");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_from_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_to_untyped");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_to_ser");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_to_ser_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_serdata_to_ser_unref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_to_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_serdata_untyped_to_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_eqkey");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_print");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_serdata_print_untyped");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_get_keyhash");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_serdata_from_loaned_sample");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_serdata_from_psmx");

#ifdef DDS_HAS_TYPELIB
  // ddsi_typewrap.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_compare");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_copy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_dup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_ser");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_is_none");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_is_hash");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_is_minimal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_is_complete");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_typeid_is_fully_descriptive");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_typeid_get_equivalence_hash");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_kind");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeid_fini");

  // ddsi_typelib.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeinfo_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeinfo_typeid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeinfo_deser");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeinfo_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeinfo_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typeinfo_dup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_typeinfo_minimal_typeid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_typeinfo_complete_typeid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typemap_deser");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typemap_fini");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_typemap_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_typemap_get_type_name");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_type_lookup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_type_compare");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_type_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_type_unref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_type_add");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_make_typeid_str");
#endif

  // ddsi_config.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_config_init_default");

  // ddsi_config_impl.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_config_fini");

  // ddsi/ddsi_thread.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_lookup_thread_state");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_lookup_thread_state_real");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_thread_state_asleep");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_thread_state_awake");

  // ddsi/ddsi_gc.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_gcreq_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_gcreq_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_gcreq_enqueue");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_gcreq_get_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_gcreq_set_arg");

#if DDS_HAS_TYPELIB // In dds/ddsrt/features.h
  // ddsi/ddsi_typebuilder.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_topic_descriptor_from_type");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_topic_descriptor_fini");
#endif

  // ddsi/ddsi_proxy_endpoint.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_new_proxy_writer");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_new_proxy_reader");

  // ddsi/ddsi_proxy_participant.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_new_proxy_participant");

  // ddsi/ddsi_plist.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_plist_init_empty");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_plist_fini");

  // ddsi/ddsi_xqos.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_xqos_delta");

  // ddsi/ddsi_xmsg.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_xpack_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_xpack_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_xpack_send");

  // ddsi/ddsi_guid.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_hton_guid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_ntoh_guid");

  // ddsi/ddsi_tkmap.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_tkmap_lookup_instance_ref");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_tkmap_instance_unref");

  // ddsi/ddsi_transmit.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_write_sample_gc");

  // ddsi/ddsi_entity_index.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_entidx_lookup_writer_guid");

  // ddsi/ddsi_addrset.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_ref_addrset");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_unref_addrset");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_new_addrset");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_add_locator_to_addrset");

  // ddsi/ddsi_tran.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_locator_to_string");

  // ddsi/ddsi_portmapping.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_get_port_int");

  // ddsi__ipaddr.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsi_ipaddr_to_loc");

  // ddsi__discovery_addrset.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsi_include_multicast_locator_in_discovery");

  // ddsrt/atomics.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_ld32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_ldptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_ldvoidp");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_st32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_stptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_stvoidp");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_inc32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_incptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_inc32_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_incptr_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_inc32_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_incptr_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_dec32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_decptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_dec32_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_decptr_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_dec32_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_decptr_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_add32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_addptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_add32_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_addptr_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_add32_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_addptr_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_sub32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_subptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_sub32_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_subptr_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_sub32_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_subptr_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_and32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_andptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_and32_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_andptr_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_and32_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_andptr_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_or32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_orptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_or32_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_orptr_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_or32_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_orptr_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_cas32");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_casptr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_casvoidp");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_fence");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_fence_ldld");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_fence_stst");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_fence_acq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_fence_rel");
#if DDSRT_64BIT
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_ld64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_st64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_inc64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_inc64_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_inc64_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_dec64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_dec64_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_dec64_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_add64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_add64_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_add64_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_sub64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_sub64_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_sub64_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_and64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_and64_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_and64_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_or64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_or64_ov");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_or64_nv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_atomic_cas64");
#endif

  // ddsrt/bits.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_ffs32u");

  // ddsrt/md5.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_md5_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_md5_append");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_md5_finish");

  // ddsrt/mh3.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mh3");

  // ddsrt/hopscotch.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_lookup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_add");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_remove");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_add_absent");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_remove_present");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_enum");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_iter_first");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_hh_iter_next");

  // ddsrt/sync.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mutex_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mutex_destroy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mutex_lock");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mutex_trylock");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mutex_unlock");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_destroy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_wait");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_signal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_broadcast");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_wctime_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_wctime_destroy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_wctime_wait");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_wctime_waituntil");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_wctime_signal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_wctime_broadcast");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_mtime_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_mtime_destroy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_mtime_wait");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_mtime_waituntil");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_mtime_signal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_mtime_broadcast");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_etime_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_etime_destroy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_etime_wait");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_etime_waituntil");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_cond_etime_signal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_cond_etime_broadcast");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_destroy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_read");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_write");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_tryread");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_trywrite");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_rwlock_unlock");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_once");

  // ddsrt/threads.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_threadattr_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_create");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_gettid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_gettid_for_thread");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_self");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_equal");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_join");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_getname");
#ifdef DDSRT_HAVE_THREAD_SETNAME
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_setname");
#endif
#ifdef DDSRT_HAVE_THREAD_LIST
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_list");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_thread_getname_anythread");
#endif
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_thread_cleanup_push");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_cleanup_pop");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_fini");

  // ddsrt/process.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_getpid");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_getprocessname");

  // ddsrt/time.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_time");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_sleepfor");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_time_wallclock");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_time_monotonic");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_time_elapsed");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_time_highres");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_ctime");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_time_add_duration");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mtime_add_duration");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_wctime_add_duration");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_etime_add_duration");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_mtime_to_sec_usec");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_wctime_to_sec_usec");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_etime_to_sec_usec");

  // ddsrt/bswap.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_bswap2u");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_bswap2");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_bswap4u");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_bswap4");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_bswap8u");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_bswap8");

  // ddsrt/random.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_random");

  // ddsrt/avl.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_treedef_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_treedef_init_r");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_free_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_root");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_root_non_empty");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup_ipath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup_dpath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup_pred_eq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup_succ_eq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup_pred");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_lookup_succ");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_insert");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_delete");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_insert_ipath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_delete_dpath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_swap_node");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_augment_update");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_is_empty");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_is_singleton");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_find_min");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_find_max");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_find_pred");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_find_succ");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_walk");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_const_walk");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_walk_range");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_const_walk_range");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_walk_range_reverse");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_const_walk_range_reverse");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_iter_first");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_iter_succ_eq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_iter_succ");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_iter_next");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_ctreedef_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_ctreedef_init_r");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cinit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cfree");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cfree_arg");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_croot");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_croot_non_empty");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_clookup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_clookup_ipath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_clookup_dpath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_clookup_pred_eq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_clookup_succ_eq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_clookup_pred");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_clookup_succ");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cinsert");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cdelete");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cinsert_ipath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cdelete_dpath");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cswap_node");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_caugment_update");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cis_empty");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cis_singleton");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_ccount");

  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cfind_min");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cfind_max");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cfind_pred");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cfind_succ");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cwalk");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cconst_walk");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_cwalk_range");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_cconst_walk_range");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_cwalk_range_reverse");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_avl_cconst_walk_range_reverse");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_citer_first");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_citer_succ_eq");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_citer_succ");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_avl_citer_next");

  // ddsrt/fibheap.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_fibheap_def_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_fibheap_init");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_fibheap_min");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_fibheap_merge");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_fibheap_insert");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_fibheap_delete");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_fibheap_extract_min");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_fibheap_decrease_key");

#if DDSRT_HAVE_NETSTAT
  // ddsrt/netstat.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_netstat_new");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_netstat_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_netstat_get");
#endif

#if DDSRT_HAVE_RUSAGE
#ifdef DDSRT_HAVE_THREAD_LIST
  // ddsrt/rusage.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_thread_list");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_getrusage");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "ddsrt_getrusage_anythread");
#endif
#endif

  // ddsrt/environ.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_getenv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_setenv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_unsetenv");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_expand_envvars");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_expand_envvars_sh");

  // ddsrt/retcode.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_strretcode");

  // ddsrt/log.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_log");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_set_log_mask"); // ROS 2 rmw_cyclonedds_cpp needs
                                           // this, probably erroneously
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_get_log_mask");

  // ddsrt/sockets.h
#if DDSRT_HAVE_GETHOSTNAME
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_gethostname");
#endif

  // ddsrt/heap.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_malloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_malloc_s");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_calloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_calloc_s");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_realloc");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_realloc_s");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_set_allocator");

  // ddsrt/string.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strcasecmp");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strncasecmp");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strsep");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_memdup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strdup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strndup");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strlcpy");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strlcat");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_str_replace");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_str_trim_ord_space");

  // ddsrt/strtol.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_todigit");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strtoint64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_strtouint64");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_int64tostr");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_uint64tostr");

  // ddsrt/xmlparser.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_xmlp_new_file");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_xmlp_new_string");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_xmlp_set_options");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_xmlp_get_bufpos");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_xmlp_free");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_xmlp_parse");

  // ddsrt/machineid.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_get_machineid");

#if DDSRT_HAVE_FILESYSTEM
  // ddsrt/filesystem.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_opendir");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_closedir");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_readdir");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_stat");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_file_normalize");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_file_sep");
#endif

  // ddsrt/io.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_vasprintf");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_asprintf");

  // ddsrt/ifaddrs.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_getifaddrs");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_freeifaddrs");

  // ddsrt/sockets.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "ddsrt_sockaddrfromstr");

  // dds__write.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_write_impl");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_writecdr_impl");

  // dds__writer.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_writer_psmx_loan_raw");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_writer_psmx_loan_from_serdata");

  // dds__entity.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_entity_pin");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_entity_unpin");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_entity_lock");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_entity_unlock");

  // dds__sysdef_parser.h
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_sysdef_init_sysdef");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_sysdef_init_sysdef_str");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_sysdef_fini_sysdef");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_sysdef_init_data_types");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_sysdef_init_data_types_str");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle,
                      "dds_sysdef_fini_data_types");

  // deprecated functions from v0.1
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_fail_set");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_fail_get");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_err_str");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_fail");
  CHECK_SYMBOL_EXPORT(&result, ddsc_library_handle, "dds_err_check");

  printf("Symbol Export result: %s\n",
         result == EXIT_SUCCESS ? "OK" : "FAILED");

  close_library(ddsc_library_handle);

  return result;
}
