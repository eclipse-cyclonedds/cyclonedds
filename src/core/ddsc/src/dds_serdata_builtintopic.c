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
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/q_plist.h"
#include "dds__stream.h"
#include "dds__serdata_builtintopic.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"

static const uint64_t unihashconsts[] = {
  UINT64_C (16292676669999574021),
  UINT64_C (10242350189706880077),
  UINT64_C (12844332200329132887),
  UINT64_C (16728792139623414127)
};

static uint32_t hash_guid (const ddsi_guid_t *g)
{
  return
  (uint32_t) (((((uint32_t) g->prefix.u[0] + unihashconsts[0]) *
                ((uint32_t) g->prefix.u[1] + unihashconsts[1])) +
               (((uint32_t) g->prefix.u[2] + unihashconsts[2]) *
                ((uint32_t) g->entityid.u  + unihashconsts[3])))
              >> 32);
}

static struct ddsi_serdata *fix_serdata_builtin(struct ddsi_serdata_builtintopic *d, uint32_t basehash)
{
  d->c.hash = hash_guid (&d->key) ^ basehash;
  return &d->c;
}

static bool serdata_builtin_eqkey(const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct ddsi_serdata_builtintopic *a = (const struct ddsi_serdata_builtintopic *)acmn;
  const struct ddsi_serdata_builtintopic *b = (const struct ddsi_serdata_builtintopic *)bcmn;
  return memcmp (&a->key, &b->key, sizeof (a->key)) == 0;
}

static void serdata_builtin_free(struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_builtintopic *d = (struct ddsi_serdata_builtintopic *)dcmn;
  if (d->c.kind == SDK_DATA)
    nn_xqos_fini (&d->xqos);
  ddsrt_free (d);
}

static struct ddsi_serdata_builtintopic *serdata_builtin_new(const struct ddsi_sertopic_builtintopic *tp, enum ddsi_serdata_kind kind)
{
  struct ddsi_serdata_builtintopic *d = ddsrt_malloc(sizeof (*d));
  ddsi_serdata_init (&d->c, &tp->c, kind);
  return d;
}

static void from_entity_pp (struct ddsi_serdata_builtintopic *d, const struct participant *pp)
{
  nn_xqos_copy(&d->xqos, &pp->plist->qos);
  d->pphandle = pp->e.iid;
}

static void from_entity_proxypp (struct ddsi_serdata_builtintopic *d, const struct proxy_participant *proxypp)
{
  nn_xqos_copy(&d->xqos, &proxypp->plist->qos);
  d->pphandle = proxypp->e.iid;
}

static void set_topic_type_from_sertopic (struct ddsi_serdata_builtintopic *d, const struct ddsi_sertopic *tp)
{
  if (!(d->xqos.present & QP_TOPIC_NAME))
  {
    d->xqos.topic_name = dds_string_dup (tp->name);
    d->xqos.present |= QP_TOPIC_NAME;
  }
  if (!(d->xqos.present & QP_TYPE_NAME))
  {
    d->xqos.type_name = dds_string_dup (tp->type_name);
    d->xqos.present |= QP_TYPE_NAME;
  }
}

static void from_entity_rd (struct ddsi_serdata_builtintopic *d, const struct reader *rd)
{
  d->pphandle = rd->c.pp->e.iid;
  nn_xqos_copy(&d->xqos, rd->xqos);
  set_topic_type_from_sertopic(d, rd->topic);
}

static void from_entity_prd (struct ddsi_serdata_builtintopic *d, const struct proxy_reader *prd)
{
  d->pphandle = prd->c.proxypp->e.iid;
  nn_xqos_copy(&d->xqos, prd->c.xqos);
  assert (d->xqos.present & QP_TOPIC_NAME);
  assert (d->xqos.present & QP_TYPE_NAME);
}

static void from_entity_wr (struct ddsi_serdata_builtintopic *d, const struct writer *wr)
{
  d->pphandle = wr->c.pp->e.iid;
  nn_xqos_copy(&d->xqos, wr->xqos);
  set_topic_type_from_sertopic(d, wr->topic);
}

static void from_entity_pwr (struct ddsi_serdata_builtintopic *d, const struct proxy_writer *pwr)
{
  d->pphandle = pwr->c.proxypp->e.iid;
  nn_xqos_copy(&d->xqos, pwr->c.xqos);
  assert (d->xqos.present & QP_TOPIC_NAME);
  assert (d->xqos.present & QP_TYPE_NAME);
}

static struct ddsi_serdata *ddsi_serdata_builtin_from_keyhash (const struct ddsi_sertopic *tpcmn, const nn_keyhash_t *keyhash)
{
  /* FIXME: not quite elegant to manage the creation of a serdata for a built-in topic via this function, but I also find it quite unelegant to let from_sample read straight from the underlying internal entity, and to_sample convert to the external format ... I could claim the internal entity is the "serialised form", but that forces wrapping it in a fragchain in one way or another, which, though possible, is also a bit lacking in elegance. */
  const struct ddsi_sertopic_builtintopic *tp = (const struct ddsi_sertopic_builtintopic *)tpcmn;
  /* keyhash must in host format (which the GUIDs always are internally) */
  struct entity_common *entity = entidx_lookup_guid_untyped (tp->gv->entity_index, (const ddsi_guid_t *) keyhash->value);
  struct ddsi_serdata_builtintopic *d = serdata_builtin_new(tp, entity ? SDK_DATA : SDK_KEY);
  memcpy (&d->key, keyhash->value, sizeof (d->key));
  if (entity)
  {
    ddsrt_mutex_lock (&entity->qos_lock);
    switch (entity->kind)
    {
      case EK_PARTICIPANT:
        assert (tp->type == DSBT_PARTICIPANT);
        from_entity_pp (d, (const struct participant *) entity);
        break;
      case EK_READER:
        assert (tp->type == DSBT_READER);
        from_entity_rd (d, (const struct reader *) entity);
        break;
      case EK_WRITER:
        assert (tp->type == DSBT_WRITER);
        from_entity_wr (d, (const struct writer *) entity);
        break;
      case EK_PROXY_PARTICIPANT:
        assert (tp->type == DSBT_PARTICIPANT);
        from_entity_proxypp (d, (const struct proxy_participant *) entity);
        break;
      case EK_PROXY_READER:
        assert (tp->type == DSBT_READER);
        from_entity_prd (d, (const struct proxy_reader *) entity);
        break;
      case EK_PROXY_WRITER:
        assert (tp->type == DSBT_WRITER);
        from_entity_pwr (d, (const struct proxy_writer *) entity);
        break;
    }
    ddsrt_mutex_unlock (&entity->qos_lock);
  }
  return fix_serdata_builtin(d, tp->c.serdata_basehash);
}

