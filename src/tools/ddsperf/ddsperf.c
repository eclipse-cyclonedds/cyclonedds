/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#define _ISOC99_SOURCE
#define _POSIX_PTHREAD_SEMANTICS
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#if _WIN32
#include <getopt.h>
#endif

#include "dds/dds.h"
#include "ddsperf_types.h"

#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/atomics.h"

#include "cputime.h"
#include "netload.h"

#if !defined(_WIN32) && !defined(LWIP_SOCKET)
#include <errno.h>
#endif

#define UDATA_MAGIC "DDSPerf:"
#define UDATA_MAGIC_SIZE (sizeof (UDATA_MAGIC) - 1)

#define PINGPONG_RAWSIZE 20000

enum topicsel {
  KS,   /* KeyedSeq type: seq#, key, sequence-of-octet */
  K32,  /* Keyed32  type: seq#, key, array-of-24-octet (sizeof = 32) */
  K256, /* Keyed256 type: seq#, key, array-of-248-octet (sizeof = 256) */
  OU,   /* OneULong type: seq# */
  UK16, /* Unkeyed16, type: seq#, array-of-12-octet (sizeof = 16) */
  UK1024/* Unkeyed1024, type: seq#, array-of-1020-octet (sizeof = 1024) */
};

enum submode {
  SM_NONE,    /* no subscriber at all */
  SM_WAITSET, /* subscriber using a waitset */
  SM_POLLING, /* ... using polling, sleeping for 1ms if no data */
  SM_LISTENER /* ... using a DATA_AVAILABLE listener */
};

static const char *argv0;
static ddsrt_atomic_uint32_t termflag = DDSRT_ATOMIC_UINT32_INIT (0);

/* Domain participant, guard condition for termination, domain id */
static dds_entity_t dp;
static dds_instance_handle_t dp_handle;
static dds_entity_t termcond;
static dds_domainid_t did = DDS_DOMAIN_DEFAULT;

/* Readers for built-in topics to get discovery information */
static dds_entity_t rd_participants, rd_subscriptions, rd_publications;

/* Topics, readers, writers (except for pong writers: there are
   many of those) */
static dds_entity_t tp_data, tp_ping, tp_pong, tp_stat;
static char tpname_data[32], tpname_ping[32], tpname_pong[32];
static dds_entity_t sub, pub, wr_data, wr_ping, wr_stat, rd_data, rd_ping, rd_pong, rd_stat;

/* Number of different key values to use (must be 1 for OU type) */
static unsigned nkeyvals = 1;

/* Topic type to use */
static enum topicsel topicsel = KS;

/* Data and ping/pong subscriber triggering modes */
static enum submode submode = SM_LISTENER;
static enum submode pingpongmode = SM_LISTENER;

/* Size of the sequence in KeyedSeq type in bytes */
static uint32_t baggagesize = 0;

/* Whether or not to register instances prior to writing */
static bool register_instances = true;

/* Maximum run time in seconds */
static double dur = HUGE_VAL;

/* Minimum number of peers (if not met, exit status is 1) */
static uint32_t minmatch = 0;

/* Maximum time it may take to discover all MINMATCH peers */
static double maxwait = HUGE_VAL;

/* Number of participants for which all expected endpoints
   have been matched (this includes the local participant
   if ignorelocal is DDS_IGNORELOCAL_NONE) [protected by
   disc_lock] */
static uint32_t matchcount = 0;

/* An error is always signalled if not all endpoints of a
   participant have been discovered within a set amount of
   time (5s, currently) [protected by disc_lock] */
static uint32_t matchtimeout = 0;

/* Data is published in bursts of this many samples */
static uint32_t burstsize = 1;

/* Whether to use reliable or best-effort readers/writers */
static bool reliable = true;

/* History depth for throughput data reader and writer; 0 is
   KEEP_ALL, otherwise it is KEEP_LAST histdepth.  Ping/pong
   always uses KEEP_LAST 1. */
static int32_t histdepth = 0;

/* Publishing rate in Hz, HUGE_VAL means as fast as possible,
   0 means no throughput data is published at all */
static double pub_rate;

/* Fraction of throughput data samples that double as a ping
   message */
static uint32_t ping_frac = 0;

/* Setting for "ignore local" reader/writer QoS: whether or
   not to ignore readers and writers in the same particiapnt
   that would otherwise match */
static dds_ignorelocal_kind_t ignorelocal = DDS_IGNORELOCAL_PARTICIPANT;

/* Pinging interval for roundtrip testing, 0 means as fast as
   possible, DDS_INFINITY means never */
static dds_duration_t ping_intv;

/* Number of times a new ping was sent before all expected
   pongs had been received */
static uint32_t ping_timeouts = 0;

static ddsrt_mutex_t disc_lock;

/* Publisher statistics and lock protecting it */
struct hist {
  unsigned nbins;
  uint64_t binwidth;
  uint64_t bin0; /* bins are [bin0,bin0+binwidth),[bin0+binwidth,bin0+2*binwidth) */
  uint64_t binN; /* bin0 + nbins*binwidth */
  uint64_t min, max; /* min and max observed since last reset */
  uint64_t under, over; /* < bin0, >= binN */
  uint64_t bins[];
};

static ddsrt_mutex_t pubstat_lock;
static struct hist *pubstat_hist;

/* Subscriber statistics for tracking number of samples received
   and lost per source */
struct eseq_stat {
  /* totals */
  uint64_t nrecv;
  uint64_t nlost;
  uint64_t nrecv_bytes;
  uint32_t last_size;

  /* stats printer state */
  uint64_t nrecv_ref;
  uint64_t nlost_ref;
  uint64_t nrecv_bytes_ref;
};

struct eseq_admin {
  ddsrt_mutex_t lock;
  unsigned nkeys;
  unsigned nph;
  dds_instance_handle_t *ph;
  struct eseq_stat *stats;
  uint32_t **eseq;
};

static struct eseq_admin eseq_admin;

/* Entry for mapping ping/data publication handle to pong writer */
struct subthread_arg_pongwr {
  dds_instance_handle_t pubhandle;
  dds_instance_handle_t pphandle;
  dds_entity_t wr_pong;
};

/* Entry for mapping pong publication handle to latency statistics */
struct subthread_arg_pongstat {
  dds_instance_handle_t pubhandle;
  dds_instance_handle_t pphandle;
  uint64_t min, max;
  uint64_t sum;
  uint32_t cnt;
  uint64_t *raw;
};

/* Pong statistics is stored in n array of npongstat entries
   [protected by pongstat_lock] */
static ddsrt_mutex_t pongstat_lock;
static uint32_t npongstat;
static struct subthread_arg_pongstat *pongstat;

/* All topics have a sequence number, this is the one of the
   latest ping sent and the number of pongs received for that
   sequence number.  Also the time at which it was sent for
   generating new ping messages in the case of loss of topology
   changes, and a timestamp after which a warning is printed
   when a new ping is published.  [All protected by
   pongwr_lock] */
static dds_time_t cur_ping_time;
static dds_time_t twarn_ping_timeout;
static uint32_t cur_ping_seq;
static uint32_t n_pong_seen;

/* Number of pongs expected for each ping [protected by
   pongwr_lock] */
static uint32_t n_pong_expected;

/* Table mapping data and ping publication handles to writers
   of pongs (one per participant in a unique partition so that
   a participant only receives responses to its own pings) is
   a simply array of npongwr entries [protected by pongwr_lock] */
static ddsrt_mutex_t pongwr_lock;
static uint32_t npongwr;
static struct subthread_arg_pongwr *pongwr;

/* Each subscriber thread gets its own not-quite-pre-allocated
   set of samples (it does use a loan, but that loan gets reused) */
struct subthread_arg {
  dds_entity_t rd;
  uint32_t max_samples;
  dds_sample_info_t *iseq;
  void **mseq;
};

/* Type used for converting GUIDs to strings, used for generating
   the per-participant partition names */
struct guidstr {
  char str[36];
};

/* Endpoints that can be matched; all endpoints except for a data
   subscriber always exist; the data subscriber is only created if
   requested */
#define MM_RD_DATA   1u
#define MM_RD_PING   2u
#define MM_RD_PONG   4u
#define MM_WR_DATA   8u
#define MM_WR_PING  16u
#define MM_WR_PONG  32u
#define MM_ALL (2 * MM_WR_PONG - 1)

struct ppant {
  ddsrt_avl_node_t avlnode;     /* embedded AVL node for handle index */
  ddsrt_fibheap_node_t fhnode;  /* prio queue for timeout handling */
  dds_instance_handle_t handle; /* participant instance handle */
  dds_builtintopic_guid_t guid; /* participant GUID */
  char *hostname;               /* hostname is taken from user_data QoS */
  uint32_t pid;                 /* pid is also taken from user_data QoS */
  dds_time_t tdisc;             /* time at which it was discovered */
  dds_time_t tdeadline;         /* by what time must unmatched be 0 */
  uint32_t unmatched;           /* expected but not yet detected endpoints */
};

static int cmp_instance_handle (const void *va, const void *vb)
{
  const dds_instance_handle_t *a = va;
  const dds_instance_handle_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

/* AVL tree of ppant structures indexed on handle using cmp_instance_handle */
static ddsrt_avl_treedef_t ppants_td = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ppant, avlnode), offsetof (struct ppant, handle), cmp_instance_handle, 0);
static ddsrt_avl_tree_t ppants;

/* Priority queue (Fibonacci heap) of ppant structures with tdeadline as key */
static int cmp_ppant_tdeadline (const void *va, const void *vb)
{
  const struct ppant *a = va;
  const struct ppant *b = vb;
  return (a->tdeadline == b->tdeadline) ? 0 : (a->tdeadline < b->tdeadline) ? -1 : 1;
}

static ddsrt_fibheap_def_t ppants_to_match_fhd = DDSRT_FIBHEAPDEF_INITIALIZER (offsetof (struct ppant, fhnode), cmp_ppant_tdeadline);
static ddsrt_fibheap_t ppants_to_match;

/* Printing error messages: error2 is for DDS errors, error3 is for usage errors */
static void verrorx (int exitcode, const char *fmt, va_list ap) ddsrt_attribute_noreturn;
static void error2 (const char *fmt, ...) ddsrt_attribute_format ((printf, 1, 2)) ddsrt_attribute_noreturn;
static void error3 (const char *fmt, ...) ddsrt_attribute_format ((printf, 1, 2)) ddsrt_attribute_noreturn;

static void publication_matched_listener (dds_entity_t wr, const dds_publication_matched_status_t status, void *arg);

struct seq_keyval {
  uint32_t seq;
  int32_t keyval;
};

union data {
  uint32_t seq;
  struct seq_keyval seq_keyval;
  KeyedSeq ks;
  Keyed32 k32;
  Keyed256 k256;
  OneULong ou;
  Unkeyed16 uk16;
  Unkeyed1024 uk1024;
};

static void verrorx (int exitcode, const char *fmt, va_list ap)
{
  vprintf (fmt, ap);
  fflush (stdout);
  exit (exitcode);
}

static void error2 (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  verrorx (2, fmt, ap);
}

static void error3 (const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  verrorx (3, fmt, ap);
}

static char *make_guidstr (struct guidstr *buf, const dds_builtintopic_guid_t *guid)
{
  snprintf (buf->str, sizeof (buf->str), "%02x%02x%02x%02x_%02x%02x%02x%02x_%02x%02x%02x%02x_%02x%02x%02x%02x",
            guid->v[0], guid->v[1], guid->v[2], guid->v[3],
            guid->v[4], guid->v[5], guid->v[6], guid->v[7],
            guid->v[8], guid->v[9], guid->v[10], guid->v[11],
            guid->v[12], guid->v[13], guid->v[14], guid->v[15]);
  return buf->str;
}

static void hist_reset_minmax (struct hist *h)
{
  h->min = UINT64_MAX;
  h->max = 0;
}

static void hist_reset (struct hist *h)
{
  hist_reset_minmax (h);
  h->under = 0;
  h->over = 0;
  memset (h->bins, 0, h->nbins * sizeof (*h->bins));
}

