/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

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

#line 157 "parser.c"

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
  YYSYMBOL_wstring_type = 117,             /* wstring_type  */
  YYSYMBOL_constr_type_dcl = 118,          /* constr_type_dcl  */
  YYSYMBOL_struct_dcl = 119,               /* struct_dcl  */
  YYSYMBOL_struct_forward_dcl = 120,       /* struct_forward_dcl  */
  YYSYMBOL_struct_def = 121,               /* struct_def  */
  YYSYMBOL_struct_header = 122,            /* struct_header  */
  YYSYMBOL_struct_inherit_spec = 123,      /* struct_inherit_spec  */
  YYSYMBOL_struct_body = 124,              /* struct_body  */
  YYSYMBOL_members = 125,                  /* members  */
  YYSYMBOL_member = 126,                   /* member  */
  YYSYMBOL_union_dcl = 127,                /* union_dcl  */
  YYSYMBOL_union_def = 128,                /* union_def  */
  YYSYMBOL_union_forward_dcl = 129,        /* union_forward_dcl  */
  YYSYMBOL_union_header = 130,             /* union_header  */
  YYSYMBOL_switch_header = 131,            /* switch_header  */
  YYSYMBOL_switch_type_spec = 132,         /* switch_type_spec  */
  YYSYMBOL_switch_body = 133,              /* switch_body  */
  YYSYMBOL_case = 134,                     /* case  */
  YYSYMBOL_case_labels = 135,              /* case_labels  */
  YYSYMBOL_case_label = 136,               /* case_label  */
  YYSYMBOL_element_spec = 137,             /* element_spec  */
  YYSYMBOL_enum_dcl = 138,                 /* enum_dcl  */
  YYSYMBOL_enum_def = 139,                 /* enum_def  */
  YYSYMBOL_enumerators = 140,              /* enumerators  */
  YYSYMBOL_enumerator = 141,               /* enumerator  */
  YYSYMBOL_bitmask_dcl = 142,              /* bitmask_dcl  */
  YYSYMBOL_bitmask_def = 143,              /* bitmask_def  */
  YYSYMBOL_bit_values = 144,               /* bit_values  */
  YYSYMBOL_bit_value = 145,                /* bit_value  */
  YYSYMBOL_array_declarator = 146,         /* array_declarator  */
  YYSYMBOL_fixed_array_sizes = 147,        /* fixed_array_sizes  */
  YYSYMBOL_fixed_array_size = 148,         /* fixed_array_size  */
  YYSYMBOL_simple_declarator = 149,        /* simple_declarator  */
  YYSYMBOL_complex_declarator = 150,       /* complex_declarator  */
  YYSYMBOL_typedef_dcl = 151,              /* typedef_dcl  */
  YYSYMBOL_declarators = 152,              /* declarators  */
  YYSYMBOL_declarator = 153,               /* declarator  */
  YYSYMBOL_identifier = 154,               /* identifier  */
  YYSYMBOL_annotation_dcl = 155,           /* annotation_dcl  */
  YYSYMBOL_annotation_header = 156,        /* annotation_header  */
  YYSYMBOL_157_1 = 157,                    /* $@1  */
  YYSYMBOL_annotation_body = 158,          /* annotation_body  */
  YYSYMBOL_annotation_member = 159,        /* annotation_member  */
  YYSYMBOL_annotation_member_type = 160,   /* annotation_member_type  */
  YYSYMBOL_annotation_member_default = 161, /* annotation_member_default  */
  YYSYMBOL_any_const_type = 162,           /* any_const_type  */
  YYSYMBOL_annotations = 163,              /* annotations  */
  YYSYMBOL_annotation_appls = 164,         /* annotation_appls  */
  YYSYMBOL_annotation_appl = 165,          /* annotation_appl  */
  YYSYMBOL_annotation_appl_header = 166,   /* annotation_appl_header  */
  YYSYMBOL_167_2 = 167,                    /* $@2  */
  YYSYMBOL_annotation_appl_name = 168,     /* annotation_appl_name  */
  YYSYMBOL_annotation_appl_params = 169,   /* annotation_appl_params  */
  YYSYMBOL_annotation_appl_keyword_params = 170, /* annotation_appl_keyword_params  */
  YYSYMBOL_annotation_appl_keyword_param = 171, /* annotation_appl_keyword_param  */
  YYSYMBOL_172_3 = 172                     /* @3  */
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

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
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
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
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
#define YYFINAL  13
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   414

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  76
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  97
/* YYNRULES -- Number of rules.  */
#define YYNRULES  195
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  291

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
       0,   290,   290,   292,   297,   299,   304,   306,   310,   314,
     321,   328,   333,   335,   337,   344,   349,   351,   353,   355,
     357,   359,   361,   375,   378,   379,   387,   388,   396,   397,
     405,   406,   414,   415,   418,   419,   427,   428,   431,   433,
     441,   442,   443,   446,   451,   456,   457,   458,   462,   478,
     480,   485,   507,   530,   537,   544,   555,   557,   562,   569,
     585,   590,   591,   595,   597,   601,   603,   616,   617,   618,
     619,   620,   621,   625,   626,   627,   631,   632,   636,   637,
     638,   640,   641,   642,   643,   647,   648,   649,   651,   652,
     653,   654,   658,   661,   664,   667,   670,   671,   672,   676,
     678,   683,   685,   690,   692,   697,   698,   699,   700,   704,
     705,   709,   714,   721,   726,   729,   744,   748,   753,   755,
     760,   767,   768,   772,   779,   784,   789,   800,   802,   804,
     806,   812,   814,   819,   821,   826,   833,   835,   840,   842,
     849,   855,   858,   863,   865,   870,   876,   879,   884,   886,
     891,   898,   903,   905,   910,   915,   919,   922,   924,   941,
     943,   948,   949,   953,   972,   983,   982,   991,   993,   995,
     997,   999,  1001,  1006,  1011,  1013,  1018,  1020,  1025,  1030,
    1032,  1037,  1039,  1044,  1055,  1054,  1080,  1082,  1084,  1091,
    1093,  1095,  1100,  1102,  1108,  1107
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
  "string_type", "wstring_type", "constr_type_dcl", "struct_dcl",
  "struct_forward_dcl", "struct_def", "struct_header",
  "struct_inherit_spec", "struct_body", "members", "member", "union_dcl",
  "union_def", "union_forward_dcl", "union_header", "switch_header",
  "switch_type_spec", "switch_body", "case", "case_labels", "case_label",
  "element_spec", "enum_dcl", "enum_def", "enumerators", "enumerator",
  "bitmask_dcl", "bitmask_def", "bit_values", "bit_value",
  "array_declarator", "fixed_array_sizes", "fixed_array_size",
  "simple_declarator", "complex_declarator", "typedef_dcl", "declarators",
  "declarator", "identifier", "annotation_dcl", "annotation_header", "$@1",
  "annotation_body", "annotation_member", "annotation_member_type",
  "annotation_member_default", "any_const_type", "annotations",
  "annotation_appls", "annotation_appl", "annotation_appl_header", "$@2",
  "annotation_appl_name", "annotation_appl_params",
  "annotation_appl_keyword_params", "annotation_appl_keyword_param", "@3", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-202)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-195)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      54,    26,    42,    72,  -202,    41,    31,    83,    89,  -202,
      44,  -202,    68,  -202,  -202,  -202,  -202,   103,   335,   103,
     245,   103,   103,   103,    69,    59,    71,    75,  -202,  -202,
    -202,  -202,    76,  -202,  -202,  -202,    78,  -202,  -202,  -202,
    -202,  -202,  -202,  -202,    17,  -202,   103,  -202,   103,  -202,
     117,   150,  -202,   103,    58,    63,  -202,  -202,  -202,    77,
    -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,
    -202,   124,   103,  -202,  -202,  -202,  -202,  -202,  -202,  -202,
    -202,  -202,   -34,    73,    79,  -202,   124,   103,  -202,  -202,
    -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,
     103,   118,    82,    84,  -202,   130,  -202,  -202,     6,    95,
    -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,  -202,    17,
     124,    81,    86,    87,    85,    67,    60,    40,  -202,    43,
    -202,  -202,  -202,   140,    93,    45,  -202,  -202,  -202,   103,
    -202,  -202,    98,  -202,   100,   102,   105,   106,   103,  -202,
    -202,  -202,   125,    17,  -202,  -202,   103,   107,    51,  -202,
     290,    17,  -202,  -202,  -202,    96,  -202,    90,    96,   104,
    -202,    89,    89,    29,   111,    32,  -202,   290,    17,    97,
      -2,  -202,    70,  -202,   108,  -202,    17,    17,    17,  -202,
    -202,    17,  -202,  -202,    17,  -202,  -202,  -202,    17,  -202,
    -202,   121,  -202,   103,  -202,  -202,  -202,  -202,  -202,  -202,
     162,  -202,  -202,  -202,   129,  -202,    17,   124,    57,   131,
     103,    17,    90,  -202,    89,   -13,  -202,   103,     2,  -202,
     103,  -202,  -202,  -202,   103,   116,  -202,  -202,  -202,  -202,
     135,   290,  -202,    87,    85,    67,    60,    40,  -202,    17,
    -202,  -202,    17,  -202,  -202,  -202,    17,  -202,  -202,  -202,
     127,  -202,   364,  -202,    89,  -202,  -202,    89,  -202,   -24,
    -202,  -202,   103,  -202,  -202,   132,  -202,   172,   124,  -202,
    -202,  -202,  -202,  -202,   141,  -202,  -202,  -202,  -202,  -202,
    -202
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
     180,   184,     0,   180,     4,     0,     0,     0,   179,   181,
     189,   165,     0,     1,     5,     6,   167,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    61,   105,
     110,   109,     0,   106,   121,   122,     0,   107,   141,   108,
     146,    62,   184,   182,     0,   183,     0,   163,     0,   186,
     185,     0,    11,     0,     0,   102,    73,    74,    78,    79,
      92,    94,    95,    81,    82,    83,    84,    88,    89,    90,
      91,    22,     0,    17,    16,    76,    77,    18,    19,    20,
      21,    12,   111,     0,   104,    93,    66,     0,    63,    65,
      68,    67,    69,    70,    71,    72,    64,    96,    97,    98,
       0,   124,     0,     0,     7,   180,     8,     9,   180,     0,
      53,    58,    51,    52,    56,    57,    46,    45,    47,     0,
      48,     0,    23,    24,    26,    28,    30,    34,    38,     0,
      44,    49,    54,    55,    12,     0,   192,   166,   187,     0,
     178,   164,     0,   174,     0,     0,     0,     0,     0,   175,
      13,    85,    86,     0,    75,    80,     0,     0,     0,   113,
       0,     0,   156,   161,   162,   157,   159,   155,   158,     0,
     125,   180,   180,   180,     0,   180,   118,     0,     0,     0,
       0,   133,   180,   136,     0,   190,     0,     0,     0,    33,
      32,     0,    36,    37,     0,    40,    41,    42,     0,    43,
      59,     0,   191,     0,   188,   171,   169,   170,   172,   168,
     176,   155,    87,    60,     0,    14,     0,   115,     0,     0,
       0,     0,   151,   152,   180,     0,   143,     0,     0,   148,
       0,    10,   112,   119,     0,     0,   139,   123,   134,   137,
       0,     0,    50,    25,    27,    29,    31,    35,    39,     0,
     194,   193,     0,   173,   101,    15,     0,   100,   103,   160,
       0,   153,     0,   142,   180,   145,   147,   180,   150,     0,
     138,   135,     0,   195,   177,     0,   154,    79,   130,   127,
     128,   131,   129,   132,     0,   144,   149,   120,   140,    99,
     126
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -202,  -202,   109,     3,  -202,  -202,    -6,   160,   161,   -43,
    -202,    27,    30,    28,  -202,    33,  -202,    25,  -202,    22,
    -202,    92,  -202,  -202,  -202,  -143,  -202,  -144,  -202,  -202,
      10,   -16,  -202,  -202,   -14,   -39,    -5,    -3,  -202,  -202,
      11,  -202,   205,  -202,  -202,  -202,  -202,  -202,  -202,  -202,
      52,  -202,  -202,  -202,  -202,  -202,  -202,  -202,    46,  -202,
      47,  -202,   177,  -202,  -202,   -33,   179,  -202,  -202,   -35,
    -202,  -202,    12,    88,  -202,   182,   -92,  -201,   -12,  -202,
    -202,  -202,  -202,  -202,  -202,  -202,  -202,  -105,  -202,   229,
    -202,  -202,  -202,  -202,  -202,    35,  -202
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
       0,     2,     3,     4,    24,    25,   120,    26,    72,   213,
     122,   123,   124,   125,   191,   126,   194,   127,   198,   128,
     129,   130,   131,   132,   133,   214,    27,    87,    88,    89,
      90,    91,    75,    76,    92,    93,    94,    95,    96,    97,
      98,    99,    28,    29,    30,    31,    32,   159,   174,   175,
     176,    33,    34,    35,    36,   170,   284,   180,   181,   182,
     183,   240,    37,    38,   225,   226,    39,    40,   228,   229,
     162,   222,   223,   163,   164,    41,   165,   166,    81,     5,
       6,    46,    51,   147,   148,   253,   149,     7,     8,     9,
      10,    12,    50,    45,   135,   136,   201
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      49,   121,    74,   177,    77,    52,    14,    82,   168,   101,
     102,   103,    71,    78,    86,    79,   218,    42,   219,   259,
     178,   179,  -114,    47,   110,   111,   112,   113,    73,    80,
      53,   287,   134,   234,   137,    74,   138,    77,    11,   158,
       1,   150,    13,    42,   263,    71,    78,   220,    79,    47,
     110,   111,   112,   113,    -2,   237,    53,    47,   264,   266,
     157,    73,    80,  -117,    53,     1,   227,   230,   114,   115,
     177,   288,    -3,   267,    47,   167,   184,   241,   260,   116,
     117,    42,    48,     1,   118,   119,   231,    16,   167,  -116,
     151,   152,   178,   179,   114,   115,    15,   272,    17,    18,
      42,    19,    20,    21,   195,   196,   197,    22,   154,    47,
     155,   119,    44,   275,   202,   105,   203,   178,   179,   262,
     189,   190,   192,   193,   104,    23,   106,   204,   256,   257,
     107,   139,   108,   153,   109,   235,   211,   156,   171,   169,
     172,     1,   269,   160,   215,   186,   188,   187,   200,   161,
     185,  -194,   217,   205,    86,   206,    47,   207,   212,   227,
     208,   209,   230,    53,   221,   216,    18,   220,   232,    20,
     236,    86,   224,   255,    22,    54,    14,   242,    55,   249,
      56,    57,    58,    59,    60,   252,    61,    62,   140,   270,
     271,   250,    23,    63,    64,    65,    66,    67,    68,    69,
      70,   254,   276,   258,   289,   155,   273,   141,   167,   274,
     290,   142,   143,   243,   173,   265,   245,   244,   268,   247,
     248,   199,   167,   281,   246,   100,   238,   233,   144,   239,
     145,   285,   286,   146,   261,    86,   210,    43,   251,     0,
       0,     0,     0,     0,     0,     0,   279,     0,   280,     0,
       0,    47,     0,     0,     0,     0,   278,   282,    53,   283,
     167,     0,     0,    19,     0,    21,     0,     0,     0,    22,
      54,     0,    83,    55,    84,    56,    57,    58,    59,    60,
      85,    61,    62,     0,     0,     0,     0,    23,    63,    64,
      65,    66,    67,    68,    69,    70,    47,     0,     0,     0,
       0,     0,     0,    53,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,     0,    83,    55,    84,
      56,    57,    58,    59,    60,    85,    61,    62,     0,     0,
       0,     0,     0,    63,    64,    65,    66,    67,    68,    69,
      70,    47,     0,     0,     0,     0,     0,     0,    53,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      54,     0,     0,    55,     0,    56,    57,    58,    59,    60,
      47,    61,    62,     0,     0,     0,     0,    53,    63,    64,
      65,    66,    67,    68,    69,    70,     0,     0,     0,    54,
       0,     0,     0,     0,     0,     0,    58,   277,    60,    85,
      61,    62,     0,     0,     0,     0,     0,    63,    64,    65,
      66,    67,    68,    69,    70
};

