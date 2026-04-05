// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"

#include <assert.h>
#include <string.h>

#include "dds/dds.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_init.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_thread.h"
#include "ddsi__addrset.h"
#include "ddsi__entity.h"
#include "ddsi__misc.h"
#include "ddsi__protocol.h"
#include "ddsi__transmit.h"
#include "ddsi__vendor.h"
#include "ddsi__xmsg.h"
#include "mem_ser.h"

static struct ddsi_domaingv gv;
static struct ddsi_thread_state *thrst;
static struct test_sertype *test_st;

static struct test_sertype *test_sertype_new (void);
static struct test_sertype *test_sertype_new_with_serdata_ops (const struct ddsi_serdata_ops *serdata_ops);
static struct ddsi_xmsg *build_message_for_sertype (const struct test_sertype *st, bool fragmented, bool include_related_sample_identity, const ddsi_guid_t *related_writer_guid, ddsi_seqno_t related_seq);

struct test_sertype
{
  struct ddsi_sertype c;
};

struct test_serdata
{
  struct ddsi_serdata c;
  uint32_t size;
  bool have_related_sample_identity;
  ddsi_guid_t related_writer_guid;
  ddsi_seqno_t related_seq;
  unsigned char payload[];
};

static void setup (void)
{
  ddsrt_init ();
  ddsi_iid_init ();
  ddsi_thread_states_init ();

  thrst = ddsi_lookup_thread_state ();
  assert (thrst->state == DDSI_THREAD_STATE_LAZILY_CREATED);
  thrst->state = DDSI_THREAD_STATE_ALIVE;
  ddsrt_atomic_stvoidp (&thrst->gv, &gv);

  memset (&gv, 0, sizeof (gv));
  ddsi_config_init_default (&gv.config);
  gv.config.transport_selector = DDSI_TRANS_NONE;

  ddsi_config_prep (&gv, NULL);
  ddsi_init (&gv, NULL);
  test_st = test_sertype_new ();
}

static void teardown (void)
{
  ddsi_sertype_unref (&test_st->c);
  ddsi_fini (&gv);
  thrst->state = DDSI_THREAD_STATE_LAZILY_CREATED;
  ddsi_thread_states_fini ();
  ddsi_iid_fini ();
  ddsrt_fini ();
}

static void test_sertype_free (struct ddsi_sertype *tp)
{
  struct test_sertype *st = (struct test_sertype *) tp;
  ddsi_sertype_fini (tp);
  ddsrt_free (st);
}

static uint32_t test_sertype_hash (const struct ddsi_sertype *tp)
{
  DDSRT_UNUSED_ARG (tp);
  return 0x13572468u;
}

static bool test_sertype_equal (const struct ddsi_sertype *a, const struct ddsi_sertype *b)
{
  return a == b;
}

static uint32_t test_serdata_get_size (const struct ddsi_serdata *dcmn)
{
  const struct test_serdata *d = (const struct test_serdata *) dcmn;
  return d->size;
}

static void test_serdata_free (struct ddsi_serdata *dcmn)
{
  ddsrt_free (dcmn);
}

static bool test_serdata_eqkey (const struct ddsi_serdata *a, const struct ddsi_serdata *b)
{
  return a == b;
}

static struct ddsi_serdata *test_serdata_to_untyped (const struct ddsi_serdata *d)
{
  return ddsi_serdata_ref (d);
}

static bool test_serdata_get_related_sample_identity (const struct ddsi_serdata *dcmn, ddsi_guid_t *writer_guid, ddsi_seqno_t *seq)
{
  const struct test_serdata *d = (const struct test_serdata *) dcmn;
  if (!d->have_related_sample_identity)
    return false;
  *writer_guid = d->related_writer_guid;
  *seq = d->related_seq;
  return true;
}

static struct ddsi_serdata *test_serdata_to_ser_ref (const struct ddsi_serdata *dcmn, size_t off, size_t sz, ddsrt_iovec_t *ref)
{
  const struct test_serdata *d = (const struct test_serdata *) dcmn;
  struct ddsi_serdata *ret = ddsi_serdata_ref (dcmn);
  unsigned char *buf = ddsrt_malloc (sz);

  memset (buf, 0, sz);
  if (off < d->size)
  {
    size_t copy = d->size - off;
    if (copy > sz)
      copy = sz;
    memcpy (buf, d->payload + off, copy);
  }

