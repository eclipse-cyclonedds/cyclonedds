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
#include <math.h>

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_builtin_topic_if.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__entity.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__participant.h"
#include "ddsi__rhc.h"
#include "ddsi__entity_index.h"
#include "ddsi__mcgroup.h"
#include "ddsi__nwpart.h"
#include "ddsi__udp.h"
#include "ddsi__wraddrset.h"
#include "ddsi__security_omg.h"
#include "ddsi__discovery_endpoint.h"
#include "ddsi__whc.h"
#include "ddsi__xevent.h"
#include "ddsi__addrset.h"
#include "ddsi__radmin.h"
#include "ddsi__misc.h"
#include "ddsi__sysdeps.h"
#include "ddsi__endpoint.h"
#include "ddsi__gc.h"
#include "ddsi__topic.h"
#include "ddsi__tran.h"
#include "ddsi__typelib.h"
#include "ddsi__vendor.h"
#include "ddsi__xqos.h"
#include "ddsi__hbcontrol.h"
#include "ddsi__lease.h"
#include "dds/dds.h"

static dds_return_t delete_writer_nolinger_locked (struct ddsi_writer *wr);
static void augment_wr_prd_match (void *vnode, const void *vleft, const void *vright);

const ddsrt_avl_treedef_t ddsi_wr_readers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_wr_prd_match, avlnode), offsetof (struct ddsi_wr_prd_match, prd_guid), ddsi_compare_guid, augment_wr_prd_match);
const ddsrt_avl_treedef_t ddsi_wr_local_readers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_wr_rd_match, avlnode), offsetof (struct ddsi_wr_rd_match, rd_guid), ddsi_compare_guid, 0);
const ddsrt_avl_treedef_t ddsi_rd_writers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_rd_pwr_match, avlnode), offsetof (struct ddsi_rd_pwr_match, pwr_guid), ddsi_compare_guid, 0);
const ddsrt_avl_treedef_t ddsi_rd_local_writers_treedef =
  DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_rd_wr_match, avlnode), offsetof (struct ddsi_rd_wr_match, wr_guid), ddsi_compare_guid, 0);

int ddsi_is_builtin_volatile_endpoint (ddsi_entityid_t id)
{
  switch (id.u) {
#ifdef DDS_HAS_SECURITY
  case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER:
  case DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER:
    return 1;
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
  case DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER:
  case DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER:
  case DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER:
  case DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_READER:
    return 1;
#endif
  default:
    break;
  }
  return 0;
}

int ddsi_is_builtin_endpoint (ddsi_entityid_t id, ddsi_vendorid_t vendorid)
{
  return ddsi_is_builtin_entityid (id, vendorid) && id.u != DDSI_ENTITYID_PARTICIPANT && !ddsi_is_topic_entityid (id);
}

bool ddsi_is_local_orphan_endpoint (const struct ddsi_entity_common *e)
{
  return (e->guid.prefix.u[0] == 0 && e->guid.prefix.u[1] == 0 && e->guid.prefix.u[2] == 0 &&
          ddsi_is_builtin_endpoint (e->guid.entityid, DDSI_VENDORID_ECLIPSE));
}

int ddsi_is_writer_entityid (ddsi_entityid_t id)
{
  switch (id.u & DDSI_ENTITYID_KIND_MASK)
  {
    case DDSI_ENTITYID_KIND_WRITER_WITH_KEY:
    case DDSI_ENTITYID_KIND_WRITER_NO_KEY:
      return 1;
    default:
      return 0;
  }
}

int ddsi_is_reader_entityid (ddsi_entityid_t id)
{
  switch (id.u & DDSI_ENTITYID_KIND_MASK)
  {
    case DDSI_ENTITYID_KIND_READER_WITH_KEY:
    case DDSI_ENTITYID_KIND_READER_NO_KEY:
      return 1;
    default:
      return 0;
  }
}

int ddsi_is_keyed_endpoint_entityid (ddsi_entityid_t id)
{
  switch (id.u & DDSI_ENTITYID_KIND_MASK)
  {
    case DDSI_ENTITYID_KIND_READER_WITH_KEY:
    case DDSI_ENTITYID_KIND_WRITER_WITH_KEY:
      return 1;
    case DDSI_ENTITYID_KIND_READER_NO_KEY:
    case DDSI_ENTITYID_KIND_WRITER_NO_KEY:
      return 0;
    default:
      return 0;
  }
}

void ddsi_make_writer_info(struct ddsi_writer_info *wrinfo, const struct ddsi_entity_common *e, const struct dds_qos *xqos, uint32_t statusinfo)
{
#ifndef DDS_HAS_LIFESPAN
  DDSRT_UNUSED_ARG (statusinfo);
#endif
  wrinfo->guid = e->guid;
  wrinfo->ownership_strength = xqos->ownership_strength.value;
  wrinfo->auto_dispose = xqos->writer_data_lifecycle.autodispose_unregistered_instances;
  wrinfo->iid = e->iid;
#ifdef DDS_HAS_LIFESPAN
  if (xqos->lifespan.duration != DDS_INFINITY && (statusinfo & (DDSI_STATUSINFO_UNREGISTER | DDSI_STATUSINFO_DISPOSE)) == 0)
    wrinfo->lifespan_exp = ddsrt_mtime_add_duration(ddsrt_time_monotonic(), xqos->lifespan.duration);
  else
    wrinfo->lifespan_exp = DDSRT_MTIME_NEVER;
#endif
}

static uint32_t get_min_receive_buffer_size (struct ddsi_writer *wr)
{
  uint32_t min_receive_buffer_size = UINT32_MAX;
  struct ddsi_entity_index *gh = wr->e.gv->entity_index;
  ddsrt_avl_iter_t it;
  for (struct ddsi_wr_prd_match *m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_reader *prd;
    if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
      continue;
    if (prd->receive_buffer_size < min_receive_buffer_size)
      min_receive_buffer_size = prd->receive_buffer_size;
  }
  return min_receive_buffer_size;
}

void ddsi_rebuild_writer_addrset (struct ddsi_writer *wr)
{
  /* FIXME: way too inefficient in this form:
     - it gets computed for every change
     - in many cases the set of addresses from the readers
       is identical, so we could cache the results */

  /* only one operation at a time */
  ASSERT_MUTEX_HELD (&wr->e.lock);

  /* swap in new address set; this simple procedure is ok as long as
     wr->as is never accessed without the wr->e.lock held */
  struct ddsi_addrset * const oldas = wr->as;
  wr->as = ddsi_compute_writer_addrset (wr);
  ddsi_unref_addrset (oldas);

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

  ELOGDISC (wr, "ddsi_rebuild_writer_addrset("PGUIDFMT"):", PGUID (wr->e.guid));
  ddsi_log_addrset(wr->e.gv, DDS_LC_DISCOVERY, "", wr->as);
  ELOGDISC (wr, " (burst size %"PRIu32" rexmit %"PRIu32")\n", wr->init_burst_size_limit, wr->rexmit_burst_size_limit);
}

static void writer_get_alive_state_locked (struct ddsi_writer *wr, struct ddsi_alive_state *st)
{
  st->alive = wr->alive;
  st->vclock = wr->alive_vclock;
}

void ddsi_writer_get_alive_state (struct ddsi_writer *wr, struct ddsi_alive_state *st)
{
  ddsrt_mutex_lock (&wr->e.lock);
  writer_get_alive_state_locked (wr, st);
  ddsrt_mutex_unlock (&wr->e.lock);
}

