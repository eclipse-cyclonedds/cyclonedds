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
#include "dds__report.h"



_Ret_notnull_
dds_listener_t*
dds_listener_create(_In_opt_ void* arg)
{
    c_listener_t *l = dds_alloc(sizeof(*l));
    dds_listener_reset(l);
    l->arg = arg;
    return l;
}

void
dds_listener_delete(_In_ _Post_invalid_ dds_listener_t * __restrict listener)
{
    if (listener) {
        dds_free(listener);
    }
}


void
dds_listener_reset(_Out_ dds_listener_t * __restrict listener)
{
    if (listener) {
        c_listener_t *l = listener;
        l->on_data_available = DDS_LUNSET;
        l->on_data_on_readers = DDS_LUNSET;
        l->on_inconsistent_topic = DDS_LUNSET;
        l->on_liveliness_changed = DDS_LUNSET;
        l->on_liveliness_lost = DDS_LUNSET;
        l->on_offered_deadline_missed = DDS_LUNSET;
        l->on_offered_incompatible_qos = DDS_LUNSET;
        l->on_publication_matched = DDS_LUNSET;
        l->on_requested_deadline_missed = DDS_LUNSET;
        l->on_requested_incompatible_qos = DDS_LUNSET;
        l->on_sample_lost = DDS_LUNSET;
        l->on_sample_rejected = DDS_LUNSET;
        l->on_subscription_matched = DDS_LUNSET;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_listener_copy(_Out_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src)
{
    const c_listener_t *srcl = src;
    c_listener_t *dstl = dst;

    if(!src){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument source(src) is NULL");
        return ;
    }
    if(!dst){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument destination(dst) is NULL");
        return ;
    }
    dstl->on_data_available = srcl->on_data_available;
    dstl->on_data_on_readers = srcl->on_data_on_readers;
    dstl->on_inconsistent_topic = srcl->on_inconsistent_topic;
    dstl->on_liveliness_changed = srcl->on_liveliness_changed;
    dstl->on_liveliness_lost = srcl->on_liveliness_lost;
    dstl->on_offered_deadline_missed = srcl->on_offered_deadline_missed;
    dstl->on_offered_incompatible_qos = srcl->on_offered_incompatible_qos;
    dstl->on_publication_matched = srcl->on_publication_matched;
    dstl->on_requested_deadline_missed = srcl->on_requested_deadline_missed;
    dstl->on_requested_incompatible_qos = srcl->on_requested_incompatible_qos;
    dstl->on_sample_lost = srcl->on_sample_lost;
    dstl->on_sample_rejected = srcl->on_sample_rejected;
    dstl->on_subscription_matched = srcl->on_subscription_matched;
}

void
dds_listener_merge (_Inout_ dds_listener_t * __restrict dst, _In_ const dds_listener_t * __restrict src)
{
    const c_listener_t *srcl = src;
    c_listener_t *dstl = dst;

    if(!src){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument source(src) is NULL");
        return ;
    }
    if(!dst){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument destination(dst) is NULL");
        return ;
    }
    if (dstl->on_data_available == DDS_LUNSET) {
        dstl->on_data_available = srcl->on_data_available;
    }
    if (dstl->on_data_on_readers == DDS_LUNSET) {
        dstl->on_data_on_readers = srcl->on_data_on_readers;
    }
    if (dstl->on_inconsistent_topic == DDS_LUNSET) {
        dstl->on_inconsistent_topic = srcl->on_inconsistent_topic;
    }
    if (dstl->on_liveliness_changed == DDS_LUNSET) {
        dstl->on_liveliness_changed = srcl->on_liveliness_changed;
    }
    if (dstl->on_liveliness_lost == DDS_LUNSET) {
        dstl->on_liveliness_lost = srcl->on_liveliness_lost;
    }
    if (dstl->on_offered_deadline_missed == DDS_LUNSET) {
        dstl->on_offered_deadline_missed = srcl->on_offered_deadline_missed;
    }
    if (dstl->on_offered_incompatible_qos == DDS_LUNSET) {
        dstl->on_offered_incompatible_qos = srcl->on_offered_incompatible_qos;
    }
    if (dstl->on_publication_matched == DDS_LUNSET) {
        dstl->on_publication_matched = srcl->on_publication_matched;
    }
    if (dstl->on_requested_deadline_missed == DDS_LUNSET) {
        dstl->on_requested_deadline_missed = srcl->on_requested_deadline_missed;
    }
    if (dstl->on_requested_incompatible_qos == DDS_LUNSET) {
        dstl->on_requested_incompatible_qos = srcl->on_requested_incompatible_qos;
    }
    if (dstl->on_sample_lost == DDS_LUNSET) {
        dstl->on_sample_lost = srcl->on_sample_lost;
    }
    if (dstl->on_sample_rejected == DDS_LUNSET) {
        dstl->on_sample_rejected = srcl->on_sample_rejected;
    }
    if (dstl->on_subscription_matched == DDS_LUNSET) {
        dstl->on_subscription_matched = srcl->on_subscription_matched;
    }
}

/************************************************************************************************
 *  Setters
 ************************************************************************************************/

void
dds_lset_data_available (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_data_available_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_data_available = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_data_on_readers (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_data_on_readers_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_data_on_readers = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_inconsistent_topic (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_inconsistent_topic_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_inconsistent_topic = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_liveliness_changed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_liveliness_changed_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_liveliness_changed = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_liveliness_lost (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_liveliness_lost_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_liveliness_lost = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_offered_deadline_missed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_offered_deadline_missed_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_offered_deadline_missed = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_offered_incompatible_qos (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_offered_incompatible_qos_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_offered_incompatible_qos = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_publication_matched (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_publication_matched_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_publication_matched = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_requested_deadline_missed (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_requested_deadline_missed_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_requested_deadline_missed = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_requested_incompatible_qos (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_requested_incompatible_qos_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_requested_incompatible_qos = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_sample_lost (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_sample_lost_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_sample_lost = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_sample_rejected (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_sample_rejected_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_sample_rejected = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

void
dds_lset_subscription_matched (_Inout_ dds_listener_t * __restrict listener, _In_opt_ dds_on_subscription_matched_fn callback)
{
    if (listener) {
        ((c_listener_t*)listener)->on_subscription_matched = callback;
    } else {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
    }
}

/************************************************************************************************
 *  Getters
 ************************************************************************************************/

void
dds_lget_data_available (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_data_available_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_data_available;
}

void
dds_lget_data_on_readers (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_data_on_readers_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_data_on_readers;
}

void dds_lget_inconsistent_topic (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_inconsistent_topic_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_inconsistent_topic;
}

void
dds_lget_liveliness_changed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_liveliness_changed_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_liveliness_changed;
}

void
dds_lget_liveliness_lost (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_liveliness_lost_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_liveliness_lost;
}

void
dds_lget_offered_deadline_missed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_offered_deadline_missed_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_offered_deadline_missed;
}

void
dds_lget_offered_incompatible_qos (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_offered_incompatible_qos_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_offered_incompatible_qos;
}

void
dds_lget_publication_matched (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_publication_matched_fn *callback)
{
    if(!callback){
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_publication_matched;
}

void
dds_lget_requested_deadline_missed (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_requested_deadline_missed_fn *callback)
{
    if(!callback) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_requested_deadline_missed;
}

void
dds_lget_requested_incompatible_qos (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_requested_incompatible_qos_fn *callback)
{
    if(!callback) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_requested_incompatible_qos;
}

void
dds_lget_sample_lost (_In_ const dds_listener_t *__restrict listener, _Outptr_result_maybenull_ dds_on_sample_lost_fn *callback)
{
    if(!callback) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_sample_lost;
}

void
dds_lget_sample_rejected (_In_ const dds_listener_t  *__restrict listener, _Outptr_result_maybenull_ dds_on_sample_rejected_fn *callback)
{
    if(!callback) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_sample_rejected;
}

void
dds_lget_subscription_matched (_In_ const dds_listener_t * __restrict listener, _Outptr_result_maybenull_ dds_on_subscription_matched_fn *callback)
{
    if(!callback) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument callback is NULL");
        return ;
    }
    if (!listener) {
        DDS_ERROR(DDS_RETCODE_BAD_PARAMETER, "Argument listener is NULL");
        return ;
    }
    *callback = ((c_listener_t*)listener)->on_subscription_matched;
}
