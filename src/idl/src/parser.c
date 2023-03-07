/* A Bison parser, made by GNU Bison 3.5.1.  */

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

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

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

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
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

#line 219 "parser.c"

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

#line 330 "parser.c"

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

#line 371 "parser.c"

#endif /* !YY_IDL_YY_PARSER_H_INCLUDED  */



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

#if ! defined yyoverflow || YYERROR_VERBOSE

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
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


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

#define YYUNDEFTOK  2
#define YYMAXUTOK   309


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

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
       0,   288,   288,   290,   295,   297,   302,   304,   308,   312,
     319,   326,   331,   333,   335,   342,   347,   349,   351,   353,
     355,   357,   359,   373,   376,   377,   385,   386,   394,   395,
     403,   404,   412,   413,   416,   417,   425,   426,   429,   431,
     439,   440,   441,   444,   449,   454,   455,   456,   460,   476,
     478,   483,   505,   528,   535,   542,   552,   554,   559,   566,
     582,   587,   588,   592,   594,   598,   600,   613,   614,   615,
     616,   617,   618,   622,   623,   624,   628,   629,   633,   634,
     635,   637,   638,   639,   640,   644,   645,   646,   648,   649,
     650,   651,   655,   658,   661,   664,   667,   668,   672,   674,
     679,   681,   686,   687,   688,   689,   693,   694,   698,   703,
     710,   715,   718,   733,   737,   742,   744,   749,   756,   757,
     761,   768,   773,   778,   789,   791,   793,   795,   801,   803,
     808,   810,   815,   822,   824,   829,   831,   838,   844,   847,
     852,   854,   859,   865,   868,   873,   875,   880,   887,   892,
     894,   899,   904,   908,   911,   913,   930,   932,   937,   938,
     942,   961,   972,   971,   980,   982,   984,   986,   988,   990,
     995,  1000,  1002,  1007,  1009,  1014,  1019,  1021,  1026,  1028,
    1033,  1044,  1043,  1069,  1071,  1073,  1080,  1082,  1084,  1089,
    1091,  1097,  1096
};
#endif

#if IDL_YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "IDL_TOKEN_LINE_COMMENT",
  "IDL_TOKEN_COMMENT", "IDL_TOKEN_PP_NUMBER", "IDL_TOKEN_IDENTIFIER",
  "IDL_TOKEN_CHAR_LITERAL", "IDL_TOKEN_STRING_LITERAL",
  "IDL_TOKEN_INTEGER_LITERAL", "IDL_TOKEN_FLOATING_PT_LITERAL", "\"@\"",
  "\"annotation\"", "IDL_TOKEN_SCOPE", "IDL_TOKEN_SCOPE_NO_SPACE",
  "\"module\"", "\"const\"", "\"native\"", "\"struct\"", "\"typedef\"",
  "\"union\"", "\"switch\"", "\"case\"", "\"default\"", "\"enum\"",
  "\"unsigned\"", "\"fixed\"", "\"sequence\"", "\"string\"", "\"wstring\"",
  "\"float\"", "\"double\"", "\"short\"", "\"long\"", "\"char\"",
  "\"wchar\"", "\"boolean\"", "\"octet\"", "\"any\"", "\"map\"",
  "\"bitset\"", "\"bitfield\"", "\"bitmask\"", "\"int8\"", "\"int16\"",
  "\"int32\"", "\"int64\"", "\"uint8\"", "\"uint16\"", "\"uint32\"",
  "\"uint64\"", "\"TRUE\"", "\"FALSE\"", "\"<<\"", "\">>\"", "';'", "'{'",
  "'}'", "'='", "'|'", "'^'", "'&'", "'+'", "'-'", "'*'", "'/'", "'%'",
  "'~'", "'('", "')'", "'<'", "','", "'>'", "':'", "'['", "']'", "$accept",
  "specification", "definitions", "definition", "module_dcl",
  "module_header", "scoped_name", "const_dcl", "const_type", "const_expr",
  "or_expr", "xor_expr", "and_expr", "shift_expr", "shift_operator",
  "add_expr", "add_operator", "mult_expr", "mult_operator", "unary_expr",
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
#endif

# ifdef YYPRINT
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
# endif

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


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
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

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256


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

#ifndef YY_LOCATION_PRINT
# if defined IDL_YYLTYPE_IS_TRIVIAL && IDL_YYLTYPE_IS_TRIVIAL

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

#  define YY_LOCATION_PRINT(File, Loc)          \
  yy_location_print_ (File, &(Loc))

# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, Location, pstate, result); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  YYUSE (yylocationp);
  YYUSE (pstate);
  YYUSE (result);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, YYLTYPE const * const yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  YY_LOCATION_PRINT (yyo, *yylocationp);
  YYFPRINTF (yyo, ": ");
  yy_symbol_value_print (yyo, yytype, yyvaluep, yylocationp, pstate, result);
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
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, YYLTYPE *yylsp, int yyrule, idl_pstate_t *pstate, idl_retcode_t *result)
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
                       yystos[+yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                       , &(yylsp[(yyi + 1) - (yynrhs)])                       , pstate, result);
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
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
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


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#  else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                yy_state_t *yyssp, int yytoken)
{
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[+*yyssp];
      YYPTRDIFF_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
      yysize = yysize0;
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYPTRDIFF_T yysize1
                    = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    YYPTRDIFF_T yysize1 = yysize + (yystrlen (yyformat) - 2 * yycount) + 1;
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, YYLTYPE *yylocationp, idl_pstate_t *pstate, idl_retcode_t *result)
{
  YYUSE (yyvaluep);
  YYUSE (yylocationp);
  YYUSE (pstate);
  YYUSE (result);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  switch (yytype)
    {
    case 78: /* definitions  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1602 "parser.c"
        break;

    case 79: /* definition  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1608 "parser.c"
        break;

    case 80: /* module_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1614 "parser.c"
        break;

    case 81: /* module_header  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).module_dcl)); }
#line 1620 "parser.c"
        break;

    case 82: /* scoped_name  */
#line 210 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 1626 "parser.c"
        break;

    case 83: /* const_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).const_dcl)); }
#line 1632 "parser.c"
        break;

    case 84: /* const_type  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1638 "parser.c"
        break;

    case 85: /* const_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1644 "parser.c"
        break;

    case 86: /* or_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1650 "parser.c"
        break;

    case 87: /* xor_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1656 "parser.c"
        break;

    case 88: /* and_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1662 "parser.c"
        break;

    case 89: /* shift_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1668 "parser.c"
        break;

    case 91: /* add_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1674 "parser.c"
        break;

    case 93: /* mult_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1680 "parser.c"
        break;

    case 95: /* unary_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1686 "parser.c"
        break;

    case 97: /* primary_expr  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1692 "parser.c"
        break;

    case 98: /* literal  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1698 "parser.c"
        break;

    case 100: /* string_literal  */
#line 205 "src/parser.y"
            { idl_free(((*yyvaluep).string_literal)); }
#line 1704 "parser.c"
        break;

    case 101: /* positive_int_const  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1710 "parser.c"
        break;

    case 102: /* type_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1716 "parser.c"
        break;

    case 103: /* type_spec  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1722 "parser.c"
        break;

    case 104: /* simple_type_spec  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1728 "parser.c"
        break;

    case 114: /* template_type_spec  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1734 "parser.c"
        break;

    case 115: /* sequence_type  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).sequence)); }
#line 1740 "parser.c"
        break;

    case 116: /* string_type  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).string)); }
#line 1746 "parser.c"
        break;

    case 117: /* constr_type_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1752 "parser.c"
        break;

    case 118: /* struct_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1758 "parser.c"
        break;

    case 119: /* struct_forward_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).forward)); }
#line 1764 "parser.c"
        break;

    case 120: /* struct_def  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1770 "parser.c"
        break;

    case 121: /* struct_header  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).struct_dcl)); }
#line 1776 "parser.c"
        break;

    case 122: /* struct_inherit_spec  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1782 "parser.c"
        break;

    case 123: /* struct_body  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1788 "parser.c"
        break;

    case 124: /* members  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1794 "parser.c"
        break;

    case 125: /* member  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).member)); }
#line 1800 "parser.c"
        break;

    case 126: /* union_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1806 "parser.c"
        break;

    case 127: /* union_def  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1812 "parser.c"
        break;

    case 128: /* union_forward_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).forward)); }
#line 1818 "parser.c"
        break;

    case 129: /* union_header  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).union_dcl)); }
#line 1824 "parser.c"
        break;

    case 130: /* switch_header  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).switch_type_spec)); }
#line 1830 "parser.c"
        break;

    case 131: /* switch_type_spec  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1836 "parser.c"
        break;

    case 132: /* switch_body  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1842 "parser.c"
        break;

    case 133: /* case  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1848 "parser.c"
        break;

    case 134: /* case_labels  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1854 "parser.c"
        break;

    case 135: /* case_label  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).case_label)); }
#line 1860 "parser.c"
        break;

    case 136: /* element_spec  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep)._case)); }
#line 1866 "parser.c"
        break;

    case 137: /* enum_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1872 "parser.c"
        break;

    case 138: /* enum_def  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enum_dcl)); }
#line 1878 "parser.c"
        break;

    case 139: /* enumerators  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1884 "parser.c"
        break;

    case 140: /* enumerator  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).enumerator)); }
#line 1890 "parser.c"
        break;

    case 141: /* bitmask_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).node)); }
#line 1896 "parser.c"
        break;

    case 142: /* bitmask_def  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bitmask_dcl)); }
#line 1902 "parser.c"
        break;

    case 143: /* bit_values  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bit_value)); }
#line 1908 "parser.c"
        break;

    case 144: /* bit_value  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).bit_value)); }
#line 1914 "parser.c"
        break;

    case 145: /* array_declarator  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1920 "parser.c"
        break;

    case 146: /* fixed_array_sizes  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 1926 "parser.c"
        break;

    case 147: /* fixed_array_size  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).literal)); }
#line 1932 "parser.c"
        break;

    case 148: /* simple_declarator  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1938 "parser.c"
        break;

    case 149: /* complex_declarator  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1944 "parser.c"
        break;

    case 150: /* typedef_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).typedef_dcl)); }
#line 1950 "parser.c"
        break;

    case 151: /* declarators  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1956 "parser.c"
        break;

    case 152: /* declarator  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).declarator)); }
#line 1962 "parser.c"
        break;

    case 153: /* identifier  */
#line 207 "src/parser.y"
            { idl_delete_name(((*yyvaluep).name)); }
#line 1968 "parser.c"
        break;

    case 154: /* annotation_dcl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1974 "parser.c"
        break;

    case 155: /* annotation_header  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation)); }
#line 1980 "parser.c"
        break;

    case 157: /* annotation_body  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1986 "parser.c"
        break;

    case 158: /* annotation_member  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_member)); }
#line 1992 "parser.c"
        break;

    case 159: /* annotation_member_type  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 1998 "parser.c"
        break;

    case 160: /* annotation_member_default  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).const_expr)); }
#line 2004 "parser.c"
        break;

    case 161: /* any_const_type  */
#line 213 "src/parser.y"
            { idl_unreference_node(((*yyvaluep).type_spec)); }
#line 2010 "parser.c"
        break;

    case 162: /* annotations  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 2016 "parser.c"
        break;

    case 163: /* annotation_appls  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 2022 "parser.c"
        break;

    case 164: /* annotation_appl  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 2028 "parser.c"
        break;

    case 165: /* annotation_appl_header  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl)); }
#line 2034 "parser.c"
        break;

    case 167: /* annotation_appl_name  */
#line 210 "src/parser.y"
            { idl_delete_scoped_name(((*yyvaluep).scoped_name)); }
#line 2040 "parser.c"
        break;

    case 168: /* annotation_appl_params  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 2046 "parser.c"
        break;

    case 169: /* annotation_appl_keyword_params  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 2052 "parser.c"
        break;

    case 170: /* annotation_appl_keyword_param  */
#line 216 "src/parser.y"
            { idl_delete_node(((*yyvaluep).annotation_appl_param)); }
#line 2058 "parser.c"
        break;

      default:
        break;
    }
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}



