// Copyright(c) 2020 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/dds.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "ddsi__addrset.h"
#include "ddsi__entity.h"
#include "ddsi__xevent.h"
#include "dds__entity.h"
#include "dds__serdata_default.h"
#include "dds__psmx.h"

#include "config_env.h"
#include "test_common.h"
#include "Array100.h"
#include "DynamicData.h"
#include "PsmxDataModels.h"

const uint32_t test_index_start = 0;
const uint32_t test_index_end = UINT32_MAX;

static const struct psmx_locator {
  unsigned char a[16];
} psmx_locators[] = {
  {{1}}, {{1}}, {{2}}, {{2}}
};
#define MAX_DOMAINS 4
#define MAX_READERS_PER_DOMAIN 2

static bool failed;

static void fail (void) { failed = true; }
static void fail_match (void) { fail (); }
static void fail_addrset (void) { fail (); }
static void fail_valid_data (void) { fail (); }
static void fail_instance_state (void) { fail (); }
static void fail_timestamp (void) { fail (); }
static void fail_content (void) { fail (); }
static void fail_no_data (void) { fail (); }
static void fail_too_much_data (void) { fail (); }

#if 0
#define TRACE_CATEGORY "trace,rhc"
#else
#define TRACE_CATEGORY "discovery"
#endif

static dds_entity_t create_participant (dds_domainid_t int_dom)
{
  assert (int_dom < MAX_DOMAINS);
  const unsigned char *l = psmx_locators[int_dom].a;
  char *configstr;
  ddsrt_asprintf (&configstr, "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
  <AllowMulticast>spdp</AllowMulticast>\
  <Interfaces>\
    <PubSubMessageExchange name=\"${CDDS_PSMX_NAME:-cdds}\" library=\"psmx_${CDDS_PSMX_NAME:-cdds}\" priority=\"1000000\" config=\"LOCATOR=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x;SERVICE_NAME=psmx%d;KEYED_TOPICS=true;\" />\
  </Interfaces>\
</General>\
<Discovery>\
  <Tag>${CYCLONEDDS_PID}</Tag>\
  <ExternalDomainId>0</ExternalDomainId>\
</Discovery>\
<Tracing>\
  <Category>" TRACE_CATEGORY "</Category>\
  <OutputFile>cdds.log.%d</OutputFile>\
</Tracing>\
",
    l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8], l[9], l[10], l[11], l[12], l[13], l[14], l[15],
    (int) l[0],   // This prevents Iceoryx and Cyclone-based plugins from forwarding across the "network"
    (int) int_dom // log file name
  );
  char *xconfigstr = ddsrt_expand_envvars (configstr, int_dom);
  const dds_entity_t dom = dds_create_domain (int_dom, xconfigstr);
  ddsrt_free (xconfigstr);
  ddsrt_free (configstr);
  CU_ASSERT_FATAL (dom > 0);
  const dds_entity_t pp = dds_create_participant (int_dom, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  return pp;
}

struct tracebuf {
  char buf[512];
  size_t pos;
};

static void print (struct tracebuf *tb, const char *fmt, ...) ddsrt_attribute_format_printf(2, 3);

static bool endpoint_has_psmx_enabled (dds_entity_t rd_or_wr)
{
  dds_return_t rc;
  struct dds_entity *x;
  bool psmx_enabled = false;
  rc = dds_entity_pin (rd_or_wr, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  switch (dds_entity_kind (x))
  {
    case DDS_KIND_READER: {
      struct dds_reader const * const rd = (struct dds_reader *) x;
      psmx_enabled = (rd->m_endpoint.psmx_endpoints.length > 0);
      break;
    }
    case DDS_KIND_WRITER: {
      struct dds_writer const * const wr = (struct dds_writer *) x;
      psmx_enabled = (wr->m_endpoint.psmx_endpoints.length > 0);
      break;
    }
    default: {
      CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER || dds_entity_kind (x) == DDS_KIND_WRITER);
      break;
    }
  }
  dds_entity_unpin (x);
  return psmx_enabled;
}

static bool psmx_supports_keys (dds_entity_t pp)
{
  // Default writer QoS: reliable, keep last 1, by-reception-timestamp, one non-wildcard partition,
  // absolute nothing fancy in short.  If PSMX doesn't support that, we have a problem.
  char checktopicname[100];
  create_unique_topic_name ("test_psmx_keys", checktopicname, sizeof (checktopicname));
  const dds_entity_t tp = dds_create_topic (pp, &PsmxKeySupportCheck_desc, checktopicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);
  const dds_entity_t wr = dds_create_writer (pp, tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);
  const bool type_supported = endpoint_has_psmx_enabled (wr);
  dds_return_t rc = dds_delete (wr);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (tp);
  CU_ASSERT_FATAL (rc == 0);
  return type_supported;
}

static uint32_t reader_unicast_port (dds_entity_t rdhandle)
{
  dds_return_t rc;
  struct dds_entity *x;
  rc = dds_entity_pin (rdhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER);
  uint32_t port = x->m_domain->gv.loc_default_uc.port;
  dds_entity_unpin (x);
  // FIXME test code assumes non-Iceoryx means port > 0
  CU_ASSERT_FATAL (port > 0);
  return port;
}

struct check_writer_addrset_helper_arg {
  uint32_t ports_seen;
  bool unexpected_present;
  struct tracebuf *tb;
  int nports;
  const uint32_t *ports;
};

static void check_writer_addrset_helper (const ddsi_xlocator_t *loc, void *varg)
{
  struct check_writer_addrset_helper_arg * const arg = varg;
  // PSMX locators are not allowed in writer's address set because that causes it to go through the transmit path
  CU_ASSERT_FATAL (loc->c.kind != DDSI_LOCATOR_KIND_PSMX);
  CU_ASSERT_FATAL (loc->c.port != 0);
  int i;
  if (arg->tb)
    print (arg->tb, "[%"PRIu32"]", loc->c.port);
  for (i = 0; i < arg->nports; i++)
  {
    if (arg->ports[i] == loc->c.port)
    {
      CU_ASSERT_FATAL ((arg->ports_seen & (1u << i)) == 0);
      arg->ports_seen |= 1u << i;
      break;
    }
  }
  // Unknown expected ports not allowed but it can happen temporarily:
  // - when we switch from a dds to a psmx writer, we start with just one local reader, but
  // - the discovery of the demise of the old readers may not have taken place yet at the time of creation of the writer, and
  // - acknowledgements had been received for all published data, and
  // - consequently the writer was matched with the old readers, and then
  // - the discovery of said demise happens just prior to fetching the list of matched subscriptions, but
  // - also prior to the GC performing the actual unmatching of the proxy readers
  // and in that case, the test code will see the correct set of readers, but there are still some ports
  // lingering around.  It is very unlikely, but it *can* happen.  We will just have to restart the check,
  // because the interface offers no good alternatives (total_count won't work because it doesn't count
  // deletes, changing get_matched_subscriptions to include the ones being ferried over would mean storing
  // additional data only for this weird case, etc.)
  if (i == arg->nports)
  {
    if (arg->tb)
      print (arg->tb, "[unexpected port %"PRIu32"]", loc->c.port);
    arg->unexpected_present = true;
  }
}

static bool check_writer_addrset_once (struct tracebuf *tb, dds_entity_t wrhandle, int nports, const uint32_t *ports)
{
  dds_return_t rc;
  struct dds_entity *x;
  rc = dds_entity_pin (wrhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_WRITER);
  struct ddsi_writer * const wr = ((struct dds_writer *) x)->m_wr;
  CU_ASSERT_FATAL (nports < 31);
  struct check_writer_addrset_helper_arg arg = {
    .ports_seen = 0,
    .tb = tb,
    .unexpected_present = false,
    .nports = nports,
    .ports = ports
  };
  ddsrt_mutex_lock (&wr->e.lock);
  ddsi_addrset_forall (wr->as, check_writer_addrset_helper, &arg);
  ddsrt_mutex_unlock (&wr->e.lock);
  dds_entity_unpin (x);
  return (arg.ports_seen == (1u << nports) - 1) && !arg.unexpected_present;
}

