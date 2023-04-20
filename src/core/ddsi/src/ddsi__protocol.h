// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PROTOCOL_H
#define DDSI__PROTOCOL_H

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsi/ddsi_feature_check.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "ddsi__time.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSI_GUID_PREFIX_UNKNOWN_INITIALIZER {{0,0,0,0, 0,0,0,0, 0,0,0,0}}

#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_ANNOUNCER (1u << 0)
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_DETECTOR (1u << 1)
#define DDSI_DISC_BUILTIN_ENDPOINT_PUBLICATION_ANNOUNCER (1u << 2)
#define DDSI_DISC_BUILTIN_ENDPOINT_PUBLICATION_DETECTOR (1u << 3)
#define DDSI_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER (1u << 4)
#define DDSI_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_DETECTOR (1u << 5)
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_ANNOUNCER (1u << 6) /* undefined meaning */
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_PROXY_DETECTOR (1u << 7) /* undefined meaning */
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_ANNOUNCER (1u << 8) /* undefined meaning */
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_STATE_DETECTOR (1u << 9) /* undefined meaning */
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER (1u << 10)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_READER (1u << 11)
#define DDSI_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_WRITER (1u << 12)
#define DDSI_BUILTIN_ENDPOINT_TL_SVC_REQUEST_DATA_READER (1u << 13)
#define DDSI_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_WRITER (1u << 14)
#define DDSI_BUILTIN_ENDPOINT_TL_SVC_REPLY_DATA_READER (1u << 15)
/* Security extensions */
#define DDSI_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_ANNOUNCER (1u<<16)
#define DDSI_BUILTIN_ENDPOINT_PUBLICATION_MESSAGE_SECURE_DETECTOR (1u<<17)
#define DDSI_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_ANNOUNCER (1u<<18)
#define DDSI_BUILTIN_ENDPOINT_SUBSCRIPTION_MESSAGE_SECURE_DETECTOR (1u<<19)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_ANNOUNCER (1u<<20)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_SECURE_DETECTOR (1u<<21)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_ANNOUNCER (1u<<22)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_STATELESS_MESSAGE_DETECTOR (1u<<23)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_ANNOUNCER (1u<<24)
#define DDSI_BUILTIN_ENDPOINT_PARTICIPANT_VOLATILE_SECURE_DETECTOR (1u<<25)
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_ANNOUNCER (1u << 26)
#define DDSI_DISC_BUILTIN_ENDPOINT_PARTICIPANT_SECURE_DETECTOR (1u << 27)
/* End security extensions */
#define DDSI_DISC_BUILTIN_ENDPOINT_TOPICS_ANNOUNCER (1u << 28)
#define DDSI_DISC_BUILTIN_ENDPOINT_TOPICS_DETECTOR (1u << 29)

#define DDSI_BES_MASK_NON_SECURITY 0xf000ffffu

/* Only one specific version is grokked */
#define DDSI_RTPS_MAJOR 2
#define DDSI_RTPS_MINOR 1
#define DDSI_RTPS_MINOR_MINIMUM 1

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define DDSI_PROTOCOLID_AS_UINT32 (((uint32_t)'R' << 0) | ((uint32_t)'T' << 8) | ((uint32_t)'P' << 16) | ((uint32_t)'S' << 24))
#elif DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
#define DDSI_PROTOCOLID_AS_UINT32 (((uint32_t)'R' << 24) | ((uint32_t)'T' << 16) | ((uint32_t)'P' << 8) | ((uint32_t)'S' << 0))
#else
#error "DDSRT_ENDIAN neither LITTLE nor BIG"
#endif

typedef uint16_t ddsi_parameterid_t; /* spec says short */
typedef struct ddsi_parameter {
  ddsi_parameterid_t parameterid;
  uint16_t length; /* spec says signed short */
  /* char value[] */
} ddsi_parameter_t;

