/* A Bison parser, made by GNU Bison 3.7.  */

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
#define YYBISON_VERSION "3.7"

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
_Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")
#if (__GNUC__ >= 10)
_Pragma("GCC diagnostic ignored \"-Wanalyzer-free-of-non-heap\"")
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

#line 155 "parser.c"

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
  YYSYMBOL_struct_forward_dcl = 119,       /* struct_forward_dcl  */
  YYSYMBOL_struct_def = 120,               /* struct_def  */
  YYSYMBOL_struct_header = 121,            /* struct_header  */
  YYSYMBOL_struct_inherit_spec = 122,      /* struct_inherit_spec  */
  YYSYMBOL_struct_body = 123,              /* struct_body  */
  YYSYMBOL_members = 124,                  /* members  */
  YYSYMBOL_member = 125,                   /* member  */
  YYSYMBOL_union_dcl = 126,                /* union_dcl  */
  YYSYMBOL_union_def = 127,                /* union_def  */
  YYSYMBOL_union_forward_dcl = 128,        /* union_forward_dcl  */
  YYSYMBOL_union_header = 129,             /* union_header  */
  YYSYMBOL_switch_header = 130,            /* switch_header  */
  YYSYMBOL_switch_type_spec = 131,         /* switch_type_spec  */
  YYSYMBOL_switch_body = 132,              /* switch_body  */
  YYSYMBOL_case = 133,                     /* case  */
  YYSYMBOL_case_labels = 134,              /* case_labels  */
  YYSYMBOL_case_label = 135,               /* case_label  */
  YYSYMBOL_element_spec = 136,             /* element_spec  */
  YYSYMBOL_enum_dcl = 137,                 /* enum_dcl  */
  YYSYMBOL_enum_def = 138,                 /* enum_def  */
  YYSYMBOL_enumerators = 139,              /* enumerators  */
  YYSYMBOL_enumerator = 140,               /* enumerator  */
  YYSYMBOL_bitmask_dcl = 141,              /* bitmask_dcl  */
  YYSYMBOL_bitmask_def = 142,              /* bitmask_def  */
  YYSYMBOL_bit_values = 143,               /* bit_values  */
  YYSYMBOL_bit_value = 144,                /* bit_value  */
  YYSYMBOL_array_declarator = 145,         /* array_declarator  */
  YYSYMBOL_fixed_array_sizes = 146,        /* fixed_array_sizes  */
  YYSYMBOL_fixed_array_size = 147,         /* fixed_array_size  */
  YYSYMBOL_simple_declarator = 148,        /* simple_declarator  */
  YYSYMBOL_complex_declarator = 149,       /* complex_declarator  */
  YYSYMBOL_typedef_dcl = 150,              /* typedef_dcl  */
  YYSYMBOL_declarators = 151,              /* declarators  */
  YYSYMBOL_declarator = 152,               /* declarator  */
  YYSYMBOL_identifier = 153,               /* identifier  */
  YYSYMBOL_annotation_dcl = 154,           /* annotation_dcl  */
  YYSYMBOL_annotation_header = 155,        /* annotation_header  */
  YYSYMBOL_156_1 = 156,                    /* $@1  */
  YYSYMBOL_annotation_body = 157,          /* annotation_body  */
  YYSYMBOL_annotation_member = 158,        /* annotation_member  */
  YYSYMBOL_annotation_member_type = 159,   /* annotation_member_type  */
  YYSYMBOL_annotation_member_default = 160, /* annotation_member_default  */
  YYSYMBOL_any_const_type = 161,           /* any_const_type  */
  YYSYMBOL_annotations = 162,              /* annotations  */
  YYSYMBOL_annotation_appls = 163,         /* annotation_appls  */
  YYSYMBOL_annotation_appl = 164,          /* annotation_appl  */
  YYSYMBOL_annotation_appl_header = 165,   /* annotation_appl_header  */
  YYSYMBOL_166_2 = 166,                    /* $@2  */
  YYSYMBOL_annotation_appl_name = 167,     /* annotation_appl_name  */
  YYSYMBOL_annotation_appl_params = 168,   /* annotation_appl_params  */
  YYSYMBOL_annotation_appl_keyword_params = 169, /* annotation_appl_keyword_params  */
  YYSYMBOL_annotation_appl_keyword_param = 170, /* annotation_appl_keyword_param  */
  YYSYMBOL_171_3 = 171                     /* @3  */
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
#define YYFINAL  13
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   409

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  76
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  96
/* YYNRULES -- Number of rules.  */
#define YYNRULES  192
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  286

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
       0,   286,   286,   288,   293,   295,   300,   302,   306,   310,
     317,   324,   329,   331,   333,   340,   345,   347,   349,   351,
     353,   355,   357,   371,   374,   375,   383,   384,   392,   393,
     401,   402,   410,   411,   414,   415,   423,   424,   427,   429,
     437,   438,   439,   442,   447,   452,   453,   454,   458,   474,
     476,   481,   503,   526,   533,   540,   550,   552,   557,   564,
     580,   585,   586,   590,   592,   596,   598,   611,   612,   613,
     614,   615,   616,   620,   621,   622,   626,   627,   631,   632,
     633,   635,   636,   637,   638,   642,   643,   644,   646,   647,
     648,   649,   653,   656,   659,   662,   665,   666,   670,   672,
     677,   679,   684,   685,   686,   687,   691,   692,   696,   701,
     708,   713,   716,   731,   735,   740,   742,   747,   754,   755,
     759,   766,   771,   776,   787,   789,   791,   793,   799,   801,
     806,   808,   813,   820,   822,   827,   829,   836,   842,   845,
     850,   852,   857,   863,   866,   871,   873,   878,   885,   890,
     892,   897,   902,   906,   909,   911,   928,   930,   935,   936,
     940,   957,   968,   967,   976,   978,   980,   982,   984,   986,
     991,   996,   998,  1003,  1005,  1010,  1015,  1017,  1022,  1024,
    1029,  1040,  1039,  1065,  1067,  1069,  1076,  1078,  1080,  1085,
    1087,  1093,  1092
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
  "string_type", "constr_type_dcl", "struct_dcl", "struct_forward_dcl",
  "struct_def", "struct_header", "struct_inherit_spec", "struct_body",
  "members", "member", "union_dcl", "union_def", "union_forward_dcl",
  "union_header", "switch_header", "switch_type_spec", "switch_body",
  "case", "case_labels", "case_label", "element_spec", "enum_dcl",
  "enum_def", "enumerators", "enumerator", "bitmask_dcl", "bitmask_def",
  "bit_values", "bit_value", "array_declarator", "fixed_array_sizes",
  "fixed_array_size", "simple_declarator", "complex_declarator",
  "typedef_dcl", "declarators", "declarator", "identifier",
  "annotation_dcl", "annotation_header", "$@1", "annotation_body",
  "annotation_member", "annotation_member_type",
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

