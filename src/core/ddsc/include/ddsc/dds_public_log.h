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
/* TODO: do we really need to expose this as an API? */

/** @file
 *
 * @brief DDS C Logging API
 *
 * This header file defines the public API for logging in the
 * CycloneDDS C language binding.
 */
#ifndef DDS_LOG_H
#define DDS_LOG_H

#include "os/os_public.h"
#include "ddsc/dds_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

DDS_EXPORT void dds_log_info (const char * fmt, ...);
DDS_EXPORT void dds_log_warn (const char * fmt, ...);
DDS_EXPORT void dds_log_error (const char * fmt, ...);
DDS_EXPORT void dds_log_fatal (const char * fmt, ...);

#if defined (__cplusplus)
}
#endif
#endif
