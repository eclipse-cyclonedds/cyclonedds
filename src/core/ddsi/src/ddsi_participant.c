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

#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_handshake.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_receive.h"
#include "dds/ddsi/q_addrset.h"
#include "dds__whc.h"

static const unsigned builtin_writers_besmask =
  NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER |
  NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
  NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER |
  NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER
#ifdef DDS_HAS_TYPE_DISCOVERY
  | NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_WRITER
  | NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_WRITER
#endif
;

int compare_ldur (const void *va, const void *vb);

/* used in participant for keeping writer liveliness renewal */
const ddsrt_fibheap_def_t ldur_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct ldur_fhnode, heapnode), compare_ldur);
/* used in (proxy)participant for writer liveliness monitoring */
const ddsrt_fibheap_def_t lease_fhdef_pp = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct lease, pp_heapnode), compare_lease_tdur);

const ddsrt_avl_treedef_t deleted_participants_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct deleted_participant, avlnode), offsetof (struct deleted_participant, guid), ddsi_compare_guid, 0);

int compare_ldur (const void *va, const void *vb)
{
  const struct ldur_fhnode *a = va;
  const struct ldur_fhnode *b = vb;
  return (a->ldur == b->ldur) ? 0 : (a->ldur < b->ldur) ? -1 : 1;
}

struct deleted_participants_admin *ddsi_deleted_participants_admin_new (const ddsrt_log_cfg_t *logcfg, int64_t delay)
{
  struct deleted_participants_admin *admin = ddsrt_malloc (sizeof (*admin));
  ddsrt_mutex_init (&admin->deleted_participants_lock);
  ddsrt_avl_init (&deleted_participants_treedef, &admin->deleted_participants);
  admin->logcfg = logcfg;
  admin->delay = delay;
  return admin;
}

void ddsi_deleted_participants_admin_free (struct deleted_participants_admin *admin)
{
  ddsrt_avl_free (&deleted_participants_treedef, &admin->deleted_participants, ddsrt_free);
  ddsrt_mutex_destroy (&admin->deleted_participants_lock);
  ddsrt_free (admin);
}

static void ddsi_prune_deleted_participant_guids_unlocked (struct deleted_participants_admin *admin, ddsrt_mtime_t tnow)
{
  /* Could do a better job of finding prunable ones efficiently under
     all circumstances, but I expect the tree to be very small at all
     times, so a full scan is fine, too ... */
  struct deleted_participant *dpp;
  dpp = ddsrt_avl_find_min (&deleted_participants_treedef, &admin->deleted_participants);
  while (dpp)
  {
    struct deleted_participant *dpp1 = ddsrt_avl_find_succ (&deleted_participants_treedef, &admin->deleted_participants, dpp);
    if (dpp->t_prune.v < tnow.v)
    {
      DDS_CLOG (DDS_LC_DISCOVERY, admin->logcfg, "ddsi_prune_deleted_participant_guid("PGUIDFMT")\n", PGUID (dpp->guid));
      ddsrt_avl_delete (&deleted_participants_treedef, &admin->deleted_participants, dpp);
      ddsrt_free (dpp);
    }
    dpp = dpp1;
  }
}

void ddsi_prune_deleted_participant_guids (struct deleted_participants_admin *admin, ddsrt_mtime_t tnow)
{
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  ddsi_prune_deleted_participant_guids_unlocked (admin, tnow);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

void ddsi_remember_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid)
{
  struct deleted_participant *n;
  ddsrt_avl_ipath_t path;
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  if (ddsrt_avl_lookup_ipath (&deleted_participants_treedef, &admin->deleted_participants, guid, &path) == NULL)
  {
    if ((n = ddsrt_malloc (sizeof (*n))) != NULL)
    {
      n->guid = *guid;
      n->t_prune = DDSRT_MTIME_NEVER;
      n->for_what = DPG_LOCAL | DPG_REMOTE;
      ddsrt_avl_insert_ipath (&deleted_participants_treedef, &admin->deleted_participants, n, &path);
    }
  }
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

int ddsi_is_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid, unsigned for_what)
{
  struct deleted_participant *n;
  int known;
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  ddsi_prune_deleted_participant_guids_unlocked (admin, ddsrt_time_monotonic ());
  if ((n = ddsrt_avl_lookup (&deleted_participants_treedef, &admin->deleted_participants, guid)) == NULL)
    known = 0;
  else
    known = ((n->for_what & for_what) != 0);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
  return known;
}

void ddsi_remove_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid, unsigned for_what)
{
  struct deleted_participant *n;
  DDS_CLOG (DDS_LC_DISCOVERY, admin->logcfg, "ddsi_remove_deleted_participant_guid("PGUIDFMT" for_what=%x)\n", PGUID (*guid), for_what);
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  if ((n = ddsrt_avl_lookup (&deleted_participants_treedef, &admin->deleted_participants, guid)) != NULL)
    n->t_prune = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), admin->delay);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