static const yytype_int16 yycheck[] =
{
      12,    44,    18,   108,    18,    17,     3,    19,   100,    21,
      22,    23,    18,    18,    20,    18,   160,    11,   161,   220,
      22,    23,    56,     6,     7,     8,     9,    10,    18,    18,
      13,    55,    44,   177,    46,    51,    48,    51,    12,    73,
      11,    53,     0,    11,    57,    51,    51,    71,    51,     6,
       7,     8,     9,    10,     0,    57,    13,     6,    71,    57,
      72,    51,    51,    57,    13,    11,   171,   172,    51,    52,
     175,   272,     0,    71,     6,    87,   119,   182,   221,    62,
      63,    11,    14,    11,    67,    68,    57,    56,   100,    57,
      32,    33,    22,    23,    51,    52,    55,   241,    15,    16,
      11,    18,    19,    20,    64,    65,    66,    24,    31,     6,
      33,    68,    68,   256,    69,    56,    71,    22,    23,   224,
      53,    54,    62,    63,    55,    42,    55,   139,    71,    72,
      55,    14,    56,    70,    56,   178,   148,    13,    56,    21,
      56,    11,   234,    70,   156,    59,    61,    60,     8,    70,
      69,    58,   158,    55,   160,    55,     6,    55,    33,   264,
      55,    55,   267,    13,    74,    58,    16,    71,    57,    19,
      73,   177,    68,   216,    24,    25,   173,    69,    28,    58,
      30,    31,    32,    33,    34,    23,    36,    37,    38,    73,
      55,   203,    42,    43,    44,    45,    46,    47,    48,    49,
      50,    72,    75,    72,    72,    33,   249,    57,   220,   252,
      69,    51,    51,   186,   105,   227,   188,   187,   230,   194,
     198,   129,   234,   262,   191,    20,   180,   175,    51,   182,
      51,   264,   267,    51,   222,   241,   148,     8,   203,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,   262,    -1,   262,    -1,
      -1,     6,    -1,    -1,    -1,    -1,   262,   262,    13,   262,
     272,    -1,    -1,    18,    -1,    20,    -1,    -1,    -1,    24,
      25,    -1,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    -1,    -1,    -1,    -1,    42,    43,    44,
      45,    46,    47,    48,    49,    50,     6,    -1,    -1,    -1,
      -1,    -1,    -1,    13,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,    -1,    27,    28,    29,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    -1,    43,    44,    45,    46,    47,    48,    49,
      50,     6,    -1,    -1,    -1,    -1,    -1,    -1,    13,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,    -1,    -1,    28,    -1,    30,    31,    32,    33,    34,
       6,    36,    37,    -1,    -1,    -1,    -1,    13,    43,    44,
      45,    46,    47,    48,    49,    50,    -1,    -1,    -1,    25,
      -1,    -1,    -1,    -1,    -1,    -1,    32,    33,    34,    35,
      36,    37,    -1,    -1,    -1,    -1,    -1,    43,    44,    45,
      46,    47,    48,    49,    50
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    11,    77,    78,    79,   155,   156,   163,   164,   165,
     166,    12,   167,     0,    79,    55,    56,    15,    16,    18,
      19,    20,    24,    42,    80,    81,    83,   102,   118,   119,
     120,   121,   122,   127,   128,   129,   130,   138,   139,   142,
     143,   151,    11,   165,    68,   169,   157,     6,    14,   154,
     168,   158,   154,    13,    25,    28,    30,    31,    32,    33,
      34,    36,    37,    43,    44,    45,    46,    47,    48,    49,
      50,    82,    84,   106,   107,   108,   109,   110,   112,   113,
     116,   154,   154,    27,    29,    35,    82,   103,   104,   105,
     106,   107,   110,   111,   112,   113,   114,   115,   116,   117,
     118,   154,   154,   154,    55,    56,    55,    55,    56,    56,
       7,     8,     9,    10,    51,    52,    62,    63,    67,    68,
      82,    85,    86,    87,    88,    89,    91,    93,    95,    96,
      97,    98,    99,   100,   154,   170,   171,   154,   154,    14,
      38,    57,    83,    84,   138,   142,   151,   159,   160,   162,
     154,    32,    33,    70,    31,    33,    13,   154,    73,   123,
      70,    70,   146,   149,   150,   152,   153,   154,   152,    21,
     131,    56,    56,    78,   124,   125,   126,   163,    22,    23,
     133,   134,   135,   136,    85,    69,    59,    60,    61,    53,
      54,    90,    62,    63,    92,    64,    65,    66,    94,    97,
       8,   172,    69,    71,   154,    55,    55,    55,    55,    55,
     149,   154,    33,    85,   101,   154,    58,    82,   103,   101,
      71,    74,   147,   148,    68,   140,   141,   163,   144,   145,
     163,    57,    57,   126,   103,    85,    73,    57,   134,   136,
     137,   163,    69,    87,    88,    89,    91,    93,    95,    58,
     154,   171,    23,   161,    72,    85,    71,    72,    72,   153,
     101,   148,   163,    57,    71,   154,    57,    71,   154,   152,
      73,    55,   103,    85,    85,   101,    75,    33,    82,   107,
     110,   111,   112,   113,   132,   141,   145,    55,   153,    72,
      69
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
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
     109,   109,   110,   111,   112,   113,   114,   114,   114,   115,
     115,   116,   116,   117,   117,   118,   118,   118,   118,   119,
     119,   120,   121,   122,   123,   123,   124,   124,   125,   125,
     126,   127,   127,   128,   129,   130,   131,   132,   132,   132,
     132,   132,   132,   133,   133,   134,   135,   135,   136,   136,
     137,   138,   139,   140,   140,   141,   142,   143,   144,   144,
     145,   146,   147,   147,   148,   149,   150,   151,   151,   152,
     152,   153,   153,   154,   155,   157,   156,   158,   158,   158,
     158,   158,   158,   159,   160,   160,   161,   161,   162,   163,
     163,   164,   164,   165,   167,   166,   168,   168,   168,   169,
     169,   169,   170,   170,   172,   171
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
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
       1,     1,     1,     1,     1,     1,     1,     1,     1,     6,
       4,     4,     1,     4,     1,     1,     1,     1,     1,     1,
       1,     2,     4,     3,     0,     2,     1,     0,     1,     2,
       4,     1,     1,     4,     2,     3,     5,     1,     1,     1,
       1,     1,     1,     1,     2,     3,     1,     2,     3,     2,
       3,     1,     5,     1,     3,     2,     1,     5,     1,     3,
       2,     2,     1,     2,     3,     1,     1,     3,     3,     1,
       3,     1,     1,     1,     4,     0,     4,     0,     3,     3,
       3,     3,     3,     3,     1,     1,     0,     2,     1,     1,
       0,     1,     2,     2,     0,     3,     1,     2,     3,     0,
       3,     3,     1,     3,     0,     4
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = IDL_YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


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
        yyerror (&yylloc, pstate, result, YY_("syntax error: cannot back up")); \
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


/* YYLOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

# ifndef YYLOCATION_PRINT

#  if defined YY_LOCATION_PRINT

   /* Temporary convenience wrapper in case some people defined the
      undocumented and private YY_LOCATION_PRINT macros.  */
#   define YYLOCATION_PRINT(File, Loc)  YY_LOCATION_PRINT(File, *(Loc))

#  elif defined IDL_YYLTYPE_IS_TRIVIAL && IDL_YYLTYPE_IS_TRIVIAL

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

#   define YYLOCATION_PRINT  yy_location_print_

    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT(File, Loc)  YYLOCATION_PRINT(File, &(Loc))

#  else

#   define YYLOCATION_PRINT(File, Loc) ((void) 0)
    /* Temporary convenience wrapper in case some people defined the
       undocumented and private YY_LOCATION_PRINT macros.  */
#   define YY_LOCATION_PRINT  YYLOCATION_PRINT

#  endif
# endif /* !defined YYLOCATION_PRINT */


# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value, Location, pstate, result); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  YY_USE (yylocationp);
  YY_USE (pstate);
  YY_USE (result);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YYLOCATION_PRINT (yyo, yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yykind, yyvaluep, yylocationp, pstate, result);
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
                 int yyrule, idl_pstate_t *pstate, idl_retcode_t *result)
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
                       &(yylsp[(yyi + 1) - (yynrhs)]), pstate, result);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, yylsp, Rule, pstate, result); \
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
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  YY_USE (yyvaluep);
  YY_USE (yylocationp);
  YY_USE (pstate);
  YY_USE (result);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_definitions: /* definitions  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1434 "parser.c"
        break;

    case YYSYMBOL_definition: /* definition  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1440 "parser.c"
        break;

    case YYSYMBOL_module_dcl: /* module_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1446 "parser.c"
        break;

    case YYSYMBOL_module_header: /* module_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1452 "parser.c"
        break;

    case YYSYMBOL_scoped_name: /* scoped_name  */
