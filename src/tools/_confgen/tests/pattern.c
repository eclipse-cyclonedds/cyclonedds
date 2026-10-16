// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Test.h"

int confgen_main (int argc, char **argv);
#define main confgen_main
#include "../_confgen.c"
#undef main

static void check_pattern (const char *type, const char **values, const char *default_value, const char *expected)
{
  struct cfgelem elem = {
    .value = default_value,
    .meta = { .type = type, .values = values }
  };
  CU_ASSERT_EQ_FATAL (makepattern (&elem, NULL), 0);
  CU_ASSERT_NEQ_FATAL (elem.meta.pattern, NULL);
  CU_ASSERT_STREQ (elem.meta.pattern, expected);
  free (elem.meta.pattern);
}

CU_Test (confgen, enum_pattern)
{
  const char *empty[] = { NULL };
  const char *one[] = { "a", NULL };
  const char *two[] = { "a", "bb", NULL };
  check_pattern ("enum", empty, NULL, "()");
  check_pattern ("enum", one, NULL, "(\"a\")");
  check_pattern ("enum", two, NULL, "(\"a\"|\"bb\")");
}

CU_Test (confgen, list_pattern)
{
  const char *empty[] = { NULL };
  const char *one[] = { "a", NULL };
  const char *two[] = { "a", "bb", NULL };
  check_pattern ("list", empty, NULL, "(()(,())*)|");
  check_pattern ("list", one, NULL, "((a)(,(a))*)|");
  check_pattern ("list", two, NULL, "((a|bb)(,(a|bb))*)|");
  check_pattern ("list", two, "default", "default|((a|bb)(,(a|bb))*)");
}
