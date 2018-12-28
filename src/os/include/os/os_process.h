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
/****************************************************************
 * Interface definition for process management                  *
 ****************************************************************/

/** \file os_process.h
 *  \brief Process management - process creation and termination
 */

#ifndef OS_PROCESS_H
#define OS_PROCESS_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "os/os_defs.h"
/* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

/** \brief Return the process ID of the calling process
 *
 * Possible Results:
 * - returns the process ID of the calling process
 */
OSAPI_EXPORT os_procId os_getpid(void);

#if defined (__cplusplus)
}
#endif

#endif /* OS_PROCESS_H */