static struct hist *hist_new (unsigned nbins, uint64_t binwidth, uint64_t bin0)
{
  struct hist *h = malloc (sizeof (*h) + nbins * sizeof (*h->bins));
  h->nbins = nbins;
  h->binwidth = binwidth;
  h->bin0 = bin0;
  h->binN = h->bin0 + h->nbins * h->binwidth;
  hist_reset (h);
  return h;
}

static void hist_free (struct hist *h)
{
  free (h);
}

static void hist_record (struct hist *h, uint64_t x, unsigned weight)
{
  if (x < h->min)
    h->min = x;
  if (x > h->max)
    h->max = x;
  if (x < h->bin0)
    h->under += weight;
  else if (x >= h->binN)
    h->over += weight;
  else
    h->bins[(x - h->bin0) / h->binwidth] += weight;
}

static void xsnprintf(char *buf, size_t bufsz, size_t *p, const char *fmt, ...)
{
  if (*p < bufsz)
  {
    int n;
    va_list ap;
    va_start (ap, fmt);
    n = vsnprintf (buf + *p, bufsz - *p, fmt, ap);
    va_end (ap);
    *p += (size_t) n;
  }
}

static void hist_print (const char *prefix, struct hist *h, dds_time_t dt, int reset)
{
  const size_t l_size = sizeof(char) * h->nbins + 200 + strlen (prefix);
  const size_t hist_size = sizeof(char) * h->nbins + 1;
  char *l = (char *) malloc(l_size);
  char *hist = (char *) malloc(hist_size);
  double dt_s = (double)dt / 1e9, avg;
  uint64_t peak = 0, cnt = h->under + h->over;
  size_t p = 0;

  xsnprintf (l, l_size, &p, "%s", prefix);

  hist[h->nbins] = 0;
  for (unsigned i = 0; i < h->nbins; i++)
  {
    cnt += h->bins[i];
    if (h->bins[i] > peak)
      peak = h->bins[i];
  }

  const uint64_t p1 = peak / 100;
  const uint64_t p10 = peak / 10;
  const uint64_t p20 = 1 * peak / 5;
  const uint64_t p40 = 2 * peak / 5;
  const uint64_t p60 = 3 * peak / 5;
  const uint64_t p80 = 4 * peak / 5;
  for (unsigned i = 0; i < h->nbins; i++)
  {
    if (h->bins[i] == 0) hist[i] = ' ';
    else if (h->bins[i] <= p1) hist[i] = '.';
    else if (h->bins[i] <= p10) hist[i] = '_';
    else if (h->bins[i] <= p20) hist[i] = '-';
    else if (h->bins[i] <= p40) hist[i] = '=';
    else if (h->bins[i] <= p60) hist[i] = 'x';
    else if (h->bins[i] <= p80) hist[i] = 'X';
    else hist[i] = '@';
  }

  avg = (double) cnt / dt_s;
  if (avg < 999.5)
    xsnprintf (l, l_size, &p, "%5.3g", avg);
  else if (avg < 1e6)
    xsnprintf (l, l_size, &p, "%4.3gk", avg / 1e3);
  else
    xsnprintf (l, l_size, &p, "%4.3gM", avg / 1e6);
  xsnprintf (l, l_size, &p, "/s ");

  if (h->min == UINT64_MAX)
    xsnprintf (l, l_size, &p, " inf ");
  else if (h->min < 1000)
    xsnprintf (l, l_size, &p, "%3"PRIu64"n ", h->min);
  else if (h->min + 500 < 1000000)
    xsnprintf (l, l_size, &p, "%3"PRIu64"u ", (h->min + 500) / 1000);
  else if (h->min + 500000 < 1000000000)
    xsnprintf (l, l_size, &p, "%3"PRIu64"m ", (h->min + 500000) / 1000000);
  else
    xsnprintf (l, l_size, &p, "%3"PRIu64"s ", (h->min + 500000000) / 1000000000);

  if (h->bin0 > 0) {
    int pct = (cnt == 0) ? 0 : 100 * (int) ((h->under + cnt/2) / cnt);
    xsnprintf (l, l_size, &p, "%3d%% ", pct);
  }

  {
    int pct = (cnt == 0) ? 0 : 100 * (int) ((h->over + cnt/2) / cnt);
    xsnprintf (l, l_size, &p, "|%s| %3d%%", hist, pct);
  }

  if (h->max < 1000)
    xsnprintf (l, l_size, &p, " %3"PRIu64"n", h->max);
  else if (h->max + 500 < 1000000)
    xsnprintf (l, l_size, &p, " %3"PRIu64"u", (h->max + 500) / 1000);
  else if (h->max + 500000 < 1000000000)
    xsnprintf (l, l_size, &p, " %3"PRIu64"m", (h->max + 500000) / 1000000);
  else
    xsnprintf (l, l_size, &p, " %3"PRIu64"s", (h->max + 500000000) / 1000000000);

  (void) p;
  puts (l);
  fflush (stdout);
  free (l);
  free (hist);
  if (reset)
    hist_reset (h);
}

static void *make_baggage (dds_sequence_t *b, uint32_t cnt)
{
  b->_maximum = b->_length = cnt;
  if (cnt == 0)
    b->_buffer = NULL;
  else
  {
    b->_buffer = malloc (b->_maximum);
    memset(b->_buffer, 0xee, b->_maximum);
  }
  return b->_buffer;
}

static void *init_sample (union data *data, uint32_t seq)
{
  void *baggage = NULL;
  switch (topicsel)
  {
    case KS:
      data->ks.seq = seq;
      data->ks.keyval = 0;
      baggage = make_baggage (&data->ks.baggage, baggagesize);
      break;
    case K32:
      data->k32.seq = seq;
      data->k32.keyval = 0;
      memset (data->k32.baggage, 0xee, sizeof (data->k32.baggage));
      break;
    case K256:
      data->k256.seq = seq;
      data->k256.keyval = 0;
      memset (data->k256.baggage, 0xee, sizeof (data->k256.baggage));
      break;
    case OU:
      data->ou.seq = seq;
      break;
    case UK16:
      data->uk16.seq = seq;
      memset (data->uk16.baggage, 0xee, sizeof (data->uk16.baggage));
      break;
    case UK1024:
      data->uk1024.seq = seq;
      memset (data->uk1024.baggage, 0xee, sizeof (data->uk1024.baggage));
      break;
  }
  return baggage;
}

static uint32_t pubthread (void *varg)
{
  int result;
  dds_instance_handle_t *ihs;
  dds_time_t ntot = 0, tfirst, tfirst0;
  union data data;
  uint64_t timeouts = 0;
  void *baggage = NULL;
  (void) varg;

  memset (&data, 0, sizeof (data));
  assert (nkeyvals > 0);
  assert (topicsel != OU || nkeyvals == 1);

  baggage = init_sample (&data, 0);
  ihs = malloc (nkeyvals * sizeof (dds_instance_handle_t));
  for (unsigned k = 0; k < nkeyvals; k++)
  {
    data.seq_keyval.keyval = (int32_t) k;
    if (register_instances)
      dds_register_instance (wr_data, &ihs[k], &data);
    else
      ihs[k] = 0;
  }
  data.seq_keyval.keyval = 0;

  tfirst0 = tfirst = dds_time();

  uint32_t bi = 0;
  while (!ddsrt_atomic_ld32 (&termflag))
  {
    /* lsb of timestamp is abused to signal whether the sample is a ping requiring a response or not */
    bool reqresp = (ping_frac == 0) ? 0 : (ping_frac == UINT32_MAX) ? 1 : (ddsrt_random () <= ping_frac);
    const dds_time_t t_write = (dds_time () & ~1) | reqresp;
    if ((result = dds_write_ts (wr_data, &data, t_write)) != DDS_RETCODE_OK)
    {
      printf ("write error: %d\n", result);
      fflush (stdout);
      if (result != DDS_RETCODE_TIMEOUT)
        exit (2);
      timeouts++;
      /* retry with original timestamp, it really is just a way of reporting
         blocking for an exceedingly long time */
      continue;
    }
    if (reqresp)
    {
      dds_write_flush (wr_data);
    }

    const dds_time_t t_post_write = dds_time ();
    dds_time_t t = t_post_write;
    ddsrt_mutex_lock (&pubstat_lock);
    hist_record (pubstat_hist, (uint64_t) ((t_post_write - t_write) / 1), 1);
    ntot++;
    ddsrt_mutex_unlock (&pubstat_lock);

    data.seq_keyval.keyval = (data.seq_keyval.keyval + 1) % (int32_t) nkeyvals;
    data.seq++;

    if (pub_rate < HUGE_VAL)
    {
      if (++bi == burstsize)
      {
        /* FIXME: should average rate over a short-ish period, rather than over the entire run */
        while (((double) (ntot / burstsize) / ((double) (t - tfirst0) / 1e9 + 5e-3)) > pub_rate && !ddsrt_atomic_ld32 (&termflag))
        {
          /* FIXME: flushing manually because batching is not yet implemented properly */
          dds_write_flush (wr_data);
          dds_sleepfor (DDS_MSECS (1));
          t = dds_time ();
        }
        bi = 0;
      }
    }
  }
  if (baggage)
    free (baggage);
  free (ihs);
  return 0;
}

static void init_eseq_admin (struct eseq_admin *ea, unsigned nkeys)
{
  ddsrt_mutex_init (&ea->lock);
  ea->nkeys = nkeys;
  ea->nph = 0;
  ea->ph = NULL;
  ea->stats = NULL;
  ea->eseq = NULL;
}

static void fini_eseq_admin (struct eseq_admin *ea)
{
  free (ea->ph);
  free (ea->stats);
  for (unsigned i = 0; i < ea->nph; i++)
    free (ea->eseq[i]);
  ddsrt_mutex_destroy (&ea->lock);
  free (ea->eseq);
}

static int check_eseq (struct eseq_admin *ea, uint32_t seq, uint32_t keyval, uint32_t size, const dds_instance_handle_t pubhandle)
{
  uint32_t *eseq;
  if (keyval >= ea->nkeys)
  {
    printf ("received key %"PRIu32" >= nkeys %u\n", keyval, ea->nkeys);
    exit (3);
  }
  ddsrt_mutex_lock (&ea->lock);
  for (uint32_t i = 0; i < ea->nph; i++)
    if (pubhandle == ea->ph[i])
    {
      uint32_t e = ea->eseq[i][keyval];
      ea->eseq[i][keyval] = seq + ea->nkeys;
      ea->stats[i].nrecv++;
      ea->stats[i].nrecv_bytes += size;
      ea->stats[i].nlost += seq - e;
      ea->stats[i].last_size = size;
      ddsrt_mutex_unlock (&ea->lock);
      return seq == e;
    }
  ea->ph = realloc (ea->ph, (ea->nph + 1) * sizeof (*ea->ph));
  ea->ph[ea->nph] = pubhandle;
  ea->eseq = realloc (ea->eseq, (ea->nph + 1) * sizeof (*ea->eseq));
  ea->eseq[ea->nph] = malloc (ea->nkeys * sizeof (*ea->eseq[ea->nph]));
  eseq = ea->eseq[ea->nph];
  for (unsigned i = 0; i < ea->nkeys; i++)
    eseq[i] = seq + (i - keyval) + (i <= keyval ? ea->nkeys : 0);
  ea->stats = realloc (ea->stats, (ea->nph + 1) * sizeof (*ea->stats));
  memset (&ea->stats[ea->nph], 0, sizeof (ea->stats[ea->nph]));
  ea->stats[ea->nph].nrecv = 1;
  ea->stats[ea->nph].nrecv_bytes = size;
  ea->stats[ea->nph].last_size = size;
  ea->nph++;
  ddsrt_mutex_unlock (&ea->lock);
  return 1;
}

static dds_instance_handle_t get_pphandle_for_pubhandle (dds_instance_handle_t pubhandle)
{
  /* FIXME: implement the get_matched_... interfaces so there's no need for keeping a reader
   (and having to GC it, which I'm skipping here ...) */
  int32_t n;
  void *msg = NULL;
  dds_sample_info_t info;
  if ((n = dds_read_instance (rd_publications, &msg, &info, 1, 1, pubhandle)) < 0)
    error2 ("dds_read_instance(rd_publications, %"PRIx64") failed: %d\n", pubhandle, (int) n);
  if (n == 0 || !info.valid_data)
  {
    printf ("get_pong_writer: publication handle %"PRIx64" not found\n", pubhandle);
    fflush (stdout);
    return 0;
  }
  else
  {
    const dds_builtintopic_endpoint_t *sample = msg;
    dds_instance_handle_t pphandle = sample->participant_instance_handle;
    dds_return_loan (rd_publications, &msg, n);
    return pphandle;
  }
}