static void reader_update_notify_alive_state_invoke_cb (struct ddsi_reader *rd, uint64_t iid, bool notify, int delta, const struct ddsi_alive_state *alive_state)
{
  /* Liveliness changed events can race each other and can, potentially, be delivered
   in a different order. */
  if (notify && rd->status_cb)
  {
    ddsi_status_cb_data_t data;
    data.handle = iid;
    data.raw_status_id = (int) DDS_LIVELINESS_CHANGED_STATUS_ID;
    if (delta < 0) {
      data.extra = (uint32_t) DDSI_LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE;
      (rd->status_cb) (rd->status_cb_entity, &data);
    } else if (delta > 0) {
      data.extra = (uint32_t) DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE;
      (rd->status_cb) (rd->status_cb_entity, &data);
    } else {
      /* Twitch: the resulting (proxy)writer state is unchanged, but there has been
        a transition to another state and back to the current state. So we'll call
        the callback twice in this case. */
      static const enum ddsi_liveliness_changed_data_extra x[] = {
        DDSI_LIVELINESS_CHANGED_NOT_ALIVE_TO_ALIVE,
        DDSI_LIVELINESS_CHANGED_ALIVE_TO_NOT_ALIVE
      };
      data.extra = (uint32_t) x[alive_state->alive];
      (rd->status_cb) (rd->status_cb_entity, &data);
      data.extra = (uint32_t) x[!alive_state->alive];
      (rd->status_cb) (rd->status_cb_entity, &data);
    }
  }
}

void ddsi_reader_update_notify_wr_alive_state (struct ddsi_reader *rd, const struct ddsi_writer *wr, const struct ddsi_alive_state *alive_state)
{
  struct ddsi_rd_wr_match *m;
  bool notify = false;
  int delta = 0; /* -1: alive -> not_alive; 0: unchanged; 1: not_alive -> alive */
  ddsrt_mutex_lock (&rd->e.lock);
  if ((m = ddsrt_avl_lookup (&ddsi_rd_local_writers_treedef, &rd->local_writers, &wr->e.guid)) != NULL)
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
    ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, DDSI_STATUSINFO_UNREGISTER);
    ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
  }

  reader_update_notify_alive_state_invoke_cb (rd, wr->e.iid, notify, delta, alive_state);
}

static void ddsi_reader_update_notify_wr_alive_state_guid (const struct ddsi_guid *rd_guid, const struct ddsi_writer *wr, const struct ddsi_alive_state *alive_state)
{
  struct ddsi_reader *rd;
  if ((rd = ddsi_entidx_lookup_reader_guid (wr->e.gv->entity_index, rd_guid)) != NULL)
    ddsi_reader_update_notify_wr_alive_state (rd, wr, alive_state);
}

void ddsi_reader_update_notify_pwr_alive_state (struct ddsi_reader *rd, const struct ddsi_proxy_writer *pwr, const struct ddsi_alive_state *alive_state)
{
  struct ddsi_rd_pwr_match *m;
  bool notify = false;
  int delta = 0; /* -1: alive -> not_alive; 0: unchanged; 1: not_alive -> alive */
  ddsrt_mutex_lock (&rd->e.lock);
  if ((m = ddsrt_avl_lookup (&ddsi_rd_writers_treedef, &rd->writers, &pwr->e.guid)) != NULL)
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
    ddsi_make_writer_info (&wrinfo, &pwr->e, pwr->c.xqos, DDSI_STATUSINFO_UNREGISTER);
    ddsi_rhc_unregister_wr (rd->rhc, &wrinfo);
  }

  reader_update_notify_alive_state_invoke_cb (rd, pwr->e.iid, notify, delta, alive_state);
}

void ddsi_reader_update_notify_pwr_alive_state_guid (const struct ddsi_guid *rd_guid, const struct ddsi_proxy_writer *pwr, const struct ddsi_alive_state *alive_state)
{
  struct ddsi_reader *rd;
  if ((rd = ddsi_entidx_lookup_reader_guid (pwr->e.gv->entity_index, rd_guid)) != NULL)
    ddsi_reader_update_notify_pwr_alive_state (rd, pwr, alive_state);
}

void ddsi_update_reader_init_acknack_count (const ddsrt_log_cfg_t *logcfg, const struct ddsi_entity_index *entidx, const struct ddsi_guid *rd_guid, ddsi_count_t count)
{
  struct ddsi_reader *rd;

  /* Update the initial acknack sequence number for the reader.  See
     also ddsi_reader_add_connection(). */
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "ddsi_update_reader_init_acknack_count ("PGUIDFMT", %"PRIu32"): ", PGUID (*rd_guid), count);
  if ((rd = ddsi_entidx_lookup_reader_guid (entidx, rd_guid)) != NULL)
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

void ddsi_deliver_historical_data (const struct ddsi_writer *wr, const struct ddsi_reader *rd)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  struct ddsi_tkmap * const tkmap = gv->m_tkmap;
  struct ddsi_whc_sample_iter it;
  struct ddsi_whc_borrowed_sample sample;
  /* FIXME: should limit ourselves to what it is available because of durability history, not writer history */
  ddsi_whc_sample_iter_init (wr->whc, &it);
  while (ddsi_whc_sample_iter_borrow_next (&it, &sample))
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

static void new_reader_writer_common (const struct ddsrt_log_cfg *logcfg, const struct ddsi_guid *guid, const char *topic_name, const char *type_name, const struct dds_qos *xqos)
{
  const char *partition = "(default)";
  const char *partition_suffix = "";
  assert (topic_name != NULL);
  assert (type_name != NULL);
  if (ddsi_is_builtin_entityid (guid->entityid, DDSI_VENDORID_ECLIPSE))
  {
    /* continue printing it as not being in a partition, the actual
       value doesn't matter because it is never matched based on QoS
       settings */
    partition = "(null)";
  }
  else if ((xqos->present & DDSI_QP_PARTITION) && xqos->partition.n > 0 && strcmp (xqos->partition.strs[0], "") != 0)
  {
    partition = xqos->partition.strs[0];
    if (xqos->partition.n > 1)
      partition_suffix = "+";
  }
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "new_%s(guid "PGUIDFMT", %s%s.%s/%s)\n",
            ddsi_is_writer_entityid (guid->entityid) ? "writer" : "reader",
            PGUID (*guid),
            partition, partition_suffix,
            topic_name,
            type_name);
}

static bool is_onlylocal_endpoint (struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos)
{
  if (ddsi_builtintopic_is_builtintopic (pp->e.gv->builtin_topic_interface, type))
    return true;
#ifdef DDS_HAS_NETWORK_PARTITIONS
  if (ddsi_is_ignored_nwpart (pp->e.gv, xqos, topic_name))
    return true;
#endif
  return false;
}

