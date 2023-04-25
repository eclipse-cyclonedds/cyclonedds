// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/xmlparser.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "access_control_parser.h"
#include "access_control_utils.h"

#define DEBUG_PARSER 0
#if (DEBUG_PARSER)

static void print_tab(int spaces)
{
  while (spaces > 0)
  {
    printf(" ");
    spaces--;
  }
}

static void print_string_value(struct string_value *val, const char *info, int spaces)
{
  print_tab(spaces);
  printf("%s", info);
  if (val)
    printf(": %s", val->value ? val->value : "<noval>");
  printf("\n");
}

#define PRINT_VALUE_BASIC(name_, type_) \
  static void print_##name_##_value (type_ *val, const char *info, int spaces) \
  { \
    print_tab(spaces); \
    printf("%s", info); \
    if (val) \
      printf(": %d", val->value); \
    printf("\n"); \
  }
PRINT_VALUE_BASIC(bool, struct boolean_value)
PRINT_VALUE_BASIC(int, struct integer_value)
PRINT_VALUE_BASIC(protection, struct protection_kind_value)
PRINT_VALUE_BASIC(basic_protection, struct basicprotection_kind_value)
#undef PRINT_VALUE_BASIC

static void print_domains(struct domains *domains, int spaces)
{
  print_tab(spaces);
  printf("domains {\n");
  if (domains)
  {
    struct domain_id_set *current = domains->domain_id_set;
    while (current != NULL)
    {
      if (current->max == NULL)
      {
        print_int_value(current->min, "id", spaces + 3);
      }
      else
      {
        print_int_value(current->min, "min", spaces + 3);
        print_int_value(current->max, "max", spaces + 3);
      }
      current = (struct domain_id_set *)current->node.next;
    }
  }
  else
  {
    printf(" {\n");
  }
  print_tab(spaces);
  printf("}\n");
}

static void print_topic_rule(struct topic_rule *rule, int spaces)
{
  print_tab(spaces);
  printf("topic_rule {\n");
  if (rule)
  {
    print_string_value(rule->topic_expression, "topic_expression", spaces + 3);
    print_bool_value(rule->enable_discovery_protection, "enable_discovery_protection", spaces + 3);
    print_bool_value(rule->enable_liveliness_protection, "enable_liveliness_protection", spaces + 3);
    print_bool_value(rule->enable_read_access_control, "enable_read_access_control", spaces + 3);
    print_bool_value(rule->enable_write_access_control, "enable_write_access_control", spaces + 3);
    print_protection_value(rule->metadata_protection_kind, "metadata_protection_kind", spaces + 3);
    print_basic_protection_value(rule->data_protection_kind, "data_protection_kind", spaces + 3);
  }
  else
  {
    printf(" {\n");
  }
  print_tab(spaces);
  printf("}\n");
}

static void print_topic_access_rules(struct topic_access_rules *tar, int spaces)
{
  print_tab(spaces);
  printf("topic_access_rules {\n");
  if (tar)
  {
    struct topic_rule *current = tar->topic_rule;
    while (current != NULL)
    {
      print_topic_rule(current, spaces + 3);
      current = (struct topic_rule *)current->node.next;
    }
  }
  else
  {
    printf(" {\n");
  }
  print_tab(spaces);
  printf("}\n");
}

static void print_domain_rule(struct domain_rule *rule, int spaces)
{
  print_tab(spaces);
  printf("domain_rule {\n");
  if (rule)
  {
    print_domains(rule->domains, spaces + 3);
    print_bool_value(rule->allow_unauthenticated_participants, "allow_unauthenticated_participants", spaces + 3);
    print_bool_value(rule->enable_join_access_control, "enable_join_access_control", spaces + 3);
    print_protection_value(rule->rtps_protection_kind, "rtps_protection_kind", spaces + 3);
    print_protection_value(rule->discovery_protection_kind, "discovery_protection_kind", spaces + 3);
    print_protection_value(rule->liveliness_protection_kind, "liveliness_protection_kind", spaces + 3);
    print_topic_access_rules(rule->topic_access_rules, spaces + 3);
  }
  else
  {
    printf(" {\n");
  }
  print_tab(spaces);
  printf("}\n");
}

static void print_domain_access_rules(struct domain_access_rules *dar, int spaces)
{
  print_tab(spaces);
  printf("domain_access_rules {\n");
  if (dar)
  {
    struct domain_rule *current = dar->domain_rule;
    while (current != NULL)
    {
      print_domain_rule(current, spaces + 3);
      current = (struct domain_rule *)current->node.next;
    }
  }
  else
  {
    printf(" {\n");
  }
  print_tab(spaces);
  printf("}\n");
}

static void print_governance_parser_result(struct governance_parser *parser)
{
  assert(parser);
  assert(parser->dds);
  assert(parser->dds->domain_access_rules);
  printf("-----------------------------------------------\n");
  print_domain_access_rules(parser->dds->domain_access_rules, 0);
  printf("-----------------------------------------------\n");
}

static void print_topic(struct string_value *topic, int spaces)
{
  if (topic)
  {
    print_string_value(topic, "topic", spaces);
    print_topic((struct string_value *)topic->node.next, spaces);
  }
}

static void print_topics(struct topics *topics, int spaces)
{
  if (topics)
  {
    print_tab(spaces);
    printf("topics {\n");
    print_topic(topics->topic, spaces + 3);
    print_tab(spaces);
    printf("}\n");
  }
}