#line 212 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1458 "parser.c"
        break;

    case YYSYMBOL_const_dcl: /* const_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).const_dcl)); }
#line 1464 "parser.c"
        break;

    case YYSYMBOL_const_type: /* const_type  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1470 "parser.c"
        break;

    case YYSYMBOL_const_expr: /* const_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1476 "parser.c"
        break;

    case YYSYMBOL_or_expr: /* or_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1482 "parser.c"
        break;

    case YYSYMBOL_xor_expr: /* xor_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1488 "parser.c"
        break;

    case YYSYMBOL_and_expr: /* and_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1494 "parser.c"
        break;

    case YYSYMBOL_shift_expr: /* shift_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1500 "parser.c"
        break;

    case YYSYMBOL_add_expr: /* add_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1506 "parser.c"
        break;

    case YYSYMBOL_mult_expr: /* mult_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1512 "parser.c"
        break;

    case YYSYMBOL_unary_expr: /* unary_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1518 "parser.c"
        break;

    case YYSYMBOL_primary_expr: /* primary_expr  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1524 "parser.c"
        break;

    case YYSYMBOL_literal: /* literal  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1530 "parser.c"
        break;

    case YYSYMBOL_string_literal: /* string_literal  */
#line 207 "src/parser.y"
            { idl_free(((*yyvaluep).string_literal)); }
