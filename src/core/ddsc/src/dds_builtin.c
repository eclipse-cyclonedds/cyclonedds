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
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds__init.h"
#include "dds__domain.h"
#include "dds__participant.h"
#include "dds__types.h"
#include "dds__builtin.h"
#include "dds__entity.h"
#include "dds__subscriber.h"
#include "dds__topic.h"
#include "dds__write.h"
#include "dds__writer.h"
#include "dds__whc_builtintopic.h"
#include "dds__serdata_builtintopic.h"
#include "dds/ddsi/q_qosmatch.h"
#include "dds/ddsi/ddsi_tkmap.h"

static dds_qos_t *dds__create_builtin_qos (void)
{
  const char *partition = "__BUILT-IN PARTITION__";
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_presentation (qos, DDS_PRESENTATION_TOPIC, false, false);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS(100));
  dds_qset_partition (qos, 1, &partition);
  return qos;
}

dds_entity_t dds__get_builtin_topic (dds_entity_t entity, dds_entity_t topic)
{
  dds_entity_t tp;
  dds_return_t rc;
  dds_entity *e;
  dds_participant *par;

  if ((rc = dds_entity_pin (entity, &e)) < 0)
    return rc;
  if ((par = dds_entity_participant (e)) == NULL)
  {
    dds_entity_unpin (e);
    return DDS_RETCODE_ILLEGAL_OPERATION;
  }

  char *topic_name;
  struct ddsi_sertype *sertype;
  switch (topic)
  {
    case DDS_BUILTIN_TOPIC_DCPSPARTICIPANT:
      topic_name = DDS_BUILTIN_TOPIC_PARTICIPANT_NAME;
      sertype = e->m_domain->builtin_participant_type;
      break;
    case DDS_BUILTIN_TOPIC_DCPSPUBLICATION:
      topic_name = DDS_BUILTIN_TOPIC_PUBLICATION_NAME;
      sertype = e->m_domain->builtin_writer_type;
      break;
    case DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION:
      topic_name = DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME;
      sertype = e->m_domain->builtin_reader_type;
      break;
    default:
      assert (0);
      dds_entity_unpin (e);
      return DDS_RETCODE_BAD_PARAMETER;
  }

  dds_qos_t *qos = dds__create_builtin_qos ();
  if ((tp = dds_create_topic_impl (par->m_entity.m_hdllink.hdl, topic_name, true, &sertype, qos, NULL, NULL)) > 0)
  {
    /* keep ownership for built-in sertypes because there are re-used, lifetime for these
       sertypes is bound to domain */
    ddsi_sertype_ref (sertype);
  }
  dds_delete_qos (qos);
  dds_entity_unpin (e);
  return tp;
}

static bool qos_has_resource_limits (const dds_qos_t *qos)
{
  assert (qos->present & QP_RESOURCE_LIMITS);
  return (qos->resource_limits.max_samples != DDS_LENGTH_UNLIMITED ||
          qos->resource_limits.max_instances != DDS_LENGTH_UNLIMITED ||
          qos->resource_limits.max_samples_per_instance != DDS_LENGTH_UNLIMITED);
}

bool dds__validate_builtin_reader_qos (const dds_domain *dom, dds_entity_t topic, const dds_qos_t *qos)
{
  if (qos == NULL)
    /* default QoS inherited from topic is ok by definition */
    return true;
  else
  {
    /* failing writes on built-in topics are unwelcome complications, so we simply
       forbid the creation of a reader matching a built-in topics writer that has
       resource limits */
    struct local_orphan_writer *bwr;
    switch (topic)
    {
      case DDS_BUILTIN_TOPIC_DCPSPARTICIPANT:
        bwr = dom->builtintopic_writer_participant;
        break;
      case DDS_BUILTIN_TOPIC_DCPSPUBLICATION:
        bwr = dom->builtintopic_writer_publications;
        break;
      case DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION:
        bwr = dom->builtintopic_writer_subscriptions;
        break;
      default:
        assert (0);
        return false;
    }

    /* FIXME: DDSI-level readers, writers have topic, type name in their QoS, but
       DDSC-level ones don't and that gives an automatic mismatch when comparing
       the full QoS object ...  Here the two have the same topic by construction
       so ignoring them in the comparison makes things work.  The discrepancy
       should be addressed one day. */
    const uint64_t qmask = ~(QP_TOPIC_NAME | QP_TYPE_NAME | QP_CYCLONE_TYPE_INFORMATION);
    dds_qos_policy_id_t dummy;
#ifdef DDS_HAS_TYPE_DISCOVERY
    return qos_match_mask_p (bwr->wr.e.gv, qos, bwr->wr.xqos, qmask, &dummy, NULL, NULL, NULL, NULL) && !qos_has_resource_limits (qos);
#else
    return qos_match_mask_p (bwr->wr.e.gv, qos, bwr->wr.xqos, qmask, &dummy) && !qos_has_resource_limits (qos);
#endif
  }
}

