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
#ifndef DDSI_PLIST_H
#define DDSI_PLIST_H

#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_tran.h" /* FIXME: eliminate */

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
#define PP_ADLINK_PARTICIPANT_VERSION_INFO      ((uint64_t)1 << 26)
#define PP_ADLINK_TYPE_DESCRIPTION              ((uint64_t)1 << 27)
#define PP_COHERENT_SET                         ((uint64_t)1 << 28)
#ifdef DDSI_INCLUDE_SSM
#define PP_READER_FAVOURS_SSM                   ((uint64_t)1 << 29)
#endif
/* Security extensions. */
#define PP_IDENTITY_TOKEN                       ((uint64_t)1 << 30)
#define PP_PERMISSIONS_TOKEN                    ((uint64_t)1 << 31)
#define PP_DOMAIN_ID                            ((uint64_t)1 << 32)
#define PP_DOMAIN_TAG                           ((uint64_t)1 << 33)
/* Set for unrecognized parameters that are in the reserved space or
   in our own vendor-specific space that have the
   PID_UNRECOGNIZED_INCOMPATIBLE_FLAG set (see DDSI 2.1 9.6.2.2.1) */
#define PP_INCOMPATIBLE                         ((uint64_t)1 << 63)

#define NN_ADLINK_PARTICIPANT_VERSION_INFO_FIXED_CDRSIZE (24)

#define NN_ADLINK_FL_KERNEL_SEQUENCE_NUMBER     (1u << 0)
#define NN_ADLINK_FL_DISCOVERY_INCLUDES_GID     (1u << 1)
#define NN_ADLINK_FL_PTBES_FIXED_0              (1u << 2)
#define NN_ADLINK_FL_DDSI2_PARTICIPANT_FLAG     (1u << 3)
#define NN_ADLINK_FL_PARTICIPANT_IS_DDSI2       (1u << 4)
#define NN_ADLINK_FL_MINIMAL_BES_MODE           (1u << 5)
#define NN_ADLINK_FL_SUPPORTS_STATUSINFOX       (1u << 5)
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
  uint32_t n;
  struct nn_locators_one *first;
  struct nn_locators_one *last;
} nn_locators_t;

typedef uint32_t nn_ipv4address_t;

typedef uint32_t nn_port_t;

typedef struct nn_keyhash {
  unsigned char value[16];
} nn_keyhash_t;


#ifdef DDSI_INCLUDE_SSM
typedef struct nn_reader_favours_ssm {
  uint32_t state; /* default is false */
} nn_reader_favours_ssm_t;
#endif

typedef struct nn_adlink_participant_version_info
{
  uint32_t version;
  uint32_t flags;
  uint32_t unused[3];
  char *internals;
} nn_adlink_participant_version_info_t;

typedef struct ddsi_plist {
  uint64_t present;
  uint64_t aliased;

  dds_qos_t qos;

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
  uint32_t participant_builtin_endpoints;
  dds_duration_t participant_lease_duration;
  /* nn_content_filter_property_t content_filter_property; */
  ddsi_guid_t participant_guid;
  ddsi_guid_t endpoint_guid;
  ddsi_guid_t group_guid;
#if 0 /* reserved, rather than NIY */
  nn_entityid_t participant_entityid;
  nn_entityid_t group_entityid;
#endif
  uint32_t builtin_endpoint_set;
  /* int type_max_size_serialized; */
  char *entity_name;
  nn_keyhash_t keyhash;
  uint32_t statusinfo;
  nn_adlink_participant_version_info_t adlink_participant_version_info;
  char *type_description;
  nn_sequence_number_t coherent_set_seqno;
#ifdef DDSI_INCLUDE_SSM
  nn_reader_favours_ssm_t reader_favours_ssm;
#endif
  uint32_t domain_id;
  char *domain_tag;
} ddsi_plist_t;


/***/

typedef struct ddsi_plist_src {
  nn_protocol_version_t protocol_version;
  nn_vendorid_t vendorid;
  int encoding;
  const unsigned char *buf;
  size_t bufsz;
  bool strict;
  ddsi_tran_factory_t factory; /* eliminate this */
  struct ddsrt_log_cfg *logconfig;
} ddsi_plist_src_t;

