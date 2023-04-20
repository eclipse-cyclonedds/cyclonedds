// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dds/features.h"
#include "dds/cdr/dds_cdrstream.h"
#include "idl/heap.h"
#include "idl/misc.h"
#include "idl/tree.h"
#include "idl/string.h"
#include "idl/processor.h"
#include "idl/version.h"
#include "idl/stream.h"

#include "mcpp_lib.h"
#include "mcpp_out.h"

#include "idlc/generator.h"
#include "generator.h"
#include "plugin.h"
#include "options.h"
#include "descriptor_type_meta.h"
#include "file.h"

#if 0
#define IDLC_DEBUG_PREPROCESSOR (1u<<2)
#define IDLC_DEBUG_SCANNER (1u<<3)
#define IDLC_DEBUG_PARSER (1u<<4)
#endif

#define DISABLE_WARNING_CHSZ 10
struct idlc_disable_warning_list
{
  idl_warning_t *list;
  size_t size;
  size_t count;
};

static struct {
  char *file; /* path of input file or "-" for STDIN */
  const char *lang;
  const char *output_dir; /* path to write completed files */
  const char* base_dir; /* Path to start reconstruction of dir structure */
  int compile;
  int preprocess;
  int keylist;
  int case_sensitive;
  int default_extensibility;
  bool default_nested;
  struct idlc_disable_warning_list disable_warnings;
  bool werror;
  int help;
  int version;
#ifdef DDS_HAS_TYPE_DISCOVERY
  int no_type_info;
#endif
  /* (emulated) command line options for mcpp */
  int argc;
  char **argv;
} config;

/* mcpp does not accept userdata */
static idl_retcode_t retcode = IDL_RETCODE_OK;
static idl_pstate_t *pstate = NULL;

static bool has_warnings = false;

static int idlc_putc(int chr, MCPP_OUTDEST od);
static int idlc_puts(const char *str, MCPP_OUTDEST od);
static int idlc_printf(MCPP_OUTDEST od, const char *str, ...);

#define CHUNK (4096)

static int idlc_putn(const char *str, size_t len)
{
  assert(pstate->config.flags & IDL_WRITE);

  /* tokenize to free up space */
  if (pstate->buffer.data && (pstate->buffer.size - pstate->buffer.used) <= len) {
    if ((retcode = idl_parse(pstate)) == IDL_RETCODE_NEED_REFILL)
      retcode = IDL_RETCODE_OK;
    /* move non-tokenized data to start of buffer */
    pstate->buffer.used =
      (uintptr_t)pstate->scanner.limit - (uintptr_t)pstate->scanner.cursor;
    memmove(pstate->buffer.data, pstate->scanner.cursor, pstate->buffer.used);
    pstate->scanner.cursor = pstate->buffer.data;
    pstate->scanner.limit = pstate->scanner.cursor + pstate->buffer.used;
  }

  if (retcode != IDL_RETCODE_OK)
    return -1;

  /* expand buffer if necessary */
  if (pstate->buffer.data == NULL || (pstate->buffer.size - pstate->buffer.used) <= len) {
    size_t size = pstate->buffer.size + (((len / CHUNK) + 1) * CHUNK);
    char *buf = idl_realloc(pstate->buffer.data, size + 2 /* '\0' + '\0' */);
    if (buf == NULL) {
      retcode = IDL_RETCODE_NO_MEMORY;
      return -1;
    }
    /* update scanner location */
    pstate->scanner.cursor = buf + (pstate->scanner.cursor - pstate->buffer.data);
    pstate->scanner.limit = buf + pstate->buffer.used;
    /* update input buffer */
    pstate->buffer.data = buf;
    pstate->buffer.size = size;
  }

  /* write to buffer */
  memcpy(pstate->buffer.data + pstate->buffer.used, str, len);
  pstate->buffer.used += len;
  assert(pstate->buffer.used <= pstate->buffer.size);
  /* update scanner location */
  pstate->scanner.limit = pstate->buffer.data + pstate->buffer.used;

  return 0;
}

static int idlc_putc(int chr, MCPP_OUTDEST od)
{
  int ret = -1;
  char str[2] = { (char)chr, '\0' };

  switch (od) {
  case MCPP_OUT:
    if (!(config.compile))
      ret = printf("%c", chr);
    else
      ret = idlc_putn(str, 1);
    break;
  case MCPP_ERR:
    ret = fprintf(stderr, "%c", chr);
    break;
  case MCPP_DBG:
#if 0
    if (config.flags & IDLC_DEBUG_PREPROCESSOR)
#endif
      ret = fprintf(stderr, "%c", chr);
    break;
  default:
    assert(0);
    break;
  }

  return ret < 0 ? -1 : ret;
}

