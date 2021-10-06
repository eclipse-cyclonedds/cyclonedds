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

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("Running test for type %s\n", desc->m_typename);

    // init data
    void *msg_wr = ddsrt_calloc (1, desc->m_size);
    init_sample(msg_wr);

    // create sertype
    struct ddsi_sertype_default sertype;
    memset (&sertype, 0, sizeof (sertype));
    sertype.type = (struct ddsi_sertype_default_desc) {
      .size = desc->m_size,
      .align = desc->m_align,
      .flagset = desc->m_flagset,
      .keys.nkeys = 0,
      .keys.keys = NULL,
      .ops.nops = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys),
      .ops.ops = (uint32_t *) desc->m_ops
    };

    // write data
    dds_ostream_t os;
    os.m_buffer = NULL;
    os.m_index = 0;
    os.m_size = 0;
    os.m_xcdr_version = CDR_ENC_VERSION_2;
    dds_stream_write_sample (&os, msg_wr, &sertype);
    printf("cdr write complete\n");

    // output raw cdr
    // for (uint32_t n = 0; n < os.m_index; n++)
    // {
    //   printf("%02x ", os.m_buffer[n]);
    //   if (!((n + 1) % 16))
    //     printf("\n");
    // }
    // printf("\n");

    // read data
    dds_istream_t is;
    is.m_buffer = os.m_buffer;
    is.m_index = 0;
    is.m_size = os.m_size;
    is.m_xcdr_version = CDR_ENC_VERSION_2;

    void *msg_rd = ddsrt_calloc (1, desc->m_size);
    dds_stream_read_sample (&is, msg_rd, &sertype);
    printf("cdr read complete\n");

    /* Check for expected result */
    int res = cmp_sample(msg_wr, msg_rd);
    printf("compare result: %d\n", res);

    dds_ostream_fini (&os);
    // is->_buffer aliases os->_buffer, so no free
    free_sample(msg_wr);
    free_sample(msg_rd);
    return res;
}
