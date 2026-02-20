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
  check_symbol_export(&result, ddsc_library_handle, "dds_enable");
  check_symbol_export(&result, ddsc_library_handle, "dds_delete");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_publisher");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_subscriber");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_datareader");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_instance_handle");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_guid");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_status");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_status");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_status_changes");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_status_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_set_status_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_set_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_listener");
  check_symbol_export(&result, ddsc_library_handle, "dds_set_listener");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_participant");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_create_participant_guid");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_domain");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_create_domain_with_rawconfig");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_parent");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_participant");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_children");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_domainid");
  check_symbol_export(&result, ddsc_library_handle, "dds_lookup_participant");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_topic");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_topic_sertype");
  check_symbol_export(&result, ddsc_library_handle, "dds_find_topic");
  check_symbol_export(&result, ddsc_library_handle, "dds_find_topic_scoped");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_create_topic_descriptor");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_delete_topic_descriptor");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_name");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_type_name");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_set_topic_filter_and_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_set_topic_filter_extended");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_topic_filter_and_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_topic_filter_extended");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_subscriber");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_publisher");
  check_symbol_export(&result, ddsc_library_handle, "dds_suspend");
  check_symbol_export(&result, ddsc_library_handle, "dds_resume");
  check_symbol_export(&result, ddsc_library_handle, "dds_wait_for_acks");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_reader");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_reader_guid");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_reader_rhc");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_reader_wait_for_historical_data");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_writer");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_writer_guid");
  check_symbol_export(&result, ddsc_library_handle, "dds_register_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_unregister_instance");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_unregister_instance_ih");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_unregister_instance_ts");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_unregister_instance_ih_ts");
  check_symbol_export(&result, ddsc_library_handle, "dds_writedispose");
  check_symbol_export(&result, ddsc_library_handle, "dds_writedispose_ts");
  check_symbol_export(&result, ddsc_library_handle, "dds_dispose");
  check_symbol_export(&result, ddsc_library_handle, "dds_dispose_ts");
  check_symbol_export(&result, ddsc_library_handle, "dds_dispose_ih");
  check_symbol_export(&result, ddsc_library_handle, "dds_dispose_ih_ts");
  check_symbol_export(&result, ddsc_library_handle, "dds_write");
  check_symbol_export(&result, ddsc_library_handle, "dds_write_flush");
  check_symbol_export(&result, ddsc_library_handle, "dds_writecdr");
  check_symbol_export(&result, ddsc_library_handle, "dds_forwardcdr");
  check_symbol_export(&result, ddsc_library_handle, "dds_write_ts");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_readcondition");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_create_querycondition");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_create_guardcondition");
  check_symbol_export(&result, ddsc_library_handle, "dds_set_guardcondition");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_guardcondition");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_guardcondition");
  check_symbol_export(&result, ddsc_library_handle, "dds_create_waitset");
  check_symbol_export(&result, ddsc_library_handle, "dds_waitset_get_entities");
  check_symbol_export(&result, ddsc_library_handle, "dds_waitset_attach");
  check_symbol_export(&result, ddsc_library_handle, "dds_waitset_detach");
  check_symbol_export(&result, ddsc_library_handle, "dds_waitset_set_trigger");
  check_symbol_export(&result, ddsc_library_handle, "dds_waitset_wait");
  check_symbol_export(&result, ddsc_library_handle, "dds_waitset_wait_until");
  check_symbol_export(&result, ddsc_library_handle, "dds_peek");
  check_symbol_export(&result, ddsc_library_handle, "dds_peek_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_peek_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_peek_instance_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_peek_next");
  check_symbol_export(&result, ddsc_library_handle, "dds_read");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_mask_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_instance_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_instance_mask");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_read_instance_mask_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_next");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_next_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_take");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_mask");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_mask_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_instance_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_instance_mask");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_take_instance_mask_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_next");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_next_wl");
  check_symbol_export(&result, ddsc_library_handle, "dds_peekcdr");
  check_symbol_export(&result, ddsc_library_handle, "dds_peekcdr_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_readcdr");
  check_symbol_export(&result, ddsc_library_handle, "dds_readcdr_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_takecdr");
  check_symbol_export(&result, ddsc_library_handle, "dds_takecdr_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_peek_with_collector");
  check_symbol_export(&result, ddsc_library_handle, "dds_read_with_collector");
  check_symbol_export(&result, ddsc_library_handle, "dds_take_with_collector");
  check_symbol_export(&result, ddsc_library_handle, "dds_lookup_instance");
  check_symbol_export(&result, ddsc_library_handle, "dds_instance_get_key");
  check_symbol_export(&result, ddsc_library_handle, "dds_begin_coherent");
  check_symbol_export(&result, ddsc_library_handle, "dds_end_coherent");
  check_symbol_export(&result, ddsc_library_handle, "dds_notify_readers");
  check_symbol_export(&result, ddsc_library_handle, "dds_triggered");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_topic");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_matched_subscriptions");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_matched_subscription_data");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_matched_publications");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_matched_publication_data");
