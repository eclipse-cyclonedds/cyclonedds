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
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"

bool ddsi_sertopic_equal (const struct ddsi_sertopic *a, const struct ddsi_sertopic *b)
{
  if (strcmp (a->name, b->name) != 0)
    return false;
  if (strcmp (a->type_name, b->type_name) != 0)
    return false;
  if (a->serdata_basehash != b->serdata_basehash)
    return false;
  if (a->ops != b->ops)
    return false;
  if (a->serdata_ops != b->serdata_ops)
    return false;
  if (a->topickind_no_key != b->topickind_no_key)
    return false;
  return a->ops->equal (a, b);
}

uint32_t ddsi_sertopic_hash (const struct ddsi_sertopic *a)
{
  uint32_t h;
  h = ddsrt_mh3 (a->name, strlen (a->name), a->serdata_basehash);
  h = ddsrt_mh3 (a->type_name, strlen (a->type_name), h);
  h ^= a->serdata_basehash ^ (uint32_t) a->topickind_no_key;
  return h ^ a->ops->hash (a);
}

struct ddsi_sertopic *ddsi_sertopic_ref (const struct ddsi_sertopic *sertopic_const)
{
  struct ddsi_sertopic *sertopic = (struct ddsi_sertopic *) sertopic_const;
  ddsrt_atomic_inc32 (&sertopic->refc);
  return sertopic;
}

void ddsi_sertopic_unref (struct ddsi_sertopic *sertopic)
{
  if (ddsrt_atomic_dec32_nv (&sertopic->refc) == 0)
    ddsi_sertopic_free (sertopic);
}

void ddsi_sertopic_init (struct ddsi_sertopic *tp, const char *name, const char *type_name, const struct ddsi_sertopic_ops *sertopic_ops, const struct ddsi_sertopic_serdata_ops *serdata_ops, bool topickind_no_key)
{
  ddsrt_atomic_st32 (&tp->refc, 1);
  tp->name = ddsrt_strdup (name);
  tp->type_name = ddsrt_strdup (type_name);
  tp->ops = sertopic_ops;
  tp->serdata_ops = serdata_ops;
  tp->serdata_basehash = ddsi_sertopic_compute_serdata_basehash (tp->serdata_ops);
  tp->topickind_no_key = topickind_no_key;
  tp->gv = NULL;
}

void ddsi_sertopic_fini (struct ddsi_sertopic *tp)
{
  ddsrt_free (tp->name);
  ddsrt_free (tp->type_name);
}

uint32_t ddsi_sertopic_compute_serdata_basehash (const struct ddsi_sertopic_serdata_ops *ops)
{
  ddsrt_md5_state_t md5st;
  ddsrt_md5_byte_t digest[16];
  uint32_t res;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *) &ops, sizeof (ops));
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *) ops, sizeof (*ops));
  ddsrt_md5_finish (&md5st, digest);
  memcpy (&res, digest, sizeof (res));
  return res;
}

DDS_EXPORT extern inline void ddsi_sertopic_free (struct ddsi_sertopic *tp);
DDS_EXPORT extern inline void ddsi_sertopic_zero_samples (const struct ddsi_sertopic *tp, void *samples, size_t count);
DDS_EXPORT extern inline void ddsi_sertopic_realloc_samples (void **ptrs, const struct ddsi_sertopic *tp, void *old, size_t oldcount, size_t count);
DDS_EXPORT extern inline void ddsi_sertopic_free_samples (const struct ddsi_sertopic *tp, void **ptrs, size_t count, dds_free_op_t op);
DDS_EXPORT extern inline void ddsi_sertopic_zero_sample (const struct ddsi_sertopic *tp, void *sample);
DDS_EXPORT extern inline void ddsi_sertopic_free_sample (const struct ddsi_sertopic *tp, void *sample, dds_free_op_t op);
DDS_EXPORT extern inline void *ddsi_sertopic_alloc_sample (const struct ddsi_sertopic *tp);

static bool sertopic_equal_wrap (const struct ddsi_sertype *a, const struct ddsi_sertype *b)
{
  const struct ddsi_sertopic *a1 = a->wrapped_sertopic;
  const struct ddsi_sertopic *b1 = b->wrapped_sertopic;
  if (a1->ops != b1->ops)
    return false;
  else if (a1->serdata_ops != b1->serdata_ops)
    return false;
  else
    return strcmp (a1->name, b1->name) == 0 && a1->ops->equal (a1, b1);
}

