// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/dds.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__radmin.h"
#include "dds__entity.h"

#include "test_common.h"

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define NATIVE_ENCODING DDSI_RTPS_CDR_LE
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
#define NATIVE_ENCODING DDSI_RTPS_CDR_BE
#else
#error "DDSRT_ENDIAN neither LITTLE nor BIG"
#endif

struct sampletype {
  char *key;
  char *value;
};

struct stp {
  struct ddsi_sertype c;
};

static bool stp_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  // no fields in stp beyond the common ones, and those are all checked for equality before this function is called
  (void) acmn; (void) bcmn;
  return true;
}

static uint32_t stp_hash (const struct ddsi_sertype *tpcmn)
{
  // nothing beyond the common fields
  (void) tpcmn;
  return 0;
}

static void stp_free (struct ddsi_sertype *tpcmn)
{
  struct stp * const stp = (struct stp *) tpcmn;
  ddsi_sertype_fini (&stp->c);
  free (stp);
}

static void stpx_zero_samples (void *samples, size_t count)
{
  memset (samples, 0, count * sizeof (struct sampletype));
}

static void stp_zero_samples (const struct ddsi_sertype *dcmn, void *samples, size_t count)
{
  (void) dcmn;
  stpx_zero_samples (samples, count);
}

static void stpx_realloc_samples (void **ptrs, void *old, size_t oldcount, size_t count)
{
  const size_t size = sizeof (struct sampletype);
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static void stp_realloc_samples (void **ptrs, const struct ddsi_sertype *dcmn, void *old, size_t oldcount, size_t count)
{
  (void) dcmn;
  stpx_realloc_samples (ptrs, old, oldcount, count);
}

static void stpx_free_samples (void **ptrs, size_t count, dds_free_op_t op)
{
  if (count == 0)
    return;

  char *ptr = ptrs[0];
  for (size_t i = 0; i < count; i++)
  {
    struct sampletype *s = (struct sampletype *) ptr;
    dds_free (s->key);
    dds_free (s->value);
    ptr += sizeof (struct sampletype);
  }

  if (op & DDS_FREE_ALL_BIT)
  {
    dds_free (ptrs[0]);
  }
}

static void stp_free_samples (const struct ddsi_sertype *dcmn, void **ptrs, size_t count, dds_free_op_t op)
{
  (void) dcmn;
  stpx_free_samples (ptrs, count, op);
}

static const struct ddsi_sertype_ops stp_ops = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .free = stp_free,
  .zero_samples = stp_zero_samples,
  .realloc_samples = stp_realloc_samples,
  .free_samples = stp_free_samples,
  .equal = stp_equal,
  .hash = stp_hash
};

struct sdx {
  struct sampletype data;
  uint32_t keysz, pad0, valuesz, pad1;
};

struct sd {
  struct ddsi_serdata c;
  struct sdx x;
};

static uint32_t sdx_get_size (const struct sdx *d, enum ddsi_serdata_kind kind)
{
  // FIXME: 4 is DDSI CDR header size and shouldn't be included here; same is true for pad1
  return 4 + 4 + d->keysz + d->pad0 + (kind == SDK_DATA ? 4 + d->valuesz + d->pad1 : 0);
}

static uint32_t sd_get_size (const struct ddsi_serdata *dcmn)
{
  const struct sd *d = (const struct sd *) dcmn;
  return sdx_get_size (&d->x, d->c.kind);
}

static bool sdx_eqkey (const struct sdx *a, const struct sdx *b)
{
  return a->keysz == b->keysz && memcmp (a->data.key, b->data.key, a->keysz) == 0;
}

static bool sd_eqkey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct sd *a = (const struct sd *) acmn;
  const struct sd *b = (const struct sd *) bcmn;
  return sdx_eqkey (&a->x, &b->x);
}

static void sdx_free (struct sdx *d)
{
  free (d->data.key);
  free (d->data.value);
}

static void sd_free (struct ddsi_serdata *dcmn)
{
  struct sd *d = (struct sd *) dcmn;
  sdx_free (&d->x);
  free (d);
}

static char *strdup_with_len (const char *x, size_t l)
{
  char *y = malloc (l);
  assert(y);
  memcpy (y, x, l);
  return y;
}

