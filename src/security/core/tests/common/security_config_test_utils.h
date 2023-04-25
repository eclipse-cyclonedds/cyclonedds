// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_CORE_TEST_SECURITY_CONFIG_TEST_UTILS_H_
#define SECURITY_CORE_TEST_SECURITY_CONFIG_TEST_UTILS_H_

#include <stdlib.h>
#include "dds/ddsrt/environ.h"

struct kvp {
  const char *key;
  const char *value;
  int32_t count;
};

const char * expand_lookup_vars (const char *name, void * data);
const char * expand_lookup_vars_env (const char *name, void * data);
int32_t expand_lookup_unmatched (const struct kvp * lookup_table);

char * get_governance_topic_rule (const char * topic_expr, bool discovery_protection, bool liveliness_protection,
    bool read_ac, bool write_ac, DDS_Security_ProtectionKind metadata_protection_kind, DDS_Security_BasicProtectionKind data_protection_kind);
char * get_governance_config (bool allow_unauth_pp, bool enable_join_ac, DDS_Security_ProtectionKind discovery_protection_kind, DDS_Security_ProtectionKind liveliness_protection_kind,
    DDS_Security_ProtectionKind rtps_protection_kind, const char * topic_rules, bool add_prefix);

char * get_permissions_rules_w_partitions (const char * domain_id, const char * allow_pub_topic, const char * allow_sub_topic, const char ** allow_parts, const char * deny_pub_topic, const char * deny_sub_topic, const char ** deny_parts);
char * get_permissions_rules (const char * domain_id, const char * allow_pub_topic, const char * allow_sub_topic,
    const char * deny_pub_topic, const char * deny_sub_topic);
char * get_permissions_grant (const char * grant_name, const char * subject_name, dds_time_t not_before, dds_time_t not_after,
    const char * rules_xml, const char * default_policy);
char * get_permissions_default_grant (const char * grant_name, const char * subject_name, const char * topic_name);
char * get_permissions_config (char * grants[], size_t ngrants, bool add_prefix);

#endif /* SECURITY_CORE_TEST_SECURITY_CONFIG_TEST_UTILS_H_ */
