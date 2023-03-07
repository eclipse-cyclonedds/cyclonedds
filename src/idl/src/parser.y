/*
 * Copyright(c) 2021 to 2022 ZettaScale Technology and others
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
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/string.h"
#include "idl/processor.h"
#include "idl/heap.h"
#include "annotation.h"
#include "expression.h"
#include "scope.h"
#include "symbol.h"
#include "tree.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")
#if (__GNUC__ >= 10)
_Pragma("GCC diagnostic ignored \"-Wanalyzer-free-of-non-heap\"")
_Pragma("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"")
#endif
#endif

static void yyerror(idl_location_t *, idl_pstate_t *, idl_retcode_t *, const char *);

/* convenience macros to complement YYABORT */
#define NO_MEMORY() \
  do { \
    yylen = 0; \
    *result = IDL_RETCODE_NO_MEMORY; \
    YYABORT; \
  } while(0)

#define SEMANTIC_ERROR(loc, ...) \
  do { \
    idl_error(pstate, loc, __VA_ARGS__); \
    yylen = 0; /* pop right-hand side tokens */ \
    *result = IDL_RETCODE_SEMANTIC_ERROR; \
    YYABORT; \
  } while(0)

#define YYLLOC_DEFAULT(Cur, Rhs, N) \
  do { \
    if (N) { \
      (Cur).first = YYRHSLOC(Rhs, 1).first; \
    } else { \
      (Cur).first = YYRHSLOC(Rhs, 0).last; \
    } \
    (Cur).last = YYRHSLOC(Rhs, N).last; \
  } while(0)

#define TRY_EXCEPT(action, except) \
  do { \
    int _ret_; \
    switch ((_ret_ = (action))) { \
      case IDL_RETCODE_OK: \
        break; \
      default: \
        yylen = 0; \
        (void)(except); \
        *result = (_ret_); \
        YYABORT; \
        break; \
    } \
  } while(0)

#define TRY(action) \
  TRY_EXCEPT((action), 0)
%}

%code requires {
#include "tree.h"

/* make yytoknum available */
#define YYPRINT(A,B,C) (void)0
/* use YYLTYPE definition below */
#define IDL_YYLTYPE_IS_DECLARED
typedef struct idl_location IDL_YYLTYPE;

#define LOC(first, last) \
  &(IDL_YYLTYPE){ first, last }
}

%code provides {
int idl_iskeyword(idl_pstate_t *pstate, const char *str, int nc);
void idl_yypstate_delete_stack(idl_yypstate *yyps);
}

%union {
  void *node;
  /* expressions */
  idl_literal_t *literal;
  idl_const_expr_t *const_expr;
  /* simple specifications */
  idl_mask_t kind;
  idl_name_t *name;
  idl_scoped_name_t *scoped_name;
  idl_inherit_spec_t *inherit_spec;
  char *string_literal;
  /* specifications */
  idl_switch_type_spec_t *switch_type_spec;
  idl_type_spec_t *type_spec;
  idl_sequence_t *sequence;
  idl_string_t *string;
  /* declarations */
  idl_definition_t *definition;
  idl_module_t *module_dcl;
  idl_struct_t *struct_dcl;
  idl_forward_t *forward;
  idl_member_t *member;
  idl_declarator_t *declarator;
  idl_union_t *union_dcl;
  idl_case_t *_case;
  idl_case_label_t *case_label;
  idl_enum_t *enum_dcl;
  idl_enumerator_t *enumerator;
  idl_bitmask_t *bitmask_dcl;
  idl_bit_value_t *bit_value;
  idl_typedef_t *typedef_dcl;
  idl_const_t *const_dcl;
  /* annotations */
  idl_annotation_t *annotation;
  idl_annotation_member_t *annotation_member;
  idl_annotation_appl_t *annotation_appl;
  idl_annotation_appl_param_t *annotation_appl_param;

  bool bln;
  char *str;
  char chr;
  unsigned long long ullng;
  long double ldbl;
}