static dds_entity_t dds__create_builtin_subscriber (dds_participant *participant)
{
  dds_qos_t *qos = dds__create_builtin_qos ();
  dds_entity_t sub = dds__create_subscriber_l (participant, false, qos, NULL);
  dds_delete_qos (qos);
  return sub;
}

dds_entity_t dds__get_builtin_subscriber (dds_entity_t e)
{
  dds_entity_t sub;
  dds_return_t ret;
  dds_entity_t pp;
  dds_participant *p;

  if ((pp = dds_get_participant (e)) <= 0)
    return pp;
  if ((ret = dds_participant_lock (pp, &p)) != DDS_RETCODE_OK)
    return ret;

  if (p->m_builtin_subscriber <= 0) {
    p->m_builtin_subscriber = dds__create_builtin_subscriber (p);
  }
  sub = p->m_builtin_subscriber;
  dds_participant_unlock(p);
  return sub;
}

static bool dds__builtin_is_builtintopic (const struct ddsi_sertype *tp, void *vdomain)
{
  (void) vdomain;
  return tp->ops == &ddsi_sertype_ops_builtintopic;
}

static bool dds__builtin_is_visible (const ddsi_guid_t *guid, nn_vendorid_t vendorid, void *vdomain)
{
  (void) vdomain;
  if (is_builtin_endpoint (guid->entityid, vendorid))
    return false;
  return true;
}

static struct ddsi_tkmap_instance *dds__builtin_get_tkmap_entry (const struct ddsi_guid *guid, void *vdomain)
{
  struct dds_domain *domain = vdomain;
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *sd;
  union { ddsi_guid_t guid; struct ddsi_keyhash keyhash; } x;
  x.guid = nn_hton_guid (*guid);
  /* any random builtin topic will do (provided it has a GUID for a key), because what matters is the "class"
     of the topic, not the actual topic; also, this is called early in the initialisation of the entity with
     this GUID, which simply causes serdata_from_keyhash to create a key-only serdata because the key lookup
     fails. */
  sd = ddsi_serdata_from_keyhash (domain->builtin_participant_type, &x.keyhash);
  tk = ddsi_tkmap_find (domain->gv.m_tkmap, sd, true);
  ddsi_serdata_unref (sd);
  return tk;
}

struct ddsi_serdata *dds__builtin_make_sample (const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive)
{
  /* initialize to avoid gcc warning ultimately caused by C's horrible type system */
  struct dds_domain *dom = e->gv->builtin_topic_interface->arg;
  struct ddsi_sertype *type = NULL;
  struct ddsi_serdata *serdata;
  union { ddsi_guid_t guid; struct ddsi_keyhash keyhash; } x;
  switch (e->kind)
  {
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      type = dom->builtin_participant_type;
      break;
    case EK_WRITER:
    case EK_PROXY_WRITER:
      type = dom->builtin_writer_type;
      break;
    case EK_READER:
    case EK_PROXY_READER:
      type = dom->builtin_reader_type;
      break;
  }
  assert (type != NULL);
  x.guid = nn_hton_guid (e->guid);
  serdata = ddsi_serdata_from_keyhash (type, &x.keyhash);
  serdata->timestamp = timestamp;
  serdata->statusinfo = alive ? 0 : NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER;
  return serdata;
}

