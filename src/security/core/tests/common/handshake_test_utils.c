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

static void add_handshake(DDS_Security_HandshakeHandle handle, int isRequest, DDS_Security_IdentityHandle lHandle, DDS_Security_IdentityHandle rHandle, DDS_Security_ValidationResult_t result)
{
  printf("add handshake %"PRId64"\n", handle);
  handshakeList[numHandshake].handle = handle;
  handshakeList[numHandshake].isRequest = isRequest;
  handshakeList[numHandshake].handshakeResult = result;
  handshakeList[numHandshake].lidx = find_local_identity(lHandle);
  handshakeList[numHandshake].ridx = find_remote_identity(rHandle);
  handshakeList[numHandshake].finalResult = DDS_SECURITY_VALIDATION_FAILED;
  numHandshake++;
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

static char * get_validation_result_str(DDS_Security_ValidationResult_t result)
{
  switch (result)
  {
    case DDS_SECURITY_VALIDATION_OK:
      return "ok";
    case DDS_SECURITY_VALIDATION_PENDING_RETRY:
      return "pending retry";
    case DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST:
      return "handshake request";
    case DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE:
      return "handshake message";
    case DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE:
      return "ok final";
    default:
    case DDS_SECURITY_VALIDATION_FAILED:
      return "failed";
  }
}

static bool handle_process_message(dds_domainid_t domain_id, DDS_Security_IdentityHandle handshake)
{
  struct message *msg;
  bool result = false;
  if ((msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_PROCESS_HANDSHAKE, 0, 0, handshake, TIMEOUT)))
  {
    int idx;
    if ((idx = find_handshake(msg->hsHandle)) >= 0)
    {
      printf("set handshake %"PRId64" final result to '%s'\n", msg->hsHandle, get_validation_result_str(msg->result));
      handshakeList[idx].finalResult = msg->result;
      result = true;
    }
    test_authentication_plugin_release_msg(msg);
  }
  return result;
}

static bool handle_begin_handshake_request(dds_domainid_t domain_id, DDS_Security_IdentityHandle lid, DDS_Security_IdentityHandle rid, char ** err_msg)
{
  struct message *msg;
  bool result = false;
  printf("handle begin handshake request %"PRId64"<->%"PRId64"\n", lid, rid);
  if ((msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_BEGIN_HANDSHAKE_REQUEST, lid, rid, 0, TIMEOUT)))
  {
    add_handshake(msg->hsHandle, 1, msg->lidHandle, msg->ridHandle, msg->result);
    if (msg->result == DDS_SECURITY_VALIDATION_OK)
      result = handle_process_message(domain_id, msg->hsHandle);
    else if (msg->result == DDS_SECURITY_VALIDATION_FAILED && err_msg)
      *err_msg = ddsrt_strdup (msg->err_msg);
    test_authentication_plugin_release_msg(msg);
  }
  return result;
}

static bool handle_begin_handshake_reply(dds_domainid_t domain_id, DDS_Security_IdentityHandle lid, DDS_Security_IdentityHandle rid, char ** err_msg)
{
  struct message *msg;
  bool result = false;
  printf("handle begin handshake reply %"PRId64"<->%"PRId64"\n", lid, rid);
  if ((msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_BEGIN_HANDSHAKE_REPLY, lid, rid, 0, TIMEOUT)))
  {
    add_handshake(msg->hsHandle, 0, msg->lidHandle, msg->ridHandle, msg->result);
    if (msg->result == DDS_SECURITY_VALIDATION_OK)
      result = handle_process_message(domain_id, msg->hsHandle);
    else if (msg->result == DDS_SECURITY_VALIDATION_FAILED && err_msg)
      *err_msg = ddsrt_strdup (msg->err_msg);
    test_authentication_plugin_release_msg(msg);
  }
  return result;
}

static bool handle_validate_remote_identity(dds_domainid_t domain_id, DDS_Security_IdentityHandle lid, int count, bool * is_hs_requester, char ** err_msg_req, char ** err_msg_reply)
{
  bool result = true;
  struct message *msg;
  assert(is_hs_requester);
  while (count-- > 0 && result && (msg = test_authentication_plugin_take_msg(domain_id, MESSAGE_KIND_VALIDATE_REMOTE_IDENTITY, lid, 0, 0, TIMEOUT)))
  {
    add_remote_identity(msg->ridHandle, &msg->rguid);
    if (msg->result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST)
    {
      result = handle_begin_handshake_request(domain_id, lid, msg->ridHandle, err_msg_req);
      *is_hs_requester = true;
    }
    else if (msg->result == DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE)
    {
      result = handle_begin_handshake_reply(domain_id, lid, msg->ridHandle, err_msg_reply);
      *is_hs_requester = false;
    }
    else
      result = false;

    test_authentication_plugin_release_msg(msg);
  }
  return result;
}

void validate_handshake(dds_domainid_t domain_id, bool exp_req_fail, const char * exp_req_msg, bool exp_reply_fail, const char * exp_reply_msg)
{
  printf("validate handshake for domain %d\n", domain_id);
  clear_stores();

  struct message *msg = test_authentication_plugin_take_msg (domain_id, MESSAGE_KIND_VALIDATE_LOCAL_IDENTITY, 0, 0, 0, TIMEOUT);
  CU_ASSERT_FATAL (msg != NULL);
  add_local_identity (msg->lidHandle, &msg->lguid);
  test_authentication_plugin_release_msg (msg);
  bool is_requester = false;
  char * err_msg_req = NULL, *err_msg_reply = NULL;
  bool ret = handle_validate_remote_identity (domain_id, localIdentityList[0].handle, 1, &is_requester, &err_msg_req, &err_msg_reply);
  CU_ASSERT_FATAL ((is_requester && ret != exp_req_fail) || (!is_requester && ret != exp_reply_fail));
  if (ret)
  {
    DDS_Security_ValidationResult_t exp_result = is_requester ? DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE : DDS_SECURITY_VALIDATION_OK;
    CU_ASSERT_EQUAL_FATAL (handshakeList[0].finalResult, exp_result);
  }
  else if (is_requester && exp_req_msg)
  {
    CU_ASSERT_FATAL (err_msg_req && strstr(err_msg_req, exp_req_msg) != NULL);
  }
  else if (!is_requester && exp_reply_msg)
  {
    CU_ASSERT_FATAL (err_msg_reply && strstr(err_msg_reply, exp_reply_msg) != NULL);
  }
  if (err_msg_req)
    ddsrt_free (err_msg_req);
  if (err_msg_reply)
    ddsrt_free (err_msg_reply);
  printf ("finished validate handshake for domain %d\n\n", domain_id);
}