%define api.pure true
%define api.prefix {idl_yy}
%define api.push-pull push
%define parse.trace

%locations

%param { idl_pstate_t *pstate }
%parse-param { idl_retcode_t *result }

%token-table

%start specification

%type <node> definitions definition type_dcl
             constr_type_dcl struct_dcl union_dcl enum_dcl bitmask_dcl
%type <type_spec> type_spec simple_type_spec template_type_spec
                  switch_type_spec const_type annotation_member_type
                  any_const_type struct_inherit_spec
%type <literal> literal positive_int_const fixed_array_size
%type <const_expr> const_expr or_expr xor_expr and_expr shift_expr add_expr
                   mult_expr unary_expr primary_expr fixed_array_sizes
                   annotation_member_default
%type <kind> shift_operator add_operator mult_operator unary_operator base_type_spec floating_pt_type integer_type
             signed_int unsigned_int char_type wide_char_type boolean_type
             octet_type
%type <bln> boolean_literal
%type <name> identifier
%type <scoped_name> scoped_name annotation_appl_name
%type <string_literal> string_literal
%type <sequence> sequence_type
%type <string> string_type
%type <module_dcl> module_dcl module_header
%type <struct_dcl> struct_def struct_header
%type <member> members member struct_body
%type <union_dcl> union_def union_header
%type <switch_type_spec> switch_header
%type <_case> switch_body case element_spec
%type <case_label> case_labels case_label
%type <forward> struct_forward_dcl union_forward_dcl
%type <enum_dcl> enum_def
%type <enumerator> enumerators enumerator
%type <bitmask_dcl> bitmask_def
%type <bit_value> bit_values bit_value
%type <declarator> declarators declarator simple_declarator
                   complex_declarator array_declarator
%type <typedef_dcl> typedef_dcl
%type <const_dcl> const_dcl
%type <annotation> annotation_dcl annotation_header
%type <annotation_member> annotation_body annotation_member
%type <annotation_appl> annotations annotation_appl annotation_appls annotation_appl_header
%type <annotation_appl_param> annotation_appl_params
                              annotation_appl_keyword_param
                              annotation_appl_keyword_params

%destructor { idl_free($$); } <string_literal>

%destructor { idl_delete_name($$); }
  <name>

%destructor { idl_delete_scoped_name($$); }
  <scoped_name>

%destructor { idl_unreference_node($$); }
  <type_spec> <const_expr>

%destructor { idl_delete_node($$); } <node> <literal> <sequence>
                                     <string> <module_dcl> <struct_dcl> <member> <union_dcl>
                                     <_case> <case_label> <enum_dcl> <enumerator> <bitmask_dcl> <bit_value> <declarator> <typedef_dcl>
                                     <const_dcl> <annotation> <annotation_member> <annotation_appl> <annotation_appl_param> <forward>
                                     <switch_type_spec>

%token IDL_TOKEN_LINE_COMMENT
%token IDL_TOKEN_COMMENT
%token <str> IDL_TOKEN_PP_NUMBER
%token <str> IDL_TOKEN_IDENTIFIER
%token <chr> IDL_TOKEN_CHAR_LITERAL
%token <str> IDL_TOKEN_STRING_LITERAL
%token <ullng> IDL_TOKEN_INTEGER_LITERAL
%token <ldbl> IDL_TOKEN_FLOATING_PT_LITERAL

%token IDL_TOKEN_ANNOTATION_SYMBOL "@"
%token IDL_TOKEN_ANNOTATION "annotation"

/* scope operators, see scanner.c for details */
%token IDL_TOKEN_SCOPE
%token IDL_TOKEN_SCOPE_NO_SPACE

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

%token IDL_TOKEN_LSHIFT "<<"
%token IDL_TOKEN_RSHIFT ">>"

%%

specification:
    %empty
      { pstate->root = NULL; }
  | definitions
      { pstate->root = $1; }
  ;

definitions:
    definition
      { $$ = $1; }
  | definitions definition
      { $$ = idl_push_node($1, $2); }
  ;

