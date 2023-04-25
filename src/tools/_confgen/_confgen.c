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
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"

/* ddsi_config.h provides the definition of "struct ddsi_config", which is needed for the offsetof macro */
#include "dds/ddsi/ddsi_config.h"

#include "_confgen.h"

/* configuration units */
#define UNIT(str, ...) { .name = str, __VA_ARGS__ }
#define DESCRIPTION(str) .description = str
#define PATTERN(str) .pattern = str
#define END_MARKER { NULL, NULL, NULL }

#include "ddsi__cfgunits.h"
/* undefine unit macros */
#undef UNIT
#undef DESCRIPTION
#undef PATTERN
#undef END_MARKER


/* configuration elements */
#define DEPRECATED(name) "|" name
#define MEMBER(name) offsetof (struct ddsi_config, name), #name
#define MEMBEROF(parent,name) 0, NULL /* default config for doesn't contain lists, so these aren't needed */
/* renaming print functions to use prefix gendef_pf so that the symbols are different from those in ddsi_config.c
   (they have file-local scope, so this isn't required, but I am guessing it will be less confusing in the long
   run, even if it means that 0/NULL will get translated to gendef_0/gendef_NULL, which then need additional
   macros to convert them back to 0 ... */
#define gendef_0 0
#define gendef_NULL 0
#define FUNCTIONS(if, uf, ff, pf) gendef_##pf
#define DESCRIPTION(str) .description = str
#define RANGE(str) .range = str
#define UNIT(str) .unit = str
#define VALUES(...) .values = (const char *[]){ __VA_ARGS__, NULL }
#define MAXIMUM(num) .force_maximum = 1, .maximum = num
#define MINIMUM(num) .force_minimum = 1, .minimum = num
#define BEHIND_FLAG(name) .flag = name

#define NOMEMBER 0, NULL
#define NOFUNCTIONS 0
#define NOMETADATA { NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL, NULL }
#define END_MARKER { NULL, NULL, NULL, 0, NULL, 0, NULL, 0, NULL, NOMETADATA }

#define ELEMENT(name, elems, attrs, multip, dflt, ofst, mname, funcs, desc, ...) \
  { name, elems, attrs, multip, dflt, ofst, mname, funcs, desc, { __VA_ARGS__ } }

#define MOVED(name, whereto) \
  { ">" name, NULL, NULL, 0, whereto, 0, NULL, 0, NULL, NOMETADATA}

#define EXPAND(macro, args) macro args /* Visual Studio */

#define NOP(name) \
  EXPAND(ELEMENT, (name, NULL, NULL, 1, NULL, 0, NULL, 0, NULL, .type = "nop"))
#define BOOL(name, attrs, multip, dflt, ofst, funcs, desc, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, ofst, funcs, desc, .type = "bool", __VA_ARGS__))
#define INT(name, attrs, multip, dflt, ofst, funcs, desc, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, ofst, funcs, desc, .type = "int", __VA_ARGS__))
#define ENUM(name, attrs, multip, dflt, ofst, funcs, desc, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, ofst, funcs, desc, .type = "enum", __VA_ARGS__))
#define STRING(name, attrs, multip, dflt, ofst, funcs, desc, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, ofst, funcs, desc, .type = "string", __VA_ARGS__))
#define LIST(name, attrs, multip, dflt, ofst, funcs, desc, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, ofst, funcs, desc, .type = "list", __VA_ARGS__))
#define GROUP(name, elems, attrs, multip, ofst, funcs, desc, ...) \
  EXPAND(ELEMENT, (name, elems, attrs, multip, NULL, ofst, funcs, desc, .type = "group", __VA_ARGS__))

#include "ddsi__cfgelems.h"
/* undefine element macros */
#undef DEPRECATED
#undef MEMBER
#undef MEMBEROF
#undef FUNCTIONS
#undef DESCRIPTION
#undef RANGE
#undef UNIT
#undef VALUES
#undef MAXIMUM
#undef MINIMUM
#undef NOMEMBER
#undef NOFUNCTIONS
#undef NOMETADATA
#undef END_MARKER
#undef ELEMENT
#undef MOVED
#undef NOP
#undef BOOL
#undef INT
#undef ENUM
#undef STRING
#undef LIST
#undef GROUP
#undef BEHIND_FLAG


