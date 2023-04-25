// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PROTOCOL_H
#define DDSI_PROTOCOL_H

#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsi/ddsi_feature_check.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_guid.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSI_STATUSINFO_DISPOSE      0x1u
#define DDSI_STATUSINFO_UNREGISTER   0x2u
#define DDSI_STATUSINFO_STANDARDIZED (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER)
#define DDSI_STATUSINFO_OSPL_AUTO    0x10000000u /* OSPL extension, not on the wire */
#define DDSI_STATUSINFOX_OSPL_AUTO   0x1         /* statusinfo word 2, OSPL L_AUTO flag on the wire */

#define DDSI_LOCATOR_KIND_INVALID -1
#define DDSI_LOCATOR_KIND_RESERVED 0
#define DDSI_LOCATOR_KIND_UDPv4 1
#define DDSI_LOCATOR_KIND_UDPv6 2
#define DDSI_LOCATOR_KIND_TCPv4 4
#define DDSI_LOCATOR_KIND_TCPv6 8
#define DDSI_LOCATOR_KIND_SHEM 16
#define DDSI_LOCATOR_KIND_RAWETH 0x8000 /* proposed vendor-specific */
#define DDSI_LOCATOR_KIND_UDPv4MCGEN 0x4fff0000
#define DDSI_LOCATOR_PORT_INVALID 0

typedef uint32_t ddsi_count_t;

typedef struct ddsi_protocol_version {
  uint8_t major, minor;
} ddsi_protocol_version_t;

typedef uint64_t ddsi_seqno_t;
#define DDSI_MAX_SEQ_NUMBER INT64_MAX

#define PGUIDPREFIX(gp) (gp).u[0], (gp).u[1], (gp).u[2]
#define PGUID(g) PGUIDPREFIX ((g).prefix), (g).entityid.u
#define PGUIDPREFIXFMT "%" PRIx32 ":%" PRIx32 ":%" PRIx32
#define PGUIDFMT PGUIDPREFIXFMT ":%" PRIx32

typedef struct {
  uint8_t id[2];
} ddsi_vendorid_t;

typedef struct ddsi_protocolid {
  uint8_t id[4];
} ddsi_protocolid_t;

typedef struct ddsi_rtps_header {
  ddsi_protocolid_t protocol;
  ddsi_protocol_version_t version;
  ddsi_vendorid_t vendorid;
  ddsi_guid_prefix_t guid_prefix;
} ddsi_rtps_header_t;
#define DDSI_RTPS_MESSAGE_HEADER_SIZE (sizeof (ddsi_rtps_header_t))

typedef struct ddsi_rtps_submessage_header {
  uint8_t submessageId;
  uint8_t flags;
  uint16_t octetsToNextHeader;
} ddsi_rtps_submessage_header_t;
#define DDSI_RTPS_SUBMESSAGE_HEADER_SIZE (sizeof (ddsi_rtps_submessage_header_t))
#define DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS 0x01u

typedef enum ddsi_rtps_submessage_kind {
  DDSI_RTPS_SMID_PAD = 0x01,
  DDSI_RTPS_SMID_ACKNACK = 0x06,
  DDSI_RTPS_SMID_HEARTBEAT = 0x07,
  DDSI_RTPS_SMID_GAP = 0x08,
  DDSI_RTPS_SMID_INFO_TS = 0x09,
  DDSI_RTPS_SMID_INFO_SRC = 0x0c,
  DDSI_RTPS_SMID_INFO_REPLY_IP4 = 0x0d,
  DDSI_RTPS_SMID_INFO_DST = 0x0e,
  DDSI_RTPS_SMID_INFO_REPLY = 0x0f,
  DDSI_RTPS_SMID_NACK_FRAG = 0x12,
  DDSI_RTPS_SMID_HEARTBEAT_FRAG = 0x13,
  DDSI_RTPS_SMID_DATA = 0x15,
  DDSI_RTPS_SMID_DATA_FRAG = 0x16,
  /* security-specific sub messages */
  DDSI_RTPS_SMID_SEC_BODY = 0x30,
  DDSI_RTPS_SMID_SEC_PREFIX = 0x31,
  DDSI_RTPS_SMID_SEC_POSTFIX = 0x32,
  DDSI_RTPS_SMID_SRTPS_PREFIX = 0x33,
  DDSI_RTPS_SMID_SRTPS_POSTFIX = 0x34,
  /* vendor-specific sub messages (0x80 .. 0xff) */
  DDSI_RTPS_SMID_ADLINK_MSG_LEN = 0x81,
  DDSI_RTPS_SMID_ADLINK_ENTITY_ID = 0x82
} ddsi_rtps_submessage_kind_t;

