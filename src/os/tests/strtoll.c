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
#include "CUnit/Test.h"
#include "os/os.h"

long long ll;
unsigned long long ull;
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
long long llmin = OS_MIN_INTEGER(long long);
long long llmax = OS_MAX_INTEGER(long long);
unsigned long long ullmax = OS_MAX_INTEGER(unsigned long long);

CU_Init(os_str_convert)
{
    int result = 0;
    os_osInit();
    printf("Run os_str_convert_Initialize\n");

    (void)snprintf (str_llmin, sizeof(str_llmin), "%lld", llmin);
    (void)snprintf (str_llmax, sizeof(str_llmax), "%lld", llmax);
    (void)snprintf (str_llrange, sizeof(str_llrange), "%lld1", llmax);
    (void)snprintf (str_ullmax, sizeof(str_ullmax), "%llu", ullmax);
    (void)snprintf (str_ullrange, sizeof(str_ullrange), "%llu1", ullmax);
    (void)snprintf (str_xllmin, sizeof(str_xllmin), "-%llx", llmin);
    (void)snprintf (str_xllmax, sizeof(str_xllmax), "+%llx", llmax);

    return result;
}

CU_Clean(os_str_convert)
{
    int result = 0;

    printf("Run os_str_convert_Cleanup\n");
    os_osExit();
    return result;
}

CU_Test(os_str_convert, strtoll)
{
    printf ("Starting os_strtoll_001a\n");
    str = "gibberish";
    ll = os_strtoll(str, &ptr, 0);
    CU_ASSERT (ll == 0 && ptr == str);

    printf ("Starting os_strtoll_001b\n");
    str = "+gibberish";
    ll = os_strtoll(str, &ptr, 0);
    CU_ASSERT (ll == 0 && ptr == str);

    printf ("Starting os_strtoll_001c\n");
    str = "-gibberish";
    ll = os_strtoll(str, &ptr, 0);
    CU_ASSERT (ll == 0 && ptr == str);

    printf ("Starting os_strtoll_001d\n");
    str = "gibberish";
    ptr = NULL;
    errno=0;
    ll = os_strtoll(str, &ptr, 36);
    CU_ASSERT (ll == 46572948005345 && errno == 0 && ptr && *ptr == '\0');

    printf ("Starting os_strtoll_001e\n");
    str = "1050505055";
    ptr = NULL;
    errno = 0;
    ll = os_strtoll(str, &ptr, 37);
    CU_ASSERT (ll == 0LL && errno == EINVAL && ptr == str);

    printf ("Starting os_strtoll_001f\n");
    str = " \t \n 1050505055";
    ll = os_strtoll(str, NULL, 10);
    CU_ASSERT (ll == 1050505055LL);

    printf ("Starting os_strtoll_001g\n");
    str = " \t \n -1050505055";
    ptr = NULL;
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == -1050505055LL);

    printf ("Starting os_strtoll_001h\n");
    str = " \t \n - \t \n 1050505055";
    ptr = NULL;
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == 0LL && ptr == str);

    printf ("Starting os_strtoll_002a\n");
    str = "10x";
    ptr = NULL;
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == 10LL && ptr && *ptr == 'x');

    printf ("Starting os_strtoll_002b\n");
    str = "+10x";
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == 10LL && ptr && *ptr == 'x');

    printf ("Starting os_strtoll_002c\n");
    str = "-10x";
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == -10LL && ptr && *ptr == 'x');

    printf ("Starting os_strtoll_002d\n");
    str = (const char *)str_llmax;
    ll = os_strtoll(str, NULL, 10);
    CU_ASSERT (ll == llmax);

    printf ("Starting os_strtoll_002e\n");
    str = (const char *)str_llmin;
    ll = os_strtoll(str, NULL, 10);
    CU_ASSERT (ll == llmin);

    printf ("Starting os_strtoll_002f\n");
    str = (const char *)str_llrange;
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == llmax && *ptr == '1');

    printf ("Starting os_strtoll_003a\n");
    str = "0x100";
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == 0x100LL);

    printf ("Starting os_strtoll_003b\n");
    str = "0X100";
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == 0x100LL);

    printf ("Starting os_strtoll_003c\n");
    str = "0x1DEFCAB";
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == 0x1DEFCABLL);

    printf ("Starting os_strtoll_003d\n");
    str = "0x1defcab";
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == 0x1DEFCABLL);

    printf ("Starting os_strtoll_003e\n");
    str = (char *)str_xllmin;
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == llmin);

    printf ("Starting os_strtoll_003f\n");
    str = (char *)str_xllmax;
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == llmax);

    printf ("Starting os_strtoll_003g\n");
    str = "0x100";
    ll = os_strtoll(str, NULL, 0);
    CU_ASSERT (ll == 0x100LL);

    printf ("Starting os_strtoll_003h\n");
    str = "100";
    ll = os_strtoll(str, NULL, 16);
    CU_ASSERT (ll == 0x100LL);

    printf ("Starting os_strtoll_003i\n");
    /* calling os_strtoll with \"%s\" and base 10, expected result 0 */
    str = "0x100";
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT (ll == 0 && ptr && *ptr == 'x');

    printf ("Starting os_strtoll_003j\n");
    /* calling os_strtoll with \"%s\" and base 0, expected result 256 */
    str = "0x100g";
    ll = os_strtoll(str, &ptr, 0);
    CU_ASSERT (ll == 256 && ptr && *ptr == 'g');

    printf ("Starting os_strtoll_004a\n");
    str = "0100";
    ll = os_strtoll(str, NULL, 0);
    CU_ASSERT(ll == 64LL);

    printf ("Starting os_strtoll_004b\n");
    str = "0100";
    ll = os_strtoll(str, NULL, 8);
    CU_ASSERT(ll == 64LL);

    printf ("Starting os_strtoll_004c\n");
    str = "100";
    ll = os_strtoll(str, NULL, 8);
    CU_ASSERT(ll == 64LL);

    printf ("Starting os_strtoll_004d\n");
    /* calling os_strtoll with \"%s\" and base 10, expected result 100 */
    str = "0100";
    ll = os_strtoll(str, &ptr, 10);
    CU_ASSERT(ll == 100);

    printf ("Starting os_strtoll_004e\n");
    /* calling os_strtoll with \"%s\" and base 0, expected result 64 */
    str = "01008";
    ll = os_strtoll(str, &ptr, 8);
    CU_ASSERT(ll == 64LL && ptr && *ptr == '8');

    printf ("Starting os_strtoll_004f\n");
    str = "00001010";
    ll = os_strtoll(str, NULL, 2);
    CU_ASSERT(ll == 10LL);

    printf ("Ending os_strtoll\n");
}