const char *schema(void) { return "config"; }

const char *url(void) { return "https://cdds.io/config"; }

const char *name(const struct cfgelem *elem)
{
  if (elem->meta.name)
    return (const char *)elem->meta.name;
  return elem->name;
}

static char spaces[32];

void printspc(FILE *out, unsigned int cols, const char *fmt, ...)
{
  va_list ap;
  assert((size_t)cols < sizeof(spaces));
  spaces[cols] = '\0';
  fprintf(out, "%s", spaces);
  spaces[cols] = ' ';
  va_start(ap, fmt);
  vfprintf(out, fmt, ap);
  va_end(ap);
}

static void usage(const char *prog)
{
  static const char fmt[] = "usage: %s [OPTIONS] -f FORMAT\n";
  fprintf(stderr, fmt, prog);
}

static void help(const char *prog)
{
  static const char fmt[] = "\
usage: %s -f FORMAT\n\
\n\
OPTIONS:\n\
    -h           print help message\n\
    -f FORMAT    output format. one of md, rnc, rst or xsd\n\
    -o FILENAME  output file. specify - to use stdout\n\
";

  fprintf(stdout, fmt, prog);
}

struct cfgelem *nextelem(const struct cfgelem *list, const struct cfgelem *elem)
{
  const struct cfgelem *next = NULL;
  if (list) {
    const struct cfgelem *ce = list;
    /* find next lexicographic element in list */
    for (; ce && ce->name; ce++) {
      if (ismoved(ce) || isdeprecated(ce) || isnop(ce))
        continue;
      if ((!elem || strcmp(ce->name, elem->name) > 0) &&
          (!next || strcmp(ce->name, next->name) < 0))
        next = ce;
    }
  }
  return (struct cfgelem *)next;
}

struct cfgelem *firstelem(const struct cfgelem *list)
{
  return nextelem(list, NULL);
}

const struct cfgunit *findunit(const struct cfgunit *units, const char *name)
{
  for (const struct cfgunit *cu = units; cu->name; cu++) {
    if (strcmp(cu->name, name) == 0) {
      return cu;
    }
  }
  return NULL;
}

int ismoved(const struct cfgelem *elem)
{
  return elem && elem->name && elem->name[0] == '>';
}

int isdeprecated(const struct cfgelem *elem)
{
  return elem && elem->name && elem->name[0] == '|';
}

int isgroup(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "group") == 0;
}

int isnop(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "nop") == 0;
}

int isbool(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "bool") == 0;
}

int isint(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "int") == 0;
}

int isstring(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "string") == 0;
}

int isenum(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "enum") == 0;
}

int islist(const struct cfgelem *elem)
{
  return strcmp(elem->meta.type, "list") == 0;
}

int minimum(const struct cfgelem *elem)
{
  if (elem->meta.force_minimum) {
    return elem->meta.minimum;
  } else {
    switch (elem->multiplicity) {
      case 0: /* special case, treat as-if 1 */
      case 1: /* required if there is no default */
        if (isgroup(elem))
          return 0;
        return (!elem->value);
      default:
        return 0;
    }
  }
}

int maximum(const struct cfgelem *elem)
{
  if (elem->meta.force_maximum) {
    return elem->meta.maximum;
  } else {
    switch (elem->multiplicity) {
      case INT_MAX:
        return 0;
      case 0:
      case 1:
        return 1;
      default:
        return elem->multiplicity;
    }
  }
}

int haschildren(const struct cfgelem *elem)
{
  int cnt = 0;
  if (elem->children != NULL) {
    for (const struct cfgelem *e = elem->children; e->name; e++) {
      cnt += !(ismoved(e) || isdeprecated(e));
    }
  }
  return cnt;
}

