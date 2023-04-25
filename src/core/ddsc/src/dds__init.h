// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__INIT_H
#define DDS__INIT_H

#include "dds__types.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @component cyclone_root
 *
 * Initializes the library and constructs the global pseudo-entity identified
 * by DDS_CYCLONEDDS_HANDLE with one reference that must (eventually) be
 * released by calling dds_delete on that handle.
 *
 * @return dds_return_t Returns 0 on success or a non-zero error status
 */
dds_return_t dds_init (void);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__INIT_H */
