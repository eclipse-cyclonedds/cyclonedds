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
#ifndef DDSI_SERDATA_BUILTINTOPIC_H
#define DDSI_SERDATA_BUILTINTOPIC_H

#include "dds/ddsi/q_xqos.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertopic.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_serdata_builtintopic {
  struct ddsi_serdata c;
  nn_guid_t key;
  dds_instance_handle_t pphandle;
  dds_qos_t xqos;
};

enum ddsi_sertopic_builtintopic_type {
  DSBT_PARTICIPANT,
  DSBT_READER,
  DSBT_WRITER
};

struct q_globals;
struct ddsi_sertopic_builtintopic {
  struct ddsi_sertopic c;
  enum ddsi_sertopic_builtintopic_type type;
  struct q_globals *gv;
};

extern const struct ddsi_sertopic_ops ddsi_sertopic_ops_builtintopic;
extern const struct ddsi_serdata_ops ddsi_serdata_ops_builtintopic;

struct ddsi_sertopic *new_sertopic_builtintopic (enum ddsi_sertopic_builtintopic_type type, const char *name, const char *typename, struct q_globals *gv);

#if defined (__cplusplus)
}
#endif

#endif
