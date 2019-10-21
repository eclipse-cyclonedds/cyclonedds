#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "mpt/mpt.h"

#include "cyclonedds/dds.h"

#include "cyclonedds/ddsrt/time.h"
#include "cyclonedds/ddsrt/strtol.h"
#include "cyclonedds/ddsrt/process.h"
#include "cyclonedds/ddsrt/environ.h"
#include "cyclonedds/ddsrt/cdtors.h"
#include "cyclonedds/ddsrt/sync.h"
#include "cyclonedds/ddsrt/hopscotch.h"

#include "createwriter.h"
#include "createwriterdata.h"

void createwriter_init (void)
{
}

void createwriter_fini (void)
{
}

#define N_WRITERS 3
#define N_READERS 4
#define N_ROUNDS 100
#define DEPTH 7

static dds_return_t get_matched_count_writers (uint32_t *count, dds_entity_t writers[N_WRITERS])
{
  *count = 0;
  for (int i = 0; i < N_WRITERS; i++)
  {
    dds_publication_matched_status_t st;
    dds_return_t rc = dds_get_publication_matched_status (writers[i], &st);
    if (rc != 0)
      return rc;
    *count += st.current_count;
  }
  return 0;
}

static dds_return_t get_matched_count_readers (uint32_t *count, dds_entity_t readers[N_READERS])
{
  *count = 0;
  for (int i = 0; i < N_READERS; i++)
  {
    dds_subscription_matched_status_t st;
    dds_return_t rc = dds_get_subscription_matched_status (readers[i], &st);
    if (rc != 0)
      return rc;
    *count += st.current_count;
  }
  return 0;
}

MPT_ProcessEntry (createwriter_publisher,
                  MPT_Args (dds_domainid_t domainid,
                            const char *topic_name))
{
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writers[N_WRITERS];
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("=== [Publisher(%d)] Start(%d) ...\n", id, domainid);

  qos = dds_create_qos ();
  dds_qset_durability (qos, DDS_DURABILITY_VOLATILE);
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, DEPTH);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));

  participant = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (participant, 0, "Could not create participant: %s\n", dds_strretcode(participant));

  topic = dds_create_topic (participant, &DiscStress_CreateWriter_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (topic, 0, "Could not create topic: %s\n", dds_strretcode(topic));

  for (int i = 0; i < N_WRITERS; i++)
  {
    writers[i] = dds_create_writer (participant, topic, qos, NULL);
    MPT_ASSERT_FATAL_GT (writers[i], 0, "Could not create writer: %s\n", dds_strretcode (writers[i]));
  }

  /* At some point in time, all remote readers will be known, and will consequently be matched
     immediately on creation of the writer.  That means it expects ACKs from all those readers, and
     that in turn means they should go into their "linger" state until all samples they wrote have
     been acknowledged (or the timeout occurs - but it shouldn't in a quiescent setup with a pretty
     reliable transport like a local loopback).  So other than waiting for some readers to show up
     at all, there's no need to look at matching events or to use anything other than volatile,
     provided the readers accept an initial short sequence in the first batch.  */
  printf ("=== [Publisher(%d)] Publishing while waiting for some reader ...\n", id);
  fflush (stdout);
  uint32_t seq = 0;
  int32_t round = -1;
  uint32_t wrseq = 0;
  bool matched = false;
  while (round < N_ROUNDS)
  {
    if (matched)
      round++;
    else
    {
      uint32_t mc;
      rc = get_matched_count_writers (&mc, writers);
      MPT_ASSERT_FATAL_EQ (rc, 0, "Could not get publication matched status: %s\n", dds_strretcode (rc));
      matched = (mc == N_READERS * N_WRITERS);
      if (matched)
      {
        printf ("=== [Publisher(%d)] All readers found; continuing at [%"PRIu32",%"PRIu32"] for %d rounds\n",
                id, wrseq * N_WRITERS + 1, (wrseq + 1) * N_WRITERS, N_ROUNDS);
        fflush (stdout);
      }
    }

    for (uint32_t i = 0; i < N_WRITERS; i++)
    {
      for (uint32_t j = 0; j < DEPTH; j++)
      {
        /* Note: +1 makes wrseq equal to the writer entity id */
        DiscStress_CreateWriter_Msg m = {
          .round = round, .wrseq = wrseq * N_WRITERS + i + 1, .wridx = i, .histidx = j, .seq = seq
        };
        rc = dds_write (writers[i], &m);
        MPT_ASSERT_FATAL_EQ (rc, 0, "Could not write data: %s\n", dds_strretcode (rc));
        seq++;
      }
    }

    /* Delete, then create writer: this should result in the other process processing alternating
       deletes and creates, and consequently, all readers should always have at least some matching
       writers.

       Round 0 is the first round where all readers have been matched with writer 0 */
    for (int i = 0; i < N_WRITERS; i++)
    {
      rc = dds_delete (writers[i]);
      MPT_ASSERT_FATAL_EQ (rc, 0, "Could not delete writer: %s\n", dds_strretcode (rc));
      writers[i] = dds_create_writer (participant, topic, qos, NULL);
      MPT_ASSERT_FATAL_GT (writers[i], 0, "Could not create writer: %s\n", dds_strretcode (writers[i]));
    }

    wrseq++;
  }

  rc = dds_delete (participant);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Teardown failed\n");
  dds_delete_qos (qos);
  printf ("=== [Publisher(%d)] Done\n", id);
}