#ifdef DDS_HAS_TYPELIB
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_builtintopic_get_endpoint_type_info");
#endif
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_builtintopic_free_endpoint");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_builtintopic_free_topic");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_builtintopic_free_participant");
  check_symbol_export(&result, ddsc_library_handle, "dds_assert_liveliness");
  check_symbol_export(&result, ddsc_library_handle, "dds_domain_set_deafmute");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_typeobj");
  check_symbol_export(&result, ddsc_library_handle, "dds_free_typeobj");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_typeinfo");
  check_symbol_export(&result, ddsc_library_handle, "dds_free_typeinfo");
  check_symbol_export(&result, ddsc_library_handle, "dds_get_entity_sertype");

  // dds_public_loan_api.h
  check_symbol_export(&result, ddsc_library_handle, "dds_request_loan");
  check_symbol_export(&result, ddsc_library_handle, "dds_return_loan");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_is_shared_memory_available");
  check_symbol_export(&result, ddsc_library_handle, "dds_request_loan_of_size");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_is_loan_available"); // deprecated
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_loan_sample"); // deprecated

  // dds_public_alloc.h
  check_symbol_export(&result, ddsc_library_handle, "dds_alloc");
  check_symbol_export(&result, ddsc_library_handle, "dds_realloc");
  check_symbol_export(&result, ddsc_library_handle, "dds_realloc_zero");
  check_symbol_export(&result, ddsc_library_handle, "dds_free");
  check_symbol_export(&result, ddsc_library_handle, "dds_string_alloc");
  check_symbol_export(&result, ddsc_library_handle, "dds_string_dup");
  check_symbol_export(&result, ddsc_library_handle, "dds_string_free");
  check_symbol_export(&result, ddsc_library_handle, "dds_sample_free");

  // dds_public_impl.h
  check_symbol_export(&result, ddsc_library_handle, "dds_write_set_batch");

  // dds_public_listener.h
  check_symbol_export(&result, ddsc_library_handle, "dds_create_listener");
  check_symbol_export(&result, ddsc_library_handle, "dds_delete_listener");
  check_symbol_export(&result, ddsc_library_handle, "dds_reset_listener");
  check_symbol_export(&result, ddsc_library_handle, "dds_copy_listener");
  check_symbol_export(&result, ddsc_library_handle, "dds_merge_listener");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_data_available_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_data_on_readers_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_inconsistent_topic_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_liveliness_changed_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_liveliness_lost_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_offered_deadline_missed_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_offered_incompatible_qos_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_publication_matched_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_requested_deadline_missed_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_requested_incompatible_qos_arg");
  check_symbol_export(&result, ddsc_library_handle, "dds_lset_sample_lost_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_sample_rejected_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_subscription_matched_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_inconsistent_topic");
  check_symbol_export(&result, ddsc_library_handle, "dds_lset_liveliness_lost");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_offered_deadline_missed");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_offered_incompatible_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_lset_data_on_readers");
  check_symbol_export(&result, ddsc_library_handle, "dds_lset_sample_lost");
  check_symbol_export(&result, ddsc_library_handle, "dds_lset_data_available");
  check_symbol_export(&result, ddsc_library_handle, "dds_lset_sample_rejected");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_liveliness_changed");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_requested_deadline_missed");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_requested_incompatible_qos");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_publication_matched");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lset_subscription_matched");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_data_available_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_data_on_readers_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_inconsistent_topic_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_liveliness_changed_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_liveliness_lost_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_offered_deadline_missed_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_offered_incompatible_qos_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_publication_matched_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_requested_deadline_missed_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_requested_incompatible_qos_arg");
  check_symbol_export(&result, ddsc_library_handle, "dds_lget_sample_lost_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_sample_rejected_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_subscription_matched_arg");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_inconsistent_topic");
  check_symbol_export(&result, ddsc_library_handle, "dds_lget_liveliness_lost");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_offered_deadline_missed");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_offered_incompatible_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_lget_data_on_readers");
  check_symbol_export(&result, ddsc_library_handle, "dds_lget_sample_lost");
  check_symbol_export(&result, ddsc_library_handle, "dds_lget_data_available");
  check_symbol_export(&result, ddsc_library_handle, "dds_lget_sample_rejected");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_liveliness_changed");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_requested_deadline_missed");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_requested_incompatible_qos");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_publication_matched");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_lget_subscription_matched");

  // dds_public_qos
  check_symbol_export(&result, ddsc_library_handle, "dds_create_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_delete_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_reset_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_copy_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_merge_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_qos_equal");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_userdata");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_topicdata");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_groupdata");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_durability");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_history");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_resource_limits");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_presentation");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_lifespan");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_deadline");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_latency_budget");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_ownership");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_ownership_strength");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_liveliness");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_time_based_filter");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_partition");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_partition1");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_reliability");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_transport_priority");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_destination_order");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_writer_data_lifecycle");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_reader_data_lifecycle");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_writer_batching");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_durability_service");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_ignorelocal");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_prop");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_prop_propagate");
  check_symbol_export(&result, ddsc_library_handle, "dds_qunset_prop");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_bprop");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_bprop_propagate");
  check_symbol_export(&result, ddsc_library_handle, "dds_qunset_bprop");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_type_consistency");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qset_data_representation");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_entity_name");
  check_symbol_export(&result, ddsc_library_handle, "dds_qset_psmx_instances");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_userdata");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_topicdata");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_groupdata");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_durability");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_history");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_resource_limits");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_presentation");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_lifespan");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_deadline");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_latency_budget");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_ownership");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_ownership_strength");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_liveliness");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_time_based_filter");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_partition");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_reliability");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_transport_priority");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_destination_order");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_writer_data_lifecycle");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_reader_data_lifecycle");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_writer_batching");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_durability_service");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_ignorelocal");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_propnames");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_prop");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_prop_propagate");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_bpropnames");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_bprop");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_bprop_propagate");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_type_consistency");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_qget_data_representation");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_entity_name");
  check_symbol_export(&result, ddsc_library_handle, "dds_qget_psmx_instances");

  // dds_public_status.h
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_inconsistent_topic_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_publication_matched_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_liveliness_lost_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_offered_deadline_missed_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_offered_incompatible_qos_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_subscription_matched_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_liveliness_changed_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_sample_rejected_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_sample_lost_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_requested_deadline_missed_status");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_get_requested_incompatible_qos_status");