static bool update_roundtrip (dds_instance_handle_t pubhandle, uint64_t tdelta, bool isping, uint32_t seq)
{
  bool allseen;
  ddsrt_mutex_lock (&pongstat_lock);
  if (isping && seq == cur_ping_seq)
  {
    ddsrt_mutex_lock (&pongwr_lock);
    allseen = (++n_pong_seen == n_pong_expected);
    ddsrt_mutex_unlock (&pongwr_lock);
  }
  else
  {
    allseen = false;
  }
  for (uint32_t i = 0; i < npongstat; i++)
    if (pongstat[i].pubhandle == pubhandle)
    {
      struct subthread_arg_pongstat * const x = &pongstat[i];
      if (tdelta < x->min) x->min = tdelta;
      if (tdelta > x->max) x->max = tdelta;
      x->sum += tdelta;
      if (x->cnt < PINGPONG_RAWSIZE)
        x->raw[x->cnt] = tdelta;
      x->cnt++;
      ddsrt_mutex_unlock (&pongstat_lock);
      return allseen;
    }
  pongstat = realloc (pongstat, (npongstat + 1) * sizeof (*pongstat));
  struct subthread_arg_pongstat * const x = &pongstat[npongstat];
  x->pubhandle = pubhandle;
  x->pphandle = get_pphandle_for_pubhandle (pubhandle);
  x->min = x->max = x->sum = tdelta;
  x->cnt = 1;
  x->raw = malloc (PINGPONG_RAWSIZE * sizeof (*x->raw));
  x->raw[0] = tdelta;
  npongstat++;
  ddsrt_mutex_unlock (&pongstat_lock);
  return allseen;
}

static dds_entity_t get_pong_writer_locked (dds_instance_handle_t pubhandle)
{
  dds_instance_handle_t pphandle;

  for (uint32_t j = 0; j < npongwr; j++)
    if (pongwr[j].pubhandle == pubhandle)
      return pongwr[j].wr_pong;

  /* FIXME: implement the get_matched_... interfaces so there's no need for keeping a reader
     (and having to GC it, which I'm skipping here ...) */
  pphandle = get_pphandle_for_pubhandle (pubhandle);

  /* This gets called when no writer is associaed yet with pubhandle, but it may be that a writer
     is associated already with pphandle (because there is the data writer and the ping writer) */
  for (uint32_t i = 0; i < npongwr; i++)
  {
    if (pongwr[i].pphandle == pphandle)
    {
      dds_entity_t wr_pong = pongwr[i].wr_pong;
      if (pongwr[i].pubhandle == 0)
      {
        pongwr[i].pubhandle = pubhandle;
        return wr_pong;
      }
      else
      {
        pongwr = realloc (pongwr, (npongwr + 1) * sizeof (*pongwr));
        pongwr[npongwr].pubhandle = pubhandle;
        pongwr[npongwr].pphandle = pphandle;
        pongwr[npongwr].wr_pong = wr_pong;
        npongwr++;
        return wr_pong;
      }
    }
  }
  printf ("get_pong_writer: participant handle %"PRIx64" not found\n", pphandle);
  fflush (stdout);
  return 0;
}

static dds_entity_t get_pong_writer (dds_instance_handle_t pubhandle)
{
  dds_entity_t wr_pong = 0;
  ddsrt_mutex_lock (&pongwr_lock);
  wr_pong = get_pong_writer_locked (pubhandle);
  ddsrt_mutex_unlock (&pongwr_lock);
  return wr_pong;
}

static uint32_t topic_payload_size (enum topicsel tp, uint32_t bgsize)
{
  uint32_t size = 0;
  switch (tp)
  {
    case KS:     size = 12 + bgsize; break;
    case K32:    size = 32; break;
    case K256:   size = 256; break;
    case OU:     size = 4; break;
    case UK16:   size = 16; break;
    case UK1024: size = 1024; break;
  }
  return size;
}

static bool process_data (dds_entity_t rd, struct subthread_arg *arg)
{
  uint32_t max_samples = arg->max_samples;
  dds_sample_info_t *iseq = arg->iseq;
  void **mseq = arg->mseq;
  int32_t nread_data;
  if ((nread_data = dds_take (rd, mseq, iseq, max_samples, max_samples)) < 0)
    error2 ("dds_take (rd_data): %d\n", (int) nread_data);
  for (int32_t i = 0; i < nread_data; i++)
  {
    if (iseq[i].valid_data)
    {
      uint32_t seq = 0, keyval = 0, size = 0;
      switch (topicsel)
      {
        case KS: {
          KeyedSeq *d = mseq[i]; keyval = d->keyval; seq = d->seq; size = topic_payload_size (topicsel, d->baggage._length);
          break;
        }
        case K32:    { Keyed32 *d     = mseq[i]; keyval = d->keyval; seq = d->seq; size = topic_payload_size (topicsel, 0); } break;
        case K256:   { Keyed256 *d    = mseq[i]; keyval = d->keyval; seq = d->seq; size = topic_payload_size (topicsel, 0); } break;
        case OU:     { OneULong *d    = mseq[i]; keyval = 0;         seq = d->seq; size = topic_payload_size (topicsel, 0); } break;
        case UK16:   { Unkeyed16 *d   = mseq[i]; keyval = 0;         seq = d->seq; size = topic_payload_size (topicsel, 0); } break;
        case UK1024: { Unkeyed1024 *d = mseq[i]; keyval = 0;         seq = d->seq; size = topic_payload_size (topicsel, 0); } break;
      }
      (void) check_eseq (&eseq_admin, seq, keyval, size, iseq[i].publication_handle);
      if (iseq[i].source_timestamp & 1)
      {
        dds_entity_t wr_pong;
        if ((wr_pong = get_pong_writer (iseq[i].publication_handle)) != 0)
        {
          dds_return_t rc;
          if ((rc = dds_write_ts (wr_pong, mseq[i], iseq[i].source_timestamp - 1)) < 0 && rc != DDS_RETCODE_TIMEOUT)
            error2 ("dds_write_ts (wr_pong, mseq[i], iseq[i].source_timestamp): %d\n", (int) rc);
          dds_write_flush (wr_pong);
        }
      }
    }
  }
  return (nread_data > 0);
}

static bool process_ping (dds_entity_t rd, struct subthread_arg *arg)
{
  /* Ping sends back Pongs with the lsb 1; Data sends back Pongs with the lsb 0.  This way, the Pong handler can
     figure out whether to Ping again or not by looking at the lsb.  If it is 1, another Ping is required */
  uint32_t max_samples = arg->max_samples;
  dds_sample_info_t *iseq = arg->iseq;
  void **mseq = arg->mseq;
  int32_t nread_ping;
  if ((nread_ping = dds_take (rd, mseq, iseq, max_samples, max_samples)) < 0)
    error2 ("dds_take (rd_data): %d\n", (int) nread_ping);
  for (int32_t i = 0; i < nread_ping; i++)
  {
    if (iseq[i].valid_data)
    {
      dds_entity_t wr_pong;
      if ((wr_pong = get_pong_writer (iseq[i].publication_handle)) != 0)
      {
        dds_return_t rc;
        if ((rc = dds_write_ts (wr_pong, mseq[i], iseq[i].source_timestamp | 1)) < 0 && rc != DDS_RETCODE_TIMEOUT)
          error2 ("dds_write_ts (wr_pong, mseq[i], iseq[i].source_timestamp): %d\n", (int) rc);
        dds_write_flush (wr_pong);
      }
    }
  }
  return (nread_ping > 0);
}

static bool process_pong (dds_entity_t rd, struct subthread_arg *arg)
{
  uint32_t max_samples = arg->max_samples;
  dds_sample_info_t *iseq = arg->iseq;
  void **mseq = arg->mseq;
  int32_t nread_pong;
  if ((nread_pong = dds_take (rd, mseq, iseq, max_samples, max_samples)) < 0)
    error2 ("dds_take (rd_pong): %d\n", (int) nread_pong);
  else if (nread_pong > 0)
  {
    dds_time_t tnow = dds_time ();
    for (int32_t i = 0; i < nread_pong; i++)
      if (iseq[i].valid_data)
      {
        uint32_t * const seq = mseq[i];
        const bool isping = (iseq[i].source_timestamp & 1) != 0;
        const bool all = update_roundtrip (iseq[i].publication_handle, (uint64_t) (tnow - iseq[i].source_timestamp) / 2, isping, *seq);
        if (isping && all && ping_intv == 0)
        {
          /* If it is a pong sent in response to a ping, and all known nodes have responded, send out a new ping */
          dds_return_t rc;
          ddsrt_mutex_lock (&pongwr_lock);
          n_pong_seen = 0;
          cur_ping_time = dds_time ();
          cur_ping_seq = ++(*seq);
          ddsrt_mutex_unlock (&pongwr_lock);
          if ((rc = dds_write_ts (wr_ping, mseq[i], dds_time () | 1)) < 0 && rc != DDS_RETCODE_TIMEOUT)
            error2 ("dds_write (wr_ping, mseq[i]): %d\n", (int) rc);
          dds_write_flush (wr_ping);
        }
      }
  }
  return (nread_pong > 0);
}

static void maybe_send_new_ping (dds_time_t tnow, dds_time_t *tnextping)
{
  void *baggage;
  union data data;
  int32_t rc;
  assert (ping_intv != DDS_INFINITY);
  ddsrt_mutex_lock (&pongwr_lock);
  if (tnow < cur_ping_time + (ping_intv == 0 ? DDS_SECS (1) : ping_intv))
  {
    if (ping_intv == 0)
      *tnextping = cur_ping_time + DDS_SECS (1);
    ddsrt_mutex_unlock (&pongwr_lock);
  }
  else
  {
    if (n_pong_seen < n_pong_expected)
    {
      ping_timeouts++;
      if (tnow > twarn_ping_timeout)
      {
        printf ("[%"PRIdPID"] ping timed out (total %"PRIu32" times) ... sending new ping\n", ddsrt_getpid (), ping_timeouts);
        twarn_ping_timeout = tnow + DDS_SECS (1);
        fflush (stdout);
      }
    }
    n_pong_seen = 0;
    if (ping_intv == 0)
    {
      *tnextping = tnow + DDS_SECS (1);
      cur_ping_time = tnow;
    }
    else
    {
      /* tnow should be ~ cur_ping_time + ping_intv, but it won't be if the
         wakeup was delayed significantly, the machine was suspended in the
         meantime, so slow down if we can't keep up */
      cur_ping_time += ping_intv;
      if (cur_ping_time < tnow - ping_intv / 2)
        cur_ping_time = tnow;
      *tnextping = cur_ping_time + ping_intv;
    }
    cur_ping_seq++;
    baggage = init_sample (&data, cur_ping_seq);
    ddsrt_mutex_unlock (&pongwr_lock);
    if ((rc = dds_write_ts (wr_ping, &data, dds_time () | 1)) < 0 && rc != DDS_RETCODE_TIMEOUT)
      error2 ("send_new_ping: dds_write (wr_ping, &data): %d\n", (int) rc);
    dds_write_flush (wr_ping);
    if (baggage)
      free (baggage);
  }
}

static uint32_t subthread_waitset (void *varg)
{
  struct subthread_arg * const arg = varg;
  dds_entity_t ws;
  int32_t rc;
  ws = dds_create_waitset (dp);
  if ((rc = dds_waitset_attach (ws, termcond, 0)) < 0)
    error2 ("dds_waitset_attach (termcond, 0): %d\n", (int) rc);
  if ((rc = dds_set_status_mask (rd_data, DDS_DATA_AVAILABLE_STATUS)) < 0)
    error2 ("dds_set_status_mask (rd_data, DDS_DATA_AVAILABLE_STATUS): %d\n", (int) rc);
  if ((rc = dds_waitset_attach (ws, rd_data, 1)) < 0)
    error2 ("dds_waitset_attach (ws, rd_data, 1): %d\n", (int) rc);
  while (!ddsrt_atomic_ld32 (&termflag))
  {
    if (!process_data (rd_data, arg))
    {
      /* when we use DATA_AVAILABLE, we must read until nothing remains, or we would deadlock
         if more than max_samples were available and nothing further is received */
      int32_t nxs;
      if ((nxs = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY)) < 0)
        error2 ("dds_waitset_wait: %d\n", (int) nxs);
    }
  }
  return 0;
}

