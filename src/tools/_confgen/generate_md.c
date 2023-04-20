// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "_confgen.h"

#include "dds/ddsrt/heap.h"

static const char *xlatmd(const char *str, const char **end)
{
  static struct { const char *search; const char *replace; } tr[] = {
    { "<p>", "" }, { "</p>", "" },
    { "<b>", "" }, { "</b>", "" },
    { "<i>", "" }, { "</i>", "" },
    { "<li>", " * " }, { "</li>", "\n" },
    { "<ul>", "" }, { "</ul>", "" },
    { "<sup>", "^" }, { "</sup>", "" },
    { "<code>", "`" }, { "</code>", "`" },
    { "&lt;empty&gt;", "<empty>" },
    { "*", "\\*" }, { "_", "\\_" },
    { NULL, NULL }
  };

  /* replace </p>\s*<p> by double newline */
  if (strncmp(str, "</p>", 4) == 0) {
    const char *ptr = str + 4;
    while (*ptr == '\n' || *ptr == ' ') ptr++;
    if (strncmp(ptr, "<p>", 3) == 0) {
      *end = ptr;
      return "\n\n";
    }
  }

  for (size_t cnt = 0; tr[cnt].search; cnt++) {
    size_t len = strlen(tr[cnt].search);
    if (strncmp(str, tr[cnt].search, len) == 0) {
      *end = str + len;
      return tr[cnt].replace;
    }
  }

  return NULL;
}

static char hashes[16];

static void printhead(
  FILE *out,
  unsigned int level,
  unsigned int flags,
  struct cfgelem *elem,
  const struct cfgunit *units)
{
  (void)level;
  (void)flags;
  (void)units;
  assert(level < (sizeof(hashes) - 1));
  hashes[level+1] = '\0';
  fprintf(out, "%s %s\n", hashes, elem->meta.title);
  hashes[level+1] = '#';
}

static void printlink(
  FILE *out,
  unsigned int level,
  unsigned int flags,
  struct cfgelem *elem,
  const struct cfgunit *units)
{
  int chr;
  (void)level;
  (void)flags;
  (void)units;
  assert(elem->meta.title);
  fputc('#', out);
  for (const char *ptr = elem->meta.title; *ptr; ptr++) {
    chr = (unsigned char)*ptr;
    if (chr >= 'A' && chr <= 'Z')
      chr = (chr - 'A') + 'a';
    if (chr >= 'a' && chr <= 'z')
      fputc(chr, out);
  }
}

static void printtype(
  FILE *out,
  unsigned int level,
  unsigned int flags,
  struct cfgelem *elem,
  const struct cfgunit *units)
{
  (void)level;
  (void)flags;
  (void)units;
  assert(!isgroup(elem));
  if (isbool(elem)) {
    fputs("Boolean\n", out);
  } else if (islist(elem)) {
    assert(elem->meta.values);
    fputs("One of:\n", out);
    if (elem->value && strlen(elem->value))
      fprintf(out, "* Keyword: %s\n", elem->value);
    fputs("* Comma-separated list of: ", out);
    for (const char **v = elem->meta.values; *v; v++) {
      fprintf(out, "%s%s", v == elem->meta.values ? "" : ", ", *v);
    }
    fputs("\n", out);
    if (!elem->value || !strlen(elem->value))
      fputs("* Or empty\n", out);
  } else if (isenum(elem)) {
    assert(elem->meta.values);
    fputs("One of: ", out);
    for (const char **v = elem->meta.values; *v; v++) {
      fprintf(out, "%s%s", v == elem->meta.values ? "" : ", ", *v);
    }
    fputs("\n", out);
  } else if (isint(elem)) {
    fputs("Integer\n", out);
  } else if (elem->meta.unit) {
    fputs("Number-with-unit\n", out);
  } else if (isstring(elem)) {
    fputs("Text\n", out);
  }
}

#define FLAG_LF (1u<<0)

static void printattr(
  FILE *out,
  unsigned int level,
  unsigned int flags,
  struct cfgelem *elem,
  const struct cfgunit *units)
{
  if (flags & FLAG_LF)
    fputs("\n\n", out);
  printhead(out, level, flags, elem, units);
  printtype(out, level, flags, elem, units);
  fputs("\n", out);
  if (elem->description) {
    fputs(elem->meta.description, out);
    fputs("\n", out);
  }
}