static uint32_t sdx_from_ser_iov (struct sdx *d, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, uint32_t basehash)
{
  // assuming not fragmented, CDR Header specifies native encoding, input is valid and all that
  // CDR encoding of strings: uint32_t length-including-terminating-0 ; blob-including-terminating-0
  assert (niov == 1);
  (void) niov;
  size_t off = 0;
  const char *base = (const char *) iov[0].iov_base + 4;
  memcpy (&d->keysz, base + off, sizeof (uint32_t));
  off += sizeof (uint32_t);
  d->data.key = strdup_with_len (base + off, d->keysz);
  off += d->keysz;
  d->pad0 = (uint32_t) (((off % 4) == 0) ? 0 : 4 - (off % 4));
  off += d->pad0;
  if (kind == SDK_DATA)
  {
    memcpy (&d->valuesz, base + off, sizeof (uint32_t));
    off += sizeof (uint32_t);
    d->data.value = strdup_with_len (base + off, d->valuesz);
    off += d->valuesz;
    // FIXME: not sure if this is still needed, it shouldn't be, but ...
    d->pad1 = (uint32_t) (((off % 4) == 0) ? 0 : 4 - (off % 4));
  }
  else
  {
    d->data.value = NULL;
    d->valuesz = d->pad1 = 0;
  }
  return ddsrt_mh3 (d->data.key, d->keysz, 0) ^ basehash;
}

static struct ddsi_serdata *sd_from_ser_iov (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  struct stp const * const stp = (const struct stp *) tpcmn;
  struct sd *sd = malloc (sizeof (*sd));
  assert(sd);
  ddsi_serdata_init (&sd->c, &stp->c, kind);
  (void) size;
  sd->c.hash = sdx_from_ser_iov (&sd->x, kind, niov, iov, tpcmn->serdata_basehash);
  return &sd->c;
}

static struct ddsi_serdata *sd_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  assert (fragchain->nextfrag == NULL);
  ddsrt_iovec_t iov = {
    .iov_base = DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_PAYLOAD_OFF (fragchain)),
    .iov_len = fragchain->maxp1 // fragchain->min = 0 for first fragment, by definition
  };
  const ddsi_keyhash_t *kh = ddsi_serdata_keyhash_from_fragchain (fragchain);
  CU_ASSERT_FATAL (kh != NULL);
  assert (kh != NULL); // for Clang's static analyzer
  printf ("kh rcv %02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x\n",
          kh->value[0], kh->value[1], kh->value[2], kh->value[3],
          kh->value[4], kh->value[5], kh->value[6], kh->value[7],
          kh->value[8], kh->value[9], kh->value[10], kh->value[11],
          kh->value[12], kh->value[13], kh->value[14], kh->value[15]);
  return sd_from_ser_iov (tpcmn, kind, 1, &iov, size);
}

static struct ddsi_serdata *sd_from_keyhash (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  // unbounded string, therefore keyhash is MD5 and we can't extract the value
  // (don't try disposing/unregistering in RTI and expect this to accept the data)
  (void) tpcmn; (void) keyhash;
  return NULL;
}

static uint32_t sdx_from_sample (struct sdx *d, enum ddsi_serdata_kind kind, const struct sampletype *s, uint32_t basehash)
{
  d->keysz = (uint32_t) strlen (s->key) + 1;
  d->data.key = strdup_with_len (s->key, d->keysz);
  d->pad0 = ((d->keysz % 4) == 0) ? 0 : 4 - (d->keysz % 4);
  if (kind == SDK_DATA)
  {
    d->valuesz = (uint32_t) strlen (s->value) + 1;
    d->data.value = strdup_with_len (s->value, d->valuesz);
    d->pad1 = ((d->valuesz % 4) == 0) ? 0 : 4 - (d->valuesz % 4);
  }
  else
  {
    d->data.value = NULL;
    d->valuesz = d->pad1 = 0;
  }
  return ddsrt_mh3 (d->data.key, d->keysz, 0) ^ basehash;
}

static struct ddsi_serdata *sd_from_sample (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct stp *tp = (const struct stp *) tpcmn;
  const struct sampletype *s = sample;
  if (s->key == NULL || (kind == SDK_DATA && s->value == NULL))
    return NULL;
  struct sd *sd = malloc (sizeof (*sd));
  assert(sd);
  ddsi_serdata_init (&sd->c, &tp->c, kind);
  sd->c.hash = sdx_from_sample (&sd->x, kind, s, tpcmn->serdata_basehash);
  return &sd->c;
}

static void sdx_to_untyped (struct sdx *sd_tl, const struct sdx *sd)
{
  sd_tl->data.key = strdup_with_len (sd->data.key, sd->keysz);
  sd_tl->data.value = NULL;
  sd_tl->keysz = sd->keysz;
  sd_tl->pad0 = sd->pad0;
  sd_tl->valuesz = sd_tl->pad1 = 0;
}

static struct ddsi_serdata *sd_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct sd *sd = (const struct sd *) serdata_common;
  const struct stp *tp = (const struct stp *) sd->c.type;
  struct sd *sd_tl = malloc (sizeof (*sd_tl));
  assert(sd_tl);
  ddsi_serdata_init (&sd_tl->c, &tp->c, SDK_KEY);
  sd_tl->c.type = NULL;
  sd_tl->c.hash = sd->c.hash;
  sd_tl->c.timestamp.v = INT64_MIN;
  sdx_to_untyped (&sd_tl->x, &sd->x);
  return &sd_tl->c;
}

