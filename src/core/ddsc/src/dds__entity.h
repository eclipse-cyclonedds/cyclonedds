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
#ifndef _DDS_ENTITY_H_
#define _DDS_ENTITY_H_

#include "dds__types.h"
#include "ddsi/q_thread.h"

#if defined (__cplusplus)
extern "C" {
#endif

_Check_return_ dds_entity_t
dds_entity_init(
        _In_       dds_entity * e,
        _When_(kind != DDS_KIND_PARTICIPANT, _Notnull_)
        _When_(kind == DDS_KIND_PARTICIPANT, _Null_)
          _In_opt_ dds_entity * parent,
        _In_       dds_entity_kind_t kind,
        _In_opt_   dds_qos_t * qos,
        _In_opt_   const dds_listener_t *listener,
        _In_       uint32_t mask);

void
dds_entity_add_ref(
        _In_ dds_entity *e);
void
dds_entity_add_ref_nolock(
        _In_ dds_entity *e);

_Check_return_ dds__retcode_t
dds_entity_listener_propagation(
        _Inout_opt_ dds_entity *e,
        _In_ dds_entity *src,
        _In_ uint32_t status,
        _In_opt_ void *metrics,
        _In_ bool propagate);

#define dds_entity_is_enabled(e, k)   (((dds_entity*)e)->m_flags & DDS_ENTITY_ENABLED)

#define dds_entity_status_set(e, t)   (((dds_entity*)e)->m_trigger |= (((dds_entity*)e)->m_status_enable & t))
#define dds_entity_status_reset(e,t)  (((dds_entity*)e)->m_trigger &= ~t)
#define dds_entity_status_match(e,t)  (((dds_entity*)e)->m_trigger &   t)

/* The mutex needs to be unlocked when calling this because the entity can be called
 * within the signal callback from other contexts. That shouldn't deadlock. */
void
dds_entity_status_signal(
        _In_ dds_entity *e);

_Check_return_ dds__retcode_t
dds_valid_hdl(
        _In_ dds_entity_t hdl,
        _In_ dds_entity_kind_t kind);

_Acquires_exclusive_lock_(*e)
_Check_return_ dds__retcode_t
dds_entity_lock(
        _In_ dds_entity_t hdl,
        _In_ dds_entity_kind_t kind,
        _Out_ dds_entity **e);

_Releases_exclusive_lock_(e)
void
dds_entity_unlock(
        _Inout_ dds_entity *e);

#define dds_entity_kind(hdl) ((hdl > 0) ? (hdl & DDS_ENTITY_KIND_MASK) : 0)

_Check_return_ dds__retcode_t
dds_entity_observer_register_nl(
        _In_ dds_entity*  observed,
        _In_ dds_entity_t observer,
        _In_ dds_entity_callback cb);

_Check_return_ dds__retcode_t
dds_entity_observer_register(
        _In_ dds_entity_t observed,
        _In_ dds_entity_t observer,
        _In_ dds_entity_callback cb);

dds__retcode_t
dds_entity_observer_unregister_nl(
        _In_ dds_entity*  observed,
        _In_ dds_entity_t observer);

dds__retcode_t
dds_entity_observer_unregister(
        _In_ dds_entity_t observed,
        _In_ dds_entity_t observer);

_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
dds_return_t
dds_delete_impl(
        _In_ dds_entity_t entity,
        _In_ bool keep_if_explicit);

const char *
dds__entity_kind_str(
        _In_ dds_entity_t e);

#if defined (__cplusplus)
}
#endif
#endif