static void printelem(
  FILE *out,
  unsigned int level,
  unsigned int flags,
  struct cfgelem *elem,
  const struct cfgunit *units)
{
  if (flags & FLAG_LF)
    fputs("\n\n", out);
  printhead(out, level, flags, elem, units);
  flags &= ~FLAG_LF;
  if (hasattributes(elem)) {
    int cnt = 0;
    const char *sep = "Attributes: ";
    struct cfgelem *ce = firstelem(elem->attributes);
    while (ce) {
      if (!isnop(ce)) {
        fprintf(out, "%s[%s](", sep, name(ce));
        printlink(out, level, flags, ce, units);
        fprintf(out, ")");
        sep = ", ";
        cnt++;
      }
      ce = nextelem(elem->attributes, ce);
    }
    if (cnt != 0) {
      fputs("\n", out);
      flags |= FLAG_LF;
    }
  }
  if (haschildren(elem)) {
    const char *sep = "Children: ";
    struct cfgelem *ce = firstelem(elem->children);
    while (ce) {
      fprintf(out, "%s[%s](", sep, name(ce));
      printlink(out, level, flags, ce, units);
      fprintf(out, ")");
      sep = ", ";
      ce = nextelem(elem->children, ce);
    }
    fputs("\n", out);
    flags |= FLAG_LF;
  } else if (!isgroup(elem)) {
    if (flags & FLAG_LF)
      fputs("\n", out);
    printtype(out, level+1, flags, elem, units);
    flags |= FLAG_LF;
  }
  if (elem->description) {
    if (flags & FLAG_LF)
      fputs("\n", out);
    fputs(elem->meta.description, out);
    fputs("\n", out);
  }
  if (hasattributes(elem)) {
    struct cfgelem *ce = firstelem(elem->attributes);
    while (ce) {
      if (!isnop(ce))
        printattr(out, level, flags, ce, units);
      ce = nextelem(elem->attributes, ce);
    }
  }
  if (isgroup(elem)) {
    struct cfgelem *ce = firstelem(elem->children);
    while (ce) {
      printelem(out, level+1, flags, ce, units);
      ce = nextelem(elem->children, ce);
    }
  }
}

static int maketitles(
  struct cfgelem *elem, int attr, const char *path, size_t pathlen)
{
  char *str;
  size_t namelen = 0, len = 0;
  struct cfgelem *ce;
  if (ismoved(elem) || isdeprecated(elem) || elem->meta.title)
    return 0;
  namelen = strlen(name(elem));
  if (!(str = ddsrt_malloc(pathlen + namelen + (attr ? 4 : 2))))
    return -1;
  memcpy(str, path, pathlen);
  len += pathlen;
  if (attr) {
    str[len++] = '[';
    str[len++] = '@';
  } else {
    str[len++] = '/';
  }
  memcpy(str+len, name(elem), namelen);
  len += namelen;
  if (attr) {
    str[len++] = ']';
  }
  str[len] = '\0';
  elem->meta.title = str;
  ce = firstelem(elem->children);
  while (ce) {
    if (maketitles(ce, 0, str, len) == -1)
      return -1;
    ce = nextelem(elem->children, ce);
  }
  ce = firstelem(elem->attributes);
  while (ce) {
    if (maketitles(ce, 1, str, len) == -1)
      return -1;
    ce = nextelem(elem->attributes, ce);
  }
  return 0;
}

static int initmd(struct cfgelem *elem, const struct cfgunit *units)
{
  if (ismoved(elem) || isdeprecated(elem))
    return 0;
  if (makedescription(elem, units, xlatmd) == -1)
    return -1;
  for (struct cfgelem *ce = elem->children; ce && ce->name; ce++) {
    if (initmd(ce, units) == -1)
      return -1;
  }
  for (struct cfgelem *ce = elem->attributes; ce && ce->name; ce++) {
    if (initmd(ce, units) == - 1)
      return -1;
  }
  return 0;
}

int printmd(FILE *out, struct cfgelem *elem, const struct cfgunit *units)
{
  if (initmd(elem, units) == -1)
    return -1;
  if (maketitles(elem, 0, "/", 1) == -1)
    return -1;
  memset(hashes, '#', sizeof(hashes));
  printelem(out, 0u, 0u, elem, units);
  return 0;
}