static void print_partition(struct string_value *partition, int spaces)
{
  if (partition)
  {
    print_string_value(partition, "partition", spaces);
    print_partition((struct string_value *)partition->node.next, spaces);
  }
}

static void print_partitions(struct partitions *partitions, int spaces)
{
  if (partitions)
  {
    print_tab(spaces);
    printf("partitions {\n");
    print_partition(partitions->partition, spaces + 3);
    print_tab(spaces);
    printf("}\n");
  }
}

static void print_criteria(struct criteria *criteria, int spaces)
{
  if (criteria)
  {
    struct criteria *current = criteria;
    while (current != NULL)
    {
      print_tab(spaces);
      if (current->criteria_type == SUBSCRIBE_CRITERIA)
        printf("subscribe {\n");
      else if (current->criteria_type == PUBLISH_CRITERIA)
        printf("publish {\n");
      else
        assert(0);
      print_topics(current->topics, spaces + 3);
      print_partitions(current->partitions, spaces + 3);
      print_tab(spaces);
      printf("}\n");
      current = (struct criteria *)current->node.next;
    }
  }
}

static void print_allow_deny_rule(struct allow_deny_rule *allow_deny_rule, int spaces)
{
  if (allow_deny_rule)
  {
    struct allow_deny_rule *current = allow_deny_rule;
    while (current != NULL)
    {
      print_tab(spaces);
      if (current->rule_type == ALLOW_RULE)
        printf("allow_rule {\n");
      else if (current->rule_type == DENY_RULE)
        printf("deny_rule {\n");
      else
        assert(0);
      print_domains(current->domains, spaces + 3);
      print_criteria(current->criteria, spaces + 3);
      print_tab(spaces);
      printf("}\n");
      current = (struct allow_deny_rule *)current->node.next;
    }
  }
}

static void print_permissions(struct permissions *permissions, int spaces)
{
  struct grant *current = permissions->grant;
  print_tab(spaces);
  printf("permissions {\n");
  while (current != NULL)
  {
    print_tab(spaces + 3);
    printf("grant {\n");
    print_tab(spaces + 6);
    printf("name: %s\n", current->name);
    print_string_value(current->subject_name, "subject_name", spaces + 6);
    print_string_value(current->validity->not_before, "validity_not_before", spaces + 6);
    print_string_value(current->validity->not_after, "validity_not_after", spaces + 6);
    print_allow_deny_rule(current->allow_deny_rule, spaces + 6);
    print_string_value(current->default_action, "default", spaces + 6);
    current = (struct grant *)current->node.next;
    print_tab(spaces + 3);
    printf("}\n");
  }
  print_tab(spaces);
  printf("}\n");
}

static void print_permissions_parser_result(struct permissions_parser *parser)
{
  assert(parser);
  assert(parser->dds);
  assert(parser->dds->permissions);
  printf("-----------------------------------------------\n");
  print_permissions(parser->dds->permissions, 0);
  printf("-----------------------------------------------\n");
}

#endif /* DEBUG_PARSER */

static struct element *new_element(element_kind kind, struct element *parent, size_t size)
{
  struct element *e = ddsrt_malloc(size);
  memset(e, 0, size);
  e->parent = parent;
  e->kind = kind;
  e->next = NULL;
  return e;
}

