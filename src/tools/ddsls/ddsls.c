/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/dds.h"

// FIXME Temporary workaround for lack of wait_for_historical implementation. Remove this on completion of CHAM-268.
#define dds_reader_wait_for_historical_data(a,b) DDS_RETCODE_OK; dds_sleepfor(DDS_MSECS(200));

// FIXME should fix read/take interface to allow simple unlimited take
#define MAX_SAMPLES 10

#define MAX_DURATION_BUFSZ 21
static char *qp_duration_str (char *buf, size_t bufsz, dds_duration_t d)
{
  if (d == DDS_INFINITY)
    (void) snprintf (buf, bufsz, "infinite");
  else
    (void) snprintf (buf, bufsz, "%u.%09u", (unsigned)(d / DDS_NSECS_IN_SEC), (unsigned)(d % DDS_NSECS_IN_SEC));
  return buf;
}

static size_t printable_seq_length (const unsigned char *as, size_t n)
{
  size_t i;
  for (i = 0; i < n; i++) {
    if (as[i] < 32 || as[i] >= 127)
      break;
  }
  return i;
}

static void print_octetseq (const unsigned char *v, size_t sz, FILE *fp)
{
  size_t i, n;
  fprintf (fp, "%zu<", sz);
  i = 0;
  while (i < sz)
  {
    if ((n = printable_seq_length (v + i, sz - i)) < 4)
    {
      if (n == 0)
        n = 1;
      while (n--)
      {
        fprintf (fp, "%s%u", i == 0 ? "" : ",", v[i]);
        i++;
      }
    }
    else
    {
      fprintf (fp, "\"%*.*s\"", (int)n, (int)n, v + i);
      i += n;
    }
  }
  fprintf (fp, ">");
}

static void qp_user_data (const dds_qos_t *q, FILE *fp)
{
  void *ud;
  size_t udsz;
  if (dds_qget_userdata(q, &ud, &udsz))
  {
    fprintf (fp, "  user_data: value = ");
    print_octetseq (ud, udsz,fp);
    fprintf (fp, "\n");
    dds_free (ud);
  }
}

static void qp_topic_data (const dds_qos_t *q, FILE *fp)
{
  void *ud;
  size_t udsz;
  if (dds_qget_topicdata(q, &ud, &udsz))
  {
    fprintf (fp, "  topic_data: value = ");
    print_octetseq (ud, udsz,fp);
    fprintf (fp, "\n");
    dds_free (ud);
  }
}

static void qp_group_data (const dds_qos_t *q, FILE *fp)
{
  void *ud;
  size_t udsz;
  if (dds_qget_groupdata(q, &ud, &udsz))
  {
    fprintf (fp, "  group_data: value = ");
    print_octetseq (ud, udsz,fp);
    fprintf (fp, "\n");
    dds_free (ud);
  }
}

static void qp_durability (const dds_qos_t *q, FILE *fp)
{
  dds_durability_kind_t kind;
  if (dds_qget_durability (q, &kind))
  {
    static const char *s = "?";
    switch (kind)
    {
      case DDS_DURABILITY_VOLATILE: s = "volatile"; break;
      case DDS_DURABILITY_TRANSIENT_LOCAL: s = "transient-local"; break;
      case DDS_DURABILITY_TRANSIENT: s = "transient"; break;
      case DDS_DURABILITY_PERSISTENT: s = "persistent"; break;
    }
    fprintf (fp, "  durability: kind = %s\n", s);
  }
}

static void qp_history (const dds_qos_t *q, FILE *fp)
{
  dds_history_kind_t kind;
  int32_t depth;
  if (dds_qget_history (q, &kind, &depth))
  {
    fprintf (fp, "  history: kind = ");
    switch (kind)
    {
      case DDS_HISTORY_KEEP_LAST:
        fprintf (fp, "keep-last, depth = %"PRId32"\n", depth);
        break;
      case DDS_HISTORY_KEEP_ALL:
        fprintf (fp, "keep-all (depth = %"PRId32")\n", depth);
        break;
    }
  }
}

