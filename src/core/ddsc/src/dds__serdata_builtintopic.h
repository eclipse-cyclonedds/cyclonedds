// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__SERDATA_BUILTINTYPE_H
#define DDS__SERDATA_BUILTINTYPE_H

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_entity_common;
struct ddsi_topic_definition;

enum ddsi_sertype_builtintopic_entity_kind {
  DSBT_PARTICIPANT,
  DSBT_TOPIC,
  DSBT_READER,
  DSBT_WRITER
};

struct ddsi_serdata_builtintopic {
  struct ddsi_serdata c;
  union { unsigned char raw[16]; ddsi_guid_t guid; } key;
  dds_qos_t xqos;
};

struct ddsi_serdata_builtintopic_participant {
  struct ddsi_serdata_builtintopic common;
  dds_instance_handle_t pphandle;
};

#ifdef DDS_HAS_TOPIC_DISCOVERY
struct ddsi_serdata_builtintopic_topic {
  struct ddsi_serdata_builtintopic common;
};
#endif

struct ddsi_serdata_builtintopic_endpoint {
  struct ddsi_serdata_builtintopic common;
  dds_instance_handle_t pphandle;
};

struct ddsi_sertype_builtintopic {
  struct ddsi_sertype c;
  enum ddsi_sertype_builtintopic_entity_kind entity_kind;
};

extern const struct ddsi_sertype_ops ddsi_sertype_ops_builtintopic;
extern const struct ddsi_serdata_ops ddsi_serdata_ops_builtintopic;

/** @component typesupport_builtin */
struct ddsi_sertype *dds_new_sertype_builtintopic (enum ddsi_sertype_builtintopic_entity_kind entity_kind, const char *typename);

/** @component typesupport_builtin */
struct ddsi_serdata *dds_serdata_builtin_from_endpoint (const struct ddsi_sertype *tpcmn, const ddsi_guid_t *guid, struct ddsi_entity_common *entity, enum ddsi_serdata_kind kind);


#ifdef DDS_HAS_TOPIC_DISCOVERY
extern const struct ddsi_serdata_ops ddsi_serdata_ops_builtintopic_topic;

/** @component typesupport_builtin */
struct ddsi_sertype *dds_new_sertype_builtintopic_topic (enum ddsi_sertype_builtintopic_entity_kind entity_kind, const char *typename);

/** @component typesupport_builtin */
struct ddsi_serdata *dds_serdata_builtin_from_topic_definition (const struct ddsi_sertype *tpcmn, const dds_builtintopic_topic_key_t *key, const struct ddsi_topic_definition *tpd, enum ddsi_serdata_kind kind);

#endif /* DDS_HAS_TOPIC_DISCOVERY */


#if defined (__cplusplus)
}
#endif

#endif /* DDS__SERDATA_BUILTINTYPE_H */