definition:
    annotation_dcl ';'
      { $$ = $1; }
  | annotations module_dcl ';'
      { TRY(idl_annotate(pstate, $2, $1));
        $$ = $2;
      }
  | annotations const_dcl ';'
      { TRY(idl_annotate(pstate, $2, $1));
        $$ = $2;
      }
  | annotations type_dcl ';'
      { TRY(idl_annotate(pstate, $2, $1));
        $$ = $2;
      }
  ;

module_dcl:
    module_header '{' definitions '}'
      { TRY(idl_finalize_module(pstate, LOC(@1.first, @4.last), $1, $3));
        $$ = $1;
      }
  ;

module_header:
    "module" identifier
      { TRY(idl_create_module(pstate, LOC(@1.first, @2.last), $2, &$$)); }
  ;

scoped_name:
    identifier
      { TRY(idl_create_scoped_name(pstate, &@1, $1, false, &$$)); }
  | IDL_TOKEN_SCOPE identifier
      { TRY(idl_create_scoped_name(pstate, LOC(@1.first, @2.last), $2, true, &$$)); }
  | scoped_name IDL_TOKEN_SCOPE identifier
      { TRY(idl_push_scoped_name(pstate, $1, $3));
        $$ = $1;
      }
  ;

const_dcl:
    "const" const_type identifier '=' const_expr
      { TRY(idl_create_const(pstate, LOC(@1.first, @5.last), $2, $3, $5, &$$)); }
  ;

const_type:
    integer_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | floating_pt_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | char_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | boolean_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | octet_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | string_type
      { $$ = (idl_type_spec_t *)$1; }
  | scoped_name
      { idl_node_t *node;
        const idl_declaration_t *declaration;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a valid constant type";
        TRY(idl_resolve(pstate, 0u, $1, &declaration));
        node = idl_unalias(declaration->node);
        if (!(idl_mask(node) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM|IDL_BITMASK)))
          SEMANTIC_ERROR(&@1, fmt, $1->identifier);
        $$ = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name($1);
      }
  ;

const_expr: or_expr { $$ = $1; } ;

or_expr:
    xor_expr
  | or_expr '|' xor_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &@2, IDL_OR, $1, $3, &$$));
      }
  ;

xor_expr:
    and_expr
  | xor_expr '^' and_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &@2, IDL_XOR, $1, $3, &$$));
      }
  ;

and_expr:
    shift_expr
  | and_expr '&' shift_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &@2, IDL_AND, $1, $3, &$$));
      }
  ;

shift_expr:
    add_expr
  | shift_expr shift_operator add_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &@2, $2, $1, $3, &$$));
      }
  ;

shift_operator:
    ">>" { $$ = IDL_RSHIFT; }
  | "<<" { $$ = IDL_LSHIFT; }

add_expr:
    mult_expr
  | add_expr add_operator mult_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &@2, $2, $1, $3, &$$));
      }
  ;

add_operator:
    '+' { $$ = IDL_ADD; }
  | '-' { $$ = IDL_SUBTRACT; }

mult_expr:
    unary_expr
      { $$ = $1; }
  | mult_expr mult_operator unary_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &@2, $2, $1, $3, &$$));
      }
  ;

mult_operator:
    '*' { $$ = IDL_MULTIPLY; }
  | '/' { $$ = IDL_DIVIDE; }
  | '%' { $$ = IDL_MODULO; }

unary_expr:
    unary_operator primary_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_unary_expr(pstate, &@1, $1, $2, &$$));
      }
  | primary_expr
      { $$ = $1; }
  ;

unary_operator:
    '-' { $$ = IDL_MINUS; }
  | '+' { $$ = IDL_PLUS; }
  | '~' { $$ = IDL_NOT; }
  ;

