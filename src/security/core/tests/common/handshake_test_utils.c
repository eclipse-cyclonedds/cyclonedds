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
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/security/dds_security_api.h"
#include "authentication_wrapper.h"
#include "handshake_test_utils.h"

#define TIMEOUT DDS_SECS(2)

static const char * validatation_result_str[] = { "OK", "FAILED", "PENDING_RETRY", "PENDING_HANDSHAKE_REQUEST", "PENDING_HANDSHAKE_MESSAGE", "OK_FINAL_MESSAGE" };
static const char * node_type_str[] = { "UNDEFINED", "REQUESTER", "REPLIER" };

struct Identity localIdentityList[MAX_LOCAL_IDENTITIES];
int numLocal = 0;

struct Identity remoteIdentityList[MAX_REMOTE_IDENTITIES];
int numRemote = 0;

struct Handshake handshakeList[MAX_HANDSHAKES];
int numHandshake = 0;

static void add_local_identity(DDS_Security_IdentityHandle handle, DDS_Security_GUID_t *guid)
{
  printf("add local identity %"PRId64"\n", handle);
  localIdentityList[numLocal].handle = handle;
  memcpy(&localIdentityList[numLocal].guid, guid, sizeof(DDS_Security_GUID_t));
  numLocal++;
}

static int find_local_identity(DDS_Security_IdentityHandle handle)
{
  for (int i = 0; i < (int)numLocal; i++)
  {
    if (localIdentityList[i].handle == handle)
      return i;
  }
  return -1;
}

static int find_remote_identity(DDS_Security_IdentityHandle handle)
{
  for (int i = 0; i < numRemote; i++)
  {
    if (remoteIdentityList[i].handle == handle)
      return i;
  }
  return -1;
}

static void add_remote_identity(DDS_Security_IdentityHandle handle, DDS_Security_GUID_t *guid)
{
  if (find_remote_identity(handle) < 0)
  {
    printf("add remote identity %"PRId64"\n", handle);
    remoteIdentityList[numRemote].handle = handle;
    memcpy(&remoteIdentityList[numRemote].guid, guid, sizeof(DDS_Security_GUID_t));
    numRemote++;
  }
}

static void clear_stores(void)
{
  numLocal = 0;
  numRemote = 0;
  numHandshake = 0;
}

static struct Handshake *add_handshake(enum hs_node_type node_type, DDS_Security_IdentityHandle lHandle, DDS_Security_IdentityHandle rHandle)
{
  printf("add handshake %"PRId64"-%"PRId64"\n", lHandle, rHandle);
  handshakeList[numHandshake].handle = -1;
  handshakeList[numHandshake].node_type = node_type;
  handshakeList[numHandshake].handshakeResult = DDS_SECURITY_VALIDATION_FAILED;
  handshakeList[numHandshake].lidx = find_local_identity(lHandle);
  handshakeList[numHandshake].ridx = find_remote_identity(rHandle);
  handshakeList[numHandshake].finalResult = DDS_SECURITY_VALIDATION_FAILED;
  handshakeList[numHandshake].err_msg = NULL;
  numHandshake++;
  return &handshakeList[numHandshake - 1];
}

static int find_handshake(DDS_Security_HandshakeHandle handle)
{
  for (int i = 0; i < numHandshake; i++)
  {
    if (handshakeList[i].handle == handle)
      return i;
  }
  return -1;
}

static void handle_process_message(dds_domainid_t domain_id, DDS_Security_IdentityHandle handshake)
{
  struct message *msg;
  if ((msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_PROCESS_HANDSHAKE, 0, 0, handshake, TIMEOUT)))
  {
    int idx;
    if ((idx = find_handshake(msg->hsHandle)) >= 0)
    {
      printf("set handshake %"PRId64" final result to '%s' (errmsg: %s)\n", msg->hsHandle, validatation_result_str[msg->result], msg->err_msg);
      handshakeList[idx].finalResult = msg->result;
      handshakeList[idx].err_msg = ddsrt_strdup (msg->err_msg);
    }
    test_authentication_plugin_release_msg(msg);
  }
}

