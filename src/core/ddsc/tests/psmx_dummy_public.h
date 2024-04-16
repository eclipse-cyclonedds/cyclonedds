
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

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct dynamic_array_s{
  uint32_t _maximum;
  uint32_t _length;
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
  char* config;
  dynamic_array_t topics;
  dynamic_array_t endpoints;
}dummy_mockstats_t;

typedef struct dummy_psmx {
  dds_psmx_t c;
  dummy_mockstats_t* (*mockstats_get_ptr)();
}dummy_psmx_t;

DDS_EXPORT void dummy_topics_alloc(dummy_mockstats_t* mockstats, uint32_t topics_capacity);
DDS_EXPORT void dummy_endpoints_alloc(dummy_mockstats_t* mockstats, uint32_t endpoints_capacity);

#if defined (__cplusplus)
}
#endif

#endif /* PSMX_DUMMY_PUBLIC_H */
