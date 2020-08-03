/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")
#endif

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsts/typetree.h"

#include "idl.h"
#include "tt_create.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
#endif

static void yyerror(idl_location_t *loc, idl_processor_t *proc, const char *);
%}

%code provides {
int idl_istoken(const char *str, int nc);
}

%code requires {

/* convenience macro to complement YYABORT */
#define ABORT(proc, loc, ...) \
  do { idl_error(proc, loc, __VA_ARGS__); YYABORT; } while(0)
#define EXHAUSTED \
  do { goto yyexhaustedlab; } while (0)

/* Make yytoknum available */
#define YYPRINT(A,B,C) YYUSE(A)
/* Use YYLTYPE definition below */
#define IDL_YYLTYPE_IS_DECLARED

#define YYSTYPE IDL_YYSTYPE
#define YYLTYPE IDL_YYLTYPE

typedef struct idl_location YYLTYPE;

#define YYLLOC_DEFAULT(Cur, Rhs, N) \
  do { \
    if (N) { \
      (Cur).first.file = YYRHSLOC(Rhs, 1).first.file; \
      (Cur).first.line = YYRHSLOC(Rhs, 1).first.line; \
      (Cur).first.column = YYRHSLOC(Rhs, 1).first.column; \
    } else { \
      (Cur).first.file = YYRHSLOC(Rhs, 0).last.file; \
      (Cur).first.line = YYRHSLOC(Rhs, 0).last.line; \
      (Cur).first.column = YYRHSLOC(Rhs, 0).last.column; \
    } \
    (Cur).last.line = YYRHSLOC(Rhs, N).last.line; \
    (Cur).last.column = YYRHSLOC(Rhs, N).last.column; \
  } while (0)

#define YYLLOC_INITIAL(Cur, File) \
  do { \
    (Cur).first.file = NULL; \
    (Cur).first.line = 0; \
    (Cur).first.column = 0; \
    (Cur).last.file = (File); \
    (Cur).last.line = 1; \
    (Cur).last.column = 1; \
  } while (0);
}

%union {
  ddsts_flags_t base_type_flags;
  ddsts_type_t *type_ptr;
  ddsts_literal_t literal;
  ddsts_scoped_name_t *scoped_name;
  ddsts_identifier_t identifier;
  char *str;
  long long llng;
  unsigned long long ullng;
}

%define api.pure true
%define api.prefix {idl_yy}
%define api.push-pull push
%define parse.trace

%locations

%param { idl_processor_t *proc }
%initial-action { YYLLOC_INITIAL(@$, proc->files ? proc->files->name : NULL); }


%token-table

%start specification

%token IDL_TOKEN_LINE_COMMENT
%token IDL_TOKEN_COMMENT

%token <str> IDL_TOKEN_PP_NUMBER
%token <str> IDL_TOKEN_IDENTIFIER
%token <str> IDL_TOKEN_CHAR_LITERAL
%token <str> IDL_TOKEN_STRING_LITERAL
%token <ullng> IDL_TOKEN_INTEGER_LITERAL

%type <base_type_flags>
  base_type_spec
  switch_type_spec
  floating_pt_type
  integer_type
  signed_int
  signed_tiny_int
  signed_short_int
  signed_long_int
  signed_longlong_int
  unsigned_int
  unsigned_tiny_int
  unsigned_short_int
  unsigned_long_int
  unsigned_longlong_int
  char_type
  wide_char_type
  boolean_type
  octet_type

%type <type_ptr>
  type_spec
  simple_type_spec
  template_type_spec
  sequence_type
  string_type
  wide_string_type
  fixed_pt_type
  map_type
  struct_type
  struct_def

%destructor { ddsts_free_type($$); } <type_ptr>

%type <scoped_name>
  scoped_name
  at_scoped_name

%destructor { ddsts_free_scoped_name($$); } <scoped_name>

%type <literal>
  positive_int_const
  literal
  const_expr

%destructor { ddsts_free_literal(&($$)); } <literal>

%type <identifier>
  simple_declarator
  identifier