static uint32_t sertopic_hash_wrap (const struct ddsi_sertype *tp)
{
  const struct ddsi_sertopic *tp1 = tp->wrapped_sertopic;
  uint32_t h;
  h = ddsrt_mh3 (tp1->name, strlen (tp1->name), tp1->serdata_basehash);
  return h ^ tp1->ops->hash (tp1);
}

static void sertopic_free_wrap (struct ddsi_sertype *tp)
{
  ddsi_sertopic_unref (tp->wrapped_sertopic);
  ddsi_sertype_fini (tp);
  ddsrt_free (tp);
}

static void ddsi_sertopic_zero_samples_wrap (const struct ddsi_sertype *d, void *samples, size_t count)
{
  ddsi_sertopic_zero_samples (d->wrapped_sertopic, samples, count);
}

static void ddsi_sertopic_realloc_samples_wrap (void **ptrs, const struct ddsi_sertype *d, void *old, size_t oldcount, size_t count)
{
  ddsi_sertopic_realloc_samples (ptrs, d->wrapped_sertopic, old, oldcount, count);
}

static void ddsi_sertopic_free_samples_wrap (const struct ddsi_sertype *d, void **ptrs, size_t count, dds_free_op_t op)
{
  ddsi_sertopic_free_samples (d->wrapped_sertopic, ptrs, count, op);
}

static const struct ddsi_sertype_ops sertopic_ops_wrap = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertopic_equal_wrap,
  .hash = sertopic_hash_wrap,
  .free = sertopic_free_wrap,
  .zero_samples = ddsi_sertopic_zero_samples_wrap,
  .realloc_samples = ddsi_sertopic_realloc_samples_wrap,
  .free_samples = ddsi_sertopic_free_samples_wrap,
  .type_id = 0,
  .type_map = 0,
  .type_info = 0,
  .get_serialized_size = 0,
  .serialize_into = 0
};

static uint32_t sertopic_serdata_get_size_wrap (const struct ddsi_serdata *d)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  return x->serdata_ops->get_size (w->compat_wrap);
}

static void sertopic_serdata_free_wrap (struct ddsi_serdata *d)
{
  struct ddsi_serdata_wrapper *w = (struct ddsi_serdata_wrapper *) d;
  struct ddsi_serdata *dwrapped = w->compat_wrap;
  if (ddsrt_atomic_dec32_ov (&dwrapped->refc) == 1)
    dwrapped->ops->free (dwrapped);
  ddsrt_free (d);
}

static struct ddsi_serdata *wrap_serdata (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, void *old)
{
  if (old == NULL)
    return NULL;
  struct ddsi_serdata * const old_typed = old;
  struct ddsi_serdata_wrapper * const wrap = ddsrt_malloc (sizeof (*wrap));
  if (wrap == NULL)
  {
    if (ddsrt_atomic_dec32_ov (&old_typed->refc) == 1)
      old_typed->ops->free (old_typed);
    return NULL;
  }
  ddsi_serdata_init (&wrap->c, type, kind);
  wrap->compat_wrap = old;
  wrap->c.statusinfo = old_typed->statusinfo;
  wrap->c.timestamp = old_typed->timestamp;
  return &wrap->c;
}

struct ddsi_serdata *ddsi_sertopic_wrap_serdata (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, void *old)
{
  return wrap_serdata (type, kind, ddsi_serdata_ref (old));
}

static struct ddsi_serdata *sertopic_serdata_from_ser_wrap (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  const struct ddsi_sertopic *x = type->wrapped_sertopic;
  return wrap_serdata (type, kind, x->serdata_ops->from_ser (type->wrapped_sertopic, kind, fragchain, size));
}

static struct ddsi_serdata *sertopic_serdata_from_ser_iov_wrap (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  const struct ddsi_sertopic *x = type->wrapped_sertopic;
  return wrap_serdata (type, kind, x->serdata_ops->from_ser_iov (type->wrapped_sertopic, kind, niov, iov, size));
}

static struct ddsi_serdata *sertopic_serdata_from_keyhash_wrap (const struct ddsi_sertype *type, const struct ddsi_keyhash *keyhash)
{
  const struct ddsi_sertopic *x = type->wrapped_sertopic;
  return wrap_serdata (type, SDK_KEY, x->serdata_ops->from_keyhash (type->wrapped_sertopic, keyhash));
}

