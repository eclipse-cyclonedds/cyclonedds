// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>

#include "dds/dds.h"
#include "dds__listener.h"

dds_listener_t *dds_create_listener (void* arg)
{
  dds_listener_t *l = dds_alloc (sizeof (*l));
  dds_reset_listener (l);
  l->on_inconsistent_topic_arg = arg;
  l->on_liveliness_lost_arg = arg;
  l->on_offered_deadline_missed_arg = arg;
  l->on_offered_incompatible_qos_arg = arg;
  l->on_data_on_readers_arg = arg;
  l->on_sample_lost_arg = arg;
  l->on_data_available_arg = arg;
  l->on_sample_rejected_arg = arg;
  l->on_liveliness_changed_arg = arg;
  l->on_requested_deadline_missed_arg = arg;
  l->on_requested_incompatible_qos_arg = arg;
  l->on_publication_matched_arg = arg;
  l->on_subscription_matched_arg = arg;
  return l;
}

void dds_delete_listener (dds_listener_t * __restrict listener)
{
  dds_free (listener);
}

void dds_reset_listener (dds_listener_t * __restrict listener)
{
  if (listener)
  {
    dds_listener_t * const l = listener;
    l->inherited = 0;
    l->reset_on_invoke = 0;
    l->on_data_available = 0;
    l->on_data_on_readers = 0;
    l->on_inconsistent_topic = 0;
    l->on_liveliness_changed = 0;
    l->on_liveliness_lost = 0;
    l->on_offered_deadline_missed = 0;
    l->on_offered_incompatible_qos = 0;
    l->on_publication_matched = 0;
    l->on_requested_deadline_missed = 0;
    l->on_requested_incompatible_qos = 0;
    l->on_sample_lost = 0;
    l->on_sample_rejected = 0;
    l->on_subscription_matched = 0;
  }
}

void dds_copy_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  if (dst && src)
    *dst = *src;
}

static bool dds_combine_listener_merge (uint32_t inherited, void (*dst)(void), void (*src)(void))
{
  (void)inherited;
  return dst == 0 && src != 0;
}

static bool dds_combine_listener_override_inherited (uint32_t inherited, void (*dst)(void), void (*src)(void))
{
  (void)dst;
  (void)src;
  return inherited;
}

static uint32_t copy_bits (uint32_t a, uint32_t b, uint32_t mask)
{
  return (a & ~mask) | (b & mask);
}

static uint32_t combine_reset_on_invoke (const dds_listener_t *dst, const dds_listener_t *src, uint32_t status)
{
  return copy_bits (dst->reset_on_invoke, src->reset_on_invoke, status);
}

static void dds_combine_listener (bool (*op) (uint32_t inherited, void (*)(void), void (*)(void)), dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
#define C(NAME_, name_) do { \
    if (op (dst->inherited & DDS_##NAME_##_STATUS, (void (*)(void)) dst->on_##name_, (void (*)(void)) src->on_##name_)){ \
      dst->inherited |= DDS_##NAME_##_STATUS; \
      dst->reset_on_invoke = combine_reset_on_invoke (dst, src, DDS_##NAME_##_STATUS); \
      dst->on_##name_ = src->on_##name_; \
      dst->on_##name_##_arg = src->on_##name_##_arg; \
    } \
  } while (0)
  C(DATA_AVAILABLE, data_available);
  C(DATA_ON_READERS, data_on_readers);
  C(INCONSISTENT_TOPIC, inconsistent_topic);
  C(LIVELINESS_CHANGED, liveliness_changed);
  C(LIVELINESS_LOST, liveliness_lost);
  C(OFFERED_DEADLINE_MISSED, offered_deadline_missed);
  C(OFFERED_INCOMPATIBLE_QOS, offered_incompatible_qos);
  C(PUBLICATION_MATCHED, publication_matched);
  C(REQUESTED_DEADLINE_MISSED, requested_deadline_missed);
  C(REQUESTED_INCOMPATIBLE_QOS, requested_incompatible_qos);
  C(SAMPLE_LOST, sample_lost);
  C(SAMPLE_REJECTED, sample_rejected);
  C(SUBSCRIPTION_MATCHED, subscription_matched);
#undef C
}

void dds_override_inherited_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  if (dst && src)
    dds_combine_listener (dds_combine_listener_override_inherited, dst, src);
}

void dds_inherit_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  if (dst && src)
    dds_combine_listener (dds_combine_listener_merge, dst, src);
}

void dds_merge_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  if (dst && src)
  {
    uint32_t inherited = dst->inherited;
    dds_combine_listener (dds_combine_listener_merge, dst, src);
    dst->inherited = inherited;
  }
}

