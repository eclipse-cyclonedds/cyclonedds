// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "idl/heap.h"
#include "idl/processor.h"
#include "idl/string.h"
#include "scanner.h"
#include "parser.h"

/* treat every cr+lf, lf+cr, cr, lf sequence as a single newline */
static int32_t
have_newline(idl_pstate_t *pstate, const char *cur)
{
  if (cur == pstate->scanner.limit)
    return pstate->config.flags & IDL_WRITE ? -2 : 0;
  assert(cur < pstate->scanner.limit);
  if (cur[0] == '\n') {
    if (cur < pstate->scanner.limit - 1)
      return cur[1] == '\r' ? 2 : 1;
    return pstate->config.flags & IDL_WRITE ? -1 : 1;
  } else if (cur[0] == '\r') {
    if (cur < pstate->scanner.limit - 1)
      return cur[1] == '\n' ? 2 : 1;
    return pstate->config.flags & IDL_WRITE ? -1 : 1;
  }
  return 0;
}

static int32_t
have_skip(idl_pstate_t *pstate, const char *cur)
{
  int cnt = 0;
  if (cur == pstate->scanner.limit)
    return pstate->config.flags & IDL_WRITE ? -3 : 0;
  assert(cur < pstate->scanner.limit);
  if (*cur == '\\' && (cnt = have_newline(pstate, cur + 1)) > 0)
    cnt++;
  return cnt;
}

static int32_t
have_space(idl_pstate_t *pstate, const char *cur)
{
  if (cur == pstate->scanner.limit)
    return pstate->config.flags & IDL_WRITE ? -2 : 0;
  assert(cur < pstate->scanner.limit);
  if (*cur == ' ' || *cur == '\t' || *cur == '\f' || *cur == '\v')
    return 1;
  return have_newline(pstate, cur);
}

static int32_t
have_digit(idl_pstate_t *pstate, const char *cur)
{
  if (cur == pstate->scanner.limit)
    return pstate->config.flags & IDL_WRITE ? -1 : 0;
  assert(cur < pstate->scanner.limit);
  return (*cur >= '0' && *cur <= '9');
}

static int32_t
have_alpha(idl_pstate_t *pstate, const char *cur)
{
  if (cur == pstate->scanner.limit)
    return pstate->config.flags & IDL_WRITE ? -1 : 0;
  assert(cur < pstate->scanner.limit);
  return (*cur >= 'a' && *cur <= 'z') ||
         (*cur >= 'A' && *cur <= 'Z') ||
         (*cur == '_');
}

static int32_t
have_alnum(idl_pstate_t *pstate, const char *cur)
{
  if (cur == pstate->scanner.limit)
    return pstate->config.flags & IDL_WRITE ? -1 : 0;
  assert(cur < pstate->scanner.limit);
  return (*cur >= 'a' && *cur <= 'z') ||
         (*cur >= 'A' && *cur <= 'Z') ||
         (*cur >= '0' && *cur <= '9') ||
         (*cur == '_');
}

static void
error(idl_pstate_t *pstate, const char *cur, const char *fmt, ...)
{
  int cnt;
  const char *ptr = pstate->scanner.cursor;
  idl_location_t loc;
  va_list ap;

  /* determine exact location */
  loc.first = pstate->scanner.position;
  loc.last = (idl_position_t){NULL, NULL, 0, 0};
  while (ptr < cur) {
    if ((cnt = have_newline(pstate, ptr)) > 0) {
      loc.first.line++;
      loc.first.column = 0;
      ptr += cnt;
    } else {
      ptr++;
    }
    loc.first.column++;
  }

  va_start(ap, fmt);
  idl_verror(pstate, &loc, fmt, ap);
  va_end(ap);
}

