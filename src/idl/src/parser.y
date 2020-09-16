/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include <stdlib.h>
#include <string.h>

#include "idl/string.h"
#include "idl/processor.h"
#include "expression.h"
#include "scope.h"
#include "tree.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
#endif

static void yyerror(idl_location_t *loc, idl_processor_t *proc, idl_node_t **, const char *);
static void push(void *list, void *node);

#define TRY_CATCH(action, catch) \
  do { \
    int _ret_; \
    switch ((_ret_ = (action))) { \
      case IDL_RETCODE_OK: \
        break; \
      case IDL_RETCODE_NO_MEMORY: \
        yylen = 0; /* pop right-hand side tokens */ \
        (void)(catch);\
        goto yyexhaustedlab; \
      case IDL_RETCODE_SYNTAX_ERROR: \
        yylen = 0; /* pop right-hand side tokens */ \
        (void)(catch); \
        goto yyabortlab; \
      default: \
        yylen = 0; \
        yyresult = _ret_; \
        (void)(catch); \
        goto yyreturn; \
    } \
  } while (0)

#define TRY(action) \
  TRY_CATCH((action), 0)

#define LOC(first, last) &(idl_location_t){ first, last }

%}

%code provides {
#include "idl/export.h"
#include "idl/processor.h"
IDL_EXPORT int idl_iskeyword(idl_processor_t *proc, const char *str, int nc);
IDL_EXPORT void idl_yypstate_delete_stack(idl_yypstate *yyss);
}

%code requires {
#include "idl/processor.h"
/* convenience macros to complement YYABORT */
#define EXHAUSTED \
  do { \
    yylen = 0; \
    goto yyexhaustedlab; \
  } while(0)
#define ERROR(proc, loc, ...) \
  do { \
    idl_error(proc, loc, __VA_ARGS__); \
    yylen = 0; /* pop right-hand side tokens */ \
    yyresult = IDL_RETCODE_SEMANTIC_ERROR; \
    goto yyreturn; \
  } while(0)

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
  void *node;
  /* expressions */
  idl_literal_t *literal;
  idl_const_expr_t *const_expr;
  idl_constval_t *constval;
  /* simple specifications */
  idl_mask_t kind;
  idl_name_t *name;
  idl_scoped_name_t *scoped_name;
  idl_inherit_spec_t *inherit_spec;
  char *string_literal;
  /* specifications */
  idl_type_spec_t *type_spec;
  idl_sequence_t *sequence;
  idl_string_t *string;
  /* declarations */
  idl_definition_t *definition;
  idl_module_t *module_dcl;
  idl_struct_t *struct_dcl;
  idl_forward_t *forward_dcl;
  idl_member_t *member;
  idl_declarator_t *declarator;
  idl_union_t *union_dcl;
  idl_case_t *_case;
  idl_case_label_t *case_label;
  idl_enum_t *enum_dcl;
  idl_enumerator_t *enumerator;
  idl_typedef_t *typedef_dcl;
  idl_const_t *const_dcl;
  /* annotations */
  idl_annotation_appl_t *annotation_appl;
  idl_annotation_appl_param_t *annotation_appl_param;

  char *str;
  unsigned long long ullng;
  long double ldbl;
}

%define api.pure true
%define api.prefix {idl_yy}
%define api.push-pull push
%define parse.trace

%locations

%param { idl_processor_t *proc }
%param { idl_node_t **nodeptr }


%token-table

%start specification

%type <node> definitions definition type_dcl
             constr_type_dcl struct_dcl union_dcl enum_dcl
%type <type_spec> type_spec simple_type_spec template_type_spec
                  switch_type_spec const_type
%type <constval> positive_int_const
%type <const_expr> const_expr or_expr xor_expr and_expr shift_expr add_expr
                   mult_expr unary_expr primary_expr fixed_array_sizes
                   fixed_array_size
