// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <setjmp.h>

#include "dds/ddsrt/align.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/strtod.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_xevent.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_participant.h"
#include "ddsi__lease.h"
#include "ddsi__proxy_participant.h"
#include "dds/dds.h"
#include "dds__types.h"
#include "dds__entity.h"
#include "dds__writer.h"

#include "test_util.h"
#include "test_oneliner.h"

#include "Space.h"
#include "RoundTrip.h"

#define MAXDOMS (sizeof (((struct oneliner_ctx){.result=0}).doms) / sizeof (((struct oneliner_ctx){.result=0}).doms[0]))

static const char knownentities[] = "PRWrstwxy";
typedef struct { char n[MAXDOMS + 1]; } entname_t;

#define DEFINE_STATUS_CALLBACK(name, NAME, kind) \
  static void name##_cb (dds_entity_t kind, const dds_##name##_status_t status, void *arg) \
  { \
    struct oneliner_cb *cb = arg; \
    ddsrt_mutex_lock (&cb->ctx->g_mutex); \
    cb->cb_##kind = kind; \
    cb->cb_##name##_status = status; \
    cb->cb_called[DDS_##NAME##_STATUS_ID]++; \
    ddsrt_cond_broadcast (&cb->ctx->g_cond); \
    ddsrt_mutex_unlock (&cb->ctx->g_mutex); \
  }

DEFINE_STATUS_CALLBACK (inconsistent_topic, INCONSISTENT_TOPIC, topic)
DEFINE_STATUS_CALLBACK (liveliness_changed, LIVELINESS_CHANGED, reader)
DEFINE_STATUS_CALLBACK (liveliness_lost, LIVELINESS_LOST, writer)
DEFINE_STATUS_CALLBACK (offered_deadline_missed, OFFERED_DEADLINE_MISSED, writer)
DEFINE_STATUS_CALLBACK (offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, writer)
DEFINE_STATUS_CALLBACK (publication_matched, PUBLICATION_MATCHED, writer)
DEFINE_STATUS_CALLBACK (requested_deadline_missed, REQUESTED_DEADLINE_MISSED, reader)
DEFINE_STATUS_CALLBACK (requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, reader)
DEFINE_STATUS_CALLBACK (sample_lost, SAMPLE_LOST, reader)
DEFINE_STATUS_CALLBACK (sample_rejected, SAMPLE_REJECTED, reader)
DEFINE_STATUS_CALLBACK (subscription_matched, SUBSCRIPTION_MATCHED, reader)

static void data_on_readers_cb (dds_entity_t subscriber, void *arg)
{
  struct oneliner_cb *cb = arg;
  ddsrt_mutex_lock (&cb->ctx->g_mutex);
  cb->cb_subscriber = subscriber;
  cb->cb_called[DDS_DATA_ON_READERS_STATUS_ID]++;
  ddsrt_cond_broadcast (&cb->ctx->g_cond);
  ddsrt_mutex_unlock (&cb->ctx->g_mutex);
}

static void data_available_cb (dds_entity_t reader, void *arg)
{
  struct oneliner_cb *cb = arg;
  ddsrt_mutex_lock (&cb->ctx->g_mutex);
  cb->cb_reader = reader;
  cb->cb_called[DDS_DATA_AVAILABLE_STATUS_ID]++;
  ddsrt_cond_broadcast (&cb->ctx->g_cond);
  ddsrt_mutex_unlock (&cb->ctx->g_mutex);
}

static void dummy_data_on_readers_cb (dds_entity_t subscriber, void *arg)
{
  (void)subscriber;
  (void)arg;
}

static void dummy_data_available_cb (dds_entity_t reader, void *arg)
{
  (void)reader;
  (void)arg;
}

static void dummy_subscription_matched_cb (dds_entity_t reader, const dds_subscription_matched_status_t status, void *arg)
{
  (void)reader;
  (void)status;
  (void)arg;
}

static void dummy_liveliness_changed_cb (dds_entity_t reader, const dds_liveliness_changed_status_t status, void *arg)
{
  (void)reader;
  (void)status;
  (void)arg;
}

static void dummy_cb (void)
{
  // Used as a listener function in checking merging of listeners,
  // and for that purpose, casting it to whatever function type is
  // required is ok.  It is not supposed to ever be called.
  abort ();
}

#undef DEFINE_STATUS_CALLBACK

// These had better match the corresponding type definitions!
// n   uint32_t ...count
// c   int32_t  ...count_change
// I            instance handle of a data instance
// P   uint32_t QoS policy ID
// E            instance handle of an entity
// R            sample_rejected_status_kind
static const struct {
  const char *name;
  size_t size;           // size of status struct
  const char *desc;      // description of status struct
  dds_status_id_t id;    // status id, entry in "cb_called"
  size_t cb_entity_off;  // which cb_... entity to look at
  size_t cb_status_off;  // cb_..._status to look at
} lldesc[] = {
#define S0(abbrev, NAME, entity) \
  { abbrev, 0, NULL, DDS_##NAME##_STATUS_ID, offsetof (struct oneliner_cb, cb_##entity), 0 }
#define S(abbrev, name, NAME, desc, entity) \
  { abbrev, sizeof (dds_##name##_status_t), desc, DDS_##NAME##_STATUS_ID, offsetof (struct oneliner_cb, cb_##entity), offsetof (struct oneliner_cb, cb_##name##_status) }
  S0 ("da", DATA_AVAILABLE, reader),
  S0 ("dor", DATA_ON_READERS, subscriber),
  S ("it", inconsistent_topic, INCONSISTENT_TOPIC, "nc", topic),
  S ("lc", liveliness_changed, LIVELINESS_CHANGED, "nnccE", reader),
  S ("ll", liveliness_lost, LIVELINESS_LOST, "nc", writer),
  S ("odm", offered_deadline_missed, OFFERED_DEADLINE_MISSED, "ncI", writer),
  S ("oiq", offered_incompatible_qos, OFFERED_INCOMPATIBLE_QOS, "ncP", writer),
  S ("pm",  publication_matched, PUBLICATION_MATCHED, "ncncE", writer),
  S ("rdm", requested_deadline_missed, REQUESTED_DEADLINE_MISSED, "ncI", reader),
  S ("riq", requested_incompatible_qos, REQUESTED_INCOMPATIBLE_QOS, "ncP", reader),
  S ("sl",  sample_lost, SAMPLE_LOST, "nc", reader),
  S ("sr",  sample_rejected, SAMPLE_REJECTED, "ncRI", reader),
  S ("sm",  subscription_matched, SUBSCRIPTION_MATCHED, "ncncE", reader)
#undef S
#undef S0
};


static const void *advance (const void *status, size_t *off, char code)
{
  size_t align = 1, size = 1;
  switch (code)
  {
    case 'n': case 'c': case 'P':
      align = dds_alignof (uint32_t); size = sizeof (uint32_t);
      break;
    case 'E': case 'I':
      align = dds_alignof (dds_instance_handle_t); size = sizeof (dds_instance_handle_t);
      break;
    case 'R':
      align = dds_alignof (dds_sample_rejected_status_kind); size = sizeof (dds_sample_rejected_status_kind);
      break;
    default:
      abort ();
  }
  *off = (*off + align - 1) & ~(align - 1);
  const void *p = (const char *) status + *off;
  *off += size;
  return p;
}

static dds_return_t get_status (int ll, dds_entity_t ent, void *status)
{
  dds_return_t ret;
  switch (ll)
  {
    case 2: ret = dds_get_inconsistent_topic_status (ent, status); break;
    case 3: ret = dds_get_liveliness_changed_status (ent, status); break;
    case 4: ret = dds_get_liveliness_lost_status (ent, status); break;
    case 5: ret = dds_get_offered_deadline_missed_status (ent, status); break;
    case 6: ret = dds_get_offered_incompatible_qos_status (ent, status); break;
    case 7: ret = dds_get_publication_matched_status (ent, status); break;
    case 8: ret = dds_get_requested_deadline_missed_status (ent, status); break;
    case 9: ret = dds_get_requested_incompatible_qos_status (ent, status); break;
    case 10: ret = dds_get_sample_lost_status (ent, status); break;
    case 11: ret = dds_get_sample_rejected_status (ent, status); break;
    case 12: ret = dds_get_subscription_matched_status (ent, status); break;
    default: return -1;
  }
  return (ret == 0);
}

static dds_return_t check_status_change_fields_are_0 (int ll, dds_entity_t ent)
{
  if (lldesc[ll].desc)
  {
    const char *d = lldesc[ll].desc;
    void *status = malloc (lldesc[ll].size);
    dds_return_t ret;
    if ((ret = get_status (ll, ent, status)) < 0)
    {
      free (status);
      return ret;
    }
    size_t off = 0;
    while (*d)
    {
      const uint32_t *p = advance (status, &off, *d);
      if (*d == 'c' && *p != 0)
      {
        free (status);
        return 0;
      }
      d++;
    }
    assert (off <= lldesc[ll].size);
    free (status);
  }
  return 1;
}

#define TOK_END -1
#define TOK_NAME -2
#define TOK_INT -3
#define TOK_DURATION -4
#define TOK_TIMESTAMP -5
#define TOK_ELLIPSIS -6
#define TOK_INVALID -7

static int mprintf (struct oneliner_ctx *ctx, const char *msg, ...)
  ddsrt_attribute_format_printf(2, 3);
static int mfprintf (struct oneliner_ctx *ctx, FILE *fp, const char *msg, ...)
  ddsrt_attribute_format_printf(3, 4);
static int setresult (struct oneliner_ctx *ctx, int result, const char *msg, ...) ddsrt_attribute_format_printf(3, 4);
static void error (struct oneliner_ctx *ctx, const char *msg, ...)
  ddsrt_attribute_noreturn
  ddsrt_attribute_format_printf(2, 3);
static void error_dds (struct oneliner_ctx *ctx, dds_return_t ret, const char *msg, ...)
  ddsrt_attribute_noreturn
  ddsrt_attribute_format_printf(3, 4);
static void testfail (struct oneliner_ctx *ctx, const char *msg, ...)
  ddsrt_attribute_noreturn
  ddsrt_attribute_format_printf(2, 3);

static int mvfprintf (struct oneliner_ctx *ctx, FILE *fp, const char *msg, va_list args)
{
  const int next_needs_timestamp = (strchr (msg, '\n') != NULL);
  const int needs_timestamp = ctx->mprintf_needs_timestamp;
  ctx->mprintf_needs_timestamp = next_needs_timestamp;
  if (needs_timestamp)
  {
    const dds_time_t dt = dds_time () - ctx->l.tref;
    fprintf (fp, "%d.%06d ", (int32_t) (dt / DDS_NSECS_IN_SEC), (int32_t) (dt % DDS_NSECS_IN_SEC) / 1000);
  }
  int n = vfprintf (fp, msg, args);
  fflush (fp);
  return n;
}

static int mfprintf (struct oneliner_ctx *ctx, FILE *fp, const char *msg, ...)
{
  va_list args;
  va_start (args, msg);
  int n = mvfprintf (ctx, fp, msg, args);
  va_end (args);
  return n;
}

static int mprintf (struct oneliner_ctx *ctx, const char *msg, ...)
{
  va_list args;
  va_start (args, msg);
  int n = mvfprintf (ctx, stdout, msg, args);
  va_end (args);
  return n;
}

static void vsetresult (struct oneliner_ctx *ctx, int result, const char *msg, va_list ap)
{
  assert (result <= 0);
  ctx->result = result;
  vsnprintf (ctx->msg, sizeof (ctx->msg), msg, ap);
}

static int setresult (struct oneliner_ctx *ctx, int result, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);
  vsetresult (ctx, result, msg, ap);
  va_end (ap);
  return result;
}

static void error (struct oneliner_ctx *ctx, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);
  vsetresult (ctx, -1, msg, ap);
  va_end (ap);
  longjmp (ctx->jb, 1);
}

static void error_dds (struct oneliner_ctx *ctx, dds_return_t ret, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);
  vsetresult (ctx, -1, msg, ap);
  va_end (ap);
  size_t n = strlen (ctx->msg);
  if (n < sizeof (ctx->msg))
    snprintf (ctx->msg + n, sizeof (ctx->msg) - n, " (%s)", dds_strretcode (ret));
  longjmp (ctx->jb, 1);
}

static void testfail (struct oneliner_ctx *ctx, const char *msg, ...)
{
  va_list ap;
  va_start (ap, msg);
  vsetresult (ctx, 0, msg, ap);
  va_end (ap);
  longjmp (ctx->jb, 1);
}

static void advancetok (struct oneliner_lex *l)
{
  while (isspace ((unsigned char) *l->inp))
    l->inp++;
}

static int issymchar0 (char c)
{
  return isalpha ((unsigned char) c) || c == '_';
}

static int issymchar (char c)
{
  return isalnum ((unsigned char) c) || c == '_' || c == '\'';
}

static bool lookingatnum (const struct oneliner_lex *l)
{
  return (isdigit ((unsigned char) l->inp[(l->inp[0] == '-')]));
}

static bool lookingatinf (const struct oneliner_lex *l)
{
  return strncmp (l->inp, "inf", 3) == 0 && !issymchar (l->inp[3]);
}

static int nexttok_dur (struct oneliner_lex *l, union oneliner_tokval *v, bool expecting_duration)
{
  advancetok (l);
  if (l->inp[0] == 0)
  {
    l->tok = TOK_END;
  }
  else if (strncmp (l->inp, "...", 3) == 0)
  {
    l->inp += 3;
    l->tok = TOK_ELLIPSIS;
  }
  else if (!expecting_duration && lookingatnum (l))
  {
    char *endp;
    // strtol: [0-9]+ ; endp = l->inp if no digits present
    l->v.i = (int) strtol (l->inp, &endp, 10);
    l->inp = endp;
    if (v) *v = l->v;
    l->tok = TOK_INT;
  }
  else if (l->inp[0] == '@' || (expecting_duration && (lookingatnum (l) || lookingatinf (l))))
  {
    const int ists = (l->inp[0] == '@');
    char *endp;
    if (!ists && strncmp (l->inp + ists, "inf", 3) == 0 && !issymchar (l->inp[ists + 3]))
    {
      l->inp += ists + 3;
      l->v.d = DDS_INFINITY;
    }
    else
    {
      double d;
      if (ddsrt_strtod (l->inp + ists, &endp, &d) != DDS_RETCODE_OK)
        return false;
      if (!ists && d < 0)
        return false;
      if (d >= (double) (INT64_MAX / DDS_NSECS_IN_SEC))
        l->v.d = DDS_INFINITY;
      else if (d >= 0)
        l->v.d = (int64_t) (d * 1e9 + 0.5);
      else
        l->v.d = -(int64_t) (-d * 1e9 + 0.5);
      if (ists)
        l->v.d += l->tref;
      l->inp = endp;
    }
    if (v) *v = l->v;
    l->tok = ists ? TOK_TIMESTAMP : TOK_DURATION;
  }
  else if (issymchar0 (l->inp[0]))
  {
    int p = 0;
    while (issymchar (l->inp[p]))
    {
      if (p == (int) sizeof (l->v.n))
        return TOK_INVALID;
      l->v.n[p] = l->inp[p];
      p++;
    }
    l->v.n[p] = 0;
    l->inp += p;
    if (v) *v = l->v;
    l->tok = TOK_NAME;
  }
  else
  {
    l->tok = *l->inp++;
  }
  return l->tok;
}

static int nexttok (struct oneliner_lex *l, union oneliner_tokval *v)
{
  return nexttok_dur (l, v, false);
}

static int peektok (const struct oneliner_lex *l, union oneliner_tokval *v)
{
  struct oneliner_lex l1 = *l;
  return nexttok (&l1, v);
}

static bool nexttok_if (struct oneliner_lex *l, int tok)
{
  if (peektok (l, NULL) != tok)
    return false;
  nexttok (l, NULL);
  return true;
}

static bool nexttok_int (struct oneliner_lex *l, int *dst)
{
  if (peektok (l, NULL) != TOK_INT)
    return false;
  (void) nexttok (l, NULL);
  *dst = l->v.i;
  return true;
}

struct kvarg {
  const char *k;
  size_t klen;
  int v;
  bool (*arg) (struct oneliner_lex *l, void *dst); // *inp unchanged when false
  void (*def) (void *dst);
};

static void def_kvarg_int0 (void *dst) { *(int *)dst = 0; }
static void def_kvarg_int1 (void *dst) { *(int *)dst = 1; }
static void def_kvarg_dur_inf (void *dst) { *(dds_duration_t *)dst = DDS_INFINITY; }
static void def_kvarg_dur_100ms (void *dst) { *(dds_duration_t *)dst = DDS_MSECS (100); }

static bool read_kvarg_int (struct oneliner_lex *l, void *dst)
{
  return nexttok_int (l, dst);
}

static bool read_kvarg_posint (struct oneliner_lex *l, void *dst)
{
  return nexttok_int (l, dst) && l->v.i > 0;
}

static bool read_kvarg_dur (struct oneliner_lex *l, void *dst)
{
  dds_duration_t *x = dst;
  struct oneliner_lex l1 = *l;
  if (nexttok_dur (&l1, NULL, true) != TOK_DURATION)
    return false;
  *x = l1.v.d;
  *l = l1;
  return true;
}

static bool read_kvarg_3len (struct oneliner_lex *l, void *dst)
{
  struct oneliner_lex l1 = *l;
  int *x = dst, i = 0;
  x[0] = x[1] = x[2] = DDS_LENGTH_UNLIMITED;
  do {
    if (!nexttok_int (&l1, &x[i]) || (x[i] <= 0 && x[i] != DDS_LENGTH_UNLIMITED))
      return false;
  } while (++i < 3 && nexttok_if (&l1, '/'));
  *l = l1;
  return true;
}

static bool read_kvarg (const struct kvarg *ks, size_t sizeof_ks, struct oneliner_lex *l, int *v, void *arg)
{
  // l points at name, *inp is , or ) terminated; *l unchanged when false
  const struct kvarg *kend = ks + sizeof_ks / sizeof (*ks);
  struct oneliner_lex l1 = *l;
  advancetok (&l1);
  for (const struct kvarg *k = ks; k < kend; k++)
  {
    assert (strlen (k->k) == k->klen);
    *v = k->v;
    if (k->klen == 0)
    {
      assert (k->arg != 0 && k->def == 0);
      struct oneliner_lex l2 = l1;
      if (k->arg (&l2, arg) && (peektok (&l2, NULL) == ',' || peektok (&l2, NULL) == ')'))
      {
        *l = l2;
        return true;
      }
    }
    else if (strncmp (l1.inp, k->k, k->klen) != 0)
    {
      continue;
    }
    else
    {
      /* skip symbol */
      struct oneliner_lex l2 = l1;
      l2.inp += k->klen;
      if (peektok (&l2, NULL) == ',' || peektok (&l2, NULL) == ')')
      {
        if (k->arg == 0 || k->def != 0)
        {
          if (k->def) k->def (arg);
          *l = l2;
          return true;
        }
      }
      else if (k->arg != 0 && nexttok (&l2, NULL) == ':')
      {
        if (k->arg (&l2, arg) && (peektok (&l2, NULL) == ',' || peektok (&l2, NULL) == ')'))
        {
          *l = l2;
          return true;
        }
      }
    }
  }
  return false;
}

static bool qos_durability (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "v",  1, (int) DDS_DURABILITY_VOLATILE },
    { "tl", 2, (int) DDS_DURABILITY_TRANSIENT_LOCAL },
    { "t",  1, (int) DDS_DURABILITY_TRANSIENT },
    { "p",  1, (int) DDS_DURABILITY_PERSISTENT }
  };
  int v;
  if (!read_kvarg (ks, sizeof ks, l, &v, NULL))
    return false;
  dds_qset_durability (q, (dds_durability_kind_t) v);
  return true;
}