dds_return_t ddsi_participant_allocate_entityid (ddsi_entityid_t *id, uint32_t kind, struct ddsi_participant *pp)
{
  uint32_t id1;
  int ret = 0;
  ddsrt_mutex_lock (&pp->e.lock);
  if (inverse_uint32_set_alloc(&id1, &pp->avail_entityids.x))
  {
    *id = ddsi_to_entityid (id1 * NN_ENTITYID_ALLOCSTEP + kind);
    ret = 0;
  }
  else
  {
    DDS_CERROR (&pp->e.gv->logconfig, "ddsi_participant_allocate_entityid("PGUIDFMT"): all ids in use\n", PGUID(pp->e.guid));
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsrt_mutex_unlock (&pp->e.lock);
  return ret;
}

void ddsi_participant_release_entityid (struct ddsi_participant *pp, ddsi_entityid_t id)
{
  ddsrt_mutex_lock (&pp->e.lock);
  inverse_uint32_set_free(&pp->avail_entityids.x, id.u / NN_ENTITYID_ALLOCSTEP);
  ddsrt_mutex_unlock (&pp->e.lock);
}

static void force_as_disc_address (struct ddsi_domaingv *gv, const ddsi_guid_t *subguid)
{
  struct ddsi_writer *wr = entidx_lookup_writer_guid (gv->entity_index, subguid);
  assert (wr != NULL);
  ddsrt_mutex_lock (&wr->e.lock);
  unref_addrset (wr->as);
  wr->as = ref_addrset (gv->as_disc);
  ddsrt_mutex_unlock (&wr->e.lock);
}

#ifdef DDS_HAS_SECURITY
static void add_security_builtin_endpoints (struct ddsi_participant *pp, ddsi_guid_t *subguid, const ddsi_guid_t *group_guid, struct ddsi_domaingv *gv, bool add_writers, bool add_readers)
{
  if (add_writers)
  {
    struct whc_writer_info *wrinfo;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_endpoint_xqos_wr);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_SECURE_NAME, gv->spdp_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    /* But we need the as_disc address set for SPDP, because we need to
       send it to everyone regardless of the existence of readers. */
    force_as_disc_address(gv, subguid);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_stateless_xqos_wr);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_STATELESS_MESSAGE_NAME, gv->pgm_stateless_type, &gv->builtin_stateless_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_secure_volatile_xqos_wr);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_VOLATILE_MESSAGE_SECURE_NAME, gv->pgm_volatile_type, &gv->builtin_secure_volatile_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_ANNOUNCER;

    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_endpoint_xqos_wr);

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_SECURE_NAME, gv->pmd_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_ANNOUNCER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_SECURE_NAME, gv->sedp_writer_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_ANNOUNCER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_SECURE_NAME, gv->sedp_reader_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER;

    whc_free_wrinfo (wrinfo);
  }

  if (add_readers)
  {
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_SECURE_NAME, gv->sedp_reader_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_DETECTOR;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_SECURE_NAME, gv->sedp_writer_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_DETECTOR;
  }

  /*
   * When security is enabled configure the associated necessary builtin readers independent of the
   * besmode flag setting, because all participant do require authentication.
   */
  subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER);
  ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_SECURE_NAME, gv->spdp_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_DETECTOR;

  subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER);
  ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_VOLATILE_MESSAGE_SECURE_NAME, gv->pgm_volatile_type, &gv->builtin_secure_volatile_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_DETECTOR;

  subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER);
  ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_STATELESS_MESSAGE_NAME, gv->pgm_stateless_type, &gv->builtin_stateless_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_DETECTOR;

  subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER);
  ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_SECURE_NAME, gv->pmd_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_DETECTOR;
}

static void connect_participant_secure (struct ddsi_domaingv *gv, struct ddsi_participant *pp)
{
  struct ddsi_proxy_participant *proxypp;
  struct entidx_enum_proxy_participant it;

  if (q_omg_participant_is_secure(pp))
  {
    q_omg_security_participant_set_initialized(pp);

    entidx_enum_proxy_participant_init (&it, gv->entity_index);
    while ((proxypp = entidx_enum_proxy_participant_next (&it)) != NULL)
    {
      /* Do not start handshaking when security info doesn't match. */
      if (q_omg_security_remote_participant_is_initialized(proxypp) && q_omg_is_similar_participant_security_info(pp, proxypp))
        ddsi_handshake_register(pp, proxypp, handshake_end_cb);
    }
    entidx_enum_proxy_participant_fini (&it);
  }
}

static void disconnect_participant_secure (struct ddsi_participant *pp)
{
  struct ddsi_proxy_participant *proxypp;
  struct entidx_enum_proxy_participant it;
  struct ddsi_domaingv * const gv = pp->e.gv;

  if (q_omg_participant_is_secure(pp))
  {
    entidx_enum_proxy_participant_init (&it, gv->entity_index);
    while ((proxypp = entidx_enum_proxy_participant_next (&it)) != NULL)
    {
      ddsi_handshake_remove(pp, proxypp);
    }
    entidx_enum_proxy_participant_fini (&it);
  }
}
#endif /* DDS_HAS_SECURITY */

