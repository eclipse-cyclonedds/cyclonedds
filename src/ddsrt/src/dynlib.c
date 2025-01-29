// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "dds/ddsrt/countargs.h"
#include "dds/ddsrt/foreach.h"
#include "dds/ddsrt/dynlib.h"

// HACK, we shouldn't include this file if there are none
#ifdef PLUGINS

#define COMMA() ,
#define SEMICOLON() ;

#define DLSYM_EXTERN(f) extern void f (void)
#define MAKE_DLSYM_EXTERNS_2(...) DDSRT_FOREACH_B(DLSYM_EXTERN, SEMICOLON, __VA_ARGS__)
#define MAKE_DLSYM_EXTERNS_1(...) MAKE_DLSYM_EXTERNS_2(__VA_ARGS__)
#define MAKE_DLSYM_EXTERNS(p) MAKE_DLSYM_EXTERNS_1(PLUGIN_SYMBOLS_##p)
DDSRT_FOREACH_WRAP (MAKE_DLSYM_EXTERNS, SEMICOLON, PLUGINS);

struct static_dlsym_table {
  const char *name;
  void (*f) (void);
};

#define DLSYM_TABLE_ENTRY(f) { #f, f }
#define MAKE_DLSYM_TABLE_2(libname, ...)                                \
  static const struct static_dlsym_table static_dlsym_table_##libname[] = { \
    DDSRT_FOREACH_B(DLSYM_TABLE_ENTRY, COMMA, __VA_ARGS__),                  \
    { NULL, NULL }                                                      \
  }

#define MAKE_DLSYM_TABLE_1(libname, ...) MAKE_DLSYM_TABLE_2(libname, __VA_ARGS__)
#define MAKE_DLSYM_TABLE(p) MAKE_DLSYM_TABLE_1(p, PLUGIN_SYMBOLS_##p)

DDSRT_FOREACH_WRAP (MAKE_DLSYM_TABLE, SEMICOLON, PLUGINS);

struct static_dlopen_table {
  const char *name;
  const struct static_dlsym_table *syms;
};

#define DLOPEN_TABLE_ENTRY(p) { #p, static_dlsym_table_##p }

static const struct static_dlopen_table static_dlopen_table[] = {
  DDSRT_FOREACH_WRAP(DLOPEN_TABLE_ENTRY, COMMA, PLUGINS),
  { NULL, NULL }
};

dds_return_t ddsrt_dlopen (const char *name, bool translate, ddsrt_dynlib_t *handle)
{
  for (size_t i = 0; static_dlopen_table[i].name; i++) {
    if (strcmp (static_dlopen_table[i].name, name) == 0) {
      *handle = (ddsrt_dynlib_t) static_dlopen_table[i].syms;
      return DDS_RETCODE_OK;
    }
  }
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlopen (name, translate, handle);
#else
  return DDS_RETCODE_UNSUPPORTED;
#endif
}

dds_return_t ddsrt_dlclose (ddsrt_dynlib_t handle)
{
  for (size_t i = 0; static_dlopen_table[i].name; i++)
    if (handle == (ddsrt_dynlib_t) static_dlopen_table[i].syms)
      return DDS_RETCODE_OK;
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlclose (handle);
#else
  return DDS_RETCODE_UNSUPPORTED
#endif
}

static dds_return_t fake_dlsym (ddsrt_dynlib_t handle, const char *symbol, void **address)
{
  const struct static_dlsym_table *t = (const struct static_dlsym_table *) handle;
  for (size_t i = 0; t[i].name; i++) {
    if (strcmp (t[i].name, symbol) == 0) {
      *address = (void *) t[i].f;
      return DDS_RETCODE_OK;
    }
  }
  return DDS_RETCODE_ERROR;
}

dds_return_t ddsrt_dlsym (ddsrt_dynlib_t handle, const char *symbol, void **address)
{
  for (size_t i = 0; static_dlopen_table[i].name; i++)
    if (handle == (ddsrt_dynlib_t) static_dlopen_table[i].syms)
      return fake_dlsym (handle, symbol, address);
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlsym (handle, symbol, address);
#else
  return DDS_RETCODE_UNSUPPORTED
#endif
}

dds_return_t ddsrt_dlerror (char *buf, size_t buflen)
{
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlerror (buf, buflen);
#else
  return DDS_RETCODE_UNSUPPORTED
#endif
}

#else

dds_return_t ddsrt_dlopen (const char *name, bool translate, ddsrt_dynlib_t *handle)
{
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlopen (name, translate, handle);
#else
  return DDS_RETCODE_UNSUPPORTED;
#endif
}

dds_return_t ddsrt_dlclose (ddsrt_dynlib_t handle)
{
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlclose (handle);
#else
  return DDS_RETCODE_UNSUPPORTED;
#endif
}

dds_return_t ddsrt_dlsym (ddsrt_dynlib_t handle, const char *symbol, void **address)
{
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlsym (handle, symbol, address);
#else
  return DDS_RETCODE_UNSUPPORTED;
#endif
}

dds_return_t ddsrt_dlerror (char *buf, size_t buflen)
{
#if DDSRT_HAVE_DYNLIB
  return ddsrt_platform_dlerror (buf, buflen);
#else
  return DDS_RETCODE_UNSUPPORTED;
#endif
}

#endif
