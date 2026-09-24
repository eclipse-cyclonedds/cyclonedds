// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/regex.h"

#define PATCH_NONE (-1)

struct regex_parser {
  ddsrt_regex_t *regex;
  const char *pat;
  dds_return_t rc;
};

struct regex_frag {
  int start;
  int outs;
};

struct regex_match_workspace {
  int *cur;
  int *next;
  int *stack;
  unsigned char *cur_seen;
  unsigned char *next_seen;
};

static int
patch_ref(
  int state,
  int out)
{
  assert(state >= 0);
  assert(out == 0 || out == 1);
  return 2 * state + out;
}

static int *
patch_field(
  ddsrt_regex_t *regex,
  int ref)
{
  ddsrt_regex_state_t *st;

  assert(ref >= 0);
  st = &regex->states[ref / 2];
  return (ref & 1) ? &st->out2 : &st->out1;
}

static int
patch_list1(
  ddsrt_regex_t *regex,
  int ref)
{
  *patch_field(regex, ref) = PATCH_NONE;
  return ref;
}

static int
patch_append(
  ddsrt_regex_t *regex,
  int a,
  int b)
{
  int ref;

  if (a == PATCH_NONE)
    return b;
  if (b == PATCH_NONE)
    return a;

  ref = a;
  while (*patch_field(regex, ref) != PATCH_NONE)
    ref = *patch_field(regex, ref);
  *patch_field(regex, ref) = b;
  return a;
}

static void
patch(
  ddsrt_regex_t *regex,
  int list,
  int target)
{
  while (list != PATCH_NONE) {
    int *field = patch_field(regex, list);
    int next = *field;
    *field = target;
    list = next;
  }
}

static void
set_error(
  struct regex_parser *p,
  dds_return_t rc)
{
  if (p->rc == DDS_RETCODE_OK)
    p->rc = rc;
}

static int
emit_state(
  struct regex_parser *p,
  ddsrt_regex_opcode_t op)
{
  ddsrt_regex_t *regex = p->regex;
  ddsrt_regex_state_t *st;
  size_t idx;

  if (regex->nstates >= regex->maxstates) {
    set_error(p, DDS_RETCODE_NOT_ENOUGH_SPACE);
    return -1;
  }

  idx = regex->nstates++;
  assert(idx <= (size_t) INT_MAX / 2);
  st = &regex->states[idx];
  memset(st, 0, sizeof(*st));
  st->op = op;
  st->out1 = -1;
  st->out2 = -1;
  return (int) idx;
}

static struct regex_frag
make_empty(
  struct regex_parser *p)
{
  struct regex_frag frag = { -1, PATCH_NONE };
  int jump = emit_state(p, DDSRT_REGEX_OP_JUMP);

  if (jump >= 0) {
    frag.start = jump;
    frag.outs = patch_list1(p->regex, patch_ref(jump, 0));
  }
  return frag;
}

static void
class_set(
  unsigned char *cls,
  unsigned char ch)
{
  cls[ch >> 3] = (unsigned char) (cls[ch >> 3] | (1u << (ch & 7)));
}

static bool
class_get(
  const unsigned char *cls,
  unsigned char ch)
{
  return (cls[ch >> 3] & (1u << (ch & 7))) != 0;
}

static unsigned char
decode_escape(
  unsigned char ch)
{
  switch (ch) {
    case 'n':
      return '\n';
    case 'r':
      return '\r';
    case 't':
      return '\t';
    default:
      return ch;
  }
}

static bool parse_alt(struct regex_parser *p, struct regex_frag *frag);

static bool
parse_class_char(
  struct regex_parser *p,
  unsigned char *ch)
{
  if (*p->pat == '\0') {
    set_error(p, DDS_RETCODE_BAD_PARAMETER);
    return false;
  } else if (*p->pat == '\\') {
    p->pat++;
    if (*p->pat == '\0') {
      set_error(p, DDS_RETCODE_BAD_PARAMETER);
      return false;
    }
    *ch = decode_escape((unsigned char) *p->pat++);
  } else {
    *ch = (unsigned char) *p->pat++;
  }
  return true;
}

static bool
parse_class(
  struct regex_parser *p,
  struct regex_frag *frag)
{
  int state;
  bool first = true;
  ddsrt_regex_state_t *st;

  state = emit_state(p, DDSRT_REGEX_OP_CLASS);
  if (state < 0)
    return false;
  st = &p->regex->states[state];

  if (*p->pat == '^') {
    st->invert_class = true;
    p->pat++;
  }