static void sdx_to_ser_ref (const struct sdx *sd, enum ddsi_serdata_kind kind, size_t cdr_off, size_t cdr_sz, ddsrt_iovec_t *ref)
{
  // The idea is that if the CDR is available already, one can return a reference (in which case
  // one probably should also increment the sample's refcount).  But here we generate the CDR
  // on the fly
  //
  // fragmented data results in calls with different offsets and limits, but can't be bothered for this test
  assert (cdr_off == 0 && cdr_sz == sdx_get_size (sd, kind));
  (void) cdr_off;
  (void) cdr_sz;
  ref->iov_len = sdx_get_size (sd, kind);
  ref->iov_base = malloc (ref->iov_len);
  char * const header = ref->iov_base;
  assert(header);
  assert(4 <= ref->iov_len);
  header[0] = 0;
  header[1] = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 1 : 0;
  header[2] = 0;
  header[3] = 0;
  char * const base = header + 4;
  size_t off = 0;
  memcpy (base + off, &sd->keysz, sizeof (uint32_t));
  off += sizeof (uint32_t);
  memcpy (base + off, sd->data.key, sd->keysz);
  off += sd->keysz;
  for (uint32_t i = 0; i < sd->pad0; i++)
    base[off++] = 0;
  if (kind == SDK_DATA)
  {
    memcpy (base + off, &sd->valuesz, sizeof (uint32_t));
    off += sizeof (uint32_t);
    memcpy (base + off, sd->data.value, sd->valuesz);
    off += sd->valuesz;
    for (uint32_t i = 0; i < sd->pad1; i++)
      base[off++] = 0;
  }
  assert (4 + off == ref->iov_len);
}

static struct ddsi_serdata *sd_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t cdr_off, size_t cdr_sz, ddsrt_iovec_t *ref)
{
  // even though we generate the CDR on the fly in a separately allocated memory
  // we still must increment the refcount of serdata_common: it is needed to
  // invoke the matching sd_to_ser_unref
  const struct sd *sd = (const struct sd *) serdata_common;
  sdx_to_ser_ref (&sd->x, sd->c.kind, cdr_off, cdr_sz, ref);
  return ddsi_serdata_ref (&sd->c);
}

static void sd_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  // const "ref" is a mistake in the interface ... it is owned by this code ...
  free ((void *) ref->iov_base);
  ddsi_serdata_unref (serdata_common);
}

static void sd_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  // being lazy and reusing code in sd_to_ser_ref, even though here we don't have to allocate anything
  ddsrt_iovec_t iov;
  (void) sd_to_ser_ref (serdata_common, off, sz, &iov);
  memcpy (buf, iov.iov_base, iov.iov_len);
  sd_to_ser_unref ((struct ddsi_serdata *) serdata_common, &iov);
}

static bool sdx_to_sample (const struct sdx *sd, enum ddsi_serdata_kind kind, struct sampletype *s, void **bufptr, void *buflim)
{
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  s->key = dds_realloc (s->key, sd->keysz);
  memcpy (s->key, sd->data.key, sd->keysz);
  if (kind == SDK_DATA)
  {
    s->value = dds_realloc (s->value, sd->valuesz);
    memcpy (s->value, sd->data.value, sd->valuesz);
  }
  return true;
}

static bool sd_to_sample (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct sd *sd = (const struct sd *) serdata_common;
  return sdx_to_sample (&sd->x, sd->c.kind, sample, bufptr, buflim);
}

static bool sdx_untyped_to_sample (const struct sdx *sd, struct sampletype *s, void **bufptr, void *buflim)
{
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  s->key = dds_realloc (s->key, sd->keysz);
  memcpy (s->key, sd->data.key, sd->keysz);
  return true;
}

static bool sd_untyped_to_sample (const struct ddsi_sertype *topic, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  (void) topic;
  const struct sd *sd = (const struct sd *) serdata_common;
  return sdx_untyped_to_sample (&sd->x, sample, bufptr, buflim);
  return true;
}

static size_t sdx_print (const struct sdx *sd, enum ddsi_serdata_kind kind, char *buf, size_t size)
{
  int cnt;
  if (kind == SDK_DATA)
    cnt = snprintf (buf, size, "%s -> %s", sd->data.key, sd->data.value);
  else
    cnt = snprintf (buf, size, "%s", sd->data.key);
  return ((size_t) cnt > size) ? size : (size_t) cnt;
}

static size_t sd_print (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  (void) sertype_common;
  const struct sd *sd = (const struct sd *) serdata_common;
  return sdx_print (&sd->x, sd->c.kind, buf, size);
}

