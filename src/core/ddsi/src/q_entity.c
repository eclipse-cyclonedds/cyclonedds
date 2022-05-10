/*
 * Copyright(c) 2022 ZettaScale Technology
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
#include "dds/ddsrt/md5.h"

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_qosmatch.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_domaingv.h"
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
#include "dds/ddsi/ddsi_wraddrset.h"

#include "dds/ddsi/sysdeps.h"
#include "dds__whc.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_typelookup.h"
#include "dds/ddsi/ddsi_list_tmpl.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_typelib.h"

#ifdef DDS_HAS_SECURITY
#include "dds/ddsi/ddsi_security_msg.h"
#endif

struct deleted_participant {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t guid;
  unsigned for_what;
  ddsrt_mtime_t t_prune;
};

struct deleted_participants_admin {
  ddsrt_mutex_t deleted_participants_lock;
  ddsrt_avl_tree_t deleted_participants;
  const ddsrt_log_cfg_t *logcfg;
  int64_t delay;
};

struct alive_state {
  bool alive;
  uint32_t vclock;
};

struct gc_proxy_tp {
  struct proxy_participant *proxypp;
  struct proxy_topic *proxytp;
  ddsrt_wctime_t timestamp;
};

struct gc_tpd {
  struct ddsi_topic_definition *tpd;
  ddsrt_wctime_t timestamp;
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
  NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER
#ifdef DDS_HAS_TYPE_DISCOVERY
  | NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_WRITER
  | NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_WRITER
#endif
;

static dds_return_t new_writer_guid (struct writer **wr_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct whc *whc, status_cb_t status_cb, void *status_cbarg);
static dds_return_t new_reader_guid (struct reader **rd_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_rhc *rhc, status_cb_t status_cb, void *status_cbarg);
static struct participant *ref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity);
static void unref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity);
static struct entity_common *entity_common_from_proxy_endpoint_common (const struct proxy_endpoint_common *c);

#ifdef DDS_HAS_SECURITY
static void handshake_end_cb(struct ddsi_handshake *handshake, struct participant *pp, struct proxy_participant *proxypp, enum ddsi_handshake_state result);
static void downgrade_to_nonsecure(struct proxy_participant *proxypp);
#endif

static int gcreq_participant (struct participant *pp);
static int gcreq_writer (struct writer *wr);
static int gcreq_reader (struct reader *rd);
static int gcreq_proxy_participant (struct proxy_participant *proxypp);
static int gcreq_proxy_writer (struct proxy_writer *pwr);
static int gcreq_proxy_reader (struct proxy_reader *prd);

#ifdef DDS_HAS_TOPIC_DISCOVERY
static int gcreq_topic (struct topic *tp);
static int gcreq_topic_definition (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp);
static int gcreq_proxy_topic (struct proxy_participant *proxypp, struct proxy_topic *proxytp, ddsrt_wctime_t timestamp);

static void set_topic_definition_hash (struct ddsi_topic_definition *tpd) ddsrt_nonnull_all;
static struct ddsi_topic_definition * new_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *type, const struct dds_qos *qos) ddsrt_nonnull ((1, 3));
static struct ddsi_topic_definition * ref_topic_definition_locked (struct ddsi_domaingv *gv, const struct ddsi_sertype *type, const ddsi_typeid_t *type_id, struct dds_qos *qos, bool *is_new) ddsrt_nonnull ((1, 3, 4, 5));
static struct ddsi_topic_definition * ref_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *type, const ddsi_typeid_t *type_id, struct dds_qos *qos, bool *is_new) ddsrt_nonnull ((1, 3, 4, 5));
static void unref_topic_definition_locked (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp) ddsrt_nonnull_all;
static void unref_topic_definition (struct ddsi_domaingv *gv, struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp) ddsrt_nonnull_all;
static void delete_topic_definition_locked (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp) ddsrt_nonnull_all;

static int proxy_topic_equal (const struct proxy_topic *proxy_tp_a, const struct proxy_topic *proxy_tp_b); // FIXME: ddsrt_nonnull_all?
DDSI_LIST_GENERIC_PTR_DECL(inline, proxy_topic_list, struct proxy_topic *, ddsrt_attribute_unused);
DDSI_LIST_GENERIC_PTR_CODE(inline, proxy_topic_list, struct proxy_topic *, proxy_topic_equal)
#endif /* DDS_HAS_TOPIC_DISCOVERY */

DDS_EXPORT extern inline bool builtintopic_is_visible (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid, nn_vendorid_t vendorid);
DDS_EXPORT extern inline bool builtintopic_is_builtintopic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_sertype *type);
DDS_EXPORT extern inline struct ddsi_tkmap_instance *builtintopic_get_tkmap_entry (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_guid *guid);
DDS_EXPORT extern inline void builtintopic_write_endpoint (const struct ddsi_builtin_topic_interface *btif, const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive);
DDS_EXPORT extern inline void builtintopic_write_topic (const struct ddsi_builtin_topic_interface *btif, const struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp, bool alive);

DDS_EXPORT extern inline seqno_t writer_read_seq_xmit (const struct writer *wr);
DDS_EXPORT extern inline void writer_update_seq_xmit (struct writer *wr, seqno_t nv);

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

int is_topic_entityid (ddsi_entityid_t id)
{
  switch (id.u & NN_ENTITYID_KIND_MASK)
  {
    case NN_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN:
    case NN_ENTITYID_KIND_CYCLONE_TOPIC_USER:
      return 1;
    default:
      return 0;
  }
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
  else if (!vendor_is_eclipse_or_adlink (vendorid))
    return 0;
  else
  {
    if ((id.u & NN_ENTITYID_KIND_MASK) == NN_ENTITYID_KIND_CYCLONE_TOPIC_USER)
      return 0;
    return 1;
  }
}

int is_builtin_endpoint (ddsi_entityid_t id, nn_vendorid_t vendorid)
{
  return is_builtin_entityid (id, vendorid) && id.u != NN_ENTITYID_PARTICIPANT && !is_topic_entityid (id);
}

int is_builtin_topic (ddsi_entityid_t id, nn_vendorid_t vendorid)
{
  return is_builtin_entityid (id, vendorid) && is_topic_entityid (id);
}

#if defined(DDS_HAS_SECURITY) || !defined(NDEBUG)
static int is_builtin_volatile_endpoint (ddsi_entityid_t id)
{
  switch (id.u) {
#ifdef DDS_HAS_SECURITY
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
  case NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
    return 1;
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
  case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
  case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER:
  case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
  case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER:
    return 1;
#endif
  default:
    break;
  }
  return 0;
}
#endif

bool is_local_orphan_endpoint (const struct entity_common *e)
{
  return (e->guid.prefix.u[0] == 0 && e->guid.prefix.u[1] == 0 && e->guid.prefix.u[2] == 0 &&
          is_builtin_endpoint (e->guid.entityid, NN_VENDORID_ECLIPSE));
}

static int compare_ldur (const void *va, const void *vb)
{
  const struct ldur_fhnode *a = va;
  const struct ldur_fhnode *b = vb;
  return (a->ldur == b->ldur) ? 0 : (a->ldur < b->ldur) ? -1 : 1;
}

/* used in participant for keeping writer liveliness renewal */
const ddsrt_fibheap_def_t ldur_fhdef = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct ldur_fhnode, heapnode), compare_ldur);
/* used in (proxy)participant for writer liveliness monitoring */
const ddsrt_fibheap_def_t lease_fhdef_pp = DDSRT_FIBHEAPDEF_INITIALIZER(offsetof (struct lease, pp_heapnode), compare_lease_tdur);

static void entity_common_init (struct entity_common *e, struct ddsi_domaingv *gv, const struct ddsi_guid *guid, const char *name, enum entity_kind kind, ddsrt_wctime_t tcreate, nn_vendorid_t vendorid, bool onlylocal)
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
  x->rdary = ddsrt_realloc (x->rdary, (x->n_readers + 2) * sizeof (*x->rdary));
  if (x->n_readers <= 1 || rd->type == x->rdary[x->n_readers - 1]->type)
  {
    /* if the first or second reader, or if the type is the same as that of
       the last one in the list simply appending the new will maintain order */
    x->rdary[x->n_readers] = rd;
  }
  else
  {
    uint32_t i;
    for (i = 0; i < x->n_readers; i++)
      if (x->rdary[i]->type == rd->type)
        break;
    if (i < x->n_readers)
    {
      /* shift any with the same type plus whichever follow to make room */
      memmove (&x->rdary[i + 1], &x->rdary[i], (x->n_readers - i) * sizeof (x->rdary[i]));
    }
    x->rdary[i] = rd;
  }
  x->rdary[x->n_readers + 1] = NULL;
  x->n_readers++;
  ddsrt_mutex_unlock (&x->rdary_lock);
}

static void local_reader_ary_remove (struct local_reader_ary *x, struct reader *rd)
{
  uint32_t i;
  ddsrt_mutex_lock (&x->rdary_lock);
  for (i = 0; i < x->n_readers; i++)
    if (x->rdary[i] == rd)
      break;
  assert (i < x->n_readers);
  if (i + 1 < x->n_readers)
  {
    /* dropping the final one never requires any fixups; dropping one that has
       the same type as the last is as simple as moving the last one in the
       removed one's location; else shift all following readers to keep it
       grouped by type */
    if (rd->type == x->rdary[x->n_readers - 1]->type)
      x->rdary[i] = x->rdary[x->n_readers - 1];
    else
      memmove (&x->rdary[i], &x->rdary[i + 1], (x->n_readers - i - 1) * sizeof (x->rdary[i]));
  }
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
    case EK_TOPIC:
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

void ddsi_make_writer_info(struct ddsi_writer_info *wrinfo, const struct entity_common *e, const struct dds_qos *xqos, uint32_t statusinfo)
{
#ifndef DDS_HAS_LIFESPAN
  DDSRT_UNUSED_ARG (statusinfo);
#endif
  wrinfo->guid = e->guid;
  wrinfo->ownership_strength = xqos->ownership_strength.value;
  wrinfo->auto_dispose = xqos->writer_data_lifecycle.autodispose_unregistered_instances;
  wrinfo->iid = e->iid;
#ifdef DDS_HAS_LIFESPAN
  if (xqos->lifespan.duration != DDS_INFINITY && (statusinfo & (NN_STATUSINFO_UNREGISTER | NN_STATUSINFO_DISPOSE)) == 0)
    wrinfo->lifespan_exp = ddsrt_mtime_add_duration(ddsrt_time_monotonic(), xqos->lifespan.duration);
  else
    wrinfo->lifespan_exp = DDSRT_MTIME_NEVER;
#endif
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

static void prune_deleted_participant_guids_unlocked (struct deleted_participants_admin *admin, ddsrt_mtime_t tnow)
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

static void prune_deleted_participant_guids (struct deleted_participants_admin *admin, ddsrt_mtime_t tnow)
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
      n->t_prune = DDSRT_MTIME_NEVER;
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
  prune_deleted_participant_guids_unlocked (admin, ddsrt_time_monotonic ());
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
    n->t_prune = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), admin->delay);
  ddsrt_mutex_unlock (&admin->deleted_participants_lock);
}

/* PARTICIPANT ------------------------------------------------------ */
static bool update_qos_locked (struct entity_common *e, dds_qos_t *ent_qos, const dds_qos_t *xqos, ddsrt_wctime_t timestamp)
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
  EELOGDISC (e, "update_qos_locked "PGUIDFMT" delta=%"PRIu64" QOS={", PGUID(e->guid), mask);
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

static void force_as_disc_address(struct ddsi_domaingv *gv, const ddsi_guid_t *subguid)
{
  struct writer *wr = entidx_lookup_writer_guid (gv->entity_index, subguid);
  assert (wr != NULL);
  ddsrt_mutex_lock (&wr->e.lock);
  unref_addrset (wr->as);
  unref_addrset (wr->as_group);
  wr->as = ref_addrset (gv->as_disc);
  wr->as_group = ref_addrset (gv->as_disc_group);
  ddsrt_mutex_unlock (&wr->e.lock);
}

#ifdef DDS_HAS_SECURITY
static void add_security_builtin_endpoints(struct participant *pp, ddsi_guid_t *subguid, const ddsi_guid_t *group_guid, struct ddsi_domaingv *gv, bool add_writers, bool add_readers)
{
  if (add_writers)
  {
    struct whc_writer_info *wrinfo;

    subguid->entityid = to_entityid (NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_endpoint_xqos_wr);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_SECURE_NAME, gv->spdp_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    /* But we need the as_disc address set for SPDP, because we need to
       send it to everyone regardless of the existence of readers. */
    force_as_disc_address(gv, subguid);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_stateless_xqos_wr);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_STATELESS_MESSAGE_NAME, gv->pgm_stateless_type, &gv->builtin_stateless_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_secure_volatile_xqos_wr);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_VOLATILE_MESSAGE_SECURE_NAME, gv->pgm_volatile_type, &gv->builtin_secure_volatile_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    whc_free_wrinfo (wrinfo);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_ANNOUNCER;

    wrinfo = whc_make_wrinfo (NULL, &gv->builtin_endpoint_xqos_wr);

    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_SECURE_NAME, gv->pmd_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_SECURE_NAME, gv->sedp_writer_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_SECURE_NAME, gv->sedp_reader_secure_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER;

    whc_free_wrinfo (wrinfo);
  }

  if (add_readers)
  {
    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_SECURE_NAME, gv->sedp_reader_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_DETECTOR;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_SECURE_NAME, gv->sedp_writer_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_DETECTOR;
  }

  /*
   * When security is enabled configure the associated necessary builtin readers independent of the
   * besmode flag setting, because all participant do require authentication.
   */
  subguid->entityid = to_entityid (NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_SECURE_NAME, gv->spdp_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_DETECTOR;

  subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_VOLATILE_MESSAGE_SECURE_NAME, gv->pgm_volatile_type, &gv->builtin_secure_volatile_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_DETECTOR;

  subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_STATELESS_MESSAGE_NAME, gv->pgm_stateless_type, &gv->builtin_stateless_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_DETECTOR;

  subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER);
  new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_SECURE_NAME, gv->pmd_secure_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
  pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_DETECTOR;
}
#endif

static void add_builtin_endpoints(struct participant *pp, ddsi_guid_t *subguid, const ddsi_guid_t *group_guid, struct ddsi_domaingv *gv, bool add_writers, bool add_readers)
{
  if (add_writers)
  {
    struct whc_writer_info *wrinfo_tl = whc_make_wrinfo (NULL, &gv->builtin_endpoint_xqos_wr);

    /* SEDP writers: */
    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME, gv->sedp_reader_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_NAME, gv->sedp_writer_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER;

    /* PMD writer: */
    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER);
    new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_NAME, gv->pmd_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER;

#ifdef DDS_HAS_TOPIC_DISCOVERY
    if (gv->config.enable_topic_discovery_endpoints)
    {
      /* SEDP topic writer: */
      subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER);
      new_writer_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TOPIC_NAME, gv->sedp_topic_type, &gv->builtin_endpoint_xqos_wr, whc_new(gv, wrinfo_tl), NULL, NULL);
      pp->bes |= NN_DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    /* TypeLookup writers */
    struct whc_writer_info *wrinfo_vol = whc_make_wrinfo (NULL, &gv->builtin_volatile_xqos_wr);
    struct writer *wr_tl_req, *wr_tl_reply;

    subguid->entityid = to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER);
    new_writer_guid (&wr_tl_req, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REQUEST_NAME, gv->tl_svc_request_type, &gv->builtin_volatile_xqos_wr, whc_new(gv, wrinfo_vol), NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_WRITER;

    subguid->entityid = to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
    new_writer_guid (&wr_tl_reply, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REPLY_NAME, gv->tl_svc_reply_type, &gv->builtin_volatile_xqos_wr, whc_new(gv, wrinfo_vol), NULL, NULL);
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
    subguid->entityid = to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_NAME, gv->spdp_type, &gv->spdp_endpoint_xqos, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR;

    /* SEDP readers: */
    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME, gv->sedp_reader_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR;

    subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PUBLICATION_NAME, gv->sedp_writer_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR;

    /* PMD reader: */
    subguid->entityid = to_entityid (NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_NAME, gv->pmd_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER;

#ifdef DDS_HAS_TOPIC_DISCOVERY
    if (gv->config.enable_topic_discovery_endpoints)
    {
      /* SEDP topic reader: */
      subguid->entityid = to_entityid (NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER);
      new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TOPIC_NAME, gv->sedp_topic_type, &gv->builtin_endpoint_xqos_rd, NULL, NULL, NULL);
      pp->bes |= NN_DISC_BUILTIN_ENDPOINT_TOPICS_DETECTOR;
    }
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
    /* TypeLookup readers: */
    subguid->entityid = to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REQUEST_NAME, gv->tl_svc_request_type, &gv->builtin_volatile_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_READER;

    subguid->entityid = to_entityid (NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER);
    new_reader_guid (NULL, subguid, group_guid, pp, DDS_BUILTIN_TOPIC_TYPELOOKUP_REPLY_NAME, gv->tl_svc_reply_type, &gv->builtin_volatile_xqos_rd, NULL, NULL, NULL);
    pp->bes |= NN_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_READER;
#endif
  }

#ifdef DDS_HAS_SECURITY
  if (q_omg_participant_is_secure (pp))
    add_security_builtin_endpoints (pp, subguid, group_guid, gv, add_writers, add_readers);
#endif
}

