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
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/heap.h"
#include "dds__init.h"
#include "dds__rhc.h"
#include "dds__domain.h"
#include "dds__builtin.h"
#include "dds__whc_builtintopic.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_globals.h"
#include "dds/version.h"

#define DOMAIN_ID_MIN 0
#define DOMAIN_ID_MAX 230

dds_globals dds_global;

dds_return_t dds_init (void)
{
  dds_return_t ret;

  ddsrt_init ();
  ddsrt_mutex_t * const init_mutex = ddsrt_get_singleton_mutex ();
  ddsrt_mutex_lock (init_mutex);
  if (dds_global.m_init_count++ != 0)
  {
    ddsrt_mutex_unlock (init_mutex);
    return DDS_RETCODE_OK;
  }

  ddsrt_mutex_init (&dds_global.m_mutex);
  ddsi_iid_init ();
  thread_states_init_static ();
  thread_states_init (64);
  upgrade_main_thread ();

  if (dds_handle_server_init () != DDS_RETCODE_OK)
  {
    DDS_ERROR("Failed to initialize internal handle server\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_handleserver;
  }

  ddsrt_mutex_unlock (init_mutex);
  return DDS_RETCODE_OK;

fail_handleserver:
  ddsrt_mutex_destroy (&dds_global.m_mutex);
  dds_global.m_init_count--;
  ddsrt_mutex_unlock (init_mutex);
  ddsrt_fini ();
  return ret;
}

extern void dds_fini (void)
{
  ddsrt_mutex_t * const init_mutex = ddsrt_get_singleton_mutex ();
  ddsrt_mutex_lock (init_mutex);
  assert (dds_global.m_init_count > 0);
  if (--dds_global.m_init_count == 0)
  {
    dds_handle_server_fini ();
    downgrade_main_thread ();
    thread_states_fini ();
    ddsi_iid_fini ();
    ddsrt_mutex_destroy (&dds_global.m_mutex);
  }
  ddsrt_mutex_unlock (init_mutex);
  ddsrt_fini ();
}
