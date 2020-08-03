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
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "idl.h"
#include "parser.h" /* Bison tokens */

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"

/* treat every cr+lf, lf+cr, cr, lf sequence as a single newline */
static int32_t
have_newline(idl_processor_t *proc, const char *cur)
{
  if (cur == proc->scanner.limit)
    return proc->flags & IDL_WRITE ? -2 : 0;
  assert(cur < proc->scanner.limit);
  if (cur[0] == '\n') {
    if (cur < proc->scanner.limit - 1)
      return cur[1] == '\r' ? 2 : 1;
    return proc->flags & IDL_WRITE ? -1 : 1;
  } else if (cur[0] == '\r') {
    if (cur < proc->scanner.limit - 1)
      return cur[1] == '\n' ? 2 : 1;
    return proc->flags & IDL_WRITE ? -1 : 1;
  }
  return 0;
}

static int32_t
have_skip(idl_processor_t *proc, const char *cur)
{
  int cnt = 0;
  if (cur == proc->scanner.limit)
    return proc->flags & IDL_WRITE ? -3 : 0;
  assert(cur < proc->scanner.limit);
  if (*cur == '\\' && (cnt = have_newline(proc, cur + 1)) > 0)
    cnt++;
  return cnt;
}

static int32_t
have_space(idl_processor_t *proc, const char *cur)
{
  if (cur == proc->scanner.limit)
    return proc->flags & IDL_WRITE ? -2 : 0;
  assert(cur < proc->scanner.limit);
  if (*cur == ' ' || *cur == '\t' || *cur == '\f' || *cur == '\v')
    return 1;
  return have_newline(proc, cur);
}

static int32_t
have_digit(idl_processor_t *proc, const char *cur)
{
  if (cur == proc->scanner.limit)
    return proc->flags & IDL_WRITE ? -1 : 0;
  assert(cur < proc->scanner.limit);
  return (*cur >= '0' && *cur <= '9');
}

static int32_t
have_alpha(idl_processor_t *proc, const char *cur)
{
  if (cur == proc->scanner.limit)
    return proc->flags & IDL_WRITE ? -1 : 0;
  assert(cur < proc->scanner.limit);
  return (*cur >= 'a' && *cur <= 'z') ||
         (*cur >= 'A' && *cur <= 'Z') ||
         (*cur == '_');
}

static int32_t
have_alnum(idl_processor_t *proc, const char *cur)
{
  if (cur == proc->scanner.limit)
    return proc->flags & IDL_WRITE ? -1 : 0;
  assert(cur < proc->scanner.limit);
  return (*cur >= 'a' && *cur <= 'z') ||
         (*cur >= 'A' && *cur <= 'Z') ||
         (*cur >= '0' && *cur <= '9') ||
         (*cur == '_');
}

static void
error(idl_processor_t *proc, const char *cur, const char *fmt, ...)
{
  int cnt;
  const char *ptr = proc->scanner.cursor;
  idl_location_t loc;
  va_list ap;

  /* determine exact location */
  loc.first = proc->scanner.position;
  loc.last = (idl_position_t){NULL, 0, 0};
  while (ptr < cur) {
    if ((cnt = have_newline(proc, ptr)) > 0) {
      loc.first.line++;
      loc.first.column = 0;
      ptr += cnt;
    } else {
      ptr++;
    }
    loc.first.column++;
  }

  va_start(ap, fmt);
  idl_verror(proc, &loc, fmt, ap);
  va_end(ap);
}

static const char *
next(idl_processor_t *proc, const char *cur)
{
  int cnt;

  /* might be positioned at newline */
  if ((cnt = have_newline(proc, cur))) {
    if (cnt < 0)
      return proc->scanner.limit;
    cur += (size_t)cnt;
  } else {
    cur++; /* skip to next character */
  }
  /* skip if positioned at line continuation sequence */
  for (; (cnt = have_skip(proc, cur)) > 0; cur += (size_t)cnt) ;

  return cnt < 0 ? proc->scanner.limit : cur;
}

static const char *
move(idl_processor_t *proc, const char *cur)
{
  int cnt;
  const char *ptr;

  assert(cur >= proc->scanner.cursor && cur <= proc->scanner.limit);
  for (ptr = proc->scanner.cursor; ptr < cur; ptr += cnt) {
    if ((cnt = have_newline(proc, ptr)) > 0) {
      proc->scanner.position.line++;
      proc->scanner.position.column = 1;
    } else {
      cnt = 1;
      proc->scanner.position.column++;
    }
  }
  proc->scanner.cursor = cur;
  return cur;
}

