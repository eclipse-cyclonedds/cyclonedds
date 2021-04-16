/* A Bison parser, made by GNU Bison 3.7.2.  */

/* Bison implementation for Yacc-like parsers in C

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.7.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 1

/* Pull parsers.  */
#define YYPULL 0

/* Substitute the type names.  */
#define YYSTYPE         IDL_YYSTYPE
#define YYLTYPE         IDL_YYLTYPE
/* Substitute the variable and function names.  */
#define yypush_parse    idl_yypush_parse
#define yypstate_new    idl_yypstate_new
#define yypstate_clear  idl_yypstate_clear
#define yypstate_delete idl_yypstate_delete
#define yypstate        idl_yypstate
#define yylex           idl_yylex
#define yyerror         idl_yyerror
#define yydebug         idl_yydebug
#define yynerrs         idl_yynerrs

/* First part of user prologue.  */
#line 12 "src/parser.y"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "idl/string.h"
#include "idl/processor.h"
#include "annotation.h"
#include "expression.h"
#include "scope.h"
#include "symbol.h"
#include "tree.h"

#if defined(__GNUC__)
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wconversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")
#endif

static void yyerror(idl_location_t *, idl_pstate_t *, const char *);

/* convenience macros to complement YYABORT */
#define NO_MEMORY() \
  do { \
    yylen = 0; \
    goto yyexhaustedlab; \
  } while(0)

#define SEMANTIC_ERROR(state, loc, ...) \
  do { \
    idl_error(state, loc, __VA_ARGS__); \
    yylen = 0; /* pop right-hand side tokens */ \
    yyresult = IDL_RETCODE_SEMANTIC_ERROR; \
    goto yyreturn; \
  } while(0)

#define YYLLOC_DEFAULT(Cur, Rhs, N) \
  do { \
    if (N) { \
      (Cur).first.source = YYRHSLOC(Rhs, 1).first.source; \
      (Cur).first.file = YYRHSLOC(Rhs, 1).first.file; \
      (Cur).first.line = YYRHSLOC(Rhs, 1).first.line; \
      (Cur).first.column = YYRHSLOC(Rhs, 1).first.column; \
    } else { \
      (Cur).first.source = YYRHSLOC(Rhs, 0).last.source; \
      (Cur).first.file = YYRHSLOC(Rhs, 0).last.file; \
      (Cur).first.line = YYRHSLOC(Rhs, 0).last.line; \
      (Cur).first.column = YYRHSLOC(Rhs, 0).last.column; \
    } \
    (Cur).last.line = YYRHSLOC(Rhs, N).last.line; \
    (Cur).last.column = YYRHSLOC(Rhs, N).last.column; \
  } while(0)

#define TRY_EXCEPT(action, except) \
  do { \
    int _ret_; \
    switch ((_ret_ = (action))) { \
      case IDL_RETCODE_OK: \
        break; \
      case IDL_RETCODE_NO_MEMORY: \
        yylen = 0; /* pop right-hand side tokens */ \
        (void)(except);\
        goto yyexhaustedlab; \
      case IDL_RETCODE_SYNTAX_ERROR: \
        yylen = 0; /* pop right-hand side tokens */ \
        (void)(except); \
        goto yyabortlab; \
      default: \
        yylen = 0; \
        yyresult = _ret_; \
        (void)(except); \
        goto yyreturn; \
    } \
  } while(0)

#define TRY(action) \
  TRY_EXCEPT((action), 0)

#line 167 "src/parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_IDL_TOKEN_LINE_COMMENT = 3,     /* IDL_TOKEN_LINE_COMMENT  */
  YYSYMBOL_IDL_TOKEN_COMMENT = 4,          /* IDL_TOKEN_COMMENT  */
  YYSYMBOL_IDL_TOKEN_PP_NUMBER = 5,        /* IDL_TOKEN_PP_NUMBER  */
  YYSYMBOL_IDL_TOKEN_IDENTIFIER = 6,       /* IDL_TOKEN_IDENTIFIER  */
  YYSYMBOL_IDL_TOKEN_CHAR_LITERAL = 7,     /* IDL_TOKEN_CHAR_LITERAL  */
  YYSYMBOL_IDL_TOKEN_STRING_LITERAL = 8,   /* IDL_TOKEN_STRING_LITERAL  */
  YYSYMBOL_IDL_TOKEN_INTEGER_LITERAL = 9,  /* IDL_TOKEN_INTEGER_LITERAL  */
  YYSYMBOL_IDL_TOKEN_FLOATING_PT_LITERAL = 10, /* IDL_TOKEN_FLOATING_PT_LITERAL  */
  YYSYMBOL_IDL_TOKEN_ANNOTATION_SYMBOL = 11, /* "@"  */
  YYSYMBOL_IDL_TOKEN_ANNOTATION = 12,      /* "annotation"  */
  YYSYMBOL_IDL_TOKEN_SCOPE = 13,           /* IDL_TOKEN_SCOPE  */
  YYSYMBOL_IDL_TOKEN_SCOPE_NO_SPACE = 14,  /* IDL_TOKEN_SCOPE_NO_SPACE  */
  YYSYMBOL_IDL_TOKEN_MODULE = 15,          /* "module"  */
  YYSYMBOL_IDL_TOKEN_CONST = 16,           /* "const"  */
  YYSYMBOL_IDL_TOKEN_NATIVE = 17,          /* "native"  */
  YYSYMBOL_IDL_TOKEN_STRUCT = 18,          /* "struct"  */
  YYSYMBOL_IDL_TOKEN_TYPEDEF = 19,         /* "typedef"  */
  YYSYMBOL_IDL_TOKEN_UNION = 20,           /* "union"  */
  YYSYMBOL_IDL_TOKEN_SWITCH = 21,          /* "switch"  */
  YYSYMBOL_IDL_TOKEN_CASE = 22,            /* "case"  */
  YYSYMBOL_IDL_TOKEN_DEFAULT = 23,         /* "default"  */
  YYSYMBOL_IDL_TOKEN_ENUM = 24,            /* "enum"  */
  YYSYMBOL_IDL_TOKEN_UNSIGNED = 25,        /* "unsigned"  */
  YYSYMBOL_IDL_TOKEN_FIXED = 26,           /* "fixed"  */
  YYSYMBOL_IDL_TOKEN_SEQUENCE = 27,        /* "sequence"  */
  YYSYMBOL_IDL_TOKEN_STRING = 28,          /* "string"  */
  YYSYMBOL_IDL_TOKEN_WSTRING = 29,         /* "wstring"  */
  YYSYMBOL_IDL_TOKEN_FLOAT = 30,           /* "float"  */
  YYSYMBOL_IDL_TOKEN_DOUBLE = 31,          /* "double"  */
  YYSYMBOL_IDL_TOKEN_SHORT = 32,           /* "short"  */
  YYSYMBOL_IDL_TOKEN_LONG = 33,            /* "long"  */
  YYSYMBOL_IDL_TOKEN_CHAR = 34,            /* "char"  */
  YYSYMBOL_IDL_TOKEN_WCHAR = 35,           /* "wchar"  */
  YYSYMBOL_IDL_TOKEN_BOOLEAN = 36,         /* "boolean"  */
  YYSYMBOL_IDL_TOKEN_OCTET = 37,           /* "octet"  */
  YYSYMBOL_IDL_TOKEN_ANY = 38,             /* "any"  */
  YYSYMBOL_IDL_TOKEN_MAP = 39,             /* "map"  */
  YYSYMBOL_IDL_TOKEN_BITSET = 40,          /* "bitset"  */
  YYSYMBOL_IDL_TOKEN_BITFIELD = 41,        /* "bitfield"  */
  YYSYMBOL_IDL_TOKEN_BITMASK = 42,         /* "bitmask"  */
  YYSYMBOL_IDL_TOKEN_INT8 = 43,            /* "int8"  */
  YYSYMBOL_IDL_TOKEN_INT16 = 44,           /* "int16"  */
  YYSYMBOL_IDL_TOKEN_INT32 = 45,           /* "int32"  */
  YYSYMBOL_IDL_TOKEN_INT64 = 46,           /* "int64"  */
  YYSYMBOL_IDL_TOKEN_UINT8 = 47,           /* "uint8"  */
  YYSYMBOL_IDL_TOKEN_UINT16 = 48,          /* "uint16"  */
  YYSYMBOL_IDL_TOKEN_UINT32 = 49,          /* "uint32"  */
  YYSYMBOL_IDL_TOKEN_UINT64 = 50,          /* "uint64"  */
  YYSYMBOL_IDL_TOKEN_TRUE = 51,            /* "TRUE"  */
  YYSYMBOL_IDL_TOKEN_FALSE = 52,           /* "FALSE"  */
  YYSYMBOL_IDL_TOKEN_LSHIFT = 53,          /* "<<"  */
  YYSYMBOL_IDL_TOKEN_RSHIFT = 54,          /* ">>"  */
  YYSYMBOL_55_ = 55,                       /* ';'  */
  YYSYMBOL_56_ = 56,                       /* '{'  */
  YYSYMBOL_57_ = 57,                       /* '}'  */
  YYSYMBOL_58_ = 58,                       /* '='  */
  YYSYMBOL_59_ = 59,                       /* '|'  */
  YYSYMBOL_60_ = 60,                       /* '^'  */
  YYSYMBOL_61_ = 61,                       /* '&'  */
  YYSYMBOL_62_ = 62,                       /* '+'  */
  YYSYMBOL_63_ = 63,                       /* '-'  */
  YYSYMBOL_64_ = 64,                       /* '*'  */
  YYSYMBOL_65_ = 65,                       /* '/'  */
  YYSYMBOL_66_ = 66,                       /* '%'  */
  YYSYMBOL_67_ = 67,                       /* '~'  */
  YYSYMBOL_68_ = 68,                       /* '('  */
  YYSYMBOL_69_ = 69,                       /* ')'  */
  YYSYMBOL_70_ = 70,                       /* '<'  */
  YYSYMBOL_71_ = 71,                       /* ','  */
  YYSYMBOL_72_ = 72,                       /* '>'  */
  YYSYMBOL_73_ = 73,                       /* ':'  */
  YYSYMBOL_74_ = 74,                       /* '['  */
  YYSYMBOL_75_ = 75,                       /* ']'  */
  YYSYMBOL_YYACCEPT = 76,                  /* $accept  */
  YYSYMBOL_specification = 77,             /* specification  */
  YYSYMBOL_definitions = 78,               /* definitions  */
  YYSYMBOL_definition = 79,                /* definition  */
  YYSYMBOL_module_dcl = 80,                /* module_dcl  */
  YYSYMBOL_module_header = 81,             /* module_header  */
  YYSYMBOL_scoped_name = 82,               /* scoped_name  */
  YYSYMBOL_const_dcl = 83,                 /* const_dcl  */
  YYSYMBOL_const_type = 84,                /* const_type  */
  YYSYMBOL_const_expr = 85,                /* const_expr  */
  YYSYMBOL_or_expr = 86,                   /* or_expr  */
  YYSYMBOL_xor_expr = 87,                  /* xor_expr  */
  YYSYMBOL_and_expr = 88,                  /* and_expr  */
  YYSYMBOL_shift_expr = 89,                /* shift_expr  */
  YYSYMBOL_shift_operator = 90,            /* shift_operator  */
  YYSYMBOL_add_expr = 91,                  /* add_expr  */
  YYSYMBOL_add_operator = 92,              /* add_operator  */
  YYSYMBOL_mult_expr = 93,                 /* mult_expr  */
  YYSYMBOL_mult_operator = 94,             /* mult_operator  */
  YYSYMBOL_unary_expr = 95,                /* unary_expr  */
  YYSYMBOL_unary_operator = 96,            /* unary_operator  */
  YYSYMBOL_primary_expr = 97,              /* primary_expr  */
  YYSYMBOL_literal = 98,                   /* literal  */
  YYSYMBOL_boolean_literal = 99,           /* boolean_literal  */
  YYSYMBOL_string_literal = 100,           /* string_literal  */
  YYSYMBOL_positive_int_const = 101,       /* positive_int_const  */
  YYSYMBOL_type_dcl = 102,                 /* type_dcl  */
  YYSYMBOL_type_spec = 103,                /* type_spec  */
  YYSYMBOL_simple_type_spec = 104,         /* simple_type_spec  */
  YYSYMBOL_base_type_spec = 105,           /* base_type_spec  */
  YYSYMBOL_floating_pt_type = 106,         /* floating_pt_type  */
  YYSYMBOL_integer_type = 107,             /* integer_type  */
  YYSYMBOL_signed_int = 108,               /* signed_int  */
  YYSYMBOL_unsigned_int = 109,             /* unsigned_int  */
  YYSYMBOL_char_type = 110,                /* char_type  */
  YYSYMBOL_wide_char_type = 111,           /* wide_char_type  */
  YYSYMBOL_boolean_type = 112,             /* boolean_type  */
  YYSYMBOL_octet_type = 113,               /* octet_type  */
  YYSYMBOL_template_type_spec = 114,       /* template_type_spec  */
  YYSYMBOL_sequence_type = 115,            /* sequence_type  */
  YYSYMBOL_string_type = 116,              /* string_type  */
  YYSYMBOL_constr_type_dcl = 117,          /* constr_type_dcl  */
  YYSYMBOL_struct_dcl = 118,               /* struct_dcl  */
  YYSYMBOL_struct_def = 119,               /* struct_def  */
  YYSYMBOL_struct_header = 120,            /* struct_header  */
  YYSYMBOL_struct_inherit_spec = 121,      /* struct_inherit_spec  */
  YYSYMBOL_struct_body = 122,              /* struct_body  */
  YYSYMBOL_members = 123,                  /* members  */
  YYSYMBOL_member = 124,                   /* member  */
  YYSYMBOL_union_dcl = 125,                /* union_dcl  */
  YYSYMBOL_union_def = 126,                /* union_def  */
  YYSYMBOL_union_header = 127,             /* union_header  */
  YYSYMBOL_128_1 = 128,                    /* @1  */
  YYSYMBOL_switch_type_spec = 129,         /* switch_type_spec  */
  YYSYMBOL_switch_body = 130,              /* switch_body  */
  YYSYMBOL_case = 131,                     /* case  */
  YYSYMBOL_case_labels = 132,              /* case_labels  */
  YYSYMBOL_case_label = 133,               /* case_label  */
  YYSYMBOL_element_spec = 134,             /* element_spec  */
  YYSYMBOL_enum_dcl = 135,                 /* enum_dcl  */
  YYSYMBOL_enum_def = 136,                 /* enum_def  */
  YYSYMBOL_enumerators = 137,              /* enumerators  */
  YYSYMBOL_enumerator = 138,               /* enumerator  */
  YYSYMBOL_array_declarator = 139,         /* array_declarator  */
  YYSYMBOL_fixed_array_sizes = 140,        /* fixed_array_sizes  */
  YYSYMBOL_fixed_array_size = 141,         /* fixed_array_size  */
  YYSYMBOL_simple_declarator = 142,        /* simple_declarator  */
  YYSYMBOL_complex_declarator = 143,       /* complex_declarator  */
  YYSYMBOL_typedef_dcl = 144,              /* typedef_dcl  */
  YYSYMBOL_declarators = 145,              /* declarators  */
  YYSYMBOL_declarator = 146,               /* declarator  */
  YYSYMBOL_identifier = 147,               /* identifier  */
  YYSYMBOL_annotation_dcl = 148,           /* annotation_dcl  */
  YYSYMBOL_annotation_header = 149,        /* annotation_header  */
  YYSYMBOL_150_2 = 150,                    /* $@2  */
  YYSYMBOL_annotation_body = 151,          /* annotation_body  */
  YYSYMBOL_annotation_member = 152,        /* annotation_member  */
  YYSYMBOL_annotation_member_type = 153,   /* annotation_member_type  */
  YYSYMBOL_annotation_member_default = 154, /* annotation_member_default  */
  YYSYMBOL_any_const_type = 155,           /* any_const_type  */
  YYSYMBOL_annotations = 156,              /* annotations  */
  YYSYMBOL_annotation_appls = 157,         /* annotation_appls  */
  YYSYMBOL_annotation_appl = 158,          /* annotation_appl  */
  YYSYMBOL_159_3 = 159,                    /* $@3  */
  YYSYMBOL_160_4 = 160,                    /* @4  */
  YYSYMBOL_annotation_appl_name = 161,     /* annotation_appl_name  */
  YYSYMBOL_annotation_appl_params = 162,   /* annotation_appl_params  */
  YYSYMBOL_annotation_appl_keyword_params = 163, /* annotation_appl_keyword_params  */
  YYSYMBOL_annotation_appl_keyword_param = 164, /* annotation_appl_keyword_param  */
  YYSYMBOL_165_5 = 165                     /* @5  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_int16 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined IDL_YYLTYPE_IS_TRIVIAL && IDL_YYLTYPE_IS_TRIVIAL \
             && defined IDL_YYSTYPE_IS_TRIVIAL && IDL_YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
  YYLTYPE yyls_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE) \
             + YYSIZEOF (YYLTYPE)) \
      + 2 * YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  12
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   344

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  76
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  90
/* YYNRULES -- Number of rules.  */
#define YYNRULES  180
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  267

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   309


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,    66,    61,     2,
      68,    69,    64,    62,    71,    63,     2,    65,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    73,    55,
      70,    58,    72,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    74,     2,    75,    60,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    56,    59,    57,    67,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54
};