#if DDS_HAS_TYPELIB // In dds/ddsrt/features.h
  // dds_public_dynamic_type.h
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_external");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_hashid");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_key");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_must_understand");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_member_set_optional");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_add_bitmask_field");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_add_enum_literal");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_add_member");
  check_symbol_export(&result, ddsc_library_handle, "dds_dynamic_type_create");
  check_symbol_export(&result, ddsc_library_handle, "dds_dynamic_type_dup");
  check_symbol_export(&result, ddsc_library_handle, "dds_dynamic_type_ref");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_register");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_autoid");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_bit_bound");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_extensibility");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_dynamic_type_set_nested");
  check_symbol_export(&result, ddsc_library_handle, "dds_dynamic_type_unref");
#endif

  // dds_rhs.h
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_associate");
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_store");
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_unregister_wr");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_rhc_relinquish_ownership");
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_free");
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_peek");
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_read");
  check_symbol_export(&result, ddsc_library_handle, "dds_rhc_take");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_rhc_add_readcondition");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_rhc_remove_readcondition");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_reader_data_available_cb");

  // dds_statistics.h
  check_symbol_export(&result, ddsc_library_handle, "dds_create_statistics");
  check_symbol_export(&result, ddsc_library_handle, "dds_refresh_statistics");
  check_symbol_export(&result, ddsc_library_handle, "dds_delete_statistics");
  check_symbol_export(&result, ddsc_library_handle, "dds_lookup_statistic");

  // dds_cdrstream.h
  check_symbol_export(&result, ddsc_library_handle, "dds_istream_init");
  check_symbol_export(&result, ddsc_library_handle, "dds_istream_fini");
  check_symbol_export(&result, ddsc_library_handle, "dds_ostream_init");
  check_symbol_export(&result, ddsc_library_handle, "dds_ostream_fini");
  check_symbol_export(&result, ddsc_library_handle, "dds_ostreamLE_init");
  check_symbol_export(&result, ddsc_library_handle, "dds_ostreamLE_fini");
  check_symbol_export(&result, ddsc_library_handle, "dds_ostreamBE_init");
  check_symbol_export(&result, ddsc_library_handle, "dds_ostreamBE_fini");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_normalize");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_normalize_xcdr2_data");

  check_symbol_export(&result, ddsc_library_handle, "dds_stream_write");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_writeLE");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_writeBE");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_write_with_mid");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_write_with_midLE");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_write_with_midBE");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_write_with_byte_order");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_write_sample");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_write_sampleLE");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_write_sampleBE");

  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_getsize_sample");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_getsize_key");

  check_symbol_export(&result, ddsc_library_handle, "dds_stream_read");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_read_key");

  check_symbol_export(&result, ddsc_library_handle, "dds_stream_read_sample");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_free_sample");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_countops");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_print_key");
  check_symbol_export(&result, ddsc_library_handle, "dds_stream_print_sample");

  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_extract_key_from_data");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_extract_key_from_key");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_extract_keyBE_from_data");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_stream_extract_keyBE_from_key");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_cdrstream_desc_from_topic_desc");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_cdrstream_desc_init_with_nops");
  check_symbol_export(&result, ddsc_library_handle, "dds_cdrstream_desc_init");
  check_symbol_export(&result, ddsc_library_handle, "dds_cdrstream_desc_fini");

  // dds_psmx.h
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_add_psmx_endpoint_to_list");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_add_psmx_topic_to_list");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_remove_psmx_endpoint_from_list");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_remove_psmx_topic_from_list");
  check_symbol_export(&result, ddsc_library_handle, "dds_psmx_init_generic");
  check_symbol_export(&result, ddsc_library_handle, "dds_psmx_cleanup_generic");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_psmx_topic_init_generic");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_psmx_topic_cleanup_generic");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_psmx_get_config_option_value");

  // dds_loaned_sample.h
  check_symbol_export(&result, ddsc_library_handle, "dds_loaned_sample_ref");
  check_symbol_export(&result, ddsc_library_handle, "dds_loaned_sample_unref");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_reader_store_loaned_sample");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_reader_store_loaned_sample_wr_metadata");