static void add_builtin_endpoints (struct ddsi_participant *pp, ddsi_guid_t *subguid, const ddsi_guid_t *group_guid, struct ddsi_domaingv *gv, bool add_writers, bool add_readers)
{
  if (add_writers)
  {
    struct whc_writer_info *wrinfo_tl = whc_make_wrinfo (NULL, &gv->builtin_endpoint_xqos_wr);

    /* SEDP writers: */
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME, gv->sedp_reader_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_NAME, gv->sedp_writer_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;

    /* PMD writer: */
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER);
    ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_NAME, gv->pmd_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;

#ifdef DDS_HAS_TOPIC_DISCOVERY
    if (gv->config.enable_topic_discovery_endpoints)
    {
      /* SEDP topic writer: */
      subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER);
      ddsi_new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TOPIC_NAME, gv->sedp_topic_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
      pp->bes |= NN_DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    /* TypeLookup writers */
    struct whc_writer_info *wrinfo_vol = whc_make_wrinfo (NULL, &gv->builtin_volatile_xqos_wr);
    struct ddsi_writer *wr_tl_req, *wr_tl_reply;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER);
    ddsi_new_writer_guid (&wr_tl_req, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REQUEST_NAME, gv->tl_svc_request_type, &gv->builtin_volatile_xqos_wr, whc_new(gv, wrinfo_vol), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_WRITER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
    ddsi_new_writer_guid (&wr_tl_reply, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REPLY_NAME, gv->tl_svc_reply_type, &gv->builtin_volatile_xqos_wr, whc_new(gv, wrinfo_vol), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_WRITER;

    /* The built-in type lookup writers are keep-all writers, because the topic is keyless (using DDS-RPC request
       and reply type). This means that the throttling will occur in case the WHC limits are reached. But the
       function throttle_writer asserts that the writer is not a built-in writer: throttling the type lookup
       writer may force the thread to go asleep, and because these requests are done during qos matching, this
       can cause other problems. Therefore, the WHC watermarks are set to a high value, so that no throttling
       will occur. */
    wr_tl_req->whc_low = wr_tl_req->whc_high = INT32_MAX;
    wr_tl_reply->whc_low = wr_tl_reply->whc_high = INT32_MAX;

    whc_free_wrinfo (wrinfo_vol);
#endif
    whc_free_wrinfo (wrinfo_tl);
  }

  if (add_readers)
  {
    /* SPDP reader: */
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_NAME, gv->spdp_type, &gv->spdp_endpoint_xqos, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;

    /* SEDP readers: */
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME, gv->sedp_reader_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_NAME, gv->sedp_writer_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;

    /* PMD reader: */
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_NAME, gv->pmd_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER;

#ifdef DDS_HAS_TOPIC_DISCOVERY
    if (gv->config.enable_topic_discovery_endpoints)
    {
      /* SEDP topic reader: */
      subguid->entityid = ddsi_to_entityid (NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER);
      ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TOPIC_NAME, gv->sedp_topic_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
      pp->bes |= NN_DISC_BUILTIN_ENDPOINT_TOPICS_DETECTOR;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    /* TypeLookup readers: */
    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REQUEST_NAME, gv->tl_svc_request_type, &gv->builtin_volatile_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_READER;

    subguid->entityid = ddsi_to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER);
    ddsi_new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REPLY_NAME, gv->tl_svc_reply_type, &gv->builtin_volatile_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_READER;
#endif
  }

#ifdef DDS_HAS_SECURITY
  if (q_omg_participant_is_secure (pp))
    add_security_builtin_endpoints (pp, subguid, group_guid, gv, add_writers, add_readers);
#endif
}

void ddsi_gc_participant_lease (struct gcreq *gcreq)
{
  lease_free (gcreq->arg);
  gcreq_free (gcreq);
}

static void participant_replace_minl (struct ddsi_participant *pp, struct lease *lnew)
{
  /* By loading/storing the pointer atomically, we ensure we always
     read a valid (or once valid) lease. By delaying freeing the lease
     through the garbage collector, we ensure whatever lease update
     occurs in parallel completes before the memory is released. */
  struct gcreq *gcreq = gcreq_new (pp->e.gv->gcreq_queue, ddsi_gc_participant_lease);
  struct lease *lease_old = ddsrt_atomic_ldvoidp (&pp->minl_man);
  assert (lease_old != NULL);
  lease_unregister (lease_old); /* ensures lease will not expire while it is replaced */
  gcreq->arg = lease_old;
  gcreq_enqueue (gcreq);
  ddsrt_atomic_stvoidp (&pp->minl_man, lnew);
}

void ddsi_participant_add_wr_lease_locked (struct ddsi_participant * pp, const struct ddsi_writer * wr)
{
  struct lease *minl_prev;
  struct lease *minl_new;

  assert (wr->lease != NULL);
  minl_prev = ddsrt_fibheap_min (&lease_fhdef_pp, &pp->leaseheap_man);
  ddsrt_fibheap_insert (&lease_fhdef_pp, &pp->leaseheap_man, wr->lease);
  minl_new = ddsrt_fibheap_min (&lease_fhdef_pp, &pp->leaseheap_man);
  /* ensure pp->minl_man is equivalent to min(leaseheap_man) */
  if (minl_prev != minl_new)
  {
    ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed (), minl_new->tdur);
    struct lease *lnew = lease_new (texp, minl_new->tdur, minl_new->entity);
    if (minl_prev == NULL)
    {
      assert (ddsrt_atomic_ldvoidp (&pp->minl_man) == NULL);
      ddsrt_atomic_stvoidp (&pp->minl_man, lnew);
    }
    else
    {
      participant_replace_minl (pp, lnew);
    }
    lease_register (lnew);
  }
}

void ddsi_participant_remove_wr_lease_locked (struct ddsi_participant * pp, struct ddsi_writer * wr)
{
  struct lease *minl_prev;
  struct lease *minl_new;

  assert (wr->lease != NULL);
  assert (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
  minl_prev = ddsrt_fibheap_min (&lease_fhdef_pp, &pp->leaseheap_man);
  ddsrt_fibheap_delete (&lease_fhdef_pp, &pp->leaseheap_man, wr->lease);
  minl_new = ddsrt_fibheap_min (&lease_fhdef_pp, &pp->leaseheap_man);
  /* ensure pp->minl_man is equivalent to min(leaseheap_man) */
  if (minl_prev != minl_new)
  {
    if (minl_new != NULL)
    {
      dds_duration_t trem = minl_new->tdur - minl_prev->tdur;
      assert (trem >= 0);
      ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), trem);
      struct lease *lnew = lease_new (texp, minl_new->tdur, minl_new->entity);
      participant_replace_minl (pp, lnew);
      lease_register (lnew);
    }
    else
    {
      participant_replace_minl (pp, NULL);
    }
  }
}