#line 1536 "parser.c"
        break;

    case YYSYMBOL_positive_int_const: /* positive_int_const  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1542 "parser.c"
        break;

    case YYSYMBOL_type_dcl: /* type_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1548 "parser.c"
        break;

    case YYSYMBOL_type_spec: /* type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1554 "parser.c"
        break;

    case YYSYMBOL_simple_type_spec: /* simple_type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1560 "parser.c"
        break;

    case YYSYMBOL_template_type_spec: /* template_type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1566 "parser.c"
        break;

    case YYSYMBOL_sequence_type: /* sequence_type  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).sequence)); }
#line 1572 "parser.c"
        break;

    case YYSYMBOL_string_type: /* string_type  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).string)); }
#line 1578 "parser.c"
        break;

    case YYSYMBOL_wstring_type: /* wstring_type  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).wstring)); }
#line 1584 "parser.c"
        break;

    case YYSYMBOL_constr_type_dcl: /* constr_type_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1590 "parser.c"
        break;

    case YYSYMBOL_struct_dcl: /* struct_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1596 "parser.c"
        break;

    case YYSYMBOL_struct_forward_dcl: /* struct_forward_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).forward)); }
#line 1602 "parser.c"
        break;

    case YYSYMBOL_struct_def: /* struct_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1608 "parser.c"
        break;

    case YYSYMBOL_struct_header: /* struct_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1614 "parser.c"
        break;

    case YYSYMBOL_struct_inherit_spec: /* struct_inherit_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1620 "parser.c"
        break;

    case YYSYMBOL_struct_body: /* struct_body  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1626 "parser.c"
        break;

    case YYSYMBOL_members: /* members  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1632 "parser.c"
        break;

    case YYSYMBOL_member: /* member  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1638 "parser.c"
        break;

    case YYSYMBOL_union_dcl: /* union_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1644 "parser.c"
        break;

    case YYSYMBOL_union_def: /* union_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1650 "parser.c"
        break;

    case YYSYMBOL_union_forward_dcl: /* union_forward_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).forward)); }
#line 1656 "parser.c"
        break;

    case YYSYMBOL_union_header: /* union_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1662 "parser.c"
        break;

    case YYSYMBOL_switch_header: /* switch_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).switch_type_spec)); }
#line 1668 "parser.c"
        break;

    case YYSYMBOL_switch_type_spec: /* switch_type_spec  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1674 "parser.c"
        break;

    case YYSYMBOL_switch_body: /* switch_body  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1680 "parser.c"
        break;

    case YYSYMBOL_case: /* case  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1686 "parser.c"
        break;

    case YYSYMBOL_case_labels: /* case_labels  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1692 "parser.c"
        break;

    case YYSYMBOL_case_label: /* case_label  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1698 "parser.c"
        break;

    case YYSYMBOL_element_spec: /* element_spec  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1704 "parser.c"
        break;

    case YYSYMBOL_enum_dcl: /* enum_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1710 "parser.c"
        break;

    case YYSYMBOL_enum_def: /* enum_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enum_dcl)); }
#line 1716 "parser.c"
        break;

    case YYSYMBOL_enumerators: /* enumerators  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1722 "parser.c"
        break;

    case YYSYMBOL_enumerator: /* enumerator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1728 "parser.c"
        break;

    case YYSYMBOL_bitmask_dcl: /* bitmask_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1734 "parser.c"
        break;

    case YYSYMBOL_bitmask_def: /* bitmask_def  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bitmask_dcl)); }
#line 1740 "parser.c"
        break;

    case YYSYMBOL_bit_values: /* bit_values  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bit_value)); }
#line 1746 "parser.c"
        break;

    case YYSYMBOL_bit_value: /* bit_value  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bit_value)); }
#line 1752 "parser.c"
        break;

    case YYSYMBOL_array_declarator: /* array_declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1758 "parser.c"
        break;

    case YYSYMBOL_fixed_array_sizes: /* fixed_array_sizes  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1764 "parser.c"
        break;

    case YYSYMBOL_fixed_array_size: /* fixed_array_size  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1770 "parser.c"
        break;

    case YYSYMBOL_simple_declarator: /* simple_declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1776 "parser.c"
        break;

    case YYSYMBOL_complex_declarator: /* complex_declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1782 "parser.c"
        break;

    case YYSYMBOL_typedef_dcl: /* typedef_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).typedef_dcl)); }
#line 1788 "parser.c"
        break;

    case YYSYMBOL_declarators: /* declarators  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1794 "parser.c"
        break;

    case YYSYMBOL_declarator: /* declarator  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1800 "parser.c"
        break;

    case YYSYMBOL_identifier: /* identifier  */
#line 209 "src/parser.y"
            { idl_delete_name(((*yyvaluep).name)); }
#line 1806 "parser.c"
        break;

    case YYSYMBOL_annotation_dcl: /* annotation_dcl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1812 "parser.c"
        break;

    case YYSYMBOL_annotation_header: /* annotation_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1818 "parser.c"
        break;

    case YYSYMBOL_annotation_body: /* annotation_body  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1824 "parser.c"
        break;

    case YYSYMBOL_annotation_member: /* annotation_member  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1830 "parser.c"
        break;

    case YYSYMBOL_annotation_member_type: /* annotation_member_type  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1836 "parser.c"
        break;

    case YYSYMBOL_annotation_member_default: /* annotation_member_default  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1842 "parser.c"
        break;

    case YYSYMBOL_any_const_type: /* any_const_type  */
#line 215 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1848 "parser.c"
        break;

    case YYSYMBOL_annotations: /* annotations  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1854 "parser.c"
        break;

    case YYSYMBOL_annotation_appls: /* annotation_appls  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1860 "parser.c"
        break;

    case YYSYMBOL_annotation_appl: /* annotation_appl  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1866 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_header: /* annotation_appl_header  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1872 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_name: /* annotation_appl_name  */
#line 212 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1878 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_params: /* annotation_appl_params  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1884 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_keyword_params: /* annotation_appl_keyword_params  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1890 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_keyword_param: /* annotation_appl_keyword_param  */
#line 218 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1896 "parser.c"
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
              int yypushed_char, YYSTYPE const *yypushed_val, YYLTYPE *yypushed_loc, idl_pstate_t *pstate, idl_retcode_t *result)
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
    case 0:
      yyn = yypact[yystate];
      goto yyread_pushed_token;

    case 2:
      yypstate_clear (yyps);
      break;

    default:
      break;
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
    YYNOMEM;
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
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
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
#line 291 "src/parser.y"
      { pstate->root = NULL; }
#line 2262 "parser.c"
    break;

  case 3: /* specification: definitions  */
#line 293 "src/parser.y"
      { pstate->root = (yyvsp[0].node); }
#line 2268 "parser.c"
    break;

  case 4: /* definitions: definition  */
#line 298 "src/parser.y"
      { (yyval.node) = (yyvsp[0].node); }
#line 2274 "parser.c"
    break;

  case 5: /* definitions: definitions definition  */
#line 300 "src/parser.y"
      { (yyval.node) = idl_push_node((yyvsp[-1].node), (yyvsp[0].node)); }
#line 2280 "parser.c"
    break;

  case 6: /* definition: annotation_dcl ';'  */
#line 305 "src/parser.y"
      { (yyval.node) = (yyvsp[-1].annotation); }
#line 2286 "parser.c"
    break;

  case 7: /* definition: annotations module_dcl ';'  */
#line 307 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].module_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].module_dcl);
      }
#line 2294 "parser.c"
    break;

  case 8: /* definition: annotations const_dcl ';'  */
#line 311 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].const_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].const_dcl);
      }
#line 2302 "parser.c"
    break;

  case 9: /* definition: annotations type_dcl ';'  */
#line 315 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].node), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].node);
      }
#line 2310 "parser.c"
    break;

  case 10: /* module_dcl: module_header '{' definitions '}'  */
#line 322 "src/parser.y"
      { TRY(idl_finalize_module(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].module_dcl), (yyvsp[-1].node)));
        (yyval.module_dcl) = (yyvsp[-3].module_dcl);
      }
#line 2318 "parser.c"
    break;

  case 11: /* module_header: "module" identifier  */
#line 329 "src/parser.y"
      { TRY(idl_create_module(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), &(yyval.module_dcl))); }
#line 2324 "parser.c"
    break;

  case 12: /* scoped_name: identifier  */
#line 334 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 2330 "parser.c"
    break;

  case 13: /* scoped_name: IDL_TOKEN_SCOPE identifier  */
#line 336 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 2336 "parser.c"
    break;

  case 14: /* scoped_name: scoped_name IDL_TOKEN_SCOPE identifier  */
#line 338 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 2344 "parser.c"
    break;

  case 15: /* const_dcl: "const" const_type identifier '=' const_expr  */
#line 345 "src/parser.y"
      { TRY(idl_create_const(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-2].name), (yyvsp[0].const_expr), &(yyval.const_dcl))); }
#line 2350 "parser.c"
    break;

  case 16: /* const_type: integer_type  */
#line 350 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2356 "parser.c"
    break;

  case 17: /* const_type: floating_pt_type  */
#line 352 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2362 "parser.c"
    break;

  case 18: /* const_type: char_type  */
#line 354 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2368 "parser.c"
    break;

  case 19: /* const_type: boolean_type  */
#line 356 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2374 "parser.c"
    break;

  case 20: /* const_type: octet_type  */
#line 358 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2380 "parser.c"
    break;

  case 21: /* const_type: string_type  */
#line 360 "src/parser.y"
      { (yyval.type_spec) = (idl_type_spec_t *)(yyvsp[0].string); }
