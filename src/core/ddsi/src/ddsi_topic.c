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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__topic.h"
#include "ddsi__entity_index.h"
#include "ddsi__discovery.h"
#ifdef DDS_HAS_TOPIC_DISCOVERY
#include "ddsi__discovery_topic.h"
#endif
#include "ddsi__xmsg.h"
#include "ddsi__misc.h"
#include "ddsi__gc.h"
#include "ddsi__typelib.h"
#include "ddsi__vendor.h"
#include "ddsi__xqos.h"
#include "dds/dds.h"

#ifdef DDS_HAS_TOPIC_DISCOVERY

static struct ddsi_topic_definition * ref_topic_definition_locked (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, const ddsi_typeid_t *type_id, struct dds_qos *qos, bool *is_new);
static struct ddsi_topic_definition * ref_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, const ddsi_typeid_t *type_id, struct dds_qos *qos, bool *is_new);

static void unref_topic_definition_locked (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp);
static void unref_topic_definition (struct ddsi_domaingv *gv, struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp);

static struct ddsi_topic_definition * new_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *type, const struct dds_qos *qos);

#endif /* DDS_HAS_TOPIC_DISCOVERY */

struct gc_proxy_tp {
  struct ddsi_proxy_participant *proxypp;
  struct ddsi_proxy_topic *proxytp;
  ddsrt_wctime_t timestamp;
};

struct gc_tpd {
  struct ddsi_topic_definition *tpd;
  ddsrt_wctime_t timestamp;
};

int ddsi_is_builtin_topic (ddsi_entityid_t id, ddsi_vendorid_t vendorid)
{
  return ddsi_is_builtin_entityid (id, vendorid) && ddsi_is_topic_entityid (id);
}

int ddsi_is_topic_entityid (ddsi_entityid_t id)
{
  switch (id.u & DDSI_ENTITYID_KIND_MASK)
  {
    case DDSI_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN:
    case DDSI_ENTITYID_KIND_CYCLONE_TOPIC_USER:
      return 1;
    default:
      return 0;
  }
}

#ifdef DDS_HAS_TOPIC_DISCOVERY

/* TOPIC -------------------------------------------------------- */

dds_return_t ddsi_new_topic (struct ddsi_topic **tp_out, struct ddsi_guid *tpguid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *sertype,
    const struct dds_qos *xqos, bool is_builtin, bool *new_topic_def)
{
  dds_return_t rc;
  ddsrt_wctime_t timestamp = ddsrt_time_wallclock ();
  struct ddsi_domaingv *gv = pp->e.gv;
  tpguid->prefix = pp->e.guid.prefix;
  if ((rc = ddsi_participant_allocate_entityid (&tpguid->entityid, (is_builtin ? DDSI_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN : DDSI_ENTITYID_KIND_CYCLONE_TOPIC_USER) | DDSI_ENTITYID_SOURCE_VENDOR, pp)) < 0)
    return rc;
  assert (ddsi_entidx_lookup_topic_guid (gv->entity_index, tpguid) == NULL);

  struct ddsi_topic *tp = ddsrt_malloc (sizeof (*tp));
  if (tp_out)
    *tp_out = tp;
  ddsi_entity_common_init (&tp->e, gv, tpguid, DDSI_EK_TOPIC, timestamp, DDSI_VENDORID_ECLIPSE, pp->e.onlylocal);
  tp->pp = ddsi_ref_participant (pp, &tp->e.guid);

  /* Copy QoS, merging in defaults */
  struct dds_qos *tp_qos = ddsrt_malloc (sizeof (*tp_qos));
  ddsi_xqos_copy (tp_qos, xqos);
  ddsi_xqos_mergein_missing (tp_qos, &ddsi_default_qos_topic, ~(uint64_t)0);
  assert (tp_qos->aliased == 0);

  /* Set topic name, type name and type information in qos */
  tp_qos->present |= DDSI_QP_TYPE_INFORMATION;
  tp_qos->type_information = ddsi_sertype_typeinfo (sertype);
  assert (tp_qos->type_information);
  ddsi_set_topic_type_name (tp_qos, topic_name, sertype->type_name);

  if (gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    ELOGDISC (tp, "TOPIC "PGUIDFMT" QOS={", PGUID (tp->e.guid));
    ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, tp_qos);
    ELOGDISC (tp, "}\n");
  }
  tp->definition = ref_topic_definition (gv, sertype, ddsi_typeinfo_complete_typeid (tp_qos->type_information), tp_qos, new_topic_def);
  assert (tp->definition);
  if (new_topic_def)
    ddsi_builtintopic_write_topic (gv->builtin_topic_interface, tp->definition, timestamp, true);
  ddsi_xqos_fini (tp_qos);
  ddsrt_free (tp_qos);

  ddsrt_mutex_lock (&tp->e.lock);
  ddsi_entidx_insert_topic_guid (gv->entity_index, tp);
  (void) ddsi_sedp_write_topic (tp, true);
  ddsrt_mutex_unlock (&tp->e.lock);
  return 0;
}

