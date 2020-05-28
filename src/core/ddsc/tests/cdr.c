/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include <limits.h>

#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/dds.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/q_radmin.h"
#include "dds__entity.h"

#include "test_common.h"

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

static void stp_zero_samples (const struct ddsi_sertype *dcmn, void *samples, size_t count)
{
  (void) dcmn;
  memset (samples, 0, count * sizeof (struct sampletype));
}

static void stp_realloc_samples (void **ptrs, const struct ddsi_sertype *dcmn, void *old, size_t oldcount, size_t count)
{
  (void) dcmn;
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

static void stp_free_samples (const struct ddsi_sertype *dcmn, void **ptrs, size_t count, dds_free_op_t op)
{
  (void) dcmn;
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

static const struct ddsi_sertype_ops stp_ops = {
  .free = stp_free,
  .zero_samples = stp_zero_samples,
  .realloc_samples = stp_realloc_samples,
  .free_samples = stp_free_samples,
  .equal = stp_equal,
  .hash = stp_hash
};

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define NATIVE_ENCODING CDR_LE
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
#define NATIVE_ENCODING CDR_BE
#else
#error "DDSRT_ENDIAN neither LITTLE nor BIG"
#endif

struct sd {
  struct ddsi_serdata c;
  struct sampletype data;
  uint32_t keysz, pad0, valuesz, pad1;
};

static uint32_t sd_get_size (const struct ddsi_serdata *dcmn)
{
  // FIXME: 4 is DDSI CDR header size and shouldn't be included here; same is true for pad1
  const struct sd *d = (const struct sd *) dcmn;
  return 4 + 4 + d->keysz + d->pad0 + (dcmn->kind == SDK_DATA ? 4 + d->valuesz + d->pad1 : 0);
}

static bool sd_eqkey (const struct ddsi_serdata *acmn, const struct ddsi_serdata *bcmn)
{
  const struct sd *a = (const struct sd *) acmn;
  const struct sd *b = (const struct sd *) bcmn;
  return a->keysz == b->keysz && memcmp (a->data.key, b->data.key, a->keysz) == 0;
}

static void sd_free (struct ddsi_serdata *dcmn)
{
  struct sd *d = (struct sd *) dcmn;
  free (d->data.key);
  free (d->data.value);
  free (d);
}

static char *strdup_with_len (const char *x, size_t l)
{
  char *y = malloc (l);
  memcpy (y, x, l);
  return y;
}

static struct ddsi_serdata *sd_from_ser_iov (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size)
{
  struct stp const * const stp = (const struct stp *) tpcmn;
  struct sd *sd = malloc (sizeof (*sd));
  ddsi_serdata_init (&sd->c, &stp->c, kind);
  (void) size;
  // assuming not fragmented, CDR Header specifies native encoding, input is valid and all that
  // CDR encoding of strings: uint32_t length-including-terminating-0 ; blob-including-terminating-0
  assert (niov == 1);
  (void) niov;
  size_t off = 0;
  const char *base = (const char *) iov[0].iov_base + 4;
  memcpy (&sd->keysz, base + off, sizeof (uint32_t));
  off += sizeof (uint32_t);
  sd->data.key = strdup_with_len (base + off, sd->keysz);
  off += sd->keysz;
  sd->pad0 = (uint32_t) (((off % 4) == 0) ? 0 : 4 - (off % 4));
  off += sd->pad0;
  if (kind == SDK_DATA)
  {
    memcpy (&sd->valuesz, base + off, sizeof (uint32_t));
    off += sizeof (uint32_t);
    sd->data.value = strdup_with_len (base + off, sd->valuesz);
    off += sd->valuesz;
    // FIXME: not sure if this is still needed, it shouldn't be, but ...
    sd->pad1 = (uint32_t) (((off % 4) == 0) ? 0 : 4 - (off % 4));
  }
  else
  {
    sd->data.value = NULL;
    sd->valuesz = sd->pad1 = 0;
  }
  sd->c.hash = ddsrt_mh3 (sd->data.key, sd->keysz, 0) ^ tpcmn->serdata_basehash;
  return &sd->c;
}

static struct ddsi_serdata *sd_from_ser (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size)
{
  assert (fragchain->nextfrag == NULL);
  ddsrt_iovec_t iov = {
    .iov_base = NN_RMSG_PAYLOADOFF (fragchain->rmsg, NN_RDATA_PAYLOAD_OFF (fragchain)),
    .iov_len = fragchain->maxp1 // fragchain->min = 0 for first fragment, by definition
  };
  return sd_from_ser_iov (tpcmn, kind, 1, &iov, size);
}

static struct ddsi_serdata *sd_from_keyhash (const struct ddsi_sertype *tpcmn, const ddsi_keyhash_t *keyhash)
{
  // unbounded string, therefore keyhash is MD5 and we can't extract the value
  // (don't try disposing/unregistering in RTI and expect this to accept the data)
  (void) tpcmn; (void) keyhash;
  return NULL;
}

static struct ddsi_serdata *sd_from_sample (const struct ddsi_sertype *tpcmn, enum ddsi_serdata_kind kind, const void *sample)
{
  const struct stp *tp = (const struct stp *) tpcmn;
  const struct sampletype *s = sample;
  if (s->key == NULL || (kind == SDK_DATA && s->value == NULL))
    return NULL;
  struct sd *sd = malloc (sizeof (*sd));
  ddsi_serdata_init (&sd->c, &tp->c, kind);
  sd->keysz = (uint32_t) strlen (s->key) + 1;
  sd->data.key = strdup_with_len (s->key, sd->keysz);
  sd->pad0 = ((sd->keysz % 4) == 0) ? 0 : 4 - (sd->keysz % 4);
  if (kind == SDK_DATA)
  {
    sd->valuesz = (uint32_t) strlen (s->value) + 1;
    sd->data.value = strdup_with_len (s->value, sd->valuesz);
    sd->pad1 = ((sd->valuesz % 4) == 0) ? 0 : 4 - (sd->valuesz % 4);
  }
  else
  {
    sd->data.value = NULL;
    sd->valuesz = sd->pad1 = 0;
  }
  sd->c.hash = ddsrt_mh3 (sd->data.key, sd->keysz, 0) ^ tpcmn->serdata_basehash;
  return &sd->c;
}

static struct ddsi_serdata *sd_to_untyped (const struct ddsi_serdata *serdata_common)
{
  const struct sd *sd = (const struct sd *) serdata_common;
  const struct stp *tp = (const struct stp *) sd->c.type;
  struct sd *sd_tl = malloc (sizeof (*sd_tl));
  ddsi_serdata_init (&sd_tl->c, &tp->c, SDK_KEY);
  sd_tl->c.type = NULL;
  sd_tl->c.hash = sd->c.hash;
  sd_tl->c.timestamp.v = INT64_MIN;
  sd_tl->data.key = strdup_with_len (sd->data.key, sd->keysz);
  sd_tl->data.value = NULL;
  sd_tl->keysz = sd->keysz;
  sd_tl->pad0 = sd->pad0;
  sd_tl->valuesz = sd_tl->pad1 = 0;
  return &sd_tl->c;
}

static struct ddsi_serdata *sd_to_ser_ref (const struct ddsi_serdata *serdata_common, size_t cdr_off, size_t cdr_sz, ddsrt_iovec_t *ref)
{
  // The idea is that if the CDR is available already, one can return a reference (in which case
  // one probably should also increment the sample's refcount).  But here we generaete the CDR
  // on the fly
  const struct sd *sd = (const struct sd *) serdata_common;
  // fragmented data results in calls with different offsets and limits, but can't be bothered for this test
  assert (cdr_off == 0 && cdr_sz == sd_get_size (&sd->c));
  (void) cdr_off;
  (void) cdr_sz;
  ref->iov_len = sd_get_size (&sd->c);
  ref->iov_base = malloc (ref->iov_len);
  char * const header = ref->iov_base;
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
  if (sd->c.kind == SDK_DATA)
  {
    memcpy (base + off, &sd->valuesz, sizeof (uint32_t));
    off += sizeof (uint32_t);
    memcpy (base + off, sd->data.value, sd->valuesz);
    off += sd->valuesz;
    for (uint32_t i = 0; i < sd->pad1; i++)
      base[off++] = 0;
  }
  assert (4 + off == ref->iov_len);
  return (struct ddsi_serdata *) &sd->c;
}

static void sd_to_ser_unref (struct ddsi_serdata *serdata_common, const ddsrt_iovec_t *ref)
{
  // const "ref" is a mistake in the interface ... it is owned by this code ...
  (void) serdata_common;
  free ((void *) ref->iov_base);
}

static void sd_to_ser (const struct ddsi_serdata *serdata_common, size_t off, size_t sz, void *buf)
{
  // being lazy and reusing code in sd_to_ser_ref, even though here we don't have to allocate anything
  ddsrt_iovec_t iov;
  (void) sd_to_ser_ref (serdata_common, off, sz, &iov);
  memcpy (buf, iov.iov_base, iov.iov_len);
  sd_to_ser_unref ((struct ddsi_serdata *) serdata_common, &iov);
}

static bool sd_to_sample (const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  const struct sd *sd = (const struct sd *) serdata_common;
  struct sampletype *s = sample;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  s->key = dds_realloc (s->key, sd->keysz);
  memcpy (s->key, sd->data.key, sd->keysz);
  if (sd->c.kind == SDK_DATA)
  {
    s->value = dds_realloc (s->value, sd->valuesz);
    memcpy (s->value, sd->data.value, sd->valuesz);
  }
  return true;
}

static bool sd_untyped_to_sample (const struct ddsi_sertype *topic, const struct ddsi_serdata *serdata_common, void *sample, void **bufptr, void *buflim)
{
  (void) topic;
  const struct sd *sd = (const struct sd *) serdata_common;
  struct sampletype *s = sample;
  if (bufptr) abort(); else { (void)buflim; } /* FIXME: haven't implemented that bit yet! */
  s->key = dds_realloc (s->key, sd->keysz);
  memcpy (s->key, sd->data.key, sd->keysz);
  return true;
}

static size_t sd_print (const struct ddsi_sertype *sertype_common, const struct ddsi_serdata *serdata_common, char *buf, size_t size)
{
  (void) sertype_common;
  const struct sd *sd = (const struct sd *) serdata_common;
  int cnt;
  if (sd->c.kind == SDK_DATA)
    cnt = snprintf (buf, size, "%s -> %s", sd->data.key, sd->data.value);
  else
    cnt = snprintf (buf, size, "%s", sd->data.key);
  return ((size_t) cnt > size) ? size : (size_t) cnt;
}

static void sd_get_keyhash (const struct ddsi_serdata *serdata_common, struct ddsi_keyhash *buf, bool force_md5)
{
  (void) force_md5; // unbounded string is always MD5
  struct sd const * const sd = (const struct sd *) serdata_common;
  const uint32_t keysz_be = ddsrt_toBE4u (sd->keysz);
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &keysz_be, sizeof (keysz_be));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) sd->data.key, sd->keysz);
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) (buf->value));
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