struct yypstate
  {
    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.
       'yyls': related to locations.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    /* The location stack.  */
    YYLTYPE yylsa[YYINITDEPTH];
    YYLTYPE *yyls;
    YYLTYPE *yylsp;

    /* The locations where the error started and ended.  */
    YYLTYPE yyerror_range[3];

    YYPTRDIFF_T yystacksize;
    /* Used to determine if this is the first time this instance has
       been used.  */
    int yynew;
  };

/* Initialize the parser data structure.  */
yypstate *
yypstate_new (void)
{
  yypstate *yyps;
  yyps = YY_CAST (yypstate *, malloc (sizeof *yyps));
  if (!yyps)
    return YY_NULLPTR;
  yyps->yynew = 1;
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
      if (!yyps->yynew && yyps->yyss != yyps->yyssa)
        YYSTACK_FREE (yyps->yyss);
#endif
      free (yyps);
    }
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
#define yyerror_range yyps->yyerror_range
#define yystacksize yyps->yystacksize


/*---------------.
| yypush_parse.  |
`---------------*/

int
yypush_parse (yypstate *yyps, int yypushed_char, YYSTYPE const *yypushed_val, YYLTYPE *yypushed_loc, idl_pstate_t *pstate, idl_retcode_t *result)
{
/* The lookahead symbol.  */
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
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;
  YYLTYPE yyloc;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N), yylsp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  if (!yyps->yynew)
    {
      yyn = yypact[yystate];
      goto yyread_pushed_token;
    }

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yylsp = yyls = yylsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
# undef YYSTACK_RELOCATE
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

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      if (!yyps->yynew)
        {
          YYDPRINTF ((stderr, "Return for a new token:\n"));
          yyresult = YYPUSH_MORE;
          goto yypushreturn;
        }
      yyps->yynew = 0;
yyread_pushed_token:
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yypushed_char;
      if (yypushed_val)
        yylval = *yypushed_val;
      if (yypushed_loc)
        yylloc = *yypushed_loc;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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
  yychar = YYEMPTY;
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
  case 2:
#line 289 "src/parser.y"
      { pstate->root = NULL; }
#line 2426 "parser.c"
    break;

  case 3:
#line 291 "src/parser.y"
      { pstate->root = (yyvsp[0].node); }
#line 2432 "parser.c"
    break;

  case 4:
#line 296 "src/parser.y"
      { (yyval.node) = (yyvsp[0].node); }
#line 2438 "parser.c"
    break;

  case 5:
#line 298 "src/parser.y"
      { (yyval.node) = idl_push_node((yyvsp[-1].node), (yyvsp[0].node)); }
#line 2444 "parser.c"
    break;

  case 6:
#line 303 "src/parser.y"
      { (yyval.node) = (yyvsp[-1].annotation); }
#line 2450 "parser.c"
    break;

  case 7:
#line 305 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].module_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].module_dcl);
      }
#line 2458 "parser.c"
    break;

  case 8:
#line 309 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].const_dcl), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].const_dcl);
      }
#line 2466 "parser.c"
    break;

  case 9:
#line 313 "src/parser.y"
      { TRY(idl_annotate(pstate, (yyvsp[-1].node), (yyvsp[-2].annotation_appl)));
        (yyval.node) = (yyvsp[-1].node);
      }
#line 2474 "parser.c"
    break;

  case 10:
#line 320 "src/parser.y"
      { TRY(idl_finalize_module(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].module_dcl), (yyvsp[-1].node)));
        (yyval.module_dcl) = (yyvsp[-3].module_dcl);
      }
#line 2482 "parser.c"
    break;

  case 11:
#line 327 "src/parser.y"
      { TRY(idl_create_module(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), &(yyval.module_dcl))); }
#line 2488 "parser.c"
    break;

  case 12:
#line 332 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 2494 "parser.c"
    break;

  case 13:
#line 334 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 2500 "parser.c"
    break;

  case 14:
#line 336 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 2508 "parser.c"
    break;

  case 15:
#line 343 "src/parser.y"
      { TRY(idl_create_const(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-2].name), (yyvsp[0].const_expr), &(yyval.const_dcl))); }
#line 2514 "parser.c"
    break;

  case 16:
#line 348 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2520 "parser.c"
    break;

  case 17:
#line 350 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2526 "parser.c"
    break;

  case 18:
#line 352 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2532 "parser.c"
    break;

  case 19:
#line 354 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2538 "parser.c"
    break;

  case 20:
#line 356 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2544 "parser.c"
    break;

  case 21:
#line 358 "src/parser.y"
      { (yyval.type_spec) = (idl_type_spec_t *)(yyvsp[0].string); }
#line 2550 "parser.c"
    break;

  case 22:
#line 360 "src/parser.y"
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
#line 2566 "parser.c"
    break;

  case 23:
#line 373 "src/parser.y"
                    { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2572 "parser.c"
    break;

  case 25:
#line 378 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_OR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2581 "parser.c"
    break;

  case 27:
#line 387 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_XOR, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2590 "parser.c"
    break;

  case 29:
#line 396 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), IDL_AND, (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2599 "parser.c"
    break;

  case 31:
#line 405 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2608 "parser.c"
    break;

  case 32:
#line 412 "src/parser.y"
         { (yyval.kind) = IDL_RSHIFT; }
#line 2614 "parser.c"
    break;

  case 33:
#line 413 "src/parser.y"
         { (yyval.kind) = IDL_LSHIFT; }
#line 2620 "parser.c"
    break;

  case 35:
#line 418 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2629 "parser.c"
    break;

  case 36:
#line 425 "src/parser.y"
        { (yyval.kind) = IDL_ADD; }
#line 2635 "parser.c"
    break;

  case 37:
#line 426 "src/parser.y"
        { (yyval.kind) = IDL_SUBTRACT; }
#line 2641 "parser.c"
    break;

  case 38:
#line 430 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2647 "parser.c"
    break;

  case 39:
#line 432 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_binary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[-2].const_expr), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2656 "parser.c"
    break;

  case 40:
#line 439 "src/parser.y"
        { (yyval.kind) = IDL_MULTIPLY; }
#line 2662 "parser.c"
    break;

  case 41:
#line 440 "src/parser.y"
        { (yyval.kind) = IDL_DIVIDE; }
#line 2668 "parser.c"
    break;

  case 42:
#line 441 "src/parser.y"
        { (yyval.kind) = IDL_MODULO; }
#line 2674 "parser.c"
    break;

  case 43:
#line 445 "src/parser.y"
      { (yyval.const_expr) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_create_unary_expr(pstate, &(yylsp[-1]), (yyvsp[-1].kind), (yyvsp[0].const_expr), &(yyval.const_expr)));
      }
#line 2683 "parser.c"
    break;

  case 44:
#line 450 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 2689 "parser.c"
    break;

  case 45:
#line 454 "src/parser.y"
        { (yyval.kind) = IDL_MINUS; }
#line 2695 "parser.c"
    break;

  case 46:
#line 455 "src/parser.y"
        { (yyval.kind) = IDL_PLUS; }
#line 2701 "parser.c"
    break;

  case 47:
#line 456 "src/parser.y"
        { (yyval.kind) = IDL_NOT; }
#line 2707 "parser.c"
    break;

  case 48:
#line 461 "src/parser.y"
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
#line 2727 "parser.c"
    break;

  case 49:
#line 477 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 2733 "parser.c"
    break;

  case 50:
#line 479 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[-1].const_expr); }
#line 2739 "parser.c"
    break;

  case 51:
#line 484 "src/parser.y"
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
#line 2765 "parser.c"
    break;

  case 52:
#line 506 "src/parser.y"
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
#line 2792 "parser.c"
    break;

  case 53:
#line 529 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_CHAR, &(yyval.literal)));
        (yyval.literal)->value.chr = (yyvsp[0].chr);
      }
#line 2803 "parser.c"
    break;

  case 54:
#line 536 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_BOOL, &(yyval.literal)));
        (yyval.literal)->value.bln = (yyvsp[0].bln);
      }
#line 2814 "parser.c"
    break;

  case 55:
