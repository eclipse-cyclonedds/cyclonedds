// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/heap.h"
#include "dds__init.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds__domain.h"
#include "dds__builtin.h"
#include "dds__whc_builtintopic.h"
#include "dds__entity.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_gc.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/version.h"

static void dds_close (struct dds_entity *e);
static dds_return_t dds_fini (struct dds_entity *e);

const struct dds_entity_deriver dds_entity_deriver_cyclonedds = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_close,
  .delete = dds_fini,
  .set_qos = dds_entity_deriver_dummy_set_qos,
  .validate_status = dds_entity_deriver_dummy_validate_status,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
};

dds_cyclonedds_entity dds_global;

#define CDDS_STATE_ZERO 0u
#define CDDS_STATE_STARTING 1u
#define CDDS_STATE_READY 2u
#define CDDS_STATE_STOPPING 3u
static ddsrt_atomic_uint32_t dds_state = DDSRT_ATOMIC_UINT32_INIT (CDDS_STATE_ZERO);

static void common_cleanup (void)
{
  if (ddsi_thread_states_fini ())
    dds_handle_server_fini ();

  ddsi_iid_fini ();
  ddsrt_cond_destroy (&dds_global.m_cond);
  ddsrt_mutex_destroy (&dds_global.m_mutex);

  ddsrt_atomic_st32 (&dds_state, CDDS_STATE_ZERO);
  ddsrt_cond_broadcast (ddsrt_get_singleton_cond ());
}

static bool cyclonedds_entity_ready (uint32_t s)
{
  assert (s != CDDS_STATE_ZERO);
  if (s == CDDS_STATE_STARTING || s == CDDS_STATE_STOPPING)
    return false;
  else
  {
    struct dds_handle_link *x;
    return dds_handle_pin_and_ref_with_origin (DDS_CYCLONEDDS_HANDLE, false, &x) == DDS_RETCODE_OK;
  }
}

dds_return_t dds_init (void)
{
  dds_return_t ret;

  ddsrt_init ();
  ddsrt_mutex_t * const init_mutex = ddsrt_get_singleton_mutex ();
  ddsrt_cond_t * const init_cond = ddsrt_get_singleton_cond ();

  ddsrt_mutex_lock (init_mutex);
  uint32_t s = ddsrt_atomic_ld32 (&dds_state);
  while (s != CDDS_STATE_ZERO && !cyclonedds_entity_ready (s))
  {
    ddsrt_cond_wait (init_cond, init_mutex);
    s = ddsrt_atomic_ld32 (&dds_state);
  }
  switch (s)
  {
    case CDDS_STATE_READY:
      assert (dds_global.m_entity.m_hdllink.hdl == DDS_CYCLONEDDS_HANDLE);
      ddsrt_mutex_unlock (init_mutex);
      return DDS_RETCODE_OK;
    case CDDS_STATE_ZERO:
      ddsrt_atomic_st32 (&dds_state, CDDS_STATE_STARTING);
      break;
    default:
      ddsrt_mutex_unlock (init_mutex);
      ddsrt_fini ();
      return DDS_RETCODE_ERROR;
  }

  ddsrt_mutex_init (&dds_global.m_mutex);
  ddsrt_cond_init (&dds_global.m_cond);
  ddsi_iid_init ();
  ddsi_thread_states_init ();

  if (dds_handle_server_init () != DDS_RETCODE_OK)
  {
    DDS_ERROR ("Failed to initialize internal handle server\n");
    ret = DDS_RETCODE_ERROR;
    goto fail_handleserver;
  }

  dds_entity_init (&dds_global.m_entity, NULL, DDS_KIND_CYCLONEDDS, true, true, NULL, NULL, 0);
  dds_global.m_entity.m_iid = ddsi_iid_gen ();
  dds_handle_repin (&dds_global.m_entity.m_hdllink);
  dds_entity_add_ref_locked (&dds_global.m_entity);
  dds_entity_init_complete (&dds_global.m_entity);
  ddsrt_atomic_st32 (&dds_state, CDDS_STATE_READY);
  ddsrt_mutex_unlock (init_mutex);
  return DDS_RETCODE_OK;

fail_handleserver:
  common_cleanup ();
  ddsrt_mutex_unlock (init_mutex);
  ddsrt_fini ();
  return ret;
}

static void dds_close (struct dds_entity *e)
{
  (void) e;
  assert (ddsrt_atomic_ld32 (&dds_state) == CDDS_STATE_READY);
  ddsrt_atomic_st32 (&dds_state, CDDS_STATE_STOPPING);
}

static dds_return_t dds_fini (struct dds_entity *e)
{
  (void) e;
  ddsrt_mutex_t * const init_mutex = ddsrt_get_singleton_mutex ();
  /* If there are multiple domains shutting down simultaneously, the one "deleting" the top-level
     entity (and thus arriving here) may have overtaken another thread that is still in the process
     of deleting its domain object.  For most entities such races are not an issue, but here we tear
     down the run-time, so here we must wait until everyone else is out. */
  ddsrt_mutex_lock (&dds_global.m_mutex);
  while (!ddsrt_avl_is_empty (&dds_global.m_domains))
    ddsrt_cond_wait (&dds_global.m_cond, &dds_global.m_mutex);
  ddsrt_mutex_unlock (&dds_global.m_mutex);

  ddsrt_mutex_lock (init_mutex);
  assert (ddsrt_atomic_ld32 (&dds_state) == CDDS_STATE_STOPPING);
  dds_entity_final_deinit_before_free (e);
  common_cleanup ();
  ddsrt_mutex_unlock (init_mutex);
  ddsrt_fini ();
  return DDS_RETCODE_NO_DATA;
}