static void qp_resource_limits_1 (FILE *fp, int32_t max_samples, int32_t max_instances, int32_t max_samples_per_instance, int indent)
{
  fprintf (fp, "%*.*sresource_limits: max_samples = ", indent, indent, "");
  if (max_samples == DDS_LENGTH_UNLIMITED)
    fprintf (fp, "unlimited");
  else
    fprintf (fp, "%"PRId32, max_samples);
  fprintf (fp, ", max_instances = ");
  if (max_instances == DDS_LENGTH_UNLIMITED)
    fprintf (fp, "unlimited");
  else
    fprintf (fp, "%"PRId32, max_instances);
  fprintf (fp, ", max_samples_per_instance = ");
  if (max_samples_per_instance == DDS_LENGTH_UNLIMITED)
    fprintf (fp, "unlimited\n");
  else
    fprintf (fp, "%"PRId32"\n", max_samples_per_instance);
}

static void qp_resource_limits (const dds_qos_t *q, FILE *fp)
{
  int32_t max_samples, max_instances, max_samples_per_instance;
  if (dds_qget_resource_limits (q, &max_samples, &max_instances, &max_samples_per_instance))
    qp_resource_limits_1 (fp, max_samples, max_instances, max_samples_per_instance, 2);
}

static void qp_presentation (const dds_qos_t *q, FILE *fp)
{
  dds_presentation_access_scope_kind_t access_scope;
  bool coherent_access, ordered_access;
  if (dds_qget_presentation (q, &access_scope, &coherent_access, &ordered_access))
  {
    static const char *s = "?";
    switch (access_scope)
    {
      case DDS_PRESENTATION_INSTANCE: s = "instance"; break;
      case DDS_PRESENTATION_TOPIC: s = "topic"; break;
      case DDS_PRESENTATION_GROUP: s = "group"; break;
    }
    fprintf (fp, "  presentation: scope = %s, coherent_access = %s, ordered_access = %s\n", s, coherent_access ? "true" : "false", ordered_access ? "true" : "false");
  }
}

static void qp_duration_qos (const dds_qos_t *q, FILE *fp, const char *what, bool (*qget) (const dds_qos_t * __restrict qos, dds_duration_t *d))
{
  dds_duration_t d;
  char buf[MAX_DURATION_BUFSZ];
  if (qget (q, &d))
    fprintf (fp, "  %s = %s\n", what, qp_duration_str (buf, sizeof (buf), d));
}

static void qp_lifespan (const dds_qos_t *q, FILE *fp)
{
  qp_duration_qos (q, fp, "lifespan: duration", dds_qget_lifespan);
}

static void qp_deadline (const dds_qos_t *q, FILE *fp)
{
  qp_duration_qos (q, fp, "deadline: period", dds_qget_deadline);
}

static void qp_latency_budget (const dds_qos_t *q, FILE *fp)
{
  qp_duration_qos (q, fp, "latency_budget: duration", dds_qget_latency_budget);
}

static void qp_time_based_filter (const dds_qos_t *q, FILE *fp)
{
  qp_duration_qos (q, fp, "time_based_filter: minimum_separation", dds_qget_time_based_filter);
}

static void qp_ownership (const dds_qos_t *q, FILE *fp)
{
  dds_ownership_kind_t kind;
  char *s = "?";
  if (dds_qget_ownership (q, &kind))
  {
    switch (kind)
    {
      case DDS_OWNERSHIP_SHARED: s = "shared"; break;
      case DDS_OWNERSHIP_EXCLUSIVE: s = "exclusive"; break;
    }
    fprintf (fp, "  ownership: kind = %s\n", s);
  }
}

static void qp_ownership_strength (const dds_qos_t *q, FILE *fp)
{
  int32_t value;
  if (dds_qget_ownership_strength (q, &value))
    fprintf (fp, "  ownership_strength: value = %"PRId32"\n", value);
}

static void qp_liveliness (const dds_qos_t *q, FILE *fp)
{
  dds_liveliness_kind_t kind;
  dds_duration_t lease_duration;
  if (dds_qget_liveliness (q, &kind, &lease_duration))
  {
    char *s = "?";
    char buf[MAX_DURATION_BUFSZ];
    switch (kind)
    {
      case DDS_LIVELINESS_AUTOMATIC: s = "automatic"; break;
      case DDS_LIVELINESS_MANUAL_BY_PARTICIPANT: s = "manual-by-participant"; break;
      case DDS_LIVELINESS_MANUAL_BY_TOPIC: s = "manual-by-topic"; break;
    }
    fprintf (fp, "  liveliness: kind = %s, lease_duration = %s\n", s, qp_duration_str (buf, sizeof (buf), lease_duration));
  }
}