typedef struct ddsi_sequence_number {
  int32_t high;
  uint32_t low;
} ddsi_sequence_number_t;

#define DDSI_SEQUENCE_NUMBER_UNKNOWN_HIGH -1
#define DDSI_SEQUENCE_NUMBER_UNKNOWN_LOW 0
#define DDSI_SEQUENCE_NUMBER_UNKNOWN ((ddsi_seqno_t) (((uint64_t)DDSI_SEQUENCE_NUMBER_UNKNOWN_HIGH << 32) | DDSI_SEQUENCE_NUMBER_UNKNOWN_LOW))

/* C99 disallows flex array in nested struct, so only put the
   header in.  (GCC and Clang allow it, but there are other
   compilers in the world as well.) */

typedef struct ddsi_sequence_number_set_header {
  ddsi_sequence_number_t bitmap_base;
  uint32_t numbits;
} ddsi_sequence_number_set_header_t;

/* SequenceNumberSet size is base (2 words) + numbits (1 word) +
   bitmap ((numbits+31)/32 words), and this at 4 bytes/word */
#define DDSI_SEQUENCE_NUMBER_SET_MAX_BITS (256u)
#define DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE(numbits) ((unsigned) (4 * (((numbits) + 31) / 32)))
#define DDSI_SEQUENCE_NUMBER_SET_SIZE(numbits) (sizeof (ddsi_sequence_number_set_header_t) + DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (numbits))

typedef uint32_t ddsi_fragment_number_t;
typedef struct ddsi_fragment_number_set_header {
  ddsi_fragment_number_t bitmap_base;
  uint32_t numbits;
} ddsi_fragment_number_set_header_t;

/* FragmentNumberSet size is base (2 words) + numbits (1 word) +
   bitmap ((numbits+31)/32 words), and this at 4 bytes/word */
#define DDSI_FRAGMENT_NUMBER_SET_MAX_BITS (256u)
#define DDSI_FRAGMENT_NUMBER_SET_BITS_SIZE(numbits) ((unsigned) (4 * (((numbits) + 31) / 32)))
#define DDSI_FRAGMENT_NUMBER_SET_SIZE(numbits) (sizeof (ddsi_fragment_number_set_header_t) + DDSI_FRAGMENT_NUMBER_SET_BITS_SIZE (numbits))


typedef struct ddsi_rtps_entityid {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_entityid_t entityid;
} ddsi_rtps_entityid_t;

typedef struct ddsi_rtps_info_dst {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_guid_prefix_t guid_prefix;
} ddsi_rtps_info_dst_t;

typedef struct ddsi_rtps_data_datafrag_common {
  ddsi_rtps_submessage_header_t smhdr;
  uint16_t extraFlags;
  uint16_t octetsToInlineQos;
  ddsi_entityid_t readerId;
  ddsi_entityid_t writerId;
  ddsi_sequence_number_t writerSN;
} ddsi_rtps_data_datafrag_common_t;

typedef struct ddsi_rtps_data {
  ddsi_rtps_data_datafrag_common_t x;
} ddsi_rtps_data_t;
#define DDSI_DATA_FLAG_INLINE_QOS 0x02u
#define DDSI_DATA_FLAG_DATAFLAG 0x04u
#define DDSI_DATA_FLAG_KEYFLAG 0x08u

typedef struct ddsi_rtps_datafrag {
  ddsi_rtps_data_datafrag_common_t x;
  ddsi_fragment_number_t fragmentStartingNum;
  uint16_t fragmentsInSubmessage;
  uint16_t fragmentSize;
  uint32_t sampleSize;
} ddsi_rtps_datafrag_t;
#define DDSI_DATAFRAG_FLAG_INLINE_QOS 0x02u
#define DDSI_DATAFRAG_FLAG_KEYFLAG 0x04u

