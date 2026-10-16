// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "CUnit/Theory.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/cdr/dds_cdrstream.h"
#include "ddsi__protocol.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__serdata_pserop.h"

static const ddsi_keyhash_t keyhash = {
  .value = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 }
};

static void check_serialized_bytes (const struct ddsi_serdata *data, const unsigned char *expected, size_t size)
{
  unsigned char copy[28];
  CU_ASSERT_EQ_FATAL (ddsi_serdata_size (data), size);
  CU_ASSERT_LEQ_FATAL (size, sizeof (copy));
  // Include slices crossing the identifier, options and payload boundaries.
  for (size_t off = 0; off < size; off++)
  {
    ddsi_serdata_to_ser (data, off, size - off, copy);
    CU_ASSERT_EQ (memcmp (copy, expected + off, size - off), 0);
    ddsrt_iovec_t ref;
    struct ddsi_serdata *refdata = ddsi_serdata_to_ser_ref (data, off, size - off, &ref);
    CU_ASSERT_EQ (ref.iov_len, size - off);
    CU_ASSERT_EQ (memcmp (ref.iov_base, expected + off, size - off), 0);
    ddsi_serdata_to_ser_unref (refdata, &ref);
  }
}

static void check_header_roundtrip (const struct ddsi_sertype *type, unsigned char *wire, size_t size)
{
  ddsrt_iovec_t iov = { .iov_base = wire, .iov_len = (ddsrt_iov_len_t) size };
  struct ddsi_serdata *data;
  dds_return_t rc = ddsi_serdata_from_ser_iov_err (&data, type, SDK_DATA, 1, &iov, size);
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);
  check_serialized_bytes (data, wire, size);

  ddsi_keyhash_t actual_keyhash;
  ddsi_serdata_get_keyhash (data, &actual_keyhash, false);
  CU_ASSERT_EQ (memcmp (&actual_keyhash, &keyhash, sizeof (keyhash)), 0);

  // This conversion passes the stored CDR header back through from_ser_iov.
  struct ddsi_serdata *key = ddsi_serdata_to_untyped (data);
  CU_ASSERT_NEQ_FATAL (key, NULL);
  CU_ASSERT_EQ (key->type, NULL);
  CU_ASSERT_EQ (key->kind, SDK_KEY);
  CU_ASSERT (ddsi_serdata_eqkey (data, key));
  check_serialized_bytes (key, wire, size);
  ddsi_serdata_unref (key);
  ddsi_serdata_unref (data);

  wire[0] = wire[1] = 0xff;
  rc = ddsi_serdata_from_ser_iov_err (&data, type, SDK_DATA, 1, &iov, size);
  CU_ASSERT_EQ (rc, DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (data, NULL);
}

CU_TheoryDataPoints (ddsi_serdata, pserop_header) = {
  CU_DataPoints (bool, false, false, true, true),
  CU_DataPoints (unsigned, 0, 1, 0, 1)
};

CU_Theory ((bool little_endian, unsigned offset), ddsi_serdata, pserop_header)
{
  static const enum ddsi_pserop ops[] = { XK, XSTOP };
  struct ddsi_sertype_pserop *type = ddsrt_calloc (1, sizeof (*type));
  ddsi_sertype_init (&type->c, "header_pserop", &ddsi_sertype_ops_pserop, &ddsi_serdata_ops_pserop, false);
  type->encoding_format = DDSI_RTPS_CDR_ENC_FORMAT_PLAIN;
  type->memsize = sizeof (keyhash);
  type->nops = type->nops_key = sizeof (ops) / sizeof (ops[0]);
  type->ops = type->ops_key = ops;

  // Offset one makes the header unaligned, while retaining a complete byte buffer.
  union { uint16_t align; unsigned char bytes[21]; } storage;
  unsigned char *wire = storage.bytes + offset;
  const struct dds_cdr_header header = {
    .identifier = little_endian ? DDSI_RTPS_CDR_LE : DDSI_RTPS_CDR_BE,
    .options = ddsrt_toBE2u (0x1234)
  };
  memcpy (wire, &header, sizeof (header));
  memcpy (wire + sizeof (header), &keyhash, sizeof (keyhash));
  check_header_roundtrip (&type->c, wire, sizeof (header) + sizeof (keyhash));
  ddsi_sertype_unref (&type->c);
}

CU_TheoryDataPoints (ddsi_serdata, plist_header) = {
  CU_DataPoints (bool, false, false, true, true),
  CU_DataPoints (unsigned, 0, 1, 0, 1)
};

CU_Theory ((bool little_endian, unsigned offset), ddsi_serdata, plist_header)
{
  struct ddsi_sertype_plist *type = ddsrt_calloc (1, sizeof (*type));
  ddsi_sertype_init (&type->c, "header_plist", &ddsi_sertype_ops_plist, &ddsi_serdata_ops_plist, false);
  type->encoding_format = DDSI_RTPS_CDR_ENC_FORMAT_PL;
  type->keyparam = DDSI_PID_PARTICIPANT_GUID;

  union { uint16_t align; unsigned char bytes[29]; } storage;
  unsigned char *wire = storage.bytes + offset;
  const struct {
    struct dds_cdr_header header;
    ddsi_parameter_t parameter;
    ddsi_keyhash_t key;
    ddsi_parameter_t sentinel;
  } sample = {
    .header = {
      .identifier = little_endian ? DDSI_RTPS_PL_CDR_LE : DDSI_RTPS_PL_CDR_BE,
      .options = ddsrt_toBE2u (0x1234)
    },
    .parameter = {
      .parameterid = little_endian ? ddsrt_toLE2u (DDSI_PID_PARTICIPANT_GUID) : ddsrt_toBE2u (DDSI_PID_PARTICIPANT_GUID),
      .length = little_endian ? ddsrt_toLE2u (16) : ddsrt_toBE2u (16)
    },
    .key = keyhash,
    .sentinel = {
      .parameterid = little_endian ? ddsrt_toLE2u (DDSI_PID_SENTINEL) : ddsrt_toBE2u (DDSI_PID_SENTINEL),
      .length = 0
    }
  };
  DDSRT_STATIC_ASSERT (sizeof (sample) == 28);
  memcpy (wire, &sample, sizeof (sample));
  check_header_roundtrip (&type->c, wire, sizeof (sample));
  ddsi_sertype_unref (&type->c);
}