#line 543 "src/parser.y"
      { (yyval.literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        TRY(idl_create_literal(pstate, &(yylsp[0]), IDL_STRING, &(yyval.literal)));
        (yyval.literal)->value.str = (yyvsp[0].string_literal);
      }
#line 2825 "parser.c"
    break;

  case 56:
#line 553 "src/parser.y"
      { (yyval.bln) = true; }
#line 2831 "parser.c"
    break;

  case 57:
#line 555 "src/parser.y"
      { (yyval.bln) = false; }
#line 2837 "parser.c"
    break;

  case 58:
#line 560 "src/parser.y"
      { (yyval.string_literal) = NULL;
        if (pstate->parser.state == IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          break;
        if (!((yyval.string_literal) = idl_strdup((yyvsp[0].str))))
          NO_MEMORY();
      }
#line 2848 "parser.c"
    break;

  case 59:
#line 567 "src/parser.y"
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
#line 2865 "parser.c"
    break;

  case 60:
#line 583 "src/parser.y"
      { TRY(idl_evaluate(pstate, (yyvsp[0].const_expr), IDL_ULONG, &(yyval.literal))); }
#line 2871 "parser.c"
    break;

  case 61:
#line 587 "src/parser.y"
                    { (yyval.node) = (yyvsp[0].node); }
#line 2877 "parser.c"
    break;

  case 62:
#line 588 "src/parser.y"
                { (yyval.node) = (yyvsp[0].typedef_dcl); }
#line 2883 "parser.c"
    break;

  case 65:
#line 599 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 2889 "parser.c"
    break;

  case 66:
#line 601 "src/parser.y"
      { const idl_declaration_t *declaration = NULL;
        static const char fmt[] =
          "Scoped name '%s' does not resolve to a type";
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        if (!declaration || !idl_is_type_spec(declaration->node))
          SEMANTIC_ERROR(&(yylsp[0]), fmt, (yyvsp[0].scoped_name)->identifier);
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
        idl_delete_scoped_name((yyvsp[0].scoped_name));
      }
#line 2903 "parser.c"
    break;

  case 73:
#line 622 "src/parser.y"
            { (yyval.kind) = IDL_FLOAT; }
#line 2909 "parser.c"
    break;

  case 74:
#line 623 "src/parser.y"
             { (yyval.kind) = IDL_DOUBLE; }
#line 2915 "parser.c"
    break;

  case 75:
#line 624 "src/parser.y"
                    { (yyval.kind) = IDL_LDOUBLE; }
#line 2921 "parser.c"
    break;

  case 78:
#line 633 "src/parser.y"
            { (yyval.kind) = IDL_SHORT; }
#line 2927 "parser.c"
    break;

  case 79:
#line 634 "src/parser.y"
           { (yyval.kind) = IDL_LONG; }
#line 2933 "parser.c"
    break;

  case 80:
#line 635 "src/parser.y"
                  { (yyval.kind) = IDL_LLONG; }
#line 2939 "parser.c"
    break;

  case 81:
#line 637 "src/parser.y"
           { (yyval.kind) = IDL_INT8; }
#line 2945 "parser.c"
    break;

  case 82:
#line 638 "src/parser.y"
            { (yyval.kind) = IDL_INT16; }
#line 2951 "parser.c"
    break;

  case 83:
#line 639 "src/parser.y"
            { (yyval.kind) = IDL_INT32; }
#line 2957 "parser.c"
    break;

  case 84:
#line 640 "src/parser.y"
            { (yyval.kind) = IDL_INT64; }
#line 2963 "parser.c"
    break;

  case 85:
#line 644 "src/parser.y"
                       { (yyval.kind) = IDL_USHORT; }
#line 2969 "parser.c"
    break;

  case 86:
#line 645 "src/parser.y"
                      { (yyval.kind) = IDL_ULONG; }
#line 2975 "parser.c"
    break;

  case 87:
#line 646 "src/parser.y"
                             { (yyval.kind) = IDL_ULLONG; }
#line 2981 "parser.c"
    break;

  case 88:
#line 648 "src/parser.y"
            { (yyval.kind) = IDL_UINT8; }
#line 2987 "parser.c"
    break;

  case 89:
#line 649 "src/parser.y"
             { (yyval.kind) = IDL_UINT16; }
#line 2993 "parser.c"
    break;

  case 90:
#line 650 "src/parser.y"
             { (yyval.kind) = IDL_UINT32; }
#line 2999 "parser.c"
    break;

  case 91:
#line 651 "src/parser.y"
             { (yyval.kind) = IDL_UINT64; }
#line 3005 "parser.c"
    break;

  case 92:
#line 655 "src/parser.y"
           { (yyval.kind) = IDL_CHAR; }
#line 3011 "parser.c"
    break;

  case 93:
#line 658 "src/parser.y"
            { (yyval.kind) = IDL_WCHAR; }
#line 3017 "parser.c"
    break;

  case 94:
#line 661 "src/parser.y"
              { (yyval.kind) = IDL_BOOL; }
#line 3023 "parser.c"
    break;

  case 95:
#line 664 "src/parser.y"
            { (yyval.kind) = IDL_OCTET; }
#line 3029 "parser.c"
    break;

  case 96:
#line 667 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].sequence); }
#line 3035 "parser.c"
    break;

  case 97:
#line 668 "src/parser.y"
                  { (yyval.type_spec) = (yyvsp[0].string); }
#line 3041 "parser.c"
    break;

  case 98:
#line 673 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-5]).first, (yylsp[0]).last), (yyvsp[-3].type_spec), (yyvsp[-1].literal), &(yyval.sequence))); }
#line 3047 "parser.c"
    break;

  case 99:
#line 675 "src/parser.y"
      { TRY(idl_create_sequence(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), NULL, &(yyval.sequence))); }
#line 3053 "parser.c"
    break;

  case 100:
#line 680 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-1].literal), &(yyval.string))); }
#line 3059 "parser.c"
    break;

  case 101:
#line 682 "src/parser.y"
      { TRY(idl_create_string(pstate, LOC((yylsp[0]).first, (yylsp[0]).last), NULL, &(yyval.string))); }
#line 3065 "parser.c"
    break;

  case 106:
#line 693 "src/parser.y"
               { (yyval.node) = (yyvsp[0].struct_dcl); }
#line 3071 "parser.c"
    break;

  case 107:
#line 694 "src/parser.y"
                       { (yyval.node) = (yyvsp[0].forward); }
#line 3077 "parser.c"
    break;

  case 108:
#line 699 "src/parser.y"
      { TRY(idl_create_forward(pstate, &(yylsp[-1]), (yyvsp[0].name), IDL_STRUCT, &(yyval.forward))); }
#line 3083 "parser.c"
    break;

  case 109:
#line 704 "src/parser.y"
      { TRY(idl_finalize_struct(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].struct_dcl), (yyvsp[-1].member)));
        (yyval.struct_dcl) = (yyvsp[-3].struct_dcl);
      }
#line 3091 "parser.c"
    break;

  case 110:
#line 711 "src/parser.y"
      { TRY(idl_create_struct(pstate, LOC((yylsp[-2]).first, (yyvsp[0].type_spec) ? (yylsp[0]).last : (yylsp[-1]).last), (yyvsp[-1].name), (yyvsp[0].type_spec), &(yyval.struct_dcl))); }
