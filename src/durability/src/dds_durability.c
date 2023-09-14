/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "dds/durability/dds_durability.h"
#include "ddsc/dds.h"


dds_return_t dds_durability_init2 (struct ddsi_domaingv* gv)
{
  printf("!!!!!!!!!!!!!!!!!!! src/durability/core/src/dds_durability_init()\n");
  return DDS_RETCODE_OK;
}

dds_return_t dds_durability_fini2 (void)
{
  printf("!!!!!!!!!!!!!!!!!!! src/durability/core/src/dds_durability_fini()\n");
  return DDS_RETCODE_OK;
}

void dds_durability_new_local_reader (struct dds_reader *reader, struct dds_rhc *rhc)
{
  /* create the administration to store transient data */
  /* create a durability reader that sucks and stores it in the store */

  DDSRT_UNUSED_ARG(reader);
  return;
}

void dds_durability_wait_for_ds (uint32_t quorum, dds_time_t timeout)
{
  (void)quorum;
  (void)timeout;
  return;
}