#ifdef DDS_HAS_SECURITY
  // dds_security_timed_cb.h
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_new");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_enable");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_disable");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_add");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_security_timed_dispatcher_remove");

  // dds_security_utils.h
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_copy");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_equal");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_set_by_value");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_set_by_string");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryProperty_set_by_ref");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_allocbuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BinaryPropertySeq_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Property_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Property_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Property_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Property_copy");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Property_equal");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Property_get_value");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_allocbuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_freebuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertySeq_find_property");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_copy");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_equal");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_find_property");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolder_find_binary_property");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_allocbuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_freebuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_DataHolderSeq_copy");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_ParticipantBuiltinTopicData_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_ParticipantBuiltinTopicData_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_ParticipantBuiltinTopicData_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_allocbuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_freebuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_OctetSeq_copy");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_alloc");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_allocbuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_freebuf");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_HandleSeq_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Exception_vset");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Exception_set");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Exception_reset");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Exception_clean");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertyQosPolicy_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_PropertyQosPolicy_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_set_token_nil");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_basicprotectionkind2transformationkind");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_protectionkind2transformationkind");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_get_conf_item_type");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_normalize_file");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_parse_xml_date");

  // dds_security_serializer.h
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serializer_new");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serializer_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serializer_buffer");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_PropertySeq");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_BinaryPropertyArray");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_BinaryPropertySeq");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_ParticipantBuiltinTopicData");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Serialize_KeyMaterial_AES_GCM_GMAC");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Deserializer_new");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Deserializer_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Deserialize_ParticipantBuiltinTopicData");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_BuiltinTopicKeyBE");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC");

  // shared_secret.h
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_get_challenge1_from_secret_handle");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_get_challenge2_from_secret_handle");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_get_secret_from_secret_handle");
  check_symbol_export(&result, ddsc_library_handle,
                      "DDS_Security_get_secret_size_from_secret_handle");
