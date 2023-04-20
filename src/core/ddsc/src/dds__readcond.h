// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__READCOND_H
#define DDS__READCOND_H

#include "dds__entity.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component data_query */
dds_readcond * dds_create_readcond_impl (dds_reader *rd, dds_entity_kind_t kind, uint32_t mask, dds_querycondition_filter_fn filter);

#if defined (__cplusplus)
}
#endif

#endif /* DDS__READCOND_H */
