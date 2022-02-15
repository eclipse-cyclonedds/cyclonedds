/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_bswap.h"
#include "dds__writer.h"
#include "dds__reader.h"
#include "dds__topic.h"

dds_return_t dds_get_matched_subscriptions (dds_entity_t writer, dds_instance_handle_t *rds, size_t nrds)
{
  dds_writer *wr;
  dds_return_t rc;
  if ((rds != NULL && (nrds == 0 || nrds > INT32_MAX)) || (rds == NULL && nrds != 0))
    return DDS_RETCODE_BAD_PARAMETER;
  if ((rc = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    const struct entity_index *gh = wr->m_entity.m_domain->gv.entity_index;
    size_t nrds_act = 0;
    ddsrt_avl_iter_t it;
    /* FIXME: this ought not be so tightly coupled to the lower layer */
    thread_state_awake (lookup_thread_state (), &wr->m_entity.m_domain->gv);
    ddsrt_mutex_lock (&wr->m_wr->e.lock);
    for (const struct wr_prd_match *m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->m_wr->readers, &it);
         m != NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct proxy_reader *prd;
      if ((prd = entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) != NULL)
      {
        if (nrds_act < nrds)
          rds[nrds_act] = prd->e.iid;
        nrds_act++;
      }
    }
    for (const struct wr_rd_match *m = ddsrt_avl_iter_first (&wr_local_readers_treedef, &wr->m_wr->local_readers, &it);
         m != NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct reader *rd;
      if ((rd = entidx_lookup_reader_guid (gh, &m->rd_guid)) != NULL)
      {
        if (nrds_act < nrds)
          rds[nrds_act] = rd->e.iid;
        nrds_act++;
      }
    }
    ddsrt_mutex_unlock (&wr->m_wr->e.lock);
    thread_state_asleep (lookup_thread_state ());
    dds_writer_unlock (wr);
    /* FIXME: is it really true that there can not be more than INT32_MAX matching readers?
       (in practice it'll come to a halt long before that) */
    assert (nrds_act <= INT32_MAX);
    return (dds_return_t) nrds_act;
  }
}

dds_return_t dds_get_matched_publications (dds_entity_t reader, dds_instance_handle_t *wrs, size_t nwrs)
{
  dds_reader *rd;
  dds_return_t rc;
  if ((wrs != NULL && (nwrs == 0 || nwrs > INT32_MAX)) || (wrs == NULL && nwrs != 0))
    return DDS_RETCODE_BAD_PARAMETER;
  if ((rc = dds_reader_lock (reader, &rd)) != DDS_RETCODE_OK)
    return rc;
  else
  {
    const struct entity_index *gh = rd->m_entity.m_domain->gv.entity_index;
    size_t nwrs_act = 0;
    ddsrt_avl_iter_t it;
    /* FIXME: this ought not be so tightly coupled to the lower layer */
    thread_state_awake (lookup_thread_state (), &rd->m_entity.m_domain->gv);
    ddsrt_mutex_lock (&rd->m_rd->e.lock);
    for (const struct rd_pwr_match *m = ddsrt_avl_iter_first (&rd_writers_treedef, &rd->m_rd->writers, &it);
         m != NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct proxy_writer *pwr;
      if ((pwr = entidx_lookup_proxy_writer_guid (gh, &m->pwr_guid)) != NULL)
      {
        if (nwrs_act < nwrs)
          wrs[nwrs_act] = pwr->e.iid;
        nwrs_act++;
      }
    }
    for (const struct rd_wr_match *m = ddsrt_avl_iter_first (&rd_local_writers_treedef, &rd->m_rd->local_writers, &it);
         m != NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct writer *wr;
      if ((wr = entidx_lookup_writer_guid (gh, &m->wr_guid)) != NULL)
      {
        if (nwrs_act < nwrs)
          wrs[nwrs_act] = wr->e.iid;
        nwrs_act++;
      }
    }
    ddsrt_mutex_unlock (&rd->m_rd->e.lock);
    thread_state_asleep (lookup_thread_state ());
    dds_reader_unlock (rd);
    /* FIXME: is it really true that there can not be more than INT32_MAX matching readers?
     (in practice it'll come to a halt long before that) */
    assert (nwrs_act <= INT32_MAX);
    return (dds_return_t) nwrs_act;
  }
}

static dds_builtintopic_endpoint_t *make_builtintopic_endpoint (const ddsi_guid_t *guid, const ddsi_guid_t *ppguid, dds_instance_handle_t ppiid, const dds_qos_t *qos)
{
  dds_builtintopic_endpoint_t *ep;
  ddsi_guid_t tmp;
  ep = dds_alloc (sizeof (*ep));
  tmp = nn_hton_guid (*guid);
  memcpy (&ep->key, &tmp, sizeof (ep->key));
  ep->participant_instance_handle = ppiid;
  tmp = nn_hton_guid (*ppguid);
  memcpy (&ep->participant_key, &tmp, sizeof (ep->participant_key));
  ep->qos = dds_create_qos ();
  ddsi_xqos_mergein_missing (ep->qos, qos, ~(QP_TOPIC_NAME | QP_TYPE_NAME));
  ep->topic_name = dds_string_dup (qos->topic_name);
  ep->type_name = dds_string_dup (qos->type_name);
  return ep;
}

