// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__RHC_H
#define DDSI__RHC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/time.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_rhc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_qos;

/** @component rhc_if */
inline void ddsi_rhc_unregister_wr (struct ddsi_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict wrinfo) {
  rhc->ops->unregister_wr (rhc, wrinfo);
}

/** @component rhc_if */
inline void ddsi_rhc_relinquish_ownership (struct ddsi_rhc * __restrict rhc, const uint64_t wr_iid) {
  rhc->ops->relinquish_ownership (rhc, wr_iid);
}

/** @component rhc_if */
inline void ddsi_rhc_set_qos (struct ddsi_rhc *rhc, const struct dds_qos *qos) {
  rhc->ops->set_qos (rhc, qos);
}

/** @component rhc_if */
inline void ddsi_rhc_free (struct ddsi_rhc *rhc) {
  rhc->ops->free (rhc);
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__RHC_H */
