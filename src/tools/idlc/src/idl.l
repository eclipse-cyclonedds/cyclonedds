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

%x MULTILINE_COMMENT
%x COMMENT
%x NO_WS
%x DIRECTIVE
%x LINE
%x PRAGMA
%x DATA_TYPE
%x KEY

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

<INITIAL>^[ \t]*#[ \t]* {
    BEGIN(DIRECTIVE);
    /* do not emit token */
  }

<DIRECTIVE>line {
    /* #line directives are processed entirely by the scanner as they are
       compiler directives, not idl language constructs, and the parser is
       location agnostic */
    BEGIN(LINE);
    /* do not emit token */
  }

<LINE>[ \t]+{decimal_number}([ \t]+{string_literal})?[ \t]* {
    char *str, *dquot = NULL;
    long long ln;

    (void)ddsrt_strtoll(yytext, &dquot, 10, &ln);
    assert(dquot != yytext); /* require at least one digit in input */
    while (*dquot == ' ' || *dquot == '\t') {
      dquot++; /* ignore whitespace */
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
          yyerror(yylloc, parser, context, yyscanner, "Out of memory");
        } else if ((fn = ddsrt_strndup(str, len)) == NULL) {
          ddsrt_free(file);
          yyerror(yylloc, parser, context, yyscanner, "Out of memory");
        }
        file->next = parser->files;
        file->name = fn;
        parser->files = file;
      }
      yylloc->last_file = fn;
    }
    yylloc->last_line = yylineno = (int)ln;
    yylloc->last_column = yycolumn = 1;
    /* do not emit token */
  }

<LINE>[^[:space:]] {
    yyerror(yylloc, parser, context, yyscanner, "Malformed #line directive");
  }

<DIRECTIVE>pragma([ \t]+keylist)? {
    /* #pragma directives are processed by the scanner, knowledge of specific
       directives must only be propagated to the parser if idl language
       constructs are impacted, as is the case for e.g. #pragma keylist */
    char *dir;

    BEGIN(PRAGMA);
    /* ignore pragma directive */
    dir = yytext + strlen("pragma");
    if (*dir != '\0') {
      int tok = -1;
      /* fixup location before everything but pragma is reverted */
      yycolumn -= (yyleng - 6);
      yylloc->last_column = yycolumn;

      while (*dir == ' ' || *dir == '\t') {
        dir++; /* ignore whitespace */
      }
      /* emit pragma token if directive must be propagated to the parser */
      if (strcmp(dir, "keylist") == 0) {
        tok = IDL_T_PRAGMA;
      }

      yyless(6);

      if (tok != -1) {
        return tok;
      }
    }
    /* do not emit token */
  }

<PRAGMA>keylist {
    BEGIN(DATA_TYPE);
    return IDL_T_KEYLIST;
  }

<DATA_TYPE>{identifier} {
    BEGIN(KEY);
    if ((yylval->str = ddsrt_strdup(yytext)) == NULL) {
      yyerror(yylloc, parser, context, yyscanner, "Could not copy identifier");
    }
    return IDL_T_DATA_TYPE;
  }

<KEY>{identifier} {
    if ((yylval->str = ddsrt_strdup(yytext)) == NULL) {
      yyerror(yylloc, parser, context, yyscanner, "Could not copy identifier");
    }
    return IDL_T_KEY;
  }

<DATA_TYPE,KEY>[^[:space:]] {
    yyerror(yylloc, parser, context, yyscanner, "Malformed #pragma keylist directive");
  }

<PRAGMA>[^[:space:]] {
    yyerror(yylloc, parser, context, yyscanner, "Unkown #pragma directive");
  }


<INITIAL,NO_WS>[ \t\r]    { BEGIN(INITIAL); }

<INITIAL,NO_WS>"/*"       { BEGIN(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>.      { }
<MULTILINE_COMMENT>\n     { }
<MULTILINE_COMMENT>"*/"   { BEGIN(INITIAL); }

<INITIAL,NO_WS>"//"       { BEGIN(COMMENT); }
<COMMENT>.                { }


<INITIAL,NO_WS>{integer_literal} {
                            BEGIN(INITIAL);
                            yylval->literal.flags = DDSTS_ULONGLONG;
                            yylval->literal.value.ullng = 0u;
                            /* strtoll recognizes if the value is dec, oct or hex if base is zero */
                            dds_return_t retcode = ddsrt_strtoull(yytext, NULL, 0, &yylval->literal.value.ullng);
                            if (retcode != DDS_RETCODE_OK) {
                              idl_yyerror(yylloc_param, parser, context, yyscanner, "Integer value invalid");
                            }
                            return IDL_T_INTEGER_LITERAL;
                          }

<INITIAL>{identifier}     {
                            int tok;
                            if ((tok = idl_yystrtok(yytext)) != -1) {
                              return tok;
                            }
                            yylval->identifier = yytext;
                            BEGIN(NO_WS);
                            return IDL_T_IDENTIFIER;
                          }

<INITIAL>"::"             {
                            BEGIN(NO_WS);
                            return IDL_T_COLON_COLON;
                          }

<NO_WS>{identifier}       {
                            int tok;
                            if ((tok = idl_yystrtok(yytext)) != -1) {
                              return tok;
                            }
                            yylval->identifier = yytext;
                            if (yylval->identifier == NULL) {
                              idl_yyerror(yylloc_param, parser, context, yyscanner, "Could not copy identifier");
                            }
                            return IDL_T_NOWS_IDENTIFIER;
                          }

<NO_WS>"::"               {
                            return IDL_T_NOWS_COLON_COLON;
                          }


<INITIAL>"@"              {
                            BEGIN(NO_WS);
                            return '@';
                          }

<*>[ \t]+ { /* ignore whitespace */ }
<*>\n { BEGIN(INITIAL); }

<INITIAL,NO_WS>.          {
                            BEGIN(INITIAL);
                            return yytext[0];
                          }

%%