#ifdef DDS_HAS_SECURITY
static void connect_participant_secure(struct ddsi_domaingv *gv, struct participant *pp)
{
  struct proxy_participant *proxypp;
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

static void disconnect_participant_secure(struct participant *pp)
{
  struct proxy_participant *proxypp;
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
#endif

static void gc_participant_lease (struct gcreq *gcreq)
{
  lease_free (gcreq->arg);
  gcreq_free (gcreq);
}

static void participant_replace_minl (struct participant *pp, struct lease *lnew)
{
  /* By loading/storing the pointer atomically, we ensure we always
     read a valid (or once valid) lease. By delaying freeing the lease
     through the garbage collector, we ensure whatever lease update
     occurs in parallel completes before the memory is released. */
  struct gcreq *gcreq = gcreq_new (pp->e.gv->gcreq_queue, gc_participant_lease);
  struct lease *lease_old = ddsrt_atomic_ldvoidp (&pp->minl_man);
  assert (lease_old != NULL);
  lease_unregister (lease_old); /* ensures lease will not expire while it is replaced */
  gcreq->arg = lease_old;
  gcreq_enqueue (gcreq);
  ddsrt_atomic_stvoidp (&pp->minl_man, lnew);
}

static void participant_add_wr_lease_locked (struct participant * pp, const struct writer * wr)
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

static void participant_remove_wr_lease_locked (struct participant * pp, struct writer * wr)
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
    GVLOGDISC ("new_participant("PGUIDFMT"): using security settings from QoS\n", PGUID (*ppguid));

    /* check if all required security properties exist in qos; report all missing ones, not just the first */
    dds_return_t ret = DDS_RETCODE_OK;
    for (size_t i = 0; i < sizeof(req) / sizeof(req[0]); i++)
    {
      const char *value;
      if (!ddsi_xqos_find_prop (qos, req[i], &value) || strlen (value) == 0)
      {
        GVERROR ("new_participant("PGUIDFMT"): required security property %s missing in Property QoS\n", PGUID (*ppguid), req[i]);
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
      GVERROR ("new_participant("PGUIDFMT"): CRL security property " ORG_ECLIPSE_CYCLONEDDS_SEC_AUTH_CRL " absent in Property QoS but specified in XML configuration\n", PGUID (*ppguid));
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
    }

    if (ret != DDS_RETCODE_OK)
      return ret;
  }
  else if (gv->config.omg_security_configuration)
  {
    /* For security, configuration can be provided through the configuration.  However, the specification
       (and the plugins) expect it to be in the QoS, so merge it in. */
    GVLOGDISC ("new_participant("PGUIDFMT"): using security settings from configuration\n", PGUID (*ppguid));
    ddsi_xqos_mergein_security_config (qos, &gv->config.omg_security_configuration->cfg);
  }
  else
  {
    /* No security configuration */
    return DDS_RETCODE_OK;
  }

  if (q_omg_is_security_loaded (gv->security_context))
  {
    GVLOGDISC ("new_participant("PGUIDFMT"): security is already loaded for this domain\n", PGUID (*ppguid));
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
#endif

dds_return_t new_participant_guid (ddsi_guid_t *ppguid, struct ddsi_domaingv *gv, unsigned flags, const ddsi_plist_t *plist)
{
  struct participant *pp;
  ddsi_guid_t subguid, group_guid;
  struct whc_writer_info *wrinfo;
  dds_return_t ret = DDS_RETCODE_OK;
  ddsi_tran_conn_t ppconn;

  /* no reserved bits may be set */
  assert ((flags & ~(RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS | RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP | RTPS_PF_ONLY_LOCAL)) == 0);
  /* privileged participant MUST have builtin readers and writers */
  assert (!(flags & RTPS_PF_PRIVILEGED_PP) || (flags & (RTPS_PF_NO_BUILTIN_READERS | RTPS_PF_NO_BUILTIN_WRITERS)) == 0);

  prune_deleted_participant_guids (gv->deleted_participants, ddsrt_time_monotonic ());

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
      GVERROR ("new_participant("PGUIDFMT", %x) failed: could not create network endpoint\n", PGUID (*ppguid), flags);
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
      GVERROR ("new_participant("PGUIDFMT", %x) failed: max participants reached\n", PGUID (*ppguid), flags);
      if (ppconn)
        ddsi_conn_free (ppconn);
      ret = DDS_RETCODE_OUT_OF_RESOURCES;
      goto new_pp_err;
    }
  }

  GVLOGDISC ("new_participant("PGUIDFMT", %x)\n", PGUID (*ppguid), flags);

  pp = ddsrt_malloc (sizeof (*pp));

  entity_common_init (&pp->e, gv, ppguid, "", EK_PARTICIPANT, ddsrt_time_wallclock (), NN_VENDORID_ECLIPSE, ((flags & RTPS_PF_ONLY_LOCAL) != 0));
  pp->user_refc = 1;
  pp->builtin_refc = 0;
  pp->state = PARTICIPANT_STATE_INITIALIZING;
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
    ddsi_conn_locator (pp->m_conn, &pp->m_locator);

  ddsrt_fibheap_init (&lease_fhdef_pp, &pp->leaseheap_man);
  ddsrt_atomic_stvoidp (&pp->minl_man, NULL);

  /* Before we create endpoints -- and may call unref_participant if
     things go wrong -- we must initialize all that unref_participant
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
    subguid.entityid = to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
    wrinfo = whc_make_wrinfo (NULL, &gv->spdp_endpoint_xqos);
    new_writer_guid (NULL, &subguid, &group_guid, pp, DDS_BUILTIN_TOPIC_PARTICIPANT_NAME, gv->spdp_type, &gv->spdp_endpoint_xqos, whc_new(gv, wrinfo), NULL, NULL);
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

  /* All attributes set, anyone looking for a built-in topic writer can
     now safely do so */
  ddsrt_mutex_lock (&pp->refc_lock);
  pp->state = PARTICIPANT_STATE_OPERATIONAL;
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
  entity_common_fini (&pp->e);
  ddsrt_free (pp);
  ddsrt_mutex_lock (&gv->participant_set_lock);
  gv->nparticipants--;
  ddsrt_mutex_unlock (&gv->participant_set_lock);
new_pp_err:
  return ret;
}

dds_return_t new_participant (ddsi_guid_t *p_ppguid, struct ddsi_domaingv *gv, unsigned flags, const ddsi_plist_t *plist)
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

void update_participant_plist (struct participant *pp, const ddsi_plist_t *plist)
{
  ddsrt_mutex_lock (&pp->e.lock);
  if (update_qos_locked (&pp->e, &pp->plist->qos, &plist->qos, ddsrt_time_wallclock ()))
    spdp_write (pp);
  ddsrt_mutex_unlock (&pp->e.lock);
}

bool participant_builtin_writers_ready (struct participant *pp)
{
  // lock is needed to read the state, we're fine even if the state flips
  // from operational to deleting, this exists to protect against the gap
  // between making the participant discoverable through the entity index
  // and checking pp->bes
  ddsrt_mutex_lock (&pp->refc_lock);
  const bool x = pp->state >= PARTICIPANT_STATE_OPERATIONAL;
  ddsrt_mutex_unlock (&pp->refc_lock);
  return x;
}

static void delete_builtin_endpoint (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, unsigned entityid)
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

static void update_pp_refc (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity, int n)
{
  assert (n == -1 || n == 1);
  if (guid_of_refing_entity
      && is_builtin_entityid (guid_of_refing_entity->entityid, NN_VENDORID_ECLIPSE)
      && guid_of_refing_entity->entityid.u != NN_ENTITYID_PARTICIPANT)
    pp->builtin_refc += n;
  else
    pp->user_refc += n;
  assert (pp->user_refc >= 0);
  assert (pp->builtin_refc >= 0);
}

static struct participant *ref_participant (struct participant *pp, const struct ddsi_guid *guid_of_refing_entity)
{
  ddsrt_mutex_lock (&pp->refc_lock);
  update_pp_refc (pp, guid_of_refing_entity, 1);
  ddsi_guid_t stguid;
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
  ELOGDISC (pp, "unref_participant("PGUIDFMT" @ %p <- "PGUIDFMT" @ %p) user %"PRId32" builtin %"PRId32"\n",
            PGUID (pp->e.guid), (void*)pp, PGUID (stguid), (void*)guid_of_refing_entity, pp->user_refc, pp->builtin_refc);

  if (pp->user_refc == 0 && pp->bes != 0 && pp->state < PARTICIPANT_STATE_DELETING_BUILTINS)
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
       found in entity_index and are simply ignored. */
    pp->state = PARTICIPANT_STATE_DELETING_BUILTINS;
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

dds_return_t delete_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid)
{
  struct participant *pp;
  GVLOGDISC ("delete_participant("PGUIDFMT")\n", PGUID (*ppguid));
  ddsrt_mutex_lock (&gv->lock);
  if ((pp = entidx_lookup_participant_guid (gv->entity_index, ppguid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  builtintopic_write_endpoint (gv->builtin_topic_interface, &pp->e, ddsrt_time_wallclock(), false);
  remember_deleted_participant_guid (gv->deleted_participants, &pp->e.guid);
#ifdef DDS_HAS_SECURITY
  disconnect_participant_secure (pp);
#endif
  ddsrt_mutex_lock (&pp->refc_lock);
  pp->state = PARTICIPANT_STATE_DELETE_STARTED;
  ddsrt_mutex_unlock (&pp->refc_lock);
  entidx_remove_participant_guid (gv->entity_index, pp);
  ddsrt_mutex_unlock (&gv->lock);
  gcreq_participant (pp);
  return 0;
}

struct writer *get_builtin_writer (const struct participant *pp, unsigned entityid)
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

dds_duration_t pp_get_pmd_interval (struct participant *pp)
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

/* WRITER/READER/PROXY-WRITER/PROXY-READER CONNECTION ---------------

   These are all located in a separate section because they are so
   very similar that co-locating them eases editing and checking. */

static uint32_t get_min_receive_buffer_size (struct writer *wr)
{
  uint32_t min_receive_buffer_size = UINT32_MAX;
  struct entity_index *gh = wr->e.gv->entity_index;
  ddsrt_avl_iter_t it;
  for (struct wr_prd_match *m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct proxy_reader *prd;
    if ((prd = entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
      continue;
    if (prd->receive_buffer_size < min_receive_buffer_size)
      min_receive_buffer_size = prd->receive_buffer_size;
  }
  return min_receive_buffer_size;
}

static void rebuild_writer_addrset (struct writer *wr)
{
  /* FIXME: way too inefficient in this form:
     - it gets computed for every change
     - in many cases the set of addresses from the readers
       is identical, so we could cache the results */

  /* only one operation at a time */
  ASSERT_MUTEX_HELD (&wr->e.lock);

  /* swap in new address set; this simple procedure is ok as long as
     wr->as is never accessed without the wr->e.lock held */
  struct addrset * const oldas = wr->as;
  wr->as = compute_writer_addrset (wr);
  unref_addrset (oldas);

  /* Computing burst size limit here is a bit of a hack; but anyway ...
     try to limit bursts of retransmits to 67% of the smallest receive
     buffer, and those of initial transmissions to that + overshoot%.
     It is usually best to send the full sample initially, always:
     - if the receivers manage to keep up somewhat, sending it in one
       go and then recovering anything lost is way faster then sending
       only small batches
     - the way things are now: the retransmits will be sent unicast,
       so if there are multiple receivers, that'll blow up things by
       a non-trivial amount */
  const uint32_t min_receive_buffer_size = get_min_receive_buffer_size (wr);
  wr->rexmit_burst_size_limit = min_receive_buffer_size - min_receive_buffer_size / 3;
  if (wr->rexmit_burst_size_limit < 1024)
    wr->rexmit_burst_size_limit = 1024;
  if (wr->rexmit_burst_size_limit > wr->e.gv->config.max_rexmit_burst_size)
    wr->rexmit_burst_size_limit = wr->e.gv->config.max_rexmit_burst_size;
  if (wr->rexmit_burst_size_limit > UINT32_MAX - UINT16_MAX)
    wr->rexmit_burst_size_limit = UINT32_MAX - UINT16_MAX;

  const uint64_t limit64 = (uint64_t) wr->e.gv->config.init_transmit_extra_pct * (uint64_t) min_receive_buffer_size / 100;
  if (limit64 > UINT32_MAX - UINT16_MAX)
    wr->init_burst_size_limit = UINT32_MAX - UINT16_MAX;
  else if (limit64 < wr->rexmit_burst_size_limit)
    wr->init_burst_size_limit = wr->rexmit_burst_size_limit;
  else
    wr->init_burst_size_limit = (uint32_t) limit64;

  ELOGDISC (wr, "rebuild_writer_addrset("PGUIDFMT"):", PGUID (wr->e.guid));
  nn_log_addrset(wr->e.gv, DDS_LC_DISCOVERY, "", wr->as);
  ELOGDISC (wr, " (burst size %"PRIu32" rexmit %"PRIu32")\n", wr->init_burst_size_limit, wr->rexmit_burst_size_limit);
}

void rebuild_or_clear_writer_addrsets (struct ddsi_domaingv *gv, int rebuild)
{
  struct entidx_enum_writer est;
  struct writer *wr;
  struct addrset *empty = rebuild ? NULL : new_addrset();
  GVLOGDISC ("rebuild_or_delete_writer_addrsets(%d)\n", rebuild);
  entidx_enum_writer_init (&est, gv->entity_index);
  while ((wr = entidx_enum_writer_next (&est)) != NULL)
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
  entidx_enum_writer_fini (&est);
  unref_addrset(empty);
  GVLOGDISC ("rebuild_or_delete_writer_addrsets(%d) done\n", rebuild);
}

static void free_wr_prd_match (const struct ddsi_domaingv *gv, const ddsi_guid_t *wr_guid, struct wr_prd_match *m)
{
  if (m)
  {
#ifdef DDS_HAS_SECURITY
    q_omg_security_deregister_remote_reader_match (gv, wr_guid, m);
#else
    (void) gv;
    (void) wr_guid;
#endif
    nn_lat_estim_fini (&m->hb_to_ack_latency);
    ddsrt_free (m);
  }
}

static void free_rd_pwr_match (struct ddsi_domaingv *gv, const ddsi_guid_t *rd_guid, struct rd_pwr_match *m)
{
  if (m)
  {
#ifdef DDS_HAS_SECURITY
    q_omg_security_deregister_remote_writer_match (gv, rd_guid, m);
#else
    (void) rd_guid;
#endif
#ifdef DDS_HAS_SSM
    if (!is_unspec_xlocator (&m->ssm_mc_loc))
    {
      assert (ddsi_is_mcaddr (gv, &m->ssm_mc_loc.c));
      assert (!is_unspec_xlocator (&m->ssm_src_loc));
      if (ddsi_leave_mc (gv, gv->mship, gv->data_conn_mc, &m->ssm_src_loc.c, &m->ssm_mc_loc.c) < 0)
        GVWARNING ("failed to leave network partition ssm group\n");
    }
#endif
#if !(defined DDS_HAS_SECURITY || defined DDS_HAS_SSM)
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

static void writer_get_alive_state_locked (struct writer *wr, struct alive_state *st)
{
  st->alive = wr->alive;
  st->vclock = wr->alive_vclock;
}

static void writer_get_alive_state (struct writer *wr, struct alive_state *st)
{
  ddsrt_mutex_lock (&wr->e.lock);
  writer_get_alive_state_locked (wr, st);
  ddsrt_mutex_unlock (&wr->e.lock);
}

static void proxy_writer_get_alive_state_locked (struct proxy_writer *pwr, struct alive_state *st)
{
  st->alive = pwr->alive;
  st->vclock = pwr->alive_vclock;
}

static void proxy_writer_get_alive_state (struct proxy_writer *pwr, struct alive_state *st)
{
  ddsrt_mutex_lock (&pwr->e.lock);
  proxy_writer_get_alive_state_locked (pwr, st);
  ddsrt_mutex_unlock (&pwr->e.lock);
}

static void writer_drop_connection (const struct ddsi_guid *wr_guid, const struct proxy_reader *prd)
{
  struct writer *wr;
  if ((wr = entidx_lookup_writer_guid (prd->e.gv->entity_index, wr_guid)) != NULL)
  {
    struct whc_node *deferred_free_list = NULL;
    struct wr_prd_match *m;
    ddsrt_mutex_lock (&wr->e.lock);
    if ((m = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, &prd->e.guid)) != NULL)
    {
      struct whc_state whcst;
      ddsrt_avl_delete (&wr_readers_treedef, &wr->readers, m);
      wr->num_readers--;
      wr->num_reliable_readers -= m->is_reliable;
      wr->num_readers_requesting_keyhash -= prd->requests_keyhash ? 1 : 0;
      rebuild_writer_addrset (wr);
      remove_acked_messages (wr, &whcst, &deferred_free_list);
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
    free_wr_prd_match (wr->e.gv, &wr->e.guid, m);
  }
}

static void writer_drop_local_connection (const struct ddsi_guid *wr_guid, struct reader *rd)
{
  /* Only called by gc_delete_reader, so we actually have a reader pointer */
  struct writer *wr;
  if ((wr = entidx_lookup_writer_guid (rd->e.gv->entity_index, wr_guid)) != NULL)
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

static void reader_update_notify_alive_state_invoke_cb (struct reader *rd, uint64_t iid, bool notify, int delta, const struct alive_state *alive_state)
{
  /* Liveliness changed events can race each other and can, potentially, be delivered
   in a different order. */
  if (notify && rd->status_cb)
  {
    status_cb_data_t data;
    data.handle = iid;
    data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
    if (delta < 0) {
      data.extra = (uint32_t) LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE;
      (rd->status_cb) (rd->status_cb_entity, &data);
    } else if (delta > 0) {
      data.extra = (uint32_t) LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE;
      (rd->status_cb) (rd->status_cb_entity, &data);
    } else {
      /* Twitch: the resulting (proxy)writer state is unchanged, but there has been
        a transition to another state and back to the current state. So we'll call
        the callback twice in this case. */
      static const enum liveliness_changed_data_extra x[] = {
        LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE,
        LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE
      };
      data.extra = (uint32_t) x[alive_state->alive];
      (rd->status_cb) (rd->status_cb_entity, &data);
      data.extra = (uint32_t) x[!alive_state->alive];
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void reader_update_notify_wr_alive_state (struct reader *rd, const struct writer *wr, const struct alive_state *alive_state)
{
  struct rd_wr_match *m;
  bool notify = false;
  int delta = 0; /* -1: alive -> not_alive; 0: unchanged; 1: not_alive -> alive */
  ddsrt_mutex_lock (&rd->e.lock);
  if ((m = ddsrt_avl_lookup (&rd_local_writers_treedef, &rd->local_writers, &wr->e.guid)) != NULL)
  {
    if ((int32_t) (alive_state->vclock - m->wr_alive_vclock) > 0)
    {
      delta = (int) alive_state->alive - (int) m->wr_alive;
      notify = true;
      m->wr_alive = alive_state->alive;
      m->wr_alive_vclock = alive_state->vclock;
    }
  }
  ddsrt_mutex_unlock (&rd->e.lock);

  if (delta < 0 && rd->rhc)
  {
    struct ddsi_writer_info wrinfo;
    ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, NN_STATUSINFO_UNREGISTER);
    ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
  }

  reader_update_notify_alive_state_invoke_cb (rd, wr->e.iid, notify, delta, alive_state);
}

static void reader_update_notify_wr_alive_state_guid (const struct ddsi_guid *rd_guid, const struct writer *wr, const struct alive_state *alive_state)
{
  struct reader *rd;
  if ((rd = entidx_lookup_reader_guid (wr->e.gv->entity_index, rd_guid)) != NULL)
    reader_update_notify_wr_alive_state (rd, wr, alive_state);
}

static void reader_update_notify_pwr_alive_state (struct reader *rd, const struct proxy_writer *pwr, const struct alive_state *alive_state)
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
    ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos, NN_STATUSINFO_UNREGISTER);
    ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
  }

  reader_update_notify_alive_state_invoke_cb (rd, pwr->e.iid, notify, delta, alive_state);
}

static void reader_update_notify_pwr_alive_state_guid (const struct ddsi_guid *rd_guid, const struct proxy_writer *pwr, const struct alive_state *alive_state)
{
  struct reader *rd;
  if ((rd = entidx_lookup_reader_guid (pwr->e.gv->entity_index, rd_guid)) != NULL)
    reader_update_notify_pwr_alive_state (rd, pwr, alive_state);
}

static void reader_drop_connection (const struct ddsi_guid *rd_guid, const struct proxy_writer *pwr)
{
  struct reader *rd;
  if ((rd = entidx_lookup_reader_guid (pwr->e.gv->entity_index, rd_guid)) != NULL)
  {
    struct rd_pwr_match *m;
    ddsrt_mutex_lock (&rd->e.lock);
    if ((m = ddsrt_avl_lookup (&rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
    {
      ddsrt_avl_delete (&rd_writers_treedef, &rd->writers, m);
      rd->num_writers--;
    }

    ddsrt_mutex_unlock (&rd->e.lock);
    if (m != NULL)
    {
      if (rd->rhc)
      {
        struct ddsi_writer_info wrinfo;
        ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos, NN_STATUSINFO_UNREGISTER);
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
    free_rd_pwr_match (pwr->e.gv, &rd->e.guid, m);
  }
}

static void reader_drop_local_connection (const struct ddsi_guid *rd_guid, const struct writer *wr)
{
  struct reader *rd;
  if ((rd = entidx_lookup_reader_guid (wr->e.gv->entity_index, rd_guid)) != NULL)
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
        ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, NN_STATUSINFO_UNREGISTER);
        ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
      }
      if (rd->status_cb)
      {
        status_cb_data_t data;
        data.handle = wr->e.iid;
        data.add = false;
        data.extra = (uint32_t) (m->wr_alive ? LIVELINESS_CHANGED_REMOVE_ALIVE : LIVELINESS_CHANGED_REMOVE_NOT_ALIVE);

        data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);

        data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
        (rd->status_cb) (rd->status_cb_entity, &data);
      }
    }
    free_rd_wr_match (m);
  }
}

static void update_reader_init_acknack_count (const ddsrt_log_cfg_t *logcfg, const struct entity_index *entidx, const struct ddsi_guid *rd_guid, nn_count_t count)
{
  struct reader *rd;

  /* Update the initial acknack sequence number for the reader.  See
     also reader_add_connection(). */
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "update_reader_init_acknack_count ("PGUIDFMT", %"PRIu32"): ", PGUID (*rd_guid), count);
  if ((rd = entidx_lookup_reader_guid (entidx, rd_guid)) != NULL)
  {
    ddsrt_mutex_lock (&rd->e.lock);
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "%"PRIu32" -> ", rd->init_acknack_count);
    if (count > rd->init_acknack_count)
      rd->init_acknack_count = count;
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "%"PRIu32"\n", count);
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
  if ((pwr = entidx_lookup_proxy_writer_guid (rd->e.gv->entity_index, pwr_guid)) != NULL)
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
      const bool isreliable = (pwr->c.xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
      if (pwr->n_reliable_readers == 0 && isreliable && pwr->have_seen_heartbeat)
      {
        pwr->have_seen_heartbeat = 0;
        nn_defrag_notegap (pwr->defrag, 1, pwr->last_seq + 1);
        nn_reorder_drop_upto (pwr->reorder, pwr->last_seq + 1);
      }
      local_reader_ary_remove (&pwr->rdary, rd);
    }
    ddsrt_mutex_unlock (&pwr->e.lock);
    if (m)
    {
      update_reader_init_acknack_count (&rd->e.gv->logconfig, rd->e.gv->entity_index, &rd->e.guid, m->count);
      if (m->filtered)
        nn_defrag_prune(pwr->defrag, &m->rd_guid.prefix, m->last_seq);
    }
    free_pwr_rd_match (m);
  }
}

static void proxy_reader_drop_connection (const struct ddsi_guid *prd_guid, struct writer *wr)
{
  struct proxy_reader *prd;
  if ((prd = entidx_lookup_proxy_reader_guid (wr->e.gv->entity_index, prd_guid)) != NULL)
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

static void writer_add_connection (struct writer *wr, struct proxy_reader *prd, int64_t crypto_handle)
{
  struct wr_prd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;
  int pretend_everything_acked;

#ifdef DDS_HAS_SHM
  const bool use_iceoryx = prd->is_iceoryx && !(wr->xqos->ignore_locator_type & NN_LOCATOR_KIND_SHEM);
#else
  const bool use_iceoryx = false;
#endif

  m->prd_guid = prd->e.guid;
  m->is_reliable = (prd->c.xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT);
  m->assumed_in_sync = (wr->e.gv->config.retransmit_merging == DDSI_REXMIT_MERGE_ALWAYS);
  m->has_replied_to_hb = !m->is_reliable || use_iceoryx;
  m->all_have_replied_to_hb = 0;
  m->non_responsive_count = 0;
  m->rexmit_requests = 0;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif
  /* m->demoted: see below */
  ddsrt_mutex_lock (&prd->e.lock);
  if (prd->deleting)
  {
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - prd is being deleted\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid));
    pretend_everything_acked = 1;
  }
  else if (!m->is_reliable || use_iceoryx)
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
  m->prev_acknack = 0;
  m->prev_nackfrag = 0;
  nn_lat_estim_init (&m->hb_to_ack_latency);
  m->hb_to_ack_latency_tlastlog = ddsrt_time_wallclock ();
  m->t_acknack_accepted.v = 0;
  m->t_nackfrag_accepted.v = 0;

  ddsrt_mutex_lock (&wr->e.lock);
#ifdef DDS_HAS_SHM
  if (pretend_everything_acked || prd->is_iceoryx)
#else
  if (pretend_everything_acked)
#endif
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
    ELOGDISC (wr, "  writer_add_connection(wr "PGUIDFMT" prd "PGUIDFMT") - ack seq %"PRIu64"\n",
              PGUID (wr->e.guid), PGUID (prd->e.guid), m->seq);
    ddsrt_avl_insert_ipath (&wr_readers_treedef, &wr->readers, m, &path);
    wr->num_readers++;
    wr->num_reliable_readers += m->is_reliable;
    wr->num_readers_requesting_keyhash += prd->requests_keyhash ? 1 : 0;
    rebuild_writer_addrset (wr);
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
      const int64_t delta = DDS_MSECS (1);
      const ddsrt_mtime_t tnext = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), delta);
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

static void deliver_historical_data (const struct writer *wr, const struct reader *rd)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  struct ddsi_tkmap * const tkmap = gv->m_tkmap;
  struct whc_sample_iter it;
  struct whc_borrowed_sample sample;
  /* FIXME: should limit ourselves to what it is available because of durability history, not writer history */
  whc_sample_iter_init (wr->whc, &it);
  while (whc_sample_iter_borrow_next (&it, &sample))
  {
    struct ddsi_serdata *payload;
    if ((payload = ddsi_serdata_ref_as_type (rd->type, sample.serdata)) == NULL)
    {
      GVWARNING ("local: deserialization of %s/%s as %s/%s failed in topic type conversion\n",
                 wr->xqos->topic_name, wr->type->type_name, rd->xqos->topic_name, rd->type->type_name);
    }
    else
    {
      struct ddsi_writer_info wrinfo;
      struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (tkmap, payload);
      ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, payload->statusinfo);
      (void) ddsi_rhc_store (rd->rhc, &wrinfo, payload, tk);
      ddsi_tkmap_instance_unref (tkmap, tk);
      ddsi_serdata_unref (payload);
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
     historical data for best-effort data over the wire, so also not locally). */
  if (rd->xqos->reliability.kind > DDS_RELIABILITY_BEST_EFFORT && rd->xqos->durability.kind > DDS_DURABILITY_VOLATILE)
    deliver_historical_data (wr, rd);

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

static void reader_add_connection (struct reader *rd, struct proxy_writer *pwr, nn_count_t *init_count, const struct alive_state *alive_state, int64_t crypto_handle)
{
  struct rd_pwr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->pwr_guid = pwr->e.guid;
  m->pwr_alive = alive_state->alive;
  m->pwr_alive_vclock = alive_state->vclock;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

  ddsrt_mutex_lock (&rd->e.lock);

  /* Initial sequence number of acknacks is the highest stored (+ 1,
     done when generating the acknack) -- existing connections may be
     beyond that already, but this guarantees that one particular
     writer will always see monotonically increasing sequence numbers
     from one particular reader.  This is then used for the
     pwr_rd_match initialization */
  ELOGDISC (rd, "  reader "PGUIDFMT" init_acknack_count = %"PRIu32"\n",
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
    rd->num_writers++;
    ddsrt_mutex_unlock (&rd->e.lock);

#ifdef DDS_HAS_SSM
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
      int ret = ddsi_join_mc (rd->e.gv, rd->e.gv->mship, rd->e.gv->data_conn_mc, &m->ssm_src_loc.c, &m->ssm_mc_loc.c);
      if (ret < 0)
        ELOGDISC (rd, "  unable to join\n");
    }
    else
    {
      set_unspec_xlocator (&m->ssm_src_loc);
      set_unspec_xlocator (&m->ssm_mc_loc);
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

static void reader_add_local_connection (struct reader *rd, struct writer *wr, const struct alive_state *alive_state)
{
  struct rd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
  m->wr_alive = alive_state->alive;
  m->wr_alive_vclock = alive_state->vclock;

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
      data.extra = (uint32_t) (alive_state->alive ? LIVELINESS_CHANGED_ADD_ALIVE : LIVELINESS_CHANGED_ADD_NOT_ALIVE);

      data.raw_status_id = (int) DDS_SUBSCRIPTION_MATCHED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);

      data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

static void proxy_writer_add_connection (struct proxy_writer *pwr, struct reader *rd, ddsrt_mtime_t tnow, nn_count_t init_count, int64_t crypto_handle)
{
  struct pwr_rd_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  ddsrt_mutex_lock (&pwr->e.lock);
  if (ddsrt_avl_lookup_ipath (&pwr_readers_treedef, &pwr->readers, &rd->e.guid, &path))
    goto already_matched;

  assert (rd->type || is_builtin_endpoint (rd->e.guid.entityid, NN_VENDORID_ECLIPSE));
  if (pwr->ddsi2direct_cb == 0 && rd->ddsi2direct_cb != 0)
  {
    pwr->ddsi2direct_cb = rd->ddsi2direct_cb;
    pwr->ddsi2direct_cbarg = rd->ddsi2direct_cbarg;
  }

#ifdef DDS_HAS_SHM
  const bool use_iceoryx = pwr->is_iceoryx && !(rd->xqos->ignore_locator_type & NN_LOCATOR_KIND_SHEM);
#else
  const bool use_iceoryx = false;
#endif

  ELOGDISC (pwr, "  proxy_writer_add_connection(pwr "PGUIDFMT" rd "PGUIDFMT")",
            PGUID (pwr->e.guid), PGUID (rd->e.guid));
  m->rd_guid = rd->e.guid;
  m->tcreate = ddsrt_time_monotonic ();

  /* We track the last heartbeat count value per reader--proxy-writer
     pair, so that we can correctly handle directed heartbeats. The
     only reason to bother is to prevent a directed heartbeat (with
     the FINAL flag clear) from causing AckNacks from all readers
     instead of just the addressed ones.

     If we don't mind those extra AckNacks, we could track the count
     at the proxy-writer and simply treat all incoming heartbeats as
     undirected. */
  m->prev_heartbeat = 0;
  m->hb_timestamp.v = 0;
  m->t_heartbeat_accepted.v = 0;
  m->t_last_nack.v = 0;
  m->t_last_ack.v = 0;
  m->last_nack.seq_end_p1 = 0;
  m->last_nack.seq_base = 0;
  m->last_nack.frag_end_p1 = 0;
  m->last_nack.frag_base = 0;
  m->last_seq = 0;
  m->filtered = 0;
  m->ack_requested = 0;
  m->heartbeat_since_ack = 0;
  m->heartbeatfrag_since_ack = 0;
  m->directed_heartbeat = 0;
  m->nack_sent_on_nackdelay = 0;

#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

  /* These can change as a consequence of handling data and/or
     discovery activities. The safe way of dealing with them is to
     lock the proxy writer */
  if (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) && !ddsrt_avl_is_empty (&pwr->readers) && !pwr->filtered)
  {
    /* builtins really don't care about multiple copies or anything */
    m->in_sync = PRMSS_SYNC;
  }
  else if (use_iceoryx)
  {
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
     schedule it for the configured delay. From then on it it'll keep
     sending pre-emptive ones until the proxy writer receives a heartbeat.
     (We really only need a pre-emptive AckNack per proxy writer, but
     hopefully it won't make that much of a difference in practice.) */
  if (rd->reliable)
  {
    uint32_t secondary_reorder_maxsamples = pwr->e.gv->config.secondary_reorder_maxsamples;

    if (rd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    {
      secondary_reorder_maxsamples = pwr->e.gv->config.primary_reorder_maxsamples;
      m->filtered = 1;
    }

    const ddsrt_mtime_t tsched = use_iceoryx ? DDSRT_MTIME_NEVER : ddsrt_mtime_add_duration (tnow, pwr->e.gv->config.preemptive_ack_delay);
    m->acknack_xevent = qxev_acknack (pwr->evq, tsched, &pwr->e.guid, &rd->e.guid);
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

static void proxy_reader_add_connection (struct proxy_reader *prd, struct writer *wr, int64_t crypto_handle)
{
  struct prd_wr_match *m = ddsrt_malloc (sizeof (*m));
  ddsrt_avl_ipath_t path;

  m->wr_guid = wr->e.guid;
#ifdef DDS_HAS_SECURITY
  m->crypto_handle = crypto_handle;
#else
  DDSRT_UNUSED_ARG(crypto_handle);
#endif

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
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case NN_ENTITYID_SEDP_BUILTIN_TOPIC_READER:
      res.u = NN_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER;
      break;
#endif
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
#ifdef DDS_HAS_TYPE_DISCOVERY
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER;
      break;
    case NN_ENTITYID_TL_SVC_BUILTIN_REPLY_READER:
      res.u = NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER;
      break;
#endif

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

static bool topickind_qos_match_p_lock (
    struct ddsi_domaingv *gv,
    struct entity_common *rd,
    const dds_qos_t *rdqos,
    struct entity_common *wr,
    const dds_qos_t *wrqos,
    dds_qos_policy_id_t *reason
#ifdef DDS_HAS_TYPE_DISCOVERY
    , const struct ddsi_type_pair *rd_type_pair
    , const struct ddsi_type_pair *wr_type_pair
#endif
)
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
#ifdef DDS_HAS_TYPE_DISCOVERY
  bool rd_type_lookup, wr_type_lookup;
  const ddsi_typeid_t *req_type_id = NULL;
  const ddsi_typeid_t ** req_dep_ids = NULL;
  uint32_t req_ndep_ids = 0;
  bool ret = qos_match_p (gv, rdqos, wrqos, reason, rd_type_pair, wr_type_pair, &rd_type_lookup, &wr_type_lookup);
  if (!ret)
  {
    /* In case qos_match_p returns false, one of rd_type_look and wr_type_lookup could
       be set to indicate that type information is missing. At this point, we know this
       is the case so do a type lookup request for either rd_type_pair->minimal or
       wr_type_pair->minimal. */
    if (rd_type_lookup)
    {
      req_type_id = ddsi_type_pair_minimal_id (rd_type_pair);
      if (rdqos->present & QP_TYPE_INFORMATION)
        req_ndep_ids = ddsi_typeinfo_get_dependent_typeids (rdqos->type_information, &req_dep_ids, DDSI_TYPEID_KIND_MINIMAL);
    }
    else if (wr_type_lookup)
    {
      req_type_id = ddsi_type_pair_minimal_id (wr_type_pair);
      if (wrqos->present & QP_TYPE_INFORMATION)
        req_ndep_ids = ddsi_typeinfo_get_dependent_typeids (wrqos->type_information, &req_dep_ids, DDSI_TYPEID_KIND_MINIMAL);
    }
  }
#else
  bool ret = qos_match_p (gv, rdqos, wrqos, reason);
#endif
  for (int i = 0; i < 2; i++)
    ddsrt_mutex_unlock (locks[i + shift]);

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (req_type_id)
  {
    (void) ddsi_tl_request_type (gv, req_type_id, req_dep_ids, req_ndep_ids);
    if (req_dep_ids)
      ddsrt_free ((void *) req_dep_ids);
    return false;
  }
#endif

  return ret;
}

void connect_writer_with_proxy_reader_secure(struct writer *wr, struct proxy_reader *prd, ddsrt_mtime_t tnow, int64_t crypto_handle)
{
  DDSRT_UNUSED_ARG(tnow);
  proxy_reader_add_connection (prd, wr, crypto_handle);
  writer_add_connection (wr, prd, crypto_handle);
}

void connect_reader_with_proxy_writer_secure(struct reader *rd, struct proxy_writer *pwr, ddsrt_mtime_t tnow, int64_t crypto_handle)
{
  nn_count_t init_count;
  struct alive_state alive_state;

  /* Initialize the reader's tracking information for the writer liveliness state to something
     sensible, but that may be outdated by the time the reader gets added to the writer's list
     of matching readers. */
  proxy_writer_get_alive_state (pwr, &alive_state);
  reader_add_connection (rd, pwr, &init_count,  &alive_state, crypto_handle);
  proxy_writer_add_connection (pwr, rd, tnow, init_count, crypto_handle);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  proxy_writer_get_alive_state (pwr, &alive_state);
  reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
}

static void connect_writer_with_proxy_reader (struct writer *wr, struct proxy_reader *prd, ddsrt_mtime_t tnow)
{
  struct ddsi_domaingv *gv = wr->e.gv;
  const int isb0 = (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  const int isb1 = (is_builtin_entityid (prd->e.guid.entityid, prd->c.vendor) != 0);
  dds_qos_policy_id_t reason;
  int64_t crypto_handle;
  bool relay_only;

  DDSRT_UNUSED_ARG(tnow);
  if (isb0 != isb1)
    return;
  if (wr->e.onlylocal)
    return;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!isb0 && !topickind_qos_match_p_lock (gv, &prd->e, prd->c.xqos, &wr->e, wr->xqos, &reason, prd->c.type_pair, wr->c.type_pair))
#else
  if (!isb0 && !topickind_qos_match_p_lock (gv, &prd->e, prd->c.xqos, &wr->e, wr->xqos, &reason))
#endif
  {
    writer_qos_mismatch (wr, reason);
    return;
  }

  if (!q_omg_security_check_remote_reader_permissions (prd, wr->e.gv->config.domainId, wr->c.pp, &relay_only))
  {
    GVLOGDISC ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") not allowed by security\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else if (relay_only)
  {
    GVWARNING ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") relay_only not supported\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else if (!q_omg_security_match_remote_reader_enabled (wr, prd, relay_only, &crypto_handle))
  {
    GVLOGDISC ("connect_writer_with_proxy_reader (wr "PGUIDFMT") with (prd "PGUIDFMT") waiting for approval by security\n", PGUID (wr->e.guid), PGUID (prd->e.guid));
  }
  else
  {
    proxy_reader_add_connection (prd, wr, crypto_handle);
    writer_add_connection (wr, prd, crypto_handle);
  }
}

static void connect_proxy_writer_with_reader (struct proxy_writer *pwr, struct reader *rd, ddsrt_mtime_t tnow)
{
  const int isb0 = (is_builtin_entityid (pwr->e.guid.entityid, pwr->c.vendor) != 0);
  const int isb1 = (is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE) != 0);
  dds_qos_policy_id_t reason;
  nn_count_t init_count;
  struct alive_state alive_state;
  int64_t crypto_handle;

  if (isb0 != isb1)
    return;
  if (rd->e.onlylocal)
    return;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!isb0 && !topickind_qos_match_p_lock (rd->e.gv, &rd->e, rd->xqos, &pwr->e, pwr->c.xqos, &reason, rd->c.type_pair, pwr->c.type_pair))
#else
  if (!isb0 && !topickind_qos_match_p_lock (rd->e.gv, &rd->e, rd->xqos, &pwr->e, pwr->c.xqos, &reason))
#endif
  {
    reader_qos_mismatch (rd, reason);
    return;
  }

  if (!q_omg_security_check_remote_writer_permissions(pwr, rd->e.gv->config.domainId, rd->c.pp))
  {
    EELOGDISC (&rd->e, "connect_proxy_writer_with_reader (pwr "PGUIDFMT") with (rd "PGUIDFMT") not allowed by security\n",
        PGUID (pwr->e.guid), PGUID (rd->e.guid));
  }
  else if (!q_omg_security_match_remote_writer_enabled(rd, pwr, &crypto_handle))
  {
    EELOGDISC (&rd->e, "connect_proxy_writer_with_reader (pwr "PGUIDFMT") with  (rd "PGUIDFMT") waiting for approval by security\n",
        PGUID (pwr->e.guid), PGUID (rd->e.guid));
  }
  else
  {
    /* Initialize the reader's tracking information for the writer liveliness state to something
       sensible, but that may be outdated by the time the reader gets added to the writer's list
       of matching readers. */
    proxy_writer_get_alive_state (pwr, &alive_state);
    reader_add_connection (rd, pwr, &init_count, &alive_state, crypto_handle);
    proxy_writer_add_connection (pwr, rd, tnow, init_count, crypto_handle);

    /* Once everything is set up: update with the latest state, any updates to the alive state
       happening in parallel will cause this to become a no-op. */
    proxy_writer_get_alive_state (pwr, &alive_state);
    reader_update_notify_pwr_alive_state (rd, pwr, &alive_state);
  }
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

static void connect_writer_with_reader (struct writer *wr, struct reader *rd, ddsrt_mtime_t tnow)
{
  dds_qos_policy_id_t reason;
  struct alive_state alive_state;
  (void)tnow;
  if (!is_local_orphan_endpoint (&wr->e) && (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) || is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE)))
    return;
  if (ignore_local_p (&wr->e.guid, &rd->e.guid, wr->xqos, rd->xqos))
    return;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!topickind_qos_match_p_lock (wr->e.gv, &rd->e, rd->xqos, &wr->e, wr->xqos, &reason, rd->c.type_pair, wr->c.type_pair))
#else
  if (!topickind_qos_match_p_lock (wr->e.gv, &rd->e, rd->xqos, &wr->e, wr->xqos, &reason))
#endif
  {
    writer_qos_mismatch (wr, reason);
    reader_qos_mismatch (rd, reason);
    return;
  }
  /* Initialze the reader's tracking information for the writer liveliness state to something
     sensible, but that may be outdated by the time the reader gets added to the writer's list
     of matching readers. */
  writer_get_alive_state (wr, &alive_state);
  reader_add_local_connection (rd, wr, &alive_state);
  writer_add_local_connection (wr, rd);

  /* Once everything is set up: update with the latest state, any updates to the alive state
     happening in parallel will cause this to become a no-op. */
  writer_get_alive_state (wr, &alive_state);
  reader_update_notify_wr_alive_state (rd, wr, &alive_state);
}

static void connect_writer_with_proxy_reader_wrapper (struct entity_common *vwr, struct entity_common *vprd, ddsrt_mtime_t tnow)
{
  struct writer *wr = (struct writer *) vwr;
  struct proxy_reader *prd = (struct proxy_reader *) vprd;
  assert (wr->e.kind == EK_WRITER);
  assert (prd->e.kind == EK_PROXY_READER);
  connect_writer_with_proxy_reader (wr, prd, tnow);
}

static void connect_proxy_writer_with_reader_wrapper (struct entity_common *vpwr, struct entity_common *vrd, ddsrt_mtime_t tnow)
{
  struct proxy_writer *pwr = (struct proxy_writer *) vpwr;
  struct reader *rd = (struct reader *) vrd;
  assert (pwr->e.kind == EK_PROXY_WRITER);
  assert (rd->e.kind == EK_READER);
  connect_proxy_writer_with_reader (pwr, rd, tnow);
}

static void connect_writer_with_reader_wrapper (struct entity_common *vwr, struct entity_common *vrd, ddsrt_mtime_t tnow)
{
  struct writer *wr = (struct writer *) vwr;
  struct reader *rd = (struct reader *) vrd;
  assert (wr->e.kind == EK_WRITER);
  assert (rd->e.kind == EK_READER);
  connect_writer_with_reader (wr, rd, tnow);
}

static enum entity_kind generic_do_match_mkind (enum entity_kind kind, bool local)
{
  switch (kind)
  {
    case EK_WRITER: return local ? EK_READER : EK_PROXY_READER;
    case EK_READER: return local ? EK_WRITER : EK_PROXY_WRITER;
    case EK_PROXY_WRITER: assert (!local); return EK_READER;
    case EK_PROXY_READER: assert (!local); return EK_WRITER;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    case EK_TOPIC:
      assert(0);
      return EK_WRITER;
  }
  assert(0);
  return EK_WRITER;
}

static void generic_do_match_connect (struct entity_common *e, struct entity_common *em, ddsrt_mtime_t tnow, bool local)
{
  switch (e->kind)
  {
    case EK_WRITER:
      if (local)
        connect_writer_with_reader_wrapper (e, em, tnow);
      else
        connect_writer_with_proxy_reader_wrapper (e, em, tnow);
      break;
    case EK_READER:
      if (local)
        connect_writer_with_reader_wrapper (em, e, tnow);
      else
        connect_proxy_writer_with_reader_wrapper (em, e, tnow);
      break;
    case EK_PROXY_WRITER:
      assert (!local);
      connect_proxy_writer_with_reader_wrapper (e, em, tnow);
      break;
    case EK_PROXY_READER:
      assert (!local);
      connect_writer_with_proxy_reader_wrapper (em, e, tnow);
      break;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    case EK_TOPIC:
      assert(0);
      break;
  }
}

static const char *entity_topic_name (const struct entity_common *e)
{
  switch (e->kind)
  {
    case EK_WRITER:
      return ((const struct writer *) e)->xqos->topic_name;
    case EK_READER:
      return ((const struct reader *) e)->xqos->topic_name;
    case EK_PROXY_WRITER:
    case EK_PROXY_READER:
      return ((const struct generic_proxy_endpoint *) e)->c.xqos->topic_name;
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case EK_TOPIC:
    {
      struct topic * topic = (struct topic *) e;
      ddsrt_mutex_lock (&topic->e.qos_lock);
      const char * name = topic->definition->xqos->topic_name;
      ddsrt_mutex_unlock (&topic->e.qos_lock);
      return name;
    }
#endif
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    default:
      assert (0);
  }
  return "";
}

static void generic_do_match (struct entity_common *e, ddsrt_mtime_t tnow, bool local)
{
  static const struct { const char *full; const char *full_us; const char *abbrev; } kindstr[] = {
    [EK_WRITER] = { "writer", "writer", "wr" },
    [EK_READER] = { "reader", "reader", "rd" },
    [EK_PROXY_WRITER] = { "proxy writer", "proxy_writer", "pwr" },
    [EK_PROXY_READER] = { "proxy reader", "proxy_reader", "prd" },
    [EK_PARTICIPANT] = { "participant", "participant", "pp" },
    [EK_PROXY_PARTICIPANT] = { "proxy participant", "proxy_participant", "proxypp" }
  };

  enum entity_kind mkind = generic_do_match_mkind (e->kind, local);
  struct entity_index const * const entidx = e->gv->entity_index;
  struct entidx_enum it;
  struct entity_common *em;

  if (!is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE) || (local && is_local_orphan_endpoint (e)))
  {
    /* Non-builtins need matching on topics, the local orphan endpoints
       are a bit weird because they reuse the builtin entityids but
       otherwise need to be treated as normal readers */
    struct match_entities_range_key max;
    const char *tp = entity_topic_name (e);
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning all %ss%s%s\n",
               kindstr[e->kind].full_us, kindstr[mkind].full_us,
               kindstr[e->kind].abbrev, PGUID (e->guid),
               kindstr[mkind].abbrev,
               tp ? " of topic " : "", tp ? tp : "");
    /* Note: we visit at least all proxies that existed when we called
       init (with the -- possible -- exception of ones that were
       deleted between our calling init and our reaching it while
       enumerating), but we may visit a single proxy reader multiple
       times. */
    entidx_enum_init_topic (&it, entidx, mkind, tp, &max);
    while ((em = entidx_enum_next_max (&it, &max)) != NULL)
      generic_do_match_connect (e, em, tnow, local);
    entidx_enum_fini (&it);
  }
  else if (!local)
  {
    /* Built-ins have fixed QoS and a known entity id to use, so instead of
       looking for the right topic, just probe the matching GUIDs for all
       (proxy) participants.  Local matching never needs to look at the
       discovery endpoints */
    const ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
    const bool isproxy = (e->kind == EK_PROXY_WRITER || e->kind == EK_PROXY_READER || e->kind == EK_PROXY_PARTICIPANT);
    enum entity_kind pkind = isproxy ? EK_PARTICIPANT : EK_PROXY_PARTICIPANT;
    EELOGDISC (e, "match_%s_with_%ss(%s "PGUIDFMT") scanning %sparticipants tgt=%"PRIx32"\n",
               kindstr[e->kind].full_us, kindstr[mkind].full_us,
               kindstr[e->kind].abbrev, PGUID (e->guid),
               isproxy ? "" : "proxy ", tgt_ent.u);
    if (tgt_ent.u != NN_ENTITYID_UNKNOWN)
    {
      entidx_enum_init (&it, entidx, pkind);
      while ((em = entidx_enum_next (&it)) != NULL)
      {
        const ddsi_guid_t tgt_guid = { em->guid.prefix, tgt_ent };
        struct entity_common *ep;
        if ((ep = entidx_lookup_guid (entidx, &tgt_guid, mkind)) != NULL)
          generic_do_match_connect (e, ep, tnow, local);
      }
      entidx_enum_fini (&it);
    }
  }
}

static void match_writer_with_proxy_readers (struct writer *wr, ddsrt_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow, false);
}

static void match_writer_with_local_readers (struct writer *wr, ddsrt_mtime_t tnow)
{
  generic_do_match (&wr->e, tnow, true);
}

static void match_reader_with_proxy_writers (struct reader *rd, ddsrt_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow, false);
}

static void match_reader_with_local_writers (struct reader *rd, ddsrt_mtime_t tnow)
{
  generic_do_match (&rd->e, tnow, true);
}

static void match_proxy_writer_with_readers (struct proxy_writer *pwr, ddsrt_mtime_t tnow)
{
  generic_do_match (&pwr->e, tnow, false);
}

static void match_proxy_reader_with_writers (struct proxy_reader *prd, ddsrt_mtime_t tnow)
{
  generic_do_match(&prd->e, tnow, false);
}

#ifdef DDS_HAS_SECURITY

static void match_volatile_secure_endpoints (struct participant *pp, struct proxy_participant *proxypp)
{
  struct reader *rd;
  struct writer *wr;
  struct proxy_reader *prd;
  struct proxy_writer *pwr;
  ddsi_guid_t guid;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  EELOGDISC (&pp->e, "match volatile endpoints (pp "PGUIDFMT") with (proxypp "PGUIDFMT")\n",
             PGUID(pp->e.guid), PGUID(proxypp->e.guid));

  guid = pp->e.guid;
  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((rd = entidx_lookup_reader_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
  if ((wr = entidx_lookup_writer_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid = proxypp->e.guid;
  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER;
  if ((prd = entidx_lookup_proxy_reader_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  guid.entityid.u = NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER;
  if ((pwr = entidx_lookup_proxy_writer_guid (pp->e.gv->entity_index, &guid)) == NULL)
    return;

  connect_proxy_writer_with_reader_wrapper(&pwr->e, &rd->e, tnow);
  connect_writer_with_proxy_reader_wrapper(&wr->e, &prd->e, tnow);
}

static struct entity_common * get_entity_parent(struct entity_common *e)
{
  switch (e->kind)
  {
#ifdef DDS_HAS_TOPIC_DISCOVERY
    case EK_TOPIC:
      return &((struct topic *)e)->pp->e;
#endif
    case EK_WRITER:
      return &((struct writer *)e)->c.pp->e;
    case EK_READER:
      return &((struct reader *)e)->c.pp->e;
    case EK_PROXY_WRITER:
      return &((struct proxy_writer *)e)->c.proxypp->e;
    case EK_PROXY_READER:
      return &((struct proxy_reader *)e)->c.proxypp->e;
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
    default:
      return NULL;
  }
  return NULL;
}

static void update_proxy_participant_endpoint_matching (struct proxy_participant *proxypp, struct participant *pp)
{
  struct entity_index * const entidx = pp->e.gv->entity_index;
  struct proxy_endpoint_common *cep;
  ddsi_guid_t guid;
  ddsi_entityid_t *endpoint_ids;
  uint32_t num = 0, i;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  EELOGDISC (&proxypp->e, "update_proxy_participant_endpoint_matching (proxypp "PGUIDFMT" pp "PGUIDFMT")\n",
             PGUID (proxypp->e.guid), PGUID (pp->e.guid));

  ddsrt_mutex_lock(&proxypp->e.lock);
  endpoint_ids = ddsrt_malloc(proxypp->refc * sizeof(ddsi_entityid_t));
  for (cep = proxypp->endpoints; cep != NULL; cep = cep->next_ep)
  {
    struct entity_common *e = entity_common_from_proxy_endpoint_common (cep);
    endpoint_ids[num++] = e->guid.entityid;
  }
  ddsrt_mutex_unlock(&proxypp->e.lock);

  guid.prefix = proxypp->e.guid.prefix;

  for (i = 0; i < num; i++)
  {
    struct entity_common *e;
    enum entity_kind mkind;

    guid.entityid = endpoint_ids[i];
    if ((e = entidx_lookup_guid_untyped(entidx, &guid)) == NULL)
      continue;

    mkind = generic_do_match_mkind (e->kind, false);
    if (!is_builtin_entityid (e->guid.entityid, NN_VENDORID_ECLIPSE))
    {
      struct entidx_enum it;
      struct entity_common *em;
      struct match_entities_range_key max;
      const char *tp = entity_topic_name (e);

      entidx_enum_init_topic(&it, entidx, mkind, tp, &max);
      while ((em = entidx_enum_next_max (&it, &max)) != NULL)
      {
        if (&pp->e == get_entity_parent(em))
          generic_do_match_connect (e, em, tnow, false);
      }
      entidx_enum_fini (&it);
    }
    else
    {
      const ddsi_entityid_t tgt_ent = builtin_entityid_match (e->guid.entityid);
      const ddsi_guid_t tgt_guid = { pp->e.guid.prefix, tgt_ent };

      if (!is_builtin_volatile_endpoint (tgt_ent))
      {
        struct entity_common *ep;
        if ((ep = entidx_lookup_guid (entidx, &tgt_guid, mkind)) != NULL)
          generic_do_match_connect (e, ep, tnow, false);
      }
    }
  }

  ddsrt_free(endpoint_ids);
}

#endif /* DDS_HAS_SECURITY */

void update_proxy_endpoint_matching (const struct ddsi_domaingv *gv, struct generic_proxy_endpoint *proxy_ep)
{
  GVLOGDISC ("update_proxy_endpoint_matching (proxy ep "PGUIDFMT")\n", PGUID (proxy_ep->e.guid));
  enum entity_kind mkind = generic_do_match_mkind (proxy_ep->e.kind, false);
  assert (!is_builtin_entityid (proxy_ep->e.guid.entityid, NN_VENDORID_ECLIPSE));
  struct entidx_enum it;
  struct entity_common *em;
  struct match_entities_range_key max;
  const char *tp = entity_topic_name (&proxy_ep->e);
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  thread_state_awake (lookup_thread_state (), gv);
  entidx_enum_init_topic (&it, gv->entity_index, mkind, tp, &max);
  while ((em = entidx_enum_next_max (&it, &max)) != NULL)
  {
    GVLOGDISC ("match proxy ep "PGUIDFMT" with "PGUIDFMT"\n", PGUID (proxy_ep->e.guid), PGUID (em->guid));
    generic_do_match_connect (&proxy_ep->e, em, tnow, false);
  }
  entidx_enum_fini (&it);
  thread_state_asleep (lookup_thread_state ());
}


/* ENDPOINT --------------------------------------------------------- */

static void new_reader_writer_common (const struct ddsrt_log_cfg *logcfg, const struct ddsi_guid *guid, const char *topic_name, const char *type_name, const struct dds_qos *xqos)
{
  const char *partition = "(default)";
  const char *partition_suffix = "";
  assert (topic_name != NULL);
  assert (type_name != NULL);
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
            topic_name,
            type_name);
}

static bool is_onlylocal_endpoint (struct participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos)
{
  if (builtintopic_is_builtintopic (pp->e.gv->builtin_topic_interface, type))
    return true;

#ifdef DDS_HAS_NETWORK_PARTITIONS
  char *ps_def = "";
  char **ps;
  uint32_t nps;
  if ((xqos->present & QP_PARTITION) && xqos->partition.n > 0) {
    ps = xqos->partition.strs;
    nps = xqos->partition.n;
  } else {
    ps = &ps_def;
    nps = 1;
  }
  for (uint32_t i = 0; i < nps; i++)
  {
    if (is_ignored_partition (&pp->e.gv->config, ps[i], topic_name))
      return true;
  }
#endif

  return false;
}

static void endpoint_common_init (struct entity_common *e, struct endpoint_common *c, struct ddsi_domaingv *gv, enum entity_kind kind, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, bool onlylocal, const struct ddsi_sertype *sertype)
{
#ifndef DDS_HAS_TYPE_DISCOVERY
  DDSRT_UNUSED_ARG (sertype);
#endif
  entity_common_init (e, gv, guid, NULL, kind, ddsrt_time_wallclock (), NN_VENDORID_ECLIPSE, pp->e.onlylocal || onlylocal);
  c->pp = ref_participant (pp, &e->guid);
  if (group_guid)
    c->group_guid = *group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));

#ifdef DDS_HAS_TYPE_DISCOVERY
  c->type_pair = ddsrt_malloc (sizeof (*c->type_pair));
  c->type_pair->minimal = ddsi_type_ref_local (pp->e.gv, sertype, DDSI_TYPEID_KIND_MINIMAL);
  c->type_pair->complete = ddsi_type_ref_local (pp->e.gv, sertype, DDSI_TYPEID_KIND_COMPLETE);
#endif
}

static void endpoint_common_fini (struct entity_common *e, struct endpoint_common *c)
{
  if (!is_builtin_entityid(e->guid.entityid, NN_VENDORID_ECLIPSE))
    pp_release_entityid(c->pp, e->guid.entityid);
  if (c->pp)
  {
    unref_participant (c->pp, &e->guid);
#ifdef DDS_HAS_TYPE_DISCOVERY
    if (c->type_pair)
    {
      ddsi_type_unref (e->gv, c->type_pair->minimal);
      ddsi_type_unref (e->gv, c->type_pair->complete);
      ddsrt_free (c->type_pair);
    }
#endif
  }
  else
  {
    /* only for the (almost pseudo) writers used for generating the built-in topics */
    assert (is_local_orphan_endpoint (e));
  }
  entity_common_fini (e);
}

static int set_topic_type_name (dds_qos_t *xqos, const char * topic_name, const char * type_name)
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

/* WRITER ----------------------------------------------------------- */

#ifdef DDS_HAS_NETWORK_PARTITIONS
static const struct ddsi_config_networkpartition_listelem *get_partition_from_mapping (const struct ddsrt_log_cfg *logcfg, const struct ddsi_config *config, const char *partition, const char *topic)
{
  struct ddsi_config_partitionmapping_listelem *pm;
  if ((pm = find_partitionmapping (config, partition, topic)) == NULL)
    return 0;
  else
  {
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "matched writer for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n", topic, partition, pm->networkPartition);
    return pm->partition;
  }
}
#endif /* DDS_HAS_NETWORK_PARTITIONS */

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
  else if (n->is_reliable && (n->seq == MAX_SEQ_NUMBER || n->seq == 0 || !n->has_replied_to_hb))
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
  if (ddsrt_avl_is_empty (&wr->readers))
  {
    /* Can't transmit a valid heartbeat if there is no data; and it
       wouldn't actually be sent anywhere if there are no readers, so
       there is little point in processing the xevent all the time.

       Note that add_msg_to_whc and add_proxy_reader_to_writer will
       perform a reschedule.  Since DDSI 2.3, we can send valid
       heartbeats in the absence of data. */
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
  wr->t_rexmit_start = ddsrt_time_elapsed();
  if (wr->e.gv->config.whc_adaptive && wr->whc_high > wr->whc_low)
  {
    uint32_t m = 8 * wr->whc_high / 10;
    wr->whc_high = (m > wr->whc_low) ? m : wr->whc_low;
  }
}

void writer_clear_retransmitting (struct writer *wr)
{
  wr->retransmitting = 0;
  wr->t_whc_high_upd = wr->t_rexmit_end = ddsrt_time_elapsed();
  wr->time_retransmit += (uint64_t) (wr->t_rexmit_end.v - wr->t_rexmit_start.v);
  ddsrt_cond_broadcast (&wr->throttle_cond);
}

unsigned remove_acked_messages (struct writer *wr, struct whc_state *whcst, struct whc_node **deferred_free_list)
{
  unsigned n;
  assert (wr->e.guid.entityid.u != NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  ASSERT_MUTEX_HELD (&wr->e.lock);
  n = whc_remove_acked_messages (wr->whc, writer_max_drop_seq (wr), whcst, deferred_free_list);
  /* trigger anyone waiting in throttle_writer() or wait_for_acks() */
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

static void writer_notify_liveliness_change_may_unlock (struct writer *wr)
{
  struct alive_state alive_state;
  writer_get_alive_state_locked (wr, &alive_state);

  struct ddsi_guid rdguid;
  struct pwr_rd_match *m;
  memset (&rdguid, 0, sizeof (rdguid));
  while (wr->alive_vclock == alive_state.vclock &&
         (m = ddsrt_avl_lookup_succ (&wr_local_readers_treedef, &wr->local_readers, &rdguid)) != NULL)
  {
    rdguid = m->rd_guid;
    ddsrt_mutex_unlock (&wr->e.lock);
    /* unlocking pwr means alive state may have changed already; we break out of the loop once we
       detect this but there for the reader in the current iteration, anything is possible */
    reader_update_notify_wr_alive_state_guid (&rdguid, wr, &alive_state);
    ddsrt_mutex_lock (&wr->e.lock);
  }
}

void writer_set_alive_may_unlock (struct writer *wr, bool notify)
{
  /* Caller has wr->e.lock, so we can safely read wr->alive.  Updating wr->alive requires
     also taking wr->c.pp->e.lock because wr->alive <=> (wr->lease in pp's lease heap). */
  assert (!wr->alive);

  /* check that writer still exists (when deleting it is removed from guid hash) */
  if (entidx_lookup_writer_guid (wr->e.gv->entity_index, &wr->e.guid) == NULL)
  {
    ELOGDISC (wr, "writer_set_alive_may_unlock("PGUIDFMT") - not in entity index, wr deleting\n", PGUID (wr->e.guid));
    return;
  }

  ddsrt_mutex_lock (&wr->c.pp->e.lock);
  wr->alive = true;
  wr->alive_vclock++;
  if (wr->xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
      participant_add_wr_lease_locked (wr->c.pp, wr);
    else if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
      lease_set_expiry (wr->lease, ddsrt_etime_add_duration (ddsrt_time_elapsed (), wr->lease->tdur));
  }
  ddsrt_mutex_unlock (&wr->c.pp->e.lock);

  if (notify)
    writer_notify_liveliness_change_may_unlock (wr);
}

static int writer_set_notalive_locked (struct writer *wr, bool notify)
{
  if (!wr->alive)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  /* To update wr->alive, both wr->e.lock and wr->c.pp->e.lock
     should be taken */
  ddsrt_mutex_lock (&wr->c.pp->e.lock);
  wr->alive = false;
  wr->alive_vclock++;
  if (wr->xqos->liveliness.lease_duration != DDS_INFINITY && wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
    participant_remove_wr_lease_locked (wr->c.pp, wr);
  ddsrt_mutex_unlock (&wr->c.pp->e.lock);

  if (notify)
  {
    if (wr->status_cb)
    {
      status_cb_data_t data;
      data.handle = wr->e.iid;
      data.raw_status_id = (int) DDS_LIVELINESS_LOST_STATUS_ID;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    writer_notify_liveliness_change_may_unlock (wr);
  }
  return DDS_RETCODE_OK;
}

int writer_set_notalive (struct writer *wr, bool notify)
{
  ddsrt_mutex_lock (&wr->e.lock);
  int ret = writer_set_notalive_locked(wr, notify);
  ddsrt_mutex_unlock (&wr->e.lock);
  return ret;
}

static void new_writer_guid_common_init (struct writer *wr, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct whc *whc, status_cb_t status_cb, void * status_entity)
{
  ddsrt_cond_init (&wr->throttle_cond);
  wr->seq = 0;
  wr->cs_seq = 0;
  ddsrt_atomic_st64 (&wr->seq_xmit, (uint64_t) 0);
  wr->hbcount = 1;
  wr->state = WRST_OPERATIONAL;
  wr->hbfragcount = 1;
  writer_hbcontrol_init (&wr->hbcontrol);
  wr->throttling = 0;
  wr->retransmitting = 0;
  wr->t_rexmit_end.v = 0;
  wr->t_rexmit_start.v = 0;
  wr->t_whc_high_upd.v = 0;
  wr->num_readers = 0;
  wr->num_reliable_readers = 0;
  wr->num_readers_requesting_keyhash = 0;
  wr->num_acks_received = 0;
  wr->num_nacks_received = 0;
  wr->throttle_count = 0;
  wr->throttle_tracing = 0;
  wr->rexmit_count = 0;
  wr->rexmit_lost_count = 0;
  wr->rexmit_bytes = 0;
  wr->time_throttled = 0;
  wr->time_retransmit = 0;
  wr->force_md5_keyhash = 0;
  wr->alive = 1;
  wr->test_ignore_acknack = 0;
  wr->test_suppress_retransmit = 0;
  wr->test_suppress_heartbeat = 0;
  wr->test_drop_outgoing_data = 0;
  wr->alive_vclock = 0;
  wr->init_burst_size_limit = UINT32_MAX - UINT16_MAX;
  wr->rexmit_burst_size_limit = UINT32_MAX - UINT16_MAX;

  wr->status_cb = status_cb;
  wr->status_cb_entity = status_entity;
#ifdef DDS_HAS_SECURITY
  wr->sec_attr = NULL;
#endif

  /* Copy QoS, merging in defaults */

  wr->xqos = ddsrt_malloc (sizeof (*wr->xqos));
  ddsi_xqos_copy (wr->xqos, xqos);
  ddsi_xqos_mergein_missing (wr->xqos, &ddsi_default_qos_writer, ~(uint64_t)0);
  assert (wr->xqos->aliased == 0);
  set_topic_type_name (wr->xqos, topic_name, type->type_name);

  ELOGDISC (wr, "WRITER "PGUIDFMT" QOS={", PGUID (wr->e.guid));
  ddsi_xqos_log (DDS_LC_DISCOVERY, &wr->e.gv->logconfig, wr->xqos);
  ELOGDISC (wr, "}\n");

  assert (wr->xqos->present & QP_RELIABILITY);
  wr->reliable = (wr->xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  assert (wr->xqos->present & QP_DURABILITY);
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) &&
      wr->e.guid.entityid.u != NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER
      && wr->e.guid.entityid.u != NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER
      && wr->e.guid.entityid.u != NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER)
#else
  if (is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE) &&
      wr->e.guid.entityid.u != NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)
#endif
  {
    assert (wr->xqos->history.kind == DDS_HISTORY_KEEP_LAST);
    assert ((wr->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL) ||
            (wr->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER));
  }
  wr->handle_as_transient_local = (wr->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL);
  wr->num_readers_requesting_keyhash +=
    wr->e.gv->config.generate_keyhash &&
    ((wr->e.guid.entityid.u & NN_ENTITYID_KIND_MASK) == NN_ENTITYID_KIND_WRITER_WITH_KEY);
  wr->type = ddsi_sertype_ref (type);
  wr->as = new_addrset ();
  wr->as_group = NULL;

#ifdef DDS_HAS_NETWORK_PARTITIONS
  /* This is an open issue how to encrypt mesages send for various
     partitions that match multiple network partitions.  From a safety
     point of view a wierd configuration. Here we chose the first one
     that we find */
  {
    char *ps_def = "";
    char **ps = (wr->xqos->partition.n > 0) ? wr->xqos->partition.strs : &ps_def;
    uint32_t nps = (wr->xqos->partition.n > 0) ? wr->xqos->partition.n : 1;
    wr->network_partition = NULL;
    for (uint32_t i = 0; i < nps && wr->network_partition == NULL; i++)
      wr->network_partition = get_partition_from_mapping (&wr->e.gv->logconfig, &wr->e.gv->config, ps[i], wr->xqos->topic_name);
  }
#endif /* DDS_HAS_NETWORK_PARTITIONS */

#ifdef DDS_HAS_SSM
  /* Writer supports SSM if it is mapped to a network partition for
     which the address set includes an SSM address.  If it supports
     SSM, it arbitrarily selects one SSM address from the address set
     to advertise. */
  wr->supports_ssm = 0;
  wr->ssm_as = NULL;
  if (wr->e.gv->config.allowMulticast & DDSI_AMC_SSM)
  {
    ddsi_xlocator_t loc;
    int have_loc = 0;
    if (wr->network_partition == NULL)
    {
      if (ddsi_is_ssm_mcaddr (wr->e.gv, &wr->e.gv->loc_default_mc))
      {
        loc.conn = wr->e.gv->xmit_conns[0]; // FIXME: hack
        loc.c = wr->e.gv->loc_default_mc;
        have_loc = 1;
      }
    }
    else
    {
      if (wr->network_partition->ssm_addresses)
      {
        assert (ddsi_is_ssm_mcaddr (wr->e.gv, &wr->network_partition->ssm_addresses->loc));
        loc.conn = wr->e.gv->xmit_conns[0]; // FIXME: hack
        loc.c = wr->network_partition->ssm_addresses->loc;
        have_loc = 1;
      }
    }
    if (have_loc)
    {
      wr->supports_ssm = 1;
      wr->ssm_as = new_addrset ();
      add_xlocator_to_addrset (wr->e.gv, wr->ssm_as, &loc);
      ELOGDISC (wr, "writer "PGUIDFMT": ssm=%d", PGUID (wr->e.guid), wr->supports_ssm);
      nn_log_addrset (wr->e.gv, DDS_LC_DISCOVERY, "", wr->ssm_as);
      ELOGDISC (wr, "\n");
    }
  }
#endif

  /* for non-builtin writers, select the eventqueue based on the channel it is mapped to */

#ifdef DDS_HAS_NETWORK_CHANNELS
  if (!is_builtin_entityid (wr->e.guid.entityid, ownvendorid))
  {
    struct ddsi_config_channel_listelem *channel = find_channel (&wr->e.gv->config, wr->xqos->transport_priority);
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
     writer for it in the hash table. NEVER => won't ever be
     scheduled, and this can only change by writing data, which won't
     happen until after it becomes visible. */
  if (wr->reliable)
    wr->heartbeat_xevent = qxev_heartbeat (wr->evq, DDSRT_MTIME_NEVER, &wr->e.guid);
  else
    wr->heartbeat_xevent = NULL;

  assert (wr->xqos->present & QP_LIVELINESS);
  if (wr->xqos->liveliness.lease_duration != DDS_INFINITY)
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
  assert (!(is_builtin_entityid(wr->e.guid.entityid, NN_VENDORID_ECLIPSE) && !is_builtin_volatile_endpoint(wr->e.guid.entityid)) ||
           (wr->whc_low == wr->whc_high && wr->whc_low == INT32_MAX));

  /* Connection admin */
  ddsrt_avl_init (&wr_readers_treedef, &wr->readers);
  ddsrt_avl_init (&wr_local_readers_treedef, &wr->local_readers);

  local_reader_ary_init (&wr->rdary);
}

static dds_return_t new_writer_guid (struct writer **wr_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct whc *whc, status_cb_t status_cb, void *status_entity)
{
  struct writer *wr;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  assert (is_writer_entityid (guid->entityid));
  assert (entidx_lookup_writer_guid (pp->e.gv->entity_index, guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (&pp->e.gv->logconfig, guid, topic_name, type->type_name, xqos);
  wr = ddsrt_malloc (sizeof (*wr));
  if (wr_out)
    *wr_out = wr;

  /* want a pointer to the participant so that a parallel call to
   delete_participant won't interfere with our ability to address
   the participant */

  const bool onlylocal = is_onlylocal_endpoint (pp, topic_name, type, xqos);
  endpoint_common_init (&wr->e, &wr->c, pp->e.gv, EK_WRITER, guid, group_guid, pp, onlylocal, type);
  new_writer_guid_common_init(wr, topic_name, type, xqos, whc, status_cb, status_entity);

#ifdef DDS_HAS_SECURITY
  q_omg_security_register_writer(wr);
#endif

  /* entity_index needed for protocol handling, so add it before we send
   out our first message.  Also: needed for matching, and swapping
   the order if hash insert & matching creates a window during which
   neither of two endpoints being created in parallel can discover
   the other. */
  ddsrt_mutex_lock (&wr->e.lock);
  entidx_insert_writer_guid (pp->e.gv->entity_index, wr);
  builtintopic_write_endpoint (wr->e.gv->builtin_topic_interface, &wr->e, ddsrt_time_wallclock(), true);
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
    assert (wr->lease_duration->ldur != DDS_INFINITY);
    assert (!is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE));
    if (wr->xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC)
    {
      /* Store writer lease duration in participant's heap in case of automatic liveliness */
      ddsrt_mutex_lock (&pp->e.lock);
      ddsrt_fibheap_insert (&ldur_fhdef, &pp->ldur_auto_wr, wr->lease_duration);
      ddsrt_mutex_unlock (&pp->e.lock);

      /* Trigger pmd update */
      (void) resched_xevent_if_earlier (pp->pmd_update_xevent, ddsrt_time_monotonic ());
    }
    else
    {
      ddsrt_etime_t texpire = ddsrt_etime_add_duration (ddsrt_time_elapsed (), wr->lease_duration->ldur);
      wr->lease = lease_new (texpire, wr->lease_duration->ldur, &wr->e);
      if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
      {
        ddsrt_mutex_lock (&pp->e.lock);
        participant_add_wr_lease_locked (pp, wr);
        ddsrt_mutex_unlock (&pp->e.lock);
      }
      else
      {
        lease_register (wr->lease);
      }
    }
  }
  else
  {
    wr->lease = NULL;
  }

  return 0;
}

dds_return_t new_writer (struct writer **wr_out, struct ddsi_guid *wrguid, const struct ddsi_guid *group_guid, struct participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct whc * whc, status_cb_t status_cb, void *status_cb_arg)
{
  dds_return_t rc;
  uint32_t kind;

  /* participant can't be freed while we're mucking around cos we are
     awake and do not touch the thread's vtime (entidx_lookup already
     verifies we're awake) */
  wrguid->prefix = pp->e.guid.prefix;
  kind = type->typekind_no_key ? NN_ENTITYID_KIND_WRITER_NO_KEY : NN_ENTITYID_KIND_WRITER_WITH_KEY;
  if ((rc = pp_allocate_entityid (&wrguid->entityid, kind, pp)) < 0)
    return rc;
  return new_writer_guid (wr_out, wrguid, group_guid, pp, topic_name, type, xqos, whc, status_cb, status_cb_arg);
}

struct local_orphan_writer *new_local_orphan_writer (struct ddsi_domaingv *gv, ddsi_entityid_t entityid, const char *topic_name, struct ddsi_sertype *type, const struct dds_qos *xqos, struct whc *whc)
{
  ddsi_guid_t guid;
  struct local_orphan_writer *lowr;
  struct writer *wr;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  GVLOGDISC ("new_local_orphan_writer(%s/%s)\n", topic_name, type->type_name);
  lowr = ddsrt_malloc (sizeof (*lowr));
  wr = &lowr->wr;

  memset (&guid.prefix, 0, sizeof (guid.prefix));
  guid.entityid = entityid;
  entity_common_init (&wr->e, gv, &guid, NULL, EK_WRITER, ddsrt_time_wallclock (), NN_VENDORID_ECLIPSE, true);
  wr->c.pp = NULL;
  memset (&wr->c.group_guid, 0, sizeof (wr->c.group_guid));

#ifdef DDS_HAS_TYPE_DISCOVERY
  wr->c.type_pair = NULL;
#endif

  new_writer_guid_common_init (wr, topic_name, type, xqos, whc, 0, NULL);
  entidx_insert_writer_guid (gv->entity_index, wr);
  builtintopic_write_endpoint (gv->builtin_topic_interface, &wr->e, ddsrt_time_wallclock(), true);
  match_writer_with_local_readers (wr, tnow);
  return lowr;
}

void update_writer_qos (struct writer *wr, const dds_qos_t *xqos)
{
  ddsrt_mutex_lock (&wr->e.lock);
  if (update_qos_locked (&wr->e, wr->xqos, xqos, ddsrt_time_wallclock ()))
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
    wr->hbcontrol.tsched = DDSRT_MTIME_NEVER;
    delete_xevent (wr->heartbeat_xevent);
  }

  /* Tear down connections -- no proxy reader can be adding/removing
      us now, because we can't be found via entity_index anymore.  We
      therefore need not take lock. */

  while (!ddsrt_avl_is_empty (&wr->readers))
  {
    struct wr_prd_match *m = ddsrt_avl_root_non_empty (&wr_readers_treedef, &wr->readers);
    ddsrt_avl_delete (&wr_readers_treedef, &wr->readers, m);
    proxy_reader_drop_connection (&m->prd_guid, wr);
    free_wr_prd_match (wr->e.gv, &wr->e.guid, m);
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
    if (wr->xqos->liveliness.kind != DDS_LIVELINESS_AUTOMATIC)
      lease_free (wr->lease);
  }

  /* Do last gasp on SEDP and free writer. */
  if (!is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE))
    sedp_dispose_unregister_writer (wr);
  whc_free (wr->whc);
  if (wr->status_cb)
    (wr->status_cb) (wr->status_cb_entity, NULL);

#ifdef DDS_HAS_SECURITY
  q_omg_security_deregister_writer(wr);
#endif
#ifdef DDS_HAS_SSM
  if (wr->ssm_as)
    unref_addrset (wr->ssm_as);
#endif
  unref_addrset (wr->as); /* must remain until readers gone (rebuilding of addrset) */
  ddsi_xqos_fini (wr->xqos);
  ddsrt_free (wr->xqos);
  local_reader_ary_fini (&wr->rdary);
  ddsrt_cond_destroy (&wr->throttle_cond);

  ddsi_sertype_unref ((struct ddsi_sertype *) wr->type);
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

dds_return_t unblock_throttled_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct writer *wr;
  assert (is_writer_entityid (guid->entityid));
  if ((wr = entidx_lookup_writer_guid (gv->entity_index, guid)) == NULL)
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

dds_return_t writer_wait_for_acks (struct writer *wr, const ddsi_guid_t *rdguid, dds_time_t abstimeout)
{
  dds_return_t rc;
  seqno_t ref_seq;
  ddsrt_mutex_lock (&wr->e.lock);
  ref_seq = wr->seq;
  if (rdguid == NULL)
  {
    while (wr->state == WRST_OPERATIONAL && ref_seq > writer_max_drop_seq (wr))
      if (!ddsrt_cond_waituntil (&wr->throttle_cond, &wr->e.lock, abstimeout))
        break;
    rc = (ref_seq <= writer_max_drop_seq (wr)) ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
  }
  else
  {
    struct wr_prd_match *m = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, rdguid);
    while (wr->state == WRST_OPERATIONAL && m && ref_seq > m->seq)
    {
      if (!ddsrt_cond_waituntil (&wr->throttle_cond, &wr->e.lock, abstimeout))
        break;
      m = ddsrt_avl_lookup (&wr_readers_treedef, &wr->readers, rdguid);
    }
    rc = (m == NULL || ref_seq <= m->seq) ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  return rc;
}

dds_return_t delete_writer_nolinger_locked (struct writer *wr)
{
  ASSERT_MUTEX_HELD (&wr->e.lock);

  /* We can get here via multiple paths in parallel, in particular: because all data got
     ACK'd while lingering, and because the linger timeout elapses.  Those two race each
     other, the first calling this function directly, the second calling from
     handle_xevk_delete_writer via delete_writer_nolinger.

     There are two practical options to decide whether to ignore the call: one is to check
     whether the writer is still in the GUID hashes, the second to check whether the state
     is WRST_DELETING.  The latter seems a bit less surprising. */
  if (wr->state == WRST_DELETING)
  {
    ELOGDISC (wr, "delete_writer_nolinger(guid "PGUIDFMT") already done\n", PGUID (wr->e.guid));
    return 0;
  }

  ELOGDISC (wr, "delete_writer_nolinger(guid "PGUIDFMT") ...\n", PGUID (wr->e.guid));
  builtintopic_write_endpoint (wr->e.gv->builtin_topic_interface, &wr->e, ddsrt_time_wallclock(), false);
  local_reader_ary_setinvalid (&wr->rdary);
  entidx_remove_writer_guid (wr->e.gv->entity_index, wr);
  writer_set_state (wr, WRST_DELETING);
  if (wr->lease_duration != NULL) {
    wr->lease_duration->ldur = DDS_DURATION_INVALID;
    if (wr->xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC)
    {
      ddsrt_mutex_lock (&wr->c.pp->e.lock);
      ddsrt_fibheap_delete (&ldur_fhdef, &wr->c.pp->ldur_auto_wr, wr->lease_duration);
      ddsrt_mutex_unlock (&wr->c.pp->e.lock);
      resched_xevent_if_earlier (wr->c.pp->pmd_update_xevent, ddsrt_time_monotonic ());
    }
    else
    {
      if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
        lease_unregister (wr->lease);
      if (writer_set_notalive_locked (wr, false) != DDS_RETCODE_OK)
        ELOGDISC (wr, "writer_set_notalive failed for "PGUIDFMT"\n", PGUID (wr->e.guid));
    }
  }
  gcreq_writer (wr);
  return 0;
}

dds_return_t delete_writer_nolinger (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct writer *wr;
  /* We take no care to ensure application writers are not deleted
     while they still have unacknowledged data (unless it takes too
     long), but we don't care about the DDSI built-in writers: we deal
     with that anyway because of the potential for crashes of remote
     DDSI participants. But it would be somewhat more elegant to do it
     differently. */
  assert (is_writer_entityid (guid->entityid));
  if ((wr = entidx_lookup_writer_guid (gv->entity_index, guid)) == NULL)
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

dds_return_t delete_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct writer *wr;
  struct whc_state whcst;
  if ((wr = entidx_lookup_writer_guid (gv->entity_index, guid)) == NULL)
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
    ddsrt_mtime_t tsched;
    int32_t tsec, tusec;
    writer_set_state (wr, WRST_LINGERING);
    ddsrt_mutex_unlock (&wr->e.lock);
    tsched = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), wr->e.gv->config.writer_linger_duration);
    ddsrt_mtime_to_sec_usec (&tsec, &tusec, tsched);
    GVLOGDISC ("delete_writer(guid "PGUIDFMT") - unack'ed samples, will delete when ack'd or at t = %"PRId32".%06"PRId32"\n",
               PGUID (*guid), tsec, tusec);
    qxev_delete_writer (gv->xevents, tsched, &wr->e.guid);
  }
  return 0;
}

/* READER ----------------------------------------------------------- */

#ifdef DDS_HAS_NETWORK_PARTITIONS
static const struct ddsi_config_networkpartition_listelem *get_as_from_mapping (const struct ddsi_domaingv *gv, const char *partition, const char *topic)
{
  struct ddsi_config_partitionmapping_listelem *pm;
  if ((pm = find_partitionmapping (&gv->config, partition, topic)) != NULL)
  {
    GVLOGDISC ("matched reader for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n",
               topic, partition, pm->networkPartition);
    return pm->partition;
  }
  return NULL;
}

static void joinleave_mcast_helper (struct ddsi_domaingv *gv, ddsi_tran_conn_t conn, const ddsi_locator_t *n, const char *joinleavestr, int (*joinleave) (const struct ddsi_domaingv *gv, struct nn_group_membership *mship, ddsi_tran_conn_t conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc))
{
  char buf[DDSI_LOCSTRLEN];
  assert (ddsi_is_mcaddr (gv, n));
  if (n->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
  {
    if (joinleave (gv, gv->mship, conn, NULL, n) < 0)
      GVWARNING ("failed to %s network partition multicast group %s\n", joinleavestr, ddsi_locator_to_string (buf, sizeof (buf), n));
  }
  else /* join all addresses that include this node */
  {
    ddsi_locator_t l = *n;
    nn_udpv4mcgen_address_t l1;
    uint32_t iph;
    memcpy (&l1, l.address, sizeof (l1));
    l.kind = NN_LOCATOR_KIND_UDPv4;
    memset (l.address, 0, 12);
    iph = ntohl (l1.ipv4.s_addr);
    for (uint32_t i = 1; i < ((uint32_t)1 << l1.count); i++)
    {
      uint32_t ipn, iph1 = iph;
      if (i & (1u << l1.idx))
      {
        iph1 |= (i << l1.base);
        ipn = htonl (iph1);
        memcpy (l.address + 12, &ipn, 4);
        if (joinleave (gv, gv->mship, conn, NULL, &l) < 0)
          GVWARNING ("failed to %s network partition multicast group %s\n", joinleavestr, ddsi_locator_to_string (buf, sizeof (buf), &l));
      }
    }
  }
}

static void join_mcast_helper (struct ddsi_domaingv *gv, ddsi_tran_conn_t conn, const ddsi_locator_t *n)
{
  joinleave_mcast_helper (gv, conn, n, "join", ddsi_join_mc);
}

static void leave_mcast_helper (struct ddsi_domaingv *gv, ddsi_tran_conn_t conn, const ddsi_locator_t *n)
{
  joinleave_mcast_helper (gv, conn, n, "leave", ddsi_leave_mc);
}
#endif /* DDS_HAS_NETWORK_PARTITIONS */

static dds_return_t new_reader_guid
(
  struct reader **rd_out,
  const struct ddsi_guid *guid,
  const struct ddsi_guid *group_guid,
  struct participant *pp,
  const char *topic_name,
  const struct ddsi_sertype *type,
  const struct dds_qos *xqos,
  struct ddsi_rhc *rhc,
  status_cb_t status_cb,
  void * status_entity
)
{
  /* see new_writer_guid for commenets */

  struct reader *rd;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  assert (!is_writer_entityid (guid->entityid));
  assert (entidx_lookup_reader_guid (pp->e.gv->entity_index, guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (&pp->e.gv->logconfig, guid, topic_name, type->type_name, xqos);
  rd = ddsrt_malloc (sizeof (*rd));
  if (rd_out)
    *rd_out = rd;

  const bool onlylocal = is_onlylocal_endpoint (pp, topic_name, type, xqos);
  endpoint_common_init (&rd->e, &rd->c, pp->e.gv, EK_READER, guid, group_guid, pp, onlylocal, type);

  /* Copy QoS, merging in defaults */
  rd->xqos = ddsrt_malloc (sizeof (*rd->xqos));
  ddsi_xqos_copy (rd->xqos, xqos);
  ddsi_xqos_mergein_missing (rd->xqos, &ddsi_default_qos_reader, ~(uint64_t)0);
  assert (rd->xqos->aliased == 0);
  set_topic_type_name (rd->xqos, topic_name, type->type_name);

  if (rd->e.gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    ELOGDISC (rd, "READER "PGUIDFMT" QOS={", PGUID (rd->e.guid));
    ddsi_xqos_log (DDS_LC_DISCOVERY, &rd->e.gv->logconfig, rd->xqos);
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
  rd->type = ddsi_sertype_ref (type);
  rd->request_keyhash = rd->type->request_keyhash;
  rd->ddsi2direct_cb = 0;
  rd->ddsi2direct_cbarg = 0;
  rd->init_acknack_count = 1;
  rd->num_writers = 0;
#ifdef DDS_HAS_SSM
  rd->favours_ssm = 0;
#endif
#ifdef DDS_HAS_SECURITY
  rd->sec_attr = NULL;
#endif
  rd->status_cb = status_cb;
  rd->status_cb_entity = status_entity;
  rd->rhc = rhc;
  /* set rhc qos for reader */
  if (rhc)
  {
    ddsi_rhc_set_qos (rd->rhc, rd->xqos);
  }
  assert (rd->xqos->present & QP_LIVELINESS);

#ifdef DDS_HAS_SECURITY
  q_omg_security_register_reader(rd);
#endif

#ifdef DDS_HAS_NETWORK_PARTITIONS
  rd->uc_as = rd->mc_as = NULL;
  {
    /* compile address set from the mapped network partitions */
    char *ps_def = "";
    char **ps = (rd->xqos->partition.n > 0) ? rd->xqos->partition.strs : &ps_def;
    uint32_t nps = (rd->xqos->partition.n > 0) ? rd->xqos->partition.n : 1;
    const struct ddsi_config_networkpartition_listelem *np = NULL;
    for (uint32_t i = 0; i < nps && np == NULL; i++)
      np = get_as_from_mapping (pp->e.gv, ps[i], rd->xqos->topic_name);
    if (np)
    {
      rd->uc_as = np->uc_addresses;
      rd->mc_as = np->asm_addresses;
#ifdef DDS_HAS_SSM
      if (np->ssm_addresses != NULL && (rd->e.gv->config.allowMulticast & DDSI_AMC_SSM))
        rd->favours_ssm = 1;
#endif
    }
    if (rd->mc_as)
    {
      /* Iterate over all udp addresses:
       *   - Set the correct portnumbers
       *   - Join the socket if a multicast address
       */
      for (const struct networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
        join_mcast_helper (pp->e.gv, pp->e.gv->data_conn_mc, &a->loc);
    }
#ifdef DDS_HAS_SSM
    else
    {
      /* Note: SSM requires NETWORK_PARTITIONS; if network partitions
         do not override the default, we should check whether the
         default is an SSM address. */
      if (ddsi_is_ssm_mcaddr (pp->e.gv, &pp->e.gv->loc_default_mc) && pp->e.gv->config.allowMulticast & DDSI_AMC_SSM)
        rd->favours_ssm = 1;
    }
#endif
  }
#ifdef DDS_HAS_SSM
  if (rd->favours_ssm)
    ELOGDISC (pp, "READER "PGUIDFMT" ssm=%d\n", PGUID (rd->e.guid), rd->favours_ssm);
#endif
  if ((rd->uc_as || rd->mc_as) && (pp->e.gv->logconfig.c.mask & DDS_LC_DISCOVERY))
  {
    char buf[DDSI_LOCSTRLEN];
    ELOGDISC (pp, "READER "PGUIDFMT" locators={", PGUID (rd->e.guid));
    for (const struct networkpartition_address *a = rd->uc_as; a != NULL; a = a->next)
      ELOGDISC (pp, " %s", ddsi_locator_to_string (buf, sizeof (buf), &a->loc));
    for (const struct networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
      ELOGDISC (pp, " %s", ddsi_locator_to_string (buf, sizeof (buf), &a->loc));
    ELOGDISC (pp, " }\n");
  }
#endif

  ddsrt_avl_init (&rd_writers_treedef, &rd->writers);
  ddsrt_avl_init (&rd_local_writers_treedef, &rd->local_writers);

  ddsrt_mutex_lock (&rd->e.lock);
  entidx_insert_reader_guid (pp->e.gv->entity_index, rd);
  builtintopic_write_endpoint (pp->e.gv->builtin_topic_interface, &rd->e, ddsrt_time_wallclock(), true);
  ddsrt_mutex_unlock (&rd->e.lock);

  match_reader_with_proxy_writers (rd, tnow);
  match_reader_with_local_writers (rd, tnow);
  sedp_write_reader (rd);
  return 0;
}

dds_return_t new_reader
(
  struct reader **rd_out,
  struct ddsi_guid *rdguid,
  const struct ddsi_guid *group_guid,
  struct participant *pp,
  const char *topic_name,
  const struct ddsi_sertype *type,
  const struct dds_qos *xqos,
  struct ddsi_rhc * rhc,
  status_cb_t status_cb,
  void * status_cbarg
)
{
  dds_return_t rc;
  uint32_t kind;

  rdguid->prefix = pp->e.guid.prefix;
  kind = type->typekind_no_key ? NN_ENTITYID_KIND_READER_NO_KEY : NN_ENTITYID_KIND_READER_WITH_KEY;
  if ((rc = pp_allocate_entityid (&rdguid->entityid, kind, pp)) < 0)
    return rc;
  return new_reader_guid (rd_out, rdguid, group_guid, pp, topic_name, type, xqos, rhc, status_cb, status_cbarg);
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
    free_rd_pwr_match (rd->e.gv, &rd->e.guid, m);
  }
  while (!ddsrt_avl_is_empty (&rd->local_writers))
  {
    struct rd_wr_match *m = ddsrt_avl_root_non_empty (&rd_local_writers_treedef, &rd->local_writers);
    ddsrt_avl_delete (&rd_local_writers_treedef, &rd->local_writers, m);
    writer_drop_local_connection (&m->wr_guid, rd);
    free_rd_wr_match (m);
  }

#ifdef DDS_HAS_SECURITY
  q_omg_security_deregister_reader(rd);
#endif

  if (!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE))
    sedp_dispose_unregister_reader (rd);
#ifdef DDS_HAS_NETWORK_PARTITIONS
  if (rd->mc_as)
  {
    for (const struct networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
      leave_mcast_helper (rd->e.gv, rd->e.gv->data_conn_mc, &a->loc);
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
  ddsi_sertype_unref ((struct ddsi_sertype *) rd->type);

  ddsi_xqos_fini (rd->xqos);
  ddsrt_free (rd->xqos);
  endpoint_common_fini (&rd->e, &rd->c);
  ddsrt_free (rd);
}

dds_return_t delete_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct reader *rd;
  assert (!is_writer_entityid (guid->entityid));
  if ((rd = entidx_lookup_reader_guid (gv->entity_index, guid)) == NULL)
  {
    GVLOGDISC ("delete_reader_guid(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("delete_reader_guid(guid "PGUIDFMT") ...\n", PGUID (*guid));
  builtintopic_write_endpoint (rd->e.gv->builtin_topic_interface, &rd->e, ddsrt_time_wallclock(), false);
  entidx_remove_reader_guid (gv->entity_index, rd);
  gcreq_reader (rd);
  return 0;
}

void update_reader_qos (struct reader *rd, const dds_qos_t *xqos)
{
  ddsrt_mutex_lock (&rd->e.lock);
  if (update_qos_locked (&rd->e, rd->xqos, xqos, ddsrt_time_wallclock ()))
    sedp_write_reader (rd);
  ddsrt_mutex_unlock (&rd->e.lock);
}


/* TOPIC ------------------------------------------------ */

#ifdef DDS_HAS_TOPIC_DISCOVERY

dds_return_t ddsi_new_topic
(
  struct topic **tp_out,
  struct ddsi_guid *tpguid,
  struct participant *pp,
  const char *topic_name,
  const struct ddsi_sertype *sertype,
  const struct dds_qos *xqos,
  bool is_builtin,
  bool *new_topic_def
)
{
  dds_return_t rc;
  ddsrt_wctime_t timestamp = ddsrt_time_wallclock ();
  struct ddsi_domaingv *gv = pp->e.gv;
  tpguid->prefix = pp->e.guid.prefix;
  if ((rc = pp_allocate_entityid (&tpguid->entityid, (is_builtin ? NN_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN : NN_ENTITYID_KIND_CYCLONE_TOPIC_USER) | NN_ENTITYID_SOURCE_VENDOR, pp)) < 0)
    return rc;
  assert (entidx_lookup_topic_guid (gv->entity_index, tpguid) == NULL);

  struct topic *tp = ddsrt_malloc (sizeof (*tp));
  if (tp_out)
    *tp_out = tp;
  entity_common_init (&tp->e, gv, tpguid, NULL, EK_TOPIC, timestamp, NN_VENDORID_ECLIPSE, pp->e.onlylocal);
  tp->pp = ref_participant (pp, &tp->e.guid);

  /* Copy QoS, merging in defaults */
  struct dds_qos *tp_qos = ddsrt_malloc (sizeof (*tp_qos));
  ddsi_xqos_copy (tp_qos, xqos);
  ddsi_xqos_mergein_missing (tp_qos, &ddsi_default_qos_topic, ~(uint64_t)0);
  assert (tp_qos->aliased == 0);

  /* Set topic name, type name and type information in qos */
  tp_qos->present |= QP_TYPE_INFORMATION;
  tp_qos->type_information = ddsi_sertype_typeinfo (sertype);
  assert (tp_qos->type_information);
  set_topic_type_name (tp_qos, topic_name, sertype->type_name);

  if (gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    ELOGDISC (tp, "TOPIC "PGUIDFMT" QOS={", PGUID (tp->e.guid));
    ddsi_xqos_log (DDS_LC_DISCOVERY, &gv->logconfig, tp_qos);
    ELOGDISC (tp, "}\n");
  }
  tp->definition = ref_topic_definition (gv, sertype, ddsi_typeinfo_complete_typeid (tp_qos->type_information), tp_qos, new_topic_def);
  if (new_topic_def)
    builtintopic_write_topic (gv->builtin_topic_interface, tp->definition, timestamp, true);
  ddsi_xqos_fini (tp_qos);
  ddsrt_free (tp_qos);

  ddsrt_mutex_lock (&tp->e.lock);
  entidx_insert_topic_guid (gv->entity_index, tp);
  (void) sedp_write_topic (tp, true);
  ddsrt_mutex_unlock (&tp->e.lock);
  return 0;
}

void update_topic_qos (struct topic *tp, const dds_qos_t *xqos)
{
  /* Updating the topic qos, which means replacing the topic definition for a topic,
     does not result in a new topic in the context of the find topic api. So there
     is no need to broadcast on gv->new_topic_cond */

  struct ddsi_domaingv *gv = tp->e.gv;
  ddsrt_mutex_lock (&tp->e.lock);
  ddsrt_mutex_lock (&tp->e.qos_lock);
  struct ddsi_topic_definition *tpd = tp->definition;
  uint64_t mask = ddsi_xqos_delta (tpd->xqos, xqos, QP_CHANGEABLE_MASK & ~(QP_RXO_MASK | QP_PARTITION)) & xqos->present;
  GVLOGDISC ("update_topic_qos "PGUIDFMT" delta=%"PRIu64" QOS={", PGUID(tp->e.guid), mask);
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
  unref_topic_definition_locked (tpd, ddsrt_time_wallclock());
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  if (new_tpd)
    builtintopic_write_topic (gv->builtin_topic_interface, tp->definition, ddsrt_time_wallclock(), true);
  ddsrt_mutex_unlock (&tp->e.qos_lock);
  (void) sedp_write_topic (tp, true);
  ddsrt_mutex_unlock (&tp->e.lock);
  dds_delete_qos (newqos);
}

static void gc_delete_topic (struct gcreq *gcreq)
{
  struct topic *tp = gcreq->arg;
  ELOGDISC (tp, "gc_delete_topic(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (tp->e.guid));
  gcreq_free (gcreq);
  if (!is_builtin_entityid (tp->e.guid.entityid, NN_VENDORID_ECLIPSE))
    (void) sedp_write_topic (tp, false);
  entity_common_fini (&tp->e);
  unref_topic_definition (tp->e.gv, tp->definition, ddsrt_time_wallclock());
  unref_participant (tp->pp, &tp->e.guid);
  ddsrt_free (tp);
}

dds_return_t delete_topic (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct topic *tp;
  assert (is_topic_entityid (guid->entityid));
  if ((tp = entidx_lookup_topic_guid (gv->entity_index, guid)) == NULL)
  {
    GVLOGDISC ("delete_topic(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("delete_topic(guid "PGUIDFMT") ...\n", PGUID (*guid));
  entidx_remove_topic_guid (gv->entity_index, tp);
  gcreq_topic (tp);
  return 0;
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
  set_topic_definition_hash (&templ);
  struct ddsi_topic_definition *tpd = ddsrt_hh_lookup (gv->topic_defs, &templ);
  ddsi_type_pair_free (templ.type_pair);
  if (tpd) {
    tpd->refc++;
    *is_new = false;
  } else {
    tpd = new_topic_definition (gv, sertype, qos);
    assert (tpd != NULL);
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

#endif /* DDS_HAS_TOPIC_DISCOVERY */

/* PROXY-PARTICIPANT ------------------------------------------------ */
static void proxy_participant_replace_minl (struct proxy_participant *proxypp, bool manbypp, struct lease *lnew)
{
  /* By loading/storing the pointer atomically, we ensure we always
     read a valid (or once valid) lease. By delaying freeing the lease
     through the garbage collector, we ensure whatever lease update
     occurs in parallel completes before the memory is released. */
  struct gcreq *gcreq = gcreq_new (proxypp->e.gv->gcreq_queue, gc_participant_lease);
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
    struct lease *minl = ddsrt_fibheap_min (&lease_fhdef_pp, &proxypp->leaseheap_auto);
    ddsrt_fibheap_delete (&lease_fhdef_pp, &proxypp->leaseheap_auto, proxypp->lease);
    if (minl == proxypp->lease)
    {
      if ((minl = ddsrt_fibheap_min (&lease_fhdef_pp, &proxypp->leaseheap_auto)) != NULL)
      {
        dds_duration_t trem = minl->tdur - proxypp->lease->tdur;
        assert (trem >= 0);
        ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), trem);
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
    struct gcreq *gcreq = gcreq_new (proxypp->e.gv->gcreq_queue, gc_participant_lease);
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
  unsigned entityid;
  const char *topic_name;
};

static void create_proxy_builtin_endpoint_impl (struct ddsi_domaingv *gv, ddsrt_wctime_t timestamp, const struct ddsi_guid *ppguid,
    struct proxy_participant *proxypp, const struct ddsi_guid *ep_guid, ddsi_plist_t *plist, const char *topic_name)
{
  if ((plist->qos.present & QP_TOPIC_NAME) == QP_TOPIC_NAME)
    ddsi_plist_fini_mask (plist, 0, QP_TOPIC_NAME);
  plist->qos.topic_name = dds_string_dup (topic_name);
  plist->qos.present |= QP_TOPIC_NAME;
  if (is_writer_entityid (ep_guid->entityid))
    new_proxy_writer (gv, ppguid, ep_guid, proxypp->as_meta, plist, gv->builtins_dqueue, gv->xevents, timestamp, 0);
  else
  {
#ifdef DDS_HAS_SSM
    const int ssm = addrset_contains_ssm (gv, proxypp->as_meta);
    new_proxy_reader (gv, ppguid, ep_guid, proxypp->as_meta, plist, timestamp, 0, ssm);
#else
    new_proxy_reader (gv, ppguid, ep_guid, proxypp->as_meta, plist, timestamp, 0);
#endif
  }
}

static void create_proxy_builtin_endpoints(
  struct ddsi_domaingv *gv,
  const struct bestab *bestab,
  int nbes,
  const struct ddsi_guid *ppguid,
  struct proxy_participant *proxypp,
  ddsrt_wctime_t timestamp,
  dds_qos_t *xqos_wr,
  dds_qos_t *xqos_rd)
{
  ddsi_plist_t plist_rd, plist_wr;
  /* Note: no entity name or group GUID supplied, but that shouldn't
   * matter, as these are internal to DDSI and don't use group coherency */
  ddsi_plist_init_empty (&plist_wr);
  ddsi_plist_init_empty (&plist_rd);
  ddsi_xqos_copy (&plist_wr.qos, xqos_wr);
  ddsi_xqos_copy (&plist_rd.qos, xqos_rd);
  for (int i = 0; i < nbes; i++)
  {
    const struct bestab *te = &bestab[i];
    if (proxypp->bes & te->besflag)
    {
      ddsi_guid_t ep_guid = { .prefix = proxypp->e.guid.prefix, .entityid.u = te->entityid };
      assert (is_builtin_entityid (ep_guid.entityid, proxypp->vendor));
      create_proxy_builtin_endpoint_impl (gv, timestamp, ppguid, proxypp, &ep_guid, is_writer_entityid (ep_guid.entityid) ? &plist_wr : &plist_rd, te->topic_name);
    }
  }
  ddsi_plist_fini (&plist_wr);
  ddsi_plist_fini (&plist_rd);
}


static void add_proxy_builtin_endpoints(
  struct ddsi_domaingv *gv,
  const struct ddsi_guid *ppguid,
  struct proxy_participant *proxypp,
  ddsrt_wctime_t timestamp)
{
  /* Add proxy endpoints based on the advertised (& possibly augmented
     ...) built-in endpoint set. */
#define TE(ap_, a_, bp_, b_, c_) { NN_##ap_##BUILTIN_ENDPOINT_##a_, NN_ENTITYID_##bp_##_BUILTIN_##b_, DDS_BUILTIN_TOPIC_##c_##_NAME }
#define LTE(a_, bp_, b_, c_) { NN_##BUILTIN_ENDPOINT_##a_, NN_ENTITYID_##bp_##_BUILTIN_##b_, DDS_BUILTIN_TOPIC_##c_##_NAME }

  /* 'Default' proxy endpoints. */
  static const struct bestab bestab_default[] = {
#if 0
    /* SPDP gets special treatment => no need for proxy
       writers/readers */
    TE (DISC_, PARTICIPANT_ANNOUNCER, SPDP, PARTICIPANT_WRITER, PARTICIPANT),
#endif
    TE (DISC_, PARTICIPANT_DETECTOR, SPDP, PARTICIPANT_READER, PARTICIPANT),
    TE (DISC_, PUBLICATION_ANNOUNCER, SEDP, PUBLICATIONS_WRITER, PUBLICATION),
    TE (DISC_, PUBLICATION_DETECTOR, SEDP, PUBLICATIONS_READER, PUBLICATION),
    TE (DISC_, SUBSCRIPTION_ANNOUNCER, SEDP, SUBSCRIPTIONS_WRITER, SUBSCRIPTION),
    TE (DISC_, SUBSCRIPTION_DETECTOR, SEDP, SUBSCRIPTIONS_READER, SUBSCRIPTION),
    LTE (PARTICIPANT_MESSAGE_DATA_WRITER, P2P, PARTICIPANT_MESSAGE_WRITER, PARTICIPANT_MESSAGE),
    LTE (PARTICIPANT_MESSAGE_DATA_READER, P2P, PARTICIPANT_MESSAGE_READER, PARTICIPANT_MESSAGE),
#ifdef DDS_HAS_TOPIC_DISCOVERY
    TE (DISC_, TOPICS_ANNOUNCER, SEDP, TOPIC_WRITER, TOPIC),
    TE (DISC_, TOPICS_DETECTOR, SEDP, TOPIC_READER, TOPIC),
#endif
  };
  create_proxy_builtin_endpoints(gv, bestab_default,
    (int)(sizeof (bestab_default) / sizeof (*bestab_default)),
    ppguid, proxypp, timestamp, &gv->builtin_endpoint_xqos_wr, &gv->builtin_endpoint_xqos_rd);

#ifdef DDS_HAS_TYPE_DISCOVERY
  /* Volatile proxy endpoints. */
  static const struct bestab bestab_volatile[] = {
    LTE (TL_SVC_REQUEST_DATA_WRITER, TL_SVC, REQUEST_WRITER, TYPELOOKUP_REQUEST),
    LTE (TL_SVC_REQUEST_DATA_READER, TL_SVC, REQUEST_READER, TYPELOOKUP_REQUEST),
    LTE (TL_SVC_REPLY_DATA_WRITER, TL_SVC, REPLY_WRITER, TYPELOOKUP_REPLY),
    LTE (TL_SVC_REPLY_DATA_READER, TL_SVC, REPLY_READER, TYPELOOKUP_REPLY),
  };
  create_proxy_builtin_endpoints(gv, bestab_volatile,
    (int)(sizeof (bestab_volatile) / sizeof (*bestab_volatile)),
    ppguid, proxypp, timestamp, &gv->builtin_volatile_xqos_wr, &gv->builtin_volatile_xqos_rd);
#endif

#ifdef DDS_HAS_SECURITY
  /* Security 'default' proxy endpoints. */
  static const struct bestab bestab_security[] = {
    LTE (PUBLICATION_MESSAGE_SECURE_ANNOUNCER, SEDP, PUBLICATIONS_SECURE_WRITER, PUBLICATION_SECURE),
    LTE (PUBLICATION_MESSAGE_SECURE_DETECTOR, SEDP, PUBLICATIONS_SECURE_READER, PUBLICATION_SECURE),
    LTE (SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER, SEDP, SUBSCRIPTIONS_SECURE_WRITER, SUBSCRIPTION_SECURE),
    LTE (SUBSCRIPTION_MESSAGE_SECURE_DETECTOR, SEDP, SUBSCRIPTIONS_SECURE_READER, SUBSCRIPTION_SECURE),
    LTE (PARTICIPANT_MESSAGE_SECURE_ANNOUNCER, P2P, PARTICIPANT_MESSAGE_SECURE_WRITER, PARTICIPANT_MESSAGE_SECURE),
    LTE (PARTICIPANT_MESSAGE_SECURE_DETECTOR, P2P, PARTICIPANT_MESSAGE_SECURE_READER, PARTICIPANT_MESSAGE_SECURE),
    TE (DISC_, PARTICIPANT_SECURE_ANNOUNCER, SPDP_RELIABLE, PARTICIPANT_SECURE_WRITER, PARTICIPANT_SECURE),
    TE (DISC_, PARTICIPANT_SECURE_DETECTOR, SPDP_RELIABLE, PARTICIPANT_SECURE_READER, PARTICIPANT_SECURE)
  };
  create_proxy_builtin_endpoints(gv, bestab_security,
    (int)(sizeof (bestab_security) / sizeof (*bestab_security)),
    ppguid, proxypp, timestamp, &gv->builtin_endpoint_xqos_wr, &gv->builtin_endpoint_xqos_rd);

  /* Security 'volatile' proxy endpoints. */
  static const struct bestab bestab_security_volatile[] = {
    LTE (PARTICIPANT_VOLATILE_SECURE_ANNOUNCER, P2P, PARTICIPANT_VOLATILE_SECURE_WRITER, PARTICIPANT_VOLATILE_MESSAGE_SECURE),
    LTE (PARTICIPANT_VOLATILE_SECURE_DETECTOR, P2P, PARTICIPANT_VOLATILE_SECURE_READER, PARTICIPANT_VOLATILE_MESSAGE_SECURE)
  };
  create_proxy_builtin_endpoints(gv, bestab_security_volatile,
    (int)(sizeof (bestab_security_volatile) / sizeof (*bestab_security_volatile)),
    ppguid, proxypp, timestamp, &gv->builtin_secure_volatile_xqos_wr, &gv->builtin_secure_volatile_xqos_rd);

  /* Security 'stateless' proxy endpoints. */
  static const struct bestab bestab_security_stateless[] = {
    LTE (PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER, P2P, PARTICIPANT_STATELESS_MESSAGE_WRITER, PARTICIPANT_STATELESS_MESSAGE),
    LTE (PARTICIPANT_STATELESS_MESSAGE_DETECTOR, P2P, PARTICIPANT_STATELESS_MESSAGE_READER, PARTICIPANT_STATELESS_MESSAGE)
  };
  create_proxy_builtin_endpoints(gv, bestab_security_stateless,
    (int)(sizeof (bestab_security_stateless) / sizeof (*bestab_security_stateless)),
    ppguid, proxypp, timestamp, &gv->builtin_stateless_xqos_wr, &gv->builtin_stateless_xqos_rd);
#endif

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
  minl_prev = ddsrt_fibheap_min (&lease_fhdef_pp, lh);
  ddsrt_fibheap_insert (&lease_fhdef_pp, lh, pwr->lease);
  minl_new = ddsrt_fibheap_min (&lease_fhdef_pp, lh);
  /* ensure proxypp->minl_man/minl_auto is equivalent to min(leaseheap_man/auto) */
  if (proxypp->owns_lease && minl_prev != minl_new)
  {
    ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed (), minl_new->tdur);
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
  struct lease *minl_prev;
  struct lease *minl_new;
  bool manbypp;
  ddsrt_fibheap_t *lh;

  assert (pwr->lease != NULL);
  manbypp = (pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
  lh = manbypp ? &proxypp->leaseheap_man : &proxypp->leaseheap_auto;
  minl_prev = ddsrt_fibheap_min (&lease_fhdef_pp, lh);
  ddsrt_fibheap_delete (&lease_fhdef_pp, lh, pwr->lease);
  minl_new = ddsrt_fibheap_min (&lease_fhdef_pp, lh);
  /* ensure proxypp->minl_man/minl_auto is equivalent to min(leaseheap_man/auto) */
  if (proxypp->owns_lease && minl_prev != minl_new)
  {
    if (minl_new != NULL)
    {
      dds_duration_t trem = minl_new->tdur - minl_prev->tdur;
      assert (trem >= 0);
      ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), trem);
      struct lease *lnew = lease_new (texp, minl_new->tdur, minl_new->entity);
      proxy_participant_replace_minl (proxypp, manbypp, lnew);
      lease_register (lnew);
    }
    else
    {
      proxy_participant_replace_minl (proxypp, manbypp, NULL);
    }
  }
}

#ifdef DDS_HAS_SECURITY

void handshake_end_cb(struct ddsi_handshake *handshake, struct participant *pp, struct proxy_participant *proxypp, enum ddsi_handshake_state result)
{
  const struct ddsi_domaingv * const gv = pp->e.gv;
  int64_t shared_secret;

  switch(result)
  {
  case STATE_HANDSHAKE_PROCESSED:
    shared_secret = ddsi_handshake_get_shared_secret(handshake);
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") processed\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    if (q_omg_security_register_remote_participant(pp, proxypp, shared_secret)) {
      match_volatile_secure_endpoints(pp, proxypp);
      q_omg_security_set_remote_participant_authenticated(pp, proxypp);
    }
    break;

  case STATE_HANDSHAKE_SEND_TOKENS:
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") send tokens\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    q_omg_security_participant_send_tokens(pp, proxypp);
    break;

  case STATE_HANDSHAKE_OK:
    DDS_CLOG (DDS_LC_DISCOVERY, &gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") succeeded\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid));
    update_proxy_participant_endpoint_matching(proxypp, pp);
    ddsi_handshake_remove(pp, proxypp);
    break;

  case STATE_HANDSHAKE_TIMED_OUT:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Timed out\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    if (q_omg_participant_allow_unauthenticated(pp)) {
      downgrade_to_nonsecure(proxypp);
      update_proxy_participant_endpoint_matching(proxypp, pp);
    }
    ddsi_handshake_remove(pp, proxypp);
    break;
  case STATE_HANDSHAKE_FAILED:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Failed\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    if (q_omg_participant_allow_unauthenticated(pp)) {
      downgrade_to_nonsecure(proxypp);
      update_proxy_participant_endpoint_matching(proxypp, pp);
    }
    ddsi_handshake_remove(pp, proxypp);
    break;
  default:
    DDS_CERROR (&gv->logconfig, "handshake (lguid="PGUIDFMT" rguid="PGUIDFMT") failed: (%d) Unknown failure\n", PGUID (pp->e.guid), PGUID (proxypp->e.guid), (int)result);
    ddsi_handshake_remove(pp, proxypp);
    break;
  }
}

static bool proxy_participant_has_pp_match(struct ddsi_domaingv *gv, struct proxy_participant *proxypp)
{
  bool match = false;
  struct participant *pp;
  struct entidx_enum_participant est;

  entidx_enum_participant_init (&est, gv->entity_index);
  while ((pp = entidx_enum_participant_next (&est)) != NULL && !match)
  {
    /* remote secure pp can possibly match with local non-secured pp in case allow-unauthenticated pp
       is enabled in the remote pp's security settings */
    match = !q_omg_participant_is_secure (pp) || q_omg_is_similar_participant_security_info (pp, proxypp);
  }
  entidx_enum_participant_fini (&est);
  return match;
}

static void proxy_participant_create_handshakes(struct ddsi_domaingv *gv, struct proxy_participant *proxypp)
{
  struct participant *pp;
  struct entidx_enum_participant est;

  q_omg_security_remote_participant_set_initialized(proxypp);

  entidx_enum_participant_init (&est, gv->entity_index);
  while (((pp = entidx_enum_participant_next (&est)) != NULL)) {
    if (q_omg_security_participant_is_initialized(pp))
      ddsi_handshake_register(pp, proxypp, handshake_end_cb);
  }
  entidx_enum_participant_fini(&est);
}

static void disconnect_proxy_participant_secure(struct proxy_participant *proxypp)
{
  struct participant *pp;
  struct entidx_enum_participant it;
  struct ddsi_domaingv * const gv = proxypp->e.gv;

  if (q_omg_proxy_participant_is_secure(proxypp))
  {
    entidx_enum_participant_init (&it, gv->entity_index);
    while ((pp = entidx_enum_participant_next (&it)) != NULL)
    {
      ddsi_handshake_remove(pp, proxypp);
    }
    entidx_enum_participant_fini (&it);
  }
}
#endif

static void free_proxy_participant(struct proxy_participant *proxypp)
{
  if (proxypp->owns_lease)
  {
    struct lease * minl_auto = ddsrt_atomic_ldvoidp (&proxypp->minl_auto);
    ddsrt_fibheap_delete (&lease_fhdef_pp, &proxypp->leaseheap_auto, proxypp->lease);
    assert (ddsrt_fibheap_min (&lease_fhdef_pp, &proxypp->leaseheap_auto) == NULL);
    assert (ddsrt_fibheap_min (&lease_fhdef_pp, &proxypp->leaseheap_man) == NULL);
    assert (ddsrt_atomic_ldvoidp (&proxypp->minl_man) == NULL);
    assert (!compare_guid (&minl_auto->entity->guid, &proxypp->e.guid));
    /* if the lease hasn't been registered yet (which is the case when
       new_proxy_participant calls this, it is marked as such and calling
       lease_unregister is ok */
    lease_unregister (minl_auto);
    lease_free (minl_auto);
    lease_free (proxypp->lease);
  }
#ifdef DDS_HAS_SECURITY
  disconnect_proxy_participant_secure(proxypp);
  q_omg_security_deregister_remote_participant(proxypp);
#endif
  unref_addrset (proxypp->as_default);
  unref_addrset (proxypp->as_meta);
  ddsi_plist_fini (proxypp->plist);
  ddsrt_free (proxypp->plist);
  entity_common_fini (&proxypp->e);
  ddsrt_free (proxypp);
}

bool new_proxy_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, uint32_t bes, const struct ddsi_guid *privileged_pp_guid, struct addrset *as_default, struct addrset *as_meta, const ddsi_plist_t *plist, dds_duration_t tlease_dur, nn_vendorid_t vendor, unsigned custom_flags, ddsrt_wctime_t timestamp, seqno_t seq)
{
  /* No locking => iff all participants use unique guids, and sedp
     runs on a single thread, it can't go wrong. FIXME, maybe? The
     same holds for the other functions for creating entities. */
  struct proxy_participant *proxypp;
  const bool is_secure = ((bes & NN_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER) != 0);
  assert (!is_secure || (plist->present & PP_IDENTITY_TOKEN));
  assert (is_secure || (bes & ~NN_BES_MASK_NON_SECURITY) == 0);
  (void) is_secure;

  assert (ppguid->entityid.u == NN_ENTITYID_PARTICIPANT);
  assert (entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid) == NULL);
  assert (privileged_pp_guid == NULL || privileged_pp_guid->entityid.u == NN_ENTITYID_PARTICIPANT);

  prune_deleted_participant_guids (gv->deleted_participants, ddsrt_time_monotonic ());

  proxypp = ddsrt_malloc (sizeof (*proxypp));

  entity_common_init (&proxypp->e, gv, ppguid, "", EK_PROXY_PARTICIPANT, timestamp, vendor, false);
  proxypp->refc = 1;
  proxypp->lease_expired = 0;
  proxypp->deleting = 0;
  proxypp->vendor = vendor;
  proxypp->bes = bes;
  proxypp->seq = seq;
  if (privileged_pp_guid) {
    proxypp->privileged_pp_guid = *privileged_pp_guid;
  } else {
    memset (&proxypp->privileged_pp_guid.prefix, 0, sizeof (proxypp->privileged_pp_guid.prefix));
    proxypp->privileged_pp_guid.entityid.u = NN_ENTITYID_PARTICIPANT;
  }
  if ((plist->present & PP_ADLINK_PARTICIPANT_VERSION_INFO) &&
      (plist->adlink_participant_version_info.flags & NN_ADLINK_FL_DDSI2_PARTICIPANT_FLAG) &&
      (plist->adlink_participant_version_info.flags & NN_ADLINK_FL_PARTICIPANT_IS_DDSI2))
    proxypp->is_ddsi2_pp = 1;
  else
    proxypp->is_ddsi2_pp = 0;
  if ((plist->present & PP_ADLINK_PARTICIPANT_VERSION_INFO) &&
      (plist->adlink_participant_version_info.flags & NN_ADLINK_FL_MINIMAL_BES_MODE))
    proxypp->minimal_bes_mode = 1;
  else
    proxypp->minimal_bes_mode = 0;
  proxypp->implicitly_created = ((custom_flags & CF_IMPLICITLY_CREATED_PROXYPP) != 0);
  proxypp->proxypp_have_spdp = ((custom_flags & CF_PROXYPP_NO_SPDP) == 0);
  if (plist->present & PP_CYCLONE_RECEIVE_BUFFER_SIZE)
    proxypp->receive_buffer_size = plist->cyclone_receive_buffer_size;
  else /* default to what we use */
    proxypp->receive_buffer_size = ddsi_receive_buffer_size (gv->m_factory);
  if (proxypp->receive_buffer_size < 131072)
  {
    /* if we don't know anything, or if it is implausibly tiny, use 128kB */
    proxypp->receive_buffer_size = 131072;
  }
  if (plist->present & PP_CYCLONE_REDUNDANT_NETWORKING)
    proxypp->redundant_networking = (plist->cyclone_redundant_networking != 0);
  else
    proxypp->redundant_networking = 0;

  {
    struct proxy_participant *privpp;
    privpp = entidx_lookup_proxy_participant_guid (gv->entity_index, &proxypp->privileged_pp_guid);

    ddsrt_fibheap_init (&lease_fhdef_pp, &proxypp->leaseheap_auto);
    ddsrt_fibheap_init (&lease_fhdef_pp, &proxypp->leaseheap_man);
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
      ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), tlease_dur);
      dds_duration_t dur = (tlease_dur == DDS_INFINITY) ? gv->config.lease_duration : tlease_dur;
      proxypp->lease = lease_new (texp, dur, &proxypp->e);
      proxypp->owns_lease = 1;

      /* Add the proxypp lease to heap so that monitoring liveliness will include this lease
         and uses the shortest duration for proxypp and all its pwr's (with automatic liveliness) */
      ddsrt_fibheap_insert (&lease_fhdef_pp, &proxypp->leaseheap_auto, proxypp->lease);

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

#ifdef DDS_HAS_TOPIC_DISCOVERY
  proxy_topic_list_init (&proxypp->topics);
#endif
  proxypp->plist = ddsi_plist_dup (plist);
  ddsi_xqos_mergein_missing (&proxypp->plist->qos, &ddsi_default_plist_participant.qos, ~(uint64_t)0);
  ddsrt_avl_init (&proxypp_groups_treedef, &proxypp->groups);

#ifdef DDS_HAS_SECURITY
  proxypp->sec_attr = NULL;
  set_proxy_participant_security_info (proxypp, plist);
  if (is_secure)
  {
    q_omg_security_init_remote_participant (proxypp);
    /* check if the proxy participant has a match with a local participant */
    if (!proxy_participant_has_pp_match (gv, proxypp))
    {
      GVWARNING ("Remote secure participant "PGUIDFMT" not allowed\n", PGUID (*ppguid));
      free_proxy_participant (proxypp);
      return false;
    }
  }
#endif

  /* Proxy participant must be in the hash tables for new_proxy_{writer,reader} to work */
  entidx_insert_proxy_participant_guid (gv->entity_index, proxypp);
  add_proxy_builtin_endpoints(gv, ppguid, proxypp, timestamp);

  /* write DCPSParticipant topic before the lease can expire */
  builtintopic_write_endpoint (gv->builtin_topic_interface, &proxypp->e, timestamp, true);

  /* Register lease for auto liveliness, but be careful not to accidentally re-register
     DDSI2's lease, as we may have become dependent on DDSI2 any time after
     entidx_insert_proxy_participant_guid even if privileged_pp_guid was NULL originally */
  ddsrt_mutex_lock (&proxypp->e.lock);
  if (proxypp->owns_lease)
    lease_register (ddsrt_atomic_ldvoidp (&proxypp->minl_auto));
  ddsrt_mutex_unlock (&proxypp->e.lock);

#ifdef DDS_HAS_SECURITY
  if (is_secure)
  {
    proxy_participant_create_handshakes (gv, proxypp);
  }
#endif
  return true;
}

int update_proxy_participant_plist_locked (struct proxy_participant *proxypp, seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp)
{
  if (seq > proxypp->seq)
  {
    proxypp->seq = seq;

    const uint64_t pmask = PP_ENTITY_NAME;
    const uint64_t qmask = QP_USER_DATA;
    ddsi_plist_t *new_plist = ddsrt_malloc (sizeof (*new_plist));
    ddsi_plist_init_empty (new_plist);
    ddsi_plist_mergein_missing (new_plist, datap, pmask, qmask);
    ddsi_plist_mergein_missing (new_plist, &ddsi_default_plist_participant, ~(uint64_t)0, ~(uint64_t)0);
    (void) update_qos_locked (&proxypp->e, &proxypp->plist->qos, &new_plist->qos, timestamp);
    ddsi_plist_fini (new_plist);
    ddsrt_free (new_plist);
    proxypp->proxypp_have_spdp = 1;
  }
  return 0;
}

int update_proxy_participant_plist (struct proxy_participant *proxypp, seqno_t seq, const struct ddsi_plist *datap, ddsrt_wctime_t timestamp)
{
  ddsrt_mutex_lock (&proxypp->e.lock);
  update_proxy_participant_plist_locked (proxypp, seq, datap, timestamp);
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

  proxypp->refc++;
  if (c != NULL)
  {
    c->proxypp = proxypp;
    c->next_ep = proxypp->endpoints;
    c->prev_ep = NULL;
    if (c->next_ep)
      c->next_ep->prev_ep = c;
    proxypp->endpoints = c;
  }
  ddsrt_mutex_unlock (&proxypp->e.lock);

  return DDS_RETCODE_OK;
}

static void unref_proxy_participant (struct proxy_participant *proxypp, struct proxy_endpoint_common *c)
{
  uint32_t refc;
  const ddsrt_wctime_t tnow = ddsrt_time_wallclock();

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
    struct ddsi_domaingv * const gv = proxypp->e.gv;
    const ddsi_guid_t pp_guid = proxypp->e.guid;
    assert (proxypp->endpoints == NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    /* Last unref is called from gc_delete_proxy_participant, which is added to the gc queue after all proxy
       topic gc tasks. So we can safely assert that the proxy topic list is empty at this point */
    assert (proxy_topic_list_count (&proxypp->topics) == 0);
#endif
    ddsrt_mutex_unlock (&proxypp->e.lock);
    ELOGDISC (proxypp, "unref_proxy_participant("PGUIDFMT"): refc=0, freeing\n", PGUID (proxypp->e.guid));
    free_proxy_participant (proxypp);
    remove_deleted_participant_guid (gv->deleted_participants, &pp_guid, DPG_LOCAL | DPG_REMOTE);
  }
  else if (
    proxypp->endpoints == NULL
#ifdef DDS_HAS_TOPIC_DISCOVERY
    && proxy_topic_list_count (&proxypp->topics) == 0
#endif
    && proxypp->implicitly_created)
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

static void delete_or_detach_dependent_pp (struct proxy_participant *p, struct proxy_participant *proxypp, ddsrt_wctime_t timestamp, int isimplicit)
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
    ddsrt_etime_t texp = ddsrt_etime_add_duration (ddsrt_time_elapsed(), p->e.gv->config.ds_grace_period);
    /* Clear dependency (but don't touch entity id, which must be 0x1c1) and set the lease ticking */
    ELOGDISC (p, PGUIDFMT" detach-from-DS "PGUIDFMT"\n", PGUID(p->e.guid), PGUID(proxypp->e.guid));
    memset (&p->privileged_pp_guid.prefix, 0, sizeof (p->privileged_pp_guid.prefix));
    lease_set_expiry (p->lease, texp);
    /* FIXME: replace in p->leaseheap_auto and get new minl_auto */
    ddsrt_mutex_unlock (&p->e.lock);
  }
}

static void delete_ppt (struct proxy_participant *proxypp, ddsrt_wctime_t timestamp, int isimplicit)
{
  ddsi_entityid_t *child_entities;
  uint32_t n_child_entities = 0;

  /* if any proxy participants depend on this participant, delete them */
  ELOGDISC (proxypp, "delete_ppt("PGUIDFMT") - deleting dependent proxy participants\n", PGUID (proxypp->e.guid));
  {
    struct entidx_enum_proxy_participant est;
    struct proxy_participant *p;
    entidx_enum_proxy_participant_init (&est, proxypp->e.gv->entity_index);
    while ((p = entidx_enum_proxy_participant_next (&est)) != NULL)
      delete_or_detach_dependent_pp(p, proxypp, timestamp, isimplicit);
    entidx_enum_proxy_participant_fini (&est);
  }

  ddsrt_mutex_lock (&proxypp->e.lock);
  proxypp->deleting = 1;
  if (isimplicit)
    proxypp->lease_expired = 1;

#ifdef DDS_HAS_TOPIC_DISCOVERY
  proxy_topic_list_iter_t it;
  for (struct proxy_topic *proxytp = proxy_topic_list_iter_first (&proxypp->topics, &it); proxytp != NULL; proxytp = proxy_topic_list_iter_next (&it))
    if (!proxytp->deleted)
      (void) delete_proxy_topic_locked (proxypp, proxytp, timestamp);
#endif

  /* Get snapshot of endpoints and topics so that we can release proxypp->e.lock
     Pwrs/prds may be deleted during the iteration over the entities,
     but resolving the guid will fail for these entities and the
     call to delete_proxy_writer/reader returns. */
  {
    child_entities = ddsrt_malloc (proxypp->refc * sizeof(ddsi_entityid_t));
    struct proxy_endpoint_common *cep = proxypp->endpoints;
    while (cep)
    {
      const struct entity_common *entc = entity_common_from_proxy_endpoint_common (cep);
      child_entities[n_child_entities++] = entc->guid.entityid;
      cep = cep->next_ep;
    }
  }
  ddsrt_mutex_unlock (&proxypp->e.lock);

  ELOGDISC (proxypp, "delete_ppt("PGUIDFMT") - deleting endpoints\n", PGUID (proxypp->e.guid));
  ddsi_guid_t ep_guid = { .prefix = proxypp->e.guid.prefix, .entityid = { 0 } };
  for (uint32_t n = 0; n < n_child_entities; n++)
  {
    ep_guid.entityid = child_entities[n];
    if (is_writer_entityid (ep_guid.entityid))
      delete_proxy_writer (proxypp->e.gv, &ep_guid, timestamp, isimplicit);
    else if (is_reader_entityid (ep_guid.entityid))
      delete_proxy_reader (proxypp->e.gv, &ep_guid, timestamp, isimplicit);
  }
  ddsrt_free (child_entities);

  gcreq_proxy_participant (proxypp);
}

#ifdef DDS_HAS_SECURITY

struct setab {
  enum entity_kind kind;
  uint32_t id;
};


static void downgrade_to_nonsecure(struct proxy_participant *proxypp)
{
  const ddsrt_wctime_t tnow = ddsrt_time_wallclock ();
  struct ddsi_guid guid;
  static const struct setab setab[] = {
      {EK_PROXY_WRITER, NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER},
      {EK_PROXY_WRITER, NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER},
      {EK_PROXY_READER, NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER}
  };
  int i;

  DDS_CWARNING (&proxypp->e.gv->logconfig, "downgrade participant "PGUIDFMT" to non-secure\n", PGUID (proxypp->e.guid));

  guid.prefix = proxypp->e.guid.prefix;
  /* Remove security related endpoints. */
  for (i = 0; i < (int)(sizeof(setab)/sizeof(*setab)); i++)
  {
    guid.entityid.u = setab[i].id;
    switch (setab[i].kind)
    {
    case EK_PROXY_READER:
      (void)delete_proxy_reader (proxypp->e.gv, &guid, tnow, 0);
      break;
    case EK_PROXY_WRITER:
      (void)delete_proxy_writer (proxypp->e.gv, &guid, tnow, 0);
      break;
    default:
      assert(0);
    }
  }

  /* Cleanup all kinds of related security information. */
  q_omg_security_deregister_remote_participant(proxypp);
  proxypp->bes &= NN_BES_MASK_NON_SECURITY;
}
#endif


typedef struct proxy_purge_data {
  struct proxy_participant *proxypp;
  const ddsi_xlocator_t *loc;
  ddsrt_wctime_t timestamp;
} *proxy_purge_data_t;

static void purge_helper (const ddsi_xlocator_t *n, void * varg)
{
  proxy_purge_data_t data = (proxy_purge_data_t) varg;
  if (compare_xlocators (n, data->loc) == 0)
    delete_proxy_participant_by_guid (data->proxypp->e.gv, &data->proxypp->e.guid, data->timestamp, 1);
}

void purge_proxy_participants (struct ddsi_domaingv *gv, const ddsi_xlocator_t *loc, bool delete_from_as_disc)
{
  /* FIXME: check whether addr:port can't be reused for a new connection by the time we get here. */
  /* NOTE: This function exists for the sole purpose of cleaning up after closing a TCP connection in ddsi_tcp_close_conn and the state of the calling thread could be anything at this point. Because of that we do the unspeakable and toggle the thread state conditionally. We can't afford to have it in "asleep", as that causes a race with the garbage collector. */
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct entidx_enum_proxy_participant est;
  struct proxy_purge_data data;

  thread_state_awake (ts1, gv);
  data.loc = loc;
  data.timestamp = ddsrt_time_wallclock();
  entidx_enum_proxy_participant_init (&est, gv->entity_index);
  while ((data.proxypp = entidx_enum_proxy_participant_next (&est)) != NULL)
    addrset_forall (data.proxypp->as_meta, purge_helper, &data);
  entidx_enum_proxy_participant_fini (&est);

  /* Shouldn't try to keep pinging clients once they're gone */
  if (delete_from_as_disc)
    remove_from_addrset (gv, gv->as_disc, loc);

  thread_state_asleep (ts1);
}

int delete_proxy_participant_by_guid (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit)
{
  struct proxy_participant *ppt;

  GVLOGDISC ("delete_proxy_participant_by_guid("PGUIDFMT") ", PGUID (*guid));
  ddsrt_mutex_lock (&gv->lock);
  ppt = entidx_lookup_proxy_participant_guid (gv->entity_index, guid);
  if (ppt == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("- deleting\n");
  builtintopic_write_endpoint (gv->builtin_topic_interface, &ppt->e, timestamp, false);
  remember_deleted_participant_guid (gv->deleted_participants, &ppt->e.guid);
  entidx_remove_proxy_participant_guid (gv->entity_index, ppt);
  ddsrt_mutex_unlock (&gv->lock);
  delete_ppt (ppt, timestamp, isimplicit);

  return 0;
}

uint64_t get_entity_instance_id (const struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct thread_state1 *ts1 = lookup_thread_state ();
  struct entity_common *e;
  uint64_t iid = 0;
  thread_state_awake (ts1, gv);
  if ((e = entidx_lookup_guid_untyped (gv->entity_index, guid)) != NULL)
    iid = e->iid;
  thread_state_asleep (ts1);
  return iid;
}


#ifdef DDS_HAS_TOPIC_DISCOVERY

/* TOPIC DEFINITION ---------------------------------------------- */

int topic_definition_equal (const struct ddsi_topic_definition *tpd_a, const struct ddsi_topic_definition *tpd_b)
{
  if (tpd_a != NULL && tpd_b != NULL)
  {
    // The complete type identifier and qos should always be set for a topic definition
    assert (tpd_a->xqos != NULL && tpd_b->xqos != NULL);
    const ddsi_typeid_t *tid_a = ddsi_type_pair_complete_id (tpd_a->type_pair),
      *tid_b = ddsi_type_pair_complete_id (tpd_b->type_pair);
    return !ddsi_typeid_compare (tid_a, tid_b)
        && !ddsi_xqos_delta (tpd_a->xqos, tpd_b->xqos, ~(QP_TYPE_INFORMATION));
  }
  return tpd_a == tpd_b;
}

uint32_t topic_definition_hash (const struct ddsi_topic_definition *tpd)
{
  assert (tpd != NULL);
  return *(uint32_t *) tpd->key;
}

static void set_topic_definition_hash (struct ddsi_topic_definition *tpd)
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
  struct nn_xmsg *mqos = nn_xmsg_new (tpd->gv->xmsgpool, &nullguid, NULL, 0, NN_XMSG_KIND_DATA);
  ddsi_xqos_addtomsg (mqos, tpd->xqos, ~(QP_TYPE_INFORMATION));
  size_t sqos_sz;
  void * sqos = nn_xmsg_payload (&sqos_sz, mqos);
  assert (sqos_sz <= UINT32_MAX);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) sqos, (uint32_t) sqos_sz);
  nn_xmsg_free (mqos);

  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) &tpd->key);
}

static struct ddsi_topic_definition * new_topic_definition (struct ddsi_domaingv *gv, const struct ddsi_sertype *type, const struct dds_qos *qos)
{
  assert ((qos->present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == (QP_TOPIC_NAME | QP_TYPE_NAME));
  struct ddsi_topic_definition *tpd = ddsrt_malloc (sizeof (*tpd));
  tpd->xqos = ddsi_xqos_dup (qos);
  tpd->refc = 1;
  tpd->gv = gv;
  tpd->type_pair = ddsrt_malloc (sizeof (*tpd->type_pair));
  if (type != NULL)
  {
    tpd->type_pair->minimal = ddsi_type_ref_local (gv, type, DDSI_TYPEID_KIND_MINIMAL);
    tpd->type_pair->complete = ddsi_type_ref_local (gv, type, DDSI_TYPEID_KIND_COMPLETE);
  }
  else
  {
    assert (qos->present & QP_TYPE_INFORMATION);
    tpd->type_pair->minimal = ddsi_type_ref_proxy (gv, qos->type_information, DDSI_TYPEID_KIND_MINIMAL, NULL);
    tpd->type_pair->complete = ddsi_type_ref_proxy (gv, qos->type_information, DDSI_TYPEID_KIND_COMPLETE, NULL);
  }

  set_topic_definition_hash (tpd);
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
  return tpd;
}

dds_return_t lookup_topic_definition_by_name (struct ddsi_domaingv *gv, const char * topic_name, struct ddsi_topic_definition **tpd)
{
  assert (tpd != NULL);
  *tpd = NULL;
  struct ddsrt_hh_iter it;
  dds_return_t ret = DDS_RETCODE_OK;
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  for (struct ddsi_topic_definition *tpd1 = ddsrt_hh_iter_first (gv->topic_defs, &it); tpd1; tpd1 = ddsrt_hh_iter_next (&it))
  {
    if (!strcmp (tpd1->xqos->topic_name, topic_name))
    {
      if (*tpd == NULL)
        *tpd = tpd1;
      else
      {
        *tpd = NULL;
        ret = DDS_RETCODE_PRECONDITION_NOT_MET;
        break;
      }
    }
  }
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  return ret;
}

static void gc_delete_topic_definition (struct gcreq *gcreq)
{
  struct gc_tpd *gcdata = gcreq->arg;
  struct ddsi_topic_definition *tpd = gcdata->tpd;
  struct ddsi_domaingv *gv = tpd->gv;
  GVLOGDISC ("gcreq_delete_topic_definition(%p)\n", (void *) gcreq);
  builtintopic_write_topic (gv->builtin_topic_interface, tpd, gcdata->timestamp, false);
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
  gcreq_free (gcreq);
}

static void delete_topic_definition_locked (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv *gv = tpd->gv;
  GVLOGDISC ("delete_topic_definition_locked (%p) ", tpd);
  ddsrt_hh_remove_present (gv->topic_defs, tpd);
  GVLOGDISC ("- deleting\n");
  gcreq_topic_definition (tpd, timestamp);
}


/* PROXY-TOPIC --------------------------------------------------- */

static int proxy_topic_equal (const struct proxy_topic *proxy_tp_a, const struct proxy_topic *proxy_tp_b)
{
  if (proxy_tp_a != NULL && proxy_tp_b != NULL)
    return topic_definition_equal (proxy_tp_a->definition, proxy_tp_b->definition);
  return proxy_tp_a == proxy_tp_b;
}

struct proxy_topic *lookup_proxy_topic (struct proxy_participant *proxypp, const ddsi_guid_t *guid)
{
  assert (proxypp != NULL);
  proxy_topic_list_iter_t it;
  struct proxy_topic *ptp = NULL;
  ddsrt_mutex_lock (&proxypp->e.lock);
  for (struct proxy_topic *proxytp = proxy_topic_list_iter_first (&proxypp->topics, &it); proxytp != NULL && !ptp; proxytp = proxy_topic_list_iter_next (&it))
  {
    if (proxytp->entityid.u == guid->entityid.u)
      ptp = proxytp;
  }
  ddsrt_mutex_unlock (&proxypp->e.lock);
  return ptp;
}

void new_proxy_topic (struct proxy_participant *proxypp, seqno_t seq, const ddsi_guid_t *guid, const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id_complete, struct dds_qos *qos, ddsrt_wctime_t timestamp)
{
  assert (proxypp != NULL);
  struct ddsi_domaingv *gv = proxypp->e.gv;
  bool new_tpd = false;
  struct ddsi_topic_definition *tpd;
  if (!ddsi_typeid_is_none (type_id_complete))
    tpd = ref_topic_definition (gv, NULL, type_id_complete, qos, &new_tpd);
  else
  {
    assert (!ddsi_typeid_is_none (type_id_minimal));
    tpd = ref_topic_definition (gv, NULL, type_id_minimal, qos, &new_tpd);
  }
#ifndef NDEBUG
  bool found_proxytp = lookup_proxy_topic (proxypp, guid);
  assert (!found_proxytp);
#endif
  struct proxy_topic *proxytp = ddsrt_malloc (sizeof (*proxytp));
  proxytp->entityid = guid->entityid;
  proxytp->definition = tpd;
  proxytp->seq = seq;
  proxytp->tupdate = timestamp;
  proxytp->deleted = 0;
  ddsrt_mutex_lock (&proxypp->e.lock);
  proxy_topic_list_insert (&proxypp->topics, proxytp);
  ddsrt_mutex_unlock (&proxypp->e.lock);
  if (new_tpd)
  {
    builtintopic_write_topic (gv->builtin_topic_interface, tpd, timestamp, true);

    ddsrt_mutex_lock (&gv->new_topic_lock);
    gv->new_topic_version++;
    ddsrt_cond_broadcast (&gv->new_topic_cond);
    ddsrt_mutex_unlock (&gv->new_topic_lock);
  }
}

void update_proxy_topic (struct proxy_participant *proxypp, struct proxy_topic *proxytp, seqno_t seq, struct dds_qos *xqos, ddsrt_wctime_t timestamp)
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
  uint64_t mask = ddsi_xqos_delta (tpd0->xqos, xqos, QP_CHANGEABLE_MASK & ~(QP_RXO_MASK | QP_PARTITION)) & xqos->present;
  GVLOGDISC ("update_proxy_topic %x delta=%"PRIu64" QOS={", proxytp->entityid.u, mask);
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
  unref_topic_definition_locked (tpd0, timestamp);
  proxytp->definition = tpd1;
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  ddsrt_mutex_unlock (&proxypp->e.lock);
  dds_delete_qos (newqos);
  if (new_tpd)
  {
    builtintopic_write_topic (gv->builtin_topic_interface, tpd1, timestamp, true);

    ddsrt_mutex_lock (&gv->new_topic_lock);
    gv->new_topic_version++;
    ddsrt_cond_broadcast (&gv->new_topic_cond);
    ddsrt_mutex_unlock (&gv->new_topic_lock);
  }
}

static void gc_delete_proxy_topic (struct gcreq *gcreq)
{
  struct gc_proxy_tp *gcdata = gcreq->arg;

  ddsrt_mutex_lock (&gcdata->proxypp->e.lock);
  struct ddsi_domaingv *gv = gcdata->proxypp->e.gv;
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  struct ddsi_topic_definition *tpd = gcdata->proxytp->definition;
  GVLOGDISC ("gc_delete_proxy_topic (%p)\n", (void *) gcdata->proxytp);
  proxy_topic_list_remove (&gcdata->proxypp->topics, gcdata->proxytp);
  unref_topic_definition_locked (tpd, gcdata->timestamp);
  ddsrt_free (gcdata->proxytp);
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  ddsrt_mutex_unlock (&gcdata->proxypp->e.lock);
  ddsrt_free (gcdata);
  gcreq_free (gcreq);
}

int delete_proxy_topic_locked (struct proxy_participant *proxypp, struct proxy_topic *proxytp, ddsrt_wctime_t timestamp)
{
  struct ddsi_domaingv *gv = proxypp->e.gv;
  GVLOGDISC ("delete_proxy_topic_locked (%p) ", proxypp);
  if (proxytp->deleted)
    return DDS_RETCODE_PRECONDITION_NOT_MET;
  proxytp->deleted = 1;
  gcreq_proxy_topic (proxypp, proxytp, timestamp);
  return DDS_RETCODE_OK;
}

#endif /* DDS_HAS_TOPIC_DISCOVERY */

/* PROXY-ENDPOINT --------------------------------------------------- */

static int proxy_endpoint_common_init (struct entity_common *e, struct proxy_endpoint_common *c, enum entity_kind kind, const struct ddsi_guid *guid, ddsrt_wctime_t tcreate, seqno_t seq, struct proxy_participant *proxypp, struct addrset *as, const ddsi_plist_t *plist)
{
  const char *name;
  int ret;

  if (is_builtin_entityid (guid->entityid, proxypp->vendor))
    assert ((plist->qos.present & QP_TYPE_NAME) == 0);
  else
    assert ((plist->qos.present & (QP_TOPIC_NAME | QP_TYPE_NAME)) == (QP_TOPIC_NAME | QP_TYPE_NAME));

  name = (plist->present & PP_ENTITY_NAME) ? plist->entity_name : "";
  entity_common_init (e, proxypp->e.gv, guid, name, kind, tcreate, proxypp->vendor, false);
  c->xqos = ddsi_xqos_dup (&plist->qos);
  c->as = ref_addrset (as);
  c->vendor = proxypp->vendor;
  c->seq = seq;
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (plist->qos.present & QP_TYPE_INFORMATION)
  {
    c->type_pair = ddsrt_malloc (sizeof (*c->type_pair));
    c->type_pair->minimal = ddsi_type_ref_proxy (proxypp->e.gv, plist->qos.type_information, DDSI_TYPEID_KIND_MINIMAL, guid);
    c->type_pair->complete = ddsi_type_ref_proxy (proxypp->e.gv, plist->qos.type_information, DDSI_TYPEID_KIND_COMPLETE, guid);
  }
  else
  {
    c->type_pair = NULL;
  }
  c->type = NULL;
#endif

  if (plist->present & PP_GROUP_GUID)
    c->group_guid = plist->group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));

#ifdef DDS_HAS_SECURITY
  q_omg_get_proxy_endpoint_security_info(e, &proxypp->security_info, plist, &c->security_info);
#endif

  if ((ret = ref_proxy_participant (proxypp, c)) != DDS_RETCODE_OK)
  {
#ifdef DDS_HAS_TYPE_DISCOVERY
    if (c->type_pair != NULL)
    {
      ddsi_type_unreg_proxy (proxypp->e.gv, c->type_pair->minimal, guid);
      ddsi_type_unreg_proxy (proxypp->e.gv, c->type_pair->complete, guid);
      ddsi_type_unref (proxypp->e.gv, c->type_pair->minimal);
      ddsi_type_unref (proxypp->e.gv, c->type_pair->complete);
      ddsrt_free (c->type_pair);
    }
#endif
    ddsi_xqos_fini (c->xqos);
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
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (c->type != NULL)
    ddsi_sertype_unref ((struct ddsi_sertype *) c->type);
#endif
  ddsi_xqos_fini (c->xqos);
  ddsrt_free (c->xqos);
  unref_addrset (c->as);
  entity_common_fini (e);
}

#ifdef DDS_HAS_SHM
struct has_iceoryx_address_helper_arg {
  const ddsi_locator_t *loc_iceoryx_addr;
  bool has_iceoryx_address;
};

static void has_iceoryx_address_helper (const ddsi_xlocator_t *n, void *varg)
{
  struct has_iceoryx_address_helper_arg *arg = varg;
  if (n->c.kind == NN_LOCATOR_KIND_SHEM && memcmp (arg->loc_iceoryx_addr->address, n->c.address, sizeof (arg->loc_iceoryx_addr->address)) == 0)
    arg->has_iceoryx_address = true;
}

static bool has_iceoryx_address (struct ddsi_domaingv * const gv, struct addrset * const as)
{
  if (!gv->config.enable_shm)
    return false;
  else
  {
    struct has_iceoryx_address_helper_arg arg = {
      .loc_iceoryx_addr = &gv->loc_iceoryx_addr,
      .has_iceoryx_address = false
    };
    addrset_forall (as, has_iceoryx_address_helper, &arg);
    return arg.has_iceoryx_address;
  }
}
#endif

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

int new_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const ddsi_plist_t *plist, struct nn_dqueue *dqueue, struct xeventq *evq, ddsrt_wctime_t timestamp, seqno_t seq)
{
  struct proxy_participant *proxypp;
  struct proxy_writer *pwr;
  int isreliable;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  enum nn_reorder_mode reorder_mode;
  int ret;

  assert (is_writer_entityid (guid->entityid));
  assert (entidx_lookup_proxy_writer_guid (gv->entity_index, guid) == NULL);

  if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid)) == NULL)
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
  pwr->last_fragnum = UINT32_MAX;
  pwr->nackfragcount = 1;
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
#ifdef DDS_HAS_SSM
  pwr->supports_ssm = (addrset_contains_ssm (gv, as) && gv->config.allowMulticast & DDSI_AMC_SSM) ? 1 : 0;
#endif
#ifdef DDS_HAS_SHM
  pwr->is_iceoryx = has_iceoryx_address (gv, as) ? 1 : 0;
#endif
  if (plist->present & PP_CYCLONE_REDUNDANT_NETWORKING)
    pwr->redundant_networking = (plist->cyclone_redundant_networking != 0);
  else
    pwr->redundant_networking = proxypp->redundant_networking;

  assert (pwr->c.xqos->present & QP_LIVELINESS);
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    ddsrt_etime_t texpire = ddsrt_etime_add_duration (ddsrt_time_elapsed (), pwr->c.xqos->liveliness.lease_duration);
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

  local_reader_ary_init (&pwr->rdary);

  /* locking the entity prevents matching while the built-in topic hasn't been published yet */
  ddsrt_mutex_lock (&pwr->e.lock);
  entidx_insert_proxy_writer_guid (gv->entity_index, pwr);
  builtintopic_write_endpoint (gv->builtin_topic_interface, &pwr->e, timestamp, true);
  ddsrt_mutex_unlock (&pwr->e.lock);

  match_proxy_writer_with_readers (pwr, tnow);

  ddsrt_mutex_lock (&pwr->e.lock);
  pwr->local_matching_inprogress = 0;
  ddsrt_mutex_unlock (&pwr->e.lock);

  return 0;
}

void update_proxy_writer (struct proxy_writer *pwr, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp)
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
#ifdef DDS_HAS_SSM
      pwr->supports_ssm = (addrset_contains_ssm (pwr->e.gv, as) && pwr->e.gv->config.allowMulticast & DDSI_AMC_SSM) ? 1 : 0;
#endif
      unref_addrset (pwr->c.as);
      ref_addrset (as);
      pwr->c.as = as;
      m = ddsrt_avl_iter_first (&pwr_readers_treedef, &pwr->readers, &iter);
      while (m)
      {
        rd = entidx_lookup_reader_guid (pwr->e.gv->entity_index, &m->rd_guid);
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

void update_proxy_reader (struct proxy_reader *prd, seqno_t seq, struct addrset *as, const struct dds_qos *xqos, ddsrt_wctime_t timestamp)
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
        wr = entidx_lookup_writer_guid (prd->e.gv->entity_index, &wrguid);
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

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (pwr->c.type_pair != NULL)
  {
    ddsi_type_unref (pwr->e.gv, pwr->c.type_pair->minimal);
    ddsi_type_unref (pwr->e.gv, pwr->c.type_pair->complete);
    ddsrt_free (pwr->c.type_pair);
  }
#endif

  while (!ddsrt_avl_is_empty (&pwr->readers))
  {
    struct pwr_rd_match *m = ddsrt_avl_root_non_empty (&pwr_readers_treedef, &pwr->readers);
    ddsrt_avl_delete (&pwr_readers_treedef, &pwr->readers, m);
    reader_drop_connection (&m->rd_guid, pwr);
    update_reader_init_acknack_count (&pwr->e.gv->logconfig, pwr->e.gv->entity_index, &m->rd_guid, m->count);
    free_pwr_rd_match (m);
  }
  local_reader_ary_fini (&pwr->rdary);
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
    lease_free (pwr->lease);
#ifdef DDS_HAS_SECURITY
  q_omg_security_deregister_remote_writer(pwr);
#endif
  proxy_endpoint_common_fini (&pwr->e, &pwr->c);
  nn_defrag_free (pwr->defrag);
  nn_reorder_free (pwr->reorder);
  ddsrt_free (pwr);
}

/* First stage in deleting the proxy writer. In this function the pwr and its member pointers
   will remain valid. The real cleaning-up is done async in gc_delete_proxy_writer. */
int delete_proxy_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit)
{
  struct proxy_writer *pwr;
  DDSRT_UNUSED_ARG (isimplicit);
  GVLOGDISC ("delete_proxy_writer ("PGUIDFMT") ", PGUID (*guid));

  ddsrt_mutex_lock (&gv->lock);
  if ((pwr = entidx_lookup_proxy_writer_guid (gv->entity_index, guid)) == NULL)
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
  builtintopic_write_endpoint (gv->builtin_topic_interface, &pwr->e, timestamp, false);
#ifdef DDS_HAS_TYPE_DISCOVERY
  /* Unregister from type before removing from entity index, because a tl_lookup_reply
     could be pending and will trigger an update of the endpoint matching for all
     endpoints that are registered for the type. This call removes this proxy writer
     from the type's endpoint list. */
  if (pwr->c.type_pair != NULL)
  {
    ddsi_type_unreg_proxy (gv, pwr->c.type_pair->minimal, &pwr->e.guid);
    ddsi_type_unreg_proxy (gv, pwr->c.type_pair->complete, &pwr->e.guid);
  }
#endif
  entidx_remove_proxy_writer_guid (gv->entity_index, pwr);
  ddsrt_mutex_unlock (&gv->lock);
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY && pwr->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
    lease_unregister (pwr->lease);
  if (proxy_writer_set_notalive (pwr, false) != DDS_RETCODE_OK)
    GVLOGDISC ("proxy_writer_set_notalive failed for "PGUIDFMT"\n", PGUID(*guid));
  gcreq_proxy_writer (pwr);
  return DDS_RETCODE_OK;
}

