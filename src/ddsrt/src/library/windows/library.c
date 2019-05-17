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
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/library.h"


dds_retcode_t
ddsrt_dlopen(
    const char *name,
    bool translate,
    ddsrt_lib *handle)
{
    *handle = NULL;
    dds_retcode_t retcode = DDS_RETCODE_OK;

    GetLastError();    /* Clear any existing error */

    if ( name == NULL ){
        retcode = DDS_RETCODE_BAD_PARAMETER;

    } else {
        if ((translate) &&
          (strrchr(name, '/') == NULL) &&
          (strrchr(name, '\\') == NULL)) {
            /* Add suffix to the name and try to open. */
            static const char suffix[] = ".dll";
            size_t len = strlen(name) + sizeof(suffix);
            char* libName = ddsrt_malloc(len);
            sprintf_s(libName, len, "%s%s", name, suffix);
            *handle = LoadLibrary(libName);
            ddsrt_free(libName);
        }

        if (*handle == NULL && retcode == DDS_RETCODE_OK) {
            /* Name contains a path,
             * (auto)translate is disabled or
             * LoadLibrary on translated name failed. */
            *handle = LoadLibrary( name );
        }

        if ( *handle != NULL ){
            retcode = DDS_RETCODE_OK;
        } else {
            retcode = DDS_RETCODE_ERROR;
        }
    }

    return retcode;
}


dds_retcode_t
ddsrt_dlclose(
    ddsrt_lib handle)
{
  assert(handle);
  return (FreeLibrary(handle) == 0) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}


void*
ddsrt_dlsym(
    ddsrt_lib handle,
    const char *symbol)
{
  assert(handle);
  assert(symbol);
  return GetProcAddress(handle, symbol);
}


dds_retcode_t
ddsrt_dlerror(char *buf, size_t buflen)
{
  /* Hopefully (and likely), the last error is
   * related to a Library action attempt. */
  DWORD err = GetLastError();

  /* Get the string that is related to this error. */
  if (ddsrt_strerror_r(err, buf, buflen) == DDS_RETCODE_OK) {
    return DDS_RETCODE_OK;
  }

  return DDS_RETCODE_NOT_FOUND;
}

