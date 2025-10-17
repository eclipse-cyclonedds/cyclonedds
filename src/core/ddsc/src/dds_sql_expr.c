// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/retcode.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "dds/ddsrt/string.h"
#include "dds__sql_expr.h"

#define REAL_PREC     1e-7

#define DIGIT_SEPARATOR '_'

#define CC_X          0    /* The letter 'x', or start of BLOB literal */
#define CC_KYWD0      1    /* First letter of a keyword */
#define CC_KYWD       2    /* Alphabetics or '_'.  Usable in a keyword */
#define CC_DIGIT      3    /* Digits */
#define CC_DOLLAR     4    /* '$' */
#define CC_VARNUM     6    /* '?'.  Numeric SQL variables */
#define CC_SPACE      7    /* Space characters */
#define CC_QUOTE      8    /* '"', '\'', or '`'.  String literals, quoted ids */
#define CC_PIPE      10    /* '|'.   Bitwise OR or concatenate */
#define CC_MINUS     11    /* '-'.  Minus or SQL-style comment */
#define CC_LT        12    /* '<'.  Part of < or <= or <> */
#define CC_GT        13    /* '>'.  Part of > or >= */
#define CC_EQ        14    /* '='.  Part of = or == */
#define CC_BANG      15    /* '!'.  Part of != */
#define CC_SLASH     16    /* '/'.  / or c-style comment */
#define CC_LP        17    /* '(' */
#define CC_RP        18    /* ')' */
#define CC_SEMI      19    /* ';' */
#define CC_PLUS      20    /* '+' */
#define CC_STAR      21    /* '*' */
#define CC_PERCENT   22    /* '%' */
#define CC_COMMA     23    /* ',' */
#define CC_AND       24    /* '&' */
#define CC_TILDA     25    /* '~' */
#define CC_DOT       26    /* '.' */
#define CC_ID        27    /* unicode characters usable in IDs */
#define CC_ILLEGAL   28    /* Illegal character */

static const unsigned char cc_table[] = {
/*         x0  x1  x2  x3  x4  x5  x6  x7  x8  x9  xa  xb  xc  xd  xe  xf */
/* 0x */   29, 28, 28, 28, 28, 28, 28, 28, 28,  7,  7, 28,  7,  7, 28, 28,
/* 1x */   28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
/* 2x */    7, 15,  8,  5,  4, 22, 24,  8, 17, 18, 21, 20, 23, 11, 26, 16,
/* 3x */    3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  5, 19, 12, 14, 13,  6,
/* 4x */    5,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 5x */    1,  1,  1,  1,  1,  1,  1,  1,  0,  2,  2,  9, 28, 28, 28,  2,
/* 6x */    8,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
/* 7x */    1,  1,  1,  1,  1,  1,  1,  1,  0,  2,  2, 28, 10, 28, 25, 28,
/* 8x */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* 9x */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* Ax */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* Bx */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* Cx */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* Dx */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
/* Ex */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 30,
/* Fx */   27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27
};

#define id_char(c) (isalpha(c) || c == '_')

int dds_sql_get_token(const unsigned char *s, int *token)
{
  int i, c;
  switch (cc_table[*s])
  {
#define CASE(p) CC_##p: \
  *token=DDS_SQL_TK_##p; \
  return 1
    case CC_SPACE: {
      for (i = 1; isspace(s[i]); i++) {}
      *token = DDS_SQL_TK_SPACE;
      return i;
    }
    case CC_MINUS: {
      if (s[1] == '-') {
        for (i = 2; (c = s[i]) != 0 && c != '\n'; i++) {}
        *token = DDS_SQL_TK_COMMENT;
        return i;
      }
      /* FIXME: DDS_SQL_TK_PTR ?*/
      *token = DDS_SQL_TK_MINUS;
      return 1;
    }
    case CASE(LP);
    case CASE(RP);
    case CASE(PLUS);
    case CASE(STAR);
    case CC_SLASH: {
      if (s[1] != '*' || s[2] == 0) {
        *token = DDS_SQL_TK_SLASH;
        return 1;
      }
      for (i = 3, c = s[2]; (c != '*' || s[i] != '/') && (c = s[i]) != 0; i++){}
      i+=(c)?1:0;
      *token = DDS_SQL_TK_COMMENT;
      return i;
    }
    case CC_PERCENT: {
      *token = DDS_SQL_TK_REM;
      return 1;
    }
    case CC_EQ: {
      *token = DDS_SQL_TK_EQ;
      return 1 + (s[1] == '=');
    }
    case CC_LT: {
      if (   ((c = s[1]) == '=' && (*token = DDS_SQL_TK_LE))
          || (         c == '>' && (*token = DDS_SQL_TK_NE))
          || (         c == '<' && (*token = DDS_SQL_TK_LSHIFT)))
        return 2;

      *token = DDS_SQL_TK_LT;
      return 1;
    }
    case CC_GT: {
      if (   ((c = s[1]) == '=' && (*token = DDS_SQL_TK_GE))
          || (         c == '>' && (*token = DDS_SQL_TK_RSHIFT)))
        return 2;
      *token = DDS_SQL_TK_GT;
      return 1;
    }
    case CC_BANG: {
      if (s[1] != '=' && (*token = DDS_SQL_TK_ILLEGAL))
        return 1;

      *token = DDS_SQL_TK_NE;
      return 2;
    }
    case CC_PIPE: {
      if (s[1] != '|' && (*token = DDS_SQL_TK_BITOR))
        return 1;

      *token = DDS_SQL_TK_CONCAT;
      return 2;
    }
    case CASE(COMMA);
    case CC_AND: {
      *token = DDS_SQL_TK_BITAND;
      return 1;
    }
    case CC_TILDA: {
      *token = DDS_SQL_TK_BITNOT;
      return 1;
    }
    case CC_QUOTE: {
      /* '`', '"'  for IDs
       *   '\''    for STRING */
      int delim = s[0];
      for (i = 1; (c = s[i]) != 0 && (c != delim); i++) {}
      if (   (c == '\'' && (*token = DDS_SQL_TK_STRING))
          || (c != 0    && (*token = DDS_SQL_TK_ID)))
        return i+1;

      *token = DDS_SQL_TK_ILLEGAL;
      return i;
    }
    case CC_DOT:
      if (!isdigit(s[1]) && (*token = DDS_SQL_TK_DOT))
        return 1;
      /* fall through */
    case CC_DIGIT: {
      *token = DDS_SQL_TK_INTEGER;
      if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X') && isxdigit(s[2])) {
        i = 3;
        while (1) {
          if (isxdigit(s[i]) == 0) {
            if (s[i] == DIGIT_SEPARATOR) {
              *token = DDS_SQL_TK_QNUMBER;
            } else {
              break;
            }
          }
          i++;
        }
      } else {
        i = 0;
        while(1) {
          if (isdigit(s[i]) == 0) {
            if (s[i] == DIGIT_SEPARATOR) {
              *token = DDS_SQL_TK_QNUMBER;
            } else {
              break;
            }
          }
          i++;
        }
        if (s[i] == '.') {
          if (*token == DDS_SQL_TK_INTEGER)
            *token = DDS_SQL_TK_FLOAT;
          i+=1;
          while (1) {
            if (isdigit(s[i]) == 0) {
              if (s[i] == DIGIT_SEPARATOR) {
                *token = DDS_SQL_TK_QNUMBER;
              } else {
                break;
              }
            }
            i++;
          }
        }
        if ((s[i] == 'e' || s[i] == 'E') &&
            ( isdigit(s[i+1])
             || ((s[i+1] == '+' || s[i+1] == '-') && isdigit(s[i+2]))
            )
        ) {
          if (*token == DDS_SQL_TK_INTEGER)
            *token = DDS_SQL_TK_FLOAT;
          i+=2;
          while(1) {
            if (isdigit(s[i]) == 0) {
              if (s[i] == DIGIT_SEPARATOR) {
                *token = DDS_SQL_TK_QNUMBER;
              } else {
                break;
              }
            }
            i++;
          }
        }
      }
      while (id_char(s[i])) {
        *token = DDS_SQL_TK_ILLEGAL;
        i++;
      }
      return i;
    }
    case CC_VARNUM:
      /* """
       * A question mark that is not followed by a number creates a parameter
       * with a number one greater than the largest parameter number already
       * assigned.
       * But we limit the usage of `?` to number followed only.
       * """
       */
      *token = DDS_SQL_TK_ILLEGAL;
      for(i = 1; isdigit(s[i]); i++){}
      if (i > 1) *token = DDS_SQL_TK_VARIABLE;
      return i;
    case CC_DOLLAR:
    case CC_KYWD0: {
      if (cc_table[s[1]] > CC_KYWD && (i = 1))
        break;
      for (i = 2; cc_table[s[i]] <= CC_KYWD; i++) {}
      if (id_char(s[i]) && (i++))
        break;
      *token = DDS_SQL_TK_ID;

      /* NOTE: if keyword is a token then one of those, otherwise it's id,
       * otherwise it's syntax error.
       * CAST
       * --AS
       * COLLATE
       * NOT
       * LIKE
       * --GLOB
       * --REGEXP
       * --MATCH
       * ESCAPE
       * --ISNULL
       * --NOTNULL
       * --NULL
       * --IS
       * --DISTINCT
       * --FROM
       * BETWEEN
       * AND
       * --IN
       * OR
       * --EXISTS
       * --CASE
       * --WHEN
       * --THEN
       * --ELSE
       * --END
       * */

#define IS_KYWD(w) \
      !memcmp (s, w, (unsigned)i)
      switch (i)
      {
        case 2: {
          if (     IS_KYWD("OR"))
            *token = DDS_SQL_TK_OR;
          break;
        }
        case 3: {
          if (     IS_KYWD("NOT"))
            *token = DDS_SQL_TK_NOT;
          else if (IS_KYWD("AND"))
            *token = DDS_SQL_TK_AND;
          break;
        }
        case 4: {
          if (     IS_KYWD("LIKE"))
            *token = DDS_SQL_TK_LIKE_KW;
          break;
        }
        /* case 5: */
        case 6: {
          if (     IS_KYWD("ESCAPE"))
            *token = DDS_SQL_TK_ESCAPE;
          break;
        }
        case 7: {
          if (     IS_KYWD("COLLATE"))
            *token = DDS_SQL_TK_COLLATE;
          else if (IS_KYWD("BETWEEN"))
            *token = DDS_SQL_TK_BETWEEN;
          break;
        }
        default:
          break;
      }
#undef IS_KYWD

      return i;
    }
    case CC_X:
      if ((s[0] == 'x' || s[0] == 'X') && s[1] == '\'') {
        *token = DDS_SQL_TK_BLOB;
        for (i = 2; isxdigit(s[i]); i++) {}
        if (s[i] != '\'' || i % 2) {
          *token = DDS_SQL_TK_ILLEGAL;
          for (;s[i] && s[i] != '\''; i++) {}
        }
        if (s[i]) i++;
        return i;
      }
      /* fall through */
    case CC_KYWD:
    case CC_ID: {
      i = 1;
      break;
    }
    default: {
      *token = DDS_SQL_TK_ILLEGAL;
      return 1;
    }
#undef CASE
  }

  for (; id_char(s[i]); i++) {}
  *token = DDS_SQL_TK_ID;
  return i;
}

