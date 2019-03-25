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
#ifndef DDS_TS_YY_DECL_H
#define DDS_TS_YY_DECL_H

#define YY_DECL int dds_ts_parser_lex \
  (YYSTYPE * yylval_param, \
   YYLTYPE * yylloc_param, \
   yyscan_t yyscanner, \
   dds_ts_context_t *context)

extern YY_DECL;

#define YY_USER_INIT (void)context;

#define YY_NO_INPUT
#define YY_NO_UNPUT

int parser_token_matches_keyword(const char *token, int *token_number);

#define YYTYPE_INT16 int

#endif /* DDS_TS_YY_DECL_H */