static uint32_t subpingthread_waitset (void *varg)
{
  struct subthread_arg * const arg = varg;
  dds_entity_t ws;
  int32_t rc;
  ws = dds_create_waitset (dp);
  if ((rc = dds_waitset_attach (ws, termcond, 0)) < 0)
    error2 ("dds_waitset_attach (termcond, 0): %d\n", (int) rc);
  if ((rc = dds_set_status_mask (rd_ping, DDS_DATA_AVAILABLE_STATUS)) < 0)
    error2 ("dds_set_status_mask (rd_ping, DDS_DATA_AVAILABLE_STATUS): %d\n", (int) rc);
  if ((rc = dds_waitset_attach (ws, rd_ping, 1)) < 0)
    error2 ("dds_waitset_attach (ws, rd_ping, 1): %d\n", (int) rc);
  while (!ddsrt_atomic_ld32 (&termflag))
  {
    int32_t nxs;
    if ((nxs = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY)) < 0)
      error2 ("dds_waitset_wait: %d\n", (int) nxs);
    process_ping (rd_ping, arg);
  }
  return 0;
}

static uint32_t subpongthread_waitset (void *varg)
{
  struct subthread_arg * const arg = varg;
  dds_entity_t ws;
  int32_t rc;
  ws = dds_create_waitset (dp);
  if ((rc = dds_waitset_attach (ws, termcond, 0)) < 0)
    error2 ("dds_waitset_attach (termcond, 0): %d\n", (int) rc);
  if ((rc = dds_set_status_mask (rd_pong, DDS_DATA_AVAILABLE_STATUS)) < 0)
    error2 ("dds_set_status_mask (rd_pong, DDS_DATA_AVAILABLE_STATUS): %d\n", (int) rc);
  if ((rc = dds_waitset_attach (ws, rd_pong, 1)) < 0)
    error2 ("dds_waitset_attach (ws, rd_pong, 1): %d\n", (int) rc);
  while (!ddsrt_atomic_ld32 (&termflag))
  {
    int32_t nxs;
    if ((nxs = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY)) < 0)
      error2 ("dds_waitset_wait: %d\n", (int) nxs);
    process_pong (rd_pong, arg);
  }
  return 0;
}

static uint32_t subthread_polling (void *varg)
{
  struct subthread_arg * const arg = varg;
  while (!ddsrt_atomic_ld32 (&termflag))
  {
    if (!process_data (rd_data, arg))
      dds_sleepfor (DDS_MSECS (1));
  }
  return 0;
}

static void data_available_listener (dds_entity_t rd, void *arg)
{
  process_data (rd, arg);
}

static void ping_available_listener (dds_entity_t rd, void *arg)
{
  process_ping (rd, arg);
}

static void pong_available_listener (dds_entity_t rd, void *arg)
{
  process_pong (rd, arg);
}

static dds_entity_t create_pong_writer (dds_instance_handle_t pphandle, const struct guidstr *guidstr)
{
  dds_qos_t *qos;
  dds_listener_t *listener;
  dds_entity_t pongpub;
  dds_entity_t wr_pong;

  //printf ("[%"PRIdPID"] create_pong_writer: creating writer in partition %s pubhandle %"PRIx64"\n", ddsrt_getpid (), guidstr->str, pphandle);
  //fflush (stdout);

  qos = dds_create_qos ();
  dds_qset_partition1 (qos, guidstr->str);
  if ((pongpub = dds_create_publisher (dp, qos, NULL)) < 0)
    error2 ("dds_create_publisher failed: %d\n", (int) pongpub);
  dds_delete_qos (qos);

  listener = dds_create_listener ((void *) (uintptr_t) MM_RD_PONG);
  dds_lset_publication_matched (listener, publication_matched_listener);
  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, 1);
  dds_qset_ignorelocal (qos, ignorelocal);
  if ((wr_pong = dds_create_writer (pongpub, tp_pong, qos, listener)) < 0)
    error2 ("dds_create_writer(%s) failed: %d\n", tpname_pong, (int) wr_pong);
  dds_delete_qos (qos);
  dds_delete_listener (listener);

  ddsrt_mutex_lock (&pongwr_lock);
  pongwr = realloc (pongwr, (npongwr + 1) * sizeof (*pongwr));
  pongwr[npongwr].pubhandle = 0;
  pongwr[npongwr].pphandle = pphandle;
  pongwr[npongwr].wr_pong = wr_pong;
  npongwr++;
  ddsrt_mutex_unlock (&pongwr_lock);
  return wr_pong;
}

static void delete_pong_writer (dds_instance_handle_t pphandle)
{
  uint32_t i = 0;
  dds_entity_t wr_pong = 0;
  ddsrt_mutex_lock (&pongwr_lock);
  while (i < npongwr)
  {
    if (pongwr[i].pphandle != pphandle)
      i++;
    else
    {
      assert (wr_pong == 0 || wr_pong == pongwr[i].wr_pong);
      memmove (&pongwr[i], &pongwr[i+1], (npongwr - i - 1) * sizeof (pongwr[0]));
      npongwr--;
    }
  }
  ddsrt_mutex_unlock (&pongwr_lock);
  if (wr_pong)
    dds_delete (wr_pong);
}

static void free_ppant (void *vpp)
{
  struct ppant *pp = vpp;
  free (pp->hostname);
  free (pp);
}

static void participant_data_listener (dds_entity_t rd, void *arg)
{
  dds_sample_info_t info;
  void *msg = NULL;
  uint32_t n_pong_expected_delta = 0;
  int32_t n;
  (void) arg;
  while ((n = dds_take (rd, &msg, &info, 1, 1)) > 0)
  {
    struct ppant *pp;
    assert (info.instance_state != DDS_ALIVE_INSTANCE_STATE || info.valid_data);
    if (info.instance_state != DDS_ALIVE_INSTANCE_STATE)
    {
      ddsrt_avl_dpath_t dpath;
      ddsrt_mutex_lock (&disc_lock);
      if ((pp = ddsrt_avl_lookup_dpath (&ppants_td, &ppants, &info.instance_handle, &dpath)) != NULL)
      {
        printf ("[%"PRIdPID"] participant %s:%"PRIu32": gone\n", ddsrt_getpid (), pp->hostname, pp->pid);
        fflush (stdout);

        if (pp->handle != dp_handle || ignorelocal == DDS_IGNORELOCAL_NONE)
        {
          delete_pong_writer (pp->handle);
          n_pong_expected_delta--;
        }

        ddsrt_avl_delete_dpath (&ppants_td, &ppants, pp, &dpath);
        if (pp->tdeadline != DDS_NEVER)
          ddsrt_fibheap_delete (&ppants_to_match_fhd, &ppants_to_match, pp);
        free_ppant (pp);
      }
      ddsrt_mutex_unlock (&disc_lock);
    }
    else
    {
      const dds_builtintopic_participant_t *sample = msg;
      void *vudata;
      size_t usz;
      ddsrt_avl_ipath_t ipath;
      /* only add unknown participants with the magic user_data value: DDSPerf:X:HOSTNAME, where X is decimal  */
      if (dds_qget_userdata (sample->qos, &vudata, &usz) && usz > 0)
      {
        const char *udata = vudata;
        int has_reader, pos;
        long pid;
        if (sscanf (udata, UDATA_MAGIC "%d:%ld%n", &has_reader, &pid, &pos) == 2 && udata[pos] == ':' && strlen (udata + pos) == usz - (unsigned) pos)
        {
          size_t sz = usz - (unsigned) pos;
          char *hostname = malloc (sz);
          memcpy (hostname, udata + pos + 1, sz);
          ddsrt_mutex_lock (&disc_lock);
          if ((pp = ddsrt_avl_lookup_ipath (&ppants_td, &ppants, &info.instance_handle, &ipath)) != NULL)
            free (hostname);
          else
          {
            printf ("[%"PRIdPID"] participant %s:%"PRIu32": new%s\n", ddsrt_getpid (), hostname, (uint32_t) pid, (info.instance_handle == dp_handle) ? " (self)" : "");
            pp = malloc (sizeof (*pp));
            pp->handle = info.instance_handle;
            pp->guid = sample->key;
            pp->hostname = hostname;
            pp->pid = (uint32_t) pid;
            pp->tdisc = dds_time ();
            pp->tdeadline = pp->tdisc + DDS_SECS (5);
            if (pp->handle != dp_handle || ignorelocal == DDS_IGNORELOCAL_NONE)
              pp->unmatched = MM_ALL & ~(has_reader ? 0 : MM_RD_DATA) & ~(rd_data ? 0 : MM_WR_DATA);
            else
              pp->unmatched = 0;
            ddsrt_fibheap_insert (&ppants_to_match_fhd, &ppants_to_match, pp);
            ddsrt_avl_insert_ipath (&ppants_td, &ppants, pp, &ipath);

            if (pp->handle != dp_handle || ignorelocal == DDS_IGNORELOCAL_NONE)
            {
              struct guidstr guidstr;
              make_guidstr (&guidstr, &sample->key);
              create_pong_writer (pp->handle, &guidstr);
              n_pong_expected_delta++;
            }
          }
          ddsrt_mutex_unlock (&disc_lock);
        }
        dds_free (vudata);
      }
    }
    dds_return_loan (rd, &msg, n);
  }
  if (n < 0)
    error2 ("dds_take(rd_participants): error %d\n", (int) n);

  if (n_pong_expected_delta)
  {
    ddsrt_mutex_lock (&pongwr_lock);
    n_pong_expected += n_pong_expected_delta;
    /* potential initial packet loss & lazy writer creation conspire against receiving
       the expected number of responses, so allow for a few attempts before starting to
       warn about timeouts */
    twarn_ping_timeout = dds_time () + DDS_MSECS (3333);
    //printf ("[%"PRIdPID"] n_pong_expected = %u\n", ddsrt_getpid (), n_pong_expected);
    ddsrt_mutex_unlock (&pongwr_lock);
  }
}

static void endpoint_matched_listener (uint32_t match_mask, dds_entity_t rd_epinfo, dds_instance_handle_t remote_endpoint)
{
  dds_sample_info_t info;
  void *msg = NULL;
  int32_t n;

  /* update participant data so this remote endpoint's participant will be known */
  participant_data_listener (rd_participants, NULL);

  /* FIXME: implement the get_matched_... interfaces so there's no need for keeping a reader
     (and having to GC it, which I'm skipping here ...) */
  if ((n = dds_read_instance (rd_epinfo, &msg, &info, 1, 1, remote_endpoint)) < 0)
    error2 ("dds_read_instance(rd_epinfo, %"PRIx64") failed: %d\n", remote_endpoint, (int) n);
  else if (n == 0)
    printf ("[%"PRIdPID"] endpoint %"PRIx64" not found\n", ddsrt_getpid (), remote_endpoint);
  else
  {
    if (info.valid_data)
    {
      const dds_builtintopic_endpoint_t *sample = msg;
      struct ppant *pp;
      ddsrt_mutex_lock (&disc_lock);
      if ((pp = ddsrt_avl_lookup (&ppants_td, &ppants, &sample->participant_instance_handle)) == NULL)
        printf ("[%"PRIdPID"] participant %"PRIx64" no longer exists\n", ddsrt_getpid (), sample->participant_instance_handle);
      else
      {
        pp->unmatched &= ~match_mask;
        if (pp->unmatched == 0)
          matchcount++;
      }
      ddsrt_mutex_unlock (&disc_lock);
    }
    dds_return_loan (rd_epinfo, &msg, n);
  }
  fflush (stdout);
}