#endif

#ifdef DDS_HAS_QOS_PROVIDER
  check_symbol_export(&result, ddsc_library_handle, "dds_create_qos_provider");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_create_qos_provider_scope");
  check_symbol_export(&result, ddsc_library_handle, "dds_qos_provider_get_qos");
  check_symbol_export(&result, ddsc_library_handle, "dds_delete_qos_provider");
#endif

// ddsi_sertype.h
#ifndef _WIN32
  // ddsi_sertype_v0 is a function only when not built for Windows, otherwise is
  // a struct
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_v0");
#endif
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_init_props");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_init_flags");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_fini");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_ref");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_unref");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_compute_serdata_basehash");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_equal");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_hash");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_extensibility_enc_format");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_get_native_enc_identifier");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_enc_id_xcdr_version");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_enc_id_enc_format");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_free");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_zero_samples");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_realloc_samples");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_free_samples");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_zero_sample");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_alloc_sample");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_free_sample");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_typeid");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_typemap");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_sertype_typeinfo");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_derive_sertype");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_get_serialized_size");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_sertype_serialize_into");

  // ddsi_serdata.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_ref");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_unref");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_size");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_from_ser");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_serdata_from_ser_iov");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_serdata_from_keyhash");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_from_sample");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_to_untyped");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_to_ser");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_to_ser_ref");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_serdata_to_ser_unref");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_to_sample");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_serdata_untyped_to_sample");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_eqkey");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_print");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_serdata_print_untyped");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_get_keyhash");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_serdata_from_loaned_sample");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_serdata_from_psmx");

