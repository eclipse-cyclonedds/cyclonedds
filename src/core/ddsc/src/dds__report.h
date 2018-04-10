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
#ifndef DDS_REPORT_H
#define DDS_REPORT_H

#include <stdarg.h>
#include "os/os_report.h"

#define DDS_REPORT_STACK()          \
    os_report_stack ()

#define DDS_CRITICAL(...)           \
    dds_report (                    \
        OS_REPORT_CRITICAL,         \
        __FILE__,                   \
        __LINE__,                   \
        OS_FUNCTION,                \
        DDS_RETCODE_ERROR,          \
        __VA_ARGS__)

#define DDS_ERROR(code,...)        \
    dds_report (                    \
        OS_REPORT_ERROR,            \
        __FILE__,                   \
        __LINE__,                   \
        OS_FUNCTION,                \
        (code),                     \
        __VA_ARGS__)

#define DDS_INFO(...)        \
    dds_report (                    \
        OS_REPORT_INFO,             \
        __FILE__,                   \
        __LINE__,                   \
        OS_FUNCTION,                \
        DDS_RETCODE_OK,             \
        __VA_ARGS__)

#define DDS_WARNING(code,...)       \
    dds_report (                    \
        OS_REPORT_WARNING,          \
        __FILE__,                   \
        __LINE__,                   \
        OS_FUNCTION,                \
        (code),                     \
        __VA_ARGS__)

#define DDS_REPORT(type, code,...) \
    dds_report (                    \
        type,                       \
        __FILE__,                   \
        __LINE__,                   \
        OS_FUNCTION,                \
        (code),                     \
        __VA_ARGS__)

#define DDS_REPORT_FLUSH OS_REPORT_FLUSH

void
dds_report(
    os_reportType reportType,
    const char *file,
    int32_t line,
    const char *function,
    dds_return_t code,
    const char *format,
    ...);

#endif