%token IDL_TOKEN_AT "@"

/* scope operators, see idl.l for details */
%token IDL_TOKEN_SCOPE
%token IDL_TOKEN_SCOPE_L
%token IDL_TOKEN_SCOPE_R
%token IDL_TOKEN_SCOPE_LR

/* keywords */
%token IDL_TOKEN_MODULE "module"
%token IDL_TOKEN_CONST "const"
%token IDL_TOKEN_NATIVE "native"
%token IDL_TOKEN_STRUCT "struct"
%token IDL_TOKEN_TYPEDEF "typedef"
%token IDL_TOKEN_UNION "union"
%token IDL_TOKEN_SWITCH "switch"
%token IDL_TOKEN_CASE "case"
%token IDL_TOKEN_DEFAULT "default"
%token IDL_TOKEN_ENUM "enum"
%token IDL_TOKEN_UNSIGNED "unsigned"
%token IDL_TOKEN_FIXED "fixed"
%token IDL_TOKEN_SEQUENCE "sequence"
%token IDL_TOKEN_STRING "string"
%token IDL_TOKEN_WSTRING "wstring"

%token IDL_TOKEN_FLOAT "float"
%token IDL_TOKEN_DOUBLE "double"
%token IDL_TOKEN_SHORT "short"
%token IDL_TOKEN_LONG "long"
%token IDL_TOKEN_CHAR "char"
%token IDL_TOKEN_WCHAR "wchar"
%token IDL_TOKEN_BOOLEAN "boolean"
%token IDL_TOKEN_OCTET "octet"
%token IDL_TOKEN_ANY "any"

%token IDL_TOKEN_MAP "map"
%token IDL_TOKEN_BITSET "bitset"
%token IDL_TOKEN_BITFIELD "bitfield"
%token IDL_TOKEN_BITMASK "bitmask"

%token IDL_TOKEN_INT8 "int8"
%token IDL_TOKEN_INT16 "int16"
%token IDL_TOKEN_INT32 "int32"
%token IDL_TOKEN_INT64 "int64"
%token IDL_TOKEN_UINT8 "uint8"
%token IDL_TOKEN_UINT16 "uint16"
%token IDL_TOKEN_UINT32 "uint32"
%token IDL_TOKEN_UINT64 "uint64"

%%

/* Constant Declaration */

specification:
    definitions
    { ddsts_accept(proc->context); YYACCEPT; }
  ;

definitions:
    definition definitions
  | definition
  ;

definition:
    module_dcl ';'
  | type_dcl ';'
  ;

module_dcl:
    "module" identifier
      {
        if (!ddsts_module_open(proc->context, $2)) {
          EXHAUSTED;
        }
      }
    '{' definitions '}'
      { ddsts_module_close(proc->context); };

at_scoped_name:
    identifier
      {
        if (!ddsts_new_scoped_name(proc->context, 0, false, $1, &($$))) {
          EXHAUSTED;
        }
      }
  | IDL_TOKEN_SCOPE_R identifier
      {
        if (!ddsts_new_scoped_name(proc->context, 0, true, $2, &($$))) {
          EXHAUSTED;
        }
      }
  | at_scoped_name IDL_TOKEN_SCOPE_LR identifier
      {
        if (!ddsts_new_scoped_name(proc->context, $1, false, $3, &($$))) {
          EXHAUSTED;
        }
      }
  ;

scope:
    IDL_TOKEN_SCOPE
  | IDL_TOKEN_SCOPE_L
  | IDL_TOKEN_SCOPE_R
  | IDL_TOKEN_SCOPE_LR
  ;

scoped_name:
    identifier
      {
        if (!ddsts_new_scoped_name(proc->context, 0, false, $1, &($$))) {
          EXHAUSTED;
        }
      }
  | scope identifier
      {
        if (!ddsts_new_scoped_name(proc->context, 0, true, $2, &($$))) {
          EXHAUSTED;
        }
      }
  | scoped_name scope identifier
      {
        if (!ddsts_new_scoped_name(proc->context, $1, false, $3, &($$))) {
          EXHAUSTED;
        }
      }
  ;