  for (;;) {
    unsigned char lo, hi;

    if (*p->pat == '\0') {
      set_error(p, DDS_RETCODE_BAD_PARAMETER);
      return false;
    }
    if (*p->pat == ']' && !first) {
      p->pat++;
      break;
    }

    if (!parse_class_char(p, &lo))
      return false;
    first = false;

    if (*p->pat == '-' && p->pat[1] != '\0' && p->pat[1] != ']') {
      p->pat++;
      if (!parse_class_char(p, &hi))
        return false;
      if (lo > hi) {
        set_error(p, DDS_RETCODE_BAD_PARAMETER);
        return false;
      }
      for (unsigned c = lo; c <= hi; c++)
        class_set(st->cls, (unsigned char) c);
    } else {
      class_set(st->cls, lo);
    }
  }

  frag->start = state;
  frag->outs = patch_list1(p->regex, patch_ref(state, 0));
  return true;
}

static bool
is_atom_start(
  char ch)
{
  switch (ch) {
    case '\0':
    case ')':
    case '|':
    case '*':
    case '+':
    case '?':
      return false;
    default:
      return true;
  }
}

static bool
parse_atom(
  struct regex_parser *p,
  struct regex_frag *frag)
{
  int state;

  switch (*p->pat) {
    case '(':
      p->pat++;
      if (!parse_alt(p, frag))
        return false;
      if (*p->pat != ')') {
        set_error(p, DDS_RETCODE_BAD_PARAMETER);
        return false;
      }
      p->pat++;
      return true;
    case '[':
      p->pat++;
      return parse_class(p, frag);
    case '.':
      p->pat++;
      state = emit_state(p, DDSRT_REGEX_OP_ANY);
      break;
    case '^':
      p->pat++;
      state = emit_state(p, DDSRT_REGEX_OP_BOL);
      break;
    case '$':
      p->pat++;
      state = emit_state(p, DDSRT_REGEX_OP_EOL);
      break;
    case '\\':
      p->pat++;
      if (*p->pat == '\0') {
        set_error(p, DDS_RETCODE_BAD_PARAMETER);
        return false;
      }
      state = emit_state(p, DDSRT_REGEX_OP_CHAR);
      if (state >= 0)
        p->regex->states[state].ch = decode_escape((unsigned char) *p->pat++);
      break;
    default:
      if (!is_atom_start(*p->pat)) {
        set_error(p, DDS_RETCODE_BAD_PARAMETER);
        return false;
      }
      state = emit_state(p, DDSRT_REGEX_OP_CHAR);
      if (state >= 0)
        p->regex->states[state].ch = (unsigned char) *p->pat++;
      break;
  }

  if (state < 0)
    return false;
  frag->start = state;
  frag->outs = patch_list1(p->regex, patch_ref(state, 0));
  return true;
}

static bool
parse_repeat(
  struct regex_parser *p,
  struct regex_frag *frag)
{
  if (!parse_atom(p, frag))
    return false;

  switch (*p->pat) {
    case '*': {
      int split;
      p->pat++;
      split = emit_state(p, DDSRT_REGEX_OP_SPLIT);
      if (split < 0)
        return false;
      p->regex->states[split].out1 = frag->start;
      patch(p->regex, frag->outs, split);
      frag->start = split;
      frag->outs = patch_list1(p->regex, patch_ref(split, 1));
      break;
    }
    case '+': {
      int split;
      p->pat++;
      split = emit_state(p, DDSRT_REGEX_OP_SPLIT);
      if (split < 0)
        return false;
      p->regex->states[split].out1 = frag->start;
      patch(p->regex, frag->outs, split);
      frag->outs = patch_list1(p->regex, patch_ref(split, 1));
      break;
    }
    case '?': {
      int split;
      int split_out;
      p->pat++;
      split = emit_state(p, DDSRT_REGEX_OP_SPLIT);
      if (split < 0)
        return false;
      p->regex->states[split].out1 = frag->start;
      split_out = patch_list1(p->regex, patch_ref(split, 1));
      frag->start = split;
      frag->outs = patch_append(p->regex, frag->outs, split_out);
      break;
    }
    default:
      break;
  }

  if (*p->pat == '*' || *p->pat == '+' || *p->pat == '?') {
    set_error(p, DDS_RETCODE_BAD_PARAMETER);
    return false;
  }
  return true;
}

static bool
parse_concat(
  struct regex_parser *p,
  struct regex_frag *frag)
{
  bool have_frag = false;