static const struct kvarg ks_history[] = {
  { "all", 3, (int) DDS_HISTORY_KEEP_ALL, .def = def_kvarg_int1 },
  { "",    0, (int) DDS_HISTORY_KEEP_LAST, .arg = read_kvarg_posint }
};

static bool qos_history (struct oneliner_lex *l, dds_qos_t *q)
{
  int v, x = 1;
  if (!read_kvarg (ks_history, sizeof ks_history, l, &v, &x))
    return false;
  dds_qset_history (q, (dds_history_kind_t) v, x);
  return true;
}

static bool qos_destination_order (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "r", 1, (int) DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP },
    { "s", 1, (int) DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP }
  };
  int v;
  if (!read_kvarg (ks, sizeof ks, l, &v, NULL))
    return false;
  dds_qset_destination_order (q, (dds_destination_order_kind_t) v);
  return true;
}

static bool qos_ownership (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "s", 1, (int) DDS_OWNERSHIP_SHARED, .def = def_kvarg_int0 },
    { "x", 1, (int) DDS_OWNERSHIP_EXCLUSIVE, .arg = read_kvarg_int, .def = def_kvarg_int0 }
  };
  int v, x;
  if (!read_kvarg (ks, sizeof ks, l, &v, &x))
    return false;
  dds_qset_ownership (q, (dds_ownership_kind_t) v);
  dds_qset_ownership_strength (q, x);
  return true;
}

static bool qos_transport_priority (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg k = { "", 0, 0, .arg = read_kvarg_int };
  int v, x;
  if (!read_kvarg (&k, sizeof k, l, &v, &x))
    return false;
  dds_qset_transport_priority (q, x);
  return true;
}