static void qp_reliability (const dds_qos_t *q, FILE *fp)
{
  dds_reliability_kind_t kind;
  dds_duration_t max_blocking_time;
  if (dds_qget_reliability (q, &kind, &max_blocking_time))
  {
    char *s = "?";
    char buf[MAX_DURATION_BUFSZ];
    switch (kind)
    {
      case DDS_RELIABILITY_BEST_EFFORT: s = "best-effort"; break;
      case DDS_RELIABILITY_RELIABLE: s = "reliable"; break;
    }
    fprintf (fp, "  reliability: kind = %s, max_blocking_time = %s\n", s, qp_duration_str (buf, sizeof (buf), max_blocking_time));
  }
}

static void qp_transport_priority (const dds_qos_t *q, FILE *fp)
{
  int32_t value;
  if (dds_qget_transport_priority (q, &value))
    fprintf (fp, "  transport_priority: priority = %"PRId32"\n", value);
}

static void qp_destination_order (const dds_qos_t *q, FILE *fp)
{
  dds_destination_order_kind_t kind;
  if (dds_qget_destination_order (q, &kind))
  {
    static const char *s = "?";
    switch (kind)
    {
      case DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP: s = "by-reception-timestamp"; break;
      case DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP: s = "by-source-timestamp"; break;
    }
    fprintf (fp, "  destination_order: kind = %s\n", s);
  }
}

static void qp_writer_data_lifecycle (const dds_qos_t *q, FILE *fp)
{
  bool value;
  if (dds_qget_writer_data_lifecycle (q, &value))
    fprintf (fp, "  writer_data_lifecycle: autodispose_unregistered_instances = %s\n", value ? "true" : "false");
}

static void qp_reader_data_lifecycle (const dds_qos_t *q, FILE *fp)
{
  dds_duration_t autopurge_nowriter_samples_delay, autopurge_disposed_samples_delay;
  if (dds_qget_reader_data_lifecycle (q, &autopurge_nowriter_samples_delay, &autopurge_disposed_samples_delay))
  {
    char buf1[MAX_DURATION_BUFSZ], buf2[MAX_DURATION_BUFSZ];
    fprintf (fp, "  reader_data_lifecycle: autopurge_nowriter_samples_delay = %s, autopurge_disposed_samples_delay = %s\n", qp_duration_str (buf1, sizeof (buf1), autopurge_nowriter_samples_delay), qp_duration_str (buf2, sizeof (buf2), autopurge_disposed_samples_delay));
  }
}

static void qp_durability_service (const dds_qos_t *q, FILE *fp)
{
  dds_duration_t service_cleanup_delay;
  dds_history_kind_t history_kind;
  int32_t history_depth;
  int32_t max_samples, max_instances, max_samples_per_instance;
  if (dds_qget_durability_service (q, &service_cleanup_delay, &history_kind, &history_depth, &max_samples, &max_instances, &max_samples_per_instance))
  {
    char buf[MAX_DURATION_BUFSZ];
    fprintf (fp, "  durability_service:\n");
    fprintf (fp, "    service_cleanup_delay: %s\n", qp_duration_str (buf, sizeof (buf), service_cleanup_delay));
    switch (history_kind)
    {
      case DDS_HISTORY_KEEP_LAST:
        fprintf (fp, "    history: kind = keep-last, depth = %"PRId32"\n", history_depth);
        break;
      case DDS_HISTORY_KEEP_ALL:
        fprintf (fp, "    history: kind = keep-all (depth = %"PRId32")\n", history_depth);
        break;
    }
    qp_resource_limits_1(fp, max_samples, max_instances, max_samples_per_instance, 4);
  }
}

static void qp_partition (const dds_qos_t *q, FILE *fp)
{
  uint32_t n;
  char **ps;
  if (dds_qget_partition (q, &n, &ps))
  {
    fprintf (fp, "  partition: name = ");
    if (n == 0)
      fprintf (fp, "(default)");
    else if (n == 1)
      fprintf (fp, "%s", ps[0]);
    else
    {
      fprintf (fp, "{");
      for (uint32_t i = 0; i < n; i++)
        fprintf (fp, "%s%s", (i > 0) ? "," : "", ps[i]);
      fprintf (fp, "}");
    }
    fprintf (fp, "\n");
    for (uint32_t i = 0; i < n; i++)
      dds_free (ps[i]);
    dds_free (ps);
  }
}