#ifdef DDS_HAS_SECURITY
static dds_return_t check_and_load_security_config (struct ddsi_domaingv * const gv, const ddsi_guid_t *ppguid, dds_qos_t *qos)
{
  /* If some security properties (name starts with "dds.sec." conform DDS Security spec 7.2.4.1,
     or starts with "org.eclipse.cyclonedds.sec." which we use for Cyclone-specific extensions)
     are present in the QoS, all required properties must be present and they will be used.

     If none are, take the settings from the configuration if it has them.  When no security
     configuration exists anywhere, create an unsecured participant.

     The CRL is a special case: it is optional, but it seems strange to allow an XML file
     specifying a CRL when the QoS properties don't specify one if the CA is the same. It doesn't
     seem like a good idea to compare CAs here, so instead just require a CRL property if the
     XML configuration configures a CRL.

     This may modify "qos" */
  if (ddsi_xqos_has_prop_prefix (qos, DDS_SEC_PROP_PREFIX) || ddsi_xqos_has_prop_prefix (qos, ORG_ECLIPSE_CYCLONEDDS_SEC_PREFIX))
  {
    char const * const req[] = {
      DDS_SEC_PROP_AUTH_IDENTITY_CA,
      DDS_SEC_PROP_AUTH_PRIV_KEY,
      DDS_SEC_PROP_AUTH_IDENTITY_CERT,
      DDS_SEC_PROP_ACCESS_PERMISSIONS_CA,
      DDS_SEC_PROP_ACCESS_GOVERNANCE,
      DDS_SEC_PROP_ACCESS_PERMISSIONS,

      DDS_SEC_PROP_AUTH_LIBRARY_PATH,
      DDS_SEC_PROP_AUTH_LIBRARY_INIT,
      DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE,
      DDS_SEC_PROP_CRYPTO_LIBRARY_PATH,
      DDS_SEC_PROP_CRYPTO_LIBRARY_INIT,
      DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE,
      DDS_SEC_PROP_ACCESS_LIBRARY_PATH,
      DDS_SEC_PROP_ACCESS_LIBRARY_INIT,
      DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE
    };
    GVLOGDISC ("ddsi_new_participant("PGUIDFMT"): using security settings from QoS\n", PGUID (*ppguid));

    /* check if all required security properties exist in qos; report all missing ones, not just the first */
    dds_return_t ret = DDS_RETCODE_OK;
    for (size_t i = 0; i < sizeof(req) / sizeof(req[0]); i++)
    {
      const char *value;
      if (!ddsi_xqos_find_prop (qos, req[i], &value) || strlen (value) == 0)
      {
        GVERROR ("ddsi_new_participant("PGUIDFMT"): required security property %s missing in Property QoS\n", PGUID (*ppguid), req[i]);
        ret = DDS_RETCODE_PRECONDITION_NOT_MET;
      }
    }

    /* deal with CRL: if configured in XML, require the property but allow an explicit empty configuration
       to handle cases where the CA is different and to make it at least possible to override it.  The
       goal is to avoid accidental unsecured participants, not to make things completely impossible. */
    if (gv->config.omg_security_configuration &&
        gv->config.omg_security_configuration->cfg.authentication_properties.crl &&
        *gv->config.omg_security_configuration->cfg.authentication_properties.crl &&
        !ddsi_xqos_find_prop (qos, ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL, NULL))
    {
      GVERROR ("ddsi_new_participant("PGUIDFMT"): CRL security property " ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL " absent in Property QoS but specified in XML configuration\n", PGUID (*ppguid));
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
    }

    if (ret != DDS_RETCODE_OK)
      return ret;
  }
  else if (gv->config.omg_security_configuration)
  {
    /* For security, configuration can be provided through the configuration.  However, the specification
       (and the plugins) expect it to be in the QoS, so merge it in. */
    GVLOGDISC ("ddsi_new_participant("PGUIDFMT"): using security settings from configuration\n", PGUID (*ppguid));
    ddsi_xqos_mergein_security_config (qos, &gv->config.omg_security_configuration->cfg);
  }
  else
  {
    /* No security configuration */
    return DDS_RETCODE_OK;
  }

  if (q_omg_is_security_loaded (gv->security_context))
  {
    GVLOGDISC ("ddsi_new_participant("PGUIDFMT"): security is already loaded for this domain\n", PGUID (*ppguid));
    return DDS_RETCODE_OK;
  }
  else if (q_omg_security_load (gv->security_context, qos, gv) < 0)
  {
    GVERROR ("Could not load security\n");
    return DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
  }
  else
  {
    return DDS_RETCODE_OK;
  }
}
#endif /* DDS_HAS_SECURITY */

static void update_pp_refc (struct ddsi_participant *pp, const struct ddsi_guid *guid_of_refing_entity, int n)
{
  assert (n == -1 || n == 1);
  if (guid_of_refing_entity
      && ddsi_is_builtin_entityid (guid_of_refing_entity->entityid, NN_VENDORID_ECLIPSE)
      && guid_of_refing_entity->entityid.u != NN_ENTITYID_PARTICIPANT)
    pp->builtin_refc += n;
  else
    pp->user_refc += n;
  assert (pp->user_refc >= 0);
  assert (pp->builtin_refc >= 0);
}

static void delete_builtin_endpoint (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, unsigned entityid)
{
  ddsi_guid_t guid;
  guid.prefix = ppguid->prefix;
  guid.entityid.u = entityid;
  assert (ddsi_is_builtin_entityid (ddsi_to_entityid (entityid), NN_VENDORID_ECLIPSE));
  if (ddsi_is_writer_entityid (ddsi_to_entityid (entityid)))
    ddsi_delete_writer_nolinger (gv, &guid);
  else
    (void) ddsi_delete_reader (gv, &guid);
}

