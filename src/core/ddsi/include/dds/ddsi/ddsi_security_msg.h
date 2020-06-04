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
#ifndef DDSI_SECURITY_MSG_H
#define DDSI_SECURITY_MSG_H

#ifdef DDSI_INCLUDE_SECURITY

#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsi/ddsi_plist_generic.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct participant;
struct writer;
struct proxy_reader;
struct ddsi_serdata;

#define DDS_SECURITY_AUTH_REQUEST                     "dds.sec.auth_request"
#define DDS_SECURITY_AUTH_HANDSHAKE                   "dds.sec.auth"
#define DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID      "DDS:Auth:PKI-DH:1.0+AuthReq"
#define DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID  "DDS:Auth:PKI-DH:1.0+Req"
#define DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID    "DDS:Auth:PKI-DH:1.0+Reply"
#define DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID    "DDS:Auth:PKI-DH:1.0+Final"


typedef struct nn_message_identity {
  ddsi_guid_t source_guid;
  int64_t sequence_number;
} nn_message_identity_t;

typedef struct nn_participant_generic_message {
  nn_message_identity_t message_identity;
  nn_message_identity_t related_message_identity;
  ddsi_guid_t destination_participant_guid;
  ddsi_guid_t destination_endpoint_guid;
  ddsi_guid_t source_endpoint_guid;
  const char *message_class_id;
  nn_dataholderseq_t message_data;
} nn_participant_generic_message_t;


/*
 * The arguments are aliased in the resulting message structure.
 * This means that the lifecycle of the arguments should be longer
 * then that of the message.
 */
DDS_EXPORT void
nn_participant_generic_message_init(
   nn_participant_generic_message_t *msg,
   const ddsi_guid_t *wrguid,
   int64_t wrseq,
   const ddsi_guid_t *dstpguid,
   const ddsi_guid_t *dsteguid,
   const ddsi_guid_t *srceguid,
   const char *classid,
   const nn_dataholderseq_t *mdata,
   const nn_message_identity_t *rmid);

/*
 * Aliased struct variables will not be freed.
 */
DDS_EXPORT void
nn_participant_generic_message_deinit(
   nn_participant_generic_message_t *msg);

/*
 * Some struct variables are aliased to the given buffer.
 * This means that the lifecycle of the data buffer should be
 * longer then that of the message.
 */
DDS_EXPORT dds_return_t
nn_participant_generic_message_deseralize(
   nn_participant_generic_message_t *msg,
   const unsigned char *data,
   size_t len,
   bool bswap);

DDS_EXPORT dds_return_t
nn_participant_generic_message_serialize(
   const nn_participant_generic_message_t *msg,
   unsigned char **data,
   size_t *len);

DDS_EXPORT extern const enum pserop pserop_participant_generic_message[];
DDS_EXPORT extern const size_t pserop_participant_generic_message_nops;

DDS_EXPORT int
volatile_secure_data_filter(
   struct writer *wr,
   struct proxy_reader *prd,
   struct ddsi_serdata *serdata);

#if defined (__cplusplus)
}
#endif

#endif

#endif /* DDSI_SECURITY_MSG_H */
