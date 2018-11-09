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
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "os/os.h"
#include "ddsi/sysdeps.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "ddsi/q_freelist.h"
#include "ddsi/ddsi_sertopic.h"
#include "ddsi/ddsi_serdata_default.h"

/* FIXME: sertopic /= ddstopic so a lot of stuff needs to be moved here from dds_topic.c and the free function needs to be implemented properly */

static void sertopic_default_deinit (struct ddsi_sertopic *tp)
{
  (void)tp;
}

static void sertopic_default_free_sample (const struct ddsi_sertopic *sertopic_common, void *sample, dds_free_op_t op)
{
  const struct ddsi_sertopic_default *tp = (const struct ddsi_sertopic_default *)sertopic_common;
  dds_sample_free (sample, tp->type, op);
}

const struct ddsi_sertopic_ops ddsi_sertopic_ops_default = {
  .deinit = sertopic_default_deinit,
  .free_sample = sertopic_default_free_sample
};