static int get_op_token(const unsigned char **s, int *token)
{
  assert (token != NULL);
  *token = -1;
  const unsigned char *cur = *s;
  int i = 0;
  do {
    if (*cur == '\0')
      goto exit;
    cur += (i=dds_sql_get_token(cur, token));
  } while (*token == DDS_SQL_TK_SPACE);

exit:
  *s = cur;
  return i;
}

size_t dds_sql_expr_count_params(const char *s)
{
  const unsigned char *cursor = (const unsigned char *)s;
  size_t total = 0;
  int token = 0;

  do {
    if (get_op_token(&cursor, &token) > 0 && token > 0) {
      total += (token == DDS_SQL_TK_VARIABLE);
    }
  } while (token > 0);

  return total;
}

static int get_op_affinity(const int op_token)
{
  int aff = DDS_SQL_AFFINITY_NONE;
  switch (op_token)
  {
    case DDS_SQL_TK_OR:
    case DDS_SQL_TK_AND:
    case DDS_SQL_TK_NOT:
    case DDS_SQL_TK_REM:
    case DDS_SQL_TK_PLUS:
    case DDS_SQL_TK_STAR:
    case DDS_SQL_TK_SLASH:
    case DDS_SQL_TK_MINUS:
    case DDS_SQL_TK_UMINUS:
    case DDS_SQL_TK_UPLUS:
      aff = DDS_SQL_AFFINITY_NUMERIC;
      break;
    case DDS_SQL_TK_BITOR:
    case DDS_SQL_TK_RSHIFT:
    case DDS_SQL_TK_LSHIFT:
    case DDS_SQL_TK_BITAND:
    case DDS_SQL_TK_BITNOT:
      aff = DDS_SQL_AFFINITY_INTEGER;
      break;
    case DDS_SQL_TK_ESCAPE:
    case DDS_SQL_TK_CONCAT:
    case DDS_SQL_TK_LIKE_KW:
    case DDS_SQL_TK_COLLATE:
      aff = DDS_SQL_AFFINITY_TEXT;
      break;
    case DDS_SQL_TK_EQ:
    case DDS_SQL_TK_NE:
    case DDS_SQL_TK_LT:
    case DDS_SQL_TK_GT:
    case DDS_SQL_TK_LE:
    case DDS_SQL_TK_GE:
    case DDS_SQL_TK_DOT:
    case DDS_SQL_TK_BETWEEN:
    default:
      aff = DDS_SQL_AFFINITY_NONE;
      break;
  }
  return aff;
}

static int get_op_assoc (const int op_token)
{
  int assoc = 0;
  switch (op_token)
  {
    case DDS_SQL_TK_UMINUS:
    case DDS_SQL_TK_UPLUS:
    case DDS_SQL_TK_BITNOT:
    case DDS_SQL_TK_ESCAPE:
    case DDS_SQL_TK_NOT:
      assoc = DDS_SQL_ASSOC_RIGHT;
      break;
    case DDS_SQL_TK_COLLATE:
    case DDS_SQL_TK_STAR:
    case DDS_SQL_TK_SLASH:
    case DDS_SQL_TK_REM:
    case DDS_SQL_TK_PLUS:
    case DDS_SQL_TK_MINUS:
    case DDS_SQL_TK_BITAND:
    case DDS_SQL_TK_BITOR:
    case DDS_SQL_TK_LSHIFT:
    case DDS_SQL_TK_RSHIFT:
    case DDS_SQL_TK_GT:
    case DDS_SQL_TK_LE:
    case DDS_SQL_TK_LT:
    case DDS_SQL_TK_GE:
    case DDS_SQL_TK_LIKE_KW:
    case DDS_SQL_TK_BETWEEN:
    case DDS_SQL_TK_NE:
    case DDS_SQL_TK_EQ:
    case DDS_SQL_TK_AND:
    case DDS_SQL_TK_OR:
    case DDS_SQL_TK_DOT:
      assoc = DDS_SQL_ASSOC_LEFT;
      break;
    default:
      assoc = -1;
      break;
  }
  return assoc;
}

static int get_op_precedence(const int op_token)
{
  int prec = -1;
  switch (op_token)
  {
    case DDS_SQL_TK_DOT:
      prec = 12;
      break;
    /* FIXME: are you sure about u-operator precedence? */
    case DDS_SQL_TK_UMINUS:
    case DDS_SQL_TK_UPLUS:
      prec = 11;
      break;
    case DDS_SQL_TK_BITNOT:
      prec = 10;
      break;
    case DDS_SQL_TK_COLLATE:
      prec = 9;
      break;
    case DDS_SQL_TK_STAR:
    case DDS_SQL_TK_SLASH:
    case DDS_SQL_TK_REM:
      prec = 8;
      break;
    case DDS_SQL_TK_PLUS:
    case DDS_SQL_TK_MINUS:
      prec = 7;
      break;
    case DDS_SQL_TK_BITAND:
    case DDS_SQL_TK_BITOR:
    case DDS_SQL_TK_LSHIFT:
    case DDS_SQL_TK_RSHIFT:
      prec = 6;
      break;
    case DDS_SQL_TK_ESCAPE:
      prec = 5;
      break;
    case DDS_SQL_TK_GT:
    case DDS_SQL_TK_LE:
    case DDS_SQL_TK_LT:
    case DDS_SQL_TK_GE:
      prec = 4;
      break;
    case DDS_SQL_TK_LIKE_KW:
    case DDS_SQL_TK_BETWEEN:
    case DDS_SQL_TK_NE:
    case DDS_SQL_TK_EQ:
      prec = 3;
      break;
    case DDS_SQL_TK_NOT:
      prec = 2;
      break;
    case DDS_SQL_TK_AND:
      prec = 1;
      break;
    case DDS_SQL_TK_OR:
      prec = 0;
      break;
    default:
      prec = -1;
      break;
  }
  return prec;
}

int dds_sql_get_numeric(void *blob, const unsigned char **c, int *tk, int tk_sz)
{
  assert (tk_sz >= 0);
  const unsigned char *cur = *c;
  char *s = (char *)cur;
  int prev_token = *tk; // keep it's only for double-validation later.
#ifdef NDEBUG
  (void) prev_token;
#endif
  if (*tk == DDS_SQL_TK_QNUMBER)
  {
    /* calculate resulted string size to avoid keep unused space */
    unsigned amount = 0;
    for (int i = 0; i < tk_sz; i++)
      amount += (cur[i] == DIGIT_SEPARATOR);
    unsigned int act_size = (unsigned)tk_sz - amount + 1;

    char *tmp_buff = (char *)malloc(act_size);
    for (int i = 0, j = 0; i < tk_sz; i++)
      if (cur[i] != DIGIT_SEPARATOR)
        tmp_buff[j++] = (char)cur[i];
    tmp_buff[act_size-1] = '\0';
    /* re-resolve the token. */
    int new_sz = dds_sql_get_token((const unsigned char *)tmp_buff, tk);
#ifdef NDEBUG
    (void) new_sz;
#endif
    assert (new_sz >= 0 && (act_size-1) == (unsigned)new_sz);
    s = tmp_buff;
  }

  assert (s);
  if (*tk == DDS_SQL_TK_INTEGER)
    *(int64_t*)blob = strtoll(s, NULL, 0);
  else if (*tk == DDS_SQL_TK_FLOAT)
    *(double *)blob = strtod(s, NULL);
  else
    abort(); /* something really wrong happen! */

  if (s != (char *)cur) {
    assert(prev_token == DDS_SQL_TK_QNUMBER);
    free(s);
  }
  return 0;
}

static inline uint8_t hex_to_int(int h)
{
  /* thanks to sqlite such a nice source code + proper documentation! */
  h += 9*(1&(h>>6));
  return (uint8_t)(h & 0xf);
}

