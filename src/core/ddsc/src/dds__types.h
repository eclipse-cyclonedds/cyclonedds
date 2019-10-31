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
#ifndef _DDS_TYPES_H_
#define _DDS_TYPES_H_

/* DDS internal type definitions */

#include "dds/dds.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds__handles.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_domain;
struct dds_entity;
struct dds_participant;
struct dds_reader;
struct dds_writer;
struct dds_publisher;
struct dds_subscriber;
struct dds_topic;
struct dds_readcond;
struct dds_guardcond;
struct dds_statuscond;

struct ddsi_sertopic;
struct ddsi_rhc;

typedef uint16_t status_mask_t;
typedef ddsrt_atomic_uint32_t status_and_enabled_t;
#define SAM_STATUS_MASK        0xffffu
#define SAM_ENABLED_MASK   0xffff0000u
#define SAM_ENABLED_SHIFT          16

/* This can be used when polling for various states.
 * Obviously, it is encouraged to use condition variables and such. But
 * sometimes it wouldn't make that much of a difference and taking the
 * easy route is somewhat pragmatic. */
#define DDS_HEADBANG_TIMEOUT (DDS_MSECS (10))

typedef bool (*dds_querycondition_filter_with_ctx_fn) (const void * sample, const void *ctx);


/* The listener struct. */

struct dds_listener {
  uint32_t inherited;
  dds_on_inconsistent_topic_fn on_inconsistent_topic;
  void *on_inconsistent_topic_arg;
  dds_on_liveliness_lost_fn on_liveliness_lost;
  void *on_liveliness_lost_arg;
  dds_on_offered_deadline_missed_fn on_offered_deadline_missed;
  void *on_offered_deadline_missed_arg;
  dds_on_offered_incompatible_qos_fn on_offered_incompatible_qos;
  void *on_offered_incompatible_qos_arg;
  dds_on_data_on_readers_fn on_data_on_readers;
  void *on_data_on_readers_arg;
  dds_on_sample_lost_fn on_sample_lost;
  void *on_sample_lost_arg;
  dds_on_data_available_fn on_data_available;
  void *on_data_available_arg;
  dds_on_sample_rejected_fn on_sample_rejected;
  void *on_sample_rejected_arg;
  dds_on_liveliness_changed_fn on_liveliness_changed;
  void *on_liveliness_changed_arg;
  dds_on_requested_deadline_missed_fn on_requested_deadline_missed;
  void *on_requested_deadline_missed_arg;
  dds_on_requested_incompatible_qos_fn on_requested_incompatible_qos;
  void *on_requested_incompatible_qos_arg;
  dds_on_publication_matched_fn on_publication_matched;
  void *on_publication_matched_arg;
  dds_on_subscription_matched_fn on_subscription_matched;
  void *on_subscription_matched_arg;
};

/* Entity flag values */

#define DDS_ENTITY_ENABLED      ((uint32_t) 0x1) /* DDS "enabled" state */
#define DDS_ENTITY_IMPLICIT     ((uint32_t) 0x2) /* implicit ones get deleted when the last child is deleted */

struct dds_domain;
struct dds_entity;

typedef struct dds_entity_deriver {
  /* Pending close can be used to terminate (blocking) actions on a entity before actually deleting it. */
  void (*interrupt) (struct dds_entity *e) ddsrt_nonnull_all;
  /* Close can be used to do ... */
  void (*close) (struct dds_entity *e) ddsrt_nonnull_all;
  /* Delete is used to actually free the entity. */
  dds_return_t (*delete) (struct dds_entity *e) ddsrt_nonnull_all;
  dds_return_t (*set_qos) (struct dds_entity *e, const dds_qos_t *qos, bool enabled) ddsrt_nonnull_all;
  dds_return_t (*validate_status) (uint32_t mask);
} dds_entity_deriver;

struct dds_waitset;
typedef void (*dds_entity_callback_t) (struct dds_waitset *observer, dds_entity_t observed, uint32_t status);
typedef bool (*dds_entity_attach_callback_t) (struct dds_waitset *observer, struct dds_entity *observed, void *attach_arg);
typedef void (*dds_entity_delete_callback_t) (struct dds_waitset *observer, dds_entity_t observed);

typedef struct dds_entity_observer {
  dds_entity_callback_t m_cb;
  dds_entity_delete_callback_t m_delete_cb;
  struct dds_waitset *m_observer;
  struct dds_entity_observer *m_next;
} dds_entity_observer;

