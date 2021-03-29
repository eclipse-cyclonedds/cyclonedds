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
#include "dds/ddsi/ddsi_plist.h"
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

static struct ddsi_serdata *fix_serdata_builtin(struct ddsi_serdata_builtintopic *d, enum ddsi_sertype_builtintopic_entity_kind kind, uint32_t basehash)
{
#ifndef DDS_HAS_TOPIC_DISCOVERY
  assert (kind != DSBT_TOPIC);
#endif
  if (kind == DSBT_TOPIC)
    d->c.hash = (* (uint32_t *) d->key.raw) ^ basehash;
  else
    d->c.hash = hash_guid (&d->key.guid) ^ basehash;
  return &d->c;
}

static bool serdata_builtin_eqkey(const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  struct ddsi_serdata_builtintopic *a = (struct ddsi_serdata_builtintopic *) acmn;
  struct ddsi_serdata_builtintopic *b = (struct ddsi_serdata_builtintopic *) bcmn;
  DDSRT_STATIC_ASSERT(sizeof(a->key.raw) == sizeof(a->key.guid));
#ifndef DDS_HAS_TOPIC_DISCOVERY
  assert (((struct ddsi_sertype_builtintopic *)acmn->type)->entity_kind != DSBT_TOPIC);
#endif
  return memcmp (&a->key, &b->key, sizeof (a->key)) == 0;
}

static void serdata_builtin_free(struct ddsi_serdata *dcmn)
{
  struct ddsi_serdata_builtintopic *d = (struct ddsi_serdata_builtintopic *) dcmn;
  if (d->c.kind == SDK_DATA)
    ddsi_xqos_fini (&d->xqos);
  ddsrt_free (d);
}

static struct ddsi_serdata_builtintopic *serdata_builtin_new(const struct ddsi_sertype_builtintopic *tp, enum ddsi_serdata_kind serdata_kind)
{
  size_t size = 0;
  switch (tp->entity_kind)
  {
    case DSBT_PARTICIPANT:
      size = sizeof (struct ddsi_serdata_builtintopic_participant);
      break;
    case DSBT_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      size = sizeof (struct ddsi_serdata_builtintopic_topic);
#else
      assert(0);
#endif
      break;
    case DSBT_READER:
    case DSBT_WRITER:
      size = sizeof (struct ddsi_serdata_builtintopic_endpoint);
      break;
  }
  struct ddsi_serdata_builtintopic *d = ddsrt_malloc(size);
  ddsi_serdata_init (&d->c, &tp->c, serdata_kind);
  return d;
}

static void from_entity_pp (struct ddsi_serdata_builtintopic_participant *d, const struct participant *pp)
{
  ddsi_xqos_copy(&d->common.xqos, &pp->plist->qos);
  d->pphandle = pp->e.iid;
}

static void from_entity_proxypp (struct ddsi_serdata_builtintopic_participant *d, const struct proxy_participant *proxypp)
{
  ddsi_xqos_copy(&d->common.xqos, &proxypp->plist->qos);
  d->pphandle = proxypp->e.iid;
}

static void from_qos (struct ddsi_serdata_builtintopic *d, const dds_qos_t *xqos)
{
  ddsi_xqos_copy (&d->xqos, xqos);
  assert (d->xqos.present & QP_TOPIC_NAME);
  assert (d->xqos.present & QP_TYPE_NAME);
}

static void from_entity_rd (struct ddsi_serdata_builtintopic_endpoint *d, const struct reader *rd)
{
  d->pphandle = rd->c.pp->e.iid;
#ifdef DDS_HAS_TYPE_DISCOVERY
  d->type_id = rd->c.type_id;
#endif
  from_qos (&d->common, rd->xqos);
}

static void from_entity_wr (struct ddsi_serdata_builtintopic_endpoint *d, const struct writer *wr)
{
  d->pphandle = wr->c.pp->e.iid;
#ifdef DDS_HAS_TYPE_DISCOVERY
  d->type_id = wr->c.type_id;
#endif
  from_qos (&d->common, wr->xqos);
}

static void from_proxy_endpoint_common (struct ddsi_serdata_builtintopic_endpoint *d, const struct proxy_endpoint_common *pec)
{
  d->pphandle = pec->proxypp->e.iid;
#ifdef DDS_HAS_TYPE_DISCOVERY
  d->type_id = pec->type_id;
#endif
  from_qos (&d->common, pec->xqos);
}

static void from_entity_proxy_rd (struct ddsi_serdata_builtintopic_endpoint *d, const struct proxy_reader *proxyrd)
{
  from_proxy_endpoint_common (d, &proxyrd->c);
}

