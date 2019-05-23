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
#include "dds/ddsi/q_thread.h"

#if defined (__cplusplus)
extern "C" {
#endif

DDS_EXPORT dds_entity_t
dds_entity_init(
  dds_entity * e,
  dds_entity * parent,
  dds_entity_kind_t kind,
  dds_qos_t * qos,
  const dds_listener_t *listener,
  uint32_t mask);

DDS_EXPORT void
dds_entity_add_ref(dds_entity *e);

DDS_EXPORT void
dds_entity_add_ref_nolock(dds_entity *e);

#define DEFINE_ENTITY_LOCK_UNLOCK(qualifier_, type_, kind_) \
  qualifier_ dds_return_t type_##_lock (dds_entity_t hdl, type_ **x) \
  { \
    dds_return_t rc; \
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
  qualifier_ dds_return_t type_##_lock (dds_entity_t hdl, type_ **x); \
  qualifier_ void type_##_unlock (type_ *x);

DDS_EXPORT inline dds_entity *dds_entity_from_handle_link (struct dds_handle_link *hdllink) {
  return (dds_entity *) ((char *) hdllink - offsetof (struct dds_entity, m_hdllink));
}

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
  return e->m_kind;
}

DDS_EXPORT void dds_entity_status_signal (dds_entity *e);

DDS_EXPORT void dds_entity_invoke_listener (const dds_entity *entity, enum dds_status_id which, const void *vst);

DDS_EXPORT dds_return_t
dds_entity_claim (
  dds_entity_t hdl,
  dds_entity **eptr);

DDS_EXPORT void dds_entity_release (
  dds_entity *e);

DDS_EXPORT dds_return_t
dds_entity_lock(
  dds_entity_t hdl,
  dds_entity_kind_t kind,
  dds_entity **e);

DDS_EXPORT void
dds_entity_unlock(dds_entity *e);

DDS_EXPORT dds_return_t
dds_entity_observer_register_nl(
  dds_entity *observed,
  dds_entity_t observer,
  dds_entity_callback cb);

DDS_EXPORT dds_return_t
dds_entity_observer_register(
  dds_entity_t observed,
  dds_entity_t observer,
  dds_entity_callback cb);

DDS_EXPORT dds_return_t
dds_entity_observer_unregister_nl(
  dds_entity *observed,
  dds_entity_t observer);

DDS_EXPORT dds_return_t
dds_entity_observer_unregister(
  dds_entity_t observed,
  dds_entity_t observer);

DDS_EXPORT dds_return_t
dds_delete_impl(
  dds_entity_t entity,
  bool keep_if_explicit);

DDS_EXPORT dds_domain *
dds__entity_domain(
  dds_entity* e);

DDS_EXPORT dds_return_t
dds_generic_unimplemented_operation_manykinds(
        dds_entity_t handle,
        size_t nkinds,
        const dds_entity_kind_t *kinds);

DDS_EXPORT dds_return_t
dds_generic_unimplemented_operation(
        dds_entity_t handle,
        dds_entity_kind_t kind);


#if defined (__cplusplus)
}
#endif
#endif