#define YYPACT_NINF (-201)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-192)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      59,    65,    83,    82,  -201,    42,    45,    84,    96,  -201,
      41,  -201,    62,  -201,  -201,  -201,  -201,   116,   330,   116,
     240,   116,   116,   116,    68,    71,    69,    73,  -201,  -201,
    -201,  -201,    74,  -201,  -201,  -201,    75,  -201,  -201,  -201,
    -201,  -201,  -201,  -201,    43,  -201,   116,  -201,   116,  -201,
     115,   148,  -201,   116,    80,    66,  -201,  -201,  -201,    31,
    -201,  -201,  -201,  -201,  -201,  -201,  -201,  -201,  -201,  -201,
    -201,   122,   116,  -201,  -201,  -201,  -201,  -201,  -201,  -201,
    -201,  -201,   -33,    70,  -201,   122,   116,  -201,  -201,  -201,
    -201,  -201,  -201,  -201,  -201,  -201,  -201,  -201,   116,   117,
      85,    87,  -201,   128,  -201,  -201,     6,    92,  -201,  -201,
    -201,  -201,  -201,  -201,  -201,  -201,  -201,    43,   122,    76,
      88,    86,    90,    63,    56,   -22,  -201,    20,  -201,  -201,
    -201,   136,    91,    27,  -201,  -201,  -201,   116,  -201,  -201,
      93,  -201,    98,   100,   101,   102,   116,  -201,  -201,  -201,
     125,    43,  -201,  -201,   116,   104,    79,  -201,   285,  -201,
    -201,  -201,    89,  -201,    95,    89,    97,  -201,    96,    96,
       8,   106,     9,  -201,   285,    43,   110,    16,  -201,    67,
    -201,   108,  -201,    43,    43,    43,  -201,  -201,    43,  -201,
    -201,    43,  -201,  -201,  -201,    43,  -201,  -201,   129,  -201,
     116,  -201,  -201,  -201,  -201,  -201,  -201,   143,  -201,  -201,
    -201,   127,  -201,    43,   122,    49,   116,    43,    95,  -201,
      96,    -2,  -201,   116,     4,  -201,   116,  -201,  -201,  -201,
     116,   130,  -201,  -201,  -201,  -201,   134,   285,  -201,    86,
      90,    63,    56,   -22,  -201,    43,  -201,  -201,    43,  -201,
    -201,  -201,    43,  -201,  -201,   126,  -201,   359,  -201,    96,
    -201,  -201,    96,  -201,   -24,  -201,  -201,   116,  -201,  -201,
     137,  -201,   167,   122,  -201,  -201,  -201,  -201,  -201,   133,
    -201,  -201,  -201,  -201,  -201,  -201
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
     177,   181,     0,   177,     4,     0,     0,     0,   176,   178,
     186,   162,     0,     1,     5,     6,   164,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,    61,   102,
     107,   106,     0,   103,   118,   119,     0,   104,   138,   105,
     143,    62,   181,   179,     0,   180,     0,   160,     0,   183,
     182,     0,    11,     0,     0,   101,    73,    74,    78,    79,
      92,    94,    95,    81,    82,    83,    84,    88,    89,    90,
      91,    22,     0,    17,    16,    76,    77,    18,    19,    20,
      21,    12,   108,     0,    93,    66,     0,    63,    65,    68,
      67,    69,    70,    71,    72,    64,    96,    97,     0,   121,
       0,     0,     7,   177,     8,     9,   177,     0,    53,    58,
      51,    52,    56,    57,    46,    45,    47,     0,    48,     0,
      23,    24,    26,    28,    30,    34,    38,     0,    44,    49,
      54,    55,    12,     0,   189,   163,   184,     0,   175,   161,
       0,   171,     0,     0,     0,     0,     0,   172,    13,    85,
      86,     0,    75,    80,     0,     0,     0,   110,     0,   153,
     158,   159,   154,   156,   152,   155,     0,   122,   177,   177,
     177,     0,   177,   115,     0,     0,     0,     0,   130,   177,
     133,     0,   187,     0,     0,     0,    33,    32,     0,    36,
      37,     0,    40,    41,    42,     0,    43,    59,     0,   188,
       0,   185,   168,   166,   167,   169,   165,   173,   152,    87,
      60,     0,    14,     0,   112,     0,     0,     0,   148,   149,
     177,     0,   140,     0,     0,   145,     0,    10,   109,   116,
       0,     0,   136,   120,   131,   134,     0,     0,    50,    25,
      27,    29,    31,    35,    39,     0,   191,   190,     0,   170,
     100,    15,     0,    99,   157,     0,   150,     0,   139,   177,
     142,   144,   177,   147,     0,   135,   132,     0,   192,   174,
       0,   151,    79,   127,   124,   125,   128,   126,   129,     0,
     141,   146,   117,   137,    98,   123
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -201,  -201,   103,     0,  -201,  -201,    -6,   157,   161,   -38,
    -201,    30,   -25,    32,  -201,    28,  -201,    24,  -201,    25,
    -201,    94,  -201,  -201,  -201,  -195,  -201,  -150,  -201,  -201,
       3,   -16,  -201,  -201,   -14,   -35,    -5,    -3,  -201,  -201,
       7,   199,  -201,  -201,  -201,  -201,  -201,  -201,  -201,    51,
    -201,  -201,  -201,  -201,  -201,  -201,  -201,    47,  -201,    46,
    -201,   175,  -201,  -201,   -32,   177,  -201,  -201,   -30,  -201,
    -201,    11,    99,  -201,   179,   -97,  -200,   -12,  -201,  -201,
    -201,  -201,  -201,  -201,  -201,  -201,   -88,  -201,   225,  -201,
    -201,  -201,  -201,  -201,    34,  -201
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,    24,    25,   118,    26,    72,   210,
     120,   121,   122,   123,   188,   124,   191,   125,   195,   126,
     127,   128,   129,   130,   131,   211,    27,    86,    87,    88,
      89,    90,    75,    76,    91,    92,    93,    94,    95,    96,
      97,    28,    29,    30,    31,    32,   157,   171,   172,   173,
      33,    34,    35,    36,   167,   279,   177,   178,   179,   180,
     236,    37,    38,   221,   222,    39,    40,   224,   225,   159,
     218,   219,   160,   161,    41,   162,   163,    81,     5,     6,
      46,    51,   145,   146,   249,   147,     7,     8,     9,    10,
      12,    50,    45,   133,   134,   198
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      49,   165,    74,    14,    77,    52,   119,    82,   215,    99,
     100,   101,    71,    78,    85,    79,   254,    42,   174,     1,
      42,    73,   255,  -111,   230,    80,    47,   108,   109,   110,
     111,   282,   132,    53,   135,    74,   136,    77,   175,   176,
     156,   148,   192,   193,   194,    71,    78,   216,    79,    47,
     108,   109,   110,   111,    73,   258,    53,   270,    80,    -2,
     155,   261,   152,  -114,   153,   227,  -113,   283,    47,   259,
       1,   112,   113,   233,   164,   262,    48,    11,    42,   181,
     223,   226,    -3,    13,   174,    47,   164,   267,   117,   175,
     176,   237,    53,     1,   112,   113,   199,    15,   200,    17,
      18,    16,    19,    20,    21,   114,   115,    42,    22,    44,
     116,   117,   149,   150,   175,   176,   186,   187,   189,   190,
     252,   253,    47,   102,   104,   201,    23,   103,   105,   137,
     106,   107,   257,   264,   208,   154,   151,   231,   166,     1,
     158,   168,   212,   169,   197,   182,   184,   183,   202,  -191,
     214,   185,    85,   203,    47,   204,   205,   206,   209,   240,
     216,    53,   213,   228,    18,   220,   248,    20,    85,   217,
      14,   223,    22,    54,   226,   251,    55,   238,    56,    57,
      58,    59,    60,   232,    61,    62,   138,   245,   246,   266,
      23,    63,    64,    65,    66,    67,    68,    69,    70,   250,
     153,   271,   285,   265,   164,   139,   170,   268,   140,   284,
     269,   260,   141,   239,   263,   243,   242,   241,   164,    98,
     244,   196,   276,   229,   234,   235,   142,   280,   143,   256,
     144,    85,   281,    43,   247,     0,     0,     0,     0,     0,
       0,   274,     0,   275,     0,   207,    47,     0,     0,     0,
       0,   273,   277,    53,   278,   164,     0,     0,    19,     0,
      21,     0,     0,     0,    22,    54,     0,    83,    55,     0,
      56,    57,    58,    59,    60,    84,    61,    62,     0,     0,
       0,     0,    23,    63,    64,    65,    66,    67,    68,    69,
      70,    47,     0,     0,     0,     0,     0,     0,    53,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
      54,     0,    83,    55,     0,    56,    57,    58,    59,    60,
      84,    61,    62,     0,     0,     0,     0,     0,    63,    64,
      65,    66,    67,    68,    69,    70,    47,     0,     0,     0,
       0,     0,     0,    53,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    54,     0,     0,    55,     0,
      56,    57,    58,    59,    60,    47,    61,    62,     0,     0,
       0,     0,    53,    63,    64,    65,    66,    67,    68,    69,
      70,     0,     0,     0,    54,     0,     0,     0,     0,     0,
       0,    58,   272,    60,    84,    61,    62,     0,     0,     0,
       0,     0,    63,    64,    65,    66,    67,    68,    69,    70
};