int dds_sql_get_string(void **blob, const unsigned char **c, const int *tk, int tk_sz)
{
  /* assert (*blob == NULL); */
  assert (tk_sz >= 0);
  const unsigned char *cur = *c;
  int result_size = 0;
  switch (*tk)
  {
    case DDS_SQL_TK_ID:
    {
      unsigned int act_size = (cur[0] == '`' || cur[0] == '\"')? (unsigned)tk_sz - 2: (unsigned)tk_sz;
      *blob = malloc(act_size + 1);
      (void) memcpy(*blob, cur + (act_size < (unsigned)tk_sz), act_size);
      ((char *)*blob)[act_size] = '\0';
      result_size = (int)act_size;
      break;
    }
    case DDS_SQL_TK_STRING:
    {
      assert (tk_sz >= 2); /* empty strings are string too! */
      unsigned int act_size = ((unsigned)tk_sz - 2) + 1;
      *blob = malloc(act_size);
      (void) memcpy(*blob, cur+1, act_size - 1);
      ((char *)*blob)[act_size - 1] = '\0';
      result_size = (int)act_size;
      break;
    }
    case DDS_SQL_TK_BLOB:
    {
      assert (tk_sz >= 3);
      unsigned int act_size = ((unsigned)tk_sz - 3) / 2;
      *blob = malloc(act_size);
      for (int i = 0; i < tk_sz-3; i+=2)
        ((char *)*blob)[i/2] = (char)((hex_to_int((cur+2)[i])<<4) | hex_to_int((cur+2)[i+1]));
      result_size = (int)act_size;
      break;
    }
  }
  assert (*blob != NULL);
  return result_size;
}

static int cast(const void *in, const int in_sz, const int from_tk, const int to_tk, void **out)
{
  assert (in != NULL);
  int result_size = 0;
  assert ( to_tk == DDS_SQL_TK_STRING
        || to_tk == DDS_SQL_TK_INTEGER
        || to_tk == DDS_SQL_TK_FLOAT
        || to_tk == DDS_SQL_TK_BLOB
         );

  if (from_tk == to_tk) {
    /* don't waste resources */
    goto exit;
  }

  switch (to_tk)
  {
    case DDS_SQL_TK_STRING:
    {
      if (from_tk == DDS_SQL_TK_INTEGER || from_tk == DDS_SQL_TK_FLOAT) {
        char *fmt = (from_tk == DDS_SQL_TK_INTEGER)? "%li": "%e";
        if (from_tk == DDS_SQL_TK_INTEGER)
          result_size = snprintf(NULL, 0, fmt, *((const int64_t *)in));
        else
          result_size = snprintf(NULL, 0, fmt, *((const double *)in));

        result_size += 1; // don't forget about '\0'!
        *out = (char *) malloc((unsigned) result_size);
        if (from_tk == DDS_SQL_TK_INTEGER)
          (void) snprintf (*out, (unsigned) result_size, fmt, *((const int64_t *)in));
        else
          (void) snprintf (*out, (unsigned) result_size, fmt, *((const double *)in));

      } else if (from_tk == DDS_SQL_TK_BLOB) {
        result_size = in_sz + 1;
        *out = (char *) malloc((unsigned) result_size);
        (void) memcpy(*out, in, (unsigned) in_sz);
        ((char *)*out)[in_sz] = '\0';
      }
      break;
    }
    case DDS_SQL_TK_BLOB:
    {
      char *str = NULL;
      int str_sz = 0;
      if (from_tk == DDS_SQL_TK_INTEGER || from_tk == DDS_SQL_TK_FLOAT) {
        str_sz = cast(in, in_sz, from_tk, DDS_SQL_TK_STRING, (void **)&str);
        assert (str_sz != 0);
      } else {
        str = (char *)in;
        str_sz = in_sz - 1;
      }
      result_size = str_sz;
      *out = (unsigned char *) malloc((unsigned) result_size);
      (void) memcpy(*out, str, (unsigned) result_size);
      if (str != in)
        free (str);
      break;
    }
    case DDS_SQL_TK_INTEGER:
    case DDS_SQL_TK_FLOAT:
    {
      if (from_tk == DDS_SQL_TK_BLOB || from_tk == DDS_SQL_TK_STRING)
      {
        char *str = NULL;
        int str_sz = 0;
        if (from_tk == DDS_SQL_TK_BLOB)
        {
          str_sz = cast(in, in_sz, from_tk, DDS_SQL_TK_STRING, (void**)&str);
          assert (str_sz != 0);
        } else {
          str = (char *) in;
          str_sz = in_sz;
        }
        result_size = sizeof(int64_t);
        *out = malloc(sizeof(int64_t));
        int token = to_tk;
        int res = dds_sql_get_numeric(*out, (const unsigned char **)&str, &token, str_sz);
        /* token may not be equal to 'requested' to_tk */
        if (token != to_tk && (res == 0))
        {
          char *tmp = NULL;
          res = cast(out, result_size, token, to_tk, (void **)&tmp);
          assert (res == result_size);
          (void) memcpy (out, tmp, (unsigned) res);
          free (tmp);
        }
        assert (res == 0);
        if (from_tk == DDS_SQL_TK_BLOB)
          free (str);
      } else {
        assert (from_tk == DDS_SQL_TK_INTEGER || from_tk == DDS_SQL_TK_FLOAT);
        result_size = sizeof(int64_t);
        *out = malloc(sizeof(int64_t));
        // we previously reject case operation in case of `to_tk == from_tk`,
        // so we know all conversion kinds
        if (to_tk == DDS_SQL_TK_INTEGER)
          *(*(int64_t **)out) = (int64_t)*(double *)in;
        else if (to_tk == DDS_SQL_TK_FLOAT)
          *(*(double **)out)  = (double)*(int64_t *)in;
      }
      break;
    }
    default:
      abort();
  }

  assert (*out != NULL);
exit:
  return result_size;
}

int dds_sql_apply_affinity(struct dds_sql_token *st, const int aff)
{
  int result_aff = aff;
  if (aff == DDS_SQL_AFFINITY_NONE || st->aff == aff)
  { /* NONE doesn't require anything to do, it's just indicate that ANY type is
     * valid (the same applies for EQ ex->aff and aff) */
    result_aff = st->aff;
    goto exit;
  }

  if (st->aff < aff)
  {
    if (st->aff >= DDS_SQL_AFFINITY_NUMERIC)
    {
      assert ( st->tok == DDS_SQL_TK_FLOAT
            || st->tok == DDS_SQL_TK_INTEGER);

      int token = 0;
      if (aff == DDS_SQL_AFFINITY_INTEGER)
        token = DDS_SQL_TK_INTEGER;
      else if (aff == DDS_SQL_AFFINITY_REAL)
        token = DDS_SQL_TK_FLOAT;

      char *out = NULL;
      int res = cast(&st->n, sizeof(st->n), st->tok, token, (void**)&out);
      (void) memcpy (&st->n, out, (unsigned) res);
      free (out);
      st->tok = token;

    } else if (st->aff != DDS_SQL_AFFINITY_NONE) {
      assert ( st->tok == DDS_SQL_TK_BLOB
            || st->tok == DDS_SQL_TK_STRING );
      if (st->aff == DDS_SQL_AFFINITY_BLOB)
      { /* BLOB always cast to string before any futher cast. */
        char *out = NULL;
        int res = cast(st->s, (int)st->n.i, st->tok, DDS_SQL_TK_STRING, (void**)&out);
        free (st->s);
        st->s = out;
        st->n.i = res;
        st->tok = DDS_SQL_TK_STRING;
      }

      if (aff == DDS_SQL_AFFINITY_TEXT)
      {
        result_aff = DDS_SQL_AFFINITY_TEXT;
        goto exit;
      }

      if (aff == DDS_SQL_AFFINITY_NUMERIC)
      { /* determine result token DDS_SQL_TK_INTEGER/TK_FLOAT*/
        int token = 0;
        const unsigned char *cursor = (const unsigned char *)st->s;

        /* NOTE: try to obtain both numeric kinds (TK_QNUMBER not supported here)*/
        char nums[2][8] = {{0}, {0}};
        int tokens[2] = {DDS_SQL_TK_INTEGER, DDS_SQL_TK_FLOAT};
        int res = 0;
        for (unsigned i = 0; i < sizeof(tokens)/sizeof(tokens[0]) && !res; i++)
          res = dds_sql_get_numeric(nums[i], &cursor, &tokens[i], 0);

        token = (*((double*)nums[1]) <= (double)*((int64_t*)nums[0])) ? DDS_SQL_TK_INTEGER: DDS_SQL_TK_FLOAT;
        result_aff = (token == DDS_SQL_TK_INTEGER)? DDS_SQL_AFFINITY_INTEGER: DDS_SQL_AFFINITY_REAL;
      }

      int token = 0;
      if (result_aff == DDS_SQL_AFFINITY_INTEGER)
        token = DDS_SQL_TK_INTEGER;
      else if (result_aff == DDS_SQL_AFFINITY_REAL)
        token = DDS_SQL_TK_FLOAT;

      char *out = NULL;
      int res = cast(st->s, (int)st->n.i, st->tok, token, (void**)&out);
      free (st->s); st->s = NULL;
      assert (res == sizeof(int64_t));
      (void) memcpy(&st->n, out, (unsigned) res);
      free (out);
      st->tok = token;
    } else /* ID? or VAR? */ {
      result_aff = -1;
      goto exit;
    }
  }
  else if (st->aff > aff)
  {
    if (aff > DDS_SQL_AFFINITY_NUMERIC)
    { /* REAL to INTEGER forbidden, even if it's possible to do without data
       * lose. */
      result_aff = -1;
      goto exit;
    }
    else if (aff == DDS_SQL_AFFINITY_NUMERIC)
    { /* it's nice to be there, since no any conversion needed. */
      result_aff = st->aff;
      /* FIXME: assert (st->tok == DDS_SQL_TK_REAL || st->tok ==
       * DDS_SQL_TK_INTEGER) */
      goto exit;
    }

    if (st->aff > DDS_SQL_AFFINITY_NUMERIC)
    { /* REAL or INTEGER to BLOB or STRING */
      int res = cast(&st->n, sizeof(st->n), st->tok, DDS_SQL_TK_STRING, (void**)&st->s);
      st->n.i = res;
      st->tok = DDS_SQL_TK_STRING;
    }

    if (aff == DDS_SQL_AFFINITY_BLOB) {
      char *out = NULL;
      int res = cast(st->s, (int)st->n.i, st->tok, DDS_SQL_TK_BLOB, (void**)&out);
      free (st->s);
      st->s = out;
      st->n.i = res;
      st->tok = DDS_SQL_TK_BLOB;
    }

    result_aff = aff;
  }

exit:
  return result_aff;
}