#if IDL_YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,   289,   289,   291,   296,   298,   303,   305,   309,   313,
     320,   327,   332,   334,   336,   343,   348,   350,   352,   354,
     356,   358,   360,   374,   377,   378,   386,   387,   395,   396,
     404,   405,   413,   414,   417,   418,   426,   427,   430,   432,
     440,   441,   442,   445,   450,   455,   456,   457,   461,   477,
     479,   484,   506,   522,   529,   536,   546,   548,   553,   560,
     576,   581,   582,   586,   588,   592,   594,   607,   608,   609,
     610,   611,   612,   616,   617,   618,   622,   623,   627,   628,
     629,   631,   632,   633,   634,   638,   639,   640,   642,   643,
     644,   645,   649,   652,   655,   658,   661,   662,   666,   668,
     673,   675,   680,   681,   682,   686,   690,   697,   702,   705,
     720,   724,   729,   731,   736,   743,   747,   755,   754,   767,
     769,   771,   773,   779,   781,   786,   788,   793,   800,   802,
     807,   809,   814,   818,   821,   826,   828,   833,   840,   845,
     847,   852,   857,   861,   864,   869,   871,   876,   877,   881,
     898,   909,   908,   917,   919,   921,   923,   925,   930,   935,
     937,   942,   944,   949,   954,   956,   961,   963,   969,   971,
     968,   998,  1000,  1002,  1009,  1011,  1013,  1018,  1020,  1026,
    1025
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if IDL_YYDEBUG || 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"",
  "IDL_TOKEN_LINE_COMMENT", "IDL_TOKEN_COMMENT", "IDL_TOKEN_PP_NUMBER",
  "IDL_TOKEN_IDENTIFIER", "IDL_TOKEN_CHAR_LITERAL",
  "IDL_TOKEN_STRING_LITERAL", "IDL_TOKEN_INTEGER_LITERAL",
  "IDL_TOKEN_FLOATING_PT_LITERAL", "\"@\"", "\"annotation\"",
  "IDL_TOKEN_SCOPE", "IDL_TOKEN_SCOPE_NO_SPACE", "\"module\"", "\"const\"",
  "\"native\"", "\"struct\"", "\"typedef\"", "\"union\"", "\"switch\"",
  "\"case\"", "\"default\"", "\"enum\"", "\"unsigned\"", "\"fixed\"",
  "\"sequence\"", "\"string\"", "\"wstring\"", "\"float\"", "\"double\"",
  "\"short\"", "\"long\"", "\"char\"", "\"wchar\"", "\"boolean\"",
  "\"octet\"", "\"any\"", "\"map\"", "\"bitset\"", "\"bitfield\"",
  "\"bitmask\"", "\"int8\"", "\"int16\"", "\"int32\"", "\"int64\"",
  "\"uint8\"", "\"uint16\"", "\"uint32\"", "\"uint64\"", "\"TRUE\"",
  "\"FALSE\"", "\"<<\"", "\">>\"", "';'", "'{'", "'}'", "'='", "'|'",
  "'^'", "'&'", "'+'", "'-'", "'*'", "'/'", "'%'", "'~'", "'('", "')'",
  "'<'", "','", "'>'", "':'", "'['", "']'", "$accept", "specification",
  "definitions", "definition", "module_dcl", "module_header",
  "scoped_name", "const_dcl", "const_type", "const_expr", "or_expr",
  "xor_expr", "and_expr", "shift_expr", "shift_operator", "add_expr",
  "add_operator", "mult_expr", "mult_operator", "unary_expr",
  "unary_operator", "primary_expr", "literal", "boolean_literal",
  "string_literal", "positive_int_const", "type_dcl", "type_spec",
  "simple_type_spec", "base_type_spec", "floating_pt_type", "integer_type",
  "signed_int", "unsigned_int", "char_type", "wide_char_type",
  "boolean_type", "octet_type", "template_type_spec", "sequence_type",
  "string_type", "constr_type_dcl", "struct_dcl", "struct_def",
  "struct_header", "struct_inherit_spec", "struct_body", "members",
  "member", "union_dcl", "union_def", "union_header", "@1",
  "switch_type_spec", "switch_body", "case", "case_labels", "case_label",
  "element_spec", "enum_dcl", "enum_def", "enumerators", "enumerator",
  "array_declarator", "fixed_array_sizes", "fixed_array_size",
  "simple_declarator", "complex_declarator", "typedef_dcl", "declarators",
  "declarator", "identifier", "annotation_dcl", "annotation_header", "$@2",
  "annotation_body", "annotation_member", "annotation_member_type",
  "annotation_member_default", "any_const_type", "annotations",
  "annotation_appls", "annotation_appl", "$@3", "@4",
  "annotation_appl_name", "annotation_appl_params",
  "annotation_appl_keyword_params", "annotation_appl_keyword_param", "@5", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304,
     305,   306,   307,   308,   309,    59,   123,   125,    61,   124,
      94,    38,    43,    45,    42,    47,    37,   126,    40,    41,
      60,    44,    62,    58,    91,    93
};
#endif