#define PREPARE_NODE(element_type, element_kind, element_name, parent_type, parent_kind, current) \
  {                                                                                               \
    xml_##parent_type *P = (xml_##parent_type *)current;                                          \
    if (!current || current->kind != ELEMENT_KIND_##parent_kind)                                  \
    {                                                                                             \
      return -1;                                                                                  \
    }                                                                                             \
    current = new_element(ELEMENT_KIND_##element_kind, current, sizeof(xml_##element_type));      \
    P->element_name = (xml_##element_type *)current;                                              \
  }

#define PREPARE_NODE_WITH_LIST(element_type, element_kind, element_name, parent_type, parent_kind, current) \
  {                                                                                                         \
    xml_##parent_type *P = (xml_##parent_type *)current;                                                    \
    xml_element *tail;                                                                                      \
    if (!current || current->kind != ELEMENT_KIND_##parent_kind)                                            \
    {                                                                                                       \
      return -1;                                                                                            \
    }                                                                                                       \
    tail = (xml_element *)P->element_name;                                                                  \
    current = new_element(ELEMENT_KIND_##element_kind, current, sizeof(xml_##element_type));                \
    if (!P->element_name)                                                                                   \
    {                                                                                                       \
      P->element_name = (xml_##element_type *)current;                                                      \
    }                                                                                                       \
    else                                                                                                    \
    {                                                                                                       \
      while (tail->next != NULL)                                                                            \
      {                                                                                                     \
        tail = tail->next;                                                                                  \
      }                                                                                                     \
      tail->next = current;                                                                                 \
      tail->next->next = NULL;                                                                              \
    }                                                                                                       \
  }

static void validate_domains(const struct domain_id_set *domains_set, DDS_Security_SecurityException *ex)
{
  const struct domain_id_set *domain = domains_set;
  if (!domains_set)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found domain set in Governance file without domain ids.");
    return;
  }
  while (domain != NULL && ex->code == 0)
  {
    if (!domain->min)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found domain range in Governance file without minimum value.");
    else if (!domain->max)
      ; /* The max isn't set with only an id (no range), so no error. */
    else if (domain->max->value < domain->min->value)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found domain range in Governance file with invalid range min(%d) max(%d).", domain->min->value, domain->max->value);
    domain = (struct domain_id_set *)domain->node.next;
  }
}

static void validate_topic_rules(const struct topic_rule *topic_rule, DDS_Security_SecurityException *ex)
{
  while (topic_rule && ex->code == 0)
  {
    if (!topic_rule->data_protection_kind)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found topic rule in Governance file without data_protection_kind");
    else if (!topic_rule->enable_discovery_protection)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found topic rule in Governance file without enable_discovery_protection");
    else if (!topic_rule->enable_liveliness_protection)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found topic rule in Governance file without enable_liveliness_protection");
    else if (!topic_rule->enable_read_access_control)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found topic rule in Governance file without enable_read_access_control");
    else if (!topic_rule->enable_write_access_control)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found topic rule in Governance file without enable_write_access_control");
    else if (!topic_rule->metadata_protection_kind)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found topic rule in Governance file without metadata_protection_kind");
    else
      topic_rule = (struct topic_rule *)topic_rule->node.next;
  }
}

static DDS_Security_boolean validate_rules(const struct domain_rule *rule, DDS_Security_SecurityException *ex)
{
  while (rule && ex->code == 0)
  {
    if (!rule->domains)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without domain ids.");
    else if (!rule->allow_unauthenticated_participants)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without allow_unauthenticated_participants.");
    else if (!rule->enable_join_access_control)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without enable_join_access_control.");
    else if (!rule->rtps_protection_kind)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without rtps_protection_kind.");
    else if (!rule->discovery_protection_kind)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without discovery_protection_kind.");
    else if (!rule->liveliness_protection_kind)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without liveliness_protection_kind.");
    else
    {
      /* Last but not least, check the domain ids (ex is set when there's a failure) */
      validate_domains(rule->domains->domain_id_set, ex);
      if (!(rule->topic_access_rules && rule->topic_access_rules->topic_rule))
        DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Found rule in Governance file without topic_access_rules");
      else
      {
        validate_topic_rules(rule->topic_access_rules->topic_rule, ex);
        rule = (struct domain_rule *)rule->node.next;
      }
    }
  }
  return (ex->code == 0);
}

static int validate_permissions_tree(const struct grant *grant, DDS_Security_SecurityException *ex)
{
  while (grant && (ex->code == 0))
  {
    xml_allow_deny_rule *allow_deny_rule;
    if (!grant->subject_name || !grant->subject_name->value)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, "Found tree in Permissions file without subject name.");
    else if (!grant->validity)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, "Found tree in Permissions file without Validity.");
    else if (!grant->validity->not_after || !grant->validity->not_after->value)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, "Found tree in Permissions file without Validity/not_after.");
    else if (!grant->validity->not_before || !grant->validity->not_before->value)
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, "Found tree in Permissions file without Validity/not_before.");
    else
    {
      /*validate partitions*/
      allow_deny_rule = grant->allow_deny_rule;
      while (allow_deny_rule)
      {
        xml_criteria *criteria = allow_deny_rule->criteria;
        while (criteria)
        {
          /* set to default partition, if there is no partition specifien in the XML. (DDS Security SPEC 9.4.1.3.2.3.1.4)*/
          if (criteria->partitions == NULL)
          {
            xml_element *criteria_element = &(criteria->node);
            xml_element *partitions_element;
            PREPARE_NODE(partitions, PARTITIONS, partitions, criteria, CRITERIA, criteria_element)
            assert(criteria->partitions);
            partitions_element = &(criteria->partitions->node);
            PREPARE_NODE_WITH_LIST(string_value, STRING_VALUE, partition, partitions, PARTITIONS, partitions_element)
            assert(criteria->partitions->partition);
            criteria->partitions->partition->value = ddsrt_strdup("");
          }
          criteria = (xml_criteria *)criteria->node.next;
        }
        allow_deny_rule = (xml_allow_deny_rule *)allow_deny_rule->node.next;
      }
    }
    grant = (struct grant *)grant->node.next;
  }
  return (ex->code == 0);
}

static int to_protection_kind(const char *kindStr, DDS_Security_ProtectionKind *kindEnum)
{
  if (strcmp(kindStr, "ENCRYPT_WITH_ORIGIN_AUTHENTICATION") == 0)
    *kindEnum = DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION;
  else if (strcmp(kindStr, "SIGN_WITH_ORIGIN_AUTHENTICATION") == 0)
    *kindEnum = DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION;
  else if (strcmp(kindStr, "ENCRYPT") == 0)
    *kindEnum = DDS_SECURITY_PROTECTION_KIND_ENCRYPT;
  else if (strcmp(kindStr, "SIGN") == 0)
    *kindEnum = DDS_SECURITY_PROTECTION_KIND_SIGN;
  else if (strcmp(kindStr, "NONE") == 0)
    *kindEnum = DDS_SECURITY_PROTECTION_KIND_NONE;
  else
    return -1;
  return 0;
}

static int to_basic_protection_kind(const char *kindStr, DDS_Security_BasicProtectionKind *kindEnum)
{
  if (strcmp(kindStr, "ENCRYPT") == 0)
    *kindEnum = DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT;
  else if (strcmp(kindStr, "SIGN") == 0)
    *kindEnum = DDS_SECURITY_BASICPROTECTION_KIND_SIGN;
  else if (strcmp(kindStr, "NONE") == 0)
    *kindEnum = DDS_SECURITY_BASICPROTECTION_KIND_NONE;
  else
    return -1;
  return 0;
}

static int governance_element_open_cb(void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name, int line)
{
  governance_parser *parser = (governance_parser *)varg;
  DDS_Security_SecurityException ex;
  memset(&ex, 0, sizeof(DDS_Security_SecurityException));
  DDSRT_UNUSED_ARG(parentinfo);
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);
  if (ddsrt_strcasecmp(name, "dds") == 0)
  {
    /* This should be the first element. */
    if (parser->current || parser->dds)
      return -1;
    parser->current = new_element(ELEMENT_KIND_DDS, NULL, sizeof(struct governance_dds));
    parser->dds = (struct governance_dds *)parser->current;
  }
  else if (ddsrt_strcasecmp(name, "domain_access_rules") == 0)
    PREPARE_NODE(domain_access_rules, DOMAIN_ACCESS_RULES, domain_access_rules, governance_dds, DDS, parser->current)
  else if (ddsrt_strcasecmp(name, "domain_rule") == 0)
    PREPARE_NODE_WITH_LIST(domain_rule, DOMAIN_RULE, domain_rule, domain_access_rules, DOMAIN_ACCESS_RULES, parser->current)
  else if (ddsrt_strcasecmp(name, "domains") == 0)
    PREPARE_NODE(domains, DOMAINS, domains, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "id") == 0)
  {
    xml_domains *domains = (xml_domains *)parser->current;
    xml_domain_id_set *tail;
    if (!parser->current || parser->current->kind != ELEMENT_KIND_DOMAINS)
      return -1;
    tail = domains->domain_id_set;
    parser->current = new_element(ELEMENT_KIND_DOMAIN_VALUE, parser->current, sizeof(xml_integer_value));
    if (!tail)
    {
      domains->domain_id_set = (xml_domain_id_set *)new_element(ELEMENT_KIND_DOMAIN_ID_SET, parser->current, sizeof(xml_domain_id_set));
      tail = domains->domain_id_set;
    }
    else
    {
      while (tail->node.next != NULL)
        tail = (xml_domain_id_set *)tail->node.next;
      tail->node.next = new_element(ELEMENT_KIND_DOMAIN_ID_SET, parser->current, sizeof(xml_domain_id_set));
      tail = (xml_domain_id_set *)tail->node.next;
    }
    tail->min = (xml_integer_value *)parser->current;
    tail->max = NULL;
  }
  else if (ddsrt_strcasecmp(name, "id_range") == 0)
    PREPARE_NODE_WITH_LIST(domain_id_set, DOMAIN_ID_SET, domain_id_set, domains, DOMAINS, parser->current)
  else if (ddsrt_strcasecmp(name, "min") == 0)
    PREPARE_NODE(integer_value, DOMAIN_VALUE, min, domain_id_set, DOMAIN_ID_SET, parser->current)
  else if (ddsrt_strcasecmp(name, "max") == 0)
    PREPARE_NODE(integer_value, DOMAIN_VALUE, max, domain_id_set, DOMAIN_ID_SET, parser->current)
  else if (ddsrt_strcasecmp(name, "allow_unauthenticated_participants") == 0)
    PREPARE_NODE(boolean_value, BOOLEAN_VALUE, allow_unauthenticated_participants, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "enable_join_access_control") == 0)
    PREPARE_NODE(boolean_value, BOOLEAN_VALUE, enable_join_access_control, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "rtps_protection_kind") == 0)
    PREPARE_NODE(protection_kind_value, PROTECTION_KIND_VALUE, rtps_protection_kind, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "discovery_protection_kind") == 0)
    PREPARE_NODE(protection_kind_value, PROTECTION_KIND_VALUE, discovery_protection_kind, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "liveliness_protection_kind") == 0)
    PREPARE_NODE(protection_kind_value, PROTECTION_KIND_VALUE, liveliness_protection_kind, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "topic_access_rules") == 0)
    PREPARE_NODE(topic_access_rules, TOPIC_ACCESS_RULES, topic_access_rules, domain_rule, DOMAIN_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "topic_rule") == 0)
    PREPARE_NODE_WITH_LIST(topic_rule, TOPIC_RULE, topic_rule, topic_access_rules, TOPIC_ACCESS_RULES, parser->current)
  else if (ddsrt_strcasecmp(name, "enable_read_access_control") == 0)
    PREPARE_NODE(boolean_value, BOOLEAN_VALUE, enable_read_access_control, topic_rule, TOPIC_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "enable_write_access_control") == 0)
    PREPARE_NODE(boolean_value, BOOLEAN_VALUE, enable_write_access_control, topic_rule, TOPIC_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "metadata_protection_kind") == 0)
    PREPARE_NODE(protection_kind_value, PROTECTION_KIND_VALUE, metadata_protection_kind, topic_rule, TOPIC_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "data_protection_kind") == 0)
    PREPARE_NODE(basicprotection_kind_value, BASICPROTECTION_KIND_VALUE, data_protection_kind, topic_rule, TOPIC_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "enable_liveliness_protection") == 0)
    PREPARE_NODE(boolean_value, BOOLEAN_VALUE, enable_liveliness_protection, topic_rule, TOPIC_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "enable_discovery_protection") == 0)
    PREPARE_NODE(boolean_value, BOOLEAN_VALUE, enable_discovery_protection, topic_rule, TOPIC_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "topic_expression") == 0)
  {
    /* Current should be topic_rule. */
    struct topic_rule *topicRule = (struct topic_rule *)parser->current;
    if (!parser->current || parser->current->kind != ELEMENT_KIND_TOPIC_RULE)
      return -1;
    parser->current = new_element(ELEMENT_KIND_STRING_VALUE, parser->current, sizeof(struct string_value));
    topicRule->topic_expression = (struct string_value *)parser->current;
  }
  else
  {
    printf("Unknown XML element: %s\n", name);
    return -1;
  }

  return 0;
}

