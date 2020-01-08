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

#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_qosmatch.h"
#include "dds/ddsi/q_ephash.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_xevent.h" /* qxev_spdp, &c. */
#include "dds/ddsi/q_ddsi_discovery.h" /* spdp_write, &c. */
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_protocol.h" /* NN_ENTITYID_... */
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_mcgroup.h"
#include "dds/ddsi/q_receive.h"
#include "dds/ddsi/ddsi_udp.h" /* nn_mc4gen_address_t */
#include "dds/ddsi/ddsi_rhc.h"

#include "dds/ddsi/sysdeps.h"
#include "dds__whc.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_security_omg.h"

#ifdef DDSI_INCLUDE_SECURITY
#include "dds/ddsi/ddsi_security_msg.h"
#endif

struct deleted_participant {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t guid;
  unsigned for_what;
  nn_mtime_t t_prune;
};

struct deleted_participants_admin {
  ddsrt_mutex_t deleted_participants_lock;
  ddsrt_avl_tree_t deleted_participants;
  const ddsrt_log_cfg_t *logcfg;
  int64_t delay;
};

struct proxy_writer_alive_state {
  bool alive;
  uint32_t vclock;
};

static int compare_guid (const void *va, const void *vb);
static void augment_wr_prd_match (void *vnode, const void *vleft, const void *vright);

const ddsrt_avl_treedef_t wr_readers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct wr_prd_match, avlnode), offsetof (struct wr_prd_match, prd_guid), compare_guid, augment_wr_prd_match);
const ddsrt_avl_treedef_t wr_local_readers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct wr_rd_match, avlnode), offsetof (struct wr_rd_match, rd_guid), compare_guid, 0);
const ddsrt_avl_treedef_t rd_writers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct rd_pwr_match, avlnode), offsetof (struct rd_pwr_match, pwr_guid), compare_guid, 0);
const ddsrt_avl_treedef_t rd_local_writers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct rd_wr_match, avlnode), offsetof (struct rd_wr_match, wr_guid), compare_guid, 0);
const ddsrt_avl_treedef_t pwr_readers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct pwr_rd_match, avlnode), offsetof (struct pwr_rd_match, rd_guid), compare_guid, 0);
const ddsrt_avl_treedef_t prd_writers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct prd_wr_match, avlnode), offsetof (struct prd_wr_match, wr_guid), compare_guid, 0);
const ddsrt_avl_treedef_t deleted_participants_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct deleted_participant, avlnode), offsetof (struct deleted_participant, guid), compare_guid, 0);
const ddsrt_avl_treedef_t proxypp_groups_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct proxy_group, avlnode), offsetof (struct proxy_group, guid), compare_guid, 0);

static const unsigned builtin_writers_besmask =
  NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER |
  NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER |
  NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER |
  NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
static const unsigned prismtech_builtin_writers_besmask =
  NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER |
  NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER |
  NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;

static dds_return_t new_writer_guid (struct writer **wr_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc *whc, status_cb_t status_cb, void *status_cbarg);
static dds_return_t new_reader_guid (struct reader **rd_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct ddsi_rhc *rhc, status_cb_t status_cb, void *status_cbarg);
static struct participant *ref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity);
static void unref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity);

static int gcreq_participant (struct participant *pp);
static int gcreq_writer (struct writer *wr);
static int gcreq_reader (struct reader *rd);
static int gcreq_proxy_participant (struct proxy_participant *proxypp);
static int gcreq_proxy_writer (struct proxy_writer *pwr);
static int gcreq_proxy_reader (struct proxy_reader *prd);

extern inline bool builtintopic_is_visible (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid, nn_vendorid_t vendorid);
extern inline bool builtintopic_is_builtintopic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_sertopic *topic);
extern inline struct ddsi_tkmap_instance *builtintopic_get_tkmap_entry (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid);
extern inline void builtintopic_write (const struct ddsi_builtin_topic_interface *btif, const struct entity_common *e, nn_wctime_t timestamp, bool alive);

extern inline seqno_t writer_read_seq_xmit (const struct writer *wr);
extern inline void writer_update_seq_xmit (struct writer *wr, seqno_t nv);

static int compare_guid (const void *va, const void *vb)
{
  return memcmp (va, vb, sizeof (ddsi_guid_t));
}

bool is_null_guid (const ddsi_guid_t *guid)
{
  return guid->prefix.u[0] == 0 && guid->prefix.u[1] == 0 && guid->prefix.u[2] == 0 && guid->entityid.u == 0;
}

ddsi_entityid_t to_entityid (unsigned u)
{
  ddsi_entityid_t e;
  e.u = u;
  return e;
}

int is_writer_entityid (ddsi_entityid_t id)
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

int is_reader_entityid (ddsi_entityid_t id)
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

int is_keyed_endpoint_entityid (ddsi_entityid_t id)
{
  switch (id.u & NN_ENTITYID_KIND_MASK)
  {
    case NN_ENTITYID_KIND_READER_WITH_KEY:
    case NN_ENTITYID_KIND_WRITER_WITH_KEY:
      return 1;
    case NN_ENTITYID_KIND_READER_NO_KEY:
    case NN_ENTITYID_KIND_WRITER_NO_KEY:
      return 0;
    default:
      return 0;
  }
}

int is_builtin_entityid (ddsi_entityid_t id, nn_vendorid_t vendorid)
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

int is_builtin_endpoint (ddsi_entityid_t id, nn_vendorid_t vendorid)
{
  return is_builtin_entityid (id, vendorid) && id.u != NN_ENTITYID_PARTICIPANT;
}

bool is_local_orphan_endpoint (const struct entity_common *e)
{
  return (e->guid.prefix.u[0] == 0 && e->guid.prefix.u[1] == 0 && e->guid.prefix.u[2] == 0 &&
          is_builtin_endpoint (e->guid.entityid, NN_VENDORID_ECLIPSE));
}

static void entity_common_init (struct entity_common *e, struct q_globals *gv, const struct ddsi_guid *guid, const char *name, enum entity_kind kind, nn_wctime_t tcreate, nn_vendorid_t vendorid, bool onlylocal)
{
  e->guid = *guid;
  e->kind = kind;
  e->tupdate = tcreate;
  e->name = ddsrt_strdup (name ? name : "");
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

static void entity_common_fini (struct entity_common *e)
{
  if (e->tk)
    ddsi_tkmap_instance_unref (e->gv->m_tkmap, e->tk);
  ddsrt_free (e->name);
  ddsrt_mutex_destroy (&e->qos_lock);
  ddsrt_mutex_destroy (&e->lock);
}

static void local_reader_ary_init (struct local_reader_ary *x)
{
  ddsrt_mutex_init (&x->rdary_lock);
  x->valid = 1;
  x->fastpath_ok = 1;
  x->n_readers = 0;
  x->rdary = ddsrt_malloc (sizeof (*x->rdary));
  x->rdary[0] = NULL;
}

static void local_reader_ary_fini (struct local_reader_ary *x)
{
  ddsrt_free (x->rdary);
  ddsrt_mutex_destroy (&x->rdary_lock);
}

static void local_reader_ary_insert (struct local_reader_ary *x, struct reader *rd)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  x->n_readers++;
  x->rdary = ddsrt_realloc (x->rdary, (x->n_readers + 1) * sizeof (*x->rdary));
  x->rdary[x->n_readers - 1] = rd;
  x->rdary[x->n_readers] = NULL;
  ddsrt_mutex_unlock (&x->rdary_lock);
}

static void local_reader_ary_remove (struct local_reader_ary *x, struct reader *rd)
{
  uint32_t i;
  ddsrt_mutex_lock (&x->rdary_lock);
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
  x->rdary = ddsrt_realloc (x->rdary, (x->n_readers + 1) * sizeof (*x->rdary));
  ddsrt_mutex_unlock (&x->rdary_lock);
}

void local_reader_ary_setfastpath_ok (struct local_reader_ary *x, bool fastpath_ok)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  if (x->valid)
    x->fastpath_ok = fastpath_ok;
  ddsrt_mutex_unlock (&x->rdary_lock);
}

static void local_reader_ary_setinvalid (struct local_reader_ary *x)
{
  ddsrt_mutex_lock (&x->rdary_lock);
  x->valid = 0;
  x->fastpath_ok = 0;
  ddsrt_mutex_unlock (&x->rdary_lock);
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

void ddsi_make_writer_info(struct ddsi_writer_info *wrinfo, const struct entity_common *e, const struct dds_qos *xqos)
{
  wrinfo->guid = e->guid;
  wrinfo->ownership_strength = xqos->ownership_strength.value;
  wrinfo->auto_dispose = xqos->writer_data_lifecycle.autodispose_unregistered_instances;
  wrinfo->iid = e->iid;
}

/* DELETED PARTICIPANTS --------------------------------------------- */

struct deleted_participants_admin *deleted_participants_admin_new (const ddsrt_log_cfg_t *logcfg, int64_t delay)
{
  struct deleted_participants_admin *admin = ddsrt_malloc (sizeof (*admin));
  ddsrt_mutex_init (&admin->deleted_participants_lock);
  ddsrt_avl_init (&deleted_participants_treedef, &admin->deleted_participants);
  admin->logcfg = logcfg;
  admin->delay = delay;
  return admin;
}

void deleted_participants_admin_free (struct deleted_participants_admin *admin)
{
  ddsrt_avl_free (&deleted_participants_treedef, &admin->deleted_participants, ddsrt_free);
  ddsrt_mutex_destroy (&admin->deleted_participants_lock);
  ddsrt_free (admin);
}

static void prune_deleted_participant_guids_unlocked (struct deleted_participants_admin *admin, nn_mtime_t tnow)
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
      DDS_CLOG (DDS_LC_DISCOVERY, admin->logcfg, "prune_deleted_participant_guid("PGUIDFMT")\n", PGUID (dpp->guid));
      ddsrt_avl_delete (&deleted_participants_treedef, &admin->deleted_participants, dpp);
      ddsrt_free (dpp);
    }
    dpp = dpp1;
  }
}

static void prune_deleted_participant_guids (struct deleted_participants_admin *admin, nn_mtime_t tnow)
{
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  prune_deleted_participant_guids_unlocked (admin, tnow);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

static void remember_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid)
{
  struct deleted_participant *n;
  ddsrt_avl_ipath_t path;
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  if (ddsrt_avl_lookup_ipath (&deleted_participants_treedef, &admin->deleted_participants, guid, &path) == NULL)
  {
    if ((n = ddsrt_malloc (sizeof (*n))) != NULL)
    {
      n->guid = *guid;
      n->t_prune.v = T_NEVER;
      n->for_what = DPG_LOCAL | DPG_REMOTE;
      ddsrt_avl_insert_ipath (&deleted_participants_treedef, &admin->deleted_participants, n, &path);
    }
  }
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

int is_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid, unsigned for_what)
{
  struct deleted_participant *n;
  int known;
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  prune_deleted_participant_guids_unlocked (admin, now_mt ());
  if ((n = ddsrt_avl_lookup (&deleted_participants_treedef, &admin->deleted_participants, guid)) == NULL)
    known = 0;
  else
    known = ((n->for_what & for_what) != 0);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
  return known;
}

static void remove_deleted_participant_guid (struct deleted_participants_admin *admin, const struct ddsi_guid *guid, unsigned for_what)
{
  struct deleted_participant *n;
  DDS_CLOG (DDS_LC_DISCOVERY, admin->logcfg, "remove_deleted_participant_guid("PGUIDFMT" for_what=%x)\n", PGUID (*guid), for_what);
  ddsrt_mutex_lock (&admin->deleted_participants_lock);
  if ((n = ddsrt_avl_lookup (&deleted_participants_treedef, &admin->deleted_participants, guid)) != NULL)
    n->t_prune = add_duration_to_mtime (now_mt (), admin->delay);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

/* PARTICIPANT ------------------------------------------------------ */

static int compare_ldur (const void *va, const void *vb)
{
  const struct ldur_fhnode *a = va;
  const struct ldur_fhnode *b = vb;
  return (a->ldur == b->ldur) ? 0 : (a->ldur < b->ldur) ? -1 : 1;
}

/* used in participant for keeping writer liveliness renewal */
const ddsrt_fibheap_def_t ldur_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct ldur_fhnode, heapnode), compare_ldur);

static bool update_qos_locked (struct entity_common *e, dds_qos_t *ent_qos, const dds_qos_t *xqos, nn_wctime_t timestamp)
{
  uint64_t mask;

  mask = nn_xqos_delta (ent_qos, xqos, QP_CHANGEABLE_MASK & ~(QP_RXO_MASK | QP_PARTITION)) & xqos->present;
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
  EELOGDISC (e, "update_qos_locked "PGUIDFMT" delta=%"PRIu64" QOS={", PGUID(e->guid), mask);
  nn_log_xqos (DDS_LC_DISCOVERY, &e->gv->logconfig, xqos);
  EELOGDISC (e, "}\n");

  if (mask == 0)
    /* no change, or an as-yet unsupported one */
    return false;

  ddsrt_mutex_lock (&e->qos_lock);
  nn_xqos_fini_mask (ent_qos, mask);
  nn_xqos_mergein_missing (ent_qos, xqos, mask);
  ddsrt_mutex_unlock (&e->qos_lock);
  builtintopic_write (e->gv->builtin_topic_interface, e, timestamp, true);
  return true;
}

static dds_return_t pp_allocate_entityid(ddsi_entityid_t *id, uint32_t kind, struct participant *pp)
{
  uint32_t id1;
  int ret = 0;
  ddsrt_mutex_lock (&pp->e.lock);
  if (inverse_uint32_set_alloc(&id1, &pp->avail_entityids.x))
  {
    *id = to_entityid (id1 * NN_ENTITYID_ALLOCSTEP + kind);
    ret = 0;
  }
  else
  {
    DDS_CERROR (&pp->e.gv->logconfig, "pp_allocate_entityid("PGUIDFMT"): all ids in use\n", PGUID(pp->e.guid));
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
  }
  ddsrt_mutex_unlock (&pp->e.lock);
  return ret;
}

static void pp_release_entityid(struct participant *pp, ddsi_entityid_t id)
{
  ddsrt_mutex_lock (&pp->e.lock);
  inverse_uint32_set_free(&pp->avail_entityids.x, id.u / NN_ENTITYID_ALLOCSTEP);
  ddsrt_mutex_unlock (&pp->e.lock);
}

static void force_as_disc_address(struct q_globals *gv, const ddsi_guid_t *subguid)
{
  struct writer *wr = ephash_lookup_writer_guid (gv->guid_hash, subguid);
  assert (wr != NULL);
  ddsrt_mutex_lock (&wr->e.lock);
  unref_addrset (wr->as);
  unref_addrset (wr->as_group);
  wr->as = ref_addrset (gv->as_disc);
  wr->as_group = ref_addrset (gv->as_disc_group);
  ddsrt_mutex_unlock (&wr->e.lock);
}

#ifdef DDSI_INCLUDE_SECURITY
static void add_security_builtin_endpoints(struct participant *pp, ddsi_guid_t *subguid, const ddsi_guid_t *group_guid, struct q_globals *gv, bool add_writers, bool add_readers)
{
  if (add_writers)
  {
    subguid->entityid = to_entityid (NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), NULL, NULL);
    /* But we need the as_disc address set for SPDP, because we need to
       send it to everyone regardless of the existence of readers. */
    force_as_disc_address(gv, subguid);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_stateless_xqos_wr, whc_new(gv, 0, 1, 1), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_volatile_xqos_wr, whc_new(gv, 0, 0, 0), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER;
  }

  if (add_readers)
  {
    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_DETECTOR;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_DETECTOR;
  }

  /*
   * When security is enabled configure the associated necessary builtin readers independent of the
   * besmode flag setting, because all participant do require authentication.
   */
  subguid->entityid = to_entityid (NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_DETECTOR;

  subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_volatile_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_DETECTOR;

  subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_stateless_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_DETECTOR;

  subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_DETECTOR;
}
#endif

dds_return_t new_participant_guid (const ddsi_guid_t *ppguid, struct q_globals *gv, unsigned flags, const nn_plist_t *plist)
{
  struct participant *pp;
  ddsi_guid_t subguid, group_guid;
  dds_return_t ret = DDS_RETCODE_OK;

  /* no reserved bits may be set */
  assert ((flags & ~(RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS | RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP | RTPS_PF_ONLY_LOCAL)) == 0);
  /* privileged participant MUST have builtin readers and writers */
  assert (!(flags & RTPS_PF_PRIVILEGED_PP) || (flags & (RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS)) == 0);

  prune_deleted_participant_guids (gv->deleted_participants, now_mt ());

  /* FIXME: FULL LOCKING AROUND NEW_XXX FUNCTIONS, JUST SO EXISTENCE TESTS ARE PRECISE */

  /* Participant may not exist yet, but this test is imprecise: if it
     used to exist, but is currently being deleted and we're trying to
     recreate it. */
  if (ephash_lookup_participant_guid (gv->guid_hash, ppguid) != NULL)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

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
      GVERROR ("new_participant("PGUIDFMT", %x) failed: max participants reached\n", PGUID (*ppguid), flags);
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto new_pp_err;
    }
  }

  GVLOGDISC ("new_participant("PGUIDFMT", %x)\n", PGUID (*ppguid), flags);

  pp = ddsrt_malloc (sizeof (*pp));

  entity_common_init (&pp->e, gv, ppguid, "", EK_PARTICIPANT, now (), NN_VENDORID_ECLIPSE, ((flags & RTPS_PF_ONLY_LOCAL) != 0));
  pp->user_refc = 1;
  pp->builtin_refc = 0;
  pp->builtins_deleted = 0;
  pp->is_ddsi2_pp = (flags & (RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP)) ? 1 : 0;
  ddsrt_mutex_init (&pp->refc_lock);
  inverse_uint32_set_init(&pp->avail_entityids.x, 1, UINT32_MAX / NN_ENTITYID_ALLOCSTEP);
  pp->lease_duration = gv->config.lease_duration;
  ddsrt_fibheap_init (&ldur_fhdef, &pp->ldur_auto_wr);
  pp->plist = ddsrt_malloc (sizeof (*pp->plist));
  nn_plist_copy (pp->plist, plist);
  nn_plist_mergein_missing (pp->plist, &gv->default_local_plist_pp, ~(uint64_t)0, ~(uint64_t)0);

#ifdef DDSI_INCLUDE_SECURITY
  /*
   * if there there are security properties check them .
   * if there are no security properties, then merge from security configuration if there is
   */
  /* check for existing security properties (name starts with dds.sec. conform DDS Security spec 7.2.4.1)
   * and return if any is found */
  {
    bool ready_to_load_security = false;
    if (nn_xqos_has_prop(&pp->plist->qos, "dds.sec.", true, false)) {
      char *req[] = {DDS_SEC_PROP_AUTH_IDENTITY_CA,
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
                     DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE};
      GVLOGDISC ("new_participant("
                         PGUIDFMT
                         "): using security settings from QoS\n", PGUID(*ppguid));

      /* check if all required security properties exist in qos */
      for (size_t i = 0; i < sizeof(req) / sizeof(req[0]); i++) {
        if (!nn_xqos_has_prop(&pp->plist->qos, req[i], false, true)) {
          GVERROR ("new_participant("
                           PGUIDFMT
                           "): required security property %s missing in Property QoS\n", PGUID(*ppguid), req[i]);
          ret = DDS_RETCODE_PRECONDITION_NOT_MET;
        }
      }
      if (ret == DDS_RETCODE_OK) {
        ready_to_load_security = true;
      } else {
        goto new_pp_err_secprop;
      }
    } else if (gv->config.omg_security_configuration) {
      /* For security, configuration can be provided through the configuration.
       * However, the specification (and the plugins) expect it to be in the QoS. */
      GVLOGDISC ("new_participant("
                         PGUIDFMT
                         "): using security settings from configuration\n", PGUID(*ppguid));
      nn_xqos_mergein_security_config(&pp->plist->qos, &gv->config.omg_security_configuration->cfg);
      ready_to_load_security = true;
    }

    if( q_omg_is_security_loaded( gv->security_context ) == false ){
      if (ready_to_load_security && q_omg_security_load(gv->security_context, &pp->plist->qos) < 0) {
        GVERROR("Could not load security\n");
        ret = DDS_RETCODE_NOT_ALLOWED_BY_SECURITY;
        goto new_pp_err_secprop;
      }
    } else {
      GVLOGDISC ("new_participant("
                               PGUIDFMT
                               "): security is already loaded for this domain\n", PGUID(*ppguid));
    }
  }