static const char *
next(idl_pstate_t *pstate, const char *cur)
{
  int cnt;

  /* might be positioned at newline */
  if ((cnt = have_newline(pstate, cur))) {
    if (cnt < 0)
      return pstate->scanner.limit;
    cur += (size_t)cnt;
  } else {
    cur++; /* skip to next character */
  }
  /* skip if positioned at line continuation sequence */
  for (; (cnt = have_skip(pstate, cur)) > 0; cur += (size_t)cnt) ;

  return cnt < 0 ? pstate->scanner.limit : cur;
}

static const char *
move(idl_pstate_t *pstate, const char *cur)
{
  int cnt;
  const char *ptr;

  assert(cur >= pstate->scanner.cursor && cur <= pstate->scanner.limit);
  for (ptr = pstate->scanner.cursor; ptr < cur; ptr += cnt) {
    if ((cnt = have_newline(pstate, ptr)) > 0) {
      pstate->scanner.position.line++;
      pstate->scanner.position.column = 1;
    } else {
      cnt = 1;
      pstate->scanner.position.column++;
    }
  }
  pstate->scanner.cursor = cur;
  return cur;
}

static int32_t
peek(idl_pstate_t *pstate, const char *cur)
{
  int cnt;

  /* skip if positioned at line continuation sequences */
  for (; (cnt = have_skip(pstate, cur)) > 0; cur += cnt) ;

  if (cnt < 0 || cur == pstate->scanner.limit)
    return '\0';
  return *cur;
}

static int32_t
have(idl_pstate_t *pstate, const char *cur, const char *str)
{
  int cnt;
  size_t len, pos;
  const char *lim = cur;

  for (pos = 0, len = strlen(str); pos < len; pos++, lim++) {
    /* skip any line continuation sequences */
    while ((cnt = have_skip(pstate, lim)) > 0)
      lim += cnt;
    if (cnt < 0)
      return cnt - (int)(len - pos);
    if (str[pos] != *lim)
      return 0;
  }

  return (int)(lim - cur);
}

static int32_t
need_refill(idl_pstate_t *pstate, const char *cur)
{
  return have_skip(pstate, cur) < 0;
}

static int32_t
scan_line_comment(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  cur = next(pstate, cur);
  while ((cur = next(pstate, cur)) < pstate->scanner.limit) {
    if (have_newline(pstate, cur))
      break;
  }

  if (need_refill(pstate, cur))
    return IDL_RETCODE_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_LINE_COMMENT;
}

static int32_t
scan_comment(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  enum { initial, escape, asterisk, slash } state = initial;

  cur = next(pstate, cur);
  while (state != slash && (cur = next(pstate, cur)) < pstate->scanner.limit) {
    switch (state) {
      case initial:
        if (*cur == '\\')
          state = escape;
        else if (*cur == '*')
          state = asterisk;
        break;
      case escape:
        state = initial;
        break;
      case asterisk:
        if (*cur == '\\')
          state = escape;
        else if (*cur == '/')
          state = slash;
        else if (*cur != '*')
          state = initial;
        break;
      default:
        assert(state == slash && false);
        break;
    }
  }

  *lim = cur;
  if (state == slash) {
    assert(cur < pstate->scanner.limit);
    *lim = cur + 1;
    return IDL_TOKEN_COMMENT;
  } else if (need_refill(pstate, cur)) {
    return IDL_RETCODE_NEED_REFILL;
  }
  error(pstate, cur, "unterminated comment");
  return IDL_RETCODE_SYNTAX_ERROR;
}

static int32_t
scan_char_literal(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int cnt = 0, esc = 0;
  size_t pos = 0;
  const size_t max = 5; /* \uhhhh */

  for (; pos < max && (cur = next(pstate, cur)) < pstate->scanner.limit; pos++) {
    if (esc)
      esc = 0;
    else if (*cur == '\\')
      esc = 1;
    else if (*cur == '\'')
      break;
    else if ((cnt = have_newline(pstate, cur)))
      break;
  }

  if (cnt < 0) {
    return IDL_RETCODE_NEED_REFILL;
  } else if (cnt > 0) {
    error(pstate, cur, "unterminated character constant");
    return IDL_RETCODE_SYNTAX_ERROR;
  } else if (pos > max) {
    error(pstate, cur, "invalid character constant");
    return IDL_RETCODE_SYNTAX_ERROR;
  }

  assert(*cur == '\'');
  *lim = cur + 1;
  return IDL_TOKEN_CHAR_LITERAL;
}

