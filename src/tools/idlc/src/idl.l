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
%{
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"

#include "idl.h"
#include "idl.y.h"
%}

%x COMMENT
%x DIRECTIVE
%x LINE
%x PRAGMA
%x KEYLIST
%x KEY
%x NOOP
%x SPACE
%x SCOPED_NAME

%option noyywrap
%option nounistd
%option never-interactive
%option prefix="idl_yy"
%option reentrant
%option noinput
%option nounput
%option bison-bridge
%option bison-locations
%option yylineno
%option stack

%{
#define YY_USER_ACTION \
  do { \
    yylloc->first_file = yylloc->last_file; \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    yylloc->last_line = yylineno; \
    yylloc->last_column = yycolumn+yyleng; \
    yycolumn += yyleng; \
  } while (0);
%}

decimal_number            [1-9][0-9]*
octal_number              0[0-7]*
hexadecimal_number        0[xX][a-fA-F0-9]+
integer_literal           ({decimal_number}|{octal_number}|{hexadecimal_number})
string_literal            \"([^\n\\\"]|\\.)*\"
identifier                [a-zA-Z_][a-zA-Z0-9_]*

%%
  /* yy_push_state and yy_pop_state are reserved for COMMENTs. propably better
     implemented without stack(?) */
<*>"/*"               { yymore(); yy_push_state(COMMENT, yyscanner); }
<COMMENT>[^*\n]       { yymore(); }
<COMMENT>"*"+[^*/\n]  { yymore(); }
<COMMENT>[\n]         { yymore(); return '\n'; }
<COMMENT>"*/"         {           yy_pop_state(yyscanner); }

<*>"//"+[^\n]* {
    /* FIXME: add support for RTI //@ directives like //@key, which are
              compiler directives and not idl language constructs */
  }

<*>[\n] {
    /* preprocessor pushes input to the lexer, but the preprocessor writes a
       stream of bytes, not tokens. Storing the entire stream in memory before
       tokenization is rather inefficient, therefore the write function keeps
       track of newlines. lines can safely be discarded from the input buffer
       after tokenization. however, the lexer quits once the buffer is empty.
       to ensure all input is tokenized, the lexer signals the write function
       after a line has been tokenized so that the end of the buffer is never
       reached before the end of the stream */
    /* preprocessor takes care of line-continuation sequences */
    switch (YY_START) {
      case DIRECTIVE:
      case KEYLIST:
        yyerror(yylloc, parser, "invalid compiler directive");
        yyterminate();
      case KEY:
        ddsts_pragma_close(parser->context);
        break;
      default:
        break;
    }

    BEGIN(INITIAL);
    return '\n';
  }

<INITIAL>^[ \t]*# {
    /* compiler directives like #line and #pragma are processed by the lexer
       as they are not idl language constructs */
    BEGIN(DIRECTIVE);
  }

<DIRECTIVE>{identifier} {
    if (strcmp(yytext, "line") == 0) {
      BEGIN(LINE);
    } else if (strcmp(yytext, "pragma") == 0) {
      BEGIN(PRAGMA);
    } else {
      /* include directives have been handled by the preprocessor */
      yyerror(yylloc, parser, "unsupported compiler directive '%s'", yytext);
      yyterminate();
    }
  }

<LINE>{decimal_number}([ \t]+{string_literal})? {
    char *str, *dquot = NULL;
    long long ln;

    (void)ddsrt_strtoll(yytext, &dquot, 10, &ln);
    assert(dquot != yytext); /* require at least one digit in input */
    while (*dquot == ' ' || *dquot == '\t') {
      dquot++; /* ignore white space */
    }
    if (*dquot == '"') {
      char *fn = NULL;
      size_t len;
      str = ++dquot;
      dquot = strrchr(dquot, '"');
      assert(dquot != NULL);
      len = dquot - str;
      /* see if file is already listed to avoid duplication */
      for (idl_file_t *f = parser->files; f && fn == NULL; f = f->next) {
        if (strncmp(str, f->name, len) == 0) {
          fn = f->name;
        }
      }
      if (fn == NULL) {
        idl_file_t *file;
        if ((file = ddsrt_malloc(sizeof(*file))) == NULL) {
          yyerror(yylloc, parser, "memory exhausted");
          yyterminate();
        } else if ((fn = ddsrt_strndup(str, len)) == NULL) {
          ddsrt_free(file);
          yyerror(yylloc, parser, "memory exhausted");
          yyterminate();
        }
        file->next = parser->files;
        file->name = fn;
        parser->files = file;
      }
      yylloc->last_file = fn;
    }
    yylloc->last_line = yylineno = (int)ln;
    yylloc->last_column = yycolumn = 1;
    BEGIN(SPACE);
  }