static void qp_qos (const dds_qos_t *q, FILE *fp)
{
  qp_reliability (q, fp);
  qp_durability (q, fp);
  qp_destination_order (q, fp);
  qp_partition (q, fp);
  qp_history (q, fp);
  qp_presentation (q, fp);
  qp_resource_limits (q, fp);
  qp_deadline (q, fp);
  qp_latency_budget (q, fp);
  qp_lifespan (q, fp);
  qp_liveliness (q, fp);
  qp_time_based_filter (q, fp);
  qp_transport_priority (q, fp);
  qp_writer_data_lifecycle (q, fp);
  qp_reader_data_lifecycle (q, fp);
  qp_durability_service (q, fp);
  qp_ownership (q, fp);
  qp_ownership_strength (q, fp);
  qp_user_data (q, fp);
  qp_topic_data (q, fp);
  qp_group_data (q, fp);
}

static void print_key(FILE *fp, const char *label, const dds_guid_t *key)
{
  fprintf(fp, "%s", label);
  for(size_t j = 0; j < sizeof (key->v); j++) {
    fprintf(fp, "%s%02x", (j == 0) ? " " : ":", key->v[j]);
  }
  fprintf(fp, "\n");
}

#ifdef DDS_HAS_TOPIC_DISCOVERY
static void print_key_topic(FILE *fp, const char *label, const unsigned char *key)
{
  fprintf(fp, "%s", label);
  for(size_t j = 0; j < sizeof (key); j++) {
    fprintf(fp, "%s%02x", (j == 0) ? " " : ":", key[j]);
  }
  fprintf(fp, "\n");
}