#ifdef DDS_HAS_TYPELIB
  // ddsi_typewrap.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_compare");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_copy");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_dup");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_ser");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_is_none");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_is_hash");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_is_minimal");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_is_complete");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_typeid_is_fully_descriptive");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_typeid_get_equivalence_hash");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_kind");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeid_fini");

  // ddsi_typelib.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeinfo_equal");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeinfo_typeid");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeinfo_deser");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeinfo_fini");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeinfo_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typeinfo_dup");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_typeinfo_minimal_typeid");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_typeinfo_complete_typeid");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typemap_deser");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typemap_fini");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_typemap_equal");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_typemap_get_type_name");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_type_lookup");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_type_compare");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_type_ref");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_type_unref");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_type_add");

  check_symbol_export(&result, ddsc_library_handle, "ddsi_make_typeid_str");
#endif

  // ddsi_config.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_config_init_default");

  // ddsi_config_impl.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_config_fini");

  // ddsi/ddsi_thread.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_lookup_thread_state");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_lookup_thread_state_real");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_thread_state_asleep");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_thread_state_awake");

  // ddsi/ddsi_gc.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_gcreq_new");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_gcreq_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_gcreq_enqueue");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_gcreq_get_arg");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_gcreq_set_arg");

#if DDS_HAS_TYPELIB // In dds/ddsrt/features.h
  // ddsi/ddsi_typebuilder.h
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_topic_descriptor_from_type");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_topic_descriptor_fini");
#endif

  // ddsi/ddsi_proxy_endpoint.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_new_proxy_writer");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_new_proxy_reader");

  // ddsi/ddsi_proxy_participant.h
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_new_proxy_participant");

  // ddsi/ddsi_plist.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_plist_init_empty");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_plist_fini");

  // ddsi/ddsi_xqos.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_xqos_delta");

  // ddsi/ddsi_xmsg.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_xpack_new");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_xpack_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_xpack_send");

  // ddsi/ddsi_guid.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_hton_guid");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_ntoh_guid");

  // ddsi/ddsi_tkmap.h
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_tkmap_lookup_instance_ref");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_tkmap_instance_unref");

  // ddsi/ddsi_transmit.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_write_sample_gc");

  // ddsi/ddsi_entity_index.h
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_entidx_lookup_writer_guid");

  // ddsi/ddsi_addrset.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_ref_addrset");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_unref_addrset");
  check_symbol_export(&result, ddsc_library_handle, "ddsi_new_addrset");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_add_locator_to_addrset");

  // ddsi/ddsi_tran.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_locator_to_string");

  // ddsi/ddsi_portmapping.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_get_port_int");

  // ddsi__ipaddr.h
  check_symbol_export(&result, ddsc_library_handle, "ddsi_ipaddr_to_loc");

  // ddsi__discovery_addrset.h
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsi_include_multicast_locator_in_discovery");

  // ddsrt/atomics.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_ld32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_ldptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_ldvoidp");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_st32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_stptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_stvoidp");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_inc32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_incptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_inc32_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_incptr_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_inc32_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_incptr_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_dec32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_decptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_dec32_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_decptr_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_dec32_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_decptr_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_add32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_addptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_add32_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_addptr_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_add32_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_addptr_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_sub32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_subptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_sub32_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_subptr_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_sub32_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_subptr_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_and32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_andptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_and32_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_andptr_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_and32_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_andptr_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_or32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_orptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_or32_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_orptr_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_or32_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_orptr_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_cas32");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_casptr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_casvoidp");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_fence");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_fence_ldld");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_fence_stst");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_fence_acq");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_fence_rel");