int hasattributes(const struct cfgelem *elem)
{
  int cnt = 0;
  if (elem->attributes != NULL) {
    for (const struct cfgelem *e = elem->attributes; e->name; e++) {
      cnt += !(ismoved(e) || isdeprecated(e));
    }
  }
  return cnt;
}

/* remove element aliases "s/\|.*$//" */
static int sanitize_names(struct cfgelem *elem)
{
  char *end;
  if (!elem->name || ismoved(elem) || isdeprecated(elem))
    return 0;
  if ((end = strchr(elem->name, '|'))) {
    assert(!elem->meta.name);
    size_t len = (uintptr_t)end - (uintptr_t)elem->name;
    if (!(elem->meta.name = malloc(len + 1)))
      return -1;
    memcpy(elem->meta.name, elem->name, len);
    elem->meta.name[len] = '\0';
  }
  if (elem->children) {
    for (struct cfgelem *ce = elem->children; ce->name; ce++) {
      if (sanitize_names(ce) != 0)
        return -1;
    }
  }
  if (elem->attributes) {
    for (struct cfgelem *ce = elem->attributes; ce->name; ce++) {
      if (sanitize_names(ce) != 0)
        return -1;
    }
  }
  return 0;
}

static int generate_enum_pattern(struct cfgelem *elem)
{
  char *pat;
  const char **vals;
  size_t cnt = 0, len = 0, pos = 0, size = 0;
  assert(!elem->meta.pattern);
  assert(elem->meta.values);
  for (vals = elem->meta.values; *vals; vals++) {
    cnt++;
    size += strlen(*vals) + 2;
  }
  size += (cnt - 1) + 2 + 1;
  if (!(pat = malloc(size)))
    return -1;
  pat[pos++] = '(';
  for (vals = elem->meta.values; *vals; vals++) {
    if (vals != elem->meta.values)
      pat[pos++] = '|';
    pat[pos++] = '"';
    len = strlen(*vals);
    assert(pos < size - len);
    memcpy(pat+pos, *vals, len);
    pos += len;
    pat[pos++] = '"';
  }
  pat[pos++] = ')';
  pat[pos++] = '\0';
  assert(pos == size);
  elem->meta.pattern = pat;
  return 0;
}

static int generate_list_pattern(struct cfgelem *elem)
{
  char *pat = NULL, *lst;
  const char **vals, *val = (elem->value ? elem->value : "");
  size_t cnt = 0, len = 0, pos = 0, size = 0;
  assert(elem->meta.values != NULL);
  for (vals = elem->meta.values; *vals != NULL; vals++) {
    cnt++;
    size += strlen(*vals);
  }
  size += (cnt - 1) + 2 + 1;
  if (!(lst = malloc(size)))
    return -1;
  lst[pos++] = '(';
  for (vals = elem->meta.values; *vals != NULL; vals++) {
    if (vals != elem->meta.values)
      lst[pos++] = '|';
    len = strlen(*vals);
    assert(pos < size - len);
    memcpy(lst+pos, *vals, len);
    pos += len;
  }
  lst[pos++] = ')';
  lst[pos++] = '\0';
  assert(pos == size);
  size_t patsz = 8 + strlen(val) + 2 * strlen(lst);
  if ((pat = malloc (patsz)) == NULL)
    return -1;
  if (strlen(val) != 0)
    snprintf(pat, patsz, "%s|(%s(,%s)*)", val, lst, lst);
  else
    snprintf(pat, patsz, "(%s(,%s)*)|", lst, lst);
  free(lst);
  elem->meta.pattern = pat;
  return pat ? 0 : -1;
}

/* generate patterns for enum and list elements */
int makepattern(struct cfgelem *elem, const struct cfgunit *units)
{
  (void)units;
  if (!elem->meta.pattern) {
    if (isenum(elem))
      return generate_enum_pattern(elem);
    if (islist(elem))
      return generate_list_pattern(elem);
  }
  return 0;
}

