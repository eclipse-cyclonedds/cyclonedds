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
%{
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/ddsrt/strtol.h"

/* YYSTYPE and YYLTYPE defines are required because idl.y defines api.prefix */
#define YYSTYPE DDS_TS_PARSER_STYPE
#define YYLTYPE DDS_TS_PARSER_LTYPE

#include "idl.parser.h"
/* YY_DECL must be defined here because yyparse passes an additional argument */
#include "yy_decl.h"

#define YY_USER_ACTION \
    yylloc->first_line = yylloc->last_line; \
    yylloc->first_column = yylloc->last_column; \
    for(int i = 0; yytext[i] != '\0'; i++) { \
      if(yytext[i] == '\n') { \
        yylloc->last_line++; \
        yylloc->last_column = 0; \
      } \
      else { \
        yylloc->last_column++; \
      } \
    }

#define yyerror dds_ts_parser_error
int yyerror(YYLTYPE *yylloc, yyscan_t yyscanner, dds_ts_context_t *context, char *text);

%}

%x MULTILINE_COMMENT
%x COMMENT
%x INSTRING
%x INWSTRING

%option noyywrap
%option nounistd
%option never-interactive
%option prefix="dds_ts_parser_"
%option reentrant
%option bison-bridge
%option bison-locations
%option never-interactive

number                    [1-9][0-9]*
octal_number              0[0-7]*
hexadecimal_number        0[xX][a-fA-F0-9]*
integer_literal           ({number}|{octal_number}|{hexadecimal_number})
identifier                [a-zA-Z_][a-zA-Z0-9_]*

%%

[ \t\r]                   { }
[\n]                      { }

"/*"                      { BEGIN(MULTILINE_COMMENT); }
<MULTILINE_COMMENT>.      { }
<MULTILINE_COMMENT>"\n"   { }
<MULTILINE_COMMENT>"*/"   { BEGIN(INITIAL); }

"//"                      { BEGIN(COMMENT); }
<COMMENT>"\n"             { BEGIN(INITIAL); }
<COMMENT>.                { }


{integer_literal}         {
                            yylval->literal.flags = DDS_TS_UNSIGNED_LONG_LONG_TYPE;
                            yylval->literal.value.ullng = 0;
                            /* strtoll recognizes if the value is dec, oct or hex if base is zero */
                            if (ddsrt_strtoull(yytext, NULL, 0, &yylval->literal.value.ullng) != DDS_RETCODE_OK) {
                              yyerror(yylloc_param, yyscanner, context, "Integer value invalid");
                            }
                            return INTEGER_LITERAL;
                          }

{identifier}              {
                            int token_number;
                            if (parser_token_matches_keyword(yytext, &token_number)) {
                              return token_number;
                            }
                            yylval->identifier = yytext;
                            if (yylval->identifier == NULL) {
                              yyerror(yylloc_param, yyscanner, context, "Could not copy identifier");
                            }
                            return IDENTIFIER;
                          }

.                         { return yytext[0]; }

%%

