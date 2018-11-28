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

#define YYSTYPE DDS_TS_PARSER_STYPE
#define YYLTYPE DDS_TS_PARSER_LTYPE

#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;

#define YY_NO_UNISTD_H 1 /* to surpress #include <unistd.h> */

#include "parser.h"
#include "stringify.h"
#include "idl.parser.h"
#include "yy_decl.h"
#include "idl.lexer.h"

extern void dds_ts_stringify(dds_ts_node_t *root_node, char *buffer, size_t len);

extern int
dds_ts_parse_file(const char *file, void (*error_func)(int line, int column, const char *msg))
{
  int err = 0;
  FILE *fh;

  assert(file != NULL);

DDSRT_WARNING_MSVC_OFF(4996);
  fh = fopen(file, "rb");
DDSRT_WARNING_MSVC_ON(4996);

  if (fh == NULL) {
    err = errno;
    if (error_func != 0) {
      error_func(0, 0, "Cannot open file");
    }
  }
  else {
    dds_ts_context_t *context = dds_ts_create_context();
    if (context == NULL) {
      if (error_func != 0) {
        error_func(0, 0, "Error: out of memory\n");
      }
      return 2;
    }
    dds_ts_context_set_error_func(context, error_func);
    yyscan_t scanner;
    dds_ts_parser_lex_init(&scanner);
    dds_ts_parser_set_in(fh, scanner);
    err = dds_ts_parser_parse(scanner, context);
    if (err == 0) {
      char buffer[1000];
      dds_ts_stringify(dds_ts_context_get_root_node(context), buffer, 1000);
      /* FIXME: This print statement is only temporary here to show some result. */
DDSRT_WARNING_MSVC_OFF(4996);
      printf("Result: '%s'\n", buffer);
DDSRT_WARNING_MSVC_ON(4996);
    }
    else if (dds_ts_context_get_out_of_memory_error(context)) {
      if (error_func != 0) {
        error_func(0, 0, "Error: out of memory\n");
      }
    }
    dds_ts_free_context(context);
    dds_ts_parser_lex_destroy(scanner);
    (void)fclose(fh);
  }

  return err;
}

extern int
dds_ts_parse_string(const char *str, void (*error_func)(int line, int column, const char *text))
{
  int err = 0;

  assert(str != NULL);
  if (str == NULL) {
    if (error_func != NULL) {
      error_func(0, 0, "String argument is NULL");
    }
    err = -1;
  }
  else {
    dds_ts_context_t *context = dds_ts_create_context();
    if (context == NULL) {
      if (error_func != NULL) {
        error_func(0, 0, "Out of memory");
      }
      return 2;
    }
    dds_ts_context_set_error_func(context, error_func);
    yyscan_t scanner;
    dds_ts_parser_lex_init(&scanner);
    dds_ts_parser__scan_string(str, scanner);
    err = dds_ts_parser_parse(scanner, context);
    if (err != 0) {
      if (dds_ts_context_get_out_of_memory_error(context)) {
        if (error_func != NULL) {
          error_func(0, 0, "Out of memory");
        }
      }
    }
    dds_ts_free_context(context);
    dds_ts_parser_lex_destroy(scanner);
  }

  return err;
}

/* For testing: */

int dds_ts_parse_string_stringify(const char *str, char *buffer, size_t len)
{
  int err = 0;

  assert(str != NULL);
  assert(buffer != NULL);

  if (str == NULL || buffer == NULL) {
    err = -1;
  }
  else {
    dds_ts_context_t *context = dds_ts_create_context();
    if (context == NULL) {
      ddsrt_strlcpy(buffer, "OUT_OF_MEMORY", len);
      return 2;
    }
    yyscan_t scanner;
    dds_ts_parser_lex_init(&scanner);
    dds_ts_parser__scan_string(str, scanner);
    err = dds_ts_parser_parse(scanner, context);
    if (err != 0) {
      if (dds_ts_context_get_out_of_memory_error(context)) {
        ddsrt_strlcpy(buffer, "OUT_OF_MEMORY", len);
      }
      else {
        ddsrt_strlcpy(buffer, "PARSING ERROR", len);
      }
      buffer[len-1] = '\0';
    }
    else {
      dds_ts_stringify(dds_ts_context_get_root_node(context), buffer, len);
    }

    dds_ts_free_context(context);
    dds_ts_parser_lex_destroy(scanner);
  }

  return err;
}
