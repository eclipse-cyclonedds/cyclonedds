/*
 * Copyright(c) 2019 Jeroen Koekkoek
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
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dds/version.h"
#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsts/typetree.h"

#include "idl.h"
#include "mcpp_lib.h"
#include "mcpp_out.h"
#include "gen_c99.h"

#define IDLC_PREPROCESS (1<<0)
#define IDLC_COMPILE (1<<1)
#define IDLC_DEBUG_PREPROCESSOR (1<<2)
#define IDLC_DEBUG_PROCESSOR (1<<3)
/* FIXME: make more granular. e.g. parsser, directive parser, scaner, etc */

typedef struct {
  char *file; /* path of input file or "-" for STDIN */
  char *lang;
  int flags; /* preprocess and/or compile */
  /* (emulated) command line options for mcpp */
  int argc;
  char **argv;
} idlc_options_t;

/* mcpp does not accept userdata */
static int32_t retcode = 0;
static idlc_options_t opts;
static idl_processor_t proc;

static int idlc_putc(int chr, OUTDEST od);
static int idlc_puts(const char *str, OUTDEST od)
  ddsrt_nonnull((1));
static int idlc_printf(OUTDEST od, const char *str, ...)
  ddsrt_nonnull((2))
  ddsrt_attribute_format((printf, 2, 3));
static int32_t idlc_parse(ddsts_type_t **typeptr);

#define CHUNK (4096)

static int idlc_putn(const char *str, size_t len)
{
  assert(proc.state & IDL_WRITE);

  /* tokenize to free up space */
  if ((proc.buffer.size - proc.buffer.used) <= len) {
    if ((retcode = idl_parse(&proc)) == IDL_NEED_REFILL)
      retcode = 0;
    /* move non-tokenized data to start of buffer */
    proc.buffer.used =
      (uintptr_t)proc.scanner.limit - (uintptr_t)proc.scanner.cursor;
    memmove(proc.buffer.data, proc.scanner.cursor, proc.buffer.used);
    proc.scanner.cursor = proc.buffer.data;
    proc.scanner.limit = proc.scanner.cursor + proc.buffer.used;
  }

  if (retcode != 0)
    return -1;

  /* expand buffer if necessary */
  if ((proc.buffer.size - proc.buffer.used) <= len) {
    size_t size = proc.buffer.size + (((len / CHUNK) + 1) * CHUNK);
    char *buf = ddsrt_realloc(proc.buffer.data, size + 2 /* '\0' + '\0' */);
    if (buf == NULL) {
      retcode = IDL_MEMORY_EXHAUSTED;
      return -1;
    }
    /* update scanner location */
    proc.scanner.cursor = buf + (proc.scanner.cursor - proc.buffer.data);
    proc.scanner.limit = proc.scanner.cursor + proc.buffer.used;
    /* update input buffer */
    proc.buffer.data = buf;
    proc.buffer.size = size;
  }

  /* write to buffer */
  memcpy(proc.buffer.data + proc.buffer.used, str, len);
  proc.buffer.used += len;
  assert(proc.buffer.used <= proc.buffer.size);
  /* update scanner location */
  proc.scanner.limit = proc.buffer.data + proc.buffer.used;

  return 0;
}

static int idlc_putc(int chr, OUTDEST od)
{
  int ret = -1;
  char str[2] = { (char)chr, '\0' };

  switch (od) {
  case OUT:
    if (!(opts.flags & IDLC_COMPILE))
      ret = printf("%c", chr);
    else
      ret = idlc_putn(str, 1);
    break;
  case ERR:
    ret = fprintf(stderr, "%c", chr);
    break;
  case DBG:
    if (opts.flags & IDLC_DEBUG_PREPROCESSOR)
      ret = fprintf(stderr, "%c", chr);
    break;
  default:
    assert(0);
    break;
  }

  return ret < 0 ? -1 : ret;
}