#define YYPACT_NINF (-176)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-180)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      81,    10,    26,    82,  -176,   -20,    24,    92,    74,  -176,
    -176,    51,  -176,  -176,  -176,  -176,    54,   114,    54,   249,
      54,    54,    59,    53,    64,    68,  -176,  -176,  -176,    69,
    -176,  -176,    70,  -176,  -176,  -176,  -176,  -176,    54,  -176,
      54,  -176,    91,   159,  -176,    54,    23,    60,  -176,  -176,
    -176,    44,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,
    -176,  -176,  -176,   111,    54,  -176,  -176,  -176,  -176,  -176,
    -176,  -176,  -176,  -176,    56,    62,  -176,   111,    54,  -176,
    -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,
     113,    80,  -176,   127,  -176,  -176,   -10,    78,  -176,  -176,
      54,    72,  -176,  -176,    88,  -176,    94,    97,    98,    54,
    -176,  -176,  -176,   121,    11,  -176,  -176,    54,   108,    77,
    -176,   249,  -176,  -176,  -176,    84,  -176,    93,   100,    74,
       4,    99,    14,  -176,   249,    11,   101,     9,  -176,   204,
    -176,  -176,    11,  -176,  -176,  -176,  -176,  -176,   147,  -176,
    -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,
      11,   111,  -176,   112,   117,   118,    49,    55,    30,  -176,
      36,  -176,  -176,  -176,   165,   109,  -176,    11,   111,    50,
      54,    11,    93,  -176,    74,    15,  -176,    54,  -176,  -176,
    -176,    54,   115,  -176,  -176,  -176,    54,  -176,   131,   125,
     124,    28,  -176,    11,  -176,   129,    11,    11,    11,  -176,
    -176,    11,  -176,  -176,    11,  -176,  -176,  -176,    11,  -176,
    -176,  -176,  -176,    11,  -176,  -176,   126,  -176,   294,  -176,
      74,  -176,    -1,  -176,  -176,  -176,  -176,   141,  -176,    54,
    -176,  -176,   117,   118,    49,    55,    30,  -176,   128,  -176,
     178,   111,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,
      11,  -176,  -176,  -176,   143,  -176,  -176
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
     165,   168,     0,   165,     4,     0,     0,     0,   164,   166,
     151,     0,     1,     5,     6,   153,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,    61,   102,   105,     0,
     103,   115,     0,   104,   133,    62,   168,   167,     0,   149,
       0,   171,   169,     0,    11,     0,     0,   101,    73,    74,
      78,    79,    92,    94,    95,    81,    82,    83,    84,    88,
      89,    90,    91,    22,     0,    17,    16,    76,    77,    18,
      19,    20,    21,    12,   108,     0,    93,    66,     0,    63,
      65,    68,    67,    69,    70,    71,    72,    64,    96,    97,
       0,     0,     7,   165,     8,     9,   165,     0,   152,   172,
       0,   174,   163,   150,     0,   159,     0,     0,     0,     0,
     160,    13,    85,    86,     0,    75,    80,     0,     0,     0,
     107,     0,   143,   147,   148,   144,   145,   142,     0,   165,
     165,     0,   165,   112,     0,     0,     0,     0,   125,     0,
     128,   173,     0,   170,   156,   155,   157,   154,   161,   142,
      87,    53,    58,    51,    52,    56,    57,    46,    45,    47,
       0,    48,    60,    23,    24,    26,    28,    30,    34,    38,
       0,    44,    49,    54,    55,     0,    14,     0,   109,     0,
       0,     0,   138,   139,   165,     0,   135,     0,    10,   106,
     113,     0,     0,   131,   116,   126,     0,   129,     0,     0,
      12,     0,   177,     0,   158,     0,     0,     0,     0,    33,
      32,     0,    36,    37,     0,    40,    41,    42,     0,    43,
      59,   100,    15,     0,    99,   146,     0,   140,     0,   134,
     165,   137,     0,   130,   132,   127,   175,     0,   176,     0,
     162,    50,    25,    27,    29,    31,    35,    39,     0,   141,
      79,   122,   119,   120,   123,   121,   124,   117,   136,   114,
       0,   179,   178,    98,     0,   180,   118
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -176,  -176,   137,     5,  -176,  -176,    -6,   171,   175,  -119,
    -176,    13,    17,    12,  -176,    22,  -176,     7,  -176,    25,
    -176,    75,  -176,  -176,  -176,  -175,  -176,   -70,  -176,  -176,
      16,   -15,  -176,  -176,   -13,    18,    -5,    -3,  -176,  -176,
      33,  -176,  -176,  -176,  -176,  -176,  -176,  -176,   110,  -176,
    -176,  -176,  -176,  -176,  -176,   107,  -176,   119,  -176,   213,
    -176,  -176,    27,  -176,  -176,    79,   150,  -176,   217,    73,
    -128,   -11,  -176,  -176,  -176,  -176,  -176,  -176,  -176,  -176,
     -93,  -176,   255,  -176,  -176,  -176,  -176,  -176,    29,  -176
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,    22,    23,   161,    24,    64,   162,
     163,   164,   165,   166,   211,   167,   214,   168,   218,   169,
     170,   171,   172,   173,   174,   175,    25,    78,    79,    80,
      81,    82,    67,    68,    83,    84,    85,    86,    87,    88,
      89,    26,    27,    28,    29,   120,   131,   132,   133,    30,
      31,    32,   264,   257,   137,   138,   139,   140,   198,    33,
      34,   185,   186,   122,   182,   183,   123,   124,    35,   125,
     126,    73,     5,     6,    38,    43,   108,   109,   204,   110,
       7,     8,     9,    11,   101,    42,   143,   201,   202,   237
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      41,    36,    66,   134,    69,    44,   226,    74,    13,    90,
      91,    63,    70,    77,    71,     1,   192,    39,   151,   152,
     153,   154,    10,   199,    45,    36,    12,    98,    66,    99,
      69,   135,   136,    65,   111,    14,   187,    63,    70,   134,
      71,   205,    39,   151,   152,   153,   154,  -111,   248,    45,
      72,   179,   225,   118,   259,   112,   113,    39,   222,    65,
      39,   188,   155,   156,   191,    40,   194,   127,   234,   196,
     180,  -110,   229,   157,   158,   115,    72,   116,   159,   160,
      15,    -2,    -3,    39,   240,    36,   230,   155,   156,   141,
      45,   228,     1,     1,   215,   216,   217,   238,   149,   239,
     135,   136,   209,   210,   160,   100,   176,    16,    17,    93,
      18,    19,    20,   178,    92,    77,    21,   212,   213,    94,
      39,   223,   224,    95,   117,    96,    97,    45,    77,   119,
     114,   200,   121,    77,   128,    13,   129,   187,     1,    46,
     142,   265,    47,   144,    48,    49,    50,    51,    52,   145,
      53,    54,   146,   147,   150,   180,   189,    55,    56,    57,
      58,    59,    60,    61,    62,    39,   177,   181,   184,   127,
     203,   206,    45,   220,   193,    17,   231,   207,    19,   208,
     127,   221,  -179,    21,    46,   127,   235,    47,   233,    48,
      49,    50,    51,    52,   236,    53,    54,   102,   241,   260,
     263,   249,    55,    56,    57,    58,    59,    60,    61,    62,
      39,   116,   266,   252,   104,   253,   103,    45,   105,   242,
     244,   246,   251,   255,   243,   256,   135,   136,   261,    46,
     130,    75,    47,   245,    48,    49,    50,    51,    52,    76,
      53,    54,   190,   247,   195,   219,   254,    55,    56,    57,
      58,    59,    60,    61,    62,    39,   106,   258,   197,   148,
     107,   227,    45,    37,   232,     0,     0,     0,   262,     0,
       0,     0,     0,     0,    46,     0,    75,    47,     0,    48,
      49,    50,    51,    52,    76,    53,    54,     0,     0,     0,
       0,     0,    55,    56,    57,    58,    59,    60,    61,    62,
      39,     0,     0,     0,     0,     0,     0,    45,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,    46,
       0,     0,     0,     0,     0,     0,    50,   250,    52,    76,
      53,    54,     0,     0,     0,     0,     0,    55,    56,    57,
      58,    59,    60,    61,    62
};

