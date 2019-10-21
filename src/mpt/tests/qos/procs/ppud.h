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

#ifndef MPT_QOSMATCH_PROCS_PPUD_H
#define MPT_QOSMATCH_PROCS_PPUD_H

#include <stdio.h>
#include <string.h>

#include "cyclonedds/dds.h"
#include "mpt/mpt.h"

#if defined (__cplusplus)
extern "C" {
#endif

void ppud_init (void);
void ppud_fini (void);

enum rwud {
  RWUD_USERDATA,
  RWUD_GROUPDATA,
  RWUD_TOPICDATA
};

struct rwud_barrier;

MPT_ProcessEntry (ppud,
                  MPT_Args (dds_domainid_t domainid,
                            bool master,
                            unsigned ncycles));

MPT_ProcessEntry (rwud,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool master,
                            unsigned ncycles,
                            enum rwud which,
                            struct rwud_barrier *barrier));

MPT_ProcessEntry (rwudM,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name,
                            bool master,
                            unsigned ncycles,
                            enum rwud which));

#if defined (__cplusplus)
}
#endif

#endif
