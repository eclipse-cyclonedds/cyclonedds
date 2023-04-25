// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__GUARDCOND_H
#define DDS__GUARDCOND_H

#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

DEFINE_ENTITY_LOCK_UNLOCK(dds_guardcond, DDS_KIND_COND_GUARD, guard_condition)

#if defined (__cplusplus)
}
#endif

#endif /* DDS__GUARDCOND_H */
