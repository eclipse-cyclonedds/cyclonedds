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
#include "dds/ddsrt/regex.h"

static bool
regex_match(
  const char *pattern,
  const char *str)
{
  ddsrt_regex_t regex = DDSRT_REGEX_INITIALIZER;
  bool result;

  CU_ASSERT_EQ_FATAL(ddsrt_regex_compile(&regex, pattern), DDS_RETCODE_OK);
  result = ddsrt_regex_match(&regex, str);
  ddsrt_regex_fini(&regex);
  return result;
}

static bool
regex_search(
  const char *pattern,
  const char *str)
{
  ddsrt_regex_t regex = DDSRT_REGEX_INITIALIZER;
  bool result;

  CU_ASSERT_EQ_FATAL(ddsrt_regex_compile(&regex, pattern), DDS_RETCODE_OK);
  result = ddsrt_regex_search(&regex, str);
  ddsrt_regex_fini(&regex);
  return result;
}

static dds_return_t
regex_compile(
  const char *pattern)
{
  ddsrt_regex_t regex = DDSRT_REGEX_INITIALIZER;
  dds_return_t rc = ddsrt_regex_compile(&regex, pattern);

  if (rc == DDS_RETCODE_OK)
    ddsrt_regex_fini(&regex);
  return rc;
}

CU_Test(ddsrt_regex, literal)
{
  CU_ASSERT(regex_match("abc", "abc"));
  CU_ASSERT(!regex_match("abc", "ab"));
  CU_ASSERT(!regex_match("abc", "abcd"));
  CU_ASSERT(regex_match("", ""));
  CU_ASSERT(!regex_match("", "a"));
  CU_ASSERT(regex_match("a\\.c", "a.c"));
  CU_ASSERT(!regex_match("a\\.c", "abc"));
  CU_ASSERT(regex_match("a\\nb", "a\nb"));
}

CU_Test(ddsrt_regex, alternation_and_grouping)
{
  CU_ASSERT(regex_match("a|bc", "a"));
  CU_ASSERT(regex_match("a|bc", "bc"));
  CU_ASSERT(!regex_match("a|bc", "b"));
  CU_ASSERT(regex_match("(ab|cd)e", "abe"));
  CU_ASSERT(regex_match("(ab|cd)e", "cde"));
  CU_ASSERT(!regex_match("(ab|cd)e", "ab"));
  CU_ASSERT(regex_match("a(b|c)d", "abd"));
  CU_ASSERT(regex_match("a(b|c)d", "acd"));
  CU_ASSERT(regex_match("(a|)b", "ab"));
  CU_ASSERT(regex_match("(a|)b", "b"));
}

CU_Test(ddsrt_regex, repeat)
{
  CU_ASSERT(regex_match("ab*c", "ac"));
  CU_ASSERT(regex_match("ab*c", "abbc"));
  CU_ASSERT(!regex_match("ab+c", "ac"));
  CU_ASSERT(regex_match("ab+c", "abc"));
  CU_ASSERT(regex_match("ab?c", "ac"));
  CU_ASSERT(regex_match("ab?c", "abc"));
  CU_ASSERT(!regex_match("ab?c", "abbc"));
  CU_ASSERT(regex_match("(ab)+", "abab"));
  CU_ASSERT(regex_match("(ab|c)*", "abcabc"));
}

CU_Test(ddsrt_regex, any_and_anchors)
{
  CU_ASSERT(regex_match("a.c", "abc"));
  CU_ASSERT(regex_match("a.*c", "axyzc"));
  CU_ASSERT(regex_match("^abc$", "abc"));
  CU_ASSERT(!regex_match("^abc$", "xabc"));
  CU_ASSERT(regex_search("bc", "abcd"));
  CU_ASSERT(!regex_match("bc", "abcd"));
  CU_ASSERT(regex_search("^ab", "abcd"));
  CU_ASSERT(!regex_search("^bc", "abcd"));
  CU_ASSERT(regex_search("cd$", "abcd"));
  CU_ASSERT(!regex_search("bc$", "abcd"));
}

CU_Test(ddsrt_regex, character_classes)
{
  CU_ASSERT(regex_match("[a-c]+", "abcba"));
  CU_ASSERT(!regex_match("[a-c]+", "abcd"));
  CU_ASSERT(regex_match("[^0-9]+", "abc"));
  CU_ASSERT(!regex_match("[^0-9]+", "abc1"));
  CU_ASSERT(regex_match("[-a]+", "-a--"));
  CU_ASSERT(regex_match("[a-]+", "a--"));
  CU_ASSERT(regex_match("[]]+", "]]"));
  CU_ASSERT(regex_match("[a\\-c]+", "a-c"));
}

CU_Test(ddsrt_regex, caller_storage)
{
  ddsrt_regex_state_t states[16];
  ddsrt_regex_t regex = DDSRT_REGEX_INITIALIZER;

  CU_ASSERT_EQ_FATAL(
    ddsrt_regex_compile_with_storage(&regex, "(ab|cd)+", states, sizeof(states) / sizeof(states[0])),
    DDS_RETCODE_OK);
  CU_ASSERT(ddsrt_regex_match(&regex, "abcd"));
  CU_ASSERT(!ddsrt_regex_match(&regex, "abce"));
  ddsrt_regex_fini(&regex);
}

CU_Test(ddsrt_regex, invalid_syntax)
{
  ddsrt_regex_state_t states[1];
  ddsrt_regex_t regex = DDSRT_REGEX_INITIALIZER;

  CU_ASSERT_EQ(regex_compile("("), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ(regex_compile("a)"), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ(regex_compile("[abc"), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ(regex_compile("[z-a]"), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ(regex_compile("a**"), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ(regex_compile("\\"), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ(
    ddsrt_regex_compile_with_storage(&regex, "a", states, sizeof(states) / sizeof(states[0])),
    DDS_RETCODE_NOT_ENOUGH_SPACE);
}
