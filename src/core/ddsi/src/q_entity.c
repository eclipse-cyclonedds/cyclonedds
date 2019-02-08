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
#include <stddef.h>

#include "os/os.h"


#include "ddsi/q_entity.h"
#include "ddsi/q_config.h"
#include "ddsi/q_time.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_log.h"
#include "util/ut_avl.h"
#include "ddsi/q_plist.h"
#include "ddsi/q_lease.h"
#include "ddsi/q_qosmatch.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_xevent.h" /* qxev_spdp, &c. */
#include "ddsi/q_ddsi_discovery.h" /* spdp_write, &c. */
#include "ddsi/q_gc.h"
#include "ddsi/q_radmin.h"
#include "ddsi/q_protocol.h" /* NN_ENTITYID_... */
#include "ddsi/q_unused.h"
#include "ddsi/q_error.h"
#include "ddsi/ddsi_serdata_default.h"
#include "ddsi/ddsi_mcgroup.h"
#include "ddsi/q_receive.h"

#include "ddsi/sysdeps.h"
#include "dds__whc.h"
#include "ddsi/ddsi_iid.h"
#include "ddsi/ddsi_tkmap.h"

struct deleted_participant {
  ut_avlNode_t avlnode;
  nn_guid_t guid;
  unsigned for_what;
  nn_mtime_t t_prune;
};

static os_mutex deleted_participants_lock;
static ut_avlTree_t deleted_participants;

static int compare_guid (const void *va, const void *vb);
static void augment_wr_prd_match (void *vnode, const void *vleft, const void *vright);

const ut_avlTreedef_t wr_readers_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct wr_prd_match, avlnode), offsetof (struct wr_prd_match, prd_guid), compare_guid, augment_wr_prd_match);
const ut_avlTreedef_t wr_local_readers_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct wr_rd_match, avlnode), offsetof (struct wr_rd_match, rd_guid), compare_guid, 0);
const ut_avlTreedef_t rd_writers_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct rd_pwr_match, avlnode), offsetof (struct rd_pwr_match, pwr_guid), compare_guid, 0);
const ut_avlTreedef_t rd_local_writers_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct rd_wr_match, avlnode), offsetof (struct rd_wr_match, wr_guid), compare_guid, 0);
const ut_avlTreedef_t pwr_readers_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct pwr_rd_match, avlnode), offsetof (struct pwr_rd_match, rd_guid), compare_guid, 0);
const ut_avlTreedef_t prd_writers_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct prd_wr_match, avlnode), offsetof (struct prd_wr_match, wr_guid), compare_guid, 0);
const ut_avlTreedef_t deleted_participants_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct deleted_participant, avlnode), offsetof (struct deleted_participant, guid), compare_guid, 0);
const ut_avlTreedef_t proxypp_groups_treedef =
  UT_AVL_TREEDEF_INITIALIZER (offsetof (struct proxy_group, avlnode), offsetof (struct proxy_group, guid), compare_guid, 0);

static const unsigned builtin_writers_besmask =
  NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER |
  NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
  NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER |
  NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
static const unsigned prismtech_builtin_writers_besmask =
  NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER |
  NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER |
  NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;

static struct writer * new_writer_guid (const struct nn_guid *guid, const struct nn_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc *whc, status_cb_t status_cb, void *status_cbarg);
static struct reader * new_reader_guid (const struct nn_guid *guid, const struct nn_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct rhc *rhc, status_cb_t status_cb, void *status_cbarg);
static struct participant *ref_participant (struct participant *pp, const struct nn_guid *guid_of_refing_entity);
static void unref_participant (struct participant *pp, const struct nn_guid *guid_of_refing_entity);
static void delete_proxy_group_locked (struct proxy_group *pgroup, nn_wctime_t timestamp, int isimplicit);

static int gcreq_participant (struct participant *pp);
static int gcreq_writer (struct writer *wr);
static int gcreq_reader (struct reader *rd);
static int gcreq_proxy_participant (struct proxy_participant *proxypp);
static int gcreq_proxy_writer (struct proxy_writer *pwr);
static int gcreq_proxy_reader (struct proxy_reader *prd);

static int compare_guid (const void *va, const void *vb)
{
  return memcmp (va, vb, sizeof (nn_guid_t));
}

nn_entityid_t to_entityid (unsigned u)
{
  nn_entityid_t e;
  e.u = u;
  return e;
}

int is_writer_entityid (nn_entityid_t id)
{
  switch (id.u & NN_ENTITYID_KIND_MASK)
  {
    case NN_ENTITYID_KIND_WRITER_WITH_KEY:
    case NN_ENTITYID_KIND_WRITER_NO_KEY:
      return 1;
    default:
      return 0;
  }
}

int is_reader_entityid (nn_entityid_t id)
{
  switch (id.u & NN_ENTITYID_KIND_MASK)
  {
    case NN_ENTITYID_KIND_READER_WITH_KEY:
    case NN_ENTITYID_KIND_READER_NO_KEY:
      return 1;
    default:
      return 0;
  }
}

int is_builtin_entityid (nn_entityid_t id, nn_vendorid_t vendorid)
{
  if ((id.u & NN_ENTITYID_SOURCE_MASK) == NN_ENTITYID_SOURCE_BUILTIN)
    return 1;
  else if ((id.u & NN_ENTITYID_SOURCE_MASK) != NN_ENTITYID_SOURCE_VENDOR)
    return 0;
  else if (!vendor_is_eclipse_or_prismtech (vendorid))
    return 0;
  else
  {
    /* Currently only SOURCE_VENDOR entities are for CM "topics". */
    return 1;
  }
}

int is_builtin_endpoint (nn_entityid_t id, nn_vendorid_t vendorid)
{
  return is_builtin_entityid (id, vendorid) && id.u != NN_ENTITYID_PARTICIPANT;
}

bool is_local_orphan_endpoint (const struct entity_common *e)
{
  return (e->guid.prefix.u[0] == 0 && e->guid.prefix.u[1] == 0 && e->guid.prefix.u[2] == 0 &&
          is_builtin_endpoint (e->guid.entityid, NN_VENDORID_ECLIPSE));
}

static void entity_common_init (struct entity_common *e, const struct nn_guid *guid, const char *name, enum entity_kind kind, nn_wctime_t tcreate, nn_vendorid_t vendorid, bool onlylocal)
{
  e->guid = *guid;
  e->kind = kind;
  e->tupdate = tcreate;
  e->name = os_strdup (name ? name : "");
  e->onlylocal = onlylocal;
  os_mutexInit (&e->lock);
  if (ddsi_plugin.builtintopic_is_visible (guid->entityid, onlylocal, vendorid))
  {
    e->tk = ddsi_plugin.builtintopic_get_tkmap_entry (guid);
    e->iid = e->tk->m_iid;
  }
  else
  {
    e->tk = NULL;
    e->iid = ddsi_iid_gen ();
  }
}

static void entity_common_fini (struct entity_common *e)
{
  if (e->tk)
    ddsi_tkmap_instance_unref (e->tk);
  os_free (e->name);
  os_mutexDestroy (&e->lock);
}

void local_reader_ary_init (struct local_reader_ary *x)
{
  os_mutexInit (&x->rdary_lock);
  x->valid = 1;
  x->fastpath_ok = 1;
  x->n_readers = 0;
  x->rdary = os_malloc (sizeof (*x->rdary));
  x->rdary[0] = NULL;
}

void local_reader_ary_fini (struct local_reader_ary *x)
{
  os_free (x->rdary);
  os_mutexDestroy (&x->rdary_lock);
}

void local_reader_ary_insert (struct local_reader_ary *x, struct reader *rd)
{
  os_mutexLock (&x->rdary_lock);
  x->n_readers++;
  x->rdary = os_realloc (x->rdary, (x->n_readers + 1) * sizeof (*x->rdary));
  x->rdary[x->n_readers - 1] = rd;
  x->rdary[x->n_readers] = NULL;
  os_mutexUnlock (&x->rdary_lock);
}

void local_reader_ary_remove (struct local_reader_ary *x, struct reader *rd)
{
  unsigned i;
  os_mutexLock (&x->rdary_lock);
  for (i = 0; i < x->n_readers; i++)
  {
    if (x->rdary[i] == rd)
      break;
  }
  assert (i < x->n_readers);
  /* if i == N-1 copy is a no-op */
  x->rdary[i] = x->rdary[x->n_readers-1];
  x->n_readers--;
  x->rdary[x->n_readers] = NULL;
  x->rdary = os_realloc (x->rdary, (x->n_readers + 1) * sizeof (*x->rdary));
  os_mutexUnlock (&x->rdary_lock);
}

void local_reader_ary_setinvalid (struct local_reader_ary *x)
{
  os_mutexLock (&x->rdary_lock);
  x->valid = 0;
  x->fastpath_ok = 0;
  os_mutexUnlock (&x->rdary_lock);
}

nn_vendorid_t get_entity_vendorid (const struct entity_common *e)
{
  switch (e->kind)
  {
    case EK_PARTICIPANT:
    case EK_READER:
    case EK_WRITER:
      return NN_VENDORID_ECLIPSE;
    case EK_PROXY_PARTICIPANT:
      return ((const struct proxy_participant *) e)->vendor;
    case EK_PROXY_READER:
      return ((const struct proxy_reader *) e)->c.vendor;
    case EK_PROXY_WRITER:
      return ((const struct proxy_writer *) e)->c.vendor;
  }
  assert (0);
  return NN_VENDORID_UNKNOWN;
}

/* DELETED PARTICIPANTS --------------------------------------------- */

int deleted_participants_admin_init (void)
{
  os_mutexInit (&deleted_participants_lock);
  ut_avlInit (&deleted_participants_treedef, &deleted_participants);
  return 0;
}

void deleted_participants_admin_fini (void)
{
  ut_avlFree (&deleted_participants_treedef, &deleted_participants, os_free);
  os_mutexDestroy (&deleted_participants_lock);
}

static void prune_deleted_participant_guids_unlocked (nn_mtime_t tnow)
{
  /* Could do a better job of finding prunable ones efficiently under
     all circumstances, but I expect the tree to be very small at all
     times, so a full scan is fine, too ... */
  struct deleted_participant *dpp;
  dpp = ut_avlFindMin (&deleted_participants_treedef, &deleted_participants);
  while (dpp)
  {
    struct deleted_participant *dpp1 = ut_avlFindSucc (&deleted_participants_treedef, &deleted_participants, dpp);
    if (dpp->t_prune.v < tnow.v)
    {
      ut_avlDelete (&deleted_participants_treedef, &deleted_participants, dpp);
      os_free (dpp);
    }
    dpp = dpp1;
  }
}

static void prune_deleted_participant_guids (nn_mtime_t tnow)
{
  os_mutexLock (&deleted_participants_lock);
  prune_deleted_participant_guids_unlocked (tnow);
  os_mutexUnlock (&deleted_participants_lock);
}

static void remember_deleted_participant_guid (const struct nn_guid *guid)
{
  struct deleted_participant *n;
  ut_avlIPath_t path;
  os_mutexLock (&deleted_participants_lock);
  if (ut_avlLookupIPath (&deleted_participants_treedef, &deleted_participants, guid, &path) == NULL)
  {
    if ((n = os_malloc (sizeof (*n))) != NULL)
    {
      n->guid = *guid;
      n->t_prune.v = T_NEVER;
      n->for_what = DPG_LOCAL | DPG_REMOTE;
      ut_avlInsertIPath (&deleted_participants_treedef, &deleted_participants, n, &path);
    }
  }
  os_mutexUnlock (&deleted_participants_lock);
}

int is_deleted_participant_guid (const struct nn_guid *guid, unsigned for_what)
{
  struct deleted_participant *n;
  int known;
  os_mutexLock (&deleted_participants_lock);
  prune_deleted_participant_guids_unlocked (now_mt());
  if ((n = ut_avlLookup (&deleted_participants_treedef, &deleted_participants, guid)) == NULL)
    known = 0;
  else
    known = ((n->for_what & for_what) != 0);
  os_mutexUnlock (&deleted_participants_lock);
  return known;
}

static void remove_deleted_participant_guid (const struct nn_guid *guid, unsigned for_what)
{
  struct deleted_participant *n;
  DDS_LOG(DDS_LC_DISCOVERY, "remove_deleted_participant_guid(%x:%x:%x:%x for_what=%x)\n", PGUID (*guid), for_what);
  os_mutexLock (&deleted_participants_lock);
  if ((n = ut_avlLookup (&deleted_participants_treedef, &deleted_participants, guid)) != NULL)
  {
    if (config.prune_deleted_ppant.enforce_delay)
    {
      n->t_prune = add_duration_to_mtime (now_mt (), config.prune_deleted_ppant.delay);
    }
    else
    {
      n->for_what &= ~for_what;
      if (n->for_what != 0)
      {
        /* For local participants (remove called with LOCAL, leaving
         REMOTE blacklisted, and has to do with network briding) */
        n->t_prune = add_duration_to_mtime (now_mt (), config.prune_deleted_ppant.delay);
      }
      else
      {
        ut_avlDelete (&deleted_participants_treedef, &deleted_participants, n);
        os_free (n);
      }
    }
  }
  os_mutexUnlock (&deleted_participants_lock);
}


/* PARTICIPANT ------------------------------------------------------ */

int pp_allocate_entityid(nn_entityid_t *id, unsigned kind, struct participant *pp)
{
  uint32_t id1;
  int ret = 0;
  os_mutexLock (&pp->e.lock);
  if (inverse_uint32_set_alloc(&id1, &pp->avail_entityids.x))
  {
    *id = to_entityid (id1 * NN_ENTITYID_ALLOCSTEP + kind);
    ret = 0;
  }
  else
  {
    DDS_ERROR("pp_allocate_entityid(%x:%x:%x:%x): all ids in use\n", PGUID(pp->e.guid));
    ret = ERR_OUT_OF_IDS;
  }
  os_mutexUnlock (&pp->e.lock);
  return ret;
}

void pp_release_entityid(struct participant *pp, nn_entityid_t id)
{
  os_mutexLock (&pp->e.lock);
  inverse_uint32_set_free(&pp->avail_entityids.x, id.u / NN_ENTITYID_ALLOCSTEP);
  os_mutexUnlock (&pp->e.lock);
}

int new_participant_guid (const nn_guid_t *ppguid, unsigned flags, const nn_plist_t *plist)
{
  struct participant *pp;
  nn_guid_t subguid, group_guid;

  /* no reserved bits may be set */
  assert ((flags & ~(RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS | RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP | RTPS_PF_ONLY_LOCAL)) == 0);
  /* privileged participant MUST have builtin readers and writers */
  assert (!(flags & RTPS_PF_PRIVILEGED_PP) || (flags & (RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS)) == 0);

  prune_deleted_participant_guids (now_mt ());

  /* FIXME: FULL LOCKING AROUND NEW_XXX FUNCTIONS, JUST SO EXISTENCE TESTS ARE PRECISE */

  /* Participant may not exist yet, but this test is imprecise: if it
     used to exist, but is currently being deleted and we're trying to
     recreate it. */
  if (ephash_lookup_participant_guid (ppguid) != NULL)
    return ERR_ENTITY_EXISTS;

  if (config.max_participants == 0)
  {
    os_mutexLock (&gv.participant_set_lock);
    ++gv.nparticipants;
    os_mutexUnlock (&gv.participant_set_lock);
  }
  else
  {
    os_mutexLock (&gv.participant_set_lock);
    if (gv.nparticipants < config.max_participants)
    {
      ++gv.nparticipants;
      os_mutexUnlock (&gv.participant_set_lock);
    }
    else
    {
      os_mutexUnlock (&gv.participant_set_lock);
      DDS_ERROR("new_participant(%x:%x:%x:%x, %x) failed: max participants reached\n", PGUID (*ppguid), flags);
      return ERR_OUT_OF_IDS;
    }
  }

  DDS_LOG(DDS_LC_DISCOVERY, "new_participant(%x:%x:%x:%x, %x)\n", PGUID (*ppguid), flags);

  pp = os_malloc (sizeof (*pp));

  entity_common_init (&pp->e, ppguid, "", EK_PARTICIPANT, now (), NN_VENDORID_ECLIPSE, ((flags & RTPS_PF_ONLY_LOCAL) != 0));
  pp->user_refc = 1;
  pp->builtin_refc = 0;
  pp->builtins_deleted = 0;
  pp->is_ddsi2_pp = (flags & (RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP)) ? 1 : 0;
  os_mutexInit (&pp->refc_lock);
  inverse_uint32_set_init(&pp->avail_entityids.x, 1, UINT32_MAX / NN_ENTITYID_ALLOCSTEP);
  pp->lease_duration = config.lease_duration;
  pp->plist = os_malloc (sizeof (*pp->plist));
  nn_plist_copy (pp->plist, plist);
  nn_plist_mergein_missing (pp->plist, &gv.default_plist_pp);

  if (dds_get_log_mask() & DDS_LC_DISCOVERY)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "PARTICIPANT %x:%x:%x:%x QOS={", PGUID (pp->e.guid));
    nn_log_xqos(DDS_LC_DISCOVERY, &pp->plist->qos);
    DDS_LOG(DDS_LC_DISCOVERY, "}\n");
  }

  if (config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    pp->m_conn = ddsi_factory_create_conn (gv.m_factory, 0, NULL);
    ddsi_conn_locator (pp->m_conn, &pp->m_locator);
  }
  else
  {
    pp->m_conn = NULL;
  }

  /* Before we create endpoints -- and may call unref_participant if
     things go wrong -- we must initialize all that unref_participant
     depends on. */
  pp->spdp_xevent = NULL;
  pp->pmd_update_xevent = NULL;

  /* Create built-in endpoints (note: these have no GID, and no group GUID). */
  pp->bes = 0;
  pp->prismtech_bes = 0;
  subguid.prefix = pp->e.guid.prefix;
  memset (&group_guid, 0, sizeof (group_guid));
  /* SPDP writer */
