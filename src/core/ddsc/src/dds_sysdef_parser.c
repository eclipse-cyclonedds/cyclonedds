// Copyright(c) 2024 ZettaScale Technology and others
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
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/xmlparser.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"

#include "dds__sysdef_model.h"
#include "dds__sysdef_parser.h"

#define _STR(s) #s
#define STR(s) _STR(s)

#define PP ELEMENT_KIND_QOS_PARTICIPANT
#define SUB ELEMENT_KIND_QOS_SUBSCRIBER
#define PUB ELEMENT_KIND_QOS_PUBLISHER
#define TP ELEMENT_KIND_QOS_TOPIC
#define WR ELEMENT_KIND_QOS_WRITER
#define RD ELEMENT_KIND_QOS_READER
#define QOS_POLICY_MAPPING(p,...) \
    static const enum element_kind qos_policy_mapping_ ## p[] = { __VA_ARGS__ };

  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_DEADLINE, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_DURABILITY, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY, SUB, PUB, PP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_GROUPDATA, SUB, PUB)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_HISTORY, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_LIFESPAN, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_LIVELINESS, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_OWNERSHIP, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH, WR)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_PARTITION, SUB, PUB)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_PRESENTATION, SUB, PUB)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE, RD)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_RELIABILITY, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, RD, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER, RD)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_TOPICDATA, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY, WR, TP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_USERDATA, RD, WR, PP)
  QOS_POLICY_MAPPING (ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE, WR)

#undef PP
#undef SUB
#undef PUB
#undef TP
#undef WR
#undef RD

#define MAX_ERRMSG_SZ 300

#define NO_INIT NULL
#define NO_FINI NULL

#define PARSER_ERROR(pstate,line,...) \
    do { \
      (void) snprintf ((pstate)->err_msg, MAX_ERRMSG_SZ, __VA_ARGS__); \
      (pstate)->err_line = (line); \
      (pstate)->has_err = true; \
    } while (0)

#define CHECK_PARENT_NULL(pstate, current) \
    do { if ((current) == NULL) { \
      PARSER_ERROR (pstate, line, "Current element NULL for element '%s'", name); \
      return SD_PARSE_RESULT_SYNTAX_ERR; \
    } } while (0)

#define CHECK_PARENT_KIND(pstate, parent_kind, current) \
    do { if ((current)->kind != parent_kind) { \
      PARSER_ERROR (pstate, line, "Invalid parent kind (%d) for element '%s'", (current)->kind, name); \
      return SD_PARSE_RESULT_SYNTAX_ERR; \
    } } while (0)

#define _CREATE_NODE(pstate, element_type, element_kind, element_data_type, parent_kind, current, element_init, element_fini) \
  { \
    CHECK_PARENT_KIND (pstate, (parent_kind), current); \
    if (((current) = new_node (pstate, element_kind, element_data_type, current, sizeof (struct element_type), element_init, element_fini)) == NULL) \
      return SD_PARSE_RESULT_ERR; \
  }

#define CREATE_NODE_LIST(pstate, element_type, element_kind, element_init, element_fini, element_name, parent_type, parent_kind, current) \
  do { \
    struct parent_type *parent = (struct parent_type *) current; \
    _CREATE_NODE(pstate, element_type, element_kind, ELEMENT_DATA_TYPE_GENERIC, parent_kind, current, element_init, element_fini); \
    if (parent->element_name == NULL) { \
      parent->element_name = (struct element_type *) current; \
    } else { \
      struct xml_element *tail = (struct xml_element *) parent->element_name; \
      while (tail->next != NULL) { \
        tail = tail->next; \
      } \
      tail->next = current; \
    } \
    goto status_ok; \
  } while (0)

#define CREATE_NODE_SINGLE(pstate, element_type, element_kind, element_init, element_fini, element_name, parent_type, parent_kind, current) \
  do { \
    struct parent_type *parent = (struct parent_type *) current; \
    if (parent->element_name != NULL) { \
      PARSER_ERROR (pstate, line, "Duplicate element '%s'", name); \
      return SD_PARSE_RESULT_SYNTAX_ERR; \
    } \
    _CREATE_NODE(pstate, element_type, element_kind, ELEMENT_DATA_TYPE_GENERIC, parent_kind, current, element_init, element_fini); \
    parent->element_name = (struct element_type *) current; \
    goto status_ok; \
  } while (0)

#define CREATE_NODE_CUSTOM(pstate, element_type, element_kind, element_init, element_fini, parent_kind, current) \
  do { \
    _CREATE_NODE(pstate, element_type, element_kind, ELEMENT_DATA_TYPE_GENERIC, parent_kind, current, element_init, element_fini); \
    current->retain = false; \
    goto status_ok; \
  } while (0)

#define CREATE_NODE_DURATION(pstate, element_type, element_kind, element_init, element_fini, parent_kind, current) \
  do { \
    _CREATE_NODE(pstate, element_type, element_kind, ELEMENT_DATA_TYPE_DURATION, parent_kind, current, element_init, element_fini); \
    current->retain = false; \
    current->handle_close = true; \
    goto status_ok; \
  } while (0)

#define CREATE_NODE_DURATION_SEC(pstate, current) \
  do { \
    _CREATE_NODE(pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_DURATION_SEC, ELEMENT_DATA_TYPE_GENERIC, current->kind, current, NO_INIT, NO_FINI); \
    current->retain = false; \
    goto status_ok; \
  } while (0)

#define CREATE_NODE_DURATION_NSEC(pstate, current) \
  do { \
    _CREATE_NODE(pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_DURATION_NSEC, ELEMENT_DATA_TYPE_GENERIC, current->kind, current, NO_INIT, NO_FINI); \
    current->retain = false; \
    goto status_ok; \
  } while (0)

