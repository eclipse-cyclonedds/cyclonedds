// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Test.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/strtol.h"

const char *str;
char *ptr;
char buf[100];
char str_llmin[100];
char str_llmax[100];
char str_ullmax[100];
char str_llrange[100];
char str_ullrange[100];

char str_xllmin[99], str_xllmax[99];

/* Really test with the maximum values supported on a platform, not some
   made up number. */
long long llmin = INT64_MIN;
long long llmax = INT64_MAX;
unsigned long long ullmax = UINT64_MAX;

CU_Init(ddsrt_strtoll)
{
  ddsrt_init();
  (void)snprintf (str_llmin, sizeof(str_llmin), "%lld", llmin);
  (void)snprintf (str_llmax, sizeof(str_llmax), "%lld", llmax);
  (void)snprintf (str_llrange, sizeof(str_llrange), "%lld1", llmax);
  (void)snprintf (str_ullmax, sizeof(str_ullmax), "%llu", ullmax);
  (void)snprintf (str_ullrange, sizeof(str_ullrange), "%llu1", ullmax);
  (void)snprintf (str_xllmin, sizeof(str_xllmin), "-%llx", llmin);
  (void)snprintf (str_xllmax, sizeof(str_xllmax), "+%llx", llmax);
  return 0;
}

CU_Clean(ddstr_strtoll)
{
  ddsrt_fini();
  return 0;
}

CU_Test(ddsrt_strtoll, strtoll)
{
  dds_return_t rc;
  long long ll;
  static char dummy[] = "dummy";

  str = "gibberish";
  ll = -1;
  rc = ddsrt_strtoll(str, &ptr, 0, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0 && ptr == str);

  str = "+gibberish";
  ll = -2;
  rc = ddsrt_strtoll(str, &ptr, 0, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0 && ptr == str);

  str = "-gibberish";
  ll = -3;
  rc = ddsrt_strtoll(str, &ptr, 0, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0 && ptr == str);

  str = "gibberish";
  ptr = NULL;
  ll = -4;
  rc = ddsrt_strtoll(str, &ptr, 36, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 46572948005345 && ptr && *ptr == '\0');

  str = "1050505055";
  ptr = dummy;
  ll = -5;
  rc = ddsrt_strtoll(str, &ptr, 37, &ll);
  CU_ASSERT (rc == DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT (ll == -5 && ptr == dummy);

  str = " \t \n 1050505055";
  ll = -6;
  rc = ddsrt_strtoll(str, NULL, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 1050505055LL);

  str = " \t \n -1050505055";
  ptr = NULL;
  ll = -7;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == -1050505055LL);

  str = " \t \n - \t \n 1050505055";
  ptr = NULL;
  ll = -8;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0LL && ptr == str);

  str = "10x";
  ptr = NULL;
  ll = -9;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 10LL && ptr && *ptr == 'x');

  str = "+10x";
  ll = -10;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 10LL && ptr && *ptr == 'x');

  str = "-10x";
  ll = -11;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == -10LL && ptr && *ptr == 'x');

  str = (const char *)str_llmax;
  ll = -12;
  rc = ddsrt_strtoll(str, NULL, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == llmax);

  str = (const char *)str_llmin;
  ll = -13;
  rc = ddsrt_strtoll(str, NULL, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == llmin);

  str = (const char *)str_llrange;
  ll = -14;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OUT_OF_RANGE);
  CU_ASSERT (ll == llmax && *ptr == '1');

  str = "0x100";
  ll = -15;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0x100LL);

  str = "0X100";
  ll = -16;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0x100LL);

  str = "0x1DEFCAB";
  ll = -17;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0x1DEFCABLL);

  str = "0x1defcab";
  ll = -18;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0x1DEFCABLL);

  str = (char *)str_xllmin;
  ll = -19;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == llmin);

  str = (char *)str_xllmax;
  ll = -20;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == llmax);

  str = "0x100";
  ll = -21;
  rc = ddsrt_strtoll(str, NULL, 0, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0x100LL);

  str = "100";
  ll = -22;
  rc = ddsrt_strtoll(str, NULL, 16, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0x100LL);

  /* calling os_strtoll with \"%s\" and base 10, expected result 0 */
  str = "0x100";
  ll = -23;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 0 && ptr && *ptr == 'x');

  /* calling os_strtoll with \"%s\" and base 0, expected result 256 */
  str = "0x100g";
  ll = -24;
  rc = ddsrt_strtoll(str, &ptr, 0, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 256 && ptr && *ptr == 'g');

  str = "0100";
  ll = -25;
  rc = ddsrt_strtoll(str, NULL, 0, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 64LL);

  str = "0100";
  ll = -26;
  rc = ddsrt_strtoll(str, NULL, 8, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 64LL);

  str = "100";
  ll = -27;
  rc = ddsrt_strtoll(str, NULL, 8, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 64LL);

  /* calling os_strtoll with \"%s\" and base 10, expected result 100 */
  str = "0100";
  ll = -28;
  rc = ddsrt_strtoll(str, &ptr, 10, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 100);

  /* calling os_strtoll with \"%s\" and base 0, expected result 64 */
  str = "01008";
  ll = -29;
  rc = ddsrt_strtoll(str, &ptr, 8, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 64LL && ptr && *ptr == '8');

  str = "00001010";
  ll = -30;
  rc = ddsrt_strtoll(str, NULL, 2, &ll);
  CU_ASSERT (rc == DDS_RETCODE_OK);
  CU_ASSERT (ll == 10LL);
}

