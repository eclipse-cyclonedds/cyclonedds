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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "generator.h"

#include "idl/file.h"
#include "idl/retcode.h"
#include "idl/stream.h"
#include "idl/string.h"
#include "idl/version.h"
#include "idl/processor.h"

idlc_thread_local struct idlc_auto idlc_auto__;

char *absolute_name(const void *node, const char *separator);

char *absolute_name(const void *node, const char *separator)
{
  char *str;
  size_t cnt, len = 0;
  const char *sep, *ident;
  const idl_node_t *root;
  for (root=node, sep=""; root; root=root->parent) {
    if ((idl_mask(root) & IDL_TYPEDEF) == IDL_TYPEDEF)
      continue;
    if ((idl_mask(root) & IDL_ENUM) == IDL_ENUM && root != node)
      continue;
    ident = idl_identifier(root);
    assert(ident);
    len += strlen(sep) + strlen(ident);
    sep = separator;
  }
  if (!(str = malloc(len + 1)))
    return NULL;
  str[len] = '\0';
  for (root=node, sep=separator; root; root=root->parent) {
    if ((idl_mask(root) & IDL_TYPEDEF) == IDL_TYPEDEF)
      continue;
    if ((idl_mask(root) & IDL_ENUM) == IDL_ENUM && root != node)
      continue;
    ident = idl_identifier(root);
    assert(ident);
    cnt = strlen(ident);
    assert(cnt <= len);
    len -= cnt;
    memmove(str+len, ident, cnt);
    if (len == 0)
      break;
    cnt = strlen(sep);
    assert(cnt <= len);
    len -= cnt;
    memmove(str+len, sep, cnt);
  }
  assert(len == 0);
  return str;
}

char *typename(const void *node);

