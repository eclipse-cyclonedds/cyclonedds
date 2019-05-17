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
#include <dlfcn.h>
#include <assert.h>
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/library.h"


dds_retcode_t
ddsrt_dlopen(
    const char *name,
    bool translate,
    ddsrt_lib *handle)
{
  *handle = NULL;
  dds_retcode_t retcode = DDS_RETCODE_OK;

  dlerror();    /* Clear any existing error */

  if ( name == NULL ){
      retcode = DDS_RETCODE_BAD_PARAMETER;

  } else {
      if ( (translate) && (strrchr(name, '/') == NULL)) {
        /* Add lib and suffix to the name and try to open. */
#if __APPLE__
        static const char suffix[] = ".dylib";
#else
        static const char suffix[] = ".so";
#endif
        char* libName = ddsrt_malloc(3 + strlen(name) + sizeof(suffix));
        sprintf(libName, "lib%s%s", name, suffix);
        *handle = dlopen(libName, RTLD_GLOBAL | RTLD_NOW);
        ddsrt_free(libName);


      }

      if (*handle == NULL && retcode == DDS_RETCODE_OK) {
        /* name contains a path,
         * (auto)translate is disabled or
         * dlopen on translated name failed. */
        *handle = dlopen(name, RTLD_GLOBAL | RTLD_NOW);

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
  return (dlclose(handle) == 0) ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}


void*
ddsrt_dlsym(
    ddsrt_lib handle,
    const char *symbol)
{
  assert(handle);
  assert(symbol);
  return dlsym(handle, symbol);
}


dds_retcode_t
ddsrt_dlerror(
    char *buf,
    size_t buflen)
{
  const char *err = dlerror();
  if (err == NULL) {
    return DDS_RETCODE_NOT_FOUND;
  }
  snprintf(buf, buflen, "%s", err);
  return DDS_RETCODE_OK;
}
