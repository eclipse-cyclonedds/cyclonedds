# Cyclone DDS API - documentation and tests

This document lists Cyclone's public and semi-public API functions and briefly
describes the state of the documentation and tests on these functions. Note that
this is a working document that does not yet give a complete view of the tests and
docs quality.

The file names for tests are relative to /src/core/tests/ddsc/, unless another path
is specified.

## Entity creation

|Function | Documentation | Return codes | Remarks | Tests | Test quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_create_domain | ok | missing OUT_OF_RESOURCES | - | domain.c; config.c | ok
| dds_create_domain_with_rawconfig | ok | ? | - | domain.c | tested only for default config
| dds_create_participant | unclear documentation on domain_id | missing PRECONDITION_NOT_MET and INCONSISTENT_POLICY | - | participant.c | limited tests for qos parameter, listeners tested in one-liner tests listener.c
| dds_create_topic | ok | ? | - | topic.c | limited tests for qos and listener parameters
| dds_create_topic_generic | ok | - | - | **_tests missing_** | -
| _\[deprecated\] dds_create_topic_arbitrary_ | _ok_ | n/a | _replaced by: dds_create_topic_generic_ | - | n/a
| dds_create_subscriber | ok | incomplete: dds_entity_lock | - | subscriber.c | limited tests for qos parameter, listeners tested in listener.c
| dds_create_publisher | ok | incomplete: dds_entity_lock | - | publisher.c | limited tests for qos parameter, listeners tested in listener.c
| dds_create_reader | ok | incomplete | - | reader.c | ok
| dds_create_reader_rhc | add use-case and/or link to create_rhc functions? | incomplete | - | **_tests missing_** | -
| dds_create_writer | ok | incomplete | - | writer.c | ok


## Entity hierarchy

|Function | Documentation | Return codes | Remarks | Tests | Test quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_get_parent | link to (generic) Listener and status information | missing PRECONDITION_NOT_MET | - | domain.c; entity_hierarchy.c | ok
| dds_get_publisher | allowed entity kind not mentioned, and missing link to generic dds entity relations documentation | missing: PRECONDITION_NOT_MET and ILLEGAL_OP | - | entity_hierarchy.c | ok
| dds_get_subscriber | see above | ? | - | entity_hierarchy.c | ok
| dds_get_datareader | see above | ? | - | entity_hierarchy.c | ok
| dds_get_participant | see above | ? | - | entity_hierarchy.c | ok
| dds_get_children | see above | ? | - | entity_hierarchy.c; entity_api.c | ok
| dds_get_mask | missing info on mask (described in dds_create_readcond) | ? | - | querycondition.c; readcondition.c | ok
| dds_find_topic | n/a | n/a | replaced in topic discovery PR | - | -
| dds_get_topic | ok | missing PRECONDITION_NOT_MET | - | entity_hierarchy.c | ok

## QoS

