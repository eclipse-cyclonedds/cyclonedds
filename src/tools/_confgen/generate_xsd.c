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
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "_confgen.h"

static const char *xlatxsd(const char *str, const char **end)
{
  const char *sub;

  if (*str == '&') /* ampersand */
    sub = "&amp;";
  else if (*str == '<') /* less-than sign */
    sub = "&lt;";
  else if (*str == '>') /* greater-than sign */
    sub ="&gt;";
  else
    return NULL;

  *end = ++str;
  return sub;
}

static const char *isbuiltintopic(const struct cfgelem *elem)
{
  if (strcmp(elem->meta.type, "bool") == 0) {
    return "boolean";
  } else if (strcmp(elem->meta.type, "int") == 0) {
    return "integer";
  } else if (strcmp(elem->meta.type, "string") == 0) {
    return "string";
  }
  return NULL;
}

static int iscomplextype(const struct cfgelem *elem)
{
  return haschildren(elem) != 0 || hasattributes(elem) != 0 || isgroup(elem);
}

static void
printdesc(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  (void)flags;
  (void)units;
  if (!elem->description)
    return;
  assert(elem->meta.description);
  printspc(out, cols+0, "<xs:annotation>\n");
  printspc(out, cols+2, "<xs:documentation>\n");
  fputs(elem->meta.description, out);
  fputs("</xs:documentation>\n", out);
  printspc(out, cols+0, "</xs:annotation>\n");
}

static void
printenum(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  (void)flags;
  (void)units;
  printspc(out, cols+0, "<xs:simpleType>\n");
  printspc(out, cols+2, "<xs:restriction base=\"xs:token\">\n");
  for(const char **v = elem->meta.values; v && *v; v++) {
    printspc(out, cols+4, "<xs:enumeration value=\"%s\"/>\n", *v);
  }
  printspc(out, cols+2, "</xs:restriction>\n");
  printspc(out, cols+0, "</xs:simpleType>\n");
}

static void
printlist(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  (void)flags;
  (void)units;
  printspc(out, cols+0, "<xs:simpleType>\n");
  printspc(out, cols+2, "<xs:restriction base=\"xs:token\">\n");
  printspc(out, cols+4, "<xs:pattern value=\"%s\"/>\n", elem->meta.pattern);
  printspc(out, cols+2, "</xs:restriction>\n");
  printspc(out, cols+0, "</xs:simpleType>\n");
}

static void
printattr(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  const char fmt[] = "<xs:attribute name=\"%s\"%s>\n";
  char type[64], required[32];

  (void)flags;
  (void)units;
  assert(elem != NULL);
  assert(!haschildren(elem));
  if (ismoved(elem) || isdeprecated(elem) || isnop(elem))
    return;

  type[0] = '\0';
  if (elem->meta.unit)
    snprintf(type, sizeof(type), " type=\"config:%s\"", elem->meta.unit);
  else if (!isstring(elem))
    snprintf(type, sizeof(type), " type=\"xs:%s\"", isbuiltintopic(elem));

  required[0] = '\0';
  if (minimum(elem))
    snprintf(type, sizeof(type), " use=\"required\"");

  printspc(out, cols, fmt, name(elem), type, required);
  printdesc(out, cols+2, flags, elem, units);
  printspc(out, cols, "</xs:attribute>\n");
}

static void printelem(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units);

static void
printref(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  if (elem->meta.flags & FLAG_EXPAND) {
    printelem(out, cols, flags | FLAG_REFERENCE, elem, units);
  } else {
    char minattr[32] = "";
    char maxattr[32] = "";
    const char fmt[] = "<xs:element %s%sref=\"%s:%s\"/>\n";
    if (!(flags & FLAG_NOMIN) && minimum(elem) != 1)
      snprintf(minattr, sizeof(minattr), "minOccurs=\"%d\" ", minimum(elem));
    if (!(flags & FLAG_NOMAX) && maximum(elem) == 0)
      snprintf(maxattr, sizeof(maxattr), "maxOccurs=\"unbounded\" ");
    else if (!(flags & FLAG_NOMAX) && maximum(elem) != 1)
      snprintf(maxattr, sizeof(maxattr), "maxOccurs=\"%d\" ", maximum(elem));
    printspc(out, cols, fmt, minattr, maxattr, schema(), name(elem));
  }
}