static void endpoint_common_init (struct ddsi_entity_common *e, struct ddsi_endpoint_common *c, struct ddsi_domaingv *gv, enum ddsi_entity_kind kind, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, bool onlylocal, const struct ddsi_sertype *sertype)
{
#ifndef DDS_HAS_TYPE_DISCOVERY
  DDSRT_UNUSED_ARG (sertype);
#endif
  ddsi_entity_common_init (e, gv, guid, kind, ddsrt_time_wallclock (), DDSI_VENDORID_ECLIPSE, pp->e.onlylocal || onlylocal);
  c->pp = ddsi_ref_participant (pp, &e->guid);
  if (group_guid)
    c->group_guid = *group_guid;
  else
    memset (&c->group_guid, 0, sizeof (c->group_guid));

#ifdef DDS_HAS_TYPE_DISCOVERY
  c->type_pair = ddsrt_malloc (sizeof (*c->type_pair));

  /* Referencing the top-level type shouldn't fail at this point. The sertype that is passed,
     and which is used to get the type_info and type_map from, is from the topic (or derived
     from the topic's sertype using the same type descriptor). The topic's sertype is already
     referenced (and therefore validated and in the type-lib) during topic creation. */
  dds_return_t ret;
  ret = ddsi_type_ref_local (pp->e.gv, &c->type_pair->minimal, sertype, DDSI_TYPEID_KIND_MINIMAL);
  assert (ret == DDS_RETCODE_OK);
  ret = ddsi_type_ref_local (pp->e.gv, &c->type_pair->complete, sertype, DDSI_TYPEID_KIND_COMPLETE);
  assert (ret == DDS_RETCODE_OK);
  (void) ret;
#endif
}

static void endpoint_common_fini (struct ddsi_entity_common *e, struct ddsi_endpoint_common *c)
{
  if (!ddsi_is_builtin_entityid(e->guid.entityid, DDSI_VENDORID_ECLIPSE))
    ddsi_participant_release_entityid(c->pp, e->guid.entityid);
  if (c->pp)
  {
    ddsi_unref_participant (c->pp, &e->guid);
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
    assert (ddsi_is_local_orphan_endpoint (e));
  }
  ddsi_entity_common_fini (e);
}

static void augment_wr_prd_match (void *vnode, const void *vleft, const void *vright)
{
  struct ddsi_wr_prd_match *n = vnode;
  const struct ddsi_wr_prd_match *left = vleft;
  const struct ddsi_wr_prd_match *right = vright;
  ddsi_seqno_t min_seq, max_seq;
  int have_replied = n->has_replied_to_hb;

  /* note: this means min <= seq, but not min <= max nor seq <= max!
     note: this guarantees max < DDSI_MAX_SEQ_NUMBER, which by induction
     guarantees {left,right}.max < DDSI_MAX_SEQ_NUMBER note: this treats a
     reader that has not yet replied to a heartbeat as a demoted
     one */
  min_seq = n->seq;
  max_seq = (n->seq < DDSI_MAX_SEQ_NUMBER) ? n->seq : 0;

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
  else if (n->is_reliable && (n->seq == DDSI_MAX_SEQ_NUMBER || n->seq == 0 || !n->has_replied_to_hb))
  {
    /* demoted readers and reliable readers that have not yet replied to a heartbeat are candidates */
    n->arbitrary_unacked_reader = n->prd_guid;
  }
  /* 3b: maybe we can inherit from the children */
  else if (left && left->arbitrary_unacked_reader.entityid.u != DDSI_ENTITYID_UNKNOWN)
  {
    n->arbitrary_unacked_reader = left->arbitrary_unacked_reader;
  }
  else if (right && right->arbitrary_unacked_reader.entityid.u != DDSI_ENTITYID_UNKNOWN)
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
    n->arbitrary_unacked_reader.entityid.u = DDSI_ENTITYID_UNKNOWN;
  }
}


/* WRITER ----------------------------------------------------------- */

ddsi_seqno_t ddsi_writer_max_drop_seq (const struct ddsi_writer *wr)
{
  const struct ddsi_wr_prd_match *n;
  if (ddsrt_avl_is_empty (&wr->readers))
    return wr->seq;
  n = ddsrt_avl_root_non_empty (&ddsi_wr_readers_treedef, &wr->readers);
  return (n->min_seq == DDSI_MAX_SEQ_NUMBER) ? wr->seq : n->min_seq;
}

int ddsi_writer_must_have_hb_scheduled (const struct ddsi_writer *wr, const struct ddsi_whc_state *whcst)
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
  else if (!((const struct ddsi_wr_prd_match *) ddsrt_avl_root_non_empty (&ddsi_wr_readers_treedef, &wr->readers))->all_have_replied_to_hb)
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
    return ddsi_writer_max_drop_seq (wr) < whcst->max_seq;
  }
}

void ddsi_writer_set_retransmitting (struct ddsi_writer *wr)
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

void ddsi_writer_clear_retransmitting (struct ddsi_writer *wr)
{
  wr->retransmitting = 0;
  wr->t_whc_high_upd = wr->t_rexmit_end = ddsrt_time_elapsed();
  wr->time_retransmit += (uint64_t) (wr->t_rexmit_end.v - wr->t_rexmit_start.v);
  ddsrt_cond_broadcast (&wr->throttle_cond);
}

unsigned ddsi_remove_acked_messages (struct ddsi_writer *wr, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list)
{
  unsigned n;
  assert (wr->e.guid.entityid.u != DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER);
  ASSERT_MUTEX_HELD (&wr->e.lock);
  n = ddsi_whc_remove_acked_messages (wr->whc, ddsi_writer_max_drop_seq (wr), whcst, deferred_free_list);
  /* trigger anyone waiting in throttle_writer() or wait_for_acks() */
  ddsrt_cond_broadcast (&wr->throttle_cond);
  if (wr->retransmitting && whcst->unacked_bytes == 0)
    ddsi_writer_clear_retransmitting (wr);
  if (wr->state == WRST_LINGERING && whcst->unacked_bytes == 0)
  {
    ELOGDISC (wr, "remove_acked_messages: deleting lingering writer "PGUIDFMT"\n", PGUID (wr->e.guid));
    delete_writer_nolinger_locked (wr);
  }
  return n;
}

static void writer_notify_liveliness_change_may_unlock (struct ddsi_writer *wr)
{
  struct ddsi_alive_state alive_state;
  writer_get_alive_state_locked (wr, &alive_state);

  struct ddsi_guid rdguid;
  struct ddsi_pwr_rd_match *m;
  memset (&rdguid, 0, sizeof (rdguid));
  while (wr->alive_vclock == alive_state.vclock &&
         (m = ddsrt_avl_lookup_succ (&ddsi_wr_local_readers_treedef, &wr->local_readers, &rdguid)) != NULL)
  {
    rdguid = m->rd_guid;
    ddsrt_mutex_unlock (&wr->e.lock);
    /* unlocking pwr means alive state may have changed already; we break out of the loop once we
       detect this but there for the reader in the current iteration, anything is possible */
    ddsi_reader_update_notify_wr_alive_state_guid (&rdguid, wr, &alive_state);
    ddsrt_mutex_lock (&wr->e.lock);
  }
}