static const yytype_int16 yycheck[] =
{
      11,    11,    17,    96,    17,    16,   181,    18,     3,    20,
      21,    17,    17,    19,    17,    11,   135,     6,     7,     8,
       9,    10,    12,   142,    13,    11,     0,    38,    43,    40,
      43,    22,    23,    17,    45,    55,   129,    43,    43,   132,
      43,   160,     6,     7,     8,     9,    10,    57,   223,    13,
      17,   121,   180,    64,    55,    32,    33,     6,   177,    43,
       6,    57,    51,    52,   134,    14,    57,    78,   196,   139,
      71,    57,    57,    62,    63,    31,    43,    33,    67,    68,
      56,     0,     0,     6,   203,    11,    71,    51,    52,   100,
      13,   184,    11,    11,    64,    65,    66,    69,   109,    71,
      22,    23,    53,    54,    68,    14,   117,    15,    16,    56,
      18,    19,    20,   119,    55,   121,    24,    62,    63,    55,
       6,    71,    72,    55,    13,    56,    56,    13,   134,    73,
      70,   142,    70,   139,    21,   130,    56,   230,    11,    25,
      68,   260,    28,    55,    30,    31,    32,    33,    34,    55,
      36,    37,    55,    55,    33,    71,    57,    43,    44,    45,
      46,    47,    48,    49,    50,     6,    58,    74,    68,   180,
      23,    59,    13,     8,    73,    16,   187,    60,    19,    61,
     191,    72,    58,    24,    25,   196,    55,    28,    73,    30,
      31,    32,    33,    34,    69,    36,    37,    38,    69,    58,
      72,    75,    43,    44,    45,    46,    47,    48,    49,    50,
       6,    33,    69,   228,    43,   228,    57,    13,    43,   206,
     208,   214,   228,   228,   207,   228,    22,    23,   239,    25,
      93,    27,    28,   211,    30,    31,    32,    33,    34,    35,
      36,    37,   132,   218,   137,   170,   228,    43,    44,    45,
      46,    47,    48,    49,    50,     6,    43,   230,   139,   109,
      43,   182,    13,     8,   191,    -1,    -1,    -1,   239,    -1,
      -1,    -1,    -1,    -1,    25,    -1,    27,    28,    -1,    30,
      31,    32,    33,    34,    35,    36,    37,    -1,    -1,    -1,
      -1,    -1,    43,    44,    45,    46,    47,    48,    49,    50,
       6,    -1,    -1,    -1,    -1,    -1,    -1,    13,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    25,
      -1,    -1,    -1,    -1,    -1,    -1,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    43,    44,    45,
      46,    47,    48,    49,    50
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    11,    77,    78,    79,   148,   149,   156,   157,   158,
      12,   159,     0,    79,    55,    56,    15,    16,    18,    19,
      20,    24,    80,    81,    83,   102,   117,   118,   119,   120,
     125,   126,   127,   135,   136,   144,    11,   158,   150,     6,
      14,   147,   161,   151,   147,    13,    25,    28,    30,    31,
      32,    33,    34,    36,    37,    43,    44,    45,    46,    47,
      48,    49,    50,    82,    84,   106,   107,   108,   109,   110,
     112,   113,   116,   147,   147,    27,    35,    82,   103,   104,
     105,   106,   107,   110,   111,   112,   113,   114,   115,   116,
     147,   147,    55,    56,    55,    55,    56,    56,   147,   147,
      14,   160,    38,    57,    83,    84,   135,   144,   152,   153,
     155,   147,    32,    33,    70,    31,    33,    13,   147,    73,
     121,    70,   139,   142,   143,   145,   146,   147,    21,    56,
      78,   122,   123,   124,   156,    22,    23,   130,   131,   132,
     133,   147,    68,   162,    55,    55,    55,    55,   142,   147,
      33,     7,     8,     9,    10,    51,    52,    62,    63,    67,
      68,    82,    85,    86,    87,    88,    89,    91,    93,    95,
      96,    97,    98,    99,   100,   101,   147,    58,    82,   103,
      71,    74,   140,   141,    68,   137,   138,   156,    57,    57,
     124,   103,    85,    73,    57,   131,   103,   133,   134,    85,
     147,   163,   164,    23,   154,    85,    59,    60,    61,    53,
      54,    90,    62,    63,    92,    64,    65,    66,    94,    97,
       8,    72,    85,    71,    72,   146,   101,   141,   156,    57,
      71,   147,   145,    73,   146,    55,    69,   165,    69,    71,
      85,    69,    87,    88,    89,    91,    93,    95,   101,    75,
      33,    82,   107,   110,   111,   112,   113,   129,   138,    55,
      58,   147,   164,    72,   128,    85,    69
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    76,    77,    77,    78,    78,    79,    79,    79,    79,
      80,    81,    82,    82,    82,    83,    84,    84,    84,    84,
      84,    84,    84,    85,    86,    86,    87,    87,    88,    88,
      89,    89,    90,    90,    91,    91,    92,    92,    93,    93,
      94,    94,    94,    95,    95,    96,    96,    96,    97,    97,
      97,    98,    98,    98,    98,    98,    99,    99,   100,   100,
     101,   102,   102,   103,   103,   104,   104,   105,   105,   105,
     105,   105,   105,   106,   106,   106,   107,   107,   108,   108,
     108,   108,   108,   108,   108,   109,   109,   109,   109,   109,
     109,   109,   110,   111,   112,   113,   114,   114,   115,   115,
     116,   116,   117,   117,   117,   118,   119,   120,   121,   121,
     122,   122,   123,   123,   124,   125,   126,   128,   127,   129,
     129,   129,   129,   129,   129,   130,   130,   131,   132,   132,
     133,   133,   134,   135,   136,   137,   137,   138,   139,   140,
     140,   141,   142,   143,   144,   145,   145,   146,   146,   147,
     148,   150,   149,   151,   151,   151,   151,   151,   152,   153,
     153,   154,   154,   155,   156,   156,   157,   157,   159,   160,
     158,   161,   161,   161,   162,   162,   162,   163,   163,   165,
     164
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     1,     1,     2,     2,     3,     3,     3,
       4,     2,     1,     2,     3,     5,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     3,     1,     3,     1,     3,
       1,     3,     1,     1,     1,     3,     1,     1,     1,     3,
       1,     1,     1,     2,     1,     1,     1,     1,     1,     1,
       3,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     2,     1,     1,     1,     1,
       2,     1,     1,     1,     1,     2,     2,     3,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     6,     4,
       4,     1,     1,     1,     1,     1,     4,     3,     0,     2,
       1,     0,     1,     2,     4,     1,     4,     0,     8,     1,
       1,     1,     1,     1,     1,     1,     2,     3,     1,     2,
       3,     2,     2,     1,     5,     1,     3,     2,     2,     1,
       2,     3,     1,     1,     3,     1,     3,     1,     1,     1,
       4,     0,     4,     0,     3,     3,     3,     3,     3,     1,
       1,     0,     2,     1,     1,     0,     1,     2,     0,     0,
       5,     1,     2,     3,     0,     3,     3,     1,     3,     0,
       4
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = IDL_YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == IDL_YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (&yylloc, pstate, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use IDL_YYerror or IDL_YYUNDEF. */
#define YYERRCODE IDL_YYUNDEF

/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)                                \
    do                                                                  \
      if (N)                                                            \
        {                                                               \
          (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;        \
          (Current).first_column = YYRHSLOC (Rhs, 1).first_column;      \
          (Current).last_line    = YYRHSLOC (Rhs, N).last_line;         \
          (Current).last_column  = YYRHSLOC (Rhs, N).last_column;       \
        }                                                               \
      else                                                              \
        {                                                               \
          (Current).first_line   = (Current).last_line   =              \
            YYRHSLOC (Rhs, 0).last_line;                                \
          (Current).first_column = (Current).last_column =              \
            YYRHSLOC (Rhs, 0).last_column;                              \
        }                                                               \
    while (0)
#endif

#define YYRHSLOC(Rhs, K) ((Rhs)[K])


/* Enable debugging if requested.  */
#if IDL_YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YY_LOCATION_PRINT
#  if defined IDL_YYLTYPE_IS_TRIVIAL && IDL_YYLTYPE_IS_TRIVIAL

/* Print *YYLOCP on YYO.  Private, do not rely on its existence. */

YY_ATTRIBUTE_UNUSED
static int
yy_location_print_ (FILE *yyo, YYLTYPE const * const yylocp)
{
  int res = 0;
  int end_col = 0 != yylocp->last_column ? yylocp->last_column - 1 : 0;
  if (0 <= yylocp->first_line)
    {
      res += YYFPRINTF (yyo, "%d", yylocp->first_line);
      if (0 <= yylocp->first_column)
        res += YYFPRINTF (yyo, ".%d", yylocp->first_column);
    }
  if (0 <= yylocp->last_line)
    {
      if (yylocp->first_line < yylocp->last_line)
        {
          res += YYFPRINTF (yyo, "-%d", yylocp->last_line);
          if (0 <= end_col)
            res += YYFPRINTF (yyo, ".%d", end_col);
        }
      else if (0 <= end_col && yylocp->first_column < end_col)
        res += YYFPRINTF (yyo, "-%d", end_col);
    }
  return res;
 }

#   define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

#  else
#   define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#  endif
# endif /* !defined YY_LOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, pstate); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  YYUSE (yylocationp);
  YYUSE (pstate);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yykind < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yykind], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YY_LOCATION_PRINT (yyo, *yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, pstate);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp,
                 int yyrule, idl_pstate_t *pstate)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)],
                       &(yylsp[(yyi + 1) - (yynrhs)]), pstate);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, pstate); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !IDL_YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !IDL_YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif
/* Parser data structure.  */
struct yypstate
  {
    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack: array, bottom, top.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;
    /* Whether this instance has not started parsing yet.
     * If 2, it corresponds to a finished parsing.  */
    int yynew;
  };






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, idl_pstate_t *pstate)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (pstate);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_definitions: /* definitions  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1391 "src/parser.c"
        break;

    case YYSYMBOL_definition: /* definition  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1397 "src/parser.c"
        break;

    case YYSYMBOL_module_dcl: /* module_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1403 "src/parser.c"
        break;

    case YYSYMBOL_module_header: /* module_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1409 "src/parser.c"
        break;

    case YYSYMBOL_scoped_name: /* scoped_name  */
#line 212 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1415 "src/parser.c"
        break;

    case YYSYMBOL_const_dcl: /* const_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).const_dcl)); }
#line 1421 "src/parser.c"
        break;

    case YYSYMBOL_const_type: /* const_type  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1427 "src/parser.c"
        break;

    case YYSYMBOL_const_expr: /* const_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1433 "src/parser.c"
        break;

    case YYSYMBOL_or_expr: /* or_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1439 "src/parser.c"
        break;

    case YYSYMBOL_xor_expr: /* xor_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1445 "src/parser.c"
        break;

    case YYSYMBOL_and_expr: /* and_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1451 "src/parser.c"
        break;

    case YYSYMBOL_shift_expr: /* shift_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1457 "src/parser.c"
        break;

    case YYSYMBOL_add_expr: /* add_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1463 "src/parser.c"
        break;

    case YYSYMBOL_mult_expr: /* mult_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1469 "src/parser.c"
        break;

    case YYSYMBOL_unary_expr: /* unary_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1475 "src/parser.c"
        break;

    case YYSYMBOL_primary_expr: /* primary_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1481 "src/parser.c"
        break;

    case YYSYMBOL_literal: /* literal  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1487 "src/parser.c"
        break;

    case YYSYMBOL_string_literal: /* string_literal  */
#line 207 "src/parser.y"
            { free(((*yyvaluep).string_literal)); }
#line 1493 "src/parser.c"
        break;

    case YYSYMBOL_positive_int_const: /* positive_int_const  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1499 "src/parser.c"
        break;

    case YYSYMBOL_type_dcl: /* type_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1505 "src/parser.c"
        break;

    case YYSYMBOL_type_spec: /* type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1511 "src/parser.c"
        break;

    case YYSYMBOL_simple_type_spec: /* simple_type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1517 "src/parser.c"
        break;

    case YYSYMBOL_template_type_spec: /* template_type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1523 "src/parser.c"
        break;

    case YYSYMBOL_sequence_type: /* sequence_type  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).sequence)); }
#line 1529 "src/parser.c"
        break;

    case YYSYMBOL_string_type: /* string_type  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).string)); }
#line 1535 "src/parser.c"
        break;

    case YYSYMBOL_constr_type_dcl: /* constr_type_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1541 "src/parser.c"
        break;

    case YYSYMBOL_struct_dcl: /* struct_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1547 "src/parser.c"
        break;

    case YYSYMBOL_struct_def: /* struct_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1553 "src/parser.c"
        break;

    case YYSYMBOL_struct_header: /* struct_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1559 "src/parser.c"
        break;

    case YYSYMBOL_struct_inherit_spec: /* struct_inherit_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1565 "src/parser.c"
        break;

    case YYSYMBOL_struct_body: /* struct_body  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1571 "src/parser.c"
        break;

    case YYSYMBOL_members: /* members  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1577 "src/parser.c"
        break;

    case YYSYMBOL_member: /* member  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1583 "src/parser.c"
        break;

    case YYSYMBOL_union_dcl: /* union_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1589 "src/parser.c"
        break;

    case YYSYMBOL_union_def: /* union_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1595 "src/parser.c"
        break;

    case YYSYMBOL_union_header: /* union_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1601 "src/parser.c"
        break;

    case YYSYMBOL_switch_type_spec: /* switch_type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1607 "src/parser.c"
        break;

    case YYSYMBOL_switch_body: /* switch_body  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1613 "src/parser.c"
        break;

    case YYSYMBOL_case: /* case  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1619 "src/parser.c"
        break;

    case YYSYMBOL_case_labels: /* case_labels  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1625 "src/parser.c"
        break;

    case YYSYMBOL_case_label: /* case_label  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1631 "src/parser.c"
        break;

    case YYSYMBOL_element_spec: /* element_spec  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1637 "src/parser.c"
        break;

    case YYSYMBOL_enum_dcl: /* enum_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1643 "src/parser.c"
        break;

    case YYSYMBOL_enum_def: /* enum_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enum_dcl)); }
#line 1649 "src/parser.c"
        break;

    case YYSYMBOL_enumerators: /* enumerators  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1655 "src/parser.c"
        break;

    case YYSYMBOL_enumerator: /* enumerator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1661 "src/parser.c"
        break;

    case YYSYMBOL_array_declarator: /* array_declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1667 "src/parser.c"
        break;

    case YYSYMBOL_fixed_array_sizes: /* fixed_array_sizes  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1673 "src/parser.c"
        break;

    case YYSYMBOL_fixed_array_size: /* fixed_array_size  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1679 "src/parser.c"
        break;

    case YYSYMBOL_simple_declarator: /* simple_declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1685 "src/parser.c"
        break;

    case YYSYMBOL_complex_declarator: /* complex_declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1691 "src/parser.c"
        break;

    case YYSYMBOL_typedef_dcl: /* typedef_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).typedef_dcl)); }
#line 1697 "src/parser.c"
        break;

    case YYSYMBOL_declarators: /* declarators  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1703 "src/parser.c"
        break;

    case YYSYMBOL_declarator: /* declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1709 "src/parser.c"
        break;

    case YYSYMBOL_identifier: /* identifier  */