/* The function that is called on each attribute captured in XML.
 * Only the following attributes will be handled:
 * - name : the name of an element or attribute
 */
static int governance_element_attr_cb(void *varg, uintptr_t eleminfo, const char *name, const char *value, int line)
{
  /* There is no attribute in that XML */
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(varg);
  DDSRT_UNUSED_ARG(value);
  DDSRT_UNUSED_ARG(line);

  if (ddsrt_strcasecmp(name, "xmlns:xsi") == 0 || ddsrt_strcasecmp(name, "xsi:noNamespaceSchemaLocation") == 0)
    return 0;
  return -1;
}

static bool str_to_intvalue(const char *image, int32_t *value)
{
  char *endptr;
  long long l;
  if (ddsrt_strtoll(image, &endptr, 0, &l) != DDS_RETCODE_OK)
    return false;
  *value = (int32_t)l;
  if (*endptr != '\0')
    return false;
  return true;
}

/* The function that is called on each data item captured in XML.
 * - data: the string value between the element tags
 */
static int governance_element_data_cb(void *varg, uintptr_t eleminfo, const char *data, int line)
{
  struct governance_parser *parser = (struct governance_parser *)varg;
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);
  if (!parser || !parser->current)
    return -1;
  if (parser->current->kind == ELEMENT_KIND_STRING_VALUE)
  {
    struct string_value *value = (struct string_value *)parser->current;
    value->value = ddsrt_strdup(data);
  }
  else if (parser->current->kind == ELEMENT_KIND_DOMAIN_VALUE)
  {
    struct integer_value *value = (struct integer_value *)parser->current;
    if (str_to_intvalue(data, &value->value))
    {
      if (value->value < 0 || value->value > 230)
        return -1;
    }
    else
    {
      return -1;
    }
  }
  else if (parser->current->kind == ELEMENT_KIND_BOOLEAN_VALUE)
  {
    struct boolean_value *value = (struct boolean_value *)parser->current;
    if (ddsrt_strcasecmp("true", data) == 0 || strcmp("1", data) == 0)
      value->value = true;
    else if (ddsrt_strcasecmp("false", data) == 0 || strcmp("0", data) == 0)
      value->value = false;
    else
      return -1;
  }
  else if (parser->current->kind == ELEMENT_KIND_PROTECTION_KIND_VALUE)
  {
    struct protection_kind_value *value = (struct protection_kind_value *)parser->current;
    if (to_protection_kind(data, &(value->value)) != 0)
      return -1;
  }
  else if (parser->current->kind == ELEMENT_KIND_BASICPROTECTION_KIND_VALUE)
  {
    struct basicprotection_kind_value *value = (struct basicprotection_kind_value *)parser->current;
    if (to_basic_protection_kind(data, &(value->value)) != 0)
      return -1;
  }
  else
  {
    return -1;
  }

  return 0;
}