static int idlc_puts(const char *str, MCPP_OUTDEST od)
{
  int ret = -1;
  size_t len = strlen(str);

  assert(str != NULL);
  assert(len <= INT_MAX);

  switch (od) {
  case MCPP_OUT:
    if (!(config.compile))
      ret = printf("%s", str);
    else
      ret = idlc_putn(str, len);
    break;
  case MCPP_ERR:
    ret = fprintf(stderr, "%s", str);
    break;
  case MCPP_DBG:
#if 0
    if (config.flags & IDLC_DEBUG_PREPROCESSOR)
#endif
      ret = fprintf(stderr, "%s", str);
    break;
  default:
    assert(0);
    break;
  }

  return ret < 0 ? -1 : (int)len;
}

static int idlc_printf(MCPP_OUTDEST od, const char *fmt, ...)
{
  int ret = -1;
  char *str = NULL;
  int len;
  va_list ap;

  assert(fmt != NULL);

  va_start(ap, fmt);
  len = idl_vasprintf(&str, fmt, ap);
  va_end(ap);

  if (len < 0) {
    retcode = IDL_RETCODE_NO_MEMORY;
    return -1;
  }

  switch (od) {
  case MCPP_OUT:
    if (!(config.compile))
      ret = printf("%s", str);
    else
      ret = idlc_putn(str, (size_t)len);
    break;
  case MCPP_ERR:
    ret = fprintf(stderr, "%s", str);
    break;
  case MCPP_DBG:
#if 0
    if (config.flags & IDLC_DEBUG_PREPROCESSOR)
#endif
      ret = fprintf(stderr, "%s", str);
    break;
  default:
    assert(0);
    break;
  }

  idl_free(str);

  return ret < 0 ? -1 : ret;
}

static bool track_warning (idl_warning_t warning)
{
  for (size_t n = 0; n < config.disable_warnings.count; n++)
    if (config.disable_warnings.list[n] == warning)
      return false;

  has_warnings = true;
  return true;
}

static idl_retcode_t figure_file(idl_file_t **filep)
{
  idl_file_t *file;
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  char *dir = NULL, *abs = NULL, *norm = NULL;

  if (!(file = idl_malloc(sizeof(*file))))
    goto err_file;
  if (idl_isabsolute(config.file)) {
    if ((ret = idl_normalize_path(config.file, &norm)) < 0)
      goto err_norm;
  } else {
    if (idl_current_path(&dir) < 0)
      goto err_dir;
    if (idl_asprintf(&abs, "%s/%s", dir, config.file) == -1)
      goto err_abs;
    if ((ret = idl_normalize_path(abs, &norm)) < 0)
      goto err_norm;
    idl_free(abs);
    idl_free(dir);
  }
  file->next = NULL;
  file->name = norm;
  *filep = file;
  return IDL_RETCODE_OK;
err_norm:
  if (abs) idl_free(abs);
err_abs:
  if (dir) idl_free(dir);
err_dir:
  idl_free(file);
err_file:
  return ret;
}

static idl_file_t *idlc_parse_make_file (const char *file)
{
  idl_file_t *f;
  if ((f = idl_malloc(sizeof(*f))) == NULL)
    return NULL;
  f->next = NULL;
  if (!(f->name = idl_strdup(file))) {
    idl_free(f);
    return NULL;
  }
  return f;
}

static idl_source_t *idlc_parse_make_source (idl_position_t *position, idl_file_t *paths, idl_file_t *files)
{
  idl_source_t *source;
  if ((source = idl_malloc (sizeof (*source))) == NULL)
    return NULL;
  source->parent = NULL;
  source->previous = source->next = NULL;
  source->includes = NULL;
  source->additional_directory = false;
  source->path = paths;
  source->file = files;
  position->source = source;
  position->file = (const idl_file_t *) files;
  position->line = 1;
  position->column = 1;
  return source;
}