void ddsi_update_topic_qos (struct ddsi_topic *tp, const dds_qos_t *xqos)
{
  /* Updating the topic qos, which means replacing the topic definition for a topic,
     does not result in a new topic in the context of the find topic api. So there
     is no need to broadcast on gv->new_topic_cond */

  struct ddsi_domaingv *gv = tp->e.gv;
  ddsrt_mutex_lock (&tp->e.lock);
  ddsrt_mutex_lock (&tp->e.qos_lock);
  struct ddsi_topic_definition *tpd = tp->definition;
  uint64_t mask = ddsi_xqos_delta (tpd->xqos, xqos, DDSI_QP_CHANGEABLE_MASK & ~(DDSI_QP_RXO_MASK | DDSI_QP_PARTITION)) & xqos->present;
  GVLOGDISC ("ddsi_update_topic_qos "PGUIDFMT" delta=%"PRIu64" QOS={", PGUID(tp->e.guid), mask);
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");
  if (mask == 0)
  {
    ddsrt_mutex_unlock (&tp->e.qos_lock);
    ddsrt_mutex_unlock (&tp->e.lock);
    return; /* no change, or an as-yet unsupported one */
  }

  bool new_tpd = false;
  dds_qos_t *newqos = dds_create_qos ();
  ddsi_xqos_mergein_missing (newqos, xqos, mask);
  ddsi_xqos_mergein_missing (newqos, tpd->xqos, ~(uint64_t)0);
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  tp->definition = ref_topic_definition_locked (gv, NULL, ddsi_type_pair_complete_id (tpd->type_pair), newqos, &new_tpd);
  assert (tp->definition);
  unref_topic_definition_locked (tpd, ddsrt_time_wallclock());
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  if (new_tpd)
    ddsi_builtintopic_write_topic (gv->builtin_topic_interface, tp->definition, ddsrt_time_wallclock(), true);
  ddsrt_mutex_unlock (&tp->e.qos_lock);
  (void) ddsi_sedp_write_topic (tp, true);
  ddsrt_mutex_unlock (&tp->e.lock);
  dds_delete_qos (newqos);
}

static void gc_delete_topic (struct ddsi_gcreq *gcreq)
{
  struct ddsi_topic *tp = gcreq->arg;
  ELOGDISC (tp, "gc_delete_topic (%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (tp->e.guid));
  ddsi_gcreq_free (gcreq);
  if (!ddsi_is_builtin_entityid (tp->e.guid.entityid, DDSI_VENDORID_ECLIPSE))
    (void) ddsi_sedp_write_topic (tp, false);
  ddsi_entity_common_fini (&tp->e);
  unref_topic_definition (tp->e.gv, tp->definition, ddsrt_time_wallclock());
  ddsi_unref_participant (tp->pp, &tp->e.guid);
  ddsrt_free (tp);
}

static int gcreq_topic (struct ddsi_topic *tp)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (tp->e.gv->gcreq_queue, gc_delete_topic);
  gcreq->arg = tp;
  ddsi_gcreq_enqueue (gcreq);
  return 0;
}