CU_Test(ddsrt_strtoll, strtoull)
{
  dds_return_t rc;
  unsigned long long ull;

  str = "0xffffffffffffffff";
  ull = 1;
  rc = ddsrt_strtoull(str, NULL, 0, &ull);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT(ull == ullmax);

  str = "-1";
  ull = 2;
  rc = ddsrt_strtoull(str, NULL, 0, &ull);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT(ull == ullmax);

  str = "-2";
  ull = 3;
  rc = ddsrt_strtoull(str, NULL, 0, &ull);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT(ull == (ullmax - 1));
}

CU_Test(ddsrt_strtoll, atoll)
{
  dds_return_t rc;
  long long ll;

  str = "10";
  ll = -1;
  rc = ddsrt_atoll(str, &ll);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT(ll == 10);
}

CU_Test(ddsrt_strtoll, atoull)
{
  dds_return_t rc;
  unsigned long long ull;

  str = "10";
  ull = 1;
  rc = ddsrt_atoull(str, &ull);
  CU_ASSERT_EQUAL(rc, DDS_RETCODE_OK);
  CU_ASSERT(ull == 10);
}

CU_Test(ddsrt_strtoll, lltostr)
{
  long long ll;

  ll = llmax;
  ptr = ddsrt_lltostr(ll, buf, 0, NULL);
  CU_ASSERT(ptr == NULL);

  /* calling os_lltostr with %lld with buffer size of 5, expected result \"5432\" */
  ll = 54321;
  ptr = ddsrt_lltostr(ll, buf, 5, NULL);
  CU_ASSERT(strcmp(ptr, "5432") == 0);

  ll = llmax;
  ptr = ddsrt_lltostr(ll, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, str_llmax) == 0);

  ll = llmin;
  ptr = ddsrt_lltostr(ll, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, str_llmin) == 0);

  ll = 1;
  ptr = ddsrt_lltostr(ll, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, "1") == 0);

  ll = 0;
  ptr = ddsrt_lltostr(ll, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, "0") == 0);

  ll = -1;
  ptr = ddsrt_lltostr(ll, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, "-1") == 0);
}

CU_Test(ddsrt_strtoll, ulltostr)
{
  unsigned long long ull;

  ull = ullmax;
  ptr = ddsrt_ulltostr(ull, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, str_ullmax) == 0);

  ull = 0ULL;
  ptr = ddsrt_ulltostr(ull, buf, sizeof(buf), NULL);
  CU_ASSERT(strcmp(ptr, "0") == 0);
}

