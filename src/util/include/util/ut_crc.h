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
#ifndef UT_CRC_H
#define UT_CRC_H

#include "os/os.h"
#include "util/ut_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

UTIL_EXPORT uint32_t ut_crcCalculate (const void *buf, size_t length) __nonnull_all__ __attribute_pure__;

#if defined (__cplusplus)
}
#endif

#endif /* UT_CRC_H */