static int governance_element_close_cb(void *varg, uintptr_t eleminfo, int line)
{
  struct governance_parser *parser = (struct governance_parser *)varg;
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);
  if (!parser->current)
    return -1;
  parser->current = parser->current->parent;
  return 0;
}

static void governance_error_cb(void *varg, const char *msg, int line)
{
  DDSRT_UNUSED_ARG(varg);
  printf("Failed to parse configuration file: error %d - %s\n", line, msg);
}

static void free_stringvalue(struct string_value *str)
{
  if (str)
  {
    ddsrt_free(str->value);
    ddsrt_free(str);
  }
}

static void free_domainid_set(struct domain_id_set *dis)
{
  if (dis)
  {
    if (dis->node.next)
    {
      free_domainid_set((struct domain_id_set *)dis->node.next);
    }
    ddsrt_free(dis->min);
    ddsrt_free(dis->max);
    ddsrt_free(dis);
  }
}

static void free_domains(struct domains *domains)
{
  if (domains)
  {
    free_domainid_set(domains->domain_id_set);
    ddsrt_free(domains);
  }
}

static void free_topic_rule(struct topic_rule *rule)
{
  if (rule)
  {
    if (rule->node.next)
      free_topic_rule((struct topic_rule *)rule->node.next);
    free_stringvalue(rule->topic_expression);
    ddsrt_free(rule->enable_discovery_protection);
    ddsrt_free(rule->enable_liveliness_protection);
    ddsrt_free(rule->enable_read_access_control);
    ddsrt_free(rule->enable_write_access_control);
    ddsrt_free(rule->metadata_protection_kind);
    ddsrt_free(rule->data_protection_kind);
    ddsrt_free(rule);
  }
}

static void free_topic_access_rules(struct topic_access_rules *tar)
{
  if (tar)
  {
    struct topic_rule *current = tar->topic_rule;
    free_topic_rule(current);
  }
  ddsrt_free(tar);
}