<PRAGMA>{identifier} {
    /* compatability with OpenSplice. See OpenSplice_IDLPreProcGuide.pdf for
       grammar specification. */
    if (strcmp(yytext, "keylist") == 0) {
      BEGIN(KEYLIST);
    } else {
      /* unsupported pragma directives are ignored */
      yywarning(yylloc, parser, "unsupported pragma '%s'", yytext);
      BEGIN(NOOP);
    }
  }

<KEYLIST>{identifier} {
    char *ident = NULL;

    ddsts_pragma_open(parser->context);
    if ((ident = ddsrt_strdup(yytext)) != NULL &&
        (ddsts_pragma_add_identifier(parser->context, ident)))
    {
      BEGIN(KEY);
    } else {
      ddsrt_free(ident);
      yyerror(yylloc, parser, "memory exhausted");
      yyterminate();
    }
  }

<KEY>{identifier} {
    char *ident = NULL;
    if ((ident = ddsrt_strdup(yytext)) == NULL ||
       !(ddsts_pragma_add_identifier(parser->context, ident)))
    {
      ddsrt_free(ident);
      yyerror(yylloc, parser, "memory exhausted");
      yyterminate();
    }
  }

<INITIAL,SCOPED_NAME>{identifier}(::)? {
    /* grammer for IDL (>=4.0) is incorrect (or at least ambiguous). blanks,
       horizontal and vertical tabs, newlines, form feeds, and comments
       (collective, "white space") are ignored except as they serve to
       separate tokens. the specification does not clearly state if white
       space may occur between "::" and adjacent identifiers to form a
       "scoped_name". the same is true for the "annotation_appl". in C++ "::"
       is an operator and white space is therefore allowed, in IDL it is not.
       this did not use to be a problem, but with the addition of annotations
       it became possible to have two adjacent scoped names. many compilers
       (probably) implement just the standardized annotations. the pragmatic
       approach is to forbid use of white space in annotations, which works
       for standardized annotations like "@key" and allow use of white space
       for scoped names elsewhere. to implement this feature the parser must
       know whether or not white space occurred between an identifier and the
       scope operator. however, white space cannot be communicated to the
       the parser (the grammer would explode) and an introducing an extra
       identifier class is not an option (same reason). to work around this
       problem, the lexer communicates different types of scope operators
       used by the parser to implement a specialized "scoped_name" version
       just for annotations. */
    int tok;
    char *ident;
    size_t len = yyleng;

    if (len > 2 && yytext[len-2] == ':' && yytext[len-1] == ':') {
      /* fixup location before :: is reverted */
      yycolumn -= 2;
      yylloc->last_column = yycolumn;
      len -= 2;
      BEGIN(SCOPED_NAME);
      yyless((int)len);
    } else {
      BEGIN(INITIAL);
    }

    if ((ident = ddsrt_strndup(yytext, len)) == NULL) {
      yyerror(yylloc, parser, "memory exhausted");
      yyterminate();
    } else if ((tok = yystrtok(ident, 0)) != -1) {
      ddsrt_free(ident);
      BEGIN(INITIAL);
      return tok;
    }

    yylval->identifier = ident;
    return IDL_T_IDENTIFIER;
  }

<INITIAL,SCOPED_NAME>(::)({identifier})? {
    /* see reason for optional identifier above */
    int tok;
    size_t len = yyleng;

    if (len == 2) {
      tok = ((YY_START) == SCOPED_NAME) ? IDL_T_SCOPE_L : IDL_T_SCOPE;
      BEGIN(INITIAL);
    } else {
      assert(len > 2);
      /* fixup location before identifier is reverted */
      yycolumn -= (int)(len - 2);
      yylloc->last_column = yycolumn;
      tok = ((YY_START) == SCOPED_NAME) ? IDL_T_SCOPE_LR : IDL_T_SCOPE_R;
      BEGIN(SCOPED_NAME);
      yyless(2);
    }

    return tok;
  }

@(::|{identifier}) {
    /* see reason for mandatory :: or identifier above */
    /* fixup location before :: or identifier is reverted */
    yycolumn -= (yyleng - 1);
    yylloc->last_column = yycolumn;
    yyless(1);
    return IDL_T_AT;
  }

{integer_literal} {
    unsigned long long ullng;

    /* strtoll recognizes if the value is dec, oct or hex if base is zero */
    if (ddsrt_strtoull(yytext, NULL, 0, &ullng) != DDS_RETCODE_OK) {
      idl_yyerror(yylloc, parser, "integer value %s out-of-range", yytext);
      yyterminate();
    }

    yylval->literal.flags = DDSTS_ULONGLONG;
    yylval->literal.value.ullng = ullng;
    return IDL_T_INTEGER_LITERAL;
  }