#line 3097 "parser.c"
    break;

  case 111:
#line 715 "src/parser.y"
            { (yyval.type_spec) = NULL; }
#line 3103 "parser.c"
    break;

  case 112:
#line 719 "src/parser.y"
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
#line 3119 "parser.c"
    break;

  case 113:
#line 734 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 3125 "parser.c"
    break;

  case 114:
#line 738 "src/parser.y"
      { (yyval.member) = NULL; }
#line 3131 "parser.c"
    break;

  case 115:
#line 743 "src/parser.y"
      { (yyval.member) = (yyvsp[0].member); }
#line 3137 "parser.c"
    break;

  case 116:
#line 745 "src/parser.y"
      { (yyval.member) = idl_push_node((yyvsp[-1].member), (yyvsp[0].member)); }
#line 3143 "parser.c"
    break;

  case 117:
#line 750 "src/parser.y"
      { TRY(idl_create_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), &(yyval.member)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.member), (yyvsp[-3].annotation_appl)), idl_free((yyval.member)));
      }
#line 3151 "parser.c"
    break;

  case 118:
#line 756 "src/parser.y"
              { (yyval.node) = (yyvsp[0].union_dcl); }
#line 3157 "parser.c"
    break;

  case 119:
#line 757 "src/parser.y"
                      { (yyval.node) = (yyvsp[0].forward); }
#line 3163 "parser.c"
    break;

  case 120:
#line 762 "src/parser.y"
      { TRY(idl_finalize_union(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].union_dcl), (yyvsp[-1]._case)));
        (yyval.union_dcl) = (yyvsp[-3].union_dcl);
      }
#line 3171 "parser.c"
    break;

  case 121:
#line 769 "src/parser.y"
      { TRY(idl_create_forward(pstate, &(yylsp[-1]), (yyvsp[0].name), IDL_UNION, &(yyval.forward))); }
#line 3177 "parser.c"
    break;

  case 122:
#line 774 "src/parser.y"
      { TRY(idl_create_union(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].switch_type_spec), &(yyval.union_dcl))); }
#line 3183 "parser.c"
    break;

  case 123:
#line 779 "src/parser.y"
      { /* switch_header action is a separate non-terminal, as opposed to a
           mid-rule action, to avoid freeing the type specifier twice (once
           through destruction of the type-spec and once through destruction
           of the switch-type-spec) if union creation fails */
        TRY(idl_create_switch_type_spec(pstate, &(yylsp[-1]), (yyvsp[-1].type_spec), &(yyval.switch_type_spec)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.switch_type_spec), (yyvsp[-2].annotation_appl)), idl_delete_node((yyval.switch_type_spec)));
      }
#line 3195 "parser.c"
    break;

  case 124:
#line 790 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3201 "parser.c"
    break;

  case 125:
#line 792 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3207 "parser.c"
    break;

  case 126:
#line 794 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3213 "parser.c"
    break;

  case 127:
#line 796 "src/parser.y"
      { const idl_declaration_t *declaration;
        TRY(idl_resolve(pstate, 0u, (yyvsp[0].scoped_name), &declaration));
        idl_delete_scoped_name((yyvsp[0].scoped_name));
        (yyval.type_spec) = idl_reference_node((idl_node_t *)declaration->node);
      }
#line 3223 "parser.c"
    break;

  case 128:
#line 802 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3229 "parser.c"
    break;

  case 129:
#line 804 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), (yyvsp[0].kind), &(yyval.type_spec))); }
#line 3235 "parser.c"
    break;

  case 130:
#line 809 "src/parser.y"
      { (yyval._case) = (yyvsp[0]._case); }
#line 3241 "parser.c"
    break;

  case 131:
#line 811 "src/parser.y"
      { (yyval._case) = idl_push_node((yyvsp[-1]._case), (yyvsp[0]._case)); }
#line 3247 "parser.c"
    break;

  case 132:
#line 816 "src/parser.y"
      { TRY(idl_finalize_case(pstate, &(yylsp[-1]), (yyvsp[-1]._case), (yyvsp[-2].case_label)));
        (yyval._case) = (yyvsp[-1]._case);
      }
#line 3255 "parser.c"
    break;

  case 133:
#line 823 "src/parser.y"
      { (yyval.case_label) = (yyvsp[0].case_label); }
#line 3261 "parser.c"
    break;

  case 134:
#line 825 "src/parser.y"
      { (yyval.case_label) = idl_push_node((yyvsp[-1].case_label), (yyvsp[0].case_label)); }
#line 3267 "parser.c"
    break;

  case 135:
#line 830 "src/parser.y"
      { TRY(idl_create_case_label(pstate, LOC((yylsp[-2]).first, (yylsp[-1]).last), (yyvsp[-1].const_expr), &(yyval.case_label))); }
#line 3273 "parser.c"
    break;

  case 136:
#line 832 "src/parser.y"
      { TRY(idl_create_case_label(pstate, &(yylsp[-1]), NULL, &(yyval.case_label))); }
#line 3279 "parser.c"
    break;

  case 137:
#line 839 "src/parser.y"
      { TRY(idl_create_case(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval._case)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval._case), (yyvsp[-2].annotation_appl)), idl_free((yyval._case)));
      }
#line 3287 "parser.c"
    break;

  case 138:
#line 844 "src/parser.y"
                   { (yyval.node) = (yyvsp[0].enum_dcl); }