|Function | Documentation | Return codes | Remarks | Tests | Test quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_get_qos, <br />dds_set_qos | link to generic QoS information documentation | missing PRECONDITION_NOT_MET | - | entity_api.c, /src/mpt/tests/qos/ | ok
| dds_create_qos, <br />dds_delete_qos, <br />dds_reset_qos, <br />dds_copy_qos, <br />dds_merge_qos, <br />dds_qos_equal | ok | ok | - | qos.c | **_missing_** tests for dds_merge_qos; limited tests for dds_copy_qos and dds_qos_equal
| | |
| dds_qset_userdata | missing reference to generic documentation on QoS | ok (void) | - | qos.c; /src/mpt/tests/qos/ | ok
| dds_qset_topicdata | missing reference to generic documentation on QoS | ok (void) | - | qos.c | ok
| dds_qset_groupdata | missing reference to generic documentation on QoS | ok (void) | - | qos.c | ok
| dds_qset_durability | missing reference to generic documentation on QoS | ok (void) | - | qos.c | also covered in whc, deadline and transient-local tests
| dds_qset_history | missing reference to generic documentation on QoS | ok (void) | - | qos.c; whc.c | also covered in whc tests
| dds_qset_resource_limits | missing reference to generic documentation on QoS | ok (void) | - | qos.c | only basic tests (also covered in listener/oneliner)
| dds_qset_presentation | missing reference to generic documentation on QoS | n/a | not supported | qos.c | n/a
| dds_qset_lifespan | missing reference to generic documentation on QoS | ok (void) | - | qos.c; lifespan.c | ok
| dds_qset_deadline | missing reference to generic documentation on QoS | ok (void) | - | qos.c; deadline.c | ok
| dds_qset_latency_budget | missing reference to generic documentation on QoS | n/a | not supported | qos.c | n/a
| dds_qset_ownership | missing reference to generic documentation on QoS | ok (void) | - | qos.c; filter.c | ok
| dds_qset_ownership_strength | missing reference to generic documentation on QoS | ok (void) | - | qos.c; filter.c | ok
| dds_qset_liveliness | missing reference to generic documentation on QoS | ok (void) | - | qos.c; liveliness.c | ok
| dds_qset_time_based_filter | missing reference to generic documentation on QoS | n/a | not supported | qos.c | n/a
| dds_qset_partition | missing reference to generic documentation on QoS | ok (void) | - | qos.c; publisher.c; plist.c | limited tests
| dds_qset_partition1 | missing reference to generic documentation on QoS | ok (void) | - | qos.c | see dds_qset_partition
| dds_qset_reliability | missing reference to generic documentation on QoS | ok (void) | - | qos.c; whc.c | limited tests for best-effort
| dds_qset_transport_priority | missing reference to generic documentation on QoS | n/a | not supported | qos.c | n/a
| dds_qset_destination_order | missing reference to generic documentation on QoS | ok (void) | - | qos.c | limited tests
| dds_qset_writer_data_lifecycle | missing reference to generic documentation on QoS | ok (void) | - | qos.c | autodispose disabled in many tests, but not explicitly tested
| dds_qset_reader_data_lifecycle | missing reference to generic documentation on QoS | ok (void) | - | qos.c | limited tests, delays for purging not tested
| dds_qset_durability_service | missing reference to generic documentation on QoS | ok (void) | - | qos.c | partially covered in whc tests
| dds_qset_ignorelocal | missing reference to generic documentation on QoS | ok (void) | - | - | **_missing_**
| dds_qset_prop, <br />dds_qunset_prop, <br />dds_qset_bprop, <br />dds_qunset_bprop | missing reference to generic documentation on QoS | ok (void) | - | qos.c | ok
| | |
| dds_qget_userdata | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_topicdata | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_groupdata | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_durability | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_history | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_resource_limits | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_presentation | missing reference to generic documentation on QoS | ok | not supported | qos.c | n/a
| dds_qget_lifespan | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_deadline | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_latency_budget | missing reference to generic documentation on QoS | ok | not supported | qos.c | n/a
| dds_qget_ownership | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_ownership_strength | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_liveliness | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_time_based_filter | missing reference to generic documentation on QoS | ok | not supported | qos.c | n/a
| dds_qget_partition | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_reliability | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_transport_priority | missing reference to generic documentation on QoS | ok | not supported | qos.c | n/a
| dds_qget_destination_order | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_writer_data_lifecycle | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_reader_data_lifecycle | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_durability_service | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_ignorelocal | missing reference to generic documentation on QoS | ok | - | - | **_missing_**
| dds_qget_propnames | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_prop | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_bpropnames | missing reference to generic documentation on QoS | ok | - | qos.c | ok
| dds_qget_bprop | missing reference to generic documentation on QoS | ok | - | qos.c | ok

## Topic filter

|Function | Documentation | Return codes | Remarks | Tests
|-------- | ------------- | ------------ | ------- | -----
| _\[deprecated\] dds_set_topic_filter_ | _n/a_ | _n/a_ | - |
| _\[deprecated\] dds_topic_set_filter_ | _n/a_ | _n/a_ | - |
| dds_set_topic_filter_and_arg | - | - | to be replaced | -
| dds_set_topic_filter_extended | - | - | to be replaced | -
| _\[deprecated\] dds_get_topic_filter_ | _n/a_ | _n/a_ | - |
| _\[deprecated\] dds_topic_get_filter_ | _n/a_ | _n/a_ | - |
| dds_get_topic_filter_and_arg | - | - | to be replaced | -
| dds_get_topic_filter_extended | - | - | to be replaced | -

## Instances

|Function | Documentation | Return codes | Remarks | Tests | Test quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_register_instance | limited description of data parameter (how is this parameter used?), link to generic documentation on instances | missing PRECONDITION_NOT_MET (from handle_pin) | - | register.c | ok
| dds_lookup_instance | ok | ok | - | - | register.c; unregister.c | ok
| _\[deprecated\] dds_instance_lookup_ | _n/a_ | _n/a_ | _replaced by: dds_lookup_instance_ | - | n/a
| dds_instance_get_key | link to generic data (sample) doc | missing ILLEGAL_OPERATION and PRECONDITION_NOT_MET | - | instance_get_key.c | ok
| dds_unregister_instance, <br />dds_unregister_instance_ih, <br />dds_unregister_instance_ts, <br />dds_unregister_instance_ih_ts | see dds_register_instance | missing PRECONDITION_NOT_MET | - | unregister.c |
| dds_writedispose, <br />dds_writedispose_ts | see dds_register_instance remark on data parameter | - | - | dispose.c | ok
| dds_dispose<br />, dds_dispose_ts<br />, dds_dispose_ih, <br />dds_dispose_ih_ts | ok | missing PRECONDITION_NOT_MET | - | dispose.c | ok

