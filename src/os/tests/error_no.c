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


CU_Init(os_errno)
{
    int result = 0;
    os_osInit();
    printf("Run os_errno_Initialize\n");

    return result;
}

CU_Clean(os_errno)
{
    int result = 0;
    os_osExit();
    printf("Run os_errno_Cleanup\n");

    return result;
}

CU_Test(os_errno, get_and_set)
{
    printf ("Starting os_errno_get_and_set_001\n");
    os_setErrno (0);
    CU_ASSERT (os_getErrno () == 0);

    printf ("Starting os_errno_get_and_set_002\n");
    os_setErrno (0);
    /* Call strtol with an invalid format on purpose. */
    (void)strtol ("1000000000000000000000000000000000000000000000000", NULL, 10);
    CU_ASSERT (os_getErrno () != 0);

    printf ("Ending tc_os_errno\n");
}

CU_Test(os_errstr, no_space)
{
    int err;
    char buf[1] = { 0 };
    err = os_errstr(OS_HOST_NOT_FOUND, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, ERANGE);
}

/* os_errstr only provides string representations for internal error codes. */
CU_Test(os_errstr, bad_errno)
{
    int err;
    char buf[128];
    buf[0] = '\0';
    err = os_errstr(OS_ERRBASE, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, EINVAL);
    err = os_errstr(EINVAL, buf, sizeof(buf));
    CU_ASSERT_EQUAL(err, EINVAL);
}
