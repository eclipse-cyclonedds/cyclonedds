// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_CORE_PLUGIN_WRAPPER_MSG_Q_H_
#define SECURITY_CORE_PLUGIN_WRAPPER_MSG_Q_H_

#include "dds/dds.h"
#include "dds/ddsrt/sync.h"

#include "dds/security/dds_security_api.h"

typedef enum {
    MESSAGE_KIND_VALIDATE_LOCAL_IDENTITY,
    MESSAGE_KIND_VALIDATE_REMOTE_IDENTITY,
    MESSAGE_KIND_BEGIN_HANDSHAKE_REQUEST,
    MESSAGE_KIND_BEGIN_HANDSHAKE_REPLY,
    MESSAGE_KIND_PROCESS_HANDSHAKE
} message_kind_t;

struct message {
    message_kind_t kind;
    DDS_Security_IdentityHandle lidHandle;
    DDS_Security_IdentityHandle ridHandle;
    DDS_Security_IdentityHandle hsHandle;
    DDS_Security_GUID_t lguid;
    DDS_Security_GUID_t rguid;
    DDS_Security_ValidationResult_t result;
    char * err_msg;
    DDS_Security_DataHolder token;
    void *instance;
    struct message *next;
};

struct message_queue {
    ddsrt_mutex_t lock;
    ddsrt_cond_t cond;
    struct message *head;
    struct message *tail;
};

enum take_message_result {
  TAKE_MESSAGE_OK, /* message found */
  TAKE_MESSAGE_TIMEOUT_EMPTY, /* no message found, queue is empty */
  TAKE_MESSAGE_TIMEOUT_NONEMPTY /* no message found, queue is not empty */
};

struct dds_security_authentication_impl;

void insert_message(struct message_queue *queue, struct message *msg);
void add_message(struct message_queue *queue, message_kind_t kind, DDS_Security_IdentityHandle lidHandle, DDS_Security_IdentityHandle ridHandle, DDS_Security_IdentityHandle hsHandle,
    const DDS_Security_GUID_t *lguid, const DDS_Security_GUID_t *rguid, DDS_Security_ValidationResult_t result, const char * err_msg,
    const DDS_Security_DataHolder *token, void *instance);
void delete_message(struct message *msg);
void init_message_queue(struct message_queue *queue);
void deinit_message_queue(struct message_queue *queue);
int message_matched(struct message *msg, message_kind_t kind, DDS_Security_IdentityHandle lidHandle, DDS_Security_IdentityHandle ridHandle, DDS_Security_IdentityHandle hsHandle);
enum take_message_result take_message(struct message_queue *queue, message_kind_t kind, DDS_Security_IdentityHandle lidHandle, DDS_Security_IdentityHandle ridHandle, DDS_Security_IdentityHandle hsHandle, dds_time_t abstimeout, struct message **msg);


#endif /* SECURITY_CORE_PLUGIN_WRAPPER_MSG_Q_H_ */
