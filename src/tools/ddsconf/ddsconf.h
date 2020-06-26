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
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

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