static idl_retcode_t idlc_parse(const idl_builtin_annotation_t ** generator_annotations)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  uint32_t flags = IDL_FLAG_EXTENDED_DATA_TYPES |
                   IDL_FLAG_ANONYMOUS_TYPES |
                   IDL_FLAG_ANNOTATIONS;

  if(config.case_sensitive)
    flags |= IDL_FLAG_CASE_SENSITIVE;

  if(config.compile) {
    if ((ret = idl_create_pstate(flags, generator_annotations == NULL ? NULL : *generator_annotations, &pstate))) {
      pstate = NULL;
      return ret;
    }
    assert(config.file);
    if (strcmp(config.file, "-") != 0 && (ret = figure_file(&pstate->paths)) != 0) {
      if (ret == IDL_RETCODE_NO_ENTRY)
        idl_error(pstate, NULL, "could not open file at location: %s", config.file);
      idl_delete_pstate(pstate);
      pstate = NULL;
      return ret;
    }
#if __GNUC__ >= 12
    IDL_WARNING_GNUC_OFF(analyzer-malloc-leak)
#endif
    if ((pstate->files = idlc_parse_make_file (config.file)) == NULL)
    {
      idl_delete_pstate(pstate);
      pstate = NULL;
      return IDL_RETCODE_NO_MEMORY;
    }
    if ((pstate->sources = idlc_parse_make_source (&pstate->scanner.position, pstate->paths, pstate->files)) == NULL)
    {
      idl_delete_pstate(pstate);
      pstate = NULL;
      return IDL_RETCODE_NO_MEMORY;
    }
#if __GNUC__ >= 12
    IDL_WARNING_GNUC_ON(analyzer-malloc-leak)
#endif
    pstate->config.flags |= IDL_WRITE;
    pstate->config.default_extensibility = config.default_extensibility;
    pstate->config.default_nested = config.default_nested;
    pstate->track_warning = &track_warning;
  }

  if (config.preprocess) {
    if (pstate) {
      assert(config.compile);
      pstate->config.flags |= IDL_WRITE;
    }
    mcpp_set_out_func(&idlc_putc, &idlc_puts, &idlc_printf);
    if (mcpp_lib_main(config.argc, config.argv) == 0) {
      assert(!config.compile || retcode == IDL_RETCODE_OK);
    } else if (config.compile) {
      /* retcode is not set on preprocessor error */
      ret = retcode ? retcode : IDL_RETCODE_SYNTAX_ERROR;
    }
    if (pstate) {
      pstate->config.flags &= ~IDL_WRITE;
    }
  } else {
    FILE *fin = NULL;
    char buf[1024];
    size_t nrd;
    int nwr;

    if (strcmp(config.file, "-") == 0)
      fin = stdin;
    else
      fin = idl_fopen(config.file, "rb");

    if (!fin) {
      if (errno == ENOMEM)
        ret = IDL_RETCODE_NO_MEMORY;
      else if (errno == EACCES)
        ret = IDL_RETCODE_NO_ACCESS;
      else
        ret = IDL_RETCODE_NO_ENTRY;
    } else {
      while ((nrd = fread(buf, sizeof(buf), 1, fin)) > 0) {
#if __GNUC__ >= 12
        IDL_WARNING_GNUC_OFF(analyzer-malloc-leak)
#endif
        if ((nwr = idlc_putn(buf, nrd)) == -1) {
          ret = retcode;
          assert(ret != 0);
        }
#if __GNUC__ >= 12
        IDL_WARNING_GNUC_ON(analyzer-malloc-leak)
#endif
        assert(nrd == (size_t)nwr);
      }
      if (fin != stdin)
        fclose(fin);
    }
  }

  if (ret == IDL_RETCODE_OK && config.compile) {
    assert(pstate);
    ret = idl_parse(pstate);
    assert(ret != IDL_RETCODE_NEED_REFILL);
    if (ret == IDL_RETCODE_OK) {
      if (config.keylist) {
        pstate->config.flags |= IDL_FLAG_KEYLIST;
      } else if (pstate->keylists && pstate->annotations) {
        idl_error(pstate, NULL,
          "Translation unit contains both annotations and #pragma keylist "
          "directives, use only one of these or use the -f keylist command "
          "line option to force using only #pragma keylist and ignore "
          "annotations");
        ret = IDL_RETCODE_SYNTAX_ERROR;
      } else if (pstate->keylists) {
        pstate->config.flags |= IDL_FLAG_KEYLIST;
      }
    }
  }

  if (ret != IDL_RETCODE_OK)
  {
    idl_delete_pstate(pstate);
    pstate = NULL;
  }
  return ret;
}