dds_return_t ddsi_delete_topic (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct ddsi_topic *tp;
  assert (ddsi_is_topic_entityid (guid->entityid));
  if ((tp = ddsi_entidx_lookup_topic_guid (gv->entity_index, guid)) == NULL)
  {
    GVLOGDISC ("ddsi_delete_topic (guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("ddsi_delete_topic (guid "PGUIDFMT") ...\n", PGUID (*guid));
  ddsi_entidx_remove_topic_guid (gv->entity_index, tp);
  gcreq_topic (tp);
  return 0;
}

/* TOPIC DEFINITION ---------------------------------------------- */

static void gc_delete_topic_definition (struct ddsi_gcreq *gcreq)
{
  struct gc_tpd *gcdata = gcreq->arg;
  struct ddsi_topic_definition *tpd = gcdata->tpd;
  struct ddsi_domaingv *gv = tpd->gv;
  GVLOGDISC ("gcreq_delete_topic_definition(%p)\n", (void *) gcreq);
  ddsi_builtintopic_write_topic (gv->builtin_topic_interface, tpd, gcdata->timestamp, false);
  if (tpd->type_pair)
  {
    ddsi_type_unref (gv, tpd->type_pair->minimal);
    ddsi_type_unref (gv, tpd->type_pair->complete);
    ddsrt_free (tpd->type_pair);
  }
  ddsi_xqos_fini (tpd->xqos);
  ddsrt_free (tpd->xqos);
  ddsrt_free (tpd);
  ddsrt_free (gcdata);
  ddsi_gcreq_free (gcreq);
}

static int gcreq_topic_definition (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (tpd->gv->gcreq_queue, gc_delete_topic_definition);
  struct gc_tpd *gcdata = ddsrt_malloc (sizeof (*gcdata));
  gcdata->tpd = tpd;
  gcdata->timestamp = timestamp;
  gcreq->arg = gcdata;
  ddsi_gcreq_enqueue (gcreq);
  return 0;
}

static void delete_topic_definition_locked (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv *gv = tpd->gv;
  GVLOGDISC ("delete_topic_definition_locked (%p) ", tpd);
  ddsrt_hh_remove_present (gv->topic_defs, tpd);
  GVLOGDISC ("- deleting\n");
  gcreq_topic_definition (tpd, timestamp);
}

uint32_t ddsi_topic_definition_hash (const struct ddsi_topic_definition *tpd)
{
  assert (tpd != NULL);
  return *(uint32_t *) tpd->key;
}

static void set_ddsi_topic_definition_hash (struct ddsi_topic_definition *tpd)
{
  const ddsi_typeid_t *tid_complete = ddsi_type_pair_complete_id (tpd->type_pair);
  assert (!ddsi_typeid_is_none (tid_complete));
  assert (tpd->xqos != NULL);

  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);

  /* Add type id to the key */
  unsigned char *buf = NULL;
  uint32_t sz = 0;
  ddsi_typeid_ser (tid_complete, &buf, &sz);
  assert (sz && buf);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) buf, sz);
  ddsrt_free (buf);

  /* Add serialized qos as part of the key. The type_information field
     of the QoS is not included, as this field may contain a list of
     dependent type ids and therefore may be different for equal
     type definitions */
  struct ddsi_xmsg *mqos = ddsi_xmsg_new (tpd->gv->xmsgpool, &ddsi_nullguid, NULL, 0, DDSI_XMSG_KIND_DATA);
  ddsi_xqos_addtomsg (mqos, tpd->xqos, ~(DDSI_QP_TYPE_INFORMATION), DDSI_PLIST_CONTEXT_TOPIC);
  size_t sqos_sz;
  void * sqos = ddsi_xmsg_payload (&sqos_sz, mqos);
  assert (sqos_sz <= UINT32_MAX);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) sqos, (uint32_t) sqos_sz);
  ddsi_xmsg_free (mqos);

  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) &tpd->key);
}

static struct ddsi_topic_definition * ref_topic_definition_locked (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, const ddsi_typeid_t *type_id, struct dds_qos *qos, bool *is_new)
{
  const ddsi_typeid_t *type_id_minimal = NULL, *type_id_complete = NULL;
  if (ddsi_typeid_is_minimal (type_id))
    type_id_minimal = type_id;
  else
    type_id_complete = type_id;
  struct ddsi_topic_definition templ = {
    .xqos = qos,
    .type_pair = ddsi_type_pair_init (type_id_minimal, type_id_complete),
    .gv = gv
  };
  set_ddsi_topic_definition_hash (&templ);
  struct ddsi_topic_definition *tpd = ddsrt_hh_lookup (gv->topic_defs, &templ);
  ddsi_type_pair_free (templ.type_pair);
  if (tpd) {
    tpd->refc++;
    *is_new = false;
  } else {
    tpd = new_topic_definition (gv, sertype, qos);
    if (tpd)
      *is_new = true;
  }
  return tpd;
}