static bool qos_reliability (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "be", 2, (int) DDS_RELIABILITY_BEST_EFFORT, .def = def_kvarg_dur_100ms },
    { "r",  1, (int) DDS_RELIABILITY_RELIABLE,    .def = def_kvarg_dur_100ms, .arg = read_kvarg_dur }
  };
  int v;
  dds_duration_t x;
  if (!read_kvarg (ks, sizeof ks, l, &v, &x))
    return false;
  dds_qset_reliability (q, (dds_reliability_kind_t) v, x);
  return true;
}

static bool qos_liveliness (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "a", 1, (int) DDS_LIVELINESS_AUTOMATIC, .def = def_kvarg_dur_inf, .arg = read_kvarg_dur },
    { "p", 1, (int) DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, .arg = read_kvarg_dur },
    { "w", 1, (int) DDS_LIVELINESS_MANUAL_BY_TOPIC, .arg = read_kvarg_dur }
  };
  int v;
  dds_duration_t x;
  if (!read_kvarg (ks, sizeof ks, l, &v, &x))
    return false;
  dds_qset_liveliness (q, (dds_liveliness_kind_t) v, x);
  return true;
}

static bool qos_simple_duration (struct oneliner_lex *l, dds_qos_t *q, void (*set) (dds_qos_t * __restrict q, dds_duration_t dur))
{
  static const struct kvarg k = { "", 0, 0, .arg = read_kvarg_dur };
  int v;
  dds_duration_t x;
  if (!read_kvarg (&k, sizeof k, l, &v, &x))
    return false;
  set (q, x);
  return true;
}

static bool qos_latency_budget (struct oneliner_lex *l, dds_qos_t *q)
{
  return qos_simple_duration (l, q, dds_qset_latency_budget);
}

static bool qos_deadline (struct oneliner_lex *l, dds_qos_t *q)
{
  return qos_simple_duration (l, q, dds_qset_deadline);
}

static bool qos_lifespan (struct oneliner_lex *l, dds_qos_t *q)
{
  return qos_simple_duration (l, q, dds_qset_lifespan);
}

static bool qos_resource_limits (struct oneliner_lex *l, dds_qos_t *q)
{
  int rl[3];
  if (!read_kvarg_3len (l, rl))
    return false;
  dds_qset_resource_limits (q, rl[0], rl[1], rl[2]);
  return true;
}

static bool qos_durability_service (struct oneliner_lex *l, dds_qos_t *q)
{
  struct oneliner_lex l1 = *l;
  dds_duration_t scd;
  int hk = DDS_HISTORY_KEEP_LAST, hd = 1, rl[3];
  if (!read_kvarg_dur (&l1, &scd))
    return false;
  if (peektok (&l1, NULL) == '/')
  {
    (void) nexttok (&l1, NULL);
    if (!read_kvarg (ks_history, sizeof ks_history, &l1, &hk, &hd))
      return false;
  }
  if (peektok (&l1, NULL) != '/')
    rl[0] = rl[1] = rl[2] = DDS_LENGTH_UNLIMITED;
  else
  {
    (void) nexttok (&l1, NULL);
    if (!read_kvarg_3len (&l1, rl))
      return false;
  }
  dds_qset_durability_service (q, scd, (dds_history_kind_t) hk, hd, rl[0], rl[1], rl[2]);
  *l = l1;
  return true;
}

static bool qos_presentation (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "i", 1, (int) DDS_PRESENTATION_INSTANCE, .def = def_kvarg_int0 },
    { "t", 1, (int) DDS_PRESENTATION_TOPIC,    .def = def_kvarg_int1 },
    { "g", 1, (int) DDS_PRESENTATION_GROUP,    .def = def_kvarg_int1 }
  };
  int v, x;
  if (!read_kvarg (ks, sizeof ks, l, &v, &x))
    return false;
  dds_qset_presentation (q, (dds_presentation_access_scope_kind_t) v, x, 0);
  return true;
}

static bool qos_autodispose_unregistered_instances (struct oneliner_lex *l, dds_qos_t *q)
{
  static const struct kvarg ks[] = {
    { "y", 1, 1 },
    { "n", 1, 0 }
  };
  int v;
  if (!read_kvarg (ks, sizeof ks, l, &v, NULL))
    return false;
  dds_qset_writer_data_lifecycle (q, !!v);
  return true;
}

static const struct {
  char *abbrev;
  size_t n;
  bool (*fn) (struct oneliner_lex *l, dds_qos_t *q);
  dds_qos_policy_id_t id;
} qostab[] = {
  { "ll", 2, qos_liveliness, DDS_LIVELINESS_QOS_POLICY_ID },
  { "d",  1, qos_durability, DDS_DURABILITY_QOS_POLICY_ID },
  { "dl", 2, qos_deadline, DDS_DEADLINE_QOS_POLICY_ID },
  { "h",  1, qos_history, DDS_HISTORY_QOS_POLICY_ID },
  { "lb", 2, qos_latency_budget, DDS_LATENCYBUDGET_QOS_POLICY_ID },
  { "ls", 2, qos_lifespan, DDS_LIFESPAN_QOS_POLICY_ID },
  { "do", 2, qos_destination_order, DDS_DESTINATIONORDER_QOS_POLICY_ID },
  { "o",  1, qos_ownership, DDS_OWNERSHIP_QOS_POLICY_ID },
  { "tp", 2, qos_transport_priority, DDS_OWNERSHIPSTRENGTH_QOS_POLICY_ID },
  { "p",  1, qos_presentation, DDS_PRESENTATION_QOS_POLICY_ID },
  { "r",  1, qos_reliability, DDS_RELIABILITY_QOS_POLICY_ID },
  { "rl", 2, qos_resource_limits, DDS_RESOURCELIMITS_QOS_POLICY_ID },
  { "ds", 2, qos_durability_service, DDS_DURABILITYSERVICE_QOS_POLICY_ID },
  { "ad", 2, qos_autodispose_unregistered_instances, DDS_WRITERDATALIFECYCLE_QOS_POLICY_ID }
};

static bool setqos (struct oneliner_lex *l, dds_qos_t *q)
{
  struct oneliner_lex l1 = *l;
  dds_reset_qos (q);
  // no whitespace between name & QoS
  if (*l1.inp != '(')
    return true;
  nexttok (&l1, NULL); // eat '('
  do {
    size_t i;
    union oneliner_tokval name;
    if (nexttok (&l1, &name) != TOK_NAME || nexttok (&l1, NULL) != '=')
      return false;
    for (i = 0; i < sizeof (qostab) / sizeof (qostab[0]); i++)
    {
      assert (strlen (qostab[i].abbrev) == qostab[i].n);
      if (strcmp (name.n, qostab[i].abbrev) == 0)
        break;
    }
    if (i == sizeof (qostab) / sizeof (qostab[0]))
      return false;
    if (!qostab[i].fn (&l1, q))
      return false;
  } while (nexttok_if (&l1, ','));
  if (nexttok (&l1, NULL) != ')')
    return false;
  *l = l1;
  return true;
}

static int parse_entity1 (struct oneliner_lex *l, dds_qos_t *qos)
{
  struct oneliner_lex l1 = *l;
  if (nexttok (&l1, NULL) != TOK_NAME)
    return -1;
  const char *p;
  if ((p = strchr (knownentities, l1.v.n[0])) == NULL)
    return -1;
  int ent = (int) (p - knownentities);
  int i;
  for (i = 1; l1.v.n[i] == '\''; i++)
    ent += (int) sizeof (knownentities) - 1;
  if (l1.v.n[i] != 0)
    return -1;
  if (ent / 9 >= (int) MAXDOMS)
    return -1;
  if (!setqos (&l1, qos))
    return -1;
  *l = l1;
  return ent;
}

static int parse_entity (struct oneliner_ctx *ctx)
{
  return parse_entity1 (&ctx->l, ctx->entqos);
}

static int parse_listener1 (struct oneliner_lex *l)
{
  struct oneliner_lex l1 = *l;
  size_t i;
  if (nexttok (&l1, NULL) != TOK_NAME)
    return -1;
  for (i = 0; i < sizeof (lldesc) / sizeof (lldesc[0]); i++)
    if (strcmp (l1.v.n, lldesc[i].name) == 0)
      break;
  if (i == sizeof (lldesc) / sizeof (lldesc[0]))
    return -1;
  *l = l1;
  return (int) i;
}

static int parse_listener (struct oneliner_ctx *ctx)
{
  return parse_listener1 (&ctx->l);
}

static const char *getentname (entname_t *name, int ent)
{
  DDSRT_STATIC_ASSERT (sizeof (knownentities) == 10);
  DDSRT_STATIC_ASSERT (MAXDOMS == 3);
  name->n[0] = knownentities[ent % 9];
  const int dom = ent / 9;
  int i;
  for (i = 1; i <= dom; i++)
    name->n[i] = '\'';
  name->n[i] = 0;
  return name->n;
}

static void make_participant (struct oneliner_ctx *ctx, int ent, dds_qos_t *qos, dds_listener_t *list)
{
  const dds_domainid_t domid = (dds_domainid_t) (ent / 9);
  char* conf = ddsrt_expand_envvars("${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>", domid);
  if (ctx->config_override)
  {
    size_t newsize = strlen (conf) + strlen (ctx->config_override) + 1;
    char *conf1 = ddsrt_malloc (newsize);
    (void) snprintf (conf1, newsize, "%s,%s", conf, ctx->config_override);
    ddsrt_free (conf);
    conf = conf1;
  }
  entname_t name;
  dds_entity_t bisub;
  mprintf (ctx, "create domain %"PRIu32, domid);
  if ((ctx->doms[domid] = dds_create_domain (domid, conf)) <= 0)
    error_dds (ctx, ctx->doms[domid], "make_participant: create domain %"PRIu32" failed", domid);
  ddsrt_free (conf);
  mprintf (ctx, " create participant %s", getentname (&name, ent));
  if ((ctx->es[ent] = dds_create_participant (domid, qos, NULL)) <= 0)
    error_dds (ctx, ctx->es[ent], "make_participant: create participant failed in domain %"PRIu32, domid);
  if ((ctx->tps[domid] = dds_create_topic (ctx->es[ent], &Space_Type1_desc, ctx->topicname, ctx->qos, NULL)) <= 0)
    error_dds (ctx, ctx->tps[domid], "make_participant: create topic failed in domain %"PRIu32, domid);

  // Create the built-in topic readers with a dummy listener to avoid any event (data available comes to mind)
  // from propagating to the normal data available listener, in case it has been set on the participant.
  //
  // - dummy_cb aborts when it is invoked, but all reader-related listeners that can possibly trigger are set
  //   separately (incompatible qos, deadline missed, sample lost and sample rejected are all impossible by
  //   construction)
  // - regarding data_on_readers: Cyclone handles listeners installed on an ancestor by *inheriting* them,
  //   rather than by walking up ancestor chain. Setting data_on_readers on the reader therefore overrides the
  //   listener set on the subscriber. It is a nice feature!
  dds_listener_t *dummylist = dds_create_listener (ctx);
  dds_lset_data_available (dummylist, dummy_data_available_cb);
  dds_lset_data_on_readers (dummylist, dummy_data_on_readers_cb);
  dds_lset_inconsistent_topic (dummylist, (dds_on_inconsistent_topic_fn) dummy_cb);
  dds_lset_liveliness_changed (dummylist, dummy_liveliness_changed_cb);
  dds_lset_liveliness_lost (dummylist, (dds_on_liveliness_lost_fn) dummy_cb);
  dds_lset_offered_deadline_missed (dummylist, (dds_on_offered_deadline_missed_fn) dummy_cb);
  dds_lset_offered_incompatible_qos (dummylist, (dds_on_offered_incompatible_qos_fn) dummy_cb);
  dds_lset_publication_matched (dummylist, (dds_on_publication_matched_fn) dummy_cb);
  dds_lset_requested_deadline_missed (dummylist, (dds_on_requested_deadline_missed_fn) dummy_cb);
  dds_lset_requested_incompatible_qos (dummylist, (dds_on_requested_incompatible_qos_fn) dummy_cb);
  dds_lset_sample_lost (dummylist, (dds_on_sample_lost_fn) dummy_cb);
  dds_lset_sample_rejected (dummylist, (dds_on_sample_rejected_fn) dummy_cb);
  dds_lset_subscription_matched (dummylist, dummy_subscription_matched_cb);
  if ((ctx->pubrd[domid] = dds_create_reader (ctx->es[ent], DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, dummylist)) <= 0)
    error_dds (ctx, ctx->pubrd[domid], "make_participant: create DCPSPublication reader in domain %"PRIu32, domid);
  if ((ctx->subrd[domid] = dds_create_reader (ctx->es[ent], DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, dummylist)) <= 0)
    error_dds (ctx, ctx->subrd[domid], "make_participant: create DCPSSubscription reader in domain %"PRIu32, domid);
  bisub = dds_get_subscriber(ctx->pubrd[domid]);
  dds_set_listener(bisub, dummylist);
  dds_delete_listener (dummylist);
  dds_set_listener(ctx->es[ent], list);
  //mprintf (ctx, "pubrd %"PRId32" subrd %"PRId32" sub %"PRId32"\n", es->pubrd[domid], es->subrd[domid], dds_get_parent (es->pubrd[domid]));
}

