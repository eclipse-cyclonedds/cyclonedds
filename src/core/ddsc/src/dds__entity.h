// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__ENTITY_H
#define DDS__ENTITY_H

#include "dds/ddsrt/countargs.h"
#include "dds__types.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component generic_entity */
dds_entity_t dds_entity_init(
  dds_entity * e,
  dds_entity * parent,
  dds_entity_kind_t kind,
  bool implicit,
  bool user_access,
  dds_qos_t * qos,
  const dds_listener_t *listener,
  status_mask_t mask);

/** @component generic_entity */
void dds_entity_init_complete (dds_entity *entity);

/** @component generic_entity */
void dds_entity_register_child (dds_entity *parent, dds_entity *child);

/** @component generic_entity */
void dds_entity_add_ref_locked(dds_entity *e);

/** @component generic_entity */
void dds_entity_drop_ref(dds_entity *e);

/** @component generic_entity */
void dds_entity_unpin_and_drop_ref (dds_entity *e);

#define DEFINE_ENTITY_LOCK_UNLOCK(type_, kind_, component_) \
  /** @component component_ */ \
  inline dds_return_t type_##_lock (dds_entity_t hdl, type_ **x) \
  { \
    dds_return_t rc; \
    dds_entity *e; \
    if ((rc = dds_entity_lock (hdl, kind_, &e)) < 0) \
      return rc; \
    *x = (type_ *) e; \
    return DDS_RETCODE_OK; \
  } \
  /** @component component_ */ \
  inline void type_##_unlock (type_ *x) \
  { \
    dds_entity_unlock (&x->m_entity); \
  }
#define DECL_ENTITY_LOCK_UNLOCK(type_) \
  extern inline dds_return_t type_##_lock (dds_entity_t hdl, type_ **x); \
  extern inline void type_##_unlock (type_ *x);

/** @component generic_entity */
inline dds_entity *dds_entity_from_handle_link (struct dds_handle_link *hdllink) {
  return (dds_entity *) ((char *) hdllink - offsetof (struct dds_entity, m_hdllink));
}

/** @component generic_entity */
inline bool dds_entity_is_enabled (const dds_entity *e) {
  return (e->m_flags & DDS_ENTITY_ENABLED) != 0;
}

/** @component generic_entity */
bool dds_entity_status_set (dds_entity *e, status_mask_t t) ddsrt_attribute_warn_unused_result;

/** @component generic_entity */
inline void dds_entity_status_reset (dds_entity *e, status_mask_t t) {
  ddsrt_atomic_and32 (&e->m_status.m_status_and_mask, SAM_ENABLED_MASK | (status_mask_t) ~t);
}

/** @component generic_entity */
inline uint32_t dds_entity_status_reset_ov (dds_entity *e, status_mask_t t) {
  return ddsrt_atomic_and32_ov (&e->m_status.m_status_and_mask, SAM_ENABLED_MASK | (status_mask_t) ~t);
}

/** @component generic_entity */
inline dds_entity_kind_t dds_entity_kind (const dds_entity *e) {
  return e->m_kind;
}

/** @component generic_entity */
void dds_entity_observers_signal (dds_entity *observed, uint32_t status);

/** @component generic_entity */
void dds_entity_status_signal (dds_entity *e, uint32_t status);

union dds_status_union {
  dds_inconsistent_topic_status_t inconsistent_topic;
  dds_liveliness_changed_status_t liveliness_changed;
  dds_liveliness_lost_status_t liveliness_lost;
  dds_offered_deadline_missed_status_t offered_deadline_missed;
  dds_offered_incompatible_qos_status_t offered_incompatible_qos;
  dds_publication_matched_status_t publication_matched;
  dds_requested_deadline_missed_status_t requested_deadline_missed;
  dds_requested_incompatible_qos_status_t requested_incompatible_qos;
  dds_sample_lost_status_t sample_lost;
  dds_sample_rejected_status_t sample_rejected;
  dds_subscription_matched_status_t subscription_matched;
};