#define CREATE_NODE_QOS(pstate, element_type, element_kind, element_init, element_fini, current) \
  do { \
    bool allowed = false; \
    for (uint32_t n = 0; !allowed && n < sizeof (qos_policy_mapping_ ## element_kind) / sizeof (qos_policy_mapping_ ## element_kind[0]); n++) { \
      allowed = (current)->kind == qos_policy_mapping_ ## element_kind[n]; \
    } \
    if (!allowed) { \
      PARSER_ERROR (pstate, line, "Invalid parent kind (%d) for element '%s'", (current)->kind, name); \
      return SD_PARSE_RESULT_SYNTAX_ERR; \
    } \
    if (((current) = new_node (pstate, element_kind, ELEMENT_DATA_TYPE_GENERIC, current, sizeof (struct element_type), element_init, element_fini)) == NULL) \
      return SD_PARSE_RESULT_ERR;\
    current->retain = false; \
    current->handle_close = true; \
    goto status_ok; \
  } while (0)

struct parse_sysdef_state {
  struct dds_sysdef_system *sysdef;
  struct xml_element *current;
  uint32_t scope;
  bool has_err;
  int err_line;
  char err_msg[MAX_ERRMSG_SZ];
};

static bool str_to_int32 (const char *str, int32_t *value)
{
  char *endptr;
  long long l;
  if (ddsrt_strtoll (str, &endptr, 0, &l) != DDS_RETCODE_OK || l < INT32_MIN || l > INT32_MAX) {
    return false;
  }
  *value = (int32_t) l;
  return (*endptr == '\0');
}

static bool str_to_uint8 (const char *str, uint8_t *value)
{
  char *endptr;
  long long l;
  if (ddsrt_strtoll (str, &endptr, 0, &l) != DDS_RETCODE_OK || l < 0 || l > UINT8_MAX) {
    return false;
  }
  *value = (uint8_t) l;
  return (*endptr == '\0');
}

static bool str_to_uint16 (const char *str, uint16_t *value)
{
  char *endptr;
  long long l;
  if (ddsrt_strtoll (str, &endptr, 0, &l) != DDS_RETCODE_OK || l < 0 || l > UINT16_MAX) {
    return false;
  }
  *value = (uint16_t) l;
  return (*endptr == '\0');
}

static bool str_to_uint32 (const char *str, uint32_t *value)
{
  char *endptr;
  long long l;
  if (ddsrt_strtoll (str, &endptr, 0, &l) != DDS_RETCODE_OK || l < 0 || l > UINT32_MAX) {
    return false;
  }
  *value = (uint32_t) l;
  return (*endptr == '\0');
}

static bool str_to_int64 (const char *str, int64_t *value)
{
  char *endptr;
  long long l;
  if (ddsrt_strtoll (str, &endptr, 0, &l) != DDS_RETCODE_OK || l < INT64_MIN || l > INT64_MAX) {
    return false;
  }
  *value = (int64_t) l;
  return (*endptr == '\0');
}

static bool str_to_bool (const char *str, bool *value)
{
  if (strcmp (str, "true") == 0 || strcmp (str, "1") == 0)
    *value = true;
  else if (strcmp (str, "false") == 0 || strcmp (str, "0") == 0)
    *value = false;
  else
    return false;

  return true;
}

static struct xml_element *new_node (struct parse_sysdef_state * const pstate, enum element_kind kind, enum element_data_type data_type, struct xml_element *parent, size_t size, init_fn init, fini_fn fini)
{
  struct xml_element *e = ddsrt_malloc (size);
  if (e == NULL) {
    PARSER_ERROR (pstate, 0, "Out of memory");
    return NULL;
  }
  memset (e, 0, size);
  e->parent = parent;
  e->kind = kind;
  e->data_type = data_type;
  e->retain = true;
  e->handle_close = false;
  e->next = NULL;
  e->fini = fini;
  if (init) {
    if (init (pstate, e) != 0) {
      return NULL;
    }
  }
  return e;
}

static void free_node (void *node)
{
  struct xml_element *element = (struct xml_element *) node;
  while (element != NULL)
  {
    struct xml_element *tmp = element;
    element = tmp->next;
    if (tmp->fini)
      tmp->fini (tmp);
    ddsrt_free (tmp);
  }
}

static void fini_type (struct xml_element *node)
{
  struct dds_sysdef_type *type = (struct dds_sysdef_type *) node;
  ddsrt_free (type->name);
  ddsrt_free (type->identifier);
}

static void fini_type_lib (struct xml_element *node)
{
  struct dds_sysdef_type_lib *type_lib = (struct dds_sysdef_type_lib *) node;
  free_node (type_lib->types);
}

static void fini_qos_groupdata (struct xml_element *node)
{
  struct dds_sysdef_QOS_POLICY_GROUPDATA *qp = (struct dds_sysdef_QOS_POLICY_GROUPDATA *) node;
  assert (qp != NULL);
  ddsrt_free (qp->values.value);
}

static void fini_qos_topicdata (struct xml_element *node)
{
  struct dds_sysdef_QOS_POLICY_TOPICDATA *qp = (struct dds_sysdef_QOS_POLICY_TOPICDATA *) node;
  assert (qp != NULL);
  ddsrt_free (qp->values.value);
}

static void fini_qos_userdata (struct xml_element *node)
{
  struct dds_sysdef_QOS_POLICY_USERDATA *qp = (struct dds_sysdef_QOS_POLICY_USERDATA *) node;
  assert (qp != NULL);
  ddsrt_free (qp->values.value);
}

static void fini_qos_partition (struct xml_element *node)
{
  struct dds_sysdef_QOS_POLICY_PARTITION *qp = (struct dds_sysdef_QOS_POLICY_PARTITION *) node;
  assert (qp != NULL);
  free_node (qp->name);
}

static void fini_qos_partition_name (struct xml_element *node)
{
  struct dds_sysdef_QOS_POLICY_PARTITION_NAME *p = (struct dds_sysdef_QOS_POLICY_PARTITION_NAME *) node;
  assert (p != NULL);
  free_node (p->elements);
}

static void fini_qos_partition_name_element (struct xml_element *node)
{
  struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *p = (struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *) node;
  assert (p != NULL);
  ddsrt_free (p->element);
}

static void fini_qos (struct xml_element *node)
{
  struct dds_sysdef_qos *qos = (struct dds_sysdef_qos *) node;
  assert (qos != NULL);
  dds_delete_qos (qos->qos);
  ddsrt_free (qos->name);
}

static void fini_qos_profile (struct xml_element *node)
{
  struct dds_sysdef_qos_profile *qos_profile = (struct dds_sysdef_qos_profile *) node;
  free_node (qos_profile->qos);
  ddsrt_free (qos_profile->name);
}

static void fini_qos_lib (struct xml_element *node)
{
  struct dds_sysdef_qos_lib *qos_lib = (struct dds_sysdef_qos_lib *) node;
  free_node (qos_lib->qos_profiles);
  ddsrt_free (qos_lib->name);
}

static void fini_register_type (struct xml_element *node)
{
  struct dds_sysdef_register_type *reg_type = (struct dds_sysdef_register_type *) node;
  ddsrt_free (reg_type->name);
}

static void fini_topic (struct xml_element *node)
{
  struct dds_sysdef_topic *topic = (struct dds_sysdef_topic *) node;
  free_node (topic->qos);
  ddsrt_free (topic->name);
}

static void fini_domain (struct xml_element *node)
{
  struct dds_sysdef_domain *domain = (struct dds_sysdef_domain *) node;
  free_node (domain->register_types);
  free_node (domain->topics);
  ddsrt_free (domain->name);
}

static void fini_domain_lib (struct xml_element *node)
{
  struct dds_sysdef_domain_lib *domain_lib = (struct dds_sysdef_domain_lib *) node;
  free_node (domain_lib->domains);
  ddsrt_free (domain_lib->name);
}

static void fini_publisher (struct xml_element *node)
{
  struct dds_sysdef_publisher *publisher = (struct dds_sysdef_publisher *) node;
  free_node (publisher->writers);
  free_node (publisher->qos);
  ddsrt_free (publisher->name);
}

static void fini_writer (struct xml_element *node)
{
  struct dds_sysdef_writer *writer = (struct dds_sysdef_writer *) node;
  free_node (writer->qos);
  ddsrt_free (writer->name);
}

static void fini_subscriber (struct xml_element *node)
{
  struct dds_sysdef_subscriber *subscriber = (struct dds_sysdef_subscriber *) node;
  free_node (subscriber->readers);
  free_node (subscriber->qos);
  ddsrt_free (subscriber->name);
}

static void fini_reader (struct xml_element *node)
{
  struct dds_sysdef_reader *reader = (struct dds_sysdef_reader *) node;
  free_node (reader->qos);
  ddsrt_free (reader->name);
}

static void fini_participant (struct xml_element *node)
{
  struct dds_sysdef_participant *participant = (struct dds_sysdef_participant *) node;
  free_node (participant->publishers);
  free_node (participant->subscribers);
  free_node (participant->topics);
  free_node (participant->qos);
  free_node (participant->register_types);
  ddsrt_free (participant->name);
  ddsrt_free (participant->guid_prefix);
}

static void fini_participant_lib (struct xml_element *node)
{
  struct dds_sysdef_participant_lib *participant_lib = (struct dds_sysdef_participant_lib *) node;
  free_node (participant_lib->participants);
  ddsrt_free (participant_lib->name);
}

static void fini_application (struct xml_element *node)
{
  struct dds_sysdef_application *application = (struct dds_sysdef_application *) node;
  free_node (application->participants);
  ddsrt_free (application->name);
}

static void fini_application_lib (struct xml_element *node)
{
  struct dds_sysdef_application_lib *application_lib = (struct dds_sysdef_application_lib *) node;
  free_node (application_lib->applications);
  ddsrt_free (application_lib->name);
}

static void fini_node (struct xml_element *node)
{
  struct dds_sysdef_node *n = (struct dds_sysdef_node *) node;
  ddsrt_free (n->name);
  ddsrt_free (n->hostname);
  ddsrt_free (n->ipv4_addr);
  ddsrt_free (n->ipv6_addr);
  ddsrt_free (n->mac_addr);
}

static void fini_node_lib (struct xml_element *node)
{
  struct dds_sysdef_node_lib *node_lib = (struct dds_sysdef_node_lib *) node;
  free_node (node_lib->nodes);
  ddsrt_free (node_lib->name);
}

static void fini_conf (struct xml_element *node)
{
  struct dds_sysdef_configuration *conf = (struct dds_sysdef_configuration *) node;
  free_node (conf->tsn_configuration);
}

static void fini_conf_tsn (struct xml_element *node)
{
  struct dds_sysdef_tsn_configuration *conf = (struct dds_sysdef_tsn_configuration *) node;
  free_node (conf->tsn_talker_configurations);
  free_node (conf->tsn_listener_configurations);
}

static void fini_conf_tsn_talker (struct xml_element *node)
{
  struct dds_sysdef_tsn_talker_configuration *conf = (struct dds_sysdef_tsn_talker_configuration *) node;
  free_node (conf->data_frame_specification);
  free_node (conf->network_requirements);
  free_node (conf->traffic_specification);
  ddsrt_free (conf->stream_name);
  ddsrt_free (conf->name);
}

static void fini_conf_tsn_traffic_specification (struct xml_element *node)
{
  struct dds_sysdef_tsn_traffic_specification *conf = (struct dds_sysdef_tsn_traffic_specification *) node;
  free_node (conf->time_aware);
}

static void fini_conf_tsn_ip_tuple (struct xml_element *node)
{
  struct dds_sysdef_tsn_ip_tuple *conf = (struct dds_sysdef_tsn_ip_tuple *) node;
  ddsrt_free (conf->destination_ip_address);
  ddsrt_free (conf->source_ip_address);
}

static void fini_conf_tsn_data_frame_specification (struct xml_element *node)
{
  struct dds_sysdef_tsn_data_frame_specification *conf = (struct dds_sysdef_tsn_data_frame_specification *) node;
  free_node (conf->ipv4_tuple);
  free_node (conf->ipv6_tuple);
  free_node (conf->mac_addresses);
  free_node (conf->vlan_tag);
}

static void fini_conf_tsn_listener (struct xml_element *node)
{
  struct dds_sysdef_tsn_listener_configuration *conf = (struct dds_sysdef_tsn_listener_configuration *) node;
  free_node (conf->network_requirements);
  ddsrt_free (conf->stream_name);
  ddsrt_free (conf->name);
}

static void fini_application_list (struct xml_element *node)
{
  struct dds_sysdef_application_list *application_list = (struct dds_sysdef_application_list *) node;
  free_node (application_list->application_refs);
}

static void fini_deployment (struct xml_element *node)
{
  struct dds_sysdef_deployment *deployment = (struct dds_sysdef_deployment *) node;
  free_node (deployment->application_list);
  free_node (deployment->configuration);
  ddsrt_free (deployment->name);
}

static void fini_deployment_lib (struct xml_element *node)
{
  struct dds_sysdef_deployment_lib *deployment_lib = (struct dds_sysdef_deployment_lib *) node;
  free_node (deployment_lib->deployments);
  ddsrt_free (deployment_lib->name);
}

static void fini_sysdef (struct xml_element *node)
{
  struct dds_sysdef_system *sysdef = (struct dds_sysdef_system *) node;
  free_node (sysdef->type_libs);
  free_node (sysdef->qos_libs);
  free_node (sysdef->domain_libs);
  free_node (sysdef->participant_libs);
  free_node (sysdef->application_libs);
  free_node (sysdef->node_libs);
  free_node (sysdef->deployment_libs);
}

static int init_qos (UNUSED_ARG (struct parse_sysdef_state * const pstate), struct xml_element *node)
{
  struct dds_sysdef_qos *sdqos = (struct dds_sysdef_qos *) node;
  sdqos->qos = dds_create_qos ();
  enum dds_sysdef_qos_kind qos_kind;
  switch (node->kind) {
    case ELEMENT_KIND_QOS_PARTICIPANT:
      qos_kind = DDS_SYSDEF_PARTICIPANT_QOS;
      break;
    case ELEMENT_KIND_QOS_TOPIC:
      qos_kind = DDS_SYSDEF_TOPIC_QOS;
      break;
    case ELEMENT_KIND_QOS_PUBLISHER:
      qos_kind = DDS_SYSDEF_PUBLISHER_QOS;
      break;
    case ELEMENT_KIND_QOS_SUBSCRIBER:
      qos_kind = DDS_SYSDEF_SUBSCRIBER_QOS;
      break;
    case ELEMENT_KIND_QOS_READER:
      qos_kind = DDS_SYSDEF_READER_QOS;
      break;
    case ELEMENT_KIND_QOS_WRITER:
      qos_kind = DDS_SYSDEF_WRITER_QOS;
      break;
    default:
      return SD_PARSE_RESULT_SYNTAX_ERR;
  }
  sdqos->kind = qos_kind;
  return 0;
}

#define PROC_ATTR_STRING(type,attr_name,param_field,validator_fn) \
    do { \
      if (ddsrt_strcasecmp (name, attr_name) == 0) \
      { \
        if (!validator_fn (value)) { \
          PARSER_ERROR (pstate, line, "Invalid identifier '%s'", value); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } else { \
          struct type *t = (struct type *) pstate->current; \
          if (t->param_field == NULL) { \
            assert (!attr_processed); \
            t->param_field = ddsrt_strdup (value); \
            attr_processed = true; \
          } else { \
            PARSER_ERROR (pstate, line, "Duplicate attribute '%s'", attr_name); \
            ret = SD_PARSE_RESULT_SYNTAX_ERR; \
          } \
        } \
      } \
    } while (0)

#define PROC_ATTR_NAME(type) PROC_ATTR_STRING(type, "name", name, dds_sysdef_is_valid_identifier_syntax)

#define _PROC_ATTR_INTEGER(type, attr_type, attr_name, param_field, param_populated_bit) \
    do { \
      if (ddsrt_strcasecmp (name, attr_name) == 0) \
      { \
        assert (!attr_processed); \
        struct type *t = (struct type *) pstate->current; \
        if (t->populated & param_populated_bit) { \
          PARSER_ERROR (pstate, line, "Duplicate attribute '%s'", STR(param_field)); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } else { \
          attr_type ## _t v; \
          if (str_to_ ## attr_type (value, &v)) { \
            t->param_field = v; \
            t->populated |= param_populated_bit; \
            attr_processed = true; \
          } else { \
            PARSER_ERROR (pstate, line, "Invalid value for attribute '%s'", attr_name); \
            ret = SD_PARSE_RESULT_SYNTAX_ERR; \
          } \
        } \
      } \
    } while (0)

#define PROC_ATTR_UINT32(type, attr_name, param_field, param_populated_bit) \
    _PROC_ATTR_INTEGER(type,uint32,attr_name,param_field,param_populated_bit)

#define PROC_ATTR_INT32(type, attr_name, param_field, param_populated_bit) \
    _PROC_ATTR_INTEGER(type,int32,attr_name,param_field,param_populated_bit)

#define _PROC_ATTR_FN(type, attr_name, current, param_field, fn) \
    do { \
      if (ddsrt_strcasecmp (name, attr_name) == 0) \
      { \
        assert (!attr_processed); \
        struct type *t = (struct type *) current; \
        int fn_ret; \
        if ((fn_ret = fn (pstate, value, &t->param_field)) == SD_PARSE_RESULT_OK) { \
          attr_processed = true; \
        } else { \
          if (fn_ret == SD_PARSE_RESULT_DUPLICATE) \
            PARSER_ERROR (pstate, line, "Duplicate attribute '%s'", attr_name); \
          else \
            PARSER_ERROR (pstate, line, "Invalid value for attribute '%s'", attr_name); \
          ret = fn_ret; \
        } \
      } \
    } while (0)

#define PROC_ATTR_FN(type, attr_name, param_field, fn) \
    _PROC_ATTR_FN(type, attr_name, pstate->current, param_field, fn)

#define PROC_ATTR_FN_PARENT(type, attr_name, param_field, fn) \
    _PROC_ATTR_FN(type, attr_name, pstate->current->parent, param_field, fn)

static int proc_attr_resolve_type_ref (struct parse_sysdef_state * const pstate, const char *type_ref, struct dds_sysdef_type **type)
{
  if (*type != NULL) {
    return SD_PARSE_RESULT_DUPLICATE;
  }
  for (struct dds_sysdef_type_lib *tlib = pstate->sysdef->type_libs; *type == NULL && tlib != NULL; tlib = (struct dds_sysdef_type_lib *) tlib->xmlnode.next)
  {
    for (struct dds_sysdef_type *t = tlib->types; *type == NULL && t != NULL; t = (struct dds_sysdef_type *) t->xmlnode.next)
    {
      if (strcmp (t->name, type_ref) == 0) {
        *type = t;
      }
    }
  }
  return *type != NULL ? SD_PARSE_RESULT_OK : SD_PARSE_RESULT_INVALID_REF;
}

static struct dds_sysdef_register_type *find_reg_type_impl (const char *register_type_ref, struct dds_sysdef_register_type *register_types)
{
  struct dds_sysdef_register_type *register_type = NULL;
  for (struct dds_sysdef_register_type *t = register_types; register_type == NULL && t != NULL; t = (struct dds_sysdef_register_type *) t->xmlnode.next)
  {
    if (strcmp (t->name, register_type_ref) == 0)
      register_type = t;
  }
  return register_type;
}

static int proc_attr_resolve_register_type_ref (struct parse_sysdef_state * const pstate, const char *register_type_ref, struct dds_sysdef_register_type **register_type)
{
  assert (pstate->current->parent->kind == ELEMENT_KIND_DOMAIN || pstate->current->parent->kind == ELEMENT_KIND_PARTICIPANT);
  if (*register_type != NULL) {
    return SD_PARSE_RESULT_DUPLICATE;
  }
  struct dds_sysdef_domain *domain = NULL;
  if (pstate->current->parent->kind == ELEMENT_KIND_PARTICIPANT)
  {
    for (struct dds_sysdef_participant *participant = (struct dds_sysdef_participant *) pstate->current->parent; *register_type == NULL && participant != NULL; participant = participant->base)
    {
      if ((*register_type = find_reg_type_impl (register_type_ref, participant->register_types)) == NULL)
      {
        // Not found in this participant, get the domain to search in
        if (participant->domain_ref == NULL)
          ;
        else if (domain == NULL)
          domain = participant->domain_ref;
        else
        {
          // Domain ref for a participant should be equal to the domain ref of its base-participants
          if (domain != participant->domain_ref)
            return SD_PARSE_RESULT_ERR;
        }
      }
    }
    if (domain == NULL && *register_type == NULL)
      return SD_PARSE_RESULT_ERR;
  }
  else
  {
    domain = (struct dds_sysdef_domain *) pstate->current->parent;
  }

  if (*register_type == NULL)
    *register_type = find_reg_type_impl (register_type_ref, domain->register_types);

  return *register_type != NULL ? SD_PARSE_RESULT_OK : SD_PARSE_RESULT_INVALID_REF;
}

static int proc_attr_resolve_topic_ref (struct parse_sysdef_state * const pstate, const char *topic_ref, struct dds_sysdef_topic **topic)
{
  if (*topic != NULL) {
    return SD_PARSE_RESULT_DUPLICATE;
  }
  assert ((pstate->current->kind == ELEMENT_KIND_READER && pstate->current->parent->kind == ELEMENT_KIND_SUBSCRIBER) ||
      (pstate->current->kind == ELEMENT_KIND_WRITER && pstate->current->parent->kind == ELEMENT_KIND_PUBLISHER));
  assert (pstate->current->parent->parent->kind == ELEMENT_KIND_PARTICIPANT);
  struct dds_sysdef_domain *domain = NULL;
  for (struct dds_sysdef_participant *participant = (struct dds_sysdef_participant *) pstate->current->parent->parent; *topic == NULL && participant != NULL; participant = participant->base)
  {
    struct dds_sysdef_topic *topics[2] = { participant->topics };
    if (participant->domain_ref == NULL)
      ;
    else if (domain == NULL)
    {
      domain = participant->domain_ref;
      topics[1] = domain->topics;
    }
    else
    {
      /* Domain ref for a participant should be equal to the domain ref of its base-participants,
         this is checked in the validation step after parsing */
    }
    for (uint32_t n = 0; n < sizeof (topics) / sizeof (topics[0]); n++)
    {
      for (struct dds_sysdef_topic *t = topics[n]; *topic == NULL && t != NULL; t = (struct dds_sysdef_topic *) t->xmlnode.next)
      {
        if (strcmp (t->name, topic_ref) == 0) {
          *topic = t;
        }
      }
    }
  }

  return *topic != NULL ? SD_PARSE_RESULT_OK : SD_PARSE_RESULT_INVALID_REF;
}


static int split_ref (const char *ref, char **lib, char **local_name)
{
  int ret = SD_PARSE_RESULT_OK;
  const char *sep = "::";
  const char *spos = strstr (ref, sep);
  if (spos != NULL)
  {
    ptrdiff_t lib_len = spos - ref;
    *lib = ddsrt_strndup (ref, (size_t) lib_len);
    *local_name = ddsrt_strdup (spos + strlen (sep));
  }
  else
  {
    ret = SD_PARSE_RESULT_INVALID_REF;
  }
  return ret;
}

#define _RESOLVE_LIB(lib_type, lib_name, dst) \
    do { for (struct dds_sysdef_## lib_type ## _lib *l = pstate->sysdef->lib_type ## _libs ; dst == NULL && l != NULL; l = (struct dds_sysdef_ ## lib_type ## _lib *) l->xmlnode.next) \
    { \
      if (strcmp (l->name, lib_name) == 0) { \
        dst = l; \
      } \
    } } while (0)

#define _RESOLVE_ENTITY(lib, entity_type, ent_var, ent_name, dst) \
    do { for (struct dds_sysdef_ ## entity_type *e = lib->ent_var; dst == NULL && e != NULL; e = (struct dds_sysdef_## entity_type *) e->xmlnode.next) \
    { \
      if (strcmp (e->name, ent_name) == 0) { \
        dst = e; \
      } \
    } } while (0)

#define RESOLVE_REF_FNDEF(type, lib_type, type_var) \
  static int proc_attr_resolve_ ## type ## _ref (struct parse_sysdef_state * const pstate, const char *type_ref, struct dds_sysdef_ ## type **type) \
  { \
    char *lib_name, *local_name; \
    if (*type != NULL) { \
      return SD_PARSE_RESULT_DUPLICATE; \
    } \
    if (split_ref (type_ref, &lib_name, &local_name) != SD_PARSE_RESULT_OK) \
      return SD_PARSE_RESULT_ERR; \
    struct dds_sysdef_ ## lib_type ## _lib *lib = NULL; \
    _RESOLVE_LIB(lib_type, lib_name, lib); \
    if (lib != NULL) \
      _RESOLVE_ENTITY(lib, type, type_var, local_name, *type); \
    ddsrt_free (lib_name); \
    ddsrt_free (local_name); \
    return *type != NULL ? SD_PARSE_RESULT_OK : SD_PARSE_RESULT_INVALID_REF; \
  }

RESOLVE_REF_FNDEF(qos_profile, qos, qos_profiles)
RESOLVE_REF_FNDEF(domain, domain, domains)
RESOLVE_REF_FNDEF(participant, participant, participants)
RESOLVE_REF_FNDEF(node, node, nodes)
RESOLVE_REF_FNDEF(application, application, applications)

enum resolve_endpoint_kind {
  RESOLVE_ENDPOINT_READER,
  RESOLVE_ENDPOINT_WRITER
};

static int proc_attr_resolve_endpoint_ref (struct parse_sysdef_state * const pstate, const char *type_ref, enum resolve_endpoint_kind kind, struct xml_element **endpoint)
{
  char *appl_lib_name, *tail;
  if (*endpoint != NULL) {
    return SD_PARSE_RESULT_ERR;
  }
  if (split_ref (type_ref, &appl_lib_name, &tail) != SD_PARSE_RESULT_OK)
    return SD_PARSE_RESULT_ERR;
  struct dds_sysdef_application_lib *appl_lib = NULL;
  _RESOLVE_LIB(application, appl_lib_name, appl_lib);
  if (appl_lib != NULL)
  {
    char *appl_name, *tail1;
    if (split_ref (tail, &appl_name, &tail1) != SD_PARSE_RESULT_OK)
      return SD_PARSE_RESULT_ERR;

    struct dds_sysdef_application *appl = NULL;
    _RESOLVE_ENTITY(appl_lib, application, applications, appl_name, appl);
    if (appl != NULL)
    {
      char *pp_name, *tail2;
      if (split_ref (tail1, &pp_name, &tail2) != SD_PARSE_RESULT_OK)
        return SD_PARSE_RESULT_ERR;

      struct dds_sysdef_participant *pp = NULL;
      _RESOLVE_ENTITY(appl, participant, participants, pp_name, pp);
      if (pp != NULL)
      {
        char *pubsub_name, *endpoint_name;
        if (split_ref (tail2, &pubsub_name, &endpoint_name) != SD_PARSE_RESULT_OK)
          return SD_PARSE_RESULT_ERR;

        if (kind == RESOLVE_ENDPOINT_WRITER)
        {
          struct dds_sysdef_publisher *pub = NULL;
          _RESOLVE_ENTITY(pp, publisher, publishers, pubsub_name, pub);
          if (pub != NULL)
          {
            struct dds_sysdef_writer **writer = (struct dds_sysdef_writer **) endpoint;
            _RESOLVE_ENTITY(pub, writer, writers, endpoint_name, *writer);
          }
        }
        else
        {
          struct dds_sysdef_subscriber *sub = NULL;
          _RESOLVE_ENTITY(pp, subscriber, subscribers, pubsub_name, sub);
          if (sub != NULL)
          {
            struct dds_sysdef_reader **reader = (struct dds_sysdef_reader **) endpoint;
            _RESOLVE_ENTITY(sub, reader, readers, endpoint_name, *reader);
          }
        }
        ddsrt_free (endpoint_name);
        ddsrt_free (pubsub_name);
      }
      ddsrt_free (pp_name);
      ddsrt_free (tail2);
    }
    ddsrt_free (appl_name);
    ddsrt_free (tail1);
  }
  ddsrt_free (appl_lib_name);
  ddsrt_free (tail);

  return *endpoint != NULL ? SD_PARSE_RESULT_OK : SD_PARSE_RESULT_INVALID_REF;
}

static int proc_attr_resolve_datawriter_ref (struct parse_sysdef_state * const pstate, const char *type_ref, struct dds_sysdef_writer **writer)
{
  return proc_attr_resolve_endpoint_ref (pstate, type_ref, RESOLVE_ENDPOINT_WRITER, (struct xml_element **) writer);
}

static int proc_attr_resolve_datareader_ref (struct parse_sysdef_state * const pstate, const char *type_ref, struct dds_sysdef_reader **reader)
{
  return proc_attr_resolve_endpoint_ref (pstate, type_ref, RESOLVE_ENDPOINT_READER, (struct xml_element **) reader);
}

static int dds_sysdef_parse_hex (const char* hex, unsigned char* bytes)
{
  size_t hex_len = strlen (hex);
  if (hex_len % 2 != 0)
    return SD_PARSE_RESULT_ERR;

  for (size_t i = 0; i < hex_len; i += 2)
  {
    int a = ddsrt_todigit (hex[i]), b = ddsrt_todigit (hex[i + 1]);
    if (a == -1 || a > 15 || b == -1 || b > 15)
      return SD_PARSE_RESULT_ERR;
    bytes[i / 2] = (unsigned char) (((uint8_t) a << 4) + b);
  }
  return SD_PARSE_RESULT_OK;
}

static int proc_attr_type_identifier (struct parse_sysdef_state * const pstate, const char *value, unsigned char **identifier)
{
  (void) pstate;
  if (*identifier != NULL)
    return SD_PARSE_RESULT_DUPLICATE;
  if (strlen (value) != 2 * TYPE_HASH_LENGTH)
    return SD_PARSE_RESULT_ERR;
  *identifier = ddsrt_malloc (TYPE_HASH_LENGTH);
  if (dds_sysdef_parse_hex (value, *identifier) != SD_PARSE_RESULT_OK)
    return SD_PARSE_RESULT_ERR;
  return SD_PARSE_RESULT_OK;
}

static int proc_attr_guid_prefix (struct parse_sysdef_state * const pstate, const char *value, struct dds_sysdef_participant_guid_prefix **prefix)
{
  (void) pstate;
  if (strlen (value) != 2 * sizeof (struct dds_sysdef_participant_guid_prefix))
    return SD_PARSE_RESULT_ERR;

  if (*prefix != NULL)
    return SD_PARSE_RESULT_DUPLICATE;

  *prefix = ddsrt_malloc (sizeof (**prefix));
  union { struct dds_sysdef_participant_guid_prefix p; unsigned char d[sizeof (struct dds_sysdef_participant_guid_prefix)]; } u = {.p = {0}};
  if (dds_sysdef_parse_hex (value, u.d) != SD_PARSE_RESULT_OK)
    return SD_PARSE_RESULT_ERR;

  (*prefix)->p = ddsrt_fromBE4u (u.p.p);
  return SD_PARSE_RESULT_OK;
}

static bool is_alpha (char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_alphanum (char c)
{
  return is_alpha (c) || (c >= '0' && c <= '9');
}

static bool is_valid_identifier_char (char c)
{
  return is_alphanum (c) || c == '_';
}

static bool dds_sysdef_is_valid_identifier_syntax (const char *name)
{
  if (strlen (name) == 0)
    return false;
  if (!is_alpha (name[0]))
    return false;
  for (size_t i = 1; i < strlen (name); i++)
  {
    if (!is_valid_identifier_char (name[i]))
      return false;
  }
  return true;
}

static int proc_attr (void *varg, UNUSED_ARG (uintptr_t eleminfo), const char *name, const char *value, int line)
{
  /* All attributes are processed immediately after opening the element */
  struct parse_sysdef_state * const pstate = varg;
  bool attr_processed = false;

  if (ddsrt_strcasecmp(name, "xmlns") == 0 || ddsrt_strcasecmp(name, "xmlns:xsi") == 0 ||ddsrt_strcasecmp(name, "xsi:schemaLocation") == 0)
    return 0;

  int ret = SD_PARSE_RESULT_OK;
  if (pstate->current == NULL) {
    PARSER_ERROR (pstate, line, "Current element NULL in proc_attr");
    ret = SD_PARSE_RESULT_ERR;
  }
  else
  {
    switch (pstate->current->kind)
    {
      // Type library
      case ELEMENT_KIND_TYPE:
        PROC_ATTR_NAME(dds_sysdef_type);
        PROC_ATTR_FN(dds_sysdef_type, "identifier", identifier, proc_attr_type_identifier);
        break;

      // QoS library
      case ELEMENT_KIND_QOS_LIB:
        PROC_ATTR_NAME(dds_sysdef_qos_lib);
        break;
      case ELEMENT_KIND_QOS_PROFILE:
        PROC_ATTR_NAME(dds_sysdef_qos_profile);
        PROC_ATTR_FN(dds_sysdef_qos_profile, "base_name", base_profile, proc_attr_resolve_qos_profile_ref);
        break;
      case ELEMENT_KIND_QOS_PARTICIPANT:
      case ELEMENT_KIND_QOS_TOPIC:
      case ELEMENT_KIND_QOS_PUBLISHER:
      case ELEMENT_KIND_QOS_SUBSCRIBER:
      case ELEMENT_KIND_QOS_WRITER:
      case ELEMENT_KIND_QOS_READER:
        PROC_ATTR_NAME(dds_sysdef_qos);
        PROC_ATTR_FN(dds_sysdef_qos, "base_name", base_profile, proc_attr_resolve_qos_profile_ref);
        break;

      // Domain library
      case ELEMENT_KIND_DOMAIN_LIB:
        PROC_ATTR_NAME(dds_sysdef_domain_lib);
        break;
      case ELEMENT_KIND_DOMAIN:
        PROC_ATTR_NAME(dds_sysdef_domain);
        PROC_ATTR_UINT32(dds_sysdef_domain, "domain_id", domain_id, SYSDEF_DOMAIN_DOMAIN_ID_PARAM_VALUE);
        PROC_ATTR_INT32(dds_sysdef_domain, "participant_index", participant_index, SYSDEF_DOMAIN_PARTICIPANT_INDEX_PARAM_VALUE);
        break;
      case ELEMENT_KIND_PARTICIPANT:
        PROC_ATTR_NAME(dds_sysdef_participant);
        PROC_ATTR_FN(dds_sysdef_participant, "domain_ref", domain_ref, proc_attr_resolve_domain_ref);
        PROC_ATTR_FN(dds_sysdef_participant, "base_name", base, proc_attr_resolve_participant_ref);
        PROC_ATTR_FN(dds_sysdef_participant, "guid_prefix", guid_prefix, proc_attr_guid_prefix);
        break;
      case ELEMENT_KIND_REGISTER_TYPE:
        PROC_ATTR_NAME(dds_sysdef_register_type);
        PROC_ATTR_FN(dds_sysdef_register_type, "type_ref", type_ref, proc_attr_resolve_type_ref);
        break;
      case ELEMENT_KIND_PARTICIPANT_LIB:
        PROC_ATTR_NAME(dds_sysdef_participant);
        break;
      case ELEMENT_KIND_TOPIC:
        PROC_ATTR_NAME(dds_sysdef_topic);
        PROC_ATTR_FN(dds_sysdef_topic, "register_type_ref", register_type_ref, proc_attr_resolve_register_type_ref);
        break;
      case ELEMENT_KIND_PUBLISHER:
        PROC_ATTR_NAME(dds_sysdef_publisher);
        break;
      case ELEMENT_KIND_SUBSCRIBER:
        PROC_ATTR_NAME(dds_sysdef_subscriber);
        break;
      case ELEMENT_KIND_WRITER:
        PROC_ATTR_NAME(dds_sysdef_writer);
        PROC_ATTR_FN(dds_sysdef_writer, "topic_ref", topic, proc_attr_resolve_topic_ref);
        PROC_ATTR_UINT32(dds_sysdef_writer, "entity_key", entity_key, SYSDEF_WRITER_ENTITY_KEY_PARAM_VALUE);
        break;
      case ELEMENT_KIND_READER:
        PROC_ATTR_NAME(dds_sysdef_reader);
        PROC_ATTR_FN(dds_sysdef_reader, "topic_ref", topic, proc_attr_resolve_topic_ref);
        PROC_ATTR_UINT32(dds_sysdef_reader, "entity_key", entity_key, SYSDEF_READER_ENTITY_KEY_PARAM_VALUE);
        break;

      // Application library
      case ELEMENT_KIND_APPLICATION_LIB:
        PROC_ATTR_NAME(dds_sysdef_application_lib);
        break;
      case ELEMENT_KIND_APPLICATION:
        PROC_ATTR_NAME(dds_sysdef_application);
        break;

      // Node library
      case ELEMENT_KIND_NODE_LIB:
        PROC_ATTR_NAME(dds_sysdef_node_lib);
        break;
      case ELEMENT_KIND_NODE:
        PROC_ATTR_NAME(dds_sysdef_node);
        break;

      // Deployment library
      case ELEMENT_KIND_DEPLOYMENT_LIB:
        PROC_ATTR_NAME(dds_sysdef_deployment_lib);
        break;
      case ELEMENT_KIND_DEPLOYMENT:
        PROC_ATTR_NAME(dds_sysdef_deployment);
        break;
      case ELEMENT_KIND_DEPLOYMENT_NODE_REF:
        PROC_ATTR_FN_PARENT(dds_sysdef_deployment, "node_ref", node, proc_attr_resolve_node_ref);
        break;
      case ELEMENT_KIND_DEPLOYMENT_APPLICATION_REF:
        PROC_ATTR_FN(dds_sysdef_application_ref, "application_ref", application, proc_attr_resolve_application_ref);
        break;
      case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER:
        PROC_ATTR_NAME(dds_sysdef_tsn_talker_configuration);
        PROC_ATTR_STRING(dds_sysdef_tsn_talker_configuration, "stream_name", stream_name, dds_sysdef_is_valid_identifier_syntax);
        PROC_ATTR_FN(dds_sysdef_tsn_talker_configuration, "datawriter_ref", writer, proc_attr_resolve_datawriter_ref);
        break;
      case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER:
        PROC_ATTR_NAME(dds_sysdef_tsn_listener_configuration);
        PROC_ATTR_STRING(dds_sysdef_tsn_listener_configuration, "stream_name", stream_name, dds_sysdef_is_valid_identifier_syntax);
        PROC_ATTR_FN(dds_sysdef_tsn_listener_configuration, "datareader_ref", reader, proc_attr_resolve_datareader_ref);
        break;

      default:
        break;
    }

    if (!attr_processed && ret == SD_PARSE_RESULT_OK)
    {
      PARSER_ERROR (pstate, line, "Unknown attribute '%s'", name);
      ret = SD_PARSE_RESULT_ERR;
    }
  }
  return ret;
}

#define ELEM_CLOSE_QOS_POLICY(policy, policy_desc) \
      do { \
        struct dds_sysdef_QOS_POLICY_ ## policy *qp = (struct dds_sysdef_QOS_POLICY_ ## policy *) pstate->current; \
        struct dds_sysdef_qos *sdqos = (struct dds_sysdef_qos *) pstate->current->parent; \
        if (qp->populated != QOS_POLICY_ ## policy ## _PARAMS) \
        { \
          PARSER_ERROR (pstate, line, "Not all params set for " policy_desc " QoS policy"); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } \
        else if (qget_ ## policy (sdqos->qos)) \
        { \
          PARSER_ERROR (pstate, line, policy_desc " QoS policy already set"); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } \
        else \
        { \
          qset_ ## policy (sdqos->qos, qp ); \
        } \
      } while (0)

#define ELEM_CLOSE_QOS_DURATION_PROPERTY(policy, param, element_name) \
      do { \
        assert (pstate->current->data_type == ELEMENT_DATA_TYPE_DURATION); \
        struct dds_sysdef_qos_duration_property *d = (struct dds_sysdef_qos_duration_property *) pstate->current; \
        struct dds_sysdef_QOS_POLICY_ ## policy *qp = (struct dds_sysdef_QOS_POLICY_ ## policy *) pstate->current->parent; \
        if (d->populated == 0) \
        { \
          PARSER_ERROR (pstate, line, "Duration not set"); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } \
        else \
        { \
          qp->values.element_name = DDS_SECS(d->sec) + d->nsec; \
          qp->populated |= QOS_POLICY_ ## policy ## _PARAM_ ## param; \
        } \
      } while (0)

static bool qget_DEADLINE (dds_qos_t *qos)
{
  return dds_qget_deadline (qos, NULL);
}

static void qset_DEADLINE (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_DEADLINE *qp)
{
  dds_qset_deadline (qos, qp->values.deadline);
}

static bool qget_DESTINATIONORDER (dds_qos_t *qos)
{
  return dds_qget_destination_order (qos, NULL);
}

static void qset_DESTINATIONORDER (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_DESTINATIONORDER *qp)
{
  dds_qset_destination_order (qos, qp->values.kind);
}

static bool qget_DURABILITY (dds_qos_t *qos)
{
  return dds_qget_durability (qos, NULL);
}

static void qset_DURABILITY (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_DURABILITY *qp)
{
  dds_qset_durability (qos, qp->values.kind);
}

static bool qget_DURABILITYSERVICE (dds_qos_t *qos)
{
  return dds_qget_durability_service (qos, NULL, NULL, NULL, NULL, NULL, NULL);
}

static void qset_DURABILITYSERVICE (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_DURABILITYSERVICE *qp)
{
  dds_qset_durability_service (qos, qp->values.service_cleanup_delay, qp->values.history.kind, qp->values.history.depth, qp->values.resource_limits.max_samples, qp->values.resource_limits.max_instances, qp->values.resource_limits.max_samples_per_instance);
}

static bool qget_GROUPDATA (dds_qos_t *qos)
{
  return dds_qget_groupdata (qos, NULL, NULL);
}

static void qset_GROUPDATA (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_GROUPDATA *qp)
{
  dds_qset_groupdata (qos, qp->values.value, qp->values.length);
}

static bool qget_TOPICDATA (dds_qos_t *qos)
{
  return dds_qget_topicdata (qos, NULL, NULL);
}

static void qset_TOPICDATA (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_TOPICDATA *qp)
{
  dds_qset_topicdata (qos, qp->values.value, qp->values.length);
}

static bool qget_USERDATA (dds_qos_t *qos)
{
  return dds_qget_userdata (qos, NULL, NULL);
}

static void qset_USERDATA (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_USERDATA *qp)
{
  dds_qset_userdata (qos, qp->values.value, qp->values.length);
}

static bool qget_HISTORY (dds_qos_t *qos)
{
  return dds_qget_history (qos, NULL, NULL);
}

static void qset_HISTORY (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_HISTORY *qp)
{
  dds_qset_history (qos, qp->values.kind, qp->values.depth);
}

static bool qget_LATENCYBUDGET (dds_qos_t *qos)
{
  return dds_qget_latency_budget (qos, NULL);
}

static void qset_LATENCYBUDGET (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_LATENCYBUDGET *qp)
{
  dds_qset_latency_budget (qos, qp->values.duration);
}

static bool qget_LIFESPAN (dds_qos_t *qos)
{
  return dds_qget_lifespan (qos, NULL);
}

static void qset_LIFESPAN (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_LIFESPAN *qp)
{
  dds_qset_lifespan (qos, qp->values.duration);
}

static bool qget_LIVELINESS (dds_qos_t *qos)
{
  return dds_qget_liveliness (qos, NULL, NULL);
}

static void qset_LIVELINESS (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_LIVELINESS *qp)
{
  dds_qset_liveliness (qos, qp->values.kind, qp->values.lease_duration);
}

static bool qget_OWNERSHIP (dds_qos_t *qos)
{
  return dds_qget_ownership (qos, NULL);
}

static void qset_OWNERSHIP (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_OWNERSHIP *qp)
{
  dds_qset_ownership (qos, qp->values.kind);
}

static bool qget_OWNERSHIPSTRENGTH (dds_qos_t *qos)
{
  return dds_qget_ownership_strength (qos, NULL);
}

static void qset_OWNERSHIPSTRENGTH (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_OWNERSHIPSTRENGTH *qp)
{
  dds_qset_ownership_strength (qos, qp->values.value);
}

static bool qget_PARTITION (dds_qos_t *qos)
{
  return dds_qget_partition (qos, NULL, NULL);
}

static void qset_PARTITION (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_PARTITION *qp)
{
  uint32_t c = 0;
  for (struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *v = qp->name->elements; v != NULL; v = (struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *) v->xmlnode.next)
    c++;

  const char **partitions = ddsrt_malloc (c * sizeof (*partitions));
  uint32_t i = 0;
  for (struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *v = qp->name->elements; v != NULL; v = (struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *) v->xmlnode.next)
    partitions[i++] = v->element;
  dds_qset_partition (qos, c, partitions);
#if _MSC_VER
__pragma(warning(push))
__pragma(warning(disable: 4090))
#endif
  ddsrt_free (partitions);
#if _MSC_VER
__pragma(warning(pop))
#endif
}

static bool qget_PRESENTATION (dds_qos_t *qos)
{
  return dds_qget_presentation (qos, NULL, NULL, NULL);
}

static void qset_PRESENTATION (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_PRESENTATION *qp)
{
  dds_qset_presentation (qos, qp->values.access_scope, qp->values.coherent_access, qp->values.ordered_access);
}

static bool qget_READERDATALIFECYCLE (dds_qos_t *qos)
{
  return dds_qget_reader_data_lifecycle (qos, NULL, NULL);
}

static void qset_READERDATALIFECYCLE (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_READERDATALIFECYCLE *qp)
{
  dds_qset_reader_data_lifecycle (qos, qp->values.autopurge_nowriter_samples_delay, qp->values.autopurge_disposed_samples_delay);
}

static bool qget_RELIABILITY (dds_qos_t *qos)
{
  return dds_qget_reliability (qos, NULL, NULL);
}

static void qset_RELIABILITY (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_RELIABILITY *qp)
{
  dds_qset_reliability (qos, qp->values.kind, qp->values.max_blocking_time);
}

static bool qget_RESOURCELIMITS (dds_qos_t *qos)
{
  return dds_qget_resource_limits (qos, NULL, NULL, NULL);
}

static void qset_RESOURCELIMITS (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_RESOURCELIMITS *qp)
{
  dds_qset_resource_limits (qos, qp->values.max_samples, qp->values.max_instances, qp->values.max_samples_per_instance);
}

static bool qget_TIMEBASEDFILTER (dds_qos_t *qos)
{
  return dds_qget_time_based_filter (qos, NULL);
}

static void qset_TIMEBASEDFILTER (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_TIMEBASEDFILTER *qp)
{
  dds_qset_time_based_filter (qos, qp->values.minimum_separation);
}

static bool qget_TRANSPORTPRIORITY (dds_qos_t *qos)
{
  return dds_qget_transport_priority (qos, NULL);
}

static void qset_TRANSPORTPRIORITY (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_TRANSPORTPRIORITY *qp)
{
  dds_qset_transport_priority (qos, qp->values.value);
}

static bool qget_WRITERDATALIFECYCLE (dds_qos_t *qos)
{
  return dds_qget_writer_data_lifecycle (qos, NULL);
}

static void qset_WRITERDATALIFECYCLE (dds_qos_t *qos, struct dds_sysdef_QOS_POLICY_WRITERDATALIFECYCLE *qp)
{
  dds_qset_writer_data_lifecycle (qos, qp->values.autodispose_unregistered_instances);
}

#define _ELEM_CLOSE_REQUIRE_ATTR(type,attr_name,current,element_name) \
      do { \
        struct type *t = (struct type *) current; \
        if (t->element_name == NULL) \
        { \
          PARSER_ERROR (pstate, line, "Attribute '%s' not set", STR(attr_name)); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } \
      } while (0)

#define ELEM_CLOSE_REQUIRE_ATTR(type,attr_name,param_field) \
    _ELEM_CLOSE_REQUIRE_ATTR(type, attr_name, pstate->current, param_field)

#define ELEM_CLOSE_REQUIRE_ATTR_PARENT(type,attr_name,param_field) \
    _ELEM_CLOSE_REQUIRE_ATTR(type, attr_name, pstate->current->parent, param_field)

#define ELEM_CLOSE_REQUIRE_ATTR_POPULATED(type,type_name,exp_populated) \
    do { \
      if (~(((struct type *) pstate->current)->populated) & exp_populated) \
      { \
        PARSER_ERROR (pstate, line, "Not all params set for %s", type_name); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } \
    } while (0)


static int proc_elem_close (void *varg, UNUSED_ARG (uintptr_t eleminfo), UNUSED_ARG (int line))
{
  struct parse_sysdef_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;
  if (pstate->current == NULL) {
    PARSER_ERROR (pstate, line, "Current element NULL in close element");
    ret = SD_PARSE_RESULT_ERR;
  }
  else
  {
    switch (pstate->current->kind)
    {
      case ELEMENT_KIND_QOS_POLICY_DEADLINE:
        ELEM_CLOSE_QOS_POLICY(DEADLINE, "Deadline");
        break;
      case ELEMENT_KIND_QOS_POLICY_DEADLINE_PERIOD:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(DEADLINE, PERIOD, deadline);
        break;
      case ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER:
        ELEM_CLOSE_QOS_POLICY(DESTINATIONORDER, "Destination Order");
        break;
      case ELEMENT_KIND_QOS_POLICY_DURABILITY:
        ELEM_CLOSE_QOS_POLICY(DURABILITY, "Durability");
        break;
      case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE:
        ELEM_CLOSE_QOS_POLICY(DURABILITYSERVICE, "Durability Service");
        break;
      case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_SERVICE_CLEANUP_DELAY:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(DURABILITYSERVICE, SERVICE_CLEANUP_DELAY, service_cleanup_delay);
        break;
      case ELEMENT_KIND_QOS_POLICY_GROUPDATA:
        ELEM_CLOSE_QOS_POLICY(GROUPDATA, "Group Data");
        break;
      case ELEMENT_KIND_QOS_POLICY_HISTORY:
        ELEM_CLOSE_QOS_POLICY(HISTORY, "History");
        break;
      case ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET:
        ELEM_CLOSE_QOS_POLICY(LATENCYBUDGET, "Latency Budget");
        break;
      case ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET_DURATION:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(LATENCYBUDGET, DURATION, duration);
        break;
      case ELEMENT_KIND_QOS_POLICY_LIFESPAN:
        ELEM_CLOSE_QOS_POLICY(LIFESPAN, "Lifespan");
        break;
      case ELEMENT_KIND_QOS_POLICY_LIFESPAN_DURATION:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(LIFESPAN, DURATION, duration);
        break;
      case ELEMENT_KIND_QOS_POLICY_LIVELINESS:
        ELEM_CLOSE_QOS_POLICY(LIVELINESS, "Liveliness");
        break;
      case ELEMENT_KIND_QOS_POLICY_LIVELINESS_LEASE_DURATION:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(LIVELINESS, LEASE_DURATION, lease_duration);
        break;
      case ELEMENT_KIND_QOS_POLICY_OWNERSHIP:
        ELEM_CLOSE_QOS_POLICY(OWNERSHIP, "Ownership");
        break;
      case ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH:
        ELEM_CLOSE_QOS_POLICY(OWNERSHIPSTRENGTH, "Ownership Strength");
        break;
      case ELEMENT_KIND_QOS_POLICY_PARTITION:
        ELEM_CLOSE_QOS_POLICY(PARTITION, "Partition");
        break;
      case ELEMENT_KIND_QOS_POLICY_PRESENTATION:
        ELEM_CLOSE_QOS_POLICY(PRESENTATION, "Presentation");
        break;
      case ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE_AUTOPURGE_NOWRITER_SAMPLES_DELAY:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(READERDATALIFECYCLE, AUTOPURGE_NOWRITER_SAMPLES_DELAY, autopurge_nowriter_samples_delay);
        break;
      case ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE_AUTOPURGE_DISPOSED_SAMPLES_DELAY:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(READERDATALIFECYCLE, AUTOPURGE_DISPOSED_SAMPLES_DELAY, autopurge_disposed_samples_delay);
        break;
      case ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE:
        ELEM_CLOSE_QOS_POLICY(READERDATALIFECYCLE, "Reader Data Life-cycle");
        break;
      case ELEMENT_KIND_QOS_POLICY_RELIABILITY:
        ELEM_CLOSE_QOS_POLICY(RELIABILITY, "Reliability");
        break;
      case ELEMENT_KIND_QOS_POLICY_RELIABILITY_MAX_BLOCKING_DELAY:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(RELIABILITY, MAX_BLOCKING_DELAY, max_blocking_time);
        break;
      case ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS:
        ELEM_CLOSE_QOS_POLICY(RESOURCELIMITS, "Resource Limits");
        break;
      case ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER_MINIMUM_SEPARATION:
        ELEM_CLOSE_QOS_DURATION_PROPERTY(TIMEBASEDFILTER, MINIMUM_SEPARATION, minimum_separation);
        break;
      case ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER:
        ELEM_CLOSE_QOS_POLICY(TIMEBASEDFILTER, "Time-based Filter");
        break;
      case ELEMENT_KIND_QOS_POLICY_TOPICDATA:
        ELEM_CLOSE_QOS_POLICY(TOPICDATA, "Topic Data");
        break;
      case ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY:
        ELEM_CLOSE_QOS_POLICY(TRANSPORTPRIORITY, "Transport Priority");
        break;
      case ELEMENT_KIND_QOS_POLICY_USERDATA:
        ELEM_CLOSE_QOS_POLICY(USERDATA, "User data");
        break;
      case ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE:
        ELEM_CLOSE_QOS_POLICY(WRITERDATALIFECYCLE, "Writer Data Life-cycle");
        break;
      case ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY:
        //ELEM_CLOSE_QOS_POLICY(ENTITYFACTORY, "Entity factory");
        PARSER_ERROR (pstate, line, "Unsupported QoS policy");
        ret = SD_PARSE_RESULT_NOT_SUPPORTED;
        break;
      case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_PERIODICITY: {
        struct dds_sysdef_tsn_traffic_specification *t = (struct dds_sysdef_tsn_traffic_specification *) pstate->current->parent;
        struct dds_sysdef_qos_duration_property *d = (struct dds_sysdef_qos_duration_property *) pstate->current;
        t->periodicity = DDS_SECS (d->sec) + d->nsec;
        break;
      }
      case ELEMENT_KIND_TYPE:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_type, "name", name);
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_type, "identifier", identifier);
        break;
      case ELEMENT_KIND_QOS_LIB:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_qos_lib, "name", name);
        break;
      case ELEMENT_KIND_QOS_PROFILE:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_qos_profile, "name", name);
        break;
      case ELEMENT_KIND_DOMAIN_LIB:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_domain_lib, "name", name);
        break;
      case ELEMENT_KIND_DOMAIN:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_domain, "name", name);
        ELEM_CLOSE_REQUIRE_ATTR_POPULATED (dds_sysdef_domain, "domain", SYSDEF_DOMAIN_PARAMS);
        break;
      case ELEMENT_KIND_PARTICIPANT:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_participant, "name", name);
        break;
      case ELEMENT_KIND_REGISTER_TYPE:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_register_type, "name", name);
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_register_type, "type_ref", type_ref);
        break;
      case ELEMENT_KIND_PARTICIPANT_LIB:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_participant, "name", name);
        break;
      case ELEMENT_KIND_TOPIC:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_topic, "name", name);
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_topic, "register_type_ref", register_type_ref);
        break;
      case ELEMENT_KIND_PUBLISHER:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_publisher, "name", name);
        break;
      case ELEMENT_KIND_SUBSCRIBER:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_subscriber, "name", name);
        break;
      case ELEMENT_KIND_WRITER:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_writer, "name", name);
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_writer, "topic_ref", topic);
        ELEM_CLOSE_REQUIRE_ATTR_POPULATED (dds_sysdef_writer, "writer", SYSDEF_WRITER_PARAMS);
        break;
      case ELEMENT_KIND_READER:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_reader, "name", name);
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_reader, "topic_ref", topic);
        ELEM_CLOSE_REQUIRE_ATTR_POPULATED (dds_sysdef_reader, "reader", SYSDEF_READER_PARAMS);
        break;
      case ELEMENT_KIND_APPLICATION_LIB:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_application_lib, "name", name);
        break;
      case ELEMENT_KIND_APPLICATION:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_application, "name", name);
        break;
      case ELEMENT_KIND_DEPLOYMENT_NODE_REF:
        ELEM_CLOSE_REQUIRE_ATTR_PARENT (dds_sysdef_deployment, "node_ref", node);
        break;
      case ELEMENT_KIND_DEPLOYMENT_APPLICATION_REF:
        ELEM_CLOSE_REQUIRE_ATTR (dds_sysdef_application_ref, "application_ref", application);
        break;
      default:
        if (pstate->current->handle_close)
        {
          PARSER_ERROR (pstate, line, "Close element not handled");
          ret = SD_PARSE_RESULT_ERR;
        }
        break;
    }
  }

  struct xml_element *parent = pstate->current->parent;
  if (!pstate->current->retain) {
    free_node (pstate->current);
  }
  pstate->current = parent;
  return ret;
}