static void make_entity1 (struct oneliner_ctx *ctx, int ent, dds_listener_t *list)
{
  entname_t wrname;
  dds_return_t ret;
  int domid = ent / 9;
  int ent1 = ent % 9;
  switch (ent1)
  {
    case 0:
      make_participant (ctx, ent, ctx->entqos, list);
      break;
    case 1:
      if (ctx->es[ent-1] == 0)
      {
        mprintf (ctx, "[");
        make_entity1 (ctx, ent-1, NULL);
        mprintf (ctx, "] ");
      }
      mprintf (ctx, "create subscriber %s", getentname (&wrname, ent));
      ctx->es[ent] = dds_create_subscriber (ctx->es[ent-1], ctx->entqos, list);
      break;
    case 2:
      if (ctx->es[ent-2] == 0)
      {
        mprintf (ctx, "[");
        make_entity1 (ctx, ent-2, NULL);
        mprintf (ctx, "] ");
      }
      mprintf (ctx, "create publisher %s", getentname (&wrname, ent));
      ctx->es[ent] = dds_create_publisher (ctx->es[ent-2], ctx->entqos, list);
      break;
    case 3: case 4: case 5:
      if (ctx->es[9*domid+1] == 0)
      {
        mprintf (ctx, "[");
        make_entity1 (ctx, 9*domid+1, NULL);
        mprintf (ctx, "] ");
      }
      mprintf (ctx, "create reader %s", getentname (&wrname, ent));
      ctx->es[ent] = dds_create_reader (ctx->es[9*domid+1], ctx->tps[domid], ctx->entqos, list);
      break;
    case 6: case 7: case 8:
      if (ctx->es[9*domid+2] == 0)
      {
        mprintf (ctx, "[");
        make_entity1 (ctx, 9*domid+2, NULL);
        mprintf (ctx, "] ");
      }
      mprintf (ctx, "create writer %s", getentname (&wrname, ent));
      ctx->es[ent] = dds_create_writer (ctx->es[9*domid+2], ctx->tps[domid], ctx->entqos, list);
      break;
    default:
      abort ();
  }
  mprintf (ctx, " = %"PRId32, ctx->es[ent]);
  if (ctx->es[ent] <= 0)
    error_dds (ctx, ctx->es[ent], "create entity %d failed", ent);
  if ((ret = dds_get_instance_handle (ctx->es[ent], &ctx->esi[ent])) != 0)
    error_dds (ctx, ret, "get instance handle for entity %"PRId32" failed", ctx->es[ent]);
  //mprintf (ctx, " %"PRIx64, es->esi[ent]);
}

static void make_entity (struct oneliner_ctx *ctx, int ent, dds_listener_t *list)
{
  make_entity1 (ctx, ent, list);
  mprintf (ctx, "\n");
}

static void setlistener (struct oneliner_ctx *ctx, struct oneliner_lex *l, int ll, int ent)
{
  mprintf (ctx, "set listener:");
  dds_return_t ret;
  int dom = ent / 9;
  dds_listener_t *list = ctx->cb[dom].list;
  dds_reset_listener (list);
  do {
    mprintf (ctx, " %s", lldesc[ll].name);
    switch (ll)
    {
      case 0: dds_lset_data_available (list, data_available_cb); break;
      case 1: dds_lset_data_on_readers (list, data_on_readers_cb); break;
      case 2: dds_lset_inconsistent_topic (list, inconsistent_topic_cb); break;
      case 3: dds_lset_liveliness_changed (list, liveliness_changed_cb); break;
      case 4: dds_lset_liveliness_lost (list, liveliness_lost_cb); break;
      case 5: dds_lset_offered_deadline_missed (list, offered_deadline_missed_cb); break;
      case 6: dds_lset_offered_incompatible_qos (list, offered_incompatible_qos_cb); break;
      case 7: dds_lset_publication_matched (list, publication_matched_cb); break;
      case 8: dds_lset_requested_deadline_missed (list, requested_deadline_missed_cb); break;
      case 9: dds_lset_requested_incompatible_qos (list, requested_incompatible_qos_cb); break;
      case 10: dds_lset_sample_lost (list, sample_lost_cb); break;
      case 11: dds_lset_sample_rejected (list, sample_rejected_cb); break;
      case 12: dds_lset_subscription_matched (list, subscription_matched_cb); break;
      default: abort ();
    }
  } while (l && (ll = parse_listener1 (l)) >= 0);
  if (ctx->es[ent] == 0)
  {
    mprintf (ctx, " for ");
    make_entity (ctx, ent, list);
  }
  else
  {
    dds_listener_t *tmplist = dds_create_listener (&ctx->cb[dom]);
    if ((ret = dds_get_listener (ctx->es[ent], tmplist)) != 0)
    {
      dds_delete_listener (tmplist);
      error_dds (ctx, ret, "set listener: dds_get_listener failed on %"PRId32, ctx->es[ent]);
    }
    dds_merge_listener (list, tmplist);
    dds_delete_listener (tmplist);
    mprintf (ctx, " on entity %"PRId32"\n", ctx->es[ent]);
    if ((ret = dds_set_listener (ctx->es[ent], list)) != 0)
      error_dds (ctx, ret, "set listener: dds_set_listener failed on %"PRId32, ctx->es[ent]);
  }
}

static dds_instance_handle_t lookup_insthandle (const struct oneliner_ctx *ctx, int ent, int ent1)
{
  // if both are in the same domain, it's easy
  if (ent / 9 == ent1 / 9)
    return ctx->esi[ent1];
  else
  {
    // if they aren't ... find GUID from instance handle in the one domain,
    // then find instance handle for GUID in the other
    dds_entity_t rd1 = 0, rd2 = 0;
    switch (ent1 % 9)
    {
      case  3: case  4: case  5: rd1 = ctx->subrd[ent1/9]; rd2 = ctx->subrd[ent/9]; break;
      case  6: case  7: case  8: rd1 = ctx->pubrd[ent1/9]; rd2 = ctx->pubrd[ent/9]; break;
      default: return 0;
    }

    dds_builtintopic_endpoint_t keysample;
    //mprintf (ctx, "(in %"PRId32" %"PRIx64" -> ", rd1, es->esi[ent1]);
    if (dds_instance_get_key (rd1, ctx->esi[ent1], &keysample) != 0)
      return 0;
    // In principle, only key fields are set in sample returned by get_key;
    // in the case of a built-in topic that is extended to the participant
    // key. The qos and topic/type names should not be set, and there is no
    // (therefore) memory allocated for the sample.
    assert (keysample.qos == NULL);
    assert (keysample.topic_name == NULL);
    assert (keysample.type_name == NULL);
    //for (size_t j = 0; j < sizeof (keysample.key.v); j++)
    //  mprintf (ctx, "%s%02x", (j > 0 && j % 4 == 0) ? ":" : "", keysample.key.v[j]);
    const dds_instance_handle_t ih = dds_lookup_instance (rd2, &keysample);
    //mprintf (ctx, " -> %"PRIx64")", ih);
    return ih;
  }
}

static void print_timestamp (struct oneliner_ctx *ctx, dds_time_t ts)
{
  dds_time_t dt = ts - ctx->l.tref;
  if ((dt % DDS_NSECS_IN_SEC) == 0)
    mprintf (ctx, "@%"PRId64, dt / DDS_NSECS_IN_SEC);
  else
  {
    unsigned frac = (unsigned) (dt % DDS_NSECS_IN_SEC);
    int digs = 9;
    while ((frac % 10) == 0)
    {
      digs--;
      frac /= 10;
    }
    mprintf (ctx, "@%"PRId64".%0*u", dt / DDS_NSECS_IN_SEC, digs, frac);
  }
}

static bool parse_sample_value (struct oneliner_ctx *ctx, Space_Type1 *s, bool *valid_data, int def)
{
  s->long_1 = s->long_2 = s->long_3 = def;
  if (nexttok (&ctx->l, NULL) == TOK_INT) // key value (invalid sample)
  {
    if (ctx->l.v.i < 0)
      return false;
    s->long_1 = ctx->l.v.i;
    *valid_data = false;
    return true;
  }
  else if (ctx->l.tok == '(')
  {
    if (nexttok (&ctx->l, NULL) != TOK_INT || ctx->l.v.i < 0)
      return false;
    s->long_1 = ctx->l.v.i;
    if (nexttok (&ctx->l, NULL) != ',' || nexttok (&ctx->l, NULL) != TOK_INT || ctx->l.v.i < 0)
      return false;
    s->long_2 = ctx->l.v.i;
    if (nexttok (&ctx->l, NULL) != ',' || nexttok (&ctx->l, NULL) != TOK_INT || ctx->l.v.i < 0)
      return false;
    s->long_3 = ctx->l.v.i;
    *valid_data = true;
    return nexttok (&ctx->l, NULL) == ')';
  }
  else
  {
    return false;
  }
}

