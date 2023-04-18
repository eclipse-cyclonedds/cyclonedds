#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <inttypes.h>

#include "dds/ddsrt/static_assert.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsi/ddsi_config.h"
#include "dds/features.h"

#include "_confgen.h"

static void *cfg_address (void *parent, struct cfgelem const * const cfgelem)
{
  return (char *) parent + cfgelem->elem_offset;
}

static void *cfg_deref_address (void *parent, struct cfgelem const * const cfgelem)
{
  return *((void **) ((char *) parent + cfgelem->elem_offset));
}

void gendef_pf_nop (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  (void) out; (void) parent; (void) cfgelem;
}

void gendef_pf_uint16 (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  const uint16_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (out, "  cfg->%s = UINT16_C (%"PRIu16");\n", cfgelem->membername, *p);
}

void gendef_pf_int32 (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  const int32_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (out, "  cfg->%s = INT32_C (%"PRId32");\n", cfgelem->membername, *p);
}

void gendef_pf_uint32 (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  const uint32_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (out, "  cfg->%s = UINT32_C (%"PRIu32");\n", cfgelem->membername, *p);
}

void gendef_pf_int64 (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  const int64_t *p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (out, "  cfg->%s = INT64_C (%"PRId64");\n", cfgelem->membername, *p);
}

void gendef_pf_maybe_int32 (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  struct ddsi_config_maybe_int32 const * const p = cfg_address (parent, cfgelem);
  fprintf (out, "  cfg->%s.isdefault = %d;\n", cfgelem->membername, p->isdefault);
  if (!p->isdefault)
    fprintf (out, "  cfg->%s.value = INT32_C (%"PRId32");\n", cfgelem->membername, p->value);
}

void gendef_pf_maybe_uint32 (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  struct ddsi_config_maybe_uint32 const * const p = cfg_address (parent, cfgelem);
  fprintf (out, "  cfg->%s.isdefault = %d;\n", cfgelem->membername, p->isdefault);
  if (!p->isdefault)
    fprintf (out, "  cfg->%s.value = UINT32_C (%"PRIu32");\n", cfgelem->membername, p->value);
}

void gendef_pf_min_tls_version (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  struct ddsi_config_ssl_min_version * const p = cfg_address (parent, cfgelem);
  if (p->major != 0 || p->minor != 0)
    fprintf (out, "\
  cfg->%s.major = %d;\n\
  cfg->%s.minor = %d;\n",
             cfgelem->membername, p->major, cfgelem->membername, p->minor);
}

void gendef_pf_string (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  const char **p = cfg_address (parent, cfgelem);
  if (*p != 0)
    fprintf (out, "  cfg->%s = \"%s\";\n", cfgelem->membername, *p);
}

void gendef_pf_networkAddresses (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  char *** const p = cfg_address (parent, cfgelem);
  if (*p != 0)
  {
    fprintf (out, "  static char *%s_init_[] = {\n", cfgelem->membername);
    for (int i = 0; (*p)[i] != NULL; i++)
      fprintf (out, "    \"%s\",\n", (*p)[i]);
    fprintf (out, "    NULL\n  };\n");
    fprintf (out, "  cfg->%s = %s_init_;\n", cfgelem->membername, cfgelem->membername);
  }
}

void gendef_pf_tracemask (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  /* tracemask is a bit bizarre: it has no member name ... all that has to do with Verbosity and Category
     existing both, and how it is output in the trace ... */
  assert (cfgelem->membername == NULL);
  assert (cfgelem->elem_offset == 0);
  (void) cfgelem;
  const struct ddsi_config *cfg = parent;
  if (cfg->tracemask != 0)
    fprintf (out, "  cfg->tracemask = UINT32_C (%"PRIu32");\n", cfg->tracemask);
}

void gendef_pf_xcheck (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (out, parent, cfgelem);
}
void gendef_pf_bandwidth (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (out, parent, cfgelem);
}
void gendef_pf_memsize (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (out, parent, cfgelem);
}
void gendef_pf_memsize16 (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint16 (out, parent, cfgelem);
}
void gendef_pf_networkAddress (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_string (out, parent, cfgelem);
}
void gendef_pf_allow_multicast(FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_uint32 (out, parent, cfgelem);
}
void gendef_pf_maybe_memsize (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_maybe_uint32 (out, parent, cfgelem);
}
void gendef_pf_int (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  DDSRT_STATIC_ASSERT (sizeof (int) == sizeof (int32_t));
  gendef_pf_int32 (out, parent, cfgelem);
}
void gendef_pf_uint (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  DDSRT_STATIC_ASSERT (sizeof (unsigned) == sizeof (uint32_t));
  gendef_pf_uint32 (out, parent, cfgelem);
}
void gendef_pf_duration (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int64 (out, parent, cfgelem);
}
void gendef_pf_domainId(FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  (void) out; (void) parent; (void) cfgelem;
  // skipped on purpose: set explicitly
}
void gendef_pf_participantIndex (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_boolean (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_boolean_default (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_besmode (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_retransmit_merging (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_sched_class (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_entity_naming_mode (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_random_seed (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  (void) out; (void) parent; (void) cfgelem;
  // skipped on purpose: set explicitly
}
void gendef_pf_transport_selector (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_many_sockets_mode (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_standards_conformance (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}
void gendef_pf_shm_loglevel (FILE *out, void *parent, struct cfgelem const * const cfgelem) {
  gendef_pf_int (out, parent, cfgelem);
}

static void gen_defaults (FILE *out, void *parent, struct cfgelem const * const cfgelem)
{
  for (const struct cfgelem *ce = cfgelem; ce && ce->name; ce++)
  {
    if (ce->name[0] == '>' || ce->name[0] == '|') /* moved or deprecated, so don't care */
      continue;

    if (ce->meta.flag)
      fprintf(out, "#ifdef %s\n", ce->meta.flag);

    if (ce->multiplicity <= 1)
    {
      if (ce->defconfig_print)
        ce->defconfig_print (out, parent, ce);
      if (ce->children)
        gen_defaults (out, parent, ce->children);
      if (ce->attributes)
        gen_defaults (out, parent, ce->attributes);
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
        if (ce->defconfig_print)
          ce->defconfig_print (out, p, ce);
        if (ce->attributes)
          gen_defaults (out, p, ce->attributes);
        if (ce->children)
          gen_defaults (out, p, ce->children);
        p = p->next;
      }
#endif
    }
    if (ce->meta.flag)
      fprintf(out, "#endif /* %s */\n", ce->meta.flag);
  }
}

int printdefconfig (FILE *out, struct cfgelem *elem)
{
  struct ddsi_config cfg;
  struct ddsi_cfgst *cfgst;

  if ((cfgst = ddsi_config_init ("", &cfg, 0)) == NULL)
  {
    fprintf (stderr, "Failed to initialize default configuration\n");
    return -1;
  }

  fprintf (out, "\
#include <string.h>\n\
#include <stdint.h>\n\
#include <inttypes.h>\n\
#include \"dds/ddsi/ddsi_config.h\"\n\
\n\
void ddsi_config_init_default (struct ddsi_config *cfg)\n\
{\n\
  memset (cfg, 0, sizeof (*cfg));\n");
  gen_defaults (out, &cfg, elem);
  fprintf (out, "}\n");

  ddsi_config_fini (cfgst);
  return 0;
}