#define QOS_PARAM_SET_NUMERIC_UNLIMITED(policy, param, param_field, type) \
    static int set_ ## policy ## _ ## param (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_ ## policy *qp, const char *value, int line) \
    { \
      int ret = SD_PARSE_RESULT_OK; \
      type ## _t s; \
      if (!strcmp(value, QOS_LENGTH_UNLIMITED)){ \
        qp->values.param_field = -1; \
      } else if (str_to_ ## type (value, &s)) { \
        qp->values.param_field = s; \
      } else { \
        PARSER_ERROR (pstate, line, "Invalid value '%s'", value); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } \
      return ret; \
    }

#define QOS_PARAM_SET_NUMERIC(policy, param, param_field, type) \
    static int set_ ## policy ## _ ## param (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_ ## policy *qp, const char *value, int line) \
    { \
      int ret = SD_PARSE_RESULT_OK; \
      type ## _t s; \
      if (str_to_ ## type (value, &s)) { \
        qp->values.param_field = s; \
      } else { \
        PARSER_ERROR (pstate, line, "Invalid value '%s'", value); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } \
      return ret; \
    }

#define QOS_PARAM_SET_BOOLEAN(policy, param, param_field) \
    static int set_ ## policy ## _ ## param (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_ ## policy *qp, const char *value, int line) \
    { \
      int ret = SD_PARSE_RESULT_OK; \
      bool s; \
      if (str_to_bool (value, &s)) { \
        qp->values.param_field = s; \
      } else { \
        PARSER_ERROR (pstate, line, "Invalid value '%s'", value); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } \
      return ret; \
    }

