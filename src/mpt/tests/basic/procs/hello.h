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

/** @file
 *
 * @brief DDS C Communication Status API
 *
 * This header file defines the public API of the Communication Status in the
 * Eclipse Cyclone DDS C language binding.
 */
#ifndef MPT_BASIC_PROCS_HELLO_H
#define MPT_BASIC_PROCS_HELLO_H

#include <stdio.h>
#include <string.h>

#include "dds/dds.h"
#include "mpt/mpt.h"

#if defined (__cplusplus)
extern "C" {
#endif

void hello_init(void);
void hello_fini(void);

MPT_ProcessEntry(hello_publisher,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name,
                          int sub_cnt,
                          const char *text));

MPT_ProcessEntry(hello_subscriber,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name,
                          int sample_cnt,
                          const char *text));

#if defined (__cplusplus)
}
#endif

#endif /* MPT_BASIC_PROCS_HELLO_H */