#define LAST_WR_PARAMS NULL, NULL

  /* Note: skip SEDP <=> skip SPDP because of the way ddsi_discovery.c does things
     currently.  */
  if (!(flags & RTPS_PF_NO_BUILTIN_WRITERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.spdp_endpoint_xqos, whc_new(1, 1, 1), LAST_WR_PARAMS);
    /* But we need the as_disc address set for SPDP, because we need to
       send it to everyone regardless of the existence of readers. */
    {
      struct writer *wr = ephash_lookup_writer_guid (&subguid);
      assert (wr != NULL);
      os_mutexLock (&wr->e.lock);
      unref_addrset (wr->as);
      unref_addrset (wr->as_group);
      wr->as = ref_addrset (gv.as_disc);
      wr->as_group = ref_addrset (gv.as_disc_group);
      os_mutexUnlock (&wr->e.lock);
    }
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
  }

  /* Make it globally visible, else the endpoint matching won't work. */
  ephash_insert_participant_guid (pp);

  /* SEDP writers: */
  if (!(flags & RTPS_PF_NO_BUILTIN_WRITERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;
  }

  if (config.do_topic_discovery)
  {
    /* TODO: make this one configurable, we don't want all participants to publish all topics (or even just those that they use themselves) */
    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_TOPIC_ANNOUNCER;
  }

  /* PMD writer: */
  if (!(flags & RTPS_PF_NO_BUILTIN_WRITERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER);
    new_writer_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_wr, whc_new(1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
  }

  /* SPDP, SEDP, PMD readers: */
  if (!(flags & RTPS_PF_NO_BUILTIN_READERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.spdp_endpoint_xqos, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;

    subguid.entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_READER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_READER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER);
    new_reader_guid (&subguid, &group_guid, pp, NULL, &gv.builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_READER;

  }
#undef LAST_WR_PARAMS

  /* If the participant doesn't have the full set of builtin writers
     it depends on the privileged participant, which must exist, hence
     the reference count of the privileged participant is incremented.
     If it is the privileged participant, set the global variable
     pointing to it.
     Except when the participant is only locally available. */
  if (!(flags & RTPS_PF_ONLY_LOCAL)) {
    os_mutexLock (&gv.privileged_pp_lock);
    if ((pp->bes & builtin_writers_besmask) != builtin_writers_besmask ||
        (pp->prismtech_bes & prismtech_builtin_writers_besmask) != prismtech_builtin_writers_besmask)
    {
      /* Simply crash when the privileged participant doesn't exist when
         it is needed.  Its existence is a precondition, and this is not
         a public API */
      assert (gv.privileged_pp != NULL);
      ref_participant (gv.privileged_pp, &pp->e.guid);
    }
    if (flags & RTPS_PF_PRIVILEGED_PP)
    {
      /* Crash when two privileged participants are created -- this is
         not a public API. */
      assert (gv.privileged_pp == NULL);
      gv.privileged_pp = pp;
    }
    os_mutexUnlock (&gv.privileged_pp_lock);
  }

  /* Make it globally visible, then signal receive threads if
     necessary. Must do in this order, or the receive thread won't
     find the new participant */

  if (config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    os_atomic_fence ();
    os_atomic_inc32 (&gv.participant_set_generation);
    trigger_recv_threads ();
  }

  ddsi_plugin.builtintopic_write (&pp->e, now(), true);

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
    pp->spdp_xevent = qxev_spdp (add_duration_to_mtime (now_mt (), 100 * T_MILLISECOND), &pp->e.guid, NULL);
  }

  /* Also write the CM data - this one being transient local, we only
   need to write it once (or when it changes, I suppose) */
  sedp_write_cm_participant (pp, 1);

  {
    nn_mtime_t tsched;
    tsched.v = (pp->lease_duration == T_NEVER) ? T_NEVER : 0;
    pp->pmd_update_xevent = qxev_pmd_update (tsched, &pp->e.guid);
  }
  return 0;
}

int new_participant (nn_guid_t *p_ppguid, unsigned flags, const nn_plist_t *plist)
{
  nn_guid_t ppguid;

  os_mutexLock (&gv.privileged_pp_lock);
  ppguid = gv.next_ppguid;
  if (gv.next_ppguid.prefix.u[2]++ == ~0u)
  {
    os_mutexUnlock (&gv.privileged_pp_lock);
    return ERR_OUT_OF_IDS;
  }
  os_mutexUnlock (&gv.privileged_pp_lock);
  *p_ppguid = ppguid;

  return new_participant_guid (p_ppguid, flags, plist);
}

static void delete_builtin_endpoint (const struct nn_guid *ppguid, unsigned entityid)
{
  nn_guid_t guid;
  guid.prefix = ppguid->prefix;
  guid.entityid.u = entityid;
  assert (is_builtin_entityid (to_entityid (entityid), NN_VENDORID_ECLIPSE));
  if (is_writer_entityid (to_entityid (entityid)))
    delete_writer_nolinger (&guid);
  else
    (void)delete_reader (&guid);
}

static struct participant *ref_participant (struct participant *pp, const struct nn_guid *guid_of_refing_entity)
{
  nn_guid_t stguid;
  os_mutexLock (&pp->refc_lock);
  if (guid_of_refing_entity && is_builtin_endpoint (guid_of_refing_entity->entityid, NN_VENDORID_ECLIPSE))
    pp->builtin_refc++;
  else
    pp->user_refc++;

  if (guid_of_refing_entity)
    stguid = *guid_of_refing_entity;
  else
    memset (&stguid, 0, sizeof (stguid));
  DDS_LOG(DDS_LC_DISCOVERY, "ref_participant(%x:%x:%x:%x @ %p <- %x:%x:%x:%x @ %p) user %d builtin %d\n",
          PGUID (pp->e.guid), (void*)pp, PGUID (stguid), (void*)guid_of_refing_entity, pp->user_refc, pp->builtin_refc);
  os_mutexUnlock (&pp->refc_lock);
  return pp;
}

static void unref_participant (struct participant *pp, const struct nn_guid *guid_of_refing_entity)
{
  static const unsigned builtin_endpoints_tab[] = {
    NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER,
    NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER,
    NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER,
    NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER,
    NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER,
    NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER,
    /* PrismTech ones: */
    NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER,
    NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER,
    NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER
  };
  nn_guid_t stguid;

  os_mutexLock (&pp->refc_lock);
  if (guid_of_refing_entity && is_builtin_endpoint (guid_of_refing_entity->entityid, NN_VENDORID_ECLIPSE))
    pp->builtin_refc--;
  else
    pp->user_refc--;
  assert (pp->user_refc >= 0);
  assert (pp->builtin_refc >= 0);

  if (guid_of_refing_entity)
    stguid = *guid_of_refing_entity;
  else
    memset (&stguid, 0, sizeof (stguid));
  DDS_LOG(DDS_LC_DISCOVERY, "unref_participant(%x:%x:%x:%x @ %p <- %x:%x:%x:%x @ %p) user %d builtin %d\n",
          PGUID (pp->e.guid), (void*)pp, PGUID (stguid), (void*)guid_of_refing_entity, pp->user_refc, pp->builtin_refc);

  if (pp->user_refc == 0 && (pp->bes != 0 || pp->prismtech_bes != 0) && !pp->builtins_deleted)
  {
    int i;

    /* The builtin ones are never deleted explicitly by the glue code,
       only implicitly by unref_participant, and we need to make sure
       they go at the very end, or else the SEDP disposes and the
       final SPDP message can't be sent.

       If there are no builtins at all, then we must go straight to
       deleting the participant, as unref_participant will never be
       called upon deleting a builtin endpoint.

       First we stop the asynchronous SPDP and PMD publication, then
       we send a dispose+unregister message on SPDP (wonder if I ought
       to send a final PMD one as well), then we kill the readers and
       expect us to finally hit the new_refc == 0 to really free this
       participant.

       The conditional execution of some of this is so we can use
       unref_participant() for some of the error handling in
       new_participant(). Non-existent built-in endpoints can't be
       found in guid_hash and are simply ignored. */
    pp->builtins_deleted = 1;
    os_mutexUnlock (&pp->refc_lock);

    if (pp->spdp_xevent)
      delete_xevent (pp->spdp_xevent);
    if (pp->pmd_update_xevent)
      delete_xevent (pp->pmd_update_xevent);

    /* SPDP relies on the WHC, but dispose-unregister will empty
       it. The event handler verifies the event has already been
       scheduled for deletion when it runs into an empty WHC */
    spdp_dispose_unregister (pp);

    /* We don't care, but other implementations might: */
    sedp_write_cm_participant (pp, 0);

    /* If this happens to be the privileged_pp, clear it */
    os_mutexLock (&gv.privileged_pp_lock);
    if (pp == gv.privileged_pp)
      gv.privileged_pp = NULL;
    os_mutexUnlock (&gv.privileged_pp_lock);

    for (i = 0; i < (int) (sizeof (builtin_endpoints_tab) / sizeof (builtin_endpoints_tab[0])); i++)
      delete_builtin_endpoint (&pp->e.guid, builtin_endpoints_tab[i]);
  }
  else if (pp->user_refc == 0 && pp->builtin_refc == 0)
  {
    os_mutexUnlock (&pp->refc_lock);

    if (!(pp->e.onlylocal))
    {
      if ((pp->bes & builtin_writers_besmask) != builtin_writers_besmask ||
          (pp->prismtech_bes & prismtech_builtin_writers_besmask) != prismtech_builtin_writers_besmask)
      {
        /* Participant doesn't have a full complement of built-in
           writers, therefore, it relies on gv.privileged_pp, and
           therefore we must decrement the reference count of that one.

           Why read it with the lock held, only to release it and use it
           without any attempt to maintain a consistent state?  We DO
           have a counted reference, so it can't be freed, but there is
           no formal guarantee that the pointer we read is valid unless
           we read it with the lock held.  We can't keep the lock across
           the unref_participant, because we may trigger a clean-up of
           it.  */
        struct participant *ppp;
        os_mutexLock (&gv.privileged_pp_lock);
        ppp = gv.privileged_pp;
        os_mutexUnlock (&gv.privileged_pp_lock);
        assert (ppp != NULL);
        unref_participant (ppp, &pp->e.guid);
      }
    }

    os_mutexLock (&gv.participant_set_lock);
    assert (gv.nparticipants > 0);
    if (--gv.nparticipants == 0)
      os_condBroadcast (&gv.participant_set_cond);
    os_mutexUnlock (&gv.participant_set_lock);
    if (config.many_sockets_mode == MSM_MANY_UNICAST)
    {
      os_atomic_fence_rel ();
      os_atomic_inc32 (&gv.participant_set_generation);

      /* Deleting the socket will usually suffice to wake up the
         receiver threads, but in general, no one cares if it takes a
         while longer for it to wakeup. */
      ddsi_conn_free (pp->m_conn);
    }
    nn_plist_fini (pp->plist);
    os_free (pp->plist);
    os_mutexDestroy (&pp->refc_lock);
    entity_common_fini (&pp->e);
    remove_deleted_participant_guid (&pp->e.guid, DPG_LOCAL);
    inverse_uint32_set_fini(&pp->avail_entityids.x);
    os_free (pp);
  }
  else
  {
    os_mutexUnlock (&pp->refc_lock);
  }
}

static void gc_delete_participant (struct gcreq *gcreq)
{
  struct participant *pp = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_participant(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (pp->e.guid));
  gcreq_free (gcreq);
  unref_participant (pp, NULL);
}

int delete_participant (const struct nn_guid *ppguid)
{
  struct participant *pp;
  if ((pp = ephash_lookup_participant_guid (ppguid)) == NULL)
    return ERR_UNKNOWN_ENTITY;
  ddsi_plugin.builtintopic_write (&pp->e, now(), false);
  remember_deleted_participant_guid (&pp->e.guid);
  ephash_remove_participant_guid (pp);
  gcreq_participant (pp);
  return 0;
}

struct writer *get_builtin_writer (const struct participant *pp, unsigned entityid)
{
  nn_guid_t bwr_guid;
  unsigned bes_mask = 0, prismtech_bes_mask = 0;

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
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      bes_mask = NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
      prismtech_bes_mask = NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
      prismtech_bes_mask = NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
      prismtech_bes_mask = NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      bes_mask = NN_DISC_BUILTIN_ENDPOINT_TOPIC_ANNOUNCER;
      break;
    default:
      DDS_FATAL ("get_builtin_writer called with entityid %x\n", entityid);
      return NULL;
  }

  if ((pp->bes & bes_mask) || (pp->prismtech_bes & prismtech_bes_mask))
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
    os_mutexLock (&gv.privileged_pp_lock);
    assert (gv.privileged_pp != NULL);
    bwr_guid.prefix = gv.privileged_pp->e.guid.prefix;
    os_mutexUnlock (&gv.privileged_pp_lock);
    bwr_guid.entityid.u = entityid;
  }

  return ephash_lookup_writer_guid (&bwr_guid);
}

/* WRITER/READER/PROXY-WRITER/PROXY-READER CONNECTION ---------------

   These are all located in a separate section because they are so
   very similar that co-locating them eases editing and checking. */

struct rebuild_flatten_locs_arg {
  nn_locator_t *locs;
  int idx;
#ifndef NDEBUG
  int size;
#endif
};

static void rebuild_flatten_locs(const nn_locator_t *loc, void *varg)
{
  struct rebuild_flatten_locs_arg *arg = varg;
  assert(arg->idx < arg->size);
  arg->locs[arg->idx++] = *loc;
}

