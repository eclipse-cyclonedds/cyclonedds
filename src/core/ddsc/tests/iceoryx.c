/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
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
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_addrset.h"
#include "dds__entity.h"

#include "test_common.h"
#include "Array100.h"

static const struct shm_locator {
  unsigned char a[16];
} shm_locators[] = {
  {{1}}, {{1}}, {{2}}, {{2}}
};
#define MAX_DOMAINS 4
#define MAX_READERS_PER_DOMAIN 2
DDSRT_STATIC_ASSERT (MAX_DOMAINS == sizeof (shm_locators) / sizeof (shm_locators[0]));

static bool failed;

static void fail (void) { failed = true; }
static void fail_match (void) { fail (); }
static void fail_addrset (void) { fail (); }
static void fail_instance_state (void) { fail (); }
static void fail_no_data (void) { fail (); }

#if 0
#define TRACE_CATEGORY "trace,rhc,shm"
#else
#define TRACE_CATEGORY "discovery"
#endif

static dds_entity_t create_participant (dds_domainid_t int_dom, bool shm_enable)
{
  assert (int_dom < MAX_DOMAINS);
  const unsigned char *l = shm_locators[int_dom].a;
  char *configstr;
  ddsrt_asprintf (&configstr, "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
  <AllowMulticast>spdp</AllowMulticast>\
</General>\
<Discovery>\
  <ExternalDomainId>0</ExternalDomainId>\
</Discovery>\
<SharedMemory>\
  <Enable>%s</Enable>\
  <Locator>%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x</Locator>\
  <Prefix>DDS_CYCLONE_%d</Prefix>\
</SharedMemory>\
<Tracing>\
  <Category>" TRACE_CATEGORY "</Category>\
  <OutputFile>cdds.log.%d</OutputFile>\
</Tracing>\
",
                  shm_enable ? "true" : "false",
                  l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7],
                  l[8], l[9], l[10], l[11], l[12], l[13], l[14], l[15],
                  (int) l[0],
                  (int) int_dom);
  char *xconfigstr = ddsrt_expand_envvars (configstr, int_dom);
  const dds_entity_t dom = dds_create_domain (int_dom, xconfigstr);
  CU_ASSERT_FATAL (dom > 0);
  ddsrt_free (xconfigstr);
  ddsrt_free (configstr);
  const dds_entity_t pp = dds_create_participant (int_dom, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  return pp;
}

static struct ddsi_domaingv *get_domaingv (dds_entity_t handle)
{
  dds_return_t rc;
  struct dds_entity *x;
  rc = dds_entity_pin (handle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  struct ddsi_domaingv * const gv = &x->m_domain->gv;
  dds_entity_unpin (x);
  return gv;
}

static bool endpoint_has_iceoryx_enabled (dds_entity_t rd_or_wr)
{
  dds_return_t rc;
  struct dds_entity *x;
  bool iceoryx_enabled = false;
  rc = dds_entity_pin (rd_or_wr, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  switch (dds_entity_kind (x))
  {
    case DDS_KIND_READER: {
      struct dds_reader const * const rd = (struct dds_reader *) x;
      iceoryx_enabled = (rd->m_iox_sub != NULL);
      break;
    }
    case DDS_KIND_WRITER: {
      struct dds_writer const * const wr = (struct dds_writer *) x;
      iceoryx_enabled = (wr->m_iox_pub != NULL);
      break;
    }
    default: {
      CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER || dds_entity_kind (x) == DDS_KIND_WRITER);
      break;
    }
  }
  dds_entity_unpin (x);
  return iceoryx_enabled;
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
  // test code assumes non-Iceoryx means port > 0
  CU_ASSERT_FATAL (port > 0);
  return port;
}

struct check_writer_addrset_helper_arg {
  uint32_t ports_seen;
  int nports;
  const uint32_t *ports;
};

static void check_writer_addrset_helper (const ddsi_xlocator_t *loc, void *varg)
{
  struct check_writer_addrset_helper_arg * const arg = varg;
  // Iceoryx locators are not allowed in writer's address set because that causes it to go through the transmit path
  CU_ASSERT_FATAL (loc->c.kind != NN_LOCATOR_KIND_SHEM);
  CU_ASSERT_FATAL (loc->c.port != 0);
  int i;
  for (i = 0; i < arg->nports; i++)
  {
    if (arg->ports[i] == loc->c.port)
    {
      CU_ASSERT_FATAL ((arg->ports_seen & (1u << i)) == 0);
      arg->ports_seen |= 1u << i;
      break;
    }
  }
  // unknown expected ports not allowed
  CU_ASSERT_FATAL (i < arg->nports);
}

static bool check_writer_addrset (dds_entity_t wrhandle, int nports, const uint32_t ports[nports])
{
  dds_return_t rc;
  struct dds_entity *x;
  rc = dds_entity_pin (wrhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_WRITER);
  struct writer * const wr = ((struct dds_writer *) x)->m_wr;
  CU_ASSERT_FATAL (nports < 31);
  struct check_writer_addrset_helper_arg arg = {
    .ports_seen = 0,
    .nports = nports,
    .ports = ports
  };
  ddsrt_mutex_lock (&wr->e.lock);
  addrset_forall (wr->as, check_writer_addrset_helper, &arg);
  ddsrt_mutex_unlock (&wr->e.lock);
  dds_entity_unpin (x);
  return (arg.ports_seen == (1u << nports) - 1);
}

static dds_entity_t create_endpoint (dds_entity_t tp, bool use_iceoryx, dds_entity_t (*f) (dds_entity_t pp, dds_entity_t tp, const dds_qos_t *qos, const dds_listener_t *listener))
{
  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, 0);
  dds_qset_writer_data_lifecycle (qos, false);
  if (!use_iceoryx) // for now, keep-all suffices to not use Iceoryx
    dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  dds_entity_t ep = f (dds_get_participant (tp), tp, qos, NULL);
  CU_ASSERT_FATAL (ep > 0);
  dds_delete_qos (qos);
  CU_ASSERT_FATAL (endpoint_has_iceoryx_enabled (ep) == use_iceoryx);
  return ep;
}

