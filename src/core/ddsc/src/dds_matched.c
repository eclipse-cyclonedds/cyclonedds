// Copyright(c) 2019 to 2022 ZettaScale Technology and others
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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint_match.h"
#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_thread.h"
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
    rc = ddsi_writer_get_matched_subscriptions (wr->m_wr, rds, nrds);
    dds_writer_unlock (wr);
    return rc;
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
    rc = ddsi_reader_get_matched_publications (rd->m_rd, wrs, nwrs);
    dds_reader_unlock (rd);
    return rc;
  }
}

static dds_builtintopic_endpoint_t *make_builtintopic_endpoint (const ddsi_guid_t *guid, const ddsi_guid_t *ppguid, dds_instance_handle_t ppiid, const dds_qos_t *qos)
{
  dds_builtintopic_endpoint_t *ep;
  ddsi_guid_t tmp;
  ep = dds_alloc (sizeof (*ep));
  tmp = ddsi_hton_guid (*guid);
  memcpy (&ep->key, &tmp, sizeof (ep->key));
  ep->participant_instance_handle = ppiid;
  tmp = ddsi_hton_guid (*ppguid);
  memcpy (&ep->participant_key, &tmp, sizeof (ep->participant_key));
  ep->qos = dds_create_qos ();
  ddsi_xqos_mergein_missing (ep->qos, qos, ~(DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME));
  ep->topic_name = dds_string_dup (qos->topic_name);
  ep->type_name = dds_string_dup (qos->type_name);
  return ep;
}

dds_builtintopic_endpoint_t *dds_get_matched_subscription_data (dds_entity_t writer, dds_instance_handle_t ih)
{
  dds_writer *wr;
  if (dds_writer_lock (writer, &wr))
    return NULL;

  dds_builtintopic_endpoint_t *ret = NULL;
  struct ddsi_entity_common *rdc;
  struct dds_qos *rdqos;
  struct ddsi_entity_common *ppc;

  // thread must be "awake" while pointers to DDSI entities are being used
  struct ddsi_domaingv * const gv = &wr->m_entity.m_domain->gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  if (ddsi_writer_find_matched_reader (wr->m_wr, ih, &rdc, &rdqos, &ppc))
    ret = make_builtintopic_endpoint (&rdc->guid, &ppc->guid, ppc->iid, rdqos);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  dds_writer_unlock (wr);
  return ret;
}

dds_builtintopic_endpoint_t *dds_get_matched_publication_data (dds_entity_t reader, dds_instance_handle_t ih)
{
  dds_reader *rd;
  if (dds_reader_lock (reader, &rd))
    return NULL;

  dds_builtintopic_endpoint_t *ret = NULL;
  struct ddsi_entity_common *wrc;
  struct dds_qos *wrqos;
  struct ddsi_entity_common *ppc;

  // thread must be "awake" while pointers to DDSI entities are being used
  struct ddsi_domaingv * const gv = &rd->m_entity.m_domain->gv;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  if (ddsi_reader_find_matched_writer (rd->m_rd, ih, &wrc, &wrqos, &ppc))
    ret = make_builtintopic_endpoint (&wrc->guid, &ppc->guid, ppc->iid, wrqos);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  dds_reader_unlock (rd);
  return ret;
}

#ifdef DDS_HAS_TYPE_DISCOVERY
dds_return_t dds_builtintopic_get_endpoint_type_info (dds_builtintopic_endpoint_t * builtintopic_endpoint, const dds_typeinfo_t ** type_info)
{
  if (builtintopic_endpoint == NULL || type_info == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if (builtintopic_endpoint->qos && builtintopic_endpoint->qos->present & DDSI_QP_TYPE_INFORMATION)
    *type_info = builtintopic_endpoint->qos->type_information;
  else
    *type_info = NULL;
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