#line 2386 "parser.c"
    break;

  case 22: /* const_type: scoped_name  */
#line 362 "src/parser.y"
      { idl_node_t *node;
        const idl_declaration_t *declaration;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a valid constant type";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        node = idl_unalias(declaration->node);
        if (!(idl_mask(node) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM|IDL_BITMASK)))
          SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2402 "parser.c"
    break;

  case 23: /* const_expr: or_expr  */
#line 375 "src/parser.y"
                    { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2408 "parser.c"
    break;

  case 25: /* or_expr: or_expr '|' xor_expr  */
#line 380 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_OR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2417 "parser.c"
    break;

  case 27: /* xor_expr: xor_expr '^' and_expr  */
#line 389 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_XOR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2426 "parser.c"
    break;

  case 29: /* and_expr: and_expr '&' shift_expr  */
#line 398 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_AND, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2435 "parser.c"
    break;

  case 31: /* shift_expr: shift_expr shift_operator add_expr  */
#line 407 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2444 "parser.c"
    break;

  case 32: /* shift_operator: ">>"  */
#line 414 "src/parser.y"
         { (yyval.kind) = IDL_RSHIFT; }
#line 2450 "parser.c"
    break;

  case 33: /* shift_operator: "<<"  */
#line 415 "src/parser.y"
         { (yyval.kind) = IDL_LSHIFT; }
#line 2456 "parser.c"
    break;

  case 35: /* add_expr: add_expr add_operator mult_expr  */
#line 420 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2465 "parser.c"
    break;

  case 36: /* add_operator: '+'  */
#line 427 "src/parser.y"
        { (yyval.kind) = IDL_ADD; }
#line 2471 "parser.c"
    break;

  case 37: /* add_operator: '-'  */
#line 428 "src/parser.y"
        { (yyval.kind) = IDL_SUBTRACT; }
#line 2477 "parser.c"
    break;

  case 38: /* mult_expr: unary_expr  */
#line 432 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2483 "parser.c"
    break;

  case 39: /* mult_expr: mult_expr mult_operator unary_expr  */
#line 434 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2492 "parser.c"
    break;

  case 40: /* mult_operator: '*'  */
#line 441 "src/parser.y"
        { (yyval.kind) = IDL_MULTIPLY; }
#line 2498 "parser.c"
    break;

  case 41: /* mult_operator: '/'  */
#line 442 "src/parser.y"
        { (yyval.kind) = IDL_DIVIDE; }
#line 2504 "parser.c"
    break;

  case 42: /* mult_operator: '%'  */
#line 443 "src/parser.y"
        { (yyval.kind) = IDL_MODULO; }
#line 2510 "parser.c"
    break;

  case 43: /* unary_expr: unary_operator primary_expr  */
#line 447 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_unary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2519 "parser.c"
    break;

  case 44: /* unary_expr: primary_expr  */
#line 452 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2525 "parser.c"
    break;

  case 45: /* unary_operator: '-'  */
#line 456 "src/parser.y"
        { (yyval.kind) = IDL_MINUS; }
#line 2531 "parser.c"
    break;

  case 46: /* unary_operator: '+'  */
#line 457 "src/parser.y"
        { (yyval.kind) = IDL_PLUS; }
#line 2537 "parser.c"
    break;

  case 47: /* unary_operator: '~'  */
#line 458 "src/parser.y"
        { (yyval.kind) = IDL_NOT; }
#line 2543 "parser.c"
    break;

  case 48: /* primary_expr: scoped_name  */
#line 463 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          /* disregard scoped names in application of unknown annotations.
             names may or may not have significance in the scope of the
             (builtin) annotation, stick to syntax checks */
          const idl_declaration_t *declaration = NULL;
          static const char fmt[] =
            "Scoped name '%s' does not resolve to an enumerator or a constant";
          TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
          if (!(idl_mask(declaration->node) & (IDL_CONST|IDL_ENUMERATOR|IDL_BIT_VALUE)))
            SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
          (yyval.const_expr) = idl_reference_node((idl_node_t *)declaration->node);
        }
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2563 "parser.c"
    break;

  case 49: /* primary_expr: literal  */
#line 479 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 2569 "parser.c"
    break;

  case 50: /* primary_expr: '(' const_expr ')'  */
#line 481 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[-1].const_expr); }
#line 2575 "parser.c"
    break;

  case 51: /* literal: IDL_TOKEN_INTEGER_LITERAL  */
#line 486 "src/parser.y"
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
#line 2601 "parser.c"
    break;

  case 52: /* literal: IDL_TOKEN_FLOATING_PT_LITERAL  */
#line 508 "src/parser.y"
      { idl_type_t type;
        idl_literal_t literal;
        (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
#if __MINGW32__
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"")
#endif
        if (isnan((double)(yyvsp[0].ldbl)) || isinf((double)(yyvsp[0].ldbl))) {
#if __MINGW32__
_Pragma("GCC diagnostic pop")
#endif
          type = IDL_LDOUBLE;
          literal.value.ldbl = (yyvsp[0].ldbl);
        } else {
          type = IDL_DOUBLE;
          literal.value.dbl = (double)(yyvsp[0].ldbl);
        }
        TRY(idl_create_literal(pstate, &(yylsp[0]), type, &(yyval.literal)));
        (yyval.literal)->value = literal.value;
      }
#line 2628 "parser.c"
    break;

  case 53: /* literal: IDL_TOKEN_CHAR_LITERAL  */
#line 531 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_CHAR, &(yyval.literal)));
        (yyval.literal)->value.chr = (yyvsp[0].chr);
      }
#line 2639 "parser.c"
    break;

  case 54: /* literal: boolean_literal  */
#line 538 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_BOOL, &(yyval.literal)));
        (yyval.literal)->value.bln = (yyvsp[0].bln);
      }
#line 2650 "parser.c"
    break;

  case 55: /* literal: string_literal  */
#line 545 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_STRING, &(yyval.literal)));
        (yyval.literal)->value.str = (yyvsp[0].string_literal);
      }
#line 2661 "parser.c"
    break;

  case 56: /* boolean_literal: "TRUE"  */
#line 556 "src/parser.y"
      { (yyval.bln) = true; }
#line 2667 "parser.c"
    break;

  case 57: /* boolean_literal: "FALSE"  */
#line 558 "src/parser.y"
      { (yyval.bln) = false; }
#line 2673 "parser.c"
    break;

  case 58: /* string_literal: IDL_TOKEN_STRING_LITERAL  */