typedef struct dds_entity {
  struct dds_handle_link m_hdllink; /* handle is constant, cnt_flags private to dds_handle.c */
  dds_entity_kind_t m_kind;         /* constant */
  struct dds_entity *m_next;        /* [m_mutex] */
  struct dds_entity *m_parent;      /* constant */
  ddsrt_avl_node_t m_avlnode_child; /* [m_mutex of m_parent] */
  ddsrt_avl_tree_t m_children;      /* [m_mutex] tree on m_iid using m_avlnode_child */
  struct dds_domain *m_domain;      /* constant */
  dds_qos_t *m_qos;                 /* [m_mutex] */
  ddsi_guid_t m_guid;               /* unique (if not 0) and constant; FIXME: set during creation, but possibly after becoming visible */
  dds_instance_handle_t m_iid;      /* unique for all time, constant; FIXME: like GUID */
  uint32_t m_flags;                 /* [m_mutex] */

  /* Allowed:
     - locking parent->...->m_mutex while holding m_mutex
     - locking topic::m_mutex while holding {publisher,subscriber}::m_mutex
       (no hierarchical relationship there)
     - locking topic::m_mutex while holding {reader,writer}::m_mutex
     - locking observers_lock while holding m_mutex
     - locking waitset::wait_lock
   */
  ddsrt_mutex_t m_mutex;
  ddsrt_cond_t m_cond;

  union {
    status_and_enabled_t m_status_and_mask; /* for most entities */
    ddsrt_atomic_uint32_t m_trigger;        /* for conditions & waitsets */
  } m_status;

  ddsrt_mutex_t m_observers_lock;   /* locking parent->...->m_observers_lock while holding it is allowed */
  ddsrt_cond_t m_observers_cond;
  dds_listener_t m_listener;        /* [m_observers_lock] */
  uint32_t m_cb_count;              /* [m_observers_lock] */
  uint32_t m_cb_pending_count;      /* [m_observers_lock] */
  dds_entity_observer *m_observers; /* [m_observers_lock] */
} dds_entity;

extern const ddsrt_avl_treedef_t dds_topictree_def;
extern const ddsrt_avl_treedef_t dds_entity_children_td;

extern const struct dds_entity_deriver dds_entity_deriver_topic;
extern const struct dds_entity_deriver dds_entity_deriver_participant;
extern const struct dds_entity_deriver dds_entity_deriver_reader;
extern const struct dds_entity_deriver dds_entity_deriver_writer;
extern const struct dds_entity_deriver dds_entity_deriver_subscriber;
extern const struct dds_entity_deriver dds_entity_deriver_publisher;
extern const struct dds_entity_deriver dds_entity_deriver_readcondition;
extern const struct dds_entity_deriver dds_entity_deriver_guardcondition;
extern const struct dds_entity_deriver dds_entity_deriver_waitset;
extern const struct dds_entity_deriver dds_entity_deriver_domain;
extern const struct dds_entity_deriver dds_entity_deriver_cyclonedds;
extern const struct dds_entity_deriver *dds_entity_deriver_table[];

void dds_entity_deriver_dummy_interrupt (struct dds_entity *e);
void dds_entity_deriver_dummy_close (struct dds_entity *e);
dds_return_t dds_entity_deriver_dummy_delete (struct dds_entity *e);
dds_return_t dds_entity_deriver_dummy_set_qos (struct dds_entity *e, const dds_qos_t *qos, bool enabled);
dds_return_t dds_entity_deriver_dummy_validate_status (uint32_t mask);

inline void dds_entity_deriver_interrupt (struct dds_entity *e) {
  (dds_entity_deriver_table[e->m_kind]->interrupt) (e);
}
inline void dds_entity_deriver_close (struct dds_entity *e) {
  (dds_entity_deriver_table[e->m_kind]->close) (e);
}
inline dds_return_t dds_entity_deriver_delete (struct dds_entity *e) {
  return dds_entity_deriver_table[e->m_kind]->delete (e);
}
inline dds_return_t dds_entity_deriver_set_qos (struct dds_entity *e, const dds_qos_t *qos, bool enabled) {
  return dds_entity_deriver_table[e->m_kind]->set_qos (e, qos, enabled);
}
inline dds_return_t dds_entity_deriver_validate_status (struct dds_entity *e, uint32_t mask) {
  return dds_entity_deriver_table[e->m_kind]->validate_status (mask);
}
inline bool dds_entity_supports_set_qos (struct dds_entity *e) {
  return dds_entity_deriver_table[e->m_kind]->set_qos != dds_entity_deriver_dummy_set_qos;
}
inline bool dds_entity_supports_validate_status (struct dds_entity *e) {
  return dds_entity_deriver_table[e->m_kind]->validate_status != dds_entity_deriver_dummy_validate_status;
}