#line 209 "src/parser.y"
            { idl_delete_name(((*yyvaluep).name)); }
#line 1715 "src/parser.c"
        break;

    case YYSYMBOL_annotation_dcl: /* annotation_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1721 "src/parser.c"
        break;

    case YYSYMBOL_annotation_header: /* annotation_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1727 "src/parser.c"
        break;

    case YYSYMBOL_annotation_body: /* annotation_body  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1733 "src/parser.c"
        break;

    case YYSYMBOL_annotation_member: /* annotation_member  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1739 "src/parser.c"
        break;

    case YYSYMBOL_annotation_member_type: /* annotation_member_type  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1745 "src/parser.c"
        break;

    case YYSYMBOL_annotation_member_default: /* annotation_member_default  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1751 "src/parser.c"
        break;

    case YYSYMBOL_any_const_type: /* any_const_type  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1757 "src/parser.c"
        break;

    case YYSYMBOL_annotations: /* annotations  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1763 "src/parser.c"
        break;

    case YYSYMBOL_annotation_appls: /* annotation_appls  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1769 "src/parser.c"
        break;

    case YYSYMBOL_annotation_appl: /* annotation_appl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1775 "src/parser.c"
        break;

    case YYSYMBOL_annotation_appl_name: /* annotation_appl_name  */
#line 212 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1781 "src/parser.c"
        break;

    case YYSYMBOL_annotation_appl_params: /* annotation_appl_params  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1787 "src/parser.c"
        break;

    case YYSYMBOL_annotation_appl_keyword_params: /* annotation_appl_keyword_params  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1793 "src/parser.c"
        break;

    case YYSYMBOL_annotation_appl_keyword_param: /* annotation_appl_keyword_param  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1799 "src/parser.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}





#define idl_yynerrs yyps->idl_yynerrs
#define yystate yyps->yystate
#define yyerrstatus yyps->yyerrstatus
#define yyssa yyps->yyssa
#define yyss yyps->yyss
#define yyssp yyps->yyssp
#define yyvsa yyps->yyvsa
#define yyvs yyps->yyvs
#define yyvsp yyps->yyvsp
#define yylsa yyps->yylsa
#define yyls yyps->yyls
#define yylsp yyps->yylsp
#define yystacksize yyps->yystacksize

/* Initialize the parser data structure.  */
static void
yypstate_clear (yypstate *yyps)
{
  yynerrs = 0;
  yystate = 0;
  yyerrstatus = 0;

  yyssp = yyss;
  yyvsp = yyvs;
  yylsp = yyls;

  /* Initialize the state stack, in case yypcontext_expected_tokens is
     called before the first call to yyparse. */
  *yyssp = 0;
  yyps->yynew = 1;
}

/* Initialize the parser data structure.  */
yypstate *
yypstate_new (void)
{
  yypstate *yyps;
  yyps = YY_CAST (yypstate *, YYMALLOC (sizeof *yyps));
  if (!yyps)
    return YY_NULLPTR;
  yystacksize = YYINITDEPTH;
  yyss = yyssa;
  yyvs = yyvsa;
  yyls = yylsa;
  yypstate_clear (yyps);
  return yyps;
}

void
yypstate_delete (yypstate *yyps)
{
  if (yyps)
    {
#ifndef yyoverflow
      /* If the stack was reallocated but the parse did not complete, then the
         stack still needs to be freed.  */
      if (yyss != yyssa)
        YYSTACK_FREE (yyss);
#endif
      YYFREE (yyps);
    }
}



/*---------------.
| yypush_parse.  |
`---------------*/

int
yypush_parse (yypstate *yyps,
              int yypushed_char, YYSTYPE const *yypushed_val, YYLTYPE *yypushed_loc, idl_pstate_t *pstate)
{
/* Lookahead token kind.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

/* Location data for the lookahead symbol.  */
static YYLTYPE yyloc_default
# if defined IDL_YYLTYPE_IS_TRIVIAL && IDL_YYLTYPE_IS_TRIVIAL
  = { 1, 1, 1, 1 }
# endif
;
YYLTYPE yylloc = yyloc_default;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

  /* The locations where the error started and ended.  */
  YYLTYPE yyerror_range[3];



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  switch (yyps->yynew)
    {
    case 2:
      yypstate_clear (yyps);
      goto case_0;

    case_0:
    case 0:
      yyn = yypact[yystate];
      goto yyread_pushed_token;
    }

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = IDL_YYEMPTY; /* Cause a token to be read.  */
  yylsp[0] = *yypushed_loc;
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;
        YYLTYPE *yyls1 = yyls;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yyls1, yysize * YYSIZEOF (*yylsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
        yyls = yyls1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
        YYSTACK_RELOCATE (yyls_alloc, yyls);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;
      yylsp = yyls + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == IDL_YYEMPTY)
    {
      if (!yyps->yynew)
        {
          YYDPRINTF ((stderr, "Return for a new token:\n"));
          yyresult = YYPUSH_MORE;
          goto yypushreturn;
        }
      yyps->yynew = 0;
yyread_pushed_token:
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yypushed_char;
      if (yypushed_val)
        yylval = *yypushed_val;
      if (yypushed_loc)
        yylloc = *yypushed_loc;
    }

  if (yychar <= IDL_YYEOF)
    {
      yychar = IDL_YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == IDL_YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = IDL_YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      yyerror_range[1] = yylloc;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END
  *++yylsp = yylloc;

  /* Discard the shifted token.  */
  yychar = IDL_YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];

  /* Default location. */
  YYLLOC_DEFAULT (yyloc, (yylsp - yylen), yylen);
  yyerror_range[1] = yyloc;
  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2: /* specification: %empty  */
#line 290 "src/parser.y"
      { pstate->root = NULL; }
#line 2161 "src/parser.c"
    break;

  case 3: /* specification: definitions  */
#line 292 "src/parser.y"
      { pstate->root = (yyvsp[0].node); }
#line 2167 "src/parser.c"
    break;

  case 4: /* definitions: definition  */
#line 297 "src/parser.y"
      { (yyval.node) = (yyvsp[0].node); }
#line 2173 "src/parser.c"
    break;

  case 5: /* definitions: definitions definition  */
#line 299 "src/parser.y"
      { (yyval.node) = idl_push_node((yyvsp[-1].node), (yyvsp[0].node)); }
#line 2179 "src/parser.c"
    break;

  case 6: /* definition: annotation_dcl ';'  */
#line 304 "src/parser.y"
      { (yyval.node) = (yyvsp[-1].annotation); }
#line 2185 "src/parser.c"
    break;

  case 7: /* definition: annotations module_dcl ';'  */
#line 306 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].module_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].module_dcl);
      }
#line 2193 "src/parser.c"
    break;

  case 8: /* definition: annotations const_dcl ';'  */
#line 310 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].const_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].const_dcl);
      }
#line 2201 "src/parser.c"
    break;

  case 9: /* definition: annotations type_dcl ';'  */
#line 314 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].node), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].node);
      }
#line 2209 "src/parser.c"
    break;

  case 10: /* module_dcl: module_header '{' definitions '}'  */
#line 321 "src/parser.y"
      { TRY(idl_finalize_module(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].module_dcl), (yyvsp[-1].node)));
        (yyval.module_dcl) = (yyvsp[-3].module_dcl);
      }
#line 2217 "src/parser.c"
    break;

  case 11: /* module_header: "module" identifier  */
#line 328 "src/parser.y"
      { TRY(idl_create_module(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), &(yyval.module_dcl))); }
#line 2223 "src/parser.c"
    break;

  case 12: /* scoped_name: identifier  */
#line 333 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 2229 "src/parser.c"
    break;

  case 13: /* scoped_name: IDL_TOKEN_SCOPE identifier  */
#line 335 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 2235 "src/parser.c"
    break;

  case 14: /* scoped_name: scoped_name IDL_TOKEN_SCOPE identifier  */
#line 337 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 2243 "src/parser.c"
    break;

  case 15: /* const_dcl: "const" const_type identifier '=' const_expr  */
#line 344 "src/parser.y"
      { TRY(idl_create_const(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-2].name), (yyvsp[0].const_expr), &(yyval.const_dcl))); }
#line 2249 "src/parser.c"
    break;

  case 16: /* const_type: integer_type  */
#line 349 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2255 "src/parser.c"
    break;

  case 17: /* const_type: floating_pt_type  */
#line 351 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2261 "src/parser.c"
    break;

  case 18: /* const_type: char_type  */
#line 353 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2267 "src/parser.c"
    break;

  case 19: /* const_type: boolean_type  */
#line 355 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2273 "src/parser.c"
    break;

  case 20: /* const_type: octet_type  */
#line 357 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2279 "src/parser.c"
    break;

  case 21: /* const_type: string_type  */
#line 359 "src/parser.y"
      { (yyval.type_spec) = (idl_type_spec_t *)(yyvsp[0].string); }
#line 2285 "src/parser.c"
    break;

  case 22: /* const_type: scoped_name  */
