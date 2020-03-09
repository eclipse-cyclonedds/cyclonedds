/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef _DDSI_BUILTIN_TOPIC_IF_H_
#define _DDSI_BUILTIN_TOPIC_IF_H_

#include "dds/ddsi/ddsi_vendor.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct entity_common;
struct ddsi_tkmap_instance;
struct ddsi_sertopic;
struct ddsi_guid;

struct ddsi_builtin_topic_interface {
  void *arg;

  bool (*builtintopic_is_builtintopic) (const struct ddsi_sertopic *topic, void *arg);
  bool (*builtintopic_is_visible) (const struct ddsi_guid *guid, nn_vendorid_t vendorid, void *arg);
  struct ddsi_tkmap_instance * (*builtintopic_get_tkmap_entry) (const struct ddsi_guid *guid, void *arg);
  void (*builtintopic_write) (const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive, void *arg);
};

inline bool builtintopic_is_visible (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid, nn_vendorid_t vendorid) {
  return btif ? btif->builtintopic_is_visible (guid, vendorid, btif->arg) : false;
}
inline bool builtintopic_is_builtintopic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_sertopic *topic) {
  return btif ? btif->builtintopic_is_builtintopic (topic, btif->arg) : false;
}
inline struct ddsi_tkmap_instance *builtintopic_get_tkmap_entry (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid) {
  return btif ? btif->builtintopic_get_tkmap_entry (guid, btif->arg) : NULL;
}
inline void builtintopic_write (const struct ddsi_builtin_topic_interface *btif, const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive) {
  if (btif) btif->builtintopic_write (e, timestamp, alive, btif->arg);
}

#if defined (__cplusplus)
}
#endif

#endif
