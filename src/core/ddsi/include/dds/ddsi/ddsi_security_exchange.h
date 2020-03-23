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
#ifndef DDSI_SECURITY_EXCHANGE_H
#define DDSI_SECURITY_EXCHANGE_H

#ifdef DDSI_INCLUDE_SECURITY

#if defined (__cplusplus)
extern "C" {
#endif

#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_security_msg.h"


#define GMCLASSID_SECURITY_PARTICIPANT_CRYPTO_TOKENS    "dds.sec.participant_crypto_tokens"
#define GMCLASSID_SECURITY_DATAWRITER_CRYPTO_TOKENS     "dds.sec.datawriter_crypto_tokens"
#define GMCLASSID_SECURITY_DATAREADER_CRYPTO_TOKENS     "dds.sec.datareader_crypto_tokens"

bool write_auth_handshake_message(const struct participant *pp, const struct proxy_participant *proxypp, nn_dataholderseq_t *mdata, bool request, const nn_message_identity_t *related_message_id);
void handle_auth_handshake_message(const struct receiver_state *rst, ddsi_entityid_t wr_entity_id, struct ddsi_serdata *sample);
void handle_crypto_exchange_message(const struct receiver_state *rst, struct ddsi_serdata *sample);
void auth_get_serialized_participant_data(struct participant *pp, ddsi_octetseq_t *seq);
bool write_crypto_participant_tokens(const struct participant *pp, const struct proxy_participant *proxypp, const nn_dataholderseq_t *tokens);
bool write_crypto_writer_tokens(const struct writer *wr, const struct proxy_reader *prd, const nn_dataholderseq_t *tokens);
bool write_crypto_reader_tokens(const struct reader *rd, const struct proxy_writer *pwr, const nn_dataholderseq_t *tokens);

#if defined (__cplusplus)
}
#endif

#else /* DDSI_INCLUDE_SECURITY */

#define volatile_secure_data_filter NULL

#endif /* DDSI_INCLUDE_SECURITY */

#endif /* DDSI_SECURITY_EXCHANGE_H */
