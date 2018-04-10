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
#ifndef OS_REPORT_H
#define OS_REPORT_H

#include <stdarg.h>

#if defined (__cplusplus)
extern "C" {
#endif

    /* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

    /* Subcomponents might need to alter the report before actually handing it over
     * to os_report. Since os_report truncates messages, those components can get
     * away with fixed size buffers as well, but the maximum size must known at
     * that point.
     */
#define OS_REPORT_BUFLEN 1024

    /*
     Note - in the below the check of reportType against os_reportVerbosity is also present
     in os_report. By duplicating it we avoid putting the call onto the stack and evaluating
     args if not necessary.
     */

#define OS_REPORT(type,context,code,message,...) \
(((type) >= os_reportVerbosity) ? os_report((type),(context),__FILE__,__LINE__,(code),(message),##__VA_ARGS__) : (void)0)


#define OS_DEBUG(context,code,message,...) OS_REPORT(OS_REPORT_DEBUG,(context),(code),(message),##__VA_ARGS__)
#define OS_INFO(context,code,message,...) OS_REPORT(OS_REPORT_INFO,(context),(code),(message),##__VA_ARGS__)
#define OS_WARNING(context,code,message,...) OS_REPORT(OS_REPORT_WARNING,(context),(code),(message),##__VA_ARGS__)
#define OS_ERROR(context,code,message,...) OS_REPORT(OS_REPORT_ERROR,(context),(code),(message),##__VA_ARGS__)
#define OS_CRITICAL(context,code,message,...) OS_REPORT(OS_REPORT_CRITICAL,(context),(code),(message),##__VA_ARGS__)
#define OS_FATAL(context,code,message,...) OS_REPORT(OS_REPORT_FATAL,(context),(code),(message),##__VA_ARGS__)

#define OS_REPORT_STACK() \
os_report_stack()

#define OS_REPORT_FLUSH(condition) \
os_report_flush((condition), OS_FUNCTION, __FILE__, __LINE__)

    /**
     * These types are an ordered series of incremental 'importance' (to the user)
     * levels.
     * @see os_reportVerbosity
     */
    typedef enum os_reportType {
        OS_REPORT_DEBUG,
        OS_REPORT_INFO,
        OS_REPORT_WARNING,
        OS_REPORT_ERROR,
        OS_REPORT_FATAL,
        OS_REPORT_CRITICAL,
        OS_REPORT_NONE
    } os_reportType;

    OSAPI_EXPORT extern os_reportType os_reportVerbosity;

    OSAPI_EXPORT void
    os_reportInit(_In_ bool forceReInit);

    OSAPI_EXPORT void
    os_reportExit(void);

    /** \brief Report message
     *
     * Consider this function private. It should be invoked by reporting functions
     * specified in the language bindings only.
     *
     * @param type type of report
     * @param context context in which report was generated, often function name
     *                from which function was invoked
     * @param path path of file from which function was invoked
     * @param line line of file from which function was invoked
     * @param code error code associated with the report
     * @param format message to log
     * @param ... Parameter to log
     */

    OSAPI_EXPORT void
    os_report(
              _In_ os_reportType type,
              _In_z_ const char *context,
              _In_z_ const char *path,
              _In_ int32_t line,
              _In_ int32_t code,
              _In_z_ _Printf_format_string_ const char *format,
              ...) __attribute_format__((printf,6,7));

    /*****************************************
     * Report stack related functions
     *****************************************/

    /**
     * The os_report_stack operation enables a report stack for the current thread.
     * The stack will be disabled again by the os_report_flush operation.
     */
    OSAPI_EXPORT void
    os_report_stack(
                    void);

    /**
     * The os_report_stack_free operation frees all memory allocated by the current
     * thread for the report stack.
     */
    OSAPI_EXPORT void
    os_report_stack_free(
                         void);

    /**
     * The os_report_flush operation removes the report message from the stack,
     * and if valid is TRUE also writes them into the report device.
     * This operation additionally disables the stack.
     */
    OSAPI_EXPORT void
    os_report_flush(
                    _In_ bool valid,
                    _In_z_ const char *context,
                    _In_z_ const char *file,
                    _In_ int line);

#if defined (__cplusplus)
}
#endif

#endif /* OS_REPORT_H */
