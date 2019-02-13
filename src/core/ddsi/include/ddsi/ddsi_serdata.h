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
#ifndef DDSI_SERDATA_H
#define DDSI_SERDATA_H

#include "ddsi/q_time.h"
#include "ddsi/ddsi_sertopic.h"

struct nn_rdata;
struct nn_keyhash;

enum ddsi_serdata_kind {
  SDK_EMPTY,
  SDK_KEY,
  SDK_DATA
};

struct ddsi_serdata {
  const struct ddsi_serdata_ops *ops; /* cached from topic->serdata_ops */
  uint32_t hash;
  os_atomic_uint32_t refc;
  enum ddsi_serdata_kind kind;
  const struct ddsi_sertopic *topic;

  /* these get set by generic code after creating the serdata */
  nn_wctime_t timestamp;
  uint32_t statusinfo;

  /* FIXME: can I get rid of this one? */
  nn_mtime_t twrite; /* write time, not source timestamp, set post-throttling */
};

/* Serialised size of sample: uint32_t because the protocol can't handle samples larger than 4GB anyway */
typedef uint32_t (*ddsi_serdata_size_t) (const struct ddsi_serdata *d);

/* Free a serdata (called by unref when refcount goes to 0) */
typedef void (*ddsi_serdata_free_t) (struct ddsi_serdata *d);

/* Construct a serdata from a fragchain received over the network */
typedef struct ddsi_serdata * (*ddsi_serdata_from_ser_t) (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size);

/* Construct a serdata from a keyhash (an SDK_KEY by definition) */
typedef struct ddsi_serdata * (*ddsi_serdata_from_keyhash_t) (const struct ddsi_sertopic *topic, const struct nn_keyhash *keyhash);

/* Construct a serdata from an application sample */
typedef struct ddsi_serdata * (*ddsi_serdata_from_sample_t) (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const void *sample);

/* Construct a topic-less serdata with a keyvalue given a normal serdata (either key or data) - used for tkmap */
typedef struct ddsi_serdata * (*ddsi_serdata_to_topicless_t) (const struct ddsi_serdata *d);

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <=
   alignup4(size(d)) */
typedef void (*ddsi_serdata_to_ser_t) (const struct ddsi_serdata *d, size_t off, size_t sz, void *buf);

/* Provide a pointer to 'size' bytes of serialised data, starting from 'off'; 0 <= off < off+sz <=
   alignup4(size(d)); it must remain valid until the corresponding call to to_ser_unref.  Multiple
   calls to to_ser_ref() may be issued in parallel, the separate ref/unref bit is there to at least
   have the option of lazily creating the serialised representation and freeing it when no one needs
   it, while the sample itself remains valid */
typedef struct ddsi_serdata * (*ddsi_serdata_to_ser_ref_t) (const struct ddsi_serdata *d, size_t off, size_t sz, os_iovec_t *ref);

/* Release a lock on serialised data, ref must be a pointer previously obtained by calling
   to_ser_ref(d, off, sz) for some offset off. */
typedef void (*ddsi_serdata_to_ser_unref_t) (struct ddsi_serdata *d, const os_iovec_t *ref);

/* Turn serdata into an application sample (or just the key values if only key values are
   available); return false on error (typically out-of-memory, but if from_ser doesn't do any
   validation it might be a deserialisation error, too).

   If (bufptr != 0), then *bufptr .. buflim is space to be used from *bufptr up (with minimal
   padding) for any data in the sample that needs to be allocated (e.g., strings, sequences);
   otherwise malloc() is to be used for those.  (This allows read/take to be given a block of memory
   by the caller.) */
typedef bool (*ddsi_serdata_to_sample_t) (const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);

/* Create a sample from a topicless serdata, as returned by serdata_to_topicless. This sample
   obviously has just the key fields filled in, and is used for generating invalid samples. */
typedef bool (*ddsi_serdata_topicless_to_sample_t) (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);

/* Test key values of two serdatas for equality. The two will have the same ddsi_serdata_ops,
   but are not necessarily of the same topic (one can decide to never consider them equal if they
   are of different topics, of course; but the nice thing about _not_ doing that is that all
   instances with a certain key value with have the same instance id, and that in turn makes
   computing equijoins across topics much simpler). */
typedef bool (*ddsi_serdata_eqkey_t) (const struct ddsi_serdata *a, const struct ddsi_serdata *b);

struct ddsi_serdata_ops {
  ddsi_serdata_eqkey_t eqkey;
  ddsi_serdata_size_t get_size;
  ddsi_serdata_from_ser_t from_ser;
  ddsi_serdata_from_keyhash_t from_keyhash;
  ddsi_serdata_from_sample_t from_sample;
  ddsi_serdata_to_ser_t to_ser;
  ddsi_serdata_to_ser_ref_t to_ser_ref;
  ddsi_serdata_to_ser_unref_t to_ser_unref;
  ddsi_serdata_to_sample_t to_sample;
  ddsi_serdata_to_topicless_t to_topicless;
  ddsi_serdata_topicless_to_sample_t topicless_to_sample;
  ddsi_serdata_free_t free;
};

DDS_EXPORT void ddsi_serdata_init (struct ddsi_serdata *d, const struct ddsi_sertopic *tp, enum ddsi_serdata_kind kind);

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_ref (const struct ddsi_serdata *serdata_const) {
  struct ddsi_serdata *serdata = (struct ddsi_serdata *)serdata_const;
  os_atomic_inc32 (&serdata->refc);
  return serdata;
}

DDS_EXPORT inline void ddsi_serdata_unref (struct ddsi_serdata *serdata) {
  if (os_atomic_dec32_ov (&serdata->refc) == 1)
    serdata->ops->free (serdata);
}

DDS_EXPORT inline uint32_t ddsi_serdata_size (const struct ddsi_serdata *d) {
  return d->ops->get_size (d);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_from_ser (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size) {
  return topic->serdata_ops->from_ser (topic, kind, fragchain, size);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_from_keyhash (const struct ddsi_sertopic *topic, const struct nn_keyhash *keyhash) {
  return topic->serdata_ops->from_keyhash (topic, keyhash);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_from_sample (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const void *sample) {
  return topic->serdata_ops->from_sample (topic, kind, sample);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_to_topicless (const struct ddsi_serdata *d) {
  return d->ops->to_topicless (d);
}

DDS_EXPORT inline void ddsi_serdata_to_ser (const struct ddsi_serdata *d, size_t off, size_t sz, void *buf) {
  d->ops->to_ser (d, off, sz, buf);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_to_ser_ref (const struct ddsi_serdata *d, size_t off, size_t sz, os_iovec_t *ref) {
  return d->ops->to_ser_ref (d, off, sz, ref);
}

DDS_EXPORT inline void ddsi_serdata_to_ser_unref (struct ddsi_serdata *d, const os_iovec_t *ref) {
  d->ops->to_ser_unref (d, ref);
}

DDS_EXPORT inline bool ddsi_serdata_to_sample (const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim) {
  return d->ops->to_sample (d, sample, bufptr, buflim);
}

DDS_EXPORT inline bool ddsi_serdata_topicless_to_sample (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim) {
  return d->ops->topicless_to_sample (topic, d, sample, bufptr, buflim);
}

DDS_EXPORT inline bool ddsi_serdata_eqkey (const struct ddsi_serdata *a, const struct ddsi_serdata *b) {
  return a->ops->eqkey (a, b);
}

#endif