static bool check_writer_addrset (struct tracebuf *tb, dds_entity_t wrhandle, int nports, const uint32_t *ports)
{
  const dds_time_t abstimeout = dds_time () + DDS_SECS (10);
  bool exclaimed = false;
  do {
    if (check_writer_addrset_once (NULL, wrhandle, nports, ports))
      return true;
    // this is such a crazily unlikely scenario a sleep is fine
    if (!exclaimed) { print (tb, "!"); exclaimed = true; }
    dds_sleepfor (DDS_MSECS (10));
  } while (dds_time () < abstimeout);
  return check_writer_addrset_once (tb, wrhandle, nports, ports);
}

static dds_entity_t create_endpoint (dds_entity_t tp, bool use_psmx, dds_entity_t (*f) (dds_entity_t pp, dds_entity_t tp, const dds_qos_t *qos, const dds_listener_t *listener))
{
  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, 0);
  // Use depth = 2 so that we can observe stuttering if the fastpath/slowpath
  // fails to correctly skip PSMX readers
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, 2);
  dds_qset_writer_data_lifecycle (qos, false);
  if (!use_psmx)
    dds_qset_psmx_instances (qos, 0, NULL);
  dds_entity_t ep = f (dds_get_participant (tp), tp, qos, NULL);
  CU_ASSERT_FATAL (ep > 0);
  dds_delete_qos (qos);
  CU_ASSERT_FATAL (endpoint_has_psmx_enabled (ep) == use_psmx);
  return ep;
}

static dds_entity_t create_reader (dds_entity_t tp, bool use_psmx)
{
  return create_endpoint (tp, use_psmx, dds_create_reader);
}

static dds_entity_t create_writer (dds_entity_t tp, bool use_psmx)
{
  return create_endpoint (tp, use_psmx, dds_create_writer);
}

static void print (struct tracebuf *tb, const char *fmt, ...)
{
  //return;
  if (tb->pos >= sizeof (tb->buf))
    abort ();

  va_list ap;
  va_start (ap, fmt);
  int pos = vsnprintf (tb->buf + tb->pos, sizeof (tb->buf) - tb->pos, fmt, ap);
  va_end (ap);
  if (pos < 0)
    pos = 0;

  fputs (tb->buf + tb->pos, stdout); fflush (stdout);
  tb->pos += (size_t) pos;
}

static int get_current_match_count (dds_entity_t rd_or_wr)
{
  dds_return_t rc;
  dds_publication_matched_status_t pm;
  dds_subscription_matched_status_t sm;
  rc = dds_get_publication_matched_status (rd_or_wr, &pm);
  if (rc == 0)
    return (int) pm.current_count;
  else if (rc == DDS_RETCODE_ILLEGAL_OPERATION)
  {
    rc = dds_get_subscription_matched_status (rd_or_wr, &sm);
    CU_ASSERT_FATAL (rc == 0);
    CU_ASSERT_FATAL (sm.current_count == 0 || sm.current_count == 1);
    return (int) sm.current_count;
  }
  else
  {
    CU_ASSERT_FATAL (rc == 0 || rc == DDS_RETCODE_ILLEGAL_OPERATION);
    return -1;
  }
}

static bool allmatched (dds_entity_t ws, dds_entity_t wr, int nrds, const dds_entity_t *rds)
{
  // Checking whether the writer is done matching can't rely on
  // publication_matched.current_count because it takes some time
  // before the endpoint discovery from other domains reaches the
  // writer and consequently, it may claim to know N readers where
  // that N is a mixture of old and new readers.  That screws up
  // checking the address set.
  //
  // So check the set of matched readers against what we expect.
  // Here, we have to convert everything to GUIDs because of the
  // readers existing in multiple domains that do not share instance
  // handles.
  dds_guid_t rdguids[MAX_DOMAINS * MAX_READERS_PER_DOMAIN];
  for (int i = 0; i < nrds; i++)
  {
    dds_return_t rc = dds_get_guid (rds[i], &rdguids[i]);
    CU_ASSERT_FATAL (rc == 0);
  }
  qsort (rdguids, (size_t) nrds, sizeof (rdguids[0]), ddsi_compare_guid);

  const dds_time_t abstimeout = dds_time () + DDS_SECS (10);
  while (dds_time () < abstimeout)
  {
    (void) dds_waitset_wait_until (ws, NULL, 0, abstimeout);

    // If we don't clear the status flags, the waitset will be spinning (which is good for
    // triggering the crazy scenario described in check_writer_addrset_helper, but otherwise
    // not so good).  So we just read it without bothering to check it.
    //
    // Use dds_take_status for a change, just because we can
    {
      uint32_t status;
      dds_take_status (wr, &status, DDS_PUBLICATION_MATCHED_STATUS);
      for (int i = 0; i < nrds; i++)
        dds_take_status (rds[i], &status, DDS_SUBSCRIPTION_MATCHED_STATUS);
    }

    dds_instance_handle_t ms[MAX_DOMAINS * MAX_READERS_PER_DOMAIN];
    int32_t nms = dds_get_matched_subscriptions (wr, ms, sizeof (ms) / sizeof (ms[0]));
    CU_ASSERT_FATAL (nms >= 0);
    if (nms != nrds)
      continue;
    dds_guid_t mguids[MAX_DOMAINS * MAX_READERS_PER_DOMAIN];
    memset (mguids, 0, sizeof (mguids));
    for (int i = 0; i < nms; i++)
    {
      dds_builtintopic_endpoint_t *ep = dds_get_matched_subscription_data (wr, ms[i]);
      if (ep == NULL)
        break;
      mguids[i] = ep->key;
      dds_builtintopic_free_endpoint (ep);
    }
    qsort (mguids, (size_t) nms, sizeof (mguids[0]), ddsi_compare_guid);
    if (memcmp (mguids, rdguids, (size_t) nms * sizeof (*mguids)) != 0)
      continue;

    int mc = 0;
    for (int i = 0; i < nrds; i++)
      mc += get_current_match_count (rds[i]);
    if (mc != nrds)
      continue;

    return true;
  }
  return false;
}

static const char *istatestr (dds_instance_state_t s)
{
  switch (s)
  {
    case DDS_ALIVE_INSTANCE_STATE: return "alive";
    case DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE: return "disposed";
    case DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE: break;
  }
  return "nowriters";
}