static void
printcomplextype(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  char minattr[32] = "";
  char maxattr[32] = "";

  assert(!ismoved(elem) && !isdeprecated(elem));

  if (flags & FLAG_REFERENCE) {
    if (!(flags & FLAG_NOMIN) && minimum(elem) != 1)
      snprintf(minattr, sizeof(minattr), "minOccurs=\"%d\" ", minimum(elem));
    if (!(flags & FLAG_NOMAX) && maximum(elem) == 0)
      snprintf(maxattr, sizeof(maxattr), "maxOccurs=\"unbounded\" ");
    else if (!(flags & FLAG_NOMAX) && maximum(elem) != 1)
      snprintf(maxattr, sizeof(maxattr), "maxOccurs=\"%d\" ", maximum(elem));
  }

  printspc(out, cols, "<xs:element %s%sname=\"%s\">\n", minattr, maxattr, name(elem));
  printdesc(out, cols+2, flags, elem, units);

  flags &= ~(FLAG_NOMIN | FLAG_NOMAX);
  if (!haschildren(elem) && !hasattributes(elem)) {
    /* special case, group has only deprecated children and/or attributes */
    printspc(out, cols+2, "<xs:complexType/>\n");
  } else {
    int cnt;
    unsigned int ofst = 0;
    printspc(out, cols+2, "<xs:complexType>\n");

    if ((cnt = haschildren(elem))) {
      const char *cont = NULL;
      struct cfgelem *ce;
      int min[3], max[3];
      int mineq, maxeq;
      assert(isgroup(elem));

      minattr[0] = '\0';
      maxattr[0] = '\0';

      if (cnt == 1) {
        cont = "sequence";
      } else {
        assert(cnt > 1);
        ce = firstelem(elem->children);
        assert(ce != NULL);
        min[0] = min[1] = min[2] = minimum(ce);
        max[0] = max[1] = max[2] = maximum(ce);
        assert(min[1] <= max[1] || max[1] == 0);
        mineq = maxeq = 1;
        ce = nextelem(elem->children, ce);
        assert(ce);
        while (ce) {
          min[1] = minimum(ce);
          max[1] = maximum(ce);
          assert(min[1] <= max[1] || max[1] == 0);
          min[2] += minimum(ce);
          max[2] += maximum(ce);
          if (min[1] != min[0]) {
            mineq = 0;
            if (min[1] > min[0])
              min[0] = min[1];
          }
          if (max[1] != max[0]) {
            maxeq = 0;
            if ((max[0] != 0 && max[1] > max[0]) || max[1] == 0)
              max[0] = max[1];
          }
          ce = nextelem(elem->children, ce);
        }
        /* xsd generation becomes significantly more difficult if the minimum
           number of occurences for an element is more non-zero and the
           maximum number of occurences of it (or one of its siblings) is more
           than one, but that is not likely to occur */
        if (min[0] <= 1 && max[0] == 1) {
          /* any order, each zero or one time */
          cont = "all";
        } else {
          cont = "choice";
          if (min[0] == 0)
            snprintf(minattr, sizeof(minattr), " minOccurs=\"0\"");
          else if (min[0] != 1) /* incorrect, but make the most of it */
            snprintf(minattr, sizeof(minattr), " minOccurs=\"%d\"", min[2]);
          if (max[0] == 0)
            snprintf(maxattr, sizeof(maxattr), " maxOccurs=\"unbounded\"");
          else if (max[0] != 1)
            snprintf(maxattr, sizeof(maxattr), " maxOccurs=\"%d\"", max[2]);
          if (mineq)
            flags |= FLAG_NOMIN;
          if (maxeq)
            flags |= FLAG_NOMAX;
        }
      }

      printspc(out, cols+4, "<xs:%s%s%s>\n", cont, minattr, maxattr);
      ce = firstelem(elem->children);
      while (ce) {
        printref(out, cols+6, flags, ce, units);
        ce = nextelem(elem->children, ce);
      }
      printspc(out, cols+4, "</xs:%s>\n", cont);
    } else if (!isgroup(elem) && (!isstring(elem) || elem->meta.unit)) {
      ofst = 4;
      printspc(out, cols+4, "<xs:simpleContent>\n");
      if (isenum(elem) || islist(elem)) {
        printspc(out, cols+6, "<xs:restriction base=\"xs:anyType\">\n");
        if (isenum(elem))
          printenum(out, cols+8, flags, elem, units);
        else
          printlist(out, cols+8, flags, elem, units);
      } else {
        const char extfmt[] = "<xs:extension base=\"%s:%s\">\n";
        if (elem->meta.unit)
          printspc(out, cols+6, extfmt, schema(), elem->meta.unit);
        else
          printspc(out, cols+6, extfmt, "xs", isbuiltintopic(elem));
      }
    }
    flags &= ~(FLAG_NOMIN | FLAG_NOMAX);
    if (hasattributes(elem)) {
      struct cfgelem *ce;
      ce = firstelem(elem->attributes);
      while (ce) {
        printattr(out, cols+ofst+4, flags, ce, units);
        ce = nextelem(elem->attributes, ce);
      }
    }
    if (!isgroup(elem) && (!isstring(elem) || elem->meta.unit)) {
      if (isenum(elem) || islist(elem))
        printspc(out, cols+6, "</xs:restriction>\n");
      else
        printspc(out, cols+6, "</xs:extension>\n");
      printspc(out, cols+4, "</xs:simpleContent>\n");
    }
    printspc(out, cols+2, "</xs:complexType>\n");
  }
  printspc(out, cols, "</xs:element>\n");
}

