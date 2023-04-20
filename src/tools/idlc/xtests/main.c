// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_opcodes.h"

extern dds_topic_descriptor_t *desc;
extern void init_sample (void *s);
extern int cmp_sample (const void *sa, const void *sb);
extern int cmp_key (const void *sa, const void *sb);

static void free_sample (void *s)
{
  dds_stream_free_sample (s, &dds_cdrstream_default_allocator, desc->m_ops);
  dds_free (s);
}

static void init_desc (struct dds_cdrstream_desc *cdrstream_desc)
{
  memset (cdrstream_desc, 0, sizeof (*cdrstream_desc));
  *cdrstream_desc = (struct dds_cdrstream_desc) {
    .size = desc->m_size,
    .align = desc->m_align,
    .flagset = desc->m_flagset,
    .ops.nops = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys),
    .ops.ops = (uint32_t *) desc->m_ops
  };

  cdrstream_desc->keys.nkeys = desc->m_nkeys;
  if (cdrstream_desc->keys.nkeys > 0)
  {
    cdrstream_desc->keys.keys = dds_alloc (cdrstream_desc->keys.nkeys  * sizeof (*cdrstream_desc->keys.keys));
    for (uint32_t i = 0; i < cdrstream_desc->keys.nkeys; i++)
    {
      cdrstream_desc->keys.keys[i].ops_offs = desc->m_keys[i].m_offset;
      cdrstream_desc->keys.keys[i].idx = desc->m_keys[i].m_idx;
    }
  }
}

static void print_raw_cdr (dds_ostream_t *os)
{
  for (uint32_t n = 0; n < os->m_index; n++)
  {
    printf("%02x ", os->m_buffer[n]);
    if (!((n + 1) % 16))
      printf("\n");
  }
  printf("\n");
}

int rd_cmp_print_key (dds_ostream_t *os, const void *msg_wr, struct dds_cdrstream_desc *cdrstream_desc)
{
  int res;
  char buf[99999];
  dds_istream_t is = { os->m_buffer, os->m_size, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };

  // read
  void *msg_rd = ddsrt_calloc (1, desc->m_size);
  dds_stream_read_key (&is, msg_rd, &dds_cdrstream_default_allocator, cdrstream_desc);

  // compare
  res = cmp_key(msg_wr, msg_rd);
  printf("key compare result: %d\n", res);

  // print
  is.m_index = 0;
  (void) dds_stream_print_key (&is, cdrstream_desc, buf, sizeof (buf));
  printf("key: %s\n", buf);

  free_sample(msg_rd);
  return res;
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  int res = 0;

  printf("Running test for type %s\n", desc->m_typename);

  // create sertype
  struct dds_cdrstream_desc cdrstream_desc;
  init_desc (&cdrstream_desc);

  enum { LE, BE } tests[2] = { LE, BE };
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    char buf[99999];

    // init data
    void *msg_wr = ddsrt_malloc (desc->m_size);
    memset (msg_wr, 0xdd, desc->m_size);
    init_sample(msg_wr);

    // write data
    printf("cdr write %s\n", tests[i] == BE ? "BE" : "LE");
    dds_ostream_t os = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
    bool ret;
    if (tests[i] == BE)
      ret = dds_stream_write_sampleBE ((dds_ostreamBE_t *)(&os), &dds_cdrstream_default_allocator, msg_wr, &cdrstream_desc);
    else
      ret = dds_stream_write_sampleLE ((dds_ostreamLE_t *)(&os), &dds_cdrstream_default_allocator, msg_wr, &cdrstream_desc);
    if (!ret)
    {
      printf("cdr write failed\n");
      return 1;
    }
    printf("sample data cdr:\n");
    print_raw_cdr (&os);

    dds_istream_t is = { os.m_buffer, os.m_size, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };

    // normalize sample
    uint32_t actual_size = 0;
    bool swap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? (tests[i] == BE) : (tests[i] == LE);
    printf("cdr normalize (%sswap)\n", swap ? "" : "no ");
    if (!dds_stream_normalize ((void *)is.m_buffer, os.m_index, swap, DDSI_RTPS_CDR_ENC_VERSION_2, &cdrstream_desc, false, &actual_size))
    {
      printf("cdr normalize failed\n");
      return 1;
    }
    if (actual_size != os.m_index)
    {
      printf("cdr normalize size invalid (actual: %u, expected: %u)\n", actual_size, os.m_index);
      return 1;
    }

    // read data and check for expected result
    printf("cdr read data\n");
    void *msg_rd = ddsrt_calloc (1, desc->m_size);
    dds_stream_read_sample (&is, msg_rd, &dds_cdrstream_default_allocator, &cdrstream_desc);
    res = cmp_sample(msg_wr, msg_rd);
    printf("data compare result: %d\n", res);

    // print sample
    is.m_index = 0;
    (void) dds_stream_print_sample (&is, &cdrstream_desc, buf, sizeof (buf));
    printf("sample: %s\n", buf);

    if (res == 0 && cdrstream_desc.keys.nkeys > 0)
    {
      // extract key from data
      is.m_index = 0;
      dds_ostream_t os_key_from_data = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
      if (!dds_stream_extract_key_from_data (&is, &os_key_from_data, &dds_cdrstream_default_allocator, &cdrstream_desc))
      {
        printf("extract key from data failed\n");
        return 1;
      }
      printf("key cdr:\n");
      print_raw_cdr (&os_key_from_data);

      res = rd_cmp_print_key (&os_key_from_data, msg_wr, &cdrstream_desc);
      if (res != 0)
        break;

      // write key
      dds_ostream_t os_wr_key = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
      dds_stream_write_key (&os_wr_key, &dds_cdrstream_default_allocator, msg_wr, &cdrstream_desc);

      // extract key from key
      dds_istream_t is_key_from_key = { os_wr_key.m_buffer, os_wr_key.m_size, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
      dds_ostream_t os_key_from_key = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
      dds_stream_extract_key_from_key (&is_key_from_key, &os_key_from_key, &dds_cdrstream_default_allocator, &cdrstream_desc);
      res = rd_cmp_print_key (&os_key_from_key, msg_wr, &cdrstream_desc);

      dds_ostream_fini (&os_key_from_data, &dds_cdrstream_default_allocator);
      dds_ostream_fini (&os_key_from_key, &dds_cdrstream_default_allocator);
      dds_ostream_fini (&os_wr_key, &dds_cdrstream_default_allocator);
    }

    dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
    free_sample(msg_wr);
    free_sample(msg_rd);
    if (res != 0)
      break;
  }

  if (cdrstream_desc.keys.nkeys > 0)
    dds_free (cdrstream_desc.keys.keys);

  return res;
}