<*>[ \t]+ {
    /* ignore white space, except in SCOPED_NAME */
    switch (YY_START) {
      case SCOPED_NAME:
        BEGIN(INITIAL);
        /* fall through */
      default:
        break;
    }
  }

<*>. {
    switch (YY_START) {
      case DIRECTIVE:
      case LINE:
      case PRAGMA:
      case KEYLIST:
      case KEY:
      case SPACE:
        yyerror(yylloc, parser, "invalid symbol in compiler directive");
        yyterminate();
        /* never reached */
      case NOOP:
        /* ignore everything up to newline */
        break;
      default:
        break;
    }

    return yytext[0];
  }

%%

#define CHUNK (4096)

int idl_puts(idl_parser_t *parser, const char *str, size_t len)
{
  dds_return_t rc = 0;
  struct yyguts_t *yyg; /* required for YY_CURRENT_BUFFER */
  YY_BUFFER_STATE yybuf;

  assert(parser != NULL);
  assert(parser->yylstate != NULL);

  yyg = (struct yyguts_t *)parser->yylstate;
  /* YY_CURRENT_BUFFER macro is used so that the yy_buffer_state struct does
     not have to be defined in order to define the idl_parser struct */
  yybuf = YY_CURRENT_BUFFER;

  /* tokenize to free up space */
  if ((parser->buffer.size - parser->buffer.used) <= len &&
      (parser->buffer.lines) > 1)
  {
    while (parser->buffer.lines > 1 && (rc = idl_scan(parser)) == 1) { /* scan */ }

    if (rc != 1) {
      assert(rc != DDS_RETCODE_OK);
      ddsts_context_set_retcode(parser->context, rc);
      return -1;
    }

    /* move non-tokenized data to start of buffer, but only if yy_more_flag is
       not set to avoid corruption of buffer state */
    if (yyg->yy_more_flag != 1) {
      size_t cnt, off;
      off = yyg->yy_c_buf_p - yybuf->yy_ch_buf;
      assert(off != 0);
      cnt = parser->buffer.used - off;
      memmove(parser->buffer.data, parser->buffer.data + off, parser->buffer.used - off);
      parser->buffer.used = cnt;
      parser->buffer.data[cnt + 0] = '\0';
      parser->buffer.data[cnt + 1] = '\0';
      /* update yybuf and yyscan */
      yybuf->yy_n_chars = yybuf->yy_buf_size = (int)cnt;
      yyg->yy_c_buf_p = yybuf->yy_ch_buf;
    }
  }

  /* expand buffer if necessary */
  if ((parser->buffer.size - parser->buffer.used) <= len) {
    size_t size = parser->buffer.size + (((len / CHUNK) + 1) * CHUNK);
    char *buf = ddsrt_realloc(parser->buffer.data, size + 2 /* '\0' + '\0' */);
    if (buf == NULL) {
      ddsts_context_set_retcode(parser->context, DDS_RETCODE_OUT_OF_RESOURCES);
      return -1;
    }
    /* update input buffer */
    parser->buffer.data = buf;
    parser->buffer.size = size;
    if (yybuf != NULL) {
      /* update yybuf and yyscan */
      yyg->yy_c_buf_p = buf + (yyg->yy_c_buf_p - yybuf->yy_ch_buf);
      yybuf->yy_buf_pos = buf + (yybuf->yy_buf_pos - yybuf->yy_ch_buf);
      yybuf->yy_ch_buf = parser->buffer.data;
    }
  }

  /* write to buffer */
  memcpy(parser->buffer.data + parser->buffer.used, str, len);
  parser->buffer.used += len;
  assert(parser->buffer.used <= parser->buffer.size);
  parser->buffer.data[parser->buffer.used + 0] = '\0';
  parser->buffer.data[parser->buffer.used + 1] = '\0';
  /* buffer must exist and contain data */
  if (yybuf == NULL) {
    yybuf = yy_scan_buffer(parser->buffer.data, parser->buffer.used + 2, yyg);
    if (yybuf == NULL) {
      ddsts_context_set_retcode(parser->context, DDS_RETCODE_OUT_OF_RESOURCES);
      return -1;
    }
    yyset_lineno(1, yyg);
    yyset_column(1, yyg);
  } else {
    yybuf->yy_n_chars = yybuf->yy_buf_size = (int)parser->buffer.used;
  }

  /* keep track of lines */
  for (const char *ptr = str; *ptr != '\0'; ptr++) {
    if (*ptr == '\n') {
      parser->buffer.lines++;
    }
  }

  return (int)len;
}