static void free_domain_rule(struct domain_rule *rule)
{
  if (rule)
  {
    if (rule->node.next)
      free_domain_rule((struct domain_rule *)rule->node.next);
    free_domains(rule->domains);
    ddsrt_free(rule->allow_unauthenticated_participants);
    ddsrt_free(rule->enable_join_access_control);
    ddsrt_free(rule->rtps_protection_kind);
    ddsrt_free(rule->discovery_protection_kind);
    ddsrt_free(rule->liveliness_protection_kind);
    free_topic_access_rules(rule->topic_access_rules);
    ddsrt_free(rule);
  }
}

static void free_domain_access_rules(struct domain_access_rules *dar)
{
  if (dar)
  {
    free_domain_rule(dar->domain_rule);
    ddsrt_free(dar);
  }
}

bool ac_parse_governance_xml(const char *xml, struct governance_parser **governance_tree, DDS_Security_SecurityException *ex)
{
  struct governance_parser *parser = NULL;
  struct ddsrt_xmlp_state *st = NULL;
  if (xml)
  {
    struct ddsrt_xmlp_callbacks cb;
    cb.elem_open = governance_element_open_cb;
    cb.elem_data = governance_element_data_cb;
    cb.elem_close = governance_element_close_cb;
    cb.attr = governance_element_attr_cb;
    cb.error = governance_error_cb;
    parser = ddsrt_malloc(sizeof(struct governance_parser));
    parser->current = NULL;
    parser->dds = NULL;
    st = ddsrt_xmlp_new_string(xml, parser, &cb);
    if (ddsrt_xmlp_parse(st) != 0)
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_MESSAGE);
      goto err_xml_parsing;
    }
#if DEBUG_PARSER
    print_governance_parser_result(parser);
#endif
    if ((parser->dds != NULL) && (parser->dds->domain_access_rules != NULL) && (parser->dds->domain_access_rules->domain_rule != NULL))
    {
      if (!validate_rules(parser->dds->domain_access_rules->domain_rule, ex))
        goto err_rules_validation;
    }
    else
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_MESSAGE);
      goto err_parser_content;
    }
    *governance_tree = parser;
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_PARSE_GOVERNANCE_MESSAGE);
    goto err_xml;
  }
  ddsrt_xmlp_free(st);
  return true;

err_parser_content:
err_rules_validation:
err_xml_parsing:
  ddsrt_xmlp_free(st);
  ac_return_governance_tree(parser);
err_xml:
  return false;
}

void ac_return_governance_tree(struct governance_parser *parser)
{
  if (parser)
  {
    if (parser->dds)
    {
      free_domain_access_rules(parser->dds->domain_access_rules);
      ddsrt_free(parser->dds);
    }
    ddsrt_free(parser);
  }
}

/* Permissions Callback functions */

