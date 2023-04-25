// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SECURITY_EXCHANGE_H
#define DDSI__SECURITY_EXCHANGE_H

#include "dds/features.h"

#ifdef DDS_HAS_SECURITY

#if defined (__cplusplus)
extern "C" {
#endif

#include "ddsi__radmin.h"
#include "ddsi__security_msg.h"


#define GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS    "dds.sec.participant_crypto_tokens"
#define GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS     "dds.sec.datawriter_crypto_tokens"
#define GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS     "dds.sec.datareader_crypto_tokens"

/** @component security_msg_exchange */
bool ddsi_write_auth_handshake_message(const struct ddsi_participant *pp, const struct ddsi_proxy_participant *proxypp, ddsi_dataholderseq_t *mdata, bool request, const ddsi_message_identity_t *related_message_id);

/** @component security_msg_exchange */
void ddsi_handle_auth_handshake_message(const struct ddsi_receiver_state *rst, ddsi_entityid_t wr_entity_id, struct ddsi_serdata *sample);

/** @component security_msg_exchange */
void ddsi_handle_crypto_exchange_message(const struct ddsi_receiver_state *rst, struct ddsi_serdata *sample);

/** @component security_msg_exchange */
void ddsi_auth_get_serialized_participant_data(struct ddsi_participant *pp, ddsi_octetseq_t *seq);

/** @component security_msg_exchange */
bool ddsi_write_crypto_participant_tokens(const struct ddsi_participant *pp, const struct ddsi_proxy_participant *proxypp, const ddsi_dataholderseq_t *tokens);

/** @component security_msg_exchange */
bool ddsi_write_crypto_writer_tokens(const struct ddsi_writer *wr, const struct ddsi_proxy_reader *prd, const ddsi_dataholderseq_t *tokens);

/** @component security_msg_exchange */
bool ddsi_write_crypto_reader_tokens(const struct ddsi_reader *rd, const struct ddsi_proxy_writer *pwr, const ddsi_dataholderseq_t *tokens);

#if defined (__cplusplus)
}
#endif

#else /* DDS_HAS_SECURITY */

#define ddsi_volatile_secure_data_filter NULL

#endif /* DDS_HAS_SECURITY */

#endif /* DDSI__SECURITY_EXCHANGE_H */
