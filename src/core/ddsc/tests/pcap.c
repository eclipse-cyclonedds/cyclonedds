// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <stdint.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "test_common.h"
#include "test_oneliner.h"

static uint16_t le16 (const unsigned char *p)
{
  return (uint16_t) ((uint16_t) p[0] | ((uint16_t) p[1] << 8));
}

static uint32_t le32 (const unsigned char *p)
{
  return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static uint16_t be16 (const unsigned char *p)
{
  return (uint16_t) (((uint16_t) p[0] << 8) | (uint16_t) p[1]);
}

static void assert_pcap_smells_ok (const char *filename)
{
  FILE *fp = fopen (filename, "rb");
  CU_ASSERT_NEQ_FATAL (fp, NULL);

  unsigned char hdr[24];
  CU_ASSERT_EQ_FATAL (fread (hdr, 1, sizeof (hdr), fp), sizeof (hdr));
  CU_ASSERT_EQ (le32 (hdr + 0), UINT32_C (0xa1b2c3d4));
  CU_ASSERT_EQ (le16 (hdr + 4), UINT16_C (2));
  CU_ASSERT_EQ (le16 (hdr + 6), UINT16_C (4));
  CU_ASSERT_EQ (le32 (hdr + 16), UINT32_C (65535));
  CU_ASSERT_EQ (le32 (hdr + 20), UINT32_C (101));

  unsigned char rechdr[16];
  CU_ASSERT_EQ_FATAL (fread (rechdr, 1, sizeof (rechdr), fp), sizeof (rechdr));
  const uint32_t incl_len = le32 (rechdr + 8);
  const uint32_t orig_len = le32 (rechdr + 12);
  CU_ASSERT_EQ (incl_len, orig_len);
  CU_ASSERT_FATAL (incl_len >= 28);
  CU_ASSERT_FATAL (incl_len < UINT32_C (65535));

  unsigned char pkt[64];
  const size_t to_read = (incl_len < sizeof (pkt)) ? incl_len : sizeof (pkt);
  CU_ASSERT_EQ_FATAL (fread (pkt, 1, to_read, fp), to_read);
  CU_ASSERT_EQ (pkt[0] >> 4, 4);
  CU_ASSERT_EQ (pkt[0] & 0x0f, 5);
  CU_ASSERT_EQ (pkt[9], 17);
  CU_ASSERT_EQ (be16 (pkt + 2), incl_len);
  CU_ASSERT_EQ (be16 (pkt + 24), incl_len - 20);
  if (incl_len >= 40)
    CU_ASSERT (pkt[28] == 'R' && pkt[29] == 'T' && pkt[30] == 'P' && pkt[31] == 'S');

  fclose (fp);
}

CU_Test(ddsc_pcap, oneliner_generates_pcap)
{
  char filename[128];
  (void) snprintf (filename, sizeof (filename), "cyclonedds_pcap_test.pcap");
  FILE *fp = fopen (filename, "wb");
  CU_ASSERT_NEQ_FATAL (fp, NULL);
  CU_ASSERT_EQ_FATAL (fclose (fp), 0);

  char *config = NULL;
  ddsrt_asprintf (&config,
    "<Domain id=\"0\">"
    "  <Tracing>"
    "    <PacketCaptureFile>%s</PacketCaptureFile>"
    "  </Tracing>"
    "</Domain>",
    filename);

  const int result = test_oneliner_with_config (
    "pm w "
    "sm r' "
    "?pm w ?sm r'",
    config);
  ddsrt_free (config);
  CU_ASSERT_GT_FATAL (result, 0);

  assert_pcap_smells_ok (filename);
}
