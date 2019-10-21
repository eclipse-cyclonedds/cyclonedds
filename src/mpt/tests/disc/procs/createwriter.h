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

#ifndef MPT_DISCSTRESS_PROCS_CREATEWRITER_H
#define MPT_DISCSTRESS_PROCS_CREATEWRITER_H

#include <stdio.h>
#include <string.h>

#include "cyclonedds/dds.h"
#include "mpt/mpt.h"

#if defined (__cplusplus)
extern "C" {
#endif

void createwriter_init(void);
void createwriter_fini(void);

MPT_ProcessEntry(createwriter_publisher,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name));

MPT_ProcessEntry(createwriter_subscriber,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name));

#if defined (__cplusplus)
}
#endif

#endif /* MPT_DISCSTRESS_PROCS_CREATEWRITER_H */
