// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_MACHINEID_H
#define DDSRT_MACHINEID_H

#include <stdbool.h>

#include "dds/export.h"
#include "dds/config.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct ddsrt_machineid {
  unsigned char x[16];
} ddsrt_machineid_t;

DDS_EXPORT bool ddsrt_get_machineid (ddsrt_machineid_t *id);

#if defined(__cplusplus)
}
#endif

#endif /* DDSRT_MACHINEID_H */