static const yytype_int16 yycheck[] =
{
      12,    98,    18,     3,    18,    17,    44,    19,   158,    21,
      22,    23,    18,    18,    20,    18,   216,    11,   106,    11,
      11,    18,   217,    56,   174,    18,     6,     7,     8,     9,
      10,    55,    44,    13,    46,    51,    48,    51,    22,    23,
      73,    53,    64,    65,    66,    51,    51,    71,    51,     6,
       7,     8,     9,    10,    51,    57,    13,   252,    51,     0,
      72,    57,    31,    57,    33,    57,    57,   267,     6,    71,
      11,    51,    52,    57,    86,    71,    14,    12,    11,   117,
     168,   169,     0,     0,   172,     6,    98,   237,    68,    22,
      23,   179,    13,    11,    51,    52,    69,    55,    71,    15,
      16,    56,    18,    19,    20,    62,    63,    11,    24,    68,
      67,    68,    32,    33,    22,    23,    53,    54,    62,    63,
      71,    72,     6,    55,    55,   137,    42,    56,    55,    14,
      56,    56,   220,   230,   146,    13,    70,   175,    21,    11,
      70,    56,   154,    56,     8,    69,    60,    59,    55,    58,
     156,    61,   158,    55,     6,    55,    55,    55,    33,   184,
      71,    13,    58,    57,    16,    68,    23,    19,   174,    74,
     170,   259,    24,    25,   262,   213,    28,    69,    30,    31,
      32,    33,    34,    73,    36,    37,    38,    58,   200,    55,
      42,    43,    44,    45,    46,    47,    48,    49,    50,    72,
      33,    75,    69,    73,   216,    57,   103,   245,    51,    72,
     248,   223,    51,   183,   226,   191,   188,   185,   230,    20,
     195,   127,   257,   172,   177,   179,    51,   259,    51,   218,
      51,   237,   262,     8,   200,    -1,    -1,    -1,    -1,    -1,
      -1,   257,    -1,   257,    -1,   146,     6,    -1,    -1,    -1,
      -1,   257,   257,    13,   257,   267,    -1,    -1,    18,    -1,
      20,    -1,    -1,    -1,    24,    25,    -1,    27,    28,    -1,
      30,    31,    32,    33,    34,    35,    36,    37,    -1,    -1,
      -1,    -1,    42,    43,    44,    45,    46,    47,    48,    49,
      50,     6,    -1,    -1,    -1,    -1,    -1,    -1,    13,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      25,    -1,    27,    28,    -1,    30,    31,    32,    33,    34,
      35,    36,    37,    -1,    -1,    -1,    -1,    -1,    43,    44,
      45,    46,    47,    48,    49,    50,     6,    -1,    -1,    -1,
      -1,    -1,    -1,    13,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    25,    -1,    -1,    28,    -1,
      30,    31,    32,    33,    34,     6,    36,    37,    -1,    -1,
      -1,    -1,    13,    43,    44,    45,    46,    47,    48,    49,
      50,    -1,    -1,    -1,    25,    -1,    -1,    -1,    -1,    -1,
      -1,    32,    33,    34,    35,    36,    37,    -1,    -1,    -1,
      -1,    -1,    43,    44,    45,    46,    47,    48,    49,    50
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    11,    77,    78,    79,   154,   155,   162,   163,   164,
     165,    12,   166,     0,    79,    55,    56,    15,    16,    18,
      19,    20,    24,    42,    80,    81,    83,   102,   117,   118,
     119,   120,   121,   126,   127,   128,   129,   137,   138,   141,
     142,   150,    11,   164,    68,   168,   156,     6,    14,   153,
     167,   157,   153,    13,    25,    28,    30,    31,    32,    33,
      34,    36,    37,    43,    44,    45,    46,    47,    48,    49,
      50,    82,    84,   106,   107,   108,   109,   110,   112,   113,
     116,   153,   153,    27,    35,    82,   103,   104,   105,   106,
     107,   110,   111,   112,   113,   114,   115,   116,   117,   153,
     153,   153,    55,    56,    55,    55,    56,    56,     7,     8,
       9,    10,    51,    52,    62,    63,    67,    68,    82,    85,
      86,    87,    88,    89,    91,    93,    95,    96,    97,    98,
      99,   100,   153,   169,   170,   153,   153,    14,    38,    57,
      83,    84,   137,   141,   150,   158,   159,   161,   153,    32,
      33,    70,    31,    33,    13,   153,    73,   122,    70,   145,
     148,   149,   151,   152,   153,   151,    21,   130,    56,    56,
      78,   123,   124,   125,   162,    22,    23,   132,   133,   134,
     135,    85,    69,    59,    60,    61,    53,    54,    90,    62,
      63,    92,    64,    65,    66,    94,    97,     8,   171,    69,
      71,   153,    55,    55,    55,    55,    55,   148,   153,    33,
      85,   101,   153,    58,    82,   103,    71,    74,   146,   147,
      68,   139,   140,   162,   143,   144,   162,    57,    57,   125,
     103,    85,    73,    57,   133,   135,   136,   162,    69,    87,
      88,    89,    91,    93,    95,    58,   153,   170,    23,   160,
      72,    85,    71,    72,   152,   101,   147,   162,    57,    71,
     153,    57,    71,   153,   151,    73,    55,   103,    85,    85,
     101,    75,    33,    82,   107,   110,   111,   112,   113,   131,
     140,   144,    55,   152,    72,    69
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
     116,   116,   117,   117,   117,   117,   118,   118,   119,   120,
     121,   122,   122,   123,   123,   124,   124,   125,   126,   126,
     127,   128,   129,   130,   131,   131,   131,   131,   131,   131,
     132,   132,   133,   134,   134,   135,   135,   136,   137,   138,
     139,   139,   140,   141,   142,   143,   143,   144,   145,   146,
     146,   147,   148,   149,   150,   150,   151,   151,   152,   152,
     153,   154,   156,   155,   157,   157,   157,   157,   157,   157,
     158,   159,   159,   160,   160,   161,   162,   162,   163,   163,
     164,   166,   165,   167,   167,   167,   168,   168,   168,   169,
     169,   171,   170
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
       4,     1,     1,     1,     1,     1,     1,     1,     2,     4,
       3,     0,     2,     1,     0,     1,     2,     4,     1,     1,
       4,     2,     3,     5,     1,     1,     1,     1,     1,     1,
       1,     2,     3,     1,     2,     3,     2,     3,     1,     5,
       1,     3,     2,     1,     5,     1,     3,     2,     2,     1,
       2,     3,     1,     1,     3,     3,     1,     3,     1,     1,
       1,     4,     0,     4,     0,     3,     3,     3,     3,     3,
       3,     1,     1,     0,     2,     1,     1,     0,     1,     2,
       2,     0,     3,     1,     2,     3,     0,     3,     3,     1,
       3,     0,     4
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
  YYUSE (yyoutput);
  YYUSE (yylocationp);
  YYUSE (pstate);
  YYUSE (result);
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
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  YY_LOCATION_PRINT (yyo, *yylocationp);
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
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (pstate);
  YYUSE (result);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yykind)
    {
    case YYSYMBOL_definitions: /* definitions  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1412 "parser.c"
        break;

    case YYSYMBOL_definition: /* definition  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1418 "parser.c"
        break;

    case YYSYMBOL_module_dcl: /* module_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1424 "parser.c"
        break;

    case YYSYMBOL_module_header: /* module_header  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1430 "parser.c"
        break;

    case YYSYMBOL_scoped_name: /* scoped_name  */
#line 208 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1436 "parser.c"
        break;

    case YYSYMBOL_const_dcl: /* const_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).const_dcl)); }
#line 1442 "parser.c"
        break;

    case YYSYMBOL_const_type: /* const_type  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1448 "parser.c"
        break;

    case YYSYMBOL_const_expr: /* const_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1454 "parser.c"
        break;

    case YYSYMBOL_or_expr: /* or_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1460 "parser.c"
        break;

    case YYSYMBOL_xor_expr: /* xor_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1466 "parser.c"
        break;

    case YYSYMBOL_and_expr: /* and_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1472 "parser.c"
        break;

    case YYSYMBOL_shift_expr: /* shift_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1478 "parser.c"
        break;

    case YYSYMBOL_add_expr: /* add_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1484 "parser.c"
        break;

    case YYSYMBOL_mult_expr: /* mult_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1490 "parser.c"
        break;

    case YYSYMBOL_unary_expr: /* unary_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1496 "parser.c"
        break;

    case YYSYMBOL_primary_expr: /* primary_expr  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1502 "parser.c"
        break;

    case YYSYMBOL_literal: /* literal  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1508 "parser.c"
        break;

    case YYSYMBOL_string_literal: /* string_literal  */
#line 203 "src/parser.y"
            { free(((*yyvaluep).string_literal)); }
#line 1514 "parser.c"
        break;

    case YYSYMBOL_positive_int_const: /* positive_int_const  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1520 "parser.c"
        break;

    case YYSYMBOL_type_dcl: /* type_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1526 "parser.c"
        break;

    case YYSYMBOL_type_spec: /* type_spec  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1532 "parser.c"
        break;

    case YYSYMBOL_simple_type_spec: /* simple_type_spec  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1538 "parser.c"
        break;

    case YYSYMBOL_template_type_spec: /* template_type_spec  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1544 "parser.c"
        break;

    case YYSYMBOL_sequence_type: /* sequence_type  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).sequence)); }
#line 1550 "parser.c"
        break;

    case YYSYMBOL_string_type: /* string_type  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).string)); }
#line 1556 "parser.c"
        break;

    case YYSYMBOL_constr_type_dcl: /* constr_type_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1562 "parser.c"
        break;

    case YYSYMBOL_struct_dcl: /* struct_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1568 "parser.c"
        break;

    case YYSYMBOL_struct_forward_dcl: /* struct_forward_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).forward)); }
#line 1574 "parser.c"
        break;

    case YYSYMBOL_struct_def: /* struct_def  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1580 "parser.c"
        break;

    case YYSYMBOL_struct_header: /* struct_header  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1586 "parser.c"
        break;

    case YYSYMBOL_struct_inherit_spec: /* struct_inherit_spec  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1592 "parser.c"
        break;

    case YYSYMBOL_struct_body: /* struct_body  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1598 "parser.c"
        break;

    case YYSYMBOL_members: /* members  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1604 "parser.c"
        break;

    case YYSYMBOL_member: /* member  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1610 "parser.c"
        break;

    case YYSYMBOL_union_dcl: /* union_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1616 "parser.c"
        break;

    case YYSYMBOL_union_def: /* union_def  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1622 "parser.c"
        break;

    case YYSYMBOL_union_forward_dcl: /* union_forward_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).forward)); }
#line 1628 "parser.c"
        break;

    case YYSYMBOL_union_header: /* union_header  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1634 "parser.c"
        break;

    case YYSYMBOL_switch_header: /* switch_header  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).switch_type_spec)); }
#line 1640 "parser.c"
        break;

    case YYSYMBOL_switch_type_spec: /* switch_type_spec  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1646 "parser.c"
        break;

    case YYSYMBOL_switch_body: /* switch_body  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1652 "parser.c"
        break;

    case YYSYMBOL_case: /* case  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1658 "parser.c"
        break;

    case YYSYMBOL_case_labels: /* case_labels  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1664 "parser.c"
        break;

    case YYSYMBOL_case_label: /* case_label  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1670 "parser.c"
        break;

    case YYSYMBOL_element_spec: /* element_spec  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1676 "parser.c"
        break;

    case YYSYMBOL_enum_dcl: /* enum_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1682 "parser.c"
        break;

    case YYSYMBOL_enum_def: /* enum_def  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enum_dcl)); }
#line 1688 "parser.c"
        break;

    case YYSYMBOL_enumerators: /* enumerators  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1694 "parser.c"
        break;

    case YYSYMBOL_enumerator: /* enumerator  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1700 "parser.c"
        break;

    case YYSYMBOL_bitmask_dcl: /* bitmask_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1706 "parser.c"
        break;

    case YYSYMBOL_bitmask_def: /* bitmask_def  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bitmask_dcl)); }
#line 1712 "parser.c"
        break;

    case YYSYMBOL_bit_values: /* bit_values  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bit_value)); }
#line 1718 "parser.c"
        break;

    case YYSYMBOL_bit_value: /* bit_value  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bit_value)); }
#line 1724 "parser.c"
        break;

    case YYSYMBOL_array_declarator: /* array_declarator  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1730 "parser.c"
        break;

    case YYSYMBOL_fixed_array_sizes: /* fixed_array_sizes  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1736 "parser.c"
        break;

    case YYSYMBOL_fixed_array_size: /* fixed_array_size  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1742 "parser.c"
        break;

    case YYSYMBOL_simple_declarator: /* simple_declarator  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1748 "parser.c"
        break;

    case YYSYMBOL_complex_declarator: /* complex_declarator  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1754 "parser.c"
        break;

    case YYSYMBOL_typedef_dcl: /* typedef_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).typedef_dcl)); }
#line 1760 "parser.c"
        break;

    case YYSYMBOL_declarators: /* declarators  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1766 "parser.c"
        break;

    case YYSYMBOL_declarator: /* declarator  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1772 "parser.c"
        break;

    case YYSYMBOL_identifier: /* identifier  */
#line 205 "src/parser.y"
            { idl_delete_name(((*yyvaluep).name)); }
#line 1778 "parser.c"
        break;

    case YYSYMBOL_annotation_dcl: /* annotation_dcl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1784 "parser.c"
        break;

    case YYSYMBOL_annotation_header: /* annotation_header  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1790 "parser.c"
        break;

    case YYSYMBOL_annotation_body: /* annotation_body  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1796 "parser.c"
        break;

    case YYSYMBOL_annotation_member: /* annotation_member  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1802 "parser.c"
        break;

    case YYSYMBOL_annotation_member_type: /* annotation_member_type  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1808 "parser.c"
        break;

    case YYSYMBOL_annotation_member_default: /* annotation_member_default  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1814 "parser.c"
        break;

    case YYSYMBOL_any_const_type: /* any_const_type  */
#line 211 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1820 "parser.c"
        break;

    case YYSYMBOL_annotations: /* annotations  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1826 "parser.c"
        break;

    case YYSYMBOL_annotation_appls: /* annotation_appls  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1832 "parser.c"
        break;

    case YYSYMBOL_annotation_appl: /* annotation_appl  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1838 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_header: /* annotation_appl_header  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 1844 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_name: /* annotation_appl_name  */
#line 208 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1850 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_params: /* annotation_appl_params  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1856 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_keyword_params: /* annotation_appl_keyword_params  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1862 "parser.c"
        break;

    case YYSYMBOL_annotation_appl_keyword_param: /* annotation_appl_keyword_param  */
#line 214 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 1868 "parser.c"
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
  yyps = YY_CAST (yypstate *, malloc (sizeof *yyps));
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
      free (yyps);
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
#line 287 "src/parser.y"
      { pstate->root = NULL; }
#line 2230 "parser.c"
    break;

  case 3: /* specification: definitions  */
#line 289 "src/parser.y"
      { pstate->root = (yyvsp[0].node); }
#line 2236 "parser.c"
    break;

  case 4: /* definitions: definition  */
#line 294 "src/parser.y"
      { (yyval.node) = (yyvsp[0].node); }
#line 2242 "parser.c"
    break;

  case 5: /* definitions: definitions definition  */
#line 296 "src/parser.y"
      { (yyval.node) = idl_push_node((yyvsp[-1].node), (yyvsp[0].node)); }
#line 2248 "parser.c"
    break;

  case 6: /* definition: annotation_dcl ';'  */
#line 301 "src/parser.y"
      { (yyval.node) = (yyvsp[-1].annotation); }
#line 2254 "parser.c"
    break;

  case 7: /* definition: annotations module_dcl ';'  */
#line 303 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].module_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].module_dcl);
      }
