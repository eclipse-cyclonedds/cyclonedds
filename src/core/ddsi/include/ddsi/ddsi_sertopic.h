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
#ifndef DDSI_SERTOPIC_H
#define DDSI_SERTOPIC_H

#include "util/ut_avl.h"
#include "ddsc/dds_public_alloc.h"

struct ddsi_serdata;
struct ddsi_serdata_ops;

struct dds_topic;
typedef void (*topic_cb_t) (struct dds_topic * topic);

struct ddsi_sertopic_ops;

struct ddsi_sertopic {
  ut_avlNode_t avlnode; /* index on name_typename */
  const struct ddsi_sertopic_ops *ops;
  const struct ddsi_serdata_ops *serdata_ops;
  uint32_t serdata_basehash;
  char *name_typename;
  char *name;
  char *typename;
  uint64_t iid;
  os_atomic_uint32_t refc; /* counts refs from entities, not from data */

  topic_cb_t status_cb;
  struct dds_topic * status_cb_entity;
};

typedef void (*ddsi_sertopic_deinit_t) (struct ddsi_sertopic *tp);

/* Release any memory allocated by ddsi_sertopic_to_sample */
typedef void (*ddsi_sertopic_free_sample_t) (const struct ddsi_sertopic *d, void *sample, dds_free_op_t op);

struct ddsi_sertopic_ops {
  ddsi_sertopic_deinit_t deinit;
  ddsi_sertopic_free_sample_t free_sample;
};

struct ddsi_sertopic *ddsi_sertopic_ref (const struct ddsi_sertopic *tp);
void ddsi_sertopic_unref (struct ddsi_sertopic *tp);
uint32_t ddsi_sertopic_compute_serdata_basehash (const struct ddsi_serdata_ops *ops);

inline void ddsi_sertopic_deinit (struct ddsi_sertopic *tp) {
  tp->ops->deinit (tp);
}
inline void ddsi_sertopic_free_sample (const struct ddsi_sertopic *tp, void *sample, dds_free_op_t op) {
  tp->ops->free_sample (tp, sample, op);
}

#endif
