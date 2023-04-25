// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SECURITY_UTIL_H
#define DDSI__SECURITY_UTIL_H

#include "dds/features.h"

#ifdef DDS_HAS_SECURITY

#include "dds/ddsi/ddsi_plist.h"
#include "dds/security/core/dds_security_utils.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component security_data */
void ddsi_omg_shallow_copy_StringSeq (DDS_Security_StringSeq *dst, const ddsi_stringseq_t *src);

/** @component security_data */
void ddsi_omg_shallow_free_StringSeq (DDS_Security_StringSeq *obj);

/** @component security_data */
void ddsi_omg_copy_PropertySeq (DDS_Security_PropertySeq *dst, const dds_propertyseq_t *src);

/** @component security_data */
void ddsi_omg_shallow_copyin_PropertySeq (DDS_Security_PropertySeq *dst, const dds_propertyseq_t *src);

/** @component security_data */
void ddsi_omg_shallow_copyout_PropertySeq (dds_propertyseq_t *dst, const DDS_Security_PropertySeq *src);

/** @component security_data */
void ddsi_omg_shallow_free_PropertySeq (DDS_Security_PropertySeq *obj);

/** @component security_data */
void ddsi_omg_shallow_copyin_BinaryPropertySeq (DDS_Security_BinaryPropertySeq *dst, const dds_binarypropertyseq_t *src);

/** @component security_data */
void ddsi_omg_shallow_copyout_BinaryPropertySeq (dds_binarypropertyseq_t *dst, const DDS_Security_BinaryPropertySeq *src);

/** @component security_data */
void ddsi_omg_shallow_free_BinaryPropertySeq (DDS_Security_BinaryPropertySeq *obj);

/** @component security_data */
void ddsi_omg_shallow_copy_PropertyQosPolicy (DDS_Security_PropertyQosPolicy *dst, const dds_property_qospolicy_t *src);

/** @component security_data */
void ddsi_omg_shallow_copy_security_qos (DDS_Security_Qos *dst, const struct dds_qos *src);

/** @component security_data */
void ddsi_omg_shallow_free_PropertyQosPolicy (DDS_Security_PropertyQosPolicy *obj);

/** @component security_data */
void ddsi_omg_shallow_free_security_qos (DDS_Security_Qos *obj);

/** @component security_data */
void ddsi_omg_security_dataholder_copyin (ddsi_dataholder_t *dh, const DDS_Security_DataHolder *holder);

/** @component security_data */
void ddsi_omg_security_dataholder_copyout (DDS_Security_DataHolder *holder, const ddsi_dataholder_t *dh);

/** @component security_data */
void ddsi_omg_shallow_copyin_DataHolder (DDS_Security_DataHolder *dst, const ddsi_dataholder_t *src);

/** @component security_data */
void ddsi_omg_shallow_copyout_DataHolder (ddsi_dataholder_t *dst, const DDS_Security_DataHolder *src);

/** @component security_data */
void ddsi_omg_shallow_free_DataHolder (DDS_Security_DataHolder *obj);

/** @component security_data */
void ddsi_omg_shallow_free_ddsi_dataholder (ddsi_dataholder_t *holder);

/** @component security_data */
void ddsi_omg_shallow_copyin_DataHolderSeq (DDS_Security_DataHolderSeq *dst, const ddsi_dataholderseq_t *src);

/** @component security_data */
void ddsi_omg_copyin_DataHolderSeq (DDS_Security_DataHolderSeq *dst, const ddsi_dataholderseq_t *src);

/** @component security_data */
void ddsi_omg_shallow_copyout_DataHolderSeq (ddsi_dataholderseq_t *dst, const DDS_Security_DataHolderSeq *src);

/** @component security_data */
void ddsi_omg_shallow_free_DataHolderSeq (DDS_Security_DataHolderSeq *obj);

/** @component security_data */
void ddsi_omg_shallow_free_ddsi_dataholderseq (ddsi_dataholderseq_t *obj);

/** @component security_data */
void ddsi_omg_shallow_copy_ParticipantBuiltinTopicDataSecure (DDS_Security_ParticipantBuiltinTopicDataSecure *dst, const ddsi_guid_t *guid, const ddsi_plist_t *plist);

/** @component security_data */
void ddsi_omg_shallow_free_ParticipantBuiltinTopicDataSecure (DDS_Security_ParticipantBuiltinTopicDataSecure *obj);

/** @component security_data */
void ddsi_omg_shallow_copy_SubscriptionBuiltinTopicDataSecure (DDS_Security_SubscriptionBuiltinTopicDataSecure *dst, const ddsi_guid_t *guid, const struct dds_qos *qos, const ddsi_security_info_t *secinfo);

/** @component security_data */
void ddsi_omg_shallow_free_SubscriptionBuiltinTopicDataSecure (DDS_Security_SubscriptionBuiltinTopicDataSecure *obj);

/** @component security_data */
void ddsi_omg_shallow_copy_PublicationBuiltinTopicDataSecure (DDS_Security_PublicationBuiltinTopicDataSecure *dst, const ddsi_guid_t *guid, const struct dds_qos *qos, const ddsi_security_info_t *secinfo);

/** @component security_data */
void ddsi_omg_shallow_free_PublicationBuiltinTopicDataSecure (DDS_Security_PublicationBuiltinTopicDataSecure *obj);

/** @component security_data */
void ddsi_omg_shallow_copy_TopicBuiltinTopicData (DDS_Security_TopicBuiltinTopicData *dst, const char *topic_name, const char *type_name);

/** @component security_data */
void ddsi_omg_shallow_free_TopicBuiltinTopicData (DDS_Security_TopicBuiltinTopicData *obj);

#if defined (__cplusplus)
}
#endif


#endif /* DDS_HAS_SECURITY */

#endif /* DDSI__SECURITY_UTIL_H */
