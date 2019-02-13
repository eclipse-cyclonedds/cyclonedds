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

_Check_return_
DDS_EXPORT dds_entity_t
dds_entity_init(
        _In_       dds_entity * e,
        _When_(kind != DDS_KIND_PARTICIPANT, _Notnull_)
        _When_(kind == DDS_KIND_PARTICIPANT, _Null_)
          _In_opt_ dds_entity * parent,
        _In_       dds_entity_kind_t kind,
        _In_opt_   dds_qos_t * qos,
        _In_opt_   const dds_listener_t *listener,
        _In_       uint32_t mask);

DDS_EXPORT void
dds_entity_add_ref(
        _In_ dds_entity *e);
DDS_EXPORT void
dds_entity_add_ref_nolock(
        _In_ dds_entity *e);

#define DEFINE_ENTITY_LOCK_UNLOCK(qualifier_, type_, kind_) \
  qualifier_ dds__retcode_t type_##_lock (dds_entity_t hdl, type_ **x) \
  { \
    dds__retcode_t rc; \
    dds_entity *e; \
    if ((rc = dds_entity_lock (hdl, kind_, &e)) != DDS_RETCODE_OK) \
      return rc; \
    *x = (type_ *) e; \
    return DDS_RETCODE_OK; \
  } \
  \
  qualifier_ void type_##_unlock (type_ *x) \
  { \
    dds_entity_unlock (&x->m_entity); \
  }
#define DECL_ENTITY_LOCK_UNLOCK(qualifier_, type_) \
  qualifier_ dds__retcode_t type_##_lock (dds_entity_t hdl, type_ **x); \
  qualifier_ void type_##_unlock (type_ *x);

DDS_EXPORT inline bool dds_entity_is_enabled (const dds_entity *e) {
  return (e->m_flags & DDS_ENTITY_ENABLED) != 0;
}

DDS_EXPORT void dds_entity_status_set (dds_entity *e, uint32_t t);

DDS_EXPORT inline void dds_entity_status_reset (dds_entity *e, uint32_t t) {
  e->m_trigger &= ~t;
}

DDS_EXPORT inline bool dds_entity_status_match (const dds_entity *e, uint32_t t) {
  return (e->m_trigger & t) != 0;
}

DDS_EXPORT inline dds_entity_kind_t dds_entity_kind (const dds_entity *e) {
  return (dds_entity_kind_t) (e->m_hdl & DDS_ENTITY_KIND_MASK);
}

DDS_EXPORT inline dds_entity_kind_t dds_entity_kind_from_handle (dds_entity_t hdl) {
  return (hdl > 0) ? (dds_entity_kind_t) (hdl & DDS_ENTITY_KIND_MASK) : DDS_KIND_DONTCARE;
}

DDS_EXPORT void dds_entity_status_signal (dds_entity *e);

DDS_EXPORT void dds_entity_invoke_listener (const dds_entity *entity, enum dds_status_id which, const void *vst);

_Check_return_ DDS_EXPORT dds__retcode_t
dds_valid_hdl(
        _In_ dds_entity_t hdl,
        _In_ dds_entity_kind_t kind);

_Acquires_exclusive_lock_(*e)
_Check_return_ DDS_EXPORT dds__retcode_t
dds_entity_lock(
        _In_ dds_entity_t hdl,
        _In_ dds_entity_kind_t kind,
        _Out_ dds_entity **e);

_Releases_exclusive_lock_(e)
DDS_EXPORT void
dds_entity_unlock(
        _Inout_ dds_entity *e);

_Check_return_ DDS_EXPORT dds__retcode_t
dds_entity_observer_register_nl(
        _In_ dds_entity*  observed,
        _In_ dds_entity_t observer,
        _In_ dds_entity_callback cb);

_Check_return_ DDS_EXPORT dds__retcode_t
dds_entity_observer_register(
        _In_ dds_entity_t observed,
        _In_ dds_entity_t observer,
        _In_ dds_entity_callback cb);

DDS_EXPORT dds__retcode_t
dds_entity_observer_unregister_nl(
        _In_ dds_entity*  observed,
        _In_ dds_entity_t observer);

DDS_EXPORT dds__retcode_t
dds_entity_observer_unregister(
        _In_ dds_entity_t observed,
        _In_ dds_entity_t observer);

_Pre_satisfies_(entity & DDS_ENTITY_KIND_MASK)
DDS_EXPORT dds_return_t
dds_delete_impl(
        _In_ dds_entity_t entity,
        _In_ bool keep_if_explicit);

DDS_EXPORT const char *
dds__entity_kind_str(
        _In_ dds_entity_t e);

DDS_EXPORT dds_domain *
dds__entity_domain(
        _In_ dds_entity* e);

#if defined (__cplusplus)
}
#endif
#endif