static void print_dcps_topic (FILE *fp, dds_entity_t pp)
{
  dds_entity_t rd = dds_create_reader (pp, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  (void)dds_reader_wait_for_historical_data (rd, DDS_SECS (5));
  while(true)
  {
    void *ptrs[MAX_SAMPLES] = { 0 };
    dds_sample_info_t info[sizeof (ptrs) / sizeof (ptrs[0])];
    int n = dds_take (rd, ptrs, info, sizeof (ptrs) / sizeof (ptrs[0]), sizeof (ptrs) / sizeof (ptrs[0]));
    if (n <= 0)
      break;
    for (int i = 0; i < n; i++)
    {
      dds_builtintopic_topic_t *data = ptrs[i];
      fprintf (fp,"TOPIC:\n");
      print_key_topic (fp, "  key =", data->key.d);
      fprintf (fp, "  name = %s\n", data->topic_name);
      fprintf (fp, "  type name = %s\n", data->type_name);
      if (info[i].valid_data)
      {
        qp_qos (data->qos, fp);
      }
    }
    dds_return_loan (rd, ptrs, n);
  }
  dds_delete (rd);
}
#endif /* DDS_HAS_TOPIC_DISCOVERY */

static void print_dcps_participant (FILE *fp, dds_entity_t pp)
{
  dds_entity_t rd = dds_create_reader (pp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  (void)dds_reader_wait_for_historical_data (rd, DDS_SECS (5));
  while(true)
  {
    void *ptrs[MAX_SAMPLES] = { 0 };
    dds_sample_info_t info[sizeof (ptrs) / sizeof (ptrs[0])];
    int n = dds_take (rd, ptrs, info, sizeof (ptrs) / sizeof (ptrs[0]), sizeof (ptrs) / sizeof (ptrs[0]));
    if (n <= 0)
      break;
    for (int i = 0; i < n; i++)
    {
      dds_builtintopic_participant_t *data = ptrs[i];
      fprintf (fp,"PARTICIPANT:\n");
      print_key (fp, "  key =", &data->key);
      if (info[i].valid_data)
      {
        qp_qos (data->qos, fp);
      }
    }
    (void) dds_return_loan (rd, ptrs, n);
  }
  (void) dds_delete (rd);
}

static void print_dcps_endpoint (FILE *fp, dds_entity_t pp, const char *type, dds_entity_t topic)
{
  dds_entity_t rd = dds_create_reader (pp, topic, NULL, NULL);
  (void)dds_reader_wait_for_historical_data (rd, DDS_SECS (5));
  while(true)
  {
    void *ptrs[MAX_SAMPLES] = { 0 };
    dds_sample_info_t info[sizeof (ptrs) / sizeof (ptrs[0])];
    int n = dds_take (rd, ptrs, info, sizeof (ptrs) / sizeof (ptrs[0]), sizeof (ptrs) / sizeof (ptrs[0]));
    if (n <= 0)
      break;
    for (int i = 0; i < n; i++)
    {
      dds_builtintopic_endpoint_t *data = ptrs[i];
      fprintf (fp,"%s:\n", type);
      print_key (fp, "  key =", &data->key);
      if (info[i].valid_data)
      {
        print_key (fp, "  participant_key =", &data->participant_key);
        fprintf (fp,"  topic_name = %s\n", data->topic_name);
        fprintf (fp,"  type_name = %s\n", data->type_name);
        qp_qos (data->qos,fp);
      }
    }
    (void) dds_return_loan (rd, ptrs, n);
  }
  (void) dds_delete (rd);
}

static void print_dcps_subscription (FILE *fp, dds_entity_t pp)
{
  print_dcps_endpoint (fp, pp, "SUBSCRIPTION", DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION);
}

static void print_dcps_publication (FILE *fp, dds_entity_t pp)
{
  print_dcps_endpoint (fp, pp, "PUBLICATION", DDS_BUILTIN_TOPIC_DCPSPUBLICATION);
}

#define DCPSTOPIC_FLAG 1
#define DCPSPARTICIPANT_FLAG (1<<1)
#define DCPSSUBSCRIPTION_FLAG (1<<2)
#define DCPSPUBLICATION_FLAG (1<<3)

static struct topictab {
  const char *name;
  const int flag;
  void (*fun) (FILE *fp, dds_entity_t pp);
} topictab[] = {
#ifdef DDS_HAS_TOPIC_DISCOVERY
  { "dcpstopic", DCPSTOPIC_FLAG, print_dcps_topic },
#endif
  { "dcpsparticipant", DCPSPARTICIPANT_FLAG, print_dcps_participant },
  { "dcpssubscription", DCPSSUBSCRIPTION_FLAG, print_dcps_subscription },
  { "dcpspublication", DCPSPUBLICATION_FLAG, print_dcps_publication }
};
#define TOPICTAB_SIZE (sizeof(topictab)/sizeof(struct topictab))

static void usage (void)
{
  fprintf (stderr, "Usage: ddsls [OPTIONS] TOPIC...  for specified topics\n\n");
  fprintf (stderr, "   or: ddsls [OPTIONS] -a        for all topics\n");
  fprintf (stderr, "\nOPTIONS:\n");
  fprintf (stderr, "-f <filename> <topics>    -- write to file\n");
  fprintf (stderr, "\nTOPICS\n");
  for (size_t i = 0; i < TOPICTAB_SIZE; i++)
    fprintf (stderr, "%s\n", topictab[i].name);
  exit (1);
}

int main (int argc, char **argv)
{
  FILE *fp = stdout;
  int flags = 0;
  dds_entity_t pp;
  int opt;
  while ((opt = getopt (argc, argv, "f:a")) != EOF)
  {
    switch (opt)
    {
      case 'f': {
        char *fname = optarg;
        DDSRT_WARNING_MSVC_OFF(4996)
        fp = fopen (fname, "w");
        DDSRT_WARNING_MSVC_ON(4996)
        if (fp == NULL)
        {
          fprintf (stderr, "%s: can't open for writing\n", fname);
          exit (1);
        }
        break;
      }
      case 'a':
        for (size_t i = 0; i < TOPICTAB_SIZE; i++)
          flags |= topictab[i].flag;
        break;
      default:
        usage ();
        break;
    }
  }

  if (argc == 1) {
    usage();
  }

  for (int i = optind; i < argc; i++)
  {
    size_t k;
    for (k = 0; k < TOPICTAB_SIZE; k++)
    {
      if (ddsrt_strcasecmp (argv[i], topictab[k].name) == 0)
      {
        flags |= topictab[k].flag;
        break;
      }
    }
    if (k == TOPICTAB_SIZE)
    {
      fprintf(stderr, "%s: topic unknown\n", argv[i]);
      exit (1);
    }
  }

  if ((pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL)) < 0)
  {
    fprintf (stderr, "failed to create participant: %s\n", dds_strretcode (pp));
    exit (1);
  }

  for (size_t i = 0; i < TOPICTAB_SIZE; i++)
  {
    if (flags & topictab[i].flag)
      topictab[i].fun (fp, pp);
  }
  dds_delete (pp);
  fclose (fp);
  return 0;
}