static int idlc_puts(const char *str, OUTDEST od)
{
  int ret;
  size_t len = strlen(str);

  assert(str != NULL);
  assert(len <= INT_MAX);
  ret = (int)len;

  switch (od) {
  case OUT:
    if (!(opts.flags & IDLC_COMPILE))
      ret = printf("%s", str);
    else
      ret = idlc_putn(str, len);
    break;
  case ERR:
    ret = fprintf(stderr, "%s", str);
    break;
  case DBG:
    if (opts.flags & IDLC_DEBUG_PREPROCESSOR)
      ret = fprintf(stderr, "%s", str);
    break;
  default:
    assert(0);
    break;
  }

  return ret < 0 ? -1 : ret;
}

static int idlc_printf(OUTDEST od, const char *fmt, ...)
{
  int ret = -1;
  char *str = NULL;
  int len;
  va_list ap;

  assert(fmt != NULL);

  va_start(ap, fmt);
  if ((len = ddsrt_vasprintf(&str, fmt, ap)) < 0) { /* FIXME: optimize */
    retcode = IDL_MEMORY_EXHAUSTED;
    return -1;
  }
  va_end(ap);

  switch (od) {
  case OUT:
    if (!(opts.flags & IDLC_COMPILE))
      ret = printf("%s", str);
    else
      ret = idlc_putn(str, (size_t)len);
    break;
  case ERR:
    ret = fprintf(stderr, "%s", str);
    break;
  case DBG:
    if (opts.flags & IDLC_DEBUG_PREPROCESSOR)
      ret = fprintf(stderr, "%s", str);
    break;
  default:
    assert(0);
    break;
  }

  ddsrt_free(str);

  return ret < 0 ? -1 : ret;
}

int32_t idlc_parse(ddsts_type_t **typeptr)
{
  int32_t ret = 0;

  if(opts.flags & IDLC_COMPILE) {
    if ((ret = idl_processor_init(&proc)) != 0)
      return ret;
    assert(opts.file);
    if (strcmp(opts.file, "-") != 0)
      proc.scanner.position.file = (const char *)opts.file;
    proc.scanner.position.line = 1;
    proc.scanner.position.column = 1;
  }

  if (opts.flags & IDLC_PREPROCESS) {
    proc.flags |= IDL_WRITE;
    mcpp_set_out_func(&idlc_putc, &idlc_puts, &idlc_printf);
    if (mcpp_lib_main(opts.argc, opts.argv) == 0) {
      assert(!(opts.flags & IDLC_COMPILE) || retcode == 0);
    } else if (opts.flags & IDLC_COMPILE) {
      assert(retcode != 0);
      ret = retcode;
    }
    proc.flags &= ~IDL_WRITE;
  } else {
    FILE *fin;
    char buf[1024];
    size_t nrd;
    int nwr;

    if (strcmp(opts.file, "-") == 0) {
      fin = stdin;
    } else {
      DDSRT_WARNING_MSVC_OFF(4996)
      fin = fopen(opts.file, "rb");
      DDSRT_WARNING_MSVC_ON(4996)
    }

    if (fin == NULL) {
      switch (errno) {
        case ENOMEM:
          ret = IDL_MEMORY_EXHAUSTED;
          break;
        default:
          ret = IDL_READ_ERROR;
          break;
      }
    } else {
      while ((nrd = fread(buf, sizeof(buf), 1, fin)) > 0) {
        if ((nwr = idlc_putn(buf, nrd)) == -1) {
          ret = retcode;
          assert(ret != 0);
        }
        assert(nrd == (size_t)nwr);
      }
    }

    if (fin != stdin)
      fclose(fin);
  }

  if (ret == 0 && (opts.flags & IDLC_COMPILE)) {
    ret = idl_parse(&proc);
    assert(ret != IDL_NEED_REFILL);
    if (ret == 0)
      *typeptr = ddsts_context_take_root_type(proc.context);
  }

  idl_processor_fini(&proc);

  return ret;
}

static void
usage(const char *prog)
{
  fprintf(stderr, "Usage: %s FILE\n", prog);
}