#line 361 "src/parser.y"
      { idl_node_t *node;
        const idl_declaration_t *declaration;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a valid constant type";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        node = idl_unalias(declaration->node, 0u);
        if (!(idl_mask(node) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM)))
          SEMANTIC_ERROR(pstate, &(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2301 "src/parser.c"
    break;

  case 23: /* const_expr: or_expr  */
#line 374 "src/parser.y"
                    { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2307 "src/parser.c"
    break;

  case 25: /* or_expr: or_expr '|' xor_expr  */
#line 379 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_OR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2316 "src/parser.c"
    break;

  case 27: /* xor_expr: xor_expr '^' and_expr  */
#line 388 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_XOR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2325 "src/parser.c"
    break;

  case 29: /* and_expr: and_expr '&' shift_expr  */
#line 397 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_AND, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2334 "src/parser.c"
    break;

  case 31: /* shift_expr: shift_expr shift_operator add_expr  */
#line 406 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2343 "src/parser.c"
    break;

  case 32: /* shift_operator: ">>"  */
#line 413 "src/parser.y"
         { (yyval.kind) = IDL_RSHIFT; }
#line 2349 "src/parser.c"
    break;

  case 33: /* shift_operator: "<<"  */
#line 414 "src/parser.y"
         { (yyval.kind) = IDL_LSHIFT; }
#line 2355 "src/parser.c"
    break;

  case 35: /* add_expr: add_expr add_operator mult_expr  */
#line 419 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2364 "src/parser.c"
    break;

  case 36: /* add_operator: '+'  */
#line 426 "src/parser.y"
        { (yyval.kind) = IDL_ADD; }
#line 2370 "src/parser.c"
    break;

  case 37: /* add_operator: '-'  */
#line 427 "src/parser.y"
        { (yyval.kind) = IDL_SUBTRACT; }
#line 2376 "src/parser.c"
    break;

  case 38: /* mult_expr: unary_expr  */
#line 431 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2382 "src/parser.c"
    break;

  case 39: /* mult_expr: mult_expr mult_operator unary_expr  */
#line 433 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2391 "src/parser.c"
    break;

  case 40: /* mult_operator: '*'  */
#line 440 "src/parser.y"
        { (yyval.kind) = IDL_MULTIPLY; }
#line 2397 "src/parser.c"
    break;

  case 41: /* mult_operator: '/'  */
#line 441 "src/parser.y"
        { (yyval.kind) = IDL_DIVIDE; }
#line 2403 "src/parser.c"
    break;

  case 42: /* mult_operator: '%'  */
#line 442 "src/parser.y"
        { (yyval.kind) = IDL_MODULO; }
#line 2409 "src/parser.c"
    break;

  case 43: /* unary_expr: unary_operator primary_expr  */
#line 446 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_unary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2418 "src/parser.c"
    break;

  case 44: /* unary_expr: primary_expr  */
#line 451 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2424 "src/parser.c"
    break;

  case 45: /* unary_operator: '-'  */
#line 455 "src/parser.y"
        { (yyval.kind) = IDL_MINUS; }
#line 2430 "src/parser.c"
    break;

  case 46: /* unary_operator: '+'  */
#line 456 "src/parser.y"
        { (yyval.kind) = IDL_PLUS; }
#line 2436 "src/parser.c"
    break;

  case 47: /* unary_operator: '~'  */
#line 457 "src/parser.y"
        { (yyval.kind) = IDL_NOT; }
#line 2442 "src/parser.c"
    break;

  case 48: /* primary_expr: scoped_name  */
#line 462 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          /* disregard scoped names in application of unknown annotations.
             names may or may not have significance in the scope of the
             (builtin) annotation, stick to syntax checks */
          const idl_declaration_t *declaration = NULL;
          static const char fmt[] =
            "Scoped name '%s' does not resolve to an enumerator or a contant";
          TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
          if (!(idl_mask(declaration->node) & (IDL_CONST|IDL_ENUMERATOR)))
            SEMANTIC_ERROR(pstate, &(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
          (yyval.const_expr) = idl_reference_node((idl_node_t *)declaration->node);
        }
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2462 "src/parser.c"
    break;

  case 49: /* primary_expr: literal  */
#line 478 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 2468 "src/parser.c"
    break;

  case 50: /* primary_expr: '(' const_expr ')'  */
#line 480 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[-1].const_expr); }
#line 2474 "src/parser.c"
    break;

  case 51: /* literal: IDL_TOKEN_INTEGER_LITERAL  */
#line 485 "src/parser.y"
      { idl_type_t type;
        idl_literal_t literal;
        (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if ((yyvsp[0].ullng) <= INT32_MAX) {
          type = IDL_LONG;
          literal.value.int32 = (int32_t)(yyvsp[0].ullng);
        } else if ((yyvsp[0].ullng) <= UINT32_MAX) {
          type = IDL_ULONG;
          literal.value.uint32 = (uint32_t)(yyvsp[0].ullng);
        } else if ((yyvsp[0].ullng) <= INT64_MAX) {
          type = IDL_LLONG;
          literal.value.int64 = (int64_t)(yyvsp[0].ullng);
        } else {
          type = IDL_ULLONG;
          literal.value.uint64 = (uint64_t)(yyvsp[0].ullng);
        }
        TRY(idl_create_literal(pstate, &(yylsp[0]), type, &(yyval.literal)));
        (yyval.literal)->value = literal.value;
      }
#line 2500 "src/parser.c"
    break;

  case 52: /* literal: IDL_TOKEN_FLOATING_PT_LITERAL  */
#line 507 "src/parser.y"
      { idl_type_t type;
        idl_literal_t literal;
        (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if (isnan((double)(yyvsp[0].ldbl)) || isinf((double)(yyvsp[0].ldbl))) {
          type = IDL_LDOUBLE;
          literal.value.ldbl = (yyvsp[0].ldbl);
        } else {
          type = IDL_DOUBLE;
          literal.value.dbl = (double)(yyvsp[0].ldbl);
        }
        TRY(idl_create_literal(pstate, &(yylsp[0]), type, &(yyval.literal)));
        (yyval.literal)->value = literal.value;
      }
#line 2520 "src/parser.c"
    break;

  case 53: /* literal: IDL_TOKEN_CHAR_LITERAL  */
#line 523 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_CHAR, &(yyval.literal)));
        (yyval.literal)->value.chr = (yyvsp[0].chr);
      }
#line 2531 "src/parser.c"
    break;

  case 54: /* literal: boolean_literal  */
#line 530 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_BOOL, &(yyval.literal)));
        (yyval.literal)->value.bln = (yyvsp[0].bln);
      }
#line 2542 "src/parser.c"
    break;

  case 55: /* literal: string_literal  */
#line 537 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_STRING, &(yyval.literal)));
        (yyval.literal)->value.str = (yyvsp[0].string_literal);
      }
#line 2553 "src/parser.c"
    break;

  case 56: /* boolean_literal: "TRUE"  */
#line 547 "src/parser.y"
      { (yyval.bln) = true; }
#line 2559 "src/parser.c"
    break;

  case 57: /* boolean_literal: "FALSE"  */
#line 549 "src/parser.y"
      { (yyval.bln) = false; }
#line 2565 "src/parser.c"
    break;

  case 58: /* string_literal: IDL_TOKEN_STRING_LITERAL  */
#line 554 "src/parser.y"
      { (yyval.string_literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if (!((yyval.string_literal) = idl_strdup((yyvsp[0].str))))
          NO_MEMORY();
      }
#line 2576 "src/parser.c"
    break;

  case 59: /* string_literal: string_literal IDL_TOKEN_STRING_LITERAL  */
#line 561 "src/parser.y"
      { size_t n1, n2;
        (yyval.string_literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        /* adjacent string literals are concatenated */
        n1 = strlen((yyvsp[-1].string_literal));
        n2 = strlen((yyvsp[0].str));
        if (!((yyval.string_literal) = realloc((yyvsp[-1].string_literal), n1+n2+1)))
          NO_MEMORY();
        memmove((yyval.string_literal)+n1, (yyvsp[0].str), n2);
        (yyval.string_literal)[n1+n2] = '\0';
      }
#line 2593 "src/parser.c"
    break;

  case 60: /* positive_int_const: const_expr  */
#line 577 "src/parser.y"
      { TRY(idl_evaluate(pstate, (yyvsp[0].const_expr), IDL_ULONG, &(yyval.literal))); }
#line 2599 "src/parser.c"
    break;

  case 61: /* type_dcl: constr_type_dcl  */
#line 581 "src/parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2605 "src/parser.c"
    break;

  case 62: /* type_dcl: typedef_dcl  */
#line 582 "src/parser.y"
                { (yyval.node) = (yyvsp[0].typedef_dcl); }
#line 2611 "src/parser.c"
    break;

  case 65: /* simple_type_spec: base_type_spec  */
#line 593 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2617 "src/parser.c"
    break;

  case 66: /* simple_type_spec: scoped_name  */
#line 595 "src/parser.y"
      { const idl_declaration_t *declaration = NULL;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a type";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        if (!declaration || !idl_is_type_spec(declaration->node))
          SEMANTIC_ERROR(pstate, &(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2631 "src/parser.c"
    break;

  case 73: /* floating_pt_type: "float"  */
#line 616 "src/parser.y"
            { (yyval.kind) = IDL_FLOAT; }
#line 2637 "src/parser.c"
    break;

  case 74: /* floating_pt_type: "double"  */
#line 617 "src/parser.y"
             { (yyval.kind) = IDL_DOUBLE; }
#line 2643 "src/parser.c"
    break;

  case 75: /* floating_pt_type: "long" "double"  */
#line 618 "src/parser.y"
                    { (yyval.kind) = IDL_LDOUBLE; }
#line 2649 "src/parser.c"
    break;

  case 78: /* signed_int: "short"  */
#line 627 "src/parser.y"
            { (yyval.kind) = IDL_SHORT; }
#line 2655 "src/parser.c"
    break;

  case 79: /* signed_int: "long"  */
#line 628 "src/parser.y"
           { (yyval.kind) = IDL_LONG; }
#line 2661 "src/parser.c"
    break;

  case 80: /* signed_int: "long" "long"  */
#line 629 "src/parser.y"
                  { (yyval.kind) = IDL_LLONG; }
#line 2667 "src/parser.c"
    break;

  case 81: /* signed_int: "int8"  */
#line 631 "src/parser.y"
           { (yyval.kind) = IDL_INT8; }
#line 2673 "src/parser.c"
    break;

  case 82: /* signed_int: "int16"  */
#line 632 "src/parser.y"
            { (yyval.kind) = IDL_INT16; }
#line 2679 "src/parser.c"
    break;

  case 83: /* signed_int: "int32"  */
#line 633 "src/parser.y"
            { (yyval.kind) = IDL_INT32; }
#line 2685 "src/parser.c"
    break;

  case 84: /* signed_int: "int64"  */
#line 634 "src/parser.y"
            { (yyval.kind) = IDL_INT64; }
#line 2691 "src/parser.c"
    break;

  case 85: /* unsigned_int: "unsigned" "short"  */
#line 638 "src/parser.y"
                       { (yyval.kind) = IDL_USHORT; }
#line 2697 "src/parser.c"
    break;

  case 86: /* unsigned_int: "unsigned" "long"  */
#line 639 "src/parser.y"
                      { (yyval.kind) = IDL_ULONG; }
#line 2703 "src/parser.c"
    break;

  case 87: /* unsigned_int: "unsigned" "long" "long"  */
#line 640 "src/parser.y"
                             { (yyval.kind) = IDL_ULLONG; }
#line 2709 "src/parser.c"
    break;

  case 88: /* unsigned_int: "uint8"  */
#line 642 "src/parser.y"
            { (yyval.kind) = IDL_UINT8; }
#line 2715 "src/parser.c"
    break;

  case 89: /* unsigned_int: "uint16"  */
#line 643 "src/parser.y"
             { (yyval.kind) = IDL_UINT16; }
#line 2721 "src/parser.c"
    break;

  case 90: /* unsigned_int: "uint32"  */
#line 644 "src/parser.y"
             { (yyval.kind) = IDL_UINT32; }
#line 2727 "src/parser.c"
    break;

  case 91: /* unsigned_int: "uint64"  */
#line 645 "src/parser.y"
             { (yyval.kind) = IDL_UINT64; }
#line 2733 "src/parser.c"
    break;

  case 92: /* char_type: "char"  */
#line 649 "src/parser.y"
           { (yyval.kind) = IDL_CHAR; }
#line 2739 "src/parser.c"
    break;

  case 93: /* wide_char_type: "wchar"  */
#line 652 "src/parser.y"
            { (yyval.kind) = IDL_WCHAR; }
#line 2745 "src/parser.c"
    break;

  case 94: /* boolean_type: "boolean"  */
#line 655 "src/parser.y"
              { (yyval.kind) = IDL_BOOL; }
#line 2751 "src/parser.c"
    break;

  case 95: /* octet_type: "octet"  */
#line 658 "src/parser.y"
            { (yyval.kind) = IDL_OCTET; }
#line 2757 "src/parser.c"
    break;

  case 96: /* template_type_spec: sequence_type  */
#line 661 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].sequence); }
#line 2763 "src/parser.c"
    break;

  case 97: /* template_type_spec: string_type  */
#line 662 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].string); }
#line 2769 "src/parser.c"
    break;

  case 98: /* sequence_type: "sequence" '<' type_spec ',' positive_int_const '>'  */
#line 667 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-5]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-1].literal), &(yyval.sequence))); }
#line 2775 "src/parser.c"
    break;

  case 99: /* sequence_type: "sequence" '<' type_spec '>'  */
#line 669 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), NULL, &(yyval.sequence))); }
#line 2781 "src/parser.c"
    break;

  case 100: /* string_type: "string" '<' positive_int_const '>'  */
#line 674 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].literal), &(yyval.string))); }
#line 2787 "src/parser.c"
    break;

  case 101: /* string_type: "string"  */
#line 676 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[0]).first, (yylsp[0]).last), NULL, &(yyval.string))); }
#line 2793 "src/parser.c"
    break;

  case 105: /* struct_dcl: struct_def  */
#line 686 "src/parser.y"
               { (yyval.node) = (yyvsp[0].struct_dcl); }
#line 2799 "src/parser.c"
    break;

  case 106: /* struct_def: struct_header '{' struct_body '}'  */
#line 691 "src/parser.y"
      { TRY(idl_finalize_struct(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].struct_dcl), (yyvsp[-1].member)));
        (yyval.struct_dcl) = (yyvsp[-3].struct_dcl);
      }
#line 2807 "src/parser.c"
    break;

  case 107: /* struct_header: "struct" identifier struct_inherit_spec  */
#line 698 "src/parser.y"
      { TRY(idl_create_struct(pstate, LOC((yylsp[-2]).first, (yyvsp[0].type_spec) ? (yylsp[0]).last : (yylsp[-1]).last), (yyvsp[-1].name), (yyvsp[0].type_spec), &(yyval.struct_dcl))); }
#line 2813 "src/parser.c"
    break;

  case 108: /* struct_inherit_spec: %empty  */
#line 702 "src/parser.y"
            { (yyval.type_spec) = NULL; }
#line 2819 "src/parser.c"
    break;

  case 109: /* struct_inherit_spec: ':' scoped_name  */
#line 706 "src/parser.y"
      { idl_node_t *node;
        const idl_declaration_t *declaration;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a struct";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        node = idl_unalias(declaration->node, 0u);
        if (!idl_is_struct(node))
          SEMANTIC_ERROR(pstate, &(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        TRY(idl_create_inherit_spec(pstate, &(yylsp[0]), idl_reference_node(node), &(yyval.type_spec)));
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2835 "src/parser.c"
    break;

  case 110: /* struct_body: members  */
#line 721 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 2841 "src/parser.c"
    break;

  case 111: /* struct_body: %empty  */
#line 725 "src/parser.y"
      { (yyval.member) = NULL; }
#line 2847 "src/parser.c"
    break;

  case 112: /* members: member  */
#line 730 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 2853 "src/parser.c"
    break;

  case 113: /* members: members member  */
#line 732 "src/parser.y"
      { (yyval.member) = idl_push_node((yyvsp[-1].member), (yyvsp[0].member)); }
#line 2859 "src/parser.c"
    break;

  case 114: /* member: annotations type_spec declarators ';'  */
#line 737 "src/parser.y"
      { TRY(idl_create_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), &(yyval.member)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.member), (yyvsp[-3].annotation_appl)), free((yyval.member)));
      }
#line 2867 "src/parser.c"
    break;

  case 115: /* union_dcl: union_def  */
#line 743 "src/parser.y"
              { (yyval.node) = (yyvsp[0].union_dcl); }
#line 2873 "src/parser.c"
    break;

  case 116: /* union_def: union_header '{' switch_body '}'  */
#line 748 "src/parser.y"
      { TRY(idl_finalize_union(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].union_dcl), (yyvsp[-1]._case)));
        (yyval.union_dcl) = (yyvsp[-3].union_dcl);
      }
#line 2881 "src/parser.c"
    break;

  case 117: /* @1: %empty  */
#line 755 "src/parser.y"
      { idl_switch_type_spec_t *node = NULL;
        TRY(idl_create_switch_type_spec(pstate, &(yylsp[0]), (yyvsp[0].type_spec), &node));
        TRY_EXCEPT(idl_annotate(pstate, node, (yyvsp[-1].annotation_appl)), idl_delete_node(node));
        (yyval.node) = node;
      }
#line 2891 "src/parser.c"
    break;

  case 118: /* union_header: "union" identifier "switch" '(' annotations switch_type_spec @1 ')'  */
#line 761 "src/parser.y"
      { idl_switch_type_spec_t *node = (yyvsp[-1].node);
        TRY(idl_create_union(pstate, LOC((yylsp[-7]).first, (yylsp[0]).last), (yyvsp[-6].name), node, &(yyval.union_dcl)));
      }
#line 2899 "src/parser.c"
    break;

  case 119: /* switch_type_spec: integer_type  */
#line 768 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2905 "src/parser.c"
    break;

  case 120: /* switch_type_spec: char_type  */
#line 770 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2911 "src/parser.c"
    break;

  case 121: /* switch_type_spec: boolean_type  */
#line 772 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2917 "src/parser.c"
    break;

  case 122: /* switch_type_spec: scoped_name  */
#line 774 "src/parser.y"
      { const idl_declaration_t *declaration;
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        idl_delete_scoped_name((yyvsp[0].scoped_name));
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
      }
#line 2927 "src/parser.c"
    break;

  case 123: /* switch_type_spec: wide_char_type  */
#line 780 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2933 "src/parser.c"
    break;

  case 124: /* switch_type_spec: octet_type  */
#line 782 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2939 "src/parser.c"
    break;

  case 125: /* switch_body: case  */
#line 787 "src/parser.y"
      { (yyval._case) = (yyvsp[0]._case); }
#line 2945 "src/parser.c"
    break;

  case 126: /* switch_body: switch_body case  */
#line 789 "src/parser.y"
      { (yyval._case) = idl_push_node((yyvsp[-1]._case), (yyvsp[0]._case)); }
#line 2951 "src/parser.c"
    break;

  case 127: /* case: case_labels element_spec ';'  */
#line 794 "src/parser.y"
      { TRY(idl_finalize_case(pstate, &(yylsp[-1]), (yyvsp[-1]._case), (yyvsp[-2].case_label)));
        (yyval._case) = (yyvsp[-1]._case);
      }
#line 2959 "src/parser.c"
    break;

  case 128: /* case_labels: case_label  */
#line 801 "src/parser.y"
      { (yyval.case_label) = (yyvsp[0].case_label); }
#line 2965 "src/parser.c"
    break;

  case 129: /* case_labels: case_labels case_label  */
#line 803 "src/parser.y"
      { (yyval.case_label) = idl_push_node((yyvsp[-1].case_label), (yyvsp[0].case_label)); }
#line 2971 "src/parser.c"
    break;

  case 130: /* case_label: "case" const_expr ':'  */
#line 808 "src/parser.y"
      { TRY(idl_create_case_label(pstate, LOC((yylsp[-2]).first, (yylsp[-1]).last), (yyvsp[-1].const_expr), &(yyval.case_label))); }
#line 2977 "src/parser.c"
    break;

  case 131: /* case_label: "default" ':'  */
#line 810 "src/parser.y"
      { TRY(idl_create_case_label(pstate, &(yylsp[-1]), NULL, &(yyval.case_label))); }
#line 2983 "src/parser.c"
    break;

  case 132: /* element_spec: type_spec declarator  */
#line 815 "src/parser.y"
      { TRY(idl_create_case(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval._case))); }
#line 2989 "src/parser.c"
    break;

  case 133: /* enum_dcl: enum_def  */
#line 818 "src/parser.y"
                   { (yyval.node) = (yyvsp[0].enum_dcl); }
#line 2995 "src/parser.c"
    break;

  case 134: /* enum_def: "enum" identifier '{' enumerators '}'  */
#line 822 "src/parser.y"
      { TRY(idl_create_enum(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].enumerator), &(yyval.enum_dcl))); }
#line 3001 "src/parser.c"
    break;

  case 135: /* enumerators: enumerator  */
#line 827 "src/parser.y"
      { (yyval.enumerator) = (yyvsp[0].enumerator); }
#line 3007 "src/parser.c"
    break;

  case 136: /* enumerators: enumerators ',' enumerator  */
#line 829 "src/parser.y"
      { (yyval.enumerator) = idl_push_node((yyvsp[-2].enumerator), (yyvsp[0].enumerator)); }
#line 3013 "src/parser.c"
    break;

  case 137: /* enumerator: annotations identifier  */
#line 834 "src/parser.y"
      { TRY(idl_create_enumerator(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.enumerator)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.enumerator), (yyvsp[-1].annotation_appl)), free((yyval.enumerator)));
      }
