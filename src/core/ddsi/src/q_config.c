/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtod.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_addrset.h"

#include "dds/ddsrt/xmlparser.h"

#include "dds/version.h"

#define MAX_PATH_DEPTH 10 /* max nesting level of configuration elements */

struct cfgelem;
struct cfgst;

enum update_result {
  URES_SUCCESS,     /* value processed successfully */
  URES_ERROR,       /* invalid value, reject configuration */
  URES_SKIP_ELEMENT /* entire subtree should be ignored */
};

typedef int (*init_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem);
typedef enum update_result (*update_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value);
typedef void (*free_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem);
typedef void (*print_fun_t) (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources);

struct unit {
  const char *name;
  int64_t multiplier;
};

struct cfgelem {
  const char *name;
  const struct cfgelem *children;
  const struct cfgelem *attributes;
  int multiplicity;
  const char *defvalue; /* NULL -> no default */
  int relative_offset;
  int elem_offset;
  init_fun_t init;
  update_fun_t update;
  free_fun_t free;
  print_fun_t print;
};

struct cfgst_nodekey {
  const struct cfgelem *e;
  void *p;
};

struct cfgst_node {
  ddsrt_avl_node_t avlnode;
  struct cfgst_nodekey key;
  int count;
  uint32_t sources;
  int failed;
};

enum implicit_toplevel {
  ITL_DISALLOWED = -1,
  ITL_ALLOWED = 0,
  ITL_INSERTED_1 = 1,
  ITL_INSERTED_2 = 2
};

struct cfgst {
  ddsrt_avl_tree_t found;
  struct config *cfg;
  const struct ddsrt_log_cfg *logcfg; /* for LOG_LC_CONFIG */
  /* error flag set so that we can continue parsing for some errors and still fail properly */
  int error;

  /* Whether this is the first element in this source: used to reject about configurations
     where the deprecated Domain/Id element is used but is set after some other things
     have been set.  The underlying reason being that the configuration handling can't
     backtrack and so needs to reject a configuration for an unrelated domain before
     anything is set.

     The new form, where the id is an attribute of the Domain element, avoids this
     by guaranteeing that the id comes first.  It seems most likely that the Domain/Id
     was either not set or at the start of the file and that this is therefore good
     enough. */
  bool first_data_in_source;

  /* Whether inserting an implicit CycloneDDS is allowed (for making it easier to hack
     a configuration through the environment variables, and if allowed, whether it has
     been inserted */
  enum implicit_toplevel implicit_toplevel;

  /* Whether unique prefix matching on a name is allowed (again for environment
     variables) */
  bool partial_match_allowed;

  /* current input, mask with 1 bit set */
  uint32_t source;

  /* ~ current input and line number */
  char *input;
  int line;

  /* path_depth, isattr and path together control the formatting of
     error messages by cfg_error() */
  int path_depth;
  int isattr[MAX_PATH_DEPTH];
  const struct cfgelem *path[MAX_PATH_DEPTH];
  void *parent[MAX_PATH_DEPTH];
};

static int cfgst_node_cmp(const void *va, const void *vb);
static const ddsrt_avl_treedef_t cfgst_found_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER(offsetof(struct cfgst_node, avlnode), offsetof(struct cfgst_node, key), cfgst_node_cmp, 0);

#define DU(fname) static enum update_result uf_##fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
#define PF(fname) static void pf_##fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
#define DUPF(fname) DU(fname) ; PF(fname)
DUPF(nop);
DUPF(networkAddress);
DUPF(networkAddresses);
DU(ipv4);
DUPF(allow_multicast);
DUPF(boolean);
DU(boolean_default);
PF(boolean_default);
DUPF(string);
DU(tracingOutputFileName);
DU(verbosity);
DUPF(tracemask);
DUPF(xcheck);
DUPF(int);
DUPF(uint);
#if 0
DUPF(int32);
DUPF(uint32);
#endif
DU(natint);
DU(natint_255);
DUPF(participantIndex);
DU(dyn_port);
DUPF(memsize);
DUPF(memsize16);
DU(duration_inf);
DU(duration_ms_1hr);
DU(duration_ms_1s);
DU(duration_us_1s);
DU(duration_100ms_1hr);
PF(duration);
DUPF(standards_conformance);
DUPF(besmode);
DUPF(retransmit_merging);
DUPF(sched_class);
DUPF(maybe_memsize);
DUPF(maybe_int32);
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
DUPF(bandwidth);
#endif
DUPF(domainId);
DUPF(transport_selector);
DUPF(many_sockets_mode);
DU(deaf_mute);
#ifdef DDSI_INCLUDE_SSL
DUPF(min_tls_version);
#endif
#undef DUPF
#undef DU
#undef PF

#define DF(fname) static void fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
DF(ff_free);
DF(ff_networkAddresses);
#undef DF

#define DI(fname) static int fname (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
DI(if_channel);
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
DI(if_network_partition);
DI(if_ignored_partition);
DI(if_partition_mapping);
#endif
DI(if_peer);
DI(if_thread_properties);
#ifdef DDSI_INCLUDE_SECURITY
DI(if_omg_security);
#endif
#undef DI

/* drop extra information, i.e. DESCRIPTION, RANGE, UNIT and VALUES */
#define ELEMENT( \
  name, elems, attrs, multip, dflt, \
  relofst, elemofst, \
  init, update, free, print, ...) \
  { \
    name, elems, attrs, multip, dflt, \
    relofst, elemofst, \
    init, update, free, print \
  }

#define DEPRECATED(name) "|" name
#define MEMBER(name) 0, ((int) offsetof (struct config, name))
#define MEMBEROF(parent, name) 1, ((int) offsetof (struct parent, name))
#define FUNCTIONS(init, update, free, print) init, update, free, print
#define DESCRIPTION(...) /* drop */
#define RANGE(...) /* drop */
#define UNIT(...) /* drop */
#define VALUES(...) /* drop */
#define MAXIMUM(...) /* drop */
#define MINIMUM(...) /* drop */

#define NOMEMBER 0, 0
#define NOFUNCTIONS 0, 0, 0, 0
#define NODATA 1, NULL, NOMEMBER, NOFUNCTIONS
#define END_MARKER { NULL, NULL, NULL, NODATA }

/* MOVED: whereto must be a path starting with CycloneDDS, may not be used in/for lists and only for elements, may not be chained */
#define MOVED(name, whereto) \
  { ">" name, NULL, NULL, 0, whereto, NOMEMBER, NOFUNCTIONS }
#define NOP(name) \
  { name, NULL, NULL, 1, NULL, NOMEMBER, 0, uf_nop, 0, pf_nop }
#define WILDCARD { "*", NULL, NULL, NODATA }

/* Visual Studio requires indirect expansion */
#define EXPAND(macro, args) macro args

#define BOOL(name, attrs, multip, dflt, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, __VA_ARGS__))
#define INT(name, attrs, multip, dflt, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, __VA_ARGS__))
#define STRING(name, attrs, multip, dflt, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, __VA_ARGS__))
#define ENUM(name, attrs, multip, dflt, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, __VA_ARGS__))
#define LIST(name, attrs, multip, dflt, ...) \
  EXPAND(ELEMENT, (name, NULL, attrs, multip, dflt, __VA_ARGS__))
#define GROUP(name, elems, attrs, multip, ...) \
  EXPAND(ELEMENT, (name, elems, attrs, multip, NULL, __VA_ARGS__))

#include "dds/ddsi/ddsi_cfgelems.h"

static const struct cfgelem root_cfgelem = {
  "/", cyclonedds_root_cfgelems, NULL, NODATA
};

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
#undef NODATA
#undef END_MARKER
#undef MOVED
#undef NOP
#undef WILDCARD
#undef EXPAND
#undef BOOL
#undef INT
#undef STRING
#undef ENUM
#undef LIST
#undef GROUP
#undef ELEMENT

static const struct unit unittab_duration[] = {
  { "ns", 1 },
  { "us", DDS_USECS (1) },
  { "ms", DDS_MSECS (1) },
  { "s", DDS_SECS (1) },
  { "min", DDS_SECS (60) },
  { "hr", DDS_SECS (3600) },
  { "day", DDS_SECS (24 * 3600) },
  { NULL, 0 }
};

/* note: order affects whether printed as KiB or kB, for consistency
   with bandwidths and pedanticness, favour KiB. */
static const struct unit unittab_memsize[] = {
  { "B", 1 },
  { "KiB", 1024 },
  { "kB", 1024 },
  { "MiB", 1048576 },
  { "MB", 1048576 },
  { "GiB", 1073741824 },
  { "GB", 1073741824 },
  { NULL, 0 }
};

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
static const struct unit unittab_bandwidth_bps[] = {
  { "b/s", 1 },{ "bps", 1 },
  { "Kib/s", 1024 },{ "Kibps", 1024 },
  { "kb/s", 1000 },{ "kbps", 1000 },
  { "Mib/s", 1048576 },{ "Mibps", 1000 },
  { "Mb/s", 1000000 },{ "Mbps", 1000000 },
  { "Gib/s", 1073741824 },{ "Gibps", 1073741824 },
  { "Gb/s", 1000000000 },{ "Gbps", 1000000000 },
  { "B/s", 8 },{ "Bps", 8 },
  { "KiB/s", 8 * 1024 },{ "KiBps", 8 * 1024 },
  { "kB/s", 8 * 1000 },{ "kBps", 8 * 1000 },
  { "MiB/s", 8 * 1048576 },{ "MiBps", 8 * 1048576 },
  { "MB/s", 8 * 1000000 },{ "MBps", 8 * 1000000 },
  { "GiB/s", 8 * (int64_t) 1073741824 },{ "GiBps", 8 * (int64_t) 1073741824 },
  { "GB/s", 8 * (int64_t) 1000000000 },{ "GBps", 8 * (int64_t) 1000000000 },
  { NULL, 0 }
};

static const struct unit unittab_bandwidth_Bps[] = {
  { "B/s", 1 },{ "Bps", 1 },
  { "KiB/s", 1024 },{ "KiBps", 1024 },
  { "kB/s", 1000 },{ "kBps", 1000 },
  { "MiB/s", 1048576 },{ "MiBps", 1048576 },
  { "MB/s", 1000000 },{ "MBps", 1000000 },
  { "GiB/s", 1073741824 },{ "GiBps", 1073741824 },
  { "GB/s", 1000000000 },{ "GBps", 1000000000 },
  { NULL, 0 }
};
#endif

static void free_configured_elements (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem);
static void free_configured_element (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem);
static const struct cfgelem *lookup_element (const char *target, bool *isattr);

static void cfgst_push (struct cfgst *cfgst, int isattr, const struct cfgelem *elem, void *parent)
{
  assert(cfgst->path_depth < MAX_PATH_DEPTH);
  assert(isattr == 0 || isattr == 1);
  cfgst->isattr[cfgst->path_depth] = isattr;
  cfgst->path[cfgst->path_depth] = elem;
  cfgst->parent[cfgst->path_depth] = parent;
  cfgst->path_depth++;
}

