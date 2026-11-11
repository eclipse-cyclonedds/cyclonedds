// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "CUnit/Test.h"
#include "psmx_iox2_impl.h"

CU_Test(ddsc_iox2, invalid_instance_name)
{
  // PSMX accepts names longer than the 255-byte iceoryx2 file name limit.
  char config[512] = "LOCATOR=0123456789abcdef0123456789abcdef;INSTANCE_NAME=";
  char *name = config + strlen (config);
  memset (name, 'a', 256);
  name[256] = ';';
  dds_psmx_t *psmx = NULL;
  CU_ASSERT_EQ (iox2_create_psmx (&psmx, 1, config), DDS_RETCODE_ERROR);
  CU_ASSERT_EQ (psmx, NULL);
}