  ref->iov_base = buf;
  ref->iov_len = (ddsrt_iov_len_t) sz;
  return ret;
}

static void test_serdata_to_ser_unref (struct ddsi_serdata *dcmn, const ddsrt_iovec_t *ref)
{
  ddsrt_free ((void *) ref->iov_base);
  ddsi_serdata_unref (dcmn);
}

static void test_serdata_to_ser (const struct ddsi_serdata *dcmn, size_t off, size_t sz, void *buf)
{
  ddsrt_iovec_t iov;
  struct ddsi_serdata *ref = test_serdata_to_ser_ref (dcmn, off, sz, &iov);
  memcpy (buf, iov.iov_base, sz);
  test_serdata_to_ser_unref (ref, &iov);
}

static const struct ddsi_serdata_ops test_serdata_ops = {
  .get_size = test_serdata_get_size,
  .eqkey = test_serdata_eqkey,
  .free = test_serdata_free,
  .to_ser = test_serdata_to_ser,
  .to_ser_ref = test_serdata_to_ser_ref,
  .to_ser_unref = test_serdata_to_ser_unref,
  .to_untyped = test_serdata_to_untyped,
  .get_related_sample_identity = test_serdata_get_related_sample_identity
};

static const struct ddsi_serdata_ops test_serdata_ops_without_related_sample_identity = {
  .get_size = test_serdata_get_size,
  .eqkey = test_serdata_eqkey,
  .free = test_serdata_free,
  .to_ser = test_serdata_to_ser,
  .to_ser_ref = test_serdata_to_ser_ref,
  .to_ser_unref = test_serdata_to_ser_unref,
  .to_untyped = test_serdata_to_untyped
};

static const struct ddsi_sertype_ops test_sertype_ops = {
  .version = ddsi_sertype_v0,
  .free = test_sertype_free,
  .hash = test_sertype_hash,
  .equal = test_sertype_equal
};

static struct test_sertype *test_sertype_new_with_serdata_ops (const struct ddsi_serdata_ops *serdata_ops)
{
  struct test_sertype *st = ddsrt_malloc (sizeof (*st));
  ddsi_sertype_init_props (
    &st->c, "related_sample_identity_test", &test_sertype_ops, serdata_ops, 1u,
    DDS_DATA_TYPE_IS_MEMCPY_SAFE, DDS_DATA_REPRESENTATION_XCDR1, 0);
  return st;
}

static struct test_sertype *test_sertype_new (void)
{
  return test_sertype_new_with_serdata_ops (&test_serdata_ops);
}

static struct test_serdata *test_serdata_new (const struct test_sertype *st, uint32_t payload_size)
{
  struct test_serdata *sd = ddsrt_malloc (sizeof (*sd) + payload_size);
  const ddsrt_wctime_t now = ddsrt_time_wallclock ();
  const ddsrt_mtime_t mnow = ddsrt_time_monotonic ();
  ddsi_serdata_init (&sd->c, &st->c, SDK_DATA);
  sd->c.hash = st->c.serdata_basehash;
  sd->c.timestamp = now;
  sd->c.twrite = mnow;
  sd->size = payload_size;
  sd->have_related_sample_identity = false;
  memset (&sd->related_writer_guid, 0, sizeof (sd->related_writer_guid));
  sd->related_seq = 0;
  sd->payload[0] = 0;
  sd->payload[1] = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 1 : 0;
  sd->payload[2] = 0;
  sd->payload[3] = 0;
  for (uint32_t i = 4; i < payload_size; i++)
    sd->payload[i] = (unsigned char) (0x40u + (i & 0x3fu));
  return sd;
}

static void init_test_writer (struct ddsi_writer *wr, struct dds_qos *xqos, ddsi_guid_t guid)
{
  memset (wr, 0, sizeof (*wr));
  ddsi_entity_common_init (&wr->e, &gv, &guid, DDSI_EK_WRITER, ddsrt_time_wallclock (), DDSI_VENDORID_ECLIPSE, true);
  wr->as = ddsi_new_addrset ();
  wr->xqos = xqos;
  wr->reliable = 0;
}

static void fini_test_writer (struct ddsi_writer *wr)
{
  ddsi_unref_addrset (wr->as);
  ddsi_entity_common_fini (&wr->e);
}