DDSRT_WARNING_MSVC_OFF(4200)
typedef struct ddsi_rtps_acknack {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_entityid_t readerId;
  ddsi_entityid_t writerId;
  ddsi_sequence_number_set_header_t readerSNState;
  uint32_t bits[];
  /* ddsi_count_t count; */
} ddsi_rtps_acknack_t;
DDSRT_WARNING_MSVC_ON(4200)
#define DDSI_ACKNACK_FLAG_FINAL 0x02u
#define DDSI_ACKNACK_SIZE(numbits) (offsetof (ddsi_rtps_acknack_t, bits) + DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (numbits) + 4)
#define DDSI_ACKNACK_SIZE_MAX DDSI_ACKNACK_SIZE (DDSI_SEQUENCE_NUMBER_SET_MAX_BITS)

DDSRT_WARNING_MSVC_OFF(4200)
typedef struct ddsi_rtps_gap {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_entityid_t readerId;
  ddsi_entityid_t writerId;
  ddsi_sequence_number_t gapStart;
  ddsi_sequence_number_set_header_t gapList;
  uint32_t bits[];
} ddsi_rtps_gap_t;
DDSRT_WARNING_MSVC_ON(4200)
#define DDSI_GAP_SIZE(numbits) (offsetof (ddsi_rtps_gap_t, bits) + DDSI_SEQUENCE_NUMBER_SET_BITS_SIZE (numbits))
#define DDSI_GAP_SIZE_MAX DDSI_GAP_SIZE (DDSI_SEQUENCE_NUMBER_SET_MAX_BITS)

typedef struct ddsi_rtps_info_ts {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_time_t time;
} ddsi_rtps_info_ts_t;
#define DDSI_INFOTS_INVALIDATE_FLAG 0x2u

typedef struct ddsi_rtps_heartbeat {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_entityid_t readerId;
  ddsi_entityid_t writerId;
  ddsi_sequence_number_t firstSN;
  ddsi_sequence_number_t lastSN;
  ddsi_count_t count;
} ddsi_rtps_heartbeat_t;
#define DDSI_HEARTBEAT_FLAG_FINAL 0x02u
#define DDSI_HEARTBEAT_FLAG_LIVELINESS 0x04u

typedef struct ddsi_rtps_heartbeatfrag {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_entityid_t readerId;
  ddsi_entityid_t writerId;
  ddsi_sequence_number_t writerSN;
  ddsi_fragment_number_t lastFragmentNum;
  ddsi_count_t count;
} ddsi_rtps_heartbeatfrag_t;

DDSRT_WARNING_MSVC_OFF(4200)
typedef struct ddsi_rtps_nackfrag {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_entityid_t readerId;
  ddsi_entityid_t writerId;
  ddsi_sequence_number_t writerSN;
  ddsi_fragment_number_set_header_t fragmentNumberState;
  uint32_t bits[];
  /* ddsi_count_t count; */
} ddsi_rtps_nackfrag_t;
DDSRT_WARNING_MSVC_ON(4200)
#define DDSI_NACKFRAG_SIZE(numbits) (offsetof (ddsi_rtps_nackfrag_t, bits) + DDSI_FRAGMENT_NUMBER_SET_BITS_SIZE (numbits) + 4)
#define DDSI_NACKFRAG_SIZE_MAX DDSI_NACKFRAG_SIZE (DDSI_FRAGMENT_NUMBER_SET_MAX_BITS)

typedef union ddsi_rtps_submessage {
  ddsi_rtps_submessage_header_t smhdr;
  ddsi_rtps_acknack_t acknack;
  ddsi_rtps_data_t data;
  ddsi_rtps_datafrag_t datafrag;
  ddsi_rtps_info_ts_t infots;
  ddsi_rtps_info_dst_t infodst;
  ddsi_rtps_info_src_t infosrc;
  ddsi_rtps_heartbeat_t heartbeat;
  ddsi_rtps_heartbeatfrag_t heartbeatfrag;
  ddsi_rtps_gap_t gap;
  ddsi_rtps_nackfrag_t nackfrag;
} ddsi_rtps_submessage_t;


