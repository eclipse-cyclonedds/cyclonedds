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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_confgen.h"
#include "dds/ddsrt/heap.h"

#define FLAG_AMP (1u<<0)
#define FLAG_ROOT (1u<<1)

static const char *amp[] = { "", "& " };
static const char docfmt[] = "%s[ a:documentation [ xml:lang=\"en\" \"\"\"\n";
static const char elemfmt[] = "%selement %s {\n";
static const char attrfmt[] = "%sattribute %s {\n";

static const char *suffix(const struct cfgelem *elem)
{
  if (minimum(elem) == 0)
    return maximum(elem) == 1 ? "?" : "*";
  else
    return maximum(elem) == 1 ? ""  : "+";
}

static void
printtype(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  (void)flags;
  (void)units;
  if (strcmp(elem->meta.type, "string") == 0) {
    if (elem->meta.unit != NULL) {
      printspc(out, cols, "%s%s\n", amp[(flags & FLAG_AMP)], elem->meta.unit);
    } else {
      printspc(out, cols, "%stext\n", amp[(flags & FLAG_AMP)]);
    }
  } else if (strcmp(elem->meta.type, "bool") == 0) {
    printspc(out, cols, "%sxsd:boolean\n", amp[(flags & FLAG_AMP)]);
  } else if (strcmp(elem->meta.type, "int") == 0) {
    printspc(out, cols, "%sxsd:integer\n", amp[(flags & FLAG_AMP)]);
  } else if (strcmp(elem->meta.type, "enum") == 0) {
    assert(elem->meta.pattern != NULL);
    printspc(out, cols, "%s%s\n", amp[(flags & FLAG_AMP)], elem->meta.pattern);
  } else if (strcmp(elem->meta.type, "list") == 0) {
    assert(elem->meta.pattern != NULL);
    printspc(out, cols, "%sxsd:token { pattern = \"%s\" }\n", amp[(flags & FLAG_AMP)], elem->meta.pattern);
  } else {
    printspc(out, cols, "%sempty\n", amp[(flags & FLAG_AMP)]);
  }
}

static void
printattr(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  assert(!ismoved(elem) && !isdeprecated(elem));
  if (elem->description != NULL) {
    printspc(out, cols, docfmt, amp[(flags & FLAG_AMP)]);
    fputs(elem->meta.description, out);
    printspc(out, 0, "\"\"\" ] ]\n");
    flags &= ~FLAG_AMP;
  }
  printspc(out, cols, attrfmt, amp[(flags & FLAG_AMP)], name(elem));
  printtype(out, cols+2, flags, elem, units);
  printspc(out, cols, "}%s\n", suffix(elem));
}

static void printelem(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  struct cfgelem *elem,
  const struct cfgunit *units)
{
  struct cfgelem *ce;

  assert(!ismoved(elem) && !isdeprecated(elem));

  if (elem->description != NULL) {
    printspc(out, cols, docfmt, amp[(flags & FLAG_AMP)]);
    fputs(elem->meta.description, out);
    printspc(out, 0, "\"\"\" ] ]\n");
    flags &= ~FLAG_AMP;
  }
  printspc(out, cols, elemfmt, amp[(flags & FLAG_AMP)], name(elem));
  flags &= ~FLAG_AMP;
  ce = firstelem(elem->attributes);
  while (ce) {
    if (!isnop(ce)) {
      printattr(out, cols+2, flags & ~FLAG_ROOT, ce, units);
      flags |= FLAG_AMP;
    }
    ce = nextelem(elem->attributes, ce);
  }
  if (haschildren(elem)) {
    ce = firstelem(elem->children);
    while (ce) {
      printelem(out, cols+2, flags & ~FLAG_ROOT, ce, units);
      flags |= FLAG_AMP;
      ce = nextelem(elem->children, ce);
    }
  } else if (!hasattributes(elem) ||
             !(isgroup(elem) || (isstring(elem) && !elem->meta.unit)))
  {
    printtype(out, cols+2, flags, elem, units);
  }
  printspc(out, cols, "}%s\n", (flags & FLAG_ROOT) ? "" : suffix(elem));
}

static int initrnc(struct cfgelem *elem, const struct cfgunit *units)
{
  if (ismoved(elem) || isdeprecated(elem))
    return 0;
  if (makedescription(elem, units, 0) == -1)
    return -1;
  if (makepattern(elem, units) == -1)
    return -1;
  for (struct cfgelem *ce = elem->children; ce && ce->name; ce++) {
    if (initrnc(ce, units) == -1)
      return -1;
  }
  for (struct cfgelem *ce = elem->attributes; ce && ce->name; ce++) {
    if (initrnc(ce, units) == -1)
      return -1;
  }
  return 0;
}

int printrnc(FILE *out, struct cfgelem *elem, const struct cfgunit *units)
{
  if (initrnc(elem, units) == -1)
    return -1;
  printspc(out, 0, "default namespace = \"%s\"\n", url());
  printspc(out, 0, "namespace a = \"http://relaxng.org/ns/compatibility/annotations/1.0\"\n");
  printspc(out, 0, "grammar {\n");
  printspc(out, 0, "  start =\n");
  printelem(out, 2, FLAG_ROOT, elem, units);
  for(const struct cfgunit *cu = units; cu->name; cu++) {
    static const char *fmt = "  %s = xsd:token { pattern = \"%s\" }\n";
    printspc(out, 0, fmt, cu->name, cu->pattern);
  }
  printspc(out, 0, "}\n");
  return 0;
}