#if 0
static int set_debug(const idlc_option_t *opt, const char *optarg)
{
  (void)opt;
  for (size_t off=0, pos=0; ; pos++) {
    if (optarg[pos] == '\0' || optarg[pos] == ',') {
      size_t len = pos - off;
      if (strncmp(optarg + off, "preprocessor", len) == 0)
        config.flags |= IDLC_DEBUG_PREPROCESSOR;
      else if (strncmp(optarg + off, "scanner", len) == 0)
        config.flags |= IDLC_DEBUG_SCANNER;
      else if (strncmp(optarg + off, "parser", len) == 0)
        config.flags |= IDLC_DEBUG_PARSER;
      else if (len)
        return IDLC_BAD_ARGUMENT;
      if (optarg[pos] == '\0')
        break;
      off = pos + 1;
    }
  }
  return 0;
}
#endif

static int set_compile_only(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  (void)arg;
  config.compile = 1;
  config.preprocess = 1;
  return 0;
}

static int set_preprocess_only(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  (void)arg;
  config.compile = 0;
  config.preprocess = 1;
  return 0;
}

static int config_default_extensibility(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  if (strcmp(arg, "final") == 0)
    config.default_extensibility = IDL_FINAL;
  else if (strcmp(arg, "appendable") == 0)
    config.default_extensibility = IDL_APPENDABLE;
  else if (strcmp(arg, "mutable") == 0)
    config.default_extensibility = IDL_MUTABLE;
  else
    return IDLC_BAD_ARGUMENT;
  return 0;
}

static int config_default_nested(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  if (strcmp(arg, "true") == 0)
    config.default_nested = true;
  else if (strcmp(arg, "false") == 0)
    config.default_nested = false;
  else
    return IDLC_BAD_ARGUMENT;
  return 0;
}

static int add_disable_warning(idl_warning_t warning)
{
  if (config.disable_warnings.count == config.disable_warnings.size) {
    config.disable_warnings.size += DISABLE_WARNING_CHSZ;
    idl_warning_t *tmp = idl_realloc(config.disable_warnings.list, config.disable_warnings.size * sizeof(*config.disable_warnings.list));
    if (!tmp)
      return IDLC_NO_MEMORY;
    config.disable_warnings.list = tmp;
  }
  config.disable_warnings.list[config.disable_warnings.count++] = warning;
  return 0;
}

static int config_warning(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  if (strcmp(arg, "no-implicit-extensibility") == 0)
    add_disable_warning(IDL_WARN_IMPLICIT_EXTENSIBILITY);
  else if (strcmp(arg, "no-extra-token-directive") == 0)
    add_disable_warning(IDL_WARN_EXTRA_TOKEN_DIRECTIVE);
  else if (strcmp(arg, "no-unknown_escape_seq") == 0)
    add_disable_warning(IDL_WARN_UNKNOWN_ESCAPE_SEQ);
  else if (strcmp(arg, "no-inherit-appendable") == 0)
    add_disable_warning(IDL_WARN_INHERIT_APPENDABLE);
  else if (strcmp(arg, "no-enum-consecutive") == 0)
    add_disable_warning(IDL_WARN_ENUM_CONSECUTIVE);
  else if (strcmp(arg, "no-unsupported-annotations") == 0)
    add_disable_warning(IDL_WARN_UNSUPPORTED_ANNOTATIONS);
  else if (strcmp(arg, "error") == 0)
    config.werror = true;
  else
    return IDLC_BAD_ARGUMENT;
  return 0;
}

static int add_include(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  config.argv[config.argc++] = "-I";
  config.argv[config.argc++] = (char*)arg;
  return 0;
}

static int add_macro(const idlc_option_t *opt, const char *arg)
{
  (void)opt;
  config.argv[config.argc++] = "-D";
  config.argv[config.argc++] = (char*)arg;
  return 0;
}

  /* FIXME: introduce compatibility options
   * -e(xtension) with e.g. embedded-struct-def. The -e flags can also be used
   *  to enable/disable building blocks from IDL 4.x.
   * -s with e.g. 3.5 and 4.0 to enable everything allowed in the specific IDL
   *  specification.
   */
static const idlc_option_t *compopts[] = {
#if 0
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_debug }, 'd', "", "<component>",
    "Display debug information for <components>(s). Comma separate or use "
    "more than one -d option to specify multiple components.\n"
    "Components: preprocessor, scanner, parser." },
