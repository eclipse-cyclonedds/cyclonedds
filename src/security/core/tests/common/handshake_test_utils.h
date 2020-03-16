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
#ifndef SECURITY_CORE_HANDSHAKE_TEST_UTILS_H_
#define SECURITY_CORE_HANDSHAKE_TEST_UTILS_H_

#include "dds/dds.h"
#include "dds/ddsrt/sync.h"

#include "dds/security/dds_security_api.h"

#define MAX_LOCAL_IDENTITIES 8
#define MAX_REMOTE_IDENTITIES 8
#define MAX_HANDSHAKES 32
#define TIMEOUT DDS_SECS(2)

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

void validate_handshake(dds_domainid_t domain_id, bool exp_localid_fail, const char * exp_localid_msg, struct Handshake *hs_list[], int *nhs);
void validate_handshake_nofail (dds_domainid_t domain_id);
void handshake_list_fini(struct Handshake *hs_list, int nhs);

#endif /* SECURITY_CORE_HANDSHAKE_TEST_UTILS_H_ */
