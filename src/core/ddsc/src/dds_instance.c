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
#include <assert.h>
#include <string.h>

#include "dds/dds.h"
#include "dds__entity.h"
#include "dds__write.h"
#include "dds__writer.h"
#include "dds/ddsc/dds_rhc.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_globals.h"

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

static struct ddsi_tkmap_instance *dds_instance_find (const dds_topic *topic, const void *data, const bool create)
{
  struct ddsi_serdata *sd = ddsi_serdata_from_sample (topic->m_stopic, SDK_KEY, data);
  struct ddsi_tkmap_instance *inst = ddsi_tkmap_find (topic->m_entity.m_domain->gv.m_tkmap, sd, create);
  ddsi_serdata_unref (sd);
  return inst;
}

static void dds_instance_remove (struct dds_domain *dom, const dds_topic *topic, const void *data, dds_instance_handle_t handle)
{
  struct ddsi_tkmap_instance *inst;
  if (handle != DDS_HANDLE_NIL)
    inst = ddsi_tkmap_find_by_id (dom->gv.m_tkmap, handle);
  else
  {
    assert (data);
    inst = dds_instance_find (topic, data, false);
  }
  if (inst)
  {
    ddsi_tkmap_instance_unref (dom->gv.m_tkmap, inst);
  }
}

dds_return_t dds_register_instance (dds_entity_t writer, dds_instance_handle_t *handle, const void *data)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_writer *wr;
  dds_return_t ret;

  if (data == NULL || handle == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
  struct ddsi_tkmap_instance * const inst = dds_instance_find (wr->m_topic, data, true);
  if (inst == NULL)
    ret = DDS_RETCODE_ERROR;
  else
  {
    *handle = inst->m_iid;
    ret = DDS_RETCODE_OK;
  }
  thread_state_asleep (ts1);
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
  struct thread_state1 * const ts1 = lookup_thread_state ();
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

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
  if (autodispose)
  {
    dds_instance_remove (wr->m_entity.m_domain, wr->m_topic, data, DDS_HANDLE_NIL);
    action |= DDS_WR_DISPOSE_BIT;
  }
  ret = dds_write_impl (wr, data, timestamp, action);
  thread_state_asleep (ts1);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_unregister_instance_ih_ts (dds_entity_t writer, dds_instance_handle_t handle, dds_time_t timestamp)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret = DDS_RETCODE_OK;
  bool autodispose = true;
  dds_write_action action = DDS_WR_ACTION_UNREGISTER;
  dds_writer *wr;
  struct ddsi_tkmap_instance *tk;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  if (wr->m_entity.m_qos)
    (void) dds_qget_writer_data_lifecycle (wr->m_entity.m_qos, &autodispose);

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
  if (autodispose)
  {
    dds_instance_remove (wr->m_entity.m_domain, wr->m_topic, NULL, handle);
    action |= DDS_WR_DISPOSE_BIT;
  }
  if ((tk = ddsi_tkmap_find_by_id (wr->m_entity.m_domain->gv.m_tkmap, handle)) == NULL)
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    struct ddsi_sertopic *tp = wr->m_topic->m_stopic;
    void *sample = ddsi_sertopic_alloc_sample (tp);
    ddsi_serdata_topicless_to_sample (tp, tk->m_sample, sample, NULL, NULL);
    ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
    ret = dds_write_impl (wr, sample, timestamp, action);
    ddsi_sertopic_free_sample (tp, sample, DDS_FREE_ALL);
  }
  thread_state_asleep (ts1);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_writedispose_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret;
  dds_writer *wr;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
  if ((ret = dds_write_impl (wr, data, timestamp, DDS_WR_ACTION_WRITE_DISPOSE)) == DDS_RETCODE_OK)
    dds_instance_remove (wr->m_entity.m_domain, wr->m_topic, data, DDS_HANDLE_NIL);
  thread_state_asleep (ts1);
  dds_writer_unlock (wr);
  return ret;
}