static struct ddsi_serdata *sertopic_serdata_from_sample_wrap (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct ddsi_sertopic *x = type->wrapped_sertopic;
  return wrap_serdata (type, kind, x->serdata_ops->from_sample (type->wrapped_sertopic, kind, sample));
}

static struct ddsi_serdata *sertopic_serdata_to_untyped_wrap (const struct ddsi_serdata *d)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  return wrap_serdata (d->type, SDK_KEY, x->serdata_ops->to_topicless (w->compat_wrap));
}

static void sertopic_serdata_to_ser_wrap (const struct ddsi_serdata *d, size_t off, size_t sz, void *buf)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  x->serdata_ops->to_ser (w->compat_wrap, off, sz, buf);
}

static struct ddsi_serdata *sertopic_serdata_to_ser_ref_wrap (const struct ddsi_serdata *d, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  x->serdata_ops->to_ser_ref (w->compat_wrap, off, sz, ref);
  return ddsi_serdata_ref (d);
}

static void sertopic_serdata_to_ser_unref_wrap (struct ddsi_serdata *d, const ddsrt_iovec_t *ref)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  struct ddsi_serdata_wrapper *w = (struct ddsi_serdata_wrapper *) d;
  x->serdata_ops->to_ser_unref (w->compat_wrap, ref);
  ddsi_serdata_unref (d);
}

static bool sertopic_serdata_to_sample_wrap (const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  return x->serdata_ops->to_sample (w->compat_wrap, sample, bufptr, buflim);
}

static bool sertopic_serdata_untyped_to_sample_wrap (const struct ddsi_sertype *type, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim)
{
  const struct ddsi_sertopic *x = type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  return x->serdata_ops->topicless_to_sample (type->wrapped_sertopic, w->compat_wrap, sample, bufptr, buflim);
}

static bool sertopic_serdata_eqkey_wrap (const struct ddsi_serdata *a, const struct ddsi_serdata *b)
{
  const struct ddsi_sertopic *x = a->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *aw = (const struct ddsi_serdata_wrapper *) a;
  const struct ddsi_serdata_wrapper *bw = (const struct ddsi_serdata_wrapper *) b;
  return x->serdata_ops->eqkey (aw->compat_wrap, bw->compat_wrap);
}

static size_t sertopic_serdata_print_wrap (const struct ddsi_sertype *type, const struct ddsi_serdata *d, char *buf, size_t size)
{
  const struct ddsi_sertopic *x = type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  return x->serdata_ops->print (type->wrapped_sertopic, w->compat_wrap, buf, size);
}

static void sertopic_serdata_get_keyhash_wrap (const struct ddsi_serdata *d, struct ddsi_keyhash *buf, bool force_md5)
{
  const struct ddsi_sertopic *x = d->type->wrapped_sertopic;
  const struct ddsi_serdata_wrapper *w = (const struct ddsi_serdata_wrapper *) d;
  x->serdata_ops->get_keyhash (w->compat_wrap, buf, force_md5);
}

const struct ddsi_serdata_ops ddsi_sertopic_serdata_ops_wrap = {
  .eqkey = sertopic_serdata_eqkey_wrap,
  .get_size = sertopic_serdata_get_size_wrap,
  .from_ser = sertopic_serdata_from_ser_wrap,
  .from_ser_iov = sertopic_serdata_from_ser_iov_wrap,
  .from_keyhash = sertopic_serdata_from_keyhash_wrap,
  .from_sample = sertopic_serdata_from_sample_wrap,
  .to_ser = sertopic_serdata_to_ser_wrap,
  .to_ser_ref = sertopic_serdata_to_ser_ref_wrap,
  .to_ser_unref = sertopic_serdata_to_ser_unref_wrap,
  .to_sample = sertopic_serdata_to_sample_wrap,
  .to_untyped = sertopic_serdata_to_untyped_wrap,
  .untyped_to_sample = sertopic_serdata_untyped_to_sample_wrap,
  .free = sertopic_serdata_free_wrap,
  .print = sertopic_serdata_print_wrap,
  .get_keyhash = sertopic_serdata_get_keyhash_wrap
};