struct wrinfo {
  uint32_t rdid;
  uint32_t wrid;
  dds_instance_handle_t wr_iid;
  bool last_not_alive;
  uint32_t seen;
};

static uint32_t wrinfo_hash (const void *va)
{
  const struct wrinfo *a = va;
  return (uint32_t) (((a->rdid + UINT64_C (16292676669999574021)) *
                      (a->wrid + UINT64_C (10242350189706880077))) >> 32);
}

static int wrinfo_eq (const void *va, const void *vb)
{
  const struct wrinfo *a = va;
  const struct wrinfo *b = vb;
  return a->rdid == b->rdid && a->wrid == b->wrid;
}

#define LOGDEPTH 30
#define LOGLINE 200

static void dumplog (char logbuf[LOGDEPTH][LOGLINE], int *logidx)
{
  if (logbuf[*logidx][0])
    for (int i = 0; i < LOGDEPTH; i++)
      fputs (logbuf[i], stdout);
  for (int i = 0; i < *logidx; i++)
    fputs (logbuf[i], stdout);
  for (int i = 0; i < LOGDEPTH; i++)
    logbuf[i][0] = 0;
  *logidx = 0;
}

/*
 * The DiscStress_CreateWriter subscriber.
 * It waits for sample(s) and checks the content.
 */
