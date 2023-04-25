// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "assert.h"
#include "access_control_utils.h"


CU_Test(ddssec_builtin_access_control_fnmatch, basic)
{
  CU_ASSERT(ac_fnmatch("", ""));
  CU_ASSERT(ac_fnmatch("abc", "abc"));
  CU_ASSERT(!ac_fnmatch("abc", "ab"));
  CU_ASSERT(!ac_fnmatch("", "a"));
  CU_ASSERT(!ac_fnmatch("a", ""));

  CU_ASSERT(ac_fnmatch("a?", "ab"));
  CU_ASSERT(ac_fnmatch("?b", "ab"));
  CU_ASSERT(ac_fnmatch("a?c", "abc"));
  CU_ASSERT(!ac_fnmatch("a?", "abc"));
  CU_ASSERT(!ac_fnmatch("?c", "abc"));

  CU_ASSERT(ac_fnmatch("a*", "a"));
  CU_ASSERT(ac_fnmatch("a*", "abc"));
  CU_ASSERT(ac_fnmatch("a*c", "abc"));
  CU_ASSERT(ac_fnmatch("a*c", "abbc"));
  CU_ASSERT(ac_fnmatch("*c", "abc"));
  CU_ASSERT(ac_fnmatch("*c", "c"));
  CU_ASSERT(!ac_fnmatch("a*", ""));
  CU_ASSERT(!ac_fnmatch("a*c", "bc"));

  CU_ASSERT(ac_fnmatch("[ab]", "a"));
  CU_ASSERT(ac_fnmatch("[ab]", "b"));
  CU_ASSERT(ac_fnmatch("a[bc]", "ab"));
  CU_ASSERT(ac_fnmatch("a[bc]", "ac"));
  CU_ASSERT(ac_fnmatch("a[bc]d", "abd"));
  CU_ASSERT(ac_fnmatch("a[b-d]", "ab"));
  CU_ASSERT(ac_fnmatch("a[b-d]", "ac"));
  CU_ASSERT(ac_fnmatch("a[b-d]", "ad"));
  CU_ASSERT(ac_fnmatch("a[-b]", "ab"));
  CU_ASSERT(ac_fnmatch("a[!b]", "ac"));
  CU_ASSERT(ac_fnmatch("a[!bc]d", "aad"));
  CU_ASSERT(ac_fnmatch("a]", "a]"));
  CU_ASSERT(!ac_fnmatch("[ab]", "c"));
  CU_ASSERT(!ac_fnmatch("a[bc]", "ad"));
  CU_ASSERT(!ac_fnmatch("a[bc]", "abc"));
  CU_ASSERT(!ac_fnmatch("a[b-]", "ab"));
  CU_ASSERT(!ac_fnmatch("a[-", "a"));
  CU_ASSERT(!ac_fnmatch("a[", "a["));
  CU_ASSERT(!ac_fnmatch("a[-", "a[-"));
  CU_ASSERT(!ac_fnmatch("a[!b]", "ab"));
  CU_ASSERT(!ac_fnmatch("a[!bc]d", "abd"));
  CU_ASSERT(!ac_fnmatch("a[!b-d]", "ac"));
  CU_ASSERT(!ac_fnmatch("a[!-b]", "ab"));
}