static void cfgst_pop (struct cfgst *cfgst)
{
  assert(cfgst->path_depth > 0);
  cfgst->path_depth--;
}

static const struct cfgelem *cfgst_tos_w_isattr (const struct cfgst *cfgst, bool *isattr)
{
  assert(cfgst->path_depth > 0);
  *isattr = cfgst->isattr[cfgst->path_depth - 1];
  return cfgst->path[cfgst->path_depth - 1];
}

static const struct cfgelem *cfgst_tos (const struct cfgst *cfgst)
{
  bool dummy_isattr;
  return cfgst_tos_w_isattr (cfgst, &dummy_isattr);
}

static void *cfgst_parent (const struct cfgst *cfgst)
{
  assert(cfgst->path_depth > 0);
  return cfgst->parent[cfgst->path_depth - 1];
}

struct cfg_note_buf {
  size_t bufpos;
  size_t bufsize;
  char *buf;
};

static size_t cfg_note_vsnprintf (struct cfg_note_buf *bb, const char *fmt, va_list ap)
{
  int x;
  x = vsnprintf(bb->buf + bb->bufpos, bb->bufsize - bb->bufpos, fmt, ap);
  if (x >= 0 && (size_t) x >= bb->bufsize - bb->bufpos)
  {
    size_t nbufsize = ((bb->bufsize + (size_t) x + 1) + 1023) & (size_t) (-1024);
    char *nbuf = ddsrt_realloc (bb->buf, nbufsize);
    bb->buf = nbuf;
    bb->bufsize = nbufsize;
    return nbufsize;
  }
  if (x < 0)
    DDS_FATAL("cfg_note_vsnprintf: vsnprintf failed\n");
  else
    bb->bufpos += (size_t) x;
  return 0;
}

static void cfg_note_snprintf (struct cfg_note_buf *bb, const char *fmt, ...) ddsrt_attribute_format ((printf, 2, 3));

static void cfg_note_snprintf (struct cfg_note_buf *bb, const char *fmt, ...)
{
  /* The reason the 2nd call to os_vsnprintf is here and not inside
     cfg_note_vsnprintf is because I somehow doubt that all platforms
     implement va_copy() */
  va_list ap;
  size_t r;
  va_start (ap, fmt);
  r = cfg_note_vsnprintf (bb, fmt, ap);
  va_end (ap);
  if (r > 0) {
    int s;
    va_start (ap, fmt);
    s = vsnprintf (bb->buf + bb->bufpos, bb->bufsize - bb->bufpos, fmt, ap);
    if (s < 0 || (size_t) s >= bb->bufsize - bb->bufpos)
      DDS_FATAL ("cfg_note_snprintf: vsnprintf failed\n");
    va_end (ap);
    bb->bufpos += (size_t) s;
  }
}

static size_t cfg_note (struct cfgst *cfgst, uint32_t cat, size_t bsz, const char *fmt, const char *suffix, va_list ap)
{
  /* Have to snprintf our way to a single string so we can OS_REPORT
     as well as nn_log.  Otherwise configuration errors will be lost
     completely on platforms where stderr doesn't actually work for
     outputting error messages (this includes Windows because of the
     way "ospl start" does its thing). */
  struct cfg_note_buf bb;
  int i, sidx;
  size_t r;

  if (cat & DDS_LC_ERROR)
    cfgst->error = 1;

  bb.bufpos = 0;
  bb.bufsize = (bsz == 0) ? 1024 : bsz;
  if ((bb.buf = ddsrt_malloc(bb.bufsize)) == NULL)
    DDS_FATAL ("cfg_note: out of memory\n");

  cfg_note_snprintf (&bb, "config: ");

  /* Path to element/attribute causing the error. Have to stop once an
     attribute is reached: a NULL marker may have been pushed onto the
     stack afterward in the default handling. */
  sidx = 0;
  while (sidx < cfgst->path_depth && cfgst->path[sidx]->name == NULL)
    sidx++;
  const struct cfgelem *prev_path = NULL;
  for (i = sidx; i < cfgst->path_depth && (i == sidx || !cfgst->isattr[i - 1]); i++)
  {
    if (cfgst->path[i] == NULL)
    {
      assert(i > sidx);
      cfg_note_snprintf (&bb, "/#text");
    }
    else if (cfgst->isattr[i])
    {
      cfg_note_snprintf (&bb, "[@%s]", cfgst->path[i]->name);
    }
    else if (cfgst->path[i] == prev_path)
    {
      /* skip printing this level: it means a group contained an element indicating that
         it was moved to the first group (i.e., stripping a level) -- this is currently
         only used for stripping out the DDSI2E level, and the sole purpose of this
         special case is making any warnings from elements contained within it look
         reasonable by always printing the new location */
    }
    else
    {
      /* first character is '>' means it was moved, so print what follows instead */
      const char *name = cfgst->path[i]->name + ((cfgst->path[i]->name[0] == '>') ? 1 : 0);
      const char *p = strchr (name, '|');
      int n = p ? (int) (p - name) : (int) strlen(name);
      cfg_note_snprintf (&bb, "%s%*.*s", (i == sidx) ? "" : "/", n, n, name);
    }
    prev_path = cfgst->path[i];
  }

  cfg_note_snprintf (&bb, ": ");
  if ((r = cfg_note_vsnprintf (&bb, fmt, ap)) > 0)
  {
    /* Can't reset ap ... and va_copy isn't widely available - so
       instead abort and hope the caller tries again with a larger
       initial buffer */
    ddsrt_free (bb.buf);
    return r;
  }

  cfg_note_snprintf (&bb, "%s", suffix);

  if (cat & (DDS_LC_WARNING | DDS_LC_ERROR))
  {
    if (cfgst->input == NULL)
      cfg_note_snprintf (&bb, " (line %d)", cfgst->line);
    else
    {
      cfg_note_snprintf (&bb, " (%s line %d)", cfgst->input, cfgst->line);
      cfgst->input = NULL;
    }
  }

  if (cfgst->logcfg)
    DDS_CLOG (cat, cfgst->logcfg, "%s\n", bb.buf);
  else
    DDS_ILOG (cat, cfgst->cfg->domainId, "%s\n", bb.buf);

  ddsrt_free (bb.buf);
  return 0;
}

static void cfg_warning (struct cfgst *cfgst, const char *fmt, ...) ddsrt_attribute_format ((printf, 2, 3));

static void cfg_warning (struct cfgst *cfgst, const char *fmt, ...)
{
  va_list ap;
  size_t bsz = 0;
  do {
    va_start (ap, fmt);
    bsz = cfg_note (cfgst, DDS_LC_WARNING, bsz, fmt, "", ap);
    va_end (ap);
  } while (bsz > 0);
}

static enum update_result cfg_error (struct cfgst *cfgst, const char *fmt, ...) ddsrt_attribute_format ((printf, 2, 3));

static enum update_result cfg_error (struct cfgst *cfgst, const char *fmt, ...)
{
  va_list ap;
  size_t bsz = 0;
  do {
    va_start (ap, fmt);
    bsz = cfg_note (cfgst, DDS_LC_ERROR, bsz, fmt, "", ap);
    va_end (ap);
  } while (bsz > 0);
  return URES_ERROR;
}

static void cfg_logelem (struct cfgst *cfgst, uint32_t sources, const char *fmt, ...) ddsrt_attribute_format ((printf, 3, 4));

static void cfg_logelem (struct cfgst *cfgst, uint32_t sources, const char *fmt, ...)
{
  /* 89 = 1 + 2 + 31 + 1 + 10 + 2*22: the number of characters in
     a string formed by concatenating all numbers from 0 .. 31 in decimal notation,
     31 separators, a opening and closing brace pair, a terminating 0, and a leading
     space */
  char srcinfo[89];
  va_list ap;
  size_t bsz = 0;
  srcinfo[0] = ' ';
  srcinfo[1] = '{';
  int pos = 2;
  for (uint32_t i = 0, m = 1; i < 32; i++, m <<= 1)
    if (sources & m)
      pos += snprintf (srcinfo + pos, sizeof (srcinfo) - (size_t) pos, "%s%"PRIu32, (pos == 2) ? "" : ",", i);
  srcinfo[pos] = '}';
  srcinfo[pos + 1] = 0;
  assert ((size_t) pos <= sizeof (srcinfo) - 2);
  do {
    va_start (ap, fmt);
    bsz = cfg_note (cfgst, DDS_LC_CONFIG, bsz, fmt, srcinfo, ap);
    va_end (ap);
  } while (bsz > 0);
}

static int list_index (const char *list[], const char *elem)
{
  for (int i = 0; list[i] != NULL; i++)
    if (ddsrt_strcasecmp (list[i], elem) == 0)
      return i;
  return -1;
}

static int64_t lookup_multiplier (struct cfgst *cfgst, const struct unit *unittab, const char *value, int unit_pos, int value_is_zero, int64_t def_mult, int err_on_unrecognised)
{
  assert (0 <= unit_pos && (size_t) unit_pos <= strlen(value));
  while (value[unit_pos] == ' ')
    unit_pos++;
  if (value[unit_pos] == 0)
  {
    if (value_is_zero) {
      /* No matter what unit, 0 remains just that.  For convenience,
         always allow 0 to be specified without a unit */
      return 1;
    } else if (def_mult == 0 && err_on_unrecognised) {
      (void) cfg_error (cfgst, "%s: unit is required", value);
      return 0;
    } else {
      cfg_warning (cfgst, "%s: use of default unit is deprecated", value);
      return def_mult;
    }
  }
  else
  {
    for (int i = 0; unittab[i].name != NULL; i++)
      if (strcmp(unittab[i].name, value + unit_pos) == 0)
        return unittab[i].multiplier;
    if (err_on_unrecognised)
      (void) cfg_error(cfgst, "%s: unrecognised unit", value + unit_pos);
    return 0;
  }
}

static void *cfg_address (UNUSED_ARG (struct cfgst *cfgst), void *parent, struct cfgelem const * const cfgelem)
{
  assert (cfgelem->multiplicity <= 1);
  return (char *) parent + cfgelem->elem_offset;
}

static void *cfg_deref_address (UNUSED_ARG (struct cfgst *cfgst), void *parent, struct cfgelem const * const cfgelem)
{
  assert (cfgelem->multiplicity > 1);
  return *((void **) ((char *) parent + cfgelem->elem_offset));
}

static void *if_common (UNUSED_ARG (struct cfgst *cfgst), void *parent, struct cfgelem const * const cfgelem, unsigned size)
{
  struct config_listelem **current = (struct config_listelem **) ((char *) parent + cfgelem->elem_offset);
  struct config_listelem *new = ddsrt_malloc (size);
  new->next = *current;
  *current = new;
  return new;
}