static struct ddsi_xmsg *build_message (bool fragmented, bool include_related_sample_identity, const ddsi_guid_t *related_writer_guid, ddsi_seqno_t related_seq)
{
  return build_message_for_sertype (test_st, fragmented, include_related_sample_identity, related_writer_guid, related_seq);
}

static struct ddsi_xmsg *build_message_for_sertype (const struct test_sertype *st, bool fragmented, bool include_related_sample_identity, const ddsi_guid_t *related_writer_guid, ddsi_seqno_t related_seq)
{
  static const ddsi_guid_t writer_guid = {
    .prefix = { .s = { 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b } },
    .entityid = { .u = 0x000003c2u }
  };

  const uint32_t payload_size = fragmented ? (uint32_t) gv.config.fragment_size + 8u : 12u;
  struct test_serdata *sd = test_serdata_new (st, payload_size);
  struct dds_qos *xqos = dds_create_qos ();
  struct ddsi_writer wr;
  struct ddsi_xmsg *msg = NULL;
  init_test_writer (&wr, xqos, writer_guid);

  if (include_related_sample_identity)
  {
    sd->have_related_sample_identity = true;
    sd->related_writer_guid = *related_writer_guid;
    sd->related_seq = related_seq;
  }

  ddsrt_mutex_lock (&wr.e.lock);
  const dds_return_t rc = ddsi_create_fragment_message (
    &wr, 11, &sd->c, 0, 1, NULL, &msg, 1, UINT32_MAX);
  ddsrt_mutex_unlock (&wr.e.lock);

  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);
  CU_ASSERT_FATAL (msg != NULL);

  fini_test_writer (&wr);
  dds_delete_qos (xqos);
  ddsi_serdata_unref (&sd->c);
  return msg;
}

static const unsigned char *find_data_submessage (const unsigned char *payload, size_t size, uint8_t expected_submessage_id)
{
  size_t off = 0;
  while (off + sizeof (ddsi_rtps_submessage_header_t) <= size)
  {
    const ddsi_rtps_submessage_header_t *hdr = (const ddsi_rtps_submessage_header_t *) (payload + off);
    if (hdr->submessageId == expected_submessage_id)
      return payload + off;
    if (hdr->octetsToNextHeader == 0)
      break;
    off += DDSI_RTPS_SUBMESSAGE_HEADER_SIZE + hdr->octetsToNextHeader;
  }
  return NULL;
}

static const ddsi_parameter_t *find_inline_qos_parameter (const unsigned char *submsg, uint8_t submessage_id, ddsi_parameterid_t pid)
{
  const uint8_t inline_qos_flag = submessage_id == DDSI_RTPS_SMID_DATA
    ? DDSI_DATA_FLAG_INLINE_QOS
    : DDSI_DATAFRAG_FLAG_INLINE_QOS;
  const ddsi_rtps_data_datafrag_common_t *ddcmn = submessage_id == DDSI_RTPS_SMID_DATA
    ? &((const ddsi_rtps_data_t *) submsg)->x
    : &((const ddsi_rtps_datafrag_t *) submsg)->x;
  if ((((const ddsi_rtps_submessage_header_t *) submsg)->flags & inline_qos_flag) == 0)
    return NULL;
  const unsigned char *param = ((const unsigned char *) &ddcmn->octetsToInlineQos) + 2 + ddcmn->octetsToInlineQos;

  while (1)
  {
    const ddsi_parameter_t *phdr = (const ddsi_parameter_t *) param;
    if (phdr->parameterid == DDSI_PID_SENTINEL)
      return NULL;
    if (phdr->parameterid == pid)
      return phdr;
    param += sizeof (*phdr) + phdr->length;
  }
}

static void assert_related_sample_identity_bytes (const ddsi_parameter_t *param, const ddsi_guid_t *guid, ddsi_seqno_t seq)
{
  const ddsi_guid_t expected_guid = ddsi_hton_guid (*guid);
  const ddsi_sequence_number_t expected_seq = ddsi_to_seqno (seq);
  unsigned char expected[24];

  CU_ASSERT_EQ_FATAL (param->length, 24);
  memcpy (expected, &expected_guid, sizeof (expected_guid));
  memcpy (expected + sizeof (expected_guid), &expected_seq, sizeof (expected_seq));
  CU_ASSERT_MEMEQ_FATAL (((const unsigned char *) (param + 1)), sizeof (expected), expected, sizeof (expected));
}