const_expr:
    literal
  | '(' const_expr ')'
      { $$ = $2; };

literal:
    IDL_TOKEN_INTEGER_LITERAL
    { $$.flags = DDSTS_INT64 | DDSTS_UNSIGNED; $$.value.ullng = $1; }
  ;

positive_int_const:
    const_expr;

type_dcl:
    constr_type_dcl
  ;

type_spec:
    simple_type_spec
  ;

simple_type_spec:
    base_type_spec
      {
        if (!ddsts_new_base_type(proc->context, $1, &($$))) {
          EXHAUSTED;
        }
      }
  | scoped_name
    {
      ddsts_type_t *type = NULL;
      if (!ddsts_get_type_from_scoped_name(proc->context, $1, &type)) {
        ABORT(proc, &@1, "scoped name cannot be resolved");
      }
      $$ = type;
    }
  ;

base_type_spec:
    integer_type
  | floating_pt_type
  | char_type
  | wide_char_type
  | boolean_type
  | octet_type
  ;

/* Basic Types */
floating_pt_type:
    "float" { $$ = DDSTS_FLOAT; }
  | "double" { $$ = DDSTS_DOUBLE; }
  | "long" "double" { $$ = DDSTS_LONGDOUBLE; };

integer_type:
    signed_int
  | unsigned_int
  ;

signed_int:
    "short" { $$ = DDSTS_INT16; }
  | "long" { $$ = DDSTS_INT32; }
  | "long" "long" { $$ = DDSTS_INT64; }
  ;

unsigned_int:
    "unsigned" "short" { $$ = DDSTS_INT16 | DDSTS_UNSIGNED; }
  | "unsigned" "long" { $$ = DDSTS_INT32 | DDSTS_UNSIGNED; }
  | "unsigned" "long" "long" { $$ = DDSTS_INT64 | DDSTS_UNSIGNED; }
  ;

char_type:
    "char" { $$ = DDSTS_CHAR; };

wide_char_type:
    "wchar" { $$ = DDSTS_CHAR | DDSTS_WIDE; };

boolean_type:
    "boolean" { $$ = DDSTS_BOOLEAN; };

octet_type:
    "octet" { $$ = DDSTS_OCTET; };

template_type_spec:
    sequence_type
  | string_type
  | wide_string_type
  | fixed_pt_type
  | struct_type
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      {
        if (!ddsts_new_sequence(proc->context, $3, &($5), &($$))) {
          EXHAUSTED;
        }
      }
  | "sequence" '<' type_spec '>'
      {
        if (!ddsts_new_sequence_unbound(proc->context, $3, &($$))) {
          EXHAUSTED;
        }
      }
  ;

string_type:
    "string" '<' positive_int_const '>'
      {
        if (!ddsts_new_string(proc->context, &($3), &($$))) {
          EXHAUSTED;
        }
      }
  | "string"
      {
        if (!ddsts_new_string_unbound(proc->context, &($$))) {
          EXHAUSTED;
        }
      }
  ;

wide_string_type:
    "wstring" '<' positive_int_const '>'
      {
        if (!ddsts_new_wide_string(proc->context, &($3), &($$))) {
          EXHAUSTED;
        }
      }
  | "wstring"
      {
        if (!ddsts_new_wide_string_unbound(proc->context, &($$))) {
          EXHAUSTED;
        }
      }
  ;

fixed_pt_type:
    "fixed" '<' positive_int_const ',' positive_int_const '>'
      {
        if (!ddsts_new_fixed_pt(proc->context, &($3), &($5), &($$))) {
          EXHAUSTED;
        }
      }
  ;

/* Annonimous struct extension: */
struct_type:
    "struct" '{'
      {
        if (!ddsts_add_struct_open(proc->context, NULL)) {
          EXHAUSTED;
        }
      }
    members '}'
      { ddsts_struct_close(proc->context, &($$)); }
  ;

constr_type_dcl:
    struct_dcl
  | union_dcl
  ;