#define QOS_PARAM_SET_STRING(policy, param, param_field) \
    static int set_ ## policy ## _ ## param (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_ ## policy *qp, const char *value, int line) \
    { \
      qp->values.param_field = ddsrt_strdup (value); \
      return SD_PARSE_RESULT_OK; \
    }

#define QOS_PARAM_SET_BASE64(policy, param, param_data_field, param_length_field) \
    static int set_ ## policy ## _ ## param (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_ ## policy *qp, const char *value, int line) \
    { \
      (void) pstate; (void) line; \
      /* FIXME: base 64 decode */ \
      qp->values.param_data_field = ddsrt_memdup (value, strlen (value)); \
      qp->values.param_length_field = (uint32_t) strlen (value); \
      return SD_PARSE_RESULT_OK; \
    }

static int set_DESTINATIONORDER_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_DESTINATIONORDER *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS") == 0) {
    qp->values.kind = DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP;
  } else if (strcmp (value, "BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS") == 0) {
    qp->values.kind = DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_DURABILITY_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_DURABILITY *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "VOLATILE_DURABILITY_QOS") == 0) {
    qp->values.kind = DDS_DURABILITY_VOLATILE;
  } else if (strcmp (value, "TRANSIENT_LOCAL_DURABILITY_QOS") == 0) {
    qp->values.kind = DDS_DURABILITY_TRANSIENT_LOCAL;
  } else if (strcmp (value, "TRANSIENT_DURABILITY_QOS") == 0) {
    qp->values.kind = DDS_DURABILITY_TRANSIENT;
  } else if (strcmp (value, "PERSISTENT_DURABILITY_QOS") == 0) {
    PARSER_ERROR (pstate, line, "Unsupported value '%s'", value);
    ret = SD_PARSE_RESULT_NOT_SUPPORTED;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_DURABILITYSERVICE_HISTORY_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_DURABILITYSERVICE *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "KEEP_LAST_HISTORY_QOS") == 0) {
    qp->values.history.kind = DDS_HISTORY_KEEP_LAST;
  } else if (strcmp (value, "KEEP_ALL_HISTORY_QOS") == 0) {
    qp->values.history.kind = DDS_HISTORY_KEEP_ALL;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_HISTORY_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_HISTORY *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "KEEP_LAST_HISTORY_QOS") == 0) {
    qp->values.kind = DDS_HISTORY_KEEP_LAST;
  } else if (strcmp (value, "KEEP_ALL_HISTORY_QOS") == 0) {
    qp->values.kind = DDS_HISTORY_KEEP_ALL;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_LIVELINESS_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_LIVELINESS *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "AUTOMATIC_LIVELINESS_QOS") == 0) {
    qp->values.kind = DDS_LIVELINESS_AUTOMATIC;
  } else if (strcmp (value, "MANUAL_BY_PARTICIPANT_LIVELINESS_QOS") == 0) {
    qp->values.kind = DDS_LIVELINESS_MANUAL_BY_PARTICIPANT;
  } else if (strcmp (value, "MANUAL_BY_TOPIC_LIVELINESS_QOS") == 0) {
    qp->values.kind = DDS_LIVELINESS_MANUAL_BY_TOPIC;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_OWNERSHIP_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_OWNERSHIP *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "SHARED_OWNERSHIP_QOS") == 0) {
    qp->values.kind = DDS_OWNERSHIP_SHARED;
  } else if (strcmp (value, "EXCLUSIVE_OWNERSHIP_QOS") == 0) {
    qp->values.kind = DDS_OWNERSHIP_EXCLUSIVE;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_PRESENTATION_ACCESS_SCOPE (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_PRESENTATION *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "INSTANCE_PRESENTATION_QOS") == 0) {
    qp->values.access_scope = DDS_PRESENTATION_INSTANCE;
  } else if (strcmp (value, "TOPIC_PRESENTATION_QOS") == 0) {
    qp->values.access_scope = DDS_PRESENTATION_TOPIC;
  } else if (strcmp (value, "GROUP_PRESENTATION_QOS") == 0) {
    qp->values.access_scope = DDS_PRESENTATION_GROUP;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

static int set_RELIABILITY_KIND (struct parse_sysdef_state * const pstate, struct dds_sysdef_QOS_POLICY_RELIABILITY *qp, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "BEST_EFFORT_RELIABILITY_QOS") == 0) {
    qp->values.kind = DDS_RELIABILITY_BEST_EFFORT;
  } else if (strcmp (value, "RELIABLE_RELIABILITY_QOS") == 0) {
    qp->values.kind = DDS_RELIABILITY_RELIABLE;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

QOS_PARAM_SET_NUMERIC(DURABILITYSERVICE, HISTORY_DEPTH, history.depth, int32)
QOS_PARAM_SET_NUMERIC_UNLIMITED(DURABILITYSERVICE, RESOURCE_LIMIT_MAX_SAMPLES, resource_limits.max_samples, int32)
QOS_PARAM_SET_NUMERIC_UNLIMITED(DURABILITYSERVICE, RESOURCE_LIMIT_MAX_INSTANCES, resource_limits.max_instances, int32)
QOS_PARAM_SET_NUMERIC_UNLIMITED(DURABILITYSERVICE, RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE, resource_limits.max_samples_per_instance, int32)
QOS_PARAM_SET_BOOLEAN(ENTITYFACTORY, AUTOENABLE_CREATED_ENTITIES, autoenable_created_entities)
QOS_PARAM_SET_NUMERIC(HISTORY, DEPTH, depth, int32)
QOS_PARAM_SET_NUMERIC(OWNERSHIPSTRENGTH, VALUE, value, int32)
QOS_PARAM_SET_BOOLEAN(PRESENTATION, COHERENT_ACCESS, coherent_access)
QOS_PARAM_SET_BOOLEAN(PRESENTATION, ORDERED_ACCESS, ordered_access)
QOS_PARAM_SET_NUMERIC_UNLIMITED(RESOURCELIMITS, MAX_SAMPLES, max_samples, int32)
QOS_PARAM_SET_NUMERIC_UNLIMITED(RESOURCELIMITS, MAX_INSTANCES, max_instances, int32)
QOS_PARAM_SET_NUMERIC_UNLIMITED(RESOURCELIMITS, MAX_SAMPLES_PER_INSTANCE, max_samples_per_instance, int32)
QOS_PARAM_SET_NUMERIC(TRANSPORTPRIORITY, VALUE, value, int32)
QOS_PARAM_SET_BOOLEAN(WRITERDATALIFECYCLE, AUTODISPOSE_UNREGISTERED_INSTANCES, autodispose_unregistered_instances)
QOS_PARAM_SET_BASE64(GROUPDATA, VALUE, value, length)
QOS_PARAM_SET_BASE64(TOPICDATA, VALUE, value, length)
QOS_PARAM_SET_BASE64(USERDATA, VALUE, value, length)

static int parse_tsn_traffic_transmission_selection (struct parse_sysdef_state * const pstate, enum dds_sysdef_tsn_traffic_transmission_selection *s, const char *value, int line)
{
  int ret = SD_PARSE_RESULT_OK;
  if (strcmp (value, "STRICT_PRIORITY") == 0) {
    *s = DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_STRICT_PRIORITY;
  } else if (strcmp (value, "CREDIT_BASED_SHAPER") == 0) {
    *s = DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_CREDIT_BASED_SHAPER;
  } else if (strcmp (value, "ENHANCED_TRANSMISSION_SELECTION") == 0) {
    *s = DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_ENHANCED_TRANSMISSION_SELECTION;
  } else if (strcmp (value, "ATS_TRANSMISSION_SELECTION") == 0) {
    *s = DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_ATS_TRANSMISSION_SELECTION;
  } else {
    PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }
  return ret;
}

#define QOS_PARAM_DATA(policy,param) \
    do { \
      struct dds_sysdef_QOS_POLICY_ ## policy *qp = (struct dds_sysdef_QOS_POLICY_ ## policy *) pstate->current->parent; \
      assert (qp->xmlnode.kind == ELEMENT_KIND_QOS_POLICY_ ## policy); \
      if (qp->populated & QOS_POLICY_ ## policy ## _PARAM_ ## param) { \
        PARSER_ERROR (pstate, line, "Parameter '%s' already set", STR(param)); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } else { \
        ret = set_## policy ## _ ## param (pstate, qp, value, line); \
        qp->populated |= QOS_POLICY_ ## policy ## _PARAM_ ## param; \
      } \
    } while (0)

#define PARENT_PARAM_DATA_STRING(parent_kind, parent_type, param_field) \
    do { \
      assert (pstate->current->parent->kind == parent_kind); \
      struct parent_type *parent = (struct parent_type *) pstate->current->parent; \
      if (parent->param_field != NULL) { \
        PARSER_ERROR (pstate, line, "Parameter '%s' already set", STR(param_field)); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } else { \
        parent->param_field = ddsrt_strdup (value); \
      } \
    } while (0)

#define PARENT_PARAM_DATA_NUMERIC(parent_kind, parent_type, type, param_field, param_populated_bit) \
    do { \
      assert (pstate->current->parent->kind == parent_kind); \
      struct parent_type *parent = (struct parent_type *) pstate->current->parent; \
      if (parent->populated & param_populated_bit) { \
        PARSER_ERROR (pstate, line, "Parameter '%s' already set", STR(param_field)); \
        ret = SD_PARSE_RESULT_SYNTAX_ERR; \
      } else { \
        type ## _t s; \
        if (str_to_ ## type (value, &s)) { \
          parent->param_field = s; \
          parent->populated |= param_populated_bit; \
        } else { \
          PARSER_ERROR (pstate, line, "Invalid value '%s'", value); \
          ret = SD_PARSE_RESULT_SYNTAX_ERR; \
        } \
      } \
    } while (0)



static int parse_mac_addr (const char *value, struct dds_sysdef_mac_addr **mac_addr)
{
  DDSRT_STATIC_ASSERT (sizeof ((*mac_addr)->addr) == 6);
  if (strlen (value) != 17 || value[2] != ':' || value[5] != ':' || value[8] != ':' || value[11] != ':' || value[14] != ':')
    return SD_PARSE_RESULT_ERR;

  *mac_addr = ddsrt_malloc (sizeof (**mac_addr));
  char v[13] = {'\0'};
  for (uint32_t i = 0; i < 6; i++)
    memcpy (v + 2 * i, value + 3 * i, 2);
  v[12] = '\0';

  if (dds_sysdef_parse_hex (v, (*mac_addr)->addr) != SD_PARSE_RESULT_OK)
    return SD_PARSE_RESULT_ERR;
  return SD_PARSE_RESULT_OK;
}

static int parse_ipv4_addr (const char *value, struct dds_sysdef_ip_addr **ipv4_addr)
{
  *ipv4_addr = ddsrt_malloc (sizeof (**ipv4_addr));
  if (ddsrt_sockaddrfromstr (AF_INET, value, (struct sockaddr *) &(*ipv4_addr)->addr) != 0)
    return SD_PARSE_RESULT_ERR;
  return SD_PARSE_RESULT_OK;
}

static int parse_ipv6_addr (const char *value, struct dds_sysdef_ip_addr **ipv6_addr)
{
  *ipv6_addr = ddsrt_malloc (sizeof (**ipv6_addr));
  if (ddsrt_sockaddrfromstr (AF_INET6, value, (struct sockaddr *) &(*ipv6_addr)->addr) != 0)
    return SD_PARSE_RESULT_ERR;
  return SD_PARSE_RESULT_OK;
}

static int proc_elem_data (void *varg, UNUSED_ARG (uintptr_t eleminfo), const char *value, int line)
{
  struct parse_sysdef_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;
  if (pstate == NULL) {
    return SD_PARSE_RESULT_ERR; 
  }

  if (!pstate->current) {
    PARSER_ERROR (pstate, line, "Current element NULL in processing element data");
    return SD_PARSE_RESULT_ERR;
  }

  switch (pstate->current->kind)
  {
    case ELEMENT_KIND_QOS_DURATION_SEC:
    case ELEMENT_KIND_QOS_DURATION_NSEC: {
      assert (pstate->current->parent->data_type == ELEMENT_DATA_TYPE_DURATION);
      struct dds_sysdef_qos_duration_property *qp = (struct dds_sysdef_qos_duration_property *) pstate->current->parent;
      int64_t v;
      bool res = true;
      if ((pstate->current->kind == ELEMENT_KIND_QOS_DURATION_SEC  && (!strcmp(value, QOS_DURATION_INFINITY) || !strcmp(value, QOS_DURATION_INFINITY_SEC))) ||
          (pstate->current->kind == ELEMENT_KIND_QOS_DURATION_NSEC && (!strcmp(value, QOS_DURATION_INFINITY) || !strcmp(value, QOS_DURATION_INFINITY_NSEC))))
        v = DDS_INFINITY;
      else
        res = str_to_int64(value, &v);

      if (res)
      {
        if (qp->populated & (pstate->current->kind == ELEMENT_KIND_QOS_DURATION_SEC ? QOS_DURATION_PARAM_SEC : QOS_DURATION_PARAM_NSEC))
        {
          PARSER_ERROR (pstate, line, "Already set");
          ret = SD_PARSE_RESULT_SYNTAX_ERR;
        }
        else
        {
          if (pstate->current->kind == ELEMENT_KIND_QOS_DURATION_SEC && (v != DDS_INFINITY)) {
            qp->sec = v;
          } else {
            qp->nsec = v;
          }
          qp->populated |= (pstate->current->kind == ELEMENT_KIND_QOS_DURATION_SEC ? QOS_DURATION_PARAM_SEC : QOS_DURATION_PARAM_NSEC);
        }
      }
      else
      {
        PARSER_ERROR (pstate, line, "Invalid value '%s'", value);
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      break;
    }
    case ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER_KIND:
      QOS_PARAM_DATA (DESTINATIONORDER, KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_DURABILITY_KIND:
      QOS_PARAM_DATA (DURABILITY, KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_HISTORY_KIND:
      QOS_PARAM_DATA (DURABILITYSERVICE, HISTORY_KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_HISTORY_DEPTH:
      QOS_PARAM_DATA (DURABILITYSERVICE, HISTORY_DEPTH);
      break;
    case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_SAMPLES:
      QOS_PARAM_DATA (DURABILITYSERVICE, RESOURCE_LIMIT_MAX_SAMPLES);
      break;
    case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_INSTANCES:
      QOS_PARAM_DATA (DURABILITYSERVICE, RESOURCE_LIMIT_MAX_INSTANCES);
      break;
    case ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE:
      QOS_PARAM_DATA (DURABILITYSERVICE, RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE);
      break;
    case ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY_AUTOENABLE_CREATED_ENTITIES:
      QOS_PARAM_DATA (ENTITYFACTORY, AUTOENABLE_CREATED_ENTITIES);
      break;
    case ELEMENT_KIND_QOS_POLICY_GROUPDATA_VALUE:
      QOS_PARAM_DATA (GROUPDATA, VALUE);
      break;
    case ELEMENT_KIND_QOS_POLICY_HISTORY_KIND:
      QOS_PARAM_DATA (HISTORY, KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_HISTORY_DEPTH:
      QOS_PARAM_DATA (HISTORY, DEPTH);
      break;
    case ELEMENT_KIND_QOS_POLICY_LIVELINESS_KIND:
      QOS_PARAM_DATA (LIVELINESS, KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_OWNERSHIP_KIND:
      QOS_PARAM_DATA (OWNERSHIP, KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH_VALUE:
      QOS_PARAM_DATA (OWNERSHIPSTRENGTH, VALUE);
      break;
    case ELEMENT_KIND_QOS_POLICY_PARTITION_NAME_ELEMENT: {
      struct dds_sysdef_QOS_POLICY_PARTITION *qp = (struct dds_sysdef_QOS_POLICY_PARTITION *) pstate->current->parent->parent;
      struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *p = (struct dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT *) pstate->current;
      if (dds_sysdef_is_valid_identifier_syntax (value))
      {
        p->element = ddsrt_strdup (value);
        qp->populated = true;
      }
      else
      {
        // free_node (qp);
        // pstate->current = NULL;
        PARSER_ERROR (pstate, line, "Invalid partition name '%s'", value);
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      break;
    }
    case ELEMENT_KIND_QOS_POLICY_RELIABILITY_KIND:
      QOS_PARAM_DATA (RELIABILITY, KIND);
      break;
    case ELEMENT_KIND_QOS_POLICY_PRESENTATION_ACCESS_SCOPE:
      QOS_PARAM_DATA (PRESENTATION, ACCESS_SCOPE);
      break;
    case ELEMENT_KIND_QOS_POLICY_PRESENTATION_COHERENT_ACCESS:
      QOS_PARAM_DATA (PRESENTATION, COHERENT_ACCESS);
      break;
    case ELEMENT_KIND_QOS_POLICY_PRESENTATION_ORDERED_ACCESS:
      QOS_PARAM_DATA (PRESENTATION, ORDERED_ACCESS);
      break;
    // case ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_INITIAL_INSTANCES:
    //   QOS_PARAM_DATA (RESOURCELIMITS, INITIAL_INSTANCES);
    //   break;
    // case ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_INITIAL_SAMPLES:
    //   QOS_PARAM_DATA (RESOURCELIMITS, INITIAL_SAMPLES);
    //   break;
    case ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_SAMPLES:
      QOS_PARAM_DATA (RESOURCELIMITS, MAX_SAMPLES);
      break;
    case ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_INSTANCES:
      QOS_PARAM_DATA (RESOURCELIMITS, MAX_INSTANCES);
      break;
    case ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_SAMPLES_PER_INSTANCE:
      QOS_PARAM_DATA (RESOURCELIMITS, MAX_SAMPLES_PER_INSTANCE);
      break;
    case ELEMENT_KIND_QOS_POLICY_TOPICDATA_VALUE:
      QOS_PARAM_DATA (TOPICDATA, VALUE);
      break;
    case ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY_VALUE:
      QOS_PARAM_DATA (TRANSPORTPRIORITY, VALUE);
      break;
    case ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE_AUTODISPOSE_UNREGISTERED_INSTANCES:
      QOS_PARAM_DATA (WRITERDATALIFECYCLE, AUTODISPOSE_UNREGISTERED_INSTANCES);
      break;
    case ELEMENT_KIND_QOS_POLICY_USERDATA_VALUE:
      QOS_PARAM_DATA (USERDATA, VALUE);
      break;
    case ELEMENT_KIND_NODE_HOSTNAME:
      PARENT_PARAM_DATA_STRING(ELEMENT_KIND_NODE, dds_sysdef_node, hostname);
      break;
    case ELEMENT_KIND_NODE_IPV4_ADDRESS: {
      assert (pstate->current->parent->kind == ELEMENT_KIND_NODE);
      struct dds_sysdef_node *node = (struct dds_sysdef_node *) pstate->current->parent;
      if (node->ipv4_addr != NULL)
      {
        PARSER_ERROR (pstate, line, "Parameter 'ipv4_addr' already set");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      else if (parse_ipv4_addr (value, &node->ipv4_addr) != SD_PARSE_RESULT_OK)
      {
        PARSER_ERROR (pstate, line, "Invalid value for parameter 'ipv4_addr'");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      break;
    }
    case ELEMENT_KIND_NODE_IPV6_ADDRESS: {
      assert (pstate->current->parent->kind == ELEMENT_KIND_NODE);
      struct dds_sysdef_node *node = (struct dds_sysdef_node *) pstate->current->parent;
      if (node->ipv6_addr != NULL)
      {
        PARSER_ERROR (pstate, line, "Parameter 'ipv6_addr' already set");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      else if (parse_ipv6_addr (value, &node->ipv6_addr) != SD_PARSE_RESULT_OK)
      {
        PARSER_ERROR (pstate, line, "Invalid value for parameter 'ipv6_addr'");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      break;
    }
    case ELEMENT_KIND_NODE_MAC_ADDRESS: {
      assert (pstate->current->parent->kind == ELEMENT_KIND_NODE);
      struct dds_sysdef_node *node = (struct dds_sysdef_node *) pstate->current->parent;
      if (node->mac_addr != NULL)
      {
        PARSER_ERROR (pstate, line, "Parameter 'mac_addr' already set");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      else if (parse_mac_addr (value, &node->mac_addr) != SD_PARSE_RESULT_OK)
      {
        PARSER_ERROR (pstate, line, "Invalid value for parameter 'mac_addr'");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      break;
    }
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_SOURCE_IP_ADDRESS:
      PARENT_PARAM_DATA_STRING(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, dds_sysdef_tsn_ip_tuple, source_ip_address);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DESTINATION_IP_ADDRESS:
      PARENT_PARAM_DATA_STRING(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, dds_sysdef_tsn_ip_tuple, destination_ip_address);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DSCP:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, dds_sysdef_tsn_ip_tuple, uint8, dscp, SYSDEF_TSN_IP_TUPLE_DSCP_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_PROTOCOL:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, dds_sysdef_tsn_ip_tuple, uint16, protocol, SYSDEF_TSN_IP_TUPLE_PROTOCOL_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_SOURCE_PORT:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, dds_sysdef_tsn_ip_tuple, uint16, source_port, SYSDEF_TSN_IP_TUPLE_SOURCE_PORT_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DESTINATION_PORT:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, dds_sysdef_tsn_ip_tuple, uint16, destination_port, SYSDEF_TSN_IP_TUPLE_DESTINATION_PORT_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_SOURCE_IP_ADDRESS:
      PARENT_PARAM_DATA_STRING(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, dds_sysdef_tsn_ip_tuple, source_ip_address);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DESTINATION_IP_ADDRESS:
      PARENT_PARAM_DATA_STRING(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, dds_sysdef_tsn_ip_tuple, destination_ip_address);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DSCP:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, dds_sysdef_tsn_ip_tuple, uint8, dscp, SYSDEF_TSN_IP_TUPLE_DSCP_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_PROTOCOL:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, dds_sysdef_tsn_ip_tuple, uint16, protocol, SYSDEF_TSN_IP_TUPLE_PROTOCOL_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_SOURCE_PORT:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, dds_sysdef_tsn_ip_tuple, uint16, source_port, SYSDEF_TSN_IP_TUPLE_SOURCE_PORT_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DESTINATION_PORT:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, dds_sysdef_tsn_ip_tuple, uint16, destination_port, SYSDEF_TSN_IP_TUPLE_DESTINATION_PORT_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_SAMPLES_PER_PERIOD:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, dds_sysdef_tsn_traffic_specification, uint16, samples_per_period, SYSDEF_TSN_TRAFFIC_SPEC_SAMPLES_PER_PERIOD_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_MAX_BYTES_PER_SAMPLE:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, dds_sysdef_tsn_traffic_specification, uint16, max_bytes_per_sample, SYSDEF_TSN_TRAFFIC_SPEC_MAX_BYTES_PER_SAMPLE_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TRANSMISSION_SELECTION: {
      assert (pstate->current->parent->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC);
      struct dds_sysdef_tsn_traffic_specification *parent = (struct dds_sysdef_tsn_traffic_specification *) pstate->current->parent;
      if (parent->populated & SYSDEF_TSN_TRAFFIC_SPEC_TRANSMISSION_SELECTION_PARAM_VALUE)
      {
        PARSER_ERROR (pstate, line, "Parameter 'transmission_selection' already set");
        ret = SD_PARSE_RESULT_SYNTAX_ERR;
      }
      else
      {
        enum dds_sysdef_tsn_traffic_transmission_selection s;
        if ((ret = parse_tsn_traffic_transmission_selection (pstate, &s, value, line)) == SD_PARSE_RESULT_OK) {
          parent->transmission_selection = s;
          parent->populated |= SYSDEF_TSN_TRAFFIC_SPEC_TRANSMISSION_SELECTION_PARAM_VALUE;
        }
      }
      break;
    }
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_EARLIEST_TRANSMIT_OFFSET:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, dds_sysdef_tsn_time_aware, uint32, earliest_transmit_offset, SYSDEF_TSN_TIME_AWARE_EARLIEST_TRANSMIT_OFFSET_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_LATEST_TRANSMIT_OFFSET:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, dds_sysdef_tsn_time_aware, uint32, latest_transmit_offset, SYSDEF_TSN_TIME_AWARE_LATEST_TRANSMIT_OFFSET_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_JITTER:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, dds_sysdef_tsn_time_aware, uint32, jitter, SYSDEF_TSN_TIME_JITTER_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS_NUM_SEAMLESS_TREES:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS, dds_sysdef_tsn_network_requirements, uint8, num_seamless_trees, SYSDEF_TSN_NETWORK_REQ_NUM_SEAMLESS_TREES_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS_MAX_LATENCY:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS, dds_sysdef_tsn_network_requirements, uint32, max_latency, SYSDEF_TSN_NETWORK_REQ_MAX_LATENCY_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS_NUM_SEAMLESS_TREES:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS, dds_sysdef_tsn_network_requirements, uint8, num_seamless_trees, SYSDEF_TSN_NETWORK_REQ_NUM_SEAMLESS_TREES_PARAM_VALUE);
      break;
    case ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS_MAX_LATENCY:
      PARENT_PARAM_DATA_NUMERIC(ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS, dds_sysdef_tsn_network_requirements, uint32, max_latency, SYSDEF_TSN_NETWORK_REQ_MAX_LATENCY_PARAM_VALUE);
      break;
    default:
      PARSER_ERROR (pstate, line, "Element data not allowed");
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
      break;
  }

  if (ret != SD_PARSE_RESULT_OK)
  {
    struct xml_element *n = pstate->current, *tmp;
    while (n != NULL)
    {
      tmp = n->parent;
      if (!n->retain)
        free_node (n);
      n = tmp;
    }
  }

  return ret;
}

#define PARSER_ERROR_INVALID_PARENT_KIND() \
    do { \
      PARSER_ERROR (pstate, line, "Invalid parent kind (%d) for element '%s'", pstate->current->kind, name); \
      return SD_PARSE_RESULT_SYNTAX_ERR; \
    } while (0)

static int proc_elem_open (void *varg, UNUSED_ARG (uintptr_t parentinfo), UNUSED_ARG (uintptr_t *eleminfo), const char *name, int line)
{
  if (varg == NULL)
    return SD_PARSE_RESULT_ERR;

  struct parse_sysdef_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;

  if (ddsrt_strcasecmp (name, "dds") == 0)
  {
    if (pstate->current != NULL || pstate->sysdef != NULL) {
      PARSER_ERROR (pstate, line, "Nested element '%s' not supported", name);
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
    }
    else if ((pstate->current = new_node (pstate, ELEMENT_KIND_DDS, ELEMENT_DATA_TYPE_GENERIC, NULL, sizeof(struct dds_sysdef_system), NO_INIT, fini_sysdef)) == NULL)
    {
      PARSER_ERROR (pstate, line, "Error creating root node");
      ret = SD_PARSE_RESULT_ERR;
    }
    else
    {
      pstate->sysdef = (struct dds_sysdef_system *) pstate->current;
      goto status_ok;
    }
  }
  else
  {
    CHECK_PARENT_NULL (pstate, pstate->current);
    if (pstate->scope & SYSDEF_SCOPE_TYPE_LIB)
    {
      // Type library
      if (ddsrt_strcasecmp (name, "types") == 0)
        CREATE_NODE_SINGLE (pstate, dds_sysdef_type_lib, ELEMENT_KIND_TYPE_LIB, NO_INIT, fini_type_lib, type_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "struct") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_type, ELEMENT_KIND_TYPE, NO_INIT, fini_type, types, dds_sysdef_type_lib, ELEMENT_KIND_TYPE_LIB, pstate->current);
    }

    if (pstate->scope & SYSDEF_SCOPE_QOS_LIB)
    {
      // QoS library
      if (ddsrt_strcasecmp (name, "qos_library") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_qos_lib, ELEMENT_KIND_QOS_LIB, NO_INIT, fini_qos_lib, qos_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "qos_profile") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, NO_INIT, fini_qos_profile, qos_profiles, dds_sysdef_qos_lib, ELEMENT_KIND_QOS_LIB, pstate->current);

      else if (ddsrt_strcasecmp (name, "domain_participant_qos") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_PROFILE)
          CREATE_NODE_LIST (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_PARTICIPANT, init_qos, fini_qos, qos, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_PARTICIPANT)
          CREATE_NODE_SINGLE (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_PARTICIPANT, init_qos, fini_qos, qos, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "publisher_qos") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_PROFILE)
          CREATE_NODE_LIST (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_PUBLISHER, init_qos, fini_qos, qos, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_PUBLISHER)
          CREATE_NODE_SINGLE (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_PUBLISHER, init_qos, fini_qos, qos, dds_sysdef_publisher, ELEMENT_KIND_PUBLISHER, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "subscriber_qos") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_PROFILE)
          CREATE_NODE_LIST (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_SUBSCRIBER, init_qos, fini_qos, qos, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_SUBSCRIBER)
          CREATE_NODE_SINGLE (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_SUBSCRIBER, init_qos, fini_qos, qos, dds_sysdef_subscriber, ELEMENT_KIND_SUBSCRIBER, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "topic_qos") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_PROFILE)
          CREATE_NODE_LIST (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_TOPIC, init_qos, fini_qos, qos, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_TOPIC)
          CREATE_NODE_SINGLE (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_TOPIC, init_qos, fini_qos, qos, dds_sysdef_topic, ELEMENT_KIND_TOPIC, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "datawriter_qos") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_PROFILE)
          CREATE_NODE_LIST (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_WRITER, init_qos, fini_qos, qos, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_WRITER)
          CREATE_NODE_SINGLE (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_WRITER, init_qos, fini_qos, qos, dds_sysdef_writer, ELEMENT_KIND_WRITER, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "datareader_qos") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_PROFILE)
          CREATE_NODE_LIST (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_READER, init_qos, fini_qos, qos, dds_sysdef_qos_profile, ELEMENT_KIND_QOS_PROFILE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_READER)
          CREATE_NODE_SINGLE (pstate, dds_sysdef_qos, ELEMENT_KIND_QOS_READER, init_qos, fini_qos, qos, dds_sysdef_reader, ELEMENT_KIND_READER, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }

      // QoS policies
      else if (ddsrt_strcasecmp (name, "deadline") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_DEADLINE, ELEMENT_KIND_QOS_POLICY_DEADLINE, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "destination_order") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_DESTINATIONORDER, ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "durability") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_DURABILITY, ELEMENT_KIND_QOS_POLICY_DURABILITY, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "durability_service") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_DURABILITYSERVICE, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "entity_factory") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_ENTITYFACTORY, ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "group_data") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_GROUPDATA, ELEMENT_KIND_QOS_POLICY_GROUPDATA, NO_INIT, fini_qos_groupdata, pstate->current);
      else if (ddsrt_strcasecmp (name, "history") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_HISTORY, ELEMENT_KIND_QOS_POLICY_HISTORY, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "latency_budget") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_LATENCYBUDGET, ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "lifespan") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_LIFESPAN, ELEMENT_KIND_QOS_POLICY_LIFESPAN, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "liveliness") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_LIVELINESS, ELEMENT_KIND_QOS_POLICY_LIVELINESS, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "ownership") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_OWNERSHIP, ELEMENT_KIND_QOS_POLICY_OWNERSHIP, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "ownership_strength") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_OWNERSHIPSTRENGTH, ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "partition") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_PARTITION, ELEMENT_KIND_QOS_POLICY_PARTITION, NO_INIT, fini_qos_partition, pstate->current);
      else if (ddsrt_strcasecmp (name, "presentation") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_PRESENTATION, ELEMENT_KIND_QOS_POLICY_PRESENTATION, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "reader_data_lifecycle") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_READERDATALIFECYCLE, ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "reliability") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_RELIABILITY, ELEMENT_KIND_QOS_POLICY_RELIABILITY, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "resource_limits") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_RESOURCELIMITS, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "time_based_filter") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_TIMEBASEDFILTER, ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "topic_data") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_TOPICDATA, ELEMENT_KIND_QOS_POLICY_TOPICDATA, NO_INIT, fini_qos_topicdata, pstate->current);
      else if (ddsrt_strcasecmp (name, "transport_priority") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_TRANSPORTPRIORITY, ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY, NO_INIT, NO_FINI, pstate->current);
      else if (ddsrt_strcasecmp (name, "user_data") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_USERDATA, ELEMENT_KIND_QOS_POLICY_USERDATA, NO_INIT, fini_qos_userdata, pstate->current);
      else if (ddsrt_strcasecmp (name, "writer_data_lifecycle") == 0)
        CREATE_NODE_QOS (pstate, dds_sysdef_QOS_POLICY_WRITERDATALIFECYCLE, ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE, NO_INIT, NO_FINI, pstate->current);

      // QoS policy parameters
      else if (ddsrt_strcasecmp (name, "period") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_DEADLINE_PERIOD, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DEADLINE, pstate->current);
      else if (ddsrt_strcasecmp (name, "duration") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET)
          CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET_DURATION, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_LATENCYBUDGET, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_LIFESPAN)
          CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_LIFESPAN_DURATION, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_LIFESPAN, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "service_cleanup_delay") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_SERVICE_CLEANUP_DELAY, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, pstate->current);
      else if (ddsrt_strcasecmp (name, "history_kind") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_HISTORY_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, pstate->current);
      else if (ddsrt_strcasecmp (name, "history_depth") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_HISTORY_DEPTH, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, pstate->current);
      else if (ddsrt_strcasecmp (name, "depth") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_HISTORY_DEPTH, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_HISTORY, pstate->current);
      else if (ddsrt_strcasecmp (name, "max_samples") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_SAMPLES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_SAMPLES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "max_instances") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_INSTANCES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_INSTANCES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "max_samples_per_instance") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE_RESOURCE_LIMIT_MAX_SAMPLES_PER_INSTANCE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITYSERVICE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_MAX_SAMPLES_PER_INSTANCE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "max_blocking_time") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_RELIABILITY_MAX_BLOCKING_DELAY, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RELIABILITY, pstate->current);
      else if (ddsrt_strcasecmp (name, "lease_duration") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_LIVELINESS_LEASE_DURATION, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_LIVELINESS, pstate->current);
      else if (ddsrt_strcasecmp (name, "access_scope") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_PRESENTATION_ACCESS_SCOPE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_PRESENTATION, pstate->current);
      else if (ddsrt_strcasecmp (name, "coherent_access") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_PRESENTATION_COHERENT_ACCESS, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_PRESENTATION, pstate->current);
      else if (ddsrt_strcasecmp (name, "ordered_access") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_PRESENTATION_ORDERED_ACCESS, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_PRESENTATION, pstate->current);
      else if (ddsrt_strcasecmp (name, "autopurge_nowriter_samples_delay") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE_AUTOPURGE_NOWRITER_SAMPLES_DELAY, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE, pstate->current);
      else if (ddsrt_strcasecmp (name, "autopurge_disposed_samples_delay") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE_AUTOPURGE_DISPOSED_SAMPLES_DELAY, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_READERDATALIFECYCLE, pstate->current);
      // else if (ddsrt_strcasecmp (name, "initial_instances") == 0)
      //   CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_INITIAL_INSTANCES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, pstate->current);
      // else if (ddsrt_strcasecmp (name, "initial_samples") == 0)
      //   CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS_INITIAL_SAMPLES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RESOURCELIMITS, pstate->current);
      else if (ddsrt_strcasecmp (name, "ordered_access") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_PRESENTATION_ORDERED_ACCESS, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_PRESENTATION, pstate->current);
      else if (ddsrt_strcasecmp (name, "minimum_separation") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER_MINIMUM_SEPARATION, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_TIMEBASEDFILTER, pstate->current);
      else if (ddsrt_strcasecmp (name, "autodispose_unregistered_instances") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE_AUTODISPOSE_UNREGISTERED_INSTANCES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_WRITERDATALIFECYCLE, pstate->current);
      else if (ddsrt_strcasecmp (name, "autoenable_created_entities") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY_AUTOENABLE_CREATED_ENTITIES, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_ENTITYFACTORY, pstate->current);
      else if (ddsrt_strcasecmp (name, "kind") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DESTINATIONORDER, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_DURABILITY)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_DURABILITY_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_DURABILITY, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_HISTORY)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_HISTORY_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_HISTORY, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_LIVELINESS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_LIVELINESS_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_LIVELINESS, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_OWNERSHIP)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_OWNERSHIP_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_OWNERSHIP, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_RELIABILITY)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_RELIABILITY_KIND, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_RELIABILITY, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "value") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_GROUPDATA)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_GROUPDATA_VALUE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_GROUPDATA, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_TOPICDATA)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_TOPICDATA_VALUE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_TOPICDATA, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_USERDATA)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_USERDATA_VALUE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_USERDATA, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH_VALUE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_OWNERSHIPSTRENGTH, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY_VALUE, NO_INIT, NO_FINI, ELEMENT_KIND_QOS_POLICY_TRANSPORTPRIORITY, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "name") == 0)
        CREATE_NODE_SINGLE (pstate, dds_sysdef_QOS_POLICY_PARTITION_NAME, ELEMENT_KIND_QOS_POLICY_PARTITION_NAME, NO_INIT, fini_qos_partition_name, name, dds_sysdef_QOS_POLICY_PARTITION, ELEMENT_KIND_QOS_POLICY_PARTITION, pstate->current);
      else if (ddsrt_strcasecmp (name, "element") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_QOS_POLICY_PARTITION_NAME_ELEMENT, ELEMENT_KIND_QOS_POLICY_PARTITION_NAME_ELEMENT, NO_INIT, fini_qos_partition_name_element, elements, dds_sysdef_QOS_POLICY_PARTITION_NAME, ELEMENT_KIND_QOS_POLICY_PARTITION_NAME, pstate->current);
      else if (ddsrt_strcasecmp (name, "sec") == 0)
      {
        if (pstate->current->data_type == ELEMENT_DATA_TYPE_DURATION)
          CREATE_NODE_DURATION_SEC (pstate, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "nanosec") == 0)
      {
        if (pstate->current->data_type == ELEMENT_DATA_TYPE_DURATION)
          CREATE_NODE_DURATION_NSEC (pstate, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
    }

    if (pstate->scope & SYSDEF_SCOPE_DOMAIN_LIB)
    {
      // Domain library
      if (ddsrt_strcasecmp (name, "domain_library") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_domain_lib, ELEMENT_KIND_DOMAIN_LIB, NO_INIT, fini_domain_lib, domain_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "domain") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_domain, ELEMENT_KIND_DOMAIN, NO_INIT, fini_domain, domains, dds_sysdef_domain_lib, ELEMENT_KIND_DOMAIN_LIB, pstate->current);
      else if (ddsrt_strcasecmp (name, "register_type") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DOMAIN)
          CREATE_NODE_LIST (pstate, dds_sysdef_register_type, ELEMENT_KIND_REGISTER_TYPE, NO_INIT, fini_register_type, register_types, dds_sysdef_domain, ELEMENT_KIND_DOMAIN, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_PARTICIPANT)
          CREATE_NODE_LIST (pstate, dds_sysdef_register_type, ELEMENT_KIND_REGISTER_TYPE, NO_INIT, fini_register_type, register_types, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "topic") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DOMAIN)
          CREATE_NODE_LIST (pstate, dds_sysdef_topic, ELEMENT_KIND_TOPIC, NO_INIT, fini_topic, topics, dds_sysdef_domain, ELEMENT_KIND_DOMAIN, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_PARTICIPANT)
          CREATE_NODE_LIST (pstate, dds_sysdef_topic, ELEMENT_KIND_TOPIC, NO_INIT, fini_topic, topics, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
    }

    if (pstate->scope & SYSDEF_SCOPE_PARTICIPANT_LIB)
    {
      // Domain participant library
      if (ddsrt_strcasecmp (name, "domain_participant_library") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_participant_lib, ELEMENT_KIND_PARTICIPANT_LIB, NO_INIT, fini_participant_lib, participant_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "domain_participant") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_PARTICIPANT_LIB)
          CREATE_NODE_LIST (pstate, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, NO_INIT, fini_participant, participants, dds_sysdef_participant_lib, ELEMENT_KIND_PARTICIPANT_LIB, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_APPLICATION)
          CREATE_NODE_LIST (pstate, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, NO_INIT, fini_participant, participants, dds_sysdef_application, ELEMENT_KIND_APPLICATION, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "publisher") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_publisher, ELEMENT_KIND_PUBLISHER, NO_INIT, fini_publisher, publishers, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, pstate->current);
      else if (ddsrt_strcasecmp (name, "data_writer") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_writer, ELEMENT_KIND_WRITER, NO_INIT, fini_writer, writers, dds_sysdef_publisher, ELEMENT_KIND_PUBLISHER, pstate->current);
      else if (ddsrt_strcasecmp (name, "subscriber") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_subscriber, ELEMENT_KIND_SUBSCRIBER, NO_INIT, fini_subscriber, subscribers, dds_sysdef_participant, ELEMENT_KIND_PARTICIPANT, pstate->current);
      else if (ddsrt_strcasecmp (name, "data_reader") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_reader, ELEMENT_KIND_READER, NO_INIT, fini_reader, readers, dds_sysdef_subscriber, ELEMENT_KIND_SUBSCRIBER, pstate->current);
    }

    if (pstate->scope & SYSDEF_SCOPE_APPLICATION_LIB)
    {
      // Application library
      if (ddsrt_strcasecmp (name, "application_library") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_application_lib, ELEMENT_KIND_APPLICATION_LIB, NO_INIT, fini_application_lib, application_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "application") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_APPLICATION_LIB)
          CREATE_NODE_LIST (pstate, dds_sysdef_application, ELEMENT_KIND_APPLICATION, NO_INIT, fini_application, applications, dds_sysdef_application_lib, ELEMENT_KIND_APPLICATION_LIB, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_APPLICATION_LIST)
          CREATE_NODE_LIST (pstate, dds_sysdef_application_ref, ELEMENT_KIND_DEPLOYMENT_APPLICATION_REF, NO_INIT, NO_FINI, application_refs, dds_sysdef_application_list, ELEMENT_KIND_DEPLOYMENT_APPLICATION_LIST, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
    }

    if (pstate->scope & SYSDEF_SCOPE_NODE_LIB)
    {
      // Node library
      if (ddsrt_strcasecmp (name, "node_library") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_node_lib, ELEMENT_KIND_NODE_LIB, NO_INIT, fini_node_lib, node_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "node") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_NODE_LIB)
          CREATE_NODE_LIST (pstate, dds_sysdef_node, ELEMENT_KIND_NODE, NO_INIT, fini_node, nodes, dds_sysdef_node_lib, ELEMENT_KIND_NODE_LIB, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_NODE_REF, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "hostname") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_NODE_HOSTNAME, NO_INIT, NO_FINI, ELEMENT_KIND_NODE, pstate->current);
      else if (ddsrt_strcasecmp (name, "ipv4_address") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_NODE_IPV4_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_NODE, pstate->current);
      else if (ddsrt_strcasecmp (name, "ipv6_address") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_NODE_IPV6_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_NODE, pstate->current);
      else if (ddsrt_strcasecmp (name, "mac_address") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_NODE_MAC_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_NODE, pstate->current);
    }

    if (pstate->scope & SYSDEF_SCOPE_DEPLOYMENT_LIB)
    {
      // Deployment library
      if (ddsrt_strcasecmp (name, "deployment_library") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_deployment_lib, ELEMENT_KIND_DEPLOYMENT_LIB, NO_INIT, fini_deployment_lib, deployment_libs, dds_sysdef_system, ELEMENT_KIND_DDS, pstate->current);
      else if (ddsrt_strcasecmp (name, "deployment") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_deployment, ELEMENT_KIND_DEPLOYMENT, NO_INIT, fini_deployment, deployments, dds_sysdef_deployment_lib, ELEMENT_KIND_DEPLOYMENT_LIB, pstate->current);
      else if (ddsrt_strcasecmp (name, "application_list") == 0)
        CREATE_NODE_SINGLE (pstate, dds_sysdef_application_list, ELEMENT_KIND_DEPLOYMENT_APPLICATION_LIST, NO_INIT, fini_application_list, application_list, dds_sysdef_deployment, ELEMENT_KIND_DEPLOYMENT, pstate->current);
      else if (ddsrt_strcasecmp (name, "configuration") == 0)
        CREATE_NODE_SINGLE (pstate, dds_sysdef_configuration, ELEMENT_KIND_DEPLOYMENT_CONF, NO_INIT, fini_conf, configuration, dds_sysdef_deployment, ELEMENT_KIND_DEPLOYMENT, pstate->current);
      else if (ddsrt_strcasecmp (name, "tsn") == 0)
        CREATE_NODE_SINGLE (pstate, dds_sysdef_tsn_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN, NO_INIT, fini_conf_tsn, tsn_configuration, dds_sysdef_configuration, ELEMENT_KIND_DEPLOYMENT_CONF, pstate->current);
      else if (ddsrt_strcasecmp (name, "tsn_talker") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_tsn_talker_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER, NO_INIT, fini_conf_tsn_talker, tsn_talker_configurations, dds_sysdef_tsn_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN, pstate->current);
      else if (ddsrt_strcasecmp (name, "tsn_listener") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_tsn_listener_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER, NO_INIT, fini_conf_tsn_listener, tsn_listener_configurations, dds_sysdef_tsn_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN, pstate->current);
      else if (ddsrt_strcasecmp (name, "data_frame_specification") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_tsn_data_frame_specification, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC, NO_INIT, fini_conf_tsn_data_frame_specification, data_frame_specification, dds_sysdef_tsn_talker_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER, pstate->current);
      else if (ddsrt_strcasecmp (name, "ipv4_tuple") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_tsn_ip_tuple, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, NO_INIT, fini_conf_tsn_ip_tuple, ipv4_tuple, dds_sysdef_tsn_data_frame_specification, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "ipv6_tuple") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_tsn_ip_tuple, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, NO_INIT, fini_conf_tsn_ip_tuple, ipv6_tuple, dds_sysdef_tsn_data_frame_specification, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "source_ip_address") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_SOURCE_IP_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_SOURCE_IP_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "destination_ip_address") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DESTINATION_IP_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DESTINATION_IP_ADDRESS, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "dscp") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DSCP, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DSCP, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "protocol") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_PROTOCOL, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_PROTOCOL, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "source_port") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_SOURCE_PORT, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_SOURCE_PORT, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "destination_port") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE_DESTINATION_PORT, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV4_TUPLE, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE_DESTINATION_PORT, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_DATA_FRAME_SPEC_IPV6_TUPLE, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "traffic_specification") == 0)
        CREATE_NODE_LIST (pstate, dds_sysdef_tsn_traffic_specification, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, NO_INIT, fini_conf_tsn_traffic_specification, traffic_specification, dds_sysdef_tsn_talker_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER, pstate->current);
      else if (ddsrt_strcasecmp (name, "periodicity") == 0)
        CREATE_NODE_DURATION (pstate, dds_sysdef_qos_duration_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_PERIODICITY, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "samples_per_period") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_SAMPLES_PER_PERIOD, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "max_bytes_per_sample") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_MAX_BYTES_PER_SAMPLE, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "transmission_selection") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TRANSMISSION_SELECTION, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "time_aware") == 0)
        CREATE_NODE_SINGLE (pstate, dds_sysdef_tsn_time_aware, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, NO_INIT, NO_FINI, time_aware, dds_sysdef_tsn_traffic_specification, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC, pstate->current);
      else if (ddsrt_strcasecmp (name, "earliest_transmit_offset") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_EARLIEST_TRANSMIT_OFFSET, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, pstate->current);
      else if (ddsrt_strcasecmp (name, "latest_transmit_offset") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_LATEST_TRANSMIT_OFFSET, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, pstate->current);
      else if (ddsrt_strcasecmp (name, "jitter") == 0)
        CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE_JITTER, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_TRAFFIC_SPEC_TIME_AWARE, pstate->current);
      else if (ddsrt_strcasecmp (name, "network_requirements") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER)
          CREATE_NODE_LIST (pstate, dds_sysdef_tsn_network_requirements, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS, NO_INIT, NO_FINI, network_requirements, dds_sysdef_tsn_talker_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER)
          CREATE_NODE_LIST (pstate, dds_sysdef_tsn_network_requirements, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS, NO_INIT, NO_FINI, network_requirements, dds_sysdef_tsn_listener_configuration, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "num_seamless_trees") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS_NUM_SEAMLESS_TREES, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS_NUM_SEAMLESS_TREES, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
      else if (ddsrt_strcasecmp (name, "max_latency") == 0)
      {
        if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS_MAX_LATENCY, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_TALKER_NETWORK_REQUIREMENTS, pstate->current);
        else if (pstate->current->kind == ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS)
          CREATE_NODE_CUSTOM (pstate, dds_sysdef_qos_generic_property, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS_MAX_LATENCY, NO_INIT, NO_FINI, ELEMENT_KIND_DEPLOYMENT_CONF_TSN_LISTENER_NETWORK_REQUIREMENTS, pstate->current);
        else
          PARSER_ERROR_INVALID_PARENT_KIND ();
      }
    }
  }

  if (ret == SD_PARSE_RESULT_OK)
  {
    PARSER_ERROR (pstate, line, "Unknown element '%s'", name);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
  }

  struct xml_element *e = pstate->current;
  while (e != NULL)
  {
    struct xml_element *tmp = e->parent;
    if (!e->retain)
      free_node (e);
    e = tmp;
  }