struct ddsi_participant *ddsi_ref_participant (struct ddsi_participant *pp, const struct ddsi_guid *guid_of_refing_entity)
{
  ddsrt_mutex_lock (&pp->refc_lock);
  update_pp_refc (pp, guid_of_refing_entity, 1);
  ddsi_guid_t stguid;
  if (guid_of_refing_entity)
    stguid = *guid_of_refing_entity;
  else
    memset (&stguid, 0, sizeof (stguid));
  ELOGDISC (pp, "ddsi_ref_participant("PGUIDFMT" @ %p <- "PGUIDFMT" @ %p) user %"PRId32" builtin %"PRId32"\n",
            PGUID (pp->e.guid), (void*)pp, PGUID (stguid), (void*)guid_of_refing_entity, pp->user_refc, pp->builtin_refc);
  ddsrt_mutex_unlock (&pp->refc_lock);
  return pp;
}

void ddsi_unref_participant (struct ddsi_participant *pp, const struct ddsi_guid *guid_of_refing_entity)
{
  static const unsigned builtin_endpoints_tab[] = {
    NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER,
    NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
#ifdef DDS_HAS_TOPIC_DISCOVERY
    NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER,
#endif
    NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER,
    NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER,
#ifdef DDS_HAS_TYPE_DISCOVERY
    NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER,
    NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER,
    NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER,
    NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER,
#endif
    /* Security ones: */
    NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER,
    NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER,
    NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER,
    NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER,
  };

  ddsrt_mutex_lock (&pp->refc_lock);
  update_pp_refc (pp, guid_of_refing_entity, -1);
  ddsi_guid_t stguid;
  if (guid_of_refing_entity)
    stguid = *guid_of_refing_entity;
  else
    memset (&stguid, 0, sizeof (stguid));
  ELOGDISC (pp, "ddsi_unref_participant("PGUIDFMT" @ %p <- "PGUIDFMT" @ %p) user %"PRId32" builtin %"PRId32"\n",
            PGUID (pp->e.guid), (void*)pp, PGUID (stguid), (void*)guid_of_refing_entity, pp->user_refc, pp->builtin_refc);

  if (pp->user_refc == 0 && pp->bes != 0 && pp->state < DDSI_PARTICIPANT_STATE_DELETING_BUILTINS)
  {
    int i;

    /* The builtin ones are never deleted explicitly by the glue code,
       only implicitly by ddsi_unref_participant, and we need to make sure
       they go at the very end, or else the SEDP disposes and the
       final SPDP message can't be sent.

       If there are no builtins at all, then we must go straight to
       deleting the participant, as ddsi_unref_participant will never be
       called upon deleting a builtin endpoint.

       First we stop the asynchronous SPDP and PMD publication, then
       we send a dispose+unregister message on SPDP (wonder if I ought
       to send a final PMD one as well), then we kill the readers and
       expect us to finally hit the new_refc == 0 to really free this
       participant.

       The conditional execution of some of this is so we can use
       ddsi_unref_participant() for some of the error handling in
       ddsi_new_participant(). Non-existent built-in endpoints can't be
       found in entity_index and are simply ignored. */
    pp->state = DDSI_PARTICIPANT_STATE_DELETING_BUILTINS;
    ddsrt_mutex_unlock (&pp->refc_lock);

    if (pp->spdp_xevent)
      delete_xevent (pp->spdp_xevent);
    if (pp->pmd_update_xevent)
      delete_xevent (pp->pmd_update_xevent);

    /* SPDP relies on the WHC, but dispose-unregister will empty
       it. The event handler verifies the event has already been
       scheduled for deletion when it runs into an empty WHC */
    spdp_dispose_unregister (pp);

    /* If this happens to be the privileged_pp, clear it */
    ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
    if (pp == pp->e.gv->privileged_pp)
      pp->e.gv->privileged_pp = NULL;
    ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);

    for (i = 0; i < (int) (sizeof (builtin_endpoints_tab) / sizeof (builtin_endpoints_tab[0])); i++)
      delete_builtin_endpoint (pp->e.gv, &pp->e.guid, builtin_endpoints_tab[i]);
  }
  else if (pp->user_refc == 0 && pp->builtin_refc == 0)
  {
    ddsrt_mutex_unlock (&pp->refc_lock);

    if (!(pp->e.onlylocal))
    {
      if ((pp->bes & builtin_writers_besmask) != builtin_writers_besmask)
      {
        /* Participant doesn't have a full complement of built-in
           writers, therefore, it relies on gv->privileged_pp, and
           therefore we must decrement the reference count of that one.

           Why read it with the lock held, only to release it and use it
           without any attempt to maintain a consistent state?  We DO
           have a counted reference, so it can't be freed, but there is
           no formal guarantee that the pointer we read is valid unless
           we read it with the lock held.  We can't keep the lock across
           the ddsi_unref_participant, because we may trigger a clean-up of
           it.  */
        struct ddsi_participant *ppp;
        ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
        ppp = pp->e.gv->privileged_pp;
        ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);
        assert (ppp != NULL);
        ddsi_unref_participant (ppp, &pp->e.guid);
      }
    }

    ddsrt_mutex_lock (&pp->e.gv->participant_set_lock);
    assert (pp->e.gv->nparticipants > 0);
    if (--pp->e.gv->nparticipants == 0)
      ddsrt_cond_broadcast (&pp->e.gv->participant_set_cond);
    ddsrt_mutex_unlock (&pp->e.gv->participant_set_lock);
    if (pp->e.gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
    {
      ddsrt_atomic_fence_rel ();
      ddsrt_atomic_inc32 (&pp->e.gv->participant_set_generation);

      /* Deleting the socket will usually suffice to wake up the
         receiver threads, but in general, no one cares if it takes a
         while longer for it to wakeup. */
      ddsi_conn_free (pp->m_conn);
    }
#ifdef DDS_HAS_SECURITY
    q_omg_security_deregister_participant(pp);
#endif
    ddsi_plist_fini (pp->plist);
    ddsrt_free (pp->plist);
    ddsrt_mutex_destroy (&pp->refc_lock);
    ddsi_entity_common_fini (&pp->e);
    ddsi_remove_deleted_participant_guid (pp->e.gv->deleted_participants, &pp->e.guid, DPG_LOCAL);
    inverse_uint32_set_fini(&pp->avail_entityids.x);
    ddsrt_free (pp);
  }
  else
  {
    ddsrt_mutex_unlock (&pp->refc_lock);
  }
}

