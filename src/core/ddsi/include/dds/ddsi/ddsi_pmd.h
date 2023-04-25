// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_PMD_H
#define DDSI_PMD_H

#include <stdint.h>
#include "dds/ddsi/ddsi_guid.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct ddsi_guid;

/** @component pmd */
void ddsi_write_pmd_message_guid (struct ddsi_domaingv * const gv, struct ddsi_guid *pp_guid, unsigned pmd_kind);

#if defined (__cplusplus)
}
#endif
#endif /* DDSI_PMD_H */