void ddsi_writer_set_alive_may_unlock (struct ddsi_writer *wr, bool notify)
{
  /* Caller has wr->e.lock, so we can safely read wr->alive.  Updating wr->alive requires
     also taking wr->c.pp->e.lock because wr->alive <=> (wr->lease in pp's lease heap). */
  assert (!wr->alive);

  /* check that writer still exists (when deleting it is removed from guid hash) */
  if (ddsi_entidx_lookup_writer_guid (wr->e.gv->entity_index, &wr->e.guid) == NULL)
  {
    ELOGDISC (wr, "ddsi_writer_set_alive_may_unlock("PGUIDFMT") - not in entity index, wr deleting\n", PGUID (wr->e.guid));
    return;
  }

  ddsrt_mutex_lock (&wr->c.pp->e.lock);
  wr->alive = true;
  wr->alive_vclock++;
  if (wr->xqos->liveliness.lease_duration != DDS_INFINITY)
  {
    if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
      ddsi_participant_add_wr_lease_locked (wr->c.pp, wr);
    else if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
      ddsi_lease_set_expiry (wr->lease, ddsrt_etime_add_duration (ddsrt_time_elapsed (), wr->lease->tdur));
  }
  ddsrt_mutex_unlock (&wr->c.pp->e.lock);

  if (notify)
    writer_notify_liveliness_change_may_unlock (wr);
}

static int writer_set_notalive_locked (struct ddsi_writer *wr, bool notify)
{
  if (!wr->alive)
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  /* To update wr->alive, both wr->e.lock and wr->c.pp->e.lock
     should be taken */
  ddsrt_mutex_lock (&wr->c.pp->e.lock);
  wr->alive = false;
  wr->alive_vclock++;
  if (wr->xqos->liveliness.lease_duration != DDS_INFINITY && wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
    ddsi_participant_remove_wr_lease_locked (wr->c.pp, wr);
  ddsrt_mutex_unlock (&wr->c.pp->e.lock);

  if (notify)
  {
    if (wr->status_cb)
    {
      ddsi_status_cb_data_t data;
      data.handle = wr->e.iid;
      data.raw_status_id = (int) DDS_LIVELINESS_LOST_STATUS_ID;
      (wr->status_cb) (wr->status_cb_entity, &data);
    }
    writer_notify_liveliness_change_may_unlock (wr);
  }
  return DDS_RETCODE_OK;
}

int ddsi_writer_set_notalive (struct ddsi_writer *wr, bool notify)
{
  ddsrt_mutex_lock (&wr->e.lock);
  int ret = writer_set_notalive_locked(wr, notify);
  ddsrt_mutex_unlock (&wr->e.lock);
  return ret;
}

static void ddsi_new_writer_guid_common_init (struct ddsi_writer *wr, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc *whc, ddsi_status_cb_t status_cb, void * status_entity)
{
  ddsrt_cond_init (&wr->throttle_cond);
  wr->seq = 0;
  ddsrt_atomic_st64 (&wr->seq_xmit, (uint64_t) 0);
  wr->hbcount = 1;
  wr->state = WRST_OPERATIONAL;
  wr->hbfragcount = 1;
  ddsi_writer_hbcontrol_init (&wr->hbcontrol);
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
#ifdef DDS_HAS_SHM
  wr->has_iceoryx = (0x0 == (xqos->ignore_locator_type & DDSI_LOCATOR_KIND_SHEM));
#endif
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
  ddsi_set_topic_type_name (wr->xqos, topic_name, type->type_name);

  ELOGDISC (wr, "WRITER "PGUIDFMT" QOS={", PGUID (wr->e.guid));
  ddsi_xqos_log (DDS_LC_DISCOVERY, &wr->e.gv->logconfig, wr->xqos);
  ELOGDISC (wr, "}\n");

  assert (wr->xqos->present & DDSI_QP_RELIABILITY);
  wr->reliable = (wr->xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  assert (wr->xqos->present & DDSI_QP_DURABILITY);
#ifdef DDS_HAS_TYPE_DISCOVERY
  if (ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE) &&
      wr->e.guid.entityid.u != DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER
      && wr->e.guid.entityid.u != DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER
      && wr->e.guid.entityid.u != DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER)
#else
  if (ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE) &&
      wr->e.guid.entityid.u != DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER)
#endif
  {
    assert (wr->xqos->history.kind == DDS_HISTORY_KEEP_LAST);
    assert ((wr->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL) ||
            (wr->e.guid.entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER));
  }
  wr->handle_as_transient_local = (wr->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL);
  wr->num_readers_requesting_keyhash +=
    wr->e.gv->config.generate_keyhash &&
    ((wr->e.guid.entityid.u & DDSI_ENTITYID_KIND_MASK) == DDSI_ENTITYID_KIND_WRITER_WITH_KEY);
  wr->type = ddsi_sertype_ref (type);
  wr->as = ddsi_new_addrset ();

#ifdef DDS_HAS_NETWORK_PARTITIONS
  /* This is an open issue how to encrypt mesages send for various
     partitions that match multiple network partitions.  From a safety
     point of view a wierd configuration. Here we chose the first one
     that we find */
  wr->network_partition = ddsi_get_nwpart_from_mapping (&wr->e.gv->logconfig, &wr->e.gv->config, wr->xqos, wr->xqos->topic_name);
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
      wr->ssm_as = ddsi_new_addrset ();
      ddsi_add_xlocator_to_addrset (wr->e.gv, wr->ssm_as, &loc);
      ELOGDISC (wr, "writer "PGUIDFMT": ssm=%d", PGUID (wr->e.guid), wr->supports_ssm);
      ddsi_log_addrset (wr->e.gv, DDS_LC_DISCOVERY, "", wr->ssm_as);
      ELOGDISC (wr, "\n");
    }
  }
#endif

  wr->evq = wr->e.gv->xevents;

  /* heartbeat event will be deleted when the handler can't find a
     writer for it in the hash table. NEVER => won't ever be
     scheduled, and this can only change by writing data, which won't
     happen until after it becomes visible. */
  if (!wr->reliable)
    wr->heartbeat_xevent = NULL;
  else
  {
    struct ddsi_heartbeat_xevent_cb_arg arg = {.wr_guid = wr->e.guid };
    wr->heartbeat_xevent = ddsi_qxev_callback (wr->evq, DDSRT_MTIME_NEVER, ddsi_heartbeat_xevent_cb, &arg, sizeof (arg), false);
  }

  assert (wr->xqos->present & DDSI_QP_LIVELINESS);
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
  assert (!(ddsi_is_builtin_entityid(wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE) && !ddsi_is_builtin_volatile_endpoint(wr->e.guid.entityid)) ||
           (wr->whc_low == wr->whc_high && wr->whc_low == INT32_MAX));

  /* Connection admin */
  ddsrt_avl_init (&ddsi_wr_readers_treedef, &wr->readers);
  ddsrt_avl_init (&ddsi_wr_local_readers_treedef, &wr->local_readers);

  ddsi_local_reader_ary_init (&wr->rdary);
}

