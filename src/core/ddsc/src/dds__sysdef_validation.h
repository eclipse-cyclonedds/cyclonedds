// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#ifndef DDS__SYSDEF_VALIDATION_H
#define DDS__SYSDEF_VALIDATION_H

#if defined (__cplusplus)
extern "C" {
#endif

#include "dds/dds.h"
#include "dds__sysdef_model.h"

dds_return_t dds_validate_qos_lib (
  const struct dds_sysdef_system *sysdef, uint64_t qos_mask);

#if defined (__cplusplus)
}
#endif
#endif // DDS__SYSDEF_VALIDATION_H
