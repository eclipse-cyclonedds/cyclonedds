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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "generator.h"

#include "idl/file.h"
#include "idl/heap.h"
#include "idl/retcode.h"
#include "idl/stream.h"
#include "idl/string.h"
#include "idl/version.h"
#include "idl/processor.h"
#include "idl/print.h"
#include "idlc/generator.h"

const char *export_macro = NULL;
const char *header_guard_prefix = "DDSC_";
int generate_cdrstream_desc = 0;

static int print_base_type(
  char *str, size_t size, const void *node, void *user_data)
{
  const char *type;

  (void)user_data;

  switch (idl_type(node)) {
    case IDL_BOOL:    type = "bool";        break;
    case IDL_CHAR:    type = "char";        break;
    case IDL_INT8:    type = "int8_t";      break;
    case IDL_OCTET:
    case IDL_UINT8:   type = "uint8_t";     break;
    case IDL_SHORT:
    case IDL_INT16:   type = "int16_t";     break;
    case IDL_USHORT:
    case IDL_UINT16:  type = "uint16_t";    break;
    case IDL_LONG:
    case IDL_INT32:   type = "int32_t";     break;
    case IDL_ULONG:
    case IDL_UINT32:  type = "uint32_t";    break;
    case IDL_LLONG:
    case IDL_INT64:   type = "int64_t";     break;
    case IDL_ULLONG:
    case IDL_UINT64:  type = "uint64_t";    break;
    case IDL_FLOAT:   type = "float";       break;
    case IDL_DOUBLE:  type = "double";      break;
    case IDL_LDOUBLE: type = "long double"; break;
    case IDL_STRING:  type = "char";        break;
    default:
      abort();
  }

  return idl_snprintf(str, size, "%s", type);
}

static void copy(
  char *dest, size_t off, size_t size, const char *src, size_t len)
{
  size_t cnt;

  if (off >= size)
    return;
  cnt = size - off;
  cnt = cnt > len ? len : cnt;
  memmove(dest+off, src, cnt);
}

static int print_templ_type(
  char *str, size_t size, const void *node, void *user_data)
{
  const idl_type_spec_t *type_spec;
  char dims[32], *name = NULL;
  const char *seg, *type;
  size_t cnt, len = 0, seq = 0;

  (void)user_data;
  if (idl_type(node) == IDL_STRING)
    return idl_snprintf(str, size, "char");
  /* sequences require a little magic */
  assert(idl_type(node) == IDL_SEQUENCE);

  dims[0] = '\0';
  type_spec = idl_type_spec(node);
  for (; idl_is_sequence(type_spec); type_spec = idl_type_spec(type_spec))
    seq++;

  if (idl_is_base_type(type_spec) || idl_is_string(type_spec)) {
    switch (idl_type(type_spec)) {
      case IDL_BOOL:     type = "bool";                break;
      case IDL_CHAR:     type = "char";                break;
      case IDL_INT8:     type = "int8";                break;
      case IDL_OCTET:    type = "octet";               break;
      case IDL_UINT8:    type = "uint8";               break;
      case IDL_SHORT:    type = "short";               break;
      case IDL_INT16:    type = "int16";               break;
      case IDL_USHORT:   type = "unsigned_short";      break;
      case IDL_UINT16:   type = "uint16";              break;
      case IDL_LONG:     type = "long";                break;
      case IDL_INT32:    type = "int32";               break;
      case IDL_ULONG:    type = "unsigned_long";       break;
      case IDL_UINT32:   type = "uint32";              break;
      case IDL_LLONG:    type = "long_long";           break;
      case IDL_INT64:    type = "int64";               break;
      case IDL_ULLONG:   type = "unsigned_long_long";  break;
      case IDL_UINT64:   type = "uint64";              break;
      case IDL_FLOAT:    type = "float";               break;
      case IDL_DOUBLE:   type = "double";              break;
      case IDL_LDOUBLE:  type = "long_double";         break;
      case IDL_STRING:
        if (idl_is_bounded(type_spec))
          idl_snprintf(dims, sizeof(dims), "%"PRIu32, idl_bound(type_spec));
        type = "string";
        break;
      default:
        abort();
    }
  } else {
    if (IDL_PRINT(&name, print_type, type_spec) < 0)
      return -1;
    type = name;
  }

  seg = "dds_sequence_";
  cnt = strlen(seg);
  copy(str, len, size, seg, cnt);
  len += cnt;
  seg = "sequence_";
  cnt = strlen(seg);
  for (; seq; len += cnt, seq--)
    copy(str, len, size, seg, cnt);
  cnt = strlen(type);
  copy(str, len, size, type, cnt);
  len += cnt;
  cnt = strlen(dims);
  copy(str, len, size, dims, cnt);
  len += cnt;
  str[ (size > len ? len : size - 1) ] = '\0';
  if (name)
    idl_free(name);
  return (int)len;
}