dds_return_t ddsi_new_writer_guid (struct ddsi_writer **wr_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc *whc, ddsi_status_cb_t status_cb, void *status_entity)
{
  struct ddsi_writer *wr;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  assert (ddsi_is_writer_entityid (guid->entityid));
  assert (ddsi_entidx_lookup_writer_guid (pp->e.gv->entity_index, guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (&pp->e.gv->logconfig, guid, topic_name, type->type_name, xqos);
  wr = ddsrt_malloc (sizeof (*wr));
  if (wr_out)
    *wr_out = wr;

  /* want a pointer to the participant so that a parallel call to
   delete_participant won't interfere with our ability to address
   the participant */

  const bool onlylocal = is_onlylocal_endpoint (pp, topic_name, type, xqos);
  endpoint_common_init (&wr->e, &wr->c, pp->e.gv, DDSI_EK_WRITER, guid, group_guid, pp, onlylocal, type);
  ddsi_new_writer_guid_common_init(wr, topic_name, type, xqos, whc, status_cb, status_entity);

#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_register_writer (wr);
#endif

  /* entity_index needed for protocol handling, so add it before we send
   out our first message.  Also: needed for matching, and swapping
   the order if hash insert & matching creates a window during which
   neither of two endpoints being created in parallel can discover
   the other. */
  ddsrt_mutex_lock (&wr->e.lock);
  ddsi_entidx_insert_writer_guid (pp->e.gv->entity_index, wr);
  ddsi_builtintopic_write_endpoint (wr->e.gv->builtin_topic_interface, &wr->e, ddsrt_time_wallclock(), true);
  ddsrt_mutex_unlock (&wr->e.lock);

  /* once it exists, match it with proxy writers and broadcast
   existence (I don't think it matters much what the order of these
   two is, but it seems likely that match-then-broadcast has a
   slightly lower likelihood that a response from a proxy reader
   gets dropped) -- but note that without adding a lock it might be
   deleted while we do so */
  ddsi_match_writer_with_proxy_readers (wr, tnow);
  ddsi_match_writer_with_local_readers (wr, tnow);
  ddsi_sedp_write_writer (wr);

  if (wr->lease_duration != NULL)
  {
    assert (wr->lease_duration->ldur != DDS_INFINITY);
    assert (!ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE));
    if (wr->xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC)
    {
      /* Store writer lease duration in participant's heap in case of automatic liveliness */
      ddsrt_mutex_lock (&pp->e.lock);
      ddsrt_fibheap_insert (&ddsi_ldur_fhdef, &pp->ldur_auto_wr, wr->lease_duration);
      ddsrt_mutex_unlock (&pp->e.lock);

      /* Trigger pmd update */
      (void) ddsi_resched_xevent_if_earlier (pp->pmd_update_xevent, ddsrt_time_monotonic ());
    }
    else
    {
      ddsrt_etime_t texpire = ddsrt_etime_add_duration (ddsrt_time_elapsed (), wr->lease_duration->ldur);
      wr->lease = ddsi_lease_new (texpire, wr->lease_duration->ldur, &wr->e);
      if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT)
      {
        ddsrt_mutex_lock (&pp->e.lock);
        ddsi_participant_add_wr_lease_locked (pp, wr);
        ddsrt_mutex_unlock (&pp->e.lock);
      }
      else
      {
        ddsi_lease_register (wr->lease);
      }
    }
  }
  else
  {
    wr->lease = NULL;
  }

  return 0;
}

dds_return_t ddsi_new_writer (struct ddsi_writer **wr_out, struct ddsi_guid *wrguid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc * whc, ddsi_status_cb_t status_cb, void *status_cb_arg)
{
  dds_return_t rc;
  uint32_t kind;

  /* participant can't be freed while we're mucking around cos we are
     awake and do not touch the thread's vtime (entidx_lookup already
     verifies we're awake) */
  wrguid->prefix = pp->e.guid.prefix;
  kind = type->typekind_no_key ? DDSI_ENTITYID_KIND_WRITER_NO_KEY : DDSI_ENTITYID_KIND_WRITER_WITH_KEY;
  if ((rc = ddsi_participant_allocate_entityid (&wrguid->entityid, kind, pp)) < 0)
    return rc;
  return ddsi_new_writer_guid (wr_out, wrguid, group_guid, pp, topic_name, type, xqos, whc, status_cb, status_cb_arg);
}

struct ddsi_local_orphan_writer *ddsi_new_local_orphan_writer (struct ddsi_domaingv *gv, ddsi_entityid_t entityid, const char *topic_name, struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_whc *whc)
{
  ddsi_guid_t guid;
  struct ddsi_local_orphan_writer *lowr;
  struct ddsi_writer *wr;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  GVLOGDISC ("ddsi_new_local_orphan_writer(%s/%s)\n", topic_name, type->type_name);
  lowr = ddsrt_malloc (sizeof (*lowr));
  wr = &lowr->wr;

  memset (&guid.prefix, 0, sizeof (guid.prefix));
  guid.entityid = entityid;
  ddsi_entity_common_init (&wr->e, gv, &guid, DDSI_EK_WRITER, ddsrt_time_wallclock (), DDSI_VENDORID_ECLIPSE, true);
  wr->c.pp = NULL;
  memset (&wr->c.group_guid, 0, sizeof (wr->c.group_guid));

#ifdef DDS_HAS_TYPE_DISCOVERY
  wr->c.type_pair = NULL;
#endif

  ddsi_new_writer_guid_common_init (wr, topic_name, type, xqos, whc, 0, NULL);
  ddsi_entidx_insert_writer_guid (gv->entity_index, wr);
  ddsi_builtintopic_write_endpoint (gv->builtin_topic_interface, &wr->e, ddsrt_time_wallclock(), true);
  ddsi_match_writer_with_local_readers (wr, tnow);
  return lowr;
}

void ddsi_update_writer_qos (struct ddsi_writer *wr, const dds_qos_t *xqos)
{
  ddsrt_mutex_lock (&wr->e.lock);
  if (ddsi_update_qos_locked (&wr->e, wr->xqos, xqos, ddsrt_time_wallclock ()))
    ddsi_sedp_write_writer (wr);
  ddsrt_mutex_unlock (&wr->e.lock);
}

static void gc_delete_writer (struct ddsi_gcreq *gcreq)
{
  struct ddsi_writer *wr = gcreq->arg;
  ELOGDISC (wr, "gc_delete_writer(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (wr->e.guid));
  ddsi_gcreq_free (gcreq);

  /* We now allow GC while blocked on a full WHC, but we still don't allow deleting a writer while blocked on it. The writer's state must be DELETING by the time we get here, and that means the transmit path is no longer blocked. It doesn't imply that the write thread is no longer in throttle_writer(), just that if it is, it will soon return from there. Therefore, block until it isn't throttling anymore. We can safely lock the writer, as we're on the separate GC thread. */
  assert (wr->state == WRST_DELETING);
  assert (!wr->throttling);

  if (wr->heartbeat_xevent)
  {
    wr->hbcontrol.tsched = DDSRT_MTIME_NEVER;
    ddsi_delete_xevent (wr->heartbeat_xevent);
  }

  /* Tear down connections -- no proxy reader can be adding/removing
      us now, because we can't be found via entity_index anymore.  We
      therefore need not take lock. */

  while (!ddsrt_avl_is_empty (&wr->readers))
  {
    struct ddsi_wr_prd_match *m = ddsrt_avl_root_non_empty (&ddsi_wr_readers_treedef, &wr->readers);
    ddsrt_avl_delete (&ddsi_wr_readers_treedef, &wr->readers, m);
    ddsi_proxy_reader_drop_connection (&m->prd_guid, wr);
    ddsi_free_wr_prd_match (wr->e.gv, &wr->e.guid, m);
  }
  while (!ddsrt_avl_is_empty (&wr->local_readers))
  {
    struct ddsi_wr_rd_match *m = ddsrt_avl_root_non_empty (&ddsi_wr_local_readers_treedef, &wr->local_readers);
    ddsrt_avl_delete (&ddsi_wr_local_readers_treedef, &wr->local_readers, m);
    ddsi_reader_drop_local_connection (&m->rd_guid, wr);
    ddsi_free_wr_rd_match (m);
  }
  if (wr->lease_duration != NULL)
  {
    assert (wr->lease_duration->ldur == DDS_DURATION_INVALID);
    ddsrt_free (wr->lease_duration);
    if (wr->xqos->liveliness.kind != DDS_LIVELINESS_AUTOMATIC)
      ddsi_lease_free (wr->lease);
  }

  /* Do last gasp on SEDP and free writer. */
  if (!ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE))
    ddsi_sedp_dispose_unregister_writer (wr);
  ddsi_whc_free (wr->whc);
  if (wr->status_cb)
    (wr->status_cb) (wr->status_cb_entity, NULL);

#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_deregister_writer (wr);
#endif
#ifdef DDS_HAS_SSM
  if (wr->ssm_as)
    ddsi_unref_addrset (wr->ssm_as);
#endif
  ddsi_unref_addrset (wr->as); /* must remain until readers gone (rebuilding of addrset) */
  ddsi_xqos_fini (wr->xqos);
  ddsrt_free (wr->xqos);
  ddsi_local_reader_ary_fini (&wr->rdary);
  ddsrt_cond_destroy (&wr->throttle_cond);

  ddsi_sertype_unref ((struct ddsi_sertype *) wr->type);
  endpoint_common_fini (&wr->e, &wr->c);
  ddsrt_free (wr);
}

