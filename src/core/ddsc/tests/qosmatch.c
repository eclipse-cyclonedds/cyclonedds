// Copyright(c) 2019 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "dds/dds.h"

#include "dds/ddsrt/time.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/log.h"
#include "ddsi__log.h"
#include "ddsi__xqos.h"

#include "test_common.h"
#include "RWData.h"

#define NPUB 10
#define NWR_PUB 2

struct thread_arg {
  dds_domainid_t domainid;
  const char *topicname;
};

static void setqos (dds_qos_t *q, size_t i, bool isrd, bool create)
{
  size_t psi = i - (i % NWR_PUB);
  /* Participant, publisher & topic get created with i == 0,
     so make sure those have some crazy data set.  The writers
     should inherit the topic and group data, but the user data
     should be updated for each writer */
  if (create)
  {
    if (psi == 1)
    {
      dds_qset_userdata (q, NULL, 0);
      dds_qset_topicdata (q, NULL, 0);
      dds_qset_groupdata (q, NULL, 0);
    }
    else
    {
      char buf[23];
      snprintf (buf, sizeof (buf), "ud%zu%c", i, isrd ? 'r' : 'w');
      dds_qset_userdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "td%zu", i);
      dds_qset_topicdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "gd%zu", psi);
      dds_qset_groupdata (q, buf, strlen (buf));
    }
  }
  else
  {
    if (psi == 1)
    {
      dds_qset_userdata (q, NULL, 0);
      dds_qset_topicdata (q, NULL, 0);
      dds_qset_groupdata (q, NULL, 0);
    }
    else
    {
      char buf[23];
      snprintf (buf, sizeof (buf), "ud%zu%c", i, isrd ? 'r' : 'w');
      dds_qset_userdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "td%zu", (size_t) 0);
      dds_qset_topicdata (q, buf, strlen (buf));
      snprintf (buf, sizeof (buf), "gd%zu", psi);
      dds_qset_groupdata (q, buf, strlen (buf));
    }
  }

  /* Cyclone's accepting unimplemented QoS settings is a bug, but it does allow
     us to feed it all kinds of nonsense and see whether discovery manages it */

  /* this makes topic transient-local, keep-last 1 */
  dds_qset_durability (q, (dds_durability_kind_t) ((i + 1) % 4));
  dds_qset_history (q, (dds_history_kind_t) ((i + 1) % 2), (int32_t) (i + 1));
  dds_qset_resource_limits (q, (int32_t) i + 3, (int32_t) i + 2, (int32_t) i + 1);
  dds_qset_presentation (q, (dds_presentation_access_scope_kind_t) ((psi + 1) % 3), 1, 1);
#ifdef DDS_HAS_LIFESPAN
  dds_qset_lifespan (q, INT64_C (23456789012345678) + (int32_t) i);
#else
  dds_qset_lifespan (q, DDS_INFINITY);
#endif
#ifdef DDS_HAS_DEADLINE_MISSED
  dds_qset_deadline (q, INT64_C (67890123456789012) + (int32_t) i);
#else
  dds_qset_deadline (q, DDS_INFINITY);