typedef struct ddsi_rtps_info_src {
  ddsi_rtps_submessage_header_t smhdr;
  unsigned unused;
  ddsi_protocol_version_t version;
  ddsi_vendorid_t vendorid;
  ddsi_guid_prefix_t guid_prefix;
} ddsi_rtps_info_src_t;

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN

#define DDSI_RTPS_CDR_BE      0x0000u
#define DDSI_RTPS_CDR_LE      0x0100u
#define DDSI_RTPS_PL_CDR_BE   0x0200u
#define DDSI_RTPS_PL_CDR_LE   0x0300u
#define DDSI_RTPS_CDR2_BE     0x0600u
#define DDSI_RTPS_CDR2_LE     0x0700u
#define DDSI_RTPS_D_CDR2_BE   0x0800u
#define DDSI_RTPS_D_CDR2_LE   0x0900u
#define DDSI_RTPS_PL_CDR2_BE  0x0a00u
#define DDSI_RTPS_PL_CDR2_LE  0x0b00u

#define DDSI_RTPS_CDR_ENC_LE(x) (((x) & 0x0100) == 0x0100)
#define DDSI_RTPS_CDR_ENC_IS_NATIVE(x) (DDSI_RTPS_CDR_ENC_LE ((x)))
#define DDSI_RTPS_CDR_ENC_IS_VALID(x) (!((x) > DDSI_RTPS_PL_CDR2_LE || (x) == 0x0400 || (x) == 0x0500))
#define DDSI_RTPS_CDR_ENC_TO_NATIVE(x) ((x) | 0x0100)

#else

#define DDSI_RTPS_CDR_BE      0x0000u
#define DDSI_RTPS_CDR_LE      0x0001u
#define DDSI_RTPS_PL_CDR_BE   0x0002u
#define DDSI_RTPS_PL_CDR_LE   0x0003u
#define DDSI_RTPS_CDR2_BE     0x0006u
#define DDSI_RTPS_CDR2_LE     0x0007u
#define DDSI_RTPS_D_CDR2_BE   0x0008u
#define DDSI_RTPS_D_CDR2_LE   0x0009u
#define DDSI_RTPS_PL_CDR2_BE  0x000au
#define DDSI_RTPS_PL_CDR2_LE  0x000bu

#define DDSI_RTPS_CDR_ENC_LE(x) (((x) & 0x0001) == 0x0001)
#define DDSI_RTPS_CDR_ENC_IS_NATIVE(x) (!DDSI_RTPS_CDR_ENC_LE ((x)))
#define DDSI_RTPS_CDR_ENC_IS_VALID(x) (!((x) > DDSI_RTPS_PL_CDR2_LE || (x) == 0x0004 || (x) == 0x0005))
#define DDSI_RTPS_CDR_ENC_TO_NATIVE(x) ((x) & ~0x0001)

#endif

typedef struct ddsi_rtps_msg_len {
  ddsi_rtps_submessage_header_t smhdr;
  uint32_t length;
} ddsi_rtps_msg_len_t;

#define DDSI_PARTICIPANT_MESSAGE_DATA_KIND_UNKNOWN 0x0u
#define DDSI_PARTICIPANT_MESSAGE_DATA_KIND_AUTOMATIC_LIVELINESS_UPDATE 0x1u
#define DDSI_PARTICIPANT_MESSAGE_DATA_KIND_MANUAL_LIVELINESS_UPDATE 0x2u
#define DDSI_PARTICIPANT_MESSAGE_DATA_VENDOR_SPECIFIC_KIND_FLAG 0x8000000u

