/*
 * Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include <stddef.h>

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/string.h"

#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_thread.h"

DDS_EXPORT extern inline bool builtintopic_is_visible (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid, nn_vendorid_t vendorid);
DDS_EXPORT extern inline bool builtintopic_is_builtintopic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_sertype *type);
DDS_EXPORT extern inline struct ddsi_tkmap_instance *builtintopic_get_tkmap_entry (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid);
DDS_EXPORT extern inline void builtintopic_write_endpoint (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_entity_common *e, ddsrt_wctime_t timestamp, bool alive);
DDS_EXPORT extern inline void builtintopic_write_topic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp, bool alive);

DDS_EXPORT extern inline seqno_t ddsi_writer_read_seq_xmit (const struct ddsi_writer *wr);
DDS_EXPORT extern inline void ddsi_writer_update_seq_xmit (struct ddsi_writer *wr, seqno_t nv);

int ddsi_compare_guid (const void *va, const void *vb)
{
  return memcmp (va, vb, sizeof (ddsi_guid_t));
}

bool ddsi_is_null_guid (const ddsi_guid_t *guid)
{
  return guid->prefix.u[0] == 0 && guid->prefix.u[1] == 0 && guid->prefix.u[2] == 0 && guid->entityid.u == 0;
}

ddsi_entityid_t ddsi_to_entityid (unsigned u)
{
  ddsi_entityid_t e;
  e.u = u;
  return e;
}

void ddsi_entity_common_init (struct ddsi_entity_common *e, struct ddsi_domaingv *gv, const struct ddsi_guid *guid, enum ddsi_entity_kind kind, ddsrt_wctime_t tcreate, nn_vendorid_t vendorid, bool onlylocal)
{
  e->guid = *guid;
  e->kind = kind;
  e->tupdate = tcreate;
  e->onlylocal = onlylocal;
  e->gv = gv;
  ddsrt_mutex_init (&e->lock);
  ddsrt_mutex_init (&e->qos_lock);
  if (builtintopic_is_visible (gv->builtin_topic_interface, guid, vendorid))
  {
    e->tk = builtintopic_get_tkmap_entry (gv->builtin_topic_interface, guid);
    e->iid = e->tk->m_iid;
  }
  else
  {
    e->tk = NULL;
    e->iid = ddsi_iid_gen ();
  }
}

void ddsi_entity_common_fini (struct ddsi_entity_common *e)
{
  if (e->tk)
    ddsi_tkmap_instance_unref (e->gv->m_tkmap, e->tk);
  ddsrt_mutex_destroy (&e->qos_lock);
  ddsrt_mutex_destroy (&e->lock);
}

nn_vendorid_t ddsi_get_entity_vendorid (const struct ddsi_entity_common *e)
{
  switch (e->kind)
  {
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_TOPIC:
    case DDSI_EK_READER:
    case DDSI_EK_WRITER:
      return NN_VENDORID_ECLIPSE;
    case DDSI_EK_PROXY_PARTICIPANT:
      return ((const struct ddsi_proxy_participant *) e)->vendor;
    case DDSI_EK_PROXY_READER:
      return ((const struct ddsi_proxy_reader *) e)->c.vendor;
    case DDSI_EK_PROXY_WRITER:
      return ((const struct ddsi_proxy_writer *) e)->c.vendor;
  }
  assert (0);
  return NN_VENDORID_UNKNOWN;
}

int ddsi_is_builtin_entityid (ddsi_entityid_t id, nn_vendorid_t vendorid)
{
  if ((id.u & NN_ENTITYID_SOURCE_MASK) == NN_ENTITYID_SOURCE_BUILTIN)
    return 1;
  else if ((id.u & NN_ENTITYID_SOURCE_MASK) != NN_ENTITYID_SOURCE_VENDOR)
    return 0;
  else if (!vendor_is_eclipse_or_adlink (vendorid))
    return 0;
  else
  {
    if ((id.u & NN_ENTITYID_KIND_MASK) == NN_ENTITYID_KIND_CYCLONE_TOPIC_USER)
      return 0;
    return 1;
  }
}

bool ddsi_update_qos_locked (struct ddsi_entity_common *e, dds_qos_t *ent_qos, const dds_qos_t *xqos, ddsrt_wctime_t timestamp)
{
  uint64_t mask;

  mask = ddsi_xqos_delta (ent_qos, xqos, QP_CHANGEABLE_MASK & ~(QP_RXO_MASK | QP_PARTITION)) & xqos->present;
#if 0
  int a = (ent_qos->present & QP_TOPIC_DATA) ? (int) ent_qos->topic_data.length : 6;
  int b = (xqos->present & QP_TOPIC_DATA) ? (int) xqos->topic_data.length : 6;
  char *astr = (ent_qos->present & QP_TOPIC_DATA) ? (char *) ent_qos->topic_data.value : "(null)";
  char *bstr = (xqos->present & QP_TOPIC_DATA) ? (char *) xqos->topic_data.value : "(null)";
  printf ("%d: "PGUIDFMT" ent_qos %d \"%*.*s\" xqos %d \"%*.*s\" => mask %d\n",
          (int) getpid (), PGUID (e->guid),
          !!(ent_qos->present & QP_TOPIC_DATA), a, a, astr,
          !!(xqos->present & QP_TOPIC_DATA), b, b, bstr,
          !!(mask & QP_TOPIC_DATA));
#endif
  EELOGDISC (e, "ddsi_update_qos_locked "PGUIDFMT" delta=%"PRIu64" QOS={", PGUID(e->guid), mask);
  ddsi_xqos_log (DDS_LC_DISCOVERY, &e->gv->logconfig, xqos);
  EELOGDISC (e, "}\n");

  if (mask == 0)
    /* no change, or an as-yet unsupported one */
    return false;

  ddsrt_mutex_lock (&e->qos_lock);
  ddsi_xqos_fini_mask (ent_qos, mask);
  ddsi_xqos_mergein_missing (ent_qos, xqos, mask);
  ddsrt_mutex_unlock (&e->qos_lock);
  builtintopic_write_endpoint (e->gv->builtin_topic_interface, e, timestamp, true);
  return true;
}

uint64_t ddsi_get_entity_instanceid (const struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct thread_state *thrst = lookup_thread_state ();
  struct ddsi_entity_common *e;
  uint64_t iid = 0;
  thread_state_awake (thrst, gv);
  if ((e = entidx_lookup_guid_untyped (gv->entity_index, guid)) != NULL)
    iid = e->iid;
  thread_state_asleep (thrst);
  return iid;
}

int ddsi_set_topic_type_name (dds_qos_t *xqos, const char * topic_name, const char * type_name)
{
  if (!(xqos->present & QP_TYPE_NAME))
  {
    xqos->present |= QP_TYPE_NAME;
    xqos->type_name = ddsrt_strdup (type_name);
  }
  if (!(xqos->present & QP_TOPIC_NAME))
  {
    xqos->present |= QP_TOPIC_NAME;
    xqos->topic_name = ddsrt_strdup (topic_name);
  }
  return 0;
}
