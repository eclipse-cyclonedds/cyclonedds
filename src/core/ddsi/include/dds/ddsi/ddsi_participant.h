// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PARTICIPANT_H
#define DDSI_PARTICIPANT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_inverse_uint32_set.h"
#include "dds/ddsi/ddsi_plist.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Set this flag in new_participant to prevent the creation SPDP, SEDP
   and PMD readers for that participant.  It doesn't really need it,
   they all share the information anyway.  But you do need it once. */
#define RTPS_PF_NO_BUILTIN_READERS 1u
/* Set this flag to prevent the creation of SPDP, SEDP and PMD
   writers.  It will then rely on the "privileged participant", which
   must exist at the time of creation.  It creates a reference to that
   "privileged participant" to ensure it won't disappear too early. */
#define RTPS_PF_NO_BUILTIN_WRITERS 2u
/* Set this flag to mark the participant as the "privileged
   participant", there can only be one of these.  The privileged
   participant MUST have all builtin readers and writers. */
#define RTPS_PF_PRIVILEGED_PP 4u
/* Set this flag to mark the participant as is_ddsi2_pp. */
#define RTPS_PF_IS_DDSI2_PP 8u
/* Set this flag to mark the participant as an local entity only. */
#define RTPS_PF_ONLY_LOCAL 16u
/* Set this flag to mark that no privileged participant should be used
   for the built-in readers and writers. Can be used with NO_BUILTIN_READER and
   NO_BUILTIN_WRITER flag to avoid any communication for built-in topics. */
#define RTPS_PF_NO_PRIVILEGED_PP 32u

struct ddsi_avail_entityid_set {
  struct ddsi_inverse_uint32_set x;
};

enum ddsi_participant_state {
  DDSI_PARTICIPANT_STATE_INITIALIZING,
  DDSI_PARTICIPANT_STATE_OPERATIONAL,
  DDSI_PARTICIPANT_STATE_DELETE_STARTED,
  DDSI_PARTICIPANT_STATE_DELETING_BUILTINS
};

struct ddsi_participant
{
  struct ddsi_entity_common e;
  uint32_t bes; /* built-in endpoint set */
  unsigned is_ddsi2_pp: 1; /* true for the "federation leader", the ddsi2 participant itself in OSPL; FIXME: probably should use this for broker mode as well ... */
  uint32_t flags; /* flags used when creating this participant */
  struct ddsi_plist *plist; /* settings/QoS for this participant */
  struct ddsi_xevent *spdp_xevent; /* timed event for periodically publishing SPDP */
  struct ddsi_xevent *pmd_update_xevent; /* timed event for periodically publishing ParticipantMessageData */
  ddsi_locator_t m_locator; /* this is always a unicast address, it is set if it is in the many unicast mode */
  struct ddsi_tran_conn * m_conn; /* this is connection to m_locator, if it is set, this is used */
  struct ddsi_avail_entityid_set avail_entityids; /* available entity ids [e.lock] */
  ddsrt_mutex_t refc_lock;
  int32_t user_refc; /* number of non-built-in endpoints in this participant [refc_lock] */
  int32_t builtin_refc; /* number of built-in endpoints in this participant [refc_lock] */
  enum ddsi_participant_state state; /* current state of this participant [refc_lock] */
  ddsrt_fibheap_t ldur_auto_wr; /* Heap that contains lease duration for writers with automatic liveliness in this participant */
  ddsrt_atomic_voidp_t minl_man; /* clone of min(leaseheap_man) */
  ddsrt_fibheap_t leaseheap_man; /* keeps leases for this participant's writers (with liveliness manual-by-participant) */
#ifdef DDS_HAS_SECURITY
  struct ddsi_participant_sec_attributes *sec_attr;
  ddsi_security_info_t security_info;
#endif
};

/**
 * @brief Generates a new participant GUID
 *
 * @param[out] ppguid  The generated participant GUID
 * @param[in] gv  Domain globals
 */
void ddsi_generate_participant_guid (ddsi_guid_t *ppguid, struct ddsi_domaingv *gv);

/**
 * @brief Create a new participant with a specified GUID
 * @component ddsi_participant
 *
 * @param[in] ppguid  The GUID for the new participant
 * @param[in]  gv  Domain globals
 * @param[in]  flags
 *               Zero or more of:
 *               - RTPS_PF_NO_BUILTIN_READERS   do not create discovery readers in new ppant
 *               - RTPS_PF_NO_BUILTIN_WRITERS   do not create discvoery writers in new ppant
 *               - RTPS_PF_PRIVILEGED_PP        FIXME: figure out how to describe this ...
 *               - RTPS_PF_IS_DDSI2_PP          FIXME: OSPL holdover - there is no DDSI2E here
 *               - RTPS_PF_ONLY_LOCAL           FIXME: not used, it seems
 * @param[in]  plist  Parameters/QoS for this participant
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *               Success
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *               The configured maximum number of participants has been reached.
*/
dds_return_t ddsi_new_participant (ddsi_guid_t *ppguid, struct ddsi_domaingv *gv, unsigned flags, const struct ddsi_plist *plist);

/**
 * @component ddsi_participant
 *
 * Initiate the deletion of the participant:
 * - dispose/unregister built-in topic
 * - list it as one of the recently deleted participants
 * - remote it from the GUID hash tables
 * - schedule the scare stuff to really delete it via the GC
 *
 * It is ok to call delete_participant without deleting all DDSI-level
 * readers/writers: those will simply be deleted.  (New ones can't be
 * created anymore because the participant can no longer be located via
 * the hash tables).
 *
 * @param[in]  ppguid
 *               GUID of the participant to be deleted.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *               Success, it is no longer visible and GC events have
 *               been scheduled for eventual deleting of all remaining
 *               readers and writers and freeing of memory
 * @retval DDS_RETCODE_BAD_PARAMETER
 *               ppguid lookup failed.
*/
dds_return_t ddsi_delete_participant (struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid);

/**
 * @brief Updates the parameters list for this participant and trigger sending an updated SPDP message.
 * @component ddsi_participant
 *
 * @param[in] pp The participant
 * @param[in] plist The new parameters
 */
void ddsi_update_participant_plist (struct ddsi_participant *pp, const struct ddsi_plist *plist);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_PARTICIPANT_H */