struct doreadlike_sample {
  dds_time_t ts;
  dds_instance_handle_t wrih;
  uint32_t state;
  int wrent;
  Space_Type1 data;
  bool valid_data;
};

static bool wrname_from_pubhandle (const struct oneliner_ctx *ctx, int ent, dds_instance_handle_t pubhandle, entname_t *wrname)
{
  dds_builtintopic_endpoint_t inf, inf1;
  if (dds_instance_get_key (ctx->pubrd[ent/9], pubhandle, &inf) != 0)
    return false;
  for (int j = 0; j < (int) (sizeof (ctx->doms) / sizeof (ctx->doms[0])); j++)
  {
    for (int k = 6; k < 9; k++)
    {
      if (ctx->esi[9*j+k] != 0)
      {
        if (dds_instance_get_key (ctx->pubrd[j], ctx->esi[9*j+k], &inf1) != 0)
          return false;
        if (memcmp (&inf.key, &inf1.key, sizeof (inf.key)) == 0)
        {
          getentname (wrname, 9*j+k);
          return true;
        }
      }
    }
  }
  return false;
}

static bool doreadlike_parse_sample (struct oneliner_ctx *ctx, struct doreadlike_sample *s)
{
  static const char *statechars = "fsaudno";
  static const uint32_t statemap[] = {
    DDS_NOT_READ_SAMPLE_STATE, DDS_READ_SAMPLE_STATE,
    DDS_ALIVE_INSTANCE_STATE, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE,
    DDS_NEW_VIEW_STATE, DDS_NOT_NEW_VIEW_STATE
  };
  // syntax: [state]k[pubhandle][@ts] or [state](k,l,m)[pubhandle][@ts]
  // the first is an invalid sample, the second a valid one, the third says anything goes
  // state is a combination of: sample state (F,S fresh/stale), instance state (A,U,D), view state (N,O)
  // unspecified: don't care
  s->state = 0;
  s->ts = -1;
  s->wrent = -1;
  s->wrih = 0;
  struct oneliner_lex l1 = ctx->l;
  if (nexttok_if (&ctx->l, TOK_NAME))
  {
    char *inp1 = ctx->l.v.n;
    char *p;
    while (*inp1 && (p = strchr (statechars, *inp1)) != NULL)
    {
      s->state |= statemap[(int) (p - statechars)];
      inp1++;
    }
    if (*inp1 == 0)
      ;
    else if (!isdigit ((unsigned char)*inp1))
      return false;
    else // rewind input to digit
      ctx->l.inp = l1.inp + (inp1 - ctx->l.v.n);
  }
  // missing states: allow everything
  if ((s->state & (statemap[0] | statemap[1])) == 0)
    s->state |= statemap[0] | statemap[1];
  if ((s->state & (statemap[2] | statemap[3] | statemap[4])) == 0)
    s->state |= statemap[2] | statemap[3] | statemap[4];
  if ((s->state & (statemap[5] | statemap[6])) == 0)
    s->state |= statemap[5] | statemap[6];
  if (!parse_sample_value (ctx, &s->data, &s->valid_data, -1))
    return false;
  s->wrent = parse_entity1 (&ctx->l, NULL);
  if (nexttok_if (&ctx->l, TOK_TIMESTAMP))
    s->ts = ctx->l.v.d;
  return true;
}

static bool doreadlike_ismatch (const dds_sample_info_t *si, const Space_Type1 *s, const struct doreadlike_sample *exp)
{
  return (si->valid_data == exp->valid_data &&
          (si->sample_state & exp->state) != 0 &&
          (si->instance_state & exp->state) != 0 &&
          (si->view_state & exp->state) != 0 &&
          (exp->data.long_1 < 0 || s->long_1 == exp->data.long_1) &&
          (!exp->valid_data || exp->data.long_2 < 0 || s->long_2 == exp->data.long_2) &&
          (!exp->valid_data || exp->data.long_3 < 0 || s->long_3 == exp->data.long_3) &&
          (exp->ts < 0 || si->source_timestamp == exp->ts) &&
          (exp->wrent < 0 || si->publication_handle == exp->wrih));
}

static bool doreadlike_matchstep (const dds_sample_info_t *si, const Space_Type1 *s, const struct doreadlike_sample *exp, int nexp, bool ellipsis, unsigned *tomatch, int *cursor, dds_instance_handle_t *lastih, int *matchidx)
{
  if (si->instance_handle != *lastih)
  {
    *lastih = si->instance_handle;
    *cursor = -1;
    for (int m = 0; m < nexp; m++)
    {
      if ((*tomatch & (1u << m)) && s->long_1 == exp[m].data.long_1)
      {
        *cursor = m;
        break;
      }
    }
  }
  if (*cursor < 0 || *cursor >= nexp)
  {
    *matchidx = ellipsis ? nexp : -1;
    return ellipsis;
  }
  else if (doreadlike_ismatch (si, s, &exp[*cursor]))
  {
    *matchidx = *cursor;
    *tomatch &= ~(1u << *cursor);
    (*cursor)++;
    return true;
  }
  else if (ellipsis)
  {
    *matchidx = nexp;
    return true;
  }
  else
  {
    *matchidx = -1;
    return false;
  }
}

static void doreadlike (struct oneliner_ctx *ctx, const char *name, dds_return_t (*fn) (dds_entity_t, void **buf, dds_sample_info_t *, size_t, uint32_t))
{
#define MAXN 10
  struct doreadlike_sample exp[MAXN];
  int nexp = 0;
  bool ellipsis = false;
  int exp_nvalid = -1, exp_ninvalid = -1;
  int ent;
  switch (peektok (&ctx->l, NULL))
  {
    default: // no expectations
      ellipsis = true;
      break;
    case '(': // (# valid, # invalid)
      nexttok (&ctx->l, NULL);
      if (!(nexttok_int (&ctx->l, &exp_nvalid) && nexttok_if (&ctx->l, ',') && nexttok_int (&ctx->l, &exp_ninvalid) && nexttok_if (&ctx->l, ')')))
        error (ctx, "%s: expecting (NINVALID, NVALID)", name);
      ellipsis = true;
      break;
    case '{':
      nexttok (&ctx->l, NULL);
      if (!nexttok_if (&ctx->l, '}'))
      {
        do {
          if (nexttok_if (&ctx->l, TOK_ELLIPSIS)) {
            ellipsis = true; break;
          } else if (nexp == MAXN) {
            error (ctx, "%s: too many samples specified", name);
          } else if (!doreadlike_parse_sample (ctx, &exp[nexp++])) {
            error (ctx, "%s: expecting sample", name);
          }
        } while (nexttok_if (&ctx->l, ','));
        if (!nexttok_if (&ctx->l, '}'))
          error (ctx, "%s: expecting '}'", name);
      }
      break;
  }
  if ((ent = parse_entity1 (&ctx->l, NULL)) < 0)
    error (ctx, "%s: entity required", name);

  for (int i = 0; i < nexp; i++)
  {
    if (exp[i].wrent >= 0 && (exp[i].wrih = lookup_insthandle (ctx, ent, exp[i].wrent)) == 0)
      error (ctx, "%s: instance lookup failed", name);
  }

  mprintf (ctx, "entity %"PRId32": %s: ", ctx->es[ent], (fn == dds_take) ? "take" : "read");
  Space_Type1 data[MAXN];
  void *raw[MAXN];
  for (int i = 0; i < MAXN; i++)
    raw[i] = &data[i];
  int matchidx[MAXN];
  dds_sample_info_t si[MAXN];
  DDSRT_STATIC_ASSERT (MAXN < CHAR_BIT * sizeof (unsigned));
  const uint32_t maxs = (uint32_t) (sizeof (raw) / sizeof (raw[0]));
  const int32_t n = fn (ctx->es[ent], raw, si, maxs, maxs);
  if (n < 0)
    error_dds (ctx, n, "%s: failed on %"PRId32, name, ctx->es[ent]);
  unsigned tomatch = (1u << nexp) - 1; // used to track result entries matched by spec
  dds_instance_handle_t lastih = 0;
  int cursor = -1;
  int count[2] = { 0, 0 };
  bool matchok = true;
  mprintf (ctx, "{");
  for (int i = 0; i < n; i++)
  {
    const Space_Type1 *s = raw[i];
    entname_t wrname;
    count[si[i].valid_data]++;
    mprintf (ctx, "%s%c%c%c",
            (i > 0) ? "," : "",
            (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE) ? 'f' : 's',
            (si[i].instance_state == DDS_ALIVE_INSTANCE_STATE) ? 'a' : (si[i].instance_state == DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE) ? 'u' : 'd',
            (si[i].view_state == DDS_NEW_VIEW_STATE) ? 'n' : 'o');
    if (si[i].valid_data)
      mprintf (ctx, "(%"PRId32",%"PRId32",%"PRId32")", s->long_1, s->long_2, s->long_3);
    else
      mprintf (ctx, "%"PRId32, s->long_1);
    if (!wrname_from_pubhandle (ctx, ent, si[i].publication_handle, &wrname))
      error (ctx, "%s: unknown publication handle received", name);
    mprintf (ctx, "%s", wrname.n);
    print_timestamp (ctx, si[i].source_timestamp);
    if (!doreadlike_matchstep (&si[i], s, exp, nexp, ellipsis, &tomatch, &cursor, &lastih, &matchidx[i]))
      matchok = false;
  }
  mprintf (ctx, "}:");
  for (int i = 0; i < n; i++)
    mprintf (ctx, " %d", matchidx[i]);
  if (tomatch != 0)
  {
    mprintf (ctx, " (samples missing)");
    matchok = false;
  }
  mprintf (ctx, " valid %d %d invalid %d %d", count[1], exp_nvalid, count[0], exp_ninvalid);
  if (exp_nvalid >= 0 && (count[1] != exp_nvalid))
    matchok = false;
  if (exp_ninvalid >= 0 && (count[0] != exp_ninvalid))
    matchok = false;
  mprintf (ctx, "\n");
  if (!matchok)
    testfail (ctx, "%s: mismatch between actual and expected set\n", name);
#undef MAXN
}

static void dotake (struct oneliner_ctx *ctx) { doreadlike (ctx, "take", dds_take); }
static void doread (struct oneliner_ctx *ctx) { doreadlike (ctx, "read", dds_read); }

