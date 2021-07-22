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
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/threads.h"

static ddsrt_thread_local DWORD dynlib_last_err;

dds_return_t ddsrt_dlopen (const char *name, bool translate, ddsrt_dynlib_t *handle)
{
  assert (handle);
  *handle = NULL;

  if (translate && (strrchr (name, '/') == NULL && strrchr (name, '\\') == NULL))
  {
    static const char suffix[] = ".dll";
    char *lib_name;
    if (ddsrt_asprintf (&lib_name, "%s%s", name, suffix) == -1)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    *handle = (ddsrt_dynlib_t) LoadLibrary (lib_name);
    ddsrt_free (lib_name);
  }

  if (*handle == NULL)
  {
    /* Name contains a path, (auto)translate is disabled or LoadLibrary on translated name failed. */
    *handle = (ddsrt_dynlib_t) LoadLibrary (name);
  }

  if (*handle == NULL)
  {
    dynlib_last_err = GetLastError ();
    return DDS_RETCODE_ERROR;
  }
  dynlib_last_err = 0;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_dlclose (ddsrt_dynlib_t handle)
{
  assert (handle);
  if (FreeLibrary ((HMODULE) handle) == 0)
  {
    dynlib_last_err = GetLastError ();
    return DDS_RETCODE_ERROR;
  }
  dynlib_last_err = 0;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_dlsym (ddsrt_dynlib_t handle, const char *symbol, void **address)
{
  assert (handle);
  assert (address);
  assert (symbol);
  if ((*address = GetProcAddress ((HMODULE) handle, symbol)) == NULL)
  {
    dynlib_last_err = GetLastError ();
    return DDS_RETCODE_ERROR;
  }
  dynlib_last_err = 0;
  return DDS_RETCODE_OK;
}

dds_return_t ddsrt_dlerror (char *buf, size_t buflen)
{
  assert (buf);
  assert (buflen);
  dds_return_t ret = 0;
  buf[0] = '\0';
  if (dynlib_last_err != 0)
  {
    if ((ret = ddsrt_strerror_r ((int)dynlib_last_err, buf, buflen)) == DDS_RETCODE_OK)
      ret = (int32_t) strlen (buf);
    else if (ret == DDS_RETCODE_BAD_PARAMETER)
    {
      const char *err_unknown = "unknown error";
      size_t len = ddsrt_strlcpy (buf, err_unknown, buflen);
      ret = (len >= buflen) ? DDS_RETCODE_NOT_ENOUGH_SPACE : (int32_t) strlen (buf);
    }
    dynlib_last_err = 0;
  }
  return ret;
}