#line 3293 "parser.c"
    break;

  case 139:
#line 848 "src/parser.y"
      { TRY(idl_create_enum(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].enumerator), &(yyval.enum_dcl))); }
#line 3299 "parser.c"
    break;

  case 140:
#line 853 "src/parser.y"
      { (yyval.enumerator) = (yyvsp[0].enumerator); }
#line 3305 "parser.c"
    break;

  case 141:
#line 855 "src/parser.y"
      { (yyval.enumerator) = idl_push_node((yyvsp[-2].enumerator), (yyvsp[0].enumerator)); }
#line 3311 "parser.c"
    break;

  case 142:
#line 860 "src/parser.y"
      { TRY(idl_create_enumerator(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.enumerator)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.enumerator), (yyvsp[-1].annotation_appl)), idl_free((yyval.enumerator)));
      }
#line 3319 "parser.c"
    break;

  case 143:
#line 865 "src/parser.y"
                         { (yyval.node) = (yyvsp[0].bitmask_dcl); }
#line 3325 "parser.c"
    break;

  case 144:
#line 869 "src/parser.y"
      { TRY(idl_create_bitmask(pstate, LOC((yylsp[-4]).first, (yylsp[0]).last), (yyvsp[-3].name), (yyvsp[-1].bit_value), &(yyval.bitmask_dcl))); }
#line 3331 "parser.c"
    break;

  case 145:
#line 874 "src/parser.y"
      { (yyval.bit_value) = (yyvsp[0].bit_value); }
#line 3337 "parser.c"
    break;

  case 146:
#line 876 "src/parser.y"
      { (yyval.bit_value) = idl_push_node((yyvsp[-2].bit_value), (yyvsp[0].bit_value)); }
#line 3343 "parser.c"
    break;

  case 147:
#line 881 "src/parser.y"
      { TRY(idl_create_bit_value(pstate, &(yylsp[0]), (yyvsp[0].name), &(yyval.bit_value)));
        TRY_EXCEPT(idl_annotate(pstate, (yyval.bit_value), (yyvsp[-1].annotation_appl)), idl_free((yyval.bit_value)));
      }
#line 3351 "parser.c"
    break;

  case 148:
#line 888 "src/parser.y"
      { TRY(idl_create_declarator(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].name), (yyvsp[0].const_expr), &(yyval.declarator))); }
#line 3357 "parser.c"
    break;

  case 149:
#line 893 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].literal); }
#line 3363 "parser.c"
    break;

  case 150:
#line 895 "src/parser.y"
      { (yyval.const_expr) = idl_push_node((yyvsp[-1].const_expr), (yyvsp[0].literal)); }
#line 3369 "parser.c"
    break;

  case 151:
#line 900 "src/parser.y"
      { (yyval.literal) = (yyvsp[-1].literal); }
#line 3375 "parser.c"
    break;

  case 152:
#line 905 "src/parser.y"
      { TRY(idl_create_declarator(pstate, &(yylsp[0]), (yyvsp[0].name), NULL, &(yyval.declarator))); }
#line 3381 "parser.c"
    break;

  case 154:
#line 912 "src/parser.y"
      { TRY(idl_create_typedef(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-1].type_spec), (yyvsp[0].declarator), &(yyval.typedef_dcl))); }
#line 3387 "parser.c"
    break;

  case 155:
#line 914 "src/parser.y"
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
#line 3405 "parser.c"
    break;

  case 156:
#line 931 "src/parser.y"
      { (yyval.declarator) = (yyvsp[0].declarator); }
#line 3411 "parser.c"
    break;

  case 157:
#line 933 "src/parser.y"
      { (yyval.declarator) = idl_push_node((yyvsp[-2].declarator), (yyvsp[0].declarator)); }
#line 3417 "parser.c"
    break;

  case 160:
#line 943 "src/parser.y"
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
#line 3437 "parser.c"
    break;

  case 161:
#line 962 "src/parser.y"
      { (yyval.annotation) = NULL;
        /* discard annotation in case of redefinition */
        if (pstate->parser.state != IDL_PARSE_EXISTING_ANNOTATION_BODY)
          (yyval.annotation) = (yyvsp[-3].annotation);
        TRY(idl_finalize_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[0]).last), (yyvsp[-3].annotation), (yyvsp[-1].annotation_member)));
      }
#line 3448 "parser.c"
    break;

  case 162:
#line 972 "src/parser.y"
      { pstate->annotations = true; /* register annotation occurence */
        pstate->parser.state = IDL_PARSE_ANNOTATION;
      }
#line 3456 "parser.c"
    break;

  case 163:
#line 976 "src/parser.y"
      { TRY(idl_create_annotation(pstate, LOC((yylsp[-3]).first, (yylsp[-2]).last), (yyvsp[0].name), &(yyval.annotation))); }
#line 3462 "parser.c"
    break;

  case 164:
#line 981 "src/parser.y"
      { (yyval.annotation_member) = NULL; }
#line 3468 "parser.c"
    break;

  case 165:
#line 983 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].annotation_member)); }
#line 3474 "parser.c"
    break;

  case 166:
#line 985 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3480 "parser.c"
    break;

  case 167:
#line 987 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].node)); }
#line 3486 "parser.c"
    break;

  case 168:
#line 989 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].const_dcl)); }
#line 3492 "parser.c"
    break;

  case 169:
#line 991 "src/parser.y"
      { (yyval.annotation_member) = idl_push_node((yyvsp[-2].annotation_member), (yyvsp[-1].typedef_dcl)); }
#line 3498 "parser.c"
    break;

  case 170:
#line 996 "src/parser.y"
      { TRY(idl_create_annotation_member(pstate, LOC((yylsp[-2]).first, (yylsp[0]).last), (yyvsp[-2].type_spec), (yyvsp[-1].declarator), (yyvsp[0].const_expr), &(yyval.annotation_member))); }
#line 3504 "parser.c"
    break;

  case 171:
#line 1001 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3510 "parser.c"
    break;

  case 172:
#line 1003 "src/parser.y"
      { (yyval.type_spec) = (yyvsp[0].type_spec); }
#line 3516 "parser.c"
    break;

  case 173:
#line 1008 "src/parser.y"
      { (yyval.const_expr) = NULL; }
#line 3522 "parser.c"
    break;

  case 174:
#line 1010 "src/parser.y"
      { (yyval.const_expr) = (yyvsp[0].const_expr); }
#line 3528 "parser.c"
    break;

  case 175:
#line 1015 "src/parser.y"
      { TRY(idl_create_base_type(pstate, &(yylsp[0]), IDL_ANY, &(yyval.type_spec))); }
#line 3534 "parser.c"
    break;

  case 176:
#line 1020 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3540 "parser.c"
    break;

  case 177:
#line 1022 "src/parser.y"
      { (yyval.annotation_appl) = NULL; }
#line 3546 "parser.c"
    break;

  case 178:
#line 1027 "src/parser.y"
      { (yyval.annotation_appl) = (yyvsp[0].annotation_appl); }
#line 3552 "parser.c"
    break;

  case 179:
#line 1029 "src/parser.y"
      { (yyval.annotation_appl) = idl_push_node((yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl)); }
#line 3558 "parser.c"
    break;

  case 180:
#line 1034 "src/parser.y"
      { if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS)
          TRY(idl_finalize_annotation_appl(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[-1].annotation_appl), (yyvsp[0].annotation_appl_param)));
        pstate->parser.state = IDL_PARSE;
        pstate->annotation_scope = NULL;
        (yyval.annotation_appl) = (yyvsp[-1].annotation_appl);
      }
#line 3569 "parser.c"
    break;

  case 181:
#line 1044 "src/parser.y"
      { pstate->parser.state = IDL_PARSE_ANNOTATION_APPL; }
#line 3575 "parser.c"
    break;

  case 182:
#line 1046 "src/parser.y"
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
#line 3600 "parser.c"
    break;

  case 183:
#line 1070 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, &(yylsp[0]), (yyvsp[0].name), false, &(yyval.scoped_name))); }
#line 3606 "parser.c"
    break;

  case 184:
#line 1072 "src/parser.y"
      { TRY(idl_create_scoped_name(pstate, LOC((yylsp[-1]).first, (yylsp[0]).last), (yyvsp[0].name), true, &(yyval.scoped_name))); }
#line 3612 "parser.c"
    break;

  case 185:
#line 1074 "src/parser.y"
      { TRY(idl_push_scoped_name(pstate, (yyvsp[-2].scoped_name), (yyvsp[0].name)));
        (yyval.scoped_name) = (yyvsp[-2].scoped_name);
      }
#line 3620 "parser.c"
    break;

  case 186:
#line 1081 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL; }
#line 3626 "parser.c"
    break;

  case 187:
#line 1083 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].const_expr); }
#line 3632 "parser.c"
    break;

  case 188:
#line 1085 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[-1].annotation_appl_param); }
#line 3638 "parser.c"
    break;

  case 189:
#line 1090 "src/parser.y"
      { (yyval.annotation_appl_param) = (yyvsp[0].annotation_appl_param); }
#line 3644 "parser.c"
    break;

  case 190:
#line 1092 "src/parser.y"
      { (yyval.annotation_appl_param) = idl_push_node((yyvsp[-2].annotation_appl_param), (yyvsp[0].annotation_appl_param)); }
#line 3650 "parser.c"
    break;

  case 191:
#line 1097 "src/parser.y"
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
#line 3670 "parser.c"
    break;

  case 192:
#line 1113 "src/parser.y"
      { (yyval.annotation_appl_param) = NULL;
        if (pstate->parser.state != IDL_PARSE_UNKNOWN_ANNOTATION_APPL_PARAMS) {
          TRY(idl_create_annotation_appl_param(pstate, &(yylsp[-3]), (yyvsp[-2].annotation_member), (yyvsp[0].const_expr), &(yyval.annotation_appl_param)));
        }
      }
#line 3680 "parser.c"
    break;


#line 3684 "parser.c"

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
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

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
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (&yylloc, pstate, result, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *, YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (&yylloc, pstate, result, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }

  yyerror_range[1] = yylloc;

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, &yylloc, pstate, result);
          yychar = YYEMPTY;
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

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
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
                  yystos[yystate], yyvsp, yylsp, pstate, result);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  yyerror_range[2] = yylloc;
  /* Using YYLLOC is tempting, but would change the location of
     the lookahead.  YYLOC is available though.  */
  YYLLOC_DEFAULT (yyloc, yyerror_range, 2);
  *++yylsp = yyloc;

  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

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


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (&yylloc, pstate, result, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
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
                  yystos[+*yyssp], yyvsp, yylsp, pstate, result);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
  yyps->yynew = 1;


/*-----------------------------------------.
| yypushreturn -- ask for the next token.  |
`-----------------------------------------*/
yypushreturn:
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
#line 1120 "src/parser.y"


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
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
/* generated from parser.y[5e3953cec3a2bb5db62be00fa8968694e1c7ff05] */