static const char *match_mask1_to_string (uint32_t mask)
{
  assert ((mask & ~MM_ALL) == 0);
  switch (mask)
  {
    case MM_WR_DATA: return "data writer";
    case MM_RD_DATA: return "data reader";
    case MM_WR_PING: return "ping writer";
    case MM_RD_PING: return "ping reader";
    case MM_WR_PONG: return "pong writer";
    case MM_RD_PONG: return "pong reader";
  }
  return "?";
}

static char *match_mask_to_string (char *buf, size_t size, uint32_t mask)
{
  size_t pos = 0;
  while (pos < size && mask != 0)
  {
    uint32_t mask1 = mask & (~mask + 1u);
    mask &= ~mask1;
    int n = snprintf (buf + pos, size - (unsigned) pos, "%s%s", (pos > 0) ? ", " : "", match_mask1_to_string (mask1));
    if (n >= 0) pos += (size_t) n;
  }
  return buf;
}

static void subscription_matched_listener (dds_entity_t rd, const dds_subscription_matched_status_t status, void *arg)
{
  /* this only works because the listener is called for every match; but I don't think that is something the
     spec guarantees, and I don't think Cyclone should guarantee that either -- and if it isn't guaranteed
     _really_ needs the get_matched_... interfaces to not have to implement the matching logic ... */
  (void) rd;
  if (status.current_count_change > 0)
  {
    uint32_t mask = (uint32_t) (uintptr_t) arg;
    //printf ("[%"PRIdPID"] subscription match: %s\n", ddsrt_getpid (), match_mask1_to_string (mask));
    endpoint_matched_listener (mask, rd_publications, status.last_publication_handle);
  }
}

static void publication_matched_listener (dds_entity_t wr, const dds_publication_matched_status_t status, void *arg)
{
  /* this only works because the listener is called for every match; but I don't think that is something the
   spec guarantees, and I don't think Cyclone should guarantee that either -- and if it isn't guaranteed
   _really_ needs the get_matched_... interfaces to not have to implement the matching logic ... */
  (void) wr;
  if (status.current_count_change > 0)
  {
    uint32_t mask = (uint32_t) (uintptr_t) arg;
    //printf ("[%"PRIdPID"] publication match: %s\n", ddsrt_getpid (), match_mask1_to_string (mask));
    endpoint_matched_listener (mask, rd_subscriptions, status.last_subscription_handle);
  }
}

static void set_data_available_listener (dds_entity_t rd, const char *rd_name, dds_on_data_available_fn fn, void *arg)
{
  /* This convoluted code is so that we leave all listeners unchanged, except the
   data_available one.  There is no real need for these complications, but it is
   a nice exercise. */
  dds_listener_t *listener = dds_create_listener (arg);
  dds_return_t rc;
  dds_lset_data_available (listener, fn);
  dds_listener_t *tmplistener = dds_create_listener (NULL);
  if ((rc = dds_get_listener (rd, tmplistener)) < 0)
    error2 ("dds_get_listener(%s) failed: %d\n", rd_name, (int) rc);
  dds_merge_listener (listener, tmplistener);
  dds_delete_listener (tmplistener);

  if ((rc = dds_set_listener (rd, listener)) < 0)
    error2 ("dds_set_listener(%s) failed: %d\n", rd_name, (int) rc);
  dds_delete_listener (listener);
}

