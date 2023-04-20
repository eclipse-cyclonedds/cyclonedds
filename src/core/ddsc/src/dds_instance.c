// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>

#include "dds/dds.h"
#include "dds__entity.h"
#include "dds__write.h"
#include "dds__writer.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_domaingv.h"

dds_return_t dds_writedispose (dds_entity_t writer, const void *data)
{
  return dds_writedispose_ts (writer, data, dds_time ());
}

dds_return_t dds_dispose (dds_entity_t writer, const void *data)
{
  return dds_dispose_ts (writer, data, dds_time ());
}

dds_return_t dds_dispose_ih (dds_entity_t writer, dds_instance_handle_t handle)
{
  return dds_dispose_ih_ts (writer, handle, dds_time ());
}

static struct ddsi_tkmap_instance *dds_instance_find (const dds_writer *writer, const void *data, const bool create)
{
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (writer->m_wr->type, SDK_KEY, data);
  if (sd == NULL)
    return NULL;
  struct ddsi_tkmap_instance *inst = ddsi_tkmap_find (writer->m_entity.m_domain->gv.m_tkmap, sd, create);
  ddsi_serdata_unref (sd);
  return inst;
}

static void dds_instance_remove (struct dds_domain *dom, const dds_writer *writer, const void *data, dds_instance_handle_t handle)
{
  struct ddsi_tkmap_instance *inst;
  if (handle != DDS_HANDLE_NIL)
    inst = ddsi_tkmap_find_by_id (dom->gv.m_tkmap, handle);
  else
  {
    assert (data);
    inst = dds_instance_find (writer, data, false);
  }
  if (inst)
  {
    ddsi_tkmap_instance_unref (dom->gv.m_tkmap, inst);
  }
}

dds_return_t dds_register_instance (dds_entity_t writer, dds_instance_handle_t *handle, const void *data)
{
  dds_writer *wr;
  dds_return_t ret;

  if (data == NULL || handle == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  struct ddsi_tkmap_instance * const inst = dds_instance_find (wr, data, true);
  if (inst == NULL)
    ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    *handle = inst->m_iid;
    ret = DDS_RETCODE_OK;
  }
  ddsi_thread_state_asleep (thrst);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_unregister_instance (dds_entity_t writer, const void *data)
{
  return dds_unregister_instance_ts (writer, data, dds_time ());
}

dds_return_t dds_unregister_instance_ih (dds_entity_t writer, dds_instance_handle_t handle)
{
  return dds_unregister_instance_ih_ts (writer, handle, dds_time ());
}

dds_return_t dds_unregister_instance_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  dds_return_t ret;
  bool autodispose = true;
  dds_write_action action = DDS_WR_ACTION_UNREGISTER;
  dds_writer *wr;

  if (data == NULL || timestamp < 0)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  if (wr->m_entity.m_qos)
    (void) dds_qget_writer_data_lifecycle (wr->m_entity.m_qos, &autodispose);

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  if (autodispose)
  {
    dds_instance_remove (wr->m_entity.m_domain, wr, data, DDS_HANDLE_NIL);
    action |= DDS_WR_DISPOSE_BIT;
  }
  ret = dds_write_impl (wr, data, timestamp, action);
  ddsi_thread_state_asleep (thrst);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_unregister_instance_ih_ts (dds_entity_t writer, dds_instance_handle_t handle, dds_time_t timestamp)
{
  dds_return_t ret = DDS_RETCODE_OK;
  bool autodispose = true;
  dds_write_action action = DDS_WR_ACTION_UNREGISTER;
  dds_writer *wr;
  struct ddsi_tkmap_instance *tk;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  if (wr->m_entity.m_qos)
    (void) dds_qget_writer_data_lifecycle (wr->m_entity.m_qos, &autodispose);

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  if (autodispose)
  {
    dds_instance_remove (wr->m_entity.m_domain, wr, NULL, handle);
    action |= DDS_WR_DISPOSE_BIT;
  }
  if ((tk = ddsi_tkmap_find_by_id (wr->m_entity.m_domain->gv.m_tkmap, handle)) == NULL)
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    struct ddsi_sertype *tp = wr->m_topic->m_stype;
    void *sample = ddsi_sertype_alloc_sample (tp);
    ddsi_serdata_untyped_to_sample (tp, tk->m_sample, sample, NULL, NULL);
    ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
    ret = dds_write_impl (wr, sample, timestamp, action);
    ddsi_sertype_free_sample (tp, sample, DDS_FREE_ALL);
  }
  ddsi_thread_state_asleep (thrst);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_writedispose_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  dds_return_t ret;
  dds_writer *wr;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  if ((ret = dds_write_impl (wr, data, timestamp, DDS_WR_ACTION_WRITE_DISPOSE)) == DDS_RETCODE_OK)
    dds_instance_remove (wr->m_entity.m_domain, wr, data, DDS_HANDLE_NIL);
  ddsi_thread_state_asleep (thrst);
  dds_writer_unlock (wr);
  return ret;
}