## Write

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_write | needs a better description, e.g. of the data parameter and wrt source timestamp | **_missing_** | - | write.c | ok
| dds_write_flush | documentation missing, is this function still needed | **_missing_** | - | **_tests missing_** | -
| dds_writecdr | link to docs on creating a serdata object | missing PRECONDITION_NOT_MET | - | cdr.c | ok
| dds_write_ts | see dds_write | **_missing_** | - | write.c; multi_sertopic.c | limited tests
| dds_write_set_batch | ok | ok | in dds_public_impl.h | - | **_tests missing_** but used in ddsperf/pubsub

## Conditions

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_create_readcondition | ok | ? | - | readcondition.c | ok
| dds_create_querycondition | explain the filter (aka expression & parameters) of the (to be implemented) new querycondition implementation; update parameters when new querycondition is introduced | - | - | querycondition.c |
| dds_create_guardcondition | description is not clear | ? |  | guardcondition.c | ok
| dds_set_guardcondition | ok | ok | - | guardcondition.c | ok
| dds_read_guardcondition | ok | ? | - | guardcondition.c | ok
| dds_take_guardcondition | ok | ? | - | guardcondition.c | ok

## Waitset

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_create_waitset | ok | BAD_PARAM missing | - | waitset.c | ok, also covered in several other tests
| dds_waitset_get_entities | ok | ok | - | waitset.c | ok
| dds_waitset_attach | ok | ok | - | waitset.c | ok
| dds_waitset_detach | ok | ok | - | waitset.c | ok
| dds_waitset_set_trigger | ok | ok | - | waitset.c | ok
| dds_waitset_wait | ok | ok | - | waitset.c | ok
| dds_waitset_wait_until | ok | ? | - | waitset.c | ok

## Read/take

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_read | unclear description: valid_data wrt values written to buffer, buffer null values for loaned samples, bufsz vs maxs (mention dds_read_wl), text 'data values (of same type)' | ok | - | reader.c |
| dds_read_wl | refer to dds_read, identical operation with null ptrs in buffer | ok | - | reader.c |
| dds_read_mask | see dds_read | ok |  | reader.c |
| dds_read_mask_wl | see dds_read | ok |  | reader.c |
| dds_read_instance, <br />dds_read_instance_wl, <br />dds_read_instance_mask, <br />dds_read_instance_mask_wl | link to docs on instance handles | ok | - | read_instance.c |
| dds_take | same issues as dds_read and unclear description of take vs read | ok | - | reader.c |
| dds_take_wl | mention difference with regular dds_take | ok | - | reader.c |
| dds_take_mask | see dds_take | ok | - | reader.c |
| dds_take_mask_wl | see dds_take | ok | - | reader.c |
| dds_readcdr | same issues as dds_read | ok | - | **_tests missing _** |
| dds_takecdr | same issues as dds_read | ok | - | cdr.c |
| dds_take_instance, <br />dds_take_instance_wl, <br />dds_take_instance_mask, <br />dds_take_instance_mask_wl | link to docs on instance handles | ok | - | take_instance.c |
| dds_take_next, <br />, dds_take_next_wl<br />, dds_read_next<br />dds_read_next_wl | describe definition of 'next' wrt multiple instances? | missing PRECONDITION_NOT_MET | - | reader_iterator.c |
| dds_return_loan | ok | incomplete | - | loan.c |

## Memory allocation and string functions

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_alloc, <br />dds_realloc,<br /> dds_realloc_zero,<br /> dds_free | docs missing (add a reference to stdlib manual) | - | - | **_tests missing _** |
| dds_string_alloc, <br />dds_string_dup, <br />dds_string_free, <br />dds_sample_free | missing | - | - | **_tests missing _** |