static int32_t
scan_string_literal(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int cnt = 0, esc = 0;

  while ((cur = next(pstate, cur)) < pstate->scanner.limit) {
    if (esc)
      esc = 0;
    else if (*cur == '\\')
      esc = 1;
    else if (*cur == '\"')
      break;
    else if ((cnt = have_newline(pstate, cur)))
      break;
  }

  if (cnt < 0 || need_refill(pstate, cur)) {
    return IDL_RETCODE_NEED_REFILL;
  } else if (cnt > 0) {
    error(pstate, cur, "unterminated string literal");
    return IDL_RETCODE_SYNTAX_ERROR;
  }

  assert(*cur == '\"');
  *lim = cur + 1;
  return IDL_TOKEN_STRING_LITERAL;
}

static int
scan_floating_pt_literal(
  idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int32_t chr = peek(pstate, cur);
  enum { integer, fraction, exponent } state = integer;

  if (chr >= '0' && chr <= '9') {
    state = integer;
  } else {
    assert(chr == '.');
    cur = next(pstate, cur);
    chr = peek(pstate, cur);
    assert(chr >= '0' && chr <= '9');
    state = fraction;
  }

  while ((cur = next(pstate, cur)) < pstate->scanner.limit) {
    chr = peek(pstate, cur);
    assert(chr != '\0');
    if (chr == '.') {
      if (state != integer)
        break;
      state = fraction;
    } else if (chr == 'e' || chr == 'E') {
      const char *exp;
      if (state != integer && state != fraction)
        break;
      state = exponent;
      exp = next(pstate, cur);
      chr = peek(pstate, exp);
      if (chr == '+' || chr == '-')
        exp = next(pstate, exp);
      if (!have_digit(pstate, exp))
        break;
      cur = exp;
    } else if (chr < '0' || chr > '9') {
      assert(state != integer);
      break;
    }
  }

  if (need_refill(pstate, cur))
    return IDL_RETCODE_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_FLOATING_PT_LITERAL;
}

static const char oct[] = "01234567";
static const char dec[] = "0123456789";
static const char hex[] = "0123456789abcdefABCDEF";

static int
scan_integer_literal(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int32_t chr = peek(pstate, cur);
  const char *base, *off = cur;
  if (chr >= '1' && chr <= '9') {
    base = dec;
  } else {
    assert(chr == '0');
    chr = peek(pstate, next(pstate, cur));
    if (chr == 'x' || chr == 'X') {
      cur = next(pstate, cur); /* skip x */
      base = hex;
    } else {
      base = oct;
    }
  }

  while ((cur = next(pstate, cur)) < pstate->scanner.limit) {
    chr = peek(pstate, cur);
    if (base != hex) {
      if (chr == '.') {
        return scan_floating_pt_literal(pstate, off, lim);
      } else if (chr == 'e' || chr == 'E') {
        const char *exp;
        exp = next(pstate, cur);
        chr = peek(pstate, exp);
        if (chr == '+' || chr == '-')
          exp = next(pstate, exp);
        if (!have_digit(pstate, exp))
          break;
        return scan_floating_pt_literal(pstate, off, lim);
      }
    }
    if (!strchr(base, chr))
      break;
  }

  if (need_refill(pstate, cur))
    return IDL_RETCODE_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_INTEGER_LITERAL;
}