#endif

  if (gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    GVLOGDISC ("PARTICIPANT "PGUIDFMT" QOS={", PGUID (pp->e.guid));
    nn_log_xqos (DDS_LC_DISCOVERY, &gv->logconfig, &pp->plist->qos);
    GVLOGDISC ("}\n");
  }

  if (gv->config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    pp->m_conn = ddsi_factory_create_conn (gv->m_factory, 0, NULL);
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
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->spdp_endpoint_xqos, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    /* But we need the as_disc address set for SPDP, because we need to
       send it to everyone regardless of the existence of readers. */
    force_as_disc_address(gv, &subguid);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER;
  }

  /* Make it globally visible, else the endpoint matching won't work. */
  ephash_insert_participant_guid (gv->guid_hash, pp);

  /* SEDP writers: */
  if (!(flags & RTPS_PF_NO_BUILTIN_WRITERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_WRITER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_WRITER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_WRITER;
  }

  if (gv->config.do_topic_discovery)
  {
    /* TODO: make this one configurable, we don't want all participants to publish all topics (or even just those that they use themselves) */
    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_TOPIC_ANNOUNCER;
  }

  /* PMD writer: */
  if (!(flags & RTPS_PF_NO_BUILTIN_WRITERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER);
    new_writer_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_wr, whc_new(gv, 1, 1, 1), LAST_WR_PARAMS);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;
  }

  /* SPDP, SEDP, PMD readers: */
  if (!(flags & RTPS_PF_NO_BUILTIN_READERS))
  {
    subguid.entityid = to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->spdp_endpoint_xqos, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;

    subguid.entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PARTICIPANT_READER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_PUBLISHER_READER;

    subguid.entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER);
    new_reader_guid (NULL, &subguid, &group_guid, pp, NULL, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->prismtech_bes |= NN_DISC_BUILTIN_ENDPOINT_CM_SUBSCRIBER_READER;

  }

#ifdef DDSI_INCLUDE_SECURITY
  if (q_omg_participant_is_secure(pp))
  {
    add_security_builtin_endpoints(pp, &subguid, &group_guid, gv, !(flags & RTPS_PF_NO_BUILTIN_WRITERS), !(flags & RTPS_PF_NO_BUILTIN_READERS));
  }
#endif

#undef LAST_WR_PARAMS

  /* If the participant doesn't have the full set of builtin writers
     it depends on the privileged participant, which must exist, hence
     the reference count of the privileged participant is incremented.
     If it is the privileged participant, set the global variable
     pointing to it.
     Except when the participant is only locally available. */
  if (!(flags & RTPS_PF_ONLY_LOCAL)) {
    ddsrt_mutex_lock (&gv->privileged_pp_lock);
    if ((pp->bes & builtin_writers_besmask) != builtin_writers_besmask ||
        (pp->prismtech_bes & prismtech_builtin_writers_besmask) != prismtech_builtin_writers_besmask)
    {
      /* Simply crash when the privileged participant doesn't exist when
         it is needed.  Its existence is a precondition, and this is not
         a public API */
      assert (gv->privileged_pp != NULL);
      ref_participant (gv->privileged_pp, &pp->e.guid);
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

  /* Make it globally visible, then signal receive threads if
     necessary. Must do in this order, or the receive thread won't
     find the new participant */

  if (gv->config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    ddsrt_atomic_fence ();
    ddsrt_atomic_inc32 (&gv->participant_set_generation);
    trigger_recv_threads (gv);
  }

  builtintopic_write (gv->builtin_topic_interface, &pp->e, now(), true);

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
    pp->spdp_xevent = qxev_spdp (gv->xevents, add_duration_to_mtime (now_mt (), 100 * T_MILLISECOND), &pp->e.guid, NULL);
  }

  /* Also write the CM data - this one being transient local, we only
   need to write it once (or when it changes, I suppose) */
  sedp_write_cm_participant (pp, 1);

  {
    nn_mtime_t tsched;
    tsched.v = (pp->lease_duration == T_NEVER) ? T_NEVER : 0;
    pp->pmd_update_xevent = qxev_pmd_update (gv->xevents, tsched, &pp->e.guid);
  }
  return ret;

#ifdef DDSI_INCLUDE_SECURITY
new_pp_err_secprop:
  nn_plist_fini (pp->plist);
  ddsrt_free (pp->plist);
  inverse_uint32_set_fini (&pp->avail_entityids.x);
  ddsrt_mutex_destroy (&pp->refc_lock);
  entity_common_fini (&pp->e);
  ddsrt_free (pp);
  ddsrt_mutex_lock (&gv->participant_set_lock);
  gv->nparticipants--;
  ddsrt_mutex_unlock (&gv->participant_set_lock);
#endif
new_pp_err:
  return ret;
}

dds_return_t new_participant (ddsi_guid_t *p_ppguid, struct q_globals *gv, unsigned flags, const nn_plist_t *plist)
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

void update_participant_plist (struct participant *pp, const nn_plist_t *plist)
{
  ddsrt_mutex_lock (&pp->e.lock);
  if (update_qos_locked (&pp->e, &pp->plist->qos, &plist->qos, now ()))
    spdp_write (pp);
  ddsrt_mutex_unlock (&pp->e.lock);
}

static void delete_builtin_endpoint (struct q_globals *gv, const struct ddsi_guid *ppguid, unsigned entityid)
{
  ddsi_guid_t guid;
  guid.prefix = ppguid->prefix;
  guid.entityid.u = entityid;
  assert (is_builtin_entityid (to_entityid (entityid), NN_VENDORID_ECLIPSE));
  if (is_writer_entityid (to_entityid (entityid)))
    delete_writer_nolinger (gv, &guid);
  else
    (void)delete_reader (gv, &guid);
}

static struct participant *ref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity)
{
  ddsi_guid_t stguid;
  ddsrt_mutex_lock (&pp->refc_lock);
  if (guid_of_refing_entity && is_builtin_endpoint (guid_of_refing_entity->entityid, NN_VENDORID_ECLIPSE))
    pp->builtin_refc++;
  else
    pp->user_refc++;

  if (guid_of_refing_entity)
    stguid = *guid_of_refing_entity;
  else
    memset (&stguid, 0, sizeof (stguid));
  ELOGDISC (pp, "ref_participant("PGUIDFMT" @ %p <- "PGUIDFMT" @ %p) user %"PRId32" builtin %"PRId32"\n",
            PGUID (pp->e.guid), (void*)pp, PGUID (stguid), (void*)guid_of_refing_entity, pp->user_refc, pp->builtin_refc);
  ddsrt_mutex_unlock (&pp->refc_lock);
  return pp;
}

static void unref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity)
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
    /* PrismTech ones: */
    NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_CM_PARTICIPANT_READER,
    NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_CM_PUBLISHER_READER,
    NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_WRITER,
    NN_ENTITYID_SEDP_BUILTIN_CM_SUBSCRIBER_READER
  };
  ddsi_guid_t stguid;

  ddsrt_mutex_lock (&pp->refc_lock);
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
  ELOGDISC (pp, "unref_participant("PGUIDFMT" @ %p <- "PGUIDFMT" @ %p) user %"PRId32" builtin %"PRId32"\n",
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
    ddsrt_mutex_unlock (&pp->refc_lock);

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
      if ((pp->bes & builtin_writers_besmask) != builtin_writers_besmask ||
          (pp->prismtech_bes & prismtech_builtin_writers_besmask) != prismtech_builtin_writers_besmask)
      {
        /* Participant doesn't have a full complement of built-in
           writers, therefore, it relies on gv->privileged_pp, and
           therefore we must decrement the reference count of that one.

           Why read it with the lock held, only to release it and use it
           without any attempt to maintain a consistent state?  We DO
           have a counted reference, so it can't be freed, but there is
           no formal guarantee that the pointer we read is valid unless
           we read it with the lock held.  We can't keep the lock across
           the unref_participant, because we may trigger a clean-up of
           it.  */
        struct participant *ppp;
        ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
        ppp = pp->e.gv->privileged_pp;
        ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);
        assert (ppp != NULL);
        unref_participant (ppp, &pp->e.guid);
      }
    }

    ddsrt_mutex_lock (&pp->e.gv->participant_set_lock);
    assert (pp->e.gv->nparticipants > 0);
    if (--pp->e.gv->nparticipants == 0)
      ddsrt_cond_broadcast (&pp->e.gv->participant_set_cond);
    ddsrt_mutex_unlock (&pp->e.gv->participant_set_lock);
    if (pp->e.gv->config.many_sockets_mode == MSM_MANY_UNICAST)
    {
      ddsrt_atomic_fence_rel ();
      ddsrt_atomic_inc32 (&pp->e.gv->participant_set_generation);

      /* Deleting the socket will usually suffice to wake up the
         receiver threads, but in general, no one cares if it takes a
         while longer for it to wakeup. */
      ddsi_conn_free (pp->m_conn);
    }
    nn_plist_fini (pp->plist);
    ddsrt_free (pp->plist);
    ddsrt_mutex_destroy (&pp->refc_lock);
    entity_common_fini (&pp->e);
    remove_deleted_participant_guid (pp->e.gv->deleted_participants, &pp->e.guid, DPG_LOCAL);
    inverse_uint32_set_fini(&pp->avail_entityids.x);
    ddsrt_free (pp);
  }
  else
  {
    ddsrt_mutex_unlock (&pp->refc_lock);
  }
}

