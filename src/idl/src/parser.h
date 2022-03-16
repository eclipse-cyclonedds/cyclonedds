/* A Bison parser, made by GNU Bison 3.7.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

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
#line 85 "src/parser.y"

#include "tree.h"

/* make yytoknum available */
#define YYPRINT(A,B,C) (void)0
/* use YYLTYPE definition below */
#define IDL_YYLTYPE_IS_DECLARED
typedef struct idl_location IDL_YYLTYPE;

#define LOC(first, last) \
  &(IDL_YYLTYPE){ first, last }

#line 70 "parser.h"

/* Token kinds.  */
#ifndef IDL_YYTOKENTYPE
# define IDL_YYTOKENTYPE
  enum idl_yytokentype
  {
    IDL_YYEMPTY = -2,
    IDL_YYEOF = 0,                 /* "end of file"  */
    IDL_YYerror = 256,             /* error  */
    IDL_YYUNDEF = 257,             /* "invalid token"  */
    IDL_TOKEN_LINE_COMMENT = 258,  /* IDL_TOKEN_LINE_COMMENT  */
    IDL_TOKEN_COMMENT = 259,       /* IDL_TOKEN_COMMENT  */
    IDL_TOKEN_PP_NUMBER = 260,     /* IDL_TOKEN_PP_NUMBER  */
    IDL_TOKEN_IDENTIFIER = 261,    /* IDL_TOKEN_IDENTIFIER  */
    IDL_TOKEN_CHAR_LITERAL = 262,  /* IDL_TOKEN_CHAR_LITERAL  */
    IDL_TOKEN_STRING_LITERAL = 263, /* IDL_TOKEN_STRING_LITERAL  */
    IDL_TOKEN_INTEGER_LITERAL = 264, /* IDL_TOKEN_INTEGER_LITERAL  */
    IDL_TOKEN_FLOATING_PT_LITERAL = 265, /* IDL_TOKEN_FLOATING_PT_LITERAL  */
    IDL_TOKEN_ANNOTATION_SYMBOL = 266, /* "@"  */
    IDL_TOKEN_ANNOTATION = 267,    /* "annotation"  */
    IDL_TOKEN_SCOPE = 268,         /* IDL_TOKEN_SCOPE  */
    IDL_TOKEN_SCOPE_NO_SPACE = 269, /* IDL_TOKEN_SCOPE_NO_SPACE  */
    IDL_TOKEN_MODULE = 270,        /* "module"  */
    IDL_TOKEN_CONST = 271,         /* "const"  */
    IDL_TOKEN_NATIVE = 272,        /* "native"  */
    IDL_TOKEN_STRUCT = 273,        /* "struct"  */
    IDL_TOKEN_TYPEDEF = 274,       /* "typedef"  */
    IDL_TOKEN_UNION = 275,         /* "union"  */
    IDL_TOKEN_SWITCH = 276,        /* "switch"  */
    IDL_TOKEN_CASE = 277,          /* "case"  */
    IDL_TOKEN_DEFAULT = 278,       /* "default"  */
    IDL_TOKEN_ENUM = 279,          /* "enum"  */
    IDL_TOKEN_UNSIGNED = 280,      /* "unsigned"  */
    IDL_TOKEN_FIXED = 281,         /* "fixed"  */
    IDL_TOKEN_SEQUENCE = 282,      /* "sequence"  */
    IDL_TOKEN_STRING = 283,        /* "string"  */
    IDL_TOKEN_WSTRING = 284,       /* "wstring"  */
    IDL_TOKEN_FLOAT = 285,         /* "float"  */
    IDL_TOKEN_DOUBLE = 286,        /* "double"  */
    IDL_TOKEN_SHORT = 287,         /* "short"  */
    IDL_TOKEN_LONG = 288,          /* "long"  */
    IDL_TOKEN_CHAR = 289,          /* "char"  */
    IDL_TOKEN_WCHAR = 290,         /* "wchar"  */
    IDL_TOKEN_BOOLEAN = 291,       /* "boolean"  */
    IDL_TOKEN_OCTET = 292,         /* "octet"  */
    IDL_TOKEN_ANY = 293,           /* "any"  */
    IDL_TOKEN_MAP = 294,           /* "map"  */
    IDL_TOKEN_BITSET = 295,        /* "bitset"  */
    IDL_TOKEN_BITFIELD = 296,      /* "bitfield"  */
    IDL_TOKEN_BITMASK = 297,       /* "bitmask"  */
    IDL_TOKEN_INT8 = 298,          /* "int8"  */
    IDL_TOKEN_INT16 = 299,         /* "int16"  */
    IDL_TOKEN_INT32 = 300,         /* "int32"  */
    IDL_TOKEN_INT64 = 301,         /* "int64"  */
    IDL_TOKEN_UINT8 = 302,         /* "uint8"  */
    IDL_TOKEN_UINT16 = 303,        /* "uint16"  */
    IDL_TOKEN_UINT32 = 304,        /* "uint32"  */
    IDL_TOKEN_UINT64 = 305,        /* "uint64"  */
    IDL_TOKEN_TRUE = 306,          /* "TRUE"  */
    IDL_TOKEN_FALSE = 307,         /* "FALSE"  */
    IDL_TOKEN_LSHIFT = 308,        /* "<<"  */
    IDL_TOKEN_RSHIFT = 309         /* ">>"  */
  };
  typedef enum idl_yytokentype idl_yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined IDL_YYSTYPE && ! defined IDL_YYSTYPE_IS_DECLARED
union IDL_YYSTYPE
{
#line 103 "src/parser.y"

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

#line 186 "parser.h"

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


int idl_yypush_parse (idl_yypstate *ps,
                  int pushed_char, IDL_YYSTYPE const *pushed_val, IDL_YYLTYPE *pushed_loc, idl_pstate_t *pstate, idl_retcode_t *result);

idl_yypstate *idl_yypstate_new (void);
void idl_yypstate_delete (idl_yypstate *ps);

/* "%code provides" blocks.  */
#line 98 "src/parser.y"

int idl_iskeyword(idl_pstate_t *pstate, const char *str, int nc);
void idl_yypstate_delete_stack(idl_yypstate *yyps);

#line 230 "parser.h"

#endif /* !YY_IDL_YY_PARSER_H_INCLUDED  */
/* generated from parser.y[c5eaedb4a13d082e1b358e1e9b92f9d888389cf7] */
/* generated from parser.y[c5eaedb4a13d082e1b358e1e9b92f9d888389cf7] */
