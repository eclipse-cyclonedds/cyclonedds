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
#include "dds/ddsi/q_rhc.h"
#include "dds/ddsi/q_xqos.h"
#include "dds/ddsi/q_entity.h"

extern inline void rhc_free (struct rhc *rhc);
extern inline bool rhc_store (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info, struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
extern inline void rhc_unregister_wr (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info);
extern inline void rhc_relinquish_ownership (struct rhc * __restrict rhc, const uint64_t wr_iid);
extern inline void rhc_set_qos (struct rhc *rhc, const struct dds_qos *qos);

void make_proxy_writer_info(struct proxy_writer_info *pwr_info, const struct entity_common *e, const struct dds_qos *xqos)
{
  pwr_info->guid = e->guid;
  pwr_info->ownership_strength = xqos->ownership_strength.value;
  pwr_info->auto_dispose = xqos->writer_data_lifecycle.autodispose_unregistered_instances;
  pwr_info->iid = e->iid;
}