static void handle_begin_handshake_request(dds_domainid_t domain_id, struct Handshake *hs, DDS_Security_IdentityHandle lid, DDS_Security_IdentityHandle rid)
{
  struct message *msg;
  printf("handle begin handshake request %"PRId64"<->%"PRId64"\n", lid, rid);
  if ((msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_BEGIN_HANDSHAKE_REQUEST, lid, rid, 0, TIMEOUT)))
  {
    hs->handle = msg->hsHandle;
    hs->handshakeResult = msg->result;
    if (msg->result != DDS_SECURITY_VALIDATION_FAILED)
      handle_process_message(domain_id, msg->hsHandle);
    else
      hs->err_msg = ddsrt_strdup (msg->err_msg);
    test_authentication_plugin_release_msg(msg);
  }
}

static void handle_begin_handshake_reply(dds_domainid_t domain_id, struct Handshake *hs, DDS_Security_IdentityHandle lid, DDS_Security_IdentityHandle rid)
{
  struct message *msg;
  printf("handle begin handshake reply %"PRId64"<->%"PRId64"\n", lid, rid);
  if ((msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_BEGIN_HANDSHAKE_REPLY, lid, rid, 0, TIMEOUT)))
  {
    hs->handle = msg->hsHandle;
    hs->handshakeResult = msg->result;
    if (msg->result != DDS_SECURITY_VALIDATION_FAILED)
      handle_process_message(domain_id, msg->hsHandle);
    else
      hs->err_msg = ddsrt_strdup (msg->err_msg);
    test_authentication_plugin_release_msg(msg);
  }
}

static void handle_validate_remote_identity(dds_domainid_t domain_id, DDS_Security_IdentityHandle lid, int count)
{
  struct message *msg;
  while (count-- > 0 && (msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_VALIDATE_REMOTE_IDENTITY, lid, 0, 0, TIMEOUT)))
  {
    struct Handshake *hs;
    add_remote_identity(msg->ridHandle, &msg->rguid);
    hs = add_handshake(HSN_UNDEFINED, lid, msg->ridHandle);
    if (msg->result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST)
    {
      hs->node_type = HSN_REQUESTER;
      handle_begin_handshake_request(domain_id, hs, lid, msg->ridHandle);
    }
    else if (msg->result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
    {
      hs->node_type = HSN_REPLIER;
      handle_begin_handshake_reply(domain_id, hs, lid, msg->ridHandle);
    }
    else
    {
      printf("validate remote failed\n");
    }
    test_authentication_plugin_release_msg(msg);
  }
}

static void handle_validate_local_identity(dds_domainid_t domain_id, bool exp_localid_fail, const char * exp_localid_msg)
{
 struct message *msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_VALIDATE_LOCAL_IDENTITY, 0, 0, 0, TIMEOUT);
  CU_ASSERT_FATAL (msg != NULL);
  CU_ASSERT_FATAL ((msg->result == DDS_SECURITY_VALIDATION_OK) != exp_localid_fail);
  if (exp_localid_fail && exp_localid_msg)
  {
    printf("validate_local_identity failed as expected (msg: %s)\n", msg->err_msg);
    CU_ASSERT_FATAL (msg->err_msg && strstr(msg->err_msg, exp_localid_msg) != NULL);
  }
  else
    add_local_identity (msg->lidHandle, &msg->lguid);
  test_authentication_plugin_release_msg (msg);
}

void validate_handshake(dds_domainid_t domain_id, bool exp_localid_fail, const char * exp_localid_msg, struct Handshake *hs_list[], int *nhs)
{
  clear_stores();

  if (nhs)
    *nhs = 0;
  if (hs_list)
    *hs_list = NULL;

  handle_validate_local_identity(domain_id, exp_localid_fail, exp_localid_msg);
  if (!exp_localid_fail)
  {
    handle_validate_remote_identity (domain_id, localIdentityList[0].handle, 1);
    for (int n = 0; n < numHandshake; n++)
    {
      struct Handshake *hs = &handshakeList[n];
      printf("Result: hs %"PRId64", node type %s, final result %s\n", hs->handle, node_type_str[hs->node_type], validatation_result_str[hs->finalResult]);
      if (hs->err_msg && strlen (hs->err_msg))
        printf("- err_msg: %s\n", hs->err_msg);
    }
    if (nhs)
      *nhs = numHandshake;
    if (hs_list)
      *hs_list = handshakeList;
    else
      handshake_list_fini(handshakeList, numHandshake);
  }
  printf ("finished validate handshake for domain %d\n\n", domain_id);
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

void handshake_list_fini(struct Handshake *hs_list, int nhs)
{
  for (int n = 0; n < nhs; n++)
  {
    struct Handshake hs = hs_list[n];
    ddsrt_free (hs.err_msg);
  }
}