// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__PARTICIPANT_H
#define DDSI__PARTICIPANT_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/fibheap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsi/ddsi_gc.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_participant.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSI_DELETED_PPGUID_LOCAL 1
#define DDSI_DELETED_PPGUID_REMOTE 2

extern const ddsrt_fibheap_def_t ddsi_ldur_fhdef;
extern const ddsrt_fibheap_def_t ddsi_lease_fhdef_pp;

struct ddsi_writer;

struct ddsi_deleted_participant {
  ddsrt_avl_node_t avlnode;
  ddsi_guid_t guid;
  ddsrt_mtime_t t_prune;
};

struct ddsi_deleted_participants_admin {
  ddsrt_mutex_t deleted_participants_lock;
  ddsrt_avl_tree_t deleted_participants;
  const ddsrt_log_cfg_t *logcfg;
  int64_t delay;
};

/** @component ddsi_participant */
void ddsi_participant_add_wr_lease_locked (struct ddsi_participant * pp, const struct ddsi_writer * wr);

/** @component ddsi_participant */
void ddsi_participant_remove_wr_lease_locked (struct ddsi_participant * pp, struct ddsi_writer * wr);

/** @component ddsi_participant */
dds_return_t ddsi_participant_allocate_entityid (ddsi_entityid_t *id, uint32_t kind, struct ddsi_participant *pp);

/** @component ddsi_participant */
void ddsi_participant_release_entityid (struct ddsi_participant *pp, ddsi_entityid_t id);

/** @component ddsi_participant */
void ddsi_gc_participant_lease (struct ddsi_gcreq *gcreq);

/** @component ddsi_participant */
void ddsi_prune_deleted_participant_guids (struct ddsi_deleted_participants_admin *admin, ddsrt_mtime_t tnow);

/** @component ddsi_participant */
void ddsi_remove_deleted_participant_guid (struct ddsi_deleted_participants_admin *admin, const struct ddsi_guid *guid);

/** @component ddsi_participant */
bool ddsi_remember_deleted_participant_guid (struct ddsi_deleted_participants_admin *admin, const struct ddsi_guid *guid)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

/** @component ddsi_participant */
struct ddsi_participant *ddsi_ref_participant (struct ddsi_participant *pp, const struct ddsi_guid *guid_of_refing_entity);

/** @component ddsi_participant */
void ddsi_unref_participant (struct ddsi_participant *pp, const struct ddsi_guid *guid_of_refing_entity);

/** @component ddsi_participant */
struct ddsi_deleted_participants_admin *ddsi_deleted_participants_admin_new (const ddsrt_log_cfg_t *logcfg, int64_t delay);

/** @component ddsi_participant */
void ddsi_deleted_participants_admin_free (struct ddsi_deleted_participants_admin *admin);

/** @component ddsi_participant */
int ddsi_is_deleted_participant_guid (struct ddsi_deleted_participants_admin *admin, const struct ddsi_guid *guid);

/**
 * @component ddsi_participant
 * @brief Gets the interval for PMD messages, which is the minimal lease duration for writers
 * with auto liveliness in this participant, or the participants lease duration if shorter
 *
 *
 * @param[in] pp The participant
 * @returns The PMD interval of the participant
 */
dds_duration_t ddsi_participant_get_pmd_interval (struct ddsi_participant *pp);

/**
 * @component ddsi_participant
 * @brief To obtain the builtin writer to be used for publishing SPDP, SEDP, PMD stuff for
 * PP and its endpoints, given the entityid.
 *
 * @param[in] pp The participant
 * @param[in] entityid The entity ID of the writer
 * @param[out] bwr The built-in writer
 * @returns Return code indicating success or failure
 * @retval `DDS_RETCODE_OK` and `bwr` != NULL writer to use
 * @retval `DDS_RETCODE_OK` and `bwr` == NULL no data needs to be written
 * @retval `DDS_RETCODE_PRECONDITION_NOT_MET` data should be written but participant does not have a writer
 * @retval `DDS_RETCODE_BAD_PARAMETER` entityid is invalid
 */
dds_return_t ddsi_get_builtin_writer (const struct ddsi_participant *pp, unsigned entityid, struct ddsi_writer **bwr);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__PARTICIPANT_H */