struct ddsi_sertype *ddsi_sertype_from_sertopic (struct ddsi_sertopic *tp)
{
  struct ddsi_sertype *st;
  st = ddsrt_malloc (sizeof (*st));
  ddsi_sertype_init (st, tp->type_name, &sertopic_ops_wrap, &ddsi_sertopic_serdata_ops_wrap, tp->topickind_no_key);
  // count the reference from the wrapping sertype to the sertopic in the sertopic:
  // provided the application handles the refcounting correctly, this means the sertopic
  // won't be freed while our sertype is still referencing it
  st->wrapped_sertopic = ddsi_sertopic_ref (tp);
  return st;
}

void ddsi_sertopic_serdata_init (struct ddsi_sertopic_serdata *d, const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind)
{
  ddsi_serdata_init ((struct ddsi_serdata *) d, (struct ddsi_sertype *) topic, kind);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_ref (const struct ddsi_sertopic_serdata *serdata_const)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_ref ((struct ddsi_serdata *) serdata_const);
}

void ddsi_sertopic_serdata_unref (struct ddsi_sertopic_serdata *serdata)
{
  ddsi_serdata_unref ((struct ddsi_serdata *) serdata);
}

uint32_t ddsi_sertopic_serdata_size (const struct ddsi_sertopic_serdata *d)
{
  return ddsi_serdata_size ((struct ddsi_serdata *) d);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_from_ser (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_from_ser ((struct ddsi_sertype *) topic, kind, fragchain, size);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_from_ser_iov (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_from_ser_iov ((struct ddsi_sertype *) topic, kind, niov, iov, size);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_from_keyhash (const struct ddsi_sertopic *topic, const struct ddsi_keyhash *keyhash)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_from_keyhash ((struct ddsi_sertype *) topic, keyhash);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_from_sample (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const void *sample)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_from_sample ((struct ddsi_sertype *) topic, kind, sample);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_to_topicless (const struct ddsi_sertopic_serdata *d)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_to_untyped ((struct ddsi_serdata *) d);
}

void ddsi_sertopic_serdata_to_ser (const struct ddsi_sertopic_serdata *d, size_t off, size_t sz, void *buf)
{
  ddsi_serdata_to_ser ((struct ddsi_serdata *) d, off, sz, buf);
}

struct ddsi_sertopic_serdata *ddsi_sertopic_serdata_to_ser_ref (const struct ddsi_sertopic_serdata *d, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  return (struct ddsi_sertopic_serdata *) ddsi_serdata_to_ser_ref ((struct ddsi_serdata *) d, off, sz, ref);
}

void ddsi_sertopic_serdata_to_ser_unref (struct ddsi_sertopic_serdata *d, const ddsrt_iovec_t *ref)
{
  ddsi_serdata_to_ser_unref ((struct ddsi_serdata *) d, ref);
}

bool ddsi_sertopic_serdata_to_sample (const struct ddsi_sertopic_serdata *d, void *sample, void **bufptr, void *buflim)
{
  return ddsi_serdata_to_sample ((struct ddsi_serdata *) d, sample, bufptr, buflim);
}

bool ddsi_sertopic_serdata_topicless_to_sample (const struct ddsi_sertopic *topic, const struct ddsi_sertopic_serdata *d, void *sample, void **bufptr, void *buflim)
{
  return ddsi_serdata_untyped_to_sample ((struct ddsi_sertype *) topic, (struct ddsi_serdata *) d, sample, bufptr, buflim);
}

bool ddsi_sertopic_serdata_eqkey (const struct ddsi_sertopic_serdata *a, const struct ddsi_sertopic_serdata *b)
{
  return ddsi_serdata_eqkey ((struct ddsi_serdata *) a, (struct ddsi_serdata *) b);
}

bool ddsi_sertopic_serdata_print (const struct ddsi_sertopic_serdata *d, char *buf, size_t size)
{
  return ddsi_serdata_print ((struct ddsi_serdata *) d, buf, size);
}

bool ddsi_sertopic_serdata_print_topicless (const struct ddsi_sertopic *topic, const struct ddsi_sertopic_serdata *d, char *buf, size_t size)
{
  return ddsi_serdata_print_untyped ((struct ddsi_sertype *) topic, (struct ddsi_serdata *) d, buf, size);
}

void ddsi_sertopic_serdata_get_keyhash (const struct ddsi_sertopic_serdata *d, struct ddsi_keyhash *buf, bool force_md5)
{
  ddsi_serdata_get_keyhash ((struct ddsi_serdata *) d, buf, force_md5);
}
