#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "dds/dds.h"

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/environ.h"

#include "test_common.h"
#include "CreateWriter.h"

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

struct thread_arg {
  dds_domainid_t domainid;
  const char *topicname;
};

static uint32_t createwriter_publisher (void *varg)
{
  struct thread_arg const * const arg = varg;
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writers[N_WRITERS];
  dds_return_t rc;
  dds_qos_t *qos;

  qos = dds_create_qos ();
  dds_qset_durability (qos, DDS_DURABILITY_VOLATILE);
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, DEPTH);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));

  participant = dds_create_participant (arg->domainid, NULL, NULL);
  CU_ASSERT_FATAL (participant > 0);

  topic = dds_create_topic (participant, &DiscStress_CreateWriter_Msg_desc, arg->topicname, qos, NULL);
  CU_ASSERT_FATAL (topic > 0);

  for (int i = 0; i < N_WRITERS; i++)
  {
    writers[i] = dds_create_writer (participant, topic, qos, NULL);
    CU_ASSERT_FATAL (writers[i] > 0);
  }

  /* At some point in time, all remote readers will be known, and will consequently be matched
     immediately on creation of the writer.  That means it expects ACKs from all those readers, and
     that in turn means they should go into their "linger" state until all samples they wrote have
     been acknowledged (or the timeout occurs - but it shouldn't in a quiescent setup with a pretty
     reliable transport like a local loopback).  So other than waiting for some readers to show up
     at all, there's no need to look at matching events or to use anything other than volatile,
     provided the readers accept an initial short sequence in the first batch.  */
  printf ("=== Publishing while waiting for some reader ...\n");
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
      CU_ASSERT_FATAL (rc == 0);
      matched = (mc == N_READERS * N_WRITERS);
      if (matched)
      {
        printf ("All readers found; continuing at [%"PRIu32",%"PRIu32"] for %d rounds\n",
                wrseq * N_WRITERS + 1, (wrseq + 1) * N_WRITERS, N_ROUNDS);
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
        CU_ASSERT_FATAL (rc == 0);
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
      CU_ASSERT_FATAL (rc == 0);
      writers[i] = dds_create_writer (participant, topic, qos, NULL);
      CU_ASSERT_FATAL (writers[i] > 0);
    }

    wrseq++;
  }

  rc = dds_delete (participant);
  CU_ASSERT_FATAL (rc == 0);
  dds_delete_qos (qos);
  return 0;
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

struct logbuf {
  char line[LOGDEPTH][LOGLINE];
  int logidx;
};

static void dumplog (struct logbuf *logbuf)
{
  if (logbuf->line[logbuf->logidx][0])
    for (int i = logbuf->logidx; i < LOGDEPTH; i++)
      fputs (logbuf->line[i], stdout);
  for (int i = 0; i < logbuf->logidx; i++)
    fputs (logbuf->line[i], stdout);
  for (int i = 0; i < LOGDEPTH; i++)
    logbuf->line[i][0] = 0;
  logbuf->logidx = 0;
  fflush (stdout);
}

static bool checksample (struct ddsrt_hh *wrinfo, const dds_sample_info_t *si, const DiscStress_CreateWriter_Msg *s, struct logbuf *logbuf, uint32_t rdid)
{
  /* Cyclone always sets the key value, other fields are 0 for invalid data */
  struct wrinfo *wri;
  bool result = true;

  CU_ASSERT_FATAL (s->wridx < N_WRITERS);
  CU_ASSERT_FATAL (s->histidx < DEPTH);

  if ((wri = ddsrt_hh_lookup (wrinfo, &(struct wrinfo){ .wrid = s->wrseq, .rdid = rdid })) == NULL)
  {
    wri = malloc (sizeof (*wri));
    assert (wri);
    memset (wri, 0, sizeof (*wri));
    wri->wrid = s->wrseq;
    wri->rdid = rdid;
    const int ok = ddsrt_hh_add (wrinfo, wri);
    CU_ASSERT_FATAL (ok);
  }

  snprintf (logbuf->line[logbuf->logidx], sizeof (logbuf->line[logbuf->logidx]),
            "%"PRIu32": %"PRId32".%"PRIu32" %"PRIu32".%"PRIu32" iid %"PRIx64" new %"PRIx64" st %c%c seq %"PRIu32" seen %"PRIu32"\n",
            rdid, s->round, s->wrseq, s->wridx, s->histidx, wri->wr_iid, si->publication_handle,
            (si->instance_state == DDS_IST_ALIVE) ? 'A' : (si->instance_state == DDS_IST_NOT_ALIVE_DISPOSED) ? 'D' : 'U',
            si->valid_data ? 'v' : 'i', s->seq, wri->seen);
  if (++logbuf->logidx == LOGDEPTH)
    logbuf->logidx = 0;

  if (wri->wr_iid != 0 && wri->wr_iid != si->publication_handle) {
    printf ("Mismatch between wrid %"PRIx64" and publication handle %"PRIx64"\n", wri->wr_iid, si->publication_handle);
    result = false;
  }
  if (wri->seen & (1u << s->histidx)) {
    printf ("Duplicate sample (wri->seen %"PRIx32" s->histidx %"PRIu32")\n", wri->seen, s->histidx);
    result = false;
  }
  //if (s->histidx > 0)
  //  XASSERT ((wri->seen & (1u << (s->histidx - 1))) != 0, "Out of order sample (1)\n");

  if (s->histidx > 0 && !(wri->seen & (1u << (s->histidx - 1)))) {
    printf ("Out of order sample (1) (wri->seen %"PRIx32" s->histidx %"PRIu32")\n", wri->seen, s->histidx);
    result = false;
  }
  if (!(wri->seen < (1u << s->histidx))) {
    printf ("Out of order sample (2) (wri->seen %"PRIx32" s->histidx %"PRIu32")\n", wri->seen, s->histidx);
    result = false;
  }
  wri->wr_iid = si->publication_handle;
  wri->seen |= 1u << s->histidx;
  if (!result)
    dumplog (logbuf);
  return result;
}