MPT_ProcessEntry(createwriter_subscriber,
                 MPT_Args(dds_domainid_t domainid,
                          const char *topic_name))
{
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t readers[N_READERS];
  dds_return_t rc;
  dds_qos_t *qos;
  int id = (int) ddsrt_getpid ();

  printf ("--- [Subscriber(%d)] Start(%d) ...\n", id, domainid);

  qos = dds_create_qos ();
  dds_qset_durability (qos, DDS_DURABILITY_VOLATILE);
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, DEPTH);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));

  participant = dds_create_participant (domainid, NULL, NULL);
  MPT_ASSERT_FATAL_GT (participant, 0, "Could not create participant: %s\n", dds_strretcode(participant));

  topic = dds_create_topic (participant, &DiscStress_CreateWriter_Msg_desc, topic_name, qos, NULL);
  MPT_ASSERT_FATAL_GT (topic, 0, "Could not create topic: %s\n", dds_strretcode(topic));

  /* Keep all history on the reader: then we should see DEPTH samples from each writer, except,
     perhaps, the very first time */
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  for (int i = 0; i < N_READERS; i++)
  {
    readers[i] = dds_create_reader (participant, topic, qos, NULL);
    MPT_ASSERT_FATAL_GT (readers[i], 0, "Could not create reader: %s\n", dds_strretcode (readers[i]));
  }

  printf ("--- [Subscriber(%d)] Waiting for some writer to match ...\n", id);
  fflush (stdout);

  /* Wait until we have matching writers */
  dds_entity_t ws = dds_create_waitset (participant);
  MPT_ASSERT_FATAL_GT (ws, 0, "Could not create waitset: %s\n", dds_strretcode (ws));
  for (int i = 0; i < N_READERS; i++)
  {
    rc = dds_set_status_mask (readers[i], DDS_SUBSCRIPTION_MATCHED_STATUS);
    MPT_ASSERT_FATAL_EQ (rc, 0, "Could not set subscription matched mask: %s\n", dds_strretcode (rc));
    rc = dds_waitset_attach (ws, readers[i], i);
    MPT_ASSERT_FATAL_EQ (rc, 0, "Could not attach reader to waitset: %s\n", dds_strretcode (rc));
  }

  {
    uint32_t mc;
    do {
      rc = get_matched_count_readers (&mc, readers);
      MPT_ASSERT_FATAL_EQ (rc, 0, "Could not get subscription matched status: %s\n", dds_strretcode (rc));
    } while (mc < N_READERS * N_WRITERS && (rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY)) >= 0);
    MPT_ASSERT_FATAL_GEQ (rc, 0, "Wait for writers failed: %s\n", dds_strretcode (rc));
  }

  /* Add DATA_AVAILABLE event; of course it would be easier to simply set it to desired value, but
     this is more fun */
  for (int i = 0; i < N_READERS; i++)
  {
    uint32_t mask;
    rc = dds_get_status_mask (readers[i], &mask);
    MPT_ASSERT_FATAL_EQ (rc, 0, "Could not get status mask: %s\n", dds_strretcode (rc));
    MPT_ASSERT_FATAL ((mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0, "Retrieved status mask doesn't have MATCHED set\n");
    mask |= DDS_DATA_AVAILABLE_STATUS;
    rc = dds_set_status_mask (readers[i], mask);
    MPT_ASSERT_FATAL_EQ (rc, 0, "Could not add data available status: %s\n", dds_strretcode (rc));
  }

  /* Loop while we have some matching writers */
  printf ("--- [Subscriber(%d)] Checking data ...\n", id);
  fflush (stdout);
  struct ddsrt_hh *wrinfo = ddsrt_hh_new (1, wrinfo_hash, wrinfo_eq);
  dds_entity_t xreader = 0;
  bool matched = true;
  char logbuf[N_READERS][LOGDEPTH][LOGLINE] = {{ "" }};
  int logidx[N_READERS] = { 0 };
  while (matched)
  {
    dds_attach_t xs[N_READERS];

    {
      uint32_t mc;
      rc = get_matched_count_readers (&mc, readers);
      MPT_ASSERT_FATAL_EQ (rc, 0, "Could not get subscription matched status: %s\n", dds_strretcode (rc));
      matched = (mc > 0);
    }

    /* Losing all matched writers will result in a transition to NO_WRITERS, and so in a DATA_AVAILABLE
       state, but the unregistering happens before the match count is updated.  I'm not sure it makes to
       make those atomic; I also don't think it is very elegant to do a final NO_WRITERS message with
       that was already signalled as no longer matching in a listener.


       Given that situation, waiting for data available, taking everything and immediately checking
       the subscription matched status doesn't guarantee in observing the absence of matched writers.
       Hence enabling the SUBSCRIPTION_MATCHED status on the readers, so that the actual removal will
       also trigger the waitset.

       The current_count == 0 case is so we do one final take after deciding to stop, just in case the
       unregisters & state change happened in between taking and checking the number of matched writers. */
    int32_t nxs = dds_waitset_wait (ws, xs, N_READERS, matched ? DDS_SECS (12) : 0);
    MPT_ASSERT_FATAL_GEQ (nxs, 0, "Waiting for data failed: %s\n", dds_strretcode (nxs));
    if (nxs == 0 && matched)
    {
      printf ("--- [Subscriber(%d)] Unexpected timeout\n", id);
      for (int i = 0; i < N_READERS; i++)
      {
        dds_subscription_matched_status_t st;
        rc = dds_get_subscription_matched_status (readers[i], &st);
        MPT_ASSERT_FATAL_EQ (rc, 0, "Could not get subscription matched status: %s\n", dds_strretcode (rc));
        printf ("--- [Subscriber(%d)] reader %d current_count %"PRIu32"\n", id, i, st.current_count);
      }
      fflush (stdout);
      MPT_ASSERT_FATAL (0, "Timed out\n");
    }

#define READ_LEN 3
    for (int32_t i = 0; i < nxs; i++)
    {
      void *raw[READ_LEN] = { NULL };
      dds_sample_info_t si[READ_LEN];
      int32_t n;
      while ((n = dds_take (readers[xs[i]], raw, si, READ_LEN, READ_LEN)) > 0)
      {
        for (int32_t j = 0; j < n; j++)
        {
          DiscStress_CreateWriter_Msg const * const s = raw[j];
          if (si[j].valid_data && s->round >= 0)
          {
            /* Cyclone always sets the key value, other fields are 0 for invalid data */
            struct wrinfo wri_key = { .wrid = s->wrseq, .rdid = (uint32_t) xs[i] };
            struct wrinfo *wri;

            MPT_ASSERT_FATAL_LT (s->wridx, N_WRITERS, "Writer id out of range (%"PRIu32" %"PRIu32"\n)", s->wrseq, s->wridx);

#define XASSERT(cond, ...) do { if (!(cond)) { \
dumplog (logbuf[xs[i]], &logidx[xs[i]]); \
MPT_ASSERT (0, __VA_ARGS__); \
} } while (0)
#define XASSERT_FATAL(cond, ...) do { if (!(cond)) { \
dumplog (logbuf[xs[i]], &logidx[xs[i]]); \
MPT_ASSERT_FATAL (0, __VA_ARGS__); \
} } while (0)

            if ((wri = ddsrt_hh_lookup (wrinfo, &wri_key)) == NULL)
            {
              wri = malloc (sizeof (*wri));
              *wri = wri_key;
              rc = ddsrt_hh_add (wrinfo, wri);
              MPT_ASSERT_FATAL_NEQ (rc, 0, "Both wrinfo lookup and add failed\n");
            }

            snprintf (logbuf[xs[i]][logidx[xs[i]]], sizeof (logbuf[xs[i]][logidx[xs[i]]]),
                      "%"PRId32": %"PRId32".%"PRIu32" %"PRIu32".%"PRIu32" iid %"PRIx64" new %"PRIx64" st %c%c seq %"PRIu32" seen %"PRIu32"\n",
                      (uint32_t) xs[i], s->round, s->wrseq, s->wridx, s->histidx, wri->wr_iid, si[j].publication_handle,
                      (si[j].instance_state == DDS_IST_ALIVE) ? 'A' : (si[j].instance_state == DDS_IST_NOT_ALIVE_DISPOSED) ? 'D' : 'U',
                      si[j].valid_data ? 'v' : 'i', s->seq, wri->seen);
            if (++logidx[xs[i]] == LOGDEPTH)
              logidx[xs[i]] = 0;

            XASSERT (wri->wr_iid == 0 || wri->wr_iid == si[j].publication_handle, "Mismatch between wrid and publication handle");

            XASSERT_FATAL (s->histidx < DEPTH, "depth_idx out of range");
            XASSERT ((wri->seen & (1u << s->histidx)) == 0, "Duplicate sample\n");
            if (s->histidx > 0)
              XASSERT ((wri->seen & (1u << (s->histidx - 1))) != 0, "Out of order sample (1)\n");
            XASSERT (wri->seen < (1u << s->histidx), "Out of order sample (2)\n");
            wri->wr_iid = si[j].publication_handle;
            wri->seen |= 1u << s->histidx;
          }
        }
        rc = dds_return_loan (readers[xs[i]], raw, n);
        MPT_ASSERT_FATAL_EQ (rc, 0, "Could not return loan: %s\n", dds_strretcode (rc));

        /* Flip-flop between create & deleting a reader to ensure matching activity on the proxy
           writers, as that, too should occasionally push the delivery out of the fast path */
        if (xreader)
        {
          rc = dds_delete (xreader);
          MPT_ASSERT_FATAL_EQ (rc, 0, "Error on deleting extra reader: %s\n", dds_strretcode (rc));
          xreader = 0;
        }
        else
        {
          xreader = dds_create_reader (participant, topic, qos, NULL);
          MPT_ASSERT_FATAL_GT (xreader, 0, "Could not create extra reader: %s\n", dds_strretcode (xreader));
        }
      }
      MPT_ASSERT_FATAL_EQ (rc, 0, "Error on reading: %s\n", dds_strretcode (rc));
    }
  }

  rc = dds_delete (participant);
  MPT_ASSERT_EQ (rc, DDS_RETCODE_OK, "Teardown failed\n");
  dds_delete_qos (qos);

  struct ddsrt_hh_iter it;
  uint32_t nwri = 0;
  for (struct wrinfo *wri = ddsrt_hh_iter_first (wrinfo, &it); wri; wri = ddsrt_hh_iter_next (&it))
  {
    nwri++;
    if (wri->seen != (1u << DEPTH) - 1)
    {
      MPT_ASSERT (0, "Some data missing at end (rd %d wr %d seen %"PRIx32")\n", wri->rdid, wri->wrid, wri->seen);
    }
    /* simple iteration won't touch an object pointer twice */
    free (wri);
  }
  ddsrt_hh_free (wrinfo);
  MPT_ASSERT (nwri >= (N_ROUNDS / 3) * N_READERS * N_WRITERS, "Less data received than expected\n");
  printf ("--- [Subscriber(%d)] Done after %"PRIu32" sets\n", id, nwri / (N_READERS * N_WRITERS));
}
