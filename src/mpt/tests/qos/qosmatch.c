/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "mpt/mpt.h"
#include "procs/rw.h"

#define TEST_PUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_qosmatch")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_qosmatch")
MPT_TestProcess(qos, qosmatch, pub, rw_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(qos, qosmatch, sub, rw_subscriber, TEST_SUB_ARGS);
MPT_Test(qos, qosmatch, .init=rw_init, .fini=rw_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS
