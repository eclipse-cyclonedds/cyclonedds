// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

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
  buf[0] = '\0';
  if (dynlib_last_err != 0)
  {
    DWORD cnt = FormatMessage(
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS |
      FORMAT_MESSAGE_MAX_WIDTH_MASK,
      NULL,
      dynlib_last_err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)buf,
      (DWORD)buflen,
      NULL);
    dynlib_last_err = 0;
    return (dds_return_t)cnt;
  }
  return 0;
}
