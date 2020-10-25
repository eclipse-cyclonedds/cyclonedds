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

#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_keyhash.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct nn_rdata;

enum ddsi_serdata_kind {
  SDK_EMPTY,
  SDK_KEY,
  SDK_DATA
};

struct ddsi_serdata {
  const struct ddsi_serdata_ops *ops; /* cached from topic->serdata_ops */
  uint32_t hash;
  ddsrt_atomic_uint32_t refc;
  enum ddsi_serdata_kind kind;
  const struct ddsi_sertopic *topic;

  /* these get set by generic code after creating the serdata */
  ddsrt_wctime_t timestamp;
  uint32_t statusinfo;

  /* FIXME: can I get rid of this one? */
  ddsrt_mtime_t twrite; /* write time, not source timestamp, set post-throttling */
};

/* Serialised size of sample inclusive of DDSI encoding header
   - uint32_t because the protocol can't handle samples larger than 4GB anyway
   - FIXME: get the encoding header out of the serialised data */
typedef uint32_t (*ddsi_serdata_size_t) (const struct ddsi_serdata *d);

/* Free a serdata (called by unref when refcount goes to 0) */
typedef void (*ddsi_serdata_free_t) (struct ddsi_serdata *d);

/* Construct a serdata from a fragchain received over the network
   - "kind" is KEY or DATA depending on the type of payload
   - "size" is the serialised size of the sample, inclusive of DDSI encoding header
   - the first fragchain always contains the encoding header in its entirety
   - fragchains may overlap, though I have never seen any DDS implementation
     actually send such nasty fragments
   - FIXME: get the encoding header out of the serialised data */
typedef struct ddsi_serdata * (*ddsi_serdata_from_ser_t) (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size);

/* Exactly like ddsi_serdata_from_ser_t, but with the data in an iovec and guaranteed absence of overlap */
typedef struct ddsi_serdata * (*ddsi_serdata_from_ser_iov_t) (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size);

/* Construct a serdata from a keyhash (an SDK_KEY by definition) */
typedef struct ddsi_serdata * (*ddsi_serdata_from_keyhash_t) (const struct ddsi_sertopic *topic, const struct ddsi_keyhash *keyhash);

/* Construct a serdata from an application sample
   - "kind" is KEY or DATA depending on the operation invoked by the application;
     e.g., write results in kind = DATA, dispose in kind = KEY.  The important bit
     is to not assume anything of the contents of non-key fields if kind = KEY
     unless additional application knowledge is available */
typedef struct ddsi_serdata * (*ddsi_serdata_from_sample_t) (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const void *sample);

/* Construct a topic-less serdata with just a keyvalue given a normal serdata (either key or data)
   - used for mapping key values to instance ids in tkmap
   - two reasons: size (keys are typically smaller than samples), and data in tkmap
     is shared across topics
   - whether a serdata is topicless or not is known from the context, and the topic
     field may have any value for a topicless serdata (so in some cases, one can
     simply do "return ddsi_serdata_ref(d);"
 */
typedef struct ddsi_serdata * (*ddsi_serdata_to_topicless_t) (const struct ddsi_serdata *d);

/* Fill buffer with 'size' bytes of serialised data, starting from 'off'
   - 0 <= off < off+sz <= alignup4(size(d))
   - bytes at offsets 0 .. 3 are DDSI encoding header, size(d) includes that header
   - what to copy for bytes in [size(d), alignup4(size(d))) depends on the serdata
     implementation, the protocol treats them as undefined
   - FIXME: get the encoding header out of the serialised data */
typedef void (*ddsi_serdata_to_ser_t) (const struct ddsi_serdata *d, size_t off, size_t sz, void *buf);

/* Provide a pointer to 'size' bytes of serialised data, starting from 'off'
   - see ddsi_serdata_to_ser_t above
   - instead of copying, this gives a reference that must remain valid until the
     corresponding call to to_ser_unref
   - multiple calls to to_ser_ref() may be issued in parallel
   - lazily creating the serialised representation is allowed (though I'm not sure
     how that would work with knowing the serialised size beforehand ...) */
typedef struct ddsi_serdata * (*ddsi_serdata_to_ser_ref_t) (const struct ddsi_serdata *d, size_t off, size_t sz, ddsrt_iovec_t *ref);

/* Release a lock on serialised data
   - ref was previousy filled by ddsi_serdata_to_ser_ref_t */
typedef void (*ddsi_serdata_to_ser_unref_t) (struct ddsi_serdata *d, const ddsrt_iovec_t *ref);

/* Turn serdata into an application sample (or just the key values if only key values are
   available); return false on error (typically out-of-memory, but if from_ser doesn't do any
   validation it might be a deserialisation error, too).

   If (bufptr != 0), then *bufptr .. buflim is space to be used from *bufptr up (with minimal
   padding) for any data in the sample that needs to be allocated (e.g., strings, sequences);
   otherwise malloc() is to be used for those.  (This allows read/take to be given a block of memory
   by the caller.) */
typedef bool (*ddsi_serdata_to_sample_t) (const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);

/* Create a sample from a topicless serdata, as returned by serdata_to_topicless.  This sample
   obviously has just the key fields filled in and is used for generating invalid samples. */
typedef bool (*ddsi_serdata_topicless_to_sample_t) (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);