static dds_return_t dds_dispose_impl (dds_writer *wr, const void *data, dds_instance_handle_t handle, dds_time_t timestamp) ddsrt_nonnull_all;

static dds_return_t dds_dispose_impl (dds_writer *wr, const void *data, dds_instance_handle_t handle, dds_time_t timestamp)
{
  dds_return_t ret;
  assert (ddsi_thread_is_awake ());
  if ((ret = dds_write_impl (wr, data, timestamp, DDS_WR_ACTION_DISPOSE)) == DDS_RETCODE_OK)
    dds_instance_remove (wr->m_entity.m_domain, wr, data, handle);
  return ret;
}

dds_return_t dds_dispose_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  dds_return_t ret;
  dds_writer *wr;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  ret = dds_dispose_impl (wr, data, DDS_HANDLE_NIL, timestamp);
  ddsi_thread_state_asleep (thrst);
  dds_writer_unlock(wr);
  return ret;
}

dds_return_t dds_dispose_ih_ts (dds_entity_t writer, dds_instance_handle_t handle, dds_time_t timestamp)
{
  dds_return_t ret;
  dds_writer *wr;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  struct ddsi_tkmap_instance *tk;
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  if ((tk = ddsi_tkmap_find_by_id (wr->m_entity.m_domain->gv.m_tkmap, handle)) == NULL)
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    const struct ddsi_sertype *tp = wr->m_wr->type;
    void *sample = ddsi_sertype_alloc_sample (tp);
    ddsi_serdata_untyped_to_sample (tp, tk->m_sample, sample, NULL, NULL);
    ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
    ret = dds_dispose_impl (wr, sample, handle, timestamp);
    ddsi_sertype_free_sample (tp, sample, DDS_FREE_ALL);
  }
  ddsi_thread_state_asleep (thrst);
  dds_writer_unlock (wr);
  return ret;
}

dds_instance_handle_t dds_lookup_instance (dds_entity_t entity, const void *data)
{
  const struct ddsi_sertype *sertype;
  struct ddsi_serdata *sd;
  dds_entity *w_or_r;

  if (data == NULL)
    return DDS_HANDLE_NIL;

  if (dds_entity_lock (entity, DDS_KIND_DONTCARE, &w_or_r) < 0)
    return DDS_HANDLE_NIL;
  switch (dds_entity_kind (w_or_r))
  {
    case DDS_KIND_WRITER:
      sertype = ((dds_writer *) w_or_r)->m_wr->type;
      break;
    case DDS_KIND_READER:
      // FIXME: used for serdata_from_sample, so maybe this should take the derived sertype for a specific data-representation?
      sertype = ((dds_reader *) w_or_r)->m_topic->m_stype;
      break;
    default:
      dds_entity_unlock (w_or_r);
      return DDS_HANDLE_NIL;
  }

  dds_instance_handle_t ih;
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &w_or_r->m_domain->gv);
  if ((sd = ddsi_serdata_from_sample (sertype, SDK_KEY, data)) == NULL)
    ih = DDS_HANDLE_NIL;
  else
  {
    ih = ddsi_tkmap_lookup (w_or_r->m_domain->gv.m_tkmap, sd);
    ddsi_serdata_unref (sd);
  }
  ddsi_thread_state_asleep (thrst);
  dds_entity_unlock (w_or_r);
  return ih;
}

dds_return_t dds_instance_get_key (dds_entity_t entity, dds_instance_handle_t ih, void *data)
{
  dds_return_t ret;
  const dds_topic *topic;
  struct ddsi_tkmap_instance *tk;
  dds_entity *e;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_entity_lock (entity, DDS_KIND_DONTCARE, &e)) < 0)
    return ret;
  switch (dds_entity_kind (e))
  {
    case DDS_KIND_WRITER:
      topic = ((dds_writer *) e)->m_topic;
      break;
    case DDS_KIND_READER:
      topic = ((dds_reader *) e)->m_topic;
      break;
    case DDS_KIND_COND_READ:
    case DDS_KIND_COND_QUERY:
      topic = ((dds_reader *) e->m_parent)->m_topic;
      break;
    default:
      dds_entity_unlock (e);
      return DDS_RETCODE_ILLEGAL_OPERATION;
  }

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_thread_state_awake (thrst, &e->m_domain->gv);
  if ((tk = ddsi_tkmap_find_by_id (e->m_domain->gv.m_tkmap, ih)) == NULL)
    ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    /* Use sertype from topic, as the zero_sample and untyped_to_sample functions
       are identical for the derived sertype that is stored in the endpoint. */
    ddsi_sertype_zero_sample (topic->m_stype, data);
    ddsi_serdata_untyped_to_sample (topic->m_stype, tk->m_sample, data, NULL, NULL);
    ddsi_tkmap_instance_unref (e->m_domain->gv.m_tkmap, tk);
    ret = DDS_RETCODE_OK;
  }
  ddsi_thread_state_asleep (thrst);
  dds_entity_unlock (e);
  return ret;
}
