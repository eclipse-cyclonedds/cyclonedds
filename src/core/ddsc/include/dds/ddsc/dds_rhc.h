// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef _DDS_RHC_H_
#define _DDS_RHC_H_

#include "dds/dds.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds__loaned_sample.h"

#define DDS_RHC_NO_STATE_MASK_SET   (DDS_ANY_STATE + 1)

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_rhc;
struct dds_readcond;
struct dds_reader;
struct ddsi_tkmap;

typedef dds_return_t (*dds_rhc_associate_t) (struct dds_rhc *rhc, struct dds_reader *reader, const struct ddsi_sertype *type, struct ddsi_tkmap *tkmap);
typedef int32_t (*dds_rhc_read_take_t) (struct dds_rhc *rhc, int32_t max_samples, uint32_t mask, dds_instance_handle_t handle, struct dds_readcond *cond, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg);

typedef bool (*dds_rhc_add_readcondition_t) (struct dds_rhc *rhc, struct dds_readcond *cond);
typedef void (*dds_rhc_remove_readcondition_t) (struct dds_rhc *rhc, struct dds_readcond *cond);

typedef uint32_t (*dds_rhc_lock_samples_t) (struct dds_rhc *rhc);

struct dds_rhc_ops {
  /* A copy of DDSI rhc ops comes first so we can use either interface without
     additional indirections */
  struct ddsi_rhc_ops rhc_ops;
  dds_rhc_read_take_t peek;
  dds_rhc_read_take_t read;
  dds_rhc_read_take_t take;
  dds_rhc_add_readcondition_t add_readcondition;
  dds_rhc_remove_readcondition_t remove_readcondition;
  dds_rhc_lock_samples_t lock_samples;
  dds_rhc_associate_t associate;
};

struct dds_rhc {
  union {
    const struct dds_rhc_ops *ops;
    struct ddsi_rhc rhc;
  } common;
};

DDSRT_STATIC_ASSERT (offsetof (struct dds_rhc, common.ops) == offsetof (struct ddsi_rhc, ops));

/** @component rhc */
DDS_INLINE_EXPORT inline dds_return_t dds_rhc_associate (struct dds_rhc *rhc, struct dds_reader *reader, const struct ddsi_sertype *type, struct ddsi_tkmap *tkmap) {
  return rhc->common.ops->associate (rhc, reader, type, tkmap);
}

/** @component rhc */
DDS_INLINE_EXPORT inline bool dds_rhc_store (struct dds_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict wr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk) {
  return rhc->common.ops->rhc_ops.store (&rhc->common.rhc, wr_info, sample, tk);
}

/** @component rhc */
DDS_INLINE_EXPORT inline void dds_rhc_unregister_wr (struct dds_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict wr_info) {
  rhc->common.ops->rhc_ops.unregister_wr (&rhc->common.rhc, wr_info);
}

/** @component rhc */
DDS_INLINE_EXPORT inline void dds_rhc_relinquish_ownership (struct dds_rhc * __restrict rhc, const uint64_t wr_iid) {
  rhc->common.ops->rhc_ops.relinquish_ownership (&rhc->common.rhc, wr_iid);
}

/** @component rhc */
DDS_INLINE_EXPORT inline void dds_rhc_set_qos (struct dds_rhc *rhc, const struct dds_qos *qos) {
  rhc->common.ops->rhc_ops.set_qos (&rhc->common.rhc, qos);
}

/** @component rhc */
DDS_INLINE_EXPORT inline void dds_rhc_free (struct dds_rhc *rhc) {
  rhc->common.ops->rhc_ops.free (&rhc->common.rhc);
}

/** @component rhc */
DDS_INLINE_EXPORT inline int32_t dds_rhc_peek (struct dds_rhc *rhc, int32_t max_samples, uint32_t mask, dds_instance_handle_t handle, struct dds_readcond *cond, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg) {
  return (rhc->common.ops->peek) (rhc, max_samples, mask, handle, cond, collect_sample, collect_sample_arg);
}

/** @component rhc */
DDS_INLINE_EXPORT inline int32_t dds_rhc_read (struct dds_rhc *rhc, int32_t max_samples, uint32_t mask, dds_instance_handle_t handle, struct dds_readcond *cond, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg) {
  return (rhc->common.ops->read) (rhc, max_samples, mask, handle, cond, collect_sample, collect_sample_arg);
}

/** @component rhc */
DDS_INLINE_EXPORT inline int32_t dds_rhc_take (struct dds_rhc *rhc, int32_t max_samples, uint32_t mask, dds_instance_handle_t handle, struct dds_readcond *cond, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg) {
  return rhc->common.ops->take (rhc, max_samples, mask, handle, cond, collect_sample, collect_sample_arg);
}

/** @component rhc */
DDS_INLINE_EXPORT inline bool dds_rhc_add_readcondition (struct dds_rhc *rhc, struct dds_readcond *cond) {
  return rhc->common.ops->add_readcondition (rhc, cond);
}

/** @component rhc */
DDS_INLINE_EXPORT inline void dds_rhc_remove_readcondition (struct dds_rhc *rhc, struct dds_readcond *cond) {
  rhc->common.ops->remove_readcondition (rhc, cond);
}

/** @component rhc */
DDS_EXPORT void dds_reader_data_available_cb (struct dds_reader *rd);

#if defined (__cplusplus)
}
#endif
#endif
