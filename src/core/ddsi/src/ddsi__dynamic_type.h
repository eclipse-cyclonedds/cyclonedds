// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__DYNAMIC_TYPE_H
#define DDSI__DYNAMIC_TYPE_H

#include "dds/export.h"
#include "dds/features.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSI_DYNAMIC_TYPE_MEMBERID_MASK 0x0fffffffu

/**
 * @brief Calculate aggregate type member ID
 *
 * Calculates the ID of a member of an aggregated type from it's (hash-)name,
 * (see Xtypes spec 1.3, 7.3.1.2.1.1)
 *
 * @component dynamic_type_support
 *
 * @param[in] member_hash_name Name of the member (or name provided in the hashid annotation).
 *
 */
uint32_t ddsi_dynamic_type_member_hashid (const char *member_hash_name);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DYNAMIC_TYPE_H */