#endif
  dds_qset_latency_budget (q, INT64_C (45678901234567890) + (int32_t) i);
  dds_qset_ownership (q, (dds_ownership_kind_t) ((i + 1) % 2));
  dds_qset_ownership_strength (q, 0x12345670 + (int32_t) i);
  dds_qset_liveliness (q, (dds_liveliness_kind_t) ((i + i) % 2), INT64_C (456789012345678901) + (int32_t) i);
  dds_qset_time_based_filter (q, INT64_C (34567890123456789) + (int32_t) i); /* must be <= deadline */
  if (psi == 0)
    dds_qset_partition1 (q, "p");
  else if (psi == 1)
    dds_qset_partition (q, 0, NULL);
  else
  {
    char **ps = ddsrt_malloc (psi * sizeof (*ps));
    for (size_t j = 0; j < psi; j++)
    {
      const size_t n = 40;
      ps[j] = ddsrt_malloc (n);
      snprintf (ps[j], n, "p%zu_%zu", psi, isrd ? (psi-j-1) : j);
    }
    dds_qset_partition (q, (uint32_t) psi, (const char **) ps);
    for (size_t j = 0; j < psi; j++)
      ddsrt_free (ps[j]);
    ddsrt_free (ps);
  }
  dds_qset_reliability (q, (dds_reliability_kind_t) ((i + 1) % 2), INT64_C (890123456789012345) + (int32_t) i);
  dds_qset_transport_priority (q, 0x23456701 + (int32_t) i);
  dds_qset_destination_order (q, (dds_destination_order_kind_t) ((i + 1) % 2));
  dds_qset_writer_data_lifecycle (q, ((i + 1) % 2) != 0);
  dds_qset_reader_data_lifecycle (q, INT64_C (41234567890123456) + (int32_t) i, INT64_C (912345678901234567) + (int32_t) i);
  dds_qset_durability_service (q, INT64_C (123456789012345678) + (int32_t) i, (dds_history_kind_t) ((i + 1) % 2), (int32_t) (i + 1), (int32_t) i + 3, (int32_t) i + 2, (int32_t) i + 1);
}

static bool pubsub_qos_eq_h (const dds_qos_t *a, dds_entity_t ent)
{
  dds_qos_t *b = dds_create_qos ();
  uint64_t delta = 1;
  struct ddsrt_log_cfg logcfg;
  dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
  if (dds_get_qos (ent, b) < 0)
  {
    DDS_CLOG (DDS_LC_ERROR, &logcfg, "publisher/subscriber qos retrieval failure\n");
  }
  else
  {
    /* internal interface is more luxurious that a simple compare for equality, and
       using that here saves us a ton of code */
    delta = ddsi_xqos_delta (a, b, DDSI_QP_GROUP_DATA | DDSI_QP_PRESENTATION | DDSI_QP_PARTITION);
    if (delta)
    {
      DDS_CLOG (DDS_LC_ERROR, &logcfg, "pub/sub: delta = %"PRIx64"\n", delta);
      ddsi_xqos_log (DDS_LC_ERROR, &logcfg, a); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
      ddsi_xqos_log (DDS_LC_ERROR, &logcfg, b); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
    }
  }
  dds_delete_qos (b);
  return delta == 0;
}

static uint64_t reader_qos_delta (const dds_qos_t *a, const dds_qos_t *b)
{
  return ddsi_xqos_delta (a, b, DDSI_QP_USER_DATA | DDSI_QP_TOPIC_DATA | DDSI_QP_GROUP_DATA | DDSI_QP_DURABILITY | DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS | DDSI_QP_PRESENTATION | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_OWNERSHIP | DDSI_QP_LIVELINESS | DDSI_QP_TIME_BASED_FILTER | DDSI_QP_PARTITION | DDSI_QP_RELIABILITY | DDSI_QP_DESTINATION_ORDER | DDSI_QP_ADLINK_READER_DATA_LIFECYCLE);
}

static bool reader_qos_eq_h (const dds_qos_t *a, dds_entity_t ent)
{
  dds_qos_t *b = dds_create_qos ();
  uint64_t delta = 1;
  struct ddsrt_log_cfg logcfg;
  dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
  if (dds_get_qos (ent, b) < 0)
  {
    DDS_CLOG (DDS_LC_ERROR, &logcfg, "reader qos retrieval failure\n");
  }
  else
  {
    delta = reader_qos_delta (a, b);
    if (delta)
    {
      DDS_CLOG (DDS_LC_ERROR, &logcfg, "reader: delta = %"PRIx64"\n", delta);
      ddsi_xqos_log (DDS_LC_ERROR, &logcfg, a); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
      ddsi_xqos_log (DDS_LC_ERROR, &logcfg, b); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
    }
  }
  dds_delete_qos (b);
  return delta == 0;
}