#line 2262 "parser.c"
    break;

  case 8: /* definition: annotations const_dcl ';'  */
#line 307 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].const_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].const_dcl);
      }
#line 2270 "parser.c"
    break;

  case 9: /* definition: annotations type_dcl ';'  */
#line 311 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].node), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].node);
      }
#line 2278 "parser.c"
    break;

  case 10: /* module_dcl: module_header '{' definitions '}'  */
#line 318 "src/parser.y"
      { TRY(idl_finalize_module(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].module_dcl), (yyvsp[-1].node)));
        (yyval.module_dcl) = (yyvsp[-3].module_dcl);
      }
#line 2286 "parser.c"
    break;

  case 11: /* module_header: "module" identifier  */
#line 325 "src/parser.y"
      { TRY(idl_create_module(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), &(yyval.module_dcl))); }
#line 2292 "parser.c"
    break;

  case 12: /* scoped_name: identifier  */
#line 330 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 2298 "parser.c"
    break;

  case 13: /* scoped_name: IDL_TOKEN_SCOPE identifier  */
#line 332 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 2304 "parser.c"
    break;

  case 14: /* scoped_name: scoped_name IDL_TOKEN_SCOPE identifier  */
#line 334 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 2312 "parser.c"
    break;

  case 15: /* const_dcl: "const" const_type identifier '=' const_expr  */