static struct ddsi_topic_definition * ref_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, const ddsi_typeid_t *type_id, struct dds_qos *qos, bool *is_new)
{
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  struct ddsi_topic_definition *tpd = ref_topic_definition_locked (gv, sertype, type_id, qos, is_new);
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  return tpd;
}

static void unref_topic_definition_locked (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp)
{
  if (!--tpd->refc)
    delete_topic_definition_locked (tpd, timestamp);
}

static void unref_topic_definition (struct ddsi_domaingv *gv, struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp)
{
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  unref_topic_definition_locked (tpd, timestamp);
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
}

int ddsi_topic_definition_equal (const struct ddsi_topic_definition *tpd_a, const struct ddsi_topic_definition *tpd_b)
{
  if (tpd_a != NULL && tpd_b != NULL)
  {
    // The complete type identifier and qos should always be set for a topic definition
    assert (tpd_a->xqos != NULL && tpd_b->xqos != NULL);
    const ddsi_typeid_t *tid_a = ddsi_type_pair_complete_id (tpd_a->type_pair),
      *tid_b = ddsi_type_pair_complete_id (tpd_b->type_pair);
    return !ddsi_typeid_compare (tid_a, tid_b)
        && !ddsi_xqos_delta (tpd_a->xqos, tpd_b->xqos, ~(DDSI_QP_TYPE_INFORMATION));
  }
  return tpd_a == tpd_b;
}

static struct ddsi_topic_definition * new_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *type, const struct dds_qos *qos)
{
  dds_return_t ret;
  assert ((qos->present & (DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME)) == (DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME));
  struct ddsi_topic_definition *tpd = ddsrt_malloc (sizeof (*tpd));
  if (!tpd)
    goto err;
  tpd->xqos = ddsi_xqos_dup (qos);
  tpd->refc = 1;
  tpd->gv = gv;
  tpd->type_pair = ddsrt_malloc (sizeof (*tpd->type_pair));
  if (!tpd->type_pair)
  {
    ddsi_xqos_fini (tpd->xqos);
    ddsrt_free (tpd);
    tpd = NULL;
    goto err;
  }
  if (type != NULL)
  {
    /* This shouldn't fail, because the sertype used here is already in the typelib
       as the types are referenced from dds_create_topic_impl */
    ret = ddsi_type_ref_local (gv, &tpd->type_pair->minimal, type, DDSI_TYPEID_KIND_MINIMAL);
    assert (ret == DDS_RETCODE_OK);
    ret = ddsi_type_ref_local (gv, &tpd->type_pair->complete, type, DDSI_TYPEID_KIND_COMPLETE);
    assert (ret == DDS_RETCODE_OK);
    (void) ret;
  }
  else
  {
    assert (qos->present & DDSI_QP_TYPE_INFORMATION);
    if ((ret = ddsi_type_ref_proxy (gv, &tpd->type_pair->minimal, qos->type_information, DDSI_TYPEID_KIND_MINIMAL, NULL)) != DDS_RETCODE_OK
        || ddsi_type_ref_proxy (gv, &tpd->type_pair->complete, qos->type_information, DDSI_TYPEID_KIND_COMPLETE, NULL) != DDS_RETCODE_OK)
    {
      if (ret == DDS_RETCODE_OK)
        ddsi_type_unref (gv, tpd->type_pair->minimal);
      ddsi_xqos_fini (tpd->xqos);
      ddsrt_free (tpd->type_pair);
      ddsrt_free (tpd);
      tpd = NULL;
      goto err;
    }
  }