static void gc_delete_participant (struct gcreq *gcreq)
{
  struct participant *pp = gcreq->arg;
  ELOGDISC (pp, "gc_delete_participant(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pp->e.guid));
  gcreq_free (gcreq);
  unref_participant (pp, NULL);
}

dds_return_t delete_participant (struct q_globals *gv, const struct ddsi_guid *ppguid)
{
  struct participant *pp;
  GVLOGDISC ("delete_participant("PGUIDFMT")\n", PGUID (*ppguid));
  if ((pp = ephash_lookup_participant_guid (gv->guid_hash, ppguid)) == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  builtintopic_write (gv->builtin_topic_interface, &pp->e, now(), false);
  remember_deleted_participant_guid (gv->deleted_participants, &pp->e.guid);
  ephash_remove_participant_guid (gv->guid_hash, pp);
  gcreq_participant (pp);
  return 0;
}

struct writer *get_builtin_writer (const struct participant *pp, unsigned entityid)
{
  ddsi_guid_t bwr_guid;
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
    ddsrt_mutex_lock (&pp->e.gv->privileged_pp_lock);
    assert (pp->e.gv->privileged_pp != NULL);
    bwr_guid.prefix = pp->e.gv->privileged_pp->e.guid.prefix;
    ddsrt_mutex_unlock (&pp->e.gv->privileged_pp_lock);
    bwr_guid.entityid.u = entityid;
  }

  return ephash_lookup_writer_guid (pp->e.gv->guid_hash, &bwr_guid);
}

dds_duration_t pp_get_pmd_interval (struct participant *pp)
{
  struct ldur_fhnode *ldur_node;
  dds_duration_t intv;
  ddsrt_mutex_lock (&pp->e.lock);
  ldur_node = ddsrt_fibheap_min (&ldur_fhdef, &pp->ldur_auto_wr);
  intv = (ldur_node != NULL) ? ldur_node->ldur : T_NEVER;
  if (pp->lease_duration < intv)
    intv = pp->lease_duration;
  ddsrt_mutex_unlock (&pp->e.lock);
  return intv;
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
  struct ephash *gh = wr->e.gv->guid_hash;
  struct wr_prd_match *m;
  ddsrt_avl_iter_t it;
#ifdef DDSI_INCLUDE_SSM
  if (wr->supports_ssm && wr->ssm_as)
    copy_addrset_into_addrset_mc (wr->e.gv, all_addrs, wr->ssm_as);
#endif
  *nreaders = 0;
  for (m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct proxy_reader *prd;
    if ((prd = ephash_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
      continue;
    (*nreaders)++;
    copy_addrset_into_addrset(wr->e.gv, all_addrs, prd->c.as);
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

static void rebuild_make_locs(const struct ddsrt_log_cfg *logcfg, int *p_nlocs, nn_locator_t **p_locs, struct addrset *all_addrs)
{
  struct rebuild_flatten_locs_arg flarg;
  int nlocs;
  int i, j;
  nn_locator_t *locs;
  nlocs = (int)addrset_count(all_addrs);
  locs = ddsrt_malloc((size_t)nlocs * sizeof(*locs));
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
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "reduced nlocs=%d\n", nlocs);
  *p_nlocs = nlocs;
  *p_locs = locs;
}

static void rebuild_make_covered(int8_t **covered, const struct writer *wr, int *nreaders, int nlocs, const nn_locator_t *locs)
{
  struct rebuild_flatten_locs_arg flarg;
  struct ephash *gh = wr->e.gv->guid_hash;
  struct wr_prd_match *m;
  ddsrt_avl_iter_t it;
  int rdidx, i, j;
  int8_t *cov = ddsrt_malloc((size_t) *nreaders * (size_t) nlocs * sizeof (*cov));
  for (i = 0; i < *nreaders * nlocs; i++)
    cov[i] = -1;
  rdidx = 0;
  flarg.locs = ddsrt_malloc((size_t) nlocs * sizeof(*flarg.locs));
#ifndef NDEBUG
  flarg.size = nlocs;
#endif
  for (m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct proxy_reader *prd;
    struct addrset *ass[] = { NULL, NULL, NULL };
    if ((prd = ephash_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
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
  ddsrt_free(flarg.locs);
  *covered = cov;
  *nreaders = rdidx;
}

static void rebuild_make_locs_nrds(int **locs_nrds, int nreaders, int nlocs, const int8_t *covered)
{
  int i, j;
  int *ln = ddsrt_malloc((size_t) nlocs * sizeof(*ln));
  for (i = 0; i < nlocs; i++)
  {
    int n = 0;
    for (j = 0; j < nreaders; j++)
      if (covered[j * nlocs + i] >= 0)
        n++;

    /* The compiler doesn't realize that ln is large enough. */
    DDSRT_WARNING_MSVC_OFF(6386);
    ln[i] = n;
    DDSRT_WARNING_MSVC_ON(6386);
  }
  *locs_nrds = ln;
}

static void rebuild_trace_covered(const struct q_globals *gv, int nreaders, int nlocs, const nn_locator_t *locs, const int *locs_nrds, const int8_t *covered)
{
  int i, j;
  for (i = 0; i < nlocs; i++)
  {
    char buf[DDSI_LOCATORSTRLEN];
    ddsi_locator_to_string(gv, buf, sizeof(buf), &locs[i]);
    GVLOGDISC ("  loc %2d = %-30s %2d {", i, buf, locs_nrds[i]);
    for (j = 0; j < nreaders; j++)
      if (covered[j * nlocs + i] >= 0)
        GVLOGDISC (" %d", covered[j * nlocs + i]);
      else
        GVLOGDISC (" .");
    GVLOGDISC (" }\n");
  }
}

static int rebuild_select(const struct q_globals *gv, int nlocs, const nn_locator_t *locs, const int *locs_nrds, bool prefer_multicast)
{
  int i, j;
  if (nlocs == 0)
    return -1;
  for (j = 0, i = 1; i < nlocs; i++) {
    if (prefer_multicast && locs_nrds[i] > 0 && ddsi_is_mcaddr(gv, &locs[i]) && !ddsi_is_mcaddr(gv, &locs[j]))
      j = i; /* obviously first step must be to try and avoid unicast if configuration says so */
    else if (locs_nrds[i] > locs_nrds[j])
      j = i; /* better coverage */
    else if (locs_nrds[i] == locs_nrds[j])
    {
      if (locs_nrds[i] == 1 && !ddsi_is_mcaddr(gv, &locs[i]))
        j = i; /* prefer unicast for single nodes */
#if DDSI_INCLUDE_SSM
      else if (ddsi_is_ssm_mcaddr(gv, &locs[i]))
        j = i; /* "reader favours SSM": all else being equal, use SSM */
#endif
    }
  }
  return (locs_nrds[j] > 0) ? j : -1;
}

static void rebuild_add(const struct q_globals *gv, struct addrset *newas, int locidx, int nreaders, int nlocs, const nn_locator_t *locs, const int8_t *covered)
{
  char str[DDSI_LOCATORSTRLEN];
  if (locs[locidx].kind != NN_LOCATOR_KIND_UDPv4MCGEN)
  {
    ddsi_locator_to_string(gv, str, sizeof(str), &locs[locidx]);
    GVLOGDISC ("  simple %s\n", str);
    add_to_addrset(gv, newas, &locs[locidx]);
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
    ddsi_locator_to_string(gv, str, sizeof(str), &l);
    GVLOGDISC ("  mcgen %s\n", str);
    add_to_addrset(gv, newas, &l);
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
  bool prefer_multicast = wr->e.gv->config.prefer_multicast;
  struct addrset *all_addrs;
  int nreaders, nlocs;
  nn_locator_t *locs;
  int *locs_nrds;
  int8_t *covered;
  int best;
  if ((all_addrs = rebuild_make_all_addrs(&nreaders, wr)) == NULL)
    return;
  nn_log_addrset(wr->e.gv, DDS_LC_DISCOVERY, "setcover: all_addrs", all_addrs);
  ELOGDISC (wr, "\n");
  rebuild_make_locs(&wr->e.gv->logconfig, &nlocs, &locs, all_addrs);
  unref_addrset(all_addrs);
  rebuild_make_covered(&covered, wr, &nreaders, nlocs, locs);
  if (nreaders == 0)
    goto done;
  rebuild_make_locs_nrds(&locs_nrds, nreaders, nlocs, covered);
  while ((best = rebuild_select(wr->e.gv, nlocs, locs, locs_nrds, prefer_multicast)) >= 0)
  {
    rebuild_trace_covered(wr->e.gv, nreaders, nlocs, locs, locs_nrds, covered);
    ELOGDISC (wr, "  best = %d\n", best);
    rebuild_add(wr->e.gv, newas, best, nreaders, nlocs, locs, covered);
    rebuild_drop(best, nreaders, nlocs, locs_nrds, covered);
    assert (locs_nrds[best] == 0);
  }
  ddsrt_free(locs_nrds);
 done:
  ddsrt_free(locs);
  ddsrt_free(covered);
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

  ELOGDISC (wr, "rebuild_writer_addrset("PGUIDFMT"):", PGUID (wr->e.guid));
  nn_log_addrset(wr->e.gv, DDS_LC_DISCOVERY, "", wr->as);
  ELOGDISC (wr, "\n");
}

void rebuild_or_clear_writer_addrsets (struct q_globals *gv, int rebuild)
{
  struct ephash_enum_writer est;
  struct writer *wr;
  struct addrset *empty = rebuild ? NULL : new_addrset();
  GVLOGDISC ("rebuild_or_delete_writer_addrsets(%d)\n", rebuild);
  ephash_enum_writer_init (&est, gv->guid_hash);
  while ((wr = ephash_enum_writer_next (&est)) != NULL)
  {
    ddsrt_mutex_lock (&wr->e.lock);
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
         gv->as_disc. Keep as_disc unchanged, and instead make the participants point to the
         empty one. */
      unref_addrset(wr->as);
      if (rebuild)
        wr->as = ref_addrset(gv->as_disc);
      else
        wr->as = ref_addrset(empty);
    }
    ddsrt_mutex_unlock (&wr->e.lock);
  }
  ephash_enum_writer_fini (&est);
  unref_addrset(empty);
  GVLOGDISC ("rebuild_or_delete_writer_addrsets(%d) done\n", rebuild);
}

static void free_wr_prd_match (struct wr_prd_match *m)
{
  if (m)
  {
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
}

static void free_rd_pwr_match (struct q_globals *gv, struct rd_pwr_match *m)
{
  if (m)
  {
#ifdef DDSI_INCLUDE_SSM
    if (!is_unspec_locator (&m->ssm_mc_loc))
    {
      assert (ddsi_is_mcaddr (gv, &m->ssm_mc_loc));
      assert (!is_unspec_locator (&m->ssm_src_loc));
      if (ddsi_leave_mc (gv, gv->mship, gv->data_conn_mc, &m->ssm_src_loc, &m->ssm_mc_loc) < 0)
        GVWARNING ("failed to leave network partition ssm group\n");
    }
#else
    (void) gv;
#endif
    ddsrt_free (m);
  }
}

static void free_pwr_rd_match (struct pwr_rd_match *m)
{
  if (m)
  {
    if (m->acknack_xevent)
      delete_xevent (m->acknack_xevent);
    nn_reorder_free (m->u.not_in_sync.reorder);
    ddsrt_free (m);
  }
}

static void free_prd_wr_match (struct prd_wr_match *m)
{
  if (m) ddsrt_free (m);
}

static void free_rd_wr_match (struct rd_wr_match *m)
{
  if (m) ddsrt_free (m);
}

static void free_wr_rd_match (struct wr_rd_match *m)
{
  if (m) ddsrt_free (m);
}

static void proxy_writer_get_alive_state_locked (struct proxy_writer *pwr, struct proxy_writer_alive_state *st)
{
  st->alive = pwr->alive;
  st->vclock = pwr->alive_vclock;
}

static void proxy_writer_get_alive_state (struct proxy_writer *pwr, struct proxy_writer_alive_state *st)
{
  ddsrt_mutex_lock (&pwr->e.lock);
  proxy_writer_get_alive_state_locked (pwr, st);
  ddsrt_mutex_unlock (&pwr->e.lock);
}

static void writer_drop_connection (const struct ddsi_guid *wr_guid, const struct proxy_reader *prd)
{
  struct writer *wr;
  if ((wr = ephash_lookup_writer_guid (prd->e.gv->guid_hash, wr_guid)) != NULL)
  {
    struct whc_node *deferred_free_list = NULL;
    struct wr_prd_match *m;
    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      struct whc_state whcst;
      ddsrt_avl_delete (&wr_readers_treedef, &wr->readers, m);
      rebuild_writer_addrset (wr);
      remove_acked_messages (wr, &whcst, &deferred_free_list);
      wr->num_reliable_readers -= m->is_reliable;
    }
    ddsrt_mutex_unlock (&wr->e.lock);
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

static void writer_drop_local_connection (const struct ddsi_guid *wr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct writer *wr;
  if ((wr = ephash_lookup_writer_guid (rd->e.gv->guid_hash, wr_guid)) != NULL)
  {
    struct wr_rd_match *m;

    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&wr_local_readers_treedef, &wr->local_readers, &rd->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&wr_local_readers_treedef, &wr->local_readers, m);
      local_reader_ary_remove (&wr->rdary, rd);
    }
    ddsrt_mutex_unlock (&wr->e.lock);
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

static void reader_update_notify_pwr_alive_state (struct reader *rd, const struct proxy_writer *pwr, const struct proxy_writer_alive_state *alive_state)
{
  struct rd_pwr_match *m;
  bool notify = false;
  int delta = 0; /* -1: alive -> not_alive; 0: unchanged; 1: not_alive -> alive */
  ddsrt_mutex_lock (&rd->e.lock);
  if ((m = ddsrt_avl_lookup (&rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
  {
    if ((int32_t) (alive_state->vclock - m->pwr_alive_vclock) > 0)
    {
      delta = (int) alive_state->alive - (int) m->pwr_alive;
      notify = true;
      m->pwr_alive = alive_state->alive;
      m->pwr_alive_vclock = alive_state->vclock;
    }
  }
  ddsrt_mutex_unlock (&rd->e.lock);

  if (delta < 0 && rd->rhc)
  {
    struct ddsi_writer_info wrinfo;
    ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos);
    ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
  }

  /* Liveliness changed events can race each other and can, potentially, be delivered
   in a different order. */
  if (notify && rd->status_cb)
  {
    status_cb_data_t data;
    data.handle = pwr->e.iid;
    if (delta == 0)
      data.extra = (uint32_t) LIVELINESS_CHANGED_TWITCH;
    else if (delta < 0)
      data.extra = (uint32_t) LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE;
    else
      data.extra = (uint32_t) LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE;
    data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
    (rd->status_cb) (rd->status_cb_entity, &data);
  }
}

static void reader_update_notify_pwr_alive_state_guid (const struct ddsi_guid *rd_guid, const struct proxy_writer *pwr, const struct proxy_writer_alive_state *alive_state)
{
  struct reader *rd;
  if ((rd = ephash_lookup_reader_guid (pwr->e.gv->guid_hash, rd_guid)) != NULL)
    reader_update_notify_pwr_alive_state (rd, pwr, alive_state);
}

static void reader_drop_connection (const struct ddsi_guid *rd_guid, const struct proxy_writer *pwr)
{
  struct reader *rd;
  if ((rd = ephash_lookup_reader_guid (pwr->e.gv->guid_hash, rd_guid)) != NULL)
  {
    struct rd_pwr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
      ddsrt_avl_delete (&rd_writers_treedef, &rd->writers, m);
    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        status_cb_data_t data;
        data.handle = pwr->e.iid;
        data.add = false;
        data.extra = (uint32_t) (m->pwr_alive ? LIVELINESS_CHANGED_REMOVE_ALIVE : LIVELINESS_CHANGED_REMOVE_NOT_ALIVE);

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    free_rd_pwr_match (pwr->e.gv, m);
  }
}

static void reader_drop_local_connection (const struct ddsi_guid *rd_guid, const struct writer *wr)
{
  struct reader *rd;
  if ((rd = ephash_lookup_reader_guid (wr->e.gv->guid_hash, rd_guid)) != NULL)
  {
    struct rd_wr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid)) != NULL)
      ddsrt_avl_delete (&rd_local_writers_treedef, &rd->local_writers, m);
    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        /* FIXME: */
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        status_cb_data_t data;
        data.handle = wr->e.iid;
        data.add = false;
        data.extra = (uint32_t) LIVELINESS_CHANGED_REMOVE_ALIVE;

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    free_rd_wr_match (m);
  }
}

static void update_reader_init_acknack_count (const ddsrt_log_cfg_t *logcfg, const struct ephash *guid_hash, const struct ddsi_guid *rd_guid, nn_count_t count)
{
  struct reader *rd;

  /* Update the initial acknack sequence number for the reader.  See
     also reader_add_connection(). */
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "update_reader_init_acknack_count ("PGUIDFMT", %"PRId32"): ", PGUID (*rd_guid), count);
  if ((rd = ephash_lookup_reader_guid (guid_hash, rd_guid)) != NULL)
  {
    ddsrt_mutex_lock (&rd->e.lock);
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "%"PRId32" -> ", rd->init_acknack_count);
    if (count > rd->init_acknack_count)
      rd->init_acknack_count = count;
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "%"PRId32"\n", count);
    ddsrt_mutex_unlock (&rd->e.lock);
  }
  else
  {
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "reader no longer exists\n");
  }
}

static void proxy_writer_drop_connection (const struct ddsi_guid *pwr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct proxy_writer *pwr;
  if ((pwr = ephash_lookup_proxy_writer_guid (rd->e.gv->guid_hash, pwr_guid)) != NULL)
  {
    struct pwr_rd_match *m;

    ddsrt_mutex_lock (&pwr->e.lock);
    if ((m = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &rd->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&pwr_readers_treedef, &pwr->readers, m);
      if (m->in_sync != PRMSS_SYNC)
      {
        if (--pwr->n_readers_out_of_sync == 0)
          local_reader_ary_setfastpath_ok (&pwr->rdary, true);
      }
      if (rd->reliable)
        pwr->n_reliable_readers--;
      /* If no reliable readers left, there is no reason to believe the heartbeats will keep
         coming and therefore reset have_seen_heartbeat so the next reader to be created
         doesn't get initialised based on stale data */
      if (pwr->n_reliable_readers == 0)
        pwr->have_seen_heartbeat = 0;
      local_reader_ary_remove (&pwr->rdary, rd);
    }
    ddsrt_mutex_unlock (&pwr->e.lock);
    if (m)
    {
      update_reader_init_acknack_count (&rd->e.gv->logconfig, rd->e.gv->guid_hash, &rd->e.guid, m->count);
      if (m->filtered)
        nn_defrag_prune(pwr->defrag, &m->rd_guid.prefix, m->last_seq);

    }
    free_pwr_rd_match (m);
  }
}

static void proxy_reader_drop_connection (const struct ddsi_guid *prd_guid, struct writer *wr)
{
  struct proxy_reader *prd;
  if ((prd = ephash_lookup_proxy_reader_guid (wr->e.gv->guid_hash, prd_guid)) != NULL)
  {
    struct prd_wr_match *m;
    ddsrt_mutex_lock (&prd->e.lock);
    m = ddsrt_avl_lookup (&prd_writers_treedef, &prd->writers, &wr->e.guid);
    if (m)
    {
      ddsrt_avl_delete (&prd_writers_treedef, &prd->writers, m);
    }
    ddsrt_mutex_unlock (&prd->e.lock);
    free_prd_wr_match (m);
  }
}

static void writer_add_connection (struct writer *wr, struct proxy_reader *prd)
{
  struct wr_prd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;
  int pretend_everything_acked;
  m->prd_guid = prd->e.guid;
  m->is_reliable = (prd->c.xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT);
  m->assumed_in_sync = (wr->e.gv->config.retransmit_merging == REXMIT_MERGE_ALWAYS);
  m->has_replied_to_hb = !m->is_reliable;
  m->all_have_replied_to_hb = 0;
  m->non_responsive_count = 0;
  m->rexmit_requests = 0;
  /* m->demoted: see below */
  ddsrt_mutex_lock (&prd->e.lock);
  if (prd->deleting)
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - prd is being deleted\n",
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
  ddsrt_mutex_unlock (&prd->e.lock);
  m->next_acknack = DDSI_COUNT_MIN;
  m->next_nackfrag = DDSI_COUNT_MIN;
  nn_lat_estim_init (&m->hb_to_ack_latency);
  m->hb_to_ack_latency_tlastlog = now ();
  m->t_acknack_accepted.v = 0;

  ddsrt_mutex_lock (&wr->e.lock);
  if (pretend_everything_acked)
    m->seq = MAX_SEQ_NUMBER;
  else
    m->seq = wr->seq;
  m->last_seq = m->seq;
  if (ddsrt_avl_lookup_ipath (&wr_readers_treedef, &wr->readers, &prd->e.guid, &path))
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_mutex_unlock (&wr->e.lock);
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - ack seq %"PRId64"\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid), m->seq);
    ddsrt_avl_insert_ipath (&wr_readers_treedef, &wr->readers, m, &path);
    rebuild_writer_addrset (wr);
    wr->num_reliable_readers += m->is_reliable;
    ddsrt_mutex_unlock (&wr->e.lock);

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
      ddsrt_mutex_lock (&wr->e.lock);
      /* To make sure that we keep sending heartbeats at a higher rate
         at the start of this discovery, reset the hbs_since_last_write
         count to zero. */
      wr->hbcontrol.hbs_since_last_write = 0;
      if (tnext.v < wr->hbcontrol.tsched.v)
      {
        wr->hbcontrol.tsched = tnext;
        (void) resched_xevent_if_earlier (wr->heartbeat_xevent, tnext);
      }
      ddsrt_mutex_unlock (&wr->e.lock);
    }
  }
}

