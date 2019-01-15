/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include "ddsc/dds.h"
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

dds_listener_t *dds_listener_create (void* arg)
{
  return dds_create_listener (arg);
}

void dds_delete_listener (dds_listener_t * __restrict listener)
{
  dds_free (listener);
}

void dds_listener_delete (dds_listener_t * __restrict listener)
{
  dds_delete_listener (listener);
}

void dds_reset_listener (dds_listener_t * __restrict listener)
{
  if (listener)
  {
    dds_listener_t * const l = listener;
    l->inherited = 0;
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

void dds_listener_reset (dds_listener_t * __restrict listener)
{
  dds_reset_listener (listener);
}

void dds_copy_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  if (dst && src)
    *dst = *src;
}

void dds_listener_copy(dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  dds_copy_listener (dst, src);
}

static bool dds_combine_listener_merge (uint32_t inherited, void (*dst)(void), void (*src)(void))
{
  (void)inherited;
  (void)src;
  return dst == 0;
}

static bool dds_combine_listener_override_inherited (uint32_t inherited, void (*dst)(void), void (*src)(void))
{
  (void)dst;
  (void)src;
  return inherited;
}

static void dds_combine_listener (bool (*op) (uint32_t inherited, void (*)(void), void (*)(void)), dds_listener_t * __restrict dst, const dds_listener_t * __restrict src)
{
  if (op (dst->inherited & DDS_DATA_AVAILABLE_STATUS, (void (*)(void)) dst->on_data_available, (void (*)(void)) src->on_data_available))
  {
    dst->inherited |= DDS_DATA_AVAILABLE_STATUS;
    dst->on_data_available = src->on_data_available;
    dst->on_data_available_arg = src->on_data_available_arg;
  }
  if (op (dst->inherited & DDS_DATA_ON_READERS_STATUS, (void (*)(void)) dst->on_data_on_readers, (void (*)(void)) src->on_data_on_readers))
  {
    dst->inherited |= DDS_DATA_ON_READERS_STATUS;
    dst->on_data_on_readers = src->on_data_on_readers;
    dst->on_data_on_readers_arg = src->on_data_on_readers_arg;
  }
  if (op (dst->inherited & DDS_INCONSISTENT_TOPIC_STATUS, (void (*)(void)) dst->on_inconsistent_topic, (void (*)(void)) src->on_inconsistent_topic))
  {
    dst->inherited |= DDS_INCONSISTENT_TOPIC_STATUS;
    dst->on_inconsistent_topic = src->on_inconsistent_topic;
    dst->on_inconsistent_topic_arg = src->on_inconsistent_topic_arg;
  }
  if (op (dst->inherited & DDS_LIVELINESS_CHANGED_STATUS, (void (*)(void)) dst->on_liveliness_changed, (void (*)(void)) src->on_liveliness_changed))
  {
    dst->inherited |= DDS_LIVELINESS_CHANGED_STATUS;
    dst->on_liveliness_changed = src->on_liveliness_changed;
    dst->on_liveliness_changed_arg = src->on_liveliness_changed_arg;
  }
  if (op (dst->inherited & DDS_LIVELINESS_LOST_STATUS, (void (*)(void)) dst->on_liveliness_lost, (void (*)(void)) src->on_liveliness_lost))
  {
    dst->inherited |= DDS_LIVELINESS_LOST_STATUS;
    dst->on_liveliness_lost = src->on_liveliness_lost;
    dst->on_liveliness_lost_arg = src->on_liveliness_lost_arg;
  }
  if (op (dst->inherited & DDS_OFFERED_DEADLINE_MISSED_STATUS, (void (*)(void)) dst->on_offered_deadline_missed, (void (*)(void)) src->on_offered_deadline_missed))
  {
    dst->inherited |= DDS_OFFERED_DEADLINE_MISSED_STATUS;
    dst->on_offered_deadline_missed = src->on_offered_deadline_missed;
    dst->on_offered_deadline_missed_arg = src->on_offered_deadline_missed_arg;
  }
  if (op (dst->inherited & DDS_OFFERED_INCOMPATIBLE_QOS_STATUS, (void (*)(void)) dst->on_offered_incompatible_qos, (void (*)(void)) src->on_offered_incompatible_qos))
  {
    dst->inherited |= DDS_OFFERED_INCOMPATIBLE_QOS_STATUS;
    dst->on_offered_incompatible_qos = src->on_offered_incompatible_qos;
    dst->on_offered_incompatible_qos_arg = src->on_offered_incompatible_qos_arg;
  }
  if (op (dst->inherited & DDS_PUBLICATION_MATCHED_STATUS, (void (*)(void)) dst->on_publication_matched, (void (*)(void)) src->on_publication_matched))
  {
    dst->inherited |= DDS_PUBLICATION_MATCHED_STATUS;
    dst->on_publication_matched = src->on_publication_matched;
    dst->on_publication_matched_arg = src->on_publication_matched_arg;
  }
  if (op (dst->inherited & DDS_REQUESTED_DEADLINE_MISSED_STATUS, (void (*)(void)) dst->on_requested_deadline_missed, (void (*)(void)) src->on_requested_deadline_missed))
  {
    dst->inherited |= DDS_REQUESTED_DEADLINE_MISSED_STATUS;
    dst->on_requested_deadline_missed = src->on_requested_deadline_missed;
    dst->on_requested_deadline_missed_arg = src->on_requested_deadline_missed_arg;
  }
  if (op (dst->inherited & DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS, (void (*)(void)) dst->on_requested_incompatible_qos, (void (*)(void)) src->on_requested_incompatible_qos))
  {
    dst->inherited |= DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS;
    dst->on_requested_incompatible_qos = src->on_requested_incompatible_qos;
    dst->on_requested_incompatible_qos_arg = src->on_requested_incompatible_qos_arg;
  }
  if (op (dst->inherited & DDS_SAMPLE_LOST_STATUS, (void (*)(void)) dst->on_sample_lost, (void (*)(void)) src->on_sample_lost))
  {
    dst->inherited |= DDS_SAMPLE_LOST_STATUS;
    dst->on_sample_lost = src->on_sample_lost;
    dst->on_sample_lost_arg = src->on_sample_lost_arg;
  }
  if (op (dst->inherited & DDS_SAMPLE_REJECTED_STATUS, (void (*)(void)) dst->on_sample_rejected, (void (*)(void)) src->on_sample_rejected))
  {
    dst->inherited |= DDS_SAMPLE_REJECTED_STATUS;
    dst->on_sample_rejected = src->on_sample_rejected;
    dst->on_sample_rejected_arg = src->on_sample_rejected_arg;
  }
  if (op (dst->inherited & DDS_SUBSCRIPTION_MATCHED_STATUS, (void (*)(void)) dst->on_subscription_matched, (void (*)(void)) src->on_subscription_matched))
  {
    dst->inherited |= DDS_SUBSCRIPTION_MATCHED_STATUS;
    dst->on_subscription_matched = src->on_subscription_matched;
    dst->on_subscription_matched_arg = src->on_subscription_matched_arg;
  }
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

void dds_listener_merge (_Inout_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src)
{
  dds_merge_listener(dst, src);
}

/************************************************************************************************
 *  Setters
 ************************************************************************************************/

void
dds_lset_data_available (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_data_available_fn callback)
{
    if (listener) {
        listener->on_data_available = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_data_on_readers (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_data_on_readers_fn callback)
{
    if (listener) {
        listener->on_data_on_readers = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_inconsistent_topic (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_inconsistent_topic_fn callback)
{
    if (listener) {
        listener->on_inconsistent_topic = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_liveliness_changed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_liveliness_changed_fn callback)
{
    if (listener) {
        listener->on_liveliness_changed = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_liveliness_lost (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_liveliness_lost_fn callback)
{
    if (listener) {
        listener->on_liveliness_lost = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_offered_deadline_missed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_offered_deadline_missed_fn callback)
{
    if (listener) {
        listener->on_offered_deadline_missed = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_offered_incompatible_qos (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_offered_incompatible_qos_fn callback)
{
    if (listener) {
        listener->on_offered_incompatible_qos = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_publication_matched (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_publication_matched_fn callback)
{
    if (listener) {
        listener->on_publication_matched = callback;
    } else {
        DDS_ERROR("Argument listener is NULL");
    }
}

void
dds_lset_requested_deadline_missed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_requested_deadline_missed_fn callback)
{
    if (listener) {
        listener->on_requested_deadline_missed = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_requested_incompatible_qos (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_requested_incompatible_qos_fn callback)
{
    if (listener) {
        listener->on_requested_incompatible_qos = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_sample_lost (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_sample_lost_fn callback)
{
    if (listener) {
        listener->on_sample_lost = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_sample_rejected (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_sample_rejected_fn callback)
{
    if (listener) {
        listener->on_sample_rejected = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

void
dds_lset_subscription_matched (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_subscription_matched_fn callback)
{
    if (listener) {
        listener->on_subscription_matched = callback;
    } else {
        DDS_ERROR("Argument listener is NULL\n");
    }
}

/************************************************************************************************
 *  Getters
 ************************************************************************************************/

void
dds_lget_data_available (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_data_available_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_data_available;
}

void
dds_lget_data_on_readers (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_data_on_readers_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_data_on_readers;
}

void dds_lget_inconsistent_topic (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_inconsistent_topic_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_inconsistent_topic;
}

void
dds_lget_liveliness_changed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_liveliness_changed_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_liveliness_changed;
}

void
dds_lget_liveliness_lost (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_liveliness_lost_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_liveliness_lost;
}

void
dds_lget_offered_deadline_missed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_offered_deadline_missed_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_offered_deadline_missed;
}

void
dds_lget_offered_incompatible_qos (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_offered_incompatible_qos_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_offered_incompatible_qos;
}

void
dds_lget_publication_matched (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_publication_matched_fn *callback)
{
    if(!callback){
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_publication_matched;
}

void
dds_lget_requested_deadline_missed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_requested_deadline_missed_fn *callback)
{
    if(!callback) {
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_requested_deadline_missed;
}

void
dds_lget_requested_incompatible_qos (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_requested_incompatible_qos_fn *callback)
{
    if(!callback) {
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_requested_incompatible_qos;
}

void
dds_lget_sample_lost (_In_ const dds_listener_t *__restrict listener, _Outptr_result_maybenull_ dds_on_sample_lost_fn *callback)
{
    if(!callback) {
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_sample_lost;
}

void
dds_lget_sample_rejected (_In_ const dds_listener_t  *__restrict listener, _Outptr_result_maybenull_ dds_on_sample_rejected_fn *callback)
{
    if(!callback) {
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_sample_rejected;
}

void
dds_lget_subscription_matched (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_subscription_matched_fn *callback)
{
    if(!callback) {
        DDS_ERROR("Argument callback is NULL\n");
        return ;
    }
    if (!listener) {
        DDS_ERROR("Argument listener is NULL\n");
        return ;
    }
    *callback = listener->on_subscription_matched;
}