status_ok:
  return ret;
}

static void proc_error (void *varg, const char *msg, int line)
{
  struct parse_sysdef_state * const pstate = varg;
  if (!pstate->has_err)
  {
    PARSER_ERROR (pstate, line, "Syntax error '%s'", msg);
  }
}

static dds_return_t sysdef_parse(struct ddsrt_xmlp_state *xmlps, struct parse_sysdef_state *pstate, struct dds_sysdef_system **sysdef)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if ((ret = ddsrt_xmlp_parse (xmlps)) != SD_PARSE_RESULT_OK)
  {
    SYSDEF_ERROR ("Error parsing system definition XML: %s (error code %d, line %d)\n", pstate->err_msg, ret, pstate->err_line);
    ret = DDS_RETCODE_ERROR;
    if (pstate->sysdef != NULL)
      dds_sysdef_fini_sysdef (pstate->sysdef);
  }
  else
  {
    *sysdef = pstate->sysdef;
  }

  return ret;
}

dds_return_t dds_sysdef_init_sysdef (FILE *fp, struct dds_sysdef_system **sysdef, uint32_t lib_scope)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct parse_sysdef_state pstate = { .sysdef = NULL, .scope = lib_scope};
  struct ddsrt_xmlp_callbacks cb = {
    .attr = proc_attr,
    .elem_close = proc_elem_close,
    .elem_data = proc_elem_data,
    .elem_open = proc_elem_open,
    .error = proc_error
  };

  struct ddsrt_xmlp_state *xmlps = ddsrt_xmlp_new_file (fp, &pstate, &cb);
  ret = sysdef_parse(xmlps, &pstate, sysdef);
  ddsrt_xmlp_free (xmlps);
  return ret;
}