static int32_t
scan_pp_number(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int32_t chr;

  if (*cur == '.')
    cur = next(pstate, cur);
  while ((cur = next(pstate, cur)) < pstate->scanner.limit) {
    chr = peek(pstate, cur);
    if (chr < '0' || chr > '9')
      break;
  }

  if (need_refill(pstate, cur))
    return IDL_RETCODE_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_PP_NUMBER;
}

static int32_t
scan_identifier(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int32_t cnt = 0;

  do {
    *lim = cur;
    /* skip over any line continuation sequences */
    for (; (cnt = have_skip(pstate, cur)) > 0; cur += cnt) ;
    if ((cnt = have_alnum(pstate, cur)) == 0 &&
        (cnt = have(pstate, cur, "_")) == 0)
      break;
    else if (cnt < 0)
      return IDL_RETCODE_NEED_REFILL;
    cur += cnt;
  } while (cnt);

  switch (pstate->scanner.state) {
    case IDL_SCAN_ANNOTATION:
      pstate->scanner.state = IDL_SCAN_ANNOTATION_NAME;
      return IDL_TOKEN_ANNOTATION;
    case IDL_SCAN_ANNOTATION_APPL:
    case IDL_SCAN_ANNOTATION_APPL_SCOPED_NAME:
      if ((cnt = have(pstate, *lim, "::")) < 0)
        return IDL_RETCODE_NEED_REFILL;
      else if (cnt > 0)
        pstate->scanner.state = IDL_SCAN_ANNOTATION_APPL_SCOPE;
      else
        pstate->scanner.state = IDL_SCAN_ANNOTATION_APPL_SCOPED_NAME;
      /* fall through */
    default:
      return IDL_TOKEN_IDENTIFIER;
  }
}

/* grammer for IDL (>=4.0) is incorrect (or at least ambiguous). blanks,
   horizontal and vertical tabs, newlines, form feeds, and comments
   (collective, "white space") are ignored except as they serve to separate
   tokens. the specification does not clearly pstate if white space may occur
   between "::" and adjacent identifiers to form a "scoped_name". the same is
   true for the "annotation_appl". in C++ "::" is an operator and white space
   is therefore allowed, in IDL it is not. this did not use to be a problem,
   but with the addition of annotations it became possible to have two
   adjacent scoped names. many compilers (probably) implement just the
   standardized annotations. the pragmatic approach is to forbid use of white
   space in annotations, which works for standardized annotations like "@key"
   and allow use of white space for scoped names elsewhere. to implement this
   feature the parser must know whether or not white space occurred between an
   identifier and the scope operator. however, white space cannot be
   communicated to the the parser (the grammer would explode) and an
   introducing an extra identifier class is not an option (same reason). to
   work around this problem, the lexer communicates different types of scope
   operators used by the parser to implement a specialized "scoped_name"
   version just for annotations. */
static int32_t
scan_scope(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int32_t cnt;

  cnt = have(pstate, cur, "::");
  assert(cnt > 0);
  *lim = (cur += cnt);

  switch(pstate->scanner.state) {
    case IDL_SCAN_ANNOTATION_APPL:
    case IDL_SCAN_ANNOTATION_APPL_SCOPE:
      if ((cnt = have_alpha(pstate, cur)) == 0 &&
          (cnt = have(pstate, cur, "_")) == 0)
      {
        pstate->scanner.state = IDL_SCAN_ANNOTATION_APPL_SCOPED_NAME;
        return IDL_TOKEN_SCOPE_NO_SPACE;
      } else if (cnt < 0) {
        return IDL_RETCODE_NEED_REFILL;
      } else {
        pstate->scanner.state = IDL_SCAN_GRAMMAR;
      }
      /* fall through */
    default:
      return IDL_TOKEN_SCOPE;
  }
}