static void gc_delete_writer_throttlewait (struct ddsi_gcreq *gcreq)
{
  struct ddsi_writer *wr = gcreq->arg;
  ELOGDISC (wr, "gc_delete_writer_throttlewait(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (wr->e.guid));
  /* We now allow GC while blocked on a full WHC, but we still don't allow deleting a writer while blocked on it. The writer's state must be DELETING by the time we get here, and that means the transmit path is no longer blocked. It doesn't imply that the write thread is no longer in throttle_writer(), just that if it is, it will soon return from there. Therefore, block until it isn't throttling anymore. We can safely lock the writer, as we're on the separate GC thread. */
  assert (wr->state == WRST_DELETING);
  ddsrt_mutex_lock (&wr->e.lock);
  while (wr->throttling)
    ddsrt_cond_wait (&wr->throttle_cond, &wr->e.lock);
  ddsrt_mutex_unlock (&wr->e.lock);
  ddsi_gcreq_requeue (gcreq, gc_delete_writer);
}

static int gcreq_writer (struct ddsi_writer *wr)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (wr->e.gv->gcreq_queue, wr->throttling ? gc_delete_writer_throttlewait : gc_delete_writer);
  gcreq->arg = wr;
  ddsi_gcreq_enqueue (gcreq);
  return 0;
}

static void writer_set_state (struct ddsi_writer *wr, enum ddsi_writer_state newstate)
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

dds_return_t ddsi_unblock_throttled_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct ddsi_writer *wr;
  assert (ddsi_is_writer_entityid (guid->entityid));
  if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, guid)) == NULL)
  {
    GVLOGDISC ("ddsi_unblock_throttled_writer(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("ddsi_unblock_throttled_writer(guid "PGUIDFMT") ...\n", PGUID (*guid));
  ddsrt_mutex_lock (&wr->e.lock);
  writer_set_state (wr, WRST_INTERRUPT);
  ddsrt_mutex_unlock (&wr->e.lock);
  return 0;
}

dds_return_t ddsi_writer_wait_for_acks (struct ddsi_writer *wr, const ddsi_guid_t *rdguid, dds_time_t abstimeout)
{
  dds_return_t rc;
  ddsi_seqno_t ref_seq;
  ddsrt_mutex_lock (&wr->e.lock);
  ref_seq = wr->seq;
  if (rdguid == NULL)
  {
    while (wr->state == WRST_OPERATIONAL && ref_seq > ddsi_writer_max_drop_seq (wr))
      if (!ddsrt_cond_waituntil (&wr->throttle_cond, &wr->e.lock, abstimeout))
        break;
    rc = (ref_seq <= ddsi_writer_max_drop_seq (wr)) ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
  }
  else
  {
    struct ddsi_wr_prd_match *m = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, rdguid);
    while (wr->state == WRST_OPERATIONAL && m && ref_seq > m->seq)
    {
      if (!ddsrt_cond_waituntil (&wr->throttle_cond, &wr->e.lock, abstimeout))
        break;
      m = ddsrt_avl_lookup (&ddsi_wr_readers_treedef, &wr->readers, rdguid);
    }
    rc = (m == NULL || ref_seq <= m->seq) ? DDS_RETCODE_OK : DDS_RETCODE_TIMEOUT;
  }
  ddsrt_mutex_unlock (&wr->e.lock);
  return rc;
}

static dds_return_t delete_writer_nolinger_locked (struct ddsi_writer *wr)
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
    ELOGDISC (wr, "ddsi_delete_writer_nolinger(guid "PGUIDFMT") already done\n", PGUID (wr->e.guid));
    return 0;
  }

  ELOGDISC (wr, "ddsi_delete_writer_nolinger(guid "PGUIDFMT") ...\n", PGUID (wr->e.guid));
  ddsi_builtintopic_write_endpoint (wr->e.gv->builtin_topic_interface, &wr->e, ddsrt_time_wallclock(), false);
  ddsi_local_reader_ary_setinvalid (&wr->rdary);
  ddsi_entidx_remove_writer_guid (wr->e.gv->entity_index, wr);
  writer_set_state (wr, WRST_DELETING);
  if (wr->lease_duration != NULL) {
    wr->lease_duration->ldur = DDS_DURATION_INVALID;
    if (wr->xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC)
    {
      ddsrt_mutex_lock (&wr->c.pp->e.lock);
      ddsrt_fibheap_delete (&ddsi_ldur_fhdef, &wr->c.pp->ldur_auto_wr, wr->lease_duration);
      ddsrt_mutex_unlock (&wr->c.pp->e.lock);
      ddsi_resched_xevent_if_earlier (wr->c.pp->pmd_update_xevent, ddsrt_time_monotonic ());
    }
    else
    {
      if (wr->xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_TOPIC)
        ddsi_lease_unregister (wr->lease);
      if (writer_set_notalive_locked (wr, false) != DDS_RETCODE_OK)
        ELOGDISC (wr, "writer_set_notalive failed for "PGUIDFMT"\n", PGUID (wr->e.guid));
    }
  }
  gcreq_writer (wr);
  return 0;
}

dds_return_t ddsi_delete_writer_nolinger (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct ddsi_writer *wr;
  /* We take no care to ensure application writers are not deleted
     while they still have unacknowledged data (unless it takes too
     long), but we don't care about the DDSI built-in writers: we deal
     with that anyway because of the potential for crashes of remote
     DDSI participants. But it would be somewhat more elegant to do it
     differently. */
  assert (ddsi_is_writer_entityid (guid->entityid));
  if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, guid)) == NULL)
  {
    GVLOGDISC ("ddsi_delete_writer_nolinger(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("ddsi_delete_writer_nolinger(guid "PGUIDFMT") ...\n", PGUID (*guid));

  ddsrt_mutex_lock (&wr->e.lock);
  delete_writer_nolinger_locked (wr);
  ddsrt_mutex_unlock (&wr->e.lock);
  return 0;
}

void ddsi_delete_local_orphan_writer (struct ddsi_local_orphan_writer *lowr)
{
  assert (ddsi_thread_is_awake ());
  ddsrt_mutex_lock (&lowr->wr.e.lock);
  delete_writer_nolinger_locked (&lowr->wr);
  ddsrt_mutex_unlock (&lowr->wr.e.lock);
}

struct ddsi_delete_writer_xevent_cb_arg {
  ddsi_guid_t wr_guid;
};

static void ddsi_delete_writer_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, UNUSED_ARG (struct ddsi_xpack *xp), void *varg, UNUSED_ARG (ddsrt_mtime_t tnow))
{
  struct ddsi_delete_writer_xevent_cb_arg const * const arg = varg;
  /* don't worry if the writer is already gone by the time we get here, delete_writer_nolinger checks for that. */
  GVTRACE ("handle_xevk_delete_writer: "PGUIDFMT"\n", PGUID (arg->wr_guid));
  ddsi_delete_writer_nolinger (gv, &arg->wr_guid);
  ddsi_delete_xevent (ev);
}