static struct ddsi_serdata *serdata_builtin_to_topicless (const struct ddsi_serdata *serdata_common)
{
  /* All built-in ones are currently topicless */
  return ddsi_serdata_ref (serdata_common);
}

static void convkey (dds_builtintopic_guid_t *key, const ddsi_guid_t *guid)
{
  ddsi_guid_t tmp;
  tmp = nn_hton_guid (*guid);
  memcpy (key, &tmp, sizeof (*key));
}

static char *dds_string_dup_reuse (char *old, const char *src)
{
  size_t size = strlen (src) + 1;
  char *new = dds_realloc(old, size);
  return memcpy (new, src, size);
}

static dds_qos_t *dds_qos_from_xqos_reuse (dds_qos_t *old, const dds_qos_t *src)
{
  if (old == NULL)
    old = ddsrt_malloc (sizeof (*old));
  else
  {
    nn_xqos_fini (old);
  }
  nn_xqos_init_empty (old);
  nn_xqos_mergein_missing (old, src, ~(QP_TOPIC_NAME | QP_TYPE_NAME));
  return old;
}

static bool to_sample_pp (const struct ddsi_serdata_builtintopic *d, struct dds_builtintopic_participant *sample)
{
  convkey (&sample->key, &d->key);
  if (d->c.kind == SDK_DATA)
  {
    sample->qos = dds_qos_from_xqos_reuse (sample->qos, &d->xqos);
  }
  return true;
}

static bool to_sample_endpoint (const struct ddsi_serdata_builtintopic *d, struct dds_builtintopic_endpoint *sample)
{
  ddsi_guid_t ppguid;
  convkey (&sample->key, &d->key);
  ppguid = d->key;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  convkey (&sample->participant_key, &ppguid);
  sample->participant_instance_handle = d->pphandle;
  if (d->c.kind == SDK_DATA)
  {
    assert (d->xqos.present & QP_TOPIC_NAME);
    assert (d->xqos.present & QP_TYPE_NAME);
    sample->topic_name = dds_string_dup_reuse (sample->topic_name, d->xqos.topic_name);
    sample->type_name = dds_string_dup_reuse (sample->type_name, d->xqos.type_name);
    sample->qos = dds_qos_from_xqos_reuse (sample->qos, &d->xqos);
  }
  return true;
}

static bool serdata_builtin_topicless_to_sample (const struct ddsi_sertopic *topic, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_builtintopic *d = (const struct ddsi_serdata_builtintopic *)serdata_common;
  const struct ddsi_sertopic_builtintopic *tp = (const struct ddsi_sertopic_builtintopic *)topic;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  /* FIXME: completing builtin topic support along these lines requires subscribers, publishers and topics to also become DDSI entities - which is probably a good thing anyway */
  switch (tp->type)
  {
    case DSBT_PARTICIPANT:
      return to_sample_pp (d, sample);
    case DSBT_READER:
    case DSBT_WRITER:
      return to_sample_endpoint (d, sample);
  }
  assert (0);
  return false;
}

static bool serdata_builtin_to_sample (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  return serdata_builtin_topicless_to_sample (serdata_common->topic, serdata_common, sample, bufptr, buflim);
}

static uint32_t serdata_builtin_get_size (const struct ddsi_serdata *serdata_common)
{
  (void)serdata_common;
  return 0;
}

static void serdata_builtin_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  (void)serdata_common; (void)off; (void)sz; (void)buf;
}

static struct ddsi_serdata *serdata_builtin_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  (void)serdata_common; (void)off; (void)sz; (void)ref;
  return NULL;
}

static void serdata_builtin_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  (void)serdata_common; (void)ref;
}

static size_t serdata_builtin_topic_print (const struct ddsi_sertopic *topic, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  (void)topic; (void)serdata_common;
  return (size_t) snprintf (buf, size, "(blob)");
}

const struct ddsi_serdata_ops ddsi_serdata_ops_builtintopic = {
  .get_size = serdata_builtin_get_size,
  .eqkey = serdata_builtin_eqkey,
  .free = serdata_builtin_free,
  .from_ser = 0,
  .from_keyhash = ddsi_serdata_builtin_from_keyhash,
  .from_sample = 0,
  .to_ser = serdata_builtin_to_ser,
  .to_sample = serdata_builtin_to_sample,
  .to_ser_ref = serdata_builtin_to_ser_ref,
  .to_ser_unref = serdata_builtin_to_ser_unref,
  .to_topicless = serdata_builtin_to_topicless,
  .topicless_to_sample = serdata_builtin_topicless_to_sample,
  .print = serdata_builtin_topic_print,
  .get_keyhash = 0
};