static int32_t
peek(idl_processor_t *proc, const char *cur)
{
  int cnt;

  /* skip if positioned at line continuation sequences */
  for (; (cnt = have_skip(proc, cur)) > 0; cur += cnt) ;

  if (cnt < 0 || cur == proc->scanner.limit)
    return '\0';
  return *cur;
}

static int32_t
have(idl_processor_t *proc, const char *cur, const char *str)
{
  int cnt;
  size_t len, pos;
  const char *lim = cur;

  for (pos = 0, len = strlen(str); pos < len; pos++, lim++) {
    /* skip any line continuation sequences */
    while ((cnt = have_skip(proc, lim)) > 0)
      lim += cnt;
    if (cnt < 0)
      return cnt - (int)(len - pos);
    if (str[pos] != *lim)
      return 0;
  }

  return (int)(lim - cur);
}

static int32_t
need_refill(idl_processor_t *proc, const char *cur)
{
  return have_skip(proc, cur) < 0;
}

static int32_t
scan_line_comment(idl_processor_t *proc, const char *cur, const char **lim)
{
  int cnt = 0;

  cur = next(proc, cur);
  while ((cur = next(proc, cur)) < proc->scanner.limit) {
    if ((cnt = have_newline(proc, cur)))
      break;
  }

  if (need_refill(proc, cur))
    return IDL_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_LINE_COMMENT;
}

static int32_t
scan_comment(idl_processor_t *proc, const char *cur, const char **lim)
{
  enum { initial, escape, asterisk, slash } state = initial;

  cur = next(proc, cur);
  while (state != slash && (cur = next(proc, cur)) < proc->scanner.limit) {
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
    assert(cur < proc->scanner.limit);
    *lim = cur + 1;
    return IDL_TOKEN_COMMENT;
  } else if (need_refill(proc, cur)) {
    return IDL_NEED_REFILL;
  }
  error(proc, cur, "unterminated comment");
  return IDL_SCAN_ERROR;
}

static int
scan_quoted_literal(
  idl_processor_t *proc, const char *cur, const char **lim, int quot)
{
  int cnt, esc = 0;
  int code = quot == '"' ? IDL_TOKEN_STRING_LITERAL : IDL_TOKEN_CHAR_LITERAL;
  const char *type = quot == '"' ? "string" : "char";

  while ((cur = next(proc, cur)) < proc->scanner.limit) {
    if (esc) {
      esc = 0;
    } else {
      if (*cur == '\\') {
        esc = 1;
      } else if (*cur == quot) {
        *lim = cur + 1;
        return code;
      } else if ((cnt = have_newline(proc, cur))) {
        break;
      }
    }
  }

  if (need_refill(proc, cur))
    return IDL_NEED_REFILL;
  *lim = cur;
  error(proc, cur, "unterminated %s literal", type);
  return IDL_SCAN_ERROR;
}

const char oct[] = "01234567";
const char dec[] = "0123456789";
const char hex[] = "0123456789abcdefABCDEF";

static int
scan_integer_literal(idl_processor_t *proc, const char *cur, const char **lim)
{
  int32_t chr = peek(proc, cur);
  const char *base;
  if (chr >= '1' && chr <= '9') {
    base = dec;
  } else {
    assert(chr == '0');
    chr = peek(proc, next(proc, cur));
    if (chr == 'x' || chr == 'X') {
      cur = next(proc, cur); /* skip x */
      base = hex;
    } else {
      base = oct;
    }
  }

  while ((cur = next(proc, cur)) < proc->scanner.limit) {
    chr = peek(proc, cur);
    if (!strchr(base, chr))
      break;
  }

  if (need_refill(proc, cur))
    return IDL_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_INTEGER_LITERAL;
}

static int32_t
scan_pp_number(idl_processor_t *proc, const char *cur, const char **lim)
{
  int cnt;

  if (*cur == '.')
    cur = next(proc, cur);
  for (; (cur = next(proc, cur)) < proc->scanner.limit; cur += cnt) {
    if ((cnt = have_digit(proc, cur)) <= 0)
      break;
  }

  if (need_refill(proc, cur))
    return IDL_NEED_REFILL;
  *lim = cur;
  return IDL_TOKEN_PP_NUMBER;
}

static int32_t
scan_identifier(idl_processor_t *proc, const char *cur, const char **lim)
{
  int cnt = 0;
  const char *end;

  for (end = cur; cur < proc->scanner.limit; end = cur) {
    /* skip over any line continuation sequences */
    for (; (cnt = have_skip(proc, cur)) > 0; cur += cnt) ;

    if (cnt < 0 || cur == proc->scanner.limit)
      break;
    else if (*cur == '_')
      cnt = 1;
    else if ((cnt = have_alnum(proc, cur)) <= 0)
      break;
    cur += cnt;
  }

  if (cnt < 0)
    return IDL_NEED_REFILL;
  /* detect if scope is attached to identifier if scanning code */
  if (((unsigned)proc->state & (unsigned)IDL_SCAN_CODE) &&
      (cnt = have(proc, cur, "::")) < 0)
    return IDL_NEED_REFILL;
  if (cnt > 0)
    proc->state = IDL_SCAN_SCOPED_NAME;
  *lim = end;
  return IDL_TOKEN_IDENTIFIER;
}

/* grammer for IDL (>=4.0) is incorrect (or at least ambiguous). blanks,
   horizontal and vertical tabs, newlines, form feeds, and comments
   (collective, "white space") are ignored except as they serve to separate
   tokens. the specification does not clearly state if white space may occur
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
scan_scope(idl_processor_t *proc, const char *cur, const char **lim)
{
  int cnt;

  cnt = have(proc, cur, "::");
  assert(cnt > 0);

  cur += cnt;
  *lim = cur;

  /* skip over any line continuation sequences */
  for (; (cnt = have_skip(proc, cur)) > 0; cur += cnt) ;

  if (cnt < 0)
    return IDL_NEED_REFILL;

  if ((*cur >= 'a' && *cur <= 'z') ||
      (*cur >= 'A' && *cur <= 'Z') ||
      (*cur == '_'))
  {
    if (proc->state == IDL_SCAN_SCOPED_NAME)
      return IDL_TOKEN_SCOPE_LR;
    else
      return IDL_TOKEN_SCOPE_R;
  } else {
    if (proc->state == IDL_SCAN_SCOPED_NAME)
      return IDL_TOKEN_SCOPE_L;
    else
      return IDL_TOKEN_SCOPE;
  }
}

int32_t
idl_lex(idl_processor_t *proc, idl_lexeme_t *lex)
{
  int chr, cnt, code = '\0';
  const char *cur, *lim = proc->scanner.cursor;

  do {
    /* skip over any line continuation sequences */
    for (; (cnt = have_skip(proc, lim)) > 0; lim += cnt) ;

    move(proc, lim);
    lex->location.first = proc->scanner.position;
    lex->marker = cur = lim;

    if (need_refill(proc, lim))
      return IDL_NEED_REFILL;

    chr = peek(proc, cur);
    if (chr == '\0') {
      assert(cur == proc->scanner.limit);
      break;
    } else if (have(proc, cur, "/*") > 0) {
      code = scan_comment(proc, cur, &lim);
    } else if (have(proc, cur, "//") > 0) {
      code = scan_line_comment(proc, cur, &lim);
    } else if (chr == '\'' || chr == '\"') {
      code = scan_quoted_literal(proc, cur, &lim, chr);
    } else if ((cnt = have_newline(proc, cur)) > 0) {
      lim = cur + cnt;
      code = '\n';
    } else if ((cnt = have_space(proc, cur)) > 0) {
      /* skip space characters, except newline */
      lim = cur + cnt;
    } else if ((unsigned)proc->state & (unsigned)IDL_SCAN_DIRECTIVE) {
      /*
       * preprocessor
       */
      if (chr == '.' && have_digit(proc, next(proc, cur))) {
        code = scan_pp_number(proc, cur, &lim);
      } else if ((cnt = have_alpha(proc, cur)) || chr == '_') {
        code = scan_identifier(proc, cur, &lim);
      } else if ((cnt = have_digit(proc, cur))) {
        code = scan_pp_number(proc, cur, &lim);
      } else {
        lim = cur + 1;
        code = (unsigned char)*cur;
      }
    } else if ((unsigned)proc->state & (unsigned)IDL_SCAN_CODE) {
      /*
       * interface definition language
       */
      if (have_digit(proc, cur)) {
        /* stroll takes care of decimal vs. octal vs. hexadecimal */
        code = scan_integer_literal(proc, cur, &lim);
      } else if (have_alpha(proc, cur) || chr == '_') {
        code = scan_identifier(proc, cur, &lim);
      } else if ((cnt = have(proc, cur, "::")) > 0) {
        code = scan_scope(proc, cur, &lim);
      } else if (chr == '@') {
        if ((cnt = have(proc, next(proc, cur), "::")) ||
            (cnt = have(proc, next(proc, cur), "_")) ||
            (cnt = have_alpha(proc, next(proc, cur))))
          code = cnt < 0 ? IDL_NEED_REFILL : IDL_TOKEN_AT;
        else
          code = (unsigned char)*cur;
        lim = cur + 1;
      } else {
        lim = cur + 1;
        code = (unsigned char)*cur;
      }
    } else if (chr == '#') {
      proc->state = IDL_SCAN_DIRECTIVE;
      lim = cur + 1;
      code = (unsigned char)*cur;
    } else {
      proc->state = IDL_SCAN_CODE;
    }
  } while (code == '\0');

  move(proc, lim);
  lex->limit = lim;
  lex->location.last = proc->scanner.position;

  if (proc->state == IDL_SCAN_SCOPED_NAME && code != IDL_TOKEN_IDENTIFIER)
    proc->state = IDL_SCAN_CODE;

  return code;
}