static void sdx_get_keyhash (const struct sdx *sd, struct ddsi_keyhash *buf, bool force_md5)
{
  (void) force_md5; // unbounded string is always MD5
  const uint32_t keysz_be = ddsrt_toBE4u (sd->keysz);
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &keysz_be, sizeof (keysz_be));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) sd->data.key, sd->keysz);
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) (buf->value));
}

static void sd_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5)
{
  struct sd const * const sd = (const struct sd *) serdata_common;
  sdx_get_keyhash (&sd->x, buf, force_md5);
  printf ("kh gen %02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x\n",
          buf->value[0], buf->value[1], buf->value[2], buf->value[3],
          buf->value[4], buf->value[5], buf->value[6], buf->value[7],
          buf->value[8], buf->value[9], buf->value[10], buf->value[11],
          buf->value[12], buf->value[13], buf->value[14], buf->value[15]);
}

static const struct ddsi_serdata_ops sd_ops = {
  .get_size = sd_get_size,
  .eqkey = sd_eqkey,
  .free = sd_free,
  .from_ser = sd_from_ser,
  .from_ser_iov = sd_from_ser_iov,
  .from_keyhash = sd_from_keyhash,
  .from_sample = sd_from_sample,
  .to_ser = sd_to_ser,
  .to_sample = sd_to_sample,
  .to_ser_ref = sd_to_ser_ref,
  .to_ser_unref = sd_to_ser_unref,
  .to_untyped = sd_to_untyped,
  .untyped_to_sample = sd_untyped_to_sample,
  .print = sd_print,
  .get_keyhash = sd_get_keyhash
};

/*----------------------------------------------------------------
 *
 * tests parameterized with sertype-based implementation
 *
 *----------------------------------------------------------------*/

struct tw {
  dds_entity_t tp;
  void *st;
};

struct ops {
  struct tw (*make_topic) (dds_entity_t pp, const char *topicname, const char *typename, const dds_qos_t *qos);
  void * (*make_sample) (const struct tw *tw, const struct sampletype *xs);
  const struct sdx * (*get_sdx) (const struct ddsi_serdata *sd);
};

