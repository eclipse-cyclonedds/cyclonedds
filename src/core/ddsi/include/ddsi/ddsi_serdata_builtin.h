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
#ifndef DDSI_SERDATA_BUILTIN_H
#define DDSI_SERDATA_BUILTIN_H

#include "os/os.h"
#include "util/ut_avl.h"
#include "sysdeps.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/ddsi_sertopic.h"
#include "ddsi/q_xqos.h"

struct ddsi_serdata_builtin {
  struct ddsi_serdata c;
  nn_guid_t key;
  nn_xqos_t xqos;
};

enum ddsi_sertopic_builtin_type {
  DSBT_PARTICIPANT,
  DSBT_READER,
  DSBT_WRITER
};

struct ddsi_sertopic_builtin {
  struct ddsi_sertopic c;
  enum ddsi_sertopic_builtin_type type;
};

extern const struct ddsi_sertopic_ops ddsi_sertopic_ops_builtin;
extern const struct ddsi_serdata_ops ddsi_serdata_ops_builtin;

struct ddsi_sertopic *new_sertopic_builtin (enum ddsi_sertopic_builtin_type type, const char *name, const char *typename);

#endif
