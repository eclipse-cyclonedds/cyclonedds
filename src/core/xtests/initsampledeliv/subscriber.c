// Copyright(c) 2019 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "InitSampleDelivData.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <getopt.h>

static void oops (const char *file, int line)
{
  fflush (stdout);
  fprintf (stderr, "%s:%d\n", file, line);
  abort ();
}

#define oops() oops(__FILE__, __LINE__)

static void wait_for_writer (dds_entity_t ppant)
{
  dds_entity_t rd;
  dds_sample_info_t info;
  void *raw = NULL;
  int32_t n;
  if ((rd = dds_create_reader (ppant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL)) < 0)
    oops ();
  bool done = false;
  do {
    dds_sleepfor (DDS_MSECS (100));
    while ((n = dds_take (rd, &raw, &info, 1, 1)) == 1)
    {
      const dds_builtintopic_endpoint_t *sample = raw;
      if (strcmp (sample->topic_name, "Msg") == 0)
        done = true;
      dds_return_loan (rd, &raw, n);
    }
    if (n < 0) oops ();
  } while (!done);
  dds_delete (rd);
}

static uint32_t get_subscription_matched_count (dds_entity_t rd)
{
  dds_subscription_matched_status_t status;
  if (dds_get_subscription_matched_status (rd, &status) < 0)
    oops ();
  return status.current_count;
}

int main (int argc, char ** argv)
{
  dds_entity_t ppant;
  dds_entity_t tp;
  dds_entity_t rd[2] = { 0, 0 };
  dds_qos_t *qos;
  int opt;
  bool flag_wait = false;
  bool flag_translocal[sizeof (rd) / sizeof (rd[0])] = { false };
  int flag_create_2nd_rd = -1;

  while ((opt = getopt (argc, argv, "d:tTw")) != EOF)
  {
    switch (opt)
    {
      case 'd':
        flag_create_2nd_rd = atoi (optarg);
        break;
      case 't':
        flag_translocal[0] = true;
        break;
      case 'T':
        flag_translocal[1] = true;
        break;
      case 'w':
        flag_wait = true;
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

  if (flag_wait)
  {
    printf ("waiting for writer ...\n");
    fflush (stdout);
    wait_for_writer (ppant);
    printf ("writer seen; giving it some time to discover us and publish data ...\n");
    fflush (stdout);
    dds_sleepfor (DDS_SECS (1));
    printf ("continuing ...\n");
    fflush (stdout);
  }

  /* Reader has overrides for history, durability */
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_qset_durability (qos, flag_translocal[0] ? DDS_DURABILITY_TRANSIENT_LOCAL : DDS_DURABILITY_VOLATILE);
  dds_qset_userdata (qos, "0", 1);
  if ((rd[0] = dds_create_reader (ppant, tp, qos, NULL)) < 0)
    oops ();

  dds_qset_durability (qos, flag_translocal[1] ? DDS_DURABILITY_TRANSIENT_LOCAL : DDS_DURABILITY_VOLATILE);
  dds_qset_userdata (qos, "1", 1);

  int32_t firstmsg[2] = { 0 };
  int32_t prevmsg[2] = { 0 };
  int32_t seqatmatch[2] = { 0 };
  int32_t tldepth = 0;
  int32_t endmsg = 0;
  while (prevmsg[0] == 0 || get_subscription_matched_count (rd[0]) > 0)
  {
    void *raw = NULL;
    dds_sample_info_t info;
    int32_t n;
    for (int i = 0; i < 2 && rd[i]; i++)
    {
      if ((n = dds_take (rd[i], &raw, &info, 1, 1)) < 0)
        oops ();
      else if (n > 0 && info.valid_data)
      {
        const Msg *msg = raw;
        if (prevmsg[i] == 0)
        {
          /* have to postpone first seq# check for transient-local data because the limit
             t-l history means the first sample we read may have an arbitrary sequence
             that antedated the matching */
          printf ("reader %d: first seq %d\n", i, (int) msg->seq);
          fflush (stdout);
          firstmsg[i] = msg->seq;
        }
        else if (msg->seq != prevmsg[i] + 1)
        {
          printf ("reader %d: received %d, previous %d\n", i, (int) msg->seq, (int) prevmsg[i]);
          oops ();
        }
        prevmsg[i] = msg->seq;
        endmsg = msg->final_seq;
        tldepth = msg->tldepth;
        if (seqatmatch[i] == 0)
          seqatmatch[i] = msg->seq_at_match[i];
        dds_return_loan (rd[i], &raw, n);
      }
    }
    if (rd[1] == 0 && prevmsg[0] == flag_create_2nd_rd)
    {
      if ((rd[1] = dds_create_reader (ppant, tp, qos, NULL)) < 0)
        oops ();
    }
    dds_sleepfor (DDS_MSECS (10));
  }
  if (tldepth == 0 || endmsg == 0)
    oops ();
  for (int32_t i = 0; i < 2; i++)
  {
    if (rd[i] == 0)
      continue;
    if (prevmsg[i] != endmsg)
      oops ();
    int32_t refseq;
    if (!flag_translocal[i])
      refseq = seqatmatch[i];
    else if (seqatmatch[i] <= tldepth)
      refseq = 1;
    else
      refseq = seqatmatch[i] - tldepth;
    if (flag_translocal[i] ? (firstmsg[i] > refseq + 1) : firstmsg[i] > refseq)
    {
      /* allow the rare cases where an additional sample was received for volatile data
         (for t-l data, the publisher waits to give so the subscriber can get the data
         in time */
      printf ("reader %"PRId32": first seq %"PRId32" but refseq %"PRId32"\n", i, firstmsg[i], refseq);
      oops ();
    }
  }

  dds_delete_qos (qos);
  dds_delete (ppant);
  return 0;
}
