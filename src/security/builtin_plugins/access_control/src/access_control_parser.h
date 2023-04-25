// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef ACCESS_CONTROL_PARSER_H
#define ACCESS_CONTROL_PARSER_H

#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"

typedef enum
{
  ELEMENT_KIND_UNDEFINED,
  ELEMENT_KIND_DDS,
  ELEMENT_KIND_DOMAIN_ACCESS_RULES,
  ELEMENT_KIND_DOMAIN_RULE,
  ELEMENT_KIND_DOMAINS,
  ELEMENT_KIND_DOMAIN_ID_SET,
  ELEMENT_KIND_RANGE,
  ELEMENT_KIND_ALLOW_UNAUTHENTICATED_PARTICIPANTS,
  ELEMENT_KIND_ENABLE_JOIN_ACCESS_CONTROL,
  ELEMENT_KIND_RTPS_PROTECTION,
  ELEMENT_KIND_DISCOVERY_PROTECTION,
  ELEMENT_KIND_LIVELINESS_PROTECTION,
  ELEMENT_KIND_TOPIC_ACCESS_RULES,
  ELEMENT_KIND_TOPIC_RULE,
  ELEMENT_KIND_STRING_VALUE,
  ELEMENT_KIND_BOOLEAN_VALUE,
  ELEMENT_KIND_DOMAIN_VALUE,
  ELEMENT_KIND_PROTECTION_KIND_VALUE,
  ELEMENT_KIND_BASICPROTECTION_KIND_VALUE,
  ELEMENT_KIND_PERMISSIONS,
  ELEMENT_KIND_GRANT,
  ELEMENT_KIND_ALLOW_DENY_RULE,
  ELEMENT_KIND_CRITERIA,
  ELEMENT_KIND_VALIDITY,
  ELEMENT_KIND_TOPICS,
  ELEMENT_KIND_PARTITIONS,
  ELEMENT_KIND_DEFAULT,
  ELEMENT_KIND_IGNORED
} element_kind;

typedef enum
{
  SUBSCRIBE_CRITERIA,
  PUBLISH_CRITERIA
} permission_criteria_type;

typedef enum
{
  ALLOW_RULE,
  DENY_RULE
} permission_rule_type;

typedef struct element
{
  struct element *parent;
  element_kind kind;
  struct element *next; /*used in case of string list usage */
} xml_element;

/* TODO: Change the value nodes for specific nodes for
 * proper value parsing and validating. */

typedef struct string_value
{
  struct element node;
  char *value;
} xml_string_value;

typedef struct boolean_value
{
  struct element node;
  bool value;
} xml_boolean_value;

typedef struct integer_value
{
  struct element node;
  int32_t value;
} xml_integer_value;

typedef struct protection_kind_value
{
  struct element node;
  DDS_Security_ProtectionKind value;
} xml_protection_kind_value;

typedef struct basicprotection_kind_value
{
  struct element node;
  DDS_Security_BasicProtectionKind value;
} xml_basicprotection_kind_value;

typedef struct domain_id_set
{
  struct element node;
  struct integer_value *min;
  struct integer_value *max;
} xml_domain_id_set;

typedef struct domains
{
  struct element node;
  struct domain_id_set *domain_id_set; /*linked list*/
} xml_domains;

typedef struct topic_rule
{
  struct element node;
  struct string_value *topic_expression;
  struct boolean_value *enable_discovery_protection;
  struct boolean_value *enable_liveliness_protection;
  struct boolean_value *enable_read_access_control;
  struct boolean_value *enable_write_access_control;
  struct protection_kind_value *metadata_protection_kind;
  struct basicprotection_kind_value *data_protection_kind;
} xml_topic_rule;

typedef struct topic_access_rules
{
  struct element node;
  struct topic_rule *topic_rule; /*linked_list*/
} xml_topic_access_rules;

typedef struct domain_rule
{
  struct element node;
  struct domains *domains;
  struct boolean_value *allow_unauthenticated_participants;
  struct boolean_value *enable_join_access_control;
  struct protection_kind_value *discovery_protection_kind;
  struct protection_kind_value *liveliness_protection_kind;
  struct protection_kind_value *rtps_protection_kind;
  struct topic_access_rules *topic_access_rules;
} xml_domain_rule;

typedef struct domain_access_rules
{
  struct element node;
  struct domain_rule *domain_rule;
} xml_domain_access_rules;

typedef struct governance_dds
{
  struct element node;
  struct domain_access_rules *domain_access_rules;
} xml_governance_dds;

typedef struct governance_parser
{
  struct governance_dds *dds;
  struct element *current;
} governance_parser;

/* permissions file specific types */
typedef struct validity
{
  struct element node;
  struct string_value *not_before;
  struct string_value *not_after;
} xml_validity;

typedef struct topics
{
  struct element node;
  struct string_value *topic;
} xml_topics;

typedef struct partitions
{
  struct element node;
  struct string_value *partition;
} xml_partitions;

