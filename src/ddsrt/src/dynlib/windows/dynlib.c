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
#include <stdio.h>
#include <assert.h>
#include <dds/ddsrt/dynlib.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/string.h"

dds_return_t ddsrt_dlopen(const char *name, bool translate,
        ddsrt_dynlib_t *handle) {
    dds_return_t retcode = DDS_RETCODE_OK;

    assert( handle );
    *handle = NULL;

    if ((translate) && (strrchr(name, '/') == NULL )
            && (strrchr(name, '\\') == NULL )) {
        /* Add suffix to the name and try to open. */
        static const char suffix[] = ".dll";
        size_t len = strlen(name) + sizeof(suffix);
        char* libName = ddsrt_malloc(len);
        sprintf_s(libName, len, "%s%s", name, suffix);
        *handle = (ddsrt_dynlib_t)LoadLibrary(libName);
        ddsrt_free(libName);
    }

    if (*handle == NULL) {
        /* Name contains a path,
        * (auto)translate is disabled or
        * LoadLibrary on translated name failed. */
        *handle = (ddsrt_dynlib_t)LoadLibrary(name);
    }

    if (*handle != NULL) {
        retcode = DDS_RETCODE_OK;
    } else {
        retcode = DDS_RETCODE_ERROR;
    }

    return retcode;
}

dds_return_t ddsrt_dlclose(ddsrt_dynlib_t handle) {

    assert ( handle );
    return (FreeLibrary((HMODULE)handle) == 0) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}

dds_return_t ddsrt_dlsym(ddsrt_dynlib_t handle, const char *symbol,
        void **address) {
    dds_return_t retcode = DDS_RETCODE_OK;

    assert( handle );
    assert( address );
    assert( symbol );

    *address = GetProcAddress((HMODULE)handle, symbol);
    if ( *address == NULL ) {
        retcode = DDS_RETCODE_ERROR;
    }

    return retcode;
}

dds_return_t ddsrt_dlerror(char *buf, size_t buflen) {

    /* Hopefully (and likely), the last error is
    * related to a Library action attempt. */
    DWORD err;
    assert ( buf );

    dds_return_t retcode = DDS_RETCODE_OK;

    err = GetLastError();
    if ( err == 0 ) {
        retcode = DDS_RETCODE_NOT_FOUND;
    } else {
        ddsrt_strerror_r(err, buf, buflen);
        SetLastError(0);
    }

    return retcode;

}

