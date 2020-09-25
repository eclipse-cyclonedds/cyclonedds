#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>

#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/q_config.h"

struct cfgmeta {
  char *name;
  char *title;
  char *pattern;
  char *description;
  unsigned int flags;
  const int force_maximum, maximum;
  const int force_minimum, minimum;
  const char *type;
  const char *unit;
  const char *range;
  const char **values;
};

struct cfgelem {
  const char *name;
  struct cfgelem *children;
  struct cfgelem *attributes;
  int multiplicity;
  const char *value;
  size_t elem_offset;
  const char *membername;
  void (*print) (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
  struct cfgmeta meta;
};

static void *cfg_address (void *parent, struct cfgelem const * const cfgelem)
{
  return (char *) parent + cfgelem->elem_offset;
}

static void *cfg_deref_address (void *parent, struct cfgelem const * const cfgelem)
{
  return *((void **) ((char *) parent + cfgelem->elem_offset));
}

static void gendef_pf_nop (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  (void) fp; (void) parent; (void) cfgelem;
}

static void gendef_pf_uint16 (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  const uint16_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (fp, "  cfg->%s = UINT16_C (%"PRIu16");\n", cfgelem->membername, *p);
}

static void gendef_pf_int32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  const int32_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (fp, "  cfg->%s = INT32_C (%"PRId32");\n", cfgelem->membername, *p);
}

static void gendef_pf_uint32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  const uint32_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (fp, "  cfg->%s = UINT32_C (%"PRIu32");\n", cfgelem->membername, *p);
}

static void gendef_pf_int64 (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  const int64_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (fp, "  cfg->%s = INT64_C (%"PRId64");\n", cfgelem->membername, *p);
}

static void gendef_pf_maybe_int32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  struct ddsi_config_maybe_int32 const * const p = cfg_address (parent, cfgelem);
  fprintf (fp, "  cfg->%s.isdefault = %d;\n", cfgelem->membername, p->isdefault);
  if (!p->isdefault)
    fprintf (fp, "  cfg->%s.value = INT32_C (%"PRId32");\n", cfgelem->membername, p->value);
}

static void gendef_pf_maybe_uint32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  struct ddsi_config_maybe_uint32 const * const p = cfg_address (parent, cfgelem);
  fprintf (fp, "  cfg->%s.isdefault = %d;\n", cfgelem->membername, p->isdefault);
  if (!p->isdefault)
    fprintf (fp, "  cfg->%s.value = UINT32_C (%"PRIu32");\n", cfgelem->membername, p->value);
}

#ifdef DDSI_INCLUDE_SSL
static void gendef_pf_min_tls_version (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  struct ddsi_config_ssl_min_version * const p = cfg_address (parent, cfgelem);
  if (p->major != 0 || p->minor != 0)
    fprintf (fp, "\
  cfg->%s.major = %d;\n\
  cfg->%s.minor = %d;\n",
             cfgelem->membername, p->major, cfgelem->membername, p->minor);
}
#endif

static void gendef_pf_string (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  const char **p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (fp, "  cfg->%s = \"%s\";\n", cfgelem->membername, *p);
}

static void gendef_pf_networkAddresses (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  char *** const p = cfg_address (parent, cfgelem);
  if (*p != 0)
  {
    int n = 0;
    for (int i = 0; (*p)[i] != NULL; i++)
      n++;
    fprintf (fp, "  static char *%s_init_[] = {\n", cfgelem->membername);
    for (int i = 0; (*p)[i] != NULL; i++)
      fprintf (fp, "    \"%s\",\n", (*p)[i]);
    fprintf (fp, "    NULL\n  };\n");
    fprintf (fp, "  cfg->%s = %s_init_;\n", cfgelem->membername, cfgelem->membername);
  }
}

static void gendef_pf_tracemask (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  /* tracemask is a bit bizarre: it has no member name ... all that has to do with Verbosity and Category
     existing both, and how it is output in the trace ... */
  const uint32_t *p = cfg_address (parent, cfgelem);
  assert (cfgelem->membername == NULL);
  if (*p != 0)
    fprintf (fp, "  cfg->tracemask = UINT32_C (%"PRIu32");\n", *p);
}