char *typename(const void *node)
{
  switch (idl_type(node)) {
    case IDL_BOOL:     return idl_strdup("bool");
    case IDL_CHAR:     return idl_strdup("char");
    case IDL_INT8:     return idl_strdup("int8_t");
    case IDL_OCTET:
    case IDL_UINT8:    return idl_strdup("uint8_t");
    case IDL_SHORT:
    case IDL_INT16:    return idl_strdup("int16_t");
    case IDL_USHORT:
    case IDL_UINT16:   return idl_strdup("uint16_t");
    case IDL_LONG:
    case IDL_INT32:    return idl_strdup("int32_t");
    case IDL_ULONG:
    case IDL_UINT32:   return idl_strdup("uint32_t");
    case IDL_LLONG:
    case IDL_INT64:    return idl_strdup("int64_t");
    case IDL_ULLONG:
    case IDL_UINT64:   return idl_strdup("uint64_t");
    case IDL_FLOAT:    return idl_strdup("float");
    case IDL_DOUBLE:   return idl_strdup("double");
    case IDL_LDOUBLE:  return idl_strdup("long double");
    case IDL_STRING:   return idl_strdup("char");
    case IDL_SEQUENCE: {
      /* sequences require a little magic */
      const char pref[] = "dds_sequence_";
      const char seq[] = "sequence_";
      char dims[32] = "";
      const idl_type_spec_t *type_spec;
      char *type, *seqtype = NULL;
      size_t cnt = 0, len = 0, pos = 0;

      type_spec = idl_type_spec(node);
      for (; idl_is_sequence(type_spec); type_spec = idl_type_spec(type_spec))
        cnt++;
      if (idl_is_base_type(type_spec) || idl_is_string(type_spec))
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
      else if (!(type = absolute_name(type_spec, "_")))
        goto err_type;
      len = strlen(pref) + strlen(type) + strlen(dims);
      if (!(seqtype = malloc(len + (cnt * strlen(seq)) + 1)))
        goto err_seqtype;
      len = strlen(pref);
      memcpy(seqtype, pref, len);
      pos += len;
      for (; cnt; pos += strlen(seq), cnt--)
        memcpy(seqtype+pos, seq, strlen(seq));
      len = strlen(type);
      memcpy(seqtype+pos, type, len);
      pos += len;
      len = strlen(dims);
      memcpy(seqtype+pos, dims, len);
      pos += len;
      seqtype[pos] = '\0';
err_seqtype:
      if (!idl_is_base_type(type_spec) && !idl_is_string(type_spec))
        free(type);
err_type:
      return seqtype;
    }
    default:
      break;
  }

  return absolute_name(node, "_");
}
static char *figure_guard(const char *file)
{
  char *inc = NULL;

  if (idl_asprintf(&inc, "DDSC_%s", file) == -1)
    return NULL;

  /* replace any non-alphanumeric characters */
  for (char *ptr = inc; *ptr; ptr++) {
    if (idl_islower((unsigned char)*ptr))
      *ptr = (char)idl_toupper((unsigned char)*ptr);
    else if (!idl_isalnum((unsigned char)*ptr))
      *ptr = '_';
  }

  return inc;
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

static idl_retcode_t print_guard_if(FILE *fh, const char *guard)
{
  static const char fmt[] =
    "#ifndef %1$s\n"
    "#define %1$s\n\n";
  if (idl_fprintf(fh, fmt, guard) < 0)
    return IDL_RETCODE_NO_MEMORY;
  return IDL_RETCODE_OK;
}

static idl_retcode_t print_guard_endif(FILE *fh, const char *guard)
{
  static const char fmt[] =
    "#endif /* %1$s */\n";
  if (idl_fprintf(fh, fmt, guard) < 0)
    return IDL_RETCODE_NO_MEMORY;
  return IDL_RETCODE_OK;
}

static idl_retcode_t print_includes(FILE *fh, const idl_source_t *source)
{
  idl_retcode_t ret;
  char *sep = NULL, *path;
  const idl_source_t *include;

  if (!(path = AUTO(idl_strdup(source->path->name))))
    return IDL_RETCODE_NO_MEMORY;
  for (char *ptr = path; *ptr; ptr++)
    if (idl_isseparator(*ptr))
      sep = ptr;
  if (sep)
    *sep = '\0';

  for (include = source->includes; include; include = include->next) {
    char *ext, *relpath = NULL;
    if ((ret = idl_relative_path(path, include->path->name, &relpath)))
      return ret;
    if (!(relpath = AUTO(relpath)))
      return IDL_RETCODE_NO_MEMORY;
    ext = relpath;
    for (char *ptr = ext; *ptr; ptr++) {
      if (*ptr == '.')
        ext = ptr;
    }
    if (ext > relpath && idl_strcasecmp(ext, ".idl") == 0) {
      const char *fmt = "#include \"%.*s.h\"\n";
      int len = (int)(ext - relpath);
      if (idl_fprintf(fh, fmt, len, relpath) < 0)
        return IDL_RETCODE_NO_MEMORY;
    } else {
      const char *fmt = "#include \"%s\"\n";
      if (idl_fprintf(fh, fmt, relpath) < 0)
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
  char *guard;
  const char *sep, *file = generator->path;
  char *const header_file = generator->header.path;
  char *const source_file = generator->source.path;

  if (!(guard = AUTO(figure_guard(header_file))))
    return IDL_RETCODE_NO_MEMORY;
  if ((ret = print_header(generator->header.handle, file, header_file)))
    return ret;
  if ((ret = print_guard_if(generator->header.handle, guard)))
    return ret;
  if ((ret = print_includes(generator->header.handle, pstate->sources)))
    return ret;
  if (fputs("#include \"dds/ddsc/dds_public_impl.h\"\n\n", generator->header.handle) < 0)
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
  if ((ret = print_guard_endif(generator->header.handle, guard)))
    return ret;

  return IDL_RETCODE_OK;
}

static FILE *open_file(const char *pathname, const char *mode)
{
#if _WIN32
  FILE *handle = NULL;
  if (fopen_s(&handle, pathname, mode) != 0)
    return NULL;
  return handle;
#else
  return fopen(pathname, mode);
#endif
}

idl_retcode_t
idlc_generate(const idl_pstate_t *pstate)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  const char *sep, *ext, *file, *path;
  char empty[1] = { '\0' };
  char *dir = NULL, *basename = NULL;
  struct generator generator;

  assert(pstate->paths);
  assert(pstate->paths->name);
  path = pstate->sources->path->name;
  /* use relative directory if user provided a relative path, use current
     word directory otherwise */
  sep = ext = NULL;
  for (const char *ptr = path; ptr[0]; ptr++) {
    if (idl_isseparator((unsigned char)ptr[0]) && ptr[1] != '\0')
      sep = ptr;
    else if (ptr[0] == '.')
      ext = ptr;
  }

  file = sep ? sep + 1 : path;
  if (idl_isabsolute(path) || !sep)
    dir = empty;
  else if (!(dir = idl_strndup(path, (size_t)(sep-path))))
    goto err_dir;
  if (!(basename = idl_strndup(file, ext ? (size_t)(ext-file) : strlen(file))))
    goto err_basename;

  /* replace backslashes by forward slashes */
  for (char *ptr = dir; *ptr; ptr++) {
    if (*ptr == '\\')
      *ptr = '/';
  }

  memset(&generator, 0, sizeof(generator));
  generator.path = file;

  sep = dir[0] == '\0' ? "" : "/";
  if (idl_asprintf(&generator.header.path, "%s%s%s.h", dir, sep, basename) < 0)
    goto err_header;
  if (!(generator.header.handle = open_file(generator.header.path, "wb")))
    goto err_header;
  if (idl_asprintf(&generator.source.path, "%s%s%s.c", dir, sep, basename) < 0)
    goto err_source;
  if (!(generator.source.handle = open_file(generator.source.path, "wb")))
    goto err_source;
  ret = generate_nosetup(pstate, &generator);

err_source:
  if (generator.source.handle)
    fclose(generator.source.handle);
  if (generator.source.path)
    free(generator.source.path);
err_header:
  if (generator.header.handle)
    fclose(generator.header.handle);
  if (generator.header.path)
    free(generator.header.path);
  if (basename)
    free(basename);
err_basename:
  if (dir && dir != empty)
    free(dir);
err_dir:
  return ret;
}