static int compare(struct cfgelem *a, struct cfgelem *b)
{
  /* shallow compare (just pointer) is good enough for now */
  if (a->children != b->children)
    return -1;
  if (a->attributes != b->attributes)
    return -1;
  if (strcmp(a->meta.type, b->meta.type) != 0)
    return -1;
  return 0;
}

static int duplicate(struct cfgelem *curs, struct cfgelem *elem)
{
  if (curs == elem)
    return 0;
  if (curs->meta.flags & FLAG_DUPLICATE)
    return 0;
  if (elem->meta.flags & FLAG_DUPLICATE)
    return 0;
  if (strcmp(curs->name, elem->name) != 0)
    return 0;
  if (compare(curs, elem) != 0)
    return -1;
  curs->meta.flags |= FLAG_DUPLICATE;
  return 0;
}

static int expand(struct cfgelem *curs, struct cfgelem *elem)
{
  if (curs == elem)
    return 0;
  if (strcmp(curs->name, elem->name) != 0)
    return 0;
  curs->meta.flags &= ~FLAG_DUPLICATE;
  curs->meta.flags |= FLAG_EXPAND;
  elem->meta.flags |= FLAG_EXPAND;
  return 0;
}

static int walk(
  struct cfgelem *root,
  struct cfgelem *elem,
  int(*func)(struct cfgelem *, struct cfgelem *))
{
  struct cfgelem *ce;

  if (func(root, elem) == -1)
    return -1;

  ce = firstelem(root->children);
  while (ce) {
    if (walk(ce, elem, func) == -1)
      return -1;
    ce = nextelem(root->children, ce);
  }

  return 0;
}

static void mark(struct cfgelem *elem, struct cfgelem *root)
{
  struct cfgelem *ce;
  if (walk(root, elem, &duplicate) != 0)
    walk(root, elem, &expand);
  ce = firstelem(elem->children);
  while (ce) {
    mark(ce, root);
    ce = nextelem(elem->children, ce);
  }
}

#define BLOCK (1024)

static int
format(
  char **strp,
  size_t *lenp,
  size_t *posp,
  const char *src,
  const char *(*xlat)(const char *, const char **))
{
  char buf[2] = { '\0', '\0' };
  size_t nonspc, pos;
  const char *alt, *end, *ptr, *sub;

  nonspc = pos = *posp;
  for (ptr = src, end = ptr; *ptr; ptr = end) {
    if (xlat && (alt = xlat(ptr, &end))) {
      sub = alt;
    } else {
      buf[0] = *ptr;
      end = ptr + 1;
      sub = (const char *)buf;
    }
    for (size_t cnt = 0; sub[cnt]; cnt++) {
      if (pos == *lenp) {
        char *str;
        size_t len = *lenp + BLOCK;
        if (!(str = realloc(*strp, (len + 1) * sizeof(char)))) {
          if (*strp)
            free(*strp);
          return -1;
        }
        *strp = str;
        *lenp = len;
      }
      (*strp)[pos++] = sub[cnt];
    }
  }
  if (nonspc != *posp && nonspc < pos)
    pos = ++nonspc;
  (*strp)[pos] = '\0';
  (*posp) = pos;
  return 0;
}

#define DFLTFMT "<p>The default value is: <code>%s</code></p>"

int makedescription(
  struct cfgelem *elem,
  const struct cfgunit *units,
  const char *(*xlat)(const char *, const char **))
{
  if (elem->description) {
    char *src = NULL;
    char *dest = elem->meta.description;
    const char *dflt = "&lt;empty&gt;";
    size_t len = 0, pos = 0;
    const struct cfgunit *unit = NULL;

    if (isgroup(elem)) {
      src = strdup(elem->description);
    } else {
      if (elem->value)
      {
        if (strlen(elem->value) > 0)
          dflt = elem->value;
      }
      if (elem->meta.unit) {
        unit = findunit(units, elem->meta.unit);
        assert(unit);
        size_t srcsz = 3 + strlen(DFLTFMT) + strlen(elem->description) + strlen(unit->description) + strlen(dflt);
        if ((src = malloc (srcsz)) == NULL)
          return -1;
        snprintf(
          src, srcsz, "%s\n%s\n"DFLTFMT, elem->description, unit->description, dflt);
      } else {
        size_t srcsz = 2 + strlen(DFLTFMT) + strlen(elem->description) + strlen(dflt);
        if ((src = malloc (srcsz)) == NULL)
          return -1;
        snprintf(
          src, srcsz, "%s\n"DFLTFMT, elem->description, dflt);
      }
    }

    if (!src)
      return -1;
    format(&dest, &len, &pos, src, xlat);
    free(src);
    if (!dest)
      return -1;
    elem->meta.description = dest;
  }
  return 0;
}