dds_return_t dds_sysdef_init_sysdef_str (const char *raw, struct dds_sysdef_system **sysdef, uint32_t lib_scope)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct parse_sysdef_state pstate = { .sysdef = NULL, .scope = lib_scope};
  struct ddsrt_xmlp_callbacks cb = {
    .attr = proc_attr,
    .elem_close = proc_elem_close,
    .elem_data = proc_elem_data,
    .elem_open = proc_elem_open,
    .error = proc_error
  };

  struct ddsrt_xmlp_state *xmlps = ddsrt_xmlp_new_string (raw, &pstate, &cb);
  ret = sysdef_parse(xmlps, &pstate, sysdef);
  ddsrt_xmlp_free (xmlps);
  return ret;
}

void dds_sysdef_fini_sysdef (struct dds_sysdef_system *sysdef)
{
  free_node (sysdef);
}

enum parse_type_scope {
  PARSE_TYPE_SCOPE_ROOT,
  PARSE_TYPE_SCOPE_TYPES,
  PARSE_TYPE_SCOPE_TYPE_ENTRY,
  PARSE_TYPE_SCOPE_TYPE_INFO,
  PARSE_TYPE_SCOPE_TYPE_MAP
};

struct parse_type_state {
  bool has_err;
  int err_line;
  char err_msg[MAX_ERRMSG_SZ];
  enum parse_type_scope scope;
  struct dds_sysdef_type_metadata_admin *type_meta_data;
  struct dds_sysdef_type_metadata *current;
};

