// Copyright(c) 2006 to 2021 ZettaScale Technology and others
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

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "ddsi__typelib.h"
#include "dds/dds.h"
#include "dds/version.h"
#include "dds__domain.h"
#include "dds__entity.h"

#include "test_common.h"
#include "config_env.h"
#include "XSpace.h"

#define DDS_DOMAINID_PUB 0
#define DDS_DOMAINID_SUB 1
#define DDS_CONFIG "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>"

static dds_entity_t g_domain1 = 0;
static dds_entity_t g_participant1 = 0;
static dds_entity_t g_publisher1 = 0;

static dds_entity_t g_domain2 = 0;
static dds_entity_t g_participant2 = 0;
static dds_entity_t g_subscriber2 = 0;

static void typelookup_init (void)
{
  /* Domains for pub and sub use a different domain id, but the portgain setting
         * in configuration is 0, so that both domains will map to the same port number.
         * This allows to create two domains in a single test process. */
  char *conf1 = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID_PUB);
  char *conf2 = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID_SUB);
  g_domain1 = dds_create_domain (DDS_DOMAINID_PUB, conf1);
  g_domain2 = dds_create_domain (DDS_DOMAINID_SUB, conf2);
  dds_free (conf1);
  dds_free (conf2);

  g_participant1 = dds_create_participant (DDS_DOMAINID_PUB, NULL, NULL);
  CU_ASSERT_FATAL (g_participant1 > 0);
  g_participant2 = dds_create_participant (DDS_DOMAINID_SUB, NULL, NULL);
  CU_ASSERT_FATAL (g_participant2 > 0);

  g_publisher1 = dds_create_publisher (g_participant1, NULL, NULL);
  CU_ASSERT_FATAL (g_publisher1 > 0);
  g_subscriber2 = dds_create_subscriber (g_participant2, NULL, NULL);
  CU_ASSERT_FATAL (g_subscriber2 > 0);
}

static void typelookup_fini (void)
{
  dds_delete (g_subscriber2);
  dds_delete (g_publisher1);
  dds_delete (g_participant2);
  dds_delete (g_participant1);
  dds_delete (g_domain2);
  dds_delete (g_domain1);
}

static void get_type (dds_entity_t entity, ddsi_typeid_t **type_id, char **type_name, ddsi_typeid_kind_t kind)
{
  struct dds_entity *e;
  CU_ASSERT_EQUAL_FATAL (dds_entity_pin (entity, &e), 0);
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  struct ddsi_entity_common *ec = ddsi_entidx_lookup_guid_untyped (e->m_domain->gv.entity_index, &e->m_guid);
  CU_ASSERT_FATAL (ec != NULL);
  assert (ec);
  if (ec->kind == DDSI_EK_PROXY_READER || ec->kind == DDSI_EK_PROXY_WRITER)
  {
    struct ddsi_generic_proxy_endpoint *gpe = (struct ddsi_generic_proxy_endpoint *)ec;
    CU_ASSERT_FATAL (gpe != NULL);
    CU_ASSERT_FATAL (gpe->c.type_pair != NULL);
    assert (gpe->c.type_pair);
    if (kind == DDSI_TYPEID_KIND_COMPLETE)
    {
      CU_ASSERT_FATAL (gpe->c.type_pair->complete != NULL);
      *type_id = ddsi_typeid_dup (ddsi_type_pair_complete_id (gpe->c.type_pair));
    }
    else
    {
      CU_ASSERT_FATAL (gpe->c.type_pair->minimal != NULL);
      *type_id = ddsi_typeid_dup (ddsi_type_pair_minimal_id (gpe->c.type_pair));
    }
    *type_name = ddsrt_strdup (gpe->c.xqos->type_name);
  }
  else if (ec->kind == DDSI_EK_READER)
  {
    struct ddsi_reader *rd = (struct ddsi_reader *) ec;
    CU_ASSERT_FATAL (rd->c.type_pair != NULL);
    assert (rd->c.type_pair);
    if (kind == DDSI_TYPEID_KIND_COMPLETE)
    {
      CU_ASSERT_FATAL (rd->c.type_pair->complete != NULL);
      *type_id = ddsi_typeid_dup (ddsi_type_pair_complete_id (rd->c.type_pair));
    }
    else
    {
      CU_ASSERT_FATAL (rd->c.type_pair->minimal != NULL);
      *type_id = ddsi_typeid_dup (ddsi_type_pair_minimal_id (rd->c.type_pair));
    }
    *type_name = ddsrt_strdup (rd->xqos->type_name);
  }
  else if (ec->kind == DDSI_EK_WRITER)
  {
    struct ddsi_writer *wr = (struct ddsi_writer *) ec;
    CU_ASSERT_FATAL (wr->c.type_pair != NULL);
    assert (wr->c.type_pair);
    if (kind == DDSI_TYPEID_KIND_COMPLETE)
    {
      CU_ASSERT_FATAL (wr->c.type_pair->complete != NULL);
      *type_id = ddsi_typeid_dup (ddsi_type_pair_complete_id (wr->c.type_pair));
    }
    else
    {
      CU_ASSERT_FATAL (wr->c.type_pair->minimal != NULL);
      *type_id = ddsi_typeid_dup (ddsi_type_pair_minimal_id (wr->c.type_pair));
    }
    *type_name = ddsrt_strdup (wr->xqos->type_name);
  }
  else
    abort ();
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  dds_entity_unpin (e);
}

