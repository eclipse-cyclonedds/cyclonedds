// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/** @file
 *
 * @brief DDS C Error API
 *
 * This header file defines the public API of error values and convenience
 * functions in the CycloneDDS C language binding.
 */
#ifndef DDS_ERROR_H
#define DDS_ERROR_H

#include "dds/export.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/retcode.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* ** DEPRECATED ** */
#ifndef DOXYGEN_SHOULD_SKIP_THIS

#define DDS_ERR_NR_MASK       0x000000ff
#define DDS_ERR_LINE_MASK     0x003fff00
#define DDS_ERR_FILE_ID_MASK  0x7fc00000

#define dds_err_nr(e) (e)
#define dds_err_line(e) (0)
#define dds_err_file_id(e) (0)
#define DDS_CHECK_REPORT 0x01
#define DDS_CHECK_FAIL 0x02
#define DDS_CHECK_EXIT 0x04

typedef void (*dds_fail_fn) (const char *, const char *);
DDS_DEPRECATED_EXPORT void dds_fail_set (dds_fail_fn fn);
DDS_DEPRECATED_EXPORT dds_fail_fn dds_fail_get (void);
DDS_DEPRECATED_EXPORT const char * dds_err_str (dds_return_t err);
DDS_DEPRECATED_EXPORT void dds_fail (const char * msg, const char * where);
DDS_DEPRECATED_EXPORT bool dds_err_check (dds_return_t err, unsigned flags, const char * where);
#define DDS_ERR_CHECK(e, f) (dds_err_check ((e), (f), __FILE__ ":" DDSRT_STRINGIFY (__LINE__)))

#endif // DOXYGEN_SHOULD_SKIP_THIS

#if defined (__cplusplus)
}
#endif
#endif