static bool type_equal (const void *a, const void *b)
{
  const struct dds_sysdef_type_metadata *type_a = a, *type_b = b;
  return memcmp (type_a->type_hash, type_b->type_hash, sizeof (TYPE_HASH_LENGTH)) == 0;
}

static uint32_t type_hash (const void *t)
{
  const struct dds_sysdef_type_metadata *type = t;
  uint32_t hash32;
  memcpy (&hash32, type->type_hash, sizeof (hash32));
  return hash32;
}

static void free_type_meta_data (struct dds_sysdef_type_metadata *tmd)
{
  if (tmd->type_hash != NULL)
    ddsrt_free (tmd->type_hash);
  if (tmd->type_info_cdr != NULL)
    ddsrt_free (tmd->type_info_cdr);
  if (tmd->type_map_cdr != NULL)
    ddsrt_free (tmd->type_map_cdr);
  ddsrt_free (tmd);
}

static int proc_type_elem_open (void *varg, UNUSED_ARG (uintptr_t parentinfo), UNUSED_ARG (uintptr_t *eleminfo), const char *name, int line)
{
  struct parse_type_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;

  if (ddsrt_strcasecmp (name, "types") == 0)
  {
    if (pstate->scope != PARSE_TYPE_SCOPE_ROOT)
    {
      PARSER_ERROR (pstate, line, "Unexpected element '%s'", name);
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
    }
    else
    {
      pstate->scope = PARSE_TYPE_SCOPE_TYPES;
      pstate->type_meta_data = ddsrt_malloc (sizeof (*pstate->type_meta_data));
      pstate->type_meta_data->m = ddsrt_hh_new (1, type_hash, type_equal);
    }
  }
  else if (ddsrt_strcasecmp (name, "type") == 0)
  {
    if (pstate->scope != PARSE_TYPE_SCOPE_TYPES)
    {
      PARSER_ERROR (pstate, line, "Unexpected element '%s'", name);
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
    }
    else
    {
      pstate->scope = PARSE_TYPE_SCOPE_TYPE_ENTRY;
      struct dds_sysdef_type_metadata *t = ddsrt_calloc (1, sizeof (*t));
      if (t == NULL)
      {
        PARSER_ERROR (pstate, line, "Error allocating type meta-data");
        ret = SD_PARSE_RESULT_ERR;
      }
      else
      {
        pstate->current = t;
      }
    }
  }
  else if (ddsrt_strcasecmp (name, "type_info") == 0)
  {
    if (pstate->scope != PARSE_TYPE_SCOPE_TYPE_ENTRY)
    {
      PARSER_ERROR (pstate, line, "Unexpected element '%s'", name);
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
    }
    else
    {
      pstate->scope = PARSE_TYPE_SCOPE_TYPE_INFO;
    }
  }
  else if (ddsrt_strcasecmp (name, "type_map") == 0)
  {
    if (pstate->scope != PARSE_TYPE_SCOPE_TYPE_ENTRY)
    {
      PARSER_ERROR (pstate, line, "Unexpected element '%s'", name);
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
    }
    else
    {
      pstate->scope = PARSE_TYPE_SCOPE_TYPE_MAP;
    }
  }
  else
  {
    PARSER_ERROR (pstate, line, "Unexpected element '%s'", name);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
    if (pstate->scope == PARSE_TYPE_SCOPE_TYPE_ENTRY)
      free_type_meta_data (pstate->current);
  }

  return ret;
}