static void dowritelike (struct oneliner_ctx *ctx, const char *name, bool fail, dds_return_t (*fn) (dds_entity_t wr, const void *sample, dds_time_t ts))
{
  dds_return_t ret;
  dds_time_t ts = dds_time ();
  bool valid_data;
  int ent;
  Space_Type1 sample;
  if ((ent = parse_entity (ctx)) < 0)
    error (ctx, "%s: expecting entity", name);
  DDSRT_WARNING_MSVC_OFF(6385)
  if (ctx->es[ent] == 0)
    make_entity (ctx, ent, NULL);
  DDSRT_WARNING_MSVC_ON(6385)
  if (!parse_sample_value (ctx, &sample, &valid_data, 0))
    error (ctx, "%s: expecting sample value", name);
  if (nexttok_if (&ctx->l, TOK_TIMESTAMP))
    ts = ctx->l.v.d;
  mprintf (ctx, "entity %"PRId32": %s (%"PRId32",%"PRId32",%"PRId32")", ctx->es[ent], name, sample.long_1, sample.long_2, sample.long_3);
  print_timestamp (ctx, ts);
  mprintf (ctx, "\n");
  ret = fn (ctx->es[ent], &sample, ts);
  if (!fail)
  {
    if (ret != 0)
      error_dds (ctx, ret, "%s: failed", name);
  }
  else
  {
    if (ret == 0)
      testfail (ctx, "%s: succeeded unexpectedly", name);
    else if (ret != DDS_RETCODE_TIMEOUT)
      error_dds (ctx, ret, "%s: failed", name);
  }
}

static void dowr (struct oneliner_ctx *ctx) { dowritelike (ctx, "wr", false, dds_write_ts); }
static void dowrfail (struct oneliner_ctx *ctx) { dowritelike (ctx, "wrfail", true, dds_write_ts); }
static void dowrdisp (struct oneliner_ctx *ctx) { dowritelike (ctx, "wrdisp", false, dds_writedispose_ts); }
static void dowrdispfail (struct oneliner_ctx *ctx) { dowritelike (ctx, "wrdispfail", true, dds_writedispose_ts); }
static void dodisp (struct oneliner_ctx *ctx) { dowritelike (ctx, "disp", false, dds_dispose_ts); }
static void dodispfail (struct oneliner_ctx *ctx) { dowritelike (ctx, "dispfail", true, dds_dispose_ts); }
static void dounreg (struct oneliner_ctx *ctx) { dowritelike (ctx, "unreg", false, dds_unregister_instance_ts); }
static void dounregfail (struct oneliner_ctx *ctx) { dowritelike (ctx, "unregfail", true, dds_unregister_instance_ts); }

static int checkstatus (struct oneliner_ctx *ctx, int ll, int ent, struct oneliner_lex *argl, const void *status)
{
  assert (lldesc[ll].desc != NULL);
  const char *d = lldesc[ll].desc;
  int field = 0;
  const char *sep = "(";
  size_t off = 0;
  if (nexttok (argl, NULL) != '(')
    abort ();
  while (*d)
  {
    const void *p = advance (status, &off, *d);
    int i;
    if (peektok (argl, NULL) == '*')
    {
      (void) nexttok (argl, NULL);
      switch (*d)
      {
        case 'n': mprintf (ctx, "%s%"PRIu32, sep, *(uint32_t *)p); break;
        case 'c': mprintf (ctx, "%s%"PRId32, sep, *(int32_t *)p); break;
        case 'P': mprintf (ctx, "%s%"PRIu32, sep, *(uint32_t *)p); break;
        case 'R': mprintf (ctx, "%s%d", sep, (int) *(dds_sample_rejected_status_kind *)p); break;
        case 'I': break; // instance handle is too complicated
        case 'E': mprintf (ctx, "%s%"PRIx64, sep, *(dds_instance_handle_t *)p); break;
        default: return DDS_RETCODE_BAD_PARAMETER;
      }
      mprintf (ctx, " *");
    }
    else
    {
      switch (*d)
      {
        case 'n':
          if (!nexttok_int (argl, &i) || i < 0)
            return setresult (ctx, -1, "checkstatus: field %d expecting non-negative integer", field);
          mprintf (ctx, "%s%"PRIu32" %d", sep, *(uint32_t *)p, i);
          if (*(uint32_t *)p != (uint32_t)i)
            return setresult (ctx, 0, "checkstatus: field %d has actual %"PRIu32" expected %d", field, *(uint32_t *)p, i);
          break;
        case 'c':
          if (!nexttok_int (argl, &i))
            return setresult (ctx, -1, "checkstatus: field %d expecting integer", field);
          mprintf (ctx, "%s%"PRId32" %d", sep, *(int32_t *)p, i);
          if (*(int32_t *)p != i)
            return setresult (ctx, 0, "checkstatus: field %d has actual %"PRId32" expected %d", field, *(int32_t *)p, i);
          break;
        case 'P':
          if (nexttok (argl, NULL) != TOK_NAME)
            return setresult (ctx, -1, "checkstatus: field %d expecting policy name", field);
          size_t polidx;
          for (polidx = 0; polidx < sizeof (qostab) / sizeof (qostab[0]); polidx++)
            if (strcmp (argl->v.n, qostab[polidx].abbrev) == 0)
              break;
          if (polidx == sizeof (qostab) / sizeof (qostab[0]))
            return setresult (ctx, -1, "checkstatus: field %d expecting policy name", field);
          mprintf (ctx, "%s%"PRIu32" %"PRIu32, sep, *(uint32_t *)p, (uint32_t) qostab[polidx].id);
          if (*(uint32_t *)p != (uint32_t) qostab[polidx].id)
            return setresult (ctx, 0, "checkstatus: field %d has actual %"PRIu32" expected %d", field, *(uint32_t *)p, (int) qostab[polidx].id);
          break;
        case 'R':
          if (nexttok (argl, NULL) != TOK_NAME)
            return setresult (ctx, -1, "checkstatus: field %d expecting reason", field);
          if (strcmp (argl->v.n, "i") == 0) i = (int) DDS_REJECTED_BY_INSTANCES_LIMIT;
          else if (strcmp (argl->v.n, "s") == 0) i = (int) DDS_REJECTED_BY_SAMPLES_LIMIT;
          else if (strcmp (argl->v.n, "spi") == 0) i = (int) DDS_REJECTED_BY_SAMPLES_PER_INSTANCE_LIMIT;
          else return setresult (ctx, -1, "checkstatus: field %d expecting reason", field);
          mprintf (ctx, "%s%d %d", sep, (int) *(dds_sample_rejected_status_kind *)p, i);
          if (*(dds_sample_rejected_status_kind *)p != (dds_sample_rejected_status_kind) i)
            return setresult (ctx, 0, "checkstatus: field %d has actual %d expected %d", field, (int) (*(dds_sample_rejected_status_kind *)p), i);
          break;
        case 'I': // instance handle is too complicated
          break;
        case 'E': {
          int ent1 = -1;
          dds_instance_handle_t esi1 = 0;
          if (nexttok_if (argl, '*'))
            ent1 = -1;
          else if ((ent1 = parse_entity1 (argl, NULL)) < 0)
            return setresult (ctx, -1, "checkstatus: field %d expecting * or entity name", field);
          else if ((esi1 = lookup_insthandle (ctx, ent, ent1)) == 0)
            return setresult (ctx, -1, "checkstatus: field %d instance handle lookup failed", field);
          mprintf (ctx, "%s%"PRIx64" %"PRIx64, sep, *(dds_instance_handle_t *)p, esi1);
          if (ent1 >= 0 && *(dds_instance_handle_t *)p != esi1)
            return setresult (ctx, 0, "checkstatus: field %d has actual %"PRIx64" expected %"PRIx64, field, *(dds_instance_handle_t *)p, esi1);
          break;
        }
        default:
          return DDS_RETCODE_BAD_PARAMETER;
      }
    }
    sep = ", ";
    if (*d != 'I')
      field++;
    ++d;
    if (*d && *d != 'I' && !nexttok_if (argl, ','))
      return setresult (ctx, -1, "checkstatus: field %d expecting ','", field);
  }
  mprintf (ctx, ")");
  if (!nexttok_if (argl, ')'))
    return setresult (ctx, -1, "checkstatus: field %d expecting ')'", field);
  assert (off <= lldesc[ll].size);
  return 1;
}

static void checklistener (struct oneliner_ctx *ctx, int ll, int ent, struct oneliner_lex *argl)
{
  bool signalled = true;
  uint32_t min_cnt = 1, max_cnt = UINT32_MAX;
  uint32_t status;
  const int dom = ent / 9;
  dds_return_t ret;
  mprintf (ctx, "listener %s: check called for entity %"PRId32, lldesc[ll].name, ctx->es[ent]);
  if (argl && lldesc[ll].cb_status_off == 0)
  {
    // those that don't have a status can check the number of invocations
    int cnt = -1;
    if (!(nexttok_if (argl, '(') && nexttok_int (argl, &cnt) && nexttok_if (argl, ')')))
      error (ctx, "listener %s: expecting (COUNT)", lldesc[ll].name);
    if (cnt < 0)
      error (ctx, "listener %s: invocation count must be at least 0", lldesc[ll].name);
    min_cnt = max_cnt = (uint32_t) cnt;
  }
  ddsrt_mutex_lock (&ctx->g_mutex);
  const dds_time_t twait_begin = dds_time ();
  bool cnt_ok = (ctx->cb[dom].cb_called[lldesc[ll].id] >= min_cnt && ctx->cb[dom].cb_called[lldesc[ll].id] <= max_cnt);
  while (ctx->cb[dom].cb_called[lldesc[ll].id] < min_cnt && signalled)
  {
    signalled = ddsrt_cond_waitfor (&ctx->g_cond, &ctx->g_mutex, DDS_SECS (5));
    cnt_ok = (ctx->cb[dom].cb_called[lldesc[ll].id] >= min_cnt && ctx->cb[dom].cb_called[lldesc[ll].id] <= max_cnt);
  }
  const dds_time_t twait_end = dds_time ();
  const dds_duration_t dt = (twait_end - twait_begin);
  mprintf (ctx, " cb_called %"PRIu32" (%s) after %"PRId64".%06us", ctx->cb[dom].cb_called[lldesc[ll].id], cnt_ok ? "ok" : "fail", dt / DDS_NSECS_IN_SEC, (unsigned) (dt % DDS_NSECS_IN_SEC) / 1000);
  if (!cnt_ok)
  {
    ddsrt_mutex_unlock (&ctx->g_mutex);
    testfail (ctx, "listener %s: not invoked [%"PRIu32",%"PRIu32"] times", lldesc[ll].name, min_cnt, max_cnt);
  }
  dds_entity_t * const cb_entity = (dds_entity_t *) ((char *) &ctx->cb[dom] + lldesc[ll].cb_entity_off);
  mprintf (ctx, " cb_entity %"PRId32" %"PRId32" (%s)", *cb_entity, ctx->es[ent], (*cb_entity == ctx->es[ent]) ? "ok" : "fail");
  if (*cb_entity != ctx->es[ent])
  {
    ddsrt_mutex_unlock (&ctx->g_mutex);
    testfail (ctx, "listener %s: invoked on %"PRId32" instead of %"PRId32, lldesc[ll].name, *cb_entity, ctx->es[ent]);
  }
  if (!(ctx->doms[0] && ctx->doms[1]))
  {
    // FIXME: two domains: listener invocation happens on another thread and we can observe non-0 "change" fields
    // they get updated, listener gets invoked, then they get reset -- pretty sure it is allowed by the spec, but
    // not quite elegant
    if ((ret = check_status_change_fields_are_0 (ll, ctx->es[ent])) <= 0)
    {
      ddsrt_mutex_unlock (&ctx->g_mutex);
      if (ret == 0)
        testfail (ctx, "listener %s: status contains non-zero change fields", lldesc[ll].name);
      else if (ret < 0)
        error_dds (ctx, ret, "listener %s: get entity status failed", lldesc[ll].name);
    }
  }
  if (argl && lldesc[ll].cb_status_off != 0)
  {
    void *cb_status = (char *) &ctx->cb[dom] + lldesc[ll].cb_status_off;
    if (checkstatus (ctx, ll, ent, argl, cb_status) <= 0)
    {
      ddsrt_mutex_unlock (&ctx->g_mutex);
      longjmp (ctx->jb, 1);
    }
  }
  mprintf (ctx, "\n");
  ctx->cb[dom].cb_called[lldesc[ll].id] = 0;
  ddsrt_mutex_unlock (&ctx->g_mutex);
  if ((ret = dds_get_status_changes (ctx->es[ent], &status)) != 0)
    error_dds (ctx, ret, "listener %s: dds_get_status_change on %"PRId32, lldesc[ll].name, ctx->es[ent]);
  if ((status & (1u << lldesc[ll].id)) != 0)
    testfail (ctx, "listener %s: status mask not cleared", lldesc[ll].name);
}