static int32_t
scan_at(idl_pstate_t *pstate, const char *cur, const char **lim)
{
  int32_t cnt, code = '@';

  cnt = have(pstate, cur, "@");
  assert(cnt > 0);
  *lim = (cur += cnt);
  /* detect @annotation */
  if ((cnt = have(pstate, cur, "annotation")) < 0) {
    return IDL_RETCODE_NEED_REFILL;
  } else if (cnt > 0) {
    cur += cnt;
    code = IDL_TOKEN_ANNOTATION_SYMBOL;
  }
  /* detect @foo and @::foo, catch @annotation:: */
  if ((cnt = have(pstate, cur, "::")) == 0 &&
      (cnt = have_alpha(pstate, cur)) == 0 &&
      (cnt = have(pstate, cur, "_")) == 0)
  {
    if (code == '@')
      pstate->scanner.state = IDL_SCAN_GRAMMAR;
    else
      pstate->scanner.state = IDL_SCAN_ANNOTATION;
    return code;
  } else if (cnt < 0) {
    return IDL_RETCODE_NEED_REFILL;
  } else {
    pstate->scanner.state = IDL_SCAN_ANNOTATION_APPL;
    return IDL_TOKEN_ANNOTATION_SYMBOL;
  }
}

static idl_retcode_t
scan(idl_pstate_t *pstate, idl_lexeme_t *lex)
{
  int chr, cnt, code = '\0';
  const char *cur, *lim = pstate->scanner.cursor;

  do {
    /* skip over any line continuation sequences */
    for (; (cnt = have_skip(pstate, lim)) > 0; lim += cnt) ;

    move(pstate, lim);
    lex->location.first = pstate->scanner.position;
    lex->marker = cur = lim;

    if (need_refill(pstate, lim))
      return IDL_RETCODE_NEED_REFILL;

    chr = peek(pstate, cur);
    if (chr == '\0') {
      assert(cur == pstate->scanner.limit);
      break;
    } else if (have(pstate, cur, "/*") > 0) {
      code = scan_comment(pstate, cur, &lim);
    } else if (have(pstate, cur, "//") > 0) {
      code = scan_line_comment(pstate, cur, &lim);
    } else if (chr == '\'') {
      code = scan_char_literal(pstate, cur, &lim);
    } else if (chr == '\"') {
      code = scan_string_literal(pstate, cur, &lim);
    } else if ((cnt = have_newline(pstate, cur)) > 0) {
      lim = cur + cnt;
      code = '\n';
    } else if ((cnt = have_space(pstate, cur)) > 0) {
      /* skip space characters, except newline */
      lim = cur + cnt;
    } else if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE) {
      /*
       * preprocessor
       */
      if (chr == '.' && have_digit(pstate, next(pstate, cur))) {
        code = scan_pp_number(pstate, cur, &lim);
      } else if (have_alpha(pstate, cur) || chr == '_') {
        code = scan_identifier(pstate, cur, &lim);
      } else if (have_digit(pstate, cur)) {
        code = scan_pp_number(pstate, cur, &lim);
      } else if (have(pstate, cur, "::")) {
        code = scan_scope(pstate, cur, &lim);
      } else {
        lim = cur + 1;
        code = (unsigned char)*cur;
      }
    } else if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_GRAMMAR) {
      /*
       * interface definition language
       */
      if (chr == '.' && have_digit(pstate, next(pstate, cur))) {
        code = scan_floating_pt_literal(pstate, cur, &lim);
      } else if (have_digit(pstate, cur)) {
        /* idl_stroull takes care of decimal vs. octal vs. hexadecimal */
        code = scan_integer_literal(pstate, cur, &lim);
      } else if (have_alpha(pstate, cur) || chr == '_') {
        code = scan_identifier(pstate, cur, &lim);
      } else if (have(pstate, cur, "::") > 0) {
        code = scan_scope(pstate, cur, &lim);
      } else if ((cnt = have(pstate, cur, "<<")) > 0) {
        lim = cur + cnt;
        code = IDL_TOKEN_LSHIFT;
      } else if ((cnt = have(pstate, cur, ">>")) > 0) {
        lim = cur + cnt;
        code = IDL_TOKEN_RSHIFT;
      } else if (chr == '@') {
        code = scan_at(pstate, cur, &lim);
      } else {
        lim = cur + 1;
        code = (unsigned char)*cur;
      }
    } else if (chr == '#') {
      pstate->scanner.state = IDL_SCAN_DIRECTIVE;
      lim = cur + 1;
      code = (unsigned char)*cur;
    } else if ((pstate->config.flags & IDL_WRITE) && next(pstate, cur) == pstate->scanner.limit) {
      code = IDL_RETCODE_NEED_REFILL;
    } else {
      pstate->scanner.state = IDL_SCAN_GRAMMAR;
    }
  } while (code == '\0');

  if (code > 0)
    move(pstate, lim);
  lex->limit = lim;
  lex->location.last = pstate->scanner.position;

  return code;
}

