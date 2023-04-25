// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "test_common.h"

CU_Test(ddsc_basic, test)
{
    dds_entity_t participant;
    dds_return_t status;

    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    status = dds_delete(participant);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
}
