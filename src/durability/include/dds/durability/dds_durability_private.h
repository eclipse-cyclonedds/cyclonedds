// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_DURABILITY_PRIVATE_H
#define DDS_DURABILITY_PRIVATE_H

#include "dds/export.h"
#include "dds/durability/dds_durability_public.h"

#if defined (__cplusplus)
extern "C" {
#endif

DDS_EXPORT void dds_durability_creator(dds_durability_t* ds);

#if defined (__cplusplus)
}
#endif

#endif /* DDS_DURABILITY_PRIVATE_H */