static int proc_type_elem_close (void *varg, UNUSED_ARG (uintptr_t eleminfo), UNUSED_ARG (int line))
{
  struct parse_type_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;
  if (pstate->scope == PARSE_TYPE_SCOPE_TYPE_INFO)
  {
    pstate->scope = PARSE_TYPE_SCOPE_TYPE_ENTRY;
  }
  else if (pstate->scope == PARSE_TYPE_SCOPE_TYPE_MAP)
  {
    pstate->scope = PARSE_TYPE_SCOPE_TYPE_ENTRY;
  }
  else if (pstate->scope == PARSE_TYPE_SCOPE_TYPE_ENTRY)
  {
    if (pstate->current->type_hash == NULL)
    {
      PARSER_ERROR (pstate, line, "Type identifier not set");
      ret = SD_PARSE_RESULT_ERR;
      free_type_meta_data (pstate->current);
    }
    else if (pstate->current->type_info_cdr == NULL || pstate->current->type_map_cdr == NULL)
    {
      PARSER_ERROR (pstate, line, "Incomplete type meta-data");
      ret = SD_PARSE_RESULT_ERR;
      free_type_meta_data (pstate->current);
    }
    else
    {
      ddsrt_hh_add (pstate->type_meta_data->m, pstate->current);
      pstate->scope = PARSE_TYPE_SCOPE_TYPES;
    }
    pstate->current = NULL;
  }
  else if (pstate->scope == PARSE_TYPE_SCOPE_TYPES)
  {
    pstate->scope = PARSE_TYPE_SCOPE_ROOT;
  }
  return ret;
}

static int proc_type_elem_data (void *varg, UNUSED_ARG (uintptr_t eleminfo), const char *value, int line)
{
  struct parse_type_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;

  switch (pstate->scope)
  {
    case PARSE_TYPE_SCOPE_TYPE_INFO:
      pstate->current->type_info_cdr_sz = strlen (value) / 2;
      pstate->current->type_info_cdr = ddsrt_malloc (pstate->current->type_info_cdr_sz);
      dds_sysdef_parse_hex (value, pstate->current->type_info_cdr);
      break;
    case PARSE_TYPE_SCOPE_TYPE_MAP:
      pstate->current->type_map_cdr_sz = strlen (value) / 2;
      pstate->current->type_map_cdr = ddsrt_malloc (pstate->current->type_map_cdr_sz);
      dds_sysdef_parse_hex (value, pstate->current->type_map_cdr);
      break;
    default:
      PARSER_ERROR (pstate, line, "Unexpected data");
      ret = SD_PARSE_RESULT_SYNTAX_ERR;
      break;
  }
  return ret;
}

static int proc_type_attr (void *varg, UNUSED_ARG (uintptr_t eleminfo), const char *name, const char *value, int line)
{
  struct parse_type_state * const pstate = varg;
  int ret = SD_PARSE_RESULT_OK;
  bool attr_processed = false;

  if (ddsrt_strcasecmp(name, "xmlns") == 0 || ddsrt_strcasecmp(name, "xmlns:xsi") == 0 || ddsrt_strcasecmp(name, "xsi:schemaLocation") == 0)
    return ret;

  switch (pstate->scope)
  {
    case PARSE_TYPE_SCOPE_TYPE_ENTRY:
      if (ddsrt_strcasecmp(name, "type_identifier") == 0)
      {
        if (strlen (value) != 2 * TYPE_HASH_LENGTH)
        {
          PARSER_ERROR (pstate, line, "Invalid type identifier length");
          ret = SD_PARSE_RESULT_SYNTAX_ERR;
          free_type_meta_data (pstate->current);
        }
        else
        {
          pstate->current->type_hash = ddsrt_malloc (TYPE_HASH_LENGTH);
          if ((ret = dds_sysdef_parse_hex (value, pstate->current->type_hash)) != SD_PARSE_RESULT_OK)
          {
            PARSER_ERROR (pstate, line, "Invalid type identifier");
            free_type_meta_data (pstate->current);
          }
          else
            attr_processed = true;
        }
      }
      break;
    default:
      break;
  }

  if (!attr_processed && ret == SD_PARSE_RESULT_OK)
  {
    PARSER_ERROR (pstate, line, "Unexpected attribute '%s'", name);
    ret = SD_PARSE_RESULT_SYNTAX_ERR;
    if (pstate->scope == PARSE_TYPE_SCOPE_TYPE_ENTRY || pstate->scope == PARSE_TYPE_SCOPE_TYPE_INFO || pstate->scope == PARSE_TYPE_SCOPE_TYPE_MAP)
      free_type_meta_data (pstate->current);
  }

  return ret;
}

static void proc_type_error (void *varg, const char *msg, int line)
{
  struct parse_type_state * const pstate = varg;
  if (!pstate->has_err)
  {
    if (pstate->current != NULL)
      free_type_meta_data (pstate->current);
    PARSER_ERROR (pstate, line, "Syntax error '%s'", msg);
  }
}

dds_return_t dds_sysdef_init_data_types (FILE *fp, struct dds_sysdef_type_metadata_admin **type_meta_data)
{
  struct parse_type_state pstate = { 0 };
  struct ddsrt_xmlp_callbacks cb = {
    .attr = proc_type_attr,
    .elem_close = proc_type_elem_close,
    .elem_data = proc_type_elem_data,
    .elem_open = proc_type_elem_open,
    .error = proc_type_error
  };

  struct ddsrt_xmlp_state *xmlps = ddsrt_xmlp_new_file (fp, &pstate, &cb);

  dds_return_t ret = DDS_RETCODE_OK;
  if ((ret = ddsrt_xmlp_parse (xmlps)) != SD_PARSE_RESULT_OK) {
    SYSDEF_ERROR ("Error parsing data types XML: %s (error code %d, line %d)\n", pstate.err_msg, ret, pstate.err_line);
    ret = DDS_RETCODE_ERROR;
    if (pstate.type_meta_data != NULL)
      dds_sysdef_fini_data_types (pstate.type_meta_data);
  }
  else
  {
    *type_meta_data = pstate.type_meta_data;
  }

  ddsrt_xmlp_free (xmlps);
  return ret;
}

static void free_type_meta_data_wrap (void *vnode, void *varg)
{
  (void) varg;
  struct dds_sysdef_type_metadata *tmd = (struct dds_sysdef_type_metadata *) vnode;
  free_type_meta_data (tmd);
}

void dds_sysdef_fini_data_types (struct dds_sysdef_type_metadata_admin *type_meta_data)
{
  if (type_meta_data->m)
  {
    ddsrt_hh_enum (type_meta_data->m, free_type_meta_data_wrap, NULL);
    ddsrt_hh_free (type_meta_data->m);
  }
  ddsrt_free (type_meta_data);
}
