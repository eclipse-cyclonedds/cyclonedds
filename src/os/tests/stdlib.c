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

#define ENABLE_TRACING 0

static int
vsnprintfTest(
              const char *format,
              ...)
{
    va_list varargs;
    int result = 0;
    char description[10];
    va_start(varargs, format);
    memset(description, 0, sizeof(description));

    result = os_vsnprintf(description, sizeof(description)-1, format, varargs);
    va_end(varargs);
    return result;
}

CU_Init(os_stdlib)
{
    int result = 0;
    os_osInit();
    return result;
}

CU_Clean(os_stdlib)
{
    os_osExit();
    return 0;
}

CU_Test(os_stdlib, gethostname)
{
    int res;
    os_result os_res;
    char os_cpu[200];
    char cpu[200];

    printf ("Starting os_stdlib_gethostname_001\n");
    os_cpu[0] = '\0';
    os_res = os_gethostname (os_cpu, sizeof(os_cpu));
    CU_ASSERT (os_res == os_resultSuccess);

    cpu[0] = '\0';
    res = gethostname (cpu, sizeof(cpu));
    CU_ASSERT (res == 0);

    printf ("Starting os_stdlib_gethostname_002\n");
    os_res = os_gethostname (os_cpu, strlen(os_cpu)-1);
    CU_ASSERT (os_res == os_resultFail);
    printf ("Ending os_stdlib_gethostname\n");
}

CU_Test(os_stdlib, putenv)
{
    os_result os_res;

    printf ("Starting os_stdlib_putenv_001\n");
    os_res = os_putenv ("ABCDE=FGHIJ");
    CU_ASSERT (os_res == os_resultSuccess);
    CU_ASSERT (strcmp (os_getenv("ABCDE"), "FGHIJ") == 0);
    printf ("Ending os_stdlib_putenv\n");
}

CU_Test(os_stdlib, getenv)
{
    const char *env;
    os_result res;

    printf ("Starting os_stdlib_getenv_001\n");

    res = os_putenv("ABCDE=FGHIJ");
    CU_ASSERT(res == os_resultSuccess);

    env = os_getenv("ABCDE");
    CU_ASSERT(env != NULL);
    if (env != NULL) {
        CU_ASSERT(strcmp(env, "FGHIJ") == 0);
    }
    printf ("Starting os_stdlib_getenv_002\n");
    CU_ASSERT (os_getenv("XXABCDEXX") == NULL );
    printf ("Ending os_stdlib_getenv\n");
}

CU_Test(os_stdlib, vsnprintf)
{
    printf ("Starting os_stdlib_vsnprintf_001\n");
    CU_ASSERT (vsnprintfTest("%s","test") == 4);
    CU_ASSERT (vsnprintfTest("%d",12) == 2);
    CU_ASSERT (vsnprintfTest("hello %s","world") == 11);

    printf ("Ending os_stdlib_vsnprintf\n");
}

CU_Test(os_stdlib, strtok_r)
{
    char * res;
    char *strtok_r_ts1;
    char *saveptr;

    printf ("Starting os_stdlib_strtok_r_001\n");
     strtok_r_ts1= os_strdup("123,234");
     res = os_strtok_r( strtok_r_ts1, ",", &saveptr );
     CU_ASSERT (strcmp(res, "123") == 0);

    printf ("Starting os_stdlib_strtok_r_002\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "234") == 0);

    printf ("Starting os_stdlib_strtok_r_003\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (res == NULL);
    os_free(strtok_r_ts1);

    printf ("Starting os_stdlib_strtok_r_004\n");
    strtok_r_ts1= os_strdup(",;,123abc,,456,:,");
    res = os_strtok_r( strtok_r_ts1, ",;", &saveptr );
    CU_ASSERT (strcmp(res, "123abc") == 0);

    printf ("Starting os_stdlib_strtok_r_005\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "456") == 0);

    printf ("Starting os_stdlib_strtok_r_006\n");
    res = os_strtok_r( NULL, ",:", &saveptr );
    CU_ASSERT (res == NULL);
    free(strtok_r_ts1);

    printf ("Starting os_stdlib_strtok_r_007\n");
    strtok_r_ts1= os_strdup(",,,123,,456,789,,,");
    res = os_strtok_r( strtok_r_ts1, ",", &saveptr );
    CU_ASSERT (strcmp(res, "123") == 0);

    printf ("Starting os_stdlib_strtok_r_008\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "456") == 0);

    printf ("Starting os_stdlib_strtok_r_009\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "789") == 0);

    printf ("Starting os_stdlib_strtok_r_010\n");
    res = os_strtok_r( NULL, ",:", &saveptr );
    CU_ASSERT (res == NULL);
    free(strtok_r_ts1);

    printf ("Ending os_stdlib_strtok_r\n");
}

CU_Test(os_stdlib, index)
{
    char * res;
    char *index_ts1;
    printf ("Starting os_stdlib_index_001\n");
    index_ts1 = "abc";
    res = os_index( index_ts1, 'a' );
    CU_ASSERT (res == index_ts1);

    printf ("Starting os_stdlib_index_002\n");
    res = os_index( index_ts1, 'c' );
    CU_ASSERT (res == &index_ts1[2]);

    printf ("Starting os_stdlib_index_003\n");
    index_ts1 = "abcdefghij";
    res = os_index( index_ts1, 'f' );
    CU_ASSERT (res == &index_ts1[5]);

    printf ("Starting os_stdlib_index_004\n");
    res = os_index( index_ts1, 'k' );
    CU_ASSERT (res == NULL);

    printf ("Ending os_stdlib_index\n");
}

CU_Test(os_stdlib, getopt)
{
        int c = 0;
        int argc = 3;
        char *argv001[] = {"", "-a", "-b"};
        char *argv002[] = {"", "-c", "foo"};
        char *argv003[] = {"", "-d"};

        /* Check correct functioning of os_getopt */
        printf ("Starting os_stdlib_getopt_001\n");
        c = os_getopt(argc, argv001, "abc:");
        CU_ASSERT (c == 'a');
        c = os_getopt(argc, argv001, "abc:");
        CU_ASSERT (c == 'b');
        c = os_getopt(argc, argv001, "abc:");
        CU_ASSERT (c == -1);

        /* Check correct functioning of os_set_optind and os_get_optind */
        printf ("Starting os_stdlib_getopt_002\n");
        os_set_optind(1);
        CU_ASSERT (os_get_optind() == 1);

        /* Check correct functioning of os_get_optarg */
        printf ("Starting os_stdlib_getopt_003\n");
        c = os_getopt (argc, argv002, "c:");
        CU_ASSERT (c == 'c');
        CU_ASSERT (strcmp(os_get_optarg(), "foo") == 0);
        c = os_getopt(argc, argv002, "c:");
        CU_ASSERT (c == -1);

        /* Check correct functioning of os_set_opterr, os_get_opterr and os_get_optopt */
        printf ("Starting os_stdlib_getopt_004\n");
        argc = 2;
        os_set_optind(1);
        os_set_opterr(0);
        CU_ASSERT(os_get_opterr() == 0)
        c = os_getopt (argc, argv003, "c:");
        CU_ASSERT (c == '?');
        CU_ASSERT (os_get_optopt() == 'd');

        printf ("Ending os_stdlib_getopt\n");
}