static void dowaitforack (struct oneliner_ctx *ctx)
{
  dds_return_t ret;
  int ent, ent1 = -1;
  union { dds_guid_t x; ddsi_guid_t i; } rdguid;
  if (*ctx->l.inp == '(') // reader present
  {
    nexttok (&ctx->l, NULL);
    if ((ent1 = parse_entity (ctx)) < 0)
      error (ctx, "wait for ack: expecting entity");
    if ((ent1 % 9) < 3 || (ent1 % 9) > 5 || ctx->es[ent1] == 0)
      error (ctx, "wait for ack: expecting existing reader as argument");
    if ((ret = dds_get_guid (ctx->es[ent1], &rdguid.x)) != 0)
      error_dds (ctx, ret, "wait for ack: failed to get GUID for reader %"PRId32, ctx->es[ent1]);
    rdguid.i = ddsi_ntoh_guid (rdguid.i);
    if (!nexttok_if (&ctx->l, ')'))
      error (ctx, "wait for ack: expecting ')'");
  }
  if ((ent = parse_entity (ctx)) < 0)
    error (ctx, "wait for ack: expecting writer");
  if (ent1 >= 0 && ent / 9 == ent1 / 9)
    error (ctx, "wait for ack: reader and writer must be in different domains");
  DDSRT_WARNING_MSVC_OFF(6385)
  if (ctx->es[ent] == 0)
    make_entity (ctx, ent, NULL);
  DDSRT_WARNING_MSVC_ON(6385)
  mprintf (ctx, "wait for ack %"PRId32" reader %"PRId32"\n", ctx->es[ent], ent1 < 0 ? 0 : ctx->es[ent1]);

  // without a reader argument a simple dds_wait_for_acks (ctx->es[ent], DDS_SECS (5)) suffices
  struct dds_entity *x;
  if ((ret = dds_entity_pin (ctx->es[ent], &x)) < 0)
    error_dds (ctx, ret, "wait for ack: pin entity failed %"PRId32, ctx->es[ent]);
  if (dds_entity_kind (x) != DDS_KIND_WRITER)
    error_dds (ctx, ret, "wait for ack: %"PRId32" is not a writer", ctx->es[ent]);
  else
    ret = dds__ddsi_writer_wait_for_acks ((struct dds_writer *) x, (ent1 < 0) ? NULL : &rdguid.i, dds_time () + DDS_SECS (5));
  dds_entity_unpin (x);
  if (ret != 0)
  {
    if (ret == DDS_RETCODE_TIMEOUT)
      testfail (ctx, "wait for acks timed out on entity %"PRId32, ctx->es[ent]);
    else
      error_dds (ctx, ret, "wait for acks failed on entity %"PRId32, ctx->es[ent]);
  }
}

static void dowaitfornolistener (struct oneliner_ctx *ctx, int ll)
{
  mprintf (ctx, "listener %s: check not called", lldesc[ll].name);
  ddsrt_mutex_lock (&ctx->g_mutex);
  bool ret = true;
  for (int i = 0; i < (int) (sizeof (ctx->doms) / sizeof (ctx->doms[0])); i++)
  {
    mprintf (ctx, " %"PRIu32, ctx->cb[i].cb_called[lldesc[ll].id]);
    if (ctx->cb[i].cb_called[lldesc[ll].id] != 0)
      ret = false;
  }
  mprintf (ctx, " (%s)\n", ret ? "ok" : "fail");
  ddsrt_mutex_unlock (&ctx->g_mutex);
  if (!ret)
    testfail (ctx, "callback %s invoked unexpectedly", lldesc[ll].name);
}

static void dowaitforlistener (struct oneliner_ctx *ctx, int ll)
{
  struct oneliner_lex l1 = ctx->l;
  // no whitespace between name and args
  const bool have_args = (*ctx->l.inp == '(');
  if (have_args)
  {
    // skip args: we need the entity before we can interpret them
    int tok;
    while ((tok = nexttok (&ctx->l, NULL)) != EOF && tok != ')')
      ;
  }
  const int ent = parse_entity (ctx);
  if (ent < 0)
    error (ctx, "check listener: requires an entity");
  DDSRT_WARNING_MSVC_OFF(6385)
  if (ctx->es[ent] == 0)
    setlistener (ctx, NULL, ll, ent);
  DDSRT_WARNING_MSVC_ON(6385)
  checklistener (ctx, ll, ent, have_args ? &l1 : NULL);
}

static void dowait (struct oneliner_ctx *ctx)
{
  union oneliner_tokval tokval;
  if (peektok (&ctx->l, &tokval) == TOK_NAME && strcmp (tokval.n, "ack") == 0)
  {
    nexttok (&ctx->l, NULL);
    dowaitforack (ctx);
  }
  else
  {
    const bool expectclear = nexttok_if (&ctx->l, '!');
    const int ll = parse_listener (ctx);
    if (ll < 0)
      error (ctx, "check listener: requires listener name");
    if (expectclear)
      dowaitfornolistener (ctx, ll);
    else
      dowaitforlistener (ctx, ll);
  }
}

DDSRT_WARNING_MSVC_OFF(6385)
DDSRT_WARNING_MSVC_OFF(6386)
static void dodelete (struct oneliner_ctx *ctx)
{
  dds_return_t ret;
  int ent;
  if ((ent = parse_entity (ctx)) < 0)
    error (ctx, "delete: requires entity");
  if ((ret = dds_delete (ctx->es[ent])) != 0)
    error_dds (ctx, ret, "delete: failed on %"PRId32, ctx->es[ent]);
  ctx->es[ent] = 0;
  // clear dependent entities
  int a, b;
  switch (ent % 9)
  {
    case 0: /* pp => everything */    a = 1; b = 8; break;
    case 1: /* sub => readers 3..5 */ a = 2; b = 4; break;
    case 2: /* pub => writers 6..8 */ a = 4; b = 6; break;
    default: a = 0; b = -1; break;
  }
  for (int dep = a; dep <= b; dep++)
    ctx->es[ent + dep] = 0;
}
DDSRT_WARNING_MSVC_ON(6386)
DDSRT_WARNING_MSVC_ON(6385)

static void dodeaf_maybe_imm (struct oneliner_ctx *ctx, bool immediate)
{
  char const * const mode = immediate ? "deaf!" : "deaf";
  dds_return_t ret;
  entname_t name;
  int ent;
  if ((ent = parse_entity (ctx)) < 0 || (ent % 9) != 0)
    error (ctx, "%s: requires participant", mode);
  mprintf (ctx, "%s: %s\n", mode, getentname (&name, ent));
  DDSRT_WARNING_MSVC_OFF(6385)
  if ((ret = dds_domain_set_deafmute (ctx->es[ent], true, false, DDS_INFINITY)) != 0)
    error_dds (ctx, ret, "deaf: dds_domain_set_deafmute failed on %"PRId32, ctx->es[ent]);
  DDSRT_WARNING_MSVC_ON(6385)
  if (immediate)
  {
    // speed up the process by forcing lease expiry
    dds_entity *x, *xprime;
    if ((ret = dds_entity_pin (ctx->es[ent], &x)) < 0)
      error_dds (ctx, ret, "%s: pin participant failed %"PRId32, mode, ctx->es[ent]);
    for (int i = 0; i < (int) (sizeof (ctx->doms) / sizeof (ctx->doms[0])); i++)
    {
      if (i == ent / 9 || ctx->es[9*i] == 0)
        continue;
      if ((ret = dds_entity_pin (ctx->es[9*i], &xprime)) < 0)
      {
        dds_entity_unpin (x);
        error_dds (ctx, ret, "%s: pin counterpart participant failed %"PRId32, mode, ctx->es[9*i]);
      }
      ddsi_thread_state_awake (ddsi_lookup_thread_state (), &x->m_domain->gv);
      ddsi_delete_proxy_participant_by_guid (&x->m_domain->gv, &xprime->m_guid, ddsrt_time_wallclock (), true);
      ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
      dds_entity_unpin (xprime);
    }
    dds_entity_unpin (x);
  }
}

static void dodeaf (struct oneliner_ctx *ctx)
{
  const bool immediate = nexttok_if (&ctx->l, '!');
  dodeaf_maybe_imm (ctx, immediate);
}

static void dohearing_maybe_imm (struct oneliner_ctx *ctx, bool immediate)
{
  char const * const mode = immediate ? "hearing!" : "hearing";
  dds_return_t ret;
  entname_t name;
  int ent;
  if ((ent = parse_entity (ctx)) < 0 || (ent % 9) != 0)
    error (ctx, "%s: requires participant", mode);
  mprintf (ctx, "%s: %s\n", mode, getentname (&name, ent));
  DDSRT_WARNING_MSVC_OFF(6385)
  if ((ret = dds_domain_set_deafmute (ctx->es[ent], false, false, DDS_INFINITY)) != 0)
    error_dds (ctx, ret, "%s: dds_domain_set_deafmute failed %"PRId32, mode, ctx->es[ent]);
  DDSRT_WARNING_MSVC_ON(6385)
  if (immediate)
  {
    // speed up the process by forcing SPDP publication on the remote
    for (int i = 0; i < (int) (sizeof (ctx->doms) / sizeof (ctx->doms[0])); i++)
    {
      if (i == ent / 9 || ctx->es[9*i] == 0)
        continue;
      dds_entity *xprime;
      struct ddsi_participant *pp;
      if ((ret = dds_entity_pin (ctx->es[9*i], &xprime)) < 0)
        error_dds (ctx, ret, "%s: pin counterpart participant failed %"PRId32, mode, ctx->es[9*i]);
      ddsi_thread_state_awake (ddsi_lookup_thread_state (), &xprime->m_domain->gv);
      if ((pp = ddsi_entidx_lookup_participant_guid (xprime->m_domain->gv.entity_index, &xprime->m_guid)) != NULL)
        ddsi_resched_xevent_if_earlier (pp->spdp_xevent, ddsrt_mtime_add_duration (ddsrt_time_monotonic (), DDS_MSECS (100)));
      ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
      dds_entity_unpin (xprime);
    }
  }
}