#line 341 "src/parser.y"
      { TRY(idl_create_const(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-2].name), (yyvsp[0].const_expr), &(yyval.const_dcl))); }
#line 2318 "parser.c"
    break;

  case 16: /* const_type: integer_type  */
#line 346 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2324 "parser.c"
    break;

  case 17: /* const_type: floating_pt_type  */
#line 348 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2330 "parser.c"
    break;

  case 18: /* const_type: char_type  */
#line 350 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2336 "parser.c"
    break;

  case 19: /* const_type: boolean_type  */
#line 352 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2342 "parser.c"
    break;

  case 20: /* const_type: octet_type  */
#line 354 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2348 "parser.c"
    break;

  case 21: /* const_type: string_type  */
#line 356 "src/parser.y"
      { (yyval.type_spec) = (idl_type_spec_t *)(yyvsp[0].string); }
#line 2354 "parser.c"
    break;

  case 22: /* const_type: scoped_name  */
#line 358 "src/parser.y"
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
#line 2370 "parser.c"
    break;

  case 23: /* const_expr: or_expr  */
#line 371 "src/parser.y"
                    { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2376 "parser.c"
    break;

  case 25: /* or_expr: or_expr '|' xor_expr  */
#line 376 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_OR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2385 "parser.c"
    break;

  case 27: /* xor_expr: xor_expr '^' and_expr  */
#line 385 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_XOR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2394 "parser.c"
    break;

  case 29: /* and_expr: and_expr '&' shift_expr  */
#line 394 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_AND, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2403 "parser.c"
    break;

  case 31: /* shift_expr: shift_expr shift_operator add_expr  */
#line 403 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2412 "parser.c"
    break;

  case 32: /* shift_operator: ">>"  */
#line 410 "src/parser.y"
         { (yyval.kind) = IDL_RSHIFT; }
#line 2418 "parser.c"
    break;

  case 33: /* shift_operator: "<<"  */
#line 411 "src/parser.y"
         { (yyval.kind) = IDL_LSHIFT; }
#line 2424 "parser.c"
    break;

  case 35: /* add_expr: add_expr add_operator mult_expr  */
#line 416 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2433 "parser.c"
    break;

  case 36: /* add_operator: '+'  */
#line 423 "src/parser.y"
        { (yyval.kind) = IDL_ADD; }
#line 2439 "parser.c"
    break;

  case 37: /* add_operator: '-'  */
#line 424 "src/parser.y"
        { (yyval.kind) = IDL_SUBTRACT; }
#line 2445 "parser.c"
    break;

  case 38: /* mult_expr: unary_expr  */
#line 428 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2451 "parser.c"
    break;

  case 39: /* mult_expr: mult_expr mult_operator unary_expr  */
#line 430 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2460 "parser.c"
    break;

  case 40: /* mult_operator: '*'  */
#line 437 "src/parser.y"
        { (yyval.kind) = IDL_MULTIPLY; }
#line 2466 "parser.c"
    break;

  case 41: /* mult_operator: '/'  */
#line 438 "src/parser.y"
        { (yyval.kind) = IDL_DIVIDE; }
#line 2472 "parser.c"
    break;

  case 42: /* mult_operator: '%'  */
#line 439 "src/parser.y"
        { (yyval.kind) = IDL_MODULO; }
#line 2478 "parser.c"
    break;

  case 43: /* unary_expr: unary_operator primary_expr  */
#line 443 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_unary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2487 "parser.c"
    break;

  case 44: /* unary_expr: primary_expr  */
#line 448 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2493 "parser.c"
    break;

  case 45: /* unary_operator: '-'  */
#line 452 "src/parser.y"
        { (yyval.kind) = IDL_MINUS; }
#line 2499 "parser.c"
    break;

  case 46: /* unary_operator: '+'  */
#line 453 "src/parser.y"
        { (yyval.kind) = IDL_PLUS; }
#line 2505 "parser.c"
    break;

  case 47: /* unary_operator: '~'  */
#line 454 "src/parser.y"
        { (yyval.kind) = IDL_NOT; }
#line 2511 "parser.c"
    break;

  case 48: /* primary_expr: scoped_name  */
#line 459 "src/parser.y"
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
#line 2531 "parser.c"
    break;

  case 49: /* primary_expr: literal  */
#line 475 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 2537 "parser.c"
    break;

  case 50: /* primary_expr: '(' const_expr ')'  */
#line 477 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[-1].const_expr); }
#line 2543 "parser.c"
    break;

  case 51: /* literal: IDL_TOKEN_INTEGER_LITERAL  */
#line 482 "src/parser.y"
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
#line 2569 "parser.c"
    break;

  case 52: /* literal: IDL_TOKEN_FLOATING_PT_LITERAL  */
#line 504 "src/parser.y"
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
#line 2596 "parser.c"
    break;

  case 53: /* literal: IDL_TOKEN_CHAR_LITERAL  */
#line 527 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_CHAR, &(yyval.literal)));
        (yyval.literal)->value.chr = (yyvsp[0].chr);
      }
#line 2607 "parser.c"
    break;

  case 54: /* literal: boolean_literal  */
#line 534 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_BOOL, &(yyval.literal)));
        (yyval.literal)->value.bln = (yyvsp[0].bln);
      }
#line 2618 "parser.c"
    break;

  case 55: /* literal: string_literal  */
#line 541 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_STRING, &(yyval.literal)));
        (yyval.literal)->value.str = (yyvsp[0].string_literal);
      }
#line 2629 "parser.c"
    break;

  case 56: /* boolean_literal: "TRUE"  */
#line 551 "src/parser.y"
      { (yyval.bln) = true; }
#line 2635 "parser.c"
    break;

  case 57: /* boolean_literal: "FALSE"  */
#line 553 "src/parser.y"
      { (yyval.bln) = false; }
#line 2641 "parser.c"
    break;

  case 58: /* string_literal: IDL_TOKEN_STRING_LITERAL  */