void ddsi_plist_init_tables (void);
DDS_EXPORT void ddsi_plist_init_empty (ddsi_plist_t *dest);
DDS_EXPORT void ddsi_plist_mergein_missing (ddsi_plist_t *a, const ddsi_plist_t *b, uint64_t pmask, uint64_t qmask);
DDS_EXPORT void ddsi_plist_copy (ddsi_plist_t *dst, const ddsi_plist_t *src);
DDS_EXPORT ddsi_plist_t *ddsi_plist_dup (const ddsi_plist_t *src);

/**
 * @brief Initialize an ddsi_plist_t from a PL_CDR_{LE,BE} paylaod.
 *
 * @param[in]  pwanted
 *               PP_... flags indicating which non-QoS parameters are of interest, treated as
 *               a hint.  Parameters in the input that are outside the mask may or may not be
 *               ignored.
 * @param[in]  qwanted
 *               QP_... flags indicating which QoS settings are of interest, and is treated as
 *               a hint.  Parameters in the input that are outside the mask may or may not be
 *               ignored.
 * @param[in]  src
 *               Serialized payload to be parsed, validated and copied into dest
 *               - protocol_version is the version protocol version according to which the list
 *                 should be parsed
 *               - vendorid is the vendor id code for the source of the parameter list (for
 *                 handling vendor-specific parameters and compatibility workarounds)
 *               - encoding is PL_CDR_LE or PL_CDR_BE
 *               - buf is a pointer to the first parameter header
 *               - bufsz is the size in bytes of the input buffer
 * @param[out] dest
 *               Filled with the recognized parameters in the input if successful, otherwise
 *               initialized to an empty parameter list.  Where possible, pointers alias the
 *               input (indicated by the "aliased" bits in the plist/xqos structures), but
 *               some things cannot be aliased (e.g., the array of pointers to strings for a
 *               sequence of strings).
 *               Generally, ddsi_plist_fini should be called when done with the parameter list,
 *               even when ddsi_plist_unlias or ddsi_xqos_unlias hasn't been called.
 * @param[out] nextafterplist
 *               If non-NULL, *nextafterplist is set to the first byte following the parameter
 *               list sentinel on successful parse, or to NULL on failure
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *               All parameters valid (or ignored), dest and *nextafterplist have been set
 *               accordingly.
 * @retval DDS_INCONSISTENT_POLICY
 *               All individual settings are valid, but there are inconsistencies between
 *               dependent settings.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *               Input contained invalid data; dest is cleared, *nextafterplist is NULL.
 * @retval DDS_RETCODE_UNSUPPORTED
 *               Input contained an unrecognized parameter with the "incompatible-if-unknown"
 *               flag set; dest is cleared, *nextafterplist is NULL.
 */
DDS_EXPORT dds_return_t ddsi_plist_init_frommsg (ddsi_plist_t *dest, char **nextafterplist, uint64_t pwanted, uint64_t qwanted, const ddsi_plist_src_t *src);
DDS_EXPORT void ddsi_plist_fini (ddsi_plist_t *ps);
DDS_EXPORT void ddsi_plist_fini_mask (ddsi_plist_t *plist, uint64_t pmask, uint64_t qmask);
DDS_EXPORT void ddsi_plist_unalias (ddsi_plist_t *plist);
DDS_EXPORT void ddsi_plist_addtomsg (struct nn_xmsg *m, const ddsi_plist_t *ps, uint64_t pwanted, uint64_t qwanted);
DDS_EXPORT void ddsi_plist_init_default_participant (ddsi_plist_t *plist);
DDS_EXPORT void ddsi_plist_delta (uint64_t *pdelta, uint64_t *qdelta, const ddsi_plist_t *x, const ddsi_plist_t *y, uint64_t pmask, uint64_t qmask);
DDS_EXPORT void ddsi_plist_log (uint32_t cat, const struct ddsrt_log_cfg *logcfg, const ddsi_plist_t *plist);
DDS_EXPORT size_t ddsi_plist_print (char * __restrict buf, size_t bufsize, const ddsi_plist_t *plist);

struct nn_rmsg;
struct nn_rsample_info;
struct nn_rdata;

DDS_EXPORT unsigned char *ddsi_plist_quickscan (struct nn_rsample_info *dest, const struct nn_rmsg *rmsg, const ddsi_plist_src_t *src);
DDS_EXPORT const unsigned char *ddsi_plist_findparam_native_unchecked (const void *src, nn_parameterid_t pid);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_PLIST_H */