  while (is_atom_start(*p->pat)) {
    struct regex_frag next;

    if (!parse_repeat(p, &next))
      return false;

    if (!have_frag) {
      *frag = next;
      have_frag = true;
    } else {
      patch(p->regex, frag->outs, next.start);
      frag->outs = next.outs;
    }
  }

  if (!have_frag)
    *frag = make_empty(p);
  return p->rc == DDS_RETCODE_OK;
}

static bool
parse_alt(
  struct regex_parser *p,
  struct regex_frag *frag)
{
  if (!parse_concat(p, frag))
    return false;

  while (*p->pat == '|') {
    struct regex_frag rhs;
    int split;

    p->pat++;
    if (!parse_concat(p, &rhs))
      return false;

    split = emit_state(p, DDSRT_REGEX_OP_SPLIT);
    if (split < 0)
      return false;
    p->regex->states[split].out1 = frag->start;
    p->regex->states[split].out2 = rhs.start;
    frag->start = split;
    frag->outs = patch_append(p->regex, frag->outs, rhs.outs);
  }

  return true;
}

dds_return_t
ddsrt_regex_compile_with_storage(
  ddsrt_regex_t *regex,
  const char *pattern,
  ddsrt_regex_state_t *states,
  size_t nstates)
{
  struct regex_parser p;
  struct regex_frag frag;
  int match;

  if (regex == NULL || pattern == NULL || states == NULL || nstates == 0)
    return DDS_RETCODE_BAD_PARAMETER;
  if (nstates > (size_t) INT_MAX / 2)
    return DDS_RETCODE_BAD_PARAMETER;

  regex->states = states;
  regex->nstates = 0;
  regex->maxstates = nstates;
  regex->start = -1;
  regex->owns_states = false;

  p.regex = regex;
  p.pat = pattern;
  p.rc = DDS_RETCODE_OK;

  if (!parse_alt(&p, &frag)) {
    regex->nstates = 0;
    return p.rc;
  }
  if (*p.pat != '\0') {
    regex->nstates = 0;
    return DDS_RETCODE_BAD_PARAMETER;
  }

  match = emit_state(&p, DDSRT_REGEX_OP_MATCH);
  if (match < 0) {
    regex->nstates = 0;
    return p.rc;
  }
  patch(regex, frag.outs, match);
  regex->start = frag.start;
  return DDS_RETCODE_OK;
}