  set_ddsi_topic_definition_hash (tpd);
  if (gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    GVLOGDISC (" topic-definition 0x%p: key 0x", tpd);
    for (size_t i = 0; i < sizeof (tpd->key); i++)
      GVLOGDISC ("%02x", tpd->key[i]);
    GVLOGDISC (" QOS={");
    ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, tpd->xqos);
    GVLOGDISC ("}\n");
  }

  ddsrt_hh_add_absent (gv->topic_defs, tpd);
err:
  return tpd;
}

dds_return_t ddsi_lookup_topic_definition (struct ddsi_domaingv *gv, const char * topic_name, const ddsi_typeid_t *type_id, struct ddsi_topic_definition **tpd)
{
  assert (tpd != NULL);
  struct ddsrt_hh_iter it;
  dds_return_t ret = DDS_RETCODE_OK;
  *tpd = NULL;
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  for (struct ddsi_topic_definition *tpd1 = ddsrt_hh_iter_first (gv->topic_defs, &it); tpd1; tpd1 = ddsrt_hh_iter_next (&it))
  {
    if (!strcmp (tpd1->xqos->topic_name, topic_name) &&
        (ddsi_typeid_is_none (type_id) || ((tpd1->xqos->present & DDSI_QP_TYPE_INFORMATION) && !ddsi_typeid_compare (type_id, ddsi_typeinfo_complete_typeid (tpd1->xqos->type_information)))))
    {
      *tpd = tpd1;
      break;
    }
  }
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  return ret;
}

/* PROXY-TOPIC --------------------------------------------------- */

struct ddsi_proxy_topic *ddsi_lookup_proxy_topic (struct ddsi_proxy_participant *proxypp, const ddsi_guid_t *guid)
{
  assert (proxypp != NULL);
  ddsrt_mutex_lock (&proxypp->e.lock);
  struct ddsi_proxy_topic *ptp = ddsrt_avl_lookup (&ddsi_proxypp_proxytp_treedef, &proxypp->topics, &guid->entityid);
  ddsrt_mutex_unlock (&proxypp->e.lock);
  return ptp;
}

dds_return_t ddsi_new_proxy_topic (struct ddsi_proxy_participant *proxypp, ddsi_seqno_t seq, const ddsi_guid_t *guid, const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id_complete, struct dds_qos *qos, ddsrt_wctime_t timestamp)
{
  assert (proxypp != NULL);
  struct ddsi_domaingv *gv = proxypp->e.gv;
  bool new_tpd = false;
  struct ddsi_topic_definition *tpd = NULL;
  if (!ddsi_typeid_is_none (type_id_complete))
    tpd = ref_topic_definition (gv, NULL, type_id_complete, qos, &new_tpd);
  else if (!ddsi_typeid_is_none (type_id_minimal))
    tpd = ref_topic_definition (gv, NULL, type_id_minimal, qos, &new_tpd);
  if (tpd == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
#ifndef NDEBUG
  bool found_proxytp = ddsi_lookup_proxy_topic (proxypp, guid);
  assert (!found_proxytp);
#endif
  struct ddsi_proxy_topic *proxytp = ddsrt_malloc (sizeof (*proxytp));
  proxytp->entityid = guid->entityid;
  proxytp->definition = tpd;
  proxytp->seq = seq;
  proxytp->tupdate = timestamp;
  proxytp->deleted = 0;
  ddsrt_mutex_lock (&proxypp->e.lock);
  ddsrt_avl_insert (&ddsi_proxypp_proxytp_treedef, &proxypp->topics, proxytp);
  ddsrt_mutex_unlock (&proxypp->e.lock);
  if (new_tpd)
  {
    ddsi_builtintopic_write_topic (gv->builtin_topic_interface, tpd, timestamp, true);
    ddsrt_mutex_lock (&gv->new_topic_lock);
    gv->new_topic_version++;
    ddsrt_cond_broadcast (&gv->new_topic_cond);
    ddsrt_mutex_unlock (&gv->new_topic_lock);
  }

  return DDS_RETCODE_OK;
}

void ddsi_update_proxy_topic (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_topic *proxytp, ddsi_seqno_t seq, struct dds_qos *xqos, ddsrt_wctime_t timestamp)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  struct ddsi_domaingv *gv = proxypp->e.gv;
  if (proxytp->deleted)
  {
    GVLOGDISC (" deleting\n");
    ddsrt_mutex_unlock (&proxypp->e.lock);
    return;
  }
  if (seq <= proxytp->seq)
  {
    GVLOGDISC (" seqno not new\n");
    ddsrt_mutex_unlock (&proxypp->e.lock);
    return;
  }
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  struct ddsi_topic_definition *tpd0 = proxytp->definition;
  proxytp->seq = seq;
  proxytp->tupdate = timestamp;
  uint64_t mask = ddsi_xqos_delta (tpd0->xqos, xqos, DDSI_QP_CHANGEABLE_MASK & ~(DDSI_QP_RXO_MASK | DDSI_QP_PARTITION)) & xqos->present;
  GVLOGDISC ("ddsi_update_proxy_topic %"PRIx32" delta=%"PRIu64" QOS={", proxytp->entityid.u, mask);
  ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, xqos);
  GVLOGDISC ("}\n");
  if (mask == 0)
  {
    ddsrt_mutex_unlock (&gv->topic_defs_lock);
    ddsrt_mutex_unlock (&proxypp->e.lock);
    return; /* no change, or an as-yet unsupported one */
  }
  dds_qos_t *newqos = dds_create_qos ();
  ddsi_xqos_mergein_missing (newqos, xqos, mask);
  ddsi_xqos_mergein_missing (newqos, tpd0->xqos, ~(uint64_t) 0);
  bool new_tpd = false;
  struct ddsi_topic_definition *tpd1 = ref_topic_definition_locked (gv, NULL, ddsi_type_pair_complete_id (tpd0->type_pair), newqos, &new_tpd);
  assert (tpd1);
  unref_topic_definition_locked (tpd0, timestamp);
  proxytp->definition = tpd1;
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  ddsrt_mutex_unlock (&proxypp->e.lock);
  dds_delete_qos (newqos);
  if (new_tpd)
  {
    ddsi_builtintopic_write_topic (gv->builtin_topic_interface, tpd1, timestamp, true);

    ddsrt_mutex_lock (&gv->new_topic_lock);
    gv->new_topic_version++;
    ddsrt_cond_broadcast (&gv->new_topic_cond);
    ddsrt_mutex_unlock (&gv->new_topic_lock);
  }
}