static void from_entity_proxy_wr (struct ddsi_serdata_builtintopic_endpoint *d, const struct proxy_writer *proxywr)
{
  from_proxy_endpoint_common (d, &proxywr->c);
}

struct ddsi_serdata *dds_serdata_builtin_from_endpoint (const struct ddsi_sertype *tpcmn, const ddsi_guid_t *guid, struct entity_common *entity, enum ddsi_serdata_kind kind)
{
  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *)tpcmn;
  assert (tp->entity_kind != DSBT_TOPIC);
  struct ddsi_serdata_builtintopic *d = serdata_builtin_new (tp, kind);
  d->key.guid = *guid;
  if (entity != NULL && kind == SDK_DATA)
  {
    ddsrt_mutex_lock (&entity->qos_lock);
    switch (entity->kind)
    {
      case EK_PARTICIPANT:
        assert (tp->entity_kind == DSBT_PARTICIPANT);
        from_entity_pp ((struct ddsi_serdata_builtintopic_participant *) d, (const struct participant *) entity);
        break;
      case EK_READER:
        assert (tp->entity_kind == DSBT_READER);
        from_entity_rd ((struct ddsi_serdata_builtintopic_endpoint *) d, (const struct reader *) entity);
        break;
      case EK_WRITER:
        assert (tp->entity_kind == DSBT_WRITER);
        from_entity_wr ((struct ddsi_serdata_builtintopic_endpoint *) d, (const struct writer *) entity);
        break;
      case EK_PROXY_PARTICIPANT:
        assert (tp->entity_kind == DSBT_PARTICIPANT);
        from_entity_proxypp ((struct ddsi_serdata_builtintopic_participant *) d, (const struct proxy_participant *) entity);
        break;
      case EK_PROXY_READER:
        assert (tp->entity_kind == DSBT_READER);
        from_entity_proxy_rd ((struct ddsi_serdata_builtintopic_endpoint *) d, (const struct proxy_reader *) entity);
        break;
      case EK_PROXY_WRITER:
        assert (tp->entity_kind == DSBT_WRITER);
        from_entity_proxy_wr ((struct ddsi_serdata_builtintopic_endpoint *) d, (const struct proxy_writer *) entity);
        break;
      case EK_TOPIC:
        abort ();
        break;
    }
    ddsrt_mutex_unlock (&entity->qos_lock);
  }
  return fix_serdata_builtin(d, tp->entity_kind, tp->c.serdata_basehash);
}

static struct ddsi_serdata *ddsi_serdata_builtin_from_sample (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *) tpcmn;
  union {
    dds_guid_t extguid;
    ddsi_guid_t guid;
    ddsi_keyhash_t keyhash;
  } x;

  /* no-one should be trying to convert user-provided data into a built-in topic sample, but converting
     a key is something that can be necessary, e.g., dds_lookup_instance depends on it */
  if (kind != SDK_KEY)
    return NULL;

  /* memset x (even though it is entirely superfluous) so we can leave out a default case from the
     switch (ensuring at least some compilers will warn when more types are added) without getting
     warnings from any compiler */
  memset (&x, 0, sizeof (x));
  switch (tp->entity_kind)
  {
    case DSBT_PARTICIPANT: {
      const dds_builtintopic_participant_t *s = sample;
      x.extguid = s->key;
      break;
    }
    case DSBT_READER:
    case DSBT_WRITER: {
      const dds_builtintopic_endpoint_t *s = sample;
      x.extguid = s->key;
      break;
    case DSBT_TOPIC:
      assert (0);
      break;
    }
  }
  struct ddsi_domaingv * const gv = ddsrt_atomic_ldvoidp (&tp->c.gv);
  x.guid = nn_ntoh_guid (x.guid);
  struct entity_common *entity = entidx_lookup_guid_untyped (gv->entity_index, &x.guid);
  return dds_serdata_builtin_from_endpoint (tpcmn, &x.guid, entity, kind);
}

static struct ddsi_serdata *serdata_builtin_to_untyped (const struct ddsi_serdata *serdata_common)
{
  /* All built-in ones are currently untyped */
  return ddsi_serdata_ref (serdata_common);
}

static void convkey (dds_guid_t *key, const ddsi_guid_t *guid)
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

#ifdef DDS_HAS_TYPE_DISCOVERY
static void *dds_mem_dup_reuse (void *old, const void *src, size_t size)
{
  void *new = dds_realloc (old, size);
  return memcpy (new, src, size);
}
#endif