CU_Test (ddsi_related_sample_identity, default_behavior, .init = setup, .fini = teardown)
{
  const ddsi_guid_t related_guid = {
    .prefix = { .s = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 } },
    .entityid = { .u = 0x000004c7u }
  };
  struct ddsi_xmsg *msg = build_message (false, false, &related_guid, 0x1122334455667788ULL);
  size_t size;
  const unsigned char *payload = ddsi_xmsg_payload (&size, msg);
  const unsigned char *submsg = find_data_submessage (payload, size, DDSI_RTPS_SMID_DATA);

  CU_ASSERT_FATAL (submsg != NULL);
  CU_ASSERT_FATAL (find_inline_qos_parameter (submsg, DDSI_RTPS_SMID_DATA, DDSI_PID_RELATED_SAMPLE_IDENTITY) == NULL);
  ddsi_xmsg_free (msg);
}

CU_Test (ddsi_related_sample_identity, absent_getter, .init = setup, .fini = teardown)
{
  const ddsi_guid_t related_guid = {
    .prefix = { .s = { 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb } },
    .entityid = { .u = 0x010203c5u }
  };
  const ddsi_seqno_t related_seq = UINT64_C (0x1112131415161718);
  struct test_sertype *st_without_getter = test_sertype_new_with_serdata_ops (&test_serdata_ops_without_related_sample_identity);
  struct ddsi_xmsg *msg = build_message_for_sertype (st_without_getter, false, true, &related_guid, related_seq);
  size_t size;
  const unsigned char *payload = ddsi_xmsg_payload (&size, msg);
  const unsigned char *submsg = find_data_submessage (payload, size, DDSI_RTPS_SMID_DATA);

  CU_ASSERT_FATAL (submsg != NULL);
  CU_ASSERT_FATAL (find_inline_qos_parameter (submsg, DDSI_RTPS_SMID_DATA, DDSI_PID_RELATED_SAMPLE_IDENTITY) == NULL);

  ddsi_xmsg_free (msg);
  ddsi_sertype_unref (&st_without_getter->c);
}

CU_Test (ddsi_related_sample_identity, emit_when_enabled, .init = setup, .fini = teardown)
{
  const ddsi_guid_t related_guid = {
    .prefix = { .s = { 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xab } },
    .entityid = { .u = 0x010203c4u }
  };
  const ddsi_seqno_t related_seq = UINT64_C (0x1122334455667788);
  struct ddsi_xmsg *msg = build_message (false, true, &related_guid, related_seq);
  size_t size;
  const unsigned char *payload = ddsi_xmsg_payload (&size, msg);
  const unsigned char *submsg = find_data_submessage (payload, size, DDSI_RTPS_SMID_DATA);
  const ddsi_parameter_t *param;

  CU_ASSERT_FATAL (submsg != NULL);
  param = find_inline_qos_parameter (submsg, DDSI_RTPS_SMID_DATA, DDSI_PID_RELATED_SAMPLE_IDENTITY);
  CU_ASSERT_FATAL (param != NULL);
  assert_related_sample_identity_bytes (param, &related_guid, related_seq);
  ddsi_xmsg_free (msg);
}

CU_Test (ddsi_related_sample_identity, fragmented_path, .init = setup, .fini = teardown)
{
  const ddsi_guid_t related_guid = {
    .prefix = { .s = { 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb } },
    .entityid = { .u = 0x0a0b0cc3u }
  };
  const ddsi_seqno_t related_seq = UINT64_C (0x0102030405060708);
  struct ddsi_xmsg *msg = build_message (true, true, &related_guid, related_seq);
  size_t size;
  const unsigned char *payload = ddsi_xmsg_payload (&size, msg);
  const unsigned char *submsg = find_data_submessage (payload, size, DDSI_RTPS_SMID_DATA_FRAG);
  const ddsi_parameter_t *param;

  CU_ASSERT_FATAL (submsg != NULL);
  param = find_inline_qos_parameter (submsg, DDSI_RTPS_SMID_DATA_FRAG, DDSI_PID_RELATED_SAMPLE_IDENTITY);
  CU_ASSERT_FATAL (param != NULL);
  assert_related_sample_identity_bytes (param, &related_guid, related_seq);
  ddsi_xmsg_free (msg);
}