/* Test key values of two serdatas for equality.  The two will have the same ddsi_serdata_ops,
   but are not necessarily of the same topic (one can decide to never consider them equal if they
   are of different topics, of course; but the nice thing about _not_ doing that is that all
   instances with a certain key value with have the same instance id, and that in turn makes
   computing equijoins across topics much simpler). */
typedef bool (*ddsi_serdata_eqkey_t) (const struct ddsi_serdata *a, const struct ddsi_serdata *b);

/* Print a serdata into the provided buffer (truncating as necessary)
   - topic is present for supporting printing of "topicless" samples
   - buf != NULL, bufsize > 0 on input
   - buf must always be terminated with a nul character on return
   - returns the number of characters (excluding the terminating 0) needed to print it
     in full (or, as an optimization, it may pretend that it has printed it in full,
     returning bufsize-1) if it had to truncate) */
typedef size_t (*ddsi_serdata_print_t) (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, char *buf, size_t size);

/* Add keyhash (from serdata) to buffer (forcing md5 when necessary).
   - key needs to be set within serdata (can already be md5)
   - buf needs to be at least 16 bytes large */
typedef void (*ddsi_serdata_get_keyhash_t) (const struct ddsi_serdata *d, struct ddsi_keyhash *buf, bool force_md5);

struct ddsi_serdata_ops {
  ddsi_serdata_eqkey_t eqkey;
  ddsi_serdata_size_t get_size;
  ddsi_serdata_from_ser_t from_ser;
  ddsi_serdata_from_ser_iov_t from_ser_iov;
  ddsi_serdata_from_keyhash_t from_keyhash;
  ddsi_serdata_from_sample_t from_sample;
  ddsi_serdata_to_ser_t to_ser;
  ddsi_serdata_to_ser_ref_t to_ser_ref;
  ddsi_serdata_to_ser_unref_t to_ser_unref;
  ddsi_serdata_to_sample_t to_sample;
  ddsi_serdata_to_topicless_t to_topicless;
  ddsi_serdata_topicless_to_sample_t topicless_to_sample;
  ddsi_serdata_free_t free;
  ddsi_serdata_print_t print;
  ddsi_serdata_get_keyhash_t get_keyhash;
};

#define DDSI_SERDATA_HAS_PRINT 1
#define DDSI_SERDATA_HAS_FROM_SER_IOV 1
#define DDSI_SERDATA_HAS_GET_KEYHASH 1

DDS_EXPORT void ddsi_serdata_init (struct ddsi_serdata *d, const struct ddsi_sertopic *tp, enum ddsi_serdata_kind kind);

/**
 * @brief Return a reference to a serdata with possible topic conversion
 *
 * If `serdata` is of topic `topic`, this increments the reference count and returns
 * `serdata`.  Otherwise, it constructs a new one from the serialised representation of
 * `serdata`.  This can fail, in which case it returns NULL.
 *
 * @param[in] topic    sertopic the returned serdata must have
 * @param[in] serdata  source sample (untouched except for the reference count and/or
 *   extracting the serialised representation)
 * @returns A reference to a serdata that is equivalent to the input with the correct
 *   topic, or a null pointer on failure.  The reference must be released with @ref
 *   ddsi_serdata_unref.
 */
DDS_EXPORT struct ddsi_serdata *ddsi_serdata_ref_as_topic (const struct ddsi_sertopic *topic, struct ddsi_serdata *serdata);

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_ref (const struct ddsi_serdata *serdata_const) {
  struct ddsi_serdata *serdata = (struct ddsi_serdata *)serdata_const;
  ddsrt_atomic_inc32 (&serdata->refc);
  return serdata;
}

DDS_EXPORT inline void ddsi_serdata_unref (struct ddsi_serdata *serdata) {
  if (ddsrt_atomic_dec32_ov (&serdata->refc) == 1)
    serdata->ops->free (serdata);
}

DDS_EXPORT inline uint32_t ddsi_serdata_size (const struct ddsi_serdata *d) {
  return d->ops->get_size (d);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_from_ser (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size) {
  return topic->serdata_ops->from_ser (topic, kind, fragchain, size);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_from_ser_iov (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size) {
  return topic->serdata_ops->from_ser_iov (topic, kind, niov, iov, size);
}

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_from_keyhash (const struct ddsi_sertopic *topic, const struct ddsi_keyhash *keyhash) {
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

DDS_EXPORT inline struct ddsi_serdata *ddsi_serdata_to_ser_ref (const struct ddsi_serdata *d, size_t off, size_t sz, ddsrt_iovec_t *ref) {
  return d->ops->to_ser_ref (d, off, sz, ref);
}

DDS_EXPORT inline void ddsi_serdata_to_ser_unref (struct ddsi_serdata *d, const ddsrt_iovec_t *ref) {
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

DDS_EXPORT inline bool ddsi_serdata_print (const struct ddsi_serdata *d, char *buf, size_t size) {
  return d->ops->print (d->topic, d, buf, size);
}

DDS_EXPORT inline bool ddsi_serdata_print_topicless (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, char *buf, size_t size) {
  if (d->ops->print)
    return d->ops->print (topic, d, buf, size);
  else
  {
    buf[0] = 0;
    return 0;
  }
}

DDS_EXPORT inline void ddsi_serdata_get_keyhash (const struct ddsi_serdata *d, struct ddsi_keyhash *buf, bool force_md5) {
  d->ops->get_keyhash (d, buf, force_md5);
}

#if defined (__cplusplus)
}
#endif

#endif