dds_builtintopic_endpoint_t *dds_get_matched_subscription_data (dds_entity_t writer, dds_instance_handle_t ih)
{
  dds_writer *wr;
  if (dds_writer_lock (writer, &wr))
    return NULL;
  else
  {
    const struct entity_index *gh = wr->m_entity.m_domain->gv.entity_index;
    dds_builtintopic_endpoint_t *ret = NULL;
    ddsrt_avl_iter_t it;
    /* FIXME: this ought not be so tightly coupled to the lower layer, and not be so inefficient besides */
    thread_state_awake (lookup_thread_state (), &wr->m_entity.m_domain->gv);
    ddsrt_mutex_lock (&wr->m_wr->e.lock);
    for (const struct wr_prd_match *m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->m_wr->readers, &it);
         m != NULL && ret == NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct proxy_reader *prd;
      if ((prd = entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) != NULL)
      {
        if (prd->e.iid == ih)
          ret = make_builtintopic_endpoint (&prd->e.guid, &prd->c.proxypp->e.guid, prd->c.proxypp->e.iid, prd->c.xqos);
      }
    }
    for (const struct wr_rd_match *m = ddsrt_avl_iter_first (&wr_local_readers_treedef, &wr->m_wr->local_readers, &it);
         m != NULL && ret == NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct reader *rd;
      if ((rd = entidx_lookup_reader_guid (gh, &m->rd_guid)) != NULL)
      {
        if (rd->e.iid == ih)
          ret = make_builtintopic_endpoint (&rd->e.guid, &rd->c.pp->e.guid, rd->c.pp->e.iid, rd->xqos);
      }
    }

    ddsrt_mutex_unlock (&wr->m_wr->e.lock);
    thread_state_asleep (lookup_thread_state ());
    dds_writer_unlock (wr);
    return ret;
  }
}

dds_builtintopic_endpoint_t *dds_get_matched_publication_data (dds_entity_t reader, dds_instance_handle_t ih)
{
  dds_reader *rd;
  if (dds_reader_lock (reader, &rd))
    return NULL;
  else
  {
    const struct entity_index *gh = rd->m_entity.m_domain->gv.entity_index;
    dds_builtintopic_endpoint_t *ret = NULL;
    ddsrt_avl_iter_t it;
    /* FIXME: this ought not be so tightly coupled to the lower layer, and not be so inefficient besides */
    thread_state_awake (lookup_thread_state (), &rd->m_entity.m_domain->gv);
    ddsrt_mutex_lock (&rd->m_rd->e.lock);
    for (const struct rd_pwr_match *m = ddsrt_avl_iter_first (&rd_writers_treedef, &rd->m_rd->writers, &it);
         m != NULL && ret == NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct proxy_writer *pwr;
      if ((pwr = entidx_lookup_proxy_writer_guid (gh, &m->pwr_guid)) != NULL)
      {
        if (pwr->e.iid == ih)
          ret = make_builtintopic_endpoint (&pwr->e.guid, &pwr->c.proxypp->e.guid, pwr->c.proxypp->e.iid, pwr->c.xqos);
      }
    }
    for (const struct rd_wr_match *m = ddsrt_avl_iter_first (&rd_local_writers_treedef, &rd->m_rd->local_writers, &it);
         m != NULL && ret == NULL;
         m = ddsrt_avl_iter_next (&it))
    {
      struct writer *wr;
      if ((wr = entidx_lookup_writer_guid (gh, &m->wr_guid)) != NULL)
      {
        if (wr->e.iid == ih)
          ret = make_builtintopic_endpoint (&wr->e.guid, &wr->c.pp->e.guid, wr->c.pp->e.iid, wr->xqos);
      }
    }
    ddsrt_mutex_unlock (&rd->m_rd->e.lock);
    thread_state_asleep (lookup_thread_state ());
    dds_reader_unlock (rd);
    return ret;
  }
}

#ifdef DDS_HAS_TYPE_DISCOVERY
dds_return_t dds_builtintopic_get_endpoint_typeid (dds_builtintopic_endpoint_t * builtintopic_endpoint, dds_typeid_kind_t kind, dds_typeid_t **type_id)
{
  if (builtintopic_endpoint == NULL || (kind != DDS_TYPEID_MINIMAL && kind != DDS_TYPEID_COMPLETE))
    return DDS_RETCODE_BAD_PARAMETER;

  if (builtintopic_endpoint->qos && builtintopic_endpoint->qos->present & QP_TYPE_INFORMATION)
  {
    ddsi_typeinfo_t *type_info = builtintopic_endpoint->qos->type_information;
    *type_id = (dds_typeid_t *) ddsi_typeid_dup (kind == DDS_TYPEID_MINIMAL ? ddsi_typeinfo_minimal_typeid (type_info) : ddsi_typeinfo_complete_typeid (type_info));
  }
  else
    *type_id = NULL;
  return DDS_RETCODE_OK;
}
#endif

void dds_builtintopic_free_endpoint (dds_builtintopic_endpoint_t * builtintopic_endpoint)
{
  dds_delete_qos (builtintopic_endpoint->qos);
  ddsrt_free (builtintopic_endpoint->topic_name);
  ddsrt_free (builtintopic_endpoint->type_name);
  ddsrt_free (builtintopic_endpoint);
}

void dds_builtintopic_free_topic (dds_builtintopic_topic_t * builtintopic_topic)
{
  dds_delete_qos (builtintopic_topic->qos);
  ddsrt_free (builtintopic_topic->topic_name);
  ddsrt_free (builtintopic_topic->type_name);
  ddsrt_free (builtintopic_topic);
}

void dds_builtintopic_free_participant (dds_builtintopic_participant_t * builtintopic_participant)
{
  dds_delete_qos (builtintopic_participant->qos);
  ddsrt_free (builtintopic_participant);
}