#define DDSI_PID_VENDORSPECIFIC_FLAG 0x8000u
#define DDSI_PID_UNRECOGNIZED_INCOMPATIBLE_FLAG 0x4000u

#define DDSI_PID_PAD                                 0x0u
#define DDSI_PID_SENTINEL                            0x1u
#define DDSI_PID_DOMAIN_ID                           0xfu
#define DDSI_PID_DOMAIN_TAG                          (0x14u | DDSI_PID_UNRECOGNIZED_INCOMPATIBLE_FLAG)
#define DDSI_PID_USER_DATA                           0x2cu
#define DDSI_PID_TOPIC_NAME                          0x5u
#define DDSI_PID_TYPE_NAME                           0x7u
#define DDSI_PID_GROUP_DATA                          0x2du
#define DDSI_PID_TOPIC_DATA                          0x2eu
#define DDSI_PID_DURABILITY                          0x1du
#define DDSI_PID_DURABILITY_SERVICE                  0x1eu
#define DDSI_PID_DEADLINE                            0x23u
#define DDSI_PID_LATENCY_BUDGET                      0x27u
#define DDSI_PID_LIVELINESS                          0x1bu
#define DDSI_PID_RELIABILITY                         0x1au
#define DDSI_PID_LIFESPAN                            0x2bu
#define DDSI_PID_DESTINATION_ORDER                   0x25u
#define DDSI_PID_HISTORY                             0x40u
#define DDSI_PID_RESOURCE_LIMITS                     0x41u
#define DDSI_PID_OWNERSHIP                           0x1fu
#define DDSI_PID_OWNERSHIP_STRENGTH                  0x6u
#define DDSI_PID_PRESENTATION                        0x21u
#define DDSI_PID_PARTITION                           0x29u
#define DDSI_PID_TIME_BASED_FILTER                   0x4u
#define DDSI_PID_TRANSPORT_PRIORITY                  0x49u
#define DDSI_PID_PROTOCOL_VERSION                    0x15u
#define DDSI_PID_VENDORID                            0x16u
#define DDSI_PID_UNICAST_LOCATOR                     0x2fu
#define DDSI_PID_MULTICAST_LOCATOR                   0x30u
#define DDSI_PID_MULTICAST_IPADDRESS                 0x11u
#define DDSI_PID_DEFAULT_UNICAST_LOCATOR             0x31u
#define DDSI_PID_DEFAULT_MULTICAST_LOCATOR           0x48u
#define DDSI_PID_METATRAFFIC_UNICAST_LOCATOR         0x32u
#define DDSI_PID_METATRAFFIC_MULTICAST_LOCATOR       0x33u
#define DDSI_PID_DEFAULT_UNICAST_IPADDRESS           0xcu
#define DDSI_PID_DEFAULT_UNICAST_PORT                0xeu
#define DDSI_PID_METATRAFFIC_UNICAST_IPADDRESS       0x45u
#define DDSI_PID_METATRAFFIC_UNICAST_PORT            0xdu
#define DDSI_PID_METATRAFFIC_MULTICAST_IPADDRESS     0xbu
#define DDSI_PID_METATRAFFIC_MULTICAST_PORT          0x46u
#define DDSI_PID_EXPECTS_INLINE_QOS                  0x43u
#define DDSI_PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT 0x34u
#define DDSI_PID_PARTICIPANT_BUILTIN_ENDPOINTS       0x44u
#define DDSI_PID_PARTICIPANT_LEASE_DURATION          0x2u
#define DDSI_PID_CONTENT_FILTER_PROPERTY             0x35u
#define DDSI_PID_PARTICIPANT_GUID                    0x50u
#define DDSI_PID_PARTICIPANT_ENTITYID                0x51u
#define DDSI_PID_GROUP_GUID                          0x52u
#define DDSI_PID_GROUP_ENTITYID                      0x53u
#define DDSI_PID_BUILTIN_ENDPOINT_SET                0x58u
#define DDSI_PID_PROPERTY_LIST                       0x59u
#define DDSI_PID_TYPE_MAX_SIZE_SERIALIZED            0x60u
#define DDSI_PID_ENTITY_NAME                         0x62u
#define DDSI_PID_KEYHASH                             0x70u
#define DDSI_PID_STATUSINFO                          0x71u
#define DDSI_PID_CONTENT_FILTER_INFO                 0x55u
#define DDSI_PID_COHERENT_SET                        0x56u
#define DDSI_PID_DIRECTED_WRITE                      0x57u
#define DDSI_PID_ORIGINAL_WRITER_INFO                0x61u
#define DDSI_PID_ENDPOINT_GUID                       0x5au /* !SPEC <=> ADLINK_ENDPOINT_GUID */
#define DDSI_PID_DATA_REPRESENTATION                 0x73u
#define DDSI_PID_TYPE_CONSISTENCY_ENFORCEMENT        0x74u
#define DDSI_PID_TYPE_INFORMATION                    0x75u

