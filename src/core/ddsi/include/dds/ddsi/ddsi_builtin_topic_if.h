// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_BUILTIN_TOPIC_IF_H
#define DDSI_BUILTIN_TOPIC_IF_H

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_entity_common;
struct ddsi_tkmap_instance;
struct ddsi_sertype;
struct ddsi_guid;
struct ddsi_topic_definition;

struct ddsi_builtin_topic_interface {
  void *arg;

  bool (*builtintopic_is_builtintopic) (const struct ddsi_sertype *type, void *arg);
  bool (*builtintopic_is_visible) (const struct ddsi_guid *guid, ddsi_vendorid_t vendorid, void *arg);
  struct ddsi_tkmap_instance * (*builtintopic_get_tkmap_entry) (const struct ddsi_guid *guid, void *arg);
  void (*builtintopic_write_endpoint) (const struct ddsi_entity_common *e, ddsrt_wctime_t timestamp, bool alive, void *arg);
  void (*builtintopic_write_topic) (const struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp, bool alive, void *arg);
};

/** @component builtintopic_if */
inline bool ddsi_builtintopic_is_visible (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid, ddsi_vendorid_t vendorid) {
  return btif ? btif->builtintopic_is_visible (guid, vendorid, btif->arg) : false;
}

/** @component builtintopic_if */
inline bool ddsi_builtintopic_is_builtintopic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_sertype *type) {
  return btif ? btif->builtintopic_is_builtintopic (type, btif->arg) : false;
}

/** @component builtintopic_if */
inline struct ddsi_tkmap_instance *ddsi_builtintopic_get_tkmap_entry (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid) {
  return btif ? btif->builtintopic_get_tkmap_entry (guid, btif->arg) : NULL;
}

/** @component builtintopic_if */
inline void ddsi_builtintopic_write_endpoint (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_entity_common *e, ddsrt_wctime_t timestamp, bool alive) {
  if (btif) btif->builtintopic_write_endpoint (e, timestamp, alive, btif->arg);
}

/** @component builtintopic_if */
inline void ddsi_builtintopic_write_topic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp, bool alive) {
  if (btif) btif->builtintopic_write_topic (tpd, timestamp, alive, btif->arg);
}

#if defined (__cplusplus)
}
#endif

#endif
