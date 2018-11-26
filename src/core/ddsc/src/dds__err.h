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
#ifndef _DDS_ERR_H_
#define _DDS_ERR_H_

#include <assert.h>
#include "os/os.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* To construct return status
 * Use '+' instead of '|'. Otherwise, the SAL checking doesn't
 * understand when a return value is negative or positive and
 * complains a lot about "A successful path through the function
 * does not set the named _Out_ parameter." */
#if !defined(__FILE_ID__)
#error "__FILE_ID__ not defined"
#endif

#define DDS__FILE_ID__ (((__FILE_ID__ & 0x1ff)) << 22)
#define DDS__LINE__ ((__LINE__ & 0x3fff) << 8)

#define DDS_ERRNO(err) \
    (assert(err > DDS_RETCODE_OK), -(DDS__FILE_ID__ + DDS__LINE__ + (err)))

#if defined (__cplusplus)
}
#endif
#endif