static void cdr_basic (struct ops const * const ops)
{
  dds_return_t rc;
  char topicname[100];

  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
#ifdef DDS_HAS_SHM
  const char* config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>";
#else
  const char* config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>";
#endif
  char *conf_pub = ddsrt_expand_envvars (config, 0);
  char *conf_sub = ddsrt_expand_envvars (config, 1);
  const dds_entity_t pub_dom = dds_create_domain (0, conf_pub);
  CU_ASSERT_FATAL (pub_dom > 0);
  const dds_entity_t sub_dom = dds_create_domain (1, conf_sub);
  CU_ASSERT_FATAL (sub_dom > 0);
  ddsrt_free (conf_pub);
  ddsrt_free (conf_sub);

  const dds_entity_t pub_pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pub_pp > 0);
  const dds_entity_t sub_pp = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL (sub_pp > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, 0);

  create_unique_topic_name ("ddsc_cdr_sertype_basic", topicname, sizeof topicname);
  struct tw pub_tw = ops->make_topic (pub_pp, topicname, "x", qos);
  struct tw sub_tw = ops->make_topic (sub_pp, topicname, "x", qos);

  dds_delete_qos (qos);

  const dds_entity_t wr = dds_create_writer (pub_pp, pub_tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  const dds_entity_t rd = dds_create_reader (sub_pp, sub_tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  // wait for writer to match reader; it is safe to write data once that happened
  dds_publication_matched_status_t pm;
  while ((rc = dds_get_publication_matched_status (wr, &pm)) == 0 && pm.current_count != 1)
    dds_sleepfor (DDS_MSECS (10));
  CU_ASSERT_FATAL (rc == 0);

  // regular write (from_sample(DATA) + to_topicless)
  struct sampletype xs[] = {
    { .key = "aap", .value = "banaan" },
    { .key = "kolibrie", .value = "nectar" }
  };
  for (int j = 0; j < 2; j++)
  {
    for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
    {
      rc = dds_write (wr, &xs[i]);
      CU_ASSERT_FATAL (rc == 0);
    }
  }

  // once acknowledged, it is available for reading - FIXME: still only true for synchronous delivery mode
  rc = dds_wait_for_acks (wr, DDS_SECS (5));
  CU_ASSERT_FATAL (rc == 0);

  // read them back (to_sample(DATA))
  // note: order of instances is not guaranteed, hence the "expected" mask
  {
    struct sampletype s = { .key = NULL, .value = NULL };
    void *raw = &s;
    dds_sample_info_t si;
    uint32_t seen = 0;
    for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
    {
      rc = dds_read_mask (rd, &raw, &si, 1, 1, DDS_NOT_READ_SAMPLE_STATE);
      CU_ASSERT_FATAL (rc == 1);
      CU_ASSERT_FATAL (si.valid_data);
      size_t j;
      for (j = 0; j < sizeof (xs) / sizeof (xs[0]); j++)
      {
        DDSRT_STATIC_ASSERT(sizeof (xs) / sizeof (xs[0]) < 32);
        if (seen & ((uint32_t)1 << j))
          continue;
        if (strcmp (s.key, xs[j].key) == 0)
          break;
      }
      CU_ASSERT_FATAL (j < sizeof (xs) / sizeof (xs[0]));
      CU_ASSERT_STRING_EQUAL_FATAL (s.value, xs[j].value);
      seen |= (uint32_t)1 << j;
    }
    CU_ASSERT_FATAL (seen == ((uint32_t)1 << (sizeof (xs) / sizeof (xs[0]))) - 1);
    rc = dds_read_mask (rd, &raw, &si, 1, 1, DDS_NOT_READ_SAMPLE_STATE);
    CU_ASSERT_FATAL (rc == 0);
    dds_free (s.key);
    dds_free (s.value);
  }

  // read them back while letting the memory for samples be allocated
  // note: order of instances is not guaranteed
  {
    void *raw[sizeof (xs) / sizeof (xs[0])] = { NULL };
    dds_sample_info_t si[sizeof (xs) / sizeof (xs[0])];
    rc = dds_read_mask (rd, raw, si, sizeof (xs) / sizeof (xs[0]), sizeof (xs) / sizeof (xs[0]), DDS_ANY_SAMPLE_STATE);
    CU_ASSERT_FATAL (rc == (int32_t) sizeof (xs) / sizeof (xs[0]));
    uint32_t seen = 0;
    for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
    {
      struct sampletype *s = raw[i];
      CU_ASSERT_FATAL (si[i].valid_data);
      CU_ASSERT_FATAL (si[i].sample_state == DDS_READ_SAMPLE_STATE);
      size_t j;
      for (j = 0; j < sizeof (xs) / sizeof (xs[0]); j++)
      {
        DDSRT_STATIC_ASSERT(sizeof (xs) / sizeof (xs[0]) < 32);
        if (seen & ((uint32_t)1 << j))
          continue;
        if (strcmp (s->key, xs[j].key) == 0)
          break;
      }
      CU_ASSERT_FATAL (j < sizeof (xs) / sizeof (xs[0]));
      CU_ASSERT_STRING_EQUAL_FATAL (s->value, xs[j].value);
      seen |= (uint32_t)1 << j;
    }
    CU_ASSERT_FATAL (seen == ((uint32_t)1 << (sizeof (xs) / sizeof (xs[0]))) - 1);
    rc = dds_return_loan (rd, raw, rc);
    CU_ASSERT_FATAL (rc == 0);
  }

  // unregister (from_sample(KEY))
  rc = dds_unregister_instance (wr, &xs[0]);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_wait_for_acks (wr, DDS_SECS (5));
  CU_ASSERT_FATAL (rc == 0);

  // read the resulting invalid sample (topicless_to_sample)
  {
    struct sampletype s = { .key = NULL, .value = NULL };
    void *raw = &s;
    dds_sample_info_t si;
    rc = dds_read_mask (rd, &raw, &si, 1, 1, DDS_NOT_READ_SAMPLE_STATE);
    CU_ASSERT_FATAL (rc == 1);
    CU_ASSERT_FATAL (!si.valid_data);
    CU_ASSERT_STRING_EQUAL_FATAL (s.key, xs[0].key);
    dds_free (s.key);
  }

  // take the serdata's out (+1 for the invalid sample)
  {
    struct ddsi_serdata *serdata[sizeof (xs) / sizeof (xs[0]) + 1];
    dds_sample_info_t si[sizeof (xs) / sizeof (xs[0]) + 1];
    rc = dds_takecdr (rd, serdata, sizeof (xs) / sizeof (xs[0]) + 1, si, DDS_ANY_STATE);
    CU_ASSERT_FATAL (rc == (int32_t) sizeof (xs) / sizeof (xs[0]) + 1);
    uint32_t seen = 0;
    for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]) + 1; i++)
    {
      if (!si[i].valid_data)
      {
        ddsi_serdata_unref (serdata[i]);
        continue;
      }

      // we know them to be of our own implementation!
      const struct sdx *sd = ops->get_sdx (serdata[i]);
      CU_ASSERT_FATAL (si[i].valid_data);
      size_t j;
      for (j = 0; j < sizeof (xs) / sizeof (xs[0]); j++)
      {
        DDSRT_STATIC_ASSERT(sizeof (xs) / sizeof (xs[0]) < 32);
        if (seen & ((uint32_t)1 << j))
          continue;
        if (strcmp (sd->data.key, xs[j].key) == 0)
          break;
      }
      CU_ASSERT_FATAL (j < sizeof (xs) / sizeof (xs[0]));
      CU_ASSERT_STRING_EQUAL_FATAL (sd->data.value, xs[j].value);
      seen |= (uint32_t)1 << j;

      // FIXME: dds_writecdr overwrites the timestamp (it shouldn't, but we'll survive)
      // dds_writecdr takes ownership of serdata
      rc = dds_writecdr (wr, serdata[i]);
      CU_ASSERT_FATAL (rc == 0);
    }
    CU_ASSERT_FATAL (seen == ((uint32_t)1 << (sizeof (xs) / sizeof (xs[0]))) - 1);
    rc = dds_wait_for_acks (wr, DDS_SECS (5));
    CU_ASSERT_FATAL (rc == 0);
  }

  // read them back again the regular way, rewriting caused the invalid sample to disappear
  {
    void *raw[sizeof (xs) / sizeof (xs[0])] = { NULL };
    dds_sample_info_t si[sizeof (xs) / sizeof (xs[0])];
    rc = dds_read_mask (rd, raw, si, sizeof (xs) / sizeof (xs[0]), sizeof (xs) / sizeof (xs[0]), DDS_ANY_SAMPLE_STATE);
    CU_ASSERT_FATAL (rc == (int32_t) sizeof (xs) / sizeof (xs[0]));
    uint32_t seen = 0;
    for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
    {
      struct sampletype *s = raw[i];
      CU_ASSERT_FATAL (si[i].valid_data);
      size_t j;
      for (j = 0; j < sizeof (xs) / sizeof (xs[0]); j++)
      {
        DDSRT_STATIC_ASSERT(sizeof (xs) / sizeof (xs[0]) < 32);
        if (seen & ((uint32_t)1 << j))
          continue;
        if (strcmp (s->key, xs[j].key) == 0)
          break;
      }
      CU_ASSERT_FATAL (j < sizeof (xs) / sizeof (xs[0]));
      CU_ASSERT_STRING_EQUAL_FATAL (s->value, xs[j].value);
      seen |= (uint32_t)1 << j;
    }
    CU_ASSERT_FATAL (seen == ((uint32_t)1 << (sizeof (xs) / sizeof (xs[0]))) - 1);
    rc = dds_return_loan (rd, raw, rc);
    CU_ASSERT_FATAL (rc == 0);
  }

  rc = dds_delete (sub_dom);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (pub_dom);
  CU_ASSERT_FATAL (rc == 0);
}