%type <kind> unary_operator base_type_spec floating_pt_type integer_type
             signed_int unsigned_int char_type wide_char_type boolean_type
             octet_type
%type <literal> literal boolean_literal
%type <name> identifier
%type <scoped_name> scoped_name at_scoped_name
%type <inherit_spec> struct_inherit_spec
%type <string_literal> string_literal
%type <sequence> sequence_type
%type <string> string_type
%type <module_dcl> module_dcl module_header
%type <struct_dcl> struct_def struct_header
%type <forward_dcl> struct_forward_dcl union_forward_dcl
%type <member> members member struct_body
%type <union_dcl> union_def union_header
%type <_case> switch_body case element_spec
%type <case_label> case_labels case_label
%type <enum_dcl> enum_def
%type <enumerator> enumerators enumerator
%type <declarator> declarators declarator simple_declarator
                   complex_declarator array_declarator
%type <typedef_dcl> typedef_dcl
%type <const_dcl> const_dcl
%type <annotation_appl> annotation_appls annotation_appl
%type <annotation_appl_param> annotation_appl_params
                              annotation_appl_keyword_param
                              annotation_appl_keyword_params

%destructor { free($$); } <string_literal>

%destructor { idl_delete_name($$); } <name>

%destructor { idl_delete_scoped_name($$); } <scoped_name>

%destructor { idl_delete_inherit_spec($$); } <inherit_spec>

%destructor { idl_delete_node($$); } <node> <type_spec> <constval> <const_expr> <literal> <sequence>
                                     <string> <module_dcl> <struct_dcl> <forward_dcl> <member> <union_dcl>
                                     <_case> <case_label> <enum_dcl> <enumerator> <declarator> <typedef_dcl>
                                     <const_dcl> <annotation_appl> <annotation_appl_param>

/* comments and line comments are not evaluated by the parser, but unique
   token identifiers are required to avoid clashes */
%token IDL_TOKEN_LINE_COMMENT
%token IDL_TOKEN_COMMENT
%token <str> IDL_TOKEN_PP_NUMBER
%token <str> IDL_TOKEN_IDENTIFIER
%token <str> IDL_TOKEN_CHAR_LITERAL
%token <str> IDL_TOKEN_STRING_LITERAL
%token <ullng> IDL_TOKEN_INTEGER_LITERAL
%token <ldbl> IDL_TOKEN_FLOATING_PT_LITERAL

%token IDL_TOKEN_AT "@"

/* scope operators, see scanner.c for details */
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

%token IDL_TOKEN_TRUE "TRUE"
%token IDL_TOKEN_FALSE "FALSE"

%%

specification:
      { *nodeptr = NULL; }
  | definitions
      { *nodeptr = $1; }
  ;

definitions:
    definition
      { $$ = $1; }
  | definitions definition
      { push($1, $2);
        $$ = $1;
      }
  ;

definition:
    annotation_appls module_dcl ';'
      { if (proc->flags & IDL_FLAG_ANNOTATIONS)
          TRY(idl_annotate(proc, $2, $1));
        $$ = $2;
      }
  | annotation_appls const_dcl ';'
      { if (proc->flags & IDL_FLAG_ANNOTATIONS)
          TRY(idl_annotate(proc, $2, $1));
        $$ = $2;
      }
  | annotation_appls type_dcl ';'
      { if (proc->flags & IDL_FLAG_ANNOTATIONS)
          TRY(idl_annotate(proc, $2, $1));
        $$ = $2;
      }
  ;

module_dcl:
    module_header definitions '}'
      { TRY(idl_finalize_module(proc, $1, LOC(@1.first, @3.last), $2));
        $$ = $1;
      }
  ;

module_header:
    "module" identifier '{'
      { TRY(idl_create_module(proc, &$$, LOC(@1.first, @2.last), $2)); }
  ;