## Listener

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_get_listener, <br />dds_set_listener | link to (generic) Listener and status information | missing PRECONDITION_NOT_MET | - | entity_api.c |
| dds_create_listener | ok | ok | - | listener.c |
| dds_delete_listener, <br />dds_reset_listener, <br />dds_copy_listener, <br />dds_merge_listener | ok | ok (void) | - | listener.c |
| dds_lset_*, <br />dds_lget_* | ok | ok | - | listener.c | one-liner tests for listener functionality

## Status

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_read_status, <br />dds_take_status, <br />dds_get_status_changes, <br />dds_get_status_mask, <br />dds_set_status_mask | limited info on status: refer to a section in DDS spec | ? | - | entity_status.c |
| _\[deprecated\] dds_get_enabled_status_ | _n/a_ | n/a | _replaced by: dds_get_status_mask_ | - |
| _\[deprecated\] dds_set_enabled_status_ | _n/a_ | n/a | _replaced by: dds_set_status_mask_ | - |
| dds_get_inconsistent_topic_status, <br /> dds_get_publication_matched_status, <br />dds_get_liveliness_lost_status, <br />dds_get_offered_deadline_missed_status, <br />dds_get_offered_incompatible_qos_status, <br />dds_get_subscription_matched_status, <br />dds_get_liveliness_changed_status, <br />dds_get_sample_rejected_status, <br />dds_get_sample_lost_status, <br />dds_get_requested_deadline_missed_status, <br />dds_get_requested_incompatible_qos_status | missing reference to generic documentation on statusses | ? | - | entity_status.c |


## Other functions

|Function | Documentation | Return codes | Remarks | Tests | Test quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_enable | ok | incomplete | delayed entity enabling not implemented (open PR) | - |
| dds_delete | error handling not well described | ? | - | entity_hierarchy.c |
| dds_get_instance_handle | missing info on instance handles | incomplete | - | entity_api.c |
| dds_get_guid | missing info on entity GUIDs | incomplete | ? | entity_api.c |
| dds_get_domainid | mention internal/external domain id | ? | - | domain.c; entity_hierarchy.c |
| dds_lookup_participant | ok | ? | - | participant.c |
| dds_get_name | ok | incomplete (BAD_PARAM missing) | - | topic.c |
| dds_get_type_name | ok | incomplete (dds_entity_pin retcodes and BAD_PARAM missing) | - | topic.c |
| dds_suspend, <br />dds_resume | unclear text: .. collecting modifications to DDS writers .. | ? | - | publisher.c |
| dds_wait_for_acks | what happens with new matched during waiting for acks? | missing PRECONDITION_NOT_MET | - | publisher.c | ok
| dds_reader_wait_for_historical_data | missing definition of 'historical data' | incomplete | not implemented yet | **_tests missing_** |
| dds_notify_readers | what is the use case for this function? | ? | unsupported | - | n/a, unsupported function
| dds_triggered | ok | missing PRECONDITION_NOT_MET | - | entity_status.c; waitset.c | ok
| dds_get_matched_subscriptions, <br />dds_get_matched_subscription_data | ok | ? | - | **_tests missing _** | used in /src/mpt/tests/qos/, but limited tests
| dds_get_matched_publications, <br />dds_get_matched_publication_data | ok | ? |  | **_tests missing _** | used in liveliness tests and /src/mpt/tests/qos/, but limited tests
| dds_assert_liveliness | ok | dds_entity_pin retcodes missing | - | liveliness.c | ok
| dds_domain_set_deafmute | ok | ? | - | listener.c | tested of the oneliner tests for listeners
| dds_begin_coherent | n/a | n/a | unsupported | n/a | n/a
| dds_end_coherent | n/a | n/a | unsupported | n/a | n/a

## Semi-public API

|Function | Documentation | Return codes | Remarks | Tests | Test Quality
|-------- | ------------- | ------------ | ------- | ----- | ------------
| dds_rhc_* | **_docs missing_** | ? | - | rhc_torture test |
| dds_*_statistics | ok | ? | used in ddsperf | **_tests missing _** |
| dds_reader_lock_samples | ok | ? | used in the C++ api | - |
| dds_set_log_mask | **_docs missing_** | ? | used in RMW | log.c |
| ddsi_serdata_init, <br />ddsi_serdata_ref, <br />ddsi_serdata_unref | **_docs missing_** | ? | used in RMW | - | no specific tests, but covered in other tests (e.g. cdr.c)
| ddsi_sertopic_init, <br />ddsi_sertopic_unref, <br />ddsi_sertopic_compute_serdata_basehash | **_docs missing_** | ? | used in RMW | - | no specific tests, but covered in other tests
| ddsi_iid_gen | **_docs missing_** | ? | used in RMW | - | no specific tests, but covered in other tests