static dds_qos_t *dds_qos_from_xqos_reuse (dds_qos_t *old, const dds_qos_t *src)
{
  if (old == NULL)
    old = ddsrt_malloc (sizeof (*old));
  else
  {
    ddsi_xqos_fini (old);
  }
  ddsi_xqos_init_empty (old);
  ddsi_xqos_mergein_missing (old, src, ~(QP_TOPIC_NAME | QP_TYPE_NAME));
  return old;
}

static bool to_sample_pp (const struct ddsi_serdata_builtintopic_participant *d, struct dds_builtintopic_participant *sample)
{
  convkey (&sample->key, &d->common.key.guid);
  if (d->common.c.kind == SDK_DATA)
    sample->qos = dds_qos_from_xqos_reuse (sample->qos, &d->common.xqos);
  return true;
}

static bool to_sample_endpoint (const struct ddsi_serdata_builtintopic_endpoint *dep, struct dds_builtintopic_endpoint *sample)
{
  ddsi_guid_t ppguid;
  convkey (&sample->key, &dep->common.key.guid);
  ppguid = dep->common.key.guid;
  ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
  convkey (&sample->participant_key, &ppguid);
  sample->participant_instance_handle = dep->pphandle;
  if (dep->common.c.kind == SDK_DATA)
  {
    assert (dep->common.xqos.present & QP_TOPIC_NAME);
    assert (dep->common.xqos.present & QP_TYPE_NAME);
    sample->topic_name = dds_string_dup_reuse (sample->topic_name, dep->common.xqos.topic_name);
    sample->type_name = dds_string_dup_reuse (sample->type_name, dep->common.xqos.type_name);
    sample->qos = dds_qos_from_xqos_reuse (sample->qos, &dep->common.xqos);
#ifdef DDS_HAS_TYPE_DISCOVERY
    if (!(sample->qos->present & QP_CYCLONE_TYPE_INFORMATION))
    {
      sample->qos->type_information.value = NULL;
      sample->qos->present |= QP_CYCLONE_TYPE_INFORMATION;
    }
    sample->qos->type_information.length = (uint32_t) sizeof (dep->type_id);
    sample->qos->type_information.value = dds_mem_dup_reuse (sample->qos->type_information.value, &dep->type_id, sample->qos->type_information.length);
#endif
  }
  return true;
}

#ifdef DDS_HAS_TOPIC_DISCOVERY
static bool to_sample_topic (const struct ddsi_serdata_builtintopic_topic *dtp, struct dds_builtintopic_topic *sample)
{
  memcpy (&sample->key, &dtp->common.key.raw, sizeof (sample->key));
  if (dtp->common.c.kind == SDK_DATA)
  {
    assert (dtp->common.xqos.present & QP_TOPIC_NAME);
    assert (dtp->common.xqos.present & QP_TYPE_NAME);
    sample->topic_name = dds_string_dup_reuse (sample->topic_name, dtp->common.xqos.topic_name);
    sample->type_name = dds_string_dup_reuse (sample->type_name, dtp->common.xqos.type_name);
    sample->qos = dds_qos_from_xqos_reuse (sample->qos, &dtp->common.xqos);
    if (!(sample->qos->present & QP_CYCLONE_TYPE_INFORMATION))
    {
      sample->qos->type_information.value = NULL;
      sample->qos->present |= QP_CYCLONE_TYPE_INFORMATION;
    }
    sample->qos->type_information.length = (uint32_t) sizeof (dtp->type_id);
    sample->qos->type_information.value = dds_mem_dup_reuse (sample->qos->type_information.value, &dtp->type_id, sample->qos->type_information.length);
  }
  return true;
}
#endif /* DDS_HAS_TOPIC_DISCOVERY */

static bool serdata_builtin_untyped_to_sample (const struct ddsi_sertype *type, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_serdata_builtintopic *d = (const struct ddsi_serdata_builtintopic *)serdata_common;
  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *)type;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  /* FIXME: completing builtin topic support along these lines requires subscribers, publishers and topics to also become DDSI entities - which is probably a good thing anyway */
  switch (tp->entity_kind)
  {
    case DSBT_PARTICIPANT:
      return to_sample_pp ((struct ddsi_serdata_builtintopic_participant *)d, sample);
    case DSBT_TOPIC:
#ifdef DDS_HAS_TOPIC_DISCOVERY
      return to_sample_topic ((struct ddsi_serdata_builtintopic_topic *)d, sample);
#else
      break;
#endif
    case DSBT_READER:
    case DSBT_WRITER:
      return to_sample_endpoint ((struct ddsi_serdata_builtintopic_endpoint *)d, sample);
  }
  assert (0);
  return false;
}

