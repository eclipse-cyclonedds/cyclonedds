// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#ifndef DDS__QOS_PROVIDER_H
#define DDS__QOS_PROVIDER_H

#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define PROVIDER_ITEM_SEP        "::"
#define PROVIDER_ITEM_SCOPE_NONE "*"
#define QOSPROV_ERROR(...) DDS_LOG(DDS_LC_QOSPROV | DDS_LC_ERROR,   __VA_ARGS__)
#define QOSPROV_WARN(...)  DDS_LOG(DDS_LC_QOSPROV | DDS_LC_WARNING, __VA_ARGS__)
#define QOSPROV_TRACE(...) DDS_LOG(DDS_LC_QOSPROV | DDS_LC_TRACE,   __VA_ARGS__)

/**
 * @brief Sample structure of the Qos stored in Provider.
 * @ingroup qos_provider
 * @component qos_provider_api
 */
typedef struct dds_qos_item
{
  char *full_name;
  dds_qos_t *qos;
  enum dds_qos_kind kind;
} dds_qos_item_t;

struct dds_qos_provider
{
  char* file_path;
  struct ddsrt_hh *keyed_qos;
};

#if defined (__cplusplus)
}
#endif

#endif // DDS__QOS_PROVIDER_H