static void proxy_writer_notify_liveliness_change_may_unlock (struct proxy_writer *pwr)
{
  struct alive_state alive_state;
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
  if (entidx_lookup_proxy_writer_guid (pwr->e.gv->entity_index, &pwr->e.guid) == NULL)
  {
    ELOGDISC (pwr, "proxy_writer_set_alive_may_unlock("PGUIDFMT") - not in entity index, pwr deleting\n", PGUID (pwr->e.guid));
    return;
  }

  ddsrt_mutex_lock (&pwr->c.proxypp->e.lock);
  pwr->alive = true;
  pwr->alive_vclock++;
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    if (pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
      proxy_participant_add_pwr_lease_locked (pwr->c.proxypp, pwr);
    else
      lease_set_expiry (pwr->lease, ddsrt_etime_add_duration (ddsrt_time_elapsed (), pwr->lease->tdur));
  }
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
  if (pwr->c.xqos->liveliness.lease_duration != DDS_INFINITY && pwr->c.xqos->liveliness.kind != DDS_LIVELINESS_MANUAL_BY_TOPIC)
    proxy_participant_remove_pwr_lease_locked (pwr->c.proxypp, pwr);
  ddsrt_mutex_unlock (&pwr->c.proxypp->e.lock);

  if (notify)
    proxy_writer_notify_liveliness_change_may_unlock (pwr);
  ddsrt_mutex_unlock (&pwr->e.lock);
  return DDS_RETCODE_OK;
}