#define DDS_RESET_STATUS_FIELDS_1(ent_, status_, reset0_) \
  ((ent_)->m_##status_##_status.reset0_ = 0);
#define DDS_RESET_STATUS_FIELDS_2(ent_, status_, reset0_, reset1_) \
  ((ent_)->m_##status_##_status.reset0_ = 0); \
  ((ent_)->m_##status_##_status.reset1_ = 0);
#define DDS_RESET_STATUS_FIELDS_MSVC_WORKAROUND(x) x
#define DDS_RESET_STATUS_FIELDS_N1(n_, ent_, status_, ...) \
  DDS_RESET_STATUS_FIELDS_MSVC_WORKAROUND (DDS_RESET_STATUS_FIELDS_##n_ (ent_, status_, __VA_ARGS__))
#define DDS_RESET_STATUS_FIELDS_N(n_, ent_, status_, ...) DDS_RESET_STATUS_FIELDS_N1 (n_, ent_, status_, __VA_ARGS__)

// Don't set the status in the mask, not even for a very short while if in spec-compliant
// mode of resetting the status prior to invoking the listener.  The only reason is so
// that the initial evaluation of the conditions in a waitset has no window to observe the
// status.  Nowhere does it say that this is required, but it makes writing a test for the
// listener/waitset-trigger ordering even more problematic.
#define STATUS_CB_IMPL_INVOKE(entity_kind_, name_, NAME_, ...)          \
  static bool status_cb_##name_##_invoke (dds_##entity_kind_ * const e) \
  {                                                                     \
    struct dds_listener const * const listener = &e->m_entity.m_listener; \
    union dds_status_union lst;                                         \
    bool signal;                                                        \
    lst.name_ = e->m_##name_##_status;                                  \
    if (listener->reset_on_invoke & DDS_##NAME_##_STATUS) {             \
      signal = false;                                                   \
      DDS_RESET_STATUS_FIELDS_N (DDSRT_COUNT_ARGS (__VA_ARGS__), e, name_, __VA_ARGS__) \
      dds_entity_status_reset (&e->m_entity, DDS_##NAME_##_STATUS);     \
    } else {                                                            \
      signal = dds_entity_status_set (&e->m_entity, DDS_##NAME_##_STATUS); \
    }                                                                   \
    ddsrt_mutex_unlock (&e->m_entity.m_observers_lock);                 \
    listener->on_##name_ (e->m_entity.m_hdllink.hdl, lst.name_, listener->on_##name_##_arg); \
    ddsrt_mutex_lock (&e->m_entity.m_observers_lock);                   \
    if (!signal)                                                        \
      return false;                                                     \
    else {                                                              \
      const uint32_t sm = ddsrt_atomic_ld32 (&e->m_entity.m_status.m_status_and_mask); \
      return ((sm & (sm >> SAM_ENABLED_SHIFT)) & DDS_##NAME_##_STATUS) != 0; \
    }                                                                   \
  }

#define STATUS_CB_IMPL(entity_kind_, name_, NAME_, ...)                 \
  STATUS_CB_IMPL_INVOKE(entity_kind_, name_, NAME_, __VA_ARGS__)        \
  static void status_cb_##name_ (dds_##entity_kind_ * const e, const ddsi_status_cb_data_t *data) \
  {                                                                     \
    struct dds_listener const * const listener = &e->m_entity.m_listener; \
    update_##name_ (&e->m_##name_##_status, data);                      \
    bool signal;                                                        \
    if (listener->on_##name_ == 0)                                      \
      signal = dds_entity_status_set (&e->m_entity, DDS_##NAME_##_STATUS); \
    else                                                                \
      signal = status_cb_##name_##_invoke (e);                          \
    if (signal)                                                         \
      dds_entity_observers_signal (&e->m_entity, DDS_##NAME_##_STATUS); \
  }

/** @component generic_entity */
dds_participant *dds_entity_participant (const dds_entity *e);

/** @component generic_entity */
const ddsi_guid_t *dds_entity_participant_guid (const dds_entity *e);

/** @component generic_entity */
void dds_entity_final_deinit_before_free (dds_entity *e);

/** @component generic_entity */
bool dds_entity_in_scope (const dds_entity *e, const dds_entity *root);

enum delete_impl_state {
  DIS_USER,        /* delete invoked directly by application */
  DIS_EXPLICIT,    /* explicit delete on this entity */
  DIS_FROM_PARENT, /* called because the parent is being deleted */
  DIS_IMPLICIT     /* called from child; delete if implicit w/o children */
};

/** @component generic_entity */
dds_return_t dds_delete_impl_pinned (dds_entity *e, enum delete_impl_state delstate);

/** @component generic_entity */
dds_return_t dds_entity_pin (dds_entity_t hdl, dds_entity **eptr);

/** @component generic_entity */
dds_return_t dds_entity_pin_with_origin (dds_entity_t hdl, bool from_user, dds_entity **eptr);

/** @component generic_entity */
dds_return_t dds_entity_pin_for_delete (dds_entity_t hdl, bool explicit, bool from_user, dds_entity **eptr);

/** @component generic_entity */
void dds_entity_unpin (dds_entity *e);

/** @component generic_entity */
dds_return_t dds_entity_lock (dds_entity_t hdl, dds_entity_kind_t kind, dds_entity **e);

/** @component generic_entity */
void dds_entity_unlock (dds_entity *e);

/** @component generic_entity */
dds_return_t dds_entity_observer_register (
  dds_entity *observed,
  dds_waitset *observer,
  dds_entity_callback_t cb,
  dds_entity_attach_callback_t attach_cb, void *attach_arg,
  dds_entity_delete_callback_t delete_cb);

/** @component generic_entity */
dds_return_t dds_entity_observer_unregister(dds_entity *observed, dds_waitset *observer, bool invoke_delete_cb);

/** @component generic_entity */
dds_return_t dds_generic_unimplemented_operation_manykinds(dds_entity_t handle, size_t nkinds, const dds_entity_kind_t *kinds);

/** @component generic_entity */
dds_return_t dds_generic_unimplemented_operation(dds_entity_t handle, dds_entity_kind_t kind);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__ENTITY_H */