static int32_t
tokenize(
  idl_processor_t *proc, idl_lexeme_t *lex, int32_t code, idl_token_t *tok)
{
  int cnt, quot = '\'';
  char buf[32], *str = buf;
  size_t len, pos = 0;

  if (code < 256) {
    /* short circuit if token is a single character */
    tok->code = code;
    tok->location = lex->location;
    tok->value.chr = code;
    return code;
  }
  len = (size_t)((uintptr_t)lex->limit - (uintptr_t)lex->marker);
  if (len >= sizeof(buf) && !(str = ddsrt_malloc(len + 1)))
    return IDL_MEMORY_EXHAUSTED;

  /* strip line continuation sequences */
  for (const char *ptr = lex->marker; ptr < lex->limit; ) {
    if ((cnt = have_skip(proc, ptr)) > 0) {
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
    case IDL_TOKEN_IDENTIFIER:
      /* preprocessor identifiers are different from idl identifiers */
      if ((unsigned)proc->state & (unsigned)IDL_SCAN_DIRECTIVE)
        break;
      code = idl_istoken(str, 0);
      if (code == 0)
        code = IDL_TOKEN_IDENTIFIER;
      break;
    case IDL_TOKEN_STRING_LITERAL:
      quot = '\"';
      /* fall through */
    case IDL_TOKEN_CHAR_LITERAL:
      assert(str[0] == quot);
      len -= 1 + (size_t)(str[len - 1] == quot);
      memmove(str, str + 1, len);
      str[len] = '\0';
      break;
    case IDL_TOKEN_INTEGER_LITERAL: {
      char *end = NULL;
      ddsrt_strtoull(str, &end, 0, &tok->value.ullng);
      assert(end && *end == '\0');
    } break;
    default:
      break;
  }

  switch (code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_PP_NUMBER:
    case IDL_TOKEN_STRING_LITERAL:
    case IDL_TOKEN_CHAR_LITERAL:
      if (str == buf && !(str = ddsrt_strdup(str)))
        return IDL_MEMORY_EXHAUSTED;
      tok->value.str = str;
      break;
    default:
      if (str != buf)
        ddsrt_free(str);
      break;
  }

  tok->code = code;
  tok->location = lex->location;
  return tok->code;
}

int32_t
idl_scan(idl_processor_t *proc, idl_token_t *tok)
{
  int code;
  idl_lexeme_t lex;

  switch ((code = idl_lex(proc, &lex))) {
    case IDL_NEED_REFILL:
    case IDL_SCAN_ERROR:
      return code;
    default:
      /* tokenize. sanitize by removing line continuation, etc */
      if ((code = tokenize(proc, &lex, code, tok)) == IDL_MEMORY_EXHAUSTED) {
        /* revert state on memory allocation failure */
        proc->scanner.position = lex.location.first;
      }
      break;
  }

  return code;
}
