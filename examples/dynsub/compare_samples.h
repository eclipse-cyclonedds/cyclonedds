// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef COMPARE_SAMPLES_H
#define COMPARE_SAMPLES_H

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

struct type_cache;

int compare_samples (struct type_cache *tc, bool valid_data, const void *sample1, const void* sample2, const DDS_XTypes_CompleteTypeObject *typeobj);

#endif /* COMPARE_SAMPLES_H */