typedef struct dds_cyclonedds_entity {
  struct dds_entity m_entity;

  ddsrt_mutex_t m_mutex;
  ddsrt_cond_t m_cond;
  ddsrt_avl_tree_t m_domains;
  uint32_t threadmon_count;
  struct ddsi_threadmon *threadmon;
} dds_cyclonedds_entity;

typedef struct dds_domain {
  struct dds_entity m_entity;

  ddsrt_avl_node_t m_node; /* for dds_global.m_domains */
  dds_domainid_t m_id;
  ddsrt_avl_tree_t m_topics;
  struct cfgst *cfgst;

  struct ddsi_sertopic *builtin_participant_topic;
  struct ddsi_sertopic *builtin_reader_topic;
  struct ddsi_sertopic *builtin_writer_topic;

  struct local_orphan_writer *builtintopic_writer_participant;
  struct local_orphan_writer *builtintopic_writer_publications;
  struct local_orphan_writer *builtintopic_writer_subscriptions;

  struct ddsi_builtin_topic_interface btif;
  struct q_globals gv;
} dds_domain;

typedef struct dds_subscriber {
  struct dds_entity m_entity;
} dds_subscriber;

typedef struct dds_publisher {
  struct dds_entity m_entity;
} dds_publisher;

typedef struct dds_participant {
  struct dds_entity m_entity;
  dds_entity_t m_builtin_subscriber;
} dds_participant;

typedef struct dds_reader {
  struct dds_entity m_entity;
  struct dds_topic *m_topic;
  struct dds_rhc *m_rhc; /* aliases m_rd->rhc with a wider interface, FIXME: but m_rd owns it for resource management */
  struct reader *m_rd;
  bool m_data_on_readers;
  bool m_loan_out;
  void *m_loan;
  uint32_t m_loan_size;

  /* Status metrics */

  dds_sample_rejected_status_t m_sample_rejected_status;
  dds_liveliness_changed_status_t m_liveliness_changed_status;
  dds_requested_deadline_missed_status_t m_requested_deadline_missed_status;
  dds_requested_incompatible_qos_status_t m_requested_incompatible_qos_status;
  dds_sample_lost_status_t m_sample_lost_status;
  dds_subscription_matched_status_t m_subscription_matched_status;
} dds_reader;

typedef struct dds_writer {
  struct dds_entity m_entity;
  struct dds_topic *m_topic;
  struct nn_xpack *m_xp;
  struct writer *m_wr;
  struct whc *m_whc; /* FIXME: ownership still with underlying DDSI writer (cos of DDSI built-in writers )*/
  bool whc_batch; /* FIXME: channels + latency budget */

  /* Status metrics */

  dds_liveliness_lost_status_t m_liveliness_lost_status;
  dds_offered_deadline_missed_status_t m_offered_deadline_missed_status;
  dds_offered_incompatible_qos_status_t m_offered_incompatible_qos_status;
  dds_publication_matched_status_t m_publication_matched_status;
} dds_writer;

#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

typedef struct dds_topic {
  struct dds_entity m_entity;
  struct ddsi_sertopic *m_stopic;

  dds_topic_intern_filter_fn filter_fn;
  void *filter_ctx;

  /* Status metrics */

  dds_inconsistent_topic_status_t m_inconsistent_topic_status;
} dds_topic;

typedef uint32_t dds_querycond_mask_t;

typedef struct dds_readcond {
  dds_entity m_entity;
  uint32_t m_qminv;
  uint32_t m_sample_states;
  uint32_t m_view_states;
  uint32_t m_instance_states;
  struct dds_readcond *m_next;
  struct {
    dds_querycondition_filter_fn m_filter;
    dds_querycond_mask_t m_qcmask; /* condition mask in RHC*/
  } m_query;
} dds_readcond;

typedef struct dds_guardcond {
  dds_entity m_entity;
} dds_guardcond;

typedef struct dds_attachment {
  dds_entity *entity;
  dds_entity_t handle;
  dds_attach_t arg;
} dds_attachment;

typedef struct dds_waitset {
  dds_entity m_entity;

  /* Need a lock other than m_entity.m_mutex because the locking order an entity lock may not be
     acquired while holding an ancestor's lock, but a waitset must be capable of triggering on
     events on its parent */
  ddsrt_mutex_t wait_lock;
  ddsrt_cond_t wait_cond;
  size_t nentities;         /* [wait_lock] */
  size_t ntriggered;        /* [wait_lock] */
  dds_attachment *entities; /* [wait_lock] 0 .. ntriggered are triggred, ntriggred .. nentities are not */
} dds_waitset;

DDS_EXPORT extern dds_cyclonedds_entity dds_global;

#if defined (__cplusplus)
}
#endif
#endif