static bool alldataseen (struct tracebuf *tb, int nrds, const dds_entity_t *rds, dds_instance_state_t instance_state, bool valid_data, const void *sample_sent, bool (*eq) (const void *sent, const void *recvd, bool valid_data), bool use_rd_loan, const dds_topic_descriptor_t *tpdesc, dds_time_t ts)
{
  assert (nrds > 0);
  dds_return_t rc;
  const dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ws > 0);
  dds_entity_t *rdconds = dds_alloc ((size_t) nrds * sizeof (*rdconds));
  for (int i = 0; i < nrds; i++)
  {
    // create read condition to make this function independent of other code that may or may not use DATA_AVAILABLE
    if (rds[i] == 0)
      rdconds[i] = 0;
    else
    {
      rdconds[i] = dds_create_readcondition (rds[i], DDS_ANY_STATE);
      CU_ASSERT_FATAL (rdconds[i] > 0);
      rc = dds_waitset_attach (ws, rdconds[i], i);
      CU_ASSERT_FATAL (rc == 0);
    }
  }

  const dds_time_t abstimeout = dds_time () + DDS_MSECS (500);
  bool alldataseen = false, toomuchdataseen = false;
  int32_t *dataseen = dds_alloc ((size_t) nrds * sizeof (*dataseen));
  for (int i = 0; i < nrds; i++)
    dataseen[i] = (rdconds[i] == 0);
  do {
    (void) dds_waitset_wait_until (ws, NULL, 0, abstimeout);
    for (int i = 0; i < nrds; i++)
    {
      if (rdconds[i] != 0)
      {
        void *sampleptr = use_rd_loan ? NULL : dds_alloc (tpdesc->m_size);
        dds_sample_info_t si;
        int32_t n;
        while ((n = dds_take (rdconds[i], &sampleptr, &si, 1, 1)) > 0)
        {
          if (si.valid_data != valid_data)
          {
            print (tb, "[rd %d valid_data %d while expecting %d] ", i, si.valid_data, valid_data);
            fail_valid_data ();
            goto out;
          }
          if (si.instance_state != instance_state)
          {
            print (tb, "[rd %d %s while expecting %s] ", i, istatestr (si.instance_state), istatestr (instance_state));
            fail_instance_state ();
            goto out;
          }
          if (si.source_timestamp != ts)
          {
            print (tb, "[rd %d source timestamp %"PRId64" while expecting %"PRId64"] ", i, si.source_timestamp, ts);
            fail_timestamp ();
            goto out;
          }
          if (!eq (sample_sent, sampleptr, valid_data))
          {
            print (tb, "[rd %d content mismatch] ", i);
            fail_content ();
            goto out;
          }
          dataseen[i] += n;
          if (dataseen[i] > 1)
            toomuchdataseen = true;
        }
        CU_ASSERT_FATAL (n == 0);
        if (!use_rd_loan)
          dds_sample_free (sampleptr, tpdesc, DDS_FREE_ALL);
      }
    }
    alldataseen = true;
    for (int i = 0; alldataseen && i < nrds; i++)
      if (dataseen[i] == 0)
        alldataseen = false;
  } while (!alldataseen && dds_time () < abstimeout);

  if (!alldataseen || toomuchdataseen)
  {
    for (int i = 0; i < nrds; i++)
    {
      if (dataseen[i] == 0)
        print (tb, "[rd %d nodata] ", i);
      else if (dataseen[i] > 1)
        print (tb, "[rd %d toomuchdata] ", i);
    }
    if (!alldataseen)
      fail_no_data ();
    else
      fail_too_much_data ();
  }

out:
  for (int i = 0; i < nrds; i++)
  {
    rc = rdconds[i] ? dds_delete (rdconds[i]) : 0;
    CU_ASSERT_FATAL (rc == 0);
  }
  rc = dds_delete (ws);
  CU_ASSERT_FATAL (rc == 0);
  dds_free (rdconds);
  dds_free (dataseen);
  return alldataseen && !toomuchdataseen;
}

struct fastpath_info {
  uint32_t nrd;
  struct ddsi_reader *rdary0;
};

static void wait_for_fastpath_ready (dds_entity_t wrhandle)
{
  dds_return_t rc;
  struct dds_entity *x;
  rc = dds_entity_pin (wrhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_WRITER);
  struct ddsi_writer * const wr = ((struct dds_writer *) x)->m_wr;
  ddsrt_mutex_lock (&wr->rdary.rdary_lock);
  while (!wr->rdary.fastpath_ok)
  {
    ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
    dds_sleepfor (DDS_MSECS (10));
    ddsrt_mutex_lock (&wr->rdary.rdary_lock);
  }
  ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
  dds_entity_unpin (x);
}

static struct fastpath_info getset_fastpath_reader_count (dds_entity_t wrhandle, struct fastpath_info new)
{
  dds_return_t rc;
  struct dds_entity *x;
  rc = dds_entity_pin (wrhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_WRITER);
  struct ddsi_writer * const wr = ((struct dds_writer *) x)->m_wr;
  ddsrt_mutex_lock (&wr->rdary.rdary_lock);
  assert (wr->rdary.fastpath_ok);
  const struct fastpath_info old = {
    .nrd = wr->rdary.n_readers,
    .rdary0 = wr->rdary.rdary[0]
  };
  wr->rdary.n_readers = new.nrd;
  wr->rdary.rdary[0] = new.rdary0;
  ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
  dds_entity_unpin (x);
  return old;
}

static void getset_fastpath_enabled (dds_entity_t wrhandle, bool enable)
{
  dds_return_t rc;
  struct dds_entity *x;

  rc = dds_entity_pin (wrhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_WRITER);
  struct ddsi_writer * const wr = ((struct dds_writer *) x)->m_wr;
  ddsrt_mutex_lock (&wr->rdary.rdary_lock);
  assert ((wr->rdary.fastpath_ok && !enable) ||
          (!wr->rdary.fastpath_ok && enable));
  wr->rdary.fastpath_ok = enable;
  ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
  dds_entity_unpin (x);
}

