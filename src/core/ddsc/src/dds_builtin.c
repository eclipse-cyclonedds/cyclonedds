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
#include "ddsi/q_entity.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_config.h"
#include "dds__init.h"
#include "dds__qos.h"
#include "dds__domain.h"
#include "dds__participant.h"
#include "dds__err.h"
#include "dds__types.h"
#include "dds__builtin.h"
#include "dds__subscriber.h"
#include "dds__write.h"
#include "dds__writer.h"
#include "dds__whc_builtintopic.h"
#include "dds__serdata_builtintopic.h"
#include "ddsi/q_qosmatch.h"
#include "ddsi/ddsi_tkmap.h"

static struct ddsi_sertopic *builtin_participant_topic;
static struct ddsi_sertopic *builtin_reader_topic;
static struct ddsi_sertopic *builtin_writer_topic;
static struct local_orphan_writer *builtintopic_writer_participant;
static struct local_orphan_writer *builtintopic_writer_publications;
static struct local_orphan_writer *builtintopic_writer_subscriptions;

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

void dds__builtin_init (void)
{
  dds_qos_t *qos = dds__create_builtin_qos ();

  builtin_participant_topic = new_sertopic_builtintopic (DSBT_PARTICIPANT, "DCPSParticipant", "org::eclipse::cyclonedds::builtin::DCPSParticipant");
  builtin_reader_topic = new_sertopic_builtintopic (DSBT_READER, "DCPSSubscription", "org::eclipse::cyclonedds::builtin::DCPSSubscription");
  builtin_writer_topic = new_sertopic_builtintopic (DSBT_WRITER, "DCPSPublication", "org::eclipse::cyclonedds::builtin::DCPSPublication");

  builtintopic_writer_participant = new_local_orphan_writer (to_entityid (NN_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER), builtin_participant_topic, qos, builtintopic_whc_new (DSBT_PARTICIPANT));
  builtintopic_writer_publications = new_local_orphan_writer (to_entityid (NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER), builtin_writer_topic, qos, builtintopic_whc_new (DSBT_WRITER));
  builtintopic_writer_subscriptions = new_local_orphan_writer (to_entityid (NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER), builtin_reader_topic, qos, builtintopic_whc_new (DSBT_READER));

  dds_delete_qos (qos);
}

void dds__builtin_fini (void)
{
  /* No more sources for builtin topic samples */
  struct thread_state1 * const self = lookup_thread_state ();
  thread_state_awake (self);
  delete_local_orphan_writer (builtintopic_writer_participant);
  delete_local_orphan_writer (builtintopic_writer_publications);
  delete_local_orphan_writer (builtintopic_writer_subscriptions);
  thread_state_asleep (self);

  ddsi_sertopic_unref (builtin_participant_topic);
  ddsi_sertopic_unref (builtin_reader_topic);
  ddsi_sertopic_unref (builtin_writer_topic);
}

dds_entity_t dds__get_builtin_topic (dds_entity_t e, dds_entity_t topic)
{
  dds_entity_t pp;
  dds_entity_t tp;

  if ((pp = dds_get_participant (e)) <= 0)
    return pp;

  struct ddsi_sertopic *sertopic;
  if (topic == DDS_BUILTIN_TOPIC_DCPSPARTICIPANT) {
    sertopic = builtin_participant_topic;
  } else if (topic == DDS_BUILTIN_TOPIC_DCPSPUBLICATION) {
    sertopic = builtin_writer_topic;
  } else if (topic == DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION) {
    sertopic = builtin_reader_topic;
  } else {
    assert (0);
    return DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
  }

  dds_qos_t *qos = dds__create_builtin_qos ();
  tp = dds_create_topic_arbitrary (pp, sertopic, sertopic->name, qos, NULL, NULL);
  dds_delete_qos (qos);
  return tp;
}

static bool qos_has_resource_limits (const dds_qos_t *qos)
{
  return (qos->resource_limits.max_samples != DDS_LENGTH_UNLIMITED ||
          qos->resource_limits.max_instances != DDS_LENGTH_UNLIMITED ||
          qos->resource_limits.max_samples_per_instance != DDS_LENGTH_UNLIMITED);
}

