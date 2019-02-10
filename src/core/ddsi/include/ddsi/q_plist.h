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
#ifndef NN_PLIST_H
#define NN_PLIST_H

#include "ddsi/q_feature_check.h"
#include "ddsi/q_xqos.h"


#if defined (__cplusplus)
extern "C" {
#endif


#define PP_PROTOCOL_VERSION                     ((uint64_t)1 <<  0)
#define PP_VENDORID                             ((uint64_t)1 <<  1)
#define PP_UNICAST_LOCATOR                      ((uint64_t)1 <<  2)
#define PP_MULTICAST_LOCATOR                    ((uint64_t)1 <<  3)
#define PP_DEFAULT_UNICAST_LOCATOR              ((uint64_t)1 <<  4)
#define PP_DEFAULT_MULTICAST_LOCATOR            ((uint64_t)1 <<  5)
#define PP_METATRAFFIC_UNICAST_LOCATOR          ((uint64_t)1 <<  6)
#define PP_METATRAFFIC_MULTICAST_LOCATOR        ((uint64_t)1 <<  7)
#define PP_EXPECTS_INLINE_QOS                   ((uint64_t)1 <<  8)
#define PP_PARTICIPANT_MANUAL_LIVELINESS_COUNT  ((uint64_t)1 <<  9)
#define PP_PARTICIPANT_BUILTIN_ENDPOINTS        ((uint64_t)1 << 10)
#define PP_PARTICIPANT_LEASE_DURATION           ((uint64_t)1 << 11)
#define PP_CONTENT_FILTER_PROPERTY              ((uint64_t)1 << 12)
#define PP_PARTICIPANT_GUID                     ((uint64_t)1 << 13)
#define PP_PARTICIPANT_ENTITYID                 ((uint64_t)1 << 14)
#define PP_GROUP_GUID                           ((uint64_t)1 << 15)
#define PP_GROUP_ENTITYID                       ((uint64_t)1 << 16)
#define PP_BUILTIN_ENDPOINT_SET                 ((uint64_t)1 << 17)
#define PP_PROPERTIES                           ((uint64_t)1 << 18)
#define PP_TYPE_MAX_SIZE_SERIALIZED             ((uint64_t)1 << 19)
#define PP_ENTITY_NAME                          ((uint64_t)1 << 20)
#define PP_KEYHASH                              ((uint64_t)1 << 21)
#define PP_STATUSINFO                           ((uint64_t)1 << 22)
#define PP_ORIGINAL_WRITER_INFO                 ((uint64_t)1 << 23)
#define PP_ENDPOINT_GUID                        ((uint64_t)1 << 24)
#define PP_PRISMTECH_WRITER_INFO                ((uint64_t)1 << 25)
#define PP_PRISMTECH_PARTICIPANT_VERSION_INFO   ((uint64_t)1 << 26)
#define PP_PRISMTECH_NODE_NAME                  ((uint64_t)1 << 27)
#define PP_PRISMTECH_EXEC_NAME                  ((uint64_t)1 << 28)
#define PP_PRISMTECH_PROCESS_ID                 ((uint64_t)1 << 29)
#define PP_PRISMTECH_SERVICE_TYPE               ((uint64_t)1 << 30)
#define PP_PRISMTECH_WATCHDOG_SCHEDULING        ((uint64_t)1 << 31)
#define PP_PRISMTECH_LISTENER_SCHEDULING        ((uint64_t)1 << 32)
#define PP_PRISMTECH_BUILTIN_ENDPOINT_SET       ((uint64_t)1 << 33)
#define PP_PRISMTECH_TYPE_DESCRIPTION           ((uint64_t)1 << 34)
#define PP_COHERENT_SET                         ((uint64_t)1 << 37)
#define PP_PRISMTECH_EOTINFO                    ((uint64_t)1 << 38)
#ifdef DDSI_INCLUDE_SSM
#define PP_READER_FAVOURS_SSM                   ((uint64_t)1 << 39)
#endif
#define PP_RTI_TYPECODE                         ((uint64_t)1 << 40)
/* Security extensions. */
#define PP_IDENTITY_TOKEN                       ((uint64_t)1 << 41)
#define PP_PERMISSIONS_TOKEN                    ((uint64_t)1 << 42)
/* Set for unrecognized parameters that are in the reserved space or
   in our own vendor-specific space that have the
   PID_UNRECOGNIZED_INCOMPATIBLE_FLAG set (see DDSI 2.1 9.6.2.2.1) */
#define PP_INCOMPATIBLE                         ((uint64_t)1 << 63)

#define NN_PRISMTECH_PARTICIPANT_VERSION_INFO_FIXED_CDRSIZE (24)

#define NN_PRISMTECH_FL_KERNEL_SEQUENCE_NUMBER  (1u << 0)
#define NN_PRISMTECH_FL_DISCOVERY_INCLUDES_GID  (1u << 1)
#define NN_PRISMTECH_FL_PTBES_FIXED_0           (1u << 2)
#define NN_PRISMTECH_FL_DDSI2_PARTICIPANT_FLAG  (1u << 3)
#define NN_PRISMTECH_FL_PARTICIPANT_IS_DDSI2    (1u << 4)
#define NN_PRISMTECH_FL_MINIMAL_BES_MODE        (1u << 5)
#define NN_PRISMTECH_FL_SUPPORTS_STATUSINFOX    (1u << 5)
/* SUPPORTS_STATUSINFOX: when set, also means any combination of
   write/unregister/dispose supported */

/* For locators one could patch the received message data to create
   singly-linked lists (parameter header -> offset of next entry in
   list relative to current), allowing aliasing of the data. But that
   requires modifying the data. For string sequences the length does
   the same thing. */
struct nn_locators_one {
  struct nn_locators_one *next;
  nn_locator_t loc;
};

typedef struct nn_locators {
  int n;
  struct nn_locators_one *first;
  struct nn_locators_one *last;
} nn_locators_t;

typedef unsigned nn_ipv4address_t;

typedef unsigned nn_port_t;

typedef struct nn_keyhash {
  unsigned char value[16];
} nn_keyhash_t;


#ifdef DDSI_INCLUDE_SSM
typedef struct nn_reader_favours_ssm {
  unsigned state; /* default is false */
} nn_reader_favours_ssm_t;
#endif

typedef struct nn_dataholder
{
  char *class_id;
  nn_propertyseq_t properties;
  nn_binarypropertyseq_t binary_properties;
} nn_dataholder_t;

typedef nn_dataholder_t nn_token_t;


typedef struct nn_prismtech_participant_version_info
{
  unsigned version;
  unsigned flags;
  unsigned unused[3];
  char *internals;
} nn_prismtech_participant_version_info_t;

typedef struct nn_prismtech_eotgroup_tid {
  nn_entityid_t writer_entityid;
  uint32_t transactionId;
} nn_prismtech_eotgroup_tid_t;

typedef struct nn_prismtech_eotinfo {
  uint32_t transactionId;
  uint32_t n;
  nn_prismtech_eotgroup_tid_t *tids;
} nn_prismtech_eotinfo_t;

typedef struct nn_plist {
  uint64_t present;
  uint64_t aliased;
  int unalias_needs_bswap;

  nn_xqos_t qos;

  nn_protocol_version_t protocol_version;
  nn_vendorid_t vendorid;
  nn_locators_t unicast_locators;
  nn_locators_t multicast_locators;
  nn_locators_t default_unicast_locators;
  nn_locators_t default_multicast_locators;
  nn_locators_t metatraffic_unicast_locators;
  nn_locators_t metatraffic_multicast_locators;

  unsigned char expects_inline_qos;
  nn_count_t participant_manual_liveliness_count;
  unsigned participant_builtin_endpoints;
  nn_duration_t participant_lease_duration;
  /* nn_content_filter_property_t content_filter_property; */
  nn_guid_t participant_guid;
  nn_guid_t endpoint_guid;
  nn_guid_t group_guid;
#if 0 /* reserved, rather than NIY */
  nn_entityid_t participant_entityid;
  nn_entityid_t group_entityid;
#endif
  unsigned builtin_endpoint_set;
  unsigned prismtech_builtin_endpoint_set;
  /* int type_max_size_serialized; */
  char *entity_name;
  nn_keyhash_t keyhash;
  unsigned statusinfo;
  nn_prismtech_participant_version_info_t prismtech_participant_version_info;
  char *node_name;
  char *exec_name;
  unsigned char is_service;
  unsigned service_type;
  unsigned process_id;
  char *type_description;
  nn_sequence_number_t coherent_set_seqno;
  nn_prismtech_eotinfo_t eotinfo;
  nn_token_t identity_token;
  nn_token_t permissions_token;
#ifdef DDSI_INCLUDE_SSM
  nn_reader_favours_ssm_t reader_favours_ssm;
#endif
} nn_plist_t;


/***/


typedef struct nn_plist_src {
  nn_protocol_version_t protocol_version;
  nn_vendorid_t vendorid;
  int encoding;
  const unsigned char *buf;
  size_t bufsz;
} nn_plist_src_t;

DDS_EXPORT void nn_plist_init_empty (nn_plist_t *dest);
DDS_EXPORT void nn_plist_mergein_missing (nn_plist_t *a, const nn_plist_t *b);
DDS_EXPORT void nn_plist_copy (nn_plist_t *dst, const nn_plist_t *src);
DDS_EXPORT nn_plist_t *nn_plist_dup (const nn_plist_t *src);
DDS_EXPORT int nn_plist_init_frommsg (nn_plist_t *dest, char **nextafterplist, uint64_t pwanted, uint64_t qwanted, const nn_plist_src_t *src);
DDS_EXPORT void nn_plist_fini (nn_plist_t *ps);
DDS_EXPORT void nn_plist_addtomsg (struct nn_xmsg *m, const nn_plist_t *ps, uint64_t pwanted, uint64_t qwanted);
DDS_EXPORT int nn_plist_init_default_participant (nn_plist_t *plist);

DDS_EXPORT int validate_history_qospolicy (const nn_history_qospolicy_t *q);
DDS_EXPORT int validate_durability_qospolicy (const nn_durability_qospolicy_t *q);
DDS_EXPORT int validate_resource_limits_qospolicy (const nn_resource_limits_qospolicy_t *q);
DDS_EXPORT int validate_history_and_resource_limits (const nn_history_qospolicy_t *qh, const nn_resource_limits_qospolicy_t *qr);
DDS_EXPORT int validate_durability_service_qospolicy (const nn_durability_service_qospolicy_t *q);
DDS_EXPORT int validate_liveliness_qospolicy (const nn_liveliness_qospolicy_t *q);
DDS_EXPORT int validate_destination_order_qospolicy (const nn_destination_order_qospolicy_t *q);
DDS_EXPORT int validate_ownership_qospolicy (const nn_ownership_qospolicy_t *q);
DDS_EXPORT int validate_ownership_strength_qospolicy (const nn_ownership_strength_qospolicy_t *q);
DDS_EXPORT int validate_presentation_qospolicy (const nn_presentation_qospolicy_t *q);
DDS_EXPORT int validate_transport_priority_qospolicy (const nn_transport_priority_qospolicy_t *q);
DDS_EXPORT int validate_reader_data_lifecycle (const nn_reader_data_lifecycle_qospolicy_t *q);
DDS_EXPORT int validate_duration (const nn_duration_t *d);


struct nn_rmsg;
struct nn_rsample_info;
struct nn_rdata;

DDS_EXPORT unsigned char *nn_plist_quickscan (struct nn_rsample_info *dest, const struct nn_rmsg *rmsg, const nn_plist_src_t *src);
DDS_EXPORT const unsigned char *nn_plist_findparam_native_unchecked (const void *src, nn_parameterid_t pid);

#if defined (__cplusplus)
}
#endif

#endif /* NN_PLIST_H */