#line 558 "src/parser.y"
      { (yyval.string_literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if (!((yyval.string_literal) = idl_strdup((yyvsp[0].str))))
          NO_MEMORY();
      }
#line 2652 "parser.c"
    break;

  case 59: /* string_literal: string_literal IDL_TOKEN_STRING_LITERAL  */
#line 565 "src/parser.y"
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
#line 2669 "parser.c"
    break;

  case 60: /* positive_int_const: const_expr  */
#line 581 "src/parser.y"
      { TRY(idl_evaluate(pstate, (yyvsp[0].const_expr), IDL_ULONG, &(yyval.literal))); }
#line 2675 "parser.c"
    break;

  case 61: /* type_dcl: constr_type_dcl  */
#line 585 "src/parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2681 "parser.c"
    break;

  case 62: /* type_dcl: typedef_dcl  */
#line 586 "src/parser.y"
                { (yyval.node) = (yyvsp[0].typedef_dcl); }
#line 2687 "parser.c"
    break;

  case 65: /* simple_type_spec: base_type_spec  */
#line 597 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2693 "parser.c"
    break;

  case 66: /* simple_type_spec: scoped_name  */
#line 599 "src/parser.y"
      { const idl_declaration_t *declaration = NULL;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a type";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        if (!declaration || !idl_is_type_spec(declaration->node))
          SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2707 "parser.c"
    break;

  case 73: /* floating_pt_type: "float"  */
#line 620 "src/parser.y"
            { (yyval.kind) = IDL_FLOAT; }
#line 2713 "parser.c"
    break;

  case 74: /* floating_pt_type: "double"  */
#line 621 "src/parser.y"
             { (yyval.kind) = IDL_DOUBLE; }
#line 2719 "parser.c"
    break;

  case 75: /* floating_pt_type: "long" "double"  */
#line 622 "src/parser.y"
                    { (yyval.kind) = IDL_LDOUBLE; }
#line 2725 "parser.c"
    break;

  case 78: /* signed_int: "short"  */
#line 631 "src/parser.y"
            { (yyval.kind) = IDL_SHORT; }
#line 2731 "parser.c"
    break;

  case 79: /* signed_int: "long"  */
#line 632 "src/parser.y"
           { (yyval.kind) = IDL_LONG; }
#line 2737 "parser.c"
    break;

  case 80: /* signed_int: "long" "long"  */
#line 633 "src/parser.y"
                  { (yyval.kind) = IDL_LLONG; }
#line 2743 "parser.c"
    break;

  case 81: /* signed_int: "int8"  */
#line 635 "src/parser.y"
           { (yyval.kind) = IDL_INT8; }
#line 2749 "parser.c"
    break;

  case 82: /* signed_int: "int16"  */
#line 636 "src/parser.y"
            { (yyval.kind) = IDL_INT16; }
#line 2755 "parser.c"
    break;

  case 83: /* signed_int: "int32"  */
#line 637 "src/parser.y"
            { (yyval.kind) = IDL_INT32; }
#line 2761 "parser.c"
    break;

  case 84: /* signed_int: "int64"  */
#line 638 "src/parser.y"
            { (yyval.kind) = IDL_INT64; }
#line 2767 "parser.c"
    break;

  case 85: /* unsigned_int: "unsigned" "short"  */
#line 642 "src/parser.y"
                       { (yyval.kind) = IDL_USHORT; }
#line 2773 "parser.c"
    break;

  case 86: /* unsigned_int: "unsigned" "long"  */
#line 643 "src/parser.y"
                      { (yyval.kind) = IDL_ULONG; }
#line 2779 "parser.c"
    break;

  case 87: /* unsigned_int: "unsigned" "long" "long"  */
#line 644 "src/parser.y"
                             { (yyval.kind) = IDL_ULLONG; }
#line 2785 "parser.c"
    break;

  case 88: /* unsigned_int: "uint8"  */
#line 646 "src/parser.y"
            { (yyval.kind) = IDL_UINT8; }
#line 2791 "parser.c"
    break;

  case 89: /* unsigned_int: "uint16"  */
#line 647 "src/parser.y"
             { (yyval.kind) = IDL_UINT16; }
#line 2797 "parser.c"
    break;

  case 90: /* unsigned_int: "uint32"  */
#line 648 "src/parser.y"
             { (yyval.kind) = IDL_UINT32; }
#line 2803 "parser.c"
    break;

  case 91: /* unsigned_int: "uint64"  */
#line 649 "src/parser.y"
             { (yyval.kind) = IDL_UINT64; }
#line 2809 "parser.c"
    break;

  case 92: /* char_type: "char"  */
#line 653 "src/parser.y"
           { (yyval.kind) = IDL_CHAR; }
#line 2815 "parser.c"
    break;

  case 93: /* wide_char_type: "wchar"  */
#line 656 "src/parser.y"
            { (yyval.kind) = IDL_WCHAR; }
#line 2821 "parser.c"
    break;

  case 94: /* boolean_type: "boolean"  */
#line 659 "src/parser.y"
              { (yyval.kind) = IDL_BOOL; }
#line 2827 "parser.c"
    break;

  case 95: /* octet_type: "octet"  */
#line 662 "src/parser.y"
            { (yyval.kind) = IDL_OCTET; }
#line 2833 "parser.c"
    break;

  case 96: /* template_type_spec: sequence_type  */
#line 665 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].sequence); }
#line 2839 "parser.c"
    break;

  case 97: /* template_type_spec: string_type  */
#line 666 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].string); }
#line 2845 "parser.c"
    break;

  case 98: /* sequence_type: "sequence" '<' type_spec ',' positive_int_const '>'  */
#line 671 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-5]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-1].literal), &(yyval.sequence))); }
#line 2851 "parser.c"
    break;

  case 99: /* sequence_type: "sequence" '<' type_spec '>'  */
#line 673 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), NULL, &(yyval.sequence))); }
#line 2857 "parser.c"
    break;

  case 100: /* string_type: "string" '<' positive_int_const '>'  */
#line 678 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].literal), &(yyval.string))); }
#line 2863 "parser.c"
    break;

  case 101: /* string_type: "string"  */
#line 680 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[0]).first, (yylsp[0]).last), NULL, &(yyval.string))); }
#line 2869 "parser.c"
    break;

  case 106: /* struct_dcl: struct_def  */
#line 691 "src/parser.y"
               { (yyval.node) = (yyvsp[0].struct_dcl); }
#line 2875 "parser.c"
    break;

  case 107: /* struct_dcl: struct_forward_dcl  */
#line 692 "src/parser.y"
                       { (yyval.node) = (yyvsp[0].forward); }
#line 2881 "parser.c"
    break;

  case 108: /* struct_forward_dcl: "struct" identifier  */
#line 697 "src/parser.y"
      { TRY(idl_create_forward(pstate, &(yylsp[-1]), (yyvsp[0].name), IDL_STRUCT, &(yyval.forward))); }
#line 2887 "parser.c"
    break;

  case 109: /* struct_def: struct_header '{' struct_body '}'  */
#line 702 "src/parser.y"
      { TRY(idl_finalize_struct(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].struct_dcl), (yyvsp[-1].member)));
        (yyval.struct_dcl) = (yyvsp[-3].struct_dcl);
      }
#line 2895 "parser.c"
    break;

  case 110: /* struct_header: "struct" identifier struct_inherit_spec  */
#line 709 "src/parser.y"
      { TRY(idl_create_struct(pstate, LOC((yylsp[-2]).first, (yyvsp[0].type_spec) ? (yylsp[0]).last : (yylsp[-1]).last), (yyvsp[-1].name), (yyvsp[0].type_spec), &(yyval.struct_dcl))); }
#line 2901 "parser.c"
    break;

  case 111: /* struct_inherit_spec: %empty  */
#line 713 "src/parser.y"
            { (yyval.type_spec) = NULL; }
#line 2907 "parser.c"
    break;

  case 112: /* struct_inherit_spec: ':' scoped_name  */
#line 717 "src/parser.y"
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
#line 2923 "parser.c"
    break;

  case 113: /* struct_body: members  */
#line 732 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 2929 "parser.c"
    break;

  case 114: /* struct_body: %empty  */
#line 736 "src/parser.y"
      { (yyval.member) = NULL; }
#line 2935 "parser.c"
    break;

  case 115: /* members: member  */
#line 741 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 2941 "parser.c"
    break;

  case 116: /* members: members member  */
#line 743 "src/parser.y"
      { (yyval.member) = idl_push_node((yyvsp[-1].member), (yyvsp[0].member)); }
#line 2947 "parser.c"
    break;

  case 117: /* member: annotations type_spec declarators ';'  */
#line 748 "src/parser.y"
      { TRY(idl_create_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), &(yyval.member)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.member), (yyvsp[-3].annotation_appl)), free((yyval.member)));
      }
#line 2955 "parser.c"
    break;

  case 118: /* union_dcl: union_def  */
#line 754 "src/parser.y"
              { (yyval.node) = (yyvsp[0].union_dcl); }
#line 2961 "parser.c"
    break;

  case 119: /* union_dcl: union_forward_dcl  */
#line 755 "src/parser.y"
                      { (yyval.node) = (yyvsp[0].forward); }
