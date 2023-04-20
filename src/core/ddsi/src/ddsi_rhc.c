// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "ddsi__rhc.h"

extern inline void ddsi_rhc_free (struct ddsi_rhc *rhc);
extern inline bool ddsi_rhc_store (struct ddsi_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict wrinfo, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
extern inline void ddsi_rhc_unregister_wr (struct ddsi_rhc * __restrict rhc, const struct ddsi_writer_info * __restrict wrinfo);
extern inline void ddsi_rhc_relinquish_ownership (struct ddsi_rhc * __restrict rhc, const uint64_t wr_iid);
extern inline void ddsi_rhc_set_qos (struct ddsi_rhc *rhc, const struct dds_qos *qos);