/*
 * The DiscStress_CreateWriter subscriber.
 * It waits for sample(s) and checks the content.
 */
static uint32_t createwriter_subscriber (void *varg)
{
  struct thread_arg const * const arg = varg;
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t readers[N_READERS];
  dds_return_t rc;
  dds_qos_t *qos;

  qos = dds_create_qos ();
  dds_qset_durability (qos, DDS_DURABILITY_VOLATILE);
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, DEPTH);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));

  participant = dds_create_participant (arg->domainid, NULL, NULL);
  CU_ASSERT_FATAL (participant > 0);

  topic = dds_create_topic (participant, &DiscStress_CreateWriter_Msg_desc, arg->topicname, qos, NULL);
  CU_ASSERT_FATAL (topic > 0);

  /* Keep all history on the reader: then we should see DEPTH samples from each writer, except,
     perhaps, the very first time */
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  for (int i = 0; i < N_READERS; i++)
  {
    readers[i] = dds_create_reader (participant, topic, qos, NULL);
    CU_ASSERT_FATAL (readers[i] > 0);
  }

  printf ("--- Waiting for some writer to match ...\n");
  fflush (stdout);

  /* Wait until we have matching writers */
  dds_entity_t ws = dds_create_waitset (participant);
  CU_ASSERT_FATAL (ws > 0);
  for (int i = 0; i < N_READERS; i++)
  {
    rc = dds_set_status_mask (readers[i], DDS_SUBSCRIPTION_MATCHED_STATUS);
    CU_ASSERT_FATAL (rc == 0);
    rc = dds_waitset_attach (ws, readers[i], i);
    CU_ASSERT_FATAL (rc == 0);
  }

  {
    uint32_t mc;
    do {
      rc = get_matched_count_readers (&mc, readers);
      CU_ASSERT_FATAL (rc == 0);
    } while (mc < N_READERS * N_WRITERS && (rc = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY)) >= 0);
    CU_ASSERT_FATAL (rc >= 0);
  }

  /* Add DATA_AVAILABLE event; of course it would be easier to simply set it to desired value, but
     this is more fun */
  for (int i = 0; i < N_READERS; i++)
  {
    uint32_t mask;
    rc = dds_get_status_mask (readers[i], &mask);
    CU_ASSERT_FATAL (rc == 0);
    CU_ASSERT_FATAL ((mask & DDS_SUBSCRIPTION_MATCHED_STATUS) != 0);
    mask |= DDS_DATA_AVAILABLE_STATUS;
    rc = dds_set_status_mask (readers[i], mask);
    CU_ASSERT_FATAL (rc == 0);
  }

  /* Loop while we have some matching writers */
  printf ("--- Checking data ...\n");
  fflush (stdout);
  struct ddsrt_hh *wrinfo = ddsrt_hh_new (1, wrinfo_hash, wrinfo_eq);
  dds_entity_t xreader = 0;
  bool matched = true;
  struct logbuf logbuf[N_READERS];
  memset (logbuf, 0, sizeof (logbuf));
  while (matched)
  {
    dds_attach_t xs[N_READERS];

    {
      uint32_t mc;
      rc = get_matched_count_readers (&mc, readers);
      CU_ASSERT_FATAL (rc == 0);
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
    CU_ASSERT_FATAL (nxs >= 0);
    if (nxs == 0 && matched)
    {
      printf ("--- Unexpected timeout\n");
      for (int i = 0; i < N_READERS; i++)
      {
        dds_subscription_matched_status_t st;
        rc = dds_get_subscription_matched_status (readers[i], &st);
        CU_ASSERT_FATAL (rc == 0);
        printf ("--- reader %d current_count %"PRIu32"\n", i, st.current_count);
      }
      fflush (stdout);
      CU_ASSERT_FATAL (0);
    }

#define READ_LEN 3
    for (int32_t i = 0; i < nxs; i++)
    {
      void *raw[READ_LEN] = { NULL };
      dds_sample_info_t si[READ_LEN];
      int32_t n;
      while ((n = dds_take (readers[xs[i]], raw, si, READ_LEN, READ_LEN)) > 0)
      {
        bool error = false;
        for (int32_t j = 0; j < n; j++)
        {
          DiscStress_CreateWriter_Msg const * const s = raw[j];
          if (si[j].valid_data && s->round >= 0)
          {
            if (!checksample (wrinfo, &si[j], s, &logbuf[xs[i]], (uint32_t)xs[i]))
              error = true;
          }
        }
        if (error)
        {
          fflush (stdout);
          CU_ASSERT_FATAL (0);
        }

        rc = dds_return_loan (readers[xs[i]], raw, n);
        CU_ASSERT_FATAL (rc == 0);

        /* Flip-flop between create & deleting a reader to ensure matching activity on the proxy
           writers, as that, too should occasionally push the delivery out of the fast path */
        if (xreader)
        {
          rc = dds_delete (xreader);
          CU_ASSERT_FATAL (rc == 0);
          xreader = 0;
        }
        else
        {
          xreader = dds_create_reader (participant, topic, qos, NULL);
          CU_ASSERT_FATAL (xreader > 0);
        }
      }
      CU_ASSERT_FATAL (rc == 0);
    }
  }

  rc = dds_delete (participant);
  CU_ASSERT_FATAL(rc == 0);
  dds_delete_qos (qos);

  int err = 0;
  struct ddsrt_hh_iter it;
  uint32_t nwri = 0;
  for (struct wrinfo *wri = ddsrt_hh_iter_first (wrinfo, &it); wri; wri = ddsrt_hh_iter_next (&it))
  {
    nwri++;
    if (wri->seen != (1u << DEPTH) - 1)
    {
      printf ("err: wri->seen = %x rdid %"PRIu32" wrid %"PRIu32" iid %"PRIx64" lna %d\n",
              wri->seen, wri->rdid, wri->wrid, wri->wr_iid, wri->last_not_alive);
      err++;
    }
    /* simple iteration won't touch an object pointer twice */
    free (wri);
  }
  ddsrt_hh_free (wrinfo);
  CU_ASSERT_FATAL (err == 0);
  CU_ASSERT_FATAL (nwri >= (N_ROUNDS / 3) * N_READERS * N_WRITERS);
  printf ("--- Done after %"PRIu32" sets\n", nwri / (N_READERS * N_WRITERS));
  return 0;
}

