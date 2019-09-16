/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "idl.h"
#include "idl.y.h"
/* Disable inclusion of unistd.h in the flex generated header file, as
   %nounistd only disables inclusion in the generated source file. */
#define YY_NO_UNISTD_H
#include "idl.l.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"

static void log_write_to_file(void *ptr, const dds_log_data_t *data)
{
  if (data->priority == DDS_LC_ERROR) {
    fprintf((FILE *)ptr, "Error at ");
  }
  else if (data->priority == DDS_LC_WARNING) {
    fprintf((FILE *)ptr, "Warning at ");
  }
  fprintf((FILE *)ptr, "%*.*s\n", (int)data->size, (int)data->size, data->message);
  fflush((FILE *)ptr);
}

static dds_return_t idl_parser_new(idl_parser_t **parserp)
{
  idl_parser_t *parser;

  if ((parser = ddsrt_calloc(1, sizeof(*parser))) == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  idl_yylex_init(&parser->scanner);
  *parserp = parser;
  return DDS_RETCODE_OK;
}

static void idl_parser_free(idl_parser_t *parser)
{
  if (parser != NULL) {
    idl_file_t *file = parser->files, *next;
    while (file != NULL) {
      next = file->next;
      if (file->name) {
        ddsrt_free(file->name);
      }
      ddsrt_free(file);
      file = next;
    }
    if (parser->scanner) {
      idl_yylex_destroy(parser->scanner);
    }
    ddsrt_free(parser);
  }
}

dds_return_t idl_parse_file(const char *file, ddsts_type_t **ref_root_type)
{
  dds_return_t rc;
  idl_parser_t *parser = NULL;

  if (file == NULL || ref_root_type == NULL) {
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if ((rc = idl_parser_new(&parser)) != DDS_RETCODE_OK) {
    return rc;
  }
  if ((parser->files = ddsrt_calloc(1, sizeof(idl_file_t))) == NULL ||
      (parser->files->name = ddsrt_strdup(file)) == NULL)
  {
    idl_parser_free(parser);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  *ref_root_type = NULL;
DDSRT_WARNING_MSVC_OFF(4996);
  FILE *fh = fopen(file, "rb");
DDSRT_WARNING_MSVC_ON(4996);

  if (fh == NULL) {
    DDS_ERROR("Cannot open file\n");
    return DDS_RETCODE_ERROR;
  }

  ddsts_context_t *context = ddsts_create_context();
  if (context == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  uint32_t log_mask = dds_get_log_mask();
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING);
  dds_set_log_sink(log_write_to_file, stderr);
  idl_yyset_in(fh, parser->scanner);
  int parse_result = idl_yyparse(parser, context, parser->scanner);
  rc = parse_result == 2 ? DDS_RETCODE_OUT_OF_RESOURCES : ddsts_context_get_retcode(context);
  if (rc == DDS_RETCODE_OK) {
    *ref_root_type = ddsts_context_take_root_type(context);
  }
  ddsts_free_context(context);
  idl_parser_free(parser);
  dds_set_log_sink(0, NULL);
  dds_set_log_mask(log_mask);
  (void)fclose(fh);

  return rc;
}

dds_return_t idl_parse_string(const char *str, ddsts_type_t **ref_root_type)
{
  dds_return_t rc;
  idl_parser_t *parser;

  if (str == NULL || ref_root_type == NULL) {
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if ((rc = idl_parser_new(&parser)) != DDS_RETCODE_OK) {
    return rc;
  }
  *ref_root_type = NULL;

  ddsts_context_t *context = ddsts_create_context();
  if (context == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  uint32_t log_mask = dds_get_log_mask();
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING);
  dds_set_log_sink(log_write_to_file, stderr);
  idl_yy_scan_string(str, parser->scanner);
  idl_yyset_lineno(1, parser->scanner);
  int parse_result = idl_yyparse(parser, context, parser->scanner);
  rc = parse_result == 2 ? DDS_RETCODE_OUT_OF_RESOURCES : ddsts_context_get_retcode(context);
  if (rc == DDS_RETCODE_OK) {
    *ref_root_type = ddsts_context_take_root_type(context);
  }
  ddsts_free_context(context);
  idl_parser_free(parser);
  dds_set_log_sink(0, NULL);
  dds_set_log_mask(log_mask);

  return rc;
}