static idl_retcode_t
unescape(
  idl_pstate_t *pstate, idl_lexeme_t *lex, char *str, size_t *len)
{
  size_t pos=0;
  static const char seq[][2] = {
    {'n','\n'}, /* newline */
    {'t','\t'}, /* horizontal tab */
    {'v','\v'}, /* vertical tab */
    {'b','\b'}, /* backspace */
    {'r','\r'}, /* carriage return */
    {'f','\f'}, /* form feed */
    {'a','\a'}, /* alert */
    {'\\', '\\'}, /* backslash */
    {'?', '\?'}, /* question mark */
    {'\'', '\''}, /* single quote */
    {'\"', '\"'}, /* double quote */
    {'\0', '\0'} /* terminator */
  };

  for (size_t i=0; i < *len;) {
    if (str[i] == '\\') {
      i++;
      if (idl_isdigit(str[i], 8) != -1) {
        int c = 0;
        for (size_t j=0; j < 3 && idl_isdigit(str[i], 8) != -1; j++)
          c = (c*8) + idl_isdigit(str[i++], 8);
        str[pos++] = (char)c;
      } else if (str[i] == 'x' || str[i] == 'X') {
        int c = 0;
        i++;
        if (idl_isdigit(str[i], 16) != -1) {
          c = (c*16) + idl_isdigit(str[i++], 16);
          if (idl_isdigit(str[i], 16) != -1)
            c = (c*16) + idl_isdigit(str[i++], 16);
          str[pos++] = (char)c;
        } else {
          idl_error(pstate, &lex->location,
            "\\x used with no following hex digits");
          return IDL_RETCODE_SYNTAX_ERROR;
        }
      } else {
        size_t j;
        for (j=0; seq[j][0] && seq[j][0] != str[i]; j++) ;
        if (seq[j][0]) {
          str[pos++] = seq[j][1];
        } else {
          str[pos++] = str[i];
          idl_warning(pstate, IDL_WARN_UNKNOWN_ESCAPE_SEQ, &lex->location,
            "unknown escape sequence '\\%c'", str[i]);
        }
        i++;
      }
    } else if (pos != i) {
      str[pos++] = str[i++];
    } else {
      pos = ++i;
    }
  }

  assert(pos <= *len);
  str[(*len = pos)] = '\0';
  return IDL_RETCODE_OK;
}

