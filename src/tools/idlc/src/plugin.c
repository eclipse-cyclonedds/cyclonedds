/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WIN32
#include <Windows.h>
static const char sep[] = "/\\";
static const char lib[] = "";
static const char ext[] = "dll";
#else
#include <dlfcn.h>
static const char sep[] = "/";
static const char lib[] = "lib";
#if __APPLE__
static const char ext[] = "dylib";
#else
static const char ext[] = "so";
#endif
#endif

#include "plugin.h"
#include "idl/string.h"

static size_t extlen = sizeof(ext) - 1;

static void *openlib(const char *filename)
{
#if WIN32
  return (void *)LoadLibrary(filename);
#else
  return dlopen(filename, RTLD_GLOBAL | RTLD_NOW);
#endif
}

static void closelib(void *handle)
{
#if WIN32
  (void)FreeLibrary((HMODULE)handle);
#else
  (void)dlclose(handle);
#endif
}

static void *loadsym(void *handle, const char *symbol)
{
#if WIN32
  return (void *)GetProcAddress((HMODULE)handle, symbol);
#else
  return dlsym(handle, symbol);
#endif
}

extern int idlc_generate(const idl_pstate_t *pstate);

int32_t
idlc_load_generator(idlc_generator_plugin_t *plugin, const char *lang)
{
  char buf[64], *file = NULL;
  const char *path;
  size_t len = strlen(lang);
  void *handle = NULL;
  idlc_generate_t generate = 0;

  /* short-circuit on builtin generator */
  if (idl_strcasecmp(lang, "C") == 0) {
    plugin->handle = NULL;
    plugin->generator_options = 0;
    plugin->generator_annotations = 0;
    plugin->generate = &idlc_generate;
    return 0;
  }

  /* figure out if user passed library or language */
  if ((sep[0] && strchr(lang, sep[0])) || (sep[1] && strchr(lang, sep[1]))) {
    path = lang;
  } else if (len > extlen && strcmp(lang + (len - extlen), ext) == 0) {
    path = lang;
  } else {
    int cnt;
    const char fmt[] = "%sidl%s.%s";
    cnt = snprintf(buf, sizeof(buf), fmt, lib, lang, ext);
    assert(cnt != -1);
    if ((size_t)cnt <= sizeof(buf)) {
      path = (const char *)buf;
    } else if (!(file = malloc((size_t)cnt+1))) {
      return -1;
    } else {
      cnt = snprintf(file, (size_t)cnt+1, fmt, lib, lang, ext);
      assert(cnt != -1);
      path = (const char *)file;
    }
  }

  if ((handle = openlib(path)) || (lang != path && (handle = openlib(lang)))) {
    generate = loadsym(handle, "generate");
    if (generate) {
      plugin->handle = handle;
      plugin->generate = generate;
      plugin->generator_options = loadsym(handle, "generator_options");
      plugin->generator_annotations = loadsym(handle, "generator_annotations");
    } else {
      closelib(handle);
    }
  }

  if (file) {
    free(file);
  }

  return (handle && generate) ? 0 : -1;
}

void idlc_unload_generator(idlc_generator_plugin_t *plugin)
{
  if (!plugin || !plugin->handle)
    return;
  closelib(plugin->handle);
  plugin->handle = NULL;
  plugin->generator_options = 0;
  plugin->generator_annotations = 0;
  plugin->generate = 0;
}