#line 563 "src/parser.y"
      { (yyval.string_literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if (!((yyval.string_literal) = idl_strdup((yyvsp[0].str))))
          NO_MEMORY();
      }
#line 2684 "parser.c"
    break;

  case 59: /* string_literal: string_literal IDL_TOKEN_STRING_LITERAL  */
#line 570 "src/parser.y"
      { size_t n1, n2;
        (yyval.string_literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        /* adjacent string literals are concatenated */
        n1 = strlen((yyvsp[-1].string_literal));
        n2 = strlen((yyvsp[0].str));
        if (!((yyval.string_literal) = idl_realloc((yyvsp[-1].string_literal), n1+n2+1)))
          NO_MEMORY();
        memmove((yyval.string_literal)+n1, (yyvsp[0].str), n2);
        (yyval.string_literal)[n1+n2] = '\0';
      }
#line 2701 "parser.c"
    break;

  case 60: /* positive_int_const: const_expr  */
#line 586 "src/parser.y"
      { TRY(idl_evaluate(pstate, (yyvsp[0].const_expr), IDL_ULONG, &(yyval.literal))); }
#line 2707 "parser.c"
    break;

  case 61: /* type_dcl: constr_type_dcl  */
#line 590 "src/parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2713 "parser.c"
    break;

  case 62: /* type_dcl: typedef_dcl  */
#line 591 "src/parser.y"
                { (yyval.node) = (yyvsp[0].typedef_dcl); }
#line 2719 "parser.c"
    break;

  case 65: /* simple_type_spec: base_type_spec  */
#line 602 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2725 "parser.c"
    break;

  case 66: /* simple_type_spec: scoped_name  */
#line 604 "src/parser.y"
      { const idl_declaration_t *declaration = NULL;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a type";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        if (!declaration || !idl_is_type_spec(declaration->node))
          SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2739 "parser.c"
    break;

  case 73: /* floating_pt_type: "float"  */
#line 625 "src/parser.y"
            { (yyval.kind) = IDL_FLOAT; }
#line 2745 "parser.c"
    break;

  case 74: /* floating_pt_type: "double"  */
#line 626 "src/parser.y"
             { (yyval.kind) = IDL_DOUBLE; }
#line 2751 "parser.c"
    break;

  case 75: /* floating_pt_type: "long" "double"  */
#line 627 "src/parser.y"
                    { (yyval.kind) = IDL_LDOUBLE; }
#line 2757 "parser.c"
    break;

  case 78: /* signed_int: "short"  */
#line 636 "src/parser.y"
            { (yyval.kind) = IDL_SHORT; }
#line 2763 "parser.c"
    break;

  case 79: /* signed_int: "long"  */
#line 637 "src/parser.y"
           { (yyval.kind) = IDL_LONG; }
#line 2769 "parser.c"
    break;

  case 80: /* signed_int: "long" "long"  */
#line 638 "src/parser.y"
                  { (yyval.kind) = IDL_LLONG; }
#line 2775 "parser.c"
    break;

  case 81: /* signed_int: "int8"  */
#line 640 "src/parser.y"
           { (yyval.kind) = IDL_INT8; }
#line 2781 "parser.c"
    break;

  case 82: /* signed_int: "int16"  */
#line 641 "src/parser.y"
            { (yyval.kind) = IDL_INT16; }
#line 2787 "parser.c"
    break;

  case 83: /* signed_int: "int32"  */
#line 642 "src/parser.y"
            { (yyval.kind) = IDL_INT32; }
#line 2793 "parser.c"
    break;

  case 84: /* signed_int: "int64"  */
#line 643 "src/parser.y"
            { (yyval.kind) = IDL_INT64; }
#line 2799 "parser.c"
    break;

  case 85: /* unsigned_int: "unsigned" "short"  */
#line 647 "src/parser.y"
                       { (yyval.kind) = IDL_USHORT; }
#line 2805 "parser.c"
    break;

  case 86: /* unsigned_int: "unsigned" "long"  */
#line 648 "src/parser.y"
                      { (yyval.kind) = IDL_ULONG; }
#line 2811 "parser.c"
    break;

  case 87: /* unsigned_int: "unsigned" "long" "long"  */
#line 649 "src/parser.y"
                             { (yyval.kind) = IDL_ULLONG; }
#line 2817 "parser.c"
    break;

  case 88: /* unsigned_int: "uint8"  */
#line 651 "src/parser.y"
            { (yyval.kind) = IDL_UINT8; }
#line 2823 "parser.c"
    break;

  case 89: /* unsigned_int: "uint16"  */
#line 652 "src/parser.y"
             { (yyval.kind) = IDL_UINT16; }
#line 2829 "parser.c"
    break;

  case 90: /* unsigned_int: "uint32"  */
#line 653 "src/parser.y"
             { (yyval.kind) = IDL_UINT32; }
#line 2835 "parser.c"
    break;

  case 91: /* unsigned_int: "uint64"  */
#line 654 "src/parser.y"
             { (yyval.kind) = IDL_UINT64; }
#line 2841 "parser.c"
    break;

  case 92: /* char_type: "char"  */
#line 658 "src/parser.y"
           { (yyval.kind) = IDL_CHAR; }
#line 2847 "parser.c"
    break;

  case 93: /* wide_char_type: "wchar"  */
#line 661 "src/parser.y"
            { (yyval.kind) = IDL_WCHAR; }
#line 2853 "parser.c"
    break;

  case 94: /* boolean_type: "boolean"  */
#line 664 "src/parser.y"
              { (yyval.kind) = IDL_BOOL; }
#line 2859 "parser.c"
    break;

  case 95: /* octet_type: "octet"  */
#line 667 "src/parser.y"
            { (yyval.kind) = IDL_OCTET; }
#line 2865 "parser.c"
    break;

  case 96: /* template_type_spec: sequence_type  */
#line 670 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].sequence); }
#line 2871 "parser.c"
    break;

  case 97: /* template_type_spec: string_type  */
#line 671 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].string); }
#line 2877 "parser.c"
    break;

  case 98: /* template_type_spec: wstring_type  */
#line 672 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].wstring); }
#line 2883 "parser.c"
    break;

  case 99: /* sequence_type: "sequence" '<' type_spec ',' positive_int_const '>'  */
#line 677 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-5]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-1].literal), &(yyval.sequence))); }
#line 2889 "parser.c"
    break;

  case 100: /* sequence_type: "sequence" '<' type_spec '>'  */
#line 679 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), NULL, &(yyval.sequence))); }
#line 2895 "parser.c"
    break;

  case 101: /* string_type: "string" '<' positive_int_const '>'  */
#line 684 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].literal), &(yyval.string))); }
#line 2901 "parser.c"
    break;

  case 102: /* string_type: "string"  */
#line 686 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[0]).first, (yylsp[0]).last), NULL, &(yyval.string))); }
#line 2907 "parser.c"
    break;

  case 103: /* wstring_type: "wstring" '<' positive_int_const '>'  */
#line 691 "src/parser.y"
      { TRY(idl_create_wstring(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].literal), &(yyval.wstring))); }
#line 2913 "parser.c"
    break;

  case 104: /* wstring_type: "wstring"  */
#line 693 "src/parser.y"
      { TRY(idl_create_wstring(pstate, LOC((yylsp[0]).first, (yylsp[0]).last), NULL, &(yyval.wstring))); }
#line 2919 "parser.c"
    break;

  case 109: /* struct_dcl: struct_def  */
#line 704 "src/parser.y"
               { (yyval.node) = (yyvsp[0].struct_dcl); }
#line 2925 "parser.c"
    break;

  case 110: /* struct_dcl: struct_forward_dcl  */
#line 705 "src/parser.y"
                       { (yyval.node) = (yyvsp[0].forward); }
#line 2931 "parser.c"
    break;

  case 111: /* struct_forward_dcl: "struct" identifier  */
#line 710 "src/parser.y"
      { TRY(idl_create_forward(pstate, &(yylsp[-1]), (yyvsp[0].name), IDL_STRUCT, &(yyval.forward))); }
#line 2937 "parser.c"
    break;

  case 112: /* struct_def: struct_header '{' struct_body '}'  */
#line 715 "src/parser.y"
      { TRY(idl_finalize_struct(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].struct_dcl), (yyvsp[-1].member)));
        (yyval.struct_dcl) = (yyvsp[-3].struct_dcl);
      }
#line 2945 "parser.c"
    break;

  case 113: /* struct_header: "struct" identifier struct_inherit_spec  */
#line 722 "src/parser.y"
      { TRY(idl_create_struct(pstate, LOC((yylsp[-2]).first, (yyvsp[0].type_spec) ? (yylsp[0]).last : (yylsp[-1]).last), (yyvsp[-1].name), (yyvsp[0].type_spec), &(yyval.struct_dcl))); }
#line 2951 "parser.c"
    break;

  case 114: /* struct_inherit_spec: %empty  */
#line 726 "src/parser.y"
            { (yyval.type_spec) = NULL; }
#line 2957 "parser.c"
    break;

  case 115: /* struct_inherit_spec: ':' scoped_name  */
#line 730 "src/parser.y"
      { idl_node_t *node;
        const idl_declaration_t *declaration;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a struct";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        node = idl_unalias(declaration->node);
        if (!idl_is_struct(node))
          SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        TRY(idl_create_inherit_spec(pstate, &(yylsp[0]), idl_reference_node(node), &(yyval.type_spec)));
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2973 "parser.c"
    break;

  case 116: /* struct_body: members  */
#line 745 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 2979 "parser.c"
    break;

  case 117: /* struct_body: %empty  */
#line 749 "src/parser.y"
      { (yyval.member) = NULL; }
#line 2985 "parser.c"
    break;

  case 118: /* members: member  */
#line 754 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 2991 "parser.c"
    break;

  case 119: /* members: members member  */
#line 756 "src/parser.y"
      { (yyval.member) = idl_push_node((yyvsp[-1].member), (yyvsp[0].member)); }
#line 2997 "parser.c"
    break;

  case 120: /* member: annotations type_spec declarators ';'  */
#line 761 "src/parser.y"
      { TRY(idl_create_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), &(yyval.member)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.member), (yyvsp[-3].annotation_appl)), idl_free((yyval.member)));
      }
#line 3005 "parser.c"
    break;

  case 121: /* union_dcl: union_def  */
#line 767 "src/parser.y"
              { (yyval.node) = (yyvsp[0].union_dcl); }
#line 3011 "parser.c"
    break;

  case 122: /* union_dcl: union_forward_dcl  */
#line 768 "src/parser.y"
                      { (yyval.node) = (yyvsp[0].forward); }
#line 3017 "parser.c"
    break;

  case 123: /* union_def: union_header '{' switch_body '}'  */
#line 773 "src/parser.y"
      { TRY(idl_finalize_union(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].union_dcl), (yyvsp[-1]._case)));
        (yyval.union_dcl) = (yyvsp[-3].union_dcl);
      }
#line 3025 "parser.c"
    break;

  case 124: /* union_forward_dcl: "union" identifier  */
#line 780 "src/parser.y"
      { TRY(idl_create_forward(pstate, &(yylsp[-1]), (yyvsp[0].name), IDL_UNION, &(yyval.forward))); }
#line 3031 "parser.c"
    break;

  case 125: /* union_header: "union" identifier switch_header  */
#line 785 "src/parser.y"
      { TRY(idl_create_union(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].switch_type_spec), &(yyval.union_dcl))); }
#line 3037 "parser.c"
    break;

  case 126: /* switch_header: "switch" '(' annotations switch_type_spec ')'  */