static int cmp_uint64 (const void *va, const void *vb)
{
  const uint64_t *a = va;
  const uint64_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static void print_stats (dds_time_t tref, dds_time_t tnow, dds_time_t tprev, struct record_cputime_state *cputime_state, struct record_netload_state *netload_state)
{
  char prefix[128];
  const double ts = (double) (tnow - tref) / 1e9;
  bool output = false;
  snprintf (prefix, sizeof (prefix), "[%"PRIdPID"] %.3f ", ddsrt_getpid (), ts);

  if (pub_rate > 0)
  {
    ddsrt_mutex_lock (&pubstat_lock);
    hist_print (prefix, pubstat_hist, tnow - tprev, 1);
    ddsrt_mutex_unlock (&pubstat_lock);
    output = true;
  }

  if (submode != SM_NONE)
  {
    struct eseq_admin * const ea = &eseq_admin;
    uint64_t tot_nrecv = 0, tot_nlost = 0, nrecv = 0, nrecv_bytes = 0, nlost = 0;
    uint32_t last_size = 0;
    ddsrt_mutex_lock (&ea->lock);
    for (uint32_t i = 0; i < ea->nph; i++)
    {
      struct eseq_stat * const x = &ea->stats[i];
      tot_nrecv += x->nrecv;
      tot_nlost += x->nlost;
      nrecv += x->nrecv - x->nrecv_ref;
      nlost += x->nlost - x->nlost_ref;
      nrecv_bytes += x->nrecv_bytes - x->nrecv_bytes_ref;
      last_size = x->last_size;
      x->nrecv_ref = x->nrecv;
      x->nlost_ref = x->nlost;
      x->nrecv_bytes_ref = x->nrecv_bytes;
    }
    ddsrt_mutex_unlock (&ea->lock);

    if (nrecv > 0)
    {
      const double dt = (double) (tnow - tprev);
      printf ("%s size %"PRIu32" total %"PRIu64" lost %"PRIu64" delta %"PRIu64" lost %"PRIu64" rate %.2f kS/s %.2f Mb/s\n",
              prefix, last_size, tot_nrecv, tot_nlost, nrecv, nlost,
              (double) nrecv * 1e6 / dt, (double) nrecv_bytes * 8 * 1e3 / dt);
      output = true;
    }
  }

  uint64_t *newraw = malloc (PINGPONG_RAWSIZE * sizeof (*newraw));
  ddsrt_mutex_lock (&pongstat_lock);
  for (uint32_t i = 0; i < npongstat; i++)
  {
    struct subthread_arg_pongstat * const x = &pongstat[i];
    struct subthread_arg_pongstat y = *x;
    x->raw = newraw;
    x->min = UINT64_MAX;
    x->max = x->sum = x->cnt = 0;
    /* pongstat entries get added at the end, npongstat only grows: so can safely
       unlock the stats in between nodes for calculating percentiles */
    ddsrt_mutex_unlock (&pongstat_lock);

    if (y.cnt > 0)
    {
      const uint32_t rawcnt = (y.cnt > PINGPONG_RAWSIZE) ? PINGPONG_RAWSIZE : y.cnt;
      char ppinfo[128];
      struct ppant *pp;
      ddsrt_mutex_lock (&disc_lock);
      if ((pp = ddsrt_avl_lookup (&ppants_td, &ppants, &y.pphandle)) == NULL)
        snprintf (ppinfo, sizeof (ppinfo), "%"PRIx64, y.pubhandle);
      else
        snprintf (ppinfo, sizeof (ppinfo), "%s:%"PRIu32, pp->hostname, pp->pid);
      ddsrt_mutex_unlock (&disc_lock);

      qsort (y.raw, rawcnt, sizeof (*y.raw), cmp_uint64);
      printf ("%s %s size %"PRIu32" mean %.3fus min %.3fus 50%% %.3fus 90%% %.3fus 99%% %.3fus max %.3fus cnt %"PRIu32"\n",
              prefix, ppinfo, topic_payload_size (topicsel, baggagesize),
              (double) y.sum / (double) y.cnt / 1e3,
              (double) y.min / 1e3,
              (double) y.raw[rawcnt - (rawcnt + 1) / 2] / 1e3,
              (double) y.raw[rawcnt - (rawcnt + 9) / 10] / 1e3,
              (double) y.raw[rawcnt - (rawcnt + 99) / 100] / 1e3,
              (double) y.max / 1e3,
              y.cnt);
      output = true;
    }
    newraw = y.raw;

    ddsrt_mutex_lock (&pongstat_lock);
  }
  ddsrt_mutex_unlock (&pongstat_lock);
  free (newraw);

  if (record_cputime (cputime_state, prefix, tnow))
    output = true;

  if (rd_stat)
  {
#define MAXS 40 /* 40 participants is enough for everyone! */
    void *raw[MAXS];
    dds_sample_info_t si[MAXS];
    int32_t n;
    /* Read everything using a keep-last-1 reader: effectively latching the
       most recent value.  While not entirely correct, the nature of the process
       is such that things should be stable, and this allows printing the stats
       always in the same way despite the absence of synchronization. */
    raw[0] = NULL;
    if ((n = dds_take_mask (rd_stat, raw, si, MAXS, MAXS, DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE | DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE)) > 0)
    {
      for (int32_t i = 0; i < n; i++)
        if (si[i].valid_data && si[i].sample_state == DDS_SST_NOT_READ)
          if (print_cputime (raw[i], prefix, true, true))
            output = true;
      dds_return_loan (rd_stat, raw, n);
    }
    if ((n = dds_read (rd_stat, raw, si, MAXS, MAXS)) > 0)
    {
      for (int32_t i = 0; i < n; i++)
        if (si[i].valid_data)
          if (print_cputime (raw[i], prefix, true, si[i].sample_state == DDS_SST_NOT_READ))
            output = true;
      dds_return_loan (rd_stat, raw, n);
    }
#undef MAXS
  }

  if (output)
    record_netload (netload_state, prefix, tnow);
  fflush (stdout);
}

static void subthread_arg_init (struct subthread_arg *arg, dds_entity_t rd, uint32_t max_samples)
{
  arg->rd = rd;
  arg->max_samples = max_samples;
  arg->mseq = malloc (arg->max_samples * sizeof (arg->mseq[0]));
  arg->iseq = malloc (arg->max_samples * sizeof (arg->iseq[0]));
  for (uint32_t i = 0; i < arg->max_samples; i++)
    arg->mseq[i] = NULL;
}

static void subthread_arg_fini (struct subthread_arg *arg)
{
  dds_return_loan(arg->rd, arg->mseq, (int32_t) arg->max_samples);
  free (arg->mseq);
  free (arg->iseq);
}

#if !DDSRT_WITH_FREERTOS
static void signal_handler (int sig)
{
  (void) sig;
  ddsrt_atomic_st32 (&termflag, 1);
  dds_set_guardcondition (termcond, true);
}
#endif

#if !_WIN32 && !DDSRT_WITH_FREERTOS
static uint32_t sigthread (void *varg)
{
  sigset_t *set = varg;
  int sig;
  if (sigwait (set, &sig) == 0)
    signal_handler (sig);
  else
    error2 ("sigwait failed: %d\n", errno);
  return 0;
}

#if defined __APPLE__ || defined __linux
static void sigxfsz_handler (int sig __attribute__ ((unused)))
{
  static const char msg[] = "file size limit reached\n";
  static ddsrt_atomic_uint32_t seen = DDSRT_ATOMIC_UINT32_INIT (0);
  if (!ddsrt_atomic_or32_ov (&seen, 1))
  {
    dds_time_t tnow = dds_time ();
    if (write (2, msg, sizeof (msg) - 1) < 0) {
      /* may not ignore return value according to Linux/gcc */
    }
    print_stats (0, tnow, tnow - DDS_SECS (1), NULL, NULL);
    kill (getpid (), 9);
  }
}
#endif
#endif

/********************
 COMMAND LINE PARSING
 ********************/

static void usage (void)
{
  printf ("\
%s help                (this text)\n\
%s sanity              (ping 1Hz)\n\
%s [OPTIONS] MODE...\n\
\n\
OPTIONS:\n\
  -L                  allow matching with endpoints in the same process\n\
                      to get throughput/latency in the same ddsperf process\n\
  -T KS|K32|K256|OU   topic (KS is default):\n\
                        KS   seq num, key value, sequence-of-octets\n\
                        K32  seq num, key value, array of 24 octets\n\
                        K256 seq num, key value, array of 248 octets\n\
                        OU   seq num\n\
  -n N                number of key values to use for data (only for\n\
                      topics with a key value)\n\
  -u                  best-effort instead of reliable\n\
  -k all|N            keep-all or keep-last-N for data (ping/pong is\n\
                      always keep-last-1)\n\
  -c                  subscribe to CPU stats from peers and show them\n\
  -d DEV:BW           report network load for device DEV with nominal\n\
                      bandwidth BW in bits/s (e.g., eth0:1e9)\n\
  -D DUR              run for at most DUR seconds\n\
  -N COUNT            require at least COUNT matching participants\n\
  -M DUR              require those participants to match within DUR seconds\n\
  -R TREF             timestamps in the output relative to TREF instead of\n\
                      process start\n\
  -i ID               use domain ID instead of the default domain\n\
\n\
MODE... is zero or more of:\n\
  ping [R[Hz]] [size S] [waitset|listener]\n\
    Send a ping upon receiving all expected pongs, or send a ping at\n\
    rate R (optionally suffixed with Hz/kHz).  The triggering mode is either\n\
    a listener (default, unless -L has been specified) or a waitset.\n\
  pong [waitset|listener]\n\
    A \"dummy\" mode that serves two purposes: configuring the triggering.\n\
    mode (but it is shared with ping's mode), and suppressing the 1Hz ping\n\
    if no other options are selected.  It always responds to pings.\n\
  sub [waitset|listener|polling]\n\
    Subscribe to data, with calls to take occurring either in a listener\n\
    (default), when a waitset is triggered, or by polling at 1kHz.\n\
  pub [R[Hz]] [size S] [burst N] [[ping] X%%]\n\
    Publish bursts of data at rate R, optionally suffixed with Hz/kHz.  If\n\
    no rate is given or R is \"inf\", data is published as fast as\n\
    possible.  Each burst is a single sample by default, but can be set\n\
    to larger value using \"burst N\".  Sample size is controlled using\n\
    \"size S\", S may be suffixed with k/M/kB/MB/KiB/MiB.\n\
    If desired, a fraction of the samples can be treated as if it were a\n\
    ping, for this, specify a percentage either as \"ping X%%\" (the\n\
    \"ping\" keyword is optional, the %% sign is not).\n\
\n\
  Payload size (including fixed part of topic) may be set as part of a\n\
  \"ping\" or \"pub\" specification for topic KS (there is only size,\n\
  the last one given determines it for all) and should be either 0 (minimal,\n\
  equivalent to 12) or >= 12.\n\
\n\
EXIT STATUS:\n\
\n\
  0  all is well\n\
  1  not enough peers discovered, other matching issues, unexpected sample\n\
     loss detected\n\
  2  unexpected failure of some DDS operation\n\
  3  incorrect arguments\n\
\n\
EXAMPLES:\n\
  ddsperf pub size 1k & ddsperf sub\n\
    basic throughput test with 1024-bytes large samples\n\
  ddsperf ping & ddsperf pong\n\
    basic latency test\n\
  ddsperf -L -TOU -D10 pub sub\n\
    basic throughput test within the process with tiny, keyless samples,\n\
    running for 10s\n\
", argv0, argv0, argv0);
  fflush (stdout);
  exit (3);
}

struct string_int_map_elem {
  const char *name;
  int value;
};

static const struct string_int_map_elem modestrings[] = {
  { "ping", 1 },
  { "pong", 2 },
  { "sub", 3 },
  { "pub", 4 },
  { NULL, 0 }
};

static const struct string_int_map_elem pingpongmodes[] = {
  { "waitset", SM_WAITSET },
  { "listener", SM_LISTENER },
  { NULL, 0 }
};

static int exact_string_int_map_lookup (const struct string_int_map_elem *elems, const char *label, const char *str, bool notfound_error)
{
  for (size_t i = 0; elems[i].name; i++)
    if (strcmp (elems[i].name, str) == 0)
      return elems[i].value;
  if (notfound_error)
    error3 ("%s: undefined %s\n", str, label);
  return -1;
}

static int string_int_map_lookup (const struct string_int_map_elem *elems, const char *label, const char *str, bool notfound_error)
{
  size_t match = SIZE_MAX;
  size_t len = strlen (str);
  bool ambiguous = false;
  for (size_t i = 0; elems[i].name; i++)
  {
    if (strcmp (elems[i].name, str) == 0)
      return elems[i].value;
    else if (len >= 3 && strlen (elems[i].name) >= 3 && strncmp (elems[i].name, str, len) == 0)
    {
      if (match == SIZE_MAX)
        match = i;
      else
        ambiguous = true;
    }
  }
  if (ambiguous)
    error3 ("%s: ambiguous %sspecification\n", str, label);
  if (match == SIZE_MAX && notfound_error)
    error3 ("%s: undefined %s\n", str, label);
  return (match == SIZE_MAX) ? -1 : elems[match].value;
}

struct multiplier {
  const char *suffix;
  int mult;
};

static const struct multiplier frequency_units[] = {
  { "Hz", 1 },
  { "kHz", 1000 },
  { NULL, 0 }
};

static const struct multiplier size_units[] = {
  { "B", 1 },
  { "k", 1024 },
  { "M", 1048576 },
  { "kB", 1024 },
  { "KiB", 1024 },
  { "MB", 1048576 },
  { "MiB", 1048576 },
  { NULL, 0 }
};

static int lookup_multiplier (const struct multiplier *units, const char *suffix)
{
  while (*suffix == ' ')
    suffix++;
  if (*suffix == 0)
    return 1;
  else if (units == NULL)
    return 0;
  else
  {
    for (size_t i = 0; units[i].suffix; i++)
      if (strcmp (units[i].suffix, suffix) == 0)
        return units[i].mult;
    return 0;
  }
}

static bool set_simple_uint32 (int *xoptind, int xargc, char * const xargv[], const char *token, const struct multiplier *units, uint32_t *val)
{
  if (strcmp (xargv[*xoptind], token) != 0)
    return false;
  else
  {
    unsigned x;
    int pos, mult;
    if (++(*xoptind) == xargc)
      error3 ("argument missing in %s specification\n", token);
    if (sscanf (xargv[*xoptind], "%u%n", &x, &pos) == 1 && (mult = lookup_multiplier (units, xargv[*xoptind] + pos)) > 0)
      *val = x * (unsigned) mult;
    else
      error3 ("%s: invalid %s specification\n", xargv[*xoptind], token);
    return true;
  }
}

static void set_mode_ping (int *xoptind, int xargc, char * const xargv[])
{
  ping_intv = 0;
  pingpongmode = SM_LISTENER;
  while (*xoptind < xargc && exact_string_int_map_lookup (modestrings, "mode string", xargv[*xoptind], false) == -1)
  {
    int pos = 0, mult = 1;
    double ping_rate;
    if (strcmp (xargv[*xoptind], "inf") == 0 && lookup_multiplier (frequency_units, xargv[*xoptind] + 3) > 0)
    {
      ping_intv = 0;
    }
    else if (sscanf (xargv[*xoptind], "%lf%n", &ping_rate, &pos) == 1 && (mult = lookup_multiplier (frequency_units, xargv[*xoptind] + pos)) > 0)
    {
      ping_rate *= mult;
      if (ping_rate == 0) ping_intv = DDS_INFINITY;
      else if (ping_rate > 0) ping_intv = (dds_duration_t) (1e9 / ping_rate + 0.5);
      else error3 ("%s: invalid ping rate\n", xargv[*xoptind]);
    }
    else if (set_simple_uint32 (xoptind, xargc, xargv, "size", size_units, &baggagesize))
    {
      /* no further work needed */
    }
    else
    {
      pingpongmode = (enum submode) string_int_map_lookup (pingpongmodes, "ping mode", xargv[*xoptind], true);
    }
    (*xoptind)++;
  }
}

static void set_mode_pong (int *xoptind, int xargc, char * const xargv[])
{
  pingpongmode = SM_LISTENER;
  while (*xoptind < xargc && exact_string_int_map_lookup (modestrings, "mode string", xargv[*xoptind], false) == -1)
  {
    pingpongmode = (enum submode) string_int_map_lookup (pingpongmodes, "pong mode", xargv[*xoptind], true);
    (*xoptind)++;
  }
}

static void set_mode_sub (int *xoptind, int xargc, char * const xargv[])
{
  static const struct string_int_map_elem submodes[] = {
    { "waitset", SM_WAITSET },
    { "polling", SM_POLLING },
    { "listener", SM_LISTENER },
    { NULL, 0 }
  };
  submode = SM_LISTENER;
  while (*xoptind < xargc && exact_string_int_map_lookup (modestrings, "mode string", xargv[*xoptind], false) == -1)
  {
    submode = (enum submode) string_int_map_lookup (submodes, "subscription mode", xargv[*xoptind], true);
    (*xoptind)++;
  }
}

static void set_mode_pub (int *xoptind, int xargc, char * const xargv[])
{
  pub_rate = HUGE_VAL;
  burstsize = 1;
  ping_frac = 0;
  while (*xoptind < xargc && exact_string_int_map_lookup (modestrings, "mode string", xargv[*xoptind], false) == -1)
  {
    int pos = 0, mult = 1;
    double r;
    if (strncmp (xargv[*xoptind], "inf", 3) == 0 && lookup_multiplier (frequency_units, xargv[*xoptind] + 3) > 0)
    {
      pub_rate = HUGE_VAL;
    }
    else if (sscanf (xargv[*xoptind], "%lf%n", &r, &pos) == 1 && (mult = lookup_multiplier (frequency_units, xargv[*xoptind] + pos)) > 0)
    {
      if (r < 0) error3 ("%s: invalid publish rate\n", xargv[*xoptind]);
      pub_rate = r * mult;
    }
    else if (set_simple_uint32 (xoptind, xargc, xargv, "burst", NULL, &burstsize))
    {
      /* no further work needed */
    }
    else if (set_simple_uint32 (xoptind, xargc, xargv, "size", size_units, &baggagesize))
    {
      /* no further work needed */
    }
    else if (sscanf (xargv[*xoptind], "%lf%n", &r, &pos) == 1 && strcmp (xargv[*xoptind] + pos, "%") == 0)
    {
      if (r < 0 || r > 100) error3 ("%s: ping fraction out of range\n", xargv[*xoptind]);
      ping_frac = (uint32_t) (UINT32_MAX * (r / 100.0) + 0.5);
    }
    else if (strcmp (xargv[*xoptind], "ping") == 0 && *xoptind + 1 < xargc && sscanf (xargv[*xoptind + 1], "%lf%%%n", &pub_rate, &pos) == 1 && xargv[*xoptind + 1][pos] == 0)
    {
      ++(*xoptind);
      if (r < 0 || r > 100) error3 ("%s: ping fraction out of range\n", xargv[*xoptind]);
      ping_frac = (uint32_t) (UINT32_MAX * (r / 100.0) + 0.5);
    }
    else
    {
      error3 ("%s: unrecognised publish specification\n", xargv[*xoptind]);
    }
    (*xoptind)++;
  }
}

static void set_mode (int xoptind, int xargc, char * const xargv[])
{
  int code;
  pub_rate = 0.0;
  submode = SM_NONE;
  pingpongmode = SM_LISTENER;
  ping_intv = DDS_INFINITY;
  ping_frac = 0;
  while (xoptind < xargc && (code = exact_string_int_map_lookup (modestrings, "mode string", xargv[xoptind], true)) != -1)
  {
    xoptind++;
    switch (code)
    {
      case 1: set_mode_ping (&xoptind, xargc, xargv); break;
      case 2: set_mode_pong (&xoptind, xargc, xargv); break;
      case 3: set_mode_sub (&xoptind, xargc, xargv); break;
      case 4: set_mode_pub (&xoptind, xargc, xargv); break;
    }
  }
  if (xoptind != xargc)
  {
    error3 ("%s: unrecognized argument\n", xargv[xoptind]);
  }
}

int main (int argc, char *argv[])
{
  dds_entity_t ws;
  dds_return_t rc;
  dds_qos_t *qos;
  dds_listener_t *listener;
  int opt;
  bool collect_stats = false;
  dds_time_t tref = DDS_INFINITY;
  ddsrt_threadattr_t attr;
  ddsrt_thread_t pubtid, subtid, subpingtid, subpongtid;
#if !_WIN32 && !DDSRT_WITH_FREERTOS
  sigset_t sigset, osigset;
  ddsrt_thread_t sigtid;
#endif
  char netload_if[256];
  double netload_bw = 0;
  ddsrt_threadattr_init (&attr);

  argv0 = argv[0];

  while ((opt = getopt (argc, argv, "cd:D:i:n:k:uLK:T:M:N:R:h")) != EOF)
  {
    switch (opt)
    {
      case 'c': collect_stats = true; break;
      case 'd': {
        char *col;
        int pos;
        (void) ddsrt_strlcpy (netload_if, optarg, sizeof (netload_if));
        if ((col = strrchr (netload_if, ':')) == NULL || col == netload_if ||
            (sscanf (col+1, "%lf%n", &netload_bw, &pos) != 1 || (col+1)[pos] != 0))
          error3 ("-d%s: expected DEVICE:BANDWIDTH\n", optarg);
        *col = 0;
        break;
      }
      case 'D': dur = atof (optarg); if (dur <= 0) dur = HUGE_VAL; break;
      case 'i': did = (dds_domainid_t) atoi (optarg); break;
      case 'n': nkeyvals = (unsigned) atoi (optarg); break;
      case 'u': reliable = false; break;
      case 'k': histdepth = atoi (optarg); if (histdepth < 0) histdepth = 0; break;
      case 'L': ignorelocal = DDS_IGNORELOCAL_NONE; break;
      case 'T': case 'K': /* 'K' because of my muscle memory with pubsub ... */
        if (strcmp (optarg, "KS") == 0) topicsel = KS;
        else if (strcmp (optarg, "K32") == 0) topicsel = K32;
        else if (strcmp (optarg, "K256") == 0) topicsel = K256;
        else if (strcmp (optarg, "OU") == 0) topicsel = OU;
        else if (strcmp (optarg, "UK16") == 0) topicsel = UK16;
        else if (strcmp (optarg, "UK1024") == 0) topicsel = UK1024;
        else error3 ("%s: unknown topic\n", optarg);
        break;
      case 'M': maxwait = atof (optarg); if (maxwait <= 0) maxwait = HUGE_VAL; break;
      case 'N': minmatch = (unsigned) atoi (optarg); break;
      case 'R': tref = 0; sscanf (optarg, "%"SCNd64, &tref); break;
      case 'h': usage (); break;
      default: error3 ("-%c: unknown option\n", opt); break;
    }
  }

  if (optind == argc || (optind + 1 == argc && strcmp (argv[optind], "help") == 0))
    usage ();
  else if (optind + 1 == argc && strcmp (argv[optind], "sanity") == 0)
  {
    char * const sanity[] = { "ping", "1Hz" };
    set_mode (0, 2, sanity);
  }
  else
  {
    set_mode (optind, argc, argv);
  }

  if (nkeyvals == 0)
    nkeyvals = 1;
  if (topicsel == OU && nkeyvals != 1)
    error3 ("-n%u invalid: topic OU has no key\n", nkeyvals);
  if (topicsel != KS && baggagesize != 0)
    error3 ("size %"PRIu32" invalid: only topic KS has a sequence\n", baggagesize);
  if (baggagesize != 0 && baggagesize < 12)
    error3 ("size %"PRIu32" invalid: too small to allow for overhead\n", baggagesize);
  else if (baggagesize > 0)
    baggagesize -= 12;

  struct record_netload_state *netload_state;
  if (netload_bw <= 0)
    netload_state = NULL;
  else if ((netload_state = record_netload_new (netload_if, netload_bw)) == NULL)
    error3 ("can't get network utilization information for device %s\n", netload_if);

  ddsrt_avl_init (&ppants_td, &ppants);
  ddsrt_fibheap_init (&ppants_to_match_fhd, &ppants_to_match);

  ddsrt_mutex_init (&disc_lock);
  ddsrt_mutex_init (&pongstat_lock);
  ddsrt_mutex_init (&pongwr_lock);
  ddsrt_mutex_init (&pubstat_lock);

  pubstat_hist = hist_new (30, 1000, 0);

  qos = dds_create_qos ();
  /* set user data: magic cookie, whether we have a reader for the Data topic
     (all other endpoints always exist), pid and hostname */
  {
    unsigned pos;
    char udata[256];
    pos = (unsigned) snprintf (udata, sizeof (udata), UDATA_MAGIC"%d:%"PRIdPID":", submode != SM_NONE, ddsrt_getpid ());
    assert (pos < sizeof (udata));
    if (ddsrt_gethostname (udata + pos, sizeof (udata) - pos) != DDS_RETCODE_OK)
      strcpy (udata + UDATA_MAGIC_SIZE, "?");
    dds_qset_userdata (qos, udata, strlen (udata));
  }
  if ((dp = dds_create_participant (did, qos, NULL)) < 0)
    error2 ("dds_create_participant(domain %d) failed: %d\n", (int) did, (int) dp);
  dds_delete_qos (qos);
  dds_write_set_batch (true);
  if ((rc = dds_get_instance_handle (dp, &dp_handle)) < 0)
    error2 ("dds_get_instance_handle(participant) failed: %d\n", (int) rc);

  qos = dds_create_qos ();
  dds_qset_partition1 (qos, "DDSPerf");
  if ((sub = dds_create_subscriber (dp, NULL, NULL)) < 0)
    error2 ("dds_create_subscriber failed: %d\n", (int) dp);
  if ((pub = dds_create_publisher (dp, NULL, NULL)) < 0)
    error2 ("dds_create_publisher failed: %d\n", (int) dp);
  dds_delete_qos (qos);

  qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS (100));
  if ((tp_stat = dds_create_topic (dp, &CPUStats_desc, "DDSPerfCPUStats", qos, NULL)) < 0)
    error2 ("dds_create_topic(%s) failed: %d\n", "DDSPerfCPUStats", (int) tp_stat);
  dds_delete_qos (qos);

  {
    const char *tp_suf = "";
    const dds_topic_descriptor_t *tp_desc = NULL;
    switch (topicsel)
    {
      case KS:     tp_suf = "KS";     tp_desc = &KeyedSeq_desc; break;
      case K32:    tp_suf = "K32";    tp_desc = &Keyed32_desc;  break;
      case K256:   tp_suf = "K256";   tp_desc = &Keyed256_desc; break;
      case OU:     tp_suf = "OU";     tp_desc = &OneULong_desc; break;
      case UK16:   tp_suf = "UK16";   tp_desc = &Unkeyed16_desc; break;
      case UK1024: tp_suf = "UK1024"; tp_desc = &Unkeyed1024_desc; break;
    }
    snprintf (tpname_data, sizeof (tpname_data), "DDSPerf%cData%s", reliable ? 'R' : 'U', tp_suf);
    snprintf (tpname_ping, sizeof (tpname_ping), "DDSPerf%cPing%s", reliable ? 'R' : 'U', tp_suf);
    snprintf (tpname_pong, sizeof (tpname_pong), "DDSPerf%cPong%s", reliable ? 'R' : 'U', tp_suf);
    qos = dds_create_qos ();
    dds_qset_reliability (qos, reliable ? DDS_RELIABILITY_RELIABLE : DDS_RELIABILITY_BEST_EFFORT, DDS_SECS (1));
    if ((tp_data = dds_create_topic (dp, tp_desc, tpname_data, qos, NULL)) < 0)
      error2 ("dds_create_topic(%s) failed: %d\n", tpname_data, (int) tp_data);
    if ((tp_ping = dds_create_topic (dp, tp_desc, tpname_ping, qos, NULL)) < 0)
      error2 ("dds_create_topic(%s) failed: %d\n", tpname_ping, (int) tp_ping);
    if ((tp_pong = dds_create_topic (dp, tp_desc, tpname_pong, qos, NULL)) < 0)
      error2 ("dds_create_topic(%s) failed: %d\n", tpname_pong, (int) tp_pong);
    dds_delete_qos (qos);
  }

  /* participants reader must exist before the "publication matched" or "subscription matched"
     listener is invoked, or it won't be able to get the details (FIXME: even the DDS spec
     has convenience functions for that ...) */
  listener = dds_create_listener (NULL);
  dds_lset_data_available (listener, participant_data_listener);
  if ((rd_participants = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, listener)) < 0)
    error2 ("dds_create_reader(participants) failed: %d\n", (int) rd_participants);
  dds_delete_listener (listener);
  if ((rd_subscriptions = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL)) < 0)
    error2 ("dds_create_reader(subscriptions) failed: %d\n", (int) rd_subscriptions);
  if ((rd_publications = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL)) < 0)
    error2 ("dds_create_reader(publications) failed: %d\n", (int) rd_publications);

  /* stats writer always exists, reader only when we were requested to collect & print stats */
  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, 1);
  dds_qset_ignorelocal (qos, DDS_IGNORELOCAL_PARTICIPANT);
  if ((wr_stat = dds_create_writer (pub, tp_stat, qos, NULL)) < 0)
    error2 ("dds_create_writer(statistics) failed: %d\n", (int) wr_stat);
  if (collect_stats)
  {
    if ((rd_stat = dds_create_reader (sub, tp_stat, qos, NULL)) < 0)
      error2 ("dds_create_reader(statistics) failed: %d\n", (int) rd_stat);
  }
  dds_delete_qos (qos);

  /* ping reader/writer uses keep-last-1 history; not checking matching on these (yet) */
  qos = dds_create_qos ();
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, 1);
  dds_qset_ignorelocal (qos, ignorelocal);
  listener = dds_create_listener ((void *) (uintptr_t) MM_WR_PING);
  dds_lset_subscription_matched (listener, subscription_matched_listener);
  if ((rd_ping = dds_create_reader (sub, tp_ping, qos, listener)) < 0)
    error2 ("dds_create_reader(%s) failed: %d\n", tpname_ping, (int) rd_ping);
  dds_delete_listener (listener);
  listener = dds_create_listener ((void *) (uintptr_t) MM_RD_PING);
  dds_lset_publication_matched (listener, publication_matched_listener);
  if ((wr_ping = dds_create_writer (pub, tp_ping, qos, listener)) < 0)
    error2 ("dds_create_writer(%s) failed: %d\n", tpname_ping, (int) wr_ping);
  dds_delete_listener (listener);
  dds_delete_qos (qos);

  /* data reader/writer use a keep-all history with generous resource limits. */
  qos = dds_create_qos ();
  if (histdepth == 0)
    dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 1);
  else
    dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, histdepth);
  dds_qset_resource_limits (qos, 10000, DDS_LENGTH_UNLIMITED, DDS_LENGTH_UNLIMITED);
  dds_qset_ignorelocal (qos, ignorelocal);
  listener = dds_create_listener ((void *) (uintptr_t) MM_WR_DATA);
  dds_lset_subscription_matched (listener, subscription_matched_listener);
  if (submode != SM_NONE && (rd_data = dds_create_reader (sub, tp_data, qos, listener)) < 0)
    error2 ("dds_create_reader(%s) failed: %d\n", tpname_data, (int) rd_data);
  dds_delete_listener (listener);
  listener = dds_create_listener ((void *) (uintptr_t) MM_RD_DATA);
  dds_lset_publication_matched (listener, publication_matched_listener);
  if ((wr_data = dds_create_writer (pub, tp_data, qos, listener)) < 0)
    error2 ("dds_create_writer(%s) failed: %d\n", tpname_data, (int) wr_data);
  dds_delete_listener (listener);

  /* We only need a pong reader when sending data with a non-zero probability
     of it being a "ping", or when sending "real" pings.  I.e., if
       rate > 0 && ping_frac > 0) || ping_intv != DDS_NEVER
     but it doesn't really hurt to have the reader either, and always creating
     it and futhermore eagerly creating the pong writers means we can do more
     checking.  */
  {
    /* participant listener should have already been called for "dp", so we
       can simply look up the details on ourself to get at the GUID of the
       participant */
    struct guidstr guidstr;
    struct ppant *pp;
    dds_entity_t sub_pong;
    ddsrt_mutex_lock (&disc_lock);
    if ((pp = ddsrt_avl_lookup (&ppants_td, &ppants, &dp_handle)) == NULL)
    {
      printf ("participant %"PRIx64" (self) not found\n", dp_handle);
      exit (2);
    }
    make_guidstr (&guidstr, &pp->guid);
    ddsrt_mutex_unlock (&disc_lock);
    dds_qos_t *subqos = dds_create_qos ();
    dds_qset_partition1 (subqos, guidstr.str);
    if ((sub_pong = dds_create_subscriber (dp, subqos, NULL)) < 0)
      error2 ("dds_create_subscriber(pong) failed: %d\n", (int) sub_pong);
    dds_delete_qos (subqos);
    listener = dds_create_listener ((void *) (uintptr_t) MM_WR_PONG);
    dds_lset_subscription_matched (listener, subscription_matched_listener);
    if ((rd_pong = dds_create_reader (sub_pong, tp_pong, qos, listener)) < 0)
      error2 ("dds_create_reader(%s) failed: %d\n", tpname_pong, (int) rd_pong);
    dds_delete_listener (listener);
  }
  dds_delete_qos (qos);

  if ((termcond = dds_create_guardcondition (dp)) < 0)
    error2 ("dds_create_guardcondition(termcond) failed: %d\n", (int) termcond);
  if ((ws = dds_create_waitset (dp)) < 0)
    error2 ("dds_create_waitset(main) failed: %d\n", (int) ws);
  if ((rc = dds_waitset_attach (ws, termcond, 0)) < 0)
    error2 ("dds_waitset_attach(main, termcond) failed: %d\n", (int) rc);

  /* I hate Unix signals in multi-threaded processes ... */
