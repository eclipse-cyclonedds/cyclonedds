// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if WIN32
#include <Windows.h>
static const char sep[] = "/\\";
static const char lib[] = "cyclonedds";
static const char ext[] = "dll";
#define SUBPROCESS_POPEN _popen
#define SUBPROCESS_PCLOSE _pclose
#else
#include <dlfcn.h>
static const char sep[] = "/";
static const char lib[] = "libcyclonedds";
#if __APPLE__
static const char ext[] = "dylib";
#else
static const char ext[] = "so";
#endif
#define SUBPROCESS_POPEN popen
#define SUBPROCESS_PCLOSE pclose
#endif

#include "plugin.h"
#include "idl/heap.h"
#include "idl/string.h"
#include "idlc/generator.h"

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

static void liberror(char *buffer, size_t bufferlen)
{
  assert(buffer != NULL);
  assert(bufferlen > 0);
#if WIN32
  DWORD error = GetLastError();
  (void)FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS |
    FORMAT_MESSAGE_MAX_WIDTH_MASK,
    NULL,
    (DWORD)error,
    0,
    (LPTSTR)buffer,
    (DWORD)(bufferlen - 1),
    NULL);
  SetLastError(error);
#else
  strncpy(buffer, dlerror(), bufferlen - 1);
#endif
  buffer[bufferlen - 1] = 0; /* ensure final zero in all cases */
}

#define SUBPROCESS_PIPE_MEMORY_LIMIT 1024 * 1024
static int run_library_locator(const char *command, char **out_output) {
  size_t output_size = 0;
  size_t output_pt = 0;
  char *output = NULL;
  FILE *pipe;
  int ret = 0;
  bool success = true;
  int c;

  if ((pipe = SUBPROCESS_POPEN (command, "r")) == NULL) {
    // broken-pipe
    return -1;
  }

  while ((c = fgetc(pipe)) != EOF) {
    if (c == '\n' || c == '\r') {
      break;
    }

    if (output_pt == output_size) {
      output_size += 128;
      if (output_size > SUBPROCESS_PIPE_MEMORY_LIMIT) {
        success = false;
        break;
      }

      char* new = (char*) idl_realloc(output, output_size);
      if (!new) {
        success = false;
        break;
      }
      output = new;
    }

    output[output_pt++] = (char) c;
  }

  ret = SUBPROCESS_PCLOSE (pipe);

  if (success && output != NULL && ret == 0) {
    // ensure proper string termination (might be newline)
    output[output_pt] = '\0';
    *out_output = output;

    return 0;
  }

  // error
  if (output) {
    idl_free(output);
  }

  return -1;
}


extern const idlc_option_t** idlc_generator_options(void);
extern int idlc_generate(const idl_pstate_t *pstate, const idlc_generator_config_t *config);

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
    plugin->generator_options = &idlc_generator_options;
    plugin->generator_annotations = 0;
    plugin->generate = &idlc_generate;
    return 0;
  }

  /* special case for python generator
        The 'active' idlpy library is dependend on which python is active. Idlpy is installed
        as part of the cyclonedds python package and it can very well be that multiple installations
        are present on the system, by use of virtual environments, user installs and system installs.
        Sadly activating a python distribution does not set a any library loading paths. However,
        the python cyclonedds package has a __idlc__ module that prints the path when executed. The
        'python3' executable is always guaranteed to point to the active python so we always get the
        correct idlpy library.
   */
  if (idl_strcasecmp(lang, "py") == 0) {
    if (run_library_locator("python3 -m cyclonedds.__idlc__", &file) != 0) {
      return -1;
    }
    path = (const char*) file;
  }

  /* figure out if user passed library or language */
  else if ((sep[0] && strchr(lang, sep[0])) || (sep[1] && strchr(lang, sep[1]))) {
    path = lang; /* lang includes a directory separator, it is a path */
  } else if (len > extlen && strcmp(lang + (len - extlen), ext) == 0) {
    path = lang; /* lang terminates with extension of plugins (libs), it is a path */
  } else {
    int cnt;
    const char fmt[] = "%sidl%s.%s"; /* builds the library name based on 'lang' */
    cnt = snprintf(buf, sizeof(buf), fmt, lib, lang, ext);
    assert(cnt != -1);
    if ((size_t)cnt < sizeof(buf)) {
      path = (const char *)buf;
    } else if (!(file = idl_malloc((size_t)cnt+1))) {
      return -1;
    } else {
      cnt = snprintf(file, (size_t)cnt+1, fmt, lib, lang, ext);
      assert(cnt != -1);
      path = (const char *)file;
    }
  }

  /* open the library */
  handle = openlib(path);
  if (handle) {
    generate = loadsym(handle, "generate");
    if (generate) {
      plugin->handle = handle;
      plugin->generate = generate;
      plugin->generator_options = loadsym(handle, "generator_options");
      plugin->generator_annotations = loadsym(handle, "generator_annotations");
    } else {
      fprintf(stderr, "Symbol 'generate' not found in %s\n", path);
      closelib(handle);
    }
  }
  else {
    char errmsg[300];
    liberror(errmsg, sizeof(errmsg));
    fprintf(stderr, "Cannot load generator %s: %s\n", path, errmsg);
  }

  if (file) {
    idl_free(file);
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