CU_Test(os_str_convert, strtoull)
{
    printf ("Starting os_strtoull_001a\n");
    str = "0xffffffffffffffff";
    ull = os_strtoull(str, NULL, 0);
    CU_ASSERT(ull == ullmax);

    printf ("Starting os_strtoull_001b\n");
    str = "-1";
    ull = os_strtoull(str, NULL, 0);
    CU_ASSERT(ull == ullmax);

    printf ("Starting os_strtoull_001c\n");
    str = "-2";
    ull = os_strtoull(str, NULL, 0);
    CU_ASSERT(ull == (ullmax - 1));

    printf ("Ending os_strtoull\n");
}

CU_Test(os_str_convert, atoll)
{
    printf ("Starting os_atoll_001\n");
    str = "10";
    ll = os_atoll(str);
    CU_ASSERT(ll == 10);

    printf ("Ending os_atoll\n");
}

CU_Test(os_str_convert, atoull)
{
    printf ("Starting os_atoull_001\n");
    str = "10";
    ull = os_atoull(str);
    CU_ASSERT(ull == 10);

    printf ("Ending tc_os_atoull\n");
}

CU_Test(os_str_convert, lltostr)
{
    printf ("Starting os_lltostr_001\n");
    ll = llmax;
    ptr = os_lltostr(ll, buf, 0, NULL);
    CU_ASSERT(ptr == NULL);

    printf ("Starting os_lltostr_002\n");
    /* calling os_lltostr with %lld with buffer size of 5, expected result \"5432\" */
    ll = 54321;
    ptr = os_lltostr(ll, buf, 5, NULL);
    CU_ASSERT(strcmp(ptr, "5432") == 0);

    printf ("Starting os_lltostr_003a\n");
    ll = llmax;
    ptr = os_lltostr(ll, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, str_llmax) == 0);

    printf ("Starting os_lltostr_003b\n");
    ll = llmin;
    ptr = os_lltostr(ll, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, str_llmin) == 0);

    printf ("Starting os_lltostr_004\n");
    ll = 1;
    ptr = os_lltostr(ll, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, "1") == 0);

    printf ("Starting os_lltostr_005\n");
    ll = 0;
    ptr = os_lltostr(ll, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, "0") == 0);

    printf ("Starting os_lltostr_006\n");
    ll = -1;
    ptr = os_lltostr(ll, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, "-1") == 0);

    printf ("Ending os_lltostr\n");
}

CU_Test(os_str_convert, ulltostr)
{
    printf ("Starting os_ulltostr_001\n");
    ull = ullmax;
    ptr = os_ulltostr(ull, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, str_ullmax) == 0);

    printf ("Starting os_ulltostr_002\n");
    ull = 0ULL;
    ptr = os_ulltostr(ull, buf, sizeof(buf), NULL);
    CU_ASSERT(strcmp(ptr, "0") == 0);

    printf ("Ending os_ulltostr\n");
}