primary_expr:
    scoped_name
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          /* disregard scoped names in application of unknown annotations.
             names may or may not have significance in the scope of the
             (builtin) annotation, stick to syntax checks */
          const idl_declaration_t *declaration = NULL;
          static const char fmt[] =
            "Scoped name '%s' does not resolve to an enumerator or a constant";
          TRY(idl_resolve(pstate, 0u, $1, &declaration));
          if (!(idl_mask(declaration->node) & (IDL_CONST|IDL_ENUMERATOR|IDL_BIT_VALUE)))
            SEMANTIC_ERROR(&@1, fmt, $1->identifier);
          $$ = idl_reference_node((idl_node_t *)declaration->node);
        }
        idl_delete_scoped_name($1);
      }
  | literal
      { $$ = $1; }
  | '(' const_expr ')'
      { $$ = $2; }
  ;

literal:
    IDL_TOKEN_INTEGER_LITERAL
      { idl_type_t type;
        idl_literal_t literal;
        $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if ($1 <= INT32_MAX) {
          type = IDL_LONG;
          literal.value.int32 = (int32_t)$1;
        } else if ($1 <= UINT32_MAX) {
          type = IDL_ULONG;
          literal.value.uint32 = (uint32_t)$1;
        } else if ($1 <= INT64_MAX) {
          type = IDL_LLONG;
          literal.value.int64 = (int64_t)$1;
        } else {
          type = IDL_ULLONG;
          literal.value.uint64 = (uint64_t)$1;
        }
        TRY(idl_create_literal(pstate, &@1, type, &$$));
        $$->value = literal.value;
      }
  | IDL_TOKEN_FLOATING_PT_LITERAL
      { idl_type_t type;
        idl_literal_t literal;
        $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
#if __MINGW32__
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"")
#endif
        if (isnan((double)$1) || isinf((double)$1)) {
#if __MINGW32__
_Pragma("GCC diagnostic pop")
#endif
          type = IDL_LDOUBLE;
          literal.value.ldbl = $1;
        } else {
          type = IDL_DOUBLE;
          literal.value.dbl = (double)$1;
        }
        TRY(idl_create_literal(pstate, &@1, type, &$$));
        $$->value = literal.value;
      }
  | IDL_TOKEN_CHAR_LITERAL
      { $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &@1, IDL_CHAR, &$$));
        $$->value.chr = $1;
      }
  | boolean_literal
      { $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &@1, IDL_BOOL, &$$));
        $$->value.bln = $1;
      }
  | string_literal
      { $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &@1, IDL_STRING, &$$));
        $$->value.str = $1;
      }
  ;

boolean_literal:
    IDL_TOKEN_TRUE
      { $$ = true; }
  | IDL_TOKEN_FALSE
      { $$ = false; }
  ;

string_literal:
    IDL_TOKEN_STRING_LITERAL
      { $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if (!($$ = idl_strdup($1)))
          NO_MEMORY();
      }
  | string_literal IDL_TOKEN_STRING_LITERAL
      { size_t n1, n2;
        $$ = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        /* adjacent string literals are concatenated */
        n1 = strlen($1);
        n2 = strlen($2);
        if (!($$ = idl_realloc($1, n1+n2+1)))
          NO_MEMORY();
        memmove($$+n1, $2, n2);
        $$[n1+n2] = '\0';
      }
  ;

positive_int_const:
    const_expr
      { TRY(idl_evaluate(pstate, $1, IDL_ULONG, &$$)); }
  ;

type_dcl:
    constr_type_dcl { $$ = $1; }
  | typedef_dcl { $$ = $1; }
  ;

type_spec:
    simple_type_spec
  /* building block anonymous types */
  | template_type_spec
  ;