#endif
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_compile_only }, 'S', "", "",
    "Compile only." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &set_preprocess_only }, 'E', "", NULL,
    "Preprocess only."},
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &config.keylist }, 'f', "keylist", "",
    "Force use of #pragma keylist directive even if annotations occur "
    "in the translation unit." },
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &config.case_sensitive }, 'f', "case-sensitive", "",
    "Switch to case-sensitive mode of operation. e.g. to allow constructed "
    "entities to contain fields that differ only in case." },
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &config.help }, 'h', "", "",
    "Display available options." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &add_include }, 'I', "", "<directory>",
    "Add <directory> to include search list." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &add_macro }, 'D', "", "<macro>[=value]",
    "Define <macro> to <value> (default:1)." },
  &(idlc_option_t){
    IDLC_STRING, { .string = &config.lang }, 'l', "", "<language>",
    "Compile representation for <language>. (default:c)." },
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &config.version }, 'v', "", "",
    "Display version information." },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &config_default_extensibility }, 'x', "", "<extensibility>",
    "Set the default extensibility that is used in case no extensibility"
    "is set on a type. Possible values are final, appendable and mutable. "
    "(default: final)" },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &config_default_nested }, 'n', "", "<nested>",
    "Set the default nestedness that is used in the absence of nestedness specifiers on a type "
    "(@topic/nested), or other specifiers in its hierarchy (@default_nestedness on modules), "
    "with an unset nestedness implicitly being false. Possible values for this option are: true, "
    "false (default: true). " },
  &(idlc_option_t){
    IDLC_FUNCTION, { .function = &config_warning }, 'W', "", "<[no-]warning>",
    "Enable or disable warnings. Possible values are: -Wno-implicit-extensibility, "
    "-Wno-extra-token-directive, -Wno-unknown_escape_seq, -Wno-inherit-appendable, "
    "-Wno-eof-newline, -Wno-enum-consecutive. Use -Werror to make all warnings into "
    "errors. " },
  &(idlc_option_t){
    IDLC_STRING, { .string = &config.output_dir }, 'o', "", "<directory>",
    "Set the output directory for compiled files." },
  &(idlc_option_t){
    IDLC_STRING, { .string = &config.base_dir }, 'b', "", "<directory>",
    "Enable directory reconstruction starting from <base_dir> "
    "Attempts to recreate directory structure matching input. "
    "Use without the -o option will compile IDL files in-place. "
    "Fail-silent if root_dir is not a parent of input" },
#ifdef DDS_HAS_TYPE_DISCOVERY
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &config.no_type_info }, 't', "", "",
    "Don't generate type information in the topic descriptor" },
#endif
  NULL
};

static void print_version(const char *prog)
{
  printf("%s (Eclipse Cyclone DDS) %s\n", prog, IDL_VERSION);
}

static const char *figure_language(int argc, char **argv)
{
  const char *lang = "c";

  for (int i=1; i < argc; ) {
    if (argv[i][0] != '-' || argv[i][1] == '\0')
      break;
    if (strcmp(argv[i], "--") == 0)
      break;
    if (argv[i][1] == 'l') {
      if (argv[i][2] != '\0')
        lang = &argv[i][2];
      else if (++i < argc)
        lang = &argv[i][0];
      break;
    } else if (argv[i++][2] == '\0') {
      /* assume argument if not option */
      i += (i < argc && argv[i][0] != '-');
    }
  }

  return lang;
}

#define xstr(s) str(s)
#define str(s) #s