static uint64_t writer_qos_delta (const dds_qos_t *a, const dds_qos_t *b)
{
  return ddsi_xqos_delta (a, b, DDSI_QP_USER_DATA | DDSI_QP_TOPIC_DATA | DDSI_QP_GROUP_DATA | DDSI_QP_DURABILITY | DDSI_QP_HISTORY | DDSI_QP_RESOURCE_LIMITS | DDSI_QP_PRESENTATION | DDSI_QP_LIFESPAN | DDSI_QP_DEADLINE | DDSI_QP_LATENCY_BUDGET | DDSI_QP_OWNERSHIP | DDSI_QP_OWNERSHIP_STRENGTH | DDSI_QP_LIVELINESS | DDSI_QP_PARTITION | DDSI_QP_RELIABILITY | DDSI_QP_DESTINATION_ORDER | DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE);
}

static bool writer_qos_eq_h (const dds_qos_t *a, dds_entity_t ent)
{
  dds_qos_t *b = dds_create_qos ();
  uint64_t delta = 1;
  struct ddsrt_log_cfg logcfg;
  dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
  if (dds_get_qos (ent, b) < 0)
  {
    DDS_CLOG (DDS_LC_ERROR, &logcfg, "writer qos retrieval failure\n");
  }
  else
  {
    delta = writer_qos_delta (a, b);
    if (delta)
    {
      DDS_CLOG (DDS_LC_ERROR, &logcfg, "writer: delta = %"PRIx64"\n", delta);
      ddsi_xqos_log (DDS_LC_ERROR, &logcfg, a); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
      ddsi_xqos_log (DDS_LC_ERROR, &logcfg, b); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
    }
  }
  dds_delete_qos (b);
  return delta == 0;
}

#define UD_QMPUB "qosmatch_publisher"
#define UD_QMPUBDONE UD_QMPUB ":ok"