typedef int (*op_callback_func_t)(struct dds_sql_token **op, const struct dds_sql_token *lhs,  const struct dds_sql_token *rhs);

static int bitnot_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs,  const struct dds_sql_token *rhs)
{
#ifdef NDEBUG
  (void) lhs;
#endif
  assert (lhs == NULL);
  assert (rhs != NULL && rhs->aff == DDS_SQL_AFFINITY_INTEGER);
  (*op)->n.i = ~(rhs->n.i);
  return  0;
}

static int bitand_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs,  const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff == DDS_SQL_AFFINITY_INTEGER);
  assert (rhs != NULL && rhs->aff == DDS_SQL_AFFINITY_INTEGER);
  (*op)->n.i = (lhs->n.i) & (rhs->n.i);
  return 0;
}

static int bitor_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff == DDS_SQL_AFFINITY_INTEGER);
  assert (rhs != NULL && rhs->aff == DDS_SQL_AFFINITY_INTEGER);
  (*op)->n.i = (lhs->n.i) | (rhs->n.i);
  return 0;
}

static int lshift_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff == DDS_SQL_AFFINITY_INTEGER);
  assert (rhs != NULL && rhs->aff == DDS_SQL_AFFINITY_INTEGER);
  (*op)->n.i = (lhs->n.i) << (rhs->n.i);
  return 0;
}

static int rshift_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff == DDS_SQL_AFFINITY_INTEGER);
  assert (rhs != NULL && rhs->aff == DDS_SQL_AFFINITY_INTEGER);
  (*op)->n.i = (lhs->n.i) >> (rhs->n.i);
  return 0;
}

static int not_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
#ifdef NDEBUG
  (void) lhs;
#endif
  assert (lhs == NULL);
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  (*op)->n.i = !(rhs->aff == DDS_SQL_AFFINITY_INTEGER? rhs->n.i: (rhs->n.r > .0 || rhs->n.r < .0));
  return 0;
}

static int and_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  (*op)->n.i = (lhs->aff == DDS_SQL_AFFINITY_INTEGER? lhs->n.i: (lhs->n.r > .0 || lhs->n.r < .0))
            && (rhs->aff == DDS_SQL_AFFINITY_INTEGER? rhs->n.i: (rhs->n.r > .0 || rhs->n.r < .0));
  return 0;
}

static int or_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  (*op)->n.i = (lhs->aff == DDS_SQL_AFFINITY_INTEGER? lhs->n.i: (lhs->n.r > .0 || lhs->n.r < .0))
            || (rhs->aff == DDS_SQL_AFFINITY_INTEGER? rhs->n.i: (rhs->n.r > .0 || rhs->n.r < .0));
  return 0;
}

static int minus_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  if (lhs != NULL)
  {
    assert (lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
    if (rhs->aff == DDS_SQL_AFFINITY_INTEGER)
      (*op)->n.i = (lhs->n.i) - (rhs->n.i);
    else if (rhs->aff == DDS_SQL_AFFINITY_REAL)
      (*op)->n.r = (lhs->n.r) - (rhs->n.r);
  } else {
    if (rhs->aff == DDS_SQL_AFFINITY_INTEGER)
      (*op)->n.i = (-1) * (rhs->n.i);
    else if (rhs->aff == DDS_SQL_AFFINITY_REAL)
      (*op)->n.r = (-1) * (rhs->n.r);
  }
  return 0;
}

static int plus_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  if (lhs != NULL)
  {
    assert (lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
    if (rhs->aff == DDS_SQL_AFFINITY_INTEGER)
      (*op)->n.i = (lhs->n.i) + (rhs->n.i);
    else if (rhs->aff == DDS_SQL_AFFINITY_REAL)
      (*op)->n.r = (lhs->n.r) + (rhs->n.r);
  } else {
    (*op)->n = (rhs->n);
  }
  return 0;
}

static int star_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  if (lhs->aff == DDS_SQL_AFFINITY_INTEGER)
    (*op)->n.i = (lhs->n.i) * (rhs->n.i);
  else if (lhs->aff == DDS_SQL_AFFINITY_REAL)
    (*op)->n.r = (lhs->n.r) * (rhs->n.r);
  return 0;
}

static int slash_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  if (lhs->aff == DDS_SQL_AFFINITY_INTEGER)
    (*op)->n.i = (lhs->n.i) / (rhs->n.i);
  else if (lhs->aff == DDS_SQL_AFFINITY_REAL)
    (*op)->n.r = (lhs->n.r) / (rhs->n.r);
  return 0;
}

static int rem_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL && lhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  assert (rhs != NULL && rhs->aff > DDS_SQL_AFFINITY_NUMERIC);
  if (lhs->aff == DDS_SQL_AFFINITY_INTEGER)
    (*op)->n.i = (lhs->n.i) % (rhs->n.i);
  else if (lhs->aff == DDS_SQL_AFFINITY_REAL)
    (*op)->n.r = lhs->n.r - ((int32_t)((lhs->n.r) / (rhs->n.r))*(rhs->n.r));
  return 0;
}

static int eq_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL);
  assert (rhs != NULL);

  bool result = 0;
  if (lhs->aff > DDS_SQL_AFFINITY_NUMERIC)
  {
    if (lhs->aff == DDS_SQL_AFFINITY_INTEGER)
      result = lhs->n.i == rhs->n.i;
    else if (lhs->aff == DDS_SQL_AFFINITY_REAL)
      result = fabs(lhs->n.r - rhs->n.r) < REAL_PREC;
  } else if (lhs->aff == DDS_SQL_AFFINITY_TEXT || lhs->aff == DDS_SQL_AFFINITY_BLOB) {
    result = (lhs->n.i == rhs->n.i) && !memcmp (lhs->s, rhs->s, (unsigned) lhs->n.i);
  } else {
    abort();
  }
  (*op)->n.i = result;

  return 0;
}

static int ne_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  int res = eq_op_callback(op, lhs, rhs);
#ifdef NDEBUG
  (void) res;
#endif
  assert (res == 0);
  (*op)->n.i = !(*op)->n.i;
  return 0;
}

static int lt_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL);
  assert (rhs != NULL);

  bool result = 0;
  if (lhs->aff > DDS_SQL_AFFINITY_NUMERIC)
  {
    if (lhs->aff == DDS_SQL_AFFINITY_INTEGER)
      result = lhs->n.i < rhs->n.i;
    else if (lhs->aff == DDS_SQL_AFFINITY_REAL)
      result = lhs->n.r < rhs->n.r;
  } else if (lhs->aff == DDS_SQL_AFFINITY_TEXT || lhs->aff == DDS_SQL_AFFINITY_BLOB) {
    result = memcmp(lhs->s, rhs->s, (unsigned) ((lhs->n.i < rhs->n.i)? lhs->n.i: rhs->n.i)) < 0;
  } else {
    abort();
  }
  (*op)->n.i = result;

  return 0;
}

static int gt_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (lhs != NULL);
  assert (rhs != NULL);

  bool result = 0;
  if (lhs->aff > DDS_SQL_AFFINITY_NUMERIC)
  {
    if (lhs->aff == DDS_SQL_AFFINITY_INTEGER)
      result = lhs->n.i > rhs->n.i;
    else if (lhs->aff == DDS_SQL_AFFINITY_REAL)
      result = lhs->n.r > rhs->n.r;
  } else if (lhs->aff == DDS_SQL_AFFINITY_TEXT || lhs->aff == DDS_SQL_AFFINITY_BLOB) {
    result = memcmp(lhs->s, rhs->s, (unsigned) ((lhs->n.i < rhs->n.i)? lhs->n.i: rhs->n.i)) > 0;
  } else {
    abort();
  }
  (*op)->n.i = result;

  return 0;
}

static int le_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  int ret = 0;
  if ((ret = eq_op_callback(op, lhs, rhs)) == 0 && !(*op)->n.i)
    ret = lt_op_callback(op, lhs, rhs);
  return ret;
}

static int ge_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  int ret = 0;
  if ((ret = eq_op_callback(op, lhs, rhs)) == 0 && !(*op)->n.i)
    ret = gt_op_callback(op, lhs, rhs);
  return ret;
}