scoped_name:
    identifier
      { TRY(idl_create_scoped_name(proc, &$$, &@1, $1, false)); }
  | scope identifier
      { TRY(idl_create_scoped_name(proc, &$$, LOC(@1.first, @2.last), $2, true)); }
  | scoped_name scope identifier
      { TRY(idl_append_to_scoped_name(proc, $1, $3));
        $$ = $1;
      }
  ;

scope:
    IDL_TOKEN_SCOPE
  | IDL_TOKEN_SCOPE_L
  | IDL_TOKEN_SCOPE_R
  | IDL_TOKEN_SCOPE_LR
  ;

const_dcl:
    "const" const_type identifier '=' const_expr
      { TRY(idl_create_const(proc, &$$, LOC(@1.first, @5.last), $2, $3, $5)); }
  ;

const_type:
    integer_type
      { TRY(idl_create_base_type(proc, (idl_base_type_t **)&$$, &@1, $1)); }
  | boolean_type
      { TRY(idl_create_base_type(proc, (idl_base_type_t **)&$$, &@1, $1)); }
  ;

const_expr: or_expr { $$ = $1; } ;

or_expr:
    xor_expr
  | or_expr '|' xor_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_OR_EXPR, $1, $3)); }
  ;

xor_expr:
    and_expr
  | or_expr '^' and_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_XOR_EXPR, $1, $3)); }
  ;

and_expr:
    shift_expr
  | and_expr '&' shift_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_AND_EXPR, $1, $3)); }
  ;

shift_expr:
    add_expr
  | shift_expr ">>" add_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_RSHIFT_EXPR, $1, $3)); }
  | shift_expr "<<" add_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_LSHIFT_EXPR, $1, $3)); }
  ;

add_expr:
    mult_expr
  | add_expr '+' mult_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_ADD_EXPR, $1, $3)); }
  | add_expr '-' mult_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_SUB_EXPR, $1, $3)); }
  ;

mult_expr:
    unary_expr
      { $$ = (void *)$1; }
  | mult_expr '*' unary_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_MULT_EXPR, $1, $3)); }
  | mult_expr '/' unary_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_DIV_EXPR, $1, $3)); }
  | mult_expr '%' unary_expr
      { TRY(idl_create_binary_expr(proc, (idl_binary_expr_t **)&$$, LOC(@1.first, @3.last), IDL_MOD_EXPR, $1, $3)); }
  ;

unary_expr:
    unary_operator primary_expr
      { TRY(idl_create_unary_expr(proc, (idl_unary_expr_t **)&$$, LOC(@1.first, @2.last), $1, $2)); }
  | primary_expr
      { $$ = (idl_const_expr_t *)$1; }
  ;

unary_operator:
    '-' { $$ = IDL_MINUS_EXPR; }
  | '+' { $$ = IDL_PLUS_EXPR; }
  | '~' { $$ = IDL_NOT_EXPR; }
  ;

primary_expr:
    scoped_name
      { idl_node_t *node;
        idl_entry_t *entry;
        TRY(idl_resolve(proc, &entry, $1));
        node = idl_unalias(entry ? entry->node : NULL);
        if (!idl_is_masked(node, IDL_DECL|IDL_CONST) &&
            !idl_is_masked(node, IDL_DECL|IDL_ENUMERATOR))
        {
          static const char errfmt[] =
            "Scoped name '%s' does not resolve to a constant";
          ERROR(proc, &@1, errfmt, $1->flat);
        }
        idl_delete_scoped_name($1);
        $$ = idl_reference((idl_node_t*)entry->node);
      }
  | literal
      { $$ = (idl_const_expr_t *)$1; }
  | '(' const_expr ')'
      { $$ = $2; }
  ;

literal:
    IDL_TOKEN_INTEGER_LITERAL
      { TRY(idl_create_literal(proc, &$$, &@1, IDL_ULLONG));
        $$->value.ullng = $1;
      }
  | boolean_literal
  | string_literal
      { TRY(idl_create_literal(proc, &$$, &@1, IDL_STRING));
        $$->value.str = $1;
      }
  ;

