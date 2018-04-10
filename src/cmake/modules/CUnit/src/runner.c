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
#include <CUnit/Basic.h>
#include <CUnit/Automated.h>

#include "CUnit/Runner.h"

static struct cunit_runner {
    bool automated;
    bool junit;
    const char * results;
    CU_BasicRunMode mode;
    CU_ErrorAction error_action;
    const char *suite;
    const char *test;
} runner;

static void
usage(
   const char * name)
{
    fprintf(stderr, "usage: %s [flags]\n", name);
    fprintf(stderr, "Supported flags:\n");
    fprintf(stderr, " -a run in automated mode\n");
    fprintf(stderr, " -r <file_name> results file for automated run\n");
    fprintf(stderr, " -j junit format results \n");
    fprintf(stderr, " -f fail fast \n");
    fprintf(stderr, " -s suite\n");
    fprintf(stderr, " -t test\n");
}

int
patmatch(
    const char *pat,
    const char *str)
{
    while (*pat) {
        if (*pat == '?') {
            /* any character will do */
            if (*str++ == 0) {
                return 0;
            }
            pat++;
        } else if (*pat == '*') {
            /* collapse a sequence of wildcards, requiring as many
               characters in str as there are ?s in the sequence */
            while (*pat == '*' || *pat == '?') {
                if (*pat == '?' && *str++ == 0) {
                    return 0;
                }
                pat++;
            }
            /* try matching on all positions where str matches pat */
            while (*str) {
                if (*str == *pat && patmatch(pat+1, str+1)) {
                    return 1;
                }
                str++;
            }
            return *pat == 0;
        } else {
            /* only an exact match */
            if (*str++ != *pat++) {
                return 0;
            }
        }
    }

    return *str == 0;
}

CU_ErrorCode
cu_runner_init(
    int argc,
    char* argv[])
{
    int c, i;
    CU_ErrorCode e = CUE_SUCCESS;

    runner.automated = false;
    runner.junit = false;
    runner.results = NULL;
    runner.mode = CU_BRM_NORMAL;
    runner.error_action = CUEA_IGNORE;
    runner.suite = "*";
    runner.test = "*";

    for (i = 1; e == CUE_SUCCESS && i < argc; i++) {
        c = (argv[i][0] == '-') ? argv[i][1] : -1;
        switch (argv[i][1]) {
            case 'a':
                runner.automated = true;
                break;
            case 'f':
                runner.error_action = CUEA_FAIL;
                break;
            case 'j':
                runner.junit = true;
                break;
            case 'r':
                if((i+1) < argc){
                    runner.results = argv[++i];
                    break;
                }
                /* no break */
            case 's':
                if ((i+1) < argc) {
                    runner.suite = argv[++i];
                    break;
                }
                /* no break */
            case 't':
                if ((i+1) < argc) {
                    runner.test = argv[++i];
                    break;
                }
                /* no break */
            default:
                e = (CU_ErrorCode)256;
                CU_set_error(e); /* Will print as "Undefined Errpr" */
                usage(argv[0]);
                break;
        }
    }

    if (e == CUE_SUCCESS) {
        if ((e = CU_initialize_registry()) != CUE_SUCCESS) {
            fprintf(
                stderr, "Test registry initialization failed: %s\n", CU_get_error_msg());
        }
    }

    CU_set_error_action (runner.error_action);

    return e;
}

void
cu_runner_fini(
    void)
{
    CU_cleanup_registry();
}

void
cu_runner_add_suite(
    const char *suite,
    CU_InitializeFunc pInitFunc,
    CU_CleanupFunc pCleanFunc)
{
    CU_pSuite pSuite;

    pSuite = CU_get_suite(suite);
    if (pSuite == NULL) {
        pSuite = CU_add_suite(suite, pInitFunc, pCleanFunc);
        //assert(pSuite != NULL);
        CU_set_suite_active(pSuite, patmatch(runner.suite, suite));
    }
}

void
cu_runner_add_test(
    const char *suite,
    const char *test,
    CU_TestFunc pTestFunc,
    bool enable)
{
    CU_pSuite pSuite;
    CU_pTest pTest;

    pSuite = CU_get_suite(suite);
    //assert(pSuite != NULL);
    pTest = CU_add_test(pSuite, test, pTestFunc);
    //assert(pTest != NULL);
    CU_set_test_active(pTest, enable && patmatch(runner.test, test));
}

CU_ErrorCode
cu_runner_run(
    void)
{
    if (runner.automated) {
        /* Generate CUnit or JUnit format results */
        if (runner.results != NULL) {
            CU_set_output_filename(runner.results);
        }

        if (runner.junit) {
            CU_automated_enable_junit_xml(CU_TRUE);
        } else {
            CU_list_tests_to_file();
        }
        CU_automated_run_tests();
    } else {
        CU_basic_set_mode(runner.mode);
        CU_basic_run_tests();
    }

    if (CU_get_error() == 0) {
        return (CU_get_number_of_failures() != 0);
    }

    return CU_get_error();
}