static int permissions_element_open_cb(void *varg, uintptr_t parentinfo, uintptr_t *eleminfo, const char *name, int line)
{
  permissions_parser *parser = (permissions_parser *)varg;
  DDS_Security_SecurityException ex;
  memset(&ex, 0, sizeof(DDS_Security_SecurityException));
  DDSRT_UNUSED_ARG(parentinfo);
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);

  /*it may be a valid element under an ignored element */
  if (parser->current && parser->current->kind == ELEMENT_KIND_IGNORED)
    parser->current = new_element(ELEMENT_KIND_IGNORED, parser->current, sizeof(struct element));
  else if (ddsrt_strcasecmp(name, "dds") == 0)
  {
    /* This should be the first element. */
    if (parser->current || parser->dds)
      return -1;
    parser->current = new_element(ELEMENT_KIND_DDS, NULL, sizeof(struct permissions_dds));
    parser->dds = (struct permissions_dds *)parser->current;
  }
  else if (ddsrt_strcasecmp(name, "permissions") == 0)
    PREPARE_NODE(permissions, PERMISSIONS, permissions, permissions_dds, DDS, parser->current)
  else if (ddsrt_strcasecmp(name, "grant") == 0)
    PREPARE_NODE_WITH_LIST(grant, GRANT, grant, permissions, PERMISSIONS, parser->current)
  else if (ddsrt_strcasecmp(name, "domains") == 0)
    PREPARE_NODE(domains, DOMAINS, domains, allow_deny_rule, ALLOW_DENY_RULE, parser->current)
  else if (ddsrt_strcasecmp(name, "id") == 0)
  {
    xml_domains *domains = (xml_domains *)parser->current;
    xml_domain_id_set *tail;
    if (!parser->current || parser->current->kind != ELEMENT_KIND_DOMAINS)
      return -1;
    tail = domains->domain_id_set;
    parser->current = new_element(ELEMENT_KIND_DOMAIN_VALUE, parser->current, sizeof(xml_integer_value));
    if (!tail)
    {
      domains->domain_id_set = (xml_domain_id_set *)new_element(ELEMENT_KIND_DOMAIN_ID_SET, parser->current, sizeof(xml_domain_id_set));
      tail = domains->domain_id_set;
    }
    else
    {
      while (tail->node.next != NULL)
        tail = (xml_domain_id_set *)tail->node.next;
      tail->node.next = new_element(ELEMENT_KIND_DOMAIN_ID_SET, parser->current, sizeof(xml_domain_id_set));
      tail = (xml_domain_id_set *)tail->node.next;
    }
    tail->min = (xml_integer_value *)parser->current;
    tail->max = NULL;
  }
  else if (ddsrt_strcasecmp(name, "id_range") == 0)
    PREPARE_NODE_WITH_LIST(domain_id_set, DOMAIN_ID_SET, domain_id_set, domains, DOMAINS, parser->current)
  else if (ddsrt_strcasecmp(name, "min") == 0)
    PREPARE_NODE(integer_value, DOMAIN_VALUE, min, domain_id_set, DOMAIN_ID_SET, parser->current)
  else if (ddsrt_strcasecmp(name, "max") == 0)
    PREPARE_NODE(integer_value, DOMAIN_VALUE, max, domain_id_set, DOMAIN_ID_SET, parser->current)
  else if (ddsrt_strcasecmp(name, "subject_name") == 0)
    PREPARE_NODE(string_value, STRING_VALUE, subject_name, grant, GRANT, parser->current)
  else if (ddsrt_strcasecmp(name, "validity") == 0)
    PREPARE_NODE(validity, VALIDITY, validity, grant, GRANT, parser->current)
  else if (ddsrt_strcasecmp(name, "not_before") == 0)
    PREPARE_NODE(string_value, STRING_VALUE, not_before, validity, VALIDITY, parser->current)
  else if (ddsrt_strcasecmp(name, "not_after") == 0)
    PREPARE_NODE(string_value, STRING_VALUE, not_after, validity, VALIDITY, parser->current)
  else if (ddsrt_strcasecmp(name, "allow_rule") == 0)
  {
    PREPARE_NODE_WITH_LIST(allow_deny_rule, ALLOW_DENY_RULE, allow_deny_rule, grant, GRANT, parser->current)
    ((xml_allow_deny_rule *)parser->current)->rule_type = ALLOW_RULE;
  }
  else if (ddsrt_strcasecmp(name, "deny_rule") == 0)
  {
    PREPARE_NODE_WITH_LIST(allow_deny_rule, ALLOW_DENY_RULE, allow_deny_rule, grant, GRANT, parser->current)
    ((xml_allow_deny_rule *)parser->current)->rule_type = DENY_RULE;
  }
  else if (ddsrt_strcasecmp(name, "subscribe") == 0)
  {
    PREPARE_NODE_WITH_LIST(criteria, CRITERIA, criteria, allow_deny_rule, ALLOW_DENY_RULE, parser->current)
    ((xml_criteria *)parser->current)->criteria_type = SUBSCRIBE_CRITERIA;
  }
  else if (ddsrt_strcasecmp(name, "publish") == 0)
  {
    PREPARE_NODE_WITH_LIST(criteria, CRITERIA, criteria, allow_deny_rule, ALLOW_DENY_RULE, parser->current)
    ((xml_criteria *)parser->current)->criteria_type = PUBLISH_CRITERIA;
  }
  else if (ddsrt_strcasecmp(name, "topics") == 0)
    PREPARE_NODE(topics, TOPICS, topics, criteria, CRITERIA, parser->current)
  else if (ddsrt_strcasecmp(name, "topic") == 0)
    PREPARE_NODE_WITH_LIST(string_value, STRING_VALUE, topic, topics, TOPICS, parser->current)
  else if (ddsrt_strcasecmp(name, "partitions") == 0)
    PREPARE_NODE(partitions, PARTITIONS, partitions, criteria, CRITERIA, parser->current)
  else if (ddsrt_strcasecmp(name, "partition") == 0)
    PREPARE_NODE_WITH_LIST(string_value, STRING_VALUE, partition, partitions, PARTITIONS, parser->current)
  else if (ddsrt_strcasecmp(name, "default") == 0)
    PREPARE_NODE(string_value, STRING_VALUE, default_action, grant, GRANT, parser->current)
  else if (ddsrt_strcasecmp(name, "relay") == 0 ||
           ddsrt_strcasecmp(name, "value") == 0 ||
           ddsrt_strcasecmp(name, "name") == 0 ||
           ddsrt_strcasecmp(name, "tag") == 0 ||
           ddsrt_strcasecmp(name, "data_tags") == 0)
  {
    parser->current = new_element(ELEMENT_KIND_IGNORED, parser->current, sizeof(struct element));
    /*if this is the first element in the IGNORED branch, then give warning for the user*/
#if 0
    if (parser->current->parent->kind != ELEMENT_KIND_IGNORED)
      printf("Warning: Unsupported element \"%s\" has been ignored in permissions file.\n", name);
#endif
  }
  else
  {
    printf("Unknown XML element: %s\n", name);
    return -1;
  }

  return 0;
}

/* The function that is called on each attribute captured in XML.
 * Only the following attributes will be handled:
 * - name : the name of an element or attribute
 */
static int permissions_element_attr_cb(void *varg, uintptr_t eleminfo, const char *name, const char *value, int line)
{
  struct permissions_parser *parser = (struct permissions_parser *)varg;
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);
  if (ddsrt_strcasecmp(name, "xmlns:xsi") == 0 || ddsrt_strcasecmp(name, "xsi:noNamespaceSchemaLocation") == 0)
    return 0;
  if (strcmp(name, "name") == 0)
  {
    /* Parent should be grants. */
    struct grant *grant = (struct grant *)parser->current;
    if (!parser->current || parser->current->kind != ELEMENT_KIND_GRANT)
      return -1;
    grant->name = ddsrt_strdup(value);
    return 0;
  }
  return -1;
}

/* The function that is called on each data item captured in XML.
 * - data: the string value between the element tags */