static bool serdata_builtin_to_sample (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  return serdata_builtin_untyped_to_sample (serdata_common->type, serdata_common, sample, bufptr, buflim);
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

static size_t serdata_builtin_type_print (const struct ddsi_sertype *type, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  (void)type; (void)serdata_common;
  return (size_t) snprintf (buf, size, "(blob)");
}

const struct ddsi_serdata_ops ddsi_serdata_ops_builtintopic = {
  .get_size = serdata_builtin_get_size,
  .eqkey = serdata_builtin_eqkey,
  .free = serdata_builtin_free,
  .from_ser = 0,
  .from_ser_iov = 0,
  .from_keyhash = 0,
  .from_sample = ddsi_serdata_builtin_from_sample,
  .to_ser = serdata_builtin_to_ser,
  .to_sample = serdata_builtin_to_sample,
  .to_ser_ref = serdata_builtin_to_ser_ref,
  .to_ser_unref = serdata_builtin_to_ser_unref,
  .to_untyped = serdata_builtin_to_untyped,
  .untyped_to_sample = serdata_builtin_untyped_to_sample,
  .print = serdata_builtin_type_print,
  .get_keyhash = 0
};

#ifdef DDS_HAS_TOPIC_DISCOVERY

struct ddsi_serdata *dds_serdata_builtin_from_topic_definition (const struct ddsi_sertype *tpcmn, const dds_builtintopic_topic_key_t *key, const struct ddsi_topic_definition *tpd, enum ddsi_serdata_kind kind)
{
  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *) tpcmn;
  assert (tp->entity_kind == DSBT_TOPIC);
  struct ddsi_serdata_builtintopic_topic *d = (struct ddsi_serdata_builtintopic_topic *) serdata_builtin_new (tp, kind);
  memcpy (&d->common.key.raw, key, sizeof (d->common.key.raw));
  if (tpd != NULL && kind == SDK_DATA)
  {
    d->type_id = tpd->type_id;
    from_qos (&d->common, tpd->xqos);
  }
  return fix_serdata_builtin (&d->common, DSBT_TOPIC, tp->c.serdata_basehash);
}

static struct ddsi_serdata *ddsi_serdata_builtin_from_sample_topic (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  /* no-one should be trying to convert user-provided data into a built-in topic sample, but converting
     a key is something that can be necessary, e.g., dds_lookup_instance depends on it */
  if (kind != SDK_KEY)
    return NULL;

  const struct ddsi_sertype_builtintopic *tp = (const struct ddsi_sertype_builtintopic *) tpcmn;
  struct ddsi_domaingv *gv = ddsrt_atomic_ldvoidp (&tp->c.gv);
  const dds_builtintopic_topic_t *s = sample;
  union { ddsi_guid_t guid; dds_builtintopic_topic_key_t key; } x;
  x.key = s->key;
  x.guid = nn_ntoh_guid (x.guid);
  struct ddsi_topic_definition templ;
  memset (&templ, 0, sizeof (templ));
  memcpy (&templ.key, &x.key, sizeof (templ.key));
  ddsrt_mutex_lock (&gv->topic_defs_lock);
  struct ddsi_topic_definition *tpd = ddsrt_hh_lookup (gv->topic_defs, &templ);
  struct ddsi_serdata *sd = dds_serdata_builtin_from_topic_definition (tpcmn, &x.key, tpd, kind);
  ddsrt_mutex_unlock (&gv->topic_defs_lock);
  return sd;
}

const struct ddsi_serdata_ops ddsi_serdata_ops_builtintopic_topic = {
  .get_size = serdata_builtin_get_size,
  .eqkey = serdata_builtin_eqkey,
  .free = serdata_builtin_free,
  .from_ser = 0,
  .from_ser_iov = 0,
  .from_keyhash = 0,
  .from_sample = ddsi_serdata_builtin_from_sample_topic,
  .to_ser = serdata_builtin_to_ser,
  .to_sample = serdata_builtin_to_sample,
  .to_ser_ref = serdata_builtin_to_ser_ref,
  .to_ser_unref = serdata_builtin_to_ser_unref,
  .to_untyped = serdata_builtin_to_untyped,
  .untyped_to_sample = serdata_builtin_untyped_to_sample,
  .print = serdata_builtin_type_print,
  .get_keyhash = 0
};

#endif /* DDS_HAS_TOPIC_DISCOVERY */
