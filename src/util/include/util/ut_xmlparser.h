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
#ifndef UT_XMLPARSER_H
#define UT_XMLPARSER_H

#include "os/os.h"
#include "util/ut_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

    typedef int (*ut_xmlpProcElemOpen_t) (void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name);
    typedef int (*ut_xmlpProcAttr_t) (void *varg, uintptr_t eleminfo, const char *name, const char *value);
    typedef int (*ut_xmlpProcElemData_t) (void *varg, uintptr_t eleminfo, const char *data);
    typedef int (*ut_xmlpProcElemClose_t) (void *varg, uintptr_t eleminfo);
    typedef void (*ut_xmlpError) (void *varg, const char *msg, int line);

    struct ut_xmlpCallbacks {
        ut_xmlpProcElemOpen_t elem_open;
        ut_xmlpProcAttr_t attr;
        ut_xmlpProcElemData_t elem_data;
        ut_xmlpProcElemClose_t elem_close;
        ut_xmlpError error;
    };

    struct ut_xmlpState;

    UTIL_EXPORT struct ut_xmlpState *ut_xmlpNewFile (FILE *fp, void *varg, const struct ut_xmlpCallbacks *cb);
    UTIL_EXPORT struct ut_xmlpState *ut_xmlpNewString (const char *string, void *varg, const struct ut_xmlpCallbacks *cb);
    UTIL_EXPORT void ut_xmlpFree (struct ut_xmlpState *st);
    UTIL_EXPORT int ut_xmlpParse (struct ut_xmlpState *st);

    UTIL_EXPORT int ut_xmlUnescapeInsitu (char *buffer, size_t *n);

#if defined (__cplusplus)
}
#endif

#endif
