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
#ifndef _DDS_GUARDCOND_H_
#define _DDS_GUARDCOND_H_

#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

dds_guardcond*
dds_create_guardcond(
        dds_participant *pp);

DEFINE_ENTITY_LOCK_UNLOCK(dds_guardcond, DDS_KIND_COND_GUARD)

#if defined (__cplusplus)
}
#endif

#endif