/* Security related PID values. */
#define DDSI_PID_IDENTITY_TOKEN                      0x1001u
#define DDSI_PID_PERMISSIONS_TOKEN                   0x1002u
#define DDSI_PID_DATA_TAGS                           0x1003u
#define DDSI_PID_ENDPOINT_SECURITY_INFO              0x1004u
#define DDSI_PID_PARTICIPANT_SECURITY_INFO           0x1005u
#define DDSI_PID_IDENTITY_STATUS_TOKEN               0x1006u

#ifdef DDS_HAS_SSM
/* To indicate whether a reader favours the use of SSM.  Iff the
   reader favours SSM, it will use SSM if available. */
#define DDSI_PID_READER_FAVOURS_SSM                  0x72u
#endif

/* Deprecated parameter IDs (accepted but ignored) */
#define DDSI_PID_PERSISTENCE                         0x03u
#define DDSI_PID_TYPE_CHECKSUM                       0x08u
#define DDSI_PID_TYPE2_NAME                          0x09u
#define DDSI_PID_TYPE2_CHECKSUM                      0x0au
#define DDSI_PID_EXPECTS_ACK                         0x10u
#define DDSI_PID_MANAGER_KEY                         0x12u
#define DDSI_PID_SEND_QUEUE_SIZE                     0x13u
#define DDSI_PID_RELIABILITY_ENABLED                 0x14u
#define DDSI_PID_VARGAPPS_SEQUENCE_NUMBER_LAST       0x17u
#define DDSI_PID_RECV_QUEUE_SIZE                     0x18u
#define DDSI_PID_RELIABILITY_OFFERED                 0x19u

#define DDSI_PID_ADLINK_BUILTIN_ENDPOINT_SET         (DDSI_PID_VENDORSPECIFIC_FLAG | 0x0u)

/* parameter ids for READER_DATA_LIFECYCLE & WRITER_DATA_LIFECYCLE are
   undefined, but let's publish them anyway */
#define DDSI_PID_ADLINK_READER_DATA_LIFECYCLE        (DDSI_PID_VENDORSPECIFIC_FLAG | 0x2u)
#define DDSI_PID_ADLINK_WRITER_DATA_LIFECYCLE        (DDSI_PID_VENDORSPECIFIC_FLAG | 0x3u)

/* ENDPOINT_GUID is formally undefined, so in strictly conforming
   mode, we use our own */
#define DDSI_PID_ADLINK_ENDPOINT_GUID                (DDSI_PID_VENDORSPECIFIC_FLAG | 0x4u)

#define DDSI_PID_ADLINK_SYNCHRONOUS_ENDPOINT         (DDSI_PID_VENDORSPECIFIC_FLAG | 0x5u)

