// Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include <stddef.h>

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_iid.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__entity_index.h"
#include "ddsi__thread.h"
#include "ddsi__endpoint.h"
#include "ddsi__vendor.h"
#include "ddsi__xqos.h"

extern inline bool ddsi_builtintopic_is_visible (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid, ddsi_vendorid_t vendorid);
extern inline bool ddsi_builtintopic_is_builtintopic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_sertype *type);
extern inline struct ddsi_tkmap_instance *ddsi_builtintopic_get_tkmap_entry (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid);
extern inline void ddsi_builtintopic_write_endpoint (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_entity_common *e, ddsrt_wctime_t timestamp, bool alive);
extern inline void ddsi_builtintopic_write_topic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp, bool alive);

extern inline ddsi_seqno_t ddsi_writer_read_seq_xmit (const struct ddsi_writer *wr);
extern inline void ddsi_writer_update_seq_xmit (struct ddsi_writer *wr, ddsi_seqno_t nv);

int ddsi_compare_guid (const void *va, const void *vb)
{
  return memcmp (va, vb, sizeof (ddsi_guid_t));
}

int ddsi_compare_entityid (const void *va, const void *vb)
{
  const ddsi_entityid_t *a = va, *b = vb;
  return (a->u == b->u) ? 0 : ((a->u < b->u) ? -1 : 1);
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

void ddsi_entity_common_init (struct ddsi_entity_common *e, struct ddsi_domaingv *gv, const struct ddsi_guid *guid, enum ddsi_entity_kind kind, ddsrt_wctime_t tcreate, ddsi_vendorid_t vendorid, bool onlylocal)
{
  e->guid = *guid;
  e->kind = kind;
  e->tupdate = tcreate;
  e->onlylocal = onlylocal;
  e->gv = gv;
  ddsrt_mutex_init (&e->lock);
  ddsrt_mutex_init (&e->qos_lock);
  if (ddsi_builtintopic_is_visible (gv->builtin_topic_interface, guid, vendorid))
  {
    e->tk = ddsi_builtintopic_get_tkmap_entry (gv->builtin_topic_interface, guid);
    assert (e->tk != NULL); // analyzer doesn't see correlation between is_visible and get_tkmap_entry
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

ddsi_vendorid_t ddsi_get_entity_vendorid (const struct ddsi_entity_common *e)
{
  switch (e->kind)
  {
    case DDSI_EK_PARTICIPANT:
    case DDSI_EK_TOPIC:
    case DDSI_EK_READER:
    case DDSI_EK_WRITER:
      return DDSI_VENDORID_ECLIPSE;
    case DDSI_EK_PROXY_PARTICIPANT:
      return ((const struct ddsi_proxy_participant *) e)->vendor;
    case DDSI_EK_PROXY_READER:
      return ((const struct ddsi_proxy_reader *) e)->c.vendor;
    case DDSI_EK_PROXY_WRITER:
      return ((const struct ddsi_proxy_writer *) e)->c.vendor;
  }
  assert (0);
  return DDSI_VENDORID_UNKNOWN;
}

int ddsi_is_builtin_entityid (ddsi_entityid_t id, ddsi_vendorid_t vendorid)
{
  if ((id.u & DDSI_ENTITYID_SOURCE_MASK) == DDSI_ENTITYID_SOURCE_BUILTIN)
    return 1;
  else if ((id.u & DDSI_ENTITYID_SOURCE_MASK) != DDSI_ENTITYID_SOURCE_VENDOR)
    return 0;
  else if (!ddsi_vendor_is_eclipse_or_adlink (vendorid))
    return 0;
  else
  {
    if ((id.u & DDSI_ENTITYID_KIND_MASK) == DDSI_ENTITYID_KIND_CYCLONE_TOPIC_USER)
      return 0;
    return 1;
  }
}

bool ddsi_update_qos_locked (struct ddsi_entity_common *e, dds_qos_t *ent_qos, const dds_qos_t *xqos, ddsrt_wctime_t timestamp)
{
  uint64_t mask;

  mask = ddsi_xqos_delta (ent_qos, xqos, DDSI_QP_CHANGEABLE_MASK & ~(DDSI_QP_RXO_MASK | DDSI_QP_PARTITION)) & xqos->present;
#if 0
  int a = (ent_qos->present & DDSI_QP_TOPIC_DATA) ? (int) ent_qos->topic_data.length : 6;
  int b = (xqos->present & DDSI_QP_TOPIC_DATA) ? (int) xqos->topic_data.length : 6;
  char *astr = (ent_qos->present & DDSI_QP_TOPIC_DATA) ? (char *) ent_qos->topic_data.value : "(null)";
  char *bstr = (xqos->present & DDSI_QP_TOPIC_DATA) ? (char *) xqos->topic_data.value : "(null)";
  printf ("%d: "PGUIDFMT" ent_qos %d \"%*.*s\" xqos %d \"%*.*s\" => mask %d\n",
          (int) getpid (), PGUID (e->guid),
          !!(ent_qos->present & DDSI_QP_TOPIC_DATA), a, a, astr,
          !!(xqos->present & DDSI_QP_TOPIC_DATA), b, b, bstr,
          !!(mask & DDSI_QP_TOPIC_DATA));
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
  ddsi_builtintopic_write_endpoint (e->gv->builtin_topic_interface, e, timestamp, true);
  return true;
}

uint64_t ddsi_get_entity_instanceid (const struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct ddsi_thread_state *thrst = ddsi_lookup_thread_state ();
  struct ddsi_entity_common *e;
  uint64_t iid = 0;
  ddsi_thread_state_awake (thrst, gv);
  if ((e = ddsi_entidx_lookup_guid_untyped (gv->entity_index, guid)) != NULL)
    iid = e->iid;
  ddsi_thread_state_asleep (thrst);
  return iid;
}

int ddsi_set_topic_type_name (dds_qos_t *xqos, const char * topic_name, const char * type_name)
{
  if (!(xqos->present & DDSI_QP_TYPE_NAME))
  {
    xqos->present |= DDSI_QP_TYPE_NAME;
    xqos->type_name = ddsrt_strdup (type_name);
  }
  if (!(xqos->present & DDSI_QP_TOPIC_NAME))
  {
    xqos->present |= DDSI_QP_TOPIC_NAME;
    xqos->topic_name = ddsrt_strdup (topic_name);
  }
  return 0;
}

ddsi_entityid_t ddsi_hton_entityid (ddsi_entityid_t e)
{
  e.u = ddsrt_toBE4u (e.u);
  return e;
}

ddsi_entityid_t ddsi_ntoh_entityid (ddsi_entityid_t e)
{
  e.u = ddsrt_fromBE4u (e.u);
  return e;
}
