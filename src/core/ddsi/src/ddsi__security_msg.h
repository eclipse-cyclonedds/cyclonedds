// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SECURITY_MSG_H
#define DDSI__SECURITY_MSG_H

#include "dds/features.h"

#ifdef DDS_HAS_SECURITY

#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_security_msg.h"
#include "ddsi__plist_generic.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_participant;
struct ddsi_writer;
struct ddsi_proxy_reader;
struct ddsi_serdata;

#define DDS_SECURITY_AUTH_REQUEST                     "dds.sec.auth_request"
#define DDS_SECURITY_AUTH_HANDSHAKE                   "dds.sec.auth"

#define DDS_SECURITY_AUTH_VERSION_MAJOR 1
#define DDS_SECURITY_AUTH_VERSION_MINOR 0

#define DDS_SECURITY_AUTH_TOKEN_CLASS_ID_BASE         "DDS:Auth:PKI-DH:"
#define DDS_SECURITY_AUTH_TOKEN_CLASS_ID              DDS_SECURITY_AUTH_TOKEN_CLASS_ID_BASE DDSRT_STRINGIFY(DDS_SECURITY_AUTH_VERSION_MAJOR) "." DDSRT_STRINGIFY(DDS_SECURITY_AUTH_VERSION_MINOR)

#define DDS_SECURITY_AUTH_REQUEST_TOKEN_CLASS_ID      DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+AuthReq"
#define DDS_SECURITY_AUTH_HANDSHAKE_REQUEST_TOKEN_ID  DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+Req"
#define DDS_SECURITY_AUTH_HANDSHAKE_REPLY_TOKEN_ID    DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+Reply"
#define DDS_SECURITY_AUTH_HANDSHAKE_FINAL_TOKEN_ID    DDS_SECURITY_AUTH_TOKEN_CLASS_ID "+Final"


typedef struct ddsi_message_identity {
  ddsi_guid_t source_guid;
  ddsi_seqno_t sequence_number;
} ddsi_message_identity_t;

typedef struct ddsi_participant_generic_message {
  ddsi_message_identity_t message_identity;
  ddsi_message_identity_t related_message_identity;
  ddsi_guid_t destination_participant_guid;
  ddsi_guid_t destination_endpoint_guid;
  ddsi_guid_t source_endpoint_guid;
  const char *message_class_id;
  ddsi_dataholderseq_t message_data;
} ddsi_participant_generic_message_t;



/**
 * @component security_msg_exchange
 *
 * The arguments are aliased in the resulting message structure. This means
 * that the lifecycle of the arguments should be longer then that of the
 * message.
 *
 * @param msg       the participant generic message to initialize
 * @param wrguid    writer guid
 * @param wrseq     writer sequence number
 * @param dstpguid  destination participant guid
 * @param dsteguid  destination endpoint guid
 * @param srceguid  source endpoint guid
 * @param classid   message class id
 * @param mdata     messate data
 * @param rmid      resulting message identity
 */
void ddsi_participant_generic_message_init(
   ddsi_participant_generic_message_t *msg,
   const ddsi_guid_t *wrguid,
   ddsi_seqno_t wrseq,
   const ddsi_guid_t *dstpguid,
   const ddsi_guid_t *dsteguid,
   const ddsi_guid_t *srceguid,
   const char *classid,
   const ddsi_dataholderseq_t *mdata,
   const ddsi_message_identity_t *rmid);

/**
 * @brief Aliased struct variables will not be freed.
 * @component security_msg_exchange
 *
 * @param msg participant generic message
 */
void ddsi_participant_generic_message_deinit(ddsi_participant_generic_message_t *msg);

/**
 * @component security_msg_exchange
 *
 * Some struct variables are aliased to the given buffer. This means that
 * the lifecycle of the data buffer should be longer then that of the message.
 *
 * @param msg participant generic message to deserialize into
 * @param data serialized data
 * @param len length of serialized data
 * @param bswap byte-swapping required
 * @return dds_return_t
 */
dds_return_t ddsi_participant_generic_message_deseralize(ddsi_participant_generic_message_t *msg, const unsigned char *data, size_t len,
   bool bswap);

/** @component security_msg_exchange */
dds_return_t ddsi_participant_generic_message_serialize(const ddsi_participant_generic_message_t *msg, unsigned char **data, size_t *len);

DDS_EXPORT extern const enum ddsi_pserop ddsi_pserop_participant_generic_message[];
DDS_EXPORT extern const size_t ddsi_pserop_participant_generic_message_nops;

/** @component security_msg_exchange */
int ddsi_volatile_secure_data_filter(struct ddsi_writer *wr, struct ddsi_proxy_reader *prd, struct ddsi_serdata *serdata);

#if defined (__cplusplus)
}
#endif

#endif

#endif /* DDSI__SECURITY_MSG_H */
