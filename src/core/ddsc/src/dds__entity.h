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
#include "cyclonedds/ddsi/q_thread.h"

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
  status_mask_t mask);

DDS_EXPORT void dds_entity_init_complete (dds_entity *entity);

DDS_EXPORT void
dds_entity_register_child (
  dds_entity *parent,
  dds_entity *child);

DDS_EXPORT void
dds_entity_add_ref_locked(dds_entity *e);

#define DEFINE_ENTITY_LOCK_UNLOCK(qualifier_, type_, kind_) \
  qualifier_ dds_return_t type_##_lock (dds_entity_t hdl, type_ **x) \
  { \
    dds_return_t rc; \
    dds_entity *e; \
    if ((rc = dds_entity_lock (hdl, kind_, &e)) < 0) \
      return rc; \
    *x = (type_ *) e; \
    return DDS_RETCODE_OK; \
  } \
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

DDS_EXPORT void dds_entity_status_set (dds_entity *e, status_mask_t t);
DDS_EXPORT void dds_entity_trigger_set (dds_entity *e, uint32_t t);

DDS_EXPORT inline void dds_entity_status_reset (dds_entity *e, status_mask_t t) {
  ddsrt_atomic_and32 (&e->m_status.m_status_and_mask, SAM_ENABLED_MASK | (status_mask_t) ~t);
}

DDS_EXPORT inline dds_entity_kind_t dds_entity_kind (const dds_entity *e) {
  return e->m_kind;
}

DDS_EXPORT void dds_entity_status_signal (dds_entity *e, uint32_t status);

DDS_EXPORT void dds_entity_invoke_listener (const dds_entity *entity, enum dds_status_id which, const void *vst);

DDS_EXPORT dds_participant *dds_entity_participant (dds_entity *e);
DDS_EXPORT void dds_entity_final_deinit_before_free (dds_entity *e);
DDS_EXPORT bool dds_entity_in_scope (const dds_entity *e, const dds_entity *root);

enum delete_impl_state {
  DIS_EXPLICIT,    /* explicit delete on this entity */
  DIS_FROM_PARENT, /* called because the parent is being deleted */
  DIS_IMPLICIT     /* called from child; delete if implicit w/o children */
};

DDS_EXPORT dds_return_t dds_delete_impl_pinned (dds_entity *e, enum delete_impl_state delstate);

DDS_EXPORT dds_return_t
dds_entity_pin (
  dds_entity_t hdl,
  dds_entity **eptr);

DDS_EXPORT void dds_entity_unpin (
  dds_entity *e);

DDS_EXPORT dds_return_t
dds_entity_lock(
  dds_entity_t hdl,
  dds_entity_kind_t kind,
  dds_entity **e);

DDS_EXPORT void
dds_entity_unlock(dds_entity *e);

DDS_EXPORT dds_return_t
dds_entity_observer_register(
  dds_entity *observed,
  dds_waitset *observer,
  dds_entity_callback_t cb,
  dds_entity_attach_callback_t attach_cb, void *attach_arg,
  dds_entity_delete_callback_t delete_cb);

DDS_EXPORT dds_return_t
dds_entity_observer_unregister(
  dds_entity *observed,
  dds_waitset *observer,
  bool invoke_delete_cb);

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
