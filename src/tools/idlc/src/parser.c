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

#define YYSTYPE DDSTS_PARSER_STYPE
#define YYLTYPE DDSTS_PARSER_LTYPE

#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;

#define YY_NO_UNISTD_H 1 /* to surpress #include <unistd.h> */

#include "parser.h"
#include "idl.parser.h"
#include "yy_decl.h"
#include "idl.lexer.h"

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
  yyscan_t scanner;
  ddsts_parser_lex_init(&scanner);
  ddsts_parser_set_in(fh, scanner);
  int parse_result = ddsts_parser_parse(scanner, context);
  if (parse_result == 0) {
    *ref_root_type = ddsts_context_take_root_type(context);
  }
  dds_return_t rc = parse_result == 2 ? DDS_RETCODE_OUT_OF_RESOURCES : ddsts_context_get_retcode(context) ;
  ddsts_free_context(context);
  ddsts_parser_lex_destroy(scanner);
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
  yyscan_t scanner;
  ddsts_parser_lex_init(&scanner);
  ddsts_parser__scan_string(str, scanner);
  int parse_result = ddsts_parser_parse(scanner, context);
  if (parse_result == 0) {
    *ref_root_type = ddsts_context_take_root_type(context);
  }
  dds_return_t rc = parse_result == 2 ? DDS_RETCODE_OUT_OF_RESOURCES : ddsts_context_get_retcode(context) ;
  ddsts_free_context(context);
  ddsts_parser_lex_destroy(scanner);

  return rc;
}