#line 790 "src/parser.y"
      { /* switch_header action is a separate non-terminal, as opposed to a
           mid-rule action, to avoid freeing the type specifier twice (once
           through destruction of the type-spec and once through destruction
           of the switch-type-spec) if union creation fails */
        TRY(idl_create_switch_type_spec(pstate, &(yylsp[-1]), (yyvsp[-1].type_spec), &(yyval.switch_type_spec)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.switch_type_spec), (yyvsp[-2].annotation_appl)), idl_delete_node((yyval.switch_type_spec)));
      }
#line 3049 "parser.c"
    break;

  case 127: /* switch_type_spec: integer_type  */
#line 801 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3055 "parser.c"
    break;

  case 128: /* switch_type_spec: char_type  */
#line 803 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3061 "parser.c"
    break;

  case 129: /* switch_type_spec: boolean_type  */
#line 805 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3067 "parser.c"
    break;

  case 130: /* switch_type_spec: scoped_name  */
#line 807 "src/parser.y"
      { const idl_declaration_t *declaration;
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        idl_delete_scoped_name((yyvsp[0].scoped_name));
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
      }
#line 3077 "parser.c"
    break;

  case 131: /* switch_type_spec: wide_char_type  */
#line 813 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3083 "parser.c"
    break;

  case 132: /* switch_type_spec: octet_type  */
#line 815 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3089 "parser.c"
    break;

  case 133: /* switch_body: case  */
#line 820 "src/parser.y"
      { (yyval._case) = (yyvsp[0]._case); }
#line 3095 "parser.c"
    break;

  case 134: /* switch_body: switch_body case  */
#line 822 "src/parser.y"
      { (yyval._case) = idl_push_node((yyvsp[-1]._case), (yyvsp[0]._case)); }
#line 3101 "parser.c"
    break;

  case 135: /* case: case_labels element_spec ';'  */
#line 827 "src/parser.y"
      { TRY(idl_finalize_case(pstate, &(yylsp[-1]), (yyvsp[-1]._case), (yyvsp[-2].case_label)));
        (yyval._case) = (yyvsp[-1]._case);
      }
#line 3109 "parser.c"
    break;

  case 136: /* case_labels: case_label  */
#line 834 "src/parser.y"
      { (yyval.case_label) = (yyvsp[0].case_label); }
#line 3115 "parser.c"
    break;

  case 137: /* case_labels: case_labels case_label  */
#line 836 "src/parser.y"
      { (yyval.case_label) = idl_push_node((yyvsp[-1].case_label), (yyvsp[0].case_label)); }
#line 3121 "parser.c"
    break;

  case 138: /* case_label: "case" const_expr ':'  */
#line 841 "src/parser.y"
      { TRY(idl_create_case_label(pstate, LOC((yylsp[-2]).first, (yylsp[-1]).last), (yyvsp[-1].const_expr), &(yyval.case_label))); }
#line 3127 "parser.c"
    break;

  case 139: /* case_label: "default" ':'  */
#line 843 "src/parser.y"
      { TRY(idl_create_case_label(pstate, &(yylsp[-1]), NULL, &(yyval.case_label))); }
#line 3133 "parser.c"
    break;

  case 140: /* element_spec: annotations type_spec declarator  */
#line 850 "src/parser.y"
      { TRY(idl_create_case(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval._case)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval._case), (yyvsp[-2].annotation_appl)), idl_free((yyval._case)));
      }
#line 3141 "parser.c"
    break;

  case 141: /* enum_dcl: enum_def  */
#line 855 "src/parser.y"
                   { (yyval.node) = (yyvsp[0].enum_dcl); }
#line 3147 "parser.c"
    break;

  case 142: /* enum_def: "enum" identifier '{' enumerators '}'  */
#line 859 "src/parser.y"
      { TRY(idl_create_enum(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].enumerator), &(yyval.enum_dcl))); }
#line 3153 "parser.c"
    break;

  case 143: /* enumerators: enumerator  */
#line 864 "src/parser.y"
      { (yyval.enumerator) = (yyvsp[0].enumerator); }
#line 3159 "parser.c"
    break;

  case 144: /* enumerators: enumerators ',' enumerator  */
#line 866 "src/parser.y"
      { (yyval.enumerator) = idl_push_node((yyvsp[-2].enumerator), (yyvsp[0].enumerator)); }
#line 3165 "parser.c"
    break;

  case 145: /* enumerator: annotations identifier  */
#line 871 "src/parser.y"
      { TRY(idl_create_enumerator(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.enumerator)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.enumerator), (yyvsp[-1].annotation_appl)), idl_free((yyval.enumerator)));
      }
#line 3173 "parser.c"
    break;

  case 146: /* bitmask_dcl: bitmask_def  */
#line 876 "src/parser.y"
                         { (yyval.node) = (yyvsp[0].bitmask_dcl); }
#line 3179 "parser.c"
    break;

  case 147: /* bitmask_def: "bitmask" identifier '{' bit_values '}'  */
#line 880 "src/parser.y"
      { TRY(idl_create_bitmask(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].bit_value), &(yyval.bitmask_dcl))); }
#line 3185 "parser.c"
    break;

  case 148: /* bit_values: bit_value  */
#line 885 "src/parser.y"
      { (yyval.bit_value) = (yyvsp[0].bit_value); }
#line 3191 "parser.c"
    break;

  case 149: /* bit_values: bit_values ',' bit_value  */
#line 887 "src/parser.y"
      { (yyval.bit_value) = idl_push_node((yyvsp[-2].bit_value), (yyvsp[0].bit_value)); }
#line 3197 "parser.c"
    break;

  case 150: /* bit_value: annotations identifier  */
#line 892 "src/parser.y"
      { TRY(idl_create_bit_value(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.bit_value)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.bit_value), (yyvsp[-1].annotation_appl)), idl_free((yyval.bit_value)));
      }
#line 3205 "parser.c"
    break;

  case 151: /* array_declarator: identifier fixed_array_sizes  */
#line 899 "src/parser.y"
      { TRY(idl_create_declarator(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].literal), &(yyval.declarator))); }
#line 3211 "parser.c"
    break;

  case 152: /* fixed_array_sizes: fixed_array_size  */
#line 904 "src/parser.y"
      { (yyval.literal) = (yyvsp[0].literal); }
#line 3217 "parser.c"
    break;

  case 153: /* fixed_array_sizes: fixed_array_sizes fixed_array_size  */
#line 906 "src/parser.y"
      { (yyval.literal) = idl_push_node((yyvsp[-1].literal), (yyvsp[0].literal)); }
#line 3223 "parser.c"
    break;

  case 154: /* fixed_array_size: '[' positive_int_const ']'  */
#line 911 "src/parser.y"
      { (yyval.literal) = (yyvsp[-1].literal); }
#line 3229 "parser.c"
    break;

  case 155: /* simple_declarator: identifier  */
#line 916 "src/parser.y"
      { TRY(idl_create_declarator(pstate, &(yylsp[0]), (yyvsp[0].name), NULL, &(yyval.declarator))); }
#line 3235 "parser.c"
    break;

  case 157: /* typedef_dcl: "typedef" type_spec declarators  */
#line 923 "src/parser.y"
      { TRY(idl_create_typedef(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval.typedef_dcl))); }
#line 3241 "parser.c"
    break;

  case 158: /* typedef_dcl: "typedef" constr_type_dcl declarators  */
#line 925 "src/parser.y"
      {
        idl_typedef_t *node;
        idl_type_spec_t *type_spec;
        assert((yyvsp[-1].node));
        /* treat forward declaration as no-op if definition is available */
        if ((idl_mask((yyvsp[-1].node)) & IDL_FORWARD) && ((idl_forward_t *)(yyvsp[-1].node))->type_spec)
          type_spec = ((idl_forward_t *)(yyvsp[-1].node))->type_spec;
        else
          type_spec = (yyvsp[-1].node);
        TRY(idl_create_typedef(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), type_spec, (yyvsp[0].declarator), &node));
        idl_reference_node(type_spec);
        (yyval.typedef_dcl) = idl_push_node((yyvsp[-1].node), node);
      }
#line 3259 "parser.c"
    break;

  case 159: /* declarators: declarator  */
#line 942 "src/parser.y"
      { (yyval.declarator) = (yyvsp[0].declarator); }
#line 3265 "parser.c"
    break;

  case 160: /* declarators: declarators ',' declarator  */
#line 944 "src/parser.y"
      { (yyval.declarator) = idl_push_node((yyvsp[-2].declarator), (yyvsp[0].declarator)); }
#line 3271 "parser.c"
    break;

  case 163: /* identifier: IDL_TOKEN_IDENTIFIER  */