static uint32_t pub_thread (void *varg)
{
  const struct thread_arg *arg = varg;
  dds_entity_t dp;
  dds_entity_t tp;
  dds_entity_t pub[NPUB];
  dds_entity_t wr[NPUB][NWR_PUB];
  bool chk[NPUB][NWR_PUB] = { { false } };
  dds_return_t rc;
  dds_qos_t *qos, *ppqos;

  ppqos = dds_create_qos ();
  dds_qset_userdata (ppqos, UD_QMPUB, sizeof (UD_QMPUB) - 1);
  dp = dds_create_participant (arg->domainid, ppqos, NULL);
  CU_ASSERT_FATAL (dp > 0);

  qos = dds_create_qos ();
  setqos (qos, 0, false, true);
  tp = dds_create_topic (dp, &RWData_Msg_desc, arg->topicname, qos, NULL);
  CU_ASSERT_FATAL (tp > 0);

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, false, true);
    pub[i] = dds_create_publisher (dp, qos, NULL);
    CU_ASSERT_FATAL (pub[i] > 0);
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, false, true);
      wr[i][j] = dds_create_writer (pub[i], tp, qos, NULL);
      CU_ASSERT_FATAL (wr[i][j] > 0);
    }
  }

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, false, false);
    CU_ASSERT_FATAL (pubsub_qos_eq_h (qos, pub[i]));
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, false, false);
      CU_ASSERT_FATAL (writer_qos_eq_h (qos, wr[i][j]));
    }
  }

  /* Each writer should match exactly one reader */
  uint32_t nchk = 0;
  while (nchk != NPUB * NWR_PUB)
  {
    for (size_t i = 0; i < NPUB; i++)
    {
      for (size_t j = 0; j < NWR_PUB; j++)
      {
        if (chk[i][j])
          continue;
        dds_instance_handle_t ih;
        dds_builtintopic_endpoint_t *ep;
        rc = dds_get_matched_subscriptions (wr[i][j], &ih, 1);
        CU_ASSERT_FATAL (rc == 0 || rc == 1);
        if (rc == 1)
        {
          ep = dds_get_matched_subscription_data (wr[i][j], ih);
          CU_ASSERT_FATAL (ep != NULL);
          setqos (qos, i * NWR_PUB + j, true, false);
          uint64_t delta = reader_qos_delta (qos, ep->qos);
          if (delta)
          {
            struct ddsrt_log_cfg logcfg;
            dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
            DDS_CLOG (DDS_LC_ERROR, &logcfg, "matched reader: delta = %"PRIx64"\n", delta);
            ddsi_xqos_log (DDS_LC_ERROR, &logcfg, qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
            ddsi_xqos_log (DDS_LC_ERROR, &logcfg, ep->qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
          }
          CU_ASSERT_FATAL (delta == 0);
          dds_builtintopic_free_endpoint (ep);
          chk[i][j] = true;
          nchk++;
        }
      }
    }
    dds_sleepfor (DDS_MSECS (100));
  }

  dds_qset_userdata (ppqos, UD_QMPUBDONE, sizeof (UD_QMPUBDONE) - 1);
  rc = dds_set_qos (dp, ppqos);
  CU_ASSERT_FATAL (rc == 0);

  /* Wait until subscribers terminate */
  printf ("wait for subscribers to terminate\n");
  fflush (stdout);
  while (true)
  {
    for (size_t i = 0; i < NPUB; i++)
    {
      for (size_t j = 0; j < NWR_PUB; j++)
      {
        dds_publication_matched_status_t st;
        rc = dds_get_publication_matched_status (wr[i][j], &st);
        CU_ASSERT_FATAL (rc == 0);
        if (st.current_count)
        {
          goto have_matches;
        }
      }
    }
    break;
  have_matches:
    ;
  }

  dds_delete_qos (qos);
  dds_delete_qos (ppqos);
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
  return 0;
}

static void wait_for_done (dds_entity_t rd, const char *userdata)
{
  void *raw = NULL;
  dds_sample_info_t si;
  bool done = false;
  while (!done)
  {
    while (!done && dds_take (rd, &raw, &si, 1, 1) == 1)
    {
      const dds_builtintopic_participant_t *sample = raw;
      void *ud = NULL;
      size_t usz = 0;
      if (!si.valid_data || !dds_qget_userdata (sample->qos, &ud, &usz))
        continue;
      if (ud && strcmp (ud, userdata) == 0)
        done = true;
      dds_free (ud);
      dds_return_loan (rd, &raw, 1);
    }

    if (!done)
      dds_sleepfor (DDS_MSECS (100));
  }
}

static uint32_t sub_thread (void *varg)
{
  const struct thread_arg *arg = varg;
  dds_entity_t dp, pprd;
  dds_entity_t tp;
  dds_entity_t sub[NPUB];
  dds_entity_t rd[NPUB][NWR_PUB];
  bool chk[NPUB][NWR_PUB] = { { false } };
  dds_return_t rc;
  dds_qos_t *qos;

  dp = dds_create_participant (arg->domainid, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  pprd = dds_create_reader (dp, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL (pprd > 0);

  qos = dds_create_qos ();
  setqos (qos, 0, true, true);
  tp = dds_create_topic (dp, &RWData_Msg_desc, arg->topicname, qos, NULL);
  CU_ASSERT_FATAL (tp > 0);

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, true, true);
    sub[i] = dds_create_subscriber (dp, qos, NULL);
    CU_ASSERT_FATAL (sub[i] > 0);
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, true, true);
      rd[i][j] = dds_create_reader (sub[i], tp, qos, NULL);
      CU_ASSERT_FATAL (rd[i][j] > 0);
    }
  }

  for (size_t i = 0; i < NPUB; i++)
  {
    setqos (qos, i * NWR_PUB, true, false);
    CU_ASSERT_FATAL (pubsub_qos_eq_h (qos, sub[i]));
    for (size_t j = 0; j < NWR_PUB; j++)
    {
      setqos (qos, i * NWR_PUB + j, true, false);
      CU_ASSERT_FATAL (reader_qos_eq_h (qos, rd[i][j]));
    }
  }

  /* Each writer should match exactly one reader */
  uint32_t nchk = 0;
  while (nchk != NPUB * NWR_PUB)
  {
    for (size_t i = 0; i < NPUB; i++)
    {
      for (size_t j = 0; j < NWR_PUB; j++)
      {
        if (chk[i][j])
          continue;
        dds_instance_handle_t ih;
        dds_builtintopic_endpoint_t *ep;
        rc = dds_get_matched_publications (rd[i][j], &ih, 1);
        CU_ASSERT_FATAL (rc == 0 || rc == 1);
        if (rc == 1)
        {
          ep = dds_get_matched_publication_data (rd[i][j], ih);
          CU_ASSERT_FATAL (ep != NULL);
          setqos (qos, i * NWR_PUB + j, false, false);
          uint64_t delta = writer_qos_delta (qos, ep->qos);
          if (delta)
          {
            struct ddsrt_log_cfg logcfg;
            dds_log_cfg_init (&logcfg, 0, DDS_LC_ERROR, stderr, stderr);
            DDS_CLOG (DDS_LC_ERROR, &logcfg, "matched writer: delta = %"PRIx64"\n", delta);
            ddsi_xqos_log (DDS_LC_ERROR, &logcfg, qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
            ddsi_xqos_log (DDS_LC_ERROR, &logcfg, ep->qos); DDS_CLOG (DDS_LC_ERROR, &logcfg, "\n");
          }
          CU_ASSERT_FATAL (delta == 0);
          dds_builtintopic_free_endpoint (ep);
          chk[i][j] = true;
          nchk++;
        }
      }
    }
    dds_sleepfor (DDS_MSECS (100));
  }

  printf ("wait for publisher to have completed its checks\n");
  wait_for_done (pprd, UD_QMPUBDONE);

  dds_delete_qos (qos);
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
  return 0;
}