static int permissions_element_data_cb(void *varg, uintptr_t eleminfo, const char *data, int line)
{
  struct permissions_parser *parser = (struct permissions_parser *)varg;
  DDS_Security_SecurityException ex;
  memset(&ex, 0, sizeof(DDS_Security_SecurityException));
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);
  if (!parser || !parser->current)
    return -1;
  if (parser->current->kind == ELEMENT_KIND_STRING_VALUE)
  {
    struct string_value *value = (struct string_value *)parser->current;
    value->value = ddsrt_strdup(data);
  }
  else if (parser->current->kind == ELEMENT_KIND_DOMAIN_VALUE)
  {
    struct integer_value *value = (struct integer_value *)parser->current;
    if (str_to_intvalue(data, &value->value))
    {
      if (value->value < 0 || value->value > 230)
        return -1;
    }
    else
      return -1;
  }
  else
  {
    if (parser->current->kind != ELEMENT_KIND_IGNORED)
      return -1;
  }
  return 0;
}

static int permissions_element_close_cb(void *varg, uintptr_t eleminfo, int line)
{
  struct permissions_parser *parser = (struct permissions_parser *)varg;
  struct element *parent;
  DDSRT_UNUSED_ARG(eleminfo);
  DDSRT_UNUSED_ARG(line);

  if (!parser->current)
    return -1;
  parent = parser->current->parent;
  if (parser->current->kind == ELEMENT_KIND_IGNORED)
    ddsrt_free(parser->current);
  parser->current = parent;
  return 0;
}

static void permissions_error_cb(void *varg, const char *msg, int line)
{
  DDSRT_UNUSED_ARG(varg);
  printf("Failed to parse configuration file: error %d - %s\n", line, msg);
}

bool ac_parse_permissions_xml(const char *xml, struct permissions_parser **permissions_tree, DDS_Security_SecurityException *ex)
{
  struct permissions_parser *parser = NULL;
  struct ddsrt_xmlp_state *st = NULL;

  if (xml)
  {
    struct ddsrt_xmlp_callbacks cb;
    cb.elem_open = permissions_element_open_cb;
    cb.elem_data = permissions_element_data_cb;
    cb.elem_close = permissions_element_close_cb;
    cb.attr = permissions_element_attr_cb;
    cb.error = permissions_error_cb;
    parser = ddsrt_malloc(sizeof(struct permissions_parser));
    parser->current = NULL;
    parser->dds = NULL;
    st = ddsrt_xmlp_new_string(xml, parser, &cb);
    if (ddsrt_xmlp_parse(st) != 0)
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_MESSAGE);
      goto err_xml_parsing;
    }
#if DEBUG_PARSER
    print_permissions_parser_result(parser);
#endif
    if ((parser->dds != NULL) && (parser->dds->permissions != NULL) && (parser->dds->permissions->grant != NULL))
    {
      if (!validate_permissions_tree(parser->dds->permissions->grant, ex))
        goto err_parser_content;
    }
    else
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_MESSAGE);
      goto err_parser_content;
    }
    *permissions_tree = parser;
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_PARSE_PERMISSIONS_MESSAGE);
    goto err_xml;
  }
  ddsrt_xmlp_free(st);
  return true;

err_parser_content:
err_xml_parsing:
  ddsrt_xmlp_free(st);
  ac_return_permissions_tree(parser);
err_xml:
  return false;
}

static void free_topic(struct string_value *topic)
{
  if (topic)
  {
    if (topic->node.next != NULL)
      free_topic((struct string_value *)topic->node.next);
    free_stringvalue(topic);
  }
}

static void free_topics(struct topics *topics)
{
  if (topics)
  {
    free_topic(topics->topic);
    ddsrt_free(topics);
  }
}

static void free_partition(struct string_value *partition)
{
  if (partition)
  {
    if (partition->node.next != NULL)
      free_partition((struct string_value *)partition->node.next);
    free_stringvalue(partition);
  }
}

static void free_partitions(struct partitions *partitions)
{
  if (partitions)
  {
    free_partition(partitions->partition);
    ddsrt_free(partitions);
  }
}

static void free_validity(struct validity *validity)
{
  if (validity)
  {
    free_stringvalue(validity->not_after);
    free_stringvalue(validity->not_before);
    ddsrt_free(validity);
  }
}

static void free_criteria(struct criteria *criteria)
{
  if (criteria)
  {
    if (criteria->node.next)
      free_criteria((struct criteria *)criteria->node.next);
    free_partitions(criteria->partitions);
    free_topics(criteria->topics);
    ddsrt_free(criteria);
  }
}

static void free_allow_deny_rule(struct allow_deny_rule *rule)
{
  if (rule)
  {
    free_allow_deny_rule((struct allow_deny_rule *)rule->node.next);
    free_domains(rule->domains);
    free_criteria(rule->criteria);
    ddsrt_free(rule);
  }
}

static void free_grant(struct grant *grant)
{
  if (grant)
  {
    if (grant->node.next)
      free_grant((struct grant *)grant->node.next);
    ddsrt_free(grant->name);
    free_stringvalue(grant->subject_name);
    free_stringvalue(grant->default_action);
    free_validity(grant->validity);
    free_allow_deny_rule(grant->allow_deny_rule);
    ddsrt_free(grant);
  }
}

static void free_permissions(struct permissions *permissions)
{
  if (permissions)
  {
    free_grant(permissions->grant);
    ddsrt_free(permissions);
  }
}

void ac_return_permissions_tree(struct permissions_parser *parser)
{
  if (parser)
  {
    if (parser->dds)
    {
      free_permissions(parser->dds->permissions);
      ddsrt_free(parser->dds);
    }
    ddsrt_free(parser);
  }
}
