// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef JPARSER_H
#define JPARSER_H

#include <stdbool.h>


#define JPARSER_END_RULE -1

enum jparser_action_kind {
  JPARSER_ACTION_OBJECT_OPEN,
  JPARSER_ACTION_OBJECT_CLOSE,
  JPARSER_ACTION_ARRAY_OPEN,
  JPARSER_ACTION_ARRAY_CLOSE,
  JPARSER_ACTION_VALUE
};

struct jparser;

typedef bool (*jparser_action) (struct jparser *parser, enum jparser_action_kind kind, const char *value, int label, void *arg);

struct jparser_rule {
  int label;
  const char *name;
  jparser_action open_callback;
  jparser_action close_callback;
  jparser_action value_callback;
  const struct jparser_rule * const children;
};

void jparser_error (struct jparser *parser, char *fmt, ...);
bool jparser_parse (const char *fname, const struct jparser_rule * const rules, void *args);

#endif /* JPARSER_H */
