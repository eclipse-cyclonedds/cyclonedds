// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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

void dds_delete_listener (dds_listener_t *listener)
{
  dds_free (listener);
}
void dds_reset_listener (dds_listener_t *listener)
{
  if (listener)
  {
    dds_listener_t * const l = listener;
    l->locally_set = 0;
    l->inherited = 0;
    l->reset_on_invoke = 0;
    l->on_data_available = NULL;
    l->on_data_on_readers = NULL;
    l->on_inconsistent_topic = NULL;
    l->on_liveliness_changed = NULL;
    l->on_liveliness_lost = NULL;
    l->on_offered_deadline_missed = NULL;
    l->on_offered_incompatible_qos = NULL;
    l->on_publication_matched = NULL;
    l->on_requested_deadline_missed = NULL;
    l->on_requested_incompatible_qos = NULL;
    l->on_sample_lost = NULL;
    l->on_sample_rejected = NULL;
    l->on_subscription_matched = NULL;
  }
}

void dds_copy_listener (dds_listener_t *dst, const dds_listener_t *src)
{
  if (dst && src)
    *dst = *src;
}

static uint32_t copy_bits (uint32_t a, uint32_t b, uint32_t mask)
{
  return (a & ~mask) | (b & mask);
}

static void copy_listener_callbacks (dds_listener_t *dst, const dds_listener_t *src, uint32_t mask)
{
  dst->reset_on_invoke = copy_bits (dst->reset_on_invoke, src->reset_on_invoke, mask);
#define C(NAME_, name_) do { \
    if (mask & DDS_##NAME_##_STATUS) { \
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

void dds_inherit_listener (dds_listener_t *dst, const dds_listener_t *parent)
{
  /* The parent already includes callbacks inherited from its own ancestors.
     Also refresh previously inherited slots that the parent has now cleared. */
  const uint32_t inherited = (parent->locally_set | parent->inherited) & ~dst->locally_set;
  copy_listener_callbacks (dst, parent, inherited | dst->inherited);
  dst->inherited = inherited;
}

void dds_entity_set_listener (dds_entity *entity, const dds_listener_t *settings)
{
  dds_listener_t *dst = &entity->m_listener;
  *dst = (dds_listener_t) { 0 };
  if (settings)
  {
    dst->locally_set = settings->locally_set;
    /* DATA_ON_READERS on a reader always comes from its subscriber. */
    if (entity->m_kind == DDS_KIND_READER)
      dst->locally_set &= ~DDS_DATA_ON_READERS_STATUS;
    copy_listener_callbacks (dst, settings, dst->locally_set);
  }
  if (entity->m_parent)
  {
    ddsrt_mutex_lock (&entity->m_parent->m_observers_lock);
    dds_inherit_listener (dst, &entity->m_parent->m_listener);
    ddsrt_mutex_unlock (&entity->m_parent->m_observers_lock);
  }
}

void dds_merge_listener (dds_listener_t *dst, const dds_listener_t *src)
{
  if (dst && src)
  {
    const uint32_t mask = (src->locally_set | src->inherited) & ~(dst->locally_set | dst->inherited);
    copy_listener_callbacks (dst, src, mask);
    dst->locally_set |= src->locally_set & mask;
    dst->inherited |= src->inherited & mask;
  }
}

#define DDS_SET_LISTENER_ARG(NAME_, name_) \
  dds_return_t dds_lset_##name_##_arg (dds_listener_t *listener, dds_on_##name_##_fn callback, void *arg, bool reset_on_invoke) \
  { \
    if (listener == NULL) \
      return DDS_RETCODE_BAD_PARAMETER; \
    listener->locally_set |= DDS_##NAME_##_STATUS; \
    listener->inherited &= ~DDS_##NAME_##_STATUS; \
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
  void dds_lset_##name_ (dds_listener_t *listener, dds_on_##name_##_fn callback) { \
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
  dds_return_t dds_lget_##name_##_arg (const dds_listener_t *listener, dds_on_##name_##_fn *callback, void **arg, bool *reset_on_invoke) \
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
  void dds_lget_##name_ (const dds_listener_t *listener, dds_on_##name_##_fn *callback) { \
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