CU_Test(ddsc_qosmatch, basic)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
   * in configuration is 0, so that both domains will map to the same port number.
   * This allows to create two domains in a single test process. */
  const char *config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>";
  char *pub_conf = ddsrt_expand_envvars (config, 0);
  char *sub_conf = ddsrt_expand_envvars (config, 1);
  const dds_entity_t pub_dom = dds_create_domain (0, pub_conf);
  CU_ASSERT_FATAL (pub_dom > 0);
  const dds_entity_t sub_dom = dds_create_domain (1, sub_conf);
  CU_ASSERT_FATAL (sub_dom > 0);
  ddsrt_free (pub_conf);
  ddsrt_free (sub_conf);

  char topicname[100];
  create_unique_topic_name ("ddsc_qosmatch_basic", topicname, sizeof topicname);

  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  ddsrt_thread_t sub_tid, pub_tid;
  dds_return_t rc;

  struct thread_arg sub_arg = {
    .domainid = 1,
    .topicname = topicname
  };
  rc = ddsrt_thread_create (&sub_tid, "sub_thread", &tattr, sub_thread, &sub_arg);
  CU_ASSERT_FATAL (rc == 0);

  struct thread_arg pub_arg = {
    .domainid = 0,
    .topicname = topicname
  };
  rc = ddsrt_thread_create (&pub_tid, "pub_thread", &tattr, pub_thread, &pub_arg);
  CU_ASSERT_FATAL (rc == 0);

  ddsrt_thread_join (pub_tid, NULL);
  ddsrt_thread_join (sub_tid, NULL);

  rc = dds_delete (pub_dom);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (sub_dom);
  CU_ASSERT_FATAL (rc == 0);
}