simple_type_spec:
    base_type_spec
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | scoped_name
      { const idl_declaration_t *declaration = NULL;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a type";
        TRY(idl_resolve(pstate, 0u, $1, &declaration));
        if (!declaration || !idl_is_type_spec(declaration->node))
          SEMANTIC_ERROR(&@1, fmt, $1->identifier);
        $$ = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name($1);
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
    sequence_type { $$ = $1; }
  | string_type   { $$ = $1; }
  ;

sequence_type:
    "sequence" '<' type_spec ',' positive_int_const '>'
      { TRY(idl_create_sequence(pstate, LOC(@1.first, @6.last), $3, $5, &$$)); }
  | "sequence" '<' type_spec '>'
      { TRY(idl_create_sequence(pstate, LOC(@1.first, @4.last), $3, NULL, &$$)); }
  ;

string_type:
    "string" '<' positive_int_const '>'
      { TRY(idl_create_string(pstate, LOC(@1.first, @4.last), $3, &$$)); }
  | "string"
      { TRY(idl_create_string(pstate, LOC(@1.first, @1.last), NULL, &$$)); }
  ;

constr_type_dcl:
    struct_dcl
  | union_dcl
  | enum_dcl
  | bitmask_dcl
  ;

struct_dcl:
    struct_def { $$ = $1; }
  | struct_forward_dcl { $$ = $1; }
  ;

struct_forward_dcl:
    "struct" identifier
      { TRY(idl_create_forward(pstate, &@1, $2, IDL_STRUCT, &$$)); }
  ;

struct_def:
    struct_header '{' struct_body '}'
      { TRY(idl_finalize_struct(pstate, LOC(@1.first, @4.last), $1, $3));
        $$ = $1;
      }
  ;

struct_header:
    "struct" identifier struct_inherit_spec
      { TRY(idl_create_struct(pstate, LOC(@1.first, $3 ? @3.last : @2.last), $2, $3, &$$)); }
  ;

struct_inherit_spec:
    %empty  { $$ = NULL; }
    /* IDL 4.2 section 7.4.13 Building Block Extended Data-Types */
    /* %?{ (proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES) } */
  | ':' scoped_name
      { idl_node_t *node;
        const idl_declaration_t *declaration;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a struct";
        TRY(idl_resolve(pstate, 0u, $2, &declaration));
        node = idl_unalias(declaration->node);
        if (!idl_is_struct(node))
          SEMANTIC_ERROR(&@2, fmt, $2->identifier);
        TRY(idl_create_inherit_spec(pstate, &@2, idl_reference_node(node), &$$));
        idl_delete_scoped_name($2);
      }
  ;

struct_body:
    members
      { $$ = $1; }
    /* IDL 4.2 section 7.4.13 Building Block Extended Data-Types */
    /* %?{ (proc->flags & IDL_FLAG_EXTENDED_DATA_TYPES) } */
  | %empty
      { $$ = NULL; }
  ;

members:
    member
      { $$ = $1; }
  | members member
      { $$ = idl_push_node($1, $2); }
  ;

member:
    annotations type_spec declarators ';'
      { TRY(idl_create_member(pstate, LOC(@2.first, @4.last), $2, $3, &$$));
        TRY_EXCEPT(idl_annotate(pstate, $$, $1), idl_free($$));
      }
  ;

union_dcl:
    union_def { $$ = $1; }
  | union_forward_dcl { $$ = $1; }
  ;

union_def:
    union_header '{' switch_body '}'
      { TRY(idl_finalize_union(pstate, LOC(@1.first, @4.last), $1, $3));
        $$ = $1;
      }
  ;

union_forward_dcl:
    "union" identifier
      { TRY(idl_create_forward(pstate, &@1, $2, IDL_UNION, &$$)); }
  ;

union_header:
    "union" identifier switch_header
      { TRY(idl_create_union(pstate, LOC(@1.first, @3.last), $2, $3, &$$)); }
  ;

switch_header:
    "switch" '(' annotations switch_type_spec ')'
      { /* switch_header action is a separate non-terminal, as opposed to a
           mid-rule action, to avoid freeing the type specifier twice (once
           through destruction of the type-spec and once through destruction
           of the switch-type-spec) if union creation fails */
        TRY(idl_create_switch_type_spec(pstate, &@4, $4, &$$));
        TRY_EXCEPT(idl_annotate(pstate, $$, $3), idl_delete_node($$));
      }
  ;

switch_type_spec:
    integer_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | char_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | boolean_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | scoped_name
      { const idl_declaration_t *declaration;
        TRY(idl_resolve(pstate, 0u, $1, &declaration));
        idl_delete_scoped_name($1);
        $$ = idl_reference_node((idl_node_t *)declaration->node);
      }
  | wide_char_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  | octet_type
      { TRY(idl_create_base_type(pstate, &@1, $1, &$$)); }
  ;

switch_body:
    case
      { $$ = $1; }
  | switch_body case
      { $$ = idl_push_node($1, $2); }
  ;

case:
    case_labels element_spec ';'
      { TRY(idl_finalize_case(pstate, &@2, $2, $1));
        $$ = $2;
      }
  ;

case_labels:
    case_label
      { $$ = $1; }
  | case_labels case_label
      { $$ = idl_push_node($1, $2); }
  ;

case_label:
    "case" const_expr ':'
      { TRY(idl_create_case_label(pstate, LOC(@1.first, @2.last), $2, &$$)); }
  | "default" ':'
      { TRY(idl_create_case_label(pstate, &@1, NULL, &$$)); }
  ;

element_spec:
    /* some annotations may also occur on the union branch definitions (@id, @hashid, @external, @try_construct)
       as defined in [XTypes v1.3] Table 21 */
    annotations type_spec declarator
      { TRY(idl_create_case(pstate, LOC(@1.first, @3.last), $2, $3, &$$));
        TRY_EXCEPT(idl_annotate(pstate, $$, $1), idl_free($$));
      }
  ;

enum_dcl: enum_def { $$ = $1; } ;

enum_def:
    "enum" identifier '{' enumerators '}'
      { TRY(idl_create_enum(pstate, LOC(@1.first, @5.last), $2, $4, &$$)); }
  ;

enumerators:
    enumerator
      { $$ = $1; }
  | enumerators ',' enumerator
      { $$ = idl_push_node($1, $3); }
  ;

enumerator:
    annotations identifier
      { TRY(idl_create_enumerator(pstate, &@2, $2, &$$));
        TRY_EXCEPT(idl_annotate(pstate, $$, $1), idl_free($$));
      }
  ;

bitmask_dcl: bitmask_def { $$ = $1; } ;

bitmask_def:
    "bitmask" identifier '{' bit_values '}'
      { TRY(idl_create_bitmask(pstate, LOC(@1.first, @5.last), $2, $4, &$$)); }
  ;

bit_values:
    bit_value
      { $$ = $1; }
  | bit_values ',' bit_value
      { $$ = idl_push_node($1, $3); }
  ;

bit_value:
    annotations identifier
      { TRY(idl_create_bit_value(pstate, &@2, $2, &$$));
        TRY_EXCEPT(idl_annotate(pstate, $$, $1), idl_free($$));
      }
  ;

array_declarator:
    identifier fixed_array_sizes
      { TRY(idl_create_declarator(pstate, LOC(@1.first, @2.last), $1, $2, &$$)); }
  ;

fixed_array_sizes:
    fixed_array_size
      { $$ = $1; }
  | fixed_array_sizes fixed_array_size
      { $$ = idl_push_node($1, $2); }
  ;

fixed_array_size:
    '[' positive_int_const ']'
      { $$ = $2; }
  ;

simple_declarator:
    identifier
      { TRY(idl_create_declarator(pstate, &@1, $1, NULL, &$$)); }
  ;

complex_declarator: array_declarator ;

typedef_dcl:
    "typedef" type_spec declarators
      { TRY(idl_create_typedef(pstate, LOC(@1.first, @3.last), $2, $3, &$$)); }
  | "typedef" constr_type_dcl declarators
      {
        idl_typedef_t *node;
        idl_type_spec_t *type_spec;
        assert($2);
        /* treat forward declaration as no-op if definition is available */
        if ((idl_mask($2) & IDL_FORWARD) && ((idl_forward_t *)$2)->type_spec)
          type_spec = ((idl_forward_t *)$2)->type_spec;
        else
          type_spec = $2;
        TRY(idl_create_typedef(pstate, LOC(@1.first, @3.last), type_spec, $3, &node));
        idl_reference_node(type_spec);
        $$ = idl_push_node($2, node);
      }
  ;

declarators:
    declarator
      { $$ = $1; }
  | declarators ',' declarator
      { $$ = idl_push_node($1, $3); }
  ;

declarator:
    simple_declarator
  | complex_declarator
  ;

identifier:
    IDL_TOKEN_IDENTIFIER
      { $$ = NULL;
        size_t n;
        bool nocase = (pstate->config.flags & IDL_FLAG_CASE_SENSITIVE) == 0;
        if (pstate->parser.state == IDL_PARSE_ANNOTATION_APPL)
          n = 0;
        else if (pstate->parser.state == IDL_PARSE_ANNOTATION)
          n = 0;
        else if (!(n = ($1[0] == '_')) && idl_iskeyword(pstate, $1, nocase))
          SEMANTIC_ERROR(&@1, "Identifier '%s' collides with a keyword", $1);

        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          bool is_annotation = pstate->parser.state == IDL_PARSE_ANNOTATION || pstate->parser.state == IDL_PARSE_ANNOTATION_APPL;
          TRY(idl_create_name(pstate, &@1, idl_strdup($1+n), is_annotation, &$$));
        }
      }
  ;