static void print_ep (const dds_guid_t *key)
{
  printf ("endpoint ");
  for (size_t j = 0; j < sizeof (key->v); j++)
    printf ("%s%02x", (j == 0 || j % 4) ? "" : ":", key->v[j]);
}

typedef struct endpoint_info {
  dds_typeinfo_t *type_info;
  char *topic_name;
  char *type_name;
} endpoint_info_t;

static endpoint_info_t * find_typeid_match (dds_entity_t participant, dds_entity_t topic, ddsi_typeid_t *type_id, const char * match_topic, ddsi_typeid_kind_t kind)
{
  endpoint_info_t *result = NULL;
  dds_time_t t_start = dds_time ();
  dds_duration_t timeout = DDS_SECS (5);
  dds_entity_t reader = dds_create_reader (participant, topic, NULL, NULL);
  CU_ASSERT_FATAL (reader > 0);
  do
  {
    void *ptrs[100] = { 0 };
    dds_sample_info_t info[sizeof (ptrs) / sizeof (ptrs[0])];
    int n = dds_take (reader, ptrs, info, sizeof (ptrs) / sizeof (ptrs[0]), sizeof (ptrs) / sizeof (ptrs[0]));
    for (int i = 0; i < n && result == NULL; i++)
    {
      if (info[i].valid_data)
      {
        dds_builtintopic_endpoint_t *data = ptrs[i];
        const dds_typeinfo_t *t;
        dds_return_t ret = dds_builtintopic_get_endpoint_type_info (data, &t);
        CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
        if (t != NULL)
        {
          struct ddsi_typeid_str tidstr;
          ddsi_typeid_t *tid = ddsi_typeinfo_typeid (t, kind);
          print_ep (&data->key);
          printf (" type: %s", ddsi_make_typeid_str (&tidstr, tid));
          if (!ddsi_typeid_compare (tid, type_id) && !strcmp (data->topic_name, match_topic))
          {
            printf(" match");
            // copy data from sample to our own struct
            result = ddsrt_malloc (sizeof (*result));
            result->type_info = ddsi_typeinfo_dup (t);
            result->topic_name = ddsrt_strdup (data->topic_name);
            result->type_name = ddsrt_strdup (data->type_name);
          }
          ddsi_typeid_fini (tid);
          ddsrt_free (tid);
          printf("\n");
        }
        else
        {
          print_ep (&data->key);
          printf (" no type\n");
        }
      }
    }
    if (n > 0)
    {
      dds_return_t ret = dds_return_loan (reader, ptrs, n);
      CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    }
    else
      dds_sleepfor (DDS_MSECS (20));
  }
  while (result == NULL && dds_time () - t_start <= timeout);
  return result;
}

static void endpoint_info_free (endpoint_info_t *ep_info) ddsrt_nonnull_all;

static void endpoint_info_free (endpoint_info_t *ep_info)
{
  ddsi_typeinfo_fini (ep_info->type_info);
  ddsrt_free (ep_info->type_info);
  ddsrt_free (ep_info->topic_name);
  ddsrt_free (ep_info->type_name);
  ddsrt_free (ep_info);
}

