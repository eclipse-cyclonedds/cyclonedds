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
#include <criterion/criterion.h>
#include <criterion/logging.h>

#define MAIN test_main
#include "../pubsub.c"

Test(tools_pubsub, main) {
    char *argv[] = {"pubsub", "-T", "pubsubTestTopic", "-K", "KS", "-w1:1", "-D", "1", "-q", "t:d=t,r=r", "pubsub_partition"};
    int argc = sizeof(argv) / sizeof(char*);

    cr_log_info("Starting pubsub basic test");
    int result = MAIN(argc, argv);
    if (result != 0)
        printf("exitcode was %d\n", result);
    cr_assert_eq(result, 0, "pubsub exited non-zero");
    cr_log_info("Stopping pubsub basic test");
}