typedef struct criteria
{
  struct element node;
  permission_criteria_type criteria_type;
  struct topics *topics;
  struct partitions *partitions;
} xml_criteria;

typedef struct allow_deny_rule
{
  struct element node;
  permission_rule_type rule_type;
  struct domains *domains;
  struct criteria *criteria;
} xml_allow_deny_rule;

typedef struct grant
{
  struct element node;
  char *name;
  struct string_value *subject_name;
  struct validity *validity;
  struct allow_deny_rule *allow_deny_rule;
  struct string_value *default_action;
} xml_grant;

typedef struct permissions
{
  struct element node;
  struct grant *grant;
} xml_permissions;

typedef struct permissions_dds
{
  struct element node;
  struct permissions *permissions;
} xml_permissions_dds;

typedef struct permissions_parser
{
  struct permissions_dds *dds;
  struct element *current;
} permissions_parser;

bool ac_parse_governance_xml(const char *xml, struct governance_parser **governance_tree, DDS_Security_SecurityException *ex);
bool ac_parse_permissions_xml(const char *xml, struct permissions_parser **permissions_tree, DDS_Security_SecurityException *ex);
void ac_return_governance_tree(struct governance_parser *parser);
void ac_return_permissions_tree(struct permissions_parser *parser);

#define DDS_SECURITY_DEFAULT_GOVERNANCE "<?xml version=\"1.0\" encoding=\"utf-8\"?> \
<dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" \
    xsi:noNamespaceSchemaLocation=\"https://www.omg.org/spec/DDS-SECURITY/20170901/omg_shared_ca_governance.xsd\"> \
    <domain_access_rules>                                                                                               \
        <domain_rule>                                                                                                   \
            <domains>                                                                                                   \
                <!-- All domains -->                                                                                    \
                <id_range>                                                                                              \
                    <min>0</min>                                                                                        \
                    <max>230</max>                                                                                      \
                </id_range>                                                                                             \
            </domains>                                                                                                  \
                                                                                                                        \
            <allow_unauthenticated_participants>false</allow_unauthenticated_participants>                              \
            <enable_join_access_control>false</enable_join_access_control>                                               \
            <discovery_protection_kind>ENCRYPT</discovery_protection_kind>                                              \
            <liveliness_protection_kind>ENCRYPT</liveliness_protection_kind>                                            \
            <rtps_protection_kind>NONE</rtps_protection_kind>                                                           \
            <topic_access_rules>                                                                                        \
                <topic_rule>                                                                                            \
                    <topic_expression>*</topic_expression>                                                              \
                    <enable_liveliness_protection>true</enable_liveliness_protection>                                   \
                    <enable_discovery_protection>true</enable_discovery_protection>                                     \
                    <enable_read_access_control>false</enable_read_access_control>                                      \
                    <enable_write_access_control>false</enable_write_access_control>                                    \
                    <metadata_protection_kind>ENCRYPT</metadata_protection_kind>                                        \
                    <data_protection_kind>ENCRYPT</data_protection_kind>                                                \
                </topic_rule>                                                                                           \
            </topic_access_rules>                                                                                       \
        </domain_rule>                                                                                                  \
    </domain_access_rules>                                                                                              \
</dds>                                      "

#define DDS_SECURITY_DEFAULT_PERMISSIONS "<?xml version=\"1.0\" encoding=\"utf-8\"?>                                    \
<dds xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"                                                            \
    xsi:noNamespaceSchemaLocation=\"https://www.omg.org/spec/DDS-SECURITY/20170901/omg_shared_ca_permissions.xsd\">      \
    <permissions>                                                                                                       \
        <grant name=\"DEFAULT_PERMISSIONS\">                                                                            \
            <subject_name>DEFAULT_SUBJECT</subject_name>                                                                               \
            <validity>                                                                                                  \
                <not_before>2015-09-15T01:00:00</not_before>                                                            \
                <not_after>2115-09-15T01:00:00</not_after>                                                              \
            </validity>                                                                                                 \
            <deny_rule>                                                                                                \
                <domains>                                                                                               \
                    <id_range>                                                                                          \
                        <min>0</min>                                                                                    \
                        <max>230</max>                                                                                  \
                    </id_range>                                                                                         \
                </domains>                                                                                              \
                <publish>                                                                                               \
                    <topics>                                                                                            \
                        <topic>*</topic>                                                                                \
                    </topics>                                                                                           \
                    <partitions/>                                                                                       \
                </publish>                                                                                              \
                <subscribe>                                                                                             \
                    <topics>                                                                                            \
                        <topic>*</topic>                                                                                \
                    </topics>                                                                                           \
                    <partitions/>                                                                                       \
                </subscribe>                                                                                            \
           </deny_rule>                                                                                                \
           <default>DENY</default>                                                                                     \
        </grant>                                                                                                        \
    </permissions>                                                                                                      \
</dds>                                  "

#endif /* ACCESS_CONTROL_UTILS_H */
