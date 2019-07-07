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
#ifndef Q_RHC_H
#define Q_RHC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "dds/export.h"

/* DDS_EXPORT inline i.c.w. __attributes__((visibility...)) and some compilers: */
#include "dds/ddsrt/attributes.h"

#include "dds/ddsi/q_rtps.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_qos;
struct ddsi_tkmap_instance;
struct ddsi_serdata;
struct ddsi_sertopic;
struct entity_common;

struct proxy_writer_info
{
  nn_guid_t guid;
  bool auto_dispose;
  int32_t ownership_strength;
  uint64_t iid;
};

struct rhc;

typedef void (*rhc_free_t) (struct rhc *rhc);
typedef bool (*rhc_store_t) (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
typedef void (*rhc_unregister_wr_t) (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info);
typedef void (*rhc_relinquish_ownership_t) (struct rhc * __restrict rhc, const uint64_t wr_iid);
typedef void (*rhc_set_qos_t) (struct rhc *rhc, const struct dds_qos *qos);

struct rhc_ops {
  rhc_store_t store;
  rhc_unregister_wr_t unregister_wr;
  rhc_relinquish_ownership_t relinquish_ownership;
  rhc_set_qos_t set_qos;
  rhc_free_t free;
};

struct rhc {
  const struct rhc_ops *ops;
};

DDS_EXPORT inline bool rhc_store (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk) {
  return rhc->ops->store (rhc, pwr_info, sample, tk);
}
DDS_EXPORT inline void rhc_unregister_wr (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info) {
  rhc->ops->unregister_wr (rhc, pwr_info);
}
DDS_EXPORT inline void rhc_relinquish_ownership (struct rhc * __restrict rhc, const uint64_t wr_iid) {
  rhc->ops->relinquish_ownership (rhc, wr_iid);
}
DDS_EXPORT inline void rhc_set_qos (struct rhc *rhc, const struct dds_qos *qos) {
  rhc->ops->set_qos (rhc, qos);
}
DDS_EXPORT inline void rhc_free (struct rhc *rhc) {
  rhc->ops->free (rhc);
}

DDS_EXPORT void make_proxy_writer_info(struct proxy_writer_info *pwr_info, const struct entity_common *e, const struct dds_qos *xqos);

#if defined (__cplusplus)
}
#endif

#endif /* Q_RHC_H */