static int if_thread_properties (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_thread_properties_listelem *new = if_common (cfgst, parent, cfgelem, sizeof(*new));
  if (new == NULL)
    return -1;
  new->name = NULL;
  return 0;
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static int if_channel(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_channel_listelem *new = if_common (cfgst, parent, cfgelem, sizeof(*new));
  if (new == NULL)
    return -1;
  new->name = NULL;
  new->channel_reader_ts = NULL;
  new->dqueue = NULL;
  new->queueId = 0;
  new->evq = NULL;
  new->transmit_conn = NULL;
  return 0;
}
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static int if_network_partition (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_networkpartition_listelem *new = if_common (cfgst, parent, cfgelem, sizeof(*new));
  if (new == NULL)
    return -1;
  new->address_string = NULL;
  new->as = NULL;
  new->name = NULL;
  new->partitionId = 0;
  new->connected = 0;
  return 0;
}

static int if_ignored_partition (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_ignoredpartition_listelem *new = if_common (cfgst, parent, cfgelem, sizeof(*new));
  if (if_common (cfgst, parent, cfgelem, sizeof (struct config_ignoredpartition_listelem)) == NULL)
    return -1;
  new->DCPSPartitionTopic = NULL;
  return 0;
}

static int if_partition_mapping (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_partitionmapping_listelem *new = if_common (cfgst, parent, cfgelem, sizeof(*new));
  if (new == NULL)
    return -1;
  new->DCPSPartitionTopic = NULL;
  new->networkPartition = NULL;
  new->partition = NULL;
  return 0;
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

static int if_peer (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_peer_listelem *new = if_common (cfgst, parent, cfgelem, sizeof (struct config_peer_listelem));
  if (new == NULL)
    return -1;
  new->peer = NULL;
  return 0;
}

#ifdef DDSI_INCLUDE_SECURITY
static int if_omg_security (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  struct config_omg_security_listelem *new = if_common (cfgst, parent, cfgelem, sizeof (struct config_omg_security_listelem));
  if (new == NULL)
    return -1;
  memset(&new->cfg, 0, sizeof(new->cfg));
  return 0;
}
#endif

static void ff_free (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  void ** const elem = cfg_address (cfgst, parent, cfgelem);
  ddsrt_free (*elem);
}

static enum update_result uf_nop (UNUSED_ARG (struct cfgst *cfgst), UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (int first), UNUSED_ARG (const char *value))
{
  return URES_SUCCESS;
}

static void pf_nop (UNUSED_ARG (struct cfgst *cfgst), UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (uint32_t sources))
{
}

static enum update_result do_uint32_bitset (struct cfgst *cfgst, uint32_t *cats, const char **names, const uint32_t *codes, const char *value)
{
  char *copy = ddsrt_strdup (value), *cursor = copy, *tok;
  while ((tok = ddsrt_strsep (&cursor, ",")) != NULL)
  {
    const int idx = list_index (names, tok[0] == '-' ? tok+1 : tok);
    if (idx < 0)
    {
      const enum update_result ret = cfg_error (cfgst, "'%s' in '%s' undefined", tok, value);
      ddsrt_free (copy);
      return ret;
    }
    if (tok[0] == '-')
      *cats &= ~codes[idx];
    else
      *cats |= codes[idx];
  }
  ddsrt_free (copy);
  return URES_SUCCESS;
}

static unsigned uint32_popcnt (uint32_t x)
{
  unsigned n = 0;
  while (x != 0)
  {
    n += ((x & 1u) != 0);
    x >>= 1;
  }
  return n;
}

static void do_print_uint32_bitset (struct cfgst *cfgst, uint32_t mask, size_t ncodes, const char **names, const uint32_t *codes, uint32_t sources, const char *suffix)
{
  char res[256] = "", *resp = res;
  const char *prefix = "";
#ifndef NDEBUG
  {
    size_t max = 0;
    for (size_t i = 0; i < ncodes; i++)
      max += 1 + strlen (names[i]);
    max += 11; /* ,0x%x */
    max += 1; /* \0 */
    assert (max <= sizeof (res));
  }
#endif
  while (mask)
  {
    size_t i_best = 0;
    unsigned pc_best = 0;
    for (size_t i = 0; i < ncodes; i++)
    {
      uint32_t m = mask & codes[i];
      if (m == codes[i])
      {
        unsigned pc = uint32_popcnt (m);
        if (pc > pc_best)
        {
          i_best = i;
          pc_best = pc;
        }
      }
    }
    if (pc_best != 0)
    {
      resp += snprintf (resp, 256, "%s%s", prefix, names[i_best]);
      mask &= ~codes[i_best];
      prefix = ",";
    }
    else
    {
      resp += snprintf (resp, 256, "%s0x%x", prefix, (unsigned) mask);
      mask = 0;
    }
  }
  assert (resp <= res + sizeof (res));
  cfg_logelem (cfgst, sources, "%s%s", res, suffix);
}

static enum update_result uf_natint64_unit(struct cfgst *cfgst, int64_t *elem, const char *value, const struct unit *unittab, int64_t def_mult, int64_t min, int64_t max)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  int pos;
  double v_dbl;
  int64_t v_int;
  int64_t mult;
  /* try convert as integer + optional unit; if that fails, try
     floating point + optional unit (round, not truncate, to integer) */
  if (*value == 0) {
    *elem = 0; /* some static analyzers don't "get it" */
    return cfg_error(cfgst, "%s: empty string is not a valid value", value);
  } else if (sscanf (value, "%lld%n", (long long int *) &v_int, &pos) == 1 && (mult = lookup_multiplier (cfgst, unittab, value, pos, v_int == 0, def_mult, 0)) != 0) {
    assert(mult > 0);
    if (v_int < 0 || v_int > max / mult || mult * v_int < min)
      return cfg_error (cfgst, "%s: value out of range", value);
    *elem = mult * v_int;
    return URES_SUCCESS;
  } else if (sscanf(value, "%lf%n", &v_dbl, &pos) == 1 && (mult = lookup_multiplier (cfgst, unittab, value, pos, v_dbl == 0, def_mult, 1)) != 0) {
    double dmult = (double) mult;
    assert (dmult > 0);
    if ((int64_t) (v_dbl * dmult + 0.5) < min || (int64_t) (v_dbl * dmult + 0.5) > max)
      return cfg_error(cfgst, "%s: value out of range", value);
    *elem = (int64_t) (v_dbl * dmult + 0.5);
    return URES_SUCCESS;
  } else {
    *elem = 0; /* some static analyzers don't "get it" */
    return cfg_error (cfgst, "%s: invalid value", value);
  }
  DDSRT_WARNING_MSVC_ON(4996);
}

static void pf_int64_unit (struct cfgst *cfgst, int64_t value, uint32_t sources, const struct unit *unittab, const char *zero_unit)
{
  if (value == 0) {
    /* 0s is a bit of a special case: we don't want to print 0hr (or
       whatever unit will have the greatest multiplier), so hard-code
       as 0s */
    cfg_logelem (cfgst, sources, "0 %s", zero_unit);
  } else {
    int64_t m = 0;
    const char *unit = NULL;
    int i;
    for (i = 0; unittab[i].name != NULL; i++)
    {
      if (unittab[i].multiplier > m && (value % unittab[i].multiplier) == 0)
      {
        m = unittab[i].multiplier;
        unit = unittab[i].name;
      }
    }
    assert (m > 0);
    assert (unit != NULL);
    cfg_logelem (cfgst, sources, "%"PRId64" %s", value / m, unit);
  }
}

#define GENERIC_ENUM_CTYPE_UF(type_, c_type_)                           \
  DDSRT_STATIC_ASSERT (sizeof (en_##type_##_vs) / sizeof (*en_##type_##_vs) == \
                       sizeof (en_##type_##_ms) / sizeof (*en_##type_##_ms)); \
                                                                        \
  static enum update_result uf_##type_ (struct cfgst *cfgst, void *parent, UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (int first), const char *value) \
  {                                                                     \
    const int idx = list_index (en_##type_##_vs, value);                \
    c_type_ * const elem = cfg_address (cfgst, parent, cfgelem);        \
    /* idx >= length of ms check is to shut up clang's static analyzer */ \
    if (idx < 0 || idx >= (int) (sizeof (en_##type_##_ms) / sizeof (en_##type_##_ms[0]))) \
      return cfg_error (cfgst, "'%s': undefined value", value);         \
    *elem = en_##type_##_ms[idx];                                       \
    return URES_SUCCESS;                                                \
  }
#define GENERIC_ENUM_CTYPE_PF(type_, c_type_)                           \
  static void pf_##type_ (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources) \
  {                                                                     \
    c_type_ const * const p = cfg_address (cfgst, parent, cfgelem);     \
    const char *str = "INVALID";                                        \
    /* i < length of ms check is to shut up clang's static analyzer */  \
    for (int i = 0; en_##type_##_vs[i] != NULL && i < (int) (sizeof (en_##type_##_ms) / sizeof (en_##type_##_ms[0])); i++) { \
      if (en_##type_##_ms[i] == *p)                                     \
      {                                                                 \
        str = en_##type_##_vs[i];                                       \
        break;                                                          \
      }                                                                 \
    }                                                                   \
    cfg_logelem(cfgst, sources, "%s", str);                             \
  }
#define GENERIC_ENUM_CTYPE(type_, c_type_)      \
  GENERIC_ENUM_CTYPE_UF(type_, c_type_)         \
  GENERIC_ENUM_CTYPE_PF(type_, c_type_)
#define GENERIC_ENUM_UF(type_) GENERIC_ENUM_CTYPE_UF(type_, enum type_)
#define GENERIC_ENUM(type_) GENERIC_ENUM_CTYPE(type_, enum type_)

static const char *en_boolean_vs[] = { "false", "true", NULL };
static const int en_boolean_ms[] = { 0, 1, 0 };
GENERIC_ENUM_CTYPE (boolean, int)

static const char *en_boolean_default_vs[] = { "default", "false", "true", NULL };
static const enum boolean_default en_boolean_default_ms[] = { BOOLDEF_DEFAULT, BOOLDEF_FALSE, BOOLDEF_TRUE, 0 };
GENERIC_ENUM (boolean_default)

static const char *en_besmode_vs[] = { "full", "writers", "minimal", NULL };
static const enum besmode en_besmode_ms[] = { BESMODE_FULL, BESMODE_WRITERS, BESMODE_MINIMAL, 0 };
GENERIC_ENUM (besmode)

static const char *en_retransmit_merging_vs[] = { "never", "adaptive", "always", NULL };
static const enum retransmit_merging en_retransmit_merging_ms[] = { REXMIT_MERGE_NEVER, REXMIT_MERGE_ADAPTIVE, REXMIT_MERGE_ALWAYS, 0 };
GENERIC_ENUM (retransmit_merging)

static const char *en_sched_class_vs[] = { "realtime", "timeshare", "default", NULL };
static const ddsrt_sched_t en_sched_class_ms[] = { DDSRT_SCHED_REALTIME, DDSRT_SCHED_TIMESHARE, DDSRT_SCHED_DEFAULT, 0 };
GENERIC_ENUM_CTYPE (sched_class, ddsrt_sched_t)

static const char *en_transport_selector_vs[] = { "default", "udp", "udp6", "tcp", "tcp6", "raweth", NULL };
static const enum transport_selector en_transport_selector_ms[] = { TRANS_DEFAULT, TRANS_UDP, TRANS_UDP6, TRANS_TCP, TRANS_TCP6, TRANS_RAWETH, 0 };
GENERIC_ENUM (transport_selector)

/* by putting the  "true" and "false" aliases at the end, they won't come out of the
   generic printing function */
static const char *en_many_sockets_mode_vs[] = { "single", "none", "many", "false", "true", NULL };
static const enum many_sockets_mode en_many_sockets_mode_ms[] = {
  MSM_SINGLE_UNICAST, MSM_NO_UNICAST, MSM_MANY_UNICAST, MSM_SINGLE_UNICAST, MSM_MANY_UNICAST, 0 };
GENERIC_ENUM (many_sockets_mode)

static const char *en_standards_conformance_vs[] = { "pedantic", "strict", "lax", NULL };
static const enum nn_standards_conformance en_standards_conformance_ms[] = { NN_SC_PEDANTIC, NN_SC_STRICT, NN_SC_LAX, 0 };
GENERIC_ENUM_CTYPE (standards_conformance, enum nn_standards_conformance)

/* "trace" is special: it enables (nearly) everything */
static const char *tracemask_names[] = {
  "fatal", "error", "warning", "info", "config", "discovery", "data", "radmin", "timing", "traffic", "topic", "tcp", "plist", "whc", "throttle", "rhc", "content", "trace", NULL
};
static const uint32_t tracemask_codes[] = {
  DDS_LC_FATAL, DDS_LC_ERROR, DDS_LC_WARNING, DDS_LC_INFO, DDS_LC_CONFIG, DDS_LC_DISCOVERY, DDS_LC_DATA, DDS_LC_RADMIN, DDS_LC_TIMING, DDS_LC_TRAFFIC, DDS_LC_TOPIC, DDS_LC_TCP, DDS_LC_PLIST, DDS_LC_WHC, DDS_LC_THROTTLE, DDS_LC_RHC, DDS_LC_CONTENT, DDS_LC_ALL
};

static enum update_result uf_tracemask (struct cfgst *cfgst, UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (int first), const char *value)
{
  return do_uint32_bitset (cfgst, &cfgst->cfg->tracemask, tracemask_names, tracemask_codes, value);
}

static enum update_result uf_verbosity (struct cfgst *cfgst, UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (int first), const char *value)
{
  static const char *vs[] = {
    "finest", "finer", "fine", "config", "info", "warning", "severe", "none", NULL
  };
  static const uint32_t lc[] = {
    DDS_LC_ALL, DDS_LC_TRAFFIC | DDS_LC_TIMING, DDS_LC_DISCOVERY | DDS_LC_THROTTLE, DDS_LC_CONFIG, DDS_LC_INFO, DDS_LC_WARNING, DDS_LC_ERROR | DDS_LC_FATAL, 0, 0
  };
  const int idx = list_index (vs, value);
  assert (sizeof (vs) / sizeof (*vs) == sizeof (lc) / sizeof (*lc));
  if (idx < 0)
    return cfg_error (cfgst, "'%s': undefined value", value);
  for (int i = (int) (sizeof (vs) / sizeof (*vs)) - 1; i >= idx; i--)
    cfgst->cfg->tracemask |= lc[i];
  return URES_SUCCESS;
}

static void pf_tracemask (struct cfgst *cfgst, UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), uint32_t sources)
{
  /* Category is also (and often) set by Verbosity, so make an effort to locate the sources for verbosity and merge them in */
  struct cfgst_node *n;
  struct cfgst_nodekey key;
  bool isattr;
  key.e = lookup_element ("CycloneDDS/Domain/Tracing/Verbosity", &isattr);
  key.p = NULL;
  assert (key.e != NULL);
  if ((n = ddsrt_avl_lookup_succ_eq (&cfgst_found_treedef, &cfgst->found, &key)) != NULL && n->key.e == key.e)
    sources |= n->sources;
  do_print_uint32_bitset (cfgst, cfgst->cfg->tracemask, sizeof (tracemask_codes) / sizeof (*tracemask_codes), tracemask_names, tracemask_codes, sources, "");
}

static const char *xcheck_names[] = {
  "whc", "rhc", "all", NULL
};
static const uint32_t xcheck_codes[] = {
  DDS_XCHECK_WHC, DDS_XCHECK_RHC, ~(uint32_t) 0
};

static enum update_result uf_xcheck (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
  return do_uint32_bitset (cfgst, elem, xcheck_names, xcheck_codes, value);
}

static void pf_xcheck (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  const uint32_t * const p = cfg_address (cfgst, parent, cfgelem);
#ifndef NDEBUG
  const char *suffix = "";
#else
  const char *suffix = " [ignored]";
#endif
  do_print_uint32_bitset (cfgst, *p, sizeof (xcheck_codes) / sizeof (*xcheck_codes), xcheck_names, xcheck_codes, sources, suffix);
}

#ifdef DDSI_INCLUDE_SSL
static enum update_result uf_min_tls_version (struct cfgst *cfgst, UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (int first), const char *value)
{
  static const char *vs[] = {
    "1.2", "1.3", NULL
  };
  static const struct ssl_min_version ms[] = {
    {1,2}, {1,3}, {0,0}
  };
  const int idx = list_index (vs, value);
  struct ssl_min_version * const elem = cfg_address (cfgst, parent, cfgelem);
  assert (sizeof (vs) / sizeof (*vs) == sizeof (ms) / sizeof (*ms));
  if (idx < 0)
    return cfg_error(cfgst, "'%s': undefined value", value);
  *elem = ms[idx];
  return URES_SUCCESS;
}

static void pf_min_tls_version (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  struct ssl_min_version * const p = cfg_address (cfgst, parent, cfgelem);
  cfg_logelem (cfgst, sources, "%d.%d", p->major, p->minor);
}
#endif

static enum update_result uf_string (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  char ** const elem = cfg_address (cfgst, parent, cfgelem);
  *elem = ddsrt_strdup (value);
  return URES_SUCCESS;
}

static void pf_string (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  char ** const p = cfg_address (cfgst, parent, cfgelem);
  cfg_logelem (cfgst, sources, "%s", *p ? *p : "(null)");
}

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
static enum update_result uf_bandwidth (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  int64_t bandwidth_bps = 0;
  if (strncmp (value, "inf", 3) == 0) {
    /* special case: inf needs no unit */
    uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
    if (strspn (value + 3, " ") != strlen (value + 3) &&
        lo.up_multiplier (cfgst, unittab_bandwidth_bps, value, 3, 1, 8, 1) == 0)
      return URES_ERROR;
    *elem = 0;
    return URES_SUCCESS;
  } else if (uf_natint64_unit (cfgst, &bandwidth_bps, value, unittab_bandwidth_bps, 8, INT64_MAX) != URES_SUCCESS) {
    return URES_ERROR;
  } else if (bandwidth_bps / 8 > INT_MAX) {
    return cfg_error (cfgst, "%s: value out of range", value);
  } else {
    uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
    *elem = (uint32_t) (bandwidth_bps / 8);
    return URES_SUCCESS;
  }
}

static void pf_bandwidth(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  uint32_t const * const elem = cfg_address (cfgst, parent, cfgelem);
  if (*elem == 0)
    cfg_logelem (cfgst, sources, "inf");
  else
    pf_int64_unit (cfgst, *elem, sources, unittab_bandwidth_Bps, "B/s");
}
#endif

static enum update_result uf_memsize (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  int64_t size = 0;
  if (uf_natint64_unit (cfgst, &size, value, unittab_memsize, 1, 0, INT32_MAX) != URES_SUCCESS)
    return URES_ERROR;
  else {
    uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
    *elem = (uint32_t) size;
    return URES_SUCCESS;
  }
}

static void pf_memsize (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  uint32_t const * const elem = cfg_address (cfgst, parent, cfgelem);
  pf_int64_unit (cfgst, (int64_t) *elem, sources, unittab_memsize, "B");
}

static enum update_result uf_memsize16 (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  int64_t size = 0;
  if (uf_natint64_unit (cfgst, &size, value, unittab_memsize, 1, 0, UINT16_MAX) != URES_SUCCESS)
    return URES_ERROR;
  else {
    uint16_t * const elem = cfg_address (cfgst, parent, cfgelem);
    *elem = (uint16_t) size;
    return URES_SUCCESS;
  }
}

static void pf_memsize16 (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  uint16_t const * const elem = cfg_address (cfgst, parent, cfgelem);
  pf_int64_unit (cfgst, (int64_t) *elem, sources, unittab_memsize, "B");
}

static enum update_result uf_tracingOutputFileName (struct cfgst *cfgst, UNUSED_ARG (void *parent), UNUSED_ARG (struct cfgelem const * const cfgelem), UNUSED_ARG (int first), const char *value)
{
  struct config * const cfg = cfgst->cfg;
  cfg->tracefile = ddsrt_strdup (value);
  return URES_SUCCESS;
}

static enum update_result uf_ipv4 (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  /* Not actually doing any checking yet */
  return uf_string (cfgst, parent, cfgelem, first, value);
}

static enum update_result uf_networkAddress (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  if (ddsrt_strcasecmp (value, "auto") != 0)
    return uf_ipv4 (cfgst, parent, cfgelem, first, value);
  else
  {
    char ** const elem = cfg_address (cfgst, parent, cfgelem);
    *elem = NULL;
    return URES_SUCCESS;
  }
}

static void pf_networkAddress (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  char ** const p = cfg_address (cfgst, parent, cfgelem);
  cfg_logelem (cfgst, sources, "%s", *p ? *p : "auto");
}

static enum update_result uf_networkAddresses_simple (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  char *** const elem = cfg_address (cfgst, parent, cfgelem);
  if ((*elem = ddsrt_malloc (2 * sizeof(char *))) == NULL)
    return cfg_error (cfgst, "out of memory");
  if (((*elem)[0] = ddsrt_strdup (value)) == NULL) {
    ddsrt_free (*elem);
    *elem = NULL;
    return cfg_error (cfgst, "out of memory");
  }
  (*elem)[1] = NULL;
  return URES_SUCCESS;
}

static enum update_result uf_networkAddresses (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  /* Check for keywords first */
  {
    static const char *keywords[] = { "all", "any", "none" };
    for (int i = 0; i < (int) (sizeof (keywords) / sizeof (*keywords)); i++) {
      if (ddsrt_strcasecmp (value, keywords[i]) == 0)
        return uf_networkAddresses_simple (cfgst, parent, cfgelem, first, keywords[i]);
    }
  }

  /* If not keyword, then comma-separated list of addresses */
  {
    char *** const elem = cfg_address (cfgst, parent, cfgelem);
    char *copy;
    uint32_t count;

    /* First count how many addresses we have - but do it stupidly by
       counting commas and adding one (so two commas in a row are both
       counted) */
    {
      const char *scan = value;
      count = 1;
      while (*scan)
        count += (*scan++ == ',');
    }

    copy = ddsrt_strdup (value);

    /* Allocate an array of address strings (which may be oversized a
       bit because of the counting of the commas) */
    *elem = ddsrt_malloc ((count + 1) * sizeof(char *));

    {
      char *cursor = copy, *tok;
      uint32_t idx = 0;
      while ((tok = ddsrt_strsep (&cursor, ",")) != NULL) {
        assert (idx < count);
        (*elem)[idx] = ddsrt_strdup (tok);
        idx++;
      }
      (*elem)[idx] = NULL;
    }
    ddsrt_free (copy);
  }
  return URES_SUCCESS;
}

static void pf_networkAddresses (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  char *** const p = cfg_address (cfgst, parent, cfgelem);
  for (int i = 0; (*p)[i] != NULL; i++)
    cfg_logelem (cfgst, sources, "%s", (*p)[i]);
}

static void ff_networkAddresses (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  char *** const elem = cfg_address (cfgst, parent, cfgelem);
  for (int i = 0; (*elem)[i]; i++)
    ddsrt_free ((*elem)[i]);
  ddsrt_free (*elem);
}

#ifdef DDSI_INCLUDE_SSM
static const char *allow_multicast_names[] = { "false", "spdp", "asm", "ssm", "true", NULL };
static const uint32_t allow_multicast_codes[] = { AMC_FALSE, AMC_SPDP, AMC_ASM, AMC_SSM, AMC_TRUE };
#else
static const char *allow_multicast_names[] = { "false", "spdp", "asm", "true", NULL };
static const uint32_t allow_multicast_codes[] = { AMC_FALSE, AMC_SPDP, AMC_ASM, AMC_TRUE };
#endif

static enum update_result uf_allow_multicast (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG(int first), const char *value)
{
  uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
  if (ddsrt_strcasecmp (value, "default") == 0)
  {
    *elem = AMC_DEFAULT;
    return URES_SUCCESS;
  }
  else
  {
    *elem = 0;
    return do_uint32_bitset (cfgst, elem, allow_multicast_names, allow_multicast_codes, value);
  }
}

static void pf_allow_multicast(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  uint32_t *p = cfg_address (cfgst, parent, cfgelem);
  if (*p == AMC_DEFAULT)
    cfg_logelem (cfgst, sources, "default");
  else if (*p == 0)
    cfg_logelem (cfgst, sources, "false");
  else
  {
    do_print_uint32_bitset (cfgst, *p, sizeof (allow_multicast_codes) / sizeof (*allow_multicast_codes), allow_multicast_names, allow_multicast_codes, sources, "");
  }
}

static enum update_result uf_maybe_int32 (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  struct config_maybe_int32 * const elem = cfg_address (cfgst, parent, cfgelem);
  int pos;
  if (ddsrt_strcasecmp (value, "default") == 0) {
    elem->isdefault = 1;
    elem->value = 0;
    return URES_SUCCESS;
  } else if (sscanf (value, "%"SCNd32"%n", &elem->value, &pos) == 1 && value[pos] == 0) {
    elem->isdefault = 0;
    return URES_SUCCESS;
  } else {
    return cfg_error (cfgst, "'%s': neither 'default' nor a decimal integer\n", value);
  }
  DDSRT_WARNING_MSVC_ON(4996);
}

static void pf_maybe_int32 (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  struct config_maybe_int32 const * const p = cfg_address (cfgst, parent, cfgelem);
  if (p->isdefault)
    cfg_logelem (cfgst, sources, "default");
  else
    cfg_logelem (cfgst, sources, "%d", p->value);
}

static enum update_result uf_maybe_memsize (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  struct config_maybe_uint32 * const elem = cfg_address (cfgst, parent, cfgelem);
  int64_t size = 0;
  if (ddsrt_strcasecmp (value, "default") == 0) {
    elem->isdefault = 1;
    elem->value = 0;
    return URES_SUCCESS;
  } else if (uf_natint64_unit (cfgst, &size, value, unittab_memsize, 1, 0, INT32_MAX) != URES_SUCCESS) {
    return URES_ERROR;
  } else {
    elem->isdefault = 0;
    elem->value = (uint32_t) size;
    return URES_SUCCESS;
  }
}

static void pf_maybe_memsize (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  struct config_maybe_uint32 const * const p = cfg_address (cfgst, parent, cfgelem);
  if (p->isdefault)
    cfg_logelem (cfgst, sources, "default");
  else
    pf_int64_unit (cfgst, p->value, sources, unittab_memsize, "B");
}

static enum update_result uf_int (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  int * const elem = cfg_address (cfgst, parent, cfgelem);
  char *endptr;
  long v = strtol (value, &endptr, 10);
  if (*value == 0 || *endptr != 0)
    return cfg_error (cfgst, "%s: not a decimal integer", value);
  if (v != (int) v)
    return cfg_error (cfgst, "%s: value out of range", value);
  *elem = (int) v;
  return URES_SUCCESS;
}

static enum update_result uf_int_min_max (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value, int min, int max)
{
  int *elem = cfg_address (cfgst, parent, cfgelem);
  if (uf_int (cfgst, parent, cfgelem, first, value) != URES_SUCCESS)
    return URES_ERROR;
  else if (*elem < min || *elem > max)
    return cfg_error (cfgst, "%s: out of range", value);
  else
    return URES_SUCCESS;
}

static void pf_int (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  int const * const p = cfg_address (cfgst, parent, cfgelem);
  cfg_logelem (cfgst, sources, "%d", *p);
}

static enum update_result uf_dyn_port(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  return uf_int_min_max(cfgst, parent, cfgelem, first, value, -1, 65535);
}

static enum update_result uf_natint(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  return uf_int_min_max(cfgst, parent, cfgelem, first, value, 0, INT32_MAX);
}

static enum update_result uf_natint_255(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  return uf_int_min_max(cfgst, parent, cfgelem, first, value, 0, 255);
}

static enum update_result uf_uint (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
  char *endptr;
  unsigned long v = strtoul (value, &endptr, 10);
  if (*value == 0 || *endptr != 0)
    return cfg_error (cfgst, "%s: not a decimal integer", value);
  if (v != (uint32_t) v)
    return cfg_error (cfgst, "%s: value out of range", value);
  *elem = (uint32_t) v;
  return URES_SUCCESS;
}

static void pf_uint (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  uint32_t const * const p = cfg_address (cfgst, parent, cfgelem);
  cfg_logelem (cfgst, sources, "%u", *p);
}

static enum update_result uf_duration_gen (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, const char *value, int64_t def_mult, int64_t min_ns, int64_t max_ns)
{
  return uf_natint64_unit (cfgst, cfg_address (cfgst, parent, cfgelem), value, unittab_duration, def_mult, min_ns, max_ns);
}

static enum update_result uf_duration_inf (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  if (ddsrt_strcasecmp (value, "inf") == 0) {
    int64_t * const elem = cfg_address (cfgst, parent, cfgelem);
    *elem = DDS_INFINITY;
    return URES_SUCCESS;
  } else {
    return uf_duration_gen (cfgst, parent, cfgelem, value, 0, 0, DDS_INFINITY - 1);
  }
}

static enum update_result uf_duration_ms_1hr (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  return uf_duration_gen (cfgst, parent, cfgelem, value, DDS_MSECS (1), 0, DDS_SECS (3600));
}

static enum update_result uf_duration_ms_1s (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  return uf_duration_gen (cfgst, parent, cfgelem, value, DDS_MSECS (1), 0, DDS_SECS (1));
}

static enum update_result uf_duration_us_1s (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  return uf_duration_gen (cfgst, parent, cfgelem, value, DDS_USECS (1), 0, DDS_SECS (1));
}

static enum update_result uf_duration_100ms_1hr (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  return uf_duration_gen (cfgst, parent, cfgelem, value, 0, DDS_MSECS (100), DDS_SECS (3600));
}

static void pf_duration (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  int64_t const * const elem = cfg_address (cfgst, parent, cfgelem);
  if (*elem == DDS_INFINITY)
    cfg_logelem (cfgst, sources, "inf");
  else
    pf_int64_unit (cfgst, *elem, sources, unittab_duration, "s");
}

static enum update_result uf_domainId (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, UNUSED_ARG (int first), const char *value)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  uint32_t * const elem = cfg_address (cfgst, parent, cfgelem);
  uint32_t tmpval;
  int pos;
  if (ddsrt_strcasecmp (value, "any") == 0) {
    return URES_SUCCESS;
  } else if (sscanf (value, "%"SCNu32"%n", &tmpval, &pos) == 1 && value[pos] == 0 && tmpval != UINT32_MAX) {
    if (*elem == UINT32_MAX || *elem == tmpval)
    {
      if (!cfgst->first_data_in_source)
        cfg_warning (cfgst, "not the first data in this source for compatible domain id");
      *elem = tmpval;
      return URES_SUCCESS;
    }
    else if (!cfgst->first_data_in_source)
    {
      /* something has been set and we can't undo any earlier assignments, so this is an error */
      return cfg_error (cfgst, "not the first data in this source for incompatible domain id");
    }
    else
    {
      //cfg_warning (cfgst, "%"PRIu32" is incompatible with domain id being configured (%"PRIu32"), skipping", tmpval, elem->value);
      return URES_SKIP_ELEMENT;
    }
  } else {
    return cfg_error (cfgst, "'%s': neither 'any' nor a less than 2**32-1", value);
  }
  DDSRT_WARNING_MSVC_ON(4996);
}

static void pf_domainId(struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  uint32_t const * const p = cfg_address (cfgst, parent, cfgelem);
  if (*p == UINT32_MAX)
    cfg_logelem (cfgst, sources, "any");
  else
    cfg_logelem (cfgst, sources, "%"PRIu32, *p);
}

static enum update_result uf_participantIndex (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  int * const elem = cfg_address (cfgst, parent, cfgelem);
  if (ddsrt_strcasecmp (value, "auto") == 0) {
    *elem = PARTICIPANT_INDEX_AUTO;
    return URES_SUCCESS;
  } else if (ddsrt_strcasecmp (value, "none") == 0) {
    *elem = PARTICIPANT_INDEX_NONE;
    return URES_SUCCESS;
  } else {
    return uf_int_min_max (cfgst, parent, cfgelem, first, value, 0, 120);
  }
}

static void pf_participantIndex (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, uint32_t sources)
{
  int const * const p = cfg_address (cfgst, parent, cfgelem);
  switch (*p)
  {
    case PARTICIPANT_INDEX_NONE:
      cfg_logelem (cfgst, sources, "none");
      break;
    case PARTICIPANT_INDEX_AUTO:
      cfg_logelem (cfgst, sources, "auto");
      break;
    default:
      cfg_logelem (cfgst, sources, "%d", *p);
      break;
  }
}

static enum update_result uf_deaf_mute (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem, int first, const char *value)
{
  return uf_boolean (cfgst, parent, cfgelem, first, value);
}

static struct cfgst_node *lookup_or_create_elem_record (struct cfgst *cfgst, struct cfgelem const * const cfgelem, void *parent, uint32_t source)
{
  struct cfgst_node *n;
  struct cfgst_nodekey key;
  ddsrt_avl_ipath_t np;
  key.e = cfgelem;
  key.p = parent;
  if ((n = ddsrt_avl_lookup_ipath (&cfgst_found_treedef, &cfgst->found, &key, &np)) == NULL)
  {
    if ((n = ddsrt_malloc (sizeof (*n))) == NULL)
    {
      cfg_error (cfgst, "out of memory");
      return NULL;
    }
    n->key = key;
    n->count = 0;
    n->failed = 0;
    n->sources = source;
    ddsrt_avl_insert_ipath (&cfgst_found_treedef, &cfgst->found, n, &np);
  }
  return n;
}

static enum update_result do_update (struct cfgst *cfgst, update_fun_t upd, void *parent, struct cfgelem const * const cfgelem, const char *value, uint32_t source)
{
  struct cfgst_node *n;
  enum update_result res;
  n = lookup_or_create_elem_record (cfgst, cfgelem, parent, source);
  if (cfgelem->multiplicity == 1 && n->count == 1 && source > n->sources)
    free_configured_element (cfgst, parent, cfgelem);
  if (cfgelem->multiplicity == 0 || n->count < cfgelem->multiplicity)
    res = upd (cfgst, parent, cfgelem, (n->count == n->failed), value);
  else
    res = cfg_error (cfgst, "only %d instance%s allowed", cfgelem->multiplicity, (cfgelem->multiplicity == 1) ? "" : "s");
  n->count++;
  n->sources |= source;
  /* deciding to skip an entire subtree in the config is not an error */
  if (res == URES_ERROR)
    n->failed++;
  return res;
}

static int set_default (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  enum update_result res;
  if (cfgelem->defvalue == NULL)
  {
    (void) cfg_error (cfgst, "element missing in configuration");
    return 0;
  }
  res = do_update (cfgst, cfgelem->update, parent, cfgelem, cfgelem->defvalue, 0);
  return (res != URES_ERROR);
}

static int set_defaults (struct cfgst *cfgst, void *parent, int isattr, struct cfgelem const * const cfgelem)
{
  int ok = 1;
  for (const struct cfgelem *ce = cfgelem; ce && ce->name; ce++)
  {
    struct cfgst_nodekey key;
    key.e = ce;
    key.p = parent;
    cfgst_push (cfgst, isattr, ce, parent);
    if (ce->multiplicity <= 1)
    {
      if (ddsrt_avl_lookup (&cfgst_found_treedef, &cfgst->found, &key) == NULL)
      {
        if (ce->update)
        {
          int ok1;
          cfgst_push (cfgst, 0, NULL, NULL);
          ok1 = set_default (cfgst, parent, ce);
          cfgst_pop (cfgst);
          ok = ok && ok1;
        }
      }
      if (ce->children)
        ok = ok && set_defaults (cfgst, parent, 0, ce->children);
      if (ce->attributes)
        ok = ok && set_defaults (cfgst, parent, 1, ce->attributes);
    }
    cfgst_pop (cfgst);
  }
  return ok;
}


static void print_configitems (struct cfgst *cfgst, void *parent, int isattr, struct cfgelem const * const cfgelem, uint32_t sources)
{
  for (const struct cfgelem *ce = cfgelem; ce && ce->name; ce++)
  {
    struct cfgst_nodekey key;
    struct cfgst_node *n;
    if (ce->name[0] == '>' || ce->name[0] == '|') /* moved or deprecated, so don't care */
      continue;
    key.e = ce;
    key.p = parent;
    cfgst_push (cfgst, isattr, ce, parent);
    if ((n = ddsrt_avl_lookup(&cfgst_found_treedef, &cfgst->found, &key)) != NULL)
      sources = n->sources;

    if (ce->multiplicity <= 1)
    {
      cfgst_push (cfgst, 0, NULL, NULL);
      if (ce->print)
        ce->print (cfgst, parent, ce, sources);
      cfgst_pop (cfgst);
      if (ce->children)
        print_configitems (cfgst, parent, 0, ce->children, sources);
      if (ce->attributes)
        print_configitems (cfgst, parent, 1, ce->attributes, sources);
    }
    else
    {
      struct config_listelem *p = cfg_deref_address (cfgst, parent, ce);
      while (p)
      {
        cfgst_push (cfgst, 0, NULL, NULL);
        if (ce->print)
          ce->print (cfgst, p, ce, sources);
        cfgst_pop(cfgst);
        if (ce->attributes)
          print_configitems (cfgst, p, 1, ce->attributes, sources);
        if (ce->children)
          print_configitems (cfgst, p, 0, ce->children, sources);
        p = p->next;
      }
    }
    cfgst_pop (cfgst);
  }
}


static void free_all_elements (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  for (const struct cfgelem *ce = cfgelem; ce && ce->name; ce++)
  {
    if (ce->name[0] == '>') /* moved, so don't care */
      continue;

    if (ce->free)
      ce->free (cfgst, parent, ce);

    if (ce->multiplicity <= 1) {
      if (ce->children)
        free_all_elements (cfgst, parent, ce->children);
      if (ce->attributes)
        free_all_elements (cfgst, parent, ce->attributes);
    } else {
      struct config_listelem *p = cfg_deref_address (cfgst, parent, ce);
      while (p) {
        struct config_listelem *p1 = p->next;
        if (ce->attributes)
          free_all_elements (cfgst, p, ce->attributes);
        if (ce->children)
          free_all_elements (cfgst, p, ce->children);
        ddsrt_free (p);
        p = p1;
      }
    }
  }
}

static void free_configured_element (struct cfgst *cfgst, void *parent, struct cfgelem const * const ce)
{
  struct cfgst_nodekey key;
  struct cfgst_node *n;
  if (ce->name[0] == '>') /* moved, so don't care */
    return;
  key.e = ce;
  key.p = parent;
  if ((n = ddsrt_avl_lookup (&cfgst_found_treedef, &cfgst->found, &key)) != NULL)
  {
    if (ce->free && n->count > n->failed)
      ce->free (cfgst, parent, ce);
    n->count = n->failed = 0;
  }

  if (ce->multiplicity <= 1)
  {
    if (ce->children)
      free_configured_elements (cfgst, parent, ce->children);
    if (ce->attributes)
      free_configured_elements (cfgst, parent, ce->attributes);
  }
  else
  {
    /* FIXME: this used to require free_all_elements because there would be no record stored for
       configuration elements within lists, but with that changed, I think this can now just use
       free_configured_elements */
    struct config_listelem *p = cfg_deref_address (cfgst, parent, ce);
    while (p) {
      struct config_listelem * const p1 = p->next;
      if (ce->attributes)
        free_all_elements (cfgst, p, ce->attributes);
      if (ce->children)
        free_all_elements (cfgst, p, ce->children);
      ddsrt_free (p);
      p = p1;
    }
  }
}

static void free_configured_elements (struct cfgst *cfgst, void *parent, struct cfgelem const * const cfgelem)
{
  for (const struct cfgelem *ce = cfgelem; ce && ce->name; ce++)
    free_configured_element (cfgst, parent, ce);
}

static int matching_name_index (const char *name_w_aliases, const char *name, size_t *partial)
{
  const char *ns = name_w_aliases;
  const char *aliases = strchr (ns, '|');
  const char *p = aliases;
  int idx = 0;
  if (partial)
  {
    /* may be set later on */
    *partial = 0;
  }
  while (p)
  {
    if (ddsrt_strncasecmp (ns, name, (size_t) (p - ns)) == 0 && name[p - ns] == 0)
    {
      /* ns upto the pipe symbol is a prefix of name, and name is terminated at that point */
      return idx;
    }
    /* If primary name followed by '||' instead of '|', aliases are non-warning */
    ns = p + 1 + (idx == 0 && p[1] == '|');
    p = strchr (ns, '|');
    idx++;
  }
  if (ddsrt_strcasecmp (ns, name) == 0)
    return idx;
  else
  {
    if (partial)
    {
      /* try a partial match on the primary name (the aliases are for backwards compatibility,
       and as partial matches are for hackability, I am of the opinion that backwards
       compatibility on those is a bit over the top) */
      size_t max_len = strlen (name);
      if (aliases && (size_t) (aliases - name_w_aliases) < max_len)
        max_len = (size_t) (aliases - name_w_aliases);
      if (ddsrt_strncasecmp (name_w_aliases, name, max_len) == 0)
        *partial = max_len;
      /* it may be a partial match, but it is still not a match */
    }
    return -1;
  }
}

static const struct cfgelem *lookup_element (const char *target, bool *isattr)
{
  const struct cfgelem *cfgelem = cyclonedds_root_cfgelems;
  char *target_copy = ddsrt_strdup (target);
  char *p = target_copy;
  *isattr = false;
  while (p)
  {
    char *p1 = p + strcspn (p, "/[");
    switch (*p1)
    {
      case '\0':
        p1 = NULL;
        break;
      case '/':
        *p1++ = 0;
        break;
      case '[':
        assert (p1[1] == '@' && p1[strlen (p1) - 1] == ']');
        p1[strlen (p1) - 1] = 0;
        *p1 = 0; p1 += 2;
        *isattr = true;
        break;
      default:
        assert (0);
    }
    for (; cfgelem->name; cfgelem++)
    {
      if (matching_name_index (cfgelem->name, p, NULL) >= 0)
      {
        /* not supporting chained redirects */
        assert (cfgelem->name[0] != '>');
        break;
      }
    }
    if (p1)
      cfgelem = *isattr ? cfgelem->attributes : cfgelem->children;
    p = p1;
  }
  ddsrt_free (target_copy);
  return cfgelem;
}

static int proc_elem_open (void *varg, UNUSED_ARG (uintptr_t parentinfo), UNUSED_ARG (uintptr_t *eleminfo), const char *name, int line)
{
  struct cfgst * const cfgst = varg;

  cfgst->line = line;
  if (cfgst->implicit_toplevel == ITL_ALLOWED)
  {
    if (ddsrt_strcasecmp (name, "CycloneDDS") == 0)
      cfgst->implicit_toplevel = ITL_DISALLOWED;
    else
    {
      cfgst_push (cfgst, 0, &cyclonedds_root_cfgelems[0], cfgst_parent (cfgst));
      /* Most likely one would want to override some domain settings without bothering,
         so also allow an implicit "Domain" */
      cfgst->implicit_toplevel = ITL_INSERTED_1;
      if (ddsrt_strcasecmp (name, "Domain") != 0)
      {
        cfgst_push (cfgst, 0, &root_cfgelems[0], cfgst_parent (cfgst));
        cfgst->implicit_toplevel = ITL_INSERTED_2;
      }
      cfgst->source = (cfgst->source == 0) ? 1 : cfgst->source << 1;
      cfgst->first_data_in_source = true;
    }
  }

  const struct cfgelem *cfgelem = cfgst_tos (cfgst);
  const struct cfgelem *cfg_subelem;
  int moved = 0;
  size_t partial = 0;
  const struct cfgelem *partial_match = NULL;

  if (cfgelem == NULL)
  {
    /* Ignoring, but do track the structure so we can know when to stop ignoring */
    cfgst_push (cfgst, 0, NULL, NULL);
    return 1;
  }
  for (cfg_subelem = cfgelem->children; cfg_subelem && cfg_subelem->name && strcmp (cfg_subelem->name, "*") != 0; cfg_subelem++)
  {
    const char *csename = cfg_subelem->name;
    size_t partial1;
    int idx;
    moved = (csename[0] == '>');
    if (moved)
      csename++;
    idx = matching_name_index (csename, name, &partial1);
    if (idx > 0)
    {
      if (csename[0] == '|')
        cfg_warning (cfgst, "'%s': deprecated setting", name);
      else
      {
        int n = (int) (strchr (csename, '|') - csename);
        if (csename[n + 1] != '|') {
          cfg_warning (cfgst, "'%s': deprecated alias for '%*.*s'", name, n, n, csename);
        }
      }
    }
    if (idx >= 0)
    {
      /* an exact match is always good */
      break;
    }
    if (partial1 > partial)
    {
      /* a longer prefix match is a candidate ... */
      partial = partial1;
      partial_match = cfg_subelem;
    }
    else if (partial1 > 0 && partial1 == partial)
    {
      /* ... but an ambiguous prefix match won't do */
      partial_match = NULL;
    }
  }
  if (cfg_subelem == NULL || cfg_subelem->name == NULL)
  {
    if (partial_match != NULL && cfgst->partial_match_allowed)
      cfg_subelem = partial_match;
    else
    {
      (void) cfg_error (cfgst, "%s: unknown element", name);
      cfgst_push (cfgst, 0, NULL, NULL);
      return 0;
    }
  }
  if (strcmp (cfg_subelem->name, "*") == 0)
  {
    /* Push a marker that we are to ignore this part of the DOM tree */
    cfgst_push (cfgst, 0, NULL, NULL);
    return 1;
  }
  else
  {
    void *parent, *dynparent;

    if (moved)
    {
      struct cfgelem const * const cfg_subelem_orig = cfg_subelem;
      bool isattr;
      cfg_subelem = lookup_element (cfg_subelem->defvalue, &isattr);
      cfgst_push (cfgst, 0, cfg_subelem_orig, NULL);
      cfg_warning (cfgst, "setting%s moved to //%s", cfg_subelem->children ? "s" : "", cfg_subelem_orig->defvalue);
      cfgst_pop (cfgst);
    }

    parent = cfgst_parent (cfgst);
    assert (cfgelem->init || cfgelem->multiplicity == 1); /* multi-items must have an init-func */
    if (cfg_subelem->init)
    {
      if (cfg_subelem->init (cfgst, parent, cfg_subelem) < 0)
        return 0;
    }

    if (cfg_subelem->multiplicity <= 1)
      dynparent = parent;
    else
      dynparent = cfg_deref_address (cfgst, parent, cfg_subelem);

    cfgst_push (cfgst, 0, cfg_subelem, dynparent);

    if (cfg_subelem == &cyclonedds_root_cfgelems[0])
    {
      cfgst->source = (cfgst->source == 0) ? 1 : cfgst->source << 1;
      cfgst->first_data_in_source = true;
    }
    else if (cfg_subelem >= &root_cfgelems[0] && cfg_subelem < &root_cfgelems[0] + sizeof (root_cfgelems) / sizeof (root_cfgelems[0]))
    {
      if (!cfgst->first_data_in_source)
        cfgst->source = (cfgst->source == 0) ? 1 : cfgst->source << 1;
      cfgst->first_data_in_source = true;
    }
    return 1;
  }
}

static int proc_update_cfgelem (struct cfgst *cfgst, const struct cfgelem *ce, const char *value, bool isattr)
{
  void *parent = cfgst_parent (cfgst);
  char *xvalue = ddsrt_expand_envvars (value, cfgst->cfg->domainId);
  enum update_result res;
  cfgst_push (cfgst, isattr, isattr ? ce : NULL, parent);
  res = do_update (cfgst, ce->update, parent, ce, xvalue, cfgst->source);
  cfgst_pop (cfgst);
  ddsrt_free (xvalue);

  /* Push a marker that we are to ignore this part of the DOM tree -- see the
     handling of WILDCARD ("*").  This is currently only used for domain ids,
     and there it is either:
     - <Domain id="X">, in which case the element at the top of the stack is
       Domain, which is the element to ignore, or:
     - <Domain><Id>X, in which case the element at the top of the stack Id,
       and we need to strip ignore the one that is one down.

     The configuration processing doesn't allow an element to contain text and
     have children at the same time, and with that restriction it never makes
     sense to ignore just the TOS element if it is text: that would be better
     done in the update function itself.

     So replacing the top stack entry for an attribute and the top two entries
     if it's text is a reasonable interpretation of SKIP.  And it seems quite
     likely that it won't be used for anything else ... */
  if (res == URES_SKIP_ELEMENT)
  {
    cfgst_pop (cfgst);
    if (!isattr)
    {
      cfgst_pop (cfgst);
      cfgst_push (cfgst, 0, NULL, NULL);
    }
    cfgst_push (cfgst, 0, NULL, NULL);
  }
  else
  {
    cfgst->first_data_in_source = false;
  }
  return res != URES_ERROR;
}

static int proc_attr (void *varg, UNUSED_ARG (uintptr_t eleminfo), const char *name, const char *value, int line)
{
  /* All attributes are processed immediately after opening the element */
  struct cfgst * const cfgst = varg;
  const struct cfgelem *cfgelem = cfgst_tos (cfgst);
  const struct cfgelem *cfg_attr;
  cfgst->line = line;
  if (cfgelem == NULL)
    return 1;
  for (cfg_attr = cfgelem->attributes; cfg_attr && cfg_attr->name; cfg_attr++)
    if (ddsrt_strcasecmp(cfg_attr->name, name) == 0)
      break;
  if (cfg_attr != NULL && cfg_attr->name != NULL)
    return proc_update_cfgelem (cfgst, cfg_attr, value, true);
  else
  {
    (void) cfg_error (cfgst, "%s: unknown attribute", name);
    return 0;
  }
}

static int proc_elem_data (void *varg, UNUSED_ARG (uintptr_t eleminfo), const char *value, int line)
{
  struct cfgst * const cfgst = varg;
  bool isattr; /* elem may have been moved to an attr */
  const struct cfgelem *cfgelem = cfgst_tos_w_isattr (cfgst, &isattr);
  cfgst->line = line;
  if (cfgelem == NULL)
    return 1;
  if (cfgelem->update != 0)
    return proc_update_cfgelem (cfgst, cfgelem, value, isattr);
  else
  {
    (void) cfg_error (cfgst, "%s: no data expected", value);
    return 0;
  }
}

static int proc_elem_close (void *varg, UNUSED_ARG (uintptr_t eleminfo), int line)
{
  struct cfgst * const cfgst = varg;
  const struct cfgelem * cfgelem = cfgst_tos (cfgst);
  int ok = 1;
  cfgst->line = line;
  if (cfgelem && cfgelem->multiplicity > 1)
  {
    void *parent = cfgst_parent (cfgst);
    int ok1;
    ok1 = set_defaults (cfgst, parent, 1, cfgelem->attributes);
    ok = ok && ok1;
    ok1 = set_defaults (cfgst, parent, 0, cfgelem->children);
    ok = ok && ok1;
  }
  cfgst_pop (cfgst);
  return ok;
}

static void proc_error (void *varg, const char *msg, int line)
{
  struct cfgst * const cfgst = varg;
  (void) cfg_error (cfgst, "parser error %s at line %d", msg, line);
}

static int cfgst_node_cmp (const void *va, const void *vb)
{
  return memcmp (va, vb, sizeof (struct cfgst_nodekey));
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static int set_default_channel (struct config *cfg)
{
  if (cfg->channels == NULL)
  {
    /* create one default channel if none configured */
    struct config_channel_listelem *c;
    if ((c = ddsrt_malloc (sizeof (*c))) == NULL)
      return ERR_OUT_OF_MEMORY;
    c->next = NULL;
    c->name = ddsrt_strdup ("user");
    c->priority = 0;
    c->resolution = DDS_MSECS (1);
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
    c->data_bandwidth_limit = 0;
    c->auxiliary_bandwidth_limit = 0;
#endif
    c->diffserv_field = 0;
    c->channel_reader_ts = NULL;
    c->queueId = 0;
    c->dqueue = NULL;
    c->evq = NULL;
    c->transmit_conn = NULL;
    cfg->channels = c;
  }
  return 0;
}

static int sort_channels_cmp (const void *va, const void *vb)
{
  const struct config_channel_listelem * const *a = va;
  const struct config_channel_listelem * const *b = vb;
  return ((*a)->priority == (*b)->priority) ? 0 : ((*a)->priority < (*b)->priority) ? -1 : 1;
}

static int sort_channels_check_nodups (struct config *cfg, uint32_t domid)
{
  /* Selecting a channel is much easier & more elegant if the channels
     are sorted on descending priority.  While we do retain the list
     structure, sorting is much easier in an array, and hence we
     convert back and forth. */
  struct config_channel_listelem **ary, *c;
  uint32_t i, n;
  int result;

  n = 0;
  for (c = cfg->channels; c; c = c->next)
    n++;
  assert(n > 0);

  ary = ddsrt_malloc (n * sizeof (*ary));

  i = 0;
  for (c = cfg->channels; c; c = c->next)
    ary[i++] = c;
  qsort (ary, n, sizeof (*ary), sort_channels_cmp);

  result = 0;
  for (i = 0; i < n - 1; i++) {
    if (ary[i]->priority == ary[i + 1]->priority) {
      DDS_ILOG (DDS_LC_ERROR, domid, "config: duplicate channel definition for priority %u: channels %s and %s\n",
                ary[i]->priority, ary[i]->name, ary[i + 1]->name);
      result = ERR_ENTITY_EXISTS;
    }
  }

  if (result == 0)
  {
    cfg->channels = ary[0];
    for (i = 0; i < n - 1; i++)
      ary[i]->next = ary[i + 1];
    ary[i]->next = NULL;
    cfg->max_channel = ary[i];
  }

  ddsrt_free (ary);
  return result;
}
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

static FILE *config_open_file (char *tok, char **cursor, uint32_t domid)
{
  assert (*tok && !(isspace ((unsigned char) *tok) || *tok == ','));
  FILE *fp;
  char *comma;
  if ((comma = strchr (tok, ',')) == NULL)
    *cursor = NULL;
  else
  {
    *comma = 0;
    *cursor = comma + 1;
  }
  DDSRT_WARNING_MSVC_OFF(4996);
  if ((fp = fopen (tok, "r")) == NULL)
  {
    if (strncmp (tok, "file://", 7) != 0 || (fp = fopen (tok + 7, "r")) == NULL)
    {
      DDS_ILOG (DDS_LC_ERROR, domid, "can't open configuration file %s\n", tok);
      return NULL;
    }
  }
  DDSRT_WARNING_MSVC_ON(4996);
  return fp;
}

struct cfgst *config_init (const char *config, struct config *cfg, uint32_t domid)
{
  int ok = 1;
  struct cfgst *cfgst;
  char env_input[32];
  char *copy, *cursor;
  struct ddsrt_xmlp_callbacks cb;

  memset (cfg, 0, sizeof (*cfg));

  cfgst = ddsrt_malloc (sizeof (*cfgst));
  memset (cfgst, 0, sizeof (*cfgst));
  ddsrt_avl_init (&cfgst_found_treedef, &cfgst->found);
  cfgst->cfg = cfg;
  cfgst->error = 0;
  cfgst->source = 0;
  cfgst->logcfg = NULL;
  cfgst->first_data_in_source = true;
  cfgst->input = "init";
  cfgst->line = 1;

  /* eventually, we domainId.value will be the real domain id selected, even if it was configured
     to the default of "any" and has "isdefault" set; initializing it to the default-default
     value of 0 means "any" in the config & DDS_DOMAIN_DEFAULT in create participant automatically
     ends up on the right value */
  cfgst->cfg->domainId = domid;

  cb.attr = proc_attr;
  cb.elem_close = proc_elem_close;
  cb.elem_data = proc_elem_data;
  cb.elem_open = proc_elem_open;
  cb.error = proc_error;

  copy = ddsrt_strdup (config);
  cursor = copy;
  while (*cursor && (isspace ((unsigned char) *cursor) || *cursor == ','))
    cursor++;
  while (ok && cursor && cursor[0])
  {
    struct ddsrt_xmlp_state *qx;
    FILE *fp;
    char *tok;
    tok = cursor;
    if (tok[0] == '<')
    {
      /* Read XML directly from input string */
      qx = ddsrt_xmlp_new_string (tok, cfgst, &cb);
      ddsrt_xmlp_set_options (qx, DDSRT_XMLP_ANONYMOUS_CLOSE_TAG | DDSRT_XMLP_MISSING_CLOSE_AS_EOF);
      fp = NULL;
      (void) snprintf (env_input, sizeof (env_input), "CYCLONEDDS_URI+%u", (unsigned) (tok - copy));
      cfgst->input = env_input;
      cfgst->line = 1;
    }
    else if ((fp = config_open_file (tok, &cursor, domid)) == NULL)
    {
      ddsrt_free (copy);
      goto error;
    }
    else
    {
      qx = ddsrt_xmlp_new_file (fp, cfgst, &cb);
      cfgst->input = tok;
      cfgst->line = 1;
    }

    cfgst->implicit_toplevel = (fp == NULL) ? ITL_ALLOWED : ITL_DISALLOWED;
    cfgst->partial_match_allowed = (fp == NULL);
    cfgst->first_data_in_source = true;
    cfgst_push (cfgst, 0, &root_cfgelem, cfgst->cfg);
    ok = (ddsrt_xmlp_parse (qx) >= 0) && !cfgst->error;
    assert (!ok ||
            (cfgst->path_depth == 1 && cfgst->implicit_toplevel == ITL_DISALLOWED) ||
            (cfgst->path_depth == 1 + (int) cfgst->implicit_toplevel));
    /* Pop until stack empty: error handling is rather brutal */
    while (cfgst->path_depth > 0)
      cfgst_pop (cfgst);
    if (fp != NULL)
      fclose (fp);
    else if (ok)
      cursor = tok + ddsrt_xmlp_get_bufpos (qx);
    ddsrt_xmlp_free (qx);
    assert (fp == NULL || cfgst->implicit_toplevel <= ITL_ALLOWED);
    if (cursor)
    {
      while (*cursor && (isspace ((unsigned char) cursor[0]) || cursor[0] == ','))
        cursor++;
    }
  }
  ddsrt_free (copy);

  /* Set defaults for everything not set that we have a default value
     for, signal errors for things unset but without a default. */
  ok = ok && set_defaults (cfgst, cfgst->cfg, 0, root_cfgelems);

  /* Domain id UINT32_MAX can only happen if the application specified DDS_DOMAIN_DEFAULT
     and the configuration has "any" (either explicitly or as a default).  In that case,
     default to 0.  (Leaving it as UINT32_MAX while reading the config has the advantage
     of warnings/errors being output without a domain id present. */
  if (cfgst->cfg->domainId == UINT32_MAX)
    cfgst->cfg->domainId = 0;

  /* Compatibility settings of IPv6, TCP -- a bit too complicated for
     the poor framework */
  if (ok)
  {
    int ok1 = 1;
    switch (cfgst->cfg->transport_selector)
    {
      case TRANS_DEFAULT:
        if (cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE)
          cfgst->cfg->transport_selector = (cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE) ? TRANS_TCP6 : TRANS_TCP;
        else
          cfgst->cfg->transport_selector = (cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE) ? TRANS_UDP6 : TRANS_UDP;
        break;
      case TRANS_TCP:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_FALSE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE);
        break;
      case TRANS_TCP6:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_FALSE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_FALSE);
        break;
      case TRANS_UDP:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE);
        break;
      case TRANS_UDP6:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_FALSE);
        break;
      case TRANS_RAWETH:
        ok1 = !(cfgst->cfg->compat_tcp_enable == BOOLDEF_TRUE || cfgst->cfg->compat_use_ipv6 == BOOLDEF_TRUE);
        break;
    }
    if (!ok1)
      DDS_ILOG (DDS_LC_ERROR, domid, "config: invalid combination of Transport, IPv6, TCP\n");
    ok = ok && ok1;
    cfgst->cfg->compat_use_ipv6 = (cfgst->cfg->transport_selector == TRANS_UDP6 || cfgst->cfg->transport_selector == TRANS_TCP6) ? BOOLDEF_TRUE : BOOLDEF_FALSE;
    cfgst->cfg->compat_tcp_enable = (cfgst->cfg->transport_selector == TRANS_TCP || cfgst->cfg->transport_selector == TRANS_TCP6) ? BOOLDEF_TRUE : BOOLDEF_FALSE;
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  /* Default channel gets set outside set_defaults -- a bit too
     complicated for the poor framework */
  if (ok)
  {
    if (set_default_channel (cfgst->cfg) < 0)
      ok = 0;
    if (cfgst->cfg->channels && sort_channels_check_nodups (cfgst->cfg) < 0)
      ok = 0;
  }
#endif

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* Assign network partition ids */
  if (ok)
  {
    struct config_networkpartition_listelem *p = cfgst->cfg->networkPartitions;
    cfgst->cfg->nof_networkPartitions = 0;
    while (p)
    {
      cfgst->cfg->nof_networkPartitions++;
      p->partitionId = cfgst->cfg->nof_networkPartitions; /* starting at 1 */
      p = p->next;
    }
  }

  /* Create links from the partitionmappings to the network partitions
     and signal errors if partitions do not exist */
  if (ok)
  {
    struct config_partitionmapping_listelem * m = cfgst->cfg->partitionMappings;
    while (m)
    {
      struct config_networkpartition_listelem * p = cfgst->cfg->networkPartitions;
      while (p && ddsrt_strcasecmp(m->networkPartition, p->name) != 0)
        p = p->next;
      if (p)
        m->partition = p;
      else
      {
        DDS_ILOG (DDS_LC_ERROR, domid, "config: DDSI2Service/Partitioning/PartitionMappings/PartitionMapping[@networkpartition]: %s: unknown partition\n", m->networkPartition);
        ok = 0;
      }
      m = m->next;
    }
  }
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

  if (ok)
  {
    cfgst->cfg->valid = 1;
    return cfgst;
  }

error:
  free_configured_elements (cfgst, cfgst->cfg, root_cfgelems);
  ddsrt_avl_free (&cfgst_found_treedef, &cfgst->found, ddsrt_free);
  ddsrt_free (cfgst);
  return NULL;
}

void config_print_cfgst (struct cfgst *cfgst, const struct ddsrt_log_cfg *logcfg)
{
  if (cfgst == NULL)
    return;
  assert (cfgst->logcfg == NULL);
  cfgst->logcfg = logcfg;
  print_configitems (cfgst, cfgst->cfg, 0, root_cfgelems, 0);
}

void config_free_source_info (struct cfgst *cfgst)
{
  assert (!cfgst->error);
  ddsrt_avl_free (&cfgst_found_treedef, &cfgst->found, ddsrt_free);
}

void config_fini (struct cfgst *cfgst)
{
  assert (cfgst);
  assert (cfgst->cfg != NULL);
  assert (cfgst->cfg->valid);

  free_all_elements (cfgst, cfgst->cfg, root_cfgelems);
  dds_set_log_file (stderr);
  dds_set_trace_file (stderr);
  if (cfgst->cfg->tracefp && cfgst->cfg->tracefp != stdout && cfgst->cfg->tracefp != stderr) {
    fclose(cfgst->cfg->tracefp);
  }
  memset (cfgst->cfg, 0, sizeof (*cfgst->cfg));
  ddsrt_avl_free (&cfgst_found_treedef, &cfgst->found, ddsrt_free);
  ddsrt_free (cfgst);
}

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static char *get_partition_search_pattern (const char *partition, const char *topic)
{
  size_t sz = strlen (partition) + strlen (topic) + 2;
  char *pt = ddsrt_malloc (sz);
  (void) snprintf (pt, sz, "%s.%s", partition, topic);
  return pt;
}

struct config_partitionmapping_listelem *find_partitionmapping (const struct config *cfg, const char *partition, const char *topic)
{
  char *pt = get_partition_search_pattern (partition, topic);
  struct config_partitionmapping_listelem *pm;
  for (pm = cfg->partitionMappings; pm; pm = pm->next)
    if (WildcardOverlap (pt, pm->DCPSPartitionTopic))
      break;
  ddsrt_free (pt);
  return pm;
}

struct config_networkpartition_listelem *find_networkpartition_by_id (const struct config *cfg, uint32_t id)
{
  struct config_networkpartition_listelem *np;
  for (np = cfg->networkPartitions; np; np = np->next)
    if (np->partitionId == id)
      return np;
  return 0;
}

int is_ignored_partition (const struct config *cfg, const char *partition, const char *topic)
{
  char *pt = get_partition_search_pattern (partition, topic);
  struct config_ignoredpartition_listelem *ip;
  for (ip = cfg->ignoredPartitions; ip; ip = ip->next)
    if (WildcardOverlap(pt, ip->DCPSPartitionTopic))
      break;
  ddsrt_free (pt);
  return ip != NULL;
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
struct config_channel_listelem *find_channel (const struct config *cfg, nn_transport_priority_qospolicy_t transport_priority)
{
  struct config_channel_listelem *c;
  /* Channel selection is to use the channel with the lowest priority
     not less than transport_priority, or else the one with the
     highest priority. */
  assert(cfg->channels != NULL);
  assert(cfg->max_channel != NULL);
  for (c = cfg->channels; c; c = c->next)
  {
    assert(c->next == NULL || c->next->priority > c->priority);
    if (transport_priority.value <= c->priority)
      return c;
  }
  return cfg->max_channel;
}
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
