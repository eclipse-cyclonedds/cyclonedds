// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

// Self-test for the Zephyr port: creates a domain and a participant and reads the
// participant back from the DCPSParticipant built-in topic.  Needs neither a peer nor
// IDL-generated code, so it doubles as the CI smoke test (see sample.yaml).

#include <stdio.h>

#ifdef CONFIG_ARCH_POSIX
#include "posix_board_if.h"
#endif

#include "dds/dds.h"
#include "dds/ddsi/ddsi_config.h"

static int finish (int rc)
{
  printf ("cyclonedds: self-test %s\n", rc == 0 ? "passed" : "FAILED");
  fflush (stdout);
#ifdef CONFIG_ARCH_POSIX
  // On native_sim the simulator keeps running after main returns; terminate it
  // so that "west build -t run" returns and the exit code reflects the result.
  posix_exit (rc);
#endif
  return rc;
}

#define CHECK(x) do { \
    const dds_return_t ret_ = (x); \
    if (ret_ < 0) { \
      printf ("cyclonedds: %s: %s\n", #x, dds_strretcode (ret_)); \
      return finish (1); \
    } \
  } while (0)

int main (void)
{
  struct ddsi_config cfg;
  dds_entity_t dom, pp, rd;
  void *samples[1] = { NULL };
  dds_sample_info_t info;
  dds_return_t n = 0;

  printf ("cyclonedds: self-test on %s\n", CONFIG_BOARD_TARGET);

  // Static configuration instead of CYCLONEDDS_URI; sized for a small receive buffer and
  // without multicast so it also works on a loopback-only system such as native_sim.
  ddsi_config_init_default (&cfg);
  cfg.rbuf_size = 40 * 1024;
  cfg.rmsg_chunk_size = 20 * 1024;
  cfg.multiple_recv_threads = DDSI_BOOLDEF_FALSE;
  cfg.allowMulticast = DDSI_AMC_FALSE;
  cfg.tracemask = DDS_LC_ERROR | DDS_LC_WARNING;
  cfg.tracefile = "stdout";

  CHECK (dom = dds_create_domain_with_rawconfig (0, &cfg));
  CHECK (pp = dds_create_participant (0, NULL, NULL));
  CHECK (rd = dds_create_reader (pp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL));
  for (int i = 0; i < 50 && n == 0; i++)
  {
    CHECK (n = dds_take (rd, samples, &info, 1, 1));
    if (n == 0)
      dds_sleepfor (DDS_MSECS (100));
  }
  if (n == 1)
    CHECK (dds_return_loan (rd, samples, n));
  CHECK (dds_delete (dom));

  return finish ((n == 1 && info.valid_data) ? 0 : 1);
}