static int rebuild_compare_locs(const void *va, const void *vb)
{
  const nn_locator_t *a = va;
  const nn_locator_t *b = vb;
  if (a->kind != b->kind || a->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
    return compare_locators(a, b);
  else
  {
    nn_locator_t u = *a, v = *b;
    nn_udpv4mcgen_address_t *u1 = (nn_udpv4mcgen_address_t *) u.address;
    nn_udpv4mcgen_address_t *v1 = (nn_udpv4mcgen_address_t *) v.address;
    u1->idx = v1->idx = 0;
    return compare_locators(&u, &v);
  }
}

static struct addrset *rebuild_make_all_addrs (int *nreaders, struct writer *wr)
{
  struct addrset *all_addrs = new_addrset();
  struct wr_prd_match *m;
  ut_avlIter_t it;
#ifdef DDSI_INCLUDE_SSM
  if (wr->supports_ssm && wr->ssm_as)
    copy_addrset_into_addrset_mc (all_addrs, wr->ssm_as);
#endif
  *nreaders = 0;
  for (m = ut_avlIterFirst (&wr_readers_treedef, &wr->readers, &it); m; m = ut_avlIterNext (&it))
  {
    struct proxy_reader *prd;
    if ((prd = ephash_lookup_proxy_reader_guid (&m->prd_guid)) == NULL)
      continue;
    (*nreaders)++;
    copy_addrset_into_addrset(all_addrs, prd->c.as);
  }
  if (addrset_empty(all_addrs) || *nreaders == 0)
  {
    unref_addrset(all_addrs);
    return NULL;
  }
  else
  {
    return all_addrs;
  }
}

static void rebuild_make_locs(int *p_nlocs, nn_locator_t **p_locs, struct addrset *all_addrs)
{
  struct rebuild_flatten_locs_arg flarg;
  int nlocs;
  int i, j;
  nn_locator_t *locs;
  nlocs = (int)addrset_count(all_addrs);
  locs = os_malloc((size_t)nlocs * sizeof(*locs));
  flarg.locs = locs;
  flarg.idx = 0;
#ifndef NDEBUG
  flarg.size = nlocs;
#endif
  addrset_forall(all_addrs, rebuild_flatten_locs, &flarg);
  assert(flarg.idx == flarg.size);
  qsort(locs, (size_t)nlocs, sizeof(*locs), rebuild_compare_locs);
  /* We want MC gens just once for each IP,BASE,COUNT pair, not once for each node */
  i = 0; j = 1;
  while (j < nlocs)
  {
    if (rebuild_compare_locs(&locs[i], &locs[j]) != 0)
      locs[++i] = locs[j];
    j++;
  }
  nlocs = i+1;
  DDS_LOG(DDS_LC_DISCOVERY, "reduced nlocs=%d\n", nlocs);
  *p_nlocs = nlocs;
  *p_locs = locs;
}

static void rebuild_make_covered(int8_t **covered, const struct writer *wr, int *nreaders, int nlocs, const nn_locator_t *locs)
{
  struct rebuild_flatten_locs_arg flarg;
  struct wr_prd_match *m;
  ut_avlIter_t it;
  int rdidx, i, j;
  int8_t *cov = os_malloc((size_t) *nreaders * (size_t) nlocs * sizeof (*cov));
  for (i = 0; i < *nreaders * nlocs; i++)
    cov[i] = -1;
  rdidx = 0;
  flarg.locs = os_malloc((size_t) nlocs * sizeof(*flarg.locs));
#ifndef NDEBUG
  flarg.size = nlocs;
#endif
  for (m = ut_avlIterFirst (&wr_readers_treedef, &wr->readers, &it); m; m = ut_avlIterNext (&it))
  {
    struct proxy_reader *prd;
    struct addrset *ass[] = { NULL, NULL, NULL };
    if ((prd = ephash_lookup_proxy_reader_guid (&m->prd_guid)) == NULL)
      continue;
    ass[0] = prd->c.as;
#ifdef DDSI_INCLUDE_SSM
    if (prd->favours_ssm && wr->supports_ssm)
      ass[1] = wr->ssm_as;
#endif
    for (i = 0; ass[i]; i++)
    {
      flarg.idx = 0;
      addrset_forall(ass[i], rebuild_flatten_locs, &flarg);
      for (j = 0; j < flarg.idx; j++)
      {
        /* all addresses should be in the combined set of addresses -- FIXME: this doesn't hold if the address sets can change */
        const nn_locator_t *l = bsearch(&flarg.locs[j], locs, (size_t) nlocs, sizeof(*locs), rebuild_compare_locs);
        int lidx;
        int8_t x;
        assert(l != NULL);
        lidx = (int) (l - locs);
        if (l->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
          x = 0;
        else
        {
          const nn_udpv4mcgen_address_t *l1 = (const nn_udpv4mcgen_address_t *) flarg.locs[j].address;
          assert(l1->base + l1->idx <= 127);
          x = (int8_t) (l1->base + l1->idx);
        }
        cov[rdidx * nlocs + lidx] = x;
      }
    }
    rdidx++;
  }
  os_free(flarg.locs);
  *covered = cov;
  *nreaders = rdidx;
}

static void rebuild_make_locs_nrds(int **locs_nrds, int nreaders, int nlocs, const int8_t *covered)
{
  int i, j;
  int *ln = os_malloc((size_t) nlocs * sizeof(*ln));
  for (i = 0; i < nlocs; i++)
  {
    int n = 0;
    for (j = 0; j < nreaders; j++)
      if (covered[j * nlocs + i] >= 0)
        n++;

/* The compiler doesn't realize that ln is large enough. */
OS_WARNING_MSVC_OFF(6386);
    ln[i] = n;
OS_WARNING_MSVC_ON(6386);
  }
  *locs_nrds = ln;
}

static void rebuild_trace_covered(int nreaders, int nlocs, const nn_locator_t *locs, const int *locs_nrds, const int8_t *covered)
{
  int i, j;
  for (i = 0; i < nlocs; i++)
  {
    char buf[INET6_ADDRSTRLEN_EXTENDED];
    ddsi_locator_to_string(buf, sizeof(buf), &locs[i]);
    DDS_LOG(DDS_LC_DISCOVERY, "  loc %2d = %-20s %2d {", i, buf, locs_nrds[i]);
    for (j = 0; j < nreaders; j++)
      if (covered[j * nlocs + i] >= 0)
        DDS_LOG(DDS_LC_DISCOVERY, " %d", covered[j * nlocs + i]);
      else
        DDS_LOG(DDS_LC_DISCOVERY, " .");
    DDS_LOG(DDS_LC_DISCOVERY, " }\n");
  }
}

static int rebuild_select(int nlocs, const nn_locator_t *locs, const int *locs_nrds)
{
  int i, j;
  if (nlocs == 0)
    return -1;
  for (j = 0, i = 1; i < nlocs; i++) {
    if (locs_nrds[i] > locs_nrds[j])
      j = i; /* better coverage */
    else if (locs_nrds[i] == locs_nrds[j])
    {
      if (locs_nrds[i] == 1 && !ddsi_is_mcaddr(&locs[i]))
        j = i; /* prefer unicast for single nodes */
#if DDSI_INCLUDE_SSM
      else if (ddsi_is_ssm_mcaddr(&locs[i]))
        j = i; /* "reader favours SSM": all else being equal, use SSM */
#endif
    }
  }
  return (locs_nrds[j] > 0) ? j : -1;
}

static void rebuild_add(struct addrset *newas, int locidx, int nreaders, int nlocs, const nn_locator_t *locs, const int8_t *covered)
{
  char str[INET6_ADDRSTRLEN_EXTENDED];
  if (locs[locidx].kind != NN_LOCATOR_KIND_UDPv4MCGEN)
  {
    ddsi_locator_to_string(str, sizeof(str), &locs[locidx]);
    DDS_LOG(DDS_LC_DISCOVERY, "  simple %s\n", str);
    add_to_addrset(newas, &locs[locidx]);
  }
  else /* convert MC gen to the correct multicast address */
  {
    nn_locator_t l = locs[locidx];
    nn_udpv4mcgen_address_t l1;
    uint32_t iph, ipn;
    int i;
    memcpy(&l1, l.address, sizeof(l1));
    l.kind = NN_LOCATOR_KIND_UDPv4;
    memset(l.address, 0, 12);
    iph = ntohl(l1.ipv4.s_addr);
    for(i = 0; i < nreaders; i++)
      if (covered[i * nlocs + locidx] >= 0)
        iph |= 1u << covered[i * nlocs + locidx];
    ipn = htonl(iph);
    memcpy(l.address + 12, &ipn, 4);
    ddsi_locator_to_string(str, sizeof(str), &l);
    DDS_LOG(DDS_LC_DISCOVERY, "  mcgen %s\n", str);
    add_to_addrset(newas, &l);
  }
}

static void rebuild_drop(int locidx, int nreaders, int nlocs, int *locs_nrds, int8_t *covered)
{
  /* readers covered by this locator no longer matter */
  int i, j;
  for (i = 0; i < nreaders; i++)
  {
    if (covered[i * nlocs + locidx] < 0)
      continue;
    for (j = 0; j < nlocs; j++)
      if (covered[i * nlocs + j] >= 0)
      {
        assert(locs_nrds[j] > 0);
        locs_nrds[j]--;
        covered[i * nlocs + j] = -1;
      }
  }
}

static void rebuild_writer_addrset_setcover(struct addrset *newas, struct writer *wr)
{
  struct addrset *all_addrs;
  int nreaders, nlocs;
  nn_locator_t *locs;
  int *locs_nrds;
  int8_t *covered;
  int best;
  if ((all_addrs = rebuild_make_all_addrs(&nreaders, wr)) == NULL)
    return;
  nn_log_addrset(DDS_LC_DISCOVERY, "setcover: all_addrs", all_addrs);
  DDS_LOG(DDS_LC_DISCOVERY, "\n");
  rebuild_make_locs(&nlocs, &locs, all_addrs);
  unref_addrset(all_addrs);
  rebuild_make_covered(&covered, wr, &nreaders, nlocs, locs);
  if (nreaders == 0)
    goto done;
  rebuild_make_locs_nrds(&locs_nrds, nreaders, nlocs, covered);
  while ((best = rebuild_select(nlocs, locs, locs_nrds)) >= 0)
  {
    rebuild_trace_covered(nreaders, nlocs, locs, locs_nrds, covered);
    DDS_LOG(DDS_LC_DISCOVERY, "  best = %d\n", best);
    rebuild_add(newas, best, nreaders, nlocs, locs, covered);
    rebuild_drop(best, nreaders, nlocs, locs_nrds, covered);
    assert (locs_nrds[best] == 0);
  }
  os_free(locs_nrds);
 done:
  os_free(locs);
  os_free(covered);
}

static void rebuild_writer_addrset (struct writer *wr)
{
  /* FIXME way too inefficient in this form */
  struct addrset *newas = new_addrset ();
  struct addrset *oldas = wr->as;

  /* only one operation at a time */
  ASSERT_MUTEX_HELD (&wr->e.lock);

  /* compute new addrset */
  rebuild_writer_addrset_setcover(newas, wr);

  /* swap in new address set; this simple procedure is ok as long as
     wr->as is never accessed without the wr->e.lock held */
  wr->as = newas;
  unref_addrset (oldas);

  DDS_LOG(DDS_LC_DISCOVERY, "rebuild_writer_addrset(%x:%x:%x:%x):", PGUID (wr->e.guid));
  nn_log_addrset(DDS_LC_DISCOVERY, "", wr->as);
  DDS_LOG(DDS_LC_DISCOVERY, "\n");
}

void rebuild_or_clear_writer_addrsets(int rebuild)
{
  struct ephash_enum_writer est;
  struct writer *wr;
  struct addrset *empty = rebuild ? NULL : new_addrset();
  DDS_LOG(DDS_LC_DISCOVERY, "rebuild_or_delete_writer_addrsets(%d)\n", rebuild);
  ephash_enum_writer_init (&est);
  os_rwlockRead (&gv.qoslock);
  while ((wr = ephash_enum_writer_next (&est)) != NULL)
  {
    os_mutexLock (&wr->e.lock);
    if (wr->e.guid.entityid.u != NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER)
    {
      if (rebuild)
        rebuild_writer_addrset(wr);
      else
        addrset_purge(wr->as);
    }
    else
    {
      /* SPDP writers have no matched readers, instead they all use the same address space,
         gv.as_disc. Keep as_disc unchanged, and instead make the participants point to the
         empty one. */
      unref_addrset(wr->as);
      if (rebuild)
        wr->as = ref_addrset(gv.as_disc);
      else
        wr->as = ref_addrset(empty);
    }
    os_mutexUnlock (&wr->e.lock);
  }
  os_rwlockUnlock (&gv.qoslock);
  ephash_enum_writer_fini (&est);
  unref_addrset(empty);
  DDS_LOG(DDS_LC_DISCOVERY, "rebuild_or_delete_writer_addrsets(%d) done\n", rebuild);
}

static void free_wr_prd_match (struct wr_prd_match *m)
{
  if (m)
  {
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    os_free (m);
  }
}

static void free_rd_pwr_match (struct rd_pwr_match *m)
{
  if (m)
  {
#ifdef DDSI_INCLUDE_SSM
    if (!is_unspec_locator (&m->ssm_mc_loc))
    {
      assert (ddsi_is_mcaddr (&m->ssm_mc_loc));
      assert (!is_unspec_locator (&m->ssm_src_loc));
      if (ddsi_leave_mc (gv.data_conn_mc, &m->ssm_src_loc, &m->ssm_mc_loc) < 0)
        DDS_WARNING("failed to leave network partition ssm group\n");
    }
#endif
    os_free (m);
  }
}

static void free_pwr_rd_match (struct pwr_rd_match *m)
{
  if (m)
  {
    if (m->acknack_xevent)
      delete_xevent (m->acknack_xevent);
    nn_reorder_free (m->u.not_in_sync.reorder);
    os_free (m);
  }
}

static void free_prd_wr_match (struct prd_wr_match *m)
{
  if (m) os_free (m);
}

static void free_rd_wr_match (struct rd_wr_match *m)
{
  if (m) os_free (m);
}

static void free_wr_rd_match (struct wr_rd_match *m)
{
  if (m) os_free (m);
}

static void writer_drop_connection (const struct nn_guid * wr_guid, const struct proxy_reader * prd)
{
  struct writer *wr;
  if ((wr = ephash_lookup_writer_guid (wr_guid)) != NULL)
  {
    struct whc_node *deferred_free_list = NULL;
    struct wr_prd_match *m;
    os_mutexLock (&wr->e.lock);
    if ((m = ut_avlLookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      struct whc_state whcst;
      ut_avlDelete (&wr_readers_treedef, &wr->readers, m);
      rebuild_writer_addrset (wr);
      remove_acked_messages (wr, &whcst, &deferred_free_list);
      wr->num_reliable_readers -= m->is_reliable;
    }
    os_mutexUnlock (&wr->e.lock);
    if (m != NULL && wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = false;
      data.handle = prd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    whc_free_deferred_free_list (wr->whc, deferred_free_list);
    free_wr_prd_match (m);
  }
}

static void writer_drop_local_connection (const struct nn_guid *wr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct writer *wr;
  if ((wr = ephash_lookup_writer_guid (wr_guid)) != NULL)
  {
    struct wr_rd_match *m;

    os_mutexLock (&wr->e.lock);
    if ((m = ut_avlLookup (&wr_local_readers_treedef, &wr->local_readers, &rd->e.guid)) != NULL)
    {
      ut_avlDelete (&wr_local_readers_treedef, &wr->local_readers, m);
    }
    local_reader_ary_remove (&wr->rdary, rd);
    os_mutexUnlock (&wr->e.lock);
    if (m != NULL && wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = false;
      data.handle = rd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    free_wr_rd_match (m);
  }
}

static void reader_drop_connection (const struct nn_guid *rd_guid, const struct proxy_writer * pwr)
{
  struct reader *rd;
  if ((rd = ephash_lookup_reader_guid (rd_guid)) != NULL)
  {
    struct rd_pwr_match *m;
    os_mutexLock (&rd->e.lock);
    if ((m = ut_avlLookup (&rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
      ut_avlDelete (&rd_writers_treedef, &rd->writers, m);
    os_mutexUnlock (&rd->e.lock);
    free_rd_pwr_match (m);

    if (rd->rhc)
    {
      struct proxy_writer_info pwr_info;
      make_proxy_writer_info(&pwr_info, &pwr->e, pwr->c.xqos);
      (ddsi_plugin.rhc_plugin.rhc_unregister_wr_fn) (rd->rhc, &pwr_info);
    }
    if (rd->status_cb)
    {
      status_cb_data_t data;

      data.add = false;
      data.handle = pwr->e.iid;

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void reader_drop_local_connection (const struct nn_guid *rd_guid, const struct writer * wr)
{
  struct reader *rd;
  if ((rd = ephash_lookup_reader_guid (rd_guid)) != NULL)
  {
    struct rd_wr_match *m;
    os_mutexLock (&rd->e.lock);
    if ((m = ut_avlLookup (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid)) != NULL)
      ut_avlDelete (&rd_local_writers_treedef, &rd->local_writers, m);
    os_mutexUnlock (&rd->e.lock);
    free_rd_wr_match (m);

    if (rd->rhc)
    {
      /* FIXME: */
      struct proxy_writer_info pwr_info;
      make_proxy_writer_info(&pwr_info, &wr->e, wr->xqos);
      (ddsi_plugin.rhc_plugin.rhc_unregister_wr_fn) (rd->rhc, &pwr_info);
    }
    if (rd->status_cb)
    {
      status_cb_data_t data;

      data.add = false;
      data.handle = wr->e.iid;

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void update_reader_init_acknack_count (const struct nn_guid *rd_guid, nn_count_t count)
{
  struct reader *rd;

  /* Update the initial acknack sequence number for the reader.  See
     also reader_add_connection(). */
  DDS_LOG(DDS_LC_DISCOVERY, "update_reader_init_acknack_count (%x:%x:%x:%x, %d): ", PGUID (*rd_guid), count);
  if ((rd = ephash_lookup_reader_guid (rd_guid)) != NULL)
  {
    os_mutexLock (&rd->e.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "%d -> ", rd->init_acknack_count);
    if (count > rd->init_acknack_count)
      rd->init_acknack_count = count;
    DDS_LOG(DDS_LC_DISCOVERY, "%d\n", count);
    os_mutexUnlock (&rd->e.lock);
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, "reader no longer exists\n");
  }
}

static void proxy_writer_drop_connection (const struct nn_guid *pwr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct proxy_writer *pwr;
  if ((pwr = ephash_lookup_proxy_writer_guid (pwr_guid)) != NULL)
  {
    struct pwr_rd_match *m;

    os_mutexLock (&pwr->e.lock);
    if ((m = ut_avlLookup (&pwr_readers_treedef, &pwr->readers, &rd->e.guid)) != NULL)
    {
      ut_avlDelete (&pwr_readers_treedef, &pwr->readers, m);
      if (m->in_sync != PRMSS_SYNC)
      {
        pwr->n_readers_out_of_sync--;
      }
    }
    if (rd->reliable)
    {
      pwr->n_reliable_readers--;
    }
    local_reader_ary_remove (&pwr->rdary, rd);
    os_mutexUnlock (&pwr->e.lock);
    if (m != NULL)
    {
      update_reader_init_acknack_count (&rd->e.guid, m->count);
    }
    free_pwr_rd_match (m);
  }
}

static void proxy_reader_drop_connection
  (const struct nn_guid *prd_guid, struct writer * wr)
{
  struct proxy_reader *prd;
  if ((prd = ephash_lookup_proxy_reader_guid (prd_guid)) != NULL)
  {
    struct prd_wr_match *m;
    os_mutexLock (&prd->e.lock);
    m = ut_avlLookup (&prd_writers_treedef, &prd->writers, &wr->e.guid);
    if (m)
    {
      ut_avlDelete (&prd_writers_treedef, &prd->writers, m);
    }
    os_mutexUnlock (&prd->e.lock);
    free_prd_wr_match (m);
  }
}

static void writer_add_connection (struct writer *wr, struct proxy_reader *prd)
{
  struct wr_prd_match *m = os_malloc (sizeof (*m));
  ut_avlIPath_t path;
  int pretend_everything_acked;
  m->prd_guid = prd->e.guid;
  m->is_reliable = (prd->c.xqos->reliability.kind > NN_BEST_EFFORT_RELIABILITY_QOS);
  m->assumed_in_sync = (config.retransmit_merging == REXMIT_MERGE_ALWAYS);
  m->has_replied_to_hb = !m->is_reliable;
  m->all_have_replied_to_hb = 0;
  m->non_responsive_count = 0;
  m->rexmit_requests = 0;
  /* m->demoted: see below */
  os_mutexLock (&prd->e.lock);
  if (prd->deleting)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  writer_add_connection(wr %x:%x:%x:%x prd %x:%x:%x:%x) - prd is being deleted\n",
            PGUID (wr->e.guid), PGUID (prd->e.guid));
    pretend_everything_acked = 1;
  }
  else if (!m->is_reliable)
  {
    /* Pretend a best-effort reader has ack'd everything, even waht is
       still to be published. */
    pretend_everything_acked = 1;
  }
  else
  {
    pretend_everything_acked = 0;
  }
  os_mutexUnlock (&prd->e.lock);
  m->next_acknack = DDSI_COUNT_MIN;
  m->next_nackfrag = DDSI_COUNT_MIN;
  nn_lat_estim_init (&m->hb_to_ack_latency);
  m->hb_to_ack_latency_tlastlog = now ();
  m->t_acknack_accepted.v = 0;

  os_mutexLock (&wr->e.lock);
  if (pretend_everything_acked)
    m->seq = MAX_SEQ_NUMBER;
  else
    m->seq = wr->seq;
  if (ut_avlLookupIPath (&wr_readers_treedef, &wr->readers, &prd->e.guid, &path))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  writer_add_connection(wr %x:%x:%x:%x prd %x:%x:%x:%x) - already connected\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
    os_mutexUnlock (&wr->e.lock);
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    os_free (m);
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  writer_add_connection(wr %x:%x:%x:%x prd %x:%x:%x:%x) - ack seq %"PRId64"\n", PGUID (wr->e.guid), PGUID (prd->e.guid), m->seq);
    ut_avlInsertIPath (&wr_readers_treedef, &wr->readers, m, &path);
    rebuild_writer_addrset (wr);
    wr->num_reliable_readers += m->is_reliable;
    os_mutexUnlock (&wr->e.lock);

    if (wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
      data.add = true;
      data.handle = prd->e.iid;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }

    /* If reliable and/or transient-local, we may have data available
       in the WHC, but if all has been acknowledged by the previously
       known proxy readers (or if the is the first proxy reader),
       there is no heartbeat event scheduled.

       A pre-emptive AckNack may be sent, but need not be, and we
       can't be certain it won't have the final flag set. So we must
       ensure a heartbeat is scheduled soon. */
    if (wr->heartbeat_xevent)
    {
      const int64_t delta = 1 * T_MILLISECOND;
      const nn_mtime_t tnext = add_duration_to_mtime (now_mt (), delta);
      os_mutexLock (&wr->e.lock);
      /* To make sure that we keep sending heartbeats at a higher rate
         at the start of this discovery, reset the hbs_since_last_write
         count to zero. */
      wr->hbcontrol.hbs_since_last_write = 0;
      if (tnext.v < wr->hbcontrol.tsched.v)
      {
        wr->hbcontrol.tsched = tnext;
        resched_xevent_if_earlier (wr->heartbeat_xevent, tnext);
      }
      os_mutexUnlock (&wr->e.lock);
    }
  }
}

static void writer_add_local_connection (struct writer *wr, struct reader *rd)
{
  struct wr_rd_match *m = os_malloc (sizeof (*m));
  ut_avlIPath_t path;

  os_mutexLock (&wr->e.lock);
  if (ut_avlLookupIPath (&wr_local_readers_treedef, &wr->local_readers, &rd->e.guid, &path))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  writer_add_local_connection(wr %x:%x:%x:%x rd %x:%x:%x:%x) - already connected\n", PGUID (wr->e.guid), PGUID (rd->e.guid));
    os_mutexUnlock (&wr->e.lock);
    os_free (m);
    return;
  }

  DDS_LOG(DDS_LC_DISCOVERY, "  writer_add_local_connection(wr %x:%x:%x:%x rd %x:%x:%x:%x)", PGUID (wr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  ut_avlInsertIPath (&wr_local_readers_treedef, &wr->local_readers, m, &path);
  local_reader_ary_insert (&wr->rdary, rd);

  /* Store available data into the late joining reader when it is reliable (we don't do
     historical data for best-effort data over the wire, so also not locally).
     FIXME: should limit ourselves to what it is available because of durability history,
     not writer history */
  if (rd->xqos->reliability.kind > NN_BEST_EFFORT_RELIABILITY_QOS && rd->xqos->durability.kind > NN_VOLATILE_DURABILITY_QOS)
  {
    struct whc_sample_iter it;
    struct whc_borrowed_sample sample;
    whc_sample_iter_init(wr->whc, &it);
    while (whc_sample_iter_borrow_next(&it, &sample))
    {
      struct proxy_writer_info pwr_info;
      struct ddsi_serdata *payload = sample.serdata;
      /* FIXME: whc has tk reference in its index nodes, which is what we really should be iterating over anyway, and so we don't really have to look them up anymore */
      struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref(payload);
      make_proxy_writer_info(&pwr_info, &wr->e, wr->xqos);
      (void)(ddsi_plugin.rhc_plugin.rhc_store_fn) (rd->rhc, &pwr_info, payload, tk);
      ddsi_tkmap_instance_unref(tk);
    }
  }

  os_mutexUnlock (&wr->e.lock);

  DDS_LOG(DDS_LC_DISCOVERY, "\n");

  if (wr->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
    data.add = true;
    data.handle = rd->e.iid;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

static void reader_add_connection (struct reader *rd, struct proxy_writer *pwr, nn_count_t *init_count)
{
  struct rd_pwr_match *m = os_malloc (sizeof (*m));
  ut_avlIPath_t path;

  m->pwr_guid = pwr->e.guid;

  os_mutexLock (&rd->e.lock);

  /* Initial sequence number of acknacks is the highest stored (+ 1,
     done when generating the acknack) -- existing connections may be
     beyond that already, but this guarantees that one particular
     writer will always see monotonically increasing sequence numbers
     from one particular reader.  This is then used for the
     pwr_rd_match initialization */
  DDS_LOG(DDS_LC_DISCOVERY, "  reader %x:%x:%x:%x init_acknack_count = %d\n", PGUID (rd->e.guid), rd->init_acknack_count);
  *init_count = rd->init_acknack_count;

  if (ut_avlLookupIPath (&rd_writers_treedef, &rd->writers, &pwr->e.guid, &path))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  reader_add_connection(pwr %x:%x:%x:%x rd %x:%x:%x:%x) - already connected\n",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
    os_mutexUnlock (&rd->e.lock);
    os_free (m);
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  reader_add_connection(pwr %x:%x:%x:%x rd %x:%x:%x:%x)\n",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
    ut_avlInsertIPath (&rd_writers_treedef, &rd->writers, m, &path);
    os_mutexUnlock (&rd->e.lock);

#ifdef DDSI_INCLUDE_SSM
  if (rd->favours_ssm && pwr->supports_ssm)
  {
    /* pwr->supports_ssm is set if addrset_contains_ssm(pwr->ssm), so
       any_ssm must succeed. */
    if (!addrset_any_uc (pwr->c.as, &m->ssm_src_loc))
      assert (0);
    if (!addrset_any_ssm (pwr->c.as, &m->ssm_mc_loc))
      assert (0);
    /* FIXME: for now, assume that the ports match for datasock_mc --
       't would be better to dynamically create and destroy sockets on
       an as needed basis. */
    ddsi_join_mc (gv.data_conn_mc, &m->ssm_src_loc, &m->ssm_mc_loc);
  }
  else
  {
    set_unspec_locator (&m->ssm_src_loc);
    set_unspec_locator (&m->ssm_mc_loc);
  }
#endif

    if (rd->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      data.add = true;
      data.handle = pwr->e.iid;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void reader_add_local_connection (struct reader *rd, struct writer *wr)
{
  struct rd_wr_match *m = os_malloc (sizeof (*m));
  ut_avlIPath_t path;

  m->wr_guid = wr->e.guid;

  os_mutexLock (&rd->e.lock);

  if (ut_avlLookupIPath (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid, &path))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  reader_add_local_connection(wr %x:%x:%x:%x rd %x:%x:%x:%x) - already connected\n", PGUID (wr->e.guid), PGUID (rd->e.guid));
    os_mutexUnlock (&rd->e.lock);
    os_free (m);
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  reader_add_local_connection(wr %x:%x:%x:%x rd %x:%x:%x:%x)\n", PGUID (wr->e.guid), PGUID (rd->e.guid));
    ut_avlInsertIPath (&rd_local_writers_treedef, &rd->local_writers, m, &path);
    os_mutexUnlock (&rd->e.lock);

    if (rd->status_cb)
    {
      status_cb_data_t data;
      data.add = true;
      data.handle = wr->e.iid;

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void proxy_writer_add_connection (struct proxy_writer *pwr, struct reader *rd, nn_mtime_t tnow, nn_count_t init_count)
{
  struct pwr_rd_match *m = os_malloc (sizeof (*m));
  ut_avlIPath_t path;
  seqno_t last_deliv_seq;

  os_mutexLock (&pwr->e.lock);
  if (ut_avlLookupIPath (&pwr_readers_treedef, &pwr->readers, &rd->e.guid, &path))
    goto already_matched;

  if (pwr->c.topic == NULL && rd->topic)
    pwr->c.topic = ddsi_sertopic_ref (rd->topic);
  if (pwr->ddsi2direct_cb == 0 && rd->ddsi2direct_cb != 0)
  {
    pwr->ddsi2direct_cb = rd->ddsi2direct_cb;
    pwr->ddsi2direct_cbarg = rd->ddsi2direct_cbarg;
  }

  DDS_LOG(DDS_LC_DISCOVERY, "  proxy_writer_add_connection(pwr %x:%x:%x:%x rd %x:%x:%x:%x)",
          PGUID (pwr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  m->tcreate = now_mt ();


  /* We track the last heartbeat count value per reader--proxy-writer
     pair, so that we can correctly handle directed heartbeats. The
     only reason to bother is to prevent a directed heartbeat (with
     the FINAL flag clear) from causing AckNacks from all readers
     instead of just the addressed ones.

     If we don't mind those extra AckNacks, we could track the count
     at the proxy-writer and simply treat all incoming heartbeats as
     undirected. */
  m->next_heartbeat = DDSI_COUNT_MIN;
  m->hb_timestamp.v = 0;
  m->t_heartbeat_accepted.v = 0;
  m->t_last_nack.v = 0;
  m->seq_last_nack = 0;

  /* These can change as a consequence of handling data and/or
     discovery activities. The safe way of dealing with them is to
     lock the proxy writer */
  last_deliv_seq = nn_reorder_next_seq (pwr->reorder) - 1;
  if (!rd->handle_as_transient_local)
  {
    m->in_sync = PRMSS_SYNC;
  }
  else if (!config.conservative_builtin_reader_startup && is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) && !ut_avlIsEmpty (&pwr->readers))
  {
    /* builtins really don't care about multiple copies */
    m->in_sync = PRMSS_SYNC;
  }
  else
  {
    /* normal transient-local, reader is behind proxy writer */
    m->in_sync = PRMSS_OUT_OF_SYNC;
    if (last_deliv_seq == 0)
    {
      m->u.not_in_sync.end_of_out_of_sync_seq = MAX_SEQ_NUMBER;
      m->u.not_in_sync.end_of_tl_seq = MAX_SEQ_NUMBER;
    }
    else
    {
      m->u.not_in_sync.end_of_tl_seq = pwr->last_seq;
      m->u.not_in_sync.end_of_out_of_sync_seq = last_deliv_seq;
    }
    DDS_LOG(DDS_LC_DISCOVERY, " - out-of-sync %"PRId64, m->u.not_in_sync.end_of_out_of_sync_seq);
  }
  if (m->in_sync != PRMSS_SYNC)
    pwr->n_readers_out_of_sync++;
  m->count = init_count;
  /* Spec says we may send a pre-emptive AckNack (8.4.2.3.4), hence we
     schedule it for the configured delay * T_MILLISECOND. From then
     on it it'll keep sending pre-emptive ones until the proxy writer
     receives a heartbeat.  (We really only need a pre-emptive AckNack
     per proxy writer, but hopefully it won't make that much of a
     difference in practice.) */
  if (rd->reliable)
  {
    m->acknack_xevent = qxev_acknack (pwr->evq, add_duration_to_mtime (tnow, config.preemptive_ack_delay), &pwr->e.guid, &rd->e.guid);
    m->u.not_in_sync.reorder =
      nn_reorder_new (NN_REORDER_MODE_NORMAL, config.secondary_reorder_maxsamples);
    pwr->n_reliable_readers++;
  }
  else
  {
    m->acknack_xevent = NULL;
    m->u.not_in_sync.reorder =
      nn_reorder_new (NN_REORDER_MODE_MONOTONICALLY_INCREASING, config.secondary_reorder_maxsamples);
  }

  ut_avlInsertIPath (&pwr_readers_treedef, &pwr->readers, m, &path);
  local_reader_ary_insert(&pwr->rdary, rd);
  os_mutexUnlock (&pwr->e.lock);
  qxev_pwr_entityid (pwr, &rd->e.guid.prefix);

  DDS_LOG(DDS_LC_DISCOVERY, "\n");

  if (rd->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
    data.add = true;
    data.handle = pwr->e.iid;
    (rd->status_cb) (rd->status_cb_entity, &data);
  }

  return;

already_matched:
  assert (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor) ? (pwr->c.topic == NULL) : (pwr->c.topic != NULL));
  DDS_LOG(DDS_LC_DISCOVERY, "  proxy_writer_add_connection(pwr %x:%x:%x:%x rd %x:%x:%x:%x) - already connected\n",
          PGUID (pwr->e.guid), PGUID (rd->e.guid));
  os_mutexUnlock (&pwr->e.lock);
  os_free (m);
  return;
}

static void proxy_reader_add_connection (struct proxy_reader *prd, struct writer *wr)
{
  struct prd_wr_match *m = os_malloc (sizeof (*m));
  ut_avlIPath_t path;

  m->wr_guid = wr->e.guid;
  os_mutexLock (&prd->e.lock);
  if (prd->c.topic == NULL)
    prd->c.topic = ddsi_sertopic_ref (wr->topic);
  if (ut_avlLookupIPath (&prd_writers_treedef, &prd->writers, &wr->e.guid, &path))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  proxy_reader_add_connection(wr %x:%x:%x:%x prd %x:%x:%x:%x) - already connected\n",
            PGUID (wr->e.guid), PGUID (prd->e.guid));
    os_mutexUnlock (&prd->e.lock);
    os_free (m);
  }
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, "  proxy_reader_add_connection(wr %x:%x:%x:%x prd %x:%x:%x:%x)\n",
            PGUID (wr->e.guid), PGUID (prd->e.guid));
    ut_avlInsertIPath (&prd_writers_treedef, &prd->writers, m, &path);
    os_mutexUnlock (&prd->e.lock);
    qxev_prd_entityid (prd, &wr->e.guid.prefix);
  }
}

static nn_entityid_t builtin_entityid_match (nn_entityid_t x)
{
  nn_entityid_t res;
  res.u = 0;
  switch (x.u)
  {
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER;
      break;

    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER:
    case NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER:
      /* SPDP is special cased because we don't -- indeed can't --
         match readers with writers, only send to matched readers and
         only accept from matched writers. That way discovery wouldn't
         work at all. No entity with NN_ENTITYID_UNKNOWN exists,
         ever, so this guarantees no connection will be made. */
      res.u = NN_ENTITYID_UNKNOWN;
      break;

    case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER;
      break;

    default:
      assert (0);
  }
  return res;
}

static void writer_qos_mismatch (struct writer * wr, uint32_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID)
  {
    if (wr->topic->status_cb) {
      /* Handle INCONSISTENT_TOPIC on topic */
      (wr->topic->status_cb) (wr->topic->status_cb_entity);
    }
    if (wr->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID;
      data.extra = reason;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
  }
}

static void reader_qos_mismatch (struct reader * rd, uint32_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID)
  {
    if (rd->topic->status_cb)
    {
      /* Handle INCONSISTENT_TOPIC on topic */
      (rd->topic->status_cb) (rd->topic->status_cb_entity);
    }
    if (rd->status_cb)
    {
      status_cb_data_t data;
      data.raw_status_id = (int) DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID;
      data.extra = reason;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void connect_writer_with_proxy_reader (struct writer *wr, struct proxy_reader *prd, nn_mtime_t tnow)
{
  const int isb0 = (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  const int isb1 = (is_builtin_entityid (prd->e.guid.entityid, prd->c.vendor) != 0);
  int32_t reason;
  OS_UNUSED_ARG(tnow);
  if (isb0 != isb1)
    return;
  if (wr->e.onlylocal)
    return;
  if (!isb0 && (reason = qos_match_p (prd->c.xqos, wr->xqos)) >= 0)
  {
    writer_qos_mismatch (wr, (uint32_t)reason);
    return;
  }
  proxy_reader_add_connection (prd, wr);
  writer_add_connection (wr, prd);
}

static void connect_proxy_writer_with_reader (struct proxy_writer *pwr, struct reader *rd, nn_mtime_t tnow)
{
  const int isb0 = (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor) != 0);
  const int isb1 = (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  int32_t reason;
  nn_count_t init_count;
  if (isb0 != isb1)
    return;
  if (rd->e.onlylocal)
    return;
  if (!isb0 && (reason = qos_match_p (rd->xqos, pwr->c.xqos)) >= 0)
  {
    reader_qos_mismatch (rd, (uint32_t)reason);
    return;
  }
  reader_add_connection (rd, pwr, &init_count);
  proxy_writer_add_connection (pwr, rd, tnow, init_count);
}

static void connect_writer_with_reader (struct writer *wr, struct reader *rd, nn_mtime_t tnow)
{
  int32_t reason;
  (void)tnow;
  if (!is_local_orphan_endpoint (&wr->e) && (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) || is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE)))
    return;
  if ((reason = qos_match_p (rd->xqos, wr->xqos)) >= 0)
  {
    writer_qos_mismatch (wr, (uint32_t)reason);
    reader_qos_mismatch (rd, (uint32_t)reason);
    return;
  }
  reader_add_local_connection (rd, wr);
  writer_add_local_connection (wr, rd);
}

static void connect_writer_with_proxy_reader_wrapper (struct entity_common *vwr, struct entity_common *vprd, nn_mtime_t tnow)
{
  struct writer *wr = (struct writer *) vwr;
  struct proxy_reader *prd = (struct proxy_reader *) vprd;
  assert (wr->e.kind == EK_WRITER);
  assert (prd->e.kind == EK_PROXY_READER);
  connect_writer_with_proxy_reader(wr, prd, tnow);
}

static void connect_proxy_writer_with_reader_wrapper (struct entity_common *vpwr, struct entity_common *vrd, nn_mtime_t tnow)
{
  struct proxy_writer *pwr = (struct proxy_writer *) vpwr;
  struct reader *rd = (struct reader *) vrd;
  assert (pwr->e.kind == EK_PROXY_WRITER);
  assert (rd->e.kind == EK_READER);
  connect_proxy_writer_with_reader(pwr, rd, tnow);
}

static void connect_writer_with_reader_wrapper (struct entity_common *vwr, struct entity_common *vrd, nn_mtime_t tnow)
{
  struct writer *wr = (struct writer *) vwr;
  struct reader *rd = (struct reader *) vrd;
  assert (wr->e.kind == EK_WRITER);
  assert (rd->e.kind == EK_READER);
  connect_writer_with_reader(wr, rd, tnow);
}

static enum entity_kind generic_do_match_mkind (enum entity_kind kind)
{
  switch (kind)
  {
    case EK_WRITER: return EK_PROXY_READER;
    case EK_READER: return EK_PROXY_WRITER;
    case EK_PROXY_WRITER: return EK_READER;
    case EK_PROXY_READER: return EK_WRITER;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      assert(0);
      return EK_WRITER;
  }
  assert(0);
  return EK_WRITER;
}

static enum entity_kind generic_do_local_match_mkind (enum entity_kind kind)
{
  switch (kind)
  {
    case EK_WRITER: return EK_READER;
    case EK_READER: return EK_WRITER;
    case EK_PROXY_WRITER:
    case EK_PROXY_READER:
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      assert(0);
      return EK_WRITER;
  }
  assert(0);
  return EK_WRITER;
}

static const char *generic_do_match_kindstr_us (enum entity_kind kind)
{
  switch (kind)
  {
    case EK_WRITER: return "writer";
    case EK_READER: return "reader";
    case EK_PROXY_WRITER: return "proxy_writer";
    case EK_PROXY_READER: return "proxy_reader";
    case EK_PARTICIPANT: return "participant";
    case EK_PROXY_PARTICIPANT: return "proxy_participant";
  }
  assert(0);
  return "?";
}

static const char *generic_do_match_kindstr (enum entity_kind kind)
{
  switch (kind)
  {
    case EK_WRITER: return "writer";
    case EK_READER: return "reader";
    case EK_PROXY_WRITER: return "proxy writer";
    case EK_PROXY_READER: return "proxy reader";
    case EK_PARTICIPANT: return "participant";
    case EK_PROXY_PARTICIPANT: return "proxy participant";
  }
  assert(0);
  return "?";
}

static const char *generic_do_match_kindabbrev (enum entity_kind kind)
{
  switch (kind)
  {
    case EK_WRITER: return "wr";
    case EK_READER: return "rd";
    case EK_PROXY_WRITER: return "pwr";
    case EK_PROXY_READER: return "prd";
    case EK_PARTICIPANT: return "pp";
    case EK_PROXY_PARTICIPANT: return "proxypp";
  }
  assert(0);
  return "?";
}

static int generic_do_match_isproxy (const struct entity_common *e)
{
  return e->kind == EK_PROXY_WRITER || e->kind == EK_PROXY_READER || e->kind == EK_PROXY_PARTICIPANT;
}

static void generic_do_match_connect (struct entity_common *e, struct entity_common *em, nn_mtime_t tnow)
{
  switch (e->kind)
  {
    case EK_WRITER:
      connect_writer_with_proxy_reader_wrapper(e, em, tnow);
      break;
    case EK_READER:
      connect_proxy_writer_with_reader_wrapper(em, e, tnow);
      break;
    case EK_PROXY_WRITER:
      connect_proxy_writer_with_reader_wrapper(e, em, tnow);
      break;
    case EK_PROXY_READER:
      connect_writer_with_proxy_reader_wrapper(em, e, tnow);
      break;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      assert(0);
  }
}

static void generic_do_local_match_connect (struct entity_common *e, struct entity_common *em, nn_mtime_t tnow)
{
  switch (e->kind)
  {
    case EK_WRITER:
      connect_writer_with_reader_wrapper(e, em, tnow);
      break;
    case EK_READER:
      connect_writer_with_reader_wrapper(em, e, tnow);
      break;
    case EK_PROXY_WRITER:
    case EK_PROXY_READER:
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      assert(0);
  }
}

static void generic_do_match (struct entity_common *e, nn_mtime_t tnow)
{
  struct ephash_enum est;
  struct entity_common *em;
  enum entity_kind mkind = generic_do_match_mkind(e->kind);
  if (!is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE))
  {
    DDS_LOG(DDS_LC_DISCOVERY, "match_%s_with_%ss(%s %x:%x:%x:%x) scanning all %ss\n",
            generic_do_match_kindstr_us (e->kind), generic_do_match_kindstr_us (mkind),
            generic_do_match_kindabbrev (e->kind), PGUID (e->guid),
            generic_do_match_kindstr(mkind));
    /* Note: we visit at least all proxies that existed when we called
     init (with the -- possible -- exception of ones that were
     deleted between our calling init and our reaching it while
     enumerating), but we may visit a single proxy reader multiple
     times. */
    ephash_enum_init (&est, mkind);
    os_rwlockRead (&gv.qoslock);
    while ((em = ephash_enum_next (&est)) != NULL)
      generic_do_match_connect(e, em, tnow);
    os_rwlockUnlock (&gv.qoslock);
    ephash_enum_fini (&est);
  }
  else
  {
    /* Built-ins have fixed QoS */
    nn_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
    enum entity_kind pkind = generic_do_match_isproxy (e) ? EK_PARTICIPANT : EK_PROXY_PARTICIPANT;
    DDS_LOG(DDS_LC_DISCOVERY, "match_%s_with_%ss(%s %x:%x:%x:%x) scanning %sparticipants tgt=%x\n",
            generic_do_match_kindstr_us (e->kind), generic_do_match_kindstr_us (mkind),
            generic_do_match_kindabbrev (e->kind), PGUID (e->guid),
            generic_do_match_isproxy (e) ? "" : "proxy ",
            tgt_ent.u);
    if (tgt_ent.u != NN_ENTITYID_UNKNOWN)
    {
      struct entity_common *ep;
      ephash_enum_init (&est, pkind);
      while ((ep = ephash_enum_next (&est)) != NULL)
      {
        nn_guid_t tgt_guid;
        tgt_guid.prefix = ep->guid.prefix;
        tgt_guid.entityid = tgt_ent;
        if ((em = ephash_lookup_guid (&tgt_guid, mkind)) != NULL)
          generic_do_match_connect(e, em, tnow);
      }
      ephash_enum_fini (&est);
    }
  }
}

static void generic_do_local_match (struct entity_common *e, nn_mtime_t tnow)
{
  struct ephash_enum est;
  struct entity_common *em;
  enum entity_kind mkind;
  if (is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE) && !is_local_orphan_endpoint (e))
    /* never a need for local matches on discovery endpoints */
    return;
  mkind = generic_do_local_match_mkind(e->kind);
  DDS_LOG(DDS_LC_DISCOVERY, "match_%s_with_%ss(%s %x:%x:%x:%x) scanning all %ss\n",
          generic_do_match_kindstr_us (e->kind), generic_do_match_kindstr_us (mkind),
          generic_do_match_kindabbrev (e->kind), PGUID (e->guid),
          generic_do_match_kindstr(mkind));
  /* Note: we visit at least all proxies that existed when we called
     init (with the -- possible -- exception of ones that were
     deleted between our calling init and our reaching it while
     enumerating), but we may visit a single proxy reader multiple
     times. */
  ephash_enum_init (&est, mkind);
  os_rwlockRead (&gv.qoslock);
  while ((em = ephash_enum_next (&est)) != NULL)
    generic_do_local_match_connect(e, em, tnow);
  os_rwlockUnlock (&gv.qoslock);
  ephash_enum_fini (&est);
}

static void match_writer_with_proxy_readers (struct writer *wr, nn_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow);
}

static void match_writer_with_local_readers (struct writer *wr, nn_mtime_t tnow)
{
  generic_do_local_match (&wr->e, tnow);
}

static void match_reader_with_proxy_writers (struct reader *rd, nn_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow);
}

static void match_reader_with_local_writers (struct reader *rd, nn_mtime_t tnow)
{
  generic_do_local_match (&rd->e, tnow);
}

static void match_proxy_writer_with_readers (struct proxy_writer *pwr, nn_mtime_t tnow)
{
  generic_do_match (&pwr->e, tnow);
}

static void match_proxy_reader_with_writers (struct proxy_reader *prd, nn_mtime_t tnow)
{
  generic_do_match(&prd->e, tnow);
}

/* ENDPOINT --------------------------------------------------------- */

static void new_reader_writer_common (const struct nn_guid *guid, const struct ddsi_sertopic * topic, const struct nn_xqos *xqos)
{
  const char *partition = "(default)";
  const char *partition_suffix = "";
  assert (is_builtin_entityid (guid->entityid, NN_VENDORID_ECLIPSE) ? (topic == NULL) : (topic != NULL));
  if (is_builtin_entityid (guid->entityid, NN_VENDORID_ECLIPSE))
  {
    /* continue printing it as not being in a partition, the actual
       value doesn't matter because it is never matched based on QoS
       settings */
    partition = "(null)";
  }
  else if ((xqos->present & QP_PARTITION) && xqos->partition.n > 0 && strcmp (xqos->partition.strs[0], "") != 0)
  {
    partition = xqos->partition.strs[0];
    if (xqos->partition.n > 1)
      partition_suffix = "+";
  }
  DDS_LOG(DDS_LC_DISCOVERY, "new_%s(guid %x:%x:%x:%x, %s%s.%s/%s)\n",
          is_writer_entityid (guid->entityid) ? "writer" : "reader",
          PGUID (*guid),
          partition, partition_suffix,
          topic ? topic->name : "(null)",
          topic ? topic->typename : "(null)");
}

static void endpoint_common_init (struct entity_common *e, struct endpoint_common *c, enum entity_kind kind, const struct nn_guid *guid, const struct nn_guid *group_guid, struct participant *pp)
{
  entity_common_init (e, guid, NULL, kind, now (), NN_VENDORID_ECLIPSE, pp->e.onlylocal);
  c->pp = ref_participant (pp, &e->guid);
  if (group_guid)
    c->group_guid = *group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));
}

static void endpoint_common_fini (struct entity_common *e, struct endpoint_common *c)
{
  if (!is_builtin_entityid(e->guid.entityid, NN_VENDORID_ECLIPSE))
    pp_release_entityid(c->pp, e->guid.entityid);
  if (c->pp)
    unref_participant (c->pp, &e->guid);
  else
  {
    /* only for the (almost pseudo) writers used for generating the built-in topics */
    assert (is_local_orphan_endpoint (e));
  }
  entity_common_fini (e);
}

static int set_topic_type_name (nn_xqos_t *xqos, const struct ddsi_sertopic * topic)
{
  if (!(xqos->present & QP_TYPE_NAME) && topic)
  {
    xqos->present |= QP_TYPE_NAME;
    xqos->type_name = os_strdup (topic->typename);
  }
  if (!(xqos->present & QP_TOPIC_NAME) && topic)
  {
    xqos->present |= QP_TOPIC_NAME;
    xqos->topic_name = os_strdup (topic->name);
  }
  return 0;
}

/* WRITER ----------------------------------------------------------- */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static uint32_t get_partitionid_from_mapping (const char *partition, const char *topic)
{
  struct config_partitionmapping_listelem *pm;
  if ((pm = find_partitionmapping (partition, topic)) == NULL)
    return 0;
  else
  {
    DDS_LOG(DDS_LC_DISCOVERY, "matched writer for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n", topic, partition, pm->networkPartition);
    return pm->partition->partitionId;
  }
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

static void augment_wr_prd_match (void *vnode, const void *vleft, const void *vright)
{
  struct wr_prd_match *n = vnode;
  const struct wr_prd_match *left = vleft;
  const struct wr_prd_match *right = vright;
  seqno_t min_seq, max_seq;
  int have_replied = n->has_replied_to_hb;

  /* note: this means min <= seq, but not min <= max nor seq <= max!
     note: this guarantees max < MAX_SEQ_NUMBER, which by induction
     guarantees {left,right}.max < MAX_SEQ_NUMBER note: this treats a
     reader that has not yet replied to a heartbeat as a demoted
     one */
  min_seq = n->seq;
  max_seq = (n->seq < MAX_SEQ_NUMBER) ? n->seq : 0;

  /* 1. Compute {min,max} & have_replied. */
  if (left)
  {
    if (left->min_seq < min_seq)
      min_seq = left->min_seq;
    if (left->max_seq > max_seq)
      max_seq = left->max_seq;
    have_replied = have_replied && left->all_have_replied_to_hb;
  }
  if (right)
  {
    if (right->min_seq < min_seq)
      min_seq = right->min_seq;
    if (right->max_seq > max_seq)
      max_seq = right->max_seq;
    have_replied = have_replied && right->all_have_replied_to_hb;
  }
  n->min_seq = min_seq;
  n->max_seq = max_seq;
  n->all_have_replied_to_hb = have_replied ? 1 : 0;

  /* 2. Compute num_reliable_readers_where_seq_equals_max */
  if (max_seq == 0)
  {
    /* excludes demoted & best-effort readers; note that max == 0
       cannot happen if {left,right}.max > 0 */
    n->num_reliable_readers_where_seq_equals_max = 0;
  }
  else
  {
    /* if demoted or best-effort, seq != max */
    n->num_reliable_readers_where_seq_equals_max =
      (n->seq == max_seq && n->has_replied_to_hb);
    if (left && left->max_seq == max_seq)
      n->num_reliable_readers_where_seq_equals_max +=
        left->num_reliable_readers_where_seq_equals_max;
    if (right && right->max_seq == max_seq)
      n->num_reliable_readers_where_seq_equals_max +=
        right->num_reliable_readers_where_seq_equals_max;
  }

  /* 3. Compute arbitrary unacked reader */
  /* 3a: maybe this reader is itself a candidate */
  if (n->seq < max_seq)
  {
    /* seq < max cannot be true for a best-effort reader or a demoted */
    n->arbitrary_unacked_reader = n->prd_guid;
  }
  else if (n->is_reliable && (n->seq == MAX_SEQ_NUMBER || !n->has_replied_to_hb))
  {
    /* demoted readers and reliable readers that have not yet replied to a heartbeat are candidates */
    n->arbitrary_unacked_reader = n->prd_guid;
  }
  /* 3b: maybe we can inherit from the children */
  else if (left && left->arbitrary_unacked_reader.entityid.u != NN_ENTITYID_UNKNOWN)
  {
    n->arbitrary_unacked_reader = left->arbitrary_unacked_reader;
  }
  else if (right && right->arbitrary_unacked_reader.entityid.u != NN_ENTITYID_UNKNOWN)
  {
    n->arbitrary_unacked_reader = right->arbitrary_unacked_reader;
  }
  /* 3c: else it may be that we can now determine one of our children
     is actually a candidate */
  else if (left && left->max_seq != 0 && left->max_seq < max_seq)
  {
    n->arbitrary_unacked_reader = left->prd_guid;
  }
  else if (right && right->max_seq != 0 && right->max_seq < max_seq)
  {
    n->arbitrary_unacked_reader = right->prd_guid;
  }
  /* 3d: else no candidate in entire subtree */
  else
  {
    n->arbitrary_unacked_reader.entityid.u = NN_ENTITYID_UNKNOWN;
  }
}

seqno_t writer_max_drop_seq (const struct writer *wr)
{
  const struct wr_prd_match *n;
  if (ut_avlIsEmpty (&wr->readers))
    return wr->seq;
  n = ut_avlRootNonEmpty (&wr_readers_treedef, &wr->readers);
  return (n->min_seq == MAX_SEQ_NUMBER) ? wr->seq : n->min_seq;
}

int writer_must_have_hb_scheduled (const struct writer *wr, const struct whc_state *whcst)
{
  if (ut_avlIsEmpty (&wr->readers) || whcst->max_seq < 0)
  {
    /* Can't transmit a valid heartbeat if there is no data; and it
       wouldn't actually be sent anywhere if there are no readers, so
       there is little point in processing the xevent all the time.

       Note that add_msg_to_whc and add_proxy_reader_to_writer will
       perform a reschedule. 8.4.2.2.3: need not (can't, really!) send
       a heartbeat if no data is available. */
    return 0;
  }
  else if (!((const struct wr_prd_match *) ut_avlRootNonEmpty (&wr_readers_treedef, &wr->readers))->all_have_replied_to_hb)
  {
    /* Labouring under the belief that heartbeats must be sent
       regardless of ack state */
    return 1;
  }
  else
  {
    /* DDSI 2.1, section 8.4.2.2.3: need not send heartbeats when all
       messages have been acknowledged.  Slightly different from
       requiring a non-empty whc_seq: if it is transient_local,
       whc_seq usually won't be empty even when all msgs have been
       ack'd. */
    return writer_max_drop_seq (wr) < whcst->max_seq;
  }
}

void writer_set_retransmitting (struct writer *wr)
{
  assert (!wr->retransmitting);
  wr->retransmitting = 1;
  if (config.whc_adaptive && wr->whc_high > wr->whc_low)
  {
    uint32_t m = 8 * wr->whc_high / 10;
    wr->whc_high = (m > wr->whc_low) ? m : wr->whc_low;
  }
}

void writer_clear_retransmitting (struct writer *wr)
{
  wr->retransmitting = 0;
  wr->t_whc_high_upd = wr->t_rexmit_end = now_et();
  os_condBroadcast (&wr->throttle_cond);
}

unsigned remove_acked_messages (struct writer *wr, struct whc_state *whcst, struct whc_node **deferred_free_list)
{
  unsigned n;
  assert (wr->e.guid.entityid.u != NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  ASSERT_MUTEX_HELD (&wr->e.lock);
  n = whc_remove_acked_messages (wr->whc, writer_max_drop_seq (wr), whcst, deferred_free_list);
  /* when transitioning from >= low-water to < low-water, signal
     anyone waiting in throttle_writer() */
  if (wr->throttling && whcst->unacked_bytes <= wr->whc_low)
    os_condBroadcast (&wr->throttle_cond);
  if (wr->retransmitting && whcst->unacked_bytes == 0)
    writer_clear_retransmitting (wr);
  if (wr->state == WRST_LINGERING && whcst->unacked_bytes == 0)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "remove_acked_messages: deleting lingering writer %x:%x:%x:%x\n", PGUID (wr->e.guid));
    delete_writer_nolinger_locked (wr);
  }
  return n;
}

static void new_writer_guid_common_init (struct writer *wr, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc *whc, status_cb_t status_cb, void * status_entity)
{
  os_condInit (&wr->throttle_cond, &wr->e.lock);
  wr->seq = 0;
  wr->cs_seq = 0;
  INIT_SEQ_XMIT(wr, 0);
  wr->hbcount = 0;
  wr->state = WRST_OPERATIONAL;
  wr->hbfragcount = 0;
  writer_hbcontrol_init (&wr->hbcontrol);
  wr->throttling = 0;
  wr->retransmitting = 0;
  wr->t_rexmit_end.v = 0;
  wr->t_whc_high_upd.v = 0;
  wr->num_reliable_readers = 0;
  wr->num_acks_received = 0;
  wr->num_nacks_received = 0;
  wr->throttle_count = 0;
  wr->throttle_tracing = 0;
  wr->rexmit_count = 0;
  wr->rexmit_lost_count = 0;

  wr->status_cb = status_cb;
  wr->status_cb_entity = status_entity;

  /* Copy QoS, merging in defaults */

  wr->xqos = os_malloc (sizeof (*wr->xqos));
  nn_xqos_copy (wr->xqos, xqos);
  nn_xqos_mergein_missing (wr->xqos, &gv.default_xqos_wr);
  assert (wr->xqos->aliased == 0);
  set_topic_type_name (wr->xqos, topic);

  DDS_LOG(DDS_LC_DISCOVERY, "WRITER %x:%x:%x:%x QOS={", PGUID (wr->e.guid));
  nn_log_xqos (DDS_LC_DISCOVERY, wr->xqos);
  DDS_LOG(DDS_LC_DISCOVERY, "}\n");

  assert (wr->xqos->present & QP_RELIABILITY);
  wr->reliable = (wr->xqos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS);
  assert (wr->xqos->present & QP_DURABILITY);
  if (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE))
  {
    assert (wr->xqos->history.kind == NN_KEEP_LAST_HISTORY_QOS);
    assert (wr->xqos->durability.kind == NN_TRANSIENT_LOCAL_DURABILITY_QOS);
    wr->aggressive_keep_last = 1;
  }
  else
  {
    wr->aggressive_keep_last = (config.aggressive_keep_last_whc && wr->xqos->history.kind == NN_KEEP_LAST_HISTORY_QOS);
  }
  wr->handle_as_transient_local = (wr->xqos->durability.kind == NN_TRANSIENT_LOCAL_DURABILITY_QOS);
  wr->include_keyhash =
    config.generate_keyhash &&
    ((wr->e.guid.entityid.u & NN_ENTITYID_KIND_MASK) == NN_ENTITYID_KIND_WRITER_WITH_KEY);
  /* Startup mode causes the writer to treat data in its WHC as if
     transient-local, for the first few seconds after startup of the
     DDSI service. It is done for volatile reliable writers only
     (which automatically excludes all builtin writers) or for all
     writers except volatile best-effort & transient-local ones.

     Which one to use depends on whether merge policies are in effect
     in durability. If yes, then durability will take care of all
     transient & persistent data; if no, DDSI discovery usually takes
     too long and this'll save you.

     Note: may still be cleared, if it turns out we are not maintaining
     an index at all (e.g., volatile KEEP_ALL) */
  if (config.startup_mode_full) {
    wr->startup_mode = gv.startup_mode &&
      (wr->xqos->durability.kind >= NN_TRANSIENT_DURABILITY_QOS ||
       (wr->xqos->durability.kind == NN_VOLATILE_DURABILITY_QOS &&
        wr->xqos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS));
  } else {
    wr->startup_mode = gv.startup_mode &&
      (wr->xqos->durability.kind == NN_VOLATILE_DURABILITY_QOS &&
       wr->xqos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS);
  }
  wr->topic = ddsi_sertopic_ref (topic);
  wr->as = new_addrset ();
  wr->as_group = NULL;

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  {
    unsigned i;
    /* This is an open issue how to encrypt mesages send for various
       partitions that match multiple network partitions.  From a safety
       point of view a wierd configuration. Here we chose the first one
       that we find */
    wr->partition_id = 0;
    for (i = 0; i < wr->xqos->partition.n && wr->partition_id == 0; i++)
      wr->partition_id = get_partitionid_from_mapping (wr->xqos->partition.strs[i], wr->xqos->topic_name);
  }
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_SSM
  /* Writer supports SSM if it is mapped to a network partition for
     which the address set includes an SSM address.  If it supports
     SSM, it arbitrarily selects one SSM address from the address set
     to advertise. */
  wr->supports_ssm = 0;
  wr->ssm_as = NULL;
  if (config.allowMulticast & AMC_SSM)
  {
    nn_locator_t loc;
    int have_loc = 0;
    if (wr->partition_id == 0)
    {
      if (ddsi_is_ssm_mcaddr (&gv.loc_default_mc))
      {
        loc = gv.loc_default_mc;
        have_loc = 1;
      }
    }
    else
    {
      const struct config_networkpartition_listelem *np = find_networkpartition_by_id (wr->partition_id);
      assert (np);
      if (addrset_any_ssm (np->as, &loc))
        have_loc = 1;
    }
    if (have_loc)
    {
      wr->supports_ssm = 1;
      wr->ssm_as = new_addrset ();
      add_to_addrset (wr->ssm_as, &loc);
      DDS_LOG(DDS_LC_DISCOVERY, "writer %x:%x:%x:%x: ssm=%d", PGUID (wr->e.guid), wr->supports_ssm);
      nn_log_addrset (DDS_LC_DISCOVERY, "", wr->ssm_as);
      DDS_LOG(DDS_LC_DISCOVERY, "\n");
    }
  }
#endif

  /* for non-builtin writers, select the eventqueue based on the channel it is mapped to */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  if (!is_builtin_entityid (wr->e.guid.entityid, ownvendorid))
  {
    struct config_channel_listelem *channel = find_channel (wr->xqos->transport_priority);
    DDS_LOG(DDS_LC_DISCOVERY, "writer %x:%x:%x:%x: transport priority %d => channel '%s' priority %d\n",
            PGUID (wr->e.guid), wr->xqos->transport_priority.value, channel->name, channel->priority);
    wr->evq = channel->evq ? channel->evq : gv.xevents;
  }
  else
#endif
  {
    wr->evq = gv.xevents;
  }

  /* heartbeat event will be deleted when the handler can't find a
     writer for it in the hash table. T_NEVER => won't ever be
     scheduled, and this can only change by writing data, which won't
     happen until after it becomes visible. */
  if (wr->reliable)
  {
    nn_mtime_t tsched;
    tsched.v = T_NEVER;
    wr->heartbeat_xevent = qxev_heartbeat (wr->evq, tsched, &wr->e.guid);
  }
  else
  {
    wr->heartbeat_xevent = NULL;
  }
  assert (wr->xqos->present & QP_LIVELINESS);
  if (wr->xqos->liveliness.kind != NN_AUTOMATIC_LIVELINESS_QOS ||
      nn_from_ddsi_duration (wr->xqos->liveliness.lease_duration) != T_NEVER)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "writer %x:%x:%x:%x: incorrectly treating it as of automatic liveliness kind with lease duration = inf (%d, %"PRId64")\n", PGUID (wr->e.guid), (int) wr->xqos->liveliness.kind, nn_from_ddsi_duration (wr->xqos->liveliness.lease_duration));
  }
  wr->lease_duration = T_NEVER; /* FIXME */

  wr->whc = whc;
  if (wr->xqos->history.kind == NN_KEEP_LAST_HISTORY_QOS && wr->aggressive_keep_last)
  {
    /* hdepth > 0 => "aggressive keep last", and in that case: why
       bother blocking for a slow receiver when the entire point of
       KEEP_LAST is to keep going (at least in a typical interpretation
       of the spec. */
    wr->whc_low = wr->whc_high = INT32_MAX;
  }
  else
  {
    wr->whc_low = config.whc_lowwater_mark;
    wr->whc_high = config.whc_init_highwater_mark.value;
  }
  assert (!is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE) || (wr->whc_low == wr->whc_high && wr->whc_low == INT32_MAX));

  /* Connection admin */
  ut_avlInit (&wr_readers_treedef, &wr->readers);
  ut_avlInit (&wr_local_readers_treedef, &wr->local_readers);

  local_reader_ary_init (&wr->rdary);
}

static struct writer *new_writer_guid (const struct nn_guid *guid, const struct nn_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc *whc, status_cb_t status_cb, void *status_entity)
{
  struct writer *wr;
  nn_mtime_t tnow = now_mt ();

  assert (is_writer_entityid (guid->entityid));
  assert (ephash_lookup_writer_guid (guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (guid, topic, xqos);
  wr = os_malloc (sizeof (*wr));

  /* want a pointer to the participant so that a parallel call to
   delete_participant won't interfere with our ability to address
   the participant */

  endpoint_common_init (&wr->e, &wr->c, EK_WRITER, guid, group_guid, pp);
  new_writer_guid_common_init(wr, topic, xqos, whc, status_cb, status_entity);

  /* guid_hash needed for protocol handling, so add it before we send
   out our first message.  Also: needed for matching, and swapping
   the order if hash insert & matching creates a window during which
   neither of two endpoints being created in parallel can discover
   the other. */
  ephash_insert_writer_guid (wr);

  /* once it exists, match it with proxy writers and broadcast
   existence (I don't think it matters much what the order of these
   two is, but it seems likely that match-then-broadcast has a
   slightly lower likelihood that a response from a proxy reader
   gets dropped) -- but note that without adding a lock it might be
   deleted while we do so */
  match_writer_with_proxy_readers (wr, tnow);
  match_writer_with_local_readers (wr, tnow);
  ddsi_plugin.builtintopic_write (&wr->e, now(), true);
  sedp_write_writer (wr);

  if (wr->lease_duration != T_NEVER)
  {
    nn_mtime_t tsched = { 0 };
    resched_xevent_if_earlier (pp->pmd_update_xevent, tsched);
  }

  return wr;
}

struct writer *new_writer (struct nn_guid *wrguid, const struct nn_guid *group_guid, const struct nn_guid *ppguid, const struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc * whc, status_cb_t status_cb, void *status_cb_arg)
{
  struct participant *pp;
  struct writer * wr;

  if ((pp = ephash_lookup_participant_guid (ppguid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "new_writer - participant %x:%x:%x:%x not found\n", PGUID (*ppguid));
    return NULL;
  }
  /* participant can't be freed while we're mucking around cos we are
     awake and do not touch the thread's vtime (ephash_lookup already
     verifies we're awake) */
  wrguid->prefix = pp->e.guid.prefix;
  if (pp_allocate_entityid (&wrguid->entityid, NN_ENTITYID_KIND_WRITER_WITH_KEY, pp) < 0)
    return NULL;
  wr = new_writer_guid (wrguid, group_guid, pp, topic, xqos, whc, status_cb, status_cb_arg);
  return wr;
}

struct local_orphan_writer *new_local_orphan_writer (nn_entityid_t entityid, struct ddsi_sertopic *topic, const struct nn_xqos *xqos, struct whc *whc)
{
  nn_guid_t guid;
  struct local_orphan_writer *lowr;
  struct writer *wr;
  nn_mtime_t tnow = now_mt ();

  DDS_LOG(DDS_LC_DISCOVERY, "new_local_orphan_writer(%s/%s)\n", topic->name, topic->typename);
  lowr = os_malloc (sizeof (*lowr));
  wr = &lowr->wr;

  memset (&guid.prefix, 0, sizeof (guid.prefix));
  guid.entityid = entityid;
  entity_common_init (&wr->e, &guid, NULL, EK_WRITER, now (), NN_VENDORID_ECLIPSE, true);
  wr->c.pp = NULL;
  memset (&wr->c.group_guid, 0, sizeof (wr->c.group_guid));
  new_writer_guid_common_init (wr, topic, xqos, whc, 0, NULL);
  ephash_insert_writer_guid (wr);
  match_writer_with_local_readers (wr, tnow);
  ddsi_plugin.builtintopic_write (&wr->e, now(), true);
  return lowr;
}

static void gc_delete_writer (struct gcreq *gcreq)
{
  struct writer *wr = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_writer(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (wr->e.guid));
  gcreq_free (gcreq);

  /* We now allow GC while blocked on a full WHC, but we still don't allow deleting a writer while blocked on it. The writer's state must be DELETING by the time we get here, and that means the transmit path is no longer blocked. It doesn't imply that the write thread is no longer in throttle_writer(), just that if it is, it will soon return from there. Therefore, block until it isn't throttling anymore. We can safely lock the writer, as we're on the separate GC thread. */
  assert (wr->state == WRST_DELETING);
  assert (!wr->throttling);

  if (wr->heartbeat_xevent)
  {
    wr->hbcontrol.tsched.v = T_NEVER;
    delete_xevent (wr->heartbeat_xevent);
  }

  /* Tear down connections -- no proxy reader can be adding/removing
      us now, because we can't be found via guid_hash anymore.  We
      therefore need not take lock. */

  while (!ut_avlIsEmpty (&wr->readers))
  {
    struct wr_prd_match *m = ut_avlRootNonEmpty (&wr_readers_treedef, &wr->readers);
    ut_avlDelete (&wr_readers_treedef, &wr->readers, m);
    proxy_reader_drop_connection (&m->prd_guid, wr);
    free_wr_prd_match (m);
  }
  while (!ut_avlIsEmpty (&wr->local_readers))
  {
    struct wr_rd_match *m = ut_avlRootNonEmpty (&wr_local_readers_treedef, &wr->local_readers);
    ut_avlDelete (&wr_local_readers_treedef, &wr->local_readers, m);
    reader_drop_local_connection (&m->rd_guid, wr);
    free_wr_rd_match (m);
  }

  /* Do last gasp on SEDP and free writer. */
  if (!is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE))
    sedp_dispose_unregister_writer (wr);
  if (wr->status_cb)
  {
    (wr->status_cb) (wr->status_cb_entity, NULL);
  }

  whc_free (wr->whc);
#ifdef DDSI_INCLUDE_SSM
  if (wr->ssm_as)
    unref_addrset (wr->ssm_as);
#endif
  unref_addrset (wr->as); /* must remain until readers gone (rebuilding of addrset) */
  nn_xqos_fini (wr->xqos);
  os_free (wr->xqos);
  local_reader_ary_fini (&wr->rdary);
  os_condDestroy (&wr->throttle_cond);

  ddsi_sertopic_unref ((struct ddsi_sertopic *) wr->topic);
  endpoint_common_fini (&wr->e, &wr->c);
  os_free (wr);
}

static void gc_delete_writer_throttlewait (struct gcreq *gcreq)
{
  struct writer *wr = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_writer_throttlewait(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (wr->e.guid));
  /* We now allow GC while blocked on a full WHC, but we still don't allow deleting a writer while blocked on it. The writer's state must be DELETING by the time we get here, and that means the transmit path is no longer blocked. It doesn't imply that the write thread is no longer in throttle_writer(), just that if it is, it will soon return from there. Therefore, block until it isn't throttling anymore. We can safely lock the writer, as we're on the separate GC thread. */
  assert (wr->state == WRST_DELETING);
  os_mutexLock (&wr->e.lock);
  while (wr->throttling)
    os_condWait (&wr->throttle_cond, &wr->e.lock);
  os_mutexUnlock (&wr->e.lock);
  gcreq_requeue (gcreq, gc_delete_writer);
}

static void writer_set_state (struct writer *wr, enum writer_state newstate)
{
  ASSERT_MUTEX_HELD (&wr->e.lock);
  DDS_LOG(DDS_LC_DISCOVERY, "writer_set_state(%x:%x:%x:%x) state transition %d -> %d\n", PGUID (wr->e.guid), wr->state, newstate);
  assert (newstate > wr->state);
  if (wr->state == WRST_OPERATIONAL)
  {
    /* Unblock all throttled writers (alternative method: clear WHC --
       but with parallel writes and very small limits on the WHC size,
       that doesn't guarantee no-one will block). A truly blocked
       write() is a problem because it prevents the gc thread from
       cleaning up the writer.  (Note: late assignment to wr->state is
       ok, 'tis all protected by the writer lock.) */
    os_condBroadcast (&wr->throttle_cond);
  }
  wr->state = newstate;
}

int delete_writer_nolinger_locked (struct writer *wr)
{
  DDS_LOG(DDS_LC_DISCOVERY, "delete_writer_nolinger(guid %x:%x:%x:%x) ...\n", PGUID (wr->e.guid));
  ASSERT_MUTEX_HELD (&wr->e.lock);
  ddsi_plugin.builtintopic_write (&wr->e, now(), false);
  local_reader_ary_setinvalid (&wr->rdary);
  ephash_remove_writer_guid (wr);
  writer_set_state (wr, WRST_DELETING);
  gcreq_writer (wr);
  return 0;
}

int delete_writer_nolinger (const struct nn_guid *guid)
{
  struct writer *wr;
  /* We take no care to ensure application writers are not deleted
     while they still have unacknowledged data (unless it takes too
     long), but we don't care about the DDSI built-in writers: we deal
     with that anyway because of the potential for crashes of remote
     DDSI participants. But it would be somewhat more elegant to do it
     differently. */
  assert (is_writer_entityid (guid->entityid));
  if ((wr = ephash_lookup_writer_guid (guid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "delete_writer_nolinger(guid %x:%x:%x:%x) - unknown guid\n", PGUID (*guid));
    return ERR_UNKNOWN_ENTITY;
  }
  DDS_LOG(DDS_LC_DISCOVERY, "delete_writer_nolinger(guid %x:%x:%x:%x) ...\n", PGUID (*guid));
  os_mutexLock (&wr->e.lock);
  delete_writer_nolinger_locked (wr);
  os_mutexUnlock (&wr->e.lock);
  return 0;
}

void delete_local_orphan_writer (struct local_orphan_writer *lowr)
{
  os_mutexLock (&lowr->wr.e.lock);
  delete_writer_nolinger_locked (&lowr->wr);
  os_mutexUnlock (&lowr->wr.e.lock);
}

int delete_writer (const struct nn_guid *guid)
{
  struct writer *wr;
  struct whc_state whcst;
  if ((wr = ephash_lookup_writer_guid (guid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "delete_writer(guid %x:%x:%x:%x) - unknown guid\n", PGUID (*guid));
    return ERR_UNKNOWN_ENTITY;
  }
  DDS_LOG(DDS_LC_DISCOVERY, "delete_writer(guid %x:%x:%x:%x) ...\n", PGUID (*guid));
  os_mutexLock (&wr->e.lock);

  /* If no unack'ed data, don't waste time or resources (expected to
     be the usual case), do it immediately.  If more data is still
     coming in (which can't really happen at the moment, but might
     again in the future) it'll potentially be discarded.  */
  whc_get_state(wr->whc, &whcst);
  if (whcst.unacked_bytes == 0)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "delete_writer(guid %x:%x:%x:%x) - no unack'ed samples\n", PGUID (*guid));
    delete_writer_nolinger_locked (wr);
    os_mutexUnlock (&wr->e.lock);
  }
  else
  {
    nn_mtime_t tsched;
    int tsec, tusec;
    writer_set_state (wr, WRST_LINGERING);
    os_mutexUnlock (&wr->e.lock);
    tsched = add_duration_to_mtime (now_mt (), config.writer_linger_duration);
    mtime_to_sec_usec (&tsec, &tusec, tsched);
    DDS_LOG(DDS_LC_DISCOVERY, "delete_writer(guid %x:%x:%x:%x) - unack'ed samples, will delete when ack'd or at t = %d.%06d\n",
            PGUID (*guid), tsec, tusec);
    qxev_delete_writer (tsched, &wr->e.guid);
  }
  return 0;
}

void writer_exit_startup_mode (struct writer *wr)
{
  struct whc_node *deferred_free_list = NULL;
  os_mutexLock (&wr->e.lock);
  if (wr->startup_mode)
  {
    unsigned cnt = 0;
    struct whc_state whcst;
    wr->startup_mode = 0;
    cnt += remove_acked_messages (wr, &whcst, &deferred_free_list);
    cnt += whc_downgrade_to_volatile (wr->whc, &whcst);
    writer_clear_retransmitting (wr);
    DDS_LOG(DDS_LC_DISCOVERY, "  %x:%x:%x:%x: dropped %u samples\n", PGUID(wr->e.guid), cnt);
  }
  os_mutexUnlock (&wr->e.lock);
  whc_free_deferred_free_list (wr->whc, deferred_free_list);
}

uint64_t writer_instance_id (const struct nn_guid *guid)
{
    struct entity_common *e;
    e = (struct entity_common*)ephash_lookup_writer_guid(guid);
    if (e) {
        return e->iid;
    }
    e = (struct entity_common*)ephash_lookup_proxy_writer_guid(guid);
    if (e) {
        return e->iid;
    }
    return 0;
}

/* READER ----------------------------------------------------------- */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static struct addrset * get_as_from_mapping (const char *partition, const char *topic)
{
  struct config_partitionmapping_listelem *pm;
  struct addrset *as = new_addrset ();
  if ((pm = find_partitionmapping (partition, topic)) != NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "matched reader for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n", topic, partition, pm->networkPartition);
    assert (pm->partition->as);
    copy_addrset_into_addrset (as, pm->partition->as);
  }
  return as;
}

static void join_mcast_helper (const nn_locator_t *n, void * varg)
{
  ddsi_tran_conn_t conn = (ddsi_tran_conn_t) varg;
  if (ddsi_is_mcaddr (n))
  {
    if (n->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
    {
      if (ddsi_join_mc (conn, NULL, n) < 0)
      {
        DDS_LOG(DDS_LC_WARNING, "failed to join network partition multicast group\n");
      }
    }
    else /* join all addresses that include this node */
    {
      {
        nn_locator_t l = *n;
        nn_udpv4mcgen_address_t l1;
        uint32_t iph;
        unsigned i;
        memcpy(&l1, l.address, sizeof(l1));
        l.kind = NN_LOCATOR_KIND_UDPv4;
        memset(l.address, 0, 12);
        iph = ntohl(l1.ipv4.s_addr);
        for (i = 1; i < (1u << l1.count); i++)
        {
          uint32_t ipn, iph1 = iph;
          if (i & (1u << l1.idx))
          {
            iph1 |= (i << l1.base);
            ipn = htonl(iph1);
            memcpy(l.address + 12, &ipn, 4);
            if (ddsi_join_mc (conn, NULL, &l) < 0)
            {
              DDS_LOG(DDS_LC_WARNING, "failed to join network partition multicast group\n");
            }
          }
        }
      }
    }
  }
}

static void leave_mcast_helper (const nn_locator_t *n, void * varg)
{
  ddsi_tran_conn_t conn = (ddsi_tran_conn_t) varg;
  if (ddsi_is_mcaddr (n))
  {
    if (n->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
    {
      if (ddsi_leave_mc (conn, NULL, n) < 0)
      {
        DDS_LOG(DDS_LC_WARNING, "failed to leave network partition multicast group\n");
      }
    }
    else /* join all addresses that include this node */
    {
      {
        nn_locator_t l = *n;
        nn_udpv4mcgen_address_t l1;
        uint32_t iph;
        unsigned i;
        memcpy(&l1, l.address, sizeof(l1));
        l.kind = NN_LOCATOR_KIND_UDPv4;
        memset(l.address, 0, 12);
        iph = ntohl(l1.ipv4.s_addr);
        for (i = 1; i < (1u << l1.count); i++)
        {
          uint32_t ipn, iph1 = iph;
          if (i & (1u << l1.idx))
          {
            iph1 |= (i << l1.base);
            ipn = htonl(iph1);
            memcpy(l.address + 12, &ipn, 4);
            if (ddsi_leave_mc (conn, NULL, &l) < 0)
            {
              DDS_LOG(DDS_LC_WARNING, "failed to leave network partition multicast group\n");
            }
          }
        }
      }
    }
  }
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

static struct reader * new_reader_guid
(
  const struct nn_guid *guid,
  const struct nn_guid *group_guid,
  struct participant *pp,
  const struct ddsi_sertopic *topic,
  const struct nn_xqos *xqos,
  struct rhc *rhc,
  status_cb_t status_cb,
  void * status_entity
)
{
  /* see new_writer_guid for commenets */

  struct reader * rd;
  nn_mtime_t tnow = now_mt ();

  assert (!is_writer_entityid (guid->entityid));
  assert (ephash_lookup_reader_guid (guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (guid, topic, xqos);
  rd = os_malloc (sizeof (*rd));

  endpoint_common_init (&rd->e, &rd->c, EK_READER, guid, group_guid, pp);

  /* Copy QoS, merging in defaults */
  rd->xqos = os_malloc (sizeof (*rd->xqos));
  nn_xqos_copy (rd->xqos, xqos);
  nn_xqos_mergein_missing (rd->xqos, &gv.default_xqos_rd);
  assert (rd->xqos->aliased == 0);
  set_topic_type_name (rd->xqos, topic);

  if (dds_get_log_mask() & DDS_LC_DISCOVERY)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "READER %x:%x:%x:%x QOS={", PGUID (rd->e.guid));
    nn_log_xqos (DDS_LC_DISCOVERY, rd->xqos);
    DDS_LOG(DDS_LC_DISCOVERY, "}\n");
  }
  assert (rd->xqos->present & QP_RELIABILITY);
  rd->reliable = (rd->xqos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS);
  assert (rd->xqos->present & QP_DURABILITY);
  rd->handle_as_transient_local = (rd->xqos->durability.kind == NN_TRANSIENT_LOCAL_DURABILITY_QOS);
  rd->topic = ddsi_sertopic_ref (topic);
  rd->ddsi2direct_cb = 0;
  rd->ddsi2direct_cbarg = 0;
  rd->init_acknack_count = 0;
#ifdef DDSI_INCLUDE_SSM
  rd->favours_ssm = 0;
#endif
  if (topic == NULL)
  {
    assert (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE));
  }
  rd->status_cb = status_cb;
  rd->status_cb_entity = status_entity;
  rd->rhc = rhc;
  /* set rhc qos for reader */
  if (rhc)
  {
    (ddsi_plugin.rhc_plugin.rhc_set_qos_fn) (rd->rhc, rd->xqos);
  }
  assert (rd->xqos->present & QP_LIVELINESS);
  if (rd->xqos->liveliness.kind != NN_AUTOMATIC_LIVELINESS_QOS ||
      nn_from_ddsi_duration (rd->xqos->liveliness.lease_duration) != T_NEVER)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "reader %x:%x:%x:%x: incorrectly treating it as of automatic liveliness kind with lease duration = inf (%d, %"PRId64")\n", PGUID (rd->e.guid), (int) rd->xqos->liveliness.kind, nn_from_ddsi_duration (rd->xqos->liveliness.lease_duration));
  }

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  rd->as = new_addrset ();
  if (config.allowMulticast & ~AMC_SPDP)
  {
    unsigned i;

    /* compile address set from the mapped network partitions */
    for (i = 0; i < rd->xqos->partition.n; i++)
    {
      struct addrset *pas = get_as_from_mapping (rd->xqos->partition.strs[i], rd->xqos->topic_name);
      if (pas)
      {
#ifdef DDSI_INCLUDE_SSM
        copy_addrset_into_addrset_no_ssm (rd->as, pas);
        if (addrset_contains_ssm (pas) && config.allowMulticast & AMC_SSM)
          rd->favours_ssm = 1;
#else
        copy_addrset_into_addrset (rd->as, pas);
#endif
        unref_addrset (pas);
      }
    }
    if (!addrset_empty (rd->as))
    {
      /* Iterate over all udp addresses:
       *   - Set the correct portnumbers
       *   - Join the socket if a multicast address
       */
      addrset_forall (rd->as, join_mcast_helper, gv.data_conn_mc);
      if (dds_get_log_mask() & DDS_LC_DISCOVERY)
      {
        DDS_LOG(DDS_LC_DISCOVERY, "READER %x:%x:%x:%x locators={", PGUID (rd->e.guid));
        nn_log_addrset(DDS_LC_DISCOVERY, "", rd->as);
        DDS_LOG(DDS_LC_DISCOVERY, "}\n");
      }
    }
#ifdef DDSI_INCLUDE_SSM
    else
    {
      /* Note: SSM requires NETWORK_PARTITIONS; if network partitions
         do not override the default, we should check whether the
         default is an SSM address. */
      if (ddsi_is_ssm_mcaddr (&gv.loc_default_mc) && config.allowMulticast & AMC_SSM)
        rd->favours_ssm = 1;
    }
#endif
  }
#ifdef DDSI_INCLUDE_SSM
  if (rd->favours_ssm)
    DDS_LOG(DDS_LC_DISCOVERY, "READER %x:%x:%x:%x ssm=%d\n", PGUID (rd->e.guid), rd->favours_ssm);
#endif
#endif

  ut_avlInit (&rd_writers_treedef, &rd->writers);
  ut_avlInit (&rd_local_writers_treedef, &rd->local_writers);

  ephash_insert_reader_guid (rd);
  match_reader_with_proxy_writers (rd, tnow);
  match_reader_with_local_writers (rd, tnow);
  ddsi_plugin.builtintopic_write (&rd->e, now(), true);
  sedp_write_reader (rd);
  return rd;
}

struct reader * new_reader
(
  struct nn_guid *rdguid,
  const struct nn_guid *group_guid,
  const struct nn_guid *ppguid,
  const struct ddsi_sertopic *topic,
  const struct nn_xqos *xqos,
  struct rhc * rhc,
  status_cb_t status_cb,
  void * status_cbarg
)
{
  struct participant * pp;
  struct reader * rd;

  if ((pp = ephash_lookup_participant_guid (ppguid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "new_reader - participant %x:%x:%x:%x not found\n", PGUID (*ppguid));
    return NULL;
  }
  rdguid->prefix = pp->e.guid.prefix;
  if (pp_allocate_entityid (&rdguid->entityid, NN_ENTITYID_KIND_READER_WITH_KEY, pp) < 0)
    return NULL;
  rd = new_reader_guid (rdguid, group_guid, pp, topic, xqos, rhc, status_cb, status_cbarg);
  return rd;
}

static void gc_delete_reader (struct gcreq *gcreq)
{
  /* see gc_delete_writer for comments */
  struct reader *rd = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_reader(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (rd->e.guid));
  gcreq_free (gcreq);

  while (!ut_avlIsEmpty (&rd->writers))
  {
    struct rd_pwr_match *m = ut_avlRootNonEmpty (&rd_writers_treedef, &rd->writers);
    ut_avlDelete (&rd_writers_treedef, &rd->writers, m);
    proxy_writer_drop_connection (&m->pwr_guid, rd);
    free_rd_pwr_match (m);
  }
  while (!ut_avlIsEmpty (&rd->local_writers))
  {
    struct rd_wr_match *m = ut_avlRootNonEmpty (&rd_local_writers_treedef, &rd->local_writers);
    ut_avlDelete (&rd_local_writers_treedef, &rd->local_writers, m);
    writer_drop_local_connection (&m->wr_guid, rd);
    free_rd_wr_match (m);
  }

  if (!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE))
    sedp_dispose_unregister_reader (rd);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  addrset_forall (rd->as, leave_mcast_helper, gv.data_conn_mc);
#endif
  if (rd->rhc)
  {
    (ddsi_plugin.rhc_plugin.rhc_free_fn) (rd->rhc);
  }
  if (rd->status_cb)
  {
    (rd->status_cb) (rd->status_cb_entity, NULL);
  }
  ddsi_sertopic_unref ((struct ddsi_sertopic *) rd->topic);

  nn_xqos_fini (rd->xqos);
  os_free (rd->xqos);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  unref_addrset (rd->as);
#endif

  endpoint_common_fini (&rd->e, &rd->c);
  os_free (rd);
}

int delete_reader (const struct nn_guid *guid)
{
  struct reader *rd;
  assert (!is_writer_entityid (guid->entityid));
  if ((rd = ephash_lookup_reader_guid (guid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "delete_reader_guid(guid %x:%x:%x:%x) - unknown guid\n", PGUID (*guid));
    return ERR_UNKNOWN_ENTITY;
  }
  if (rd->rhc)
  {
    (ddsi_plugin.rhc_plugin.rhc_fini_fn) (rd->rhc);
  }
  DDS_LOG(DDS_LC_DISCOVERY, "delete_reader_guid(guid %x:%x:%x:%x) ...\n", PGUID (*guid));
  ddsi_plugin.builtintopic_write (&rd->e, now(), false);
  ephash_remove_reader_guid (rd);
  gcreq_reader (rd);
  return 0;
}

uint64_t reader_instance_id (const struct nn_guid *guid)
{
    struct entity_common *e;
    e = (struct entity_common*)ephash_lookup_reader_guid(guid);
    if (e) {
        return e->iid;
    }
    e = (struct entity_common*)ephash_lookup_proxy_reader_guid(guid);
    if (e) {
        return e->iid;
    }
    return 0;
}


/* PROXY-PARTICIPANT ------------------------------------------------ */
static void gc_proxy_participant_lease (struct gcreq *gcreq)
{
  lease_free (gcreq->arg);
  gcreq_free (gcreq);
}

void proxy_participant_reassign_lease (struct proxy_participant *proxypp, struct lease *newlease)
{
  /* Lease renewal is done by the receive thread without locking the
     proxy participant (and I'd like to keep it that way), but that
     means we must guarantee that the lease pointer remains valid once
     loaded.

     By loading/storing the pointer atomically, we ensure we always
     read a valid (or once valid) value, by delaying the freeing
     through the garbage collector, we ensure whatever lease update
     occurs in parallel completes before the memory is released.

     The lease_renew(never) call ensures the lease will never expire
     while we are messing with it. */
  os_mutexLock (&proxypp->e.lock);
  if (proxypp->owns_lease)
  {
    const nn_etime_t never = { T_NEVER };
    struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_proxy_participant_lease);
    struct lease *oldlease = os_atomic_ldvoidp (&proxypp->lease);
    lease_renew (oldlease, never);
    gcreq->arg = oldlease;
    gcreq_enqueue (gcreq);
    proxypp->owns_lease = 0;
  }
  os_atomic_stvoidp (&proxypp->lease, newlease);
  os_mutexUnlock (&proxypp->e.lock);
}

void new_proxy_participant
(
  const struct nn_guid *ppguid,
  unsigned bes,
  unsigned prismtech_bes,
  const struct nn_guid *privileged_pp_guid,
  struct addrset *as_default,
  struct addrset *as_meta,
  const nn_plist_t *plist,
  int64_t tlease_dur,
  nn_vendorid_t vendor,
  unsigned custom_flags,
  nn_wctime_t timestamp
)
{
  /* No locking => iff all participants use unique guids, and sedp
     runs on a single thread, it can't go wrong. FIXME, maybe? The
     same holds for the other functions for creating entities. */
  struct proxy_participant *proxypp;

  assert (ppguid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (ephash_lookup_proxy_participant_guid (ppguid) == NULL);
  assert (privileged_pp_guid == NULL || privileged_pp_guid->entityid.u == NN_ENTITYID_PARTICIPANT);

  prune_deleted_participant_guids (now_mt ());

  proxypp = os_malloc (sizeof (*proxypp));

  entity_common_init (&proxypp->e, ppguid, "", EK_PROXY_PARTICIPANT, timestamp, vendor, false);
  proxypp->refc = 1;
  proxypp->lease_expired = 0;
  proxypp->vendor = vendor;
  proxypp->bes = bes;
  proxypp->prismtech_bes = prismtech_bes;
  if (privileged_pp_guid) {
    proxypp->privileged_pp_guid = *privileged_pp_guid;
  } else {
    memset (&proxypp->privileged_pp_guid.prefix, 0, sizeof (proxypp->privileged_pp_guid.prefix));
    proxypp->privileged_pp_guid.entityid.u = NN_ENTITYID_PARTICIPANT;
  }
  if ((plist->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) &&
      (plist->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_DDSI2_PARTICIPANT_FLAG) &&
      (plist->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2))
    proxypp->is_ddsi2_pp = 1;
  else
    proxypp->is_ddsi2_pp = 0;
  if ((plist->present & PP_PRISMTECH_PARTICIPANT_VERSION_INFO) &&
      (plist->prismtech_participant_version_info.flags & NN_PRISMTECH_FL_MINIMAL_BES_MODE))
    proxypp->minimal_bes_mode = 1;
  else
    proxypp->minimal_bes_mode = 0;

  {
    struct proxy_participant *privpp;
    privpp = ephash_lookup_proxy_participant_guid (&proxypp->privileged_pp_guid);
    if (privpp != NULL && privpp->is_ddsi2_pp)
    {
      os_atomic_stvoidp (&proxypp->lease, os_atomic_ldvoidp (&privpp->lease));
      proxypp->owns_lease = 0;
    }
    else
    {
      /* Lease duration is meaningless when the lease never expires, but when proxy participants are created implicitly because of endpoint discovery from a cloud service, we do want the lease to expire eventually when the cloud discovery service disappears and never reappears. The normal data path renews the lease, so if the lease expiry is changed after the DS disappears but data continues to flow (even if it is only a single sample) the proxy participant would immediately go back to a non-expiring lease with no further triggers for deleting it. Instead, we take tlease_dur == NEVER as a special value meaning a lease that doesn't expire now and that has a "reasonable" lease duration. That way the lease renewal in the data path is fine, and we only need to do something special in SEDP handling. */
      nn_etime_t texp = add_duration_to_etime (now_et(), tlease_dur);
      int64_t dur = (tlease_dur == T_NEVER) ? config.lease_duration : tlease_dur;
      os_atomic_stvoidp (&proxypp->lease, lease_new (texp, dur, &proxypp->e));
      proxypp->owns_lease = 1;
    }
  }

  proxypp->as_default = as_default;
  proxypp->as_meta = as_meta;
  proxypp->endpoints = NULL;
  proxypp->plist = nn_plist_dup (plist);
  ut_avlInit (&proxypp_groups_treedef, &proxypp->groups);


  if (custom_flags & CF_INC_KERNEL_SEQUENCE_NUMBERS)
    proxypp->kernel_sequence_numbers = 1;
  else
    proxypp->kernel_sequence_numbers = 0;
  if (custom_flags & CF_IMPLICITLY_CREATED_PROXYPP)
    proxypp->implicitly_created = 1;
  else
    proxypp->implicitly_created = 0;

  if (custom_flags & CF_PROXYPP_NO_SPDP)
    proxypp->proxypp_have_spdp = 0;
  else
    proxypp->proxypp_have_spdp = 1;
  /* Non-PrismTech doesn't implement the PT extensions and therefore won't generate
     a CMParticipant; if a PT peer does not implement a CMParticipant writer, then it
     presumably also is a handicapped implementation (perhaps simply an old one) */
  if (!vendor_is_eclipse_or_prismtech(proxypp->vendor) ||
      (proxypp->bes != 0 && !(proxypp->prismtech_bes & NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER)))
    proxypp->proxypp_have_cm = 1;
  else
    proxypp->proxypp_have_cm = 0;

  /* Proxy participant must be in the hash tables for
     new_proxy_{writer,reader} to work */
  ephash_insert_proxy_participant_guid (proxypp);

  /* Add proxy endpoints based on the advertised (& possibly augmented
     ...) built-in endpoint set. */
  {
#define PT_TE(ap_, a_, bp_, b_) { 0, NN_##ap_##BUILTIN_ENDPOINT_##a_, NN_ENTITYID_##bp_##_BUILTIN_##b_ }
#define TE(ap_, a_, bp_, b_) { NN_##ap_##BUILTIN_ENDPOINT_##a_, 0, NN_ENTITYID_##bp_##_BUILTIN_##b_ }
#define LTE(a_, bp_, b_) { NN_##BUILTIN_ENDPOINT_##a_, 0, NN_ENTITYID_##bp_##_BUILTIN_##b_ }
    static const struct bestab {
      unsigned besflag;
      unsigned prismtech_besflag;
      unsigned entityid;
    } bestab[] = {
#if 0
      /* SPDP gets special treatment => no need for proxy
         writers/readers */
      TE (DISC_, PARTICIPANT_ANNOUNCER, SPDP, PARTICIPANT_WRITER),
#endif
      TE (DISC_, PARTICIPANT_DETECTOR, SPDP, PARTICIPANT_READER),
      TE (DISC_, PUBLICATION_ANNOUNCER, SEDP, PUBLICATIONS_WRITER),
      TE (DISC_, PUBLICATION_DETECTOR, SEDP, PUBLICATIONS_READER),
      TE (DISC_, SUBSCRIPTION_ANNOUNCER, SEDP, SUBSCRIPTIONS_WRITER),
      TE (DISC_, SUBSCRIPTION_DETECTOR, SEDP, SUBSCRIPTIONS_READER),
      LTE (PARTICIPANT_MESSAGE_DATA_WRITER, P2P, PARTICIPANT_MESSAGE_WRITER),
      LTE (PARTICIPANT_MESSAGE_DATA_READER, P2P, PARTICIPANT_MESSAGE_READER),
      TE (DISC_, TOPIC_ANNOUNCER, SEDP, TOPIC_WRITER),
      TE (DISC_, TOPIC_DETECTOR, SEDP, TOPIC_READER),
      PT_TE (DISC_, CM_PARTICIPANT_READER, SEDP, CM_PARTICIPANT_READER),
      PT_TE (DISC_, CM_PARTICIPANT_WRITER, SEDP, CM_PARTICIPANT_WRITER),
      PT_TE (DISC_, CM_PUBLISHER_READER, SEDP, CM_PUBLISHER_READER),
      PT_TE (DISC_, CM_PUBLISHER_WRITER, SEDP, CM_PUBLISHER_WRITER),
      PT_TE (DISC_, CM_SUBSCRIBER_READER, SEDP, CM_SUBSCRIBER_READER),
      PT_TE (DISC_, CM_SUBSCRIBER_WRITER, SEDP, CM_SUBSCRIBER_WRITER)
    };
#undef PT_TE
#undef TE
#undef LTE
    nn_plist_t plist_rd, plist_wr;
    int i;
    /* Note: no entity name or group GUID supplied, but that shouldn't
       matter, as these are internal to DDSI and don't use group
       coherency */
    nn_plist_init_empty (&plist_wr);
    nn_plist_init_empty (&plist_rd);
    nn_xqos_copy (&plist_wr.qos, &gv.builtin_endpoint_xqos_wr);
    nn_xqos_copy (&plist_rd.qos, &gv.builtin_endpoint_xqos_rd);
    for (i = 0; i < (int) (sizeof (bestab) / sizeof (*bestab)); i++)
    {
      const struct bestab *te = &bestab[i];
      if ((proxypp->bes & te->besflag) || (proxypp->prismtech_bes & te->prismtech_besflag))
      {
        nn_guid_t guid1;
        guid1.prefix = proxypp->e.guid.prefix;
        guid1.entityid.u = te->entityid;
        assert (is_builtin_entityid (guid1.entityid, proxypp->vendor));
        if (is_writer_entityid (guid1.entityid))
        {
          new_proxy_writer (ppguid, &guid1, proxypp->as_meta, &plist_wr, gv.builtins_dqueue, gv.xevents, timestamp);
        }
        else
        {
#ifdef DDSI_INCLUDE_SSM
          const int ssm = addrset_contains_ssm (proxypp->as_meta);
          new_proxy_reader (ppguid, &guid1, proxypp->as_meta, &plist_rd, timestamp, ssm);
#else
          new_proxy_reader (ppguid, &guid1, proxypp->as_meta, &plist_rd, timestamp);
#endif
        }
      }
    }
    nn_plist_fini (&plist_wr);
    nn_plist_fini (&plist_rd);
  }

  /* Register lease, but be careful not to accidentally re-register
     DDSI2's lease, as we may have become dependent on DDSI2 any time
     after ephash_insert_proxy_participant_guid even if
     privileged_pp_guid was NULL originally */
  os_mutexLock (&proxypp->e.lock);

  if (proxypp->owns_lease)
    lease_register (os_atomic_ldvoidp (&proxypp->lease));

  ddsi_plugin.builtintopic_write (&proxypp->e, timestamp, true);
  os_mutexUnlock (&proxypp->e.lock);
}

int update_proxy_participant_plist_locked (struct proxy_participant *proxypp, const struct nn_plist *datap, enum update_proxy_participant_source source, nn_wctime_t timestamp)
{
  /* Currently, built-in processing is single-threaded, and it is only through this function and the proxy participant deletion (which necessarily happens when no-one else potentially references the proxy participant anymore).  So at the moment, the lock is superfluous. */
  nn_plist_t *new_plist;

  new_plist = nn_plist_dup (datap);
  nn_plist_mergein_missing (new_plist, proxypp->plist);
  nn_plist_fini (proxypp->plist);
  os_free (proxypp->plist);
  proxypp->plist = new_plist;

  switch (source)
  {
    case UPD_PROXYPP_SPDP:
      ddsi_plugin.builtintopic_write (&proxypp->e, timestamp, true);
      proxypp->proxypp_have_spdp = 1;
      break;
    case UPD_PROXYPP_CM:
      proxypp->proxypp_have_cm = 1;
      break;
  }

  return 0;
}

int update_proxy_participant_plist (struct proxy_participant *proxypp, const struct nn_plist *datap, enum update_proxy_participant_source source, nn_wctime_t timestamp)
{
  nn_plist_t tmp;

  /* FIXME: find a better way of restricting which bits can get updated */
  os_mutexLock (&proxypp->e.lock);
  switch (source)
  {
    case UPD_PROXYPP_SPDP:
      update_proxy_participant_plist_locked (proxypp, datap, source, timestamp);
      break;
    case UPD_PROXYPP_CM:
      tmp = *datap;
      tmp.present &=
        PP_PRISMTECH_NODE_NAME | PP_PRISMTECH_EXEC_NAME | PP_PRISMTECH_PROCESS_ID |
        PP_PRISMTECH_WATCHDOG_SCHEDULING | PP_PRISMTECH_LISTENER_SCHEDULING |
        PP_PRISMTECH_SERVICE_TYPE | PP_ENTITY_NAME;
      tmp.qos.present &= QP_PRISMTECH_ENTITY_FACTORY;
      update_proxy_participant_plist_locked (proxypp, &tmp, source, timestamp);
      break;
  }
  os_mutexUnlock (&proxypp->e.lock);
  return 0;
}

static void ref_proxy_participant (struct proxy_participant *proxypp, struct proxy_endpoint_common *c)
{
  os_mutexLock (&proxypp->e.lock);
  c->proxypp = proxypp;
  proxypp->refc++;

  c->next_ep = proxypp->endpoints;
  c->prev_ep = NULL;
  if (c->next_ep)
  {
    c->next_ep->prev_ep = c;
  }
  proxypp->endpoints = c;
  os_mutexUnlock (&proxypp->e.lock);
}

static void unref_proxy_participant (struct proxy_participant *proxypp, struct proxy_endpoint_common *c)
{
  uint32_t refc;
  const nn_wctime_t tnow = now();

  os_mutexLock (&proxypp->e.lock);
  refc = --proxypp->refc;

  if (c != NULL)
  {
    if (c->next_ep)
      c->next_ep->prev_ep = c->prev_ep;
    if (c->prev_ep)
      c->prev_ep->next_ep = c->next_ep;
    else
      proxypp->endpoints = c->next_ep;
  }

  if (refc == 0)
  {
    assert (proxypp->endpoints == NULL);
    os_mutexUnlock (&proxypp->e.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "unref_proxy_participant(%x:%x:%x:%x): refc=0, freeing\n", PGUID (proxypp->e.guid));


    unref_addrset (proxypp->as_default);
    unref_addrset (proxypp->as_meta);
    nn_plist_fini (proxypp->plist);
    os_free (proxypp->plist);
    if (proxypp->owns_lease)
      lease_free (os_atomic_ldvoidp (&proxypp->lease));
    entity_common_fini (&proxypp->e);
    remove_deleted_participant_guid (&proxypp->e.guid, DPG_LOCAL | DPG_REMOTE);
    os_free (proxypp);
  }
  else if (proxypp->endpoints == NULL && proxypp->implicitly_created)
  {
    assert (refc == 1);
    os_mutexUnlock (&proxypp->e.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "unref_proxy_participant(%x:%x:%x:%x): refc=%u, no endpoints, implicitly created, deleting\n", PGUID (proxypp->e.guid), (unsigned) refc);
    delete_proxy_participant_by_guid(&proxypp->e.guid, tnow, 1);
    /* Deletion is still (and has to be) asynchronous. A parallel endpoint creation may or may not
       succeed, and if it succeeds it will be deleted along with the proxy participant. So "your
       mileage may vary". Also, the proxy participant may be blacklisted for a little ... */
  }
  else
  {
    os_mutexUnlock (&proxypp->e.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "unref_proxy_participant(%x:%x:%x:%x): refc=%u\n", PGUID (proxypp->e.guid), (unsigned) refc);
  }
}

static void gc_delete_proxy_participant (struct gcreq *gcreq)
{
  struct proxy_participant *proxypp = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_proxy_participant(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (proxypp->e.guid));
  gcreq_free (gcreq);
  unref_proxy_participant (proxypp, NULL);
}

static struct entity_common *entity_common_from_proxy_endpoint_common (const struct proxy_endpoint_common *c)
{
  assert (offsetof (struct proxy_writer, e) == 0);
  assert (offsetof (struct proxy_reader, e) == offsetof (struct proxy_writer, e));
  assert (offsetof (struct proxy_reader, c) == offsetof (struct proxy_writer, c));
  assert (c != NULL);
  return (struct entity_common *) ((char *) c - offsetof (struct proxy_writer, c));
}

static void delete_or_detach_dependent_pp (struct proxy_participant *p, struct proxy_participant *proxypp, nn_wctime_t timestamp, int isimplicit)
{
  os_mutexLock (&p->e.lock);
  if (memcmp (&p->privileged_pp_guid, &proxypp->e.guid, sizeof (proxypp->e.guid)) != 0)
  {
    /* p not dependent on proxypp */
    os_mutexUnlock (&p->e.lock);
    return;
  }
  else if (!(vendor_is_cloud(p->vendor) && p->implicitly_created))
  {
    /* DDSI2 minimal participant mode -- but really, anything not discovered via Cloud gets deleted */
    os_mutexUnlock (&p->e.lock);
    (void) delete_proxy_participant_by_guid (&p->e.guid, timestamp, isimplicit);
  }
  else
  {
    nn_etime_t texp = add_duration_to_etime (now_et(), config.ds_grace_period);
    /* Clear dependency (but don't touch entity id, which must be 0x1c1) and set the lease ticking */
    DDS_LOG(DDS_LC_DISCOVERY, "%x:%x:%x:%x detach-from-DS %x:%x:%x:%x\n", PGUID(p->e.guid), PGUID(proxypp->e.guid));
    memset (&p->privileged_pp_guid.prefix, 0, sizeof (p->privileged_pp_guid.prefix));
    lease_set_expiry (os_atomic_ldvoidp (&p->lease), texp);
    os_mutexUnlock (&p->e.lock);
  }
}

static void delete_ppt (struct proxy_participant * proxypp, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_endpoint_common * c;
  int ret;

  /* if any proxy participants depend on this participant, delete them */
  DDS_LOG(DDS_LC_DISCOVERY, "delete_ppt(%x:%x:%x:%x) - deleting dependent proxy participants\n", PGUID (proxypp->e.guid));
  {
    struct ephash_enum_proxy_participant est;
    struct proxy_participant *p;
    ephash_enum_proxy_participant_init (&est);
    while ((p = ephash_enum_proxy_participant_next (&est)) != NULL)
      delete_or_detach_dependent_pp(p, proxypp, timestamp, isimplicit);
    ephash_enum_proxy_participant_fini (&est);
  }

  /* delete_proxy_{reader,writer} merely schedules the actual delete
     operation, so we can hold the lock -- at least, for now. */

  os_mutexLock (&proxypp->e.lock);
  if (isimplicit)
    proxypp->lease_expired = 1;

  DDS_LOG(DDS_LC_DISCOVERY, "delete_ppt(%x:%x:%x:%x) - deleting groups\n", PGUID (proxypp->e.guid));
  while (!ut_avlIsEmpty (&proxypp->groups))
    delete_proxy_group_locked (ut_avlRoot (&proxypp_groups_treedef, &proxypp->groups), timestamp, isimplicit);

  DDS_LOG(DDS_LC_DISCOVERY, "delete_ppt(%x:%x:%x:%x) - deleting endpoints\n", PGUID (proxypp->e.guid));
  c = proxypp->endpoints;
  while (c)
  {
    struct entity_common *e = entity_common_from_proxy_endpoint_common (c);
    if (is_writer_entityid (e->guid.entityid))
    {
      ret = delete_proxy_writer (&e->guid, timestamp, isimplicit);
    }
    else
    {
      ret = delete_proxy_reader (&e->guid, timestamp, isimplicit);
    }
    (void) ret;
    c = c->next_ep;
  }
  os_mutexUnlock (&proxypp->e.lock);

  gcreq_proxy_participant (proxypp);
}

typedef struct proxy_purge_data {
  struct proxy_participant *proxypp;
  const nn_locator_t *loc;
  nn_wctime_t timestamp;
} *proxy_purge_data_t;

static void purge_helper (const nn_locator_t *n, void * varg)
{
  proxy_purge_data_t data = (proxy_purge_data_t) varg;
  if (compare_locators (n, data->loc) == 0)
    delete_proxy_participant_by_guid (&data->proxypp->e.guid, data->timestamp, 1);
}

void purge_proxy_participants (const nn_locator_t *loc, bool delete_from_as_disc)
{
  /* FIXME: check whether addr:port can't be reused for a new connection by the time we get here. */
  /* NOTE: This function exists for the sole purpose of cleaning up after closing a TCP connection in ddsi_tcp_close_conn and the state of the calling thread could be anything at this point. Because of that we do the unspeakable and toggle the thread state conditionally. We can't afford to have it in "asleep", as that causes a race with the garbage collector. */
  struct thread_state1 * const self = lookup_thread_state();
  const int self_is_awake = vtime_awake_p (self->vtime);
  struct ephash_enum_proxy_participant est;
  struct proxy_purge_data data;

  if (!self_is_awake)
    thread_state_awake(self);

  data.loc = loc;
  data.timestamp = now();
  ephash_enum_proxy_participant_init (&est);
  while ((data.proxypp = ephash_enum_proxy_participant_next (&est)) != NULL)
    addrset_forall (data.proxypp->as_meta, purge_helper, &data);
  ephash_enum_proxy_participant_fini (&est);

  /* Shouldn't try to keep pinging clients once they're gone */
  if (delete_from_as_disc)
    remove_from_addrset (gv.as_disc, loc);

  if (!self_is_awake)
    thread_state_asleep(self);
}

int delete_proxy_participant_by_guid (const struct nn_guid * guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_participant * ppt;

  DDS_LOG(DDS_LC_DISCOVERY, "delete_proxy_participant_by_guid(%x:%x:%x:%x) ", PGUID (*guid));
  os_mutexLock (&gv.lock);
  ppt = ephash_lookup_proxy_participant_guid (guid);
  if (ppt == NULL)
  {
    os_mutexUnlock (&gv.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "- unknown\n");
    return ERR_UNKNOWN_ENTITY;
  }
  DDS_LOG(DDS_LC_DISCOVERY, "- deleting\n");
  ddsi_plugin.builtintopic_write (&ppt->e, timestamp, false);
  remember_deleted_participant_guid (&ppt->e.guid);
  ephash_remove_proxy_participant_guid (ppt);
  os_mutexUnlock (&gv.lock);
  delete_ppt (ppt, timestamp, isimplicit);

  return 0;
}

uint64_t participant_instance_id (const struct nn_guid *guid)
{
    struct entity_common *e;
    e = (struct entity_common*)ephash_lookup_participant_guid(guid);
    if (e) {
        return e->iid;
    }
    e = (struct entity_common*)ephash_lookup_proxy_participant_guid(guid);
    if (e) {
        return e->iid;
    }
    return 0;
}

/* PROXY-GROUP --------------------------------------------------- */

int new_proxy_group (const struct nn_guid *guid, const char *name, const struct nn_xqos *xqos, nn_wctime_t timestamp)
{
  struct proxy_participant *proxypp;
  nn_guid_t ppguid;
  (void)timestamp;
  ppguid.prefix = guid->prefix;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if ((proxypp = ephash_lookup_proxy_participant_guid (&ppguid)) == NULL)
  {
    DDS_LOG(DDS_LC_DISCOVERY, "new_proxy_group(%x:%x:%x:%x) - unknown participant\n", PGUID (*guid));
    return 0;
  }
  else
  {
    struct proxy_group *pgroup;
    ut_avlIPath_t ipath;
    int is_sub;
    switch (guid->entityid.u & (NN_ENTITYID_SOURCE_MASK | NN_ENTITYID_KIND_MASK))
    {
      case NN_ENTITYID_SOURCE_VENDOR | NN_ENTITYID_KIND_PRISMTECH_PUBLISHER:
        is_sub = 0;
        break;
      case NN_ENTITYID_SOURCE_VENDOR | NN_ENTITYID_KIND_PRISMTECH_SUBSCRIBER:
        is_sub = 1;
        break;
      default:
        DDS_WARNING("new_proxy_group: unrecognised entityid: %x\n", guid->entityid.u);
        return ERR_INVALID_DATA;
    }
    os_mutexLock (&proxypp->e.lock);
    if ((pgroup = ut_avlLookupIPath (&proxypp_groups_treedef, &proxypp->groups, guid, &ipath)) != NULL)
    {
      /* Complete proxy group definition if it was a partial
         definition made by creating a proxy reader or writer,
         otherwise ignore this call */
      if (pgroup->name != NULL)
        goto out;
    }
    else
    {
      /* Always have a guid, may not have a gid */
      DDS_LOG(DDS_LC_DISCOVERY, "new_proxy_group(%x:%x:%x:%x): new\n", PGUID (*guid));
      pgroup = os_malloc (sizeof (*pgroup));
      pgroup->guid = *guid;
      pgroup->proxypp = proxypp;
      pgroup->name = NULL;
      pgroup->xqos = NULL;
      ut_avlInsertIPath (&proxypp_groups_treedef, &proxypp->groups, pgroup, &ipath);
    }
    if (name)
    {
      assert (xqos != NULL);
      DDS_LOG(DDS_LC_DISCOVERY, "new_proxy_group(%x:%x:%x:%x): setting name (%s) and qos\n", PGUID (*guid), name);
      pgroup->name = os_strdup (name);
      pgroup->xqos = nn_xqos_dup (xqos);
      nn_xqos_mergein_missing (pgroup->xqos, is_sub ? &gv.default_xqos_sub : &gv.default_xqos_pub);
    }
  out:
    os_mutexUnlock (&proxypp->e.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "\n");
    return 0;
  }
}

static void delete_proxy_group_locked (struct proxy_group *pgroup, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_participant *proxypp = pgroup->proxypp;
  (void)timestamp;
  (void)isimplicit;
  assert ((pgroup->xqos != NULL) == (pgroup->name != NULL));
  DDS_LOG(DDS_LC_DISCOVERY, "delete_proxy_group_locked %x:%x:%x:%x\n", PGUID (pgroup->guid));
  ut_avlDelete (&proxypp_groups_treedef, &proxypp->groups, pgroup);
  /* Publish corresponding built-in topic only if it is not a place
     holder: in that case we haven't announced its presence and
     therefore don't need to dispose it, and this saves us from having
     to handle null pointers for name and QoS in the built-in topic
     generation */
  if (pgroup->name)
  {
    nn_xqos_fini (pgroup->xqos);
    os_free (pgroup->xqos);
    os_free (pgroup->name);
  }
  os_free (pgroup);
}

void delete_proxy_group (const nn_guid_t *guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_participant *proxypp;
  nn_guid_t ppguid;
  ppguid.prefix = guid->prefix;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  if ((proxypp = ephash_lookup_proxy_participant_guid (&ppguid)) != NULL)
  {
    struct proxy_group *pgroup;
    os_mutexLock (&proxypp->e.lock);
    if ((pgroup = ut_avlLookup (&proxypp_groups_treedef, &proxypp->groups, guid)) != NULL)
      delete_proxy_group_locked (pgroup, timestamp, isimplicit);
    os_mutexUnlock (&proxypp->e.lock);
  }
}

/* PROXY-ENDPOINT --------------------------------------------------- */

static void proxy_endpoint_common_init (struct entity_common *e, struct proxy_endpoint_common *c, enum entity_kind kind, const struct nn_guid *guid, nn_wctime_t tcreate, struct proxy_participant *proxypp, struct addrset *as, const nn_plist_t *plist)
{
  const char *name;

  if (is_builtin_entityid (guid->entityid, proxypp->vendor))
    assert ((plist->qos.present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == 0);
  else
    assert ((plist->qos.present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == (QP_TOPIC_NAME | QP_TYPE_NAME));

  name = (plist->present & PP_ENTITY_NAME) ? plist->entity_name : "";
  entity_common_init (e, guid, name, kind, tcreate, proxypp->vendor, false);
  c->xqos = nn_xqos_dup (&plist->qos);
  c->as = ref_addrset (as);
  c->topic = NULL; /* set from first matching reader/writer */
  c->vendor = proxypp->vendor;

  if (plist->present & PP_GROUP_GUID)
    c->group_guid = plist->group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));


  ref_proxy_participant (proxypp, c);
}

static void proxy_endpoint_common_fini (struct entity_common *e, struct proxy_endpoint_common *c)
{
  unref_proxy_participant (c->proxypp, c);

  ddsi_sertopic_unref (c->topic);
  nn_xqos_fini (c->xqos);
  os_free (c->xqos);
  unref_addrset (c->as);

  entity_common_fini (e);
}

/* PROXY-WRITER ----------------------------------------------------- */

int new_proxy_writer (const struct nn_guid *ppguid, const struct nn_guid *guid, struct addrset *as, const nn_plist_t *plist, struct nn_dqueue *dqueue, struct xeventq *evq, nn_wctime_t timestamp)
{
  struct proxy_participant *proxypp;
  struct proxy_writer *pwr;
  int isreliable;
  nn_mtime_t tnow = now_mt ();

  assert (is_writer_entityid (guid->entityid));
  assert (ephash_lookup_proxy_writer_guid (guid) == NULL);

  if ((proxypp = ephash_lookup_proxy_participant_guid (ppguid)) == NULL)
  {
    DDS_WARNING("new_proxy_writer(%x:%x:%x:%x): proxy participant unknown\n", PGUID (*guid));
    return ERR_UNKNOWN_ENTITY;
  }

  pwr = os_malloc (sizeof (*pwr));
  proxy_endpoint_common_init (&pwr->e, &pwr->c, EK_PROXY_WRITER, guid, timestamp, proxypp, as, plist);

  ut_avlInit (&pwr_readers_treedef, &pwr->readers);
  pwr->n_reliable_readers = 0;
  pwr->n_readers_out_of_sync = 0;
  pwr->last_seq = 0;
  pwr->last_fragnum = ~0u;
  pwr->nackfragcount = 0;
  pwr->last_fragnum_reset = 0;
  os_atomic_st32 (&pwr->next_deliv_seq_lowword, 1);
  if (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor)) {
    /* The DDSI built-in proxy writers always deliver
       asynchronously */
    pwr->deliver_synchronously = 0;
  } else if (nn_from_ddsi_duration (pwr->c.xqos->latency_budget.duration) <= config.synchronous_delivery_latency_bound &&
             pwr->c.xqos->transport_priority.value >= config.synchronous_delivery_priority_threshold) {
    /* Regular proxy-writers with a sufficiently low latency_budget
       and a sufficiently high transport_priority deliver
       synchronously */
    pwr->deliver_synchronously = 1;
  } else {
    pwr->deliver_synchronously = 0;
  }
  pwr->have_seen_heartbeat = 0;
  pwr->local_matching_inprogress = 1;
#ifdef DDSI_INCLUDE_SSM
  pwr->supports_ssm = (addrset_contains_ssm (as) && config.allowMulticast & AMC_SSM) ? 1 : 0;
#endif
  isreliable = (pwr->c.xqos->reliability.kind != NN_BEST_EFFORT_RELIABILITY_QOS);

  /* Only assert PP lease on receipt of data if enabled (duh) and the proxy participant is a
     "real" participant, rather than the thing we use for endpoints discovered via the DS */
  pwr->assert_pp_lease =
    (unsigned) !!config.arrival_of_data_asserts_pp_and_ep_liveliness;

  assert (pwr->c.xqos->present & QP_LIVELINESS);
  if (pwr->c.xqos->liveliness.kind != NN_AUTOMATIC_LIVELINESS_QOS)
    DDS_LOG(DDS_LC_DISCOVERY, " FIXME: only AUTOMATIC liveliness supported");
#if 0
  pwr->tlease_dur = nn_from_ddsi_duration (pwr->c.xqos->liveliness.lease_duration);
  if (pwr->tlease_dur == 0)
  {
    DDS_LOG(DDS_LC_DISCOVERY, " FIXME: treating lease_duration=0 as inf");
    pwr->tlease_dur = T_NEVER;
  }
  pwr->tlease_end = add_duration_to_wctime (tnow, pwr->tlease_dur);
#endif

  if (isreliable)
  {
    pwr->defrag = nn_defrag_new (NN_DEFRAG_DROP_LATEST, config.defrag_reliable_maxsamples);
    pwr->reorder = nn_reorder_new (NN_REORDER_MODE_NORMAL, config.primary_reorder_maxsamples);
  }
  else
  {
    pwr->defrag = nn_defrag_new (NN_DEFRAG_DROP_OLDEST, config.defrag_unreliable_maxsamples);
    pwr->reorder = nn_reorder_new (NN_REORDER_MODE_MONOTONICALLY_INCREASING, config.primary_reorder_maxsamples);
  }
  pwr->dqueue = dqueue;
  pwr->evq = evq;
  pwr->ddsi2direct_cb = 0;
  pwr->ddsi2direct_cbarg = 0;

  local_reader_ary_init (&pwr->rdary);
  ephash_insert_proxy_writer_guid (pwr);
  match_proxy_writer_with_readers (pwr, tnow);
  ddsi_plugin.builtintopic_write (&pwr->e, timestamp, true);

  os_mutexLock (&pwr->e.lock);
  pwr->local_matching_inprogress = 0;
  os_mutexUnlock (&pwr->e.lock);

  return 0;
}

void update_proxy_writer (struct proxy_writer * pwr, struct addrset * as)
{
  struct reader * rd;
  struct  pwr_rd_match * m;
  ut_avlIter_t iter;

  /* Update proxy writer endpoints (from SEDP alive) */

  os_mutexLock (&pwr->e.lock);
  if (! addrset_eq_onesidederr (pwr->c.as, as))
  {
#ifdef DDSI_INCLUDE_SSM
    pwr->supports_ssm = (addrset_contains_ssm (as) && config.allowMulticast & AMC_SSM) ? 1 : 0;
#endif
    unref_addrset (pwr->c.as);
    ref_addrset (as);
    pwr->c.as = as;
    m = ut_avlIterFirst (&pwr_readers_treedef, &pwr->readers, &iter);
    while (m)
    {
      rd = ephash_lookup_reader_guid (&m->rd_guid);
      if (rd)
      {
        qxev_pwr_entityid (pwr, &rd->e.guid.prefix);
      }
      m = ut_avlIterNext (&iter);
    }
  }
  os_mutexUnlock (&pwr->e.lock);
}

void update_proxy_reader (struct proxy_reader * prd, struct addrset * as)
{
  struct prd_wr_match * m;
  nn_guid_t wrguid;

  memset (&wrguid, 0, sizeof (wrguid));

  os_mutexLock (&prd->e.lock);
  if (! addrset_eq_onesidederr (prd->c.as, as))
  {
    /* Update proxy reader endpoints (from SEDP alive) */

    unref_addrset (prd->c.as);
    ref_addrset (as);
    prd->c.as = as;

    /* Rebuild writer endpoints */

    while ((m = ut_avlLookupSuccEq (&prd_writers_treedef, &prd->writers, &wrguid)) != NULL)
    {
      struct prd_wr_match *next;
      nn_guid_t guid_next;
      struct writer * wr;

      wrguid = m->wr_guid;
      next = ut_avlFindSucc (&prd_writers_treedef, &prd->writers, m);
      if (next)
      {
        guid_next = next->wr_guid;
      }
      else
      {
        memset (&guid_next, 0xff, sizeof (guid_next));
        guid_next.entityid.u = (guid_next.entityid.u & ~(unsigned)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
      }

      os_mutexUnlock (&prd->e.lock);
      wr = ephash_lookup_writer_guid (&wrguid);
      if (wr)
      {
        os_mutexLock (&wr->e.lock);
        rebuild_writer_addrset (wr);
        os_mutexUnlock (&wr->e.lock);
        qxev_prd_entityid (prd, &wr->e.guid.prefix);
      }
      wrguid = guid_next;
      os_mutexLock (&prd->e.lock);
    }
  }
  os_mutexUnlock (&prd->e.lock);
}

static void gc_delete_proxy_writer (struct gcreq *gcreq)
{
  struct proxy_writer *pwr = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_proxy_writer(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (pwr->e.guid));
  gcreq_free (gcreq);

  while (!ut_avlIsEmpty (&pwr->readers))
  {
    struct pwr_rd_match *m = ut_avlRootNonEmpty (&pwr_readers_treedef, &pwr->readers);
    ut_avlDelete (&pwr_readers_treedef, &pwr->readers, m);
    reader_drop_connection (&m->rd_guid, pwr);
    update_reader_init_acknack_count (&m->rd_guid, m->count);
    free_pwr_rd_match (m);
  }
  local_reader_ary_fini (&pwr->rdary);
  proxy_endpoint_common_fini (&pwr->e, &pwr->c);
  nn_defrag_free (pwr->defrag);
  nn_reorder_free (pwr->reorder);
  os_free (pwr);
}

int delete_proxy_writer (const struct nn_guid *guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_writer *pwr;
  (void)isimplicit;
  DDS_LOG(DDS_LC_DISCOVERY, "delete_proxy_writer (%x:%x:%x:%x) ", PGUID (*guid));
  os_mutexLock (&gv.lock);
  if ((pwr = ephash_lookup_proxy_writer_guid (guid)) == NULL)
  {
    os_mutexUnlock (&gv.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "- unknown\n");
    return ERR_UNKNOWN_ENTITY;
  }
  /* Set "deleting" flag in particular for Lite, to signal to the receive path it can't
     trust rdary[] anymore, which is because removing the proxy writer from the hash
     table will prevent the readers from looking up the proxy writer, and consequently
     from removing themselves from the proxy writer's rdary[]. */
  local_reader_ary_setinvalid (&pwr->rdary);
  DDS_LOG(DDS_LC_DISCOVERY, "- deleting\n");
  ddsi_plugin.builtintopic_write (&pwr->e, timestamp, false);
  ephash_remove_proxy_writer_guid (pwr);
  os_mutexUnlock (&gv.lock);
  gcreq_proxy_writer (pwr);
  return 0;
}

/* PROXY-READER ----------------------------------------------------- */

int new_proxy_reader (const struct nn_guid *ppguid, const struct nn_guid *guid, struct addrset *as, const nn_plist_t *plist, nn_wctime_t timestamp
#ifdef DDSI_INCLUDE_SSM
                      , int favours_ssm
#endif
                      )
{
  struct proxy_participant *proxypp;
  struct proxy_reader *prd;
  nn_mtime_t tnow = now_mt ();

  assert (!is_writer_entityid (guid->entityid));
  assert (ephash_lookup_proxy_reader_guid (guid) == NULL);

  if ((proxypp = ephash_lookup_proxy_participant_guid (ppguid)) == NULL)
  {
    DDS_WARNING("new_proxy_reader(%x:%x:%x:%x): proxy participant unknown\n", PGUID (*guid));
    return ERR_UNKNOWN_ENTITY;
  }

  prd = os_malloc (sizeof (*prd));
  proxy_endpoint_common_init (&prd->e, &prd->c, EK_PROXY_READER, guid, timestamp, proxypp, as, plist);

  prd->deleting = 0;
#ifdef DDSI_INCLUDE_SSM
  prd->favours_ssm = (favours_ssm && config.allowMulticast & AMC_SSM) ? 1 : 0;
#endif
  prd->is_fict_trans_reader = 0;
  /* Only assert PP lease on receipt of data if enabled (duh) and the proxy participant is a
     "real" participant, rather than the thing we use for endpoints discovered via the DS */
  prd->assert_pp_lease = (unsigned) !!config.arrival_of_data_asserts_pp_and_ep_liveliness;

  ut_avlInit (&prd_writers_treedef, &prd->writers);
  ephash_insert_proxy_reader_guid (prd);
  match_proxy_reader_with_writers (prd, tnow);
  ddsi_plugin.builtintopic_write (&prd->e, timestamp, true);
  return 0;
}

static void proxy_reader_set_delete_and_ack_all_messages (struct proxy_reader *prd)
{
  nn_guid_t wrguid;
  struct writer *wr;
  struct prd_wr_match *m;

  memset (&wrguid, 0, sizeof (wrguid));
  os_mutexLock (&prd->e.lock);
  prd->deleting = 1;
  while ((m = ut_avlLookupSuccEq (&prd_writers_treedef, &prd->writers, &wrguid)) != NULL)
  {
    /* have to be careful walking the tree -- pretty is different, but
       I want to check this before I write a lookup_succ function. */
    struct prd_wr_match *m_a_next;
    nn_guid_t wrguid_next;
    wrguid = m->wr_guid;
    if ((m_a_next = ut_avlFindSucc (&prd_writers_treedef, &prd->writers, m)) != NULL)
      wrguid_next = m_a_next->wr_guid;
    else
    {
      memset (&wrguid_next, 0xff, sizeof (wrguid_next));
      wrguid_next.entityid.u = (wrguid_next.entityid.u & ~(unsigned)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
    }

    os_mutexUnlock (&prd->e.lock);
    if ((wr = ephash_lookup_writer_guid (&wrguid)) != NULL)
    {
      struct whc_node *deferred_free_list = NULL;
      struct wr_prd_match *m_wr;
      os_mutexLock (&wr->e.lock);
      if ((m_wr = ut_avlLookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
      {
        struct whc_state whcst;
        m_wr->seq = MAX_SEQ_NUMBER;
        ut_avlAugmentUpdate (&wr_readers_treedef, m_wr);
        (void)remove_acked_messages (wr, &whcst, &deferred_free_list);
        writer_clear_retransmitting (wr);
      }
      os_mutexUnlock (&wr->e.lock);
      whc_free_deferred_free_list (wr->whc, deferred_free_list);
    }

    wrguid = wrguid_next;
    os_mutexLock (&prd->e.lock);
  }
  os_mutexUnlock (&prd->e.lock);
}

static void gc_delete_proxy_reader (struct gcreq *gcreq)
{
  struct proxy_reader *prd = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_proxy_reader(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (prd->e.guid));
  gcreq_free (gcreq);

  while (!ut_avlIsEmpty (&prd->writers))
  {
    struct prd_wr_match *m = ut_avlRootNonEmpty (&prd_writers_treedef, &prd->writers);
    ut_avlDelete (&prd_writers_treedef, &prd->writers, m);
    writer_drop_connection (&m->wr_guid, prd);
    free_prd_wr_match (m);
  }

  proxy_endpoint_common_fini (&prd->e, &prd->c);
  os_free (prd);
}

int delete_proxy_reader (const struct nn_guid *guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_reader *prd;
  (void)isimplicit;
  DDS_LOG(DDS_LC_DISCOVERY, "delete_proxy_reader (%x:%x:%x:%x) ", PGUID (*guid));
  os_mutexLock (&gv.lock);
  if ((prd = ephash_lookup_proxy_reader_guid (guid)) == NULL)
  {
    os_mutexUnlock (&gv.lock);
    DDS_LOG(DDS_LC_DISCOVERY, "- unknown\n");
    return ERR_UNKNOWN_ENTITY;
  }
  ddsi_plugin.builtintopic_write (&prd->e, timestamp, false);
  ephash_remove_proxy_reader_guid (prd);
  os_mutexUnlock (&gv.lock);
  DDS_LOG(DDS_LC_DISCOVERY, "- deleting\n");

  /* If the proxy reader is reliable, pretend it has just acked all
     messages: this allows a throttled writer to once again make
     progress, which in turn is necessary for the garbage collector to
     do its work. */
  proxy_reader_set_delete_and_ack_all_messages (prd);

  gcreq_proxy_reader (prd);
  return 0;
}

/* CONVENIENCE FUNCTIONS FOR SCHEDULING GC WORK --------------------- */

static int gcreq_participant (struct participant *pp)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_delete_participant);
  gcreq->arg = pp;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_writer (struct writer *wr)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, wr->throttling ? gc_delete_writer_throttlewait : gc_delete_writer);
  gcreq->arg = wr;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_reader (struct reader *rd)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_delete_reader);
  gcreq->arg = rd;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_proxy_participant (struct proxy_participant *proxypp)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_delete_proxy_participant);
  gcreq->arg = proxypp;
  gcreq_enqueue (gcreq);
  return 0;
}

static void gc_delete_proxy_writer_dqueue_bubble_cb (struct gcreq *gcreq)
{
  /* delete proxy_writer, phase 3 */
  struct proxy_writer *pwr = gcreq->arg;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_proxy_writer_dqueue_bubble(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (pwr->e.guid));
  gcreq_requeue (gcreq, gc_delete_proxy_writer);
}

static void gc_delete_proxy_writer_dqueue (struct gcreq *gcreq)
{
  /* delete proxy_writer, phase 2 */
  struct proxy_writer *pwr = gcreq->arg;
  struct nn_dqueue *dqueue = pwr->dqueue;
  DDS_LOG(DDS_LC_DISCOVERY, "gc_delete_proxy_writer_dqueue(%p, %x:%x:%x:%x)\n", (void *) gcreq, PGUID (pwr->e.guid));
  nn_dqueue_enqueue_callback (dqueue, (void (*) (void *)) gc_delete_proxy_writer_dqueue_bubble_cb, gcreq);
}

static int gcreq_proxy_writer (struct proxy_writer *pwr)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_delete_proxy_writer_dqueue);
  gcreq->arg = pwr;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_proxy_reader (struct proxy_reader *prd)
{
  struct gcreq *gcreq = gcreq_new (gv.gcreq_queue, gc_delete_proxy_reader);
  gcreq->arg = prd;
  gcreq_enqueue (gcreq);
  return 0;
}