int main(int argc, char *argv[])
{
  int exit_code = EXIT_FAILURE;
  const char *prog = argv[0];
  const char *lang;
  const idl_builtin_annotation_t ** generator_annotations;
  idlc_generator_plugin_t gen;
  idlc_option_t **opts = NULL;
  const idlc_option_t **genopts = NULL;
  size_t nopts = 0, ncompopts = 0, ngenopts = 0;

  for (const char *sep = argv[0]; *sep; sep++) {
    if (idl_isseparator(*sep))
      prog = sep + 1;
  }

  config.compile = 1;
  config.preprocess = 1;
  config.default_extensibility = IDL_DEFAULT_EXTENSIBILITY_UNDEFINED;
  config.default_nested = false;
  config.disable_warnings.list = NULL;
  config.disable_warnings.size = 0;
  config.disable_warnings.count = 0;
  config.werror = false;
#ifdef DDS_HAS_TYPE_DISCOVERY
  config.no_type_info = 0;
#endif

  /* determine which generator to use */
  lang = figure_language(argc, argv);
  memset(&gen, 0, sizeof(gen));
  if (idlc_load_generator(&gen, lang) == -1)
    fprintf(stderr, "%s: cannot load generator %s\n", prog, lang);

  config.argc = 0;
  if (!(config.argv = idl_calloc((size_t)argc + 7, sizeof(config.argv[0]))))
    goto err_argv;

  config.argv[config.argc++] = argv[0];
  config.argv[config.argc++] = "-C"; /* keep comments */
#if 0
  config.argv[config.argc++] = "-I-"; /* unset system include directories */
#endif
  config.argv[config.argc++] = "-k"; /* keep white space as is */
  config.argv[config.argc++] = "-N"; /* unset predefined macros */
  /* define __IDLC__, __IDLC_MINOR__ and __IDLC_PATCHLEVEL__ so that sections
     in a file can be enabled or disabled based on preprocessor macros */
  config.argv[config.argc++] = "-D__IDLC__=" xstr(IDL_VERSION_MAJOR);
  config.argv[config.argc++] = "-D__IDLC_MINOR__=" xstr(IDL_VERSION_MINOR);
  config.argv[config.argc++] = "-D__IDLC_PATCHLEVEL__=" xstr(IDL_VERSION_PATCH);
  /* parse command line options */
  ncompopts = (sizeof(compopts)/sizeof(compopts[0])) - 1;
  if (gen.generator_options) {
    genopts = gen.generator_options();
    for (; genopts[ngenopts]; ngenopts++) ;
  }
  nopts = ncompopts + ngenopts;
  if (!(opts = idl_calloc(nopts + 1, sizeof(opts[0]))))
    goto err_alloc_opts;
  memcpy(opts, compopts, ncompopts * sizeof(opts[0]));
  if (ngenopts)
    memcpy(opts+ncompopts, genopts, ngenopts * sizeof(opts[0]));
  opts[nopts] = NULL;

  switch (parse_options(argc, argv, opts)) {
    case 0:
      break;
    case IDLC_BAD_INPUT:
      fprintf(stderr, "%s: conflicting options in generator %s\n", prog, lang);
      /* fall through */
    default:
      print_usage(prog, "[OPTIONS] FILE");
      goto err_parse_opts;
  }

  if (config.help) {
    print_help(prog, "[OPTIONS] FILE", opts);
  } else if (config.version) {
    print_version(prog);
  } else {
    idl_retcode_t ret;
    if (optind != (argc - 1)) {
      print_usage(prog, "[OPTIONS] FILE");
      goto err_parse_opts;
    }
    config.file = argv[optind];
    config.argv[config.argc++] = config.file;

    if (gen.generator_annotations) {
      generator_annotations = gen.generator_annotations();
    } else {
      generator_annotations = NULL;
    }

    if ((ret = idlc_parse(generator_annotations))) {
      /* assume other errors are reported by processor */
      if (ret == IDL_RETCODE_NO_MEMORY)
        fprintf(stderr, "Out of memory\n");
      goto err_parse;
    } else if (config.compile) {
      idlc_generator_config_t generator_config;
      memset(&generator_config, 0, sizeof(generator_config));

      // Duplicate/Untaint the output dir to keep header guards neat
      if(config.output_dir) {
        if(!(generator_config.output_dir = idl_strdup(config.output_dir)))
          goto err_generate;
        if(idl_untaint_path(generator_config.output_dir) < 0)
          goto err_generate;
      }
      // Root dir must be normalized because relativity comparison will be done
      if(config.base_dir) {
        if(idl_normalize_path(config.base_dir, &generator_config.base_dir) < 0)
          goto err_generate;
      }
#ifdef DDS_HAS_TYPE_DISCOVERY
      if(!config.no_type_info)
        generator_config.generate_type_info = true;
      generator_config.generate_typeinfo_typemap = generate_type_meta_ser;
#endif // DDS_HAS_TYPE_DISCOVERY
      if (gen.generate)
        ret = gen.generate(pstate, &generator_config);

      if(generator_config.output_dir)
        idl_free(generator_config.output_dir);
      if(generator_config.base_dir)
        idl_free(generator_config.base_dir);
      if (ret) {
        fprintf(stderr, "Failed to compile '%s'\n", config.file);
        goto err_generate;
      }
    }
  }
  exit_code = (has_warnings && config.werror) ? EXIT_FAILURE : EXIT_SUCCESS;

err_generate:
err_parse:
err_parse_opts:
  idl_free(opts);
err_alloc_opts:
  if (config.disable_warnings.list)
    idl_free(config.disable_warnings.list);
  idl_free(config.argv);
err_argv:
  idl_delete_pstate(pstate);
  return exit_code;
}