dds_return_t
ddsrt_regex_compile(
  ddsrt_regex_t *regex,
  const char *pattern)
{
  ddsrt_regex_state_t *states;
  dds_return_t rc;
  size_t len;
  size_t nstates;

  if (regex == NULL || pattern == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  len = strlen(pattern);
  if (len > (((size_t) INT_MAX / 2) - 2u) / 2u)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  nstates = 2u * len + 2u;
  if (nstates > SIZE_MAX / sizeof(*states))
    return DDS_RETCODE_OUT_OF_RESOURCES;
  states = ddsrt_malloc_s(nstates * sizeof(*states));
  if (states == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  rc = ddsrt_regex_compile_with_storage(regex, pattern, states, nstates);
  if (rc == DDS_RETCODE_OK) {
    regex->owns_states = true;
  } else {
    ddsrt_free(states);
    regex->states = NULL;
    regex->maxstates = 0;
    regex->start = -1;
  }
  return rc;
}

void
ddsrt_regex_fini(
  ddsrt_regex_t *regex)
{
  if (regex->owns_states)
    ddsrt_free(regex->states);
  regex->states = NULL;
  regex->nstates = 0;
  regex->maxstates = 0;
  regex->start = -1;
  regex->owns_states = false;
}

static void
add_state(
  const ddsrt_regex_t *regex,
  int *list,
  size_t *nlist,
  int *stack,
  unsigned char *seen,
  int stateidx,
  size_t pos,
  size_t len,
  bool *matched)
{
  size_t nstack = 0;

  if (stateidx < 0)
    return;
#define PUSH_STATE(idx_) do { \
    const int idx__ = (idx_); \
    if (idx__ >= 0) { \
      assert((size_t) idx__ < regex->nstates); \
      if (!seen[idx__]) { \
        assert(nstack < regex->nstates); \
        seen[idx__] = 1; \
        stack[nstack++] = idx__; \
      } \
    } \
  } while (0)

  PUSH_STATE(stateidx);

  while (nstack > 0) {
    const ddsrt_regex_state_t *st;
    int idx = stack[--nstack];

    assert(idx >= 0);
    assert((size_t) idx < regex->nstates);
    st = &regex->states[idx];

    switch (st->op) {
      case DDSRT_REGEX_OP_SPLIT:
        PUSH_STATE(st->out1);
        PUSH_STATE(st->out2);
        break;
      case DDSRT_REGEX_OP_JUMP:
        PUSH_STATE(st->out1);
        break;
      case DDSRT_REGEX_OP_BOL:
        if (pos == 0)
          PUSH_STATE(st->out1);
        break;
      case DDSRT_REGEX_OP_EOL:
        if (pos == len)
          PUSH_STATE(st->out1);
        break;
      case DDSRT_REGEX_OP_MATCH:
        *matched = true;
        break;
      case DDSRT_REGEX_OP_CHAR:
      case DDSRT_REGEX_OP_ANY:
      case DDSRT_REGEX_OP_CLASS:
        list[(*nlist)++] = idx;
        break;
      case DDSRT_REGEX_OP_UNUSED:
        break;
    }
  }

#undef PUSH_STATE
}

static bool
state_matches(
  const ddsrt_regex_state_t *st,
  unsigned char ch)
{
  switch (st->op) {
    case DDSRT_REGEX_OP_CHAR:
      return st->ch == ch;
    case DDSRT_REGEX_OP_ANY:
      return true;
    case DDSRT_REGEX_OP_CLASS: {
      bool in_class = class_get(st->cls, ch);
      return st->invert_class ? !in_class : in_class;
    }
    default:
      return false;
  }
}

static bool
match_from(
  const ddsrt_regex_t *regex,
  const char *str,
  size_t len,
  size_t start,
  bool full,
  struct regex_match_workspace *ws)
{
  int *cur = ws->cur;
  int *next = ws->next;
  unsigned char *cur_seen = ws->cur_seen;
  unsigned char *next_seen = ws->next_seen;
  size_t ncur = 0;
  bool matched = false;

  memset(cur_seen, 0, regex->nstates);
  add_state(regex, cur, &ncur, ws->stack, cur_seen, regex->start, start, len, &matched);
  if (matched && (!full || start == len))
    return true;

  for (size_t pos = start; pos < len && ncur > 0; pos++) {
    size_t nnext = 0;
    bool next_matched = false;
    unsigned char ch = (unsigned char) str[pos];

    memset(next_seen, 0, regex->nstates);
    for (size_t i = 0; i < ncur; i++) {
      const ddsrt_regex_state_t *st = &regex->states[cur[i]];
      if (state_matches(st, ch))
        add_state(regex, next, &nnext, ws->stack, next_seen, st->out1, pos + 1, len, &next_matched);
    }

    if (next_matched && (!full || pos + 1 == len))
      return true;

    {
      int *tmp_list = cur;
      unsigned char *tmp_seen = cur_seen;
      cur = next;
      next = tmp_list;
      cur_seen = next_seen;
      next_seen = tmp_seen;
      ncur = nnext;
    }
  }

  return false;
}

static void
alloc_workspace(
  struct regex_match_workspace *ws,
  size_t nstates)
{
  ws->cur = ddsrt_malloc(nstates * sizeof(*ws->cur));
  ws->next = ddsrt_malloc(nstates * sizeof(*ws->next));
  ws->stack = ddsrt_malloc(nstates * sizeof(*ws->stack));
  ws->cur_seen = ddsrt_malloc(nstates);
  ws->next_seen = ddsrt_malloc(nstates);
}

static void
free_workspace(
  struct regex_match_workspace *ws)
{
  ddsrt_free(ws->next_seen);
  ddsrt_free(ws->cur_seen);
  ddsrt_free(ws->stack);
  ddsrt_free(ws->next);
  ddsrt_free(ws->cur);
}

bool
ddsrt_regex_match(
  const ddsrt_regex_t *regex,
  const char *str)
{
  struct regex_match_workspace ws;
  bool result;

  assert(regex != NULL);
  assert(str != NULL);
  assert(regex->states != NULL);
  assert(regex->start >= 0);

  alloc_workspace(&ws, regex->nstates);
  result = match_from(regex, str, strlen(str), 0, true, &ws);
  free_workspace(&ws);
  return result;
}

bool
ddsrt_regex_search(
  const ddsrt_regex_t *regex,
  const char *str)
{
  struct regex_match_workspace ws;
  size_t len;
  bool result = false;

  assert(regex != NULL);
  assert(str != NULL);
  assert(regex->states != NULL);
  assert(regex->start >= 0);

  len = strlen(str);
  alloc_workspace(&ws, regex->nstates);
  for (size_t start = 0; start <= len; start++) {
    if (match_from(regex, str, len, start, false, &ws)) {
      result = true;
      break;
    }
  }
  free_workspace(&ws);
  return result;
}