static void gc_delete_proxy_topic (struct ddsi_gcreq *gcreq)
{
  struct gc_proxy_tp *gcdata = gcreq->arg;

  ddsrt_mutex_lock (&gcdata->proxypp->e.lock);
  struct ddsi_domaingv *gv = gcdata->proxypp->e.gv;
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  struct ddsi_topic_definition *tpd = gcdata->proxytp->definition;
  GVLOGDISC ("gc_delete_proxy_topic (%p)\n", (void *) gcdata->proxytp);
  ddsrt_avl_delete (&ddsi_proxypp_proxytp_treedef, &gcdata->proxypp->topics, gcdata->proxytp);
  unref_topic_definition_locked (tpd, gcdata->timestamp);
  ddsrt_free (gcdata->proxytp);
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  ddsrt_mutex_unlock (&gcdata->proxypp->e.lock);
  ddsrt_free (gcdata);
  ddsi_gcreq_free (gcreq);
}

static int gcreq_proxy_topic (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_topic *proxytp, ddsrt_wctime_t timestamp)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (proxytp->definition->gv->gcreq_queue, gc_delete_proxy_topic);
  struct gc_proxy_tp *gcdata = ddsrt_malloc (sizeof (*gcdata));
  gcdata->proxypp = proxypp;
  gcdata->proxytp = proxytp;
  gcdata->timestamp = timestamp;
  gcreq->arg = gcdata;
  ddsi_gcreq_enqueue (gcreq);
  return 0;
}

int ddsi_delete_proxy_topic_locked (struct ddsi_proxy_participant *proxypp, struct ddsi_proxy_topic *proxytp, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv *gv = proxypp->e.gv;
  GVLOGDISC ("ddsi_delete_proxy_topic_locked (%p) ", proxypp);
  if (proxytp->deleted)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  proxytp->deleted = 1;
  gcreq_proxy_topic (proxypp, proxytp, timestamp);
  return DDS_RETCODE_OK;
}

#endif

