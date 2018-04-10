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
#include "ddsc/dds.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>

Test(ddsc_basic, test)
{
  dds_entity_t participant;
  dds_return_t status;

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0);

  /* TODO: CHAM-108: Add some simple read/write test(s). */

  status = dds_delete(participant);
  cr_assert_eq(status, DDS_RETCODE_OK);
}
