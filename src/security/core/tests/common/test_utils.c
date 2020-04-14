/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>
#include <assert.h>

#include "CUnit/Test.h"
#include "dds/dds.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/heap.h"
#include "dds/security/dds_security_api.h"
#include "authentication_wrapper.h"
#include "test_utils.h"
#include "SecurityCoreTests.h"

#define HS_TIMEOUT DDS_SECS(2)

struct Identity localIdentityList[MAX_LOCAL_IDENTITIES];
int numLocal = 0;

struct Identity remoteIdentityList[MAX_REMOTE_IDENTITIES];
int numRemote = 0;

struct Handshake handshakeList[MAX_HANDSHAKES];
int numHandshake = 0;

static char * get_validation_result_str (DDS_Security_ValidationResult_t result)
{
  switch (result)
  {
    case DDS_SECURITY_VALIDATION_OK: return "OK";
    case DDS_SECURITY_VALIDATION_PENDING_RETRY: return "PENDING_RETRY";
    case DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST: return "PENDING_HANDSHAKE_REQUEST";
    case DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE: return "PENDING_HANDSHAKE_MESSAGE";
    case DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE: return "OK_FINAL_MESSAGE";
    case DDS_SECURITY_VALIDATION_FAILED: return "FAILED";
  }
  abort ();
  return "";
}

static char * get_node_type_str (enum hs_node_type node_type)
{
  switch (node_type)
  {
    case HSN_UNDEFINED: return "UNDEFINED";
    case HSN_REQUESTER: return "REQUESTER";
    case HSN_REPLIER: return "REPLIER";
  }
  abort ();
  return "";
}

void print_test_msg (const char *msg, ...)
{
  va_list args;
  dds_time_t t = dds_time ();
  printf ("%d.%06d ", (int32_t) (t / DDS_NSECS_IN_SEC), (int32_t) (t % DDS_NSECS_IN_SEC) / 1000);
  va_start (args, msg);
  vprintf (msg, args);
  va_end (args);
}

static void add_local_identity (DDS_Security_IdentityHandle handle, DDS_Security_GUID_t *guid)
{
  print_test_msg ("add local identity %"PRId64"\n", handle);
  localIdentityList[numLocal].handle = handle;
  memcpy (&localIdentityList[numLocal].guid, guid, sizeof(DDS_Security_GUID_t));
  numLocal++;
}

static int find_local_identity (DDS_Security_IdentityHandle handle)
{
  for (int i = 0; i < (int) numLocal; i++)
  {
    if (localIdentityList[i].handle == handle)
      return i;
  }
  return -1;
}

static int find_remote_identity (DDS_Security_IdentityHandle handle)
{
  for (int i = 0; i < numRemote; i++)
  {
    if (remoteIdentityList[i].handle == handle)
      return i;
  }
  return -1;
}

static void add_remote_identity (DDS_Security_IdentityHandle handle, DDS_Security_GUID_t *guid)
{
  if (find_remote_identity (handle) < 0)
  {
    print_test_msg ("add remote identity %"PRId64"\n", handle);
    remoteIdentityList[numRemote].handle = handle;
    memcpy (&remoteIdentityList[numRemote].guid, guid, sizeof(DDS_Security_GUID_t));
    numRemote++;
  }
}

static void clear_stores(void)
{
  numLocal = 0;
  numRemote = 0;
  numHandshake = 0;
}

static struct Handshake *add_handshake (enum hs_node_type node_type, DDS_Security_IdentityHandle lHandle, DDS_Security_IdentityHandle rHandle)
{
  print_test_msg ("add handshake %"PRId64"-%"PRId64"\n", lHandle, rHandle);
  handshakeList[numHandshake].handle = -1;
  handshakeList[numHandshake].node_type = node_type;
  handshakeList[numHandshake].handshakeResult = DDS_SECURITY_VALIDATION_FAILED;
  handshakeList[numHandshake].lidx = find_local_identity (lHandle);
  handshakeList[numHandshake].ridx = find_remote_identity (rHandle);
  handshakeList[numHandshake].finalResult = DDS_SECURITY_VALIDATION_FAILED;
  handshakeList[numHandshake].err_msg = NULL;
  numHandshake++;
  return &handshakeList[numHandshake - 1];
}

static int find_handshake (DDS_Security_HandshakeHandle handle)
{
  for (int i = 0; i < numHandshake; i++)
  {
    if (handshakeList[i].handle == handle)
      return i;
  }
  return -1;
}