#define DDS_SET_LISTENER_ARG(NAME_, name_) \
  dds_return_t dds_lset_##name_##_arg (dds_listener_t * __restrict listener, dds_on_##name_##_fn callback, void *arg, bool reset_on_invoke) \
  { \
    if (listener == NULL) \
      return DDS_RETCODE_BAD_PARAMETER; \
    listener->reset_on_invoke = copy_bits (listener->reset_on_invoke, reset_on_invoke ? ~(uint32_t)0 : 0, DDS_##NAME_##_STATUS); \
    listener->on_##name_ = callback; \
    listener->on_##name_##_arg = arg; \
    return DDS_RETCODE_OK; \
  }
DDS_SET_LISTENER_ARG (DATA_AVAILABLE, data_available)
DDS_SET_LISTENER_ARG (DATA_ON_READERS, data_on_readers)
DDS_SET_LISTENER_ARG (INCONSISTENT_TOPIC, inconsistent_topic)
DDS_SET_LISTENER_ARG (LIVELINESS_CHANGED, liveliness_changed)
DDS_SET_LISTENER_ARG (LIVELINESS_LOST, liveliness_lost)
DDS_SET_LISTENER_ARG (OFFERED_DEADLINE_MISSED, offered_deadline_missed)
DDS_SET_LISTENER_ARG (OFFERED_INCOMPATIBLE_QOS, offered_incompatible_qos)
DDS_SET_LISTENER_ARG (PUBLICATION_MATCHED, publication_matched)
DDS_SET_LISTENER_ARG (REQUESTED_DEADLINE_MISSED, requested_deadline_missed)
DDS_SET_LISTENER_ARG (REQUESTED_INCOMPATIBLE_QOS, requested_incompatible_qos)
DDS_SET_LISTENER_ARG (SAMPLE_LOST, sample_lost)
DDS_SET_LISTENER_ARG (SAMPLE_REJECTED, sample_rejected)
DDS_SET_LISTENER_ARG (SUBSCRIPTION_MATCHED, subscription_matched)
#undef DDS_SET_LISTENER_ARG

#define DDS_SET_LISTENER(NAME_, name_) \
  void dds_lset_##name_ (dds_listener_t * __restrict listener, dds_on_##name_##_fn callback) { \
    if (listener) \
      (void) dds_lset_##name_##_arg (listener, callback, listener->on_##name_##_arg, true); \
  }
DDS_SET_LISTENER (DATA_AVAILABLE, data_available)
DDS_SET_LISTENER (DATA_ON_READERS, data_on_readers)
DDS_SET_LISTENER (INCONSISTENT_TOPIC, inconsistent_topic)
DDS_SET_LISTENER (LIVELINESS_CHANGED, liveliness_changed)
DDS_SET_LISTENER (LIVELINESS_LOST, liveliness_lost)
DDS_SET_LISTENER (OFFERED_DEADLINE_MISSED, offered_deadline_missed)
DDS_SET_LISTENER (OFFERED_INCOMPATIBLE_QOS, offered_incompatible_qos)
DDS_SET_LISTENER (PUBLICATION_MATCHED, publication_matched)
DDS_SET_LISTENER (REQUESTED_DEADLINE_MISSED, requested_deadline_missed)
DDS_SET_LISTENER (REQUESTED_INCOMPATIBLE_QOS, requested_incompatible_qos)
DDS_SET_LISTENER (SAMPLE_LOST, sample_lost)
DDS_SET_LISTENER (SAMPLE_REJECTED, sample_rejected)
DDS_SET_LISTENER (SUBSCRIPTION_MATCHED, subscription_matched)
#undef DDS_SET_LISTENER

#define DDS_GET_LISTENER_ARG(NAME_, name_) \
  dds_return_t dds_lget_##name_##_arg (const dds_listener_t * __restrict listener, dds_on_##name_##_fn *callback, void **arg, bool *reset_on_invoke) \
  { \
    if (listener == NULL) \
      return DDS_RETCODE_BAD_PARAMETER; \
    if (callback) \
      *callback = listener->on_##name_; \
    if (arg) \
      *arg = listener->on_##name_##_arg; \
    if (reset_on_invoke) \
      *reset_on_invoke = (listener->reset_on_invoke & DDS_##NAME_##_STATUS) != 0; \
    return DDS_RETCODE_OK; \
  }
DDS_GET_LISTENER_ARG (DATA_AVAILABLE, data_available)
DDS_GET_LISTENER_ARG (DATA_ON_READERS, data_on_readers)
DDS_GET_LISTENER_ARG (INCONSISTENT_TOPIC, inconsistent_topic)
DDS_GET_LISTENER_ARG (LIVELINESS_CHANGED, liveliness_changed)
DDS_GET_LISTENER_ARG (LIVELINESS_LOST, liveliness_lost)
DDS_GET_LISTENER_ARG (OFFERED_DEADLINE_MISSED, offered_deadline_missed)
DDS_GET_LISTENER_ARG (OFFERED_INCOMPATIBLE_QOS, offered_incompatible_qos)
DDS_GET_LISTENER_ARG (PUBLICATION_MATCHED, publication_matched)
DDS_GET_LISTENER_ARG (REQUESTED_DEADLINE_MISSED, requested_deadline_missed)
DDS_GET_LISTENER_ARG (REQUESTED_INCOMPATIBLE_QOS, requested_incompatible_qos)
DDS_GET_LISTENER_ARG (SAMPLE_LOST, sample_lost)
DDS_GET_LISTENER_ARG (SAMPLE_REJECTED, sample_rejected)
DDS_GET_LISTENER_ARG (SUBSCRIPTION_MATCHED, subscription_matched)
#undef DDS_GET_LISTENER_ARG

#define DDS_GET_LISTENER(name_) \
  void dds_lget_##name_ (const dds_listener_t * __restrict listener, dds_on_##name_##_fn *callback) { \
    (void) dds_lget_##name_##_arg (listener, callback, NULL, NULL); \
  }
DDS_GET_LISTENER (data_available)
DDS_GET_LISTENER (data_on_readers)
DDS_GET_LISTENER (inconsistent_topic)
DDS_GET_LISTENER (liveliness_changed)
DDS_GET_LISTENER (liveliness_lost)
DDS_GET_LISTENER (offered_deadline_missed)
DDS_GET_LISTENER (offered_incompatible_qos)
DDS_GET_LISTENER (publication_matched)
DDS_GET_LISTENER (requested_deadline_missed)
DDS_GET_LISTENER (requested_incompatible_qos)
DDS_GET_LISTENER (sample_lost)
DDS_GET_LISTENER (sample_rejected)
DDS_GET_LISTENER (subscription_matched)
#undef DDS_GET_LISTENER