static int print_decl_type(
  char *str, size_t size, const void *node, void *user_data)
{
  const char *ident, *sep = user_data;
  size_t cnt, off, len = 0;

  assert(sep);
  for (const idl_node_t *n = node; n; n = n->parent) {
    if ((idl_mask(n) & IDL_TYPEDEF) == IDL_TYPEDEF)
      continue;
    if ((idl_mask(n) & IDL_ENUM) == IDL_ENUM && n != node)
      continue;
    if ((idl_mask(n) & IDL_BITMASK) == IDL_BITMASK && n != node)
      continue;
    ident = idl_identifier(n);
    assert(ident);
    len += strlen(ident) + (len ? strlen(sep) : 0);
  }

  off = len;
  for (const idl_node_t *n = node; n; n = n->parent) {
    if ((idl_mask(n) & IDL_TYPEDEF) == IDL_TYPEDEF)
      continue;
    if ((idl_mask(n) & IDL_ENUM) == IDL_ENUM && n != node)
      continue;
    if ((idl_mask(n) & IDL_BITMASK) == IDL_BITMASK && n != node)
      continue;
    ident = idl_identifier(n);
    assert(ident);
    cnt = strlen(ident);
    assert(cnt <= off);
    off -= cnt;
    copy(str, off, size, ident, cnt);
    if (off == 0)
      break;
    cnt = strlen(sep);
    off -= cnt;
    copy(str, off, size, sep, cnt);
  }
  str[ (size > len ? len : size - 1) ] = '\0';
  return (int)len;
}

int print_type(
  char *str, size_t size, const void *ptr, void *user_data)
{
  if (idl_is_base_type(ptr))
    return print_base_type(str, size, ptr, user_data);
  if (idl_is_templ_type(ptr))
    return print_templ_type(str, size, ptr, user_data);
  return print_decl_type(str, size, ptr, "_");
}

int print_scoped_name(
  char *str, size_t size, const void *ptr, void *user_data)
{
  if (idl_is_base_type(ptr))
    return print_base_type(str, size, ptr, user_data);
  if (idl_is_templ_type(ptr))
    return print_templ_type(str, size, ptr, user_data);
  return print_decl_type(str, size, ptr, "::");
}

static idl_retcode_t print_header(FILE *fh, const char *in, const char *out)
{
  static const char fmt[] =
    "/****************************************************************\n"
    "\n"
    "  Generated by Eclipse Cyclone DDS IDL to C Translator\n"
    "  File name: %s\n"
    "  Source: %s\n"
    "  Cyclone DDS: V%s\n"
    "\n"
    "*****************************************************************/\n";

  if (idl_fprintf(fh, fmt, out, in, IDL_VERSION) < 0)
    return IDL_RETCODE_NO_MEMORY;
  return IDL_RETCODE_OK;
}