static int dot_op_callback(struct dds_sql_token **op, const struct dds_sql_token *lhs, const struct dds_sql_token *rhs)
{
  assert (rhs != NULL && rhs->aff == DDS_SQL_AFFINITY_NONE);
  assert (rhs->tok == DDS_SQL_TK_ID);
  char *res_id = NULL;
  int64_t res_sz = 0U;
  if (lhs != NULL)
  {
    assert (lhs != NULL && lhs->aff == DDS_SQL_AFFINITY_NONE);
    assert (lhs->tok == DDS_SQL_TK_ID);
    res_sz = lhs->n.i + rhs->n.i + 1U;
    res_id = calloc(1, (size_t)(res_sz + 1U)); //strlen(l|r) + '\0'
    (void) sprintf (res_id, "%s.%s", lhs->s, rhs->s);
  } else {
    res_sz = rhs->n.i;
    res_id = ddsrt_strdup(rhs->s);
  }
  (*op)->s = res_id;
  (*op)->n.i = res_sz;

  return 0;
}

static int get_op_callback(const int op_token, op_callback_func_t *fn)
{
  int result = 0;
  switch (op_token)
  {
    case DDS_SQL_TK_DOT:
      *fn = (op_callback_func_t)dot_op_callback;
      break;
    case DDS_SQL_TK_BITNOT:
      *fn = (op_callback_func_t)bitnot_op_callback;
      break;
    case DDS_SQL_TK_BITAND:
      *fn = (op_callback_func_t)bitand_op_callback;
      break;
    case DDS_SQL_TK_BITOR:
      *fn = (op_callback_func_t)bitor_op_callback;
      break;
    case DDS_SQL_TK_LSHIFT:
      *fn = (op_callback_func_t)lshift_op_callback;
      break;
    case DDS_SQL_TK_RSHIFT:
      *fn = (op_callback_func_t)rshift_op_callback;
      break;
    case DDS_SQL_TK_NOT:
      *fn = (op_callback_func_t)not_op_callback;
      break;
    case DDS_SQL_TK_AND:
      *fn = (op_callback_func_t)and_op_callback;
      break;
    case DDS_SQL_TK_OR:
      *fn = (op_callback_func_t)or_op_callback;
      break;
    case DDS_SQL_TK_UMINUS:
    case DDS_SQL_TK_MINUS:
      *fn = (op_callback_func_t)minus_op_callback;
      break;
    case DDS_SQL_TK_UPLUS:
    case DDS_SQL_TK_PLUS:
      *fn = (op_callback_func_t)plus_op_callback;
      break;
    case DDS_SQL_TK_STAR:
      *fn = (op_callback_func_t)star_op_callback;
      break;
    case DDS_SQL_TK_SLASH:
      *fn = (op_callback_func_t)slash_op_callback;
      break;
    case DDS_SQL_TK_REM:
      *fn = (op_callback_func_t)rem_op_callback;
      break;
    case DDS_SQL_TK_EQ:
      *fn = (op_callback_func_t)eq_op_callback;
      break;
    case DDS_SQL_TK_NE:
      *fn = (op_callback_func_t)ne_op_callback;
      break;
    case DDS_SQL_TK_LT:
      *fn = (op_callback_func_t)lt_op_callback;
      break;
    case DDS_SQL_TK_GT:
      *fn = (op_callback_func_t)gt_op_callback;
      break;
    case DDS_SQL_TK_LE:
      *fn = (op_callback_func_t)le_op_callback;
      break;
    case DDS_SQL_TK_GE:
      *fn = (op_callback_func_t)ge_op_callback;
      break;
    default:
      result = -1;
      break;
  }

  return result;
}

static int dds_sql_eval_op(struct dds_sql_token **op, struct dds_sql_token *lhs, struct dds_sql_token *rhs)
{
  int result = -1;
  int affinity = (*op)->aff;

  if (lhs == NULL && rhs == NULL)
    goto exit;

  /* NOTE: While most of the expression evaluation paths are similar to SQLite
   * behavior, here is exaclty the point where we start differentiating.
   * SQL have a term such as "dynamic" type, and "strict" type definition.
   * For example all "constants" are dynamic and don't strictly connected with
   * a type, this is not a story for explicit "cast" operation and type
   * definition on a table members "initialization". We are eval. all data
   * within it's type as it was defined in "strictly" typed table, that's why
   * the folowing expression:
   *
   *    SELECT '0.1' < 1;
   * will be equivalent of expression:
   *
   *    SELECT CAST('0.1' AS TEXT) < CAST(1 AS INTEGER);
   * otherwise user may be confused by different outputs of SQLite shell and
   * our internal implementation, since expressions above just follows the
   * rules of applying affinities, and return "True". While SQLite shell uses
   * custom rule for "non-strictly" typed constants and returns "False".
   *
   * P.S: In that case user should rely on MySQL-like behavior.
   * */

  int lhs_aff = DDS_SQL_AFFINITY_NONE, rhs_aff = DDS_SQL_AFFINITY_NONE;
  if (lhs && (lhs_aff = lhs->aff = dds_sql_apply_affinity(lhs, affinity)) < 0)
    goto exit;
  if (rhs && (rhs_aff = rhs->aff = dds_sql_apply_affinity(rhs, affinity)) < 0)
    goto exit;

  int aff_max = (rhs_aff > lhs_aff) ? rhs_aff : lhs_aff;

  if (lhs && (lhs->aff = dds_sql_apply_affinity(lhs, aff_max)) < 0)
    goto exit;
  if (rhs && (rhs->aff = dds_sql_apply_affinity(rhs, aff_max)) < 0)
    goto exit;

  op_callback_func_t callback = NULL;
  if ((result = get_op_callback((*op)->tok, &callback)) == 0
      && callback != NULL) {
    result = callback(op, lhs, rhs);
  }

exit:
  return result;
}