dds_return_t ddsi_delete_writer (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct ddsi_writer *wr;
  struct ddsi_whc_state whcst;
  if ((wr = ddsi_entidx_lookup_writer_guid (gv->entity_index, guid)) == NULL)
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
  ddsi_whc_get_state(wr->whc, &whcst);
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
    
    struct ddsi_delete_writer_xevent_cb_arg arg = { .wr_guid = wr->e.guid };
    ddsi_qxev_callback (gv->xevents, tsched, ddsi_delete_writer_xevent_cb, &arg, sizeof (arg), false);
  }
  return 0;
}


/* READER ----------------------------------------------------------- */

#ifdef DDS_HAS_NETWORK_PARTITIONS
static void joinleave_mcast_helper (struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_locator_t *n, const char *joinleavestr, int (*joinleave) (const struct ddsi_domaingv *gv, struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc))
{
  char buf[DDSI_LOCSTRLEN];
  assert (ddsi_is_mcaddr (gv, n));
  if (n->kind != DDSI_LOCATOR_KIND_UDPv4MCGEN)
  {
    if (joinleave (gv, gv->mship, conn, NULL, n) < 0)
      GVWARNING ("failed to %s network partition multicast group %s\n", joinleavestr, ddsi_locator_to_string (buf, sizeof (buf), n));
  }
  else /* join all addresses that include this node */
  {
    ddsi_locator_t l = *n;
    ddsi_udpv4mcgen_address_t l1;
    uint32_t iph;
    memcpy (&l1, l.address, sizeof (l1));
    l.kind = DDSI_LOCATOR_KIND_UDPv4;
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

static void join_mcast_helper (struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_locator_t *n)
{
  joinleave_mcast_helper (gv, conn, n, "join", ddsi_join_mc);
}

static void leave_mcast_helper (struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, const ddsi_locator_t *n)
{
  joinleave_mcast_helper (gv, conn, n, "leave", ddsi_leave_mc);
}

static void reader_init_network_partition (struct ddsi_reader *rd)
{
  struct ddsi_domaingv * const gv = rd->e.gv;
  rd->uc_as = rd->mc_as = NULL;

  {
    /* compile address set from the mapped network partitions */
    const struct ddsi_config_networkpartition_listelem *np;
    np = ddsi_get_nwpart_from_mapping (&gv->logconfig, &gv->config, rd->xqos, rd->xqos->topic_name);
    if (np)
    {
      rd->uc_as = np->uc_addresses;
      rd->mc_as = np->asm_addresses;
#ifdef DDS_HAS_SSM
      if (np->ssm_addresses != NULL && (gv->config.allowMulticast & DDSI_AMC_SSM))
        rd->favours_ssm = 1;
#endif
    }
    if (rd->mc_as)
    {
      /* Iterate over all udp addresses:
       *   - Set the correct portnumbers
       *   - Join the socket if a multicast address
       */
      for (const struct ddsi_networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
        join_mcast_helper (gv, gv->data_conn_mc, &a->loc);
    }
#ifdef DDS_HAS_SSM
    else
    {
      /* Note: SSM requires NETWORK_PARTITIONS; if network partitions
         do not override the default, we should check whether the
         default is an SSM address. */
      if (ddsi_is_ssm_mcaddr (gv, &gv->loc_default_mc) && (gv->config.allowMulticast & DDSI_AMC_SSM))
        rd->favours_ssm = 1;
    }
#endif
  }
#ifdef DDS_HAS_SSM
  if (rd->favours_ssm)
    ELOGDISC (rd, "READER "PGUIDFMT" ssm=%d\n", PGUID (rd->e.guid), rd->favours_ssm);
#endif
  if ((rd->uc_as || rd->mc_as) && (gv->logconfig.c.mask & DDS_LC_DISCOVERY))
  {
    char buf[DDSI_LOCSTRLEN];
    ELOGDISC (rd, "READER "PGUIDFMT" locators={", PGUID (rd->e.guid));
    for (const struct ddsi_networkpartition_address *a = rd->uc_as; a != NULL; a = a->next)
      ELOGDISC (rd, " %s", ddsi_locator_to_string (buf, sizeof (buf), &a->loc));
    for (const struct ddsi_networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
      ELOGDISC (rd, " %s", ddsi_locator_to_string (buf, sizeof (buf), &a->loc));
    ELOGDISC (rd, " }\n");
  }
}
#endif /* DDS_HAS_NETWORK_PARTITIONS */

dds_return_t ddsi_new_reader_guid (struct ddsi_reader **rd_out, const struct ddsi_guid *guid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_rhc *rhc, ddsi_status_cb_t status_cb, void * status_entity)
{
  /* see ddsi_new_writer_guid for commenets */

  struct ddsi_reader *rd;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();

  assert (!ddsi_is_writer_entityid (guid->entityid));
  assert (ddsi_entidx_lookup_reader_guid (pp->e.gv->entity_index, guid) == NULL);
  assert (memcmp (&guid->prefix, &pp->e.guid.prefix, sizeof (guid->prefix)) == 0);

  new_reader_writer_common (&pp->e.gv->logconfig, guid, topic_name, type->type_name, xqos);
  rd = ddsrt_malloc (sizeof (*rd));
  if (rd_out)
    *rd_out = rd;

  const bool onlylocal = is_onlylocal_endpoint (pp, topic_name, type, xqos);
  endpoint_common_init (&rd->e, &rd->c, pp->e.gv, DDSI_EK_READER, guid, group_guid, pp, onlylocal, type);

  /* Copy QoS, merging in defaults */
  rd->xqos = ddsrt_malloc (sizeof (*rd->xqos));
  ddsi_xqos_copy (rd->xqos, xqos);
  ddsi_xqos_mergein_missing (rd->xqos, &ddsi_default_qos_reader, ~(uint64_t)0);
  assert (rd->xqos->aliased == 0);
  ddsi_set_topic_type_name (rd->xqos, topic_name, type->type_name);

  if (rd->e.gv->logconfig.c.mask & DDS_LC_DISCOVERY)
  {
    ELOGDISC (rd, "READER "PGUIDFMT" QOS={", PGUID (rd->e.guid));
    ddsi_xqos_log (DDS_LC_DISCOVERY, &rd->e.gv->logconfig, rd->xqos);
    ELOGDISC (rd, "}\n");
  }
  assert (rd->xqos->present & DDSI_QP_RELIABILITY);
  rd->reliable = (rd->xqos->reliability.kind != DDS_RELIABILITY_BEST_EFFORT);
  assert (rd->xqos->present & DDSI_QP_DURABILITY);
  /* The builtin volatile secure writer applies a filter which is used to send the secure
   * crypto token only to the destination reader for which the crypto tokens are applicable.
   * Thus the builtin volatile secure reader will receive gaps in the sequence numbers of
   * the messages received. Therefore the out-of-order list of the proxy writer cannot be
   * used for this reader and reader specific out-of-order list must be used which is
   * used for handling transient local data.
   */
  rd->handle_as_transient_local = (rd->xqos->durability.kind == DDS_DURABILITY_TRANSIENT_LOCAL) ||
                                  (rd->e.guid.entityid.u == DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER);
  rd->type = ddsi_sertype_ref (type);
  rd->request_keyhash = rd->type->request_keyhash;
#ifdef DDS_HAS_SHM
  rd->has_iceoryx = (0x0 == (xqos->ignore_locator_type & DDSI_LOCATOR_KIND_SHEM));
#endif
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
  assert (rd->xqos->present & DDSI_QP_LIVELINESS);

#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_register_reader (rd);
#endif

#ifdef DDS_HAS_NETWORK_PARTITIONS
  reader_init_network_partition (rd);
#endif

  ddsrt_avl_init (&ddsi_rd_writers_treedef, &rd->writers);
  ddsrt_avl_init (&ddsi_rd_local_writers_treedef, &rd->local_writers);

  ddsrt_mutex_lock (&rd->e.lock);
  ddsi_entidx_insert_reader_guid (pp->e.gv->entity_index, rd);
  ddsi_builtintopic_write_endpoint (pp->e.gv->builtin_topic_interface, &rd->e, ddsrt_time_wallclock(), true);
  ddsrt_mutex_unlock (&rd->e.lock);

  ddsi_match_reader_with_proxy_writers (rd, tnow);
  ddsi_match_reader_with_local_writers (rd, tnow);
  ddsi_sedp_write_reader (rd);
  return 0;
}

dds_return_t ddsi_new_reader (struct ddsi_reader **rd_out, struct ddsi_guid *rdguid, const struct ddsi_guid *group_guid, struct ddsi_participant *pp, const char *topic_name, const struct ddsi_sertype *type, const struct dds_qos *xqos, struct ddsi_rhc * rhc, ddsi_status_cb_t status_cb, void * status_cbarg)
{
  dds_return_t rc;
  uint32_t kind;

  rdguid->prefix = pp->e.guid.prefix;
  kind = type->typekind_no_key ? DDSI_ENTITYID_KIND_READER_NO_KEY : DDSI_ENTITYID_KIND_READER_WITH_KEY;
  if ((rc = ddsi_participant_allocate_entityid (&rdguid->entityid, kind, pp)) < 0)
    return rc;
  return ddsi_new_reader_guid (rd_out, rdguid, group_guid, pp, topic_name, type, xqos, rhc, status_cb, status_cbarg);
}

static void gc_delete_reader (struct ddsi_gcreq *gcreq)
{
  /* see gc_delete_writer for comments */
  struct ddsi_reader *rd = gcreq->arg;
  ELOGDISC (rd, "gc_delete_reader(%p, "PGUIDFMT")\n", (void *) gcreq, PGUID (rd->e.guid));
  ddsi_gcreq_free (gcreq);

  while (!ddsrt_avl_is_empty (&rd->writers))
  {
    struct ddsi_rd_pwr_match *m = ddsrt_avl_root_non_empty (&ddsi_rd_writers_treedef, &rd->writers);
    ddsrt_avl_delete (&ddsi_rd_writers_treedef, &rd->writers, m);
    ddsi_proxy_writer_drop_connection (&m->pwr_guid, rd);
    ddsi_free_rd_pwr_match (rd->e.gv, &rd->e.guid, m);
  }
  while (!ddsrt_avl_is_empty (&rd->local_writers))
  {
    struct ddsi_rd_wr_match *m = ddsrt_avl_root_non_empty (&ddsi_rd_local_writers_treedef, &rd->local_writers);
    ddsrt_avl_delete (&ddsi_rd_local_writers_treedef, &rd->local_writers, m);
    ddsi_writer_drop_local_connection (&m->wr_guid, rd);
    ddsi_free_rd_wr_match (m);
  }

#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_deregister_reader (rd);
#endif

  if (!ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE))
    ddsi_sedp_dispose_unregister_reader (rd);
#ifdef DDS_HAS_NETWORK_PARTITIONS
  if (rd->mc_as)
  {
    for (const struct ddsi_networkpartition_address *a = rd->mc_as; a != NULL; a = a->next)
      leave_mcast_helper (rd->e.gv, rd->e.gv->data_conn_mc, &a->loc);
  }
#endif
  if (rd->rhc && ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE))
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

static int gcreq_reader (struct ddsi_reader *rd)
{
  struct ddsi_gcreq *gcreq = ddsi_gcreq_new (rd->e.gv->gcreq_queue, gc_delete_reader);
  gcreq->arg = rd;
  ddsi_gcreq_enqueue (gcreq);
  return 0;
}

dds_return_t ddsi_delete_reader (struct ddsi_domaingv *gv, const struct ddsi_guid *guid)
{
  struct ddsi_reader *rd;
  assert (!ddsi_is_writer_entityid (guid->entityid));
  if ((rd = ddsi_entidx_lookup_reader_guid (gv->entity_index, guid)) == NULL)
  {
    GVLOGDISC ("delete_reader_guid(guid "PGUIDFMT") - unknown guid\n", PGUID (*guid));
    return DDS_RETCODE_BAD_PARAMETER;
  }
  GVLOGDISC ("delete_reader_guid(guid "PGUIDFMT") ...\n", PGUID (*guid));
  ddsi_builtintopic_write_endpoint (rd->e.gv->builtin_topic_interface, &rd->e, ddsrt_time_wallclock(), false);
  ddsi_entidx_remove_reader_guid (gv->entity_index, rd);
  gcreq_reader (rd);
  return 0;
}

void ddsi_update_reader_qos (struct ddsi_reader *rd, const dds_qos_t *xqos)
{
  ddsrt_mutex_lock (&rd->e.lock);
  if (ddsi_update_qos_locked (&rd->e, rd->xqos, xqos, ddsrt_time_wallclock ()))
    ddsi_sedp_write_reader (rd);
  ddsrt_mutex_unlock (&rd->e.lock);
}

struct ddsi_reader *ddsi_writer_first_in_sync_reader (struct ddsi_entity_index *entity_index, struct ddsi_entity_common *wrcmn, ddsrt_avl_iter_t *it)
{
  assert (wrcmn->kind == DDSI_EK_WRITER);
  struct ddsi_writer *wr = (struct ddsi_writer *) wrcmn;
  struct ddsi_wr_rd_match *m = ddsrt_avl_iter_first (&ddsi_wr_local_readers_treedef, &wr->local_readers, it);
  return m ? ddsi_entidx_lookup_reader_guid (entity_index, &m->rd_guid) : NULL;
}

struct ddsi_reader *ddsi_writer_next_in_sync_reader (struct ddsi_entity_index *entity_index, ddsrt_avl_iter_t *it)
{
  struct ddsi_wr_rd_match *m = ddsrt_avl_iter_next (it);
  return m ? ddsi_entidx_lookup_reader_guid (entity_index, &m->rd_guid) : NULL;
}