static struct ddsi_sertype *make_sertype (dds_entity_t pp, const char *typename)
{
  struct stp *stp = malloc (sizeof (*stp));
  struct ddsi_domaingv *gv;

  {
    dds_return_t rc;
    struct dds_entity *x;
    rc = dds_entity_pin (pp, &x);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    gv = &x->m_domain->gv;
    dds_entity_unpin (x);
  }

  ddsi_sertype_init (gv, &stp->c, typename, &stp_ops, &sd_ops, false);
  return &stp->c;
}

CU_Test(ddsc_cdr, basic)
{
  dds_return_t rc;
  char topicname[100];

  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  const char *config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>";
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

  create_unique_topic_name ("ddsc_cdr_basic", topicname, sizeof topicname);
  struct ddsi_sertype *pub_st = make_sertype (pub_pp, "x");
  const dds_entity_t pub_tp = dds_create_topic_generic (pub_pp, topicname, &pub_st, qos, NULL, NULL);
  CU_ASSERT_FATAL (pub_tp > 0);

  struct ddsi_sertype *sub_st = make_sertype (sub_pp, "x");
  const dds_entity_t sub_tp = dds_create_topic_generic (sub_pp, topicname, &sub_st, qos, NULL, NULL);
  CU_ASSERT_FATAL (sub_tp > 0);

  dds_delete_qos (qos);

  const dds_entity_t wr = dds_create_writer (pub_pp, pub_tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  const dds_entity_t rd = dds_create_reader (sub_pp, sub_tp, NULL, NULL);
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

  // read the resulting invalid sample (untyped_to_sample)
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
      struct sd *sd = (struct sd *) serdata[i];
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

CU_Test(ddsc_cdr, forward)
{
  dds_return_t rc;
  char topicname[100];

  const dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);

  create_unique_topic_name ("ddsc_cdr_basic", topicname, sizeof topicname);
  struct ddsi_sertype *st = make_sertype (pp, "x");
  const dds_entity_t tp = dds_create_topic_generic (pp, topicname, &st, NULL, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);

  const dds_entity_t wr = dds_create_writer (pp, tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  const dds_entity_t rd = dds_create_reader (pp, tp, NULL, NULL);
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

  rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_cdr, invalid_data)
{
  dds_return_t rc;
  char topicname[100];

  const dds_entity_t pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);

  create_unique_topic_name ("ddsc_cdr_invalid_data", topicname, sizeof topicname);
  struct ddsi_sertype *st = make_sertype (pp, "x");
  const dds_entity_t tp = dds_create_topic_generic (pp, topicname, &st, NULL, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);

  const dds_entity_t wr = dds_create_writer (pp, tp, NULL, NULL);
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
}