CU_Test(ddsc_discstress, create_writer, .timeout = 20)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
#ifdef DDS_HAS_SHM
  const char* config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery><Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>";
#else
  const char* config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>";
#endif
  char *pub_conf = ddsrt_expand_envvars (config, 0);
  char *sub_conf = ddsrt_expand_envvars (config, 1);
  const dds_entity_t pub_dom = dds_create_domain (0, pub_conf);
  CU_ASSERT_FATAL (pub_dom > 0);
  const dds_entity_t sub_dom = dds_create_domain (1, sub_conf);
  CU_ASSERT_FATAL (sub_dom > 0);
  ddsrt_free (pub_conf);
  ddsrt_free (sub_conf);

  char topicname[100];
  create_unique_topic_name ("ddsc_discstress_create_writer", topicname, sizeof topicname);

  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  ddsrt_thread_t pub_tid, sub_tid;
  dds_return_t rc;

  struct thread_arg pub_arg = {
    .domainid = 0,
    .topicname = topicname
  };
  rc = ddsrt_thread_create (&pub_tid, "pub_thread", &tattr, createwriter_publisher, &pub_arg);
  CU_ASSERT_FATAL (rc == 0);

  struct thread_arg sub_arg = {
    .domainid = 1,
    .topicname = topicname
  };
  rc = ddsrt_thread_create (&sub_tid, "sub_thread", &tattr, createwriter_subscriber, &sub_arg);
  CU_ASSERT_FATAL (rc == 0);

  ddsrt_thread_join (pub_tid, NULL);
  ddsrt_thread_join (sub_tid, NULL);

  rc = dds_delete (pub_dom);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (sub_dom);
  CU_ASSERT_FATAL (rc == 0);
}