static idl_retcode_t print_guard(FILE *fh, const char *in)
{
  if (fputs(header_guard_prefix, fh) < 0)
    return IDL_RETCODE_NO_MEMORY;
  for (const char *ptr = in; *ptr; ptr++) {
    int chr = (unsigned char)*ptr;
    if (idl_islower((unsigned char)*ptr))
      chr = idl_toupper((unsigned char)*ptr);
    else if (!idl_isalnum((unsigned char)*ptr))
      chr = '_';
    if (fputc(chr, fh) == EOF)
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t print_guard_if(FILE *fh, const char *in)
{
  if (fputs("#ifndef ", fh) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (print_guard(fh, in))
    return IDL_RETCODE_NO_MEMORY;
  if (fputs("\n#define ", fh) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (print_guard(fh, in))
    return IDL_RETCODE_NO_MEMORY;
  if (fputs("\n\n", fh) < 0)
    return IDL_RETCODE_NO_MEMORY;
  return IDL_RETCODE_OK;
}

static idl_retcode_t print_guard_endif(FILE *fh, const char *in)
{
  if (fputs("#endif /* ", fh) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (print_guard(fh, in))
    return IDL_RETCODE_NO_MEMORY;
  if (fputs(" */\n", fh) < 0)
    return IDL_RETCODE_NO_MEMORY;
  return IDL_RETCODE_OK;
}

static idl_retcode_t print_includes(FILE *fh, const idl_source_t *source)
{
  const idl_source_t *include;

  for (include = source->includes; include; include = include->next) {
    const char *ext = include->file->name;
    for (const char *ptr = ext; *ptr; ptr++) {
      if (*ptr == '.')
        ext = ptr;
    }

    if (idl_strcasecmp(ext, ".idl") == 0) {
      const char *fmt = "#include \"%.*s.h\"\n";
      int len = (int)(ext - include->file->name);
      if (idl_fprintf(fh, fmt, len, include->file->name) < 0)
        return IDL_RETCODE_NO_MEMORY;
    } else {
      const char *fmt = "#include \"%s\"\n";
      if (idl_fprintf(fh, fmt, include->file->name) < 0)
        return IDL_RETCODE_NO_MEMORY;
    }
    if (fputs("\n", fh) < 0)
      return IDL_RETCODE_NO_MEMORY;
  }

  return IDL_RETCODE_OK;
}

extern idl_retcode_t
generate_types(const idl_pstate_t *pstate, struct generator *generator);

idl_retcode_t
generate_nosetup(const idl_pstate_t *pstate, struct generator *generator)
{
  idl_retcode_t ret;
  const char *sep, *file = generator->path;
  char *const header_file = generator->header.path;
  char *const source_file = generator->source.path;

  if ((ret = print_header(generator->header.handle, file, header_file)))
    return ret;
  if ((ret = print_guard_if(generator->header.handle, header_file)))
    return ret;
  if ((ret = print_includes(generator->header.handle, pstate->sources)))
    return ret;
  if (fputs("#include \"dds/ddsc/dds_public_impl.h\"\n", generator->header.handle) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (generator->config.generate_cdrstream_desc && fputs("#include \"dds/cdr/dds_cdrstream.h\"\n", generator->header.handle) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (fputs("\n", generator->header.handle) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if (fputs("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n", generator->header.handle) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = print_header(generator->source.handle, file, source_file)))
    return ret;
  /* generate include statement for header in source */
  sep = header_file;
  for (const char *ptr = sep; *ptr; ptr++)
    if (idl_isseparator((unsigned char)*ptr))
      sep = ptr+1;
  if (idl_fprintf(generator->source.handle, "#include \"%s\"\n\n", sep) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = generate_types(pstate, generator)))
    return ret;
  if (fputs("#ifdef __cplusplus\n}\n#endif\n\n", generator->header.handle) < 0)
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = print_guard_endif(generator->header.handle, header_file)))
    return ret;

  return IDL_RETCODE_OK;
}

static const idlc_option_t *opts[] = {
  &(idlc_option_t){
    IDLC_STRING, { .string = &export_macro }, 'e', "", "<export macro>",
    "Add export macro before topic descriptors." },
  &(idlc_option_t){
    IDLC_FLAG, { .flag = &generate_cdrstream_desc }, 'f', "cdrstream-desc", "",
    "Generate CDR descriptor in addition to regular topic descriptor." },
  &(idlc_option_t){
    IDLC_STRING, { .string = &header_guard_prefix },
    'f', "header-guard-prefix", "<header guard prefix>",
    "Prefix to use for header inclusion guard macros (default: DDSC_)." },
  NULL
};

const idlc_option_t** idlc_generator_options(void)
{
  return opts;
}

idl_retcode_t
idlc_generate(const idl_pstate_t *pstate, const idlc_generator_config_t *config)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  struct generator generator;

  assert(pstate->paths);
  assert(pstate->paths->name);
  assert(config);
  const char* path = pstate->sources->path->name;

  memset(&generator, 0, sizeof(generator));
  generator.path = path;

  if (idl_generate_out_file(path, config->output_dir, config->base_dir, "h", &generator.header.path, false) < 0)
    goto err_header;
  if (!(generator.header.handle = idl_fopen(generator.header.path, "wb")))
    goto err_header;
  if (idl_generate_out_file(path, config->output_dir, config->base_dir, "c", &generator.source.path, false) < 0)
    goto err_source;
  if (!(generator.source.handle = idl_fopen(generator.source.path, "wb")))
    goto err_source;
  generator.config.c = *config;
  if (export_macro) {
    if (!(generator.config.export_macro = idl_strdup (export_macro)))
      goto err_options;
  } else {
    generator.config.export_macro = NULL;
  }
  generator.config.generate_cdrstream_desc = (generate_cdrstream_desc != 0);
  ret = generate_nosetup(pstate, &generator);

err_options:
  if (generator.config.export_macro)
    idl_free(generator.config.export_macro);
err_source:
  if (generator.source.handle)
    fclose(generator.source.handle);
  if (generator.source.path)
    idl_free(generator.source.path);
err_header:
  if (generator.header.handle)
    fclose(generator.header.handle);
  if (generator.header.path)
    idl_free(generator.header.path);
  return ret;
}
