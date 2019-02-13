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
#ifndef DDSI_RHC_PLUGIN_H
#define DDSI_RHC_PLUGIN_H

struct rhc;
struct nn_xqos;
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

struct ddsi_rhc_plugin
{
  void (*rhc_free_fn) (struct rhc *rhc);
  void (*rhc_fini_fn) (struct rhc *rhc);
  bool (*rhc_store_fn)
  (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info,
   struct ddsi_serdata * __restrict sample, struct ddsi_tkmap_instance * __restrict tk);
  void (*rhc_unregister_wr_fn)
  (struct rhc * __restrict rhc, const struct proxy_writer_info * __restrict pwr_info);
  void (*rhc_relinquish_ownership_fn)
  (struct rhc * __restrict rhc, const uint64_t wr_iid);
  void (*rhc_set_qos_fn) (struct rhc * rhc, const struct nn_xqos * qos);
};

DDS_EXPORT void make_proxy_writer_info(struct proxy_writer_info *pwr_info, const struct entity_common *e, const struct nn_xqos *xqos);

#endif