#line 954 "src/parser.y"
      { (yyval.name) = NULL;
        size_t n;
        bool nocase = (pstate->config.flags & IDL_FLAG_CASE_SENSITIVE) == 0;
        if (pstate->parser.state == IDL_PARSE_ANNOTATION_APPL)
          n = 0;
        else if (pstate->parser.state == IDL_PARSE_ANNOTATION)
          n = 0;
        else if (!(n = ((yyvsp[0].str)[0] == '_')) && idl_iskeyword(pstate, (yyvsp[0].str), nocase))
          SEMANTIC_ERROR(&(yylsp[0]), "Identifier '%s' collides with a keyword", (yyvsp[0].str));

        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          bool is_annotation = pstate->parser.state == IDL_PARSE_ANNOTATION || pstate->parser.state == IDL_PARSE_ANNOTATION_APPL;
          TRY(idl_create_name(pstate, &(yylsp[0]), idl_strdup((yyvsp[0].str)+n), is_annotation, &(yyval.name)));
        }
      }
#line 3291 "parser.c"
    break;

  case 164: /* annotation_dcl: annotation_header '{' annotation_body '}'  */
#line 973 "src/parser.y"
      { (yyval.annotation) = NULL;
        /* discard annotation in case of redefinition */
        if (pstate->parser.state != IDL_PARSE_EXISTING_ANNOTATION_BODY)
          (yyval.annotation) = (yyvsp[-3].annotation);
        TRY(idl_finalize_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].annotation), (yyvsp[-1].annotation_member)));
      }
#line 3302 "parser.c"
    break;

  case 165: /* $@1: %empty  */
#line 983 "src/parser.y"
      { pstate->annotations = true; /* register annotation occurence */
        pstate->parser.state = IDL_PARSE_ANNOTATION;
      }
#line 3310 "parser.c"
    break;

  case 166: /* annotation_header: "@" "annotation" $@1 identifier  */
#line 987 "src/parser.y"
      { TRY(idl_create_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[-2]).last), (yyvsp[0].name), &(yyval.annotation))); }
#line 3316 "parser.c"
    break;

  case 167: /* annotation_body: %empty  */
#line 992 "src/parser.y"
      { (yyval.annotation_member) = NULL; }
#line 3322 "parser.c"
    break;

  case 168: /* annotation_body: annotation_body annotation_member ';'  */
#line 994 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].annotation_member)); }
#line 3328 "parser.c"
    break;

  case 169: /* annotation_body: annotation_body enum_dcl ';'  */
#line 996 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3334 "parser.c"
    break;

  case 170: /* annotation_body: annotation_body bitmask_dcl ';'  */
#line 998 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3340 "parser.c"
    break;

  case 171: /* annotation_body: annotation_body const_dcl ';'  */
#line 1000 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].const_dcl)); }
#line 3346 "parser.c"
    break;

  case 172: /* annotation_body: annotation_body typedef_dcl ';'  */
#line 1002 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].typedef_dcl)); }
#line 3352 "parser.c"
    break;

  case 173: /* annotation_member: annotation_member_type simple_declarator annotation_member_default  */
#line 1007 "src/parser.y"
      { TRY(idl_create_annotation_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), (yyvsp[0].const_expr), &(yyval.annotation_member))); }
#line 3358 "parser.c"
    break;

  case 174: /* annotation_member_type: const_type  */
#line 1012 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3364 "parser.c"
    break;

  case 175: /* annotation_member_type: any_const_type  */
#line 1014 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3370 "parser.c"
    break;

  case 176: /* annotation_member_default: %empty  */
#line 1019 "src/parser.y"
      { (yyval.const_expr) = NULL; }
#line 3376 "parser.c"
    break;

  case 177: /* annotation_member_default: "default" const_expr  */
#line 1021 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 3382 "parser.c"
    break;

  case 178: /* any_const_type: "any"  */
#line 1026 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), IDL_ANY, &(yyval.type_spec))); }
#line 3388 "parser.c"
    break;

  case 179: /* annotations: annotation_appls  */
#line 1031 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3394 "parser.c"
    break;

  case 180: /* annotations: %empty  */
#line 1033 "src/parser.y"
      { (yyval.annotation_appl) = NULL; }
#line 3400 "parser.c"
    break;

  case 181: /* annotation_appls: annotation_appl  */
#line 1038 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3406 "parser.c"
    break;

  case 182: /* annotation_appls: annotation_appls annotation_appl  */
#line 1040 "src/parser.y"
      { (yyval.annotation_appl) = idl_push_node((yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl)); }
#line 3412 "parser.c"
    break;

  case 183: /* annotation_appl: annotation_appl_header annotation_appl_params  */
#line 1045 "src/parser.y"
      { if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_finalize_annotation_appl(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl_param)));
        pstate->parser.state = IDL_PARSE;
        pstate->annotation_scope = NULL;
        (yyval.annotation_appl) = (yyvsp[-1].annotation_appl);
      }
#line 3423 "parser.c"
    break;

  case 184: /* $@2: %empty  */
#line 1055 "src/parser.y"
      { pstate->parser.state = IDL_PARSE_ANNOTATION_APPL; }
#line 3429 "parser.c"
    break;

  case 185: /* annotation_appl_header: "@" $@2 annotation_appl_name  */
#line 1057 "src/parser.y"
      { const idl_annotation_t *annotation;
        const idl_declaration_t *declaration =
          idl_find_scoped_name(pstate, NULL, (yyvsp[0].scoped_name), IDL_FIND_ANNOTATION);

        pstate->annotations = true; /* register annotation occurence */

        (yyval.annotation_appl) = NULL;
        if (declaration) {
          annotation = idl_reference_node((idl_node_t *)declaration->node);
          TRY(idl_create_annotation_appl(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), annotation, &(yyval.annotation_appl)));
          pstate->parser.state = IDL_PARSE_ANNOTATION_APPL_PARAMS;
          pstate->annotation_scope = declaration->scope;
        } else {
          pstate->parser.state = IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS;
          if (strcmp((yylsp[-2]).first.file->name, "<builtin>") && strcmp((yylsp[0]).last.file->name, "<builtin>"))
            idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, LOC((yylsp[-2]).first, (yylsp[0]).last), "Unrecognized annotation: @%s", (yyvsp[0].scoped_name)->identifier);
        }

        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 3454 "parser.c"
    break;

  case 186: /* annotation_appl_name: identifier  */
#line 1081 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 3460 "parser.c"
    break;

  case 187: /* annotation_appl_name: IDL_TOKEN_SCOPE_NO_SPACE identifier  */
#line 1083 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 3466 "parser.c"
    break;

  case 188: /* annotation_appl_name: annotation_appl_name IDL_TOKEN_SCOPE_NO_SPACE identifier  */
#line 1085 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 3474 "parser.c"
    break;

  case 189: /* annotation_appl_params: %empty  */
#line 1092 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL; }
#line 3480 "parser.c"
    break;

  case 190: /* annotation_appl_params: '(' const_expr ')'  */
#line 1094 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].const_expr); }
#line 3486 "parser.c"
    break;

  case 191: /* annotation_appl_params: '(' annotation_appl_keyword_params ')'  */
#line 1096 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].annotation_appl_param); }
#line 3492 "parser.c"
    break;

  case 192: /* annotation_appl_keyword_params: annotation_appl_keyword_param  */
#line 1101 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[0].annotation_appl_param); }
#line 3498 "parser.c"
    break;

  case 193: /* annotation_appl_keyword_params: annotation_appl_keyword_params ',' annotation_appl_keyword_param  */
#line 1103 "src/parser.y"
      { (yyval.annotation_appl_param) = idl_push_node((yyvsp[-2].annotation_appl_param), (yyvsp[0].annotation_appl_param)); }
#line 3504 "parser.c"
    break;

  case 194: /* @3: %empty  */
#line 1108 "src/parser.y"
      { idl_annotation_member_t *node = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          const idl_declaration_t *declaration = NULL;
          static const char fmt[] =
            "Unknown annotation member '%s'";
          declaration = idl_find(pstate, pstate->annotation_scope, (yyvsp[0].name), 0u);
          if (declaration && (idl_mask(declaration->node) & IDL_DECLARATOR))
            node = (idl_annotation_member_t *)((const idl_node_t *)declaration->node)->parent;
          if (!node || !(idl_mask(node) & IDL_ANNOTATION_MEMBER))
            SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].name)->identifier);
          node = idl_reference_node((idl_node_t *)node);
        }
        (yyval.annotation_member) = node;
        idl_delete_name((yyvsp[0].name));
      }
#line 3524 "parser.c"
    break;

  case 195: /* annotation_appl_keyword_param: identifier @3 '=' const_expr  */
#line 1124 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          TRY(idl_create_annotation_appl_param(pstate, &(yylsp[-3]), (yyvsp[-2].annotation_member), (yyvsp[0].const_expr), &(yyval.annotation_appl_param)));
        }
      }
#line 3534 "parser.c"
    break;


#line 3538 "parser.c"

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
      yyerror (&yylloc, pstate, result, YY_("syntax error"));
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
                      yytoken, &yylval, &yylloc, pstate, result);
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
  ++yynerrs;

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
                  YY_ACCESSING_SYMBOL (yystate), yyvsp, yylsp, pstate, result);
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
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, pstate, result, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != IDL_YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, &yylloc, pstate, result);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp, yylsp, pstate, result);
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
#line 1131 "src/parser.y"


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
/* generated from parser.y[3f81580cb244497effa9931afbac0c4d770b40ba] */