static int init(struct cfgelem *root)
{
  if (sanitize_names(root) != 0)
    return -1;
  mark(root, root);
  return 0;
}

static void fini(struct cfgelem *elem)
{
  struct cfgelem *ce;
  if (!elem)
    return;
  if (elem->meta.name)
    free(elem->meta.name);
  if (elem->meta.title)
    free(elem->meta.title);
  if (elem->meta.pattern)
    free(elem->meta.pattern);
  if (elem->meta.description)
    free(elem->meta.description);
  elem->meta.name = NULL;
  elem->meta.title = NULL;
  elem->meta.pattern = NULL;
  elem->meta.description = NULL;
  ce = firstelem(elem->children);
  while (ce) {
    fini(ce);
    ce = nextelem(elem->children, ce);
  }
  ce = firstelem(elem->attributes);
  while (ce) {
    fini(ce);
    ce = nextelem(elem->attributes, ce);
  }
}

int main(int argc, char *argv[])
{
  int opt;
  int code = EXIT_FAILURE;
  FILE *out = NULL;
  const char *file = "-";
  enum { rnc, xsd, md, rst, defconfig } format = rnc;

  while ((opt = getopt(argc, argv, "f:o:h")) != -1) {
    switch (opt) {
      case 'f':
        if (strcmp(optarg, "rnc") == 0) {
          format = rnc;
        } else if (strcmp(optarg, "xsd") == 0) {
          format = xsd;
        } else if (strcmp(optarg, "md") == 0) {
          format = md;
        } else if (strcmp(optarg, "rst") == 0) {
          format = rst;
        } else if (strcmp(optarg, "defconfig") == 0) {
          format = defconfig;
        } else {
          fprintf(stderr, "illegal output format: %s\n", optarg);
          usage(argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
      case 'h':
        help(argv[0]);
        exit(0);
        break;
      case 'o':
        file = (const char *)optarg;
        break;
      default:
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (init(cyclonedds_root_cfgelems) == -1) {
    fprintf(stderr, "out of memory\n");
    goto exit_failure;
  }

  DDSRT_WARNING_MSVC_OFF(4996)
  if (strcmp(file, "-") == 0) {
    out = stdout;
  } else if ((out = fopen(file, "wb")) == NULL) {
    fprintf(stderr, "cannot open %s for writing\n", file);
    goto exit_failure;
  }
  DDSRT_WARNING_MSVC_ON(4996)

  memset(spaces, ' ', sizeof(spaces));

  switch (format) {
    case rnc:
      if (printrnc(out, cyclonedds_root_cfgelems, cfgunits) == 0)
        code = EXIT_SUCCESS;
      break;
    case xsd:
      if (printxsd(out, cyclonedds_root_cfgelems, cfgunits) == 0)
        code = EXIT_SUCCESS;
      break;
    case md:
      if (printmd(out, cyclonedds_root_cfgelems, cfgunits) == 0)
        code = EXIT_SUCCESS;
      break;
    case rst:
      if (printrst(out, cyclonedds_root_cfgelems, cfgunits) == 0)
        code = EXIT_SUCCESS;
      break;
    case defconfig:
      if (printdefconfig(out, cyclonedds_root_cfgelems) == 0)
        code = EXIT_SUCCESS;
      break;
  }

exit_failure:
  fini(cyclonedds_root_cfgelems);
  if (out != NULL && out != stdout)
    fclose(out);

  return code;
}
