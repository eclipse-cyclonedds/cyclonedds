// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PLIST_H
#define DDSI_PLIST_H

#include "dds/ddsrt/bswap.h"
#include "dds/ddsi/ddsi_feature_check.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_keyhash.h"
#include "dds/ddsi/ddsi_tran.h" /* FIXME: eliminate */
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_guid.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* For locators one could patch the received message data to create
   singly-linked lists (parameter header -> offset of next entry in
   list relative to current), allowing aliasing of the data. But that
   requires modifying the data. For string sequences the length does
   the same thing. */
struct ddsi_locators_one {
  struct ddsi_locators_one *next;
  ddsi_locator_t loc;
};

typedef struct ddsi_locators {
  uint32_t n;
  struct ddsi_locators_one *first;
  struct ddsi_locators_one *last;
} ddsi_locators_t;


#ifdef DDS_HAS_SECURITY

typedef struct ddsi_tag {
  char *name;
  char *value;
} ddsi_tag_t;

typedef struct ddsi_tagseq {
  uint32_t n;
  ddsi_tag_t *tags;
} ddsi_tagseq_t;

typedef struct ddsi_datatags {
  ddsi_tagseq_t tags;
} ddsi_datatags_t;

typedef struct ddsi_dataholder {
  char *class_id;
  dds_propertyseq_t properties;
  dds_binarypropertyseq_t binary_properties;
} ddsi_dataholder_t;

typedef struct ddsi_dataholderseq {
  uint32_t n;
  ddsi_dataholder_t *tags;
} ddsi_dataholderseq_t;

typedef ddsi_dataholder_t ddsi_token_t;

/* Used for both ddsi_participant_security_info and ddsi_endpoint_security_info. */
typedef struct ddsi_security_info
{
  uint32_t security_attributes;
  uint32_t plugin_security_attributes;
} ddsi_security_info_t;

#else /* DDS_HAS_SECURITY */

struct ddsi_security_info;
typedef struct ddsi_security_info ddsi_security_info_t;

#endif /* DDS_HAS_SECURITY */


#ifdef DDS_HAS_SSM

typedef struct ddsi_reader_favours_ssm {
  uint32_t state; /* default is false */
} ddsi_reader_favours_ssm_t;

#endif /* DDS_HAS_SSM */


typedef struct ddsi_adlink_participant_version_info
{
  uint32_t version;
  uint32_t flags;
  uint32_t unused[3];
  char *internals;
} ddsi_adlink_participant_version_info_t;

typedef struct ddsi_plist {
  uint64_t present;
  uint64_t aliased;

  dds_qos_t qos;

  ddsi_protocol_version_t protocol_version;
  ddsi_vendorid_t vendorid;
  ddsi_locators_t unicast_locators;
  ddsi_locators_t multicast_locators;
  ddsi_locators_t default_unicast_locators;
  ddsi_locators_t default_multicast_locators;
  ddsi_locators_t metatraffic_unicast_locators;
  ddsi_locators_t metatraffic_multicast_locators;

  unsigned char expects_inline_qos;
  ddsi_count_t participant_manual_liveliness_count;
  uint32_t participant_builtin_endpoints;
  /* ddsi_content_filter_property_t content_filter_property; */
  ddsi_guid_t participant_guid;
  ddsi_guid_t endpoint_guid;
  ddsi_guid_t group_guid;
  ddsi_guid_t topic_guid;
#if 0 /* reserved, rather than NIY */
  ddsi_entityid_t participant_entityid;
  ddsi_entityid_t group_entityid;
#endif
  uint32_t builtin_endpoint_set;
  /* int type_max_size_serialized; */
  ddsi_keyhash_t keyhash;
  uint32_t statusinfo;
  ddsi_adlink_participant_version_info_t adlink_participant_version_info;
#ifdef DDS_HAS_SECURITY
  ddsi_token_t identity_token;
  ddsi_token_t permissions_token;
  ddsi_security_info_t endpoint_security_info;
  ddsi_security_info_t participant_security_info;
  ddsi_token_t identity_status_token;
  ddsi_datatags_t data_tags;
#endif
#ifdef DDS_HAS_SSM
  ddsi_reader_favours_ssm_t reader_favours_ssm;
#endif
  uint32_t domain_id;
  char *domain_tag;
  uint32_t cyclone_receive_buffer_size;
  unsigned char cyclone_requests_keyhash;
  unsigned char cyclone_redundant_networking;
} ddsi_plist_t;

/**
 * @brief Initialize a ddsi_plist_t as an empty object
 * @component parameter_list
 *
 * In principle, this only clears the "present" and "aliased" bitmasks.  A debug build
 * additionally initializes all other bytes to 0x55.
 *
 * @param[out] dest  plist_t to be initialized.
 */
void ddsi_plist_init_empty (ddsi_plist_t *dest);

/**
 * @brief Free memory owned by "ps"
 * @component parameter_list
 *
 * A ddsi_plist_t may own other allocated blocks of memory, depending on which fields are
 * set, their types and whether they are marked as "aliased".  This function releases any
 * such memory owned by "ps", but not "ps" itself.  Afterward, the contents of "ps" is
 * undefined and must not be used again without initialising it (either via
 * `ddsi_plist_init_empty`, `ddsi_plist_init_frommsg` or `ddsi_plist_copy`.
 *
 * @param[in] ps   ddsi_plist_t for which to free memory
 */
void ddsi_plist_fini (ddsi_plist_t *ps);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_PLIST_H */