static bool reader_wait_for_data (dds_entity_t pp, dds_entity_t rd, dds_duration_t dur)
{
  dds_attach_t triggered;
  dds_entity_t ws = dds_create_waitset (pp);
  CU_ASSERT_FATAL (ws > 0);
  dds_return_t ret = dds_waitset_attach (ws, rd, rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (ws, &triggered, 1, dur);
  if (ret > 0)
    CU_ASSERT_EQUAL_FATAL (rd, (dds_entity_t)(intptr_t) triggered);
  dds_delete (ws);
  return ret > 0;
}

CU_Test(ddsc_typelookup, get_typeobj, .init = typelookup_init, .fini = typelookup_fini)
{
  char topic_name_wr[100], topic_name_rd[100];
  dds_return_t ret;

  create_unique_topic_name ("ddsc_typelookup", topic_name_wr, sizeof (topic_name_wr));
  dds_entity_t topic_wr = dds_create_topic (g_participant1, &XSpace_XType3a_desc, topic_name_wr, NULL, NULL);
  CU_ASSERT_FATAL (topic_wr > 0);
  create_unique_topic_name ("ddsc_typelookup", topic_name_rd, sizeof (topic_name_rd));
  dds_entity_t topic_rd = dds_create_topic (g_participant1, &Space_Type3_desc, topic_name_rd, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd > 0);

  /* Topic in domain 2 with topic same name, different (incompatible) type */
  dds_entity_t topic_rd2 = dds_create_topic (g_participant2, &Space_Type1_desc, topic_name_wr, NULL, NULL);
  CU_ASSERT_FATAL (topic_rd2 > 0);

  /* create a writer and reader on domain 1 */
  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_entity_t writer = dds_create_writer (g_participant1, topic_wr, qos, NULL);
  CU_ASSERT_FATAL (writer > 0);
  dds_entity_t reader = dds_create_reader (g_participant1, topic_rd, qos, NULL);
  CU_ASSERT_FATAL (reader > 0);
  /* create reader on domain 2 (used to force typelookup) */
  dds_entity_t reader2 = dds_create_reader (g_participant2, topic_rd2, qos, NULL);
  CU_ASSERT_FATAL (reader2 > 0);
  dds_delete_qos (qos);
  ddsi_typeid_t *wr_type_id, *rd_type_id;
  char *wr_type_name, *rd_type_name;
  get_type (writer, &wr_type_id, &wr_type_name, false);
  get_type (reader, &rd_type_id, &rd_type_name, false);

  /* check that reader and writer (with correct type id) are discovered in domain 2 */
  endpoint_info_t *writer_ep = find_typeid_match (g_participant2, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, wr_type_id, topic_name_wr, DDSI_TYPEID_KIND_MINIMAL);
  endpoint_info_t *reader_ep = find_typeid_match (g_participant2, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, rd_type_id, topic_name_rd, DDSI_TYPEID_KIND_MINIMAL);
  CU_ASSERT_FATAL (writer_ep != NULL);
  CU_ASSERT_FATAL (reader_ep != NULL);
  assert (writer_ep && reader_ep); // clang static analyzer
  endpoint_info_free (writer_ep);
  endpoint_info_free (reader_ep);

  /* check that type object can be resolved in domain 2 */
  dds_typeobj_t *to_wr = NULL, *to_rd = NULL;
  dds_get_typeobj (g_participant2, wr_type_id, DDS_SECS (3), &to_wr);
  dds_get_typeobj (g_participant2, rd_type_id, DDS_SECS (3), &to_rd);
  CU_ASSERT_FATAL (to_wr != NULL);
  CU_ASSERT_FATAL (to_rd != NULL);
  assert (to_rd && to_wr);
  ret = dds_free_typeobj (to_wr);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_free_typeobj (to_rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  dds_free (wr_type_name);
  dds_free (rd_type_name);
  ddsi_typeid_fini (wr_type_id);
  dds_free (wr_type_id);
  ddsi_typeid_fini (rd_type_id);
  dds_free (rd_type_id);
}


CU_Test(ddsc_typelookup, api_resolve, .init = typelookup_init, .fini = typelookup_fini)
{
  char name[100];
  dds_return_t ret;
  Space_Type1 sample = {0, 0, 0};
  dds_sample_info_t info;
  void * samples[1];
  Space_Type1 rd_sample;
  samples[0] = &rd_sample;

  create_unique_topic_name ("ddsc_typelookup", name, sizeof name);
  dds_entity_t topic = dds_create_topic (g_participant1, &Space_Type3_desc, name, NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  CU_ASSERT_FATAL (qos != NULL);

  /* create a writer and reader on domain 1 */
  dds_entity_t writer = dds_create_writer (g_participant1, topic, qos, NULL);
  CU_ASSERT_FATAL (writer > 0);
  ddsi_typeid_t *type_id;
  char *type_name;
  get_type (writer, &type_id, &type_name, true);

  /* wait for DCPSPublication to be received */
  endpoint_info_t *writer_ep = find_typeid_match (g_participant2, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, type_id, name, DDSI_TYPEID_KIND_COMPLETE);
  CU_ASSERT_FATAL (writer_ep != NULL);
  assert (writer_ep); // clang static analyzer

  /* check if type can be resolved */
  dds_topic_descriptor_t *desc;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_GLOBAL, g_participant2, writer_ep->type_info, DDS_SECS (15), &desc);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  /* create a topic in domain 2 with this sertype and create a reader */
  dds_entity_t pp2_topic = dds_create_topic (g_participant2, desc, writer_ep->topic_name, NULL, NULL);
  CU_ASSERT_FATAL (pp2_topic > 0);
  dds_entity_t reader = dds_create_reader (g_participant2, pp2_topic, qos, NULL);
  CU_ASSERT_FATAL (reader > 0);
  sync_reader_writer (g_participant2, reader, g_participant1, writer);
  dds_delete_topic_descriptor (desc);

  /* write and take a sample */
  ret = dds_set_status_mask (reader, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_write (writer, &sample);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  reader_wait_for_data (g_participant2, reader, DDS_SECS (2));
  ret = dds_take (reader, samples, &info, 1, 1);
  CU_ASSERT_EQUAL_FATAL (ret, 1);

  dds_delete_qos (qos);
  endpoint_info_free (writer_ep);
  dds_free (type_id);
  dds_free (type_name);
}

// the definition of `ddsi_typeid_t` is well hidden, but we need it if we want to
// have a static assertion that an intentional but weird memset doesn't go out of
// bounds
#include "ddsi__xt_impl.h"

CU_Test(ddsc_typelookup, api_resolve_invalid, .init = typelookup_init, .fini = typelookup_fini)
{
  char name[100];
  dds_return_t ret;

  create_unique_topic_name ("ddsc_typelookup", name, sizeof name);
  dds_entity_t topic = dds_create_topic (g_participant1, &Space_Type1_desc, name, NULL, NULL);
  CU_ASSERT_FATAL (topic > 0);

  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (10));
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  CU_ASSERT_FATAL (qos != NULL);

  dds_entity_t writer = dds_create_writer (g_participant1, topic, qos, NULL);
  CU_ASSERT_FATAL (writer > 0);
  ddsi_typeid_t *type_id;
  char *type_name;
  get_type (writer, &type_id, &type_name, true);

  /* wait for DCPSPublication to be received */
  endpoint_info_t *writer_ep = find_typeid_match (g_participant2, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, type_id, name, DDSI_TYPEID_KIND_COMPLETE);
  CU_ASSERT_FATAL (writer_ep != NULL);
  assert (writer_ep); // clang static analyzer

  /* confirm that invalid type id cannot be resolved */
  struct dds_entity *e;
  DDSRT_STATIC_ASSERT (sizeof (*type_id) >= 8);
  // coverity[suspicious_sizeof]
  memset (type_id, 0xff, 8);
  CU_ASSERT_EQUAL_FATAL (dds_entity_pin (g_participant2, &e), 0);
  struct ddsi_type *type;
  ret = ddsi_wait_for_type_resolved (&e->m_domain->gv, type_id, DDS_SECS (3), &type, DDSI_TYPE_INCLUDE_DEPS, DDSI_TYPE_SEND_REQUEST);
  dds_entity_unpin (e);
  CU_ASSERT_NOT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  dds_delete_qos (qos);
  endpoint_info_free (writer_ep);
  ddsi_typeid_fini (type_id);
  dds_free (type_id);
  dds_free (type_name);
}