#line 3021 "src/parser.c"
    break;

  case 138: /* array_declarator: identifier fixed_array_sizes  */
#line 841 "src/parser.y"
      { TRY(idl_create_declarator(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].const_expr), &(yyval.declarator))); }
#line 3027 "src/parser.c"
    break;

  case 139: /* fixed_array_sizes: fixed_array_size  */
#line 846 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 3033 "src/parser.c"
    break;

  case 140: /* fixed_array_sizes: fixed_array_sizes fixed_array_size  */
#line 848 "src/parser.y"
      { (yyval.const_expr) = idl_push_node((yyvsp[-1].const_expr), (yyvsp[0].literal)); }
#line 3039 "src/parser.c"
    break;

  case 141: /* fixed_array_size: '[' positive_int_const ']'  */
#line 853 "src/parser.y"
      { (yyval.literal) = (yyvsp[-1].literal); }
#line 3045 "src/parser.c"
    break;

  case 142: /* simple_declarator: identifier  */
#line 858 "src/parser.y"
      { TRY(idl_create_declarator(pstate, &(yylsp[0]), (yyvsp[0].name), NULL, &(yyval.declarator))); }
#line 3051 "src/parser.c"
    break;

  case 144: /* typedef_dcl: "typedef" type_spec declarators  */
