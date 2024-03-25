// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef PARAMETERS_H
#define PARAMETERS_H

#include <stdbool.h>

#include <dds/dds.h>

#include "ddsperf_types.h"


enum topicsel {
  KS,    /* KeyedSeq type: seq#, key, sequence-of-octet */
  K32,   /* Keyed32  type: seq#, key, array-of-24-octet (sizeof = 32) */
  K256,  /* Keyed256 type: seq#, key, array-of-248-octet (sizeof = 256) */
  OU,    /* OneULong type: seq# */
  UK16,  /* Unkeyed16, type: seq#, array-of-12-octet (sizeof = 16) */
  UK1k,  /* Unkeyed1k, type: seq#, array-of-1020-octet (sizeof = 1024) */
  UK64k, /* Unkeyed64k, type: seq#, array-of-65532-octet (sizeof = 65536) */
  S16,   /* Keyed, 16 octets, int64 junk, seq#, key */
  S256,  /* Keyed, 16 * S16, int64 junk, seq#, key */
  S4k,   /* Keyed, 16 * S256, int64 junk, seq#, key */
  S32k   /* Keyed, 4 * S4k, int64 junk, seq#, key */
};

enum submode {
  SM_NONE,    /* no subscriber at all */
  SM_WAITSET, /* subscriber using a waitset */
  SM_POLLING, /* ... using polling, sleeping for 1ms if no data */
  SM_LISTENER /* ... using a DATA_AVAILABLE listener */
};

enum flowmode {
  FLOW_DEFAULT,
  FLOW_PUB,
  FLOW_SUB,
};

struct global {
  double dur; /* Maximum run time in seconds */
  dds_time_t tref;
  char netload_if[256];
  double netload_bw;
  bool is_publishing;
  enum submode submode; /* Data and ping/pong subscriber triggering modes */
  enum submode pingpongmode;
  dds_duration_t ping_intv; /* Pinging interval for roundtrip testing, 0 means as fast as possible, DDS_INFINITY means never */
  bool substat_every_second; /* Whether to show "sub" stats every second even when nothing happens */
  bool collect_stats; /* collect statistics */
  bool extended_stats; /* Whether to show extended statistics (currently just rexmit info) */
  uint32_t minmatch; /* Minimum number of peers (if not met, exit status is 1) */
  double initmaxwait; /* Wait this long for MINMATCH peers before starting */
  double maxwait; /* Maximum time it may take to discover all MINMATCH peers */
  bool rss_check; /* Maximum allowed increase in RSS between 2nd RSS sample and final RSS sample: final one must be <= init * (1 + rss_factor/100) + rss_term  */
  double rss_factor;
  double rss_term;
  uint64_t min_received; /* Minimum number of samples, minimum number of roundtrips to declare the run a success */
  uint64_t min_roundtrips;
  bool livemem_check; /* Maximum allowed increase in live memory between initial sample, and final sample: final one must be <= init * (1 + livemem_factor/100) + livemem_term  */
  double livemem_factor;
  double livemem_term;
  bool sublatency; /* Whether to gather/show latency information in "sub" mode */
};

struct dataflow {
  char *name;
  enum flowmode mode;
  char *topicname;
  char *partition;
  enum topicsel topicsel; /* Topic type to use */
  const dds_topic_descriptor_t *tp_desc;
  unsigned nkeyvals; /* Number of different key values to use (must be 1 for OU type) */
  uint32_t baggagesize; /* Size of the sequence in KeyedSeq type in bytes */
  uint32_t burstsize; /* Data is published in bursts of this many samples */
  bool register_instances; /* Whether or not to register instances prior to writing */
  double pub_rate; /* Publishing rate in Hz, HUGE_VAL means as fast as possible, 0 means no throughput data is published at all */
  bool reliable; /* Whether to use reliable or best-effort readers/writers */
  int32_t histdepth; /* History depth for throughput data reader and writer; 0 is KEEP_ALL, otherwise it is KEEP_LAST histdepth.  Ping/pong always uses KEEP_LAST 1. */
  uint32_t transport_prio;
  uint32_t ping_frac; /* Fraction of throughput data samples that double as a ping message */
  bool use_writer_loan;   /* Use writer loans (only for memcpy-able types) */
  struct dataflow *next;
};

struct multiplier {
  const char *suffix;
  int mult;
};

void init_globals (struct global *args);
struct dataflow * dataflow_new (void);
void dataflow_free (struct dataflow *flow);
const dds_topic_descriptor_t * get_topic_descriptor (enum topicsel topicsel);
bool get_topicsel_from_string (const char *name, enum topicsel *topicsel);
bool string_to_size (const char *str, uint32_t *val);
bool string_to_number (const char *str, uint32_t *val);
bool string_to_frequency (const char *str, double *val);
bool string_to_bool (const char *value, bool *result);
char * string_dup(const char *s);
bool read_parameters (const char *fname, const char *node_name, struct dataflow **flows, struct global *globals);

#endif /* PARAMETERS_H */
