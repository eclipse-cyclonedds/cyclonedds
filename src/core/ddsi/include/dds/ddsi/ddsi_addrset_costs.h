// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_WRADDRSET_COSTS_H
#define DDSI_WRADDRSET_COSTS_H

#include <stddef.h>
#include "dds/ddsrt/time.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_addrset_costs {
  int32_t uc;
  int32_t mc;
  int32_t ssm;
  int32_t delivered;
  int32_t discarded;
  int32_t redundant_psmx;
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_WRADDRSET_COSTS_H */
