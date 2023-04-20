// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_CORE_TEST_UTILS_H_
#define SECURITY_CORE_TEST_UTILS_H_

#include "dds/dds.h"
#include "dds/ddsrt/sync.h"

#include "dds/security/dds_security_api.h"

#define PK_N DDS_SECURITY_PROTECTION_KIND_NONE
#define PK_S DDS_SECURITY_PROTECTION_KIND_SIGN
#define PK_SOA DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION
#define PK_E DDS_SECURITY_PROTECTION_KIND_ENCRYPT
#define PK_EOA DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION
#define BPK_N DDS_SECURITY_BASICPROTECTION_KIND_NONE
#define BPK_S DDS_SECURITY_BASICPROTECTION_KIND_SIGN
#define BPK_E DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT

#define PF_F "file:"
#define PF_D "data:,"

#define MAX_LOCAL_IDENTITIES 8
#define MAX_REMOTE_IDENTITIES 8
#define MAX_HANDSHAKES 32

union guid {
  DDS_Security_GUID_t g;
  unsigned u[4];
};

enum hs_node_type
{
  HSN_UNDEFINED,
  HSN_REQUESTER,
  HSN_REPLIER
};

struct Identity
{
  DDS_Security_IdentityHandle handle;
  union guid guid;
};

struct Handshake
{
  DDS_Security_HandshakeHandle handle;
  enum hs_node_type node_type;
  int lidx;
  int ridx;
  DDS_Security_ValidationResult_t handshakeResult;
  DDS_Security_ValidationResult_t finalResult;
  char * err_msg;
};

void print_test_msg (const char *msg, ...);
void validate_handshake (dds_domainid_t domain_id, bool exp_localid_fail, const char * exp_localid_msg, struct Handshake *hs_list[], int *nhs, dds_duration_t timeout);
void validate_handshake_nofail (dds_domainid_t domain_id, dds_duration_t timeout);
void validate_handshake_result (struct Handshake *hs, bool exp_fail_hs_req, const char * fail_hs_req_msg, bool exp_fail_hs_reply, const char * fail_hs_reply_msg);
void handshake_list_fini (struct Handshake *hs_list, int nhs);
char *create_topic_name (const char *prefix, uint32_t nr, char *name, size_t size);
void sync_writer_to_readers (dds_entity_t pp_wr, dds_entity_t wr, uint32_t exp_count, dds_time_t abstimeout);
void sync_reader_to_writers (dds_entity_t pp_rd, dds_entity_t rd, uint32_t exp_count, dds_time_t abstimeout);
bool reader_wait_for_data (dds_entity_t pp, dds_entity_t rd, dds_duration_t dur);
dds_qos_t * get_default_test_qos (void);
void rd_wr_init (
    dds_entity_t pp_wr, dds_entity_t *pub, dds_entity_t *pub_tp, dds_entity_t *wr,
    dds_entity_t pp_rd, dds_entity_t *sub, dds_entity_t *sub_tp, dds_entity_t *rd,
    const char * topic_name);
void rd_wr_init_fail (
    dds_entity_t pp_wr, dds_entity_t *pub, dds_entity_t *pub_tp, dds_entity_t *wr,
    dds_entity_t pp_rd, dds_entity_t *sub, dds_entity_t *sub_tp, dds_entity_t *rd,
    const char * topic_name,
    bool exp_pubtp_fail, bool exp_wr_fail,
    bool exp_subtp_fail, bool exp_rd_fail);
void rd_wr_init_w_partitions_fail(
    dds_entity_t pp_wr, dds_entity_t *pub, dds_entity_t *pub_tp, dds_entity_t *wr,
    dds_entity_t pp_rd, dds_entity_t *sub, dds_entity_t *sub_tp, dds_entity_t *rd,
    const char * topic_name,
    const char ** partition_names,
    bool exp_pubtp_fail, bool exp_wr_fail,
    bool exp_subtp_fail, bool exp_rd_fail);
void write_read_for (dds_entity_t wr, dds_entity_t pp_rd, dds_entity_t rd, dds_duration_t dur, bool exp_write_fail, bool exp_read_fail);
const char * pk_to_str (DDS_Security_ProtectionKind pk);
const char * bpk_to_str (DDS_Security_BasicProtectionKind bpk);
DDS_Security_DatawriterCryptoHandle get_builtin_writer_crypto_handle(dds_entity_t participant, unsigned entityid);
DDS_Security_DatawriterCryptoHandle get_writer_crypto_handle(dds_entity_t writer);

#define GET_SECURITY_PLUGIN_CONTEXT_DECL(name_) \
  struct dds_security_##name_##_impl * get_##name_##_context(dds_entity_t participant);
GET_SECURITY_PLUGIN_CONTEXT_DECL(access_control)
GET_SECURITY_PLUGIN_CONTEXT_DECL(authentication)
GET_SECURITY_PLUGIN_CONTEXT_DECL(cryptography)


#endif /* SECURITY_CORE_TEST_UTILS_H_ */