static void handle_process_message (dds_domainid_t domain_id, DDS_Security_IdentityHandle handshake)
{
  struct message *msg;
  if ((msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_PROCESS_HANDSHAKE, 0, 0, handshake, HS_TIMEOUT)))
  {
    int idx;
    if ((idx = find_handshake (msg->hsHandle)) >= 0)
    {
      print_test_msg ("set handshake %"PRId64" final result to '%s' (errmsg: %s)\n", msg->hsHandle, get_validation_result_str (msg->result), msg->err_msg);
      handshakeList[idx].finalResult = msg->result;
      handshakeList[idx].err_msg = ddsrt_strdup (msg->err_msg);
    }
    test_authentication_plugin_release_msg (msg);
  }
}

static void handle_begin_handshake_request (dds_domainid_t domain_id, struct Handshake *hs, DDS_Security_IdentityHandle lid, DDS_Security_IdentityHandle rid)
{
  struct message *msg;
  print_test_msg ("handle begin handshake request %"PRId64"<->%"PRId64"\n", lid, rid);
  if ((msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_BEGIN_HANDSHAKE_REQUEST, lid, rid, 0, HS_TIMEOUT)))
  {
    hs->handle = msg->hsHandle;
    hs->handshakeResult = msg->result;
    if (msg->result != DDS_SECURITY_VALIDATION_FAILED)
      handle_process_message (domain_id, msg->hsHandle);
    else
      hs->err_msg = ddsrt_strdup (msg->err_msg);
    test_authentication_plugin_release_msg (msg);
  }
}

static void handle_begin_handshake_reply (dds_domainid_t domain_id, struct Handshake *hs, DDS_Security_IdentityHandle lid, DDS_Security_IdentityHandle rid)
{
  struct message *msg;
  print_test_msg ("handle begin handshake reply %"PRId64"<->%"PRId64"\n", lid, rid);
  if ((msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_BEGIN_HANDSHAKE_REPLY, lid, rid, 0, HS_TIMEOUT)))
  {
    hs->handle = msg->hsHandle;
    hs->handshakeResult = msg->result;
    if (msg->result != DDS_SECURITY_VALIDATION_FAILED)
      handle_process_message (domain_id, msg->hsHandle);
    else
      hs->err_msg = ddsrt_strdup (msg->err_msg);
    test_authentication_plugin_release_msg (msg);
  }
}

static void handle_validate_remote_identity (dds_domainid_t domain_id, DDS_Security_IdentityHandle lid, int count)
{
  struct message *msg;
  while (count-- > 0 && (msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_VALIDATE_REMOTE_IDENTITY, lid, 0, 0, HS_TIMEOUT)))
  {
    struct Handshake *hs;
    add_remote_identity (msg->ridHandle, &msg->rguid);
    hs = add_handshake (HSN_UNDEFINED, lid, msg->ridHandle);
    if (msg->result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST)
    {
      hs->node_type = HSN_REQUESTER;
      handle_begin_handshake_request (domain_id, hs, lid, msg->ridHandle);
    }
    else if (msg->result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
    {
      hs->node_type = HSN_REPLIER;
      handle_begin_handshake_reply (domain_id, hs, lid, msg->ridHandle);
    }
    else
    {
      print_test_msg ("validate remote failed\n");
    }
    test_authentication_plugin_release_msg (msg);
  }
}

static void handle_validate_local_identity (dds_domainid_t domain_id, bool exp_localid_fail, const char * exp_localid_msg)
{
 struct message *msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_VALIDATE_LOCAL_IDENTITY, 0, 0, 0, HS_TIMEOUT);
  CU_ASSERT_FATAL (msg != NULL);
  CU_ASSERT_FATAL ((msg->result == DDS_SECURITY_VALIDATION_OK) != exp_localid_fail);
  if (exp_localid_fail && exp_localid_msg)
  {
    print_test_msg ("validate_local_identity failed as expected (msg: %s)\n", msg->err_msg);
    CU_ASSERT_FATAL (msg->err_msg && strstr (msg->err_msg, exp_localid_msg) != NULL);
  }
  else
    add_local_identity (msg->lidHandle, &msg->lguid);
  test_authentication_plugin_release_msg (msg);
}

void validate_handshake (dds_domainid_t domain_id, bool exp_localid_fail, const char * exp_localid_msg, struct Handshake *hs_list[], int *nhs)
{
  clear_stores ();

  if (nhs)
    *nhs = 0;
  if (hs_list)
    *hs_list = NULL;

  handle_validate_local_identity (domain_id, exp_localid_fail, exp_localid_msg);
  if (!exp_localid_fail)
  {
    handle_validate_remote_identity (domain_id, localIdentityList[0].handle, 1);
    for (int n = 0; n < numHandshake; n++)
    {
      struct Handshake *hs = &handshakeList[n];
      print_test_msg ("Result: hs %"PRId64", node type %s, final result %s\n", hs->handle, get_node_type_str (hs->node_type), get_validation_result_str (hs->finalResult));
      if (hs->err_msg && strlen (hs->err_msg))
        print_test_msg ("- err_msg: %s\n", hs->err_msg);
    }
    if (nhs)
      *nhs = numHandshake;
    if (hs_list)
      *hs_list = handshakeList;
    else
      handshake_list_fini (handshakeList, numHandshake);
  }
  print_test_msg ("finished validate handshake for domain %d\n\n", domain_id);
}

void validate_handshake_nofail (dds_domainid_t domain_id)
{
  struct Handshake *hs_list;
  int nhs;
  validate_handshake (domain_id, false, NULL, &hs_list, &nhs);
  for (int n = 0; n < nhs; n++)
  {
    struct Handshake hs = hs_list[n];
    DDS_Security_ValidationResult_t exp_result = hs.node_type == HSN_REQUESTER ? DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE : DDS_SECURITY_VALIDATION_OK;
    CU_ASSERT_EQUAL_FATAL (hs.finalResult, exp_result);
  }
  handshake_list_fini (hs_list, nhs);
}

void handshake_list_fini (struct Handshake *hs_list, int nhs)
{
  for (int n = 0; n < nhs; n++)
  {
    struct Handshake hs = hs_list[n];
    ddsrt_free (hs.err_msg);
  }
}

void sync_writer_to_readers (dds_entity_t pp_wr, dds_entity_t wr, uint32_t exp_count)
{
  dds_attach_t triggered;
  dds_entity_t ws = dds_create_waitset (pp_wr);
  CU_ASSERT_FATAL (ws > 0);
  dds_publication_matched_status_t pub_matched;

  dds_return_t ret = dds_waitset_attach (ws, wr, wr);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  while (true)
  {
    ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(2));
    CU_ASSERT_EQUAL_FATAL (exp_count > 0, ret >= 1);
    if (exp_count > 0)
      CU_ASSERT_EQUAL_FATAL (wr, (dds_entity_t)(intptr_t) triggered);
    ret = dds_get_publication_matched_status (wr, &pub_matched);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    if (pub_matched.total_count >= exp_count)
      break;
  };
  dds_delete (ws);
  CU_ASSERT_EQUAL_FATAL (pub_matched.total_count, exp_count);
}

