
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

typedef struct dummy_mockstats_s{
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
}dummy_mockstats_t;

struct dummy_psmx {
  struct dds_psmx c;
  void (*mockstats_get_ownership)(dummy_mockstats_t*);
};

#if defined (__cplusplus)
}
#endif

#endif /* PSMX_DUMMY_PUBLIC_H */
