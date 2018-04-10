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
#ifndef CUNIT_RUNNER_H
#define CUNIT_RUNNER_H

#include <stdbool.h>
#include <CUnit/CUnit.h>
#include <CUnit/CUError.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define CUnit_Suite_Initialize_Name__(s) \
    s ## _Initialize
#define CUnit_Suite_Initialize(s) \
    int CUnit_Suite_Initialize_Name__(s)(void)
#define CUnit_Suite_Initialize_Decl__(s) \
    extern CUnit_Suite_Initialize(s)
#define CUnit_Suite_Initialize__(s) \
    CUnit_Suite_Initialize_Name__(s)

#define CUnit_Suite_Cleanup_Name__(s) \
    s ## _Cleanup
#define CUnit_Suite_Cleanup(s) \
    int CUnit_Suite_Cleanup_Name__(s)(void)
#define CUnit_Suite_Cleanup_Decl__(s) \
    extern CUnit_Suite_Cleanup(s)
#define CUnit_Suite_Cleanup__(s) \
    CUnit_Suite_Cleanup_Name__(s)

#define CUnit_Test_Name__(s, t) \
    s ## _ ## t
#define CUnit_Test(s, t, ...) \
    void CUnit_Test_Name__(s, t)(void)
#define CUnit_Test_Decl__(s, t) \
    extern CUnit_Test(s, t)

#define CUnit_Suite__(s, c, d) \
    cu_runner_add_suite(#s, c, d)
#define CUnit_Test__(s, t, e) \
    cu_runner_add_test(#s, #t, CUnit_Test_Name__(s, t), e)

CU_ErrorCode
cu_runner_init(
    int argc,
    char *argv[]);

void
cu_runner_fini(
    void);

void
cu_runner_add_suite(
    const char *suite,
    CU_InitializeFunc pInitFunc,
    CU_CleanupFunc pCleanFunc);

void
cu_runner_add_test(
    const char *suite,
    const char *test,
    CU_TestFunc pTestFunc,
    bool enable);

CU_ErrorCode
cu_runner_run(
    void);

#if defined (__cplusplus)
}
#endif

#endif /* CUNIT_RUNNER_H */
