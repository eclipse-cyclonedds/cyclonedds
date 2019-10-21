/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef _DDS_GET_STATUS_H_
#define _DDS_GET_STATUS_H_

#include "cyclonedds/ddsrt/countargs.h"

#define DDS_GET_STATUS_LOCKED_RESET_1(status_, reset0_) \
  (ent->m_##status_##_status.reset0_ = 0);
#define DDS_GET_STATUS_LOCKED_RESET_2(status_, reset0_, reset1_) \
  (ent->m_##status_##_status.reset0_ = 0); \
  (ent->m_##status_##_status.reset1_ = 0);
#define DDS_GET_STATUS_LOCKED_RESET_MSVC_WORKAROUND(x) x
#define DDS_GET_STATUS_LOCKED_RESET_N1(n_, status_, ...)                \
  DDS_GET_STATUS_LOCKED_RESET_MSVC_WORKAROUND (DDS_GET_STATUS_LOCKED_RESET_##n_ (status_, __VA_ARGS__))
#define DDS_GET_STATUS_LOCKED_RESET_N(n_, status_, ...) DDS_GET_STATUS_LOCKED_RESET_N1 (n_, status_, __VA_ARGS__)

#define DDS_GET_STATUS_LOCKED(ent_type_, status_, STATUS_, ...) \
  static void dds_get_##status_##_status_locked (dds_##ent_type_ *ent, dds_##status_##_status_t *status) \
  { \
    if (status) \
      *status = ent->m_##status_##_status; \
    if (ddsrt_atomic_ld32 (&ent->m_entity.m_status.m_status_and_mask) & (DDS_##STATUS_##_STATUS << SAM_ENABLED_SHIFT)) { \
      do { DDS_GET_STATUS_LOCKED_RESET_N (DDSRT_COUNT_ARGS (__VA_ARGS__), status_, __VA_ARGS__) } while (0); \
      dds_entity_status_reset (&ent->m_entity, DDS_##STATUS_##_STATUS); \
    } \
  }

#define DDS_GET_STATUS_COMMON(ent_type_, status_) \
  dds_return_t dds_get_##status_##_status (dds_entity_t entity, dds_##status_##_status_t *status) \
  { \
    dds_##ent_type_ *ent; \
    dds_return_t ret; \
    if ((ret = dds_##ent_type_##_lock (entity, &ent)) != DDS_RETCODE_OK) \
      return ret; \
    ddsrt_mutex_lock (&ent->m_entity.m_observers_lock); \
    dds_get_##status_##_status_locked (ent, status); \
    ddsrt_mutex_unlock (&ent->m_entity.m_observers_lock); \
    dds_##ent_type_##_unlock (ent); \
    return DDS_RETCODE_OK; \
  }

#define DDS_GET_STATUS(ent_type_, status_, STATUS_, ...) \
  DDS_GET_STATUS_LOCKED (ent_type_, status_, STATUS_, __VA_ARGS__) \
  DDS_GET_STATUS_COMMON (ent_type_, status_)

#endif
