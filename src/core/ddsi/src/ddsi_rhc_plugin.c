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
#include "ddsi/q_entity.h"
#include "ddsi/q_xqos.h"
#include "ddsi/ddsi_rhc_plugin.h"

DDS_EXPORT void make_proxy_writer_info(struct proxy_writer_info *pwr_info, const struct entity_common *e, const struct nn_xqos *xqos)
{
  pwr_info->guid = e->guid;
  pwr_info->ownership_strength = xqos->ownership_strength.value;
  pwr_info->auto_dispose = xqos->writer_data_lifecycle.autodispose_unregistered_instances;
  pwr_info->iid = e->iid;
}
