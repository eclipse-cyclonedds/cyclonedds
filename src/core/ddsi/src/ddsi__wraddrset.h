// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__WRADDRSET_H
#define DDSI__WRADDRSET_H

#include <stddef.h>
#include <stdbool.h>
#include "dds/ddsi/ddsi_addrset.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_writer;

/** @component locators */
struct ddsi_addrset *ddsi_compute_writer_addrset (const struct ddsi_writer *wr);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__WRADDRSET_H */