#if DDSRT_64BIT
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_ld64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_st64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_inc64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_inc64_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_inc64_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_dec64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_dec64_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_dec64_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_add64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_add64_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_add64_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_sub64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_sub64_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_sub64_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_and64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_and64_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_and64_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_or64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_or64_ov");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_or64_nv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_atomic_cas64");
#endif

  // ddsrt/bits.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_ffs32u");

  // ddsrt/md5.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_md5_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_md5_append");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_md5_finish");

  // ddsrt/mh3.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mh3");

  // ddsrt/hopscotch.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_new");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_lookup");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_add");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_remove");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_add_absent");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_remove_present");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_enum");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_iter_first");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_hh_iter_next");

  // ddsrt/sync.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mutex_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mutex_destroy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mutex_lock");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mutex_trylock");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mutex_unlock");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_destroy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_wait");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_signal");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_broadcast");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_wctime_init");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_wctime_destroy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_wctime_wait");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_wctime_waituntil");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_wctime_signal");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_wctime_broadcast");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_mtime_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_mtime_destroy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_mtime_wait");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_mtime_waituntil");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_mtime_signal");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_mtime_broadcast");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_etime_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_etime_destroy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_etime_wait");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_etime_waituntil");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_cond_etime_signal");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_cond_etime_broadcast");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_destroy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_read");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_write");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_tryread");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_trywrite");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_rwlock_unlock");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_once");

  // ddsrt/threads.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_threadattr_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_create");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_gettid");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_gettid_for_thread");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_self");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_equal");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_join");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_getname");
#ifdef DDSRT_HAVE_THREAD_SETNAME
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_setname");
#endif
#ifdef DDSRT_HAVE_THREAD_LIST
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_list");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_thread_getname_anythread");
#endif
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_thread_cleanup_push");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_cleanup_pop");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_fini");

  // ddsrt/process.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_getpid");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_getprocessname");

  // ddsrt/time.h
  check_symbol_export(&result, ddsc_library_handle, "dds_time");
  check_symbol_export(&result, ddsc_library_handle, "dds_sleepfor");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_time_wallclock");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_time_monotonic");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_time_elapsed");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_time_highres");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_ctime");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_time_add_duration");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mtime_add_duration");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_wctime_add_duration");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_etime_add_duration");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_mtime_to_sec_usec");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_wctime_to_sec_usec");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_etime_to_sec_usec");

  // ddsrt/bswap.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_bswap2u");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_bswap2");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_bswap4u");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_bswap4");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_bswap8u");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_bswap8");

  // ddsrt/random.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_random");

  // ddsrt/avl.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_treedef_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_treedef_init_r");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_free_arg");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_root");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_root_non_empty");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup_ipath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup_dpath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup_pred_eq");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup_succ_eq");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup_pred");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_lookup_succ");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_insert");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_delete");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_insert_ipath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_delete_dpath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_swap_node");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_augment_update");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_is_empty");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_is_singleton");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_find_min");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_find_max");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_find_pred");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_find_succ");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_walk");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_const_walk");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_walk_range");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_const_walk_range");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_walk_range_reverse");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_const_walk_range_reverse");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_iter_first");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_iter_succ_eq");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_iter_succ");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_iter_next");

  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_ctreedef_init");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_ctreedef_init_r");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cinit");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cfree");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cfree_arg");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_croot");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_croot_non_empty");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_clookup");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_clookup_ipath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_clookup_dpath");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_clookup_pred_eq");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_clookup_succ_eq");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_clookup_pred");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_clookup_succ");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cinsert");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cdelete");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cinsert_ipath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cdelete_dpath");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cswap_node");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_caugment_update");

  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cis_empty");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cis_singleton");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_ccount");

  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cfind_min");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cfind_max");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cfind_pred");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cfind_succ");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cwalk");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cconst_walk");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_cwalk_range");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_cconst_walk_range");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_cwalk_range_reverse");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_avl_cconst_walk_range_reverse");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_citer_first");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_citer_succ_eq");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_citer_succ");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_avl_citer_next");

  // ddsrt/fibheap.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_fibheap_def_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_fibheap_init");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_fibheap_min");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_fibheap_merge");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_fibheap_insert");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_fibheap_delete");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_fibheap_extract_min");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_fibheap_decrease_key");

#if DDSRT_HAVE_NETSTAT
  // ddsrt/netstat.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_netstat_new");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_netstat_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_netstat_get");
#endif