#line 865 "src/parser.y"
      { TRY(idl_create_typedef(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval.typedef_dcl))); }
#line 3057 "src/parser.c"
    break;

  case 145: /* declarators: declarator  */
#line 870 "src/parser.y"
      { (yyval.declarator) = (yyvsp[0].declarator); }
#line 3063 "src/parser.c"
    break;

  case 146: /* declarators: declarators ',' declarator  */
#line 872 "src/parser.y"
      { (yyval.declarator) = idl_push_node((yyvsp[-2].declarator), (yyvsp[0].declarator)); }
#line 3069 "src/parser.c"
    break;

  case 149: /* identifier: IDL_TOKEN_IDENTIFIER  */
#line 882 "src/parser.y"
      { (yyval.name) = NULL;
        size_t n;
        bool nocase = (pstate->flags & IDL_FLAG_CASE_SENSITIVE) == 0;
        if (pstate->parser.state == IDL_PARSE_ANNOTATION_APPL)
          n = 0;
        else if (pstate->parser.state == IDL_PARSE_ANNOTATION)
          n = 0;
        else if (!(n = ((yyvsp[0].str)[0] == '_')) && idl_iskeyword(pstate, (yyvsp[0].str), nocase))
          SEMANTIC_ERROR(pstate, &(yylsp[0]), "Identifier '%s' collides with a keyword", (yyvsp[0].str));

        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_name(pstate, &(yylsp[0]), idl_strdup((yyvsp[0].str)+n), &(yyval.name)));
      }
#line 3087 "src/parser.c"
    break;

  case 150: /* annotation_dcl: annotation_header '{' annotation_body '}'  */
#line 899 "src/parser.y"
      { (yyval.annotation) = NULL;
        /* discard annotation in case of redefinition */
        if (pstate->parser.state != IDL_PARSE_EXISTING_ANNOTATION_BODY)
          (yyval.annotation) = (yyvsp[-3].annotation);
        TRY(idl_finalize_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].annotation), (yyvsp[-1].annotation_member)));
      }
#line 3098 "src/parser.c"
    break;

  case 151: /* $@2: %empty  */
#line 909 "src/parser.y"
      { pstate->annotations = true; /* register annotation occurence */
        pstate->parser.state = IDL_PARSE_ANNOTATION;
      }
#line 3106 "src/parser.c"
    break;

  case 152: /* annotation_header: "@" "annotation" $@2 identifier  */
#line 913 "src/parser.y"
      { TRY(idl_create_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[-2]).last), (yyvsp[0].name), &(yyval.annotation))); }
#line 3112 "src/parser.c"
    break;

  case 153: /* annotation_body: %empty  */
#line 918 "src/parser.y"
      { (yyval.annotation_member) = NULL; }
#line 3118 "src/parser.c"
    break;

  case 154: /* annotation_body: annotation_body annotation_member ';'  */
#line 920 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].annotation_member)); }
#line 3124 "src/parser.c"
    break;

  case 155: /* annotation_body: annotation_body enum_dcl ';'  */
#line 922 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3130 "src/parser.c"
    break;

  case 156: /* annotation_body: annotation_body const_dcl ';'  */
#line 924 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].const_dcl)); }
#line 3136 "src/parser.c"
    break;

  case 157: /* annotation_body: annotation_body typedef_dcl ';'  */
#line 926 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].typedef_dcl)); }
#line 3142 "src/parser.c"
    break;

  case 158: /* annotation_member: annotation_member_type simple_declarator annotation_member_default  */
#line 931 "src/parser.y"
      { TRY(idl_create_annotation_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), (yyvsp[0].const_expr), &(yyval.annotation_member))); }
#line 3148 "src/parser.c"
    break;

  case 159: /* annotation_member_type: const_type  */
#line 936 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3154 "src/parser.c"
    break;

  case 160: /* annotation_member_type: any_const_type  */
#line 938 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3160 "src/parser.c"
    break;

  case 161: /* annotation_member_default: %empty  */
#line 943 "src/parser.y"
      { (yyval.const_expr) = NULL; }
#line 3166 "src/parser.c"
    break;

  case 162: /* annotation_member_default: "default" const_expr  */
#line 945 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 3172 "src/parser.c"
    break;

  case 163: /* any_const_type: "any"  */
#line 950 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), IDL_ANY, &(yyval.type_spec))); }
#line 3178 "src/parser.c"
    break;

  case 164: /* annotations: annotation_appls  */
#line 955 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3184 "src/parser.c"
    break;

  case 165: /* annotations: %empty  */
#line 957 "src/parser.y"
      { (yyval.annotation_appl) = NULL; }
#line 3190 "src/parser.c"
    break;

  case 166: /* annotation_appls: annotation_appl  */
#line 962 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3196 "src/parser.c"
    break;

  case 167: /* annotation_appls: annotation_appls annotation_appl  */
#line 964 "src/parser.y"
      { (yyval.annotation_appl) = idl_push_node((yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl)); }
#line 3202 "src/parser.c"
    break;

  case 168: /* $@3: %empty  */
#line 969 "src/parser.y"
      { pstate->parser.state = IDL_PARSE_ANNOTATION_APPL; }
#line 3208 "src/parser.c"
    break;

  case 169: /* @4: %empty  */
#line 971 "src/parser.y"
      { idl_annotation_appl_t *node = NULL;
        const idl_annotation_t *annotation;
        const idl_declaration_t *declaration =
          idl_find_scoped_name(pstate, NULL, (yyvsp[0].scoped_name), IDL_FIND_ANNOTATION);

        pstate->annotations = true; /* register annotation occurence */
        if (!declaration) {
          pstate->parser.state = IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS;
        } else {
          annotation = idl_reference_node((idl_node_t *)declaration->node);
          TRY(idl_create_annotation_appl(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), annotation, &node));
          pstate->parser.state = IDL_PARSE_ANNOTATION_APPL_PARAMS;
          pstate->annotation_scope = declaration->scope;
        }
        (yyval.annotation_appl) = node;
      }
#line 3229 "src/parser.c"
    break;

  case 170: /* annotation_appl: "@" $@3 annotation_appl_name @4 annotation_appl_params  */
#line 988 "src/parser.y"
      { if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_finalize_annotation_appl(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl_param)));
        pstate->parser.state = IDL_PARSE;
        pstate->annotation_scope = NULL;
        (yyval.annotation_appl) = (yyvsp[-1].annotation_appl);
        idl_delete_scoped_name((yyvsp[-2].scoped_name));
      }
#line 3241 "src/parser.c"
    break;

  case 171: /* annotation_appl_name: identifier  */
#line 999 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 3247 "src/parser.c"
    break;

  case 172: /* annotation_appl_name: IDL_TOKEN_SCOPE_NO_SPACE identifier  */
#line 1001 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 3253 "src/parser.c"
    break;

  case 173: /* annotation_appl_name: annotation_appl_name IDL_TOKEN_SCOPE_NO_SPACE identifier  */
#line 1003 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 3261 "src/parser.c"
    break;

  case 174: /* annotation_appl_params: %empty  */
#line 1010 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL; }
#line 3267 "src/parser.c"
    break;

  case 175: /* annotation_appl_params: '(' const_expr ')'  */
#line 1012 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].const_expr); }
#line 3273 "src/parser.c"
    break;

  case 176: /* annotation_appl_params: '(' annotation_appl_keyword_params ')'  */
#line 1014 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].annotation_appl_param); }
#line 3279 "src/parser.c"
    break;

  case 177: /* annotation_appl_keyword_params: annotation_appl_keyword_param  */
#line 1019 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[0].annotation_appl_param); }
#line 3285 "src/parser.c"
    break;

  case 178: /* annotation_appl_keyword_params: annotation_appl_keyword_params ',' annotation_appl_keyword_param  */
#line 1021 "src/parser.y"
      { (yyval.annotation_appl_param) = idl_push_node((yyvsp[-2].annotation_appl_param), (yyvsp[0].annotation_appl_param)); }
#line 3291 "src/parser.c"
    break;

  case 179: /* @5: %empty  */
#line 1026 "src/parser.y"
      { idl_annotation_member_t *node = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          const idl_declaration_t *declaration = NULL;
          static const char fmt[] =
            "Unknown annotation member '%s'";
          declaration = idl_find(pstate, pstate->annotation_scope, (yyvsp[0].name), 0u);
          if (declaration && (idl_mask(declaration->node) & IDL_DECLARATOR))
            node = (idl_annotation_member_t *)((const idl_node_t *)declaration->node)->parent;
          if (!node || !(idl_mask(node) & IDL_ANNOTATION_MEMBER))
            SEMANTIC_ERROR(pstate, &(yylsp[0]), fmt, (yyvsp[0].name)->identifier);
          node = idl_reference_node((idl_node_t *)node);
        }
        (yyval.annotation_member) = node;
        idl_delete_name((yyvsp[0].name));
      }
#line 3311 "src/parser.c"
    break;

  case 180: /* annotation_appl_keyword_param: identifier @5 '=' const_expr  */
#line 1042 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          TRY(idl_create_annotation_appl_param(pstate, &(yylsp[-3]), (yyvsp[-2].annotation_member), (yyvsp[0].const_expr), &(yyval.annotation_appl_param)));
        }
      }
#line 3321 "src/parser.c"
    break;


#line 3325 "src/parser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;
  *++yylsp = yyloc;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == IDL_YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (&yylloc, pstate, YY_("syntax error"));
    }

  yyerror_range[1] = yylloc;
  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= IDL_YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == IDL_YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, pstate);
          yychar = IDL_YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;

      yyerror_range[1] = *yylsp;
      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, pstate);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  ++yylsp;
  YYLLOC_DEFAULT (*yylsp, yyerror_range, 2);

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, pstate, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturn;
#endif


/*-------------------------------------------------------.
| yyreturn -- parsing is finished, clean up and return.  |
`-------------------------------------------------------*/
yyreturn:
  if (yychar != IDL_YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, pstate);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, pstate);
      YYPOPSTACK (1);
    }
  yyps->yynew = 2;
  goto yypushreturn;


/*-------------------------.
| yypushreturn -- return.  |
`-------------------------*/
yypushreturn:

  return yyresult;
}
#undef idl_yynerrs
#undef yystate
#undef yyerrstatus
#undef yyssa
#undef yyss
#undef yyssp
#undef yyvsa
#undef yyvs
#undef yyvsp
#undef yylsa
#undef yyls
#undef yylsp
#undef yystacksize
#line 1049 "src/parser.y"


#if defined(__GNUC__)
_Pragma("GCC diagnostic pop")
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
  if (yyps && !yyps->yynew)
    {
      YY_STACK_PRINT (yyss, yyssp);
      while (yyssp != yyss)
        {
          yydestruct ("Cleanup: popping",
                      yystos[*yyssp], yyvsp, yylsp, NULL);
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
      toknum = yytoknum[i];
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
      if (!(pstate->flags & IDL_FLAG_EXTENDED_DATA_TYPES))
        return 0;
      break;
    default:
      break;
  };

  return toknum;
}

static void
yyerror(idl_location_t *loc, idl_pstate_t *pstate, const char *str)
{
  idl_error(pstate, loc, str);
}