static void writer_add_local_connection (struct writer *wr, struct reader *rd)
{
  struct wr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&wr->e.lock);
  if (ddsrt_avl_lookup_ipath (&wr_local_readers_treedef, &wr->local_readers, &rd->e.guid, &path))
  {
    ELOGDISC (wr, "  writer_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&wr->e.lock);
    ddsrt_free (m);
    return;
  }

  ELOGDISC (wr, "  writer_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT")",
            PGUID (wr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  ddsrt_avl_insert_ipath (&wr_local_readers_treedef, &wr->local_readers, m, &path);
  local_reader_ary_insert (&wr->rdary, rd);

  /* Store available data into the late joining reader when it is reliable (we don't do
     historical data for best-effort data over the wire, so also not locally).
     FIXME: should limit ourselves to what it is available because of durability history,
     not writer history */
  if (rd->xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT && rd->xqos->durability.kind > DDS_DURABILITY_VOLATILE)
  {
    struct ddsi_tkmap *tkmap = rd->e.gv->m_tkmap;
    struct whc_sample_iter it;
    struct whc_borrowed_sample sample;
    whc_sample_iter_init(wr->whc, &it);
    while (whc_sample_iter_borrow_next(&it, &sample))
    {
      struct ddsi_writer_info wrinfo;
      struct ddsi_serdata *payload = sample.serdata;
      /* FIXME: whc has tk reference in its index nodes, which is what we really should be iterating over anyway, and so we don't really have to look them up anymore */
      struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (tkmap, payload);
      ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos);
      (void) ddsi_rhc_store (rd->rhc, &wrinfo, payload, tk);
      ddsi_tkmap_instance_unref (tkmap, tk);
    }
  }

  ddsrt_mutex_unlock (&wr->e.lock);

  ELOGDISC (wr, "\n");

  if (wr->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_PUBLICATION_MATCHED_STATUS_ID;
    data.add = true;
    data.handle = rd->e.iid;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

static void reader_add_connection (struct reader *rd, struct proxy_writer *pwr, nn_count_t *init_count, const struct proxy_writer_alive_state *alive_state)
{
  struct rd_pwr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->pwr_guid = pwr->e.guid;
  m->pwr_alive = alive_state->alive;
  m->pwr_alive_vclock = alive_state->vclock;

  ddsrt_mutex_lock (&rd->e.lock);

  /* Initial sequence number of acknacks is the highest stored (+ 1,
     done when generating the acknack) -- existing connections may be
     beyond that already, but this guarantees that one particular
     writer will always see monotonically increasing sequence numbers
     from one particular reader.  This is then used for the
     pwr_rd_match initialization */
  ELOGDISC (rd, "  reader "PGUIDFMT" init_acknack_count = %"PRId32"\n",
            PGUID (rd->e.guid), rd->init_acknack_count);
  *init_count = rd->init_acknack_count;

  if (ddsrt_avl_lookup_ipath (&rd_writers_treedef, &rd->writers, &pwr->e.guid, &path))
  {
    ELOGDISC (rd, "  reader_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (pwr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&rd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (rd, "  reader_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")\n",
              PGUID (pwr->e.guid), PGUID (rd->e.guid));
    ddsrt_avl_insert_ipath (&rd_writers_treedef, &rd->writers, m, &path);
    ddsrt_mutex_unlock (&rd->e.lock);

#ifdef DDSI_INCLUDE_SSM
    if (rd->favours_ssm && pwr->supports_ssm)
    {
      /* pwr->supports_ssm is set if addrset_contains_ssm(pwr->ssm), so
       any_ssm must succeed. */
      if (!addrset_any_uc (pwr->c.as, &m->ssm_src_loc))
        assert (0);
      if (!addrset_any_ssm (rd->e.gv, pwr->c.as, &m->ssm_mc_loc))
        assert (0);
      /* FIXME: for now, assume that the ports match for datasock_mc --
       't would be better to dynamically create and destroy sockets on
       an as needed basis. */
      int ret = ddsi_join_mc (rd->e.gv, rd->e.gv->mship, rd->e.gv->data_conn_mc, &m->ssm_src_loc, &m->ssm_mc_loc);
      if (ret < 0)
        ELOGDISC (rd, "  unable to join\n");
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
      data.handle = pwr->e.iid;
      data.add = true;
      data.extra = (uint32_t) (alive_state->alive ? LIVELINESS_CHANGED_ADD_ALIVE : LIVELINESS_CHANGED_ADD_NOT_ALIVE);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void reader_add_local_connection (struct reader *rd, struct writer *wr)
{
  struct rd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;

  ddsrt_mutex_lock (&rd->e.lock);

  if (ddsrt_avl_lookup_ipath (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid, &path))
  {
    ELOGDISC (rd, "  reader_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_mutex_unlock (&rd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    ELOGDISC (rd, "  reader_add_local_connection(wr "PGUIDFMT" rd "PGUIDFMT")\n",
              PGUID (wr->e.guid), PGUID (rd->e.guid));
    ddsrt_avl_insert_ipath (&rd_local_writers_treedef, &rd->local_writers, m, &path);
    ddsrt_mutex_unlock (&rd->e.lock);

    if (rd->status_cb)
    {
      status_cb_data_t data;
      data.handle = wr->e.iid;
      data.add = true;
      data.extra = (uint32_t) LIVELINESS_CHANGED_ADD_ALIVE;

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void proxy_writer_add_connection (struct proxy_writer *pwr, struct reader *rd, nn_mtime_t tnow, nn_count_t init_count)
{
  struct pwr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&pwr->e.lock);
  if (ddsrt_avl_lookup_ipath (&pwr_readers_treedef, &pwr->readers, &rd->e.guid, &path))
    goto already_matched;

  assert (rd->topic || is_builtin_endpoint (rd->e.guid.entityid, NN_VENDORID_ECLIPSE));
  if (pwr->ddsi2direct_cb == 0 && rd->ddsi2direct_cb != 0)
  {
    pwr->ddsi2direct_cb = rd->ddsi2direct_cb;
    pwr->ddsi2direct_cbarg = rd->ddsi2direct_cbarg;
  }

  ELOGDISC (pwr, "  proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")",
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
  m->last_seq = 0;
  m->filtered = 0;

  /* These can change as a consequence of handling data and/or
     discovery activities. The safe way of dealing with them is to
     lock the proxy writer */
  if (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) && !ddsrt_avl_is_empty (&pwr->readers))
  {
    /* builtins really don't care about multiple copies or anything */
    m->in_sync = PRMSS_SYNC;
  }
  else if (!pwr->have_seen_heartbeat || !rd->handle_as_transient_local)
  {
    /* Proxy writer hasn't seen a heartbeat yet: means we have no
       clue from what sequence number to start accepting data, nor
       where historical data ends and live data begins.

       A transient-local reader should always get all historical
       data, and so can always start-out as "out-of-sync".  Cyclone
       refuses to retransmit already ACK'd samples to a Cyclone
       reader, so if the other side is Cyclone, we can always start
       from sequence number 1.

       For non-Cyclone, if the reader is volatile, we have to just
       start from the most recent sample, even though that means
       the first samples written after matching the reader may be
       lost.  The alternative not only gets too much historical data
       but may also result in "sample lost" notifications because the
       writer is (may not be) retaining samples on behalf of this
       reader for the oldest samples and so this reader may end up
       with a partial set of old-ish samples.  Even when both are
       using KEEP_ALL and the connection doesn't fail ... */
    if (rd->handle_as_transient_local)
      m->in_sync = PRMSS_OUT_OF_SYNC;
    else if (vendor_is_eclipse (pwr->c.vendor))
      m->in_sync = PRMSS_OUT_OF_SYNC;
    else
      m->in_sync = PRMSS_SYNC;
    m->u.not_in_sync.end_of_tl_seq = MAX_SEQ_NUMBER;
  }
  else
  {
    /* transient-local reader; range of sequence numbers is already
       known */
    m->in_sync = PRMSS_OUT_OF_SYNC;
    m->u.not_in_sync.end_of_tl_seq = pwr->last_seq;
  }
  if (m->in_sync != PRMSS_SYNC)
  {
    ELOGDISC (pwr, " - out-of-sync");
    pwr->n_readers_out_of_sync++;
    local_reader_ary_setfastpath_ok (&pwr->rdary, false);
  }
  m->count = init_count;
  /* Spec says we may send a pre-emptive AckNack (8.4.2.3.4), hence we
     schedule it for the configured delay * T_MILLISECOND. From then
     on it it'll keep sending pre-emptive ones until the proxy writer
     receives a heartbeat.  (We really only need a pre-emptive AckNack
     per proxy writer, but hopefully it won't make that much of a
     difference in practice.) */
  if (rd->reliable)
  {
    uint32_t secondary_reorder_maxsamples = pwr->e.gv->config.secondary_reorder_maxsamples;

    if (rd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    {
      secondary_reorder_maxsamples = pwr->e.gv->config.primary_reorder_maxsamples;
      m->filtered = 1;
    }
    m->acknack_xevent = qxev_acknack (pwr->evq, add_duration_to_mtime (tnow, pwr->e.gv->config.preemptive_ack_delay), &pwr->e.guid, &rd->e.guid);
    m->u.not_in_sync.reorder =
      nn_reorder_new (&pwr->e.gv->logconfig, NN_REORDER_MODE_NORMAL, secondary_reorder_maxsamples, pwr->e.gv->config.late_ack_mode);
    pwr->n_reliable_readers++;
  }
  else
  {
    m->acknack_xevent = NULL;
    m->u.not_in_sync.reorder =
      nn_reorder_new (&pwr->e.gv->logconfig, NN_REORDER_MODE_MONOTONICALLY_INCREASING, pwr->e.gv->config.secondary_reorder_maxsamples, pwr->e.gv->config.late_ack_mode);
  }

  ddsrt_avl_insert_ipath (&pwr_readers_treedef, &pwr->readers, m, &path);
  local_reader_ary_insert(&pwr->rdary, rd);
  ddsrt_mutex_unlock (&pwr->e.lock);
  qxev_pwr_entityid (pwr, &rd->e.guid);

  ELOGDISC (pwr, "\n");
  return;

already_matched:
  ELOGDISC (pwr, "  proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT") - already connected\n",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
  ddsrt_mutex_unlock (&pwr->e.lock);
  ddsrt_free (m);
  return;
}




static void proxy_reader_add_connection (struct proxy_reader *prd, struct writer *wr)
{
  struct prd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
  ddsrt_mutex_lock (&prd->e.lock);
  if (ddsrt_avl_lookup_ipath (&prd_writers_treedef, &prd->writers, &wr->e.guid, &path))
  {
    ELOGDISC (prd, "  proxy_reader_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - already connected\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_mutex_unlock (&prd->e.lock);
    ddsrt_free (m);
  }
  else
  {
    assert (wr->topic || is_builtin_endpoint (wr->e.guid.entityid, NN_VENDORID_ECLIPSE));
    ELOGDISC (prd, "  proxy_reader_add_connection(wr "PGUIDFMT" prd "PGUIDFMT")\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    ddsrt_avl_insert_ipath (&prd_writers_treedef, &prd->writers, m, &path);
    ddsrt_mutex_unlock (&prd->e.lock);
    qxev_prd_entityid (prd, &wr->e.guid);
  }
}

static ddsi_entityid_t builtin_entityid_match (ddsi_entityid_t x)
{
  ddsi_entityid_t res;
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

    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER:
      res.u = NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER;
      break;
    case NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER:
      res.u = NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER;
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
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER;
      break;
    case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER:
      res.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER;
      break;
    case NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
      break;

    default:
      assert (0);
  }
  return res;
}

static void writer_qos_mismatch (struct writer * wr, dds_qos_policy_id_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID && wr->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_OFFERED_INCOMPATIBLE_QOS_STATUS_ID;
    data.extra = reason;
    (wr->status_cb) (wr->status_cb_entity, &data);
  }
}

static void reader_qos_mismatch (struct reader * rd, dds_qos_policy_id_t reason)
{
  /* When the reason is DDS_INVALID_QOS_POLICY_ID, it means that we compared
   * readers/writers from different topics: ignore that. */
  if (reason != DDS_INVALID_QOS_POLICY_ID && rd->status_cb)
  {
    status_cb_data_t data;
    data.raw_status_id = (int) DDS_REQUESTED_INCOMPATIBLE_QOS_STATUS_ID;
    data.extra = reason;
    (rd->status_cb) (rd->status_cb_entity, &data);
  }
}

static bool topickind_qos_match_p_lock (struct entity_common *rd, const dds_qos_t *rdqos, struct entity_common *wr, const dds_qos_t *wrqos, dds_qos_policy_id_t *reason)
{
  assert (is_reader_entityid (rd->guid.entityid));
  assert (is_writer_entityid (wr->guid.entityid));
  if (is_keyed_endpoint_entityid (rd->guid.entityid) != is_keyed_endpoint_entityid (wr->guid.entityid))
  {
    *reason = DDS_INVALID_QOS_POLICY_ID;
    return false;
  }
  ddsrt_mutex_t * const locks[] = { &rd->qos_lock, &wr->qos_lock, &rd->qos_lock };
  const int shift = (uintptr_t) rd > (uintptr_t) wr;
  for (int i = 0; i < 2; i++)
    ddsrt_mutex_lock (locks[i + shift]);
  bool ret = qos_match_p (rdqos, wrqos, reason);
  for (int i = 0; i < 2; i++)
    ddsrt_mutex_unlock (locks[i + shift]);
  return ret;
}

static void connect_writer_with_proxy_reader (struct writer *wr, struct proxy_reader *prd, nn_mtime_t tnow)
{
  const int isb0 = (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  const int isb1 = (is_builtin_entityid (prd->e.guid.entityid, prd->c.vendor) != 0);
  dds_qos_policy_id_t reason;
  DDSRT_UNUSED_ARG(tnow);
  if (isb0 != isb1)
    return;
  if (wr->e.onlylocal)
    return;
  if (!isb0 && !topickind_qos_match_p_lock (&prd->e, prd->c.xqos, &wr->e, wr->xqos, &reason))
  {
    writer_qos_mismatch (wr, reason);
    return;
  }
  proxy_reader_add_connection (prd, wr);
  writer_add_connection (wr, prd);
}

static void connect_proxy_writer_with_reader (struct proxy_writer *pwr, struct reader *rd, nn_mtime_t tnow)
{
  const int isb0 = (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor) != 0);
  const int isb1 = (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  dds_qos_policy_id_t reason;
  nn_count_t init_count;
  struct proxy_writer_alive_state alive_state;
  if (isb0 != isb1)
    return;
  if (rd->e.onlylocal)
    return;
  if (!isb0 && !topickind_qos_match_p_lock (&rd->e, rd->xqos, &pwr->e, pwr->c.xqos, &reason))
  {
    reader_qos_mismatch (rd, reason);
    return;
  }

  /* Initialze the reader's tracking information for the writer liveliness state to something
     sensible, but that may be outdated by the time the reader gets added to the writer's list
     of matching readers. */
  proxy_writer_get_alive_state (pwr, &alive_state);
  reader_add_connection (rd, pwr, &init_count, &alive_state);
  proxy_writer_add_connection (pwr, rd, tnow, init_count);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  proxy_writer_get_alive_state (pwr, &alive_state);
  reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
}

static bool ignore_local_p (const ddsi_guid_t *guid1, const ddsi_guid_t *guid2, const struct dds_qos *xqos1, const struct dds_qos *xqos2)
{
  assert (xqos1->present & QP_CYCLONE_IGNORELOCAL);
  assert (xqos2->present & QP_CYCLONE_IGNORELOCAL);
  switch (xqos1->ignorelocal.value)
  {
    case DDS_IGNORELOCAL_NONE:
      break;
    case DDS_IGNORELOCAL_PARTICIPANT:
      return memcmp (&guid1->prefix, &guid2->prefix, sizeof (guid1->prefix)) == 0;
    case DDS_IGNORELOCAL_PROCESS:
      return true;
  }
  switch (xqos2->ignorelocal.value)
  {
    case DDS_IGNORELOCAL_NONE:
      break;
    case DDS_IGNORELOCAL_PARTICIPANT:
      return memcmp (&guid1->prefix, &guid2->prefix, sizeof (guid1->prefix)) == 0;
    case DDS_IGNORELOCAL_PROCESS:
      return true;
  }
  return false;
}

static void connect_writer_with_reader (struct writer *wr, struct reader *rd, nn_mtime_t tnow)
{
  dds_qos_policy_id_t reason;
  (void)tnow;
  if (!is_local_orphan_endpoint (&wr->e) && (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) || is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE)))
    return;
  if (ignore_local_p (&wr->e.guid, &rd->e.guid, wr->xqos, rd->xqos))
    return;
  if (!topickind_qos_match_p_lock (&rd->e, rd->xqos, &wr->e, wr->xqos, &reason))
  {
    writer_qos_mismatch (wr, reason);
    reader_qos_mismatch (rd, reason);
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
  struct ephash * const guid_hash = e->gv->guid_hash;
  struct ephash_enum est;
  struct entity_common *em;
  enum entity_kind mkind = generic_do_match_mkind(e->kind);
  if (!is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE))
  {
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning all %ss\n",
               generic_do_match_kindstr_us (e->kind), generic_do_match_kindstr_us (mkind),
               generic_do_match_kindabbrev (e->kind), PGUID (e->guid),
               generic_do_match_kindstr(mkind));
    /* Note: we visit at least all proxies that existed when we called
     init (with the -- possible -- exception of ones that were
     deleted between our calling init and our reaching it while
     enumerating), but we may visit a single proxy reader multiple
     times. */
    ephash_enum_init (&est, guid_hash, mkind);
    while ((em = ephash_enum_next (&est)) != NULL)
      generic_do_match_connect(e, em, tnow);
    ephash_enum_fini (&est);
  }
  else
  {
    /* Built-ins have fixed QoS */
    ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
    enum entity_kind pkind = generic_do_match_isproxy (e) ? EK_PARTICIPANT : EK_PROXY_PARTICIPANT;
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning %sparticipants tgt=%"PRIx32"\n",
               generic_do_match_kindstr_us (e->kind), generic_do_match_kindstr_us (mkind),
               generic_do_match_kindabbrev (e->kind), PGUID (e->guid),
               generic_do_match_isproxy (e) ? "" : "proxy ",
               tgt_ent.u);
    if (tgt_ent.u != NN_ENTITYID_UNKNOWN)
    {
      struct entity_common *ep;
      ephash_enum_init (&est, guid_hash, pkind);
      while ((ep = ephash_enum_next (&est)) != NULL)
      {
        ddsi_guid_t tgt_guid;
        tgt_guid.prefix = ep->guid.prefix;
        tgt_guid.entityid = tgt_ent;
        if ((em = ephash_lookup_guid (guid_hash, &tgt_guid, mkind)) != NULL)
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
  mkind = generic_do_local_match_mkind (e->kind);
  EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning all %ss\n",
             generic_do_match_kindstr_us (e->kind), generic_do_match_kindstr_us (mkind),
             generic_do_match_kindabbrev (e->kind), PGUID (e->guid),
             generic_do_match_kindstr(mkind));
  /* Note: we visit at least all proxies that existed when we called
     init (with the -- possible -- exception of ones that were
     deleted between our calling init and our reaching it while
     enumerating), but we may visit a single proxy reader multiple
     times. */
  ephash_enum_init (&est, e->gv->guid_hash, mkind);
  while ((em = ephash_enum_next (&est)) != NULL)
    generic_do_local_match_connect (e, em, tnow);
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

static void new_reader_writer_common (const struct ddsrt_log_cfg *logcfg, const struct ddsi_guid *guid, const struct ddsi_sertopic *topic, const struct dds_qos *xqos)
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
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "new_%s(guid "PGUIDFMT", %s%s.%s/%s)\n",
            is_writer_entityid (guid->entityid) ? "writer" : "reader",
            PGUID (*guid),
            partition, partition_suffix,
            topic ? topic->name : "(null)",
            topic ? topic->type_name : "(null)");
}

static void endpoint_common_init (struct entity_common *e, struct endpoint_common *c, struct q_globals *gv, enum entity_kind kind, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, bool onlylocal)
{
  entity_common_init (e, gv, guid, NULL, kind, now (), NN_VENDORID_ECLIPSE, pp->e.onlylocal || onlylocal);
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

static int set_topic_type_name (dds_qos_t *xqos, const struct ddsi_sertopic * topic)
{
  if (!(xqos->present & QP_TYPE_NAME) && topic)
  {
    xqos->present |= QP_TYPE_NAME;
    xqos->type_name = ddsrt_strdup (topic->type_name);
  }
  if (!(xqos->present & QP_TOPIC_NAME) && topic)
  {
    xqos->present |= QP_TOPIC_NAME;
    xqos->topic_name = ddsrt_strdup (topic->name);
  }
  return 0;
}

/* WRITER ----------------------------------------------------------- */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static uint32_t get_partitionid_from_mapping (const struct ddsrt_log_cfg *logcfg, const struct config *config, const char *partition, const char *topic)
{
  struct config_partitionmapping_listelem *pm;
  if ((pm = find_partitionmapping (config, partition, topic)) == NULL)
    return 0;
  else
  {
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "matched writer for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n", topic, partition, pm->networkPartition);
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
  if (ddsrt_avl_is_empty (&wr->readers))
    return wr->seq;
  n = ddsrt_avl_root_non_empty (&wr_readers_treedef, &wr->readers);
  return (n->min_seq == MAX_SEQ_NUMBER) ? wr->seq : n->min_seq;
}

int writer_must_have_hb_scheduled (const struct writer *wr, const struct whc_state *whcst)
{
  if (ddsrt_avl_is_empty (&wr->readers) || whcst->max_seq < 0)
  {
    /* Can't transmit a valid heartbeat if there is no data; and it
       wouldn't actually be sent anywhere if there are no readers, so
       there is little point in processing the xevent all the time.

       Note that add_msg_to_whc and add_proxy_reader_to_writer will
       perform a reschedule. 8.4.2.2.3: need not (can't, really!) send
       a heartbeat if no data is available. */
    return 0;
  }
  else if (!((const struct wr_prd_match *) ddsrt_avl_root_non_empty (&wr_readers_treedef, &wr->readers))->all_have_replied_to_hb)
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
  if (wr->e.gv->config.whc_adaptive && wr->whc_high > wr->whc_low)
  {
    uint32_t m = 8 * wr->whc_high / 10;
    wr->whc_high = (m > wr->whc_low) ? m : wr->whc_low;
  }
}

void writer_clear_retransmitting (struct writer *wr)
{
  wr->retransmitting = 0;
  wr->t_whc_high_upd = wr->t_rexmit_end = now_et();
  ddsrt_cond_broadcast (&wr->throttle_cond);
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
    ddsrt_cond_broadcast (&wr->throttle_cond);
  if (wr->retransmitting && whcst->unacked_bytes == 0)
    writer_clear_retransmitting (wr);
  if (wr->state == WRST_LINGERING && whcst->unacked_bytes == 0)
  {
    ELOGDISC (wr, "remove_acked_messages: deleting lingering writer "PGUIDFMT"\n", PGUID (wr->e.guid));
    delete_writer_nolinger_locked (wr);
  }
  return n;
}

static void new_writer_guid_common_init (struct writer *wr, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc *whc, status_cb_t status_cb, void * status_entity)
{
  ddsrt_cond_init (&wr->throttle_cond);
  wr->seq = 0;
  wr->cs_seq = 0;
  ddsrt_atomic_st64 (&wr->seq_xmit, (uint64_t) 0);
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

  wr->xqos = ddsrt_malloc (sizeof (*wr->xqos));
  nn_xqos_copy (wr->xqos, xqos);
  nn_xqos_mergein_missing (wr->xqos, &wr->e.gv->default_xqos_wr, ~(uint64_t)0);
  assert (wr->xqos->aliased == 0);
  set_topic_type_name (wr->xqos, topic);

  ELOGDISC (wr, "WRITER "PGUIDFMT" QOS={", PGUID (wr->e.guid));
  nn_log_xqos (DDS_LC_DISCOVERY, &wr->e.gv->logconfig, wr->xqos);
  ELOGDISC (wr, "}\n");

  assert (wr->xqos->present & QP_RELIABILITY);
  wr->reliable = (wr->xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  assert (wr->xqos->present & QP_DURABILITY);
  if (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) &&
      (wr->e.guid.entityid.u != NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER))
  {
    assert (wr->xqos->history.kind == DDS_HISTORY_KEEP_LAST);
    assert ((wr->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL) ||
            (wr->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER));
  }
  wr->handle_as_transient_local = (wr->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL);
  wr->include_keyhash =
    wr->e.gv->config.generate_keyhash &&
    ((wr->e.guid.entityid.u & NN_ENTITYID_KIND_MASK) == NN_ENTITYID_KIND_WRITER_WITH_KEY);
  wr->topic = ddsi_sertopic_ref (topic);
  wr->as = new_addrset ();
  wr->as_group = NULL;

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* This is an open issue how to encrypt mesages send for various
     partitions that match multiple network partitions.  From a safety
     point of view a wierd configuration. Here we chose the first one
     that we find */
  wr->partition_id = 0;
  for (uint32_t i = 0; i < wr->xqos->partition.n && wr->partition_id == 0; i++)
    wr->partition_id = get_partitionid_from_mapping (&wr->e.gv->logconfig, &wr->e.gv->config, wr->xqos->partition.strs[i], wr->xqos->topic_name);
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

#ifdef DDSI_INCLUDE_SSM
  /* Writer supports SSM if it is mapped to a network partition for
     which the address set includes an SSM address.  If it supports
     SSM, it arbitrarily selects one SSM address from the address set
     to advertise. */
  wr->supports_ssm = 0;
  wr->ssm_as = NULL;
  if (wr->e.gv->config.allowMulticast & AMC_SSM)
  {
    nn_locator_t loc;
    int have_loc = 0;
    if (wr->partition_id == 0)
    {
      if (ddsi_is_ssm_mcaddr (wr->e.gv, &wr->e.gv->loc_default_mc))
      {
        loc = wr->e.gv->loc_default_mc;
        have_loc = 1;
      }
    }
    else
    {
      const struct config_networkpartition_listelem *np = find_networkpartition_by_id (&wr->e.gv->config, wr->partition_id);
      assert (np);
      if (addrset_any_ssm (wr->e.gv, np->as, &loc))
        have_loc = 1;
    }
    if (have_loc)
    {
      wr->supports_ssm = 1;
      wr->ssm_as = new_addrset ();
      add_to_addrset (wr->e.gv, wr->ssm_as, &loc);
      ELOGDISC (wr, "writer "PGUIDFMT": ssm=%d", PGUID (wr->e.guid), wr->supports_ssm);
      nn_log_addrset (wr->e.gv, DDS_LC_DISCOVERY, "", wr->ssm_as);
      ELOGDISC (wr, "\n");
    }
  }
#endif

  /* for non-builtin writers, select the eventqueue based on the channel it is mapped to */

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  if (!is_builtin_entityid (wr->e.guid.entityid, ownvendorid))
  {
    struct config_channel_listelem *channel = find_channel (&wr->e.gv->config, wr->xqos->transport_priority);
    ELOGDISC (wr, "writer "PGUIDFMT": transport priority %d => channel '%s' priority %d\n",
              PGUID (wr->e.guid), wr->xqos->transport_priority.value, channel->name, channel->priority);
    wr->evq = channel->evq ? channel->evq : wr->e.gv->xevents;
  }
  else
#endif
  {
    wr->evq = wr->e.gv->xevents;
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
  if (wr->xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC && wr->xqos->liveliness.lease_duration != T_NEVER)
  {
    wr->lease_duration = ddsrt_malloc (sizeof(*wr->lease_duration));
    wr->lease_duration->ldur = wr->xqos->liveliness.lease_duration;
  }
  else
  {
    wr->lease_duration = NULL;
  }

  wr->whc = whc;
  if (wr->xqos->history.kind == DDS_HISTORY_KEEP_LAST)
  {
    /* hdepth > 0 => "aggressive keep last", and in that case: why
       bother blocking for a slow receiver when the entire point of
       KEEP_LAST is to keep going (at least in a typical interpretation
       of the spec. */
    wr->whc_low = wr->whc_high = INT32_MAX;
  }
  else
  {
    wr->whc_low = wr->e.gv->config.whc_lowwater_mark;
    wr->whc_high = wr->e.gv->config.whc_init_highwater_mark.value;
  }
  assert (!(wr->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)
          ||
           (wr->whc_low == wr->whc_high && wr->whc_low == INT32_MAX));

  /* Connection admin */
  ddsrt_avl_init (&wr_readers_treedef, &wr->readers);
  ddsrt_avl_init (&wr_local_readers_treedef, &wr->local_readers);

  local_reader_ary_init (&wr->rdary);
}

static dds_return_t new_writer_guid (struct writer **wr_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc *whc, status_cb_t status_cb, void *status_entity)
{
  struct writer *wr;
  nn_mtime_t tnow = now_mt ();

  assert (is_writer_entityid (guid->entityid));
  assert (ephash_lookup_writer_guid (pp->e.gv->guid_hash, guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (&pp->e.gv->logconfig, guid, topic, xqos);
  wr = ddsrt_malloc (sizeof (*wr));
  if (wr_out)
    *wr_out = wr;

  /* want a pointer to the participant so that a parallel call to
   delete_participant won't interfere with our ability to address
   the participant */

  const bool onlylocal = topic && builtintopic_is_builtintopic (pp->e.gv->builtin_topic_interface, topic);
  endpoint_common_init (&wr->e, &wr->c, pp->e.gv, EK_WRITER, guid, group_guid, pp, onlylocal);
  new_writer_guid_common_init(wr, topic, xqos, whc, status_cb, status_entity);

  /* guid_hash needed for protocol handling, so add it before we send
   out our first message.  Also: needed for matching, and swapping
   the order if hash insert & matching creates a window during which
   neither of two endpoints being created in parallel can discover
   the other. */
  ddsrt_mutex_lock (&wr->e.lock);
  ephash_insert_writer_guid (pp->e.gv->guid_hash, wr);
  builtintopic_write (wr->e.gv->builtin_topic_interface, &wr->e, now(), true);
  ddsrt_mutex_unlock (&wr->e.lock);

  /* once it exists, match it with proxy writers and broadcast
   existence (I don't think it matters much what the order of these
   two is, but it seems likely that match-then-broadcast has a
   slightly lower likelihood that a response from a proxy reader
   gets dropped) -- but note that without adding a lock it might be
   deleted while we do so */
  match_writer_with_proxy_readers (wr, tnow);
  match_writer_with_local_readers (wr, tnow);
  sedp_write_writer (wr);

  if (wr->lease_duration != NULL)
  {
    assert (wr->lease_duration->ldur != T_NEVER);
    assert (wr->xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC);
    assert (!is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE));

    /* Store writer lease duration in participant's heap in case of automatic liveliness */
    ddsrt_mutex_lock (&pp->e.lock);
    ddsrt_fibheap_insert (&ldur_fhdef, &pp->ldur_auto_wr, wr->lease_duration);
    ddsrt_mutex_unlock (&pp->e.lock);

    /* Trigger pmd update */
    (void) resched_xevent_if_earlier (pp->pmd_update_xevent, now_mt ());
  }

  return 0;
}

dds_return_t new_writer (struct writer **wr_out, struct q_globals *gv, struct ddsi_guid *wrguid, const struct ddsi_guid *group_guid, const struct ddsi_guid *ppguid, const struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc * whc, status_cb_t status_cb, void *status_cb_arg)
{
  struct participant *pp;
  dds_return_t rc;
  uint32_t kind;

  if ((pp = ephash_lookup_participant_guid (gv->guid_hash, ppguid)) == NULL)
  {
    GVLOGDISC ("new_writer - participant "PGUIDFMT" not found\n", PGUID (*ppguid));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  /* participant can't be freed while we're mucking around cos we are
     awake and do not touch the thread's vtime (ephash_lookup already
     verifies we're awake) */
  wrguid->prefix = pp->e.guid.prefix;
  kind = topic->topickind_no_key ? NN_ENTITYID_KIND_WRITER_NO_KEY : NN_ENTITYID_KIND_WRITER_WITH_KEY;
  if ((rc = pp_allocate_entityid (&wrguid->entityid, kind, pp)) < 0)
    return rc;
  return new_writer_guid (wr_out, wrguid, group_guid, pp, topic, xqos, whc, status_cb, status_cb_arg);
}

struct local_orphan_writer *new_local_orphan_writer (struct q_globals *gv, ddsi_entityid_t entityid, struct ddsi_sertopic *topic, const struct dds_qos *xqos, struct whc *whc)
{
  ddsi_guid_t guid;
  struct local_orphan_writer *lowr;
  struct writer *wr;
  nn_mtime_t tnow = now_mt ();

  GVLOGDISC ("new_local_orphan_writer(%s/%s)\n", topic->name, topic->type_name);
  lowr = ddsrt_malloc (sizeof (*lowr));
  wr = &lowr->wr;

  memset (&guid.prefix, 0, sizeof (guid.prefix));
  guid.entityid = entityid;
  entity_common_init (&wr->e, gv, &guid, NULL, EK_WRITER, now (), NN_VENDORID_ECLIPSE, true);
  wr->c.pp = NULL;
  memset (&wr->c.group_guid, 0, sizeof (wr->c.group_guid));
  new_writer_guid_common_init (wr, topic, xqos, whc, 0, NULL);
  ephash_insert_writer_guid (gv->guid_hash, wr);
  builtintopic_write (gv->builtin_topic_interface, &wr->e, now(), true);
  match_writer_with_local_readers (wr, tnow);
  return lowr;
}

void update_writer_qos (struct writer *wr, const dds_qos_t *xqos)
{
  ddsrt_mutex_lock (&wr->e.lock);
  if (update_qos_locked (&wr->e, wr->xqos, xqos, now ()))
    sedp_write_writer (wr);
  ddsrt_mutex_unlock (&wr->e.lock);
}

static void gc_delete_writer (struct gcreq *gcreq)
{
  struct writer *wr = gcreq->arg;
  ELOGDISC (wr, "gc_delete_writer(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (wr->e.guid));
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

  while (!ddsrt_avl_is_empty (&wr->readers))
  {
    struct wr_prd_match *m = ddsrt_avl_root_non_empty (&wr_readers_treedef, &wr->readers);
    ddsrt_avl_delete (&wr_readers_treedef, &wr->readers, m);
    proxy_reader_drop_connection (&m->prd_guid, wr);
    free_wr_prd_match (m);
  }
  while (!ddsrt_avl_is_empty (&wr->local_readers))
  {
    struct wr_rd_match *m = ddsrt_avl_root_non_empty (&wr_local_readers_treedef, &wr->local_readers);
    ddsrt_avl_delete (&wr_local_readers_treedef, &wr->local_readers, m);
    reader_drop_local_connection (&m->rd_guid, wr);
    free_wr_rd_match (m);
  }
  if (wr->lease_duration != NULL)
  {
    assert (wr->lease_duration->ldur == DDS_DURATION_INVALID);
    ddsrt_free (wr->lease_duration);
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
  ddsrt_free (wr->xqos);
  local_reader_ary_fini (&wr->rdary);
  ddsrt_cond_destroy (&wr->throttle_cond);

  ddsi_sertopic_unref ((struct ddsi_sertopic *) wr->topic);
  endpoint_common_fini (&wr->e, &wr->c);
  ddsrt_free (wr);
}

static void gc_delete_writer_throttlewait (struct gcreq *gcreq)
{
  struct writer *wr = gcreq->arg;
  ELOGDISC (wr, "gc_delete_writer_throttlewait(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (wr->e.guid));
  /* We now allow GC while blocked on a full WHC, but we still don't allow deleting a writer while blocked on it. The writer's state must be DELETING by the time we get here, and that means the transmit path is no longer blocked. It doesn't imply that the write thread is no longer in throttle_writer(), just that if it is, it will soon return from there. Therefore, block until it isn't throttling anymore. We can safely lock the writer, as we're on the separate GC thread. */
  assert (wr->state == WRST_DELETING);
  ddsrt_mutex_lock (&wr->e.lock);
  while (wr->throttling)
    ddsrt_cond_wait (&wr->throttle_cond, &wr->e.lock);
  ddsrt_mutex_unlock (&wr->e.lock);
  gcreq_requeue (gcreq, gc_delete_writer);
}

static void writer_set_state (struct writer *wr, enum writer_state newstate)
{
  ASSERT_MUTEX_HELD (&wr->e.lock);
  ELOGDISC (wr, "writer_set_state("PGUIDFMT") state transition %d -> %d\n", PGUID (wr->e.guid), wr->state, newstate);
  assert (newstate > wr->state);
  if (wr->state == WRST_OPERATIONAL)
  {
    /* Unblock all throttled writers (alternative method: clear WHC --
       but with parallel writes and very small limits on the WHC size,
       that doesn't guarantee no-one will block). A truly blocked
       write() is a problem because it prevents the gc thread from
       cleaning up the writer.  (Note: late assignment to wr->state is
       ok, 'tis all protected by the writer lock.) */
    ddsrt_cond_broadcast (&wr->throttle_cond);
  }
  wr->state = newstate;
}

dds_return_t unblock_throttled_writer (struct q_globals *gv, const struct ddsi_guid *guid)
{
  struct writer *wr;
  assert (is_writer_entityid (guid->entityid));
  if ((wr = ephash_lookup_writer_guid (gv->guid_hash, guid)) == NULL)
  {
    GVLOGDISC ("unblock_throttled_writer(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("unblock_throttled_writer(guid "PGUIDFMT") ...\n", PGUID (*guid));
  ddsrt_mutex_lock (&wr->e.lock);
  writer_set_state (wr, WRST_INTERRUPT);
  ddsrt_mutex_unlock (&wr->e.lock);
  return 0;
}

dds_return_t delete_writer_nolinger_locked (struct writer *wr)
{
  ELOGDISC (wr, "delete_writer_nolinger(guid "PGUIDFMT") ...\n", PGUID (wr->e.guid));
  ASSERT_MUTEX_HELD (&wr->e.lock);
  builtintopic_write (wr->e.gv->builtin_topic_interface, &wr->e, now(), false);
  local_reader_ary_setinvalid (&wr->rdary);
  ephash_remove_writer_guid (wr->e.gv->guid_hash, wr);
  writer_set_state (wr, WRST_DELETING);
  if (wr->lease_duration != NULL) {
    ddsrt_mutex_lock (&wr->c.pp->e.lock);
    ddsrt_fibheap_delete (&ldur_fhdef, &wr->c.pp->ldur_auto_wr, wr->lease_duration);
    ddsrt_mutex_unlock (&wr->c.pp->e.lock);
    wr->lease_duration->ldur = DDS_DURATION_INVALID;
    resched_xevent_if_earlier (wr->c.pp->pmd_update_xevent, now_mt ());
  }
  gcreq_writer (wr);
  return 0;
}

dds_return_t delete_writer_nolinger (struct q_globals *gv, const struct ddsi_guid *guid)
{
  struct writer *wr;
  /* We take no care to ensure application writers are not deleted
     while they still have unacknowledged data (unless it takes too
     long), but we don't care about the DDSI built-in writers: we deal
     with that anyway because of the potential for crashes of remote
     DDSI participants. But it would be somewhat more elegant to do it
     differently. */
  assert (is_writer_entityid (guid->entityid));
  if ((wr = ephash_lookup_writer_guid (gv->guid_hash, guid)) == NULL)
  {
    GVLOGDISC ("delete_writer_nolinger(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("delete_writer_nolinger(guid "PGUIDFMT") ...\n", PGUID (*guid));
  ddsrt_mutex_lock (&wr->e.lock);
  delete_writer_nolinger_locked (wr);
  ddsrt_mutex_unlock (&wr->e.lock);
  return 0;
}

void delete_local_orphan_writer (struct local_orphan_writer *lowr)
{
  assert (thread_is_awake ());
  ddsrt_mutex_lock (&lowr->wr.e.lock);
  delete_writer_nolinger_locked (&lowr->wr);
  ddsrt_mutex_unlock (&lowr->wr.e.lock);
}

dds_return_t delete_writer (struct q_globals *gv, const struct ddsi_guid *guid)
{
  struct writer *wr;
  struct whc_state whcst;
  if ((wr = ephash_lookup_writer_guid (gv->guid_hash, guid)) == NULL)
  {
    GVLOGDISC ("delete_writer(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("delete_writer(guid "PGUIDFMT") ...\n", PGUID (*guid));
  ddsrt_mutex_lock (&wr->e.lock);

  /* If no unack'ed data, don't waste time or resources (expected to
     be the usual case), do it immediately.  If more data is still
     coming in (which can't really happen at the moment, but might
     again in the future) it'll potentially be discarded.  */
  whc_get_state(wr->whc, &whcst);
  if (whcst.unacked_bytes == 0)
  {
    GVLOGDISC ("delete_writer(guid "PGUIDFMT") - no unack'ed samples\n", PGUID (*guid));
    delete_writer_nolinger_locked (wr);
    ddsrt_mutex_unlock (&wr->e.lock);
  }
  else
  {
    nn_mtime_t tsched;
    int32_t tsec, tusec;
    writer_set_state (wr, WRST_LINGERING);
    ddsrt_mutex_unlock (&wr->e.lock);
    tsched = add_duration_to_mtime (now_mt (), wr->e.gv->config.writer_linger_duration);
    mtime_to_sec_usec (&tsec, &tusec, tsched);
    GVLOGDISC ("delete_writer(guid "PGUIDFMT") - unack'ed samples, will delete when ack'd or at t = %"PRId32".%06"PRId32"\n",
               PGUID (*guid), tsec, tusec);
    qxev_delete_writer (gv->xevents, tsched, &wr->e.guid);
  }
  return 0;
}

/* READER ----------------------------------------------------------- */

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
static struct addrset *get_as_from_mapping (const struct q_globals *gv, const char *partition, const char *topic)
{
  struct config_partitionmapping_listelem *pm;
  struct addrset *as = new_addrset ();
  if ((pm = find_partitionmapping (&gv->config, partition, topic)) != NULL)
  {
    GVLOGDISC ("matched reader for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n",
               topic, partition, pm->networkPartition);
    assert (pm->partition->as);
    copy_addrset_into_addrset (gv, as, pm->partition->as);
  }
  return as;
}

struct join_leave_mcast_helper_arg {
  ddsi_tran_conn_t conn;
  struct q_globals *gv;
};

static void join_mcast_helper (const nn_locator_t *n, void *varg)
{
  struct join_leave_mcast_helper_arg *arg = varg;
  struct q_globals *gv = arg->gv;
  if (ddsi_is_mcaddr (gv, n))
  {
    if (n->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
    {
      if (ddsi_join_mc (gv, arg->gv->mship, arg->conn, NULL, n) < 0)
      {
        GVWARNING ("failed to join network partition multicast group\n");
      }
    }
    else /* join all addresses that include this node */
    {
      {
        nn_locator_t l = *n;
        nn_udpv4mcgen_address_t l1;
        uint32_t iph;
        memcpy(&l1, l.address, sizeof(l1));
        l.kind = NN_LOCATOR_KIND_UDPv4;
        memset(l.address, 0, 12);
        iph = ntohl(l1.ipv4.s_addr);
        for (uint32_t i = 1; i < ((uint32_t)1 << l1.count); i++)
        {
          uint32_t ipn, iph1 = iph;
          if (i & (1u << l1.idx))
          {
            iph1 |= (i << l1.base);
            ipn = htonl(iph1);
            memcpy(l.address + 12, &ipn, 4);
            if (ddsi_join_mc (gv, gv->mship, arg->conn, NULL, &l) < 0)
            {
              GVWARNING ("failed to join network partition multicast group\n");
            }
          }
        }
      }
    }
  }
}

static void leave_mcast_helper (const nn_locator_t *n, void *varg)
{
  struct join_leave_mcast_helper_arg *arg = varg;
  struct q_globals *gv = arg->gv;
  if (ddsi_is_mcaddr (gv, n))
  {
    if (n->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
    {
      if (ddsi_leave_mc (gv, gv->mship, arg->conn, NULL, n) < 0)
      {
        GVWARNING ("failed to leave network partition multicast group\n");
      }
    }
    else /* join all addresses that include this node */
    {
      {
        nn_locator_t l = *n;
        nn_udpv4mcgen_address_t l1;
        uint32_t iph;
        memcpy(&l1, l.address, sizeof(l1));
        l.kind = NN_LOCATOR_KIND_UDPv4;
        memset(l.address, 0, 12);
        iph = ntohl(l1.ipv4.s_addr);
        for (uint32_t i = 1; i < ((uint32_t)1 << l1.count); i++)
        {
          uint32_t ipn, iph1 = iph;
          if (i & (1u << l1.idx))
          {
            iph1 |= (i << l1.base);
            ipn = htonl(iph1);
            memcpy(l.address + 12, &ipn, 4);
            if (ddsi_leave_mc (gv, arg->gv->mship, arg->conn, NULL, &l) < 0)
            {
              GVWARNING ("failed to leave network partition multicast group\n");
            }
          }
        }
      }
    }
  }
}
#endif /* DDSI_INCLUDE_NETWORK_PARTITIONS */

static dds_return_t new_reader_guid
(
  struct reader **rd_out,
  const struct ddsi_guid *guid,
  const struct ddsi_guid *group_guid,
  struct participant *pp,
  const struct ddsi_sertopic *topic,
  const struct dds_qos *xqos,
  struct ddsi_rhc *rhc,
  status_cb_t status_cb,
  void * status_entity
)
{
  /* see new_writer_guid for commenets */

  struct reader *rd;
  nn_mtime_t tnow = now_mt ();

  assert (!is_writer_entityid (guid->entityid));
  assert (ephash_lookup_reader_guid (pp->e.gv->guid_hash, guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (&pp->e.gv->logconfig, guid, topic, xqos);
  rd = ddsrt_malloc (sizeof (*rd));
  if (rd_out)
    *rd_out = rd;

  const bool onlylocal = topic && builtintopic_is_builtintopic (pp->e.gv->builtin_topic_interface, topic);
  endpoint_common_init (&rd->e, &rd->c, pp->e.gv, EK_READER, guid, group_guid, pp, onlylocal);

  /* Copy QoS, merging in defaults */
  rd->xqos = ddsrt_malloc (sizeof (*rd->xqos));
  nn_xqos_copy (rd->xqos, xqos);
  nn_xqos_mergein_missing (rd->xqos, &pp->e.gv->default_xqos_rd, ~(uint64_t)0);
  assert (rd->xqos->aliased == 0);
  set_topic_type_name (rd->xqos, topic);

  if (rd->e.gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    ELOGDISC (rd, "READER "PGUIDFMT" QOS={", PGUID (rd->e.guid));
    nn_log_xqos (DDS_LC_DISCOVERY, &rd->e.gv->logconfig, rd->xqos);
    ELOGDISC (rd, "}\n");
  }
  assert (rd->xqos->present & QP_RELIABILITY);
  rd->reliable = (rd->xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  assert (rd->xqos->present & QP_DURABILITY);
  /* The builtin volatile secure writer applies a filter which is used to send the secure
   * crypto token only to the destination reader for which the crypto tokens are applicable.
   * Thus the builtin volatile secure reader will receive gaps in the sequence numbers of
   * the messages received. Therefore the out-of-order list of the proxy writer cannot be
   * used for this reader and reader specific out-of-order list must be used which is
   * used for handling transient local data.
   */
  rd->handle_as_transient_local = (rd->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL) ||
                                  (rd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER);
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
    ddsi_rhc_set_qos (rd->rhc, rd->xqos);
  }
  assert (rd->xqos->present & QP_LIVELINESS);

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  rd->as = new_addrset ();
  if (pp->e.gv->config.allowMulticast & ~AMC_SPDP)
  {
    /* compile address set from the mapped network partitions */
    for (uint32_t i = 0; i < rd->xqos->partition.n; i++)
    {
      struct addrset *pas = get_as_from_mapping (pp->e.gv, rd->xqos->partition.strs[i], rd->xqos->topic_name);
      if (pas)
      {
#ifdef DDSI_INCLUDE_SSM
        copy_addrset_into_addrset_no_ssm (pp->e.gv, rd->as, pas);
        if (addrset_contains_ssm (pp->e.gv, pas) && rd->e.gv->config.allowMulticast & AMC_SSM)
          rd->favours_ssm = 1;
#else
        copy_addrset_into_addrset (pp->e.gv, rd->as, pas);
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
      struct join_leave_mcast_helper_arg arg;
      arg.conn = pp->e.gv->data_conn_mc;
      arg.gv = pp->e.gv;
      addrset_forall (rd->as, join_mcast_helper, &arg);
      if (pp->e.gv->logconfig.c.mask & DDS_LC_DISCOVERY)
      {
        ELOGDISC (pp, "READER "PGUIDFMT" locators={", PGUID (rd->e.guid));
        nn_log_addrset(pp->e.gv, DDS_LC_DISCOVERY, "", rd->as);
        ELOGDISC (pp, "}\n");
      }
    }
#ifdef DDSI_INCLUDE_SSM
    else
    {
      /* Note: SSM requires NETWORK_PARTITIONS; if network partitions
         do not override the default, we should check whether the
         default is an SSM address. */
      if (ddsi_is_ssm_mcaddr (pp->e.gv, &pp->e.gv->loc_default_mc) && pp->e.gv->config.allowMulticast & AMC_SSM)
        rd->favours_ssm = 1;
    }
#endif
  }
#ifdef DDSI_INCLUDE_SSM
  if (rd->favours_ssm)
    ELOGDISC (pp, "READER "PGUIDFMT" ssm=%d\n", PGUID (rd->e.guid), rd->favours_ssm);
#endif
#endif

  ddsrt_avl_init (&rd_writers_treedef, &rd->writers);
  ddsrt_avl_init (&rd_local_writers_treedef, &rd->local_writers);

  ddsrt_mutex_lock (&rd->e.lock);
  ephash_insert_reader_guid (pp->e.gv->guid_hash, rd);
  builtintopic_write (pp->e.gv->builtin_topic_interface, &rd->e, now(), true);
  ddsrt_mutex_unlock (&rd->e.lock);

  match_reader_with_proxy_writers (rd, tnow);
  match_reader_with_local_writers (rd, tnow);
  sedp_write_reader (rd);
  return 0;
}

dds_return_t new_reader
(
  struct reader **rd_out,
  struct q_globals *gv,
  struct ddsi_guid *rdguid,
  const struct ddsi_guid *group_guid,
  const struct ddsi_guid *ppguid,
  const struct ddsi_sertopic *topic,
  const struct dds_qos *xqos,
  struct ddsi_rhc * rhc,
  status_cb_t status_cb,
  void * status_cbarg
)
{
  struct participant * pp;
  dds_return_t rc;
  uint32_t kind;

  if ((pp = ephash_lookup_participant_guid (gv->guid_hash, ppguid)) == NULL)
  {
    GVLOGDISC ("new_reader - participant "PGUIDFMT" not found\n", PGUID (*ppguid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  rdguid->prefix = pp->e.guid.prefix;
  kind = topic->topickind_no_key ? NN_ENTITYID_KIND_READER_NO_KEY : NN_ENTITYID_KIND_READER_WITH_KEY;
  if ((rc = pp_allocate_entityid (&rdguid->entityid, kind, pp)) < 0)
    return rc;
  return new_reader_guid (rd_out, rdguid, group_guid, pp, topic, xqos, rhc, status_cb, status_cbarg);
}

static void gc_delete_reader (struct gcreq *gcreq)
{
  /* see gc_delete_writer for comments */
  struct reader *rd = gcreq->arg;
  ELOGDISC (rd, "gc_delete_reader(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (rd->e.guid));
  gcreq_free (gcreq);

  while (!ddsrt_avl_is_empty (&rd->writers))
  {
    struct rd_pwr_match *m = ddsrt_avl_root_non_empty (&rd_writers_treedef, &rd->writers);
    ddsrt_avl_delete (&rd_writers_treedef, &rd->writers, m);
    proxy_writer_drop_connection (&m->pwr_guid, rd);
    free_rd_pwr_match (rd->e.gv, m);
  }
  while (!ddsrt_avl_is_empty (&rd->local_writers))
  {
    struct rd_wr_match *m = ddsrt_avl_root_non_empty (&rd_local_writers_treedef, &rd->local_writers);
    ddsrt_avl_delete (&rd_local_writers_treedef, &rd->local_writers, m);
    writer_drop_local_connection (&m->wr_guid, rd);
    free_rd_wr_match (m);
  }

  if (!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE))
    sedp_dispose_unregister_reader (rd);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  {
    struct join_leave_mcast_helper_arg arg;
    arg.conn = rd->e.gv->data_conn_mc;
    arg.gv = rd->e.gv;
    addrset_forall (rd->as, leave_mcast_helper, &arg);
  }
#endif
  if (rd->rhc && is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE))
  {
    ddsi_rhc_free (rd->rhc);
  }
  if (rd->status_cb)
  {
    (rd->status_cb) (rd->status_cb_entity, NULL);
  }
  ddsi_sertopic_unref ((struct ddsi_sertopic *) rd->topic);

  nn_xqos_fini (rd->xqos);
  ddsrt_free (rd->xqos);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  unref_addrset (rd->as);
#endif

  endpoint_common_fini (&rd->e, &rd->c);
  ddsrt_free (rd);
}

dds_return_t delete_reader (struct q_globals *gv, const struct ddsi_guid *guid)
{
  struct reader *rd;
  assert (!is_writer_entityid (guid->entityid));
  if ((rd = ephash_lookup_reader_guid (gv->guid_hash, guid)) == NULL)
  {
    GVLOGDISC ("delete_reader_guid(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("delete_reader_guid(guid "PGUIDFMT") ...\n", PGUID (*guid));
  builtintopic_write (rd->e.gv->builtin_topic_interface, &rd->e, now(), false);
  ephash_remove_reader_guid (gv->guid_hash, rd);
  gcreq_reader (rd);
  return 0;
}

void update_reader_qos (struct reader *rd, const dds_qos_t *xqos)
{
  ddsrt_mutex_lock (&rd->e.lock);
  if (update_qos_locked (&rd->e, rd->xqos, xqos, now ()))
    sedp_write_reader (rd);
  ddsrt_mutex_unlock (&rd->e.lock);
}

/* PROXY-PARTICIPANT ------------------------------------------------ */
const ddsrt_fibheap_def_t lease_fhdef_proxypp = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct lease, pp_heapnode), compare_lease_tdur);

static void gc_proxy_participant_lease (struct gcreq *gcreq)
{
  lease_free (gcreq->arg);
  gcreq_free (gcreq);
}

static void proxy_participant_replace_minl (struct proxy_participant *proxypp, bool manbypp, struct lease *lnew)
{
  /* By loading/storing the pointer atomically, we ensure we always
     read a valid (or once valid) lease. By delaying freeing the lease
     through the garbage collector, we ensure whatever lease update
     occurs in parallel completes before the memory is released. */
  struct gcreq *gcreq = gcreq_new (proxypp->e.gv->gcreq_queue, gc_proxy_participant_lease);
  struct lease *lease_old = ddsrt_atomic_ldvoidp (manbypp ? &proxypp->minl_man : &proxypp->minl_auto);
  lease_unregister (lease_old); /* ensures lease will not expire while it is replaced */
  gcreq->arg = lease_old;
  gcreq_enqueue (gcreq);
  ddsrt_atomic_stvoidp (manbypp ? &proxypp->minl_man : &proxypp->minl_auto, lnew);
}

void proxy_participant_reassign_lease (struct proxy_participant *proxypp, struct lease *newlease)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  if (proxypp->owns_lease)
  {
    struct lease *minl = ddsrt_fibheap_min (&lease_fhdef_proxypp, &proxypp->leaseheap_auto);
    ddsrt_fibheap_delete (&lease_fhdef_proxypp, &proxypp->leaseheap_auto, proxypp->lease);
    if (minl == proxypp->lease)
    {
      if ((minl = ddsrt_fibheap_min (&lease_fhdef_proxypp, &proxypp->leaseheap_auto)) != NULL)
      {
        dds_duration_t trem = minl->tdur - proxypp->lease->tdur;
        assert (trem >= 0);
        nn_etime_t texp = add_duration_to_etime (now_et(), trem);
        struct lease *lnew = lease_new (texp, minl->tdur, minl->entity);
        proxy_participant_replace_minl (proxypp, false, lnew);
        lease_register (lnew);
      }
      else
      {
        proxy_participant_replace_minl (proxypp, false, NULL);
      }
    }

    /* Lease renewal is done by the receive thread without locking the
      proxy participant (and I'd like to keep it that way), but that
      means we must guarantee that the lease pointer remains valid once
      loaded.

      By loading/storing the pointer atomically, we ensure we always
      read a valid (or once valid) value, by delaying the freeing
      through the garbage collector, we ensure whatever lease update
      occurs in parallel completes before the memory is released.

      The lease_unregister call ensures the lease will never expire
      while we are messing with it. */
    struct gcreq *gcreq = gcreq_new (proxypp->e.gv->gcreq_queue, gc_proxy_participant_lease);
    lease_unregister (proxypp->lease);
    gcreq->arg = proxypp->lease;
    gcreq_enqueue (gcreq);
    proxypp->owns_lease = 0;
  }
  proxypp->lease = newlease;

  ddsrt_mutex_unlock (&proxypp->e.lock);
}

struct bestab {
  unsigned besflag;
  unsigned prismtech_besflag;
  unsigned entityid;
};

static void create_proxy_builtin_endpoints(
  struct q_globals *gv,
  const struct bestab *bestab,
  int nbes,
  const struct ddsi_guid *ppguid,
  struct proxy_participant *proxypp,
  nn_wctime_t timestamp,
  dds_qos_t *xqos_wr,
  dds_qos_t *xqos_rd)
{
  nn_plist_t plist_rd, plist_wr;
  int i;
  /* Note: no entity name or group GUID supplied, but that shouldn't
   * matter, as these are internal to DDSI and don't use group
   * coherency
   */
  nn_plist_init_empty (&plist_wr);
  nn_plist_init_empty (&plist_rd);
  nn_xqos_copy (&plist_wr.qos, xqos_wr);
  nn_xqos_copy (&plist_rd.qos, xqos_rd);
  for (i = 0; i < nbes; i++)
  {
    const struct bestab *te = &bestab[i];
    if ((proxypp->bes & te->besflag) || (proxypp->prismtech_bes & te->prismtech_besflag))
    {
      ddsi_guid_t guid1;
      guid1.prefix = proxypp->e.guid.prefix;
      guid1.entityid.u = te->entityid;
      assert (is_builtin_entityid (guid1.entityid, proxypp->vendor));
      if (is_writer_entityid (guid1.entityid))
      {
        new_proxy_writer (gv, ppguid, &guid1, proxypp->as_meta, &plist_wr, gv->builtins_dqueue, gv->xevents, timestamp, 0);
      }
      else
      {
#ifdef DDSI_INCLUDE_SSM
        const int ssm = addrset_contains_ssm (gv, proxypp->as_meta);
#else
        const int ssm = 0;
#endif
        new_proxy_reader (gv, ppguid, &guid1, proxypp->as_meta, &plist_rd, timestamp, 0, ssm);
      }
    }
  }
  nn_plist_fini (&plist_wr);
  nn_plist_fini (&plist_rd);
}


static void add_proxy_builtin_endpoints(
  struct q_globals *gv,
  const struct ddsi_guid *ppguid,
  struct proxy_participant *proxypp,
  nn_wctime_t timestamp)
{
  /* Add proxy endpoints based on the advertised (& possibly augmented
     ...) built-in endpoint set. */
#define PT_TE(ap_, a_, bp_, b_) { 0, NN_##ap_##BUILTIN_ENDPOINT_##a_, NN_ENTITYID_##bp_##_BUILTIN_##b_ }
#define TE(ap_, a_, bp_, b_) { NN_##ap_##BUILTIN_ENDPOINT_##a_, 0, NN_ENTITYID_##bp_##_BUILTIN_##b_ }
#define LTE(a_, bp_, b_) { NN_##BUILTIN_ENDPOINT_##a_, 0, NN_ENTITYID_##bp_##_BUILTIN_##b_ }

  /* 'Default' proxy endpoints. */
  static const struct bestab bestab_default[] = {
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
  create_proxy_builtin_endpoints(gv,
                                 bestab_default,
                                 (int)(sizeof (bestab_default) / sizeof (*bestab_default)),
                                 ppguid,
                                 proxypp,
                                 timestamp,
                                 &gv->builtin_endpoint_xqos_wr,
                                 &gv->builtin_endpoint_xqos_rd);

#ifdef DDSI_INCLUDE_SECURITY
  /* Security 'default' proxy endpoints. */
  static const struct bestab bestab_security[] = {
    LTE (PUBLICATION_MESSAGE_SECURE_ANNOUNCER, SEDP, PUBLICATIONS_SECURE_WRITER),
    LTE (PUBLICATION_MESSAGE_SECURE_DETECTOR, SEDP, PUBLICATIONS_SECURE_READER),
    LTE (SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER, SEDP, SUBSCRIPTIONS_SECURE_WRITER),
    LTE (SUBSCRIPTION_MESSAGE_SECURE_DETECTOR, SEDP, SUBSCRIPTIONS_SECURE_READER),
    LTE (PARTICIPANT_MESSAGE_SECURE_ANNOUNCER, P2P, PARTICIPANT_MESSAGE_SECURE_WRITER),
    LTE (PARTICIPANT_MESSAGE_SECURE_DETECTOR, P2P, PARTICIPANT_MESSAGE_SECURE_READER),
    TE (DISC_, PARTICIPANT_SECURE_ANNOUNCER, SPDP_RELIABLE, PARTICIPANT_SECURE_WRITER),
    TE (DISC_, PARTICIPANT_SECURE_DETECTOR, SPDP_RELIABLE, PARTICIPANT_SECURE_READER)
  };
  create_proxy_builtin_endpoints(gv,
                                 bestab_security,
                                 (int)(sizeof (bestab_security) / sizeof (*bestab_security)),
                                 ppguid,
                                 proxypp,
                                 timestamp,
                                 &gv->builtin_endpoint_xqos_wr,
                                 &gv->builtin_endpoint_xqos_rd);

  /* Security 'volatile' proxy endpoints. */
  static const struct bestab bestab_volatile[] = {
    LTE (PARTICIPANT_VOLATILE_SECURE_ANNOUNCER, P2P, PARTICIPANT_VOLATILE_SECURE_WRITER),
    LTE (PARTICIPANT_VOLATILE_SECURE_DETECTOR, P2P, PARTICIPANT_VOLATILE_SECURE_READER)
  };
  create_proxy_builtin_endpoints(gv,
                                 bestab_volatile,
                                 (int)(sizeof (bestab_volatile) / sizeof (*bestab_volatile)),
                                 ppguid,
                                 proxypp,
                                 timestamp,
                                 &gv->builtin_volatile_xqos_wr,
                                 &gv->builtin_volatile_xqos_rd);

  /* Security 'stateless' proxy endpoints. */
  static const struct bestab bestab_stateless[] = {
    LTE (PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER, P2P, PARTICIPANT_STATELESS_MESSAGE_WRITER),
    LTE (PARTICIPANT_STATELESS_MESSAGE_DETECTOR, P2P, PARTICIPANT_STATELESS_MESSAGE_READER)
  };
  create_proxy_builtin_endpoints(gv,
                                 bestab_stateless,
                                 (int)(sizeof (bestab_stateless) / sizeof (*bestab_stateless)),
                                 ppguid,
                                 proxypp,
                                 timestamp,
                                 &gv->builtin_stateless_xqos_wr,
                                 &gv->builtin_stateless_xqos_rd);
#endif

  /* Register lease for auto liveliness, but be careful not to accidentally re-register
     DDSI2's lease, as we may have become dependent on DDSI2 any time after
     ephash_insert_proxy_participant_guid even if privileged_pp_guid was NULL originally */
  ddsrt_mutex_lock (&proxypp->e.lock);

  if (proxypp->owns_lease)
    lease_register (ddsrt_atomic_ldvoidp (&proxypp->minl_auto));

  builtintopic_write (gv->builtin_topic_interface, &proxypp->e, timestamp, true);
  ddsrt_mutex_unlock (&proxypp->e.lock);

#undef PT_TE
#undef TE
#undef LTE
}

static void proxy_participant_add_pwr_lease_locked (struct proxy_participant * proxypp, const struct proxy_writer * pwr)
{
  struct lease *minl_prev;
  struct lease *minl_new;
  ddsrt_fibheap_t *lh;
  bool manbypp;

  assert (pwr->lease != NULL);
  manbypp = (pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
  lh = manbypp ? &proxypp->leaseheap_man : &proxypp->leaseheap_auto;
  minl_prev = ddsrt_fibheap_min (&lease_fhdef_proxypp, lh);
  ddsrt_fibheap_insert (&lease_fhdef_proxypp, lh, pwr->lease);
  minl_new = ddsrt_fibheap_min (&lease_fhdef_proxypp, lh);
  /* if inserted lease is new shortest lease */
  if (proxypp->owns_lease && minl_prev != minl_new)
  {
    nn_etime_t texp = add_duration_to_etime (now_et (), minl_new->tdur);
    struct lease *lnew = lease_new (texp, minl_new->tdur, minl_new->entity);
    if (minl_prev == NULL)
    {
      assert (manbypp);
      assert (ddsrt_atomic_ldvoidp (&proxypp->minl_man) == NULL);
      ddsrt_atomic_stvoidp (&proxypp->minl_man, lnew);
    }
    else
    {
      proxy_participant_replace_minl (proxypp, manbypp, lnew);
    }
    lease_register (lnew);
  }
}

static void proxy_participant_remove_pwr_lease_locked (struct proxy_participant * proxypp, struct proxy_writer * pwr)
{
  struct lease *minl;
  bool manbypp;
  ddsrt_fibheap_t *lh;

  assert (pwr->lease != NULL);
  manbypp = (pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
  lh = manbypp ? &proxypp->leaseheap_man : &proxypp->leaseheap_auto;
  minl = ddsrt_fibheap_min (&lease_fhdef_proxypp, lh);
  ddsrt_fibheap_delete (&lease_fhdef_proxypp, lh, pwr->lease);
  /* if pwr with min lease is removed: update proxypp lease to use new minimal duration */
  if (proxypp->owns_lease && pwr->lease == minl)
  {
    if ((minl = ddsrt_fibheap_min (&lease_fhdef_proxypp, lh)) != NULL)
    {
      dds_duration_t trem = minl->tdur - pwr->lease->tdur;
      assert (trem >= 0);
      nn_etime_t texp = add_duration_to_etime (now_et(), trem);
      struct lease *lnew = lease_new (texp, minl->tdur, minl->entity);
      proxy_participant_replace_minl (proxypp, manbypp, lnew);
      lease_register (lnew);
    }
    else
    {
      proxy_participant_replace_minl (proxypp, manbypp, NULL);
    }
  }
}

void new_proxy_participant
(
  struct q_globals *gv,
  const struct ddsi_guid *ppguid,
  unsigned bes,
  unsigned prismtech_bes,
  const struct ddsi_guid *privileged_pp_guid,
  struct addrset *as_default,
  struct addrset *as_meta,
  const nn_plist_t *plist,
  dds_duration_t tlease_dur,
  nn_vendorid_t vendor,
  unsigned custom_flags,
  nn_wctime_t timestamp,
  seqno_t seq
)
{
  /* No locking => iff all participants use unique guids, and sedp
     runs on a single thread, it can't go wrong. FIXME, maybe? The
     same holds for the other functions for creating entities. */
  struct proxy_participant *proxypp;

  assert (ppguid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (ephash_lookup_proxy_participant_guid (gv->guid_hash, ppguid) == NULL);
  assert (privileged_pp_guid == NULL || privileged_pp_guid->entityid.u == NN_ENTITYID_PARTICIPANT);

  prune_deleted_participant_guids (gv->deleted_participants, now_mt ());

  proxypp = ddsrt_malloc (sizeof (*proxypp));

  entity_common_init (&proxypp->e, gv, ppguid, "", EK_PROXY_PARTICIPANT, timestamp, vendor, false);
  proxypp->refc = 1;
  proxypp->lease_expired = 0;
  proxypp->deleting = 0;
  proxypp->vendor = vendor;
  proxypp->bes = bes;
  proxypp->prismtech_bes = prismtech_bes;
  proxypp->seq = seq;
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
    privpp = ephash_lookup_proxy_participant_guid (gv->guid_hash, &proxypp->privileged_pp_guid);

    ddsrt_fibheap_init (&lease_fhdef_proxypp, &proxypp->leaseheap_auto);
    ddsrt_fibheap_init (&lease_fhdef_proxypp, &proxypp->leaseheap_man);
    ddsrt_atomic_stvoidp (&proxypp->minl_man, NULL);

    if (privpp != NULL && privpp->is_ddsi2_pp)
    {
      proxypp->lease = privpp->lease;
      proxypp->owns_lease = 0;
      ddsrt_atomic_stvoidp (&proxypp->minl_auto, NULL);
    }
    else
    {
      /* Lease duration is meaningless when the lease never expires, but when proxy participants are
        created implicitly because of endpoint discovery from a cloud service, we do want the lease to expire
        eventually when the cloud discovery service disappears and never reappears. The normal data path renews
        the lease, so if the lease expiry is changed after the DS disappears but data continues to flow (even if
        it is only a single sample) the proxy participant would immediately go back to a non-expiring lease with
        no further triggers for deleting it. Instead, we take tlease_dur == NEVER as a special value meaning a
        lease that doesn't expire now and that has a "reasonable" lease duration. That way the lease renewal in
        the data path is fine, and we only need to do something special in SEDP handling. */
      nn_etime_t texp = add_duration_to_etime (now_et(), tlease_dur);
      dds_duration_t dur = (tlease_dur == T_NEVER) ? gv->config.lease_duration : tlease_dur;
      proxypp->lease = lease_new (texp, dur, &proxypp->e);
      proxypp->owns_lease = 1;

      /* Add the proxypp lease to heap so that monitoring liveliness will include this lease
         and uses the shortest duration for proxypp and all its pwr's (with automatic liveliness) */
      ddsrt_fibheap_insert (&lease_fhdef_proxypp, &proxypp->leaseheap_auto, proxypp->lease);

      /* Set the shortest lease for auto liveliness: clone proxypp's lease and store the clone in
         proxypp->minl_auto. As there are no pwr's at this point, the proxy pp's lease is the
         shortest lease. When a pwr with a shorter is added, the lease in minl_auto is replaced
         by the lease from the proxy writer in proxy_participant_add_pwr_lease_locked. This old shortest
         lease is freed, so that's why we need a clone and not the proxypp's lease in the heap.  */
      ddsrt_atomic_stvoidp (&proxypp->minl_auto, (void *) lease_clone (proxypp->lease));
    }
  }

  proxypp->as_default = as_default;
  proxypp->as_meta = as_meta;
  proxypp->endpoints = NULL;
  proxypp->plist = nn_plist_dup (plist);
  nn_xqos_mergein_missing (&proxypp->plist->qos, &gv->default_plist_pp.qos, ~(uint64_t)0);
  ddsrt_avl_init (&proxypp_groups_treedef, &proxypp->groups);

  set_proxy_participant_security_info(proxypp, plist);

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
  ephash_insert_proxy_participant_guid (gv->guid_hash, proxypp);

  /* TODO: Do security checks on the proxy participant. Either add the endpoints or delete the proxy. */
  add_proxy_builtin_endpoints(gv, ppguid, proxypp, timestamp);
}

int update_proxy_participant_plist_locked (struct proxy_participant *proxypp, seqno_t seq, const struct nn_plist *datap, enum update_proxy_participant_source source, nn_wctime_t timestamp)
{
  nn_plist_t *new_plist = ddsrt_malloc (sizeof (*new_plist));
  nn_plist_init_empty (new_plist);
  nn_plist_mergein_missing (new_plist, datap, PP_PRISMTECH_NODE_NAME | PP_PRISMTECH_EXEC_NAME | PP_PRISMTECH_PROCESS_ID | PP_ENTITY_NAME, QP_USER_DATA);
  nn_plist_mergein_missing (new_plist, &proxypp->e.gv->default_plist_pp, ~(uint64_t)0, ~(uint64_t)0);

  if (seq > proxypp->seq)
    proxypp->seq = seq;

  switch (source)
  {
    case UPD_PROXYPP_SPDP:
      (void) update_qos_locked (&proxypp->e, &proxypp->plist->qos, &new_plist->qos, timestamp);
      nn_plist_fini (new_plist);
      ddsrt_free (new_plist);
      proxypp->proxypp_have_spdp = 1;
      break;

    case UPD_PROXYPP_CM:
      nn_plist_fini (proxypp->plist);
      ddsrt_free (proxypp->plist);
      proxypp->plist = new_plist;
      proxypp->proxypp_have_cm = 1;
      break;
  }
  return 0;
}

int update_proxy_participant_plist (struct proxy_participant *proxypp, seqno_t seq, const struct nn_plist *datap, enum update_proxy_participant_source source, nn_wctime_t timestamp)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  update_proxy_participant_plist_locked (proxypp, seq, datap, source, timestamp);
  ddsrt_mutex_unlock (&proxypp->e.lock);
  return 0;
}

static int ref_proxy_participant (struct proxy_participant *proxypp, struct proxy_endpoint_common *c)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  if (proxypp->deleting)
  {
    ddsrt_mutex_unlock (&proxypp->e.lock);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }
  c->proxypp = proxypp;
  proxypp->refc++;

  c->next_ep = proxypp->endpoints;
  c->prev_ep = NULL;
  if (c->next_ep)
  {
    c->next_ep->prev_ep = c;
  }
  proxypp->endpoints = c;
  ddsrt_mutex_unlock (&proxypp->e.lock);

  return DDS_RETCODE_OK;
}

static void unref_proxy_participant (struct proxy_participant *proxypp, struct proxy_endpoint_common *c)
{
  uint32_t refc;
  const nn_wctime_t tnow = now();

  ddsrt_mutex_lock (&proxypp->e.lock);
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
    if (proxypp->owns_lease)
    {
      struct lease * minl_auto = ddsrt_atomic_ldvoidp (&proxypp->minl_auto);
      ddsrt_fibheap_delete (&lease_fhdef_proxypp, &proxypp->leaseheap_auto, proxypp->lease);
      assert (ddsrt_fibheap_min (&lease_fhdef_proxypp, &proxypp->leaseheap_auto) == NULL);
      assert (ddsrt_fibheap_min (&lease_fhdef_proxypp, &proxypp->leaseheap_man) == NULL);
      assert (ddsrt_atomic_ldvoidp (&proxypp->minl_man) == NULL);
      assert (!compare_guid (&minl_auto->entity->guid, &proxypp->e.guid));
      lease_unregister (minl_auto);
      lease_free (minl_auto);
      lease_free (proxypp->lease);
    }
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "unref_proxy_participant("PGUIDFMT"): refc=0, freeing\n", PGUID (proxypp->e.guid));
    unref_addrset (proxypp->as_default);
    unref_addrset (proxypp->as_meta);
    nn_plist_fini (proxypp->plist);
    ddsrt_free (proxypp->plist);
    entity_common_fini (&proxypp->e);
    remove_deleted_participant_guid (proxypp->e.gv->deleted_participants, &proxypp->e.guid, DPG_LOCAL | DPG_REMOTE);
    ddsrt_free (proxypp);
  }
  else if (proxypp->endpoints == NULL && proxypp->implicitly_created)
  {
    assert (refc == 1);
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "unref_proxy_participant("PGUIDFMT"): refc=%u, no endpoints, implicitly created, deleting\n",
              PGUID (proxypp->e.guid), (unsigned) refc);
    delete_proxy_participant_by_guid(proxypp->e.gv, &proxypp->e.guid, tnow, 1);
    /* Deletion is still (and has to be) asynchronous. A parallel endpoint creation may or may not
       succeed, and if it succeeds it will be deleted along with the proxy participant. So "your
       mileage may vary". Also, the proxy participant may be blacklisted for a little ... */
  }
  else
  {
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "unref_proxy_participant("PGUIDFMT"): refc=%u\n", PGUID (proxypp->e.guid), (unsigned) refc);
  }
}

static void gc_delete_proxy_participant (struct gcreq *gcreq)
{
  struct proxy_participant *proxypp = gcreq->arg;
  ELOGDISC (proxypp, "gc_delete_proxy_participant(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (proxypp->e.guid));
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
  ddsrt_mutex_lock (&p->e.lock);
  if (memcmp (&p->privileged_pp_guid, &proxypp->e.guid, sizeof (proxypp->e.guid)) != 0)
  {
    /* p not dependent on proxypp */
    ddsrt_mutex_unlock (&p->e.lock);
    return;
  }
  else if (!(vendor_is_cloud(p->vendor) && p->implicitly_created))
  {
    /* DDSI2 minimal participant mode -- but really, anything not discovered via Cloud gets deleted */
    ddsrt_mutex_unlock (&p->e.lock);
    (void) delete_proxy_participant_by_guid (p->e.gv, &p->e.guid, timestamp, isimplicit);
  }
  else
  {
    nn_etime_t texp = add_duration_to_etime (now_et(), p->e.gv->config.ds_grace_period);
    /* Clear dependency (but don't touch entity id, which must be 0x1c1) and set the lease ticking */
    ELOGDISC (p, PGUIDFMT" detach-from-DS "PGUIDFMT"\n", PGUID(p->e.guid), PGUID(proxypp->e.guid));
    memset (&p->privileged_pp_guid.prefix, 0, sizeof (p->privileged_pp_guid.prefix));
    lease_set_expiry (p->lease, texp);
    /* FIXME: replace in p->leaseheap_auto and get new minl_auto */
    ddsrt_mutex_unlock (&p->e.lock);
  }
}

static void delete_ppt (struct proxy_participant *proxypp, nn_wctime_t timestamp, int isimplicit)
{
  ddsi_entityid_t *eps;
  ddsi_guid_t ep_guid;
  uint32_t ep_count = 0;

  /* if any proxy participants depend on this participant, delete them */
  ELOGDISC (proxypp, "delete_ppt("PGUIDFMT") - deleting dependent proxy participants\n", PGUID (proxypp->e.guid));
  {
    struct ephash_enum_proxy_participant est;
    struct proxy_participant *p;
    ephash_enum_proxy_participant_init (&est, proxypp->e.gv->guid_hash);
    while ((p = ephash_enum_proxy_participant_next (&est)) != NULL)
      delete_or_detach_dependent_pp(p, proxypp, timestamp, isimplicit);
    ephash_enum_proxy_participant_fini (&est);
  }

  ddsrt_mutex_lock (&proxypp->e.lock);
  proxypp->deleting = 1;
  if (isimplicit)
    proxypp->lease_expired = 1;

  /* Get snapshot of endpoints list so that we can release proxypp->e.lock
     Pwrs/prds may be deleted during the iteration over the entities,
     but resolving the guid will fail for these entities and the our
     call to delete_proxy_writer/reader returns. */
  {
    eps = ddsrt_malloc (proxypp->refc * sizeof(ddsi_entityid_t));
    struct proxy_endpoint_common *cep = proxypp->endpoints;
    while (cep)
    {
      const struct entity_common *entc = entity_common_from_proxy_endpoint_common (cep);
      eps[ep_count++] = entc->guid.entityid;
      cep = cep->next_ep;
    }
  }
  ddsrt_mutex_unlock (&proxypp->e.lock);

  ELOGDISC (proxypp, "delete_ppt("PGUIDFMT") - deleting endpoints\n", PGUID (proxypp->e.guid));
  ep_guid.prefix = proxypp->e.guid.prefix;
  for (uint32_t n = 0; n < ep_count; n++)
  {
    ep_guid.entityid = eps[n];
    if (is_writer_entityid (ep_guid.entityid))
      delete_proxy_writer (proxypp->e.gv, &ep_guid, timestamp, isimplicit);
    else
      delete_proxy_reader (proxypp->e.gv, &ep_guid, timestamp, isimplicit);
  }
  ddsrt_free (eps);
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
    delete_proxy_participant_by_guid (data->proxypp->e.gv, &data->proxypp->e.guid, data->timestamp, 1);
}

void purge_proxy_participants (struct q_globals *gv, const nn_locator_t *loc, bool delete_from_as_disc)
{
  /* FIXME: check whether addr:port can't be reused for a new connection by the time we get here. */
  /* NOTE: This function exists for the sole purpose of cleaning up after closing a TCP connection in ddsi_tcp_close_conn and the state of the calling thread could be anything at this point. Because of that we do the unspeakable and toggle the thread state conditionally. We can't afford to have it in "asleep", as that causes a race with the garbage collector. */
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct ephash_enum_proxy_participant est;
  struct proxy_purge_data data;

  thread_state_awake_fixed_domain (ts1);
  data.loc = loc;
  data.timestamp = now();
  ephash_enum_proxy_participant_init (&est, gv->guid_hash);
  while ((data.proxypp = ephash_enum_proxy_participant_next (&est)) != NULL)
    addrset_forall (data.proxypp->as_meta, purge_helper, &data);
  ephash_enum_proxy_participant_fini (&est);

  /* Shouldn't try to keep pinging clients once they're gone */
  if (delete_from_as_disc)
    remove_from_addrset (gv, gv->as_disc, loc);

  thread_state_asleep (ts1);
}

int delete_proxy_participant_by_guid (struct q_globals *gv, const struct ddsi_guid *guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_participant *ppt;

  GVLOGDISC ("delete_proxy_participant_by_guid("PGUIDFMT") ", PGUID (*guid));
  ddsrt_mutex_lock (&gv->lock);
  ppt = ephash_lookup_proxy_participant_guid (gv->guid_hash, guid);
  if (ppt == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("- deleting\n");
  builtintopic_write (gv->builtin_topic_interface, &ppt->e, timestamp, false);
  remember_deleted_participant_guid (gv->deleted_participants, &ppt->e.guid);
  ephash_remove_proxy_participant_guid (gv->guid_hash, ppt);
  ddsrt_mutex_unlock (&gv->lock);
  delete_ppt (ppt, timestamp, isimplicit);

  return 0;
}

uint64_t get_entity_instance_id (const struct q_globals *gv, const struct ddsi_guid *guid)
{
  struct thread_state1 *ts1 = lookup_thread_state ();
  struct entity_common *e;
  uint64_t iid = 0;
  thread_state_awake (ts1, gv);
  if ((e = ephash_lookup_guid_untyped (gv->guid_hash, guid)) != NULL)
    iid = e->iid;
  thread_state_asleep (ts1);
  return iid;
}

/* PROXY-ENDPOINT --------------------------------------------------- */

static int proxy_endpoint_common_init (struct entity_common *e, struct proxy_endpoint_common *c, enum entity_kind kind, const struct ddsi_guid *guid, nn_wctime_t tcreate, seqno_t seq, struct proxy_participant *proxypp, struct addrset *as, const nn_plist_t *plist)
{
  const char *name;
  int ret;

  if (is_builtin_entityid (guid->entityid, proxypp->vendor))
    assert ((plist->qos.present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == 0);
  else
    assert ((plist->qos.present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == (QP_TOPIC_NAME | QP_TYPE_NAME));

  name = (plist->present & PP_ENTITY_NAME) ? plist->entity_name : "";
  entity_common_init (e, proxypp->e.gv, guid, name, kind, tcreate, proxypp->vendor, false);
  c->xqos = nn_xqos_dup (&plist->qos);
  c->as = ref_addrset (as);
  c->vendor = proxypp->vendor;
  c->seq = seq;

  if (plist->present & PP_GROUP_GUID)
    c->group_guid = plist->group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));

#ifdef DDSI_INCLUDE_SECURITY
  c->security_info.security_attributes = 0;
  c->security_info.plugin_security_attributes = 0;
#endif

  if ((ret = ref_proxy_participant (proxypp, c)) != DDS_RETCODE_OK)
  {
    nn_xqos_fini (c->xqos);
    ddsrt_free (c->xqos);
    unref_addrset (c->as);
    entity_common_fini (e);
    return ret;
  }

  return DDS_RETCODE_OK;
}

static void proxy_endpoint_common_fini (struct entity_common *e, struct proxy_endpoint_common *c)
{
  unref_proxy_participant (c->proxypp, c);
  nn_xqos_fini (c->xqos);
  ddsrt_free (c->xqos);
  unref_addrset (c->as);
  entity_common_fini (e);
}

/* PROXY-WRITER ----------------------------------------------------- */

static enum nn_reorder_mode
get_proxy_writer_reorder_mode(const ddsi_entityid_t pwr_entityid, int isreliable)
{
  if (isreliable)
  {
    return NN_REORDER_MODE_NORMAL;
  }
  if (pwr_entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER)
  {
    return NN_REORDER_MODE_ALWAYS_DELIVER;
  }
  return NN_REORDER_MODE_MONOTONICALLY_INCREASING;
}

int new_proxy_writer (struct q_globals *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const nn_plist_t *plist, struct nn_dqueue *dqueue, struct xeventq *evq, nn_wctime_t timestamp, seqno_t seq)
{
  struct proxy_participant *proxypp;
  struct proxy_writer *pwr;
  int isreliable;
  nn_mtime_t tnow = now_mt ();
  enum nn_reorder_mode reorder_mode;
  int ret;

  assert (is_writer_entityid (guid->entityid));
  assert (ephash_lookup_proxy_writer_guid (gv->guid_hash, guid) == NULL);

  if ((proxypp = ephash_lookup_proxy_participant_guid (gv->guid_hash, ppguid)) == NULL)
  {
    GVWARNING ("new_proxy_writer("PGUIDFMT"): proxy participant unknown\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  pwr = ddsrt_malloc (sizeof (*pwr));
  if ((ret = proxy_endpoint_common_init (&pwr->e, &pwr->c, EK_PROXY_WRITER, guid, timestamp, seq, proxypp, as, plist)) != DDS_RETCODE_OK)
  {
    ddsrt_free (pwr);
    return ret;
  }

  ddsrt_avl_init (&pwr_readers_treedef, &pwr->readers);
  pwr->n_reliable_readers = 0;
  pwr->n_readers_out_of_sync = 0;
  pwr->last_seq = 0;
  pwr->last_fragnum = ~0u;
  pwr->nackfragcount = 0;
  pwr->last_fragnum_reset = 0;
  pwr->alive = 1;
  pwr->alive_vclock = 0;
  pwr->filtered = 0;
  ddsrt_atomic_st32 (&pwr->next_deliv_seq_lowword, 1);
  if (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor)) {
    /* The DDSI built-in proxy writers always deliver
       asynchronously */
    pwr->deliver_synchronously = 0;
  } else if (pwr->c.xqos->latency_budget.duration <= gv->config.synchronous_delivery_latency_bound &&
             pwr->c.xqos->transport_priority.value >= gv->config.synchronous_delivery_priority_threshold) {
    /* Regular proxy-writers with a sufficiently low latency_budget
       and a sufficiently high transport_priority deliver
       synchronously */
    pwr->deliver_synchronously = 1;
  } else {
    pwr->deliver_synchronously = 0;
  }
  /* Pretend we have seen a heartbeat if the proxy writer is a best-effort one */
  isreliable = (pwr->c.xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  pwr->have_seen_heartbeat = !isreliable;
  pwr->local_matching_inprogress = 1;
#ifdef DDSI_INCLUDE_SSM
  pwr->supports_ssm = (addrset_contains_ssm (gv, as) && gv->config.allowMulticast & AMC_SSM) ? 1 : 0;
#endif

  assert (pwr->c.xqos->present & QP_LIVELINESS);
  if (pwr->c.xqos->liveliness.lease_duration != T_NEVER)
  {
    nn_etime_t texpire = add_duration_to_etime (now_et (), pwr->c.xqos->liveliness.lease_duration);
    pwr->lease = lease_new (texpire, pwr->c.xqos->liveliness.lease_duration, &pwr->e);
    if (pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
    {
      ddsrt_mutex_lock (&proxypp->e.lock);
      proxy_participant_add_pwr_lease_locked (proxypp, pwr);
      ddsrt_mutex_unlock (&proxypp->e.lock);
    }
    else
    {
      lease_register (pwr->lease);
    }
  }
  else
  {
    pwr->lease = NULL;
  }

  if (isreliable)
  {
    pwr->defrag = nn_defrag_new (&gv->logconfig, NN_DEFRAG_DROP_LATEST, gv->config.defrag_reliable_maxsamples);
  }
  else
  {
    pwr->defrag = nn_defrag_new (&gv->logconfig, NN_DEFRAG_DROP_OLDEST, gv->config.defrag_unreliable_maxsamples);
  }
  reorder_mode = get_proxy_writer_reorder_mode(pwr->e.guid.entityid, isreliable);
  pwr->reorder = nn_reorder_new (&gv->logconfig, reorder_mode, gv->config.primary_reorder_maxsamples, gv->config.late_ack_mode);

  if (pwr->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)
  {
    /* for the builtin_volatile_secure proxy writer which uses a content filter set the next expected
     * sequence number of the reorder administration to the maximum sequence number to ensure that effectively
     * the reorder administration of the builtin_volatile_secure proxy writer is not used and because the corresponding
     * reader is always considered out of sync the reorder administration of the corresponding reader will be used
     * instead.
     */
    nn_reorder_set_next_seq(pwr->reorder, MAX_SEQ_NUMBER);
    pwr->filtered = 1;
  }

  pwr->dqueue = dqueue;
  pwr->evq = evq;
  pwr->ddsi2direct_cb = 0;
  pwr->ddsi2direct_cbarg = 0;

  set_proxy_writer_security_info(pwr, plist);

  local_reader_ary_init (&pwr->rdary);

  /* locking the entity prevents matching while the built-in topic hasn't been published yet */
  ddsrt_mutex_lock (&pwr->e.lock);
  ephash_insert_proxy_writer_guid (gv->guid_hash, pwr);
  builtintopic_write (gv->builtin_topic_interface, &pwr->e, timestamp, true);
  ddsrt_mutex_unlock (&pwr->e.lock);

  match_proxy_writer_with_readers (pwr, tnow);

  ddsrt_mutex_lock (&pwr->e.lock);
  pwr->local_matching_inprogress = 0;
  ddsrt_mutex_unlock (&pwr->e.lock);

  return 0;
}

void update_proxy_writer (struct proxy_writer *pwr, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, nn_wctime_t timestamp)
{
  struct reader * rd;
  struct pwr_rd_match * m;
  ddsrt_avl_iter_t iter;

  /* Update proxy writer endpoints (from SEDP alive) */

  ddsrt_mutex_lock (&pwr->e.lock);
  if (seq > pwr->c.seq)
  {
    pwr->c.seq = seq;
    if (! addrset_eq_onesidederr (pwr->c.as, as))
    {
#ifdef DDSI_INCLUDE_SSM
      pwr->supports_ssm = (addrset_contains_ssm (pwr->e.gv, as) && pwr->e.gv->config.allowMulticast & AMC_SSM) ? 1 : 0;
#endif
      unref_addrset (pwr->c.as);
      ref_addrset (as);
      pwr->c.as = as;
      m = ddsrt_avl_iter_first (&pwr_readers_treedef, &pwr->readers, &iter);
      while (m)
      {
        rd = ephash_lookup_reader_guid (pwr->e.gv->guid_hash, &m->rd_guid);
        if (rd)
        {
          qxev_pwr_entityid (pwr, &rd->e.guid);
        }
        m = ddsrt_avl_iter_next (&iter);
      }
    }

    (void) update_qos_locked (&pwr->e, pwr->c.xqos, xqos, timestamp);
  }
  ddsrt_mutex_unlock (&pwr->e.lock);
}

void update_proxy_reader (struct proxy_reader *prd, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, nn_wctime_t timestamp)
{
  struct prd_wr_match * m;
  ddsi_guid_t wrguid;

  memset (&wrguid, 0, sizeof (wrguid));

  ddsrt_mutex_lock (&prd->e.lock);
  if (seq > prd->c.seq)
  {
    prd->c.seq = seq;
    if (! addrset_eq_onesidederr (prd->c.as, as))
    {
      /* Update proxy reader endpoints (from SEDP alive) */

      unref_addrset (prd->c.as);
      ref_addrset (as);
      prd->c.as = as;

      /* Rebuild writer endpoints */

      while ((m = ddsrt_avl_lookup_succ_eq (&prd_writers_treedef, &prd->writers, &wrguid)) != NULL)
      {
        struct prd_wr_match *next;
        ddsi_guid_t guid_next;
        struct writer * wr;

        wrguid = m->wr_guid;
        next = ddsrt_avl_find_succ (&prd_writers_treedef, &prd->writers, m);
        if (next)
        {
          guid_next = next->wr_guid;
        }
        else
        {
          memset (&guid_next, 0xff, sizeof (guid_next));
          guid_next.entityid.u = (guid_next.entityid.u & ~(unsigned)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
        }

        ddsrt_mutex_unlock (&prd->e.lock);
        wr = ephash_lookup_writer_guid (prd->e.gv->guid_hash, &wrguid);
        if (wr)
        {
          ddsrt_mutex_lock (&wr->e.lock);
          rebuild_writer_addrset (wr);
          ddsrt_mutex_unlock (&wr->e.lock);
          qxev_prd_entityid (prd, &wr->e.guid);
        }
        wrguid = guid_next;
        ddsrt_mutex_lock (&prd->e.lock);
      }
    }

    (void) update_qos_locked (&prd->e, prd->c.xqos, xqos, timestamp);
  }
  ddsrt_mutex_unlock (&prd->e.lock);
}

static void gc_delete_proxy_writer (struct gcreq *gcreq)
{
  struct proxy_writer *pwr = gcreq->arg;
  ELOGDISC (pwr, "gc_delete_proxy_writer(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pwr->e.guid));
  gcreq_free (gcreq);
  while (!ddsrt_avl_is_empty (&pwr->readers))
  {
    struct pwr_rd_match *m = ddsrt_avl_root_non_empty (&pwr_readers_treedef, &pwr->readers);
    ddsrt_avl_delete (&pwr_readers_treedef, &pwr->readers, m);
    reader_drop_connection (&m->rd_guid, pwr);
    update_reader_init_acknack_count (&pwr->e.gv->logconfig, pwr->e.gv->guid_hash, &m->rd_guid, m->count);
    free_pwr_rd_match (m);
  }
  local_reader_ary_fini (&pwr->rdary);
  if (pwr->c.xqos->liveliness.lease_duration != T_NEVER)
    lease_free (pwr->lease);
  proxy_endpoint_common_fini (&pwr->e, &pwr->c);
  nn_defrag_free (pwr->defrag);
  nn_reorder_free (pwr->reorder);
  ddsrt_free (pwr);
}

/* First stage in deleting the proxy writer. In this function the pwr and its member pointers
   will remain valid. The real cleaning-up is done async in gc_delete_proxy_writer. */
int delete_proxy_writer (struct q_globals *gv, const struct ddsi_guid *guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_writer *pwr;
  DDSRT_UNUSED_ARG (isimplicit);
  GVLOGDISC ("delete_proxy_writer ("PGUIDFMT") ", PGUID (*guid));

  ddsrt_mutex_lock (&gv->lock);
  if ((pwr = ephash_lookup_proxy_writer_guid (gv->guid_hash, guid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }

  /* Set "deleting" flag in particular for Lite, to signal to the receive path it can't
     trust rdary[] anymore, which is because removing the proxy writer from the hash
     table will prevent the readers from looking up the proxy writer, and consequently
     from removing themselves from the proxy writer's rdary[]. */
  local_reader_ary_setinvalid (&pwr->rdary);
  GVLOGDISC ("- deleting\n");
  builtintopic_write (gv->builtin_topic_interface, &pwr->e, timestamp, false);
  ephash_remove_proxy_writer_guid (gv->guid_hash, pwr);
  ddsrt_mutex_unlock (&gv->lock);
  if (pwr->c.xqos->liveliness.lease_duration != T_NEVER &&
      pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
    lease_unregister (pwr->lease);
  if (proxy_writer_set_notalive (pwr, false) != DDS_RETCODE_OK)
    GVLOGDISC ("proxy_writer_set_notalive failed for "PGUIDFMT"\n", PGUID(*guid));
  gcreq_proxy_writer (pwr);
  return DDS_RETCODE_OK;
}

static void proxy_writer_notify_liveliness_change_may_unlock (struct proxy_writer *pwr)
{
  struct proxy_writer_alive_state alive_state;
  proxy_writer_get_alive_state_locked (pwr, &alive_state);

  struct ddsi_guid rdguid;
  struct pwr_rd_match *m;
  memset (&rdguid, 0, sizeof (rdguid));
  while (pwr->alive_vclock == alive_state.vclock &&
         (m = ddsrt_avl_lookup_succ (&pwr_readers_treedef, &pwr->readers, &rdguid)) != NULL)
  {
    rdguid = m->rd_guid;
    ddsrt_mutex_unlock (&pwr->e.lock);
    /* unlocking pwr means alive state may have changed already; we break out of the loop once we
       detect this but there for the reader in the current iteration, anything is possible */
    reader_update_notify_pwr_alive_state_guid (&rdguid, pwr, &alive_state);
    ddsrt_mutex_lock (&pwr->e.lock);
  }
}

void proxy_writer_set_alive_may_unlock (struct proxy_writer *pwr, bool notify)
{
  /* Caller has pwr->e.lock, so we can safely read pwr->alive.  Updating pwr->alive requires
     also taking pwr->c.proxypp->e.lock because pwr->alive <=> (pwr->lease in proxypp's lease
     heap). */
  assert (!pwr->alive);

  /* check that proxy writer still exists (when deleting it is removed from guid hash) */
  if (ephash_lookup_proxy_writer_guid (pwr->e.gv->guid_hash, &pwr->e.guid) == NULL)
  {
    ELOGDISC (pwr, "proxy_writer_set_alive_may_unlock("PGUIDFMT") - not in guid_hash, pwr deleting\n", PGUID (pwr->e.guid));
    return;
  }

  ddsrt_mutex_lock (&pwr->c.proxypp->e.lock);
  pwr->alive = true;
  pwr->alive_vclock++;
  if (pwr->c.xqos->liveliness.lease_duration != T_NEVER && pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
    proxy_participant_add_pwr_lease_locked (pwr->c.proxypp, pwr);
  ddsrt_mutex_unlock (&pwr->c.proxypp->e.lock);

  if (notify)
    proxy_writer_notify_liveliness_change_may_unlock (pwr);
}

int proxy_writer_set_notalive (struct proxy_writer *pwr, bool notify)
{
  /* Caller should not have taken pwr->e.lock and pwr->c.proxypp->e.lock;
   * this function takes both locks to update pwr->alive value */
  ddsrt_mutex_lock (&pwr->e.lock);
  if (!pwr->alive)
  {
    ddsrt_mutex_unlock (&pwr->e.lock);
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  }

  ddsrt_mutex_lock (&pwr->c.proxypp->e.lock);
  pwr->alive = false;
  pwr->alive_vclock++;
  if (pwr->c.xqos->liveliness.lease_duration != T_NEVER && pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
    proxy_participant_remove_pwr_lease_locked (pwr->c.proxypp, pwr);
  ddsrt_mutex_unlock (&pwr->c.proxypp->e.lock);

  if (notify)
    proxy_writer_notify_liveliness_change_may_unlock (pwr);
  ddsrt_mutex_unlock (&pwr->e.lock);
  return DDS_RETCODE_OK;
}

void proxy_writer_set_notalive_guid (struct q_globals *gv, const struct ddsi_guid *pwrguid, bool notify)
{
  struct proxy_writer *pwr;
  if ((pwr = ephash_lookup_proxy_writer_guid (gv->guid_hash, pwrguid)) == NULL)
    GVLOGDISC (" "PGUIDFMT"?\n", PGUID (*pwrguid));
  else
  {
    GVLOGDISC ("proxy_writer_set_notalive_guid ("PGUIDFMT")", PGUID (*pwrguid));
    if (proxy_writer_set_notalive (pwr, notify) == DDS_RETCODE_PRECONDITION_NOT_MET)
      GVLOGDISC (" pwr was not alive");
    GVLOGDISC ("\n");
  }
}

/* PROXY-READER ----------------------------------------------------- */

int new_proxy_reader (struct q_globals *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const nn_plist_t *plist, nn_wctime_t timestamp, seqno_t seq
#ifdef DDSI_INCLUDE_SSM
                      , int favours_ssm
#endif
                      )
{
  struct proxy_participant *proxypp;
  struct proxy_reader *prd;
  nn_mtime_t tnow = now_mt ();
  int ret;

  assert (!is_writer_entityid (guid->entityid));
  assert (ephash_lookup_proxy_reader_guid (gv->guid_hash, guid) == NULL);

  if ((proxypp = ephash_lookup_proxy_participant_guid (gv->guid_hash, ppguid)) == NULL)
  {
    GVWARNING ("new_proxy_reader("PGUIDFMT"): proxy participant unknown\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  prd = ddsrt_malloc (sizeof (*prd));
  if ((ret = proxy_endpoint_common_init (&prd->e, &prd->c, EK_PROXY_READER, guid, timestamp, seq, proxypp, as, plist)) != DDS_RETCODE_OK)
  {
    ddsrt_free (prd);
    return ret;
  }

  prd->deleting = 0;
#ifdef DDSI_INCLUDE_SSM
  prd->favours_ssm = (favours_ssm && gv->config.allowMulticast & AMC_SSM) ? 1 : 0;
#endif
  prd->is_fict_trans_reader = 0;

  set_proxy_reader_security_info(prd, plist);

  ddsrt_avl_init (&prd_writers_treedef, &prd->writers);

#ifdef DDSI_INCLUDE_SECURITY
  if (prd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    prd->filter = volatile_secure_data_filter;
  else
    prd->filter = NULL;
#else
  prd->filter = NULL;
#endif

  /* locking the entity prevents matching while the built-in topic hasn't been published yet */
  ddsrt_mutex_lock (&prd->e.lock);
  ephash_insert_proxy_reader_guid (gv->guid_hash, prd);
  builtintopic_write (gv->builtin_topic_interface, &prd->e, timestamp, true);
  ddsrt_mutex_unlock (&prd->e.lock);

  match_proxy_reader_with_writers (prd, tnow);
  return DDS_RETCODE_OK;
}

static void proxy_reader_set_delete_and_ack_all_messages (struct proxy_reader *prd)
{
  ddsi_guid_t wrguid;
  struct writer *wr;
  struct prd_wr_match *m;

  memset (&wrguid, 0, sizeof (wrguid));
  ddsrt_mutex_lock (&prd->e.lock);
  prd->deleting = 1;
  while ((m = ddsrt_avl_lookup_succ_eq (&prd_writers_treedef, &prd->writers, &wrguid)) != NULL)
  {
    /* have to be careful walking the tree -- pretty is different, but
       I want to check this before I write a lookup_succ function. */
    struct prd_wr_match *m_a_next;
    ddsi_guid_t wrguid_next;
    wrguid = m->wr_guid;
    if ((m_a_next = ddsrt_avl_find_succ (&prd_writers_treedef, &prd->writers, m)) != NULL)
      wrguid_next = m_a_next->wr_guid;
    else
    {
      memset (&wrguid_next, 0xff, sizeof (wrguid_next));
      wrguid_next.entityid.u = (wrguid_next.entityid.u & ~(unsigned)0xff) | NN_ENTITYID_KIND_WRITER_NO_KEY;
    }

    ddsrt_mutex_unlock (&prd->e.lock);
    if ((wr = ephash_lookup_writer_guid (prd->e.gv->guid_hash, &wrguid)) != NULL)
    {
      struct whc_node *deferred_free_list = NULL;
      struct wr_prd_match *m_wr;
      ddsrt_mutex_lock (&wr->e.lock);
      if ((m_wr = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
      {
        struct whc_state whcst;
        m_wr->seq = MAX_SEQ_NUMBER;
        ddsrt_avl_augment_update (&wr_readers_treedef, m_wr);
        (void)remove_acked_messages (wr, &whcst, &deferred_free_list);
        writer_clear_retransmitting (wr);
      }
      ddsrt_mutex_unlock (&wr->e.lock);
      whc_free_deferred_free_list (wr->whc, deferred_free_list);
    }

    wrguid = wrguid_next;
    ddsrt_mutex_lock (&prd->e.lock);
  }
  ddsrt_mutex_unlock (&prd->e.lock);
}

static void gc_delete_proxy_reader (struct gcreq *gcreq)
{
  struct proxy_reader *prd = gcreq->arg;
  ELOGDISC (prd, "gc_delete_proxy_reader(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (prd->e.guid));
  gcreq_free (gcreq);

  while (!ddsrt_avl_is_empty (&prd->writers))
  {
    struct prd_wr_match *m = ddsrt_avl_root_non_empty (&prd_writers_treedef, &prd->writers);
    ddsrt_avl_delete (&prd_writers_treedef, &prd->writers, m);
    writer_drop_connection (&m->wr_guid, prd);
    free_prd_wr_match (m);
  }

  proxy_endpoint_common_fini (&prd->e, &prd->c);
  ddsrt_free (prd);
}

int delete_proxy_reader (struct q_globals *gv, const struct ddsi_guid *guid, nn_wctime_t timestamp, int isimplicit)
{
  struct proxy_reader *prd;
  (void)isimplicit;
  GVLOGDISC ("delete_proxy_reader ("PGUIDFMT") ", PGUID (*guid));
  ddsrt_mutex_lock (&gv->lock);
  if ((prd = ephash_lookup_proxy_reader_guid (gv->guid_hash, guid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  builtintopic_write (gv->builtin_topic_interface, &prd->e, timestamp, false);
  ephash_remove_proxy_reader_guid (gv->guid_hash, prd);
  ddsrt_mutex_unlock (&gv->lock);
  GVLOGDISC ("- deleting\n");

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
  struct gcreq *gcreq = gcreq_new (pp->e.gv->gcreq_queue, gc_delete_participant);
  gcreq->arg = pp;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_writer (struct writer *wr)
{
  struct gcreq *gcreq = gcreq_new (wr->e.gv->gcreq_queue, wr->throttling ? gc_delete_writer_throttlewait : gc_delete_writer);
  gcreq->arg = wr;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_reader (struct reader *rd)
{
  struct gcreq *gcreq = gcreq_new (rd->e.gv->gcreq_queue, gc_delete_reader);
  gcreq->arg = rd;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_proxy_participant (struct proxy_participant *proxypp)
{
  struct gcreq *gcreq = gcreq_new (proxypp->e.gv->gcreq_queue, gc_delete_proxy_participant);
  gcreq->arg = proxypp;
  gcreq_enqueue (gcreq);
  return 0;
}

static void gc_delete_proxy_writer_dqueue_bubble_cb (struct gcreq *gcreq)
{
  /* delete proxy_writer, phase 3 */
  struct proxy_writer *pwr = gcreq->arg;
  ELOGDISC (pwr, "gc_delete_proxy_writer_dqueue_bubble(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pwr->e.guid));
  gcreq_requeue (gcreq, gc_delete_proxy_writer);
}

static void gc_delete_proxy_writer_dqueue (struct gcreq *gcreq)
{
  /* delete proxy_writer, phase 2 */
  struct proxy_writer *pwr = gcreq->arg;
  struct nn_dqueue *dqueue = pwr->dqueue;
  ELOGDISC (pwr, "gc_delete_proxy_writer_dqueue(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (pwr->e.guid));
  nn_dqueue_enqueue_callback (dqueue, (void (*) (void *)) gc_delete_proxy_writer_dqueue_bubble_cb, gcreq);
}

static int gcreq_proxy_writer (struct proxy_writer *pwr)
{
  struct gcreq *gcreq = gcreq_new (pwr->e.gv->gcreq_queue, gc_delete_proxy_writer_dqueue);
  gcreq->arg = pwr;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_proxy_reader (struct proxy_reader *prd)
{
  struct gcreq *gcreq = gcreq_new (prd->e.gv->gcreq_queue, gc_delete_proxy_reader);
  gcreq->arg = prd;
  gcreq_enqueue (gcreq);
  return 0;
}
