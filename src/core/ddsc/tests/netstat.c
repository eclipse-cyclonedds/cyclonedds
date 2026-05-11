// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/netstat.h"

#include "dds__entity.h"
#include "dds__types.h"
#include "test_common.h"

CU_Test(ddsc_netstat, get)
{
#if DDSRT_HAVE_NETSTAT
  dds_entity_t participant;
  dds_return_t ret;
  dds_entity *entity;
  const char *device;
  struct ddsrt_netstat_control *control = NULL;
  struct ddsrt_netstat stats = {
    .ipkt = UINT64_MAX,
    .opkt = UINT64_MAX,
    .ibytes = UINT64_MAX,
    .obytes = UINT64_MAX
  };

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  ret = dds_entity_pin (participant, &entity);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_GT_FATAL (entity->m_domain->gv.n_interfaces, 0);
  CU_ASSERT_NEQ_FATAL (entity->m_domain->gv.interfaces[0].name, NULL);
  device = entity->m_domain->gv.interfaces[0].name;
  dds_entity_unpin (entity);

  ret = ddsrt_netstat_new (&control, device);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL (control, NULL);

  ret = ddsrt_netstat_get (control, &stats);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT (
    stats.ipkt != UINT64_MAX ||
    stats.opkt != UINT64_MAX ||
    stats.ibytes != UINT64_MAX ||
    stats.obytes != UINT64_MAX);

  CU_ASSERT_EQ (ddsrt_netstat_free (control), DDS_RETCODE_OK);
  CU_ASSERT_EQ (dds_delete (participant), DDS_RETCODE_OK);
#else
  CU_PASS ("netstat is not supported");
#endif
}
