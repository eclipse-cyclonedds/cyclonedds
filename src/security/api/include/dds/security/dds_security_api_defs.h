/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef DDS_SECURITY_API_DEF_H
#define DDS_SECURITY_API_DEF_H

#include "dds_security_api_err.h"

#if defined (__cplusplus)
extern "C" {
#endif



/**************************************************************************
 *                                                                        *
 * Return values.                                                         *
 *                                                                        *
 **************************************************************************/
typedef enum {
    DDS_SECURITY_VALIDATION_OK,
    DDS_SECURITY_VALIDATION_FAILED,
    DDS_SECURITY_VALIDATION_PENDING_RETRY,
    DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_REQUEST,
    DDS_SECURITY_VALIDATION_PENDING_HANDSHAKE_MESSAGE,
    DDS_SECURITY_VALIDATION_OK_FINAL_MESSAGE
} DDS_Security_ValidationResult_t;

#define DDS_SECURITY_HANDLE_NIL (0)




/**************************************************************************
 *                                                                        *
 * Attribute flags.                                                       *
 *                                                                        *
 **************************************************************************/
#define DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_PROTECTED                      (0x00000001      )
#define DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED                 (0x00000001 <<  1)
#define DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED                (0x00000001 <<  2)
#define DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID                               (0x00000001 << 31)

#define DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED               (0x00000001      )
#define DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED          (0x00000001 <<  1)
#define DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED         (0x00000001 <<  2)
#define DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED           (0x00000001 <<  3)
#define DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED      (0x00000001 <<  4)
#define DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED     (0x00000001 <<  5)

#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_READ_PROTECTED                         (0x00000001      )
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_WRITE_PROTECTED                        (0x00000001 <<  1)
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED                    (0x00000001 <<  2)
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED                   (0x00000001 <<  3)
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED                      (0x00000001 <<  4)
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_KEY_PROTECTED                          (0x00000001 <<  5)
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED                   (0x00000001 <<  6)
#define DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_VALID                                  (0x00000001 << 31)

#define DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED            (0x00000001      )
#define DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED               (0x00000001 <<  1)
#define DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED (0x00000001 <<  2)




/**************************************************************************
 *                                                                        *
 * Protection types.                                                      *
 *                                                                        *
 **************************************************************************/
typedef enum {
    DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION,
    DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION,
    DDS_SECURITY_PROTECTION_KIND_ENCRYPT,
    DDS_SECURITY_PROTECTION_KIND_SIGN,
    DDS_SECURITY_PROTECTION_KIND_NONE
} DDS_Security_ProtectionKind;

typedef enum {
    DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT,
    DDS_SECURITY_BASICPROTECTION_KIND_SIGN,
    DDS_SECURITY_BASICPROTECTION_KIND_NONE
} DDS_Security_BasicProtectionKind;




/**************************************************************************
 *                                                                        *
 * Submessage categories.                                                 *
 *                                                                        *
 **************************************************************************/
typedef enum {
    DDS_SECURITY_INFO_SUBMESSAGE,
    DDS_SECURITY_DATAWRITER_SUBMESSAGE,
    DDS_SECURITY_DATAREADER_SUBMESSAGE
} DDS_Security_SecureSubmessageCategory_t;




/**************************************************************************
 *                                                                        *
 * QoS Policies content.                                                  *
 *                                                                        *
 **************************************************************************/
typedef enum {
    DDS_SECURITY_AUTOMATIC_LIVELINESS_QOS,
    DDS_SECURITY_MANUAL_BY_PARTICIPANT_LIVELINESS_QOS,
    DDS_SECURITY_MANUAL_BY_TOPIC_LIVELINESS_QOS
} DDS_Security_LivelinessQosPolicyKind;

typedef enum {
    DDS_SECURITY_BEST_EFFORT_RELIABILITY_QOS,
    DDS_SECURITY_RELIABLE_RELIABILITY_QOS
} DDS_Security_ReliabilityQosPolicyKind;

typedef enum  {
    DDS_SECURITY_BY_RECEPTION_TIMESTAMP_DESTINATIONORDER_QOS,
    DDS_SECURITY_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS
} DDS_Security_DestinationOrderQosPolicyKind;

typedef enum {
    DDS_SECURITY_INSTANCE_PRESENTATION_QOS,
    DDS_SECURITY_TOPIC_PRESENTATION_QOS,
    DDS_SECURITY_GROUP_PRESENTATION_QOS
} DDS_Security_PresentationQosPolicyAccessScopeKind;

typedef enum {
    DDS_SECURITY_KEEP_LAST_HISTORY_QOS,
    DDS_SECURITY_KEEP_ALL_HISTORY_QOS
} DDS_Security_HistoryQosPolicyKind;

typedef enum {
    DDS_SECURITY_VOLATILE_DURABILITY_QOS,
    DDS_SECURITY_TRANSIENT_LOCAL_DURABILITY_QOS,
    DDS_SECURITY_TRANSIENT_DURABILITY_QOS,
    DDS_SECURITY_PERSISTENT_DURABILITY_QOS
} DDS_Security_DurabilityQosPolicyKind;

typedef enum {
    DDS_SECURITY_SHARED_OWNERSHIP_QOS,
    DDS_SECURITY_EXCLUSIVE_OWNERSHIP_QOS
} DDS_Security_OwnershipQosPolicyKind;




/**************************************************************************
 *                                                                        *
 * Listener information.                                                  *
 *                                                                        *
 **************************************************************************/
typedef enum {
    DDS_SECURITY_IDENTITY_STATUS
} DDS_Security_AuthStatusKind;




/**************************************************************************
 *                                                                        *
 * Some byte array sizes.                                                 *
 *                                                                        *
 **************************************************************************/
#define DDS_SECURITY_AUTHENTICATION_CHALLENGE_SIZE 32

#define DDS_SECURITY_MASTER_SALT_SIZE 32
#define DDS_SECURITY_MASTER_SENDER_KEY_SIZE 32
#define DDS_SECURITY_MASTER_RECEIVER_SPECIFIC_KEY_SIZE 32



#if defined (__cplusplus)
}
#endif


#endif /* DDS_SECURITY_API_DEF_H */