static void gendef_pf_xcheck (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (fp, parent, cfgelem);
}
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
static void gendef_pf_bandwidth (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (fp, parent, cfgelem);
}
#endif
static void gendef_pf_memsize (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (fp, parent, cfgelem);
}
static void gendef_pf_memsize16 (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint16 (fp, parent, cfgelem);
}
static void gendef_pf_networkAddress (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_string (fp, parent, cfgelem);
}
static void gendef_pf_allow_multicast(FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (fp, parent, cfgelem);
}
static void gendef_pf_maybe_memsize (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_maybe_uint32 (fp, parent, cfgelem);
}
static void gendef_pf_int (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  DDSRT_STATIC_ASSERT (sizeof (int) == sizeof (int32_t));
  gendef_pf_int32 (fp, parent, cfgelem);
}
static void gendef_pf_uint (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  DDSRT_STATIC_ASSERT (sizeof (unsigned) == sizeof (uint32_t));
  gendef_pf_uint32 (fp, parent, cfgelem);
}
static void gendef_pf_duration (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int64 (fp, parent, cfgelem);
}
static void gendef_pf_domainId(FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  (void) fp; (void) parent; (void) cfgelem;
  // skipped on purpose: set explicitly
}
static void gendef_pf_participantIndex (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_boolean (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_boolean_default (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_besmode (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_retransmit_merging (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_sched_class (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_transport_selector (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_many_sockets_mode (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}
static void gendef_pf_standards_conformance (FILE *fp, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (fp, parent, cfgelem);
}

/* configuration elements */
#define DEPRECATED(name) "|" name
#define MEMBER(name) offsetof (struct ddsi_config, name), #name
#define MEMBEROF(parent,name) 0, NULL /* default config for doesn't contain lists, so these aren't needed */

/* renaming print functions to use prefix gendef_pf so that the symbols are different from those in q_config.c
   (they have file-local scope, so this isn't required, but I am guessing it will be less confusing in the long
   run, even if it means that 0/NULL will get translated to gendef_0/gendef_NULL, which then need additional
   macros to convert them back to 0 ... */
#define gendef_0 0
#define gendef_NULL 0
#define FUNCTIONS(if, uf, ff, pf) gendef_##pf
#define DESCRIPTION(str) /* drop */
#define RANGE(str) .range = str
#define UNIT(str) .unit = str
#define VALUES(...) .values = (const char *[]){ __VA_ARGS__, NULL }
#define MAXIMUM(num) .force_maximum = 1, .maximum = num
#define MINIMUM(num) .force_minimum = 1, .minimum = num

#define NOMEMBER 0, NULL
#define NOFUNCTIONS 0
#define NOMETADATA { NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL }
#define END_MARKER { NULL, NULL, NULL, 0, NULL, 0, NULL, 0, NOMETADATA }

#define ELEMENT(name, elems, attrs, multip, dflt, ofst, mname, funcs, desc, ...) \
  { name, elems, attrs, multip, dflt, ofst, mname, funcs, { __VA_ARGS__ } }

#define MOVED(name, whereto) \
  { ">" name, NULL, NULL, 0, whereto, 0, NULL, 0, NOMETADATA }

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

#include "dds/ddsi/ddsi_cfgelems.h"
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

static void gen_defaults (FILE *fp, void *parent, struct cfgelem const * const cfgelem)
{
  for (const struct cfgelem *ce = cfgelem; ce && ce->name; ce++)
  {
    if (ce->name[0] == '>' || ce->name[0] == '|') /* moved or deprecated, so don't care */
      continue;

    if (ce->multiplicity <= 1)
    {
      if (ce->print)
        ce->print (fp, parent, ce);
      if (ce->children)
        gen_defaults (fp, parent, ce->children);
      if (ce->attributes)
        gen_defaults (fp, parent, ce->attributes);
    }
    else
    {
      struct ddsi_config_listelem *p = cfg_deref_address (parent, ce);
#if 1
      if (p != NULL)
        abort ();
#else // not all of the machinery for handling lists is in place
      while (p)
      {
        if (ce->print)
          ce->print (fp, p, ce);
        if (ce->attributes)
          gen_defaults (fp, p, ce->attributes);
        if (ce->children)
          gen_defaults (fp, p, ce->children);
        p = p->next;
      }
#endif
    }
  }
}

int main (int argc, char **argv)
{
  struct ddsi_config cfg;
  struct cfgst *cfgst;

  if (argc > 2)
  {
    fprintf (stderr, "usage: %s [OUTPUT]\n", argv[0]);
    return 2;
  }

  if ((cfgst = config_init ("", &cfg, 0)) == NULL)
  {
    fprintf (stderr, "Failed to initialize default configuration\n");
    return 1;
  }

  FILE *fp = (argc == 1) ? stdout : fopen (argv[1], "w");
  if (fp == NULL)
  {
    fprintf (stderr, "Failed to create output file\n");
    config_fini (cfgst);
    return 1;
  }

  fprintf (fp, "\
#include <string.h>\n\
#include <stdint.h>\n\
#include <inttypes.h>\n\
#include \"dds/ddsi/ddsi_config.h\"\n\
\n\
void ddsi_config_init_default (struct ddsi_config *cfg)\n\
{\n\
  memset (cfg, 0, sizeof (*cfg));\n");
  gen_defaults (fp, &cfg, cyclonedds_root_cfgelems);
  fprintf (fp, "}\n");

  if (fp != stdout)
    fclose (fp);

  config_fini (cfgst);
  return 0;
}