/* PROXY-READER ----------------------------------------------------- */

int new_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct addrset *as, const ddsi_plist_t *plist, ddsrt_wctime_t timestamp, seqno_t seq
#ifdef DDS_HAS_SSM
                      , int favours_ssm
#endif
                      )
{
  struct proxy_participant *proxypp;
  struct proxy_reader *prd;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  int ret;

  assert (!is_writer_entityid (guid->entityid));
  assert (entidx_lookup_proxy_reader_guid (gv->entity_index, guid) == NULL);

  if ((proxypp = entidx_lookup_proxy_participant_guid (gv->entity_index, ppguid)) == NULL)
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
#ifdef DDS_HAS_SSM
  prd->favours_ssm = (favours_ssm && gv->config.allowMulticast & DDSI_AMC_SSM) ? 1 : 0;
#endif
#ifdef DDS_HAS_SHM
  prd->is_iceoryx = has_iceoryx_address (gv, as) ? 1 : 0;
#endif
  prd->is_fict_trans_reader = 0;
  prd->receive_buffer_size = proxypp->receive_buffer_size;
  prd->requests_keyhash = (plist->present & PP_CYCLONE_REQUESTS_KEYHASH) && plist->cyclone_requests_keyhash;
  if (plist->present & PP_CYCLONE_REDUNDANT_NETWORKING)
    prd->redundant_networking = (plist->cyclone_redundant_networking != 0);
  else
    prd->redundant_networking = proxypp->redundant_networking;

  ddsrt_avl_init (&prd_writers_treedef, &prd->writers);

#ifdef DDS_HAS_SECURITY
  if (prd->e.guid.entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER)
    prd->filter = volatile_secure_data_filter;
  else
    prd->filter = NULL;
#else
  prd->filter = NULL;
#endif

  /* locking the entity prevents matching while the built-in topic hasn't been published yet */
  ddsrt_mutex_lock (&prd->e.lock);
  entidx_insert_proxy_reader_guid (gv->entity_index, prd);
  builtintopic_write_endpoint (gv->builtin_topic_interface, &prd->e, timestamp, true);
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
    if ((wr = entidx_lookup_writer_guid (prd->e.gv->entity_index, &wrguid)) != NULL)
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

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (prd->c.type_pair != NULL)
  {
    ddsi_type_unref (prd->e.gv, prd->c.type_pair->minimal);
    ddsi_type_unref (prd->e.gv, prd->c.type_pair->complete);
    ddsrt_free (prd->c.type_pair);
  }
#endif

  while (!ddsrt_avl_is_empty (&prd->writers))
  {
    struct prd_wr_match *m = ddsrt_avl_root_non_empty (&prd_writers_treedef, &prd->writers);
    ddsrt_avl_delete (&prd_writers_treedef, &prd->writers, m);
    writer_drop_connection (&m->wr_guid, prd);
    free_prd_wr_match (m);
  }
#ifdef DDS_HAS_SECURITY
  q_omg_security_deregister_remote_reader(prd);
#endif
  proxy_endpoint_common_fini (&prd->e, &prd->c);
  ddsrt_free (prd);
}