static dds_entity_t create_reader (dds_entity_t tp, bool use_iceoryx)
{
  return create_endpoint (tp, use_iceoryx, dds_create_reader);
}

static dds_entity_t create_writer (dds_entity_t tp, bool use_iceoryx)
{
  return create_endpoint (tp, use_iceoryx, dds_create_writer);
}

struct tracebuf {
  char buf[512];
  size_t pos;
};

static void print (struct tracebuf *tb, const char *fmt, ...) ddsrt_attribute_format_printf(2, 3);

static void print (struct tracebuf *tb, const char *fmt, ...)
{
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

static int compare_guid (const void *a, const void *b)
{
  return memcmp (a, b, sizeof (dds_guid_t));
}

static bool allmatched (dds_entity_t ws, dds_entity_t wr, int nrds, const dds_entity_t rds[nrds])
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
  qsort (rdguids, (size_t) nrds, sizeof (rdguids[0]), compare_guid);

  const dds_time_t abstimeout = dds_time () + DDS_SECS (2);
  while (dds_time () < abstimeout)
  {
    (void) dds_waitset_wait_until (ws, NULL, 0, abstimeout);

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
    qsort (mguids, (size_t) nms, sizeof (mguids[0]), compare_guid);
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

static bool alldataseen (struct tracebuf *tb, int nrds, const dds_entity_t rds[nrds], dds_instance_state_t instance_state)
{
  assert (nrds > 0);
  dds_return_t rc;
  const dds_entity_t ws = dds_create_waitset (dds_get_participant (rds[0]));
  CU_ASSERT_FATAL (ws > 0);
  dds_entity_t rdconds[nrds];
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
  bool alldataseen = false;
  int dataseen[nrds];
  for (int i = 0; i < nrds; i++)
    dataseen[i] = (rdconds[i] == 0);
  do {
    (void) dds_waitset_wait_until (ws, NULL, 0, abstimeout);
    for (int i = 0; i < nrds; i++)
    {
      if (rdconds[i] != 0)
      {
        Space_Type1 sample;
        void *raw = &sample;
        dds_sample_info_t si;
        int32_t n;
        while ((n = dds_take (rdconds[i], &raw, &si, 1, 1)) > 0)
        {
          if (si.instance_state != instance_state)
          {
            print (tb, "[rd %d %s while expecting %s] ", i, istatestr (si.instance_state), istatestr (instance_state));
            fail_instance_state ();
            goto out;
          }
          dataseen[i]++;
        }
        CU_ASSERT_FATAL (n == 0);
      }
    }
    alldataseen = true;
    for (int i = 0; alldataseen && i < nrds; i++)
      if (dataseen[i] == 0)
        alldataseen = false;
  } while (!alldataseen && dds_time () < abstimeout);

  if (!alldataseen)
  {
    for (int i = 0; i < nrds; i++)
      if (dataseen[i] == 0)
        print (tb, "[rd %d nodata] ", i);
    fail_no_data ();
  }

out:
  for (int i = 0; i < nrds; i++)
  {
    rc = rdconds[i] ? dds_delete (rdconds[i]) : 0;
    CU_ASSERT_FATAL (rc == 0);
  }
  rc = dds_delete (ws);
  CU_ASSERT_FATAL (rc == 0);
  return alldataseen;
}

static int compare_uint32 (const void *va, const void *vb)
{
  const uint32_t *a = va;
  const uint32_t *b = vb;
  return (*a == *b) ? 0 : (*a < *b) ? -1 : 1;
}

static void dotest (void)
{
  dds_return_t rc;
  dds_entity_t pp[MAX_DOMAINS];
  dds_entity_t tp[MAX_DOMAINS];
  struct ddsi_domaingv *gvs[MAX_DOMAINS];

  const dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ws > 0);

  char topicname[100];
  create_unique_topic_name ("test_iceoryx", topicname, sizeof (topicname));
  for (int i = 0; i < MAX_DOMAINS; i++)
  {
    pp[i] = create_participant ((dds_domainid_t) i, true); // FIXME: vary shm_enable for i > 0
    tp[i] = dds_create_topic (pp[i], &Space_Type1_desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL (tp[i] > 0);
    gvs[i] = get_domaingv (pp[i]);
  }

  for (int wr_use_iceoryx = 0; wr_use_iceoryx <= 1; wr_use_iceoryx++)
  {
    const dds_entity_t wr = create_writer (tp[0], (wr_use_iceoryx != 0));
    rc = dds_set_status_mask (wr, DDS_PUBLICATION_MATCHED_STATUS);
    CU_ASSERT_FATAL (rc == 0);
    rc = dds_waitset_attach (ws, wr, 0);
    CU_ASSERT_FATAL (rc == 0);

    // rdmode: trit 0: reader 0; trit 1: reader 1; ...
    //   0: no reader
    //   1: non-iceoryx reader
    //   2: iceoryx reader
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
      bool fail_one = false;

      //if (!wr_use_iceoryx)
      //  goto next;

      if (wr_use_iceoryx)
      {
        // Currently unsupported? Iceoryx writer with DDS readers in same domain
        bool skip = false;
        for (int i = 0; !skip && i < MAX_READERS_PER_DOMAIN; i++)
          if (rdmode[i] == 1)
            skip = true;
        if (skip)
          goto skip;
      }

      print (&tb, "wr: %s; rds:", wr_use_iceoryx ? "iox" : "dds");
      for (int i = 0; rdmode[i] != 0; i++)
      {
        const int dom = i / MAX_READERS_PER_DOMAIN;
        if (i > 0 && dom > (i - 1) / MAX_READERS_PER_DOMAIN)
          print (&tb, " |");
        print (&tb, " %s", (rdmode[i] == 2) ? "iox" : "dds");

        rds[i] = create_reader (tp[dom], rdmode[i] == 2);
        const uint32_t port = reader_unicast_port (rds[i]);
        if (dom == 0)
        {
          // intra-domain: no locators used, ever
        }
        else if (wr_use_iceoryx && dom <= 1 && rdmode[i] == 2)
        {
          // dom 0, 1: same Iceoryx "domain" (service name, locator)
          // Iceoryx should be used -> no locator expected in addrset
        }
        else
        {
          // non-Iceoryx writer, reader, or a different Iceoryx "domain"
          ports[nports++] = port;
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
      if (!allmatched (ws, wr, nrds_active, rds))
      {
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
      if (!check_writer_addrset (wr, nports, ports))
      {
        fail_addrset ();
        fail_one = true;
        goto next;
      }
      print (&tb, "; ");

      // easier on the eyes in the log:
      //dds_sleepfor (DDS_MSECS (100));
      static struct {
        const char *info;
        dds_return_t (*op) (dds_entity_t wr, const void *data);
        dds_instance_state_t istate;
      } const ops[] = {
        { "w", dds_write, DDS_ALIVE_INSTANCE_STATE },
        { "d", dds_dispose, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE },
        { "w", dds_write, DDS_ALIVE_INSTANCE_STATE }, // needed to make unregister visible in RHC
        { "u", dds_unregister_instance, DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE }
      };
      for (size_t opidx = 0; opidx < sizeof (ops) / sizeof (ops[0]); opidx++)
      {
        print (&tb, "%s ", ops[opidx].info); fflush (stdout);
        rc = ops[opidx].op (wr, &(Space_Type1){ 0 });
        CU_ASSERT_FATAL (rc == 0);
        if (!alldataseen (&tb, MAX_READERS_PER_DOMAIN, rds, ops[opidx].istate))
        {
          fail_one = true;
          goto next;
        }
      }

    next:
      print (&tb, ": %s", fail_one ? "FAIL" : "ok");
      for (int i = 0; i < MAX_DOMAINS; i++)
      {
        struct ddsi_domaingv *gv = gvs[i];
        GVTRACE ("#### %s ####\n", tb.buf);
      }
      fputs (fail_one ? "\n" : " $$\r", stdout);
      fflush (stdout);

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

  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_iceoryx, one_writer, .timeout = 30)
{
  failed = false;
  dotest ();
  CU_ASSERT (!failed);
}

CU_Test(ddsc_iceoryx, return_loan)
{
  dds_return_t rc;
  const dds_entity_t pp = create_participant (0, true);
  char topicname[100];
  create_unique_topic_name ("test_iceoryx", topicname, sizeof (topicname));
  const dds_entity_t tp = dds_create_topic (pp, &Array100_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp > 0);
  const dds_entity_t wr = create_writer (tp, true);

  // RouDI config says 10000 * 256 bytes
  // there's some overhead involved, 100 bytes should be ok
  // try 1 .. 3 outstanding loans, and try it so often that there is virtually
  // no chance of it working unless dds_return_loan really does return all loans
  for (int n = 1; n <= 3; n++)
  {
    for (int i = 0; i < 20000; i += n)
    {
      void *sample[3];
      for (int j = 0; j < n; j++)
      {
        rc = dds_loan_sample (wr, &sample[j]);
        CU_ASSERT_FATAL (rc == 0);
      }
      rc = dds_return_loan (wr, sample, n);
      CU_ASSERT_FATAL (rc == 0);
    }
  }

  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
}