boolean_literal:
    IDL_TOKEN_TRUE
      { TRY(idl_create_literal(proc, &$$, &@1, IDL_BOOL));
        $$->value.bln = true;
      }
  | IDL_TOKEN_FALSE
      { TRY(idl_create_literal(proc, &$$, &@1, IDL_BOOL));
        $$->value.bln = false;
      }
  ;

string_literal:
    IDL_TOKEN_STRING_LITERAL
      { if (!($$ = idl_strdup($1)))
          EXHAUSTED;
      }
  ;

positive_int_const:
    const_expr
      { TRY(idl_evaluate(proc, (idl_node_t **)&$$, $1, IDL_ULLONG)); }
  ;

type_dcl:
    constr_type_dcl { $$ = $1; }
  | typedef_dcl { $$ = $1; }
  ;

type_spec:
    simple_type_spec
  /* building block anonymous types */
  | template_type_spec
  /* embedded-struct-def extension */
  /* not valid in IDL >3.5 */
  /*| constr_type */
  ;

simple_type_spec:
    base_type_spec
      { TRY(idl_create_base_type(proc, (idl_base_type_t **)&$$, &@1, $1)); }
  | scoped_name
      { idl_entry_t *entry;
        TRY(idl_resolve(proc, &entry, $1));
        if (!entry || !idl_is_masked(entry->node, IDL_TYPE)) {
          static const char errfmt[] =
            "Scoped name '%s' does not resolve to a type";
          ERROR(proc, &@1, errfmt, $1->flat);
        }
        idl_delete_scoped_name($1);
        $$ = idl_reference((idl_node_t*)entry->node);
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

floating_pt_type:
    "float" { $$ = IDL_FLOAT; }
  | "double" { $$ = IDL_DOUBLE; }
  | "long" "double" { $$ = IDL_LDOUBLE; }
  ;

integer_type:
    signed_int
  | unsigned_int
  ;

signed_int:
    "short" { $$ = IDL_SHORT; }
  | "long" { $$ = IDL_LONG; }
  | "long" "long" { $$ = IDL_LLONG; }
  /* building block extended data-types */
  | "int8" { $$ = IDL_INT8; }
  | "int16" { $$ = IDL_INT16; }
  | "int32" { $$ = IDL_INT32; }
  | "int64" { $$ = IDL_INT64; }
  ;

unsigned_int:
    "unsigned" "short" { $$ = IDL_USHORT; }
  | "unsigned" "long" { $$ = IDL_ULONG; }
  | "unsigned" "long" "long" { $$ = IDL_ULLONG; }
  /* building block extended data-types */
  | "uint8" { $$ = IDL_UINT8; }
  | "uint16" { $$ = IDL_UINT16; }
  | "uint32" { $$ = IDL_UINT32; }
  | "uint64" { $$ = IDL_UINT64; }
  ;

char_type:
    "char" { $$ = IDL_CHAR; };

wide_char_type:
    "wchar" { $$ = IDL_WCHAR; };

boolean_type:
    "boolean" { $$ = IDL_BOOL; };

octet_type:
    "octet" { $$ = IDL_OCTET; };

template_type_spec:
    sequence_type { $$ = (idl_type_spec_t *)$1; }
  | string_type { $$ = (idl_type_spec_t *)$1; }
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      { TRY(idl_create_sequence(proc, &$$, LOC(@1.first, @6.last), $3, $5)); }
  | "sequence" '<' type_spec '>'
      { TRY(idl_create_sequence(proc, &$$, LOC(@1.first, @4.last), $3, NULL)); }
  ;

string_type:
    "string" '<' positive_int_const '>'
      { TRY(idl_create_string(proc, &$$, LOC(@1.first, @4.last), $3)); }
  | "string"
      { TRY(idl_create_string(proc, &$$, LOC(@1.first, @1.last), NULL)); }
  ;

  /*
    constr_type:
    struct_def { $$ = $1; }
  | union_def { $$ = $1; }
  | enum_def { $$ = $1; }
  ;
  */

constr_type_dcl:
    struct_dcl
  | union_dcl
  | enum_dcl
  ;

struct_dcl:
    struct_def { $$ = $1; }
  | struct_forward_dcl { $$ = $1; }
  ;

struct_def:
    struct_header '{' struct_body '}'
      { TRY(idl_finalize_struct(proc, $1, LOC(@1.first, @4.last), $3));
        $$ = $1;
      }
  ;

struct_header:
    "struct" identifier struct_inherit_spec
      { idl_location_t loc = { @1.first, $3 ? @3.last : @2.last };
        TRY(idl_create_struct(proc, &$$, &loc, $2, $3));
      }
  ;

struct_inherit_spec:
      { $$ = NULL; }
    /* IDL 4.2 section 7.4.13 Building Block Extended Data-Types */
    /* %?{ (proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES) } */
  | ':' scoped_name
      { idl_node_t *node;
        idl_entry_t *entry;
        TRY(idl_resolve(proc, &entry, $2));
        node = idl_unalias(entry->node);
        if (!idl_is_masked(node, IDL_STRUCT) ||
             idl_is_masked(node, IDL_FORWARD))
        {
          static const char errfmt[] =
            "Scoped name '%s' does not resolve to a struct";
          ERROR(proc, &@2, errfmt, $2->flat);
        }
        TRY(idl_create_inherit_spec(proc, &$$, $2, entry->scope));
      }
  ;

struct_body:
    members
      { $$ = $1; }
    /* IDL 4.2 section 7.4.13 Building Block Extended Data-Types */
//  | %?{ (proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES) }
  |   { $$ = NULL; }
  ;

members:
    member
      { $$ = $1; }
  | members member
      { push($1, $2);
        $$ = $1;
      }
  ;

member:
    annotation_appls type_spec declarators ';'
      { TRY(idl_create_member(proc, (idl_member_t **)&$$, LOC(@2.first, @4.last), $2, $3));
        if (proc->flags & IDL_FLAG_ANNOTATIONS)
          TRY_CATCH(idl_annotate(proc, $$, $1), free($$));
      }
  ;

struct_forward_dcl:
    "struct" identifier
      { TRY(idl_create_forward(proc, &$$, LOC(@1.first, @2.last), IDL_STRUCT, $2)); }
  ;

union_dcl:
    union_def { $$ = $1; }
  | union_forward_dcl { $$ = $1; }
  ;

union_def:
    union_header '{' switch_body '}'
      { TRY(idl_finalize_union(proc, $1, LOC(@1.first, @4.last), $3));
        $$ = $1;
      }
  ;

union_header:
    "union" identifier "switch" '(' annotation_appls switch_type_spec ')'
      { TRY(idl_create_union(proc, &$$, LOC(@1.first, @7.last), $2, $6));
        if (proc->flags & IDL_FLAG_ANNOTATIONS)
          TRY_CATCH(idl_annotate(proc, $6, $5), idl_delete_node($$));
      }
  ;

switch_type_spec:
    integer_type
      { TRY(idl_create_base_type(proc, (idl_base_type_t **)&$$, &@1, $1)); }
  | char_type
      { TRY(idl_create_base_type(proc, (idl_base_type_t **)&$$, &@1, $1)); }
  | boolean_type
      { TRY(idl_create_base_type(proc, (idl_base_type_t **)&$$, &@1, $1)); }
  | scoped_name
      { idl_node_t *node;
        idl_entry_t *entry;
        TRY(idl_resolve(proc, &entry, $1));
        node = idl_unalias(entry ? entry->node : NULL);
        if (!idl_is_masked(node, IDL_BASE_TYPE) ||
             idl_is_masked(node, IDL_FLOATING_PT_TYPE))
        {
          static const char fmt[] =
            "Scoped name '%s' does not resolve to a valid switch type";
          ERROR(proc, &@1, fmt, $1->flat);
        }
        idl_delete_scoped_name($1);
        $$ = idl_reference((idl_node_t*)entry->node);
      }
  ;

switch_body:
    case
      { $$ = $1; }
  | switch_body case
      { push($1, $2);
        $$ = $1;
      }
  ;

case:
    case_labels element_spec ';'
      { TRY(idl_finalize_case(proc, $2, &@2, $1));
        $$ = $2;
      }
  ;

case_labels:
    case_label
      { $$ = $1; }
  | case_labels case_label
      { push($1, $2);
        $$ = $1;
      }
  ;

case_label:
    "case" const_expr ':'
      { TRY(idl_create_case_label(proc, &$$, LOC(@1.first, @2.last), $2)); }
  | "default" ':'
      { TRY(idl_create_case_label(proc, &$$, &@1, NULL)); }
  ;

element_spec:
    type_spec declarator
      { TRY(idl_create_case(proc, &$$, LOC(@1.first, @2.last), $1, $2)); }
  ;

union_forward_dcl:
    "union" identifier
      { TRY(idl_create_forward(proc, &$$, LOC(@1.first, @2.last), IDL_UNION, $2)); }
  ;

enum_dcl: enum_def { $$ = $1; } ;

enum_def:
    "enum" identifier '{' enumerators '}'
      { TRY(idl_create_enum(proc, &$$, LOC(@1.first, @5.last), $2, $4)); }
  ;

enumerators:
    enumerator
      { $$ = $1; }
  | enumerators ',' enumerator
      { push($1, $3);
        $$ = $1;
      }
  ;

enumerator:
    annotation_appls identifier
      { TRY(idl_create_enumerator(proc, &$$, &@2, $2));
        if (proc->flags & IDL_FLAG_ANNOTATIONS)
          TRY_CATCH(idl_annotate(proc, $$, $1), free($$));
      }
  ;

array_declarator:
    identifier fixed_array_sizes
      { TRY(idl_create_declarator(proc, &$$, LOC(@1.first, @2.last), $1, $2)); }
  ;

fixed_array_sizes:
    fixed_array_size
      { $$ = $1; }
  | fixed_array_sizes fixed_array_size
      { push($1, $2);
        $$ = $1;
      }
  ;

fixed_array_size:
    '[' positive_int_const ']'
      { $$ = (idl_const_expr_t *)$2; }
  ;

simple_declarator:
    identifier
      { TRY(idl_create_declarator(proc, &$$, &@1, $1, NULL)); }
  ;

complex_declarator: array_declarator ;

typedef_dcl:
    "typedef" type_spec declarators
      { TRY(idl_create_typedef(proc, &$$, LOC(@1.first, @3.last), $2, $3)); }
  ;

declarators:
    declarator
      { $$ = $1; }
  | declarators ',' declarator
      { push($1, $3);
        $$ = $1;
      }
  ;

declarator:
    simple_declarator
  | complex_declarator
  ;

identifier:
    IDL_TOKEN_IDENTIFIER
      { if ($1[0] != '_' && idl_iskeyword(proc, $1, 1))
          ERROR(proc, &@1, "Identifier '%s' collides with a keyword", $1);
        //if (!($$ = idl_strdup($1)))
        //  EXHAUSTED;
        TRY(idl_create_name(proc, &$$, &@1, idl_strdup($1)));
      }
  ;

annotation_appls:
      { $$ = NULL; }
  | annotation_appl
      { $$ = $1; }
  | annotation_appls annotation_appl
      { push($1, $2);
        $$ = $1;
      }

annotation_appl:
    "@" at_scoped_name annotation_appl_params
      { TRY(idl_create_annotation_appl(proc, &$$, LOC(@1.first, @2.last), $2, $3)); }
  ;

at_scoped_name:
    identifier
      { TRY(idl_create_scoped_name(proc, &$$, &@1, $1, false)); }
  | IDL_TOKEN_SCOPE_R identifier
      { TRY(idl_create_scoped_name(proc, &$$, LOC(@1.first, @2.last), $2, true)); }
  | at_scoped_name IDL_TOKEN_SCOPE_LR identifier
      { TRY(idl_append_to_scoped_name(proc, $1, $3));
        $$ = $1;
      }
  ;

annotation_appl_params:
      { $$ = NULL; }
  | '(' const_expr ')'
      { $$ = (idl_annotation_appl_param_t *)$2; }
  | '(' annotation_appl_keyword_params ')'
      { $$ = $2; }
  ;

annotation_appl_keyword_params:
    annotation_appl_keyword_param
      { $$ = $1; }
  | annotation_appl_keyword_params ',' annotation_appl_keyword_param
      { push($1, $3);
        $$ = $1;
      }
  ;

annotation_appl_keyword_param:
    identifier '=' const_expr
      { TRY(idl_create_annotation_appl_param(proc, &$$, &@1, $1, $3)); }
  ;

%%

#if defined(__GNUC__)
_Pragma("GCC diagnostic pop")
_Pragma("GCC diagnostic pop")
#endif

/* copied from yyreturn in Bison generated parser */
void idl_yypstate_delete_stack(idl_yypstate *yyps)
{
#ifndef yyss
# define yyss yyps->yyss
#endif
#ifndef yyssp
# define yyssp yyps->yyssp
#endif
#ifndef yyvsp
# define yyvsp yyps->yyvsp
#endif
#ifndef yylsp
# define yylsp yyps->yylsp
#endif
  if (yyps)
    {
      YY_STACK_PRINT (yyss, yyssp);
      while (yyssp != yyss)
        {
          yydestruct ("Cleanup: popping",
                      yystos[*yyssp], yyvsp, yylsp, NULL, NULL);
          YYPOPSTACK (1);
        }
    }
}

static void
yyerror(idl_location_t *loc, idl_processor_t *proc, idl_node_t **nodeptr, const char *str)
{
  (void)nodeptr;
  idl_error(proc, loc, str);
}

static void push(void *list, void *node)
{
  idl_node_t *last;

  assert(list);
  assert(node);

  for (last = (idl_node_t *)list; last->next; last = last->next) ;
  last->next = (idl_node_t *)node;
  ((idl_node_t *)node)->previous = last;
}

int32_t idl_iskeyword(idl_processor_t *proc, const char *str, int nc)
{
  int toknum = 0;
  int(*cmp)(const char *s1, const char *s2, size_t n);

  assert(str != NULL);

  cmp = (nc ? &idl_strncasecmp : strncmp);

  for (size_t i = 0, n = strlen(str); i < YYNTOKENS && !toknum; i++) {
    if (yytname[i] != 0
        && yytname[i][    0] == '"'
        && cmp(yytname[i] + 1, str, n) == 0
        && yytname[i][n + 1] == '"'
        && yytname[i][n + 2] == '\0') {
      toknum = yytoknum[i];
    }
  }

  switch (toknum) {
    case IDL_TOKEN_INT8:
    case IDL_TOKEN_INT16:
    case IDL_TOKEN_INT32:
    case IDL_TOKEN_INT64:
    case IDL_TOKEN_UINT8:
    case IDL_TOKEN_UINT16:
    case IDL_TOKEN_UINT32:
    case IDL_TOKEN_UINT64:
    case IDL_TOKEN_MAP:
      /* intX and uintX are considered keywords if and only if building block
         extended data-types is enabled */
      if (!(proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES))
        return 0;
      break;
    default:
      break;
  };

  return toknum;
}
