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
#include "dds/dds.h"

CU_Test(ddsc_time, request_time)
{
    dds_time_t now, then;
    dds_duration_t pause = 1 * DDS_NSECS_IN_SEC;

    now = dds_time();
    CU_ASSERT_FATAL(now > 0);
    /* Sleep for 1 second, every platform should (hopefully) support that */
    dds_sleepfor(pause);
    then = dds_time();
    CU_ASSERT_FATAL(then >= (now + pause));
}