static void
help(const char *prog)
{
  static const char fmt[] =
"Usage: %s [OPTIONS] FILE\n"
"Options:\n"
"  -d <component>       Display debug information for <component>(s)\n"
"                       Comma separate or use more than one -d option to\n"
"                       specify multiple components\n"
"                       Components: preprocessor, parser\n"
"  -D <macro>[=value]   Define <macro> to <value> (default:1)\n"
"  -E                   Preprocess only\n"
"  -h                   Display available options\n"
"  -I <directory>       Add <directory> to include search list\n"
"  -l <language>        Compile representation for <language>\n"
"  -S                   Compile only\n"
"  -v                   Display version information\n";

  printf(fmt, prog);
}

static void
version(const char *prog)
{
  printf("%s (Eclipse Cyclone DDS) %s\n", prog, DDS_VERSION);
}

int main(int argc, char *argv[])
{
  int opt;
  char *prog = argv[0];
  int32_t ret;
  ddsts_type_t *root = NULL;

  /* determine basename */
  for (char *sep = argv[0]; *sep; sep++) {
    if (*sep == '/' || *sep == '\\') {
      prog = sep + 1;
    }
  }

  opts.file = "-"; /* default to STDIN */
  opts.flags = IDLC_PREPROCESS | IDLC_COMPILE;
  opts.lang = "c";
  opts.argc = 0;
  opts.argv = ddsrt_calloc((unsigned int)argc + 6, sizeof(char *));
  if (opts.argv == NULL) {
    return EXIT_FAILURE;
  }

  opts.argv[opts.argc++] = argv[0];
  opts.argv[opts.argc++] = "-C"; /* keep comments */
  opts.argv[opts.argc++] = "-I-"; /* unset system include directories */
  opts.argv[opts.argc++] = "-k"; /* keep white space as is */
  opts.argv[opts.argc++] = "-N"; /* unset predefined macros */
  /* FIXME: mcpp option -K embeds macro notifications into comments to allow
            reconstruction of the original source position from the
            preprocessed output */

  /* parse command line options */
  while ((opt = getopt(argc, argv, "Cd:D:EhI:l:Sv")) != -1) {
    switch (opt) {
      case 'd':
        {
          char *tok, *str = optarg;
          while ((tok = ddsrt_strsep(&str, ",")) != NULL) {
            if (ddsrt_strcasecmp(tok, "preprocessor") == 0) {
              opts.flags |= IDLC_DEBUG_PREPROCESSOR;
            } else if (ddsrt_strcasecmp(tok, "parser") == 0) {
              opts.flags |= IDLC_DEBUG_PROCESSOR;
            }
          }
        }
        break;
      case 'D':
        opts.argv[opts.argc++] = "-D";
        opts.argv[opts.argc++] = optarg;
        break;
      case 'E':
        opts.flags &= ~IDLC_COMPILE;
        opts.flags |= IDLC_PREPROCESS;
        break;
      case 'h':
        help(prog);
        exit(EXIT_SUCCESS);
      case 'I':
        opts.argv[opts.argc++] = "-I";
        opts.argv[opts.argc++] = optarg;
        break;
      case 'l':
        opts.lang = optarg;
        break;
      case 'S':
        opts.flags &= ~IDLC_PREPROCESS;
        opts.flags |= IDLC_COMPILE;
        break;
      case 'v':
        version(prog);
        exit(EXIT_SUCCESS);
      case '?':
        usage(prog);
        exit(EXIT_FAILURE);
    }
  }

  if (optind == argc) { /* default to STDIN */
    assert(opts.file != NULL);
    assert(strcmp(opts.file, "-") == 0);
  } else {
    opts.file = argv[optind];
  }

  opts.argv[opts.argc++] = opts.file;

  if ((ret = idlc_parse(&root)) == 0 && (opts.flags & IDLC_COMPILE)) {
    assert(root != NULL);
    assert(ddsrt_strcasecmp(opts.lang, "c") == 0);
    ddsts_generate_C99(opts.file, root);
    ddsts_free_type(root);
  }

  ddsrt_free(opts.argv);

  return EXIT_SUCCESS;
}