static void dohearing (struct oneliner_ctx *ctx)
{
  const bool immediate = nexttok_if (&ctx->l, '!');
  dohearing_maybe_imm (ctx, immediate);
}

static void dosleep (struct oneliner_ctx *ctx)
{
  if (nexttok_dur (&ctx->l, NULL, true) != TOK_DURATION)
    error (ctx, "sleep: invalid duration");
  dds_sleepfor (ctx->l.v.d);
}

static void dosetflags (struct oneliner_ctx *ctx)
{
  dds_return_t ret;
  entname_t name;
  int ent;
  int tok;
  union oneliner_tokval flagstok;
  if (nexttok (&ctx->l, NULL) != '(')
    error (ctx, "setflags: args required");
  if ((tok = nexttok (&ctx->l, &flagstok)) == TOK_NAME)
    tok = nexttok (&ctx->l, NULL);
  else
    flagstok.n[0] = 0;
  if (tok != ')')
    error (ctx, "setflags: invalid argument");
  if ((ent = parse_entity (ctx)) < 0)
    error (ctx, "setflags: requires writer");
  if (ctx->es[ent] == 0)
    make_entity (ctx, ent, NULL);
  mprintf (ctx, "setflags(%s): %s\n", flagstok.n, getentname (&name, ent));

  dds_entity *xwr;
  if ((ret = dds_entity_pin (ctx->es[ent], &xwr)) < 0)
    error_dds (ctx, ret, "setflags: pin writer failed %"PRId32, ctx->es[ent]);
  if (xwr->m_kind != DDS_KIND_WRITER)
  {
    dds_entity_unpin (xwr);
    error (ctx, "setflags: entity is not a writer");
  }
  dds_writer *wr = (dds_writer *) xwr;
  if (strspn (flagstok.n, "arhd") != strlen (flagstok.n))
  {
    dds_entity_unpin (xwr);
    error (ctx, "setflags: unknown flags");
  }
  wr->m_wr->test_ignore_acknack = (strchr (flagstok.n, 'a') != NULL);
  wr->m_wr->test_suppress_retransmit = (strchr (flagstok.n, 'r') != NULL);
  wr->m_wr->test_suppress_heartbeat = (strchr (flagstok.n, 'h') != NULL);
  wr->m_wr->test_drop_outgoing_data = (strchr (flagstok.n, 'd') != NULL);
  dds_entity_unpin (xwr);
}

DDSRT_WARNING_MSVC_OFF(6385)
DDSRT_WARNING_MSVC_OFF(6001)
static void docheckstatus (struct oneliner_ctx *ctx)
{
  const int ll = parse_listener (ctx);
  if (ll < 0)
    error (ctx, "checkstatus: requires listener name");
  if (lldesc[ll].cb_status_off == 0)
    error (ctx, "checkstatus: listener %s has no status", lldesc[ll].name);
  struct oneliner_lex l1 = ctx->l;
  if (*ctx->l.inp != '(')
    error (ctx, "checkstatus: missing arguments");
  int tok;
  while ((tok = nexttok (&ctx->l, NULL)) != EOF && tok != ')')
    ;
  const int ent = parse_entity (ctx);
  if (ent < 0)
    error (ctx, "check listener: requires an entity");
  if (ctx->es[ent] == 0)
    make_entity (ctx, ent, NULL);
  entname_t name;
  mprintf (ctx, "status(%s %s): ", lldesc[ll].name, getentname (&name, ent));

  void *status = malloc (lldesc[ll].size);
  dds_return_t ret;
  if ((ret = get_status (ll, ctx->es[ent], status)) < 0)
  {
    free (status);
    error_dds (ctx, ret, "get_status failed");
  }
  if (checkstatus (ctx, ll, ent, &l1, status) <= 0)
  {
    free (status);
    longjmp (ctx->jb, 1);
  }
  free (status);
  mprintf (ctx, "\n");
}
DDSRT_WARNING_MSVC_ON(6001)
DDSRT_WARNING_MSVC_ON(6385)

static void dispatchcmd (struct oneliner_ctx *ctx)
{
  static const struct {
    const char *name;
    void (*fn) (struct oneliner_ctx *ct);
  } cs[] = {
    { "-",          dodelete },
    { "?",          dowait },
    { "status",     docheckstatus },
    { "wr",         dowr },
    { "wrdisp",     dowrdisp },
    { "disp",       dodisp },
    { "unreg",      dounreg },
    { "wrfail",     dowrfail },
    { "wrdispfail", dowrdispfail },
    { "dispfail",   dodispfail },
    { "unregfail",  dounregfail },
    { "take",       dotake },
    { "read",       doread },
    { "deaf",       dodeaf },
    { "hearing",    dohearing },
    { "sleep",      dosleep },
    { "setflags",   dosetflags },
  };
  size_t i;
  if (ctx->l.tok > 0)
  {
    // convert single-character token to string
    ctx->l.v.n[0] = (char) ctx->l.tok;
    ctx->l.v.n[1] = 0;
  }
  for (i = 0; i < sizeof (cs) / sizeof (cs[0]); i++)
    if (strcmp (ctx->l.v.n, cs[i].name) == 0)
      break;
  if (i == sizeof (cs) / sizeof (cs[0]))
    error (ctx, "%s: unknown command", ctx->l.v.n);
  cs[i].fn (ctx);
}

static void dosetlistener (struct oneliner_ctx *ctx, int ll)
{
  int ent;
  struct oneliner_lex l1 = ctx->l;
  // scan past listener names to get at the entity, which we need
  // to get the right listener object (and hence argument)
  while (parse_listener1 (&ctx->l) >= 0)
    ;
  if ((ent = parse_entity (ctx)) < 0)
    error (ctx, "set listener: entity required");
  setlistener (ctx, &l1, ll, ent);
}

static void test_oneliner_step1 (struct oneliner_ctx *ctx)
{
  while (peektok (&ctx->l, NULL) != TOK_END)
  {
    int ent, ll;
    if (nexttok_if (&ctx->l, ';'))
      ; // skip ;s
    else if ((ent = parse_entity (ctx)) >= 0)
      make_entity (ctx, ent, NULL);
    else if ((ll = parse_listener (ctx)) >= 0)
      dosetlistener (ctx, ll);
    else if (nexttok (&ctx->l, NULL) == TOK_NAME || ctx->l.tok > 0)
      dispatchcmd (ctx);
    else
      error (ctx, "unexpected token %d", ctx->l.tok);
  }
}

void test_oneliner_init (struct oneliner_ctx *ctx, const char *config_override)
{
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_MSECS (100));
  dds_qset_destination_order (qos, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);

  *ctx = (struct oneliner_ctx) {
    .l = { .tref = dds_time () },
    .qos = qos,
    .entqos = dds_create_qos (),
    .result = 1,
    .config_override = config_override,
    .cb = {
      [0] = { .ctx = ctx, .list = dds_create_listener (&ctx->cb[0]) },
      [1] = { .ctx = ctx, .list = dds_create_listener (&ctx->cb[1]) },
      [2] = { .ctx = ctx, .list = dds_create_listener (&ctx->cb[2]) }
    }
  };

  ctx->mprintf_needs_timestamp = 1;
  ddsrt_mutex_init (&ctx->g_mutex);
  ddsrt_cond_init (&ctx->g_cond);

  create_unique_topic_name ("test_oneliner", ctx->topicname, sizeof (ctx->topicname));
}

int test_oneliner_step (struct oneliner_ctx *ctx, const char *ops)
{
  if (ctx->result > 0 && setjmp (ctx->jb) == 0)
  {
    ctx->l.inp = ops;
    test_oneliner_step1 (ctx);
  }
  return ctx->result;
}

const char *test_oneliner_message (const struct oneliner_ctx *ctx)
{
  return ctx->msg;
}

int test_oneliner_fini (struct oneliner_ctx *ctx)
{
  for (size_t i = 0; i < sizeof (ctx->cb) / sizeof (ctx->cb[0]); i++)
    dds_delete_listener (ctx->cb[i].list);
  dds_delete_qos (ctx->entqos);
  dds_delete_qos ((dds_qos_t *) ctx->qos);
  // prevent any listeners from being invoked so we can safely delete the
  // mutex and the condition variable -- must do this going down the
  // hierarchy, or listeners may remain set through inheritance
  dds_return_t ret;
  for (size_t i = 0; i < sizeof (ctx->es) / sizeof (ctx->es[0]); i++)
    if (ctx->es[i] && (ret = dds_set_listener (ctx->es[i], NULL)) != 0)
      setresult (ctx, ret, "terminate: reset listener failed on %"PRId32, ctx->es[i]);
  if (ctx->result == 0)
  {
    mprintf (ctx, "\n-- dumping content of readers after failure --\n");
    for (int i = 0; i < (int) (sizeof (ctx->doms) / sizeof (ctx->doms[0])); i++)
    {
      for (int j = 3; j <= 5; j++)
      {
        if (ctx->es[9*i + j])
        {
          const char *inp_orig = ctx->l.inp;
          entname_t n;
          ctx->l.inp = getentname (&n, 9*i + j);
          doreadlike (ctx, "read", dds_read);
          ctx->l.inp = inp_orig;
        }
      }
    }
  }
  ddsrt_mutex_destroy (&ctx->g_mutex);
  ddsrt_cond_destroy (&ctx->g_cond);
  for (size_t i = 0; i < sizeof (ctx->doms) / sizeof (ctx->doms[0]); i++)
    if (ctx->doms[i] && (ret = dds_delete (ctx->doms[i])) != 0)
      setresult (ctx, ret, "terminate: delete domain on %"PRId32, ctx->doms[i]);
  return ctx->result;
}

int test_oneliner_with_config (const char *ops, const char *config_override)
{
  struct oneliner_ctx ctx;
  test_oneliner_init (&ctx, config_override);
  mprintf (&ctx, "dotest: %s\n", ops);
  test_oneliner_step (&ctx, ops);
  if (test_oneliner_fini (&ctx) <= 0)
    mfprintf (&ctx, stderr, "FAIL: %s\n", test_oneliner_message (&ctx));
  return ctx.result;
}

int test_oneliner (const char *ops)
{
  return test_oneliner_with_config (ops, NULL);
}

int test_oneliner_no_shm (const char *ops)
{
#ifdef DDS_HAS_SHM
  const char *config_override = "<Domain id=\"any\"><SharedMemory><Enable>false</Enable></SharedMemory></Domain>";
#else
  const char *config_override = NULL;
#endif
  return test_oneliner_with_config (ops, config_override);
}