struct_dcl:
    struct_def
  | struct_forward_dcl
  ;

struct_def:
    "struct" identifier '{'
      {
        if (!ddsts_add_struct_open(proc->context, $2)) {
          EXHAUSTED;
        }
      }
    members '}'
      { ddsts_struct_close(proc->context, &($$)); }
  ;
members:
    member members
  | member
  ;

member:
    annotation_appls type_spec
      {
        if (!ddsts_add_struct_member(proc->context, &($2))) {
          ABORT(proc, &@2, "forward struct used as type for member declaration");
        }
      }
    declarators ';'
      {
        ddsts_struct_member_close(proc->context);
      }
  | type_spec
      {
        if (!ddsts_add_struct_member(proc->context, &($1))) {
          ABORT(proc, &@1, "forward struct used as type for member declaration");
        }
      }
    declarators ';'
      { ddsts_struct_member_close(proc->context); }
/* Embedded struct extension: */
  | struct_def { ddsts_add_struct_member(proc->context, &($1)); }
    declarators ';'
  ;

struct_forward_dcl:
    "struct" identifier
      {
        if (!ddsts_add_struct_forward(proc->context, $2)) {
          EXHAUSTED;
        }
      };

union_dcl:
    union_def
  | union_forward_dcl
  ;

union_def:
    "union" identifier
       {
         if (!ddsts_add_union_open(proc->context, $2)) {
           EXHAUSTED;
         }
       }
    "switch" '(' switch_type_spec ')'
       {
         if (!ddsts_union_set_switch_type(proc->context, $6)) {
           EXHAUSTED;
         }
       }
    '{' switch_body '}'
       { ddsts_union_close(proc->context); }
  ;

switch_type_spec:
    integer_type
  | char_type
  | boolean_type
  | scoped_name
    {
      ddsts_type_t *type = NULL;
      if (!ddsts_get_type_from_scoped_name(proc->context, $1, &type)) {
        ABORT(proc, &@1, "scoped name cannot be resolved");
      }
      if (!(DDSTS_TYPE_OF(type) & DDSTS_BASIC_TYPES)) {
        ABORT(proc, &@1, "scoped name does not resolve to a basic type");
      }
      $$ = DDSTS_TYPE_OF(type);
    }
  ;

switch_body: cases ;
cases:
    case cases
  | case
  ;

case:
    case_labels element_spec ';'
  ;

case_labels:
    case_label case_labels
  | case_label
  ;

case_label:
    "case" const_expr ':'
      {
        switch (ddsts_union_add_case_label(proc->context, &($2))) {
          case DDS_RETCODE_OUT_OF_RESOURCES:
            EXHAUSTED;
          case DDS_RETCODE_OK:
            break;
          default:
            YYABORT;
        }
      }
  | "default" ':'
      {
        if (!ddsts_union_add_case_default(proc->context)) {
          EXHAUSTED;
        }
      }
  ;

element_spec:
    type_spec
      {
        if (!ddsts_union_add_element(proc->context, &($1))) {
          EXHAUSTED;
        }
      }
    declarator
  ;

union_forward_dcl:
    "union" identifier
      {
        if (!ddsts_add_union_forward(proc->context, $2)) {
          EXHAUSTED;
        }
      }
  ;

array_declarator:
    identifier fixed_array_sizes
      {
        switch (ddsts_add_declarator(proc->context, $1)) {
          case DDS_RETCODE_OUT_OF_RESOURCES:
            EXHAUSTED;
          case DDS_RETCODE_OK:
            break;
          default:
            YYABORT;
        }
      }
  ;

fixed_array_sizes:
    fixed_array_size fixed_array_sizes
  | fixed_array_size
  ;

fixed_array_size:
    '[' positive_int_const ']'
      {
        if (!ddsts_add_array_size(proc->context, &($2))) {
          EXHAUSTED;
        }
      }
  ;

simple_declarator: identifier ;

declarators:
    declarator ',' declarators
  | declarator
  ;