static void dds__builtin_write (const struct entity_common *e, ddsrt_wctime_t timestamp, bool alive, void *vdomain)
{
  struct dds_domain *dom = vdomain;
  if (dds__builtin_is_visible (&e->guid, get_entity_vendorid (e), dom))
  {
    /* initialize to avoid gcc warning ultimately caused by C's horrible type system */
    struct local_orphan_writer *bwr = NULL;
    struct ddsi_serdata *serdata = dds__builtin_make_sample (e, timestamp, alive);
    assert (e->tk != NULL);
    switch (e->kind)
    {
      case EK_PARTICIPANT:
      case EK_PROXY_PARTICIPANT:
        bwr = dom->builtintopic_writer_participant;
        break;
      case EK_WRITER:
      case EK_PROXY_WRITER:
        bwr = dom->builtintopic_writer_publications;
        break;
      case EK_READER:
      case EK_PROXY_READER:
        bwr = dom->builtintopic_writer_subscriptions;
        break;
    }
    dds_writecdr_impl (&bwr->wr, NULL, serdata, true, NULL);
  }
}

static void unref_builtin_types (struct dds_domain *dom)
{
  ddsi_sertype_unref (dom->builtin_participant_type);
  ddsi_sertype_unref (dom->builtin_reader_type);
  ddsi_sertype_unref (dom->builtin_writer_type);
}

void dds__builtin_init (struct dds_domain *dom)
{
  dds_qos_t *qos = dds__create_builtin_qos ();

  dom->btif.arg = dom;
  dom->btif.builtintopic_get_tkmap_entry = dds__builtin_get_tkmap_entry;
  dom->btif.builtintopic_is_builtintopic = dds__builtin_is_builtintopic;
  dom->btif.builtintopic_is_visible = dds__builtin_is_visible;
  dom->btif.builtintopic_write = dds__builtin_write;
  dom->gv.builtin_topic_interface = &dom->btif;

  dom->builtin_participant_type = new_sertype_builtintopic (DSBT_PARTICIPANT, "org::eclipse::cyclonedds::builtin::DCPSParticipant");
  dom->builtin_reader_type = new_sertype_builtintopic (DSBT_READER, "org::eclipse::cyclonedds::builtin::DCPSSubscription");
  dom->builtin_writer_type = new_sertype_builtintopic (DSBT_WRITER, "org::eclipse::cyclonedds::builtin::DCPSPublication");

  ddsrt_mutex_lock (&dom->gv.sertypes_lock);
  ddsi_sertype_register_locked (&dom->gv, dom->builtin_participant_type);
  ddsi_sertype_register_locked (&dom->gv, dom->builtin_reader_type);
  ddsi_sertype_register_locked (&dom->gv, dom->builtin_writer_type);
  ddsrt_mutex_unlock (&dom->gv.sertypes_lock);

  thread_state_awake (lookup_thread_state (), &dom->gv);
  const struct entity_index *gh = dom->gv.entity_index;
  dom->builtintopic_writer_participant = new_local_orphan_writer (&dom->gv, to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER), DDS_BUILTIN_TOPIC_PARTICIPANT_NAME, dom->builtin_participant_type, qos, builtintopic_whc_new (DSBT_PARTICIPANT, gh));
  dom->builtintopic_writer_publications = new_local_orphan_writer (&dom->gv, to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER), DDS_BUILTIN_TOPIC_PUBLICATION_NAME, dom->builtin_writer_type, qos, builtintopic_whc_new (DSBT_WRITER, gh));
  dom->builtintopic_writer_subscriptions = new_local_orphan_writer (&dom->gv, to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER), DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME, dom->builtin_reader_type, qos, builtintopic_whc_new (DSBT_READER, gh));
  thread_state_asleep (lookup_thread_state ());

  dds_delete_qos (qos);

  /* ddsi_sertype_init initializes the refcount to 1 and dds_sertopic_register_locked increments
     it.  All "real" references (such as readers and writers) are also accounted for in the
     reference count, so we have an excess reference here. */
  unref_builtin_types (dom);
}

void dds__builtin_fini (struct dds_domain *dom)
{
  /* No more sources for builtin topic samples */
  thread_state_awake (lookup_thread_state (), &dom->gv);
  delete_local_orphan_writer (dom->builtintopic_writer_participant);
  delete_local_orphan_writer (dom->builtintopic_writer_publications);
  delete_local_orphan_writer (dom->builtintopic_writer_subscriptions);
  thread_state_asleep (lookup_thread_state ());
  unref_builtin_types (dom);
}
