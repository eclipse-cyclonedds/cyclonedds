// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef PSMX_DUMMY_IMPL_H
#define PSMX_DUMMY_IMPL_H

#include "dds/psmx_dummy_v0/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

PSMX_DUMMY_EXPORT dds_return_t dummy_v0_create_psmx(dds_psmx_t **psmx, dds_psmx_instance_id_t instance_id, const char *config);

#if defined (__cplusplus)
}
#endif

#endif /* PSMX_DUMMY_IMPL_H */