#ifdef _WIN32
  signal (SIGINT, signal_handler);
#elif !DDSRT_WITH_FREERTOS
  sigemptyset (&sigset);
  sigaddset (&sigset, SIGINT);
  sigaddset (&sigset, SIGTERM);
  sigprocmask (SIG_BLOCK, &sigset, &osigset);
  ddsrt_thread_create (&sigtid, "sigthread", &attr, sigthread, &sigset);
#if defined __APPLE__ || defined __linux
  signal (SIGXFSZ, sigxfsz_handler);
#endif
#endif

  /* Make publisher & subscriber thread arguments and start the threads we
     need (so what if we allocate memory for reading data even if we don't
     have a reader or will never really be receiving data) */
  struct subthread_arg subarg_data, subarg_ping, subarg_pong;
  init_eseq_admin (&eseq_admin, nkeyvals);
  subthread_arg_init (&subarg_data, rd_data, 1000);
  subthread_arg_init (&subarg_ping, rd_ping, 100);
  subthread_arg_init (&subarg_pong, rd_pong, 100);
  uint32_t (*subthread_func) (void *arg) = 0;
  switch (submode)
  {
    case SM_NONE:     break;
    case SM_WAITSET:  subthread_func = subthread_waitset; break;
    case SM_POLLING:  subthread_func = subthread_polling; break;
    case SM_LISTENER: break;
  }
  memset (&pubtid, 0, sizeof (pubtid));
  memset (&subtid, 0, sizeof (subtid));
  memset (&subpingtid, 0, sizeof (subpingtid));
  memset (&subpongtid, 0, sizeof (subpongtid));
  if (pub_rate > 0)
    ddsrt_thread_create (&pubtid, "pub", &attr, pubthread, NULL);
  if (subthread_func != 0)
    ddsrt_thread_create (&subtid, "sub", &attr, subthread_func, &subarg_data);
  else if (submode == SM_LISTENER)
    set_data_available_listener (rd_data, "rd_data", data_available_listener, &subarg_data);
  /* Need to handle incoming "pong"s only if we can be sending "ping"s (whether that
     be pings from the "ping" mode (i.e. ping_intv != DDS_NEVER), or pings embedded
     in the published data stream (i.e. rate > 0 && ping_frac > 0).  The trouble with
     the first category is that a publication/subscription pair in the same process
     would result in a "thread awake nesting" overflow (and otherwise in a stack
     overflow) because each sample triggers the next.  So in that particular case we
     had better create a waitset. */
  const bool pingpong_waitset = (ping_intv != DDS_NEVER && ignorelocal == DDS_IGNORELOCAL_NONE) || pingpongmode == SM_WAITSET;
  if (pingpong_waitset)
  {
    ddsrt_thread_create (&subpingtid, "ping", &attr, subpingthread_waitset, &subarg_pong);
    ddsrt_thread_create (&subpongtid, "pong", &attr, subpongthread_waitset, &subarg_pong);
  }
  else
  {
    set_data_available_listener (rd_ping, "rd_ping", ping_available_listener, &subarg_ping);
    set_data_available_listener (rd_pong, "rd_pong", pong_available_listener, &subarg_pong);
  }

  /* Have to do this after all threads have been created because it caches the list */
  struct record_cputime_state *cputime_state;
  cputime_state = record_cputime_new (wr_stat);

  /* Run until time limit reached or a signal received.  (The time calculations
     ignore the possibility of overflow around the year 2260.) */
  dds_time_t tnow = dds_time ();
  const dds_time_t tstart = tnow;
  if (tref == DDS_INFINITY)
    tref = tstart;
  dds_time_t tmatch = (maxwait == HUGE_VAL) ? DDS_NEVER : tstart + (int64_t) (maxwait * 1e9 + 0.5);
  const dds_time_t tstop = (dur == HUGE_VAL) ? DDS_NEVER : tstart + (int64_t) (dur * 1e9 + 0.5);
  dds_time_t tnext = tstart + DDS_SECS (1);
  dds_time_t tlast = tstart;
  dds_time_t tnextping = (ping_intv == DDS_INFINITY) ? DDS_NEVER : (ping_intv == 0) ? tstart + DDS_SECS (1) : tstart + ping_intv;
  while (!ddsrt_atomic_ld32 (&termflag) && tnow < tstop)
  {
    dds_time_t twakeup = DDS_NEVER;
    int32_t nxs;

    /* bail out if too few readers discovered within the deadline */
    if (tnow >= tmatch)
    {
      bool ok;
      ddsrt_mutex_lock (&disc_lock);
      ok = (matchcount >= minmatch);
      ddsrt_mutex_unlock (&disc_lock);
      if (ok)
        tmatch = DDS_NEVER;
      else
      {
        /* set minmatch to an impossible value to avoid a match occurring between now and
           the determining of the exit status from causing a successful return */
        minmatch = UINT32_MAX;
        break;
      }
    }

    /* sometimes multicast only works one way, thus it makes sense to verify
       reader/writer matching takes place within a set amount of time after
       discovering the participant. */
    {
      struct ppant *pp;
      ddsrt_mutex_lock (&disc_lock);
      while ((pp = ddsrt_fibheap_min (&ppants_to_match_fhd, &ppants_to_match)) != NULL && pp->tdeadline < tnow)
      {
        (void) ddsrt_fibheap_extract_min (&ppants_to_match_fhd, &ppants_to_match);
        if (pp->unmatched != 0)
        {
          printf ("[%"PRIdPID"] participant %s:%"PRIu32": failed to match in %.3fs\n", ddsrt_getpid (), pp->hostname, pp->pid, (double) (pp->tdeadline - pp->tdisc) / 1e9);
          fflush (stdout);
          matchtimeout++;
        }
        /* keep the participant in the admin so we will never look at it again */
        pp->tdeadline = DDS_NEVER;
      }
      if (pp && pp->tdeadline < tnext)
      {
        twakeup = pp->tdeadline;
      }
      ddsrt_mutex_unlock (&disc_lock);
    }

    /* next wakeup should be when the next event occurs */
    if (tnext < twakeup)
      twakeup = tnext;
    if (tstop < twakeup)
      twakeup = tstop;
    if (tmatch < twakeup)
      twakeup = tmatch;
    if (tnextping < twakeup)
      twakeup = tnextping;

    if ((nxs = dds_waitset_wait_until (ws, NULL, 0, twakeup)) < 0)
      error2 ("dds_waitset_wait_until(main): error %d\n", (int) nxs);

    /* try to print exactly once per second, but do gracefully handle a very late wakeup */
    tnow = dds_time ();
    if (tnext <= tnow)
    {
      print_stats (tref, tnow, tlast, cputime_state, netload_state);
      tlast = tnow;
      if (tnow > tnext + DDS_MSECS (500))
        tnext = tnow + DDS_SECS (1);
      else
        tnext += DDS_SECS (1);
    }

    /* If a "real" ping doesn't result in the expected number of pongs within a reasonable
       time, send a new ping to restart the process.  This can happen as a result of starting
       or stopping a process, as a result of packet loss if best-effort reliability is
       selected, or as a result of overwhelming the ping/pong from the data publishing thread
       (as the QoS is a simple keep-last-1) */
    if (tnextping <= tnow)
    {
      maybe_send_new_ping (tnow, &tnextping);
    }
  }
  record_netload_free (netload_state);
  record_cputime_free (cputime_state);