annotation_dcl:
    annotation_header '{' annotation_body '}'
      { $$ = NULL;
        /* discard annotation in case of redefinition */
        if (pstate->parser.state != IDL_PARSE_EXISTING_ANNOTATION_BODY)
          $$ = $1;
        TRY(idl_finalize_annotation(pstate, LOC(@1.first, @4.last), $1, $3));
      }
  ;

annotation_header:
    "@" "annotation"
      { pstate->annotations = true; /* register annotation occurence */
        pstate->parser.state = IDL_PARSE_ANNOTATION;
      }
    identifier
      { TRY(idl_create_annotation(pstate, LOC(@1.first, @2.last), $4, &$$)); }
  ;

annotation_body:
    %empty
      { $$ = NULL; }
  | annotation_body annotation_member ';'
      { $$ = idl_push_node($1, $2); }
  | annotation_body enum_dcl ';'
      { $$ = idl_push_node($1, $2); }
  | annotation_body bitmask_dcl ';'
      { $$ = idl_push_node($1, $2); }
  | annotation_body const_dcl ';'
      { $$ = idl_push_node($1, $2); }
  | annotation_body typedef_dcl ';'
      { $$ = idl_push_node($1, $2); }
  ;

annotation_member:
    annotation_member_type simple_declarator annotation_member_default
      { TRY(idl_create_annotation_member(pstate, LOC(@1.first, @3.last), $1, $2, $3, &$$)); }
  ;