static dds_return_t dds_dispose_impl (dds_writer *wr, const void *data, dds_instance_handle_t handle, dds_time_t timestamp) ddsrt_nonnull_all;

static dds_return_t dds_dispose_impl (dds_writer *wr, const void *data, dds_instance_handle_t handle, dds_time_t timestamp)
{
  dds_return_t ret;
  assert (thread_is_awake ());
  if ((ret = dds_write_impl (wr, data, timestamp, DDS_WR_ACTION_DISPOSE)) == DDS_RETCODE_OK)
    dds_instance_remove (wr->m_entity.m_domain, wr->m_topic, data, handle);
  return ret;
}

dds_return_t dds_dispose_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret;
  dds_writer *wr;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
  ret = dds_dispose_impl (wr, data, DDS_HANDLE_NIL, timestamp);
  thread_state_asleep (ts1);
  dds_writer_unlock(wr);
  return ret;
}

dds_return_t dds_dispose_ih_ts (dds_entity_t writer, dds_instance_handle_t handle, dds_time_t timestamp)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_return_t ret;
  dds_writer *wr;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  struct ddsi_tkmap_instance *tk;
  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
  if ((tk = ddsi_tkmap_find_by_id (wr->m_entity.m_domain->gv.m_tkmap, handle)) == NULL)
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  else
  {
    struct ddsi_sertopic *tp = wr->m_topic->m_stopic;
    void *sample = ddsi_sertopic_alloc_sample (tp);
    ddsi_serdata_topicless_to_sample (tp, tk->m_sample, sample, NULL, NULL);
    ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
    ret = dds_dispose_impl (wr, sample, handle, timestamp);
    ddsi_sertopic_free_sample (tp, sample, DDS_FREE_ALL);
  }
  thread_state_asleep (ts1);
  dds_writer_unlock (wr);
  return ret;
}

dds_instance_handle_t dds_lookup_instance (dds_entity_t entity, const void *data)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_instance_handle_t ih = DDS_HANDLE_NIL;
  const dds_topic *topic;
  struct ddsi_serdata *sd;
  dds_entity *w_or_r;

  if (data == NULL)
    return DDS_HANDLE_NIL;

  if (dds_entity_lock (entity, DDS_KIND_DONTCARE, &w_or_r) < 0)
    return DDS_HANDLE_NIL;
  switch (dds_entity_kind (w_or_r))
  {
    case DDS_KIND_WRITER:
      topic = ((dds_writer *) w_or_r)->m_topic;
      break;
    case DDS_KIND_READER:
      topic = ((dds_reader *) w_or_r)->m_topic;
      break;
    default:
      dds_entity_unlock (w_or_r);
      return DDS_HANDLE_NIL;
  }

  thread_state_awake (ts1, &w_or_r->m_domain->gv);
  sd = ddsi_serdata_from_sample (topic->m_stopic, SDK_KEY, data);
  ih = ddsi_tkmap_lookup (w_or_r->m_domain->gv.m_tkmap, sd);
  ddsi_serdata_unref (sd);
  thread_state_asleep (ts1);
  dds_entity_unlock (w_or_r);
  return ih;
}

dds_instance_handle_t dds_instance_lookup (dds_entity_t entity, const void *data)
{
  return dds_lookup_instance (entity, data);
}

dds_return_t dds_instance_get_key (dds_entity_t entity, dds_instance_handle_t ih, void *data)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
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

  thread_state_awake (ts1, &e->m_domain->gv);
  if ((tk = ddsi_tkmap_find_by_id (e->m_domain->gv.m_tkmap, ih)) == NULL)
    ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    ddsi_sertopic_zero_sample (topic->m_stopic, data);
    ddsi_serdata_topicless_to_sample (topic->m_stopic, tk->m_sample, data, NULL, NULL);
    ddsi_tkmap_instance_unref (e->m_domain->gv.m_tkmap, tk);
    ret = DDS_RETCODE_OK;
  }
  thread_state_asleep (ts1);
  dds_entity_unlock (e);
  return ret;
}