static void
printsimpletype(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  char min[32] = "";
  char max[32] = "";
  const char *type;
  const char fmt[] = "<xs:element %s%sname=\"%s\">\n";
  const char builtinfmt[] = "<xs:element %s%sname=\"%s\" type=\"%s:%s\">\n";

  assert(!ismoved(elem) && !isdeprecated(elem));

  if (flags & FLAG_REFERENCE) {
    if (minimum(elem) != 1)
      snprintf(min, sizeof(min), "minOccurs=\"%d\" ", minimum(elem));
    if (maximum(elem) == 0)
      snprintf(max, sizeof(max), "maxOccurs=\"unbounded\" ");
    else if (maximum(elem) != 1)
      snprintf(max, sizeof(max), "maxOccurs=\"%d\" ", maximum(elem));
  }

  if (!(type = isbuiltintopic(elem)))
    printspc(out, cols, fmt, min, max, name(elem));
  else if (elem->meta.unit)
    printspc(out, cols, builtinfmt, min, max, name(elem), schema(), elem->meta.unit);
  else
    printspc(out, cols, builtinfmt, min, max, name(elem), "xs", type);
  printdesc(out, cols+2, flags, elem, units);

  if (isenum(elem))
    printenum(out, cols+2, flags, elem, units);
  else if (islist(elem))
    printlist(out, cols+2, flags, elem, units);

  printspc(out, cols, "</xs:element>\n");
}

static void
printelem(
  FILE *out,
  unsigned int cols,
  unsigned int flags,
  const struct cfgelem *elem,
  const struct cfgunit *units)
{
  struct cfgelem *ce;

  assert(!(ismoved(elem) || isdeprecated(elem)));

  if (!(elem->meta.flags & FLAG_DUPLICATE) && (flags & FLAG_EXPAND)) {
    if (iscomplextype(elem))
      printcomplextype(out, cols, flags, elem, units);
    else
      printsimpletype(out, cols, flags, elem, units);
  }

  if (!(flags & FLAG_REFERENCE)) {
    ce = firstelem(elem->children);
    while (ce) {
      if (ce->meta.flags & FLAG_EXPAND)
        flags &= ~FLAG_EXPAND;
      else
        flags |= FLAG_EXPAND;
      printelem(out, cols, flags, ce, units);
      ce = nextelem(elem->children, ce);
    }
  }
}

static int initxsd(struct cfgelem *elem, const struct cfgunit *units)
{
  if (ismoved(elem) || isdeprecated(elem))
    return 0;
  if (makedescription(elem, units, &xlatxsd) == -1)
    return -1;
  if (makepattern(elem, units) == -1)
    return -1;
  for (struct cfgelem *ce = elem->children; ce && ce->name; ce++) {
    if (initxsd(ce, units) == -1)
      return -1;
  }
  for (struct cfgelem *ce = elem->attributes; ce && ce->name; ce++) {
    if (initxsd(ce, units) == -1)
      return -1;
  }

  return 0;
}

int printxsd(FILE *out, struct cfgelem *elem, const struct cfgunit *units)
{
  if (initxsd(elem, units) == -1)
    return -1;
  printspc(out, 0, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
  printspc(out, 0, "<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\" "
    "elementFormDefault=\"qualified\" targetNamespace=\"%s\" xmlns:%s=\"%s\">\n",
    url(), schema(), url());
  printelem(out, 2, FLAG_EXPAND, elem, units);
  for (const struct cfgunit *cu = units; cu->name; cu++) {
    printspc(out, 2, "<xs:simpleType name=\"%s\">\n", cu->name);
    printspc(out, 4, "<xs:restriction base=\"xs:token\">\n");
    printspc(out, 6, "<xs:pattern value=\"%s\"/>\n", cu->pattern);
    printspc(out, 4, "</xs:restriction>\n");
    printspc(out, 2, "</xs:simpleType>\n");
  }
  printspc(out, 0, "</xs:schema>\n");
  return 0;
}