int delete_proxy_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid, ddsrt_wctime_t timestamp, int isimplicit)
{
  struct proxy_reader *prd;
  (void)isimplicit;
  GVLOGDISC ("delete_proxy_reader ("PGUIDFMT") ", PGUID (*guid));

  ddsrt_mutex_lock (&gv->lock);
  if ((prd = entidx_lookup_proxy_reader_guid (gv->entity_index, guid)) == NULL)
  {
    ddsrt_mutex_unlock (&gv->lock);
    GVLOGDISC ("- unknown\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  builtintopic_write_endpoint (gv->builtin_topic_interface, &prd->e, timestamp, false);
#ifdef DDS_HAS_TYPE_DISCOVERY
  /* Unregister the proxy guid with the ddsi_type before removing from
     entity index, because a tl_lookup_reply could be pending and will
     trigger an update of the endpoint matching for all endpoints that
     are registered for the type. This removes this proxy writer from
     the endpoint list for the type. */
  if (prd->c.type_pair != NULL)
  {
    ddsi_type_unreg_proxy (gv, prd->c.type_pair->minimal, &prd->e.guid);
    ddsi_type_unreg_proxy (gv, prd->c.type_pair->complete, &prd->e.guid);
  }
#endif
  entidx_remove_proxy_reader_guid (gv->entity_index, prd);
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

#ifdef DDS_HAS_TOPIC_DISCOVERY
static int gcreq_topic (struct topic *tp)
{
  struct gcreq *gcreq = gcreq_new (tp->e.gv->gcreq_queue, gc_delete_topic);
  gcreq->arg = tp;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_topic_definition (struct ddsi_topic_definition *tpd, ddsrt_wctime_t timestamp)
{
  struct gcreq *gcreq = gcreq_new (tpd->gv->gcreq_queue, gc_delete_topic_definition);
  struct gc_tpd *gcdata = ddsrt_malloc (sizeof (*gcdata));
  gcdata->tpd = tpd;
  gcdata->timestamp = timestamp;
  gcreq->arg = gcdata;
  gcreq_enqueue (gcreq);
  return 0;
}

static int gcreq_proxy_topic (struct proxy_participant *proxypp, struct proxy_topic *proxytp, ddsrt_wctime_t timestamp)
{
  struct gcreq *gcreq = gcreq_new (proxytp->definition->gv->gcreq_queue, gc_delete_proxy_topic);
  struct gc_proxy_tp *gcdata = ddsrt_malloc (sizeof (*gcdata));
  gcdata->proxypp = proxypp;
  gcdata->proxytp = proxytp;
  gcdata->timestamp = timestamp;
  gcreq->arg = gcdata;
  gcreq_enqueue (gcreq);
  return 0;
}

#endif

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