bool dds__validate_builtin_reader_qos (dds_entity_t topic, const dds_qos_t *qos)
{
  if (qos == NULL)
    /* default QoS inherited from topic is ok by definition */
    return true;
  else
  {
    /* failing writes on built-in topics are unwelcome complications, so we simply forbid the creation of
       a reader matching a built-in topics writer that has resource limits */
    struct local_orphan_writer *bwr;
    if (topic == DDS_BUILTIN_TOPIC_DCPSPARTICIPANT) {
      bwr = builtintopic_writer_participant;
    } else if (topic == DDS_BUILTIN_TOPIC_DCPSPUBLICATION) {
      bwr = builtintopic_writer_publications;
    } else if (topic == DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION) {
      bwr = builtintopic_writer_subscriptions;
    } else {
      assert (0);
      return false;
    }
    return qos_match_p (qos, bwr->wr.xqos) && !qos_has_resource_limits (qos);
  }
}

static dds_entity_t dds__create_builtin_subscriber (dds_entity *participant)
{
  dds_qos_t *qos = dds__create_builtin_qos ();
  dds_entity_t sub = dds__create_subscriber_l (participant, qos, NULL);
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
    p->m_builtin_subscriber = dds__create_builtin_subscriber (&p->m_entity);
  }
  sub = p->m_builtin_subscriber;
  dds_participant_unlock(p);
  return sub;
}

bool dds__builtin_is_visible (nn_entityid_t entityid, bool onlylocal, nn_vendorid_t vendorid)
{
  return !(onlylocal || is_builtin_endpoint (entityid, vendorid));
}

struct ddsi_tkmap_instance *dds__builtin_get_tkmap_entry (const struct nn_guid *guid)
{
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *sd;
  struct nn_keyhash kh;
  memcpy (&kh, guid, sizeof (kh));
  /* any random builtin topic will do (provided it has a GUID for a key), because what matters is the "class" of the topic, not the actual topic; also, this is called early in the initialisation of the entity with this GUID, which simply causes serdata_from_keyhash to create a key-only serdata because the key lookup fails. */
  sd = ddsi_serdata_from_keyhash (builtin_participant_topic, &kh);
  tk = ddsi_tkmap_find (sd, false, true);
  ddsi_serdata_unref (sd);
  return tk;
}

struct ddsi_serdata *dds__builtin_make_sample (const struct entity_common *e, nn_wctime_t timestamp, bool alive)
{
  /* initialize to avoid gcc warning ultimately caused by C's horrible type system */
  struct ddsi_sertopic *topic = NULL;
  struct ddsi_serdata *serdata;
  struct nn_keyhash keyhash;
  switch (e->kind)
  {
    case EK_PARTICIPANT:
    case EK_PROXY_PARTICIPANT:
      topic = builtin_participant_topic;
      break;
    case EK_WRITER:
    case EK_PROXY_WRITER:
      topic = builtin_writer_topic;
      break;
    case EK_READER:
    case EK_PROXY_READER:
      topic = builtin_reader_topic;
      break;
  }
  assert (topic != NULL);
  memcpy (&keyhash, &e->guid, sizeof (keyhash));
  serdata = ddsi_serdata_from_keyhash (topic, &keyhash);
  serdata->timestamp = timestamp;
  serdata->statusinfo = alive ? 0 : NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER;
  return serdata;
}

void dds__builtin_write (const struct entity_common *e, nn_wctime_t timestamp, bool alive)
{
  if (ddsi_plugin.builtintopic_is_visible (e->guid.entityid, e->onlylocal, get_entity_vendorid (e)))
  {
    /* initialize to avoid gcc warning ultimately caused by C's horrible type system */
    struct local_orphan_writer *bwr = NULL;
    struct ddsi_serdata *serdata = dds__builtin_make_sample (e, timestamp, alive);
    assert (e->tk != NULL);
    switch (e->kind)
    {
      case EK_PARTICIPANT:
      case EK_PROXY_PARTICIPANT:
        bwr = builtintopic_writer_participant;
        break;
      case EK_WRITER:
      case EK_PROXY_WRITER:
        bwr = builtintopic_writer_publications;
        break;
      case EK_READER:
      case EK_PROXY_READER:
        bwr = builtintopic_writer_subscriptions;
        break;
    }
    dds_writecdr_impl_lowlevel (&bwr->wr, NULL, serdata);
  }
}
