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
#include "dds__listener.h"
#include "dds__participant.h"
#include "dds__subscriber.h"
#include "dds__qos.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsrt/heap.h"
#include "dds/version.h"

DECL_ENTITY_LOCK_UNLOCK (dds_subscriber)

#define DDS_SUBSCRIBER_STATUS_MASK                               \
                        (DDS_DATA_ON_READERS_STATUS)

static dds_return_t dds_subscriber_qos_set (dds_entity *e, const dds_qos_t *qos, bool enabled)
{
  /* note: e->m_qos is still the old one to allow for failure here */
  (void) e; (void) qos; (void) enabled;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_subscriber_status_validate (uint32_t mask)
{
  return (mask & ~DDS_SUBSCRIBER_STATUS_MASK) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK;
}

const struct dds_entity_deriver dds_entity_deriver_subscriber = {
  .interrupt = dds_entity_deriver_dummy_interrupt,
  .close = dds_entity_deriver_dummy_close,
  .delete = dds_entity_deriver_dummy_delete,
  .set_qos = dds_subscriber_qos_set,
  .validate_status = dds_subscriber_status_validate,
  .create_statistics = dds_entity_deriver_dummy_create_statistics,
  .refresh_statistics = dds_entity_deriver_dummy_refresh_statistics
};

dds_entity_t dds__create_subscriber_l (dds_participant *participant, bool implicit, const dds_qos_t *qos, const dds_listener_t *listener)
{
  /* participant entity lock must be held */
  dds_subscriber *sub;
  dds_entity_t subscriber;
  dds_return_t ret;
  dds_qos_t *new_qos;

  new_qos = dds_create_qos ();
  if (qos)
    ddsi_xqos_mergein_missing (new_qos, qos, DDS_SUBSCRIBER_QOS_MASK);
  ddsi_xqos_mergein_missing (new_qos, &ddsi_default_qos_publisher_subscriber, ~(uint64_t)0);
  dds_apply_entity_naming(new_qos, participant->m_entity.m_qos, &participant->m_entity.m_domain->gv);

  if ((ret = ddsi_xqos_valid (&participant->m_entity.m_domain->gv.logconfig, new_qos)) != DDS_RETCODE_OK)
  {
    dds_delete_qos (new_qos);
    return ret;
  }

  sub = dds_alloc (sizeof (*sub));
  subscriber = dds_entity_init (&sub->m_entity, &participant->m_entity, DDS_KIND_SUBSCRIBER, implicit, true, new_qos, listener, DDS_SUBSCRIBER_STATUS_MASK);
  sub->m_entity.m_iid = ddsi_iid_gen ();
  sub->materialize_data_on_readers = 0;
  dds_entity_register_child (&participant->m_entity, &sub->m_entity);
  dds_entity_init_complete (&sub->m_entity);
  return subscriber;
}

dds_entity_t dds_create_subscriber (dds_entity_t participant, const dds_qos_t *qos, const dds_listener_t *listener)
{
  dds_participant *par;
  dds_entity_t hdl;
  dds_return_t ret;
  if ((ret = dds_participant_lock (participant, &par)) != DDS_RETCODE_OK)
    return ret;
  hdl = dds__create_subscriber_l (par, false, qos, listener);
  dds_participant_unlock (par);
  return hdl;
}

dds_return_t dds_notify_readers (dds_entity_t subscriber)
{
  dds_subscriber *sub;
  dds_return_t ret;
  if ((ret = dds_subscriber_lock (subscriber, &sub)) != DDS_RETCODE_OK)
    return ret;
  dds_subscriber_unlock (sub);
  return DDS_RETCODE_UNSUPPORTED;
}

dds_return_t dds_subscriber_begin_coherent (dds_entity_t e)
{
  return dds_generic_unimplemented_operation (e, DDS_KIND_SUBSCRIBER);
}

dds_return_t dds_subscriber_end_coherent (dds_entity_t e)
{
  return dds_generic_unimplemented_operation (e, DDS_KIND_SUBSCRIBER);
}

bool dds_subscriber_compute_data_on_readers_locked (dds_subscriber *sub)
{
  // sub->m_entity.m_mutex must be locked
  ddsrt_avl_iter_t it;

  // Returning true when some reader has DATA_AVAILABLE set isn't the correct behaviour
  // because it doesn't reset the DATA_ON_READERS state on the first read/take on one of
  // the subscriber's readers.  It seems highly unlikely to be a problem in practice:
  //
  // - if one uses a listener, why look at the status?
  // - if one uses a waitset, it is precise because it is materialized
  //
  // so that leaves polling DATA_ON_READERS.  Also it doesn't add any functionality in
  // Cyclone at this time because the group ordering/coherency isn't implemented yet and
  // neither is get_datareaders() implemented.
  //
  // A possible way to solve this is to add a "virtual clock" to the subscriber (just an
  // integer that gets updated atomically on all relevant operations) and record in each
  // reader virtual "time stamp" at which DATA_AVAILABLE was last updated.
  for (dds_entity *rd = ddsrt_avl_iter_first (&dds_entity_children_td, &sub->m_entity.m_children, &it); rd; rd = ddsrt_avl_iter_next (&it))
  {
    const uint32_t sm = ddsrt_atomic_ld32 (&rd->m_status.m_status_and_mask);
    if (sm & DDS_DATA_AVAILABLE_STATUS)
      return true;
  }
  return false;
}

void dds_subscriber_adjust_materialize_data_on_readers (dds_subscriber *sub, bool materialization_needed)
{
  // no locks held, sub is pinned
  bool propagate = false;
  ddsrt_mutex_lock (&sub->m_entity.m_observers_lock);
  if (materialization_needed)
  {
    // FIXME: indeed no need to propagate if flag is already set?
    if (sub->materialize_data_on_readers++ == 0)
      propagate = true;
  }
  else
  {
    assert ((sub->materialize_data_on_readers & DDS_SUB_MATERIALIZE_DATA_ON_READERS_MASK) > 0);
    if (--sub->materialize_data_on_readers == 0)
    {
      sub->materialize_data_on_readers &= ~DDS_SUB_MATERIALIZE_DATA_ON_READERS_FLAG;
      propagate = true;
    }
  }
  ddsrt_mutex_unlock (&sub->m_entity.m_observers_lock);

  ddsrt_mutex_lock (&sub->m_entity.m_mutex); // needed for iterating over readers, order is m_mutex, then m_observers_lock
  if (propagate)
  {
    // propagate into readers and set DATA_ON_READERS if there is any reader with DATA_AVAILABLE set
    // no need to trigger waitsets, as this gets done prior to attaching
    dds_instance_handle_t last_iid = 0;
    dds_entity *rd;
    while ((rd = ddsrt_avl_lookup_succ (&dds_entity_children_td, &sub->m_entity.m_children, &last_iid)) != NULL)
    {
      last_iid = rd->m_iid;
      dds_entity *x;
      if (dds_entity_pin (rd->m_hdllink.hdl, &x) < 0)
        continue;
      if (x == rd) // FIXME: can this ever not be true?
      {
        ddsrt_mutex_unlock (&sub->m_entity.m_mutex);

        ddsrt_mutex_lock (&x->m_observers_lock);
        ddsrt_mutex_lock (&sub->m_entity.m_observers_lock);
        if (sub->materialize_data_on_readers)
          ddsrt_atomic_or32 (&x->m_status.m_status_and_mask, DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT);
        else
          ddsrt_atomic_and32 (&x->m_status.m_status_and_mask, ~(uint32_t)(DDS_DATA_ON_READERS_STATUS << SAM_ENABLED_SHIFT));
        ddsrt_mutex_unlock (&sub->m_entity.m_observers_lock);
        ddsrt_mutex_unlock (&x->m_observers_lock);

        ddsrt_mutex_lock (&sub->m_entity.m_mutex);
      }
      dds_entity_unpin (x);
    }
  }

  /* Set/clear DATA_ON_READERS - no point in triggering waitsets as it becomes materialized prior to
     attaching it to a waitset. */
  ddsrt_mutex_lock (&sub->m_entity.m_observers_lock);
  if (dds_subscriber_compute_data_on_readers_locked (sub))
    ddsrt_atomic_or32 (&sub->m_entity.m_status.m_status_and_mask, DDS_DATA_ON_READERS_STATUS);
  else
    dds_entity_status_reset (&sub->m_entity, DDS_DATA_ON_READERS_STATUS);
  if ((sub->materialize_data_on_readers & DDS_SUB_MATERIALIZE_DATA_ON_READERS_MASK) != 0)
    sub->materialize_data_on_readers |= DDS_SUB_MATERIALIZE_DATA_ON_READERS_FLAG;
  ddsrt_mutex_unlock (&sub->m_entity.m_observers_lock);
  ddsrt_mutex_unlock (&sub->m_entity.m_mutex);
}