static dds_return_t dds_sql_token_init(struct dds_sql_token **token)
{
  struct dds_sql_token *t = malloc(sizeof(*t));
  t->tok = 0;
  t->aff = 0;
  t->asc = 0;
  t->prc = 0;
  t->s = NULL;
  (void) memset (&t->n, 0, sizeof(t->n));
  *token = t;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_sql_expr_node_init(struct dds_sql_expr_node **node)
{
  struct dds_sql_expr_node *n = malloc(sizeof(*n));
  n->token = NULL;
  n->height = 0;
  n->l = NULL;
  n->r = NULL;
  n->p = NULL;
  *node = n;
  return DDS_RETCODE_OK;
}

static void dds_sql_expr_node_fini(struct dds_sql_expr_node *node)
{
  if (node == NULL)
    return;

  if (node->token && node->token->tok > 0)
  {
    if (node->token->s != NULL)
      free (node->token->s);
    free (node->token);
  }
  dds_sql_expr_node_fini(node->l);
  dds_sql_expr_node_fini(node->r);
  free (node);
}

#define IS_VAR_TK(token) (token == DDS_SQL_TK_ID || token == DDS_SQL_TK_VARIABLE)

static int eval_expr(const unsigned char **s, int prec, struct dds_sql_token **exp, struct dds_sql_expr_node **node, struct ddsrt_hh **params)
{
  const unsigned char *cursor = *s;
  int token = 0;
  int token_sz = 0;
  int ret = 0;
  struct dds_sql_token *l_for = *exp;
  struct dds_sql_expr_node *opnode = NULL;

  do {
    if ((token_sz = get_op_token(&cursor, &token)) <= 0 || token < 0) {
      ret = -1;
      goto err;
    }
  } while (token == DDS_SQL_TK_COMMENT);

  switch (token)
  {
    case DDS_SQL_TK_LP:
    {
      if ((ret = eval_expr(&cursor, 0, &l_for, node, params)) < 0)
        goto err;
      token_sz = get_op_token(&cursor, &token);
      assert (token_sz > 0);
      assert (ret >= 0 && token == DDS_SQL_TK_RP);
      break;
    }
    case DDS_SQL_TK_QNUMBER:
    case DDS_SQL_TK_INTEGER:
    case DDS_SQL_TK_FLOAT:
    {
      const unsigned char *precursor = cursor - token_sz;
      if ((ret = dds_sql_get_numeric(&(l_for)->n, &precursor, &token, token_sz)) < 0)
        goto err;
      (l_for)->tok = token;
      (l_for)->aff = (token == DDS_SQL_TK_INTEGER)? DDS_SQL_AFFINITY_INTEGER: DDS_SQL_AFFINITY_REAL;
      break;
    }
    case DDS_SQL_TK_STRING:
    case DDS_SQL_TK_BLOB:
    case DDS_SQL_TK_ID:
    {
      const unsigned char *precursor = cursor - token_sz;
      if ((ret = dds_sql_get_string((void **)&(l_for->s), &precursor, &token, token_sz)) <= 0)
        goto err;

      (l_for)->tok = token;
      (l_for)->n.i = ret;
      (l_for)->aff = (token == DDS_SQL_TK_STRING)? DDS_SQL_AFFINITY_TEXT:
                     (token == DDS_SQL_TK_BLOB)?   DDS_SQL_AFFINITY_BLOB:
                                                   DDS_SQL_AFFINITY_NONE;
      if (l_for->tok == DDS_SQL_TK_ID)
      {
        ret = dds_sql_expr_node_init(node);
        assert (ret == DDS_RETCODE_OK);
        (*node)->token = l_for;
      }
      break;
    }
    case DDS_SQL_TK_VARIABLE:
    {
      const unsigned char *precursor = cursor - token_sz + 1;
      int tok = DDS_SQL_TK_INTEGER;
      if ((ret = dds_sql_get_numeric(&(l_for)->n, &precursor, &tok, token_sz - 1)) < 0)
        goto err;
      assert (tok == DDS_SQL_TK_INTEGER);
      (l_for)->tok = token;
      (l_for)->aff = DDS_SQL_AFFINITY_NONE;
      ret = dds_sql_expr_node_init(node);
      assert (ret == DDS_RETCODE_OK);
      struct dds_sql_param *param = NULL;
      struct dds_sql_param tmpl = {.token = *l_for, .id.index = (uint32_t)l_for->n.i, .tok = token};
      if ((param = ddsrt_hh_lookup(*params, &tmpl)) == NULL)
      {
        param = malloc(sizeof(*param));
        (void) memcpy(&param->token, &tmpl.token, sizeof(param->token));
        param->id = tmpl.id;
        param->tok = tmpl.tok;
        ddsrt_hh_add(*params, param);
      }
      (void) memset (*exp, 0, sizeof(**exp));
      (*node)->token = (struct dds_sql_token *)param;
      l_for = (*node)->token;
      break;
    }
    default:
    {
      /* "default" case possible under 2 conditions only:
       * (DDS_SQL_ASSOC_RIGHT of the operator) || (UNARY operation)*/
      assert (get_op_assoc(token) == DDS_SQL_ASSOC_RIGHT
          || (get_op_precedence(token) == 7));
      l_for = NULL;
      if (token == DDS_SQL_TK_PLUS || token == DDS_SQL_TK_MINUS)
      {
        token = (token == DDS_SQL_TK_MINUS)? DDS_SQL_TK_UMINUS: DDS_SQL_TK_UPLUS;
        ret = dds_sql_expr_node_init(&opnode);
        assert (ret == DDS_RETCODE_OK);
        goto enter;
      } else {
        cursor -= token_sz;
      }
      break;
    }
  }


  ret = dds_sql_expr_node_init(&opnode);
  assert (ret == DDS_RETCODE_OK);
  while (((token_sz = get_op_token((const unsigned char **)&cursor, &token))
       && (get_op_precedence(token) >= prec)))
enter:
  {
    int op_prc = get_op_precedence(token);
    struct dds_sql_token *op = NULL;
    ret = dds_sql_token_init(&op);
    assert (ret == DDS_RETCODE_OK);
    op->tok = token;
    op->aff = get_op_affinity(token);
    op->prc = op_prc;
    op->asc = get_op_assoc(token);

    if (*node != NULL)
    {
      /* let "TK_MINUS" fall-through inside inner expression as "UNARY", since
       * we not able to evaluate operation on a fly (cause "*node" != NULL
       * indicate about "VARIABLE"|"ID" existance) */
      if (token == DDS_SQL_TK_MINUS)
      {
        op->tok = DDS_SQL_TK_PLUS;
        op_prc = prec;
        cursor -= token_sz;
      }
    }
    opnode->token = op;
    opnode->l = *node;
    struct dds_sql_token *rhs = NULL;
    ret = dds_sql_token_init(&rhs);
    assert (ret == DDS_RETCODE_OK);
    if ((ret = eval_expr(&cursor, (op_prc + (op->asc == DDS_SQL_ASSOC_LEFT? 1: 0)), &rhs, &opnode->r, params)) < 0)
    {
      *s = cursor;
      goto err;
    }

    if (rhs->tok == DDS_SQL_TK_ID && (l_for == NULL || l_for->tok == DDS_SQL_TK_ID) && op->tok == DDS_SQL_TK_DOT) {
      ; /* better to handle nested ID access on that stage, then after. */
    } else if (l_for && !IS_VAR_TK(l_for->tok) && l_for->prc == op->prc && opnode->r == NULL) {
      ; /* we are finally have all conditions in place for operation to evaluate! */
    } else {
      if (rhs->tok == 0) {
        free (rhs); rhs = NULL;
      }

      if (opnode->l != NULL)
      {
        size_t height = opnode->l->height;
        if (opnode->r == NULL && rhs != NULL)
        {
          ret = dds_sql_expr_node_init(&opnode->r);
          assert (ret == DDS_RETCODE_OK);
          opnode->r->token = rhs;
          l_for = opnode->r->token;
          l_for->prc = opnode->token->prc;
        } else {
          assert (opnode->r != NULL);
          height = (opnode->r->height > height)? opnode->r->height: height;
        }

        *node = opnode;
        (*node)->height = height + 1;
        ret = dds_sql_expr_node_init(&opnode->p);
        assert (ret == DDS_RETCODE_OK);
        opnode = opnode->p;
        continue;
      }

      if (opnode->r != NULL)
      {
        size_t height = opnode->r->height;
        if (opnode->l == NULL && l_for && l_for->tok != DDS_SQL_TK_ID && l_for->tok != DDS_SQL_TK_VARIABLE)
        {
          ret = dds_sql_expr_node_init(&opnode->l);
          assert (ret == DDS_RETCODE_OK);
          opnode->l->token = l_for;
          l_for = opnode->l->token; l_for->prc = opnode->token->prc;
        } else if (opnode->l != NULL) {
          height = (opnode->l->height > height)? opnode->l->height: height;
        }

        *node = opnode;
        (*node)->height = height + 1;
        ret = dds_sql_expr_node_init(&opnode->p);
        assert (ret == DDS_RETCODE_OK);
        opnode = opnode->p;
        continue;
      }
    }

    assert (rhs != NULL);
    if ((ret = dds_sql_eval_op(&op, l_for, rhs)) < 0)
    {
      *s = cursor;
      goto err;
    }

    if (opnode->token->tok == DDS_SQL_TK_DOT && opnode->r)
      free(opnode->r);

    /* since "l_for" should alway point to available memory to store evaluation
     * result, in case of "RIGHT_ASSOC"&"UNARY" operators eval. we should
     * update "l_for" for that purpouse.*/
    if (l_for == NULL)
    {
      l_for = *exp;
      l_for->aff = rhs->aff;
      l_for->tok = rhs->tok;
    }


    switch (op->aff)
    {
      /* FIXME: not all NONE affinity operators store result in 'n' */
      case DDS_SQL_AFFINITY_NONE:
      {
        if (rhs->aff == DDS_SQL_AFFINITY_BLOB || rhs->aff == DDS_SQL_AFFINITY_TEXT) {
          if (rhs->s) {
            free(rhs->s);
            rhs->s = NULL;
          }
        } else if (rhs->aff == DDS_SQL_AFFINITY_NONE) {
          if (l_for->s) free (l_for->s);
          l_for->s = op->s;
        }
      } /* fall through */
      case DDS_SQL_AFFINITY_NUMERIC:
      case DDS_SQL_AFFINITY_INTEGER:
      case DDS_SQL_AFFINITY_REAL:
      {
        l_for->n = op->n;
        break;
      }
      case DDS_SQL_AFFINITY_BLOB:
      case DDS_SQL_AFFINITY_TEXT:
      {
        if (l_for->s != NULL) free (l_for->s);
        l_for->n = op->n;
        l_for->s = op->s;
        break;
      }
      default:
        abort();
    }
    free (opnode->token);
    opnode->token = NULL;
    if (rhs->s != NULL) free (rhs->s);
    free (rhs);
  }

  /* in case of unupdated operation on "opnode", return latest meaningful node
   * pointed by "*node".*/
  if (opnode->token == NULL)
     free (opnode);

  *s = cursor - token_sz;
err:
  return ret;
}

#undef IS_VAR_TK

static uint32_t param_hash_fn(const void *a)
{
  const struct dds_sql_param *item = (const struct dds_sql_param *)a;
  uint32_t x = ddsrt_mh3(&item->id, sizeof(item->id), 0);
  return x;
}

static bool param_equals_fn(const void *a, const void *b)
{
  const struct dds_sql_param *aa = (const struct dds_sql_param *)a;
  const struct dds_sql_param *bb = (const struct dds_sql_param *)b;
  return aa->id.index == bb->id.index;
}

static uint32_t variable_hash_fn(const void *a)
{
  const struct dds_sql_param *item = (const struct dds_sql_param *)a;
  uint32_t x = ddsrt_mh3(item->id.str, strlen(item->id.str), 0);
  return x;
}

static bool variable_equals_fn(const void *a, const void *b)
{
  const struct dds_sql_param *aa = (const struct dds_sql_param *)a;
  const struct dds_sql_param *bb = (const struct dds_sql_param *)b;
  return strcmp(aa->id.str, bb->id.str) == 0;
}

dds_return_t dds_sql_expr_init(struct dds_sql_expr **expr, enum dds_sql_expr_param_kind kind)
{
  if (expr == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  struct dds_sql_expr *e = (struct dds_sql_expr *)malloc(sizeof(*e));
  e->nparams = 0;
  e->param_kind = kind;
  if (kind == DDS_SQL_EXPR_KIND_PARAMETER)
    e->param_tokens = ddsrt_hh_new(1, param_hash_fn, param_equals_fn);
  else if (kind == DDS_SQL_EXPR_KIND_VARIABLE)
    e->param_tokens = ddsrt_hh_new(1, variable_hash_fn, variable_equals_fn);
  e->node = NULL;
  *expr = e;
  return DDS_RETCODE_OK;
}

static void param_disable_fn(void *vnode, void *varg)
{
  (void) varg;
  struct dds_sql_param *p = (struct dds_sql_param *) vnode;
  p->token.tok = -1;
}

static void param_fini_fn(void *vnode, void *varg)
{
  assert (varg != NULL);
  enum dds_sql_expr_param_kind *kind = (enum dds_sql_expr_param_kind *)varg;
  struct dds_sql_param *p = (struct dds_sql_param *) vnode;
  struct dds_sql_token *t = (struct dds_sql_token *) p;
  if (t->s != NULL) free (t->s);
  if (*kind == DDS_SQL_EXPR_KIND_VARIABLE) free(p->id.str);
  free (p);
}

void dds_sql_expr_fini(struct dds_sql_expr *expr)
{
  ddsrt_hh_enum(expr->param_tokens, param_disable_fn, NULL);
  dds_sql_expr_node_fini(expr->node);
  ddsrt_hh_enum(expr->param_tokens, param_fini_fn, &expr->param_kind);
  ddsrt_hh_free(expr->param_tokens);
  free (expr);
}

dds_return_t dds_sql_expr_parse(const unsigned char *s, struct dds_sql_expr **ex)
{
  const unsigned char *cursor = s;
  struct dds_sql_token *token = NULL;
  dds_return_t ret = dds_sql_token_init(&token);
  assert (ret == DDS_RETCODE_OK);
  if ((ret = eval_expr(&cursor, 0, &token, &(*ex)->node, &(*ex)->param_tokens)) < 0)
    goto err;
  if ((*ex)->node == NULL) {
    ret = dds_sql_expr_node_init(&(*ex)->node);
    assert (ret == DDS_RETCODE_OK);
    (*ex)->node->token = token;
  } else if (token->tok == 0) {
    free (token);
  }

  struct ddsrt_hh_iter it;
  for (dds_sql_param_t *tok = ddsrt_hh_iter_first((*ex)->param_tokens, &it); tok != NULL; tok = ddsrt_hh_iter_next(&it))
    (*ex)->nparams++;
err:
  return ret;
}

dds_return_t dds_sql_expr_bind_integer(const struct dds_sql_expr *ex, uintptr_t i, int64_t p)
{
  struct dds_sql_param tmpl = {0}; (void) memcpy(&tmpl.id, &i, sizeof(i));
  struct dds_sql_param *param = ddsrt_hh_lookup(ex->param_tokens, &tmpl);
  if (param == NULL) return DDS_RETCODE_BAD_PARAMETER;
  struct dds_sql_token *t = (struct dds_sql_token *)param;
  if (t->s != NULL) { free (t->s); t->s = NULL; }
  t->tok = DDS_SQL_TK_INTEGER;
  t->aff = DDS_SQL_AFFINITY_INTEGER;
  t->n.i = p;
  return DDS_RETCODE_OK;
}

dds_return_t dds_sql_expr_bind_real(const struct dds_sql_expr *ex, uintptr_t i, double p)
{
  struct dds_sql_param tmpl = {0}; (void) memcpy(&tmpl.id, &i, sizeof(i));
  struct dds_sql_param *param = ddsrt_hh_lookup(ex->param_tokens, &tmpl);
  if (param == NULL) return DDS_RETCODE_BAD_PARAMETER;
  struct dds_sql_token *t = (struct dds_sql_token *)param;
  if (t->s != NULL) { free (t->s); t->s = NULL; }
  t->tok = DDS_SQL_TK_FLOAT;
  t->aff = DDS_SQL_AFFINITY_REAL;
  t->n.r = p;
  return DDS_RETCODE_OK;
}

dds_return_t dds_sql_expr_bind_string(const struct dds_sql_expr *ex, uintptr_t i, char s[])
{
  struct dds_sql_param tmpl = {0}; (void) memcpy(&tmpl.id, &i, sizeof(i));
  struct dds_sql_param *param = ddsrt_hh_lookup(ex->param_tokens, &tmpl);
  if (param == NULL) return DDS_RETCODE_BAD_PARAMETER;
  struct dds_sql_token *t = (struct dds_sql_token *)param;
  if (t->s != NULL) { free (t->s); t->s = NULL; }
  t->tok = DDS_SQL_TK_STRING;
  t->aff = DDS_SQL_AFFINITY_TEXT;
  t->n.i = (int64_t) strlen(s);
  t->s = malloc(strlen(s) + 1);
  (void) strcpy(t->s, s);
  return DDS_RETCODE_OK;
}

dds_return_t dds_sql_expr_bind_blob(const struct dds_sql_expr *ex, uintptr_t i, unsigned char b[], size_t s)
{
  if (s >= INT64_MAX)
    return DDS_RETCODE_UNSUPPORTED;
  struct dds_sql_param tmpl = {0}; (void) memcpy(&tmpl.id, &i, sizeof(i));
  struct dds_sql_param *param = ddsrt_hh_lookup(ex->param_tokens, &tmpl);
  if (param == NULL) return DDS_RETCODE_BAD_PARAMETER;
  struct dds_sql_token *t = (struct dds_sql_token *)param;
  if (t->s != NULL) { free (t->s); t->s = NULL; }
  t->tok = DDS_SQL_TK_BLOB;
  t->aff = DDS_SQL_AFFINITY_BLOB;
  t->n.i = (int64_t) s;
  t->s = malloc(s);
  (void) memcpy (t->s, b, s);
  return DDS_RETCODE_OK;
}


static void prepare_parameters_set (void *vnode, void *varg)
{
  struct dds_sql_token *t = (struct dds_sql_token *) vnode;
  assert (t != NULL);
  uint32_t *arg = (uint32_t *) varg;
  assert (*arg != 0);
  /*
   * we are pretty loyal to unset parameters ("not for variables!"), if user
   * ignore setting it, force param. to be `0`.
   * */
  if (t->tok == DDS_SQL_TK_VARIABLE || t->tok == DDS_SQL_TK_ID)
  {
    t->tok = DDS_SQL_TK_INTEGER;
    t->aff = DDS_SQL_AFFINITY_INTEGER;
    t->n.i = 0;
  }
  *arg -= (t->tok != DDS_SQL_TK_ID && t->tok != DDS_SQL_TK_VARIABLE)? 1: 0;
}

static int expr_pre_eval(const struct dds_sql_token *optoken, struct dds_sql_token **token)
{
  int result = 0;
  int tk = (*token)->tok;
  if (tk == DDS_SQL_TK_ID || tk == DDS_SQL_TK_VARIABLE)
    return result;

  if (tk == DDS_SQL_TK_INTEGER || tk == DDS_SQL_TK_FLOAT
      || tk == DDS_SQL_TK_STRING || tk == DDS_SQL_TK_BLOB)
  {
    if (optoken->aff >= DDS_SQL_AFFINITY_NUMERIC)
    {
      int opt = optoken->tok;
      if (opt == DDS_SQL_TK_OR || opt == DDS_SQL_TK_AND
          || opt == DDS_SQL_TK_STAR)
      {
        /* apply required affinity on first and if it's '0'|'1' skip
         * calculation of the second or just return a result, no matter what
         * inside a previous branch.
         * */
        struct dds_sql_token preop = {
          .tok = DDS_SQL_TK_EQ,
          .asc = get_op_assoc(DDS_SQL_TK_EQ),
          .aff = get_op_affinity(DDS_SQL_TK_EQ)
        };
        struct dds_sql_token prel = {
          .tok = DDS_SQL_TK_FLOAT,
          .aff = DDS_SQL_AFFINITY_REAL,
          .n = {0}
        };
        struct dds_sql_token prer = {
          .tok = (*token)->tok,
          .aff = (*token)->aff,
          .n = (*token)->n,
        };
        if ((*token)->s != NULL && ((*token)->tok == DDS_SQL_TK_STRING || (*token)->tok == DDS_SQL_TK_BLOB))
        {
          prer.s = malloc((unsigned)(*token)->n.i);
          (void) memcpy(prer.s, (*token)->s, (unsigned)(*token)->n.i);
        }
        struct dds_sql_token *op_p = &preop;
        result = dds_sql_eval_op(&op_p, &prel, &prer);
        preop.tok = DDS_SQL_TK_INTEGER;
        bool is_zero = (bool) preop.n.i;
        switch (opt)
        {
          case DDS_SQL_TK_OR:
          {
            if ((result = !is_zero)) {
              if ((*token)->s) free ((*token)->s);
              op_p->n.i = result;
              op_p->aff = DDS_SQL_AFFINITY_INTEGER;
              (void) memcpy(*token, op_p, sizeof(*op_p));
            }
            break;
          }
          case DDS_SQL_TK_STAR:
          case DDS_SQL_TK_AND:
          {
            if ((result = is_zero)) {
              if ((*token)->s) free ((*token)->s);
              op_p->n.i = !result;
              op_p->aff = DDS_SQL_AFFINITY_INTEGER;
              (void) memcpy(*token, op_p, sizeof(*op_p));
            }
            break;
          }
          default:
            break;
        }
      } else if (opt == DDS_SQL_TK_UPLUS || opt == DDS_SQL_TK_UMINUS) {
        /* struct dds_sql_token preop = { */
        /*   .tok = opt, */
        /*   .asc = get_op_assoc(opt), */
        /*   .aff = get_op_affinity(opt), */
        /* }; */
        /* struct dds_sql_token prer = { */
        /*   .tok = (*token)->tok, */
        /*   .aff = (*token)->aff, */
        /*   .n = (*token)->n, */
        /* }; */
        /* if ((*token)->s != NULL && ((*token)->tok == DDS_SQL_TK_STRING || (*token)->tok == DDS_SQL_TK_BLOB)) */
        /* { */
        /*   prer.s = malloc((unsigned)(*token)->n.i); */
        /*   (void) memcpy(prer.s, (*token)->s, (unsigned)(*token)->n.i); */
        /* } */
        /* struct dds_sql_token *op_p = &preop; */
        /* result = dds_sql_eval_op(&op_p, NULL, &prer); */
        /* assert (result == 0); */
        /* op_p->tok = prer.tok; */
        /* op_p->aff = prer.aff; */
        /* if ((*token)->s) free ((*token)->s); */
        /* (void) memcpy(*token, op_p, sizeof(*op_p)); */
        /* result = 1; */
      }
    }
  }
  return result;
}

static dds_return_t expr_node_optimize(const struct dds_sql_expr_node *orig, struct dds_sql_expr_node **node, struct ddsrt_hh **vars)
{
  dds_return_t ret = 0;
  if (orig == NULL)
    return ret;
  if (orig->r != NULL) {
    size_t olh = (orig->l)? orig->l->height: 0;
    size_t orh = (orig->r)? orig->r->height: 0;
    struct dds_sql_expr_node *first = (olh <= orh)? orig->l: orig->r;
    struct dds_sql_expr_node *fres = NULL;
    ret = expr_node_optimize(first, &fres, vars);
    assert (ret == DDS_RETCODE_OK);
    if (fres != NULL && expr_pre_eval(orig->token, &fres->token) != 0)
    {
      (*node) = fres;
      goto exit;
    }
    struct dds_sql_expr_node *second = (olh <= orh)? orig->r: orig->l;
    struct dds_sql_expr_node *sres = NULL;
    ret = expr_node_optimize(second, &sres, vars);
    if (sres != NULL && (ret = expr_pre_eval(orig->token, &sres->token)) != 0)
    {
      if (fres != NULL) dds_sql_expr_node_fini(fres);
      (*node) = sres;
      ret = 0;
      goto exit;
    }
    struct dds_sql_token *op = malloc(sizeof(*op));
    (void) memcpy (op, orig->token, sizeof(*orig->token));
    (*node) = malloc(sizeof(struct dds_sql_expr_node));
    (*node)->token = op;
    int ftk = (fres != NULL)? fres->token->tok: DDS_SQL_TK_INTEGER, stk = (sres != NULL)? sres->token->tok: DDS_SQL_TK_INTEGER;
    struct dds_sql_expr_node *l = (olh <= orh)? fres: sres;
    struct dds_sql_expr_node *r = (olh <= orh)? sres: fres;
    assert (r != NULL); /* FIXME: !?!?!? */
    if ((ftk == DDS_SQL_TK_INTEGER || ftk == DDS_SQL_TK_FLOAT || ftk == DDS_SQL_TK_STRING || ftk == DDS_SQL_TK_BLOB)
        && (stk == DDS_SQL_TK_INTEGER || stk == DDS_SQL_TK_FLOAT || stk == DDS_SQL_TK_STRING || stk == DDS_SQL_TK_BLOB))
    {
      ret = dds_sql_eval_op(&op, (l!=NULL)? l->token: NULL, r->token);
      assert (ret == 0);
      op->tok = r->token->tok;
      op->aff = r->token->aff;
      if (l != NULL) dds_sql_expr_node_fini(l);
      dds_sql_expr_node_fini(r);
      (*node)->l = (*node)->r = NULL;
      (*node)->height = 0;
    } else if ((ftk == DDS_SQL_TK_ID || stk == DDS_SQL_TK_ID) && (op->tok == DDS_SQL_TK_DOT)) {
      /* FIXME: (already done during parsing)
       * Special case, we need to calculate comb. hash and update variables
       * hash-table with new value. Not worries cleanup will take into account
       * later, and unused ID's will be removed.
       * Potentially the same place for variable indexing?
       * */
      assert (false);
    } else {
      (*node)->l = l;
      (*node)->r = r;
      (*node)->height = ((l != NULL && l->height > r->height)? l->height: r->height) + 1;
    }

  } else {
    int token = orig->token->tok;
    ret = dds_sql_expr_node_init(node);
    if (token == DDS_SQL_TK_ID)
    {
      struct dds_sql_param tmpl = {.id.str = ddsrt_strdup(orig->token->s), .tok = token};
      struct dds_sql_param *par = ddsrt_hh_lookup(*vars, &tmpl);
      if (par == NULL)
      {
        par = malloc(sizeof(*par));
        (void) memcpy(&par->token, orig->token, sizeof(par->token));
        par->token.s = calloc(1, (unsigned)orig->token->n.i + 1U);
        (void) memcpy(par->token.s, orig->token->s, (unsigned)orig->token->n.i);
        par->id = tmpl.id;
        par->tok = token;
        /*
         * optimizer can decide to remove related to variable node, but we
         * don't want erase of this var. to happen. at least for now. further
         * optimizations can reduce it if not used within expression.
         * */
        par->token.tok = -1;
        ddsrt_hh_add(*vars, par);
      } else {
        free (tmpl.id.str);
      }
      (*node)->token = (struct dds_sql_token *)par;
    } else {
      (*node)->token = malloc(sizeof(struct dds_sql_token));
      (void) memcpy((*node)->token, orig->token, sizeof(struct dds_sql_token));
      if (orig->token->s != NULL && (token == DDS_SQL_TK_BLOB)) {
        (*node)->token->s = malloc((unsigned int)orig->token->n.i);
        (void) memcpy((*node)->token->s, orig->token->s, (unsigned int)orig->token->n.i);
      } else if (orig->token->s != NULL && (token == DDS_SQL_TK_STRING)) {
        (*node)->token->s = calloc(1, (unsigned int)orig->token->n.i + 1);
        (void) memcpy((*node)->token->s, orig->token->s, (unsigned int)orig->token->n.i);
      }
    }
  }
exit:
  return ret;
}

static struct dds_sql_token *expr_node_token_lookup(const struct dds_sql_expr_node *node, struct dds_sql_token **token)
{
  if (node == NULL)
    return NULL;
  if (node->token == (*token))
    return node->token;

  struct dds_sql_token *result = NULL;
  if ((result = expr_node_token_lookup(node->l, token)) != NULL)
    return result;

  return expr_node_token_lookup(node->r, token);
}

static void reduce_variables_set (void *vnode, void *varg)
{
  struct dds_sql_param *param = (struct dds_sql_param *) vnode;
  struct dds_sql_expr *expr = (struct dds_sql_expr *) varg;
  if (expr_node_token_lookup(expr->node, (struct dds_sql_token **)&param) == NULL) {
#ifndef NDEBUG
    assert (ddsrt_hh_remove(expr->param_tokens, param));
#else
    (void) ddsrt_hh_remove(expr->param_tokens, param);
#endif
    param_fini_fn(param, &expr->param_kind);
    expr->nparams--;
  }
}

dds_return_t dds_sql_expr_build(const struct dds_sql_expr *ex, struct dds_sql_expr **out)
{
  uint32_t count = ex->nparams;
  ddsrt_hh_enum(ex->param_tokens, prepare_parameters_set, &count);
  assert (count == 0);
  dds_return_t ret = DDS_RETCODE_OK;
  if ((ret = expr_node_optimize(ex->node, &(*out)->node, &(*out)->param_tokens) != DDS_RETCODE_OK))
    goto err;
  struct ddsrt_hh_iter it;
  for (dds_sql_param_t *token = ddsrt_hh_iter_first((*out)->param_tokens, &it); token != NULL; token = ddsrt_hh_iter_next(&it))
  {
    token->token.tok = DDS_SQL_TK_ID;
    (*out)->nparams++;
  }
  /*
   * according to optimize process, there is "ghost" parameters may be
   * presented. it can be fixed by counting parameters at the end, to be sure
   * that we don't waste space.
   * */
  ddsrt_hh_enum((*out)->param_tokens, reduce_variables_set, (*out));
err:
  return ret;
}

static dds_return_t expr_node_evaluate(const struct dds_sql_expr_node *node, struct dds_sql_token **out)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_sql_token *res = NULL;
  ret = dds_sql_token_init(&res);
  (void) memcpy(res, node->token, sizeof(*node->token));
  int token = node->token->tok;
  if (node->token->s != NULL && (token == DDS_SQL_TK_BLOB)) {
    res->s = malloc((unsigned int)node->token->n.i);
    (void) memcpy(res->s, node->token->s, (unsigned int)node->token->n.i);
  } else if (node->token->s != NULL && (token == DDS_SQL_TK_STRING)) {
    res->s = calloc(1, (unsigned int)node->token->n.i + 1);
    (void) memcpy(res->s, node->token->s, (unsigned int)node->token->n.i);
  }
  if (node->r != NULL)
  {
    /* FIXME:
     * in case of valid for optimization operators (at lease for AND, OR),
     * evaluate subnode with minimal `height` first and in case:
     * -  0 OR  `node` ==> 0;
     * -  1 AND `node` ==> 0;
     * -  1 *   `node` ==> 0;
     * skip evaluation of the second node.
     * [IMPORTANT]: optimization doesn't make any sense if height of the second node is <= 2.
     * */
    /* FIXME:
     * According to "DDS-XTypes_1.1 (7.6.6.2 Optional Type Members)" not all
     * members may be known during expr. evaluation, in that case they are
     * compared against special "null" value. Simply say, if any member are not
     * set replace it with "false" a.k.a '0'?
     * */

    struct dds_sql_token *r = NULL;
    ret = expr_node_evaluate(node->r, &r);
    assert (ret == DDS_RETCODE_OK);
    if (node->l != NULL) {
      struct dds_sql_token *l = NULL;
      ret = expr_node_evaluate(node->l, &l);
      assert (ret == DDS_RETCODE_OK);
      ret = dds_sql_eval_op(&res, l, r);
      assert (ret == DDS_RETCODE_OK);
      if (l->s) free (l->s);
      free (l);
    } else {
      ret = dds_sql_eval_op(&res, NULL, r);
    }
    assert (ret == 0);
    res->tok = r->tok;
    res->aff = r->aff;
    if (r->s) free (r->s);
    free (r);
  }
  *out = res;
  return ret;
}

dds_return_t dds_sql_expr_eval(const struct dds_sql_expr *ex, struct dds_sql_token **out)
{
  dds_return_t ret = DDS_RETCODE_OK;
  ret = expr_node_evaluate(ex->node, out);
  return ret;
}