static int compare_uint32 (const void *va, const void *vb)
{
  const uint32_t *a = va;
  const uint32_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static void print_time (struct tracebuf *tb)
{
  dds_time_t t = dds_time ();
  print (tb, "%d.%06d", (int32_t) (t / DDS_NSECS_IN_SEC), (int32_t) (t % DDS_NSECS_IN_SEC) / 1000);
}

static void check_transmit_queue_empty_helper (void *vflag)
{
  ddsrt_atomic_or32 (vflag, 1);
}

static void check_transmit_queue_empty (struct tracebuf *tb, struct ddsi_domaingv *gvs[MAX_DOMAINS])
{
  dds_time_t tstop = dds_time () + DDS_SECS (5);
  ddsrt_atomic_uint32_t flag;
  print (tb, "; check xmit queue (");
  print_time (tb);
  print (tb, ") dom ");
  for (int i = 0; i < MAX_DOMAINS; i++)
  {
    ddsrt_atomic_st32 (&flag, 0);
    print (tb, "%d:", i);
    ddsi_qxev_nt_callback (gvs[i]->xevents, check_transmit_queue_empty_helper, &flag);
    while (!ddsrt_atomic_ld32 (&flag) && dds_time () < tstop)
      dds_sleepfor (DDS_MSECS (10));
    print_time (tb);
    if (ddsrt_atomic_ld32 (&flag))
      print (tb, "(ok)");
    else
      print (tb, "(dead)");
  }
}

enum local_delivery_mode {
  // Avoid Cyclone's internal local delivery path, to guarantee that PSMX is actually used
  // (done by overwriting the fast path table)
  LDM_NONE,
  // Also perform delivery via Cyclone's internal path, we must be sure it doesn't also
  // arrive via this one (trivial to do: it is the default, though we block until it is
  // enabled just in case it takes a little while)
  LDM_FASTPATH,
  // Also perform delivery via Cyclone's internal *slow* path, which is very rarely used
  // because it only covers some transient states during matching and deletion of a writer
  // (done by overwriting the fast path validity flag)
  LDM_SLOWPATH
};

enum write_mode {
  WM_NORMAL,
  WM_LOAN,
  WM_FORWARDCDR
};

static void dotest (const dds_topic_descriptor_t *tpdesc, const void *sample, bool (*eq) (const void *sent, const void *recvd, bool valid_data), enum local_delivery_mode ldm, enum write_mode write_mode, bool use_rd_loan)
{
  dds_return_t rc;
  dds_entity_t pp[MAX_DOMAINS];
  dds_entity_t tp[MAX_DOMAINS];
  struct ddsi_domaingv *gvs[MAX_DOMAINS];

  const dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ws > 0);

  char topicname[100];
  create_unique_topic_name ("test_psmx", topicname, sizeof (topicname));
  for (int i = 0; i < MAX_DOMAINS; i++)
  {
    pp[i] = create_participant ((dds_domainid_t) i); // FIXME: vary shm_enable for i > 0
    tp[i] = dds_create_topic (pp[i], tpdesc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp[i] > 0);
    gvs[i] = get_domaingv (pp[i]);
  }

  if (tpdesc->m_nkeys > 0 && !psmx_supports_keys (pp[0]))
  {
    CU_PASS("Topic uses keys, but PSMX does not");
    goto skip_because_of_keys;
  }

  uint32_t test_index = 0;
  for (int wr_use_psmx = 0; wr_use_psmx <= 1; wr_use_psmx++)
  {
    const dds_entity_t wr = create_writer (tp[0], (wr_use_psmx != 0));
    rc = dds_set_status_mask (wr, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_FATAL (rc == 0);
    rc = dds_waitset_attach (ws, wr, 0);
    CU_ASSERT_FATAL (rc == 0);

    // rdmode: trit 0: reader 0; trit 1: reader 1; ...
    //   0: no reader
    //   1: non-PSMX reader
    //   2: PSMX reader
    // reader i exists in domain floor(i/MAX_READERS_PER_DOMAIN)
    // exists i >= 0 . (trit i > 0)
    // forall 0 <= j < i . (trit i == 0 || trit j != 0)
    // final element in rdmode is for easily detecting completion
    int rdmode[MAX_DOMAINS * MAX_READERS_PER_DOMAIN + 1] = { 1 };
    while (rdmode[MAX_DOMAINS * MAX_READERS_PER_DOMAIN] == 0)
    {
      dds_entity_t rds[MAX_DOMAINS * MAX_READERS_PER_DOMAIN] = { 0 };
      uint32_t ports[MAX_DOMAINS * MAX_READERS_PER_DOMAIN];
      struct tracebuf tb = { .pos = 0 };
      int nrds_active = 0;
      int nports = 0;
      bool fatal = false;
      bool fail_one = false;

      // We want to be certain that local delivery happens via the PSMX, which is tricky
      // because we rather try not to make it visible at the API level. We test it here by
      // preventing the local short-circuit from working by temporarily forcing the "fast
      // path" reader count to 0 if there are no local non-PSMX readers, so that nothing
      // will get delivered if it picks the wrong path.
      bool have_local_dds_readers = false;
      for (int i = 0; !have_local_dds_readers && i < MAX_READERS_PER_DOMAIN; i++)
        if (rdmode[i] == 1)
          have_local_dds_readers = true;
      bool override_fastpath_rdcount = wr_use_psmx && !have_local_dds_readers;

      test_index++;
      if (test_index >= test_index_start && test_index <= test_index_end)
      {
        print_time (&tb);
        print (&tb, " %05u -- wr: %s; rds:", test_index, wr_use_psmx ? "psmx" : "dds ");
      }
      else
      {
        goto skip;
      }

      for (int i = 0; rdmode[i] != 0; i++)
      {
        const int dom = i / MAX_READERS_PER_DOMAIN;
        if (i > 0 && dom > (i - 1) / MAX_READERS_PER_DOMAIN)
          print (&tb, " |");
        print (&tb, " %s", (rdmode[i] == 2) ? "psmx" : "dds ");

        rds[i] = create_reader (tp[dom], rdmode[i] == 2);
        const uint32_t port = reader_unicast_port (rds[i]);
        if (dom == 0)
        {
          // intra-domain: no locators used, ever
        }
        else if (wr_use_psmx && dom <= 1 && rdmode[i] == 2)
        {
          // dom 0, 1: same PSMX "domain" (service name, locator)
          // PSMX should be used -> no locator expected in addrset
        }
        else
        {
          // non-PSMX writer, reader, or a different PSMX "domain"
          ports[nports++] = port;
          if (dom == 0 && rdmode[i] != 2)
          {
            // if non-PSMX reader present in same PSMX "domain", we need
            // the native short-circuiting
            override_fastpath_rdcount = false;
          }
        }

        rc = dds_set_status_mask (rds[i], DDS_SUBSCRIPTION_MATCHED_STATUS);
        CU_ASSERT_FATAL (rc == 0);
        rc = dds_waitset_attach (ws, rds[i], i + 1);
        CU_ASSERT_FATAL (rc == 0);
        nrds_active++;
      }

      for (int i = 0; i < MAX_DOMAINS; i++)
      {
        struct ddsi_domaingv *gv = gvs[i];
        GVTRACE ("#### %s ####\n", tb.buf);
      }

      print (&tb, "; match");
      struct fastpath_info old_fastpath = {
        .nrd = UINT32_MAX,
        .rdary0 = 0
      };
      if (!allmatched (ws, wr, nrds_active, rds))
      {
        // sanity check for a once-observed problem where SEDP fails because a
        // scheduling problem with a pre-emptive ACKNACK event causes event thread
        // to not get around to sending queued messages, which resulted in the SEDP
        // data never making it out
        check_transmit_queue_empty (&tb, gvs);
        fail_match ();
        fail_one = true;
        goto next;
      }

      print (&tb, "; addrset");
      if (nports > 0)
      {
        qsort (ports, (size_t) nports, sizeof (ports[0]), compare_uint32);
        int i, j;
        for (i = 1, j = 0; i < nports; i++)
          if (ports[i] != ports[j])
            ports[++j] = ports[i];
        nports = j + 1;
      }
      print (&tb, "{"); for (int i = 0; i < nports; i++) print (&tb, " %u", ports[i]); print (&tb, " }");
      if (!check_writer_addrset (&tb, wr, nports, ports))
      {
        fail_addrset ();
        fail_one = true;
        goto next;
      }

      // Once matched, fast path should go to "ok" and we can override it (if needed)
      print (&tb, "; ");
      wait_for_fastpath_ready (wr);
      switch (ldm)
      {
        case LDM_NONE:
          if (override_fastpath_rdcount)
          {
            print (&tb, "hack-fastpath");
            old_fastpath = getset_fastpath_reader_count (wr, (struct fastpath_info){ .nrd = 0, .rdary0 = 0 });
            print (&tb, "(%"PRIu32"); ", old_fastpath.nrd);
          }
          break;
        case LDM_FASTPATH:
          break;
        case LDM_SLOWPATH:
          print (&tb, "force-slowpath; ");
          getset_fastpath_enabled (wr, false);
          break;
      }
      // easier on the eyes in the log:
      //dds_sleepfor (DDS_MSECS (100));
      static struct {
        const char *info;
        enum { OP_WRITE, OP_DISPOSE, OP_WRITEDISPOSE, OP_UNREGISTER } op;
        dds_instance_state_t istate;
      } const ops[] = {
        { "w", OP_WRITE, DDS_ALIVE_INSTANCE_STATE },
        { "d", OP_DISPOSE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE },
        { "w", OP_WRITE, DDS_ALIVE_INSTANCE_STATE },
        { "wd", OP_WRITEDISPOSE, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE },
        { "w", OP_WRITE, DDS_ALIVE_INSTANCE_STATE },
        { "u", OP_UNREGISTER, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE }
      };
      for (size_t opidx = 0; opidx < sizeof (ops) / sizeof (ops[0]); opidx++)
      {
        print (&tb, "%s ", ops[opidx].info); fflush (stdout);

        // If using a loan, copy the template in. We can only allow loans for writes,
        // because we guarantee that invalid samples have non-key fields zeroed out,
        // and we can't trust the application to get that right.
        // FIXME: dds_dispose does a use-after-free when handed a loan, that's bad
        const void *sample_to_write = sample;
        if (write_mode == WM_LOAN && ops[opidx].op == OP_WRITE)
        {
          void *loan;
          rc = dds_request_loan (wr, &loan);
          CU_ASSERT_FATAL (rc == 0);
          memcpy (loan, sample, tpdesc->m_size);
          sample_to_write = loan;
        }

        dds_time_t ts = dds_time ();
        const bool valid_data = (ops[opidx].op == OP_WRITE || ops[opidx].op == OP_WRITEDISPOSE);
        if (write_mode != WM_FORWARDCDR)
        {
          switch (ops[opidx].op)
          {
            case OP_WRITE: rc = dds_write_ts (wr, sample_to_write, ts); break;
            case OP_DISPOSE: rc = dds_dispose_ts (wr, sample_to_write, ts); break;
            case OP_WRITEDISPOSE: rc = dds_writedispose_ts (wr, sample_to_write, ts); break;
            case OP_UNREGISTER: rc = dds_unregister_instance_ts (wr, sample_to_write, ts); break;
          }
        }
        else
        {
          const struct ddsi_sertype *st;
          rc = dds_get_entity_sertype (wr, &st);
          CU_ASSERT_FATAL (rc == 0);
          struct ddsi_serdata * const sd = ddsi_serdata_from_sample (st, valid_data ? SDK_DATA : SDK_KEY, sample);
          CU_ASSERT_FATAL (sd != NULL);
          // the odious dds_writecdr overwrites statusinfo and timestamp, but dds_forwardcdr is better behaved
          switch (ops[opidx].op)
          {
            case OP_WRITE: sd->statusinfo = 0; break;
            case OP_DISPOSE: case OP_WRITEDISPOSE: sd->statusinfo = DDSI_STATUSINFO_DISPOSE; break;
            case OP_UNREGISTER: sd->statusinfo = DDSI_STATUSINFO_UNREGISTER; break;
          }
          sd->timestamp.v = ts;
          // dds_forwardcdr should make a copy when using PSMX and the input refcount > 1, so let's do
          // a sanity check.  We want to catch both paths, set do half of them with refc 1, and half
          // with refc > 1
          if (!wr_use_psmx || (opidx % 2) == 0)
            rc = dds_forwardcdr (wr, sd);
          else
          {
            assert (ddsrt_atomic_ld32 (&sd->refc) >= 1 && sd->loan == NULL);
            struct ddsi_serdata * const check_sd = ddsi_serdata_ref (sd);
            rc = dds_forwardcdr (wr, check_sd);
            assert (ddsrt_atomic_ld32 (&check_sd->refc) >= 1 && check_sd->loan == NULL);
            ddsi_serdata_unref (check_sd);
          }
        }
        CU_ASSERT_FATAL (rc == 0);
        if (!alldataseen (&tb, nrds_active, rds, ops[opidx].istate, valid_data, sample, eq, use_rd_loan, tpdesc, ts))
        {
          fail_one = true;
          goto next;
        }
      }

    next:
      switch (ldm)
      {
        case LDM_NONE:
          if (old_fastpath.nrd != UINT32_MAX)
            (void) getset_fastpath_reader_count (wr, old_fastpath);
          break;
        case LDM_FASTPATH:
          break;
        case LDM_SLOWPATH:
          getset_fastpath_enabled (wr, true);
          break;
      }
      print (&tb, ": %s", fail_one ? "FAIL" : "ok");
      for (int i = 0; i < MAX_DOMAINS; i++)
      {
        struct ddsi_domaingv *gv = gvs[i];
        GVTRACE ("#### %s ####\n", tb.buf);
      }
      fputs ("\n", stdout);
      fflush (stdout);
      CU_ASSERT_FATAL (!fatal);

    skip:
      // delete the readers, keep the writer
      CU_ASSERT_FATAL (nrds_active == MAX_DOMAINS * MAX_READERS_PER_DOMAIN || rds[nrds_active] == 0);
      for (int i = 0; i < nrds_active; i++)
      {
        rc = dds_delete (rds[i]);
        CU_ASSERT_FATAL (rc == 0);
      }
      // "increment" rdmode according to the rules
      for (int i = 0; i <= MAX_DOMAINS * MAX_READERS_PER_DOMAIN; i++)
      {
        if (++rdmode[i] <= 2)
          break;
        rdmode[i] = 1;
      }
    }

    rc = dds_delete (wr);
    CU_ASSERT_FATAL (rc == 0);
  }

skip_because_of_keys:
  rc = dds_delete (ws);
  CU_ASSERT_FATAL (rc == 0);
  for (int i = 0; i < MAX_DOMAINS; i++)
  {
    rc = dds_delete (dds_get_parent (pp[i]));
    CU_ASSERT_FATAL (rc == 0);
  }
}

static bool eq_PsmxType1 (const void *vsent, const void *vrecvd, bool valid_data)
{
  const PsmxType1 *sent = vsent;
  const PsmxType1 *recvd = vrecvd;
  if (valid_data)
    return sent->xy.x == recvd->xy.x && sent->xy.y == recvd->xy.y && sent->z == recvd->z;
  else
    return recvd->xy.x == 0 && recvd->xy.y == 0 && recvd->z == 0;
}

static bool eq_DynamicData_Msg (const void *vsent, const void *vrecvd, bool valid_data)
{
  const DynamicData_Msg *sent = vsent;
  const DynamicData_Msg *recvd = vrecvd;
  if (valid_data)
  {
    if (strcmp (sent->message, recvd->message) != 0 || sent->scalar != recvd->scalar || sent->values._length != recvd->values._length)
      return false;
    for (uint32_t i = 0; i < sent->values._length; i++)
      if (sent->values._buffer[i] != recvd->values._buffer[i])
        return false;
    return true;
  }
  else
  {
    return recvd->message == NULL && recvd->scalar == 0 && recvd->values._length == 0;
  }
}

static bool eq_DynamicData_KMsg (const void *vsent, const void *vrecvd, bool valid_data)
{
  const DynamicData_KMsg *sent = vsent;
  const DynamicData_KMsg *recvd = vrecvd;
  if (valid_data)
  {
    if (strcmp (sent->message, recvd->message) != 0 || sent->scalar != recvd->scalar || sent->values._length != recvd->values._length)
      return false;
    for (uint32_t i = 0; i < sent->values._length; i++)
      if (sent->values._buffer[i] != recvd->values._buffer[i])
        return false;
    return true;
  }
  else
  {
    if (strcmp (sent->message, recvd->message) != 0)
      return false;
    return recvd->scalar == 0 && recvd->values._length == 0;
  }
}

CU_Test(ddsc_psmx, one_writer, .timeout = 240)
{
  failed = false;
  dotest (&PsmxType1_desc, &(const PsmxType1){ {5, 3}, 53 }, eq_PsmxType1, LDM_NONE, WM_NORMAL, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_dynsize, .timeout = 240)
{
  failed = false;
  dotest (&DynamicData_Msg_desc, &(const DynamicData_Msg){
    .message = "Muss es sein?",
    .scalar = 135,
    .values = {
      ._length = 4, ._maximum = 4, ._release = false,
      ._buffer = (int32_t[]) { 193, 272, 54, 277 }
    }
  }, eq_DynamicData_Msg, LDM_NONE, WM_NORMAL, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_dynsize_strkey, .timeout = 240)
{
  failed = false;
  dotest (&DynamicData_KMsg_desc, &(const DynamicData_KMsg){
    .message = "Muss es sein?",
    .scalar = 135,
    .values = {
      ._length = 4, ._maximum = 4, ._release = false,
      ._buffer = (int32_t[]) { 193, 272, 54, 277 }
    }
  }, eq_DynamicData_KMsg, LDM_NONE, WM_NORMAL, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_fastpath, .timeout = 240)
{
  failed = false;
  dotest (&PsmxType1_desc, &(const PsmxType1){ {7, 3}, 73 }, eq_PsmxType1, LDM_FASTPATH, WM_NORMAL, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_slowpath, .timeout = 240)
{
  failed = false;
  dotest (&Space_Type3_desc, &(const PsmxType1){ {2, 3}, 23 }, eq_PsmxType1, LDM_SLOWPATH, WM_NORMAL, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_wloan, .timeout = 240)
{
  failed = false;
  dotest (&PsmxType1_desc, &(const PsmxType1){ {3, 7}, 37 }, eq_PsmxType1, LDM_NONE, WM_LOAN, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_rloan, .timeout = 240)
{
  failed = false;
  dotest (&PsmxType1_desc, &(const PsmxType1){ {11, 17}, 251 }, eq_PsmxType1, LDM_NONE, WM_NORMAL, true);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_wrloan, .timeout = 240)
{
  failed = false;
  dotest (&PsmxType1_desc, &(const PsmxType1){ {5113, 51}, 13 }, eq_PsmxType1, LDM_NONE, WM_LOAN, true);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_forwardcdr, .timeout = 240)
{
  failed = false;
  dotest (&PsmxType1_desc, &(const PsmxType1){ {5, 3}, 53 }, eq_PsmxType1, LDM_NONE, WM_FORWARDCDR, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_forwardcdr_dynsize, .timeout = 240)
{
  failed = false;
  dotest (&DynamicData_Msg_desc, &(const DynamicData_Msg){
    .message = "Muss es sein?",
    .scalar = 135,
    .values = {
      ._length = 4, ._maximum = 4, ._release = false,
      ._buffer = (int32_t[]) { 193, 272, 54, 277 }
    }
  }, eq_DynamicData_Msg, LDM_NONE, WM_FORWARDCDR, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, one_writer_forwardcdr_dynsize_strkey, .timeout = 240)
{
  failed = false;
  dotest (&DynamicData_KMsg_desc, &(const DynamicData_KMsg){
    .message = "Muss es sein?",
    .scalar = 135,
    .values = {
      ._length = 4, ._maximum = 4, ._release = false,
      ._buffer = (int32_t[]) { 193, 272, 54, 277 }
    }
  }, eq_DynamicData_KMsg, LDM_NONE, WM_FORWARDCDR, false);
  CU_ASSERT (!failed);
}

CU_Test(ddsc_psmx, return_loan)
{
  dds_return_t rc;
  const dds_entity_t pp = create_participant (0);
  char topicname[100];
  create_unique_topic_name ("test_psmx", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (pp, &Array100_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);
  const dds_entity_t wr = create_writer (tp, true);

  for (int i = 0; i < 10000; i ++)
  {
    void *sample;
    rc = dds_request_loan (wr, &sample);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    rc = dds_return_loan (wr, &sample, 1);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }

  // FIXME: check RSS

  rc = dds_delete (dds_get_parent (pp));
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
}

CU_Test(ddsc_psmx, partition_xtalk)
{
  dds_return_t rc;
  const dds_entity_t pp = create_participant (0);
  char topicname[100];
  create_unique_topic_name ("test_psmx", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (pp, &PsmxType1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);

  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, 0);
  dds_qset_writer_data_lifecycle (qos, false);

  static const struct testcase {
    struct epspec {
      uint32_t np;
      const char *p[2];
    } wr, rd;
    const char *checkwrp; // null: wr & rd match; non-null: partition to use for check writer
  } testcases[] = {
    // QoS checking and PSMX name construction code are the same for reader & writer, so
    // no need to check all combinations
    { { 0, {0,0}    }, {0, {0,0}    }, 0 },
    { { 1, {"",0}   }, {0, {0,0}    }, 0 },
    { { 2, {"","a"} }, {0, {0,0}    }, 0 },   // 2 pt -> no PSMX for writer, still match
    { { 1, {"a"}    }, {1, {"b",0}  }, "b" }, // diff partition, no match (so need checkwr)
    { { 2, {"","a"} }, {1, {"b",0}  }, "b" }, // 2 pt -> no PSMX for writer, no match
    { { 2, {"","a"} }, {1, {"*",0}  }, 0 },   // 2 pt, wildcard: no PSMX involved
    { { 1, {"*",0}  }, {1, {"*",0}  }, "x" }, // rd&wr both wildcard: no match, hence "x"
    { { 1, {"?",0}  }, {1, {"b",0}  }, 0 },   // ? is also a wildcard character
    { { 2, {"",""}  }, {1, {"",0}   }, 0 },   // 2 pt -> no PSMX, even if the two are the same
    { { 1, {".",0}  }, {1, {".",0}  }, 0 },   // a dot and \ should be escaped (can't check the
    { { 1, {"\\",0} }, {1, {"\\",0} }, 0 },   // ... name in PSMX, but should at leas t try)
  };

  for (size_t i = 0; i < sizeof (testcases) / sizeof (testcases[0]); i++)
  {
    const struct testcase *tc = &testcases[i];
    printf ("wr %s %s rd %s %s checkwr %s\n",
            tc->wr.p[0] ? tc->wr.p[0] : "(null)",
            tc->wr.p[1] ? tc->wr.p[1] : "(null)",
            tc->rd.p[0] ? tc->rd.p[0] : "(null)",
            tc->rd.p[1] ? tc->rd.p[1] : "(null)",
            tc->checkwrp ? tc->checkwrp : "(null)");

    dds_qset_partition (qos, tc->wr.np, (const char **) tc->wr.p);
    dds_entity_t wr = dds_create_writer (pp, tp, qos, NULL);
    CU_ASSERT_FATAL (wr > 0);

    dds_qset_partition (qos, tc->rd.np, (const char **) tc->rd.p);
    dds_entity_t rd = dds_create_reader (pp, tp, qos, NULL);
    CU_ASSERT_FATAL (rd > 0);

    dds_entity_t checkwr = 0;
    if (tc->checkwrp)
    {
      dds_qset_partition1 (qos, tc->checkwrp);
      checkwr = dds_create_writer (pp, tp, qos, NULL);
      CU_ASSERT_FATAL (checkwr > 0);
    }

    rc = dds_write (wr, &(PsmxType1){ {1, 2}, 3 });
    CU_ASSERT_FATAL (rc == 0);
    if (checkwr)
    {
      rc = dds_write (checkwr, &(PsmxType1){ {4, 5}, 6 });
      CU_ASSERT_FATAL (rc == 0);
    }

    PsmxType1 t;
    void *tptr = &t;
    dds_sample_info_t si;
    while ((rc = dds_take (rd, &tptr, &si, 1, 1)) <= 0)
      dds_sleepfor (DDS_MSECS (10));
    CU_ASSERT_FATAL (rc == 1);
    if (checkwr == 0) {
      CU_ASSERT_FATAL (t.xy.x == 1 && t.xy.y == 2 && t.z == 3);
    } else {
      CU_ASSERT_FATAL (t.xy.x == 4 && t.xy.y == 5 && t.z == 6);
    }

    rc = dds_delete (wr);
    CU_ASSERT_FATAL (rc == 0);
    rc = dds_delete (rd);
    CU_ASSERT_FATAL (rc == 0);
    if (checkwr)
    {
      rc = dds_delete (checkwr);
      CU_ASSERT_FATAL (rc == 0);
    }
  }

  dds_delete_qos (qos);
  rc = dds_delete (dds_get_parent (pp));
  CU_ASSERT_FATAL (rc == 0);
}


#define MAX_SAMPLES 8
CU_Test (ddsc_psmx, basic)
{
  dds_return_t rc;
  void *samples[MAX_SAMPLES];
  dds_sample_info_t infos[MAX_SAMPLES];
  dds_entity_t participant, topic, writer, reader;
  bool psmx_enabled;

  participant = create_participant (0);
  CU_ASSERT_FATAL (participant > 0);

  char topicname[100];
  create_unique_topic_name ("test_psmx_basic", topicname, sizeof (topicname));
  topic = dds_create_topic (participant, &SC_Model_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  writer = dds_create_writer (participant, topic, NULL, NULL);
  CU_ASSERT_FATAL (writer > 0);
  psmx_enabled = endpoint_has_psmx_enabled (writer);
  CU_ASSERT_FATAL (psmx_enabled);

  reader = dds_create_reader (participant, topic, NULL, NULL);
  CU_ASSERT_FATAL (reader > 0);
  psmx_enabled = endpoint_has_psmx_enabled (reader);
  CU_ASSERT_FATAL (psmx_enabled);

  sync_reader_writer (participant, reader, participant, writer);

  // write
  SC_Model mod = {.a = 0x1, .b = 0x4, .c = 0x9};
  rc = dds_write (writer, &mod);
  CU_ASSERT_EQUAL_FATAL (rc, DDS_RETCODE_OK);

  // read
  samples[0] = SC_Model__alloc ();
  dds_entity_t ws = dds_create_waitset (participant);
  dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
  dds_waitset_attach (ws, reader, reader);
  bool sample_read = false;
  do
  {
    dds_waitset_wait (ws, NULL, 0, DDS_MSECS (100));
    rc = dds_read (reader, samples, infos, MAX_SAMPLES, MAX_SAMPLES);
    if (rc > 0 && infos[0].valid_data)
    {
      CU_ASSERT_EQUAL_FATAL (rc, 1);
      sample_read = true;
    }
  }
  while (!sample_read);

  SC_Model_free (samples[0], DDS_FREE_ALL);
  dds_delete (dds_get_parent (participant));
}

CU_Test (ddsc_psmx, zero_copy)
{
  const struct {
    const dds_topic_descriptor_t *desc;
    bool wrloan_ok;
    const void *data;
  } cases[] = {
    { &SC_Model_desc, true, &(SC_Model){ 5, 11, 31 } },
    { &NSC_Model_desc, false, &(NSC_Model){ "aap", "noot", "mies" } }
  };
  dds_return_t rc;

  const dds_entity_t pp = create_participant (0);
  CU_ASSERT_FATAL (pp > 0);
  for (size_t k = 0; k < sizeof (cases) / sizeof (cases[0]); k++)
  {
    char topicname[100];
    create_unique_topic_name ("zero_copy", topicname, sizeof (topicname));
    const dds_entity_t tp = dds_create_topic (pp, cases[k].desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp > 0);
    for (int psmx_enabled_i = 0; psmx_enabled_i <= 1; psmx_enabled_i++)
    {
      const bool psmx_enabled = psmx_enabled_i;
      dds_qos_t *qos = dds_create_qos ();
      if (!psmx_enabled)
        dds_qset_psmx_instances (qos, 0, NULL);
      for (int wrloan_i = 0; wrloan_i <= (cases[k].wrloan_ok ? 1 : 0); wrloan_i++)
      {
        const bool wrloan = wrloan_i;
        printf ("ddsc_psmx read_vs_read_with_loan: %s psmx %d wrloan %d", cases[k].desc->m_typename, psmx_enabled, wrloan);
        fflush (stdout);

        const dds_entity_t wr = dds_create_writer (pp, tp, qos, NULL);
        CU_ASSERT_FATAL (wr > 0);
        CU_ASSERT_FATAL (endpoint_has_psmx_enabled (wr) == psmx_enabled);
        dds_entity_t rds[2];
        const dds_entity_t ws = dds_create_waitset (pp);
        for (size_t i = 0; i < sizeof (rds) / sizeof (rds[0]); i++)
        {
          rds[i] = dds_create_reader (pp, tp, qos, NULL);
          CU_ASSERT_FATAL (rds[i] > 0);
          CU_ASSERT_FATAL (endpoint_has_psmx_enabled (rds[i]) == psmx_enabled);
          sync_reader_writer (pp, rds[i], pp, wr);
          rc = dds_set_status_mask (rds[i], DDS_DATA_AVAILABLE_STATUS);
          CU_ASSERT_FATAL (rc == 0);
          rc = dds_waitset_attach (ws, rds[i], rds[i]);
          CU_ASSERT_FATAL (rc == 0);
        }

        const void *wrdata = cases[k].data;
        if (wrloan)
        {
          void *tmp;
          rc = dds_request_loan (wr, &tmp);
          CU_ASSERT_FATAL (rc == 0);
          memcpy (tmp, wrdata, cases[k].desc->m_size);
          wrdata = tmp;
        }
        printf (" | write %p", wrdata); fflush (stdout);
        rc = dds_write (wr, wrdata);
        CU_ASSERT_FATAL (rc == 0);

        // wait until all readers have data
        {
          size_t ready = 0;
          while (ready != sizeof (rds) / sizeof (rds[0]))
          {
            dds_attach_t ts[2];
            int32_t n = dds_waitset_wait (ws, ts, sizeof (ts) / sizeof (ts[0]), DDS_INFINITY);
            CU_ASSERT_FATAL (n > 0);
            ready += (size_t) n;
            for (int32_t i = 0; i < n; i++)
            {
              rc = dds_waitset_detach (ws, (dds_entity_t) ts[i]);
              CU_ASSERT_FATAL (rc == 0);
            }
          }
        }
        rc = dds_delete (ws);
        CU_ASSERT_FATAL (rc == 0);

        dds_sample_info_t si;
        void *rddata[sizeof (rds) / sizeof (rds[0])] = { NULL };
        for (size_t i = 0; i < sizeof (rds) / sizeof (rds[0]); i++)
        {
          rc = dds_read (rds[i], &rddata[i], &si, 1, 1);
          CU_ASSERT_FATAL (rc == 1);
          printf (" | rddata[%d] %p", (int) i, rddata[i]); fflush (stdout);
        }

        // For self-contained types: if somewhere in the process a loan gets
        // involved the readers will get the same address.
        //
        // Iceoryx -> shared memory -> whenever the writer uses a loan or Iceoryx is used
        // CDDS-based plugin -> many copies but it works if the writer uses a loan and
        //   PSMX is avoided (because the plugin always produces separate copies)
        //
        // This is obviously very implementation-specific, but as this is a test,
        // let's check. Changes to the implementation will likely require changing
        // this test.
        if (cases[k].wrloan_ok &&
            (( dds_is_shared_memory_available (wr) && (wrloan || psmx_enabled)) ||
             (!dds_is_shared_memory_available (wr) && (wrloan && !psmx_enabled))))
        {
          for (size_t i = 1; i < sizeof (rds) / sizeof (rds[0]); i++)
            CU_ASSERT (rddata[i] == rddata[0]);
          CU_ASSERT ((wrloan && wrdata == rddata[0]) || (!wrloan && wrdata != rddata[0]));
        }
        else
        {
          for (size_t i = 0; i < sizeof (rds) / sizeof (rds[0]) - 1; i++)
            for (size_t j = i + 1; j < sizeof (rds) / sizeof (rds[0]); j++)
              CU_ASSERT (rddata[i] != rddata[j]);
        }

        printf ("\n"); fflush (stdout);
        rc = dds_delete (wr);
        CU_ASSERT_FATAL (rc == 0);
        for (size_t i = 0; i < sizeof (rds) / sizeof (rds[0]); i++)
        {
          rc = dds_delete (rds[i]);
          CU_ASSERT_FATAL (rc == 0);
        }
      }
      dds_delete_qos (qos);
    }

    rc = dds_delete (tp);
    CU_ASSERT_FATAL (rc == 0);
  }
  dds_delete (dds_get_parent (pp));
}

static void deepcopy_sample_contents (const dds_topic_descriptor_t *tpdesc, void *output, const void *input)
{
  struct dds_cdrstream_desc desc;
  dds_cdrstream_desc_from_topic_desc (&desc, tpdesc);
  struct dds_ostream os;
  dds_ostream_init (&os, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
  dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, input, &desc);
  struct dds_istream is;
  dds_istream_init (&is, os.m_index, os.m_buffer, os.m_xcdr_version);
  memset (output, 0, desc.size);
  dds_stream_read_sample (&is, output, &dds_cdrstream_default_allocator, &desc);
  dds_istream_fini (&is);
  dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
  dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
}

static const void *get_write_datapointer (dds_entity_t wr, const dds_topic_descriptor_t *tpdesc, const void *template, bool wrloan)
{
  if (!wrloan)
    return template;
  else
  {
    void *tmp;
    dds_return_t rc = dds_request_loan (wr, &tmp);
    CU_ASSERT_FATAL (rc == 0);
    deepcopy_sample_contents (tpdesc, tmp, template);
    return tmp;
  }
}

static bool data_equal (const dds_topic_descriptor_t *tpdesc, const void *a, const void *b, bool justkey)
{
  struct dds_cdrstream_desc desc;
  dds_cdrstream_desc_from_topic_desc (&desc, tpdesc);
  struct dds_ostream osa, osb;
  dds_ostream_init (&osa, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
  dds_ostream_init (&osb, &dds_cdrstream_default_allocator, 0, DDSI_RTPS_CDR_ENC_VERSION_2);
  if (justkey)
  {
    dds_stream_write_key (&osa, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, a, &desc);
    dds_stream_write_key (&osb, DDS_CDR_KEY_SERIALIZATION_SAMPLE, &dds_cdrstream_default_allocator, b, &desc);
  }
  else
  {
    dds_stream_write_sample (&osa, &dds_cdrstream_default_allocator, a, &desc);
    dds_stream_write_sample (&osb, &dds_cdrstream_default_allocator, b, &desc);
  }
  const bool eq = (osa.m_index == osb.m_index) && memcmp (osa.m_buffer, osb.m_buffer, osa.m_index) == 0;
  dds_ostream_fini (&osa, &dds_cdrstream_default_allocator);
  dds_ostream_fini (&osb, &dds_cdrstream_default_allocator);
  dds_cdrstream_desc_fini (&desc, &dds_cdrstream_default_allocator);
  return eq;
}

CU_Test (ddsc_psmx, writer_loan)
{
  const struct {
    const dds_topic_descriptor_t *desc;
    const struct {
      dds_return_t (*op) (dds_entity_t wr, const void *data);
      const void *data;
    } step[2];
  } cases[] = {
    { &PsmxLoanTest0_desc, {
      { dds_write, &(PsmxLoanTest0){ { 0x12345678, 0x55 }, 0x22 } },
      { dds_dispose, &(PsmxLoanTest0){ { 0, 0 }, 0x22 } } } },
    { &PsmxLoanTest1_desc, {
      { dds_write, &(PsmxLoanTest1){ { 0x12345678, 0x55 }, 0x22, "aap" } },
      { dds_dispose, &(PsmxLoanTest1){ { 0, 0, }, 0x22, "" } } } },
    { &PsmxLoanTest2_desc, {
      { dds_write, &(PsmxLoanTest2){ { 0x12345678, 0x55 }, 0x22, "noot" } },
      { dds_dispose, &(PsmxLoanTest2){ { 0, 0, }, 0x22, "noot" } } } },
  };
  dds_return_t rc;

  const dds_entity_t pp = create_participant (0);
  CU_ASSERT_FATAL (pp > 0);
  for (size_t k = 0; k < sizeof (cases) / sizeof (cases[0]); k++)
  {
    char topicname[100];
    create_unique_topic_name ("writer_loan", topicname, sizeof (topicname));
    const dds_entity_t tp = dds_create_topic (pp, cases[k].desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp > 0);
    for (int wr_psmx_enabled_i = 0; wr_psmx_enabled_i <= 1; wr_psmx_enabled_i++)
    {
      const bool wr_psmx_enabled = wr_psmx_enabled_i;
      for (int rd_psmx_enabled_i = 0; rd_psmx_enabled_i <= 1; rd_psmx_enabled_i++)
      {
        const bool rd_psmx_enabled = rd_psmx_enabled_i;
        for (int wrloan_i = 0; wrloan_i <= 1; wrloan_i++)
        {
          const bool wrloan = wrloan_i;
          for (int rdloan_i = 0; rdloan_i <= 1; rdloan_i++)
          {
            const bool rdloan = rdloan_i;
            dds_qos_t *qos;
            printf ("ddsc_psmx writer_loan: %s psmx %d %d wrloan %d rdloan %d", cases[k].desc->m_typename, wr_psmx_enabled, rd_psmx_enabled, wrloan, rdloan);
            fflush (stdout);

            qos = dds_create_qos ();
            if (!wr_psmx_enabled)
              dds_qset_psmx_instances (qos, 0, NULL);
            const dds_entity_t wr = dds_create_writer (pp, tp, qos, NULL);
            CU_ASSERT_FATAL (wr > 0);
            CU_ASSERT_FATAL (endpoint_has_psmx_enabled (wr) == wr_psmx_enabled);
            dds_delete_qos (qos);

            qos = dds_create_qos ();
            if (!rd_psmx_enabled)
              dds_qset_psmx_instances (qos, 0, NULL);
            dds_entity_t rd;
            const dds_entity_t ws = dds_create_waitset (pp);
            rd = dds_create_reader (pp, tp, qos, NULL);
            CU_ASSERT_FATAL (rd > 0);
            CU_ASSERT_FATAL (endpoint_has_psmx_enabled (rd) == rd_psmx_enabled);
            dds_delete_qos (qos);

            rc = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
            CU_ASSERT_FATAL (rc == 0);
            rc = dds_waitset_attach (ws, rd, 0);
            CU_ASSERT_FATAL (rc == 0);

            for (int step_i = 0; step_i < (int) (sizeof (cases[k].step) / sizeof (cases[k].step[0])); step_i++)
            {
              const void *wrdata = cases[k].step[step_i].data;
              dds_return_t (* const op) (dds_entity_t wr, const void *data) = cases[k].step[step_i].op;

              // dispose + loan is not supported (triggers use-after-free; this is documented, but bad)
              if (op == dds_dispose && wrloan)
                continue;

              const bool justkey = (op != dds_write);
              printf (" | %s %p", (op == dds_write) ? "write" : "dispose", wrdata); fflush (stdout);
              rc = op (wr, get_write_datapointer (wr, cases[k].desc, wrdata, wrloan));
              CU_ASSERT_FATAL (rc == 0);
              (void) dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
              dds_sample_info_t si;
              void *rddata;
              if (rdloan)
                rddata = NULL;
              else
              {
                rddata = ddsrt_malloc (cases[k].desc->m_size);
                memset (rddata, 0, cases[k].desc->m_size);
              }
              rc = dds_take (rd, &rddata, &si, 1, 1);
              CU_ASSERT_FATAL (rc == 1);
              printf (" | rddata %p", rddata); fflush (stdout);
              CU_ASSERT_FATAL (data_equal (cases[k].desc, wrdata, rddata, justkey));
              if (!rdloan)
                dds_sample_free (rddata, cases[k].desc, DDS_FREE_ALL);
              else
              {
                rc = dds_return_loan (rd, &rddata, 1);
                CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
              }
            }
            printf ("\n"); fflush (stdout);

            rc = dds_delete (wr);
            CU_ASSERT_FATAL (rc == 0);
            rc = dds_delete (rd);
            CU_ASSERT_FATAL (rc == 0);
            rc = dds_delete (ws);
            CU_ASSERT_FATAL (rc == 0);
          }
        }
      }
    }
    rc = dds_delete (tp);
    CU_ASSERT_FATAL (rc == 0);
  }
  dds_delete (dds_get_parent (pp));
}

CU_Test (ddsc_psmx, configstr)
{
  static const struct { const char *in; const char *out; } cases[] = {
    { "", "" },
    { ";", NULL },
    { "X", NULL },
    { "=", NULL },
    { "=Y", NULL },
    { "X;Y", NULL },
    { "X=", "X=;" },
    { "X=3", "X=3;" },
    { "X=3;", "X=3;" },
    { "X=3;;", NULL },
    { "X=3;YY=456", "X=3;YY=456;" },
    { "X=3;YY=456;", "X=3;YY=456;" },
    { "X=3;YY=4\\56;", "X=3;YY=4\\56;" },
    { "X=3;;YY=4\\56;", NULL },
    { "X\\=3;", NULL },
    { "X=3;\\", NULL },
    { "X=3\\", NULL },
    { "X=3\\\\", "X=3\\\\;" },
    { "X=3\\;Y=", "X=3\\;Y=;" },
    { "CYCLONEDDS_=", NULL },
    { "CYCLONEDDS_X=", NULL },
    { "X=3;CYCLONEDDS_=", NULL },
    { "X=3;CYCLONEDDS_X=", NULL }
  };
  for (size_t i = 0; i < sizeof (cases) / sizeof (cases[0]); i++) {
    char *p = dds_pubsub_message_exchange_configstr (cases[i].in);
    CU_ASSERT_FATAL ((p == NULL) == (cases[i].out == NULL));
    if (p) {
      CU_ASSERT_FATAL (strcmp (p, cases[i].out) == 0);
      ddsrt_free (p);
    }
  }
}