#if _WIN32
  signal_handler (SIGINT);
#elif !DDSRT_WITH_FREERTOS
  {
    /* get the attention of the signal handler thread */
    void (*osigint) (int);
    void (*osigterm) (int);
    kill (getpid (), SIGTERM);
    ddsrt_thread_join (sigtid, NULL);
    osigint = signal (SIGINT, SIG_IGN);
    osigterm = signal (SIGTERM, SIG_IGN);
    sigprocmask (SIG_SETMASK, &osigset, NULL);
    signal (SIGINT, osigint);
    signal (SIGINT, osigterm);
  }
#endif

  if (pub_rate > 0)
    ddsrt_thread_join (pubtid, NULL);
  if (subthread_func != 0)
    ddsrt_thread_join (subtid, NULL);
  if (pingpong_waitset)
  {
    ddsrt_thread_join (subpingtid, NULL);
    ddsrt_thread_join (subpongtid, NULL);
  }

  /* stop the listeners before deleting the readers: otherwise they may
     still try to access a reader that has already become inaccessible
     (not quite good, but ...) */
  dds_set_listener (rd_ping, NULL);
  dds_set_listener (rd_pong, NULL);
  dds_set_listener (rd_data, NULL);
  dds_set_listener (rd_participants, NULL);
  dds_set_listener (rd_subscriptions, NULL);
  dds_set_listener (rd_publications, NULL);

  /* Delete rd_data early to workaround a deadlock deleting a reader
     or writer while the receive thread (or a delivery thread) got
     stuck trying to write into a reader that hit its resource limits.

     The deadlock is that the deleting of a reader/writer requires
     waiting for the DDSI-level entity to be deleted (a multi-stage
     GC process), and a "stuck" receive thread prevents the GC from
     making progress.

     The fix is to eliminate the waiting and retrying, and instead
     flip the reader's state to out-of-sync and rely on retransmits
     to let it make progress once room is available again.  */
  dds_delete (rd_data);

  uint64_t nlost = 0;
  for (uint32_t i = 0; i < eseq_admin.nph; i++)
    nlost += eseq_admin.stats[i].nlost;
  fini_eseq_admin (&eseq_admin);
  subthread_arg_fini (&subarg_data);
  subthread_arg_fini (&subarg_ping);
  subthread_arg_fini (&subarg_pong);
  dds_delete (dp);
  ddsrt_mutex_destroy (&disc_lock);
  ddsrt_mutex_destroy (&pongwr_lock);
  ddsrt_mutex_destroy (&pongstat_lock);
  ddsrt_mutex_destroy (&pubstat_lock);
  hist_free (pubstat_hist);
  free (pongwr);
  for (uint32_t i = 0; i < npongstat; i++)
    free (pongstat[i].raw);
  free (pongstat);

  bool ok = true;

  {
    ddsrt_avl_iter_t it;
    struct ppant *pp;
    for (pp = ddsrt_avl_iter_first (&ppants_td, &ppants, &it); pp; pp = ddsrt_avl_iter_next (&it))
      if (pp->unmatched != 0)
      {
        char buf[256];
        printf ("[%"PRIdPID"] error: %s:%"PRIu32" failed to match %s\n", ddsrt_getpid (), pp->hostname, pp->pid, match_mask_to_string (buf, sizeof (buf), pp->unmatched));
        ok = false;
      }
  }

  ddsrt_avl_free (&ppants_td, &ppants, free_ppant);

  if (matchcount < minmatch)
  {
    printf ("[%"PRIdPID"] error: too few matching participants (%"PRIu32" instead of %"PRIu32")\n", ddsrt_getpid (), matchcount, minmatch);
    ok = false;
  }
  if (nlost > 0 && (reliable && histdepth == 0))
  {
    printf ("[%"PRIdPID"] error: %"PRIu64" samples lost\n", ddsrt_getpid (), nlost);
    ok = false;
  }
  return ok ? 0 : 1;
}