static void cdr_forward (struct ops const * const ops)
{
  dds_return_t rc;
  char topicname[100];

#ifdef DDS_HAS_SHM
  const char* config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>";
#else
  const char* config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}";
#endif
  char* conf = ddsrt_expand_envvars(config, 0);
  const dds_entity_t dom = dds_create_domain(0, conf);
  ddsrt_free (conf);

  const dds_entity_t pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);

  create_unique_topic_name ("ddsc_cdr_sertype_basic", topicname, sizeof topicname);
  struct tw tw = ops->make_topic (pp, topicname, "x", NULL);

  const dds_entity_t wr = dds_create_writer (pp, tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  const dds_entity_t rd = dds_create_reader (pp, tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  // write & writedispose
  struct sampletype xs = { .key = "aap", .value = "banaan" };
  rc = dds_write (wr, &xs);
  CU_ASSERT_FATAL (rc == 0);

  // take the serdata's out (+1 for the invalid sample)
  struct ddsi_serdata *serdata;
  dds_sample_info_t si0, si1;

  rc = dds_takecdr (rd, &serdata, 1, &si0, DDS_ANY_STATE);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (si0.valid_data);
  CU_ASSERT_FATAL (si0.instance_state == DDS_ALIVE_INSTANCE_STATE);

  rc = dds_forwardcdr (wr, serdata);
  CU_ASSERT_FATAL (rc == 0);

  rc = dds_takecdr (rd, &serdata, 1, &si1, DDS_ANY_STATE);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (si1.valid_data == si0.valid_data);
  CU_ASSERT_FATAL (si1.instance_state == si0.instance_state);
  CU_ASSERT_FATAL (si1.source_timestamp == si0.source_timestamp);
  ddsi_serdata_unref (serdata);

  rc = dds_writedispose (wr, &xs);
  CU_ASSERT_FATAL (rc == 0);

  rc = dds_takecdr (rd, &serdata, 1, &si0, DDS_ANY_STATE);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (si0.valid_data);
  CU_ASSERT_FATAL (si0.instance_state == DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE);

  rc = dds_forwardcdr (wr, serdata);
  CU_ASSERT_FATAL (rc == 0);

  rc = dds_takecdr (rd, &serdata, 1, &si1, DDS_ANY_STATE);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (si1.valid_data == si0.valid_data);
  CU_ASSERT_FATAL (si1.instance_state == si0.instance_state);
  CU_ASSERT_FATAL (si1.source_timestamp == si0.source_timestamp);
  ddsi_serdata_unref (serdata);

  rc = dds_delete (dom);
  CU_ASSERT_FATAL (rc == 0);
}

static void cdr_invalid_data (struct ops const * const ops)
{
  dds_return_t rc;
  char topicname[100];

  const dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);

  create_unique_topic_name ("ddsc_cdr_sertype_invalid_data", topicname, sizeof topicname);
  struct tw tw = ops->make_topic (pp, topicname, "x", NULL);

  const dds_entity_t wr = dds_create_writer (pp, tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  rc = dds_write (wr, &((struct sampletype){ .key = NULL, .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_write (wr, &((struct sampletype){ .key = "x", .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_write (wr, &((struct sampletype){ .key = NULL, .value = "x" }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

  rc = dds_writedispose (wr, &((struct sampletype){ .key = NULL, .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_writedispose (wr, &((struct sampletype){ .key = "x", .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_writedispose (wr, &((struct sampletype){ .key = NULL, .value = "x" }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

  rc = dds_dispose (wr, &((struct sampletype){ .key = NULL, .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_dispose (wr, &((struct sampletype){ .key = "x", .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_dispose (wr, &((struct sampletype){ .key = NULL, .value = "x" }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

  dds_instance_handle_t ih;
  rc = dds_register_instance (wr, &ih, &((struct sampletype){ .key = NULL, .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_register_instance (wr, &ih, &((struct sampletype){ .key = "x", .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ih != 0);
  rc = dds_register_instance (wr, &ih, &((struct sampletype){ .key = NULL, .value = "x" }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

  rc = dds_unregister_instance (wr, &((struct sampletype){ .key = NULL, .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_unregister_instance (wr, &((struct sampletype){ .key = "x", .value = NULL }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_unregister_instance (wr, &((struct sampletype){ .key = NULL, .value = "x" }));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

  rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
}

static void cdr_timeout (struct ops const * const ops)
{
  dds_return_t rc;
  char topicname[100];

  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  const char *config = "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"
#ifdef DDS_HAS_SHM
"<Domain id=\"any\">\
  <SharedMemory>\
    <Enable>false</Enable>\
  </SharedMemory>\
</Domain>"
#endif
"<Internal>\
  <Watermarks>\
    <WhcHigh>0B</WhcHigh>\
    <WhcHighInit>0B</WhcHighInit>\
    <WhcLow>0B</WhcLow>\
    <WhcAdaptive>false</WhcAdaptive>\
  </Watermarks>\
  <WriterLingerDuration>0s</WriterLingerDuration>\
</Internal>";
  char *conf_pub = ddsrt_expand_envvars (config, 0);
  char *conf_sub = ddsrt_expand_envvars (config, 1);
  const dds_entity_t pub_dom = dds_create_domain (0, conf_pub);
  CU_ASSERT_FATAL (pub_dom > 0);
  const dds_entity_t sub_dom = dds_create_domain (1, conf_sub);
  CU_ASSERT_FATAL (sub_dom > 0);
  ddsrt_free (conf_pub);
  ddsrt_free (conf_sub);

  const dds_entity_t pub_pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pub_pp > 0);
  const dds_entity_t sub_pp = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL (sub_pp > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, 0);

  create_unique_topic_name ("ddsc_cdr_sertype_basic", topicname, sizeof topicname);
  struct tw pub_tw = ops->make_topic (pub_pp, topicname, "x", qos);
  struct tw sub_tw = ops->make_topic (sub_pp, topicname, "x", qos);

  dds_delete_qos (qos);

  const dds_entity_t wr = dds_create_writer (pub_pp, pub_tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  const dds_entity_t rd = dds_create_reader (sub_pp, sub_tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  // wait for writer to match reader; it is safe to write data once that happened
  dds_publication_matched_status_t pm;
  while ((rc = dds_get_publication_matched_status (wr, &pm)) == 0 && pm.current_count != 1)
    dds_sleepfor (DDS_MSECS (10));
  CU_ASSERT_FATAL (rc == 0);

  // make writer mute: no data & heartbeats -> no ACKs, so first sample remains
  // unacknowledged and second sample bumps into WHC limits, forcing an error path in
  // writecdr_impl
  dds_domain_set_deafmute (wr, false, true, DDS_INFINITY);

  struct sampletype xs[] = {
    { .key = "krab", .value = "hemocyanine" },
    { .key = "boom", .value = "chlorofyl" }
  };
  for (size_t i = 0; i < sizeof (xs) / sizeof (xs[0]); i++)
  {
    void *sd = ops->make_sample (&pub_tw, &xs[i]);
    CU_ASSERT_FATAL (sd != NULL);
    rc = dds_writecdr (wr, sd);
    if (i == 0) {
      CU_ASSERT_FATAL (rc == 0);
    } else {
      CU_ASSERT_FATAL (rc == DDS_RETCODE_TIMEOUT);
    }
  }

  dds_delete (DDS_CYCLONEDDS_HANDLE);
}

/*----------------------------------------------------------------
 *
 * test wrappers
 *
 *----------------------------------------------------------------*/

static struct ddsi_sertype *make_sertype (const char *typename)
{
  struct stp *stp = malloc (sizeof (*stp));
  assert(stp);
  ddsi_sertype_init_flags (&stp->c, typename, &stp_ops, &sd_ops, DDSI_SERTYPE_FLAG_REQUEST_KEYHASH);
  return &stp->c;
}

static struct tw make_topic (dds_entity_t pp, const char *topicname, const char *typename, const dds_qos_t *qos)
{
  struct ddsi_sertype *st = make_sertype (typename);
  dds_entity_t tp = dds_create_topic_sertype (pp, topicname, &st, qos, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);
  return ((struct tw) { .tp = tp, .st = st });
}

static void *make_sample (const struct tw *tw, const struct sampletype *s)
{
  return sd_from_sample (tw->st, SDK_DATA, s);
}

static const struct sdx *get_sdx (const struct ddsi_serdata *serdata)
{
  const struct sd *sd = (const struct sd *) serdata;
  return &sd->x;
}

static const struct ops gops = {
  .make_topic = make_topic,
  .make_sample = make_sample,
  .get_sdx = get_sdx
};

CU_Test(ddsc_cdr, basic)
{
  cdr_basic (&gops);
}

CU_Test(ddsc_cdr, forward)
{
  cdr_forward (&gops);
}

CU_Test(ddsc_cdr, invalid_data)
{
  cdr_invalid_data (&gops);
}

CU_Test(ddsc_cdr, timeout)
{
  cdr_timeout (&gops);
}

CU_Test(ddsc_cdr, forward_conv_serdata)
{
  dds_return_t rc;

  const dds_entity_t pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);

  char topicname[100];
  create_unique_topic_name ("ddsc_cdr_forward_conv_serdata", topicname, sizeof topicname);
  struct tw tw = make_topic (pp, topicname, "x", NULL);
  struct ddsi_sertype *st1 = make_sertype ("x");

  const dds_entity_t wr = dds_create_writer (pp, tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  // we make a reader so the test is not dependent on the writer trying to
  // convert the serdata representations even in the absence of a reader
  const dds_entity_t rd = dds_create_reader (pp, tw.tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  // construct a serdata of type 1, then publish it with the writer of type 0
  struct sampletype xs = { .key = "aap", .value = "banaan" };
  struct ddsi_serdata *sd1 = sd_from_sample (st1, SDK_DATA, &xs);
  // ... but increment the refcount first so that we can inspect the refcount later
  (void) ddsi_serdata_ref (sd1);
  rc = dds_forwardcdr (wr, sd1);
  CU_ASSERT_FATAL (rc == 0);

  // forwardcdr drops the refcount, but we held on to one
  CU_ASSERT_FATAL (ddsrt_atomic_ld32 (&sd1->refc) == 1);
  ddsi_serdata_unref (sd1);

  // given that we have a reader, we can take out the sample
  // and check that it, too, has a refcount of 1
  struct ddsi_serdata *sd0;
  dds_sample_info_t si;
  rc = dds_takecdr (rd, &sd0, 1, &si, DDS_ANY_STATE);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (si.valid_data);
  CU_ASSERT_FATAL (si.instance_state == DDS_ALIVE_INSTANCE_STATE);
  CU_ASSERT_FATAL (ddsrt_atomic_ld32 (&sd0->refc) == 1);
  CU_ASSERT_FATAL (sd0->type == tw.st);

  struct sampletype ys = { 0 };
  bool ok = ddsi_serdata_to_sample (sd0, &ys, NULL, NULL);
  CU_ASSERT_FATAL (ok);
  CU_ASSERT_FATAL (strcmp (ys.key, xs.key) == 0);
  CU_ASSERT_FATAL (strcmp (ys.value, xs.value) == 0);
  ddsi_sertype_free_sample (tw.st, &ys, DDS_FREE_CONTENTS);
  ddsi_serdata_unref (sd0);

  // the weird sertype "st1" doesn't get tracked by Cyclone because we never
  // used it in a topic, so we must release it ourselves
  ddsi_sertype_unref (st1);
  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
}