annotation_member_type:
    const_type
      { $$ = $1; }
  | any_const_type
      { $$ = $1; }
  ;

annotation_member_default:
    %empty
      { $$ = NULL; }
  | IDL_TOKEN_DEFAULT const_expr
      { $$ = $2; }
  ;

any_const_type:
    IDL_TOKEN_ANY
      { TRY(idl_create_base_type(pstate, &@1, IDL_ANY, &$$)); }
  ;

annotations:
    annotation_appls
      { $$ = $1; }
  | %empty
      { $$ = NULL; }
  ;

annotation_appls:
    annotation_appl
      { $$ = $1; }
  | annotation_appls annotation_appl
      { $$ = idl_push_node($1, $2); }
  ;

annotation_appl:
    annotation_appl_header annotation_appl_params
      { if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_finalize_annotation_appl(pstate, LOC(@1.first, @2.last), $1, $2));
        pstate->parser.state = IDL_PARSE;
        pstate->annotation_scope = NULL;
        $$ = $1;
      }
  ;

annotation_appl_header:
    "@"
      { pstate->parser.state = IDL_PARSE_ANNOTATION_APPL; }
    annotation_appl_name
      { const idl_annotation_t *annotation;
        const idl_declaration_t *declaration =
          idl_find_scoped_name(pstate, NULL, $3, IDL_FIND_ANNOTATION);

        pstate->annotations = true; /* register annotation occurence */

        $$ = NULL;
        if (declaration) {
          annotation = idl_reference_node((idl_node_t *)declaration->node);
          TRY(idl_create_annotation_appl(pstate, LOC(@1.first, @3.last), annotation, &$$));
          pstate->parser.state = IDL_PARSE_ANNOTATION_APPL_PARAMS;
          pstate->annotation_scope = declaration->scope;
        } else {
          pstate->parser.state = IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS;
          if (strcmp(@1.first.file->name, "<builtin>") && strcmp(@3.last.file->name, "<builtin>"))
            idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, LOC(@1.first, @3.last), "Unrecognized annotation: @%s", $3->identifier);
        }

        idl_delete_scoped_name($3);
      }
  ;