/* Relaxed QoS matching readers/writers are best ignored by
   implementations that don't understand them.  This also covers "old"
   DDSI2's, although they may emit an error. */
#define DDSI_PID_ADLINK_RELAXED_QOS_MATCHING         (DDSI_PID_VENDORSPECIFIC_FLAG | DDSI_PID_UNRECOGNIZED_INCOMPATIBLE_FLAG | 0x6u)

#define DDSI_PID_ADLINK_PARTICIPANT_VERSION_INFO     (DDSI_PID_VENDORSPECIFIC_FLAG | 0x7u)

#define DDSI_PID_ADLINK_NODE_NAME                    (DDSI_PID_VENDORSPECIFIC_FLAG | 0x8u)
#define DDSI_PID_ADLINK_EXEC_NAME                    (DDSI_PID_VENDORSPECIFIC_FLAG | 0x9u)
#define DDSI_PID_ADLINK_PROCESS_ID                   (DDSI_PID_VENDORSPECIFIC_FLAG | 0xau)
#define DDSI_PID_ADLINK_SERVICE_TYPE                 (DDSI_PID_VENDORSPECIFIC_FLAG | 0xbu)
#define DDSI_PID_ADLINK_ENTITY_FACTORY               (DDSI_PID_VENDORSPECIFIC_FLAG | 0xcu)
#define DDSI_PID_ADLINK_WATCHDOG_SCHEDULING          (DDSI_PID_VENDORSPECIFIC_FLAG | 0xdu)
#define DDSI_PID_ADLINK_LISTENER_SCHEDULING          (DDSI_PID_VENDORSPECIFIC_FLAG | 0xeu)
#define DDSI_PID_ADLINK_SUBSCRIPTION_KEYS            (DDSI_PID_VENDORSPECIFIC_FLAG | 0xfu)
#define DDSI_PID_ADLINK_READER_LIFESPAN              (DDSI_PID_VENDORSPECIFIC_FLAG | 0x10u)
#define DDSI_PID_ADLINK_TYPE_DESCRIPTION             (DDSI_PID_VENDORSPECIFIC_FLAG | 0x12u)
#define DDSI_PID_ADLINK_LAN                          (DDSI_PID_VENDORSPECIFIC_FLAG | 0x13u)
#define DDSI_PID_ADLINK_ENDPOINT_GID                 (DDSI_PID_VENDORSPECIFIC_FLAG | 0x14u)
#define DDSI_PID_ADLINK_GROUP_GID                    (DDSI_PID_VENDORSPECIFIC_FLAG | 0x15u)
#define DDSI_PID_ADLINK_EOTINFO                      (DDSI_PID_VENDORSPECIFIC_FLAG | 0x16u)
#define DDSI_PID_ADLINK_PART_CERT_NAME               (DDSI_PID_VENDORSPECIFIC_FLAG | 0x17u)
#define DDSI_PID_ADLINK_LAN_CERT_NAME                (DDSI_PID_VENDORSPECIFIC_FLAG | 0x18u)
#define DDSI_PID_CYCLONE_RECEIVE_BUFFER_SIZE         (DDSI_PID_VENDORSPECIFIC_FLAG | 0x19u)
#define DDSI_PID_CYCLONE_TYPE_INFORMATION            (DDSI_PID_VENDORSPECIFIC_FLAG | 0x1au)
#define DDSI_PID_CYCLONE_TOPIC_GUID                  (DDSI_PID_VENDORSPECIFIC_FLAG | 0x1bu)
#define DDSI_PID_CYCLONE_REQUESTS_KEYHASH            (DDSI_PID_VENDORSPECIFIC_FLAG | 0x1cu)
#define DDSI_PID_CYCLONE_REDUNDANT_NETWORKING        (DDSI_PID_VENDORSPECIFIC_FLAG | 0x1du)


#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PROTOCOL_H */
