// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/atomics.h"
#include "InitSampleDelivData.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

static void oops (const char *file, int line)
{
  fflush (stdout);
  fprintf (stderr, "%s:%d\n", file, line);
  abort ();
}

#define oops() oops(__FILE__, __LINE__)

static void on_pub_matched (dds_entity_t wr, const dds_publication_matched_status_t st, void *varg)
{
  ddsrt_atomic_uint32_t *new_readers = varg;
  dds_sample_info_t info;
  void *raw = NULL;
  dds_entity_t rd;
  printf ("pubmatched\n");
  if ((rd = dds_create_reader (dds_get_participant (wr), DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL)) < 0)
    oops ();
  if (dds_read_instance (rd, &raw, &info, 1, 1, st.last_subscription_handle) != 1)
    oops ();
  const dds_builtintopic_endpoint_t *sample = raw;
  /* in our test the user data must be present */
  void *ud;
  size_t udsz;
  if (!dds_qget_userdata (sample->qos, &ud, &udsz))
    oops ();
  int rdid = atoi (ud);
  if (rdid < 0 || rdid > 31)
    oops ();
  printf ("pubmatched: %d\n", rdid);
  fflush (stdout);
  ddsrt_atomic_or32 (new_readers, UINT32_C (1) << rdid);
  dds_free (ud);
  dds_return_loan (rd, &raw, 1);
}

static uint32_t get_publication_matched_count (dds_entity_t wr)
{
  dds_publication_matched_status_t status;
  if (dds_get_publication_matched_status (wr, &status) < 0)
    oops ();
  return status.current_count;
}

int main (int argc, char ** argv)
{
  dds_entity_t ppant;
  dds_entity_t tp;
  dds_entity_t wr;
  dds_qos_t *qos;
  ddsrt_atomic_uint32_t newreaders = DDSRT_ATOMIC_UINT32_INIT (0);
  int opt;
  bool flag_prewrite = false;
  bool flag_translocal = false;
  const int32_t tlhist = 10;

  while ((opt = getopt (argc, argv, "tp")) != EOF)
  {
    switch (opt)
    {
      case 't':
        flag_translocal = true;
        break;
      case 'p':
        flag_prewrite = true;
        break;
      default:
        fprintf (stderr, "usage error: see source code\n");
        exit (2);
    }
  }

  if ((ppant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL)) < 0)
    oops ();

  qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  if ((tp = dds_create_topic (ppant, &Msg_desc, "Msg", qos, NULL)) < 0)
    oops ();

  /* Writer has overrides for history, durability */
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_durability (qos, flag_translocal ? DDS_DURABILITY_TRANSIENT_LOCAL : DDS_DURABILITY_VOLATILE);
  dds_qset_durability_service (qos, 0, DDS_HISTORY_KEEP_LAST, tlhist, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);

  dds_listener_t *list = dds_create_listener (&newreaders);
  dds_lset_publication_matched (list, on_pub_matched);
  if ((wr = dds_create_writer (ppant, tp, qos, list)) < 0)
    oops ();
  dds_delete_listener (list);
  dds_delete_qos (qos);

  Msg sample = {
    .keyval = 0,
    .seq = 1,
    .tldepth = tlhist,
    .final_seq = 30,
    .seq_at_match = { 0, 0 }
  };
  dds_time_t tlast = 0, tnewrd = 0;
  while (sample.seq <= sample.final_seq)
  {
    uint32_t newrd = ddsrt_atomic_and32_ov (&newreaders, 0);
    for (uint32_t i = 0; i < 32; i++)
    {
      if (newrd & (UINT32_C (1) << i))
      {
        if (i >= (uint32_t) (sizeof (sample.seq_at_match) / sizeof (sample.seq_at_match[0])))
          oops ();
        if (sample.seq_at_match[i] != 0)
          oops ();
        sample.seq_at_match[i] = sample.seq;
        tnewrd = dds_time ();
        printf ("%d.%09d newreader %d: start seq %d\n", (int) (tnewrd / DDS_NSECS_IN_SEC), (int) (tnewrd % DDS_NSECS_IN_SEC), (int) i, (int) sample.seq_at_match[i]);
        fflush (stdout);
      }
    }

    if (get_publication_matched_count (wr) || (flag_prewrite && sample.seq <= tlhist + 1))
    {
      dds_time_t tnow = dds_time ();
      if (tnow - tlast > DDS_MSECS (100) || newrd)
      {
        if (dds_write (wr, &sample) < 0)
          oops ();
        sample.seq++;
        tlast = tnow;
        if (sample.seq > sample.final_seq)
        {
          tnow = dds_time ();
          printf ("%d.%09d done writing\n", (int) (tnow / DDS_NSECS_IN_SEC), (int) (tnow % DDS_NSECS_IN_SEC));
          fflush (stdout);
        }
      }
    }

    dds_sleepfor (DDS_MSECS (1));
  }

  dds_sleepfor (DDS_MSECS (100));
  dds_wait_for_acks (wr, DDS_INFINITY);
  dds_delete (ppant);
  return 0;
}
