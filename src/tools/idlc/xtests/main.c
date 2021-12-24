/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsc/dds_opcodes.h"

extern dds_topic_descriptor_t *desc;
extern void init_sample (void *s);
extern int cmp_sample (const void *sa, const void *sb);

static void free_sample (void *s)
{
  dds_stream_free_sample (s, desc->m_ops);
  dds_free (s);
}

static void init_sertype (struct ddsi_sertype_default *sertype)
{
  memset (sertype, 0, sizeof (*sertype));
  sertype->type = (struct ddsi_sertype_default_desc) {
    .size = desc->m_size,
    .align = desc->m_align,
    .flagset = desc->m_flagset,
    .keys.nkeys = 0,
    .keys.keys = NULL,
    .ops.nops = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys),
    .ops.ops = (uint32_t *) desc->m_ops
  };
}

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  printf("Running test for type %s\n", desc->m_typename);

  // create sertype
  struct ddsi_sertype_default sertype;
  init_sertype (&sertype);

  enum { LE, BE } tests[2] = { LE, BE };
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    // init data
    void *msg_wr = ddsrt_calloc (1, desc->m_size);
    init_sample(msg_wr);

    // write data
    printf("cdr write %s\n", tests[i] == BE ? "BE" : "LE");
    dds_ostream_t os = { NULL, 0, 0, CDR_ENC_VERSION_2 };
    if (tests[i] == BE)
      dds_stream_write_sampleBE ((dds_ostreamBE_t *)(&os), msg_wr, &sertype);
    else
      dds_stream_write_sampleLE ((dds_ostreamLE_t *)(&os), msg_wr, &sertype);


    // output raw cdr
    for (uint32_t n = 0; n < os.m_index; n++)
    {
      printf("%02x ", os.m_buffer[n]);
      if (!((n + 1) % 16))
        printf("\n");
    }
    printf("\n");

    dds_istream_t is = { os.m_buffer, os.m_size, 0, CDR_ENC_VERSION_2 };

    // normalize sample
    uint32_t actual_size = 0;
    bool swap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? (tests[i] == BE) : (tests[i] == LE);
    printf("cdr normalize (%sswap)\n", swap ? "" : "no ");
    if (!dds_stream_normalize ((void *)is.m_buffer, os.m_index, swap, CDR_ENC_VERSION_2, &sertype, false, &actual_size))
    {
      printf("cdr normalize failed\n");
      return 1;
    }
    if (actual_size != os.m_index)
    {
      printf("cdr normalize size invalid (actual: %u, expected: %u)\n", actual_size, os.m_index);
      return 1;
    }

    // read data
    printf("cdr read\n");
    void *msg_rd = ddsrt_calloc (1, desc->m_size);
    dds_stream_read_sample (&is, msg_rd, &sertype);

    // check for expected result
    int res = cmp_sample(msg_wr, msg_rd);
    printf("compare result: %d\n", res);

    // print sample
    char buf[99999];
    is.m_index = 0;
    (void) dds_stream_print_sample (&is, &sertype, buf, sizeof (buf));
    printf("sample: %s\n", buf);

    dds_ostream_fini (&os);
    // is->_buffer aliases os->_buffer, so no free
    free_sample(msg_wr);
    free_sample(msg_rd);

    if (res != 0)
      return res;
  }

  return 0;
}