#if DDSRT_HAVE_RUSAGE
#ifdef DDSRT_HAVE_THREAD_LIST
  // ddsrt/rusage.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_thread_list");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_getrusage");
  check_symbol_export(&result, ddsc_library_handle,
                      "ddsrt_getrusage_anythread");
#endif
#endif

  // ddsrt/environ.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_getenv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_setenv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_unsetenv");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_expand_envvars");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_expand_envvars_sh");

  // ddsrt/retcode.h
  check_symbol_export(&result, ddsc_library_handle, "dds_strretcode");

  // ddsrt/log.h
  check_symbol_export(&result, ddsc_library_handle, "dds_log");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_set_log_mask"); // ROS 2 rmw_cyclonedds_cpp needs
                                           // this, probably erroneously
  check_symbol_export(&result, ddsc_library_handle, "dds_get_log_mask");

  // ddsrt/sockets.h
#if DDSRT_HAVE_GETHOSTNAME
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_gethostname");
#endif

  // ddsrt/heap.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_malloc");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_malloc_s");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_calloc");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_calloc_s");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_realloc");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_realloc_s");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_set_allocator");

  // ddsrt/string.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strcasecmp");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strncasecmp");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strsep");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_memdup");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strdup");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strndup");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strlcpy");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strlcat");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_str_replace");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_str_trim_ord_space");

  // ddsrt/strtol.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_todigit");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strtoint64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_strtouint64");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_int64tostr");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_uint64tostr");

  // ddsrt/xmlparser.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_xmlp_new_file");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_xmlp_new_string");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_xmlp_set_options");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_xmlp_get_bufpos");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_xmlp_free");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_xmlp_parse");

  // ddsrt/machineid.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_get_machineid");

#if DDSRT_HAVE_FILESYSTEM
  // ddsrt/filesystem.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_opendir");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_closedir");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_readdir");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_stat");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_file_normalize");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_file_sep");
#endif

  // ddsrt/io.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_vasprintf");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_asprintf");

  // ddsrt/ifaddrs.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_getifaddrs");
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_freeifaddrs");

  // ddsrt/sockets.h
  check_symbol_export(&result, ddsc_library_handle, "ddsrt_sockaddrfromstr");

  // dds__write.h
  check_symbol_export(&result, ddsc_library_handle, "dds_write_impl");
  check_symbol_export(&result, ddsc_library_handle, "dds_writecdr_impl");

  // dds__writer.h
  check_symbol_export(&result, ddsc_library_handle, "dds_writer_psmx_loan_raw");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_writer_psmx_loan_from_serdata");

  // dds__entity.h
  check_symbol_export(&result, ddsc_library_handle, "dds_entity_pin");
  check_symbol_export(&result, ddsc_library_handle, "dds_entity_unpin");
  check_symbol_export(&result, ddsc_library_handle, "dds_entity_lock");
  check_symbol_export(&result, ddsc_library_handle, "dds_entity_unlock");

  // dds__sysdef_parser.h
  check_symbol_export(&result, ddsc_library_handle, "dds_sysdef_init_sysdef");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_sysdef_init_sysdef_str");
  check_symbol_export(&result, ddsc_library_handle, "dds_sysdef_fini_sysdef");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_sysdef_init_data_types");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_sysdef_init_data_types_str");
  check_symbol_export(&result, ddsc_library_handle,
                      "dds_sysdef_fini_data_types");

  // deprecated functions from v0.1
  check_symbol_export(&result, ddsc_library_handle, "dds_fail_set");
  check_symbol_export(&result, ddsc_library_handle, "dds_fail_get");
  check_symbol_export(&result, ddsc_library_handle, "dds_err_str");
  check_symbol_export(&result, ddsc_library_handle, "dds_fail");
  check_symbol_export(&result, ddsc_library_handle, "dds_err_check");

  printf("Symbol Export result: %s\n",
         result == EXIT_SUCCESS ? "OK" : "FAILED");

  close_library(ddsc_library_handle);

  return result;
}
