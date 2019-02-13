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

#include "os/os.h"
#include "ddsc/dds.h"
#include "ddsi/q_rtps.h"
#include "util/ut_avl.h"
#include "util/ut_handleserver.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef _Return_type_success_(return == DDS_RETCODE_OK) int32_t dds__retcode_t;

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
struct rhc;

/* Internal entity status flags */

#define DDS_INTERNAL_STATUS_MASK     (0xFF000000u)

#define DDS_WAITSET_TRIGGER_STATUS   (0x01000000u)
#define DDS_DELETING_STATUS          (0x02000000u)

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

#define DDS_ENTITY_ENABLED      0x0001u
#define DDS_ENTITY_IMPLICIT     0x0002u

typedef struct dds_domain
{
  ut_avlNode_t m_node;
  dds_domainid_t m_id;
  ut_avlTree_t m_topics;
  uint32_t m_refc;
}
dds_domain;

struct dds_entity;
typedef struct dds_entity_deriver {
    /* Close can be used to terminate (blocking) actions on a entity before actually deleting it. */
    dds_return_t (*close)(struct dds_entity *e);
    /* Delete is used to actually free the entity. */
    dds_return_t (*delete)(struct dds_entity *e);
    dds_return_t (*set_qos)(struct dds_entity *e, const dds_qos_t *qos, bool enabled);
    dds_return_t (*validate_status)(uint32_t mask);
    dds_return_t (*get_instance_hdl)(struct dds_entity *e, dds_instance_handle_t *i);
}
dds_entity_deriver;

typedef void (*dds_entity_callback)(dds_entity_t observer, dds_entity_t observed, uint32_t status);

typedef struct dds_entity_observer
{
    dds_entity_callback m_cb;
    dds_entity_t m_observer;
    struct dds_entity_observer *m_next;
}
dds_entity_observer;

typedef struct dds_entity
{
  ut_handle_t m_hdl;
  dds_entity_deriver m_deriver;
  uint32_t m_refc;
  struct dds_entity * m_next;
  struct dds_entity * m_parent;
  struct dds_entity * m_children;
  struct dds_entity * m_participant;
  struct dds_domain * m_domain;
  dds_qos_t * m_qos;
  dds_domainid_t m_domainid;
  nn_guid_t m_guid;
  uint32_t m_flags;
  os_mutex m_mutex;
  os_cond m_cond;

  os_mutex m_observers_lock;
  os_cond m_observers_cond;
  dds_listener_t m_listener;
  uint32_t m_trigger;
  uint32_t m_status_enable;
  uint32_t m_cb_count;
  dds_entity_observer *m_observers;

  struct ut_handlelink *m_hdllink;
}
dds_entity;

extern const ut_avlTreedef_t dds_topictree_def;

typedef struct dds_subscriber
{
  struct dds_entity m_entity;
}
dds_subscriber;

typedef struct dds_publisher
{
  struct dds_entity m_entity;
}
dds_publisher;

typedef struct dds_participant
{
  struct dds_entity m_entity;
  struct dds_entity * m_dur_reader;
  struct dds_entity * m_dur_writer;
  dds_entity_t m_builtin_subscriber;
}
dds_participant;

typedef struct dds_reader
{
  struct dds_entity m_entity;
  const struct dds_topic * m_topic;
  struct reader * m_rd;
  bool m_data_on_readers;
  bool m_loan_out;
  void * m_loan;
  uint32_t m_loan_size;

  /* Status metrics */

  dds_sample_rejected_status_t m_sample_rejected_status;
  dds_liveliness_changed_status_t m_liveliness_changed_status;
  dds_requested_deadline_missed_status_t m_requested_deadline_missed_status;
  dds_requested_incompatible_qos_status_t m_requested_incompatible_qos_status;
  dds_sample_lost_status_t m_sample_lost_status;
  dds_subscription_matched_status_t m_subscription_matched_status;
}
dds_reader;

typedef struct dds_writer
{
  struct dds_entity m_entity;
  const struct dds_topic * m_topic;
  struct nn_xpack * m_xp;
  struct writer * m_wr;
  struct whc *m_whc; /* FIXME: ownership still with underlying DDSI writer (cos of DDSI built-in writers )*/

  /* Status metrics */

  dds_liveliness_lost_status_t m_liveliness_lost_status;
  dds_offered_deadline_missed_status_t m_offered_deadline_missed_status;
  dds_offered_incompatible_qos_status_t m_offered_incompatible_qos_status;
  dds_publication_matched_status_t m_publication_matched_status;
}
dds_writer;

#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

typedef struct dds_topic
{
  struct dds_entity m_entity;
  struct ddsi_sertopic * m_stopic;

  dds_topic_intern_filter_fn filter_fn;
  void * filter_ctx;

  /* Status metrics */

  dds_inconsistent_topic_status_t m_inconsistent_topic_status;
}
dds_topic;

typedef uint32_t dds_querycond_mask_t;

typedef struct dds_readcond
{
  dds_entity m_entity;
  struct rhc * m_rhc;
  uint32_t m_qminv;
  uint32_t m_sample_states;
  uint32_t m_view_states;
  uint32_t m_instance_states;
  nn_guid_t m_rd_guid;
  struct dds_readcond * m_next;
  struct
  {
      dds_querycondition_filter_fn m_filter;
      dds_querycond_mask_t m_qcmask; /* condition mask in RHC*/
  } m_query;
}
dds_readcond;

typedef struct dds_guardcond
{
  dds_entity m_entity;
}
dds_guardcond;

typedef struct dds_attachment
{
    dds_entity  *entity;
    dds_attach_t arg;
    struct dds_attachment* next;
}
dds_attachment;

typedef struct dds_waitset
{
  dds_entity m_entity;
  dds_attachment *observed;
  dds_attachment *triggered;
}
dds_waitset;

/* Globals */

typedef struct dds_globals
{
  dds_domainid_t m_default_domain;
  int32_t m_init_count;
  void (*m_dur_reader) (struct dds_reader * reader, struct rhc * rhc);
  int (*m_dur_wait) (struct dds_reader * reader, dds_duration_t timeout);
  void (*m_dur_init) (void);
  void (*m_dur_fini) (void);
  ut_avlTree_t m_domains;
  os_mutex m_mutex;
}
dds_globals;

DDS_EXPORT extern dds_globals dds_global;

#if defined (__cplusplus)
}
#endif
#endif