annotation_appl_name:
    identifier
      { TRY(idl_create_scoped_name(pstate, &@1, $1, false, &$$)); }
  | IDL_TOKEN_SCOPE_NO_SPACE identifier
      { TRY(idl_create_scoped_name(pstate, LOC(@1.first, @2.last), $2, true, &$$)); }
  | annotation_appl_name IDL_TOKEN_SCOPE_NO_SPACE identifier
      { TRY(idl_push_scoped_name(pstate, $1, $3));
        $$ = $1;
      }
  ;

annotation_appl_params:
    %empty
      { $$ = NULL; }
  | '(' const_expr ')'
      { $$ = $2; }
  | '(' annotation_appl_keyword_params ')'
      { $$ = $2; }
  ;

annotation_appl_keyword_params:
    annotation_appl_keyword_param
      { $$ = $1; }
  | annotation_appl_keyword_params ',' annotation_appl_keyword_param
      { $$ = idl_push_node($1, $3); }
  ;

annotation_appl_keyword_param:
    identifier
      { idl_annotation_member_t *node = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          const idl_declaration_t *declaration = NULL;
          static const char fmt[] =
            "Unknown annotation member '%s'";
          declaration = idl_find(pstate, pstate->annotation_scope, $1, 0u);
          if (declaration && (idl_mask(declaration->node) & IDL_DECLARATOR))
            node = (idl_annotation_member_t *)((const idl_node_t *)declaration->node)->parent;
          if (!node || !(idl_mask(node) & IDL_ANNOTATION_MEMBER))
            SEMANTIC_ERROR(&@1, fmt, $1->identifier);
          node = idl_reference_node((idl_node_t *)node);
        }
        $<annotation_member>$ = node;
        idl_delete_name($1);
      }
    '=' const_expr
      { $$ = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          TRY(idl_create_annotation_appl_param(pstate, &@1, $<annotation_member>2, $4, &$$));
        }
      }
  ;

%%

#if defined(__GNUC__)
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
  if (yyps && !yyps->yynew)
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

int idl_iskeyword(idl_pstate_t *pstate, const char *str, int nc)
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
#if YYBISON >= 30800
      // "yytname" is long deprecated and "yytokname" has been removed in bison 3.8.
      // This hack seems to be enough to buy us some time to rewrite the keyword
      // recognition to not rely on anything deprecated
      toknum = (int) (255 + i);
#else
      toknum = yytoknum[i];
#endif
    }
  }

  switch (toknum) {
    case IDL_TOKEN_ANNOTATION:
      return 0;
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
      if (!(pstate->config.flags & IDL_FLAG_EXTENDED_DATA_TYPES))
        return 0;
      break;
    default:
      break;
  };

  return toknum;
}

static void
yyerror(idl_location_t *loc, idl_pstate_t *pstate, idl_retcode_t *result, const char *str)
{
  idl_error(pstate, loc, "%s", str);
  *result = IDL_RETCODE_SYNTAX_ERROR;
}
