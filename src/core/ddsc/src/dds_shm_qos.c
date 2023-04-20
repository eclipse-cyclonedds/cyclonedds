// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds__qos.h"
#include "dds__topic.h"
#include "dds__shm_qos.h"

static bool is_wildcard_partition (const char *str)
{
  return strchr (str, '*') || strchr (str, '?');
}

#define QOS_CHECK_FIELDS (DDSI_QP_LIVELINESS|DDSI_QP_DEADLINE|DDSI_QP_RELIABILITY|DDSI_QP_DURABILITY|DDSI_QP_HISTORY)

bool dds_shm_compatible_qos_and_topic (const struct dds_qos *qos, const struct dds_topic *tp, bool check_durability_service)
{
  // check necessary condition: fixed size data type OR serializing into shared
  // memory is available
  if (!tp->m_stype->fixed_size && (!tp->m_stype->ops->get_serialized_size ||
                                   !tp->m_stype->ops->serialize_into))
  {
    return false;
  }

  if (qos->history.kind != DDS_HISTORY_KEEP_LAST)
  {
    return false;
  }

  if (!(qos->durability.kind == DDS_DURABILITY_VOLATILE || qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL))
  {
    return false;
  }

  // we cannot support the required history with iceoryx
  if (check_durability_service &&
      qos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL &&
      qos->durability_service.history.kind == DDS_HISTORY_KEEP_LAST &&
      qos->durability_service.history.depth > (int32_t) iox_cfg_max_publisher_history ())
  {
    return false;
  }

  if (qos->ignorelocal.value != DDS_IGNORELOCAL_NONE)
  {
    return false;
  }

  // only default partition or one non-wildcard partition
  if (qos->partition.n > 1 ||
      (qos->partition.n == 1 && is_wildcard_partition (qos->partition.strs[0])))
  {
    return false;
  }

  return (QOS_CHECK_FIELDS == (qos->present & QOS_CHECK_FIELDS) &&
          DDS_LIVELINESS_AUTOMATIC == qos->liveliness.kind &&
          DDS_INFINITY == qos->deadline.deadline);
}

char *dds_shm_partition_topic (const struct dds_qos *qos, const struct dds_topic *tp)
{
  const char *partition = "";
  if (qos->present & DDSI_QP_PARTITION)
  {
    assert (qos->partition.n <= 1);
    if (qos->partition.n == 1)
      partition = qos->partition.strs[0];
  }
  assert (partition);
  assert (!is_wildcard_partition (partition));

  // compute combined string length, allowing for escaping dots
  // (using \ in good traditional C style)
  size_t size = 1 + strlen (tp->m_name) + 1; // dot & terminating 0
  for (char const *src = partition; *src; src++)
  {
    if (*src == '\\' || *src == '.')
      size++;
    size++;
  }
  char *combined = ddsrt_malloc (size);
  if (combined == NULL)
    return NULL;
  char *dst = combined;
  for (char const *src = partition; *src; src++)
  {
    if (*src == '\\' || *src == '.')
      *dst++ = '\\';
    *dst++ = *src;
  }
  *dst++ = '.';
  strcpy (dst, tp->m_name);
  return combined;
}