/* predefined entity ids; here viewed as an unsigned int, on the
   network as four bytes corresponding to the integer in network byte
   order */
#define DDSI_ENTITYID_UNKNOWN 0x0
#define DDSI_ENTITYID_PARTICIPANT 0x1c1
#define DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_WRITER 0x2c2
#define DDSI_ENTITYID_SEDP_BUILTIN_TOPIC_READER 0x2c7
#define DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER 0x3c2
#define DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_READER 0x3c7
#define DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER 0x4c2
#define DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER 0x4c7
#define DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER 0x100c2
#define DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_READER 0x100c7
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER 0x200c2
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER 0x200c7

#define DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER 0x300c3
#define DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_READER 0x300c4
#define DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER 0x301c3
#define DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_READER 0x301c4

#define DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER 0xff0003c2
#define DDSI_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER 0xff0003c7
#define DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER 0xff0004c2
#define DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER 0xff0004c7
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER 0x201c3
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER 0x201c4
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER 0xff0200c2
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER 0xff0200c7
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER 0xff0202c3
#define DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER 0xff0202c4
#define DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER 0xff0101c2
#define DDSI_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER 0xff0101c7

#define DDSI_ENTITYID_SOURCE_MASK 0xc0
#define DDSI_ENTITYID_SOURCE_USER 0x00
#define DDSI_ENTITYID_SOURCE_BUILTIN 0xc0
#define DDSI_ENTITYID_SOURCE_VENDOR 0x40
#define DDSI_ENTITYID_KIND_MASK 0x3f
#define DDSI_ENTITYID_KIND_WRITER_WITH_KEY 0x02
#define DDSI_ENTITYID_KIND_WRITER_NO_KEY 0x03
#define DDSI_ENTITYID_KIND_READER_NO_KEY 0x04
#define DDSI_ENTITYID_KIND_READER_WITH_KEY 0x07
/* Entity kind topic is not defined in the RTPS spec, so the following two
   should to be used as vendor specific entities using DDSI_ENTITYID_SOURCE_VENDOR.
   Two entity kinds for built-in and user topics are required, because the
   vendor and built-in flags cannot be combined. */
#define DDSI_ENTITYID_KIND_CYCLONE_TOPIC_BUILTIN 0x0c
#define DDSI_ENTITYID_KIND_CYCLONE_TOPIC_USER 0x0d

#define DDSI_ENTITYID_ALLOCSTEP 0x100


/* Names of the built-in topics */
#define DDS_BUILTIN_TOPIC_PARTICIPANT_NAME "DCPSParticipant"
#define DDS_BUILTIN_TOPIC_PUBLICATION_NAME "DCPSPublication"
#define DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME "DCPSSubscription"
#define DDS_BUILTIN_TOPIC_TOPIC_NAME "DCPSTopic"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_NAME "DCPSParticipantMessage"
#define DDS_BUILTIN_TOPIC_TYPELOOKUP_REQUEST_NAME "DCPSTypeLookupRequest"
#define DDS_BUILTIN_TOPIC_TYPELOOKUP_REPLY_NAME "DCPSTypeLookupReply"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_SECURE_NAME "DCPSParticipantsSecure"
#define DDS_BUILTIN_TOPIC_PUBLICATION_SECURE_NAME "DCPSPublicationsSecure"
#define DDS_BUILTIN_TOPIC_SUBSCRIPTION_SECURE_NAME "DCPSSubscriptionsSecure"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_SECURE_NAME "DCPSParticipantMessageSecure"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_STATELESS_MESSAGE_NAME "DCPSParticipantStatelessMessage"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_VOLATILE_MESSAGE_SECURE_NAME "DCPSParticipantVolatileMessageSecure"

/* Participant built-in topic qos properties */
#define DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_PROCESS_NAME "__ProcessName"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_PID "__Pid"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_HOSTNAME "__Hostname"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_NETWORKADDRESSES "__NetworkAddresses"
#define DDS_BUILTIN_TOPIC_PARTICIPANT_DEBUG_MONITOR "__DebugMonitor"
#define DDS_BUILTIN_TOPIC_NULL_NAME NULL

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_PROTOCOL_H */
