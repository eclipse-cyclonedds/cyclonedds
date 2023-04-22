// A Bison parser, made by GNU Bison 3.5.1.

// Bison interface for Yacc-like parsers in C
//
// Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
// Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_IDL_YY_PARSER_H_INCLUDED
# define YY_IDL_YY_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef IDL_YYDEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define IDL_YYDEBUG 1
#  else
#   define IDL_YYDEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define IDL_YYDEBUG 1
# endif /* ! defined YYDEBUG */
#endif  /* ! defined IDL_YYDEBUG */
#if IDL_YYDEBUG
extern int idl_yydebug;
#endif
/* "%code requires" blocks.  */
#line 87 "src/parser.y"

#include "tree.h"

/* make yytoknum available */
#define YYPRINT(A,B,C) (void)0
/* use YYLTYPE definition below */
#define IDL_YYLTYPE_IS_DECLARED
typedef struct idl_location IDL_YYLTYPE;

#define LOC(first, last) \
  &(IDL_YYLTYPE){ first, last }

#line 69 "parser.h"

/* Token type.  */
#ifndef IDL_YYTOKENTYPE
# define IDL_YYTOKENTYPE
  enum idl_yytokentype
  {
    IDL_TOKEN_LINE_COMMENT = 258,
    IDL_TOKEN_COMMENT = 259,
    IDL_TOKEN_PP_NUMBER = 260,
    IDL_TOKEN_IDENTIFIER = 261,
    IDL_TOKEN_CHAR_LITERAL = 262,
    IDL_TOKEN_STRING_LITERAL = 263,
    IDL_TOKEN_INTEGER_LITERAL = 264,
    IDL_TOKEN_FLOATING_PT_LITERAL = 265,
    IDL_TOKEN_ANNOTATION_SYMBOL = 266,
    IDL_TOKEN_ANNOTATION = 267,
    IDL_TOKEN_SCOPE = 268,
    IDL_TOKEN_SCOPE_NO_SPACE = 269,
    IDL_TOKEN_MODULE = 270,
    IDL_TOKEN_CONST = 271,
    IDL_TOKEN_NATIVE = 272,
    IDL_TOKEN_STRUCT = 273,
    IDL_TOKEN_TYPEDEF = 274,
    IDL_TOKEN_UNION = 275,
    IDL_TOKEN_SWITCH = 276,
    IDL_TOKEN_CASE = 277,
    IDL_TOKEN_DEFAULT = 278,
    IDL_TOKEN_ENUM = 279,
    IDL_TOKEN_UNSIGNED = 280,
    IDL_TOKEN_FIXED = 281,
    IDL_TOKEN_SEQUENCE = 282,
    IDL_TOKEN_STRING = 283,
    IDL_TOKEN_WSTRING = 284,
    IDL_TOKEN_FLOAT = 285,
    IDL_TOKEN_DOUBLE = 286,
    IDL_TOKEN_SHORT = 287,
    IDL_TOKEN_LONG = 288,
    IDL_TOKEN_CHAR = 289,
    IDL_TOKEN_WCHAR = 290,
    IDL_TOKEN_BOOLEAN = 291,
    IDL_TOKEN_OCTET = 292,
    IDL_TOKEN_ANY = 293,
    IDL_TOKEN_MAP = 294,
    IDL_TOKEN_BITSET = 295,
    IDL_TOKEN_BITFIELD = 296,
    IDL_TOKEN_BITMASK = 297,
    IDL_TOKEN_INT8 = 298,
    IDL_TOKEN_INT16 = 299,
    IDL_TOKEN_INT32 = 300,
    IDL_TOKEN_INT64 = 301,
    IDL_TOKEN_UINT8 = 302,
    IDL_TOKEN_UINT16 = 303,
    IDL_TOKEN_UINT32 = 304,
    IDL_TOKEN_UINT64 = 305,
    IDL_TOKEN_TRUE = 306,
    IDL_TOKEN_FALSE = 307,
    IDL_TOKEN_LSHIFT = 308,
    IDL_TOKEN_RSHIFT = 309
  };
#endif

/* Value type.  */
#if ! defined IDL_YYSTYPE && ! defined IDL_YYSTYPE_IS_DECLARED
union IDL_YYSTYPE
{
#line 105 "src/parser.y"

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

#line 180 "parser.h"

};
typedef union IDL_YYSTYPE IDL_YYSTYPE;
# define IDL_YYSTYPE_IS_TRIVIAL 1
# define IDL_YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined IDL_YYLTYPE && ! defined IDL_YYLTYPE_IS_DECLARED
typedef struct IDL_YYLTYPE IDL_YYLTYPE;
struct IDL_YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define IDL_YYLTYPE_IS_DECLARED 1
# define IDL_YYLTYPE_IS_TRIVIAL 1
#endif



#ifndef YYPUSH_MORE_DEFINED
# define YYPUSH_MORE_DEFINED
enum { YYPUSH_MORE = 4 };
#endif

typedef struct idl_yypstate idl_yypstate;

int idl_yypush_parse (idl_yypstate *ps, int pushed_char, IDL_YYSTYPE const *pushed_val, IDL_YYLTYPE *pushed_loc, idl_pstate_t *pstate, idl_retcode_t *result);

idl_yypstate * idl_yypstate_new (void);
void idl_yypstate_delete (idl_yypstate *ps);
/* "%code provides" blocks.  */
#line 100 "src/parser.y"

int idl_iskeyword(idl_pstate_t *pstate, const char *str, int nc);
void idl_yypstate_delete_stack(idl_yypstate *yyps);

#line 221 "parser.h"

#endif /* !YY_IDL_YY_PARSER_H_INCLUDED  */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
