/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsts/typetree.h"
#include "parser.h"
#include "idl.parser.h"
#include "yy_decl.h"
#define YY_NO_UNISTD_H 1 /* to suppress #include <unistd.h> in: */
#include "idl.lexer.h"

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

dds_return_t ddsts_idl_parse_file(const char *file, ddsts_type_t **ref_root_type)
{
  if (file == NULL || ref_root_type == NULL) {
    return DDS_RETCODE_BAD_PARAMETER;
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
  yyscan_t scanner;
  ddsts_parser_lex_init(&scanner);
  ddsts_parser_set_in(fh, scanner);
  int parse_result = ddsts_parser_parse(scanner, context);
  dds_return_t rc = parse_result == 2 ? DDS_RETCODE_OUT_OF_RESOURCES : ddsts_context_get_retcode(context);
  if (rc == DDS_RETCODE_OK) {
    *ref_root_type = ddsts_context_take_root_type(context);
  }
  ddsts_free_context(context);
  ddsts_parser_lex_destroy(scanner);
  dds_set_log_sink(0, NULL);
  dds_set_log_mask(log_mask);
  (void)fclose(fh);

  return rc;
}

dds_return_t ddsts_idl_parse_string(const char *str, ddsts_type_t **ref_root_type)
{
  if (str == NULL || ref_root_type == NULL) {
    return DDS_RETCODE_BAD_PARAMETER;
  }
  *ref_root_type = NULL;

  ddsts_context_t *context = ddsts_create_context();
  if (context == NULL) {
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  uint32_t log_mask = dds_get_log_mask();
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_WARNING);
  dds_set_log_sink(log_write_to_file, stderr);
  yyscan_t scanner;
  ddsts_parser_lex_init(&scanner);
  ddsts_parser__scan_string(str, scanner);
  int parse_result = ddsts_parser_parse(scanner, context);
  dds_return_t rc = parse_result == 2 ? DDS_RETCODE_OUT_OF_RESOURCES : ddsts_context_get_retcode(context);
  if (rc == DDS_RETCODE_OK) {
    *ref_root_type = ddsts_context_take_root_type(context);
  }
  ddsts_free_context(context);
  ddsts_parser_lex_destroy(scanner);
  dds_set_log_sink(0, NULL);
  dds_set_log_mask(log_mask);

  return rc;
}