static dds_return_t new_participant_guid (ddsi_guid_t *ppguid, struct ddsi_domaingv *gv, unsigned flags, const ddsi_plist_t *plist)
{
  struct ddsi_participant *pp;
  ddsi_guid_t subguid, group_guid;
  struct whc_writer_info *wrinfo;
  dds_return_t ret = DDS_RETCODE_OK;
  ddsi_tran_conn_t ppconn;

  /* no reserved bits may be set */
  assert ((flags & ~(RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS | RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP | RTPS_PF_ONLY_LOCAL)) == 0);
  /* privileged participant MUST have builtin readers and writers */
  assert (!(flags & RTPS_PF_PRIVILEGED_PP) || (flags & (RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS)) == 0);

  ddsi_prune_deleted_participant_guids (gv->deleted_participants, ddsrt_time_monotonic ());

  /* FIXME: FULL LOCKING AROUND NEW_XXX FUNCTIONS, JUST SO EXISTENCE TESTS ARE PRECISE */

  /* Participant may not exist yet, but this test is imprecise: if it
     used to exist, but is currently being deleted and we're trying to
     recreate it. */
  if (entidx_lookup_participant_guid (gv->entity_index, ppguid) != NULL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  if (gv->config.many_sockets_mode != DDSI_MSM_MANY_UNICAST)
    ppconn = NULL;
  else
  {
    const ddsi_tran_qos_t qos = { .m_purpose = DDSI_TRAN_QOS_RECV_UC, .m_diffserv = 0, .m_interface = NULL };
    if (ddsi_factory_create_conn (&ppconn, gv->m_factory, 0, &qos) != DDS_RETCODE_OK)
    {
      GVERROR ("ddsi_new_participant("PGUIDFMT", %x) failed: could not create network endpoint\n", PGUID (*ppguid), flags);
      return DDS_RETCODE_OUT_OF_RESOURCES;
    }
  }

  if (gv->config.max_participants == 0)
  {
    ddsrt_mutex_lock (&gv->participant_set_lock);
    ++gv->nparticipants;
    ddsrt_mutex_unlock (&gv->participant_set_lock);
  }
  else
  {
    ddsrt_mutex_lock (&gv->participant_set_lock);
    if (gv->nparticipants < gv->config.max_participants)
    {
      ++gv->nparticipants;
      ddsrt_mutex_unlock (&gv->participant_set_lock);
    }
    else
    {
      ddsrt_mutex_unlock (&gv->participant_set_lock);
      GVERROR ("ddsi_new_participant("PGUIDFMT", %x) failed: max participants reached\n", PGUID (*ppguid), flags);
      if (ppconn)
        ddsi_conn_free (ppconn);
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto new_pp_err;
    }
  }

  GVLOGDISC ("ddsi_new_participant("PGUIDFMT", %x)\n", PGUID (*ppguid), flags);

  pp = ddsrt_malloc (sizeof (*pp));

  ddsi_entity_common_init (&pp->e, gv, ppguid, DDSI_EK_PARTICIPANT, ddsrt_time_wallclock (), NN_VENDORID_ECLIPSE, ((flags & RTPS_PF_ONLY_LOCAL) != 0));
  pp->user_refc = 1;
  pp->builtin_refc = 0;
  pp->state = DDSI_PARTICIPANT_STATE_INITIALIZING;
  pp->is_ddsi2_pp = (flags & (RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP)) ? 1 : 0;
  ddsrt_mutex_init (&pp->refc_lock);
  inverse_uint32_set_init(&pp->avail_entityids.x, 1, UINT32_MAX / NN_ENTITYID_ALLOCSTEP);
  if (plist->present & PP_PARTICIPANT_LEASE_DURATION)
    pp->lease_duration = plist->participant_lease_duration;
  else
    pp->lease_duration = gv->config.lease_duration;
  ddsrt_fibheap_init (&ldur_fhdef, &pp->ldur_auto_wr);
  pp->plist = ddsrt_malloc (sizeof (*pp->plist));
  ddsi_plist_copy (pp->plist, plist);
  ddsi_plist_mergein_missing (pp->plist, &gv->default_local_plist_pp, ~(uint64_t)0, ~(uint64_t)0);

#ifdef DDS_HAS_SECURITY
  pp->sec_attr = NULL;
  if ((ret = check_and_load_security_config (gv, ppguid, &pp->plist->qos)) != DDS_RETCODE_OK)
    goto not_allowed;
  if ((ret = q_omg_security_check_create_participant (pp, gv->config.domainId)) != DDS_RETCODE_OK)
    goto not_allowed;
  *ppguid = pp->e.guid;
  // FIXME: Adjusting the GUID and then fixing up the GUID -> iid mapping here is an ugly hack
  if (pp->e.tk)
  {
    ddsi_tkmap_instance_unref (gv->m_tkmap, pp->e.tk);
    pp->e.tk = builtintopic_get_tkmap_entry (gv->builtin_topic_interface, &pp->e.guid);
    pp->e.iid = pp->e.tk->m_iid;
 }
#else
  if (ddsi_xqos_has_prop_prefix (&pp->plist->qos, "dds.sec."))
  {
    /* disallow creating a participant with a security configuration if there is support for security
       has been left out */
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
    goto not_allowed;
  }
#endif

  if (gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    GVLOGDISC ("PARTICIPANT "PGUIDFMT" QOS={", PGUID (pp->e.guid));
    ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, &pp->plist->qos);
    GVLOGDISC ("}\n");
  }

  pp->m_conn = ppconn;
  if (gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
  {
    assert (pp->m_conn);
    ddsi_conn_locator (pp->m_conn, &pp->m_locator);
  }

  ddsrt_fibheap_init (&lease_fhdef_pp, &pp->leaseheap_man);
  ddsrt_atomic_stvoidp (&pp->minl_man, NULL);

  /* Before we create endpoints -- and may call ddsi_unref_participant if
     things go wrong -- we must initialize all that ddsi_unref_participant
     depends on. */
  pp->spdp_xevent = NULL;
  pp->pmd_update_xevent = NULL;

  /* Create built-in endpoints (note: these have no GID, and no group GUID). */
  pp->bes = 0;
  subguid.prefix = pp->e.guid.prefix;
  memset (&group_guid, 0, sizeof (group_guid));

  /* SPDP is very much special and must be done first */
  if (!(flags & RTPS_PF_NO_BUILTIN_WRITERS))
  {
    subguid.entityid = ddsi_to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->spdp_endpoint_xqos);
    ddsi_new_writer_guid (NULL, &subguid, &group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_NAME, gv->spdp_type, &gv->spdp_endpoint_xqos, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    /* But we need the as_disc address set for SPDP, because we need to
       send it to everyone regardless of the existence of readers. */
    force_as_disc_address (gv, &subguid);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
  }

  /* Make it globally visible, else the endpoint matching won't work. */
  entidx_insert_participant_guid (gv->entity_index, pp);

  /* add all built-in endpoints other than the SPDP writer */
  add_builtin_endpoints (pp, &subguid, &group_guid, gv, !(flags & RTPS_PF_NO_BUILTIN_WRITERS), !(flags & RTPS_PF_NO_BUILTIN_READERS));

  /* If the participant doesn't have the full set of builtin writers
     it depends on the privileged participant, which must exist, hence
     the reference count of the privileged participant is incremented.
     If it is the privileged participant, set the global variable
     pointing to it.
     Except when the participant is only locally available. */
  if (!(flags & RTPS_PF_ONLY_LOCAL))
  {
    ddsrt_mutex_lock (&gv->privileged_pp_lock);
    if ((pp->bes & builtin_writers_besmask) != builtin_writers_besmask)
    {
      /* Simply crash when the privileged participant doesn't exist when
         it is needed.  Its existence is a precondition, and this is not
         a public API */
      assert (gv->privileged_pp != NULL);
      ddsi_ref_participant (gv->privileged_pp, &pp->e.guid);
    }
    if (flags & RTPS_PF_PRIVILEGED_PP)
    {
      /* Crash when two privileged participants are created -- this is
         not a public API. */
      assert (gv->privileged_pp == NULL);
      gv->privileged_pp = pp;
    }
    ddsrt_mutex_unlock (&gv->privileged_pp_lock);
  }

  /* All attributes set, anyone looking for a built-in topic writer can
     now safely do so */
  ddsrt_mutex_lock (&pp->refc_lock);
  pp->state = DDSI_PARTICIPANT_STATE_OPERATIONAL;
  ddsrt_mutex_unlock (&pp->refc_lock);

  /* Signal receive threads if necessary. Must do this after adding it
     to the entity index, or the receive thread won't find the new
     participant */
  if (gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
  {
    ddsrt_atomic_fence ();
    ddsrt_atomic_inc32 (&gv->participant_set_generation);
    trigger_recv_threads (gv);
  }

  builtintopic_write_endpoint (gv->builtin_topic_interface, &pp->e, ddsrt_time_wallclock(), true);

  /* SPDP periodic broadcast uses the retransmit path, so the initial
     publication must be done differently. Must be later than making
     the participant globally visible, or the SPDP processing won't
     recognise the participant as a local one. */
  if (spdp_write (pp) >= 0)
  {
    /* Once the initial sample has been written, the automatic and
       asynchronous broadcasting required by SPDP can start. Also,
       since we're new alive, PMD updates can now start, too.
       Schedule the first update for 100ms in the future to reduce the
       impact of the first sample getting lost.  Note: these two may
       fire before the calls return.  If the initial sample wasn't
       accepted, all is lost, but we continue nonetheless, even though
       the participant won't be able to discover or be discovered.  */
    pp->spdp_xevent = qxev_spdp (gv->xevents, ddsrt_mtime_add_duration (ddsrt_time_monotonic (), DDS_MSECS (100)), &pp->e.guid, NULL);
  }

  {
    ddsrt_mtime_t tsched;
    tsched = (pp->lease_duration == DDS_INFINITY) ? DDSRT_MTIME_NEVER : (ddsrt_mtime_t){0};
    pp->pmd_update_xevent = qxev_pmd_update (gv->xevents, tsched, &pp->e.guid);
  }

#ifdef DDS_HAS_SECURITY
  if (q_omg_participant_is_secure (pp))
  {
    connect_participant_secure (gv, pp);
  }
#endif
  return ret;

not_allowed:
  if (ppconn)
    ddsi_conn_free (ppconn);
  ddsi_plist_fini (pp->plist);
  ddsrt_free (pp->plist);
  inverse_uint32_set_fini (&pp->avail_entityids.x);
  ddsrt_mutex_destroy (&pp->refc_lock);
  ddsi_entity_common_fini (&pp->e);
  ddsrt_free (pp);
  ddsrt_mutex_lock (&gv->participant_set_lock);
  gv->nparticipants--;
  ddsrt_mutex_unlock (&gv->participant_set_lock);
new_pp_err:
  return ret;
}

dds_return_t ddsi_new_participant (ddsi_guid_t *p_ppguid, struct ddsi_domaingv *gv, unsigned flags, const ddsi_plist_t *plist)
{
  union { uint64_t u64; uint32_t u32[2]; } u;
  u.u32[0] = gv->ppguid_base.prefix.u[1];
  u.u32[1] = gv->ppguid_base.prefix.u[2];
  u.u64 += ddsi_iid_gen ();
  p_ppguid->prefix.u[0] = gv->ppguid_base.prefix.u[0];
  p_ppguid->prefix.u[1] = u.u32[0];
  p_ppguid->prefix.u[2] = u.u32[1];
  p_ppguid->entityid.u = NN_ENTITYID_PARTICIPANT;
  return new_participant_guid (p_ppguid, gv, flags, plist);
}

void ddsi_update_participant_plist (struct ddsi_participant *pp, const ddsi_plist_t *plist)
{
  ddsrt_mutex_lock (&pp->e.lock);
  if (ddsi_update_qos_locked (&pp->e, &pp->plist->qos, &plist->qos, ddsrt_time_wallclock ()))
    spdp_write (pp);
  ddsrt_mutex_unlock (&pp->e.lock);
}

static void gc_delete_participant (struct gcreq *gcreq)
{
  struct ddsi_participant *pp = gcreq->arg;
  ELOGDISC (pp, "gc_delete_participant (%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pp->e.guid));
  gcreq_free (gcreq);
  ddsi_unref_participant (pp, NULL);
}

static int gcreq_participant (struct ddsi_participant *pp)
{
  struct gcreq *gcreq = gcreq_new (pp->e.gv->gcreq_queue, gc_delete_participant);
  gcreq->arg = pp;
  gcreq_enqueue (gcreq);
  return 0;
}

dds_return_t ddsi_delete_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid)
{
  struct ddsi_participant *pp;
  GVLOGDISC ("ddsi_delete_participant ("PGUIDFMT")\n", PGUID (*ppguid));
  ddsrt_mutex_lock (&gv->lock);
  if ((pp = entidx_lookup_participant_guid (gv->entity_index, ppguid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  builtintopic_write_endpoint (gv->builtin_topic_interface, &pp->e, ddsrt_time_wallclock(), false);
  ddsi_remember_deleted_participant_guid (gv->deleted_participants, &pp->e.guid);
#ifdef DDS_HAS_SECURITY
  disconnect_participant_secure (pp);
#endif
  ddsrt_mutex_lock (&pp->refc_lock);
  pp->state = DDSI_PARTICIPANT_STATE_DELETE_STARTED;
  ddsrt_mutex_unlock (&pp->refc_lock);
  entidx_remove_participant_guid (gv->entity_index, pp);
  ddsrt_mutex_unlock (&gv->lock);
  gcreq_participant (pp);
  return 0;
}

struct ddsi_writer *ddsi_get_builtin_writer (const struct ddsi_participant *pp, unsigned entityid)
{
  ddsi_guid_t bwr_guid;
  uint32_t bes_mask = 0;

  if (pp->e.onlylocal) {
      return NULL;
  }

  /* If the participant the required built-in writer, we use it.  We
     check by inspecting the "built-in endpoint set" advertised by the
     participant, which is a constant. */
  switch (entityid)
  {
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_ANNOUNCER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
      break;
#ifdef DDS_HAS_TYPE_DISCOVERY
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_WRITER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER:
      bes_mask = NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_READER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_WRITER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER:
      bes_mask = NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_READER;
      break;
#endif
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER;
      break;
#endif
    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_ANNOUNCER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_ANNOUNCER;
      break;
    default:
      DDS_FATAL ("get_builtin_writer called with entityid %x\n", entityid);
      return NULL;
  }

  if (pp->bes & bes_mask)
  {
    /* Participant has this SEDP writer => use it. */
    bwr_guid.prefix = pp->e.guid.prefix;
    bwr_guid.entityid.u = entityid;
  }
  else
  {
    /* Must have a designated participant to use -- that is, before
       any application readers and writers may be created (indeed,
       before any PMD message may go out), one participant must be
       created with the built-in writers, and this participant then
       automatically becomes the designated participant.  Woe betide
       who deletes it early!  Lock's not really needed but provides
       the memory barriers that guarantee visibility of the correct
       value of privileged_pp. */
    ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
    assert (pp->e.gv->privileged_pp != NULL);
    bwr_guid.prefix = pp->e.gv->privileged_pp->e.guid.prefix;
    ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);
    bwr_guid.entityid.u = entityid;
  }

  return entidx_lookup_writer_guid (pp->e.gv->entity_index, &bwr_guid);
}

dds_duration_t ddsi_participant_get_pmd_interval (struct ddsi_participant *pp)
{
  struct ldur_fhnode *ldur_node;
  dds_duration_t intv;
  ddsrt_mutex_lock (&pp->e.lock);
  ldur_node = ddsrt_fibheap_min (&ldur_fhdef, &pp->ldur_auto_wr);
  intv = (ldur_node != NULL) ? ldur_node->ldur : DDS_INFINITY;
  if (pp->lease_duration < intv)
    intv = pp->lease_duration;
  ddsrt_mutex_unlock (&pp->e.lock);
  return intv;
}