static idl_retcode_t
tokenize(
  idl_pstate_t *pstate, idl_lexeme_t *lex, int32_t code, idl_token_t *tok)
{
  int cnt;
  char buf[32], *str = buf;
  size_t len = 0, pos = 0;
  idl_retcode_t ret;

  /* short-circuit if token is a single character */
  if (code < 256) {
    tok->code = code;
    tok->location = lex->location;
    return code;
  }

  len = (size_t)((uintptr_t)lex->limit - (uintptr_t)lex->marker);
  if (len >= sizeof(buf) && !(str = idl_malloc(len + 1)))
    return IDL_RETCODE_NO_MEMORY;

  /* strip line continuation sequences */
  for (const char *ptr = lex->marker; ptr < lex->limit; ) {
    if ((cnt = have_skip(pstate, ptr)) > 0) {
      ptr += cnt;
    } else {
      assert(cnt == 0);
      str[pos++] = *ptr++;
    }
  }
  assert(pos <= len);
  len = pos;
  str[pos] = '\0';

  switch (code) {
    case IDL_TOKEN_ANNOTATION:
      assert(strcmp(str, "annotation") == 0);
      assert(pstate->scanner.state == IDL_SCAN_ANNOTATION_NAME);
      break;
    case IDL_TOKEN_IDENTIFIER:
      /* preprocessor identifiers are not IDL identifiers */
      if ((unsigned)pstate->scanner.state & (unsigned)IDL_SCAN_DIRECTIVE)
        break;
      /* annotation names cannot be keywords, i.e. "@default" */
      if (pstate->scanner.state == IDL_SCAN_ANNOTATION_APPL_NAME)
        goto identifier;
      if (pstate->scanner.state == IDL_SCAN_ANNOTATION_APPL_SCOPED_NAME)
        goto identifier;
      if (pstate->scanner.state == IDL_SCAN_ANNOTATION_NAME)
        goto identifier;
      if ((code = idl_iskeyword(pstate, str, !(pstate->config.flags & IDL_FLAG_CASE_SENSITIVE))))
        break;
identifier:
      pstate->scanner.state = IDL_SCAN_GRAMMAR;
      code = IDL_TOKEN_IDENTIFIER;
      break;
    case IDL_TOKEN_CHAR_LITERAL:
    case IDL_TOKEN_STRING_LITERAL:
      assert((str[0] == '\'' || str[0] == '"'));
      len -= 1 + (size_t)(str[len - 1] == str[0]);
      memmove(str, str + 1, len);
      str[len] = '\0';
      if ((ret = unescape(pstate, lex, str, &len)) != IDL_RETCODE_OK) {
        if (str != buf)
          idl_free(str);
        return ret;
      }
      break;
    case IDL_TOKEN_INTEGER_LITERAL: {
      char *end = NULL;
      tok->value.ullng = idl_strtoull(str, &end, 0);
      assert(end && *end == '\0');
    } break;
    case IDL_TOKEN_FLOATING_PT_LITERAL: {
      char *end = NULL;
      tok->value.ldbl = idl_strtold(str, &end);
      assert(end && *end == '\0');
    } break;
    default:
      break;
  }

  switch (code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_PP_NUMBER:
    case IDL_TOKEN_STRING_LITERAL:
    case IDL_TOKEN_COMMENT:
    case IDL_TOKEN_LINE_COMMENT:
      if (str == buf && !(str = idl_strdup(str)))
        return IDL_RETCODE_NO_MEMORY;
      tok->value.str = str;
      break;
    case IDL_TOKEN_CHAR_LITERAL:
      if (len != 1) {
        idl_error(pstate, &lex->location, "invalid character constant");
        if (str != buf)
          idl_free(str);
        return IDL_RETCODE_SYNTAX_ERROR;
      }
      tok->value.chr = *str;
      /* fall through */
    default:
      if (str != buf)
        idl_free(str);
      break;
  }

  tok->code = code;
  tok->location = lex->location;
  return code;
}

idl_retcode_t
idl_scan(idl_pstate_t *pstate, idl_token_t *tok)
{
  idl_retcode_t code;
  idl_lexeme_t lex;

  if ((code = scan(pstate, &lex)) < 0) {
    return code;
  /* tokenize. sanitize by removing line continuation, etc */
  } else if ((code = tokenize(pstate, &lex, code, tok)) < 0) {
    /* revert pstate on memory allocation failure */
    pstate->scanner.position = lex.location.first;
    return code;
  }

  // FIXME: verify state is correct, i.e. reset if it does not match token

  return code;
}