declarator:
    simple_declarator
      {
        switch (ddsts_add_declarator(proc->context, $1)) {
          case DDS_RETCODE_OUT_OF_RESOURCES:
            EXHAUSTED;
          case DDS_RETCODE_OK:
            break;
          default:
            YYABORT;
        }
      };


/* From Building Block Extended Data-Types: */
struct_def:
    "struct" identifier ':' scoped_name '{'
      {
        if (!ddsts_add_struct_extension_open(proc->context, $2, $4)) {
          EXHAUSTED;
        }
      }
    members '}'
      { ddsts_struct_close(proc->context, &($$)); }
  | "struct" identifier '{'
      {
        if (!ddsts_add_struct_open(proc->context, $2)) {
          EXHAUSTED;
        }
      }
    '}'
      { ddsts_struct_empty_close(proc->context, &($$)); }
  ;

template_type_spec:
     map_type
  ;

map_type:
    "map" '<' type_spec ',' type_spec ',' positive_int_const '>'
      {
        if (!ddsts_new_map(proc->context, $3, $5, &($7), &($$))) {
          EXHAUSTED;
        }
      }
  | "map" '<' type_spec ',' type_spec '>'
      {
        if (!ddsts_new_map_unbound(proc->context, $3, $5, &($$))) {
          EXHAUSTED;
        }
      }
  ;

signed_int:
    signed_tiny_int
  | signed_short_int
  | signed_long_int
  | signed_longlong_int
  ;

unsigned_int:
    unsigned_tiny_int
  | unsigned_short_int
  | unsigned_long_int
  | unsigned_longlong_int
  ;

signed_tiny_int: "int8" { $$ = DDSTS_INT8; };
unsigned_tiny_int: "uint8" { $$ = DDSTS_INT8 | DDSTS_UNSIGNED; };
signed_short_int: "int16" { $$ = DDSTS_INT16; };
signed_long_int: "int32" { $$ = DDSTS_INT32; };
signed_longlong_int: "int64" { $$ = DDSTS_INT64; };
unsigned_short_int: "uint16" { $$ = DDSTS_INT16 | DDSTS_UNSIGNED; };
unsigned_long_int: "uint32" { $$ = DDSTS_INT32 | DDSTS_UNSIGNED; };
unsigned_longlong_int: "uint64" { $$ = DDSTS_INT64 | DDSTS_UNSIGNED; };

/* From Building Block Anonymous Types: */
type_spec: template_type_spec ;
declarator: array_declarator ;


/* From Building Block Annotations (minimal for support of @key): */

annotation_appls:
    annotation_appl annotation_appls
  | annotation_appl
  ;

annotation_appl:
    "@" at_scoped_name
    {
      if (!ddsts_add_annotation(proc->context, $2)) {
        EXHAUSTED;
      }
    }
  ;

identifier:
    IDL_TOKEN_IDENTIFIER
      {
        size_t off = 0;
        if ($1[0] == '_') {
          off = 1;
        } else if (idl_istoken($1, 1)) {
          ABORT(proc, &@1, "identifier '%s' collides with a keyword", $1);
        }
        if (!ddsts_context_copy_identifier(proc->context, $1 + off, &($$))) {
          EXHAUSTED;
        }
      };

%%

#if defined(__GNUC__)
_Pragma("GCC diagnostic pop")
_Pragma("GCC diagnostic pop")
#endif

static void
yyerror(idl_location_t *loc, idl_processor_t *proc, const char *str)
{
  idl_error(proc, loc, str);
}

int32_t idl_istoken(const char *str, int nc)
{
  size_t i, n;
  int(*cmp)(const char *s1, const char *s2, size_t n);

  assert(str != NULL);

  cmp = (nc ? &ddsrt_strncasecmp : strncmp);
  for (i = 0, n = strlen(str); i < YYNTOKENS; i++) {
    if (yytname[i] != 0
        && yytname[i][    0] == '"'
        && cmp(yytname[i] + 1, str, n) == 0
        && yytname[i][n + 1] == '"'
        && yytname[i][n + 2] == '\0') {
      return yytoknum[i];
    }
  }

  return 0;
}