#line 2967 "parser.c"
    break;

  case 120: /* union_def: union_header '{' switch_body '}'  */
#line 760 "src/parser.y"
      { TRY(idl_finalize_union(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].union_dcl), (yyvsp[-1]._case)));
        (yyval.union_dcl) = (yyvsp[-3].union_dcl);
      }
#line 2975 "parser.c"
    break;

  case 121: /* union_forward_dcl: "union" identifier  */
#line 767 "src/parser.y"
      { TRY(idl_create_forward(pstate, &(yylsp[-1]), (yyvsp[0].name), IDL_UNION, &(yyval.forward))); }
#line 2981 "parser.c"
    break;

  case 122: /* union_header: "union" identifier switch_header  */
#line 772 "src/parser.y"
      { TRY(idl_create_union(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].switch_type_spec), &(yyval.union_dcl))); }
#line 2987 "parser.c"
    break;

  case 123: /* switch_header: "switch" '(' annotations switch_type_spec ')'  */
#line 777 "src/parser.y"
      { /* switch_header action is a separate non-terminal, as opposed to a
           mid-rule action, to avoid freeing the type specifier twice (once
           through destruction of the type-spec and once through destruction
           of the switch-type-spec) if union creation fails */
        TRY(idl_create_switch_type_spec(pstate, &(yylsp[-1]), (yyvsp[-1].type_spec), &(yyval.switch_type_spec)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.switch_type_spec), (yyvsp[-2].annotation_appl)), idl_delete_node((yyval.switch_type_spec)));
      }
#line 2999 "parser.c"
    break;

  case 124: /* switch_type_spec: integer_type  */
#line 788 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3005 "parser.c"
    break;

  case 125: /* switch_type_spec: char_type  */
#line 790 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3011 "parser.c"
    break;

  case 126: /* switch_type_spec: boolean_type  */
#line 792 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3017 "parser.c"
    break;

  case 127: /* switch_type_spec: scoped_name  */
#line 794 "src/parser.y"
      { const idl_declaration_t *declaration;
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        idl_delete_scoped_name((yyvsp[0].scoped_name));
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
      }
#line 3027 "parser.c"
    break;

  case 128: /* switch_type_spec: wide_char_type  */
#line 800 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3033 "parser.c"
    break;

  case 129: /* switch_type_spec: octet_type  */
#line 802 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3039 "parser.c"
    break;

  case 130: /* switch_body: case  */
#line 807 "src/parser.y"
      { (yyval._case) = (yyvsp[0]._case); }
#line 3045 "parser.c"
    break;

  case 131: /* switch_body: switch_body case  */
#line 809 "src/parser.y"
      { (yyval._case) = idl_push_node((yyvsp[-1]._case), (yyvsp[0]._case)); }
#line 3051 "parser.c"
    break;

  case 132: /* case: case_labels element_spec ';'  */
#line 814 "src/parser.y"
      { TRY(idl_finalize_case(pstate, &(yylsp[-1]), (yyvsp[-1]._case), (yyvsp[-2].case_label)));
        (yyval._case) = (yyvsp[-1]._case);
      }
#line 3059 "parser.c"
    break;

  case 133: /* case_labels: case_label  */
#line 821 "src/parser.y"
      { (yyval.case_label) = (yyvsp[0].case_label); }
#line 3065 "parser.c"
    break;

  case 134: /* case_labels: case_labels case_label  */
#line 823 "src/parser.y"
      { (yyval.case_label) = idl_push_node((yyvsp[-1].case_label), (yyvsp[0].case_label)); }
#line 3071 "parser.c"
    break;

  case 135: /* case_label: "case" const_expr ':'  */
#line 828 "src/parser.y"
      { TRY(idl_create_case_label(pstate, LOC((yylsp[-2]).first, (yylsp[-1]).last), (yyvsp[-1].const_expr), &(yyval.case_label))); }
#line 3077 "parser.c"
    break;

  case 136: /* case_label: "default" ':'  */
#line 830 "src/parser.y"
      { TRY(idl_create_case_label(pstate, &(yylsp[-1]), NULL, &(yyval.case_label))); }
#line 3083 "parser.c"
    break;

  case 137: /* element_spec: annotations type_spec declarator  */
#line 837 "src/parser.y"
      { TRY(idl_create_case(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval._case)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval._case), (yyvsp[-2].annotation_appl)), free((yyval._case)));
      }
#line 3091 "parser.c"
    break;

  case 138: /* enum_dcl: enum_def  */
#line 842 "src/parser.y"
                   { (yyval.node) = (yyvsp[0].enum_dcl); }
#line 3097 "parser.c"
    break;

  case 139: /* enum_def: "enum" identifier '{' enumerators '}'  */
#line 846 "src/parser.y"
      { TRY(idl_create_enum(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].enumerator), &(yyval.enum_dcl))); }
#line 3103 "parser.c"
    break;

  case 140: /* enumerators: enumerator  */
#line 851 "src/parser.y"
      { (yyval.enumerator) = (yyvsp[0].enumerator); }
#line 3109 "parser.c"
    break;

  case 141: /* enumerators: enumerators ',' enumerator  */
#line 853 "src/parser.y"
      { (yyval.enumerator) = idl_push_node((yyvsp[-2].enumerator), (yyvsp[0].enumerator)); }
#line 3115 "parser.c"
    break;

  case 142: /* enumerator: annotations identifier  */
#line 858 "src/parser.y"
      { TRY(idl_create_enumerator(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.enumerator)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.enumerator), (yyvsp[-1].annotation_appl)), free((yyval.enumerator)));
      }
#line 3123 "parser.c"
    break;

  case 143: /* bitmask_dcl: bitmask_def  */
#line 863 "src/parser.y"
                         { (yyval.node) = (yyvsp[0].bitmask_dcl); }
#line 3129 "parser.c"
    break;

  case 144: /* bitmask_def: "bitmask" identifier '{' bit_values '}'  */
#line 867 "src/parser.y"
      { TRY(idl_create_bitmask(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].bit_value), &(yyval.bitmask_dcl))); }
#line 3135 "parser.c"
    break;

  case 145: /* bit_values: bit_value  */
#line 872 "src/parser.y"
      { (yyval.bit_value) = (yyvsp[0].bit_value); }
#line 3141 "parser.c"
    break;

  case 146: /* bit_values: bit_values ',' bit_value  */
#line 874 "src/parser.y"
      { (yyval.bit_value) = idl_push_node((yyvsp[-2].bit_value), (yyvsp[0].bit_value)); }
#line 3147 "parser.c"
    break;

  case 147: /* bit_value: annotations identifier  */
#line 879 "src/parser.y"
      { TRY(idl_create_bit_value(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.bit_value)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.bit_value), (yyvsp[-1].annotation_appl)), free((yyval.bit_value)));
      }
#line 3155 "parser.c"
    break;

  case 148: /* array_declarator: identifier fixed_array_sizes  */
#line 886 "src/parser.y"
      { TRY(idl_create_declarator(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].const_expr), &(yyval.declarator))); }
#line 3161 "parser.c"
    break;

  case 149: /* fixed_array_sizes: fixed_array_size  */
#line 891 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 3167 "parser.c"
    break;

  case 150: /* fixed_array_sizes: fixed_array_sizes fixed_array_size  */
#line 893 "src/parser.y"
      { (yyval.const_expr) = idl_push_node((yyvsp[-1].const_expr), (yyvsp[0].literal)); }
#line 3173 "parser.c"
    break;

  case 151: /* fixed_array_size: '[' positive_int_const ']'  */
#line 898 "src/parser.y"
      { (yyval.literal) = (yyvsp[-1].literal); }
#line 3179 "parser.c"
    break;

  case 152: /* simple_declarator: identifier  */
#line 903 "src/parser.y"
      { TRY(idl_create_declarator(pstate, &(yylsp[0]), (yyvsp[0].name), NULL, &(yyval.declarator))); }
#line 3185 "parser.c"
    break;

  case 154: /* typedef_dcl: "typedef" type_spec declarators  */
#line 910 "src/parser.y"
      { TRY(idl_create_typedef(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval.typedef_dcl))); }
