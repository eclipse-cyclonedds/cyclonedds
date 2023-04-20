// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "dds/features.h"


struct cfgelem;

void gendef_pf_nop (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_uint16 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_int32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_uint32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_int64 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_maybe_int32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_maybe_uint32 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_maybe_boolean (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_min_tls_version (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_string (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_networkAddresses (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_tracemask (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_xcheck (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_bandwidth (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_memsize (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_memsize16 (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_networkAddress (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_allow_multicast(FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_maybe_memsize (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_int (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_uint (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_duration (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_domainId(FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_participantIndex (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_boolean (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_boolean_default (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_besmode (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_retransmit_merging (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_sched_class (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_entity_naming_mode (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_random_seed (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_transport_selector (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_many_sockets_mode (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_standards_conformance (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
void gendef_pf_shm_loglevel (FILE *fp, void *parent, struct cfgelem const * const cfgelem);

struct cfgunit {
  const char *name;
  const char *description;
  const char *pattern;
};

#define FLAG_DUPLICATE (1u<<0) /* exact same element exists, print other */
#define FLAG_EXPAND (1u<<1) /* element with same name exists, expand */
#define FLAG_REFERENCE (1u<<2)
#define FLAG_NOMIN (1u<<3)
#define FLAG_NOMAX (1u<<4)

struct cfgmeta {
  char *name;
  char *title;
  char *pattern;
  char *description;
  unsigned int flags;
  const int force_maximum, maximum;
  const int force_minimum, minimum;
  const char *flag;
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
  void (*defconfig_print) (FILE *fp, void *parent, struct cfgelem const * const cfgelem);
  const char *description;
  struct cfgmeta meta;
};

int makedescription(
  struct cfgelem *elem,
  const struct cfgunit *units,
  const char *(*xlat)(const char *, const char **));
int makepattern(
  struct cfgelem *elem,
  const struct cfgunit *units);

const char *schema(void);
const char *url(void);
const char *name(const struct cfgelem *elem);
int ismoved(const struct cfgelem *elem);
int isdeprecated(const struct cfgelem *elem);
int isgroup(const struct cfgelem *elem);
int isnop(const struct cfgelem *elem);
int isbool(const struct cfgelem *elem);
int isint(const struct cfgelem *elem);
int isstring(const struct cfgelem *elem);
int isenum(const struct cfgelem *elem);
int islist(const struct cfgelem *elem);
int minimum(const struct cfgelem *elem);
int maximum(const struct cfgelem *elem);
int haschildren(const struct cfgelem *elem);
int hasattributes(const struct cfgelem *elem);
struct cfgelem *firstelem(const struct cfgelem *list);
struct cfgelem *nextelem(const struct cfgelem *list, const struct cfgelem *elem);
const struct cfgunit *findunit(const struct cfgunit *units, const char *name);
void printspc(FILE *out, unsigned int cols, const char *fmt, ...);
int printrnc(FILE *out, struct cfgelem *elem, const struct cfgunit *units);
int printxsd(FILE *out, struct cfgelem *elem, const struct cfgunit *units);
int printmd(FILE *out, struct cfgelem *elem, const struct cfgunit *units);
int printrst(FILE *out, struct cfgelem *elem, const struct cfgunit *units);
int printdefconfig(FILE *out, struct cfgelem *elem);
