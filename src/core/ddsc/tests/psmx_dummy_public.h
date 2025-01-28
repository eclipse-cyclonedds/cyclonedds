
// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef PSMX_DUMMY_PUBLIC_H
#define PSMX_DUMMY_PUBLIC_H

#include "dds/psmx_dummy/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct dynamic_array_s{
  size_t _maximum;
  size_t _length;
  void* _buffer;
}dynamic_array_t;

typedef struct dummy_mockstats_s{
  int cnt_create_psmx;

  // dds_psmx_ops
  int cnt_type_qos_supported;
  int cnt_create_topic;
  int cnt_delete_topic;
  int cnt_deinit;
  int cnt_get_node_id;
  int cnt_supported_features;

  // dds_psmx_topic_ops
  int cnt_create_endpoint;
  int cnt_delete_endpoint;

  // dds_psmx_endpoint_ops
  int cnt_request_loan;
  int cnt_write;
  int cnt_take;
  int cnt_on_data_available;

  // Exposed internals
  bool supports_shared_memory;
  bool fail_create_topic;
  char* config;
  dynamic_array_t topics;
  dynamic_array_t endpoints;
  dynamic_array_t loans;
  dds_loaned_sample_t loan;
  dds_psmx_metadata_t loan_metadata;

  dds_psmx_topic_t* create_endpoint_rcv_topic;
  dds_psmx_endpoint_t* delete_endpoint_rcv_endpt;
  dds_psmx_endpoint_t* request_loan_rcv_endpt;
  dds_psmx_endpoint_t* write_rcv_endpt;
  dds_loaned_sample_t* write_rcv_loan;
}dummy_mockstats_t;

PSMX_DUMMY_EXPORT dummy_mockstats_t* dummy_mockstats_get_ptr(void);
PSMX_DUMMY_EXPORT void dummy_topics_alloc(dummy_mockstats_t* mockstats, size_t topics_capacity);
PSMX_DUMMY_EXPORT void dummy_endpoints_alloc(dummy_mockstats_t* mockstats, size_t endpoints_capacity);
PSMX_DUMMY_EXPORT void dummy_loans_alloc(dummy_mockstats_t* mockstats, size_t loans_capacity);

#if defined (__cplusplus)
}
#endif

#endif /* PSMX_DUMMY_PUBLIC_H */