#line 3191 "parser.c"
    break;

  case 155: /* typedef_dcl: "typedef" constr_type_dcl declarators  */
#line 912 "src/parser.y"
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
#line 3209 "parser.c"
    break;

  case 156: /* declarators: declarator  */
#line 929 "src/parser.y"
      { (yyval.declarator) = (yyvsp[0].declarator); }
#line 3215 "parser.c"
    break;

  case 157: /* declarators: declarators ',' declarator  */
#line 931 "src/parser.y"
      { (yyval.declarator) = idl_push_node((yyvsp[-2].declarator), (yyvsp[0].declarator)); }
#line 3221 "parser.c"
    break;

  case 160: /* identifier: IDL_TOKEN_IDENTIFIER  */
#line 941 "src/parser.y"
      { (yyval.name) = NULL;
        size_t n;
        bool nocase = (pstate->config.flags & IDL_FLAG_CASE_SENSITIVE) == 0;
        if (pstate->parser.state == IDL_PARSE_ANNOTATION_APPL)
          n = 0;
        else if (pstate->parser.state == IDL_PARSE_ANNOTATION)
          n = 0;
        else if (!(n = ((yyvsp[0].str)[0] == '_')) && idl_iskeyword(pstate, (yyvsp[0].str), nocase))
          SEMANTIC_ERROR(&(yylsp[0]), "Identifier '%s' collides with a keyword", (yyvsp[0].str));

        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_name(pstate, &(yylsp[0]), idl_strdup((yyvsp[0].str)+n), &(yyval.name)));
      }
#line 3239 "parser.c"
    break;

  case 161: /* annotation_dcl: annotation_header '{' annotation_body '}'  */
#line 958 "src/parser.y"
      { (yyval.annotation) = NULL;
        /* discard annotation in case of redefinition */
        if (pstate->parser.state != IDL_PARSE_EXISTING_ANNOTATION_BODY)
          (yyval.annotation) = (yyvsp[-3].annotation);
        TRY(idl_finalize_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].annotation), (yyvsp[-1].annotation_member)));
      }
#line 3250 "parser.c"
    break;

  case 162: /* $@1: %empty  */
#line 968 "src/parser.y"
      { pstate->annotations = true; /* register annotation occurence */
        pstate->parser.state = IDL_PARSE_ANNOTATION;
      }
#line 3258 "parser.c"
    break;

  case 163: /* annotation_header: "@" "annotation" $@1 identifier  */
#line 972 "src/parser.y"
      { TRY(idl_create_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[-2]).last), (yyvsp[0].name), &(yyval.annotation))); }
#line 3264 "parser.c"
    break;

  case 164: /* annotation_body: %empty  */
#line 977 "src/parser.y"
      { (yyval.annotation_member) = NULL; }
#line 3270 "parser.c"
    break;

  case 165: /* annotation_body: annotation_body annotation_member ';'  */
#line 979 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].annotation_member)); }
#line 3276 "parser.c"
    break;

  case 166: /* annotation_body: annotation_body enum_dcl ';'  */
#line 981 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3282 "parser.c"
    break;

  case 167: /* annotation_body: annotation_body bitmask_dcl ';'  */
#line 983 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3288 "parser.c"
    break;

  case 168: /* annotation_body: annotation_body const_dcl ';'  */
#line 985 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].const_dcl)); }
#line 3294 "parser.c"
    break;

  case 169: /* annotation_body: annotation_body typedef_dcl ';'  */
#line 987 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].typedef_dcl)); }
#line 3300 "parser.c"
    break;

  case 170: /* annotation_member: annotation_member_type simple_declarator annotation_member_default  */
#line 992 "src/parser.y"
      { TRY(idl_create_annotation_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), (yyvsp[0].const_expr), &(yyval.annotation_member))); }
#line 3306 "parser.c"
    break;

  case 171: /* annotation_member_type: const_type  */
#line 997 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3312 "parser.c"
    break;

  case 172: /* annotation_member_type: any_const_type  */
#line 999 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3318 "parser.c"
    break;

  case 173: /* annotation_member_default: %empty  */
#line 1004 "src/parser.y"
      { (yyval.const_expr) = NULL; }
#line 3324 "parser.c"
    break;

  case 174: /* annotation_member_default: "default" const_expr  */
#line 1006 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 3330 "parser.c"
    break;

  case 175: /* any_const_type: "any"  */
#line 1011 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), IDL_ANY, &(yyval.type_spec))); }
#line 3336 "parser.c"
    break;

  case 176: /* annotations: annotation_appls  */
#line 1016 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3342 "parser.c"
    break;

  case 177: /* annotations: %empty  */
#line 1018 "src/parser.y"
      { (yyval.annotation_appl) = NULL; }
#line 3348 "parser.c"
    break;

  case 178: /* annotation_appls: annotation_appl  */
#line 1023 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3354 "parser.c"
    break;

  case 179: /* annotation_appls: annotation_appls annotation_appl  */
#line 1025 "src/parser.y"
      { (yyval.annotation_appl) = idl_push_node((yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl)); }
#line 3360 "parser.c"
    break;

  case 180: /* annotation_appl: annotation_appl_header annotation_appl_params  */
#line 1030 "src/parser.y"
      { if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_finalize_annotation_appl(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl_param)));
        pstate->parser.state = IDL_PARSE;
        pstate->annotation_scope = NULL;
        (yyval.annotation_appl) = (yyvsp[-1].annotation_appl);
      }
#line 3371 "parser.c"
    break;

  case 181: /* $@2: %empty  */
#line 1040 "src/parser.y"
      { pstate->parser.state = IDL_PARSE_ANNOTATION_APPL; }
#line 3377 "parser.c"
    break;

  case 182: /* annotation_appl_header: "@" $@2 annotation_appl_name  */
#line 1042 "src/parser.y"
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
#line 3402 "parser.c"
    break;

  case 183: /* annotation_appl_name: identifier  */
#line 1066 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 3408 "parser.c"
    break;

  case 184: /* annotation_appl_name: IDL_TOKEN_SCOPE_NO_SPACE identifier  */
#line 1068 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 3414 "parser.c"
    break;

  case 185: /* annotation_appl_name: annotation_appl_name IDL_TOKEN_SCOPE_NO_SPACE identifier  */
#line 1070 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 3422 "parser.c"
    break;

  case 186: /* annotation_appl_params: %empty  */
#line 1077 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL; }
#line 3428 "parser.c"
    break;

  case 187: /* annotation_appl_params: '(' const_expr ')'  */
#line 1079 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].const_expr); }
#line 3434 "parser.c"
    break;

  case 188: /* annotation_appl_params: '(' annotation_appl_keyword_params ')'  */
#line 1081 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].annotation_appl_param); }
#line 3440 "parser.c"
    break;

  case 189: /* annotation_appl_keyword_params: annotation_appl_keyword_param  */
#line 1086 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[0].annotation_appl_param); }
#line 3446 "parser.c"
    break;

  case 190: /* annotation_appl_keyword_params: annotation_appl_keyword_params ',' annotation_appl_keyword_param  */
#line 1088 "src/parser.y"
      { (yyval.annotation_appl_param) = idl_push_node((yyvsp[-2].annotation_appl_param), (yyvsp[0].annotation_appl_param)); }
#line 3452 "parser.c"
    break;

  case 191: /* @3: %empty  */
#line 1093 "src/parser.y"
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
#line 3472 "parser.c"
    break;

  case 192: /* annotation_appl_keyword_param: identifier @3 '=' const_expr  */
#line 1109 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          TRY(idl_create_annotation_appl_param(pstate, &(yylsp[-3]), (yyvsp[-2].annotation_member), (yyvsp[0].const_expr), &(yyval.annotation_appl_param)));
        }
      }
#line 3482 "parser.c"
    break;


#line 3486 "parser.c"

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
  yyerror (&yylloc, pstate, result, YY_("memory exhausted"));
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
#line 1116 "src/parser.y"


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
/* generated from parser.y[c5eaedb4a13d082e1b358e1e9b92f9d888389cf7] */
/* generated from parser.y[c5eaedb4a13d082e1b358e1e9b92f9d888389cf7] */