void sync_reader_to_writers (dds_entity_t pp_rd, dds_entity_t rd, uint32_t exp_count)
{
  dds_attach_t triggered;
  dds_entity_t ws = dds_create_waitset (pp_rd);
  CU_ASSERT_FATAL (ws > 0);
  dds_subscription_matched_status_t sub_matched;

  dds_return_t ret = dds_waitset_attach (ws, rd, rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  while (true)
  {
    ret = dds_waitset_wait (ws, &triggered, 1, DDS_SECS(2));
    CU_ASSERT_EQUAL_FATAL (exp_count > 0, ret >= 1);
    if (exp_count > 0)
      CU_ASSERT_EQUAL_FATAL (rd, (dds_entity_t)(intptr_t) triggered);
    ret = dds_get_subscription_matched_status (rd, &sub_matched);
    CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
    if (sub_matched.total_count >= exp_count)
      break;
  };
  dds_delete (ws);
  CU_ASSERT_EQUAL_FATAL (sub_matched.total_count, exp_count);
}

char *create_topic_name (const char *prefix, uint32_t nr, char *name, size_t size)
{
  ddsrt_pid_t pid = ddsrt_getpid ();
  ddsrt_tid_t tid = ddsrt_gettid ();
  (void)snprintf(name, size, "%s%d_pid%" PRIdPID "_tid%" PRIdTID "", prefix, nr, pid, tid);
  return name;
}

bool reader_wait_for_data (dds_entity_t pp, dds_entity_t rd, dds_duration_t dur)
{
  dds_attach_t triggered;
  dds_entity_t ws = dds_create_waitset (pp);
  CU_ASSERT_FATAL (ws > 0);
  dds_return_t ret = dds_waitset_attach (ws, rd, rd);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (ws, &triggered, 1, dur);
  if (ret > 0)
    CU_ASSERT_EQUAL_FATAL (rd, (dds_entity_t)(intptr_t)triggered);
  dds_delete (ws);
  return ret > 0;
}

void rd_wr_init_fail(
    dds_entity_t pp_wr, dds_entity_t *pub, dds_entity_t *pub_tp, dds_entity_t *wr,
    dds_entity_t pp_rd, dds_entity_t *sub, dds_entity_t *sub_tp, dds_entity_t *rd,
    const char * topic_name,
    bool exp_pubtp_fail, bool exp_wr_fail,
    bool exp_subtp_fail, bool exp_rd_fail)
{
  dds_qos_t * qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, -1);
  dds_qset_durability (qos, DDS_DURABILITY_TRANSIENT_LOCAL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);

  *pub = dds_create_publisher (pp_wr, NULL, NULL);
  CU_ASSERT_FATAL (*pub > 0);
  *sub = dds_create_subscriber (pp_rd, NULL, NULL);
  CU_ASSERT_FATAL (*sub > 0);
  *pub_tp = dds_create_topic (pp_wr, &SecurityCoreTests_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL (exp_pubtp_fail, *pub_tp <= 0);
  *sub_tp = dds_create_topic (pp_rd, &SecurityCoreTests_Type1_desc, topic_name, NULL, NULL);
  CU_ASSERT_EQUAL_FATAL (exp_subtp_fail, *sub_tp <= 0);
  if (!exp_pubtp_fail)
  {
    *wr = dds_create_writer (*pub, *pub_tp, qos, NULL);
    CU_ASSERT_EQUAL_FATAL (exp_wr_fail, *wr <= 0);
    if (exp_wr_fail)
      goto fail;
    dds_set_status_mask (*wr, DDS_PUBLICATION_MATCHED_STATUS);
  }
  if (!exp_subtp_fail)
  {
    *rd = dds_create_reader (*sub, *sub_tp, qos, NULL);
    CU_ASSERT_EQUAL_FATAL (exp_rd_fail, *rd <= 0);
    if (exp_rd_fail)
      goto fail;
    dds_set_status_mask (*rd, DDS_SUBSCRIPTION_MATCHED_STATUS);
  }
fail:
  dds_delete_qos (qos);
}

void rd_wr_init(
    dds_entity_t pp_wr, dds_entity_t *pub, dds_entity_t *pub_tp, dds_entity_t *wr,
    dds_entity_t pp_rd, dds_entity_t *sub, dds_entity_t *sub_tp, dds_entity_t *rd,
    const char * topic_name)
{
  rd_wr_init_fail (pp_wr, pub, pub_tp, wr, pp_rd, sub, sub_tp, rd, topic_name, false, false, false, false);
}

void write_read_for(dds_entity_t wr, dds_entity_t pp_rd, dds_entity_t rd, dds_duration_t dur, bool exp_write_fail, bool exp_read_fail)
{
  SecurityCoreTests_Type1 sample = { 1, 1 };
  SecurityCoreTests_Type1 rd_sample;
  void * samples[] = { &rd_sample };
  dds_sample_info_t info[1];
  dds_return_t ret;
  dds_time_t tend = dds_time () + dur;
  bool write_fail = false, read_fail = false;

  dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  do
  {
    print_test_msg ("write\n");
    if (dds_write (wr, &sample) != DDS_RETCODE_OK)
      write_fail = true;

    while (!write_fail)
    {
      if ((ret = dds_take (rd, samples, info, 1, 1)) > 0)
      {
        print_test_msg ("take sample\n");
        if (info[0].instance_state != DDS_IST_ALIVE || !info[0].valid_data)
        {
          print_test_msg ("invalid sample instance_state=%d valid_data=%d\n", info[0].instance_state, info[0].valid_data);
          read_fail = true;
        }
        else
          CU_ASSERT_EQUAL_FATAL (rd_sample.value, 1);
        CU_ASSERT_EQUAL_FATAL (ret, 1);
        break;
      }
      if (ret < 0 || !reader_wait_for_data (pp_rd, rd, DDS_MSECS (1000)))
      {
        print_test_msg ("take no sample\n");
        read_fail = true;
        break;
      }
    }
    if (write_fail || read_fail)
      break;
    dds_sleepfor (DDS_MSECS (100));
  }
  while (dds_time () < tend);
  CU_ASSERT_EQUAL_FATAL (write_fail, exp_write_fail);
  CU_ASSERT_EQUAL_FATAL (read_fail, exp_read_fail);
}
