/*
 * Copyright(c) 2006 to 2021 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI__DISCOVERY_H
#define DDSI__DISCOVERY_H

#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h" // FIXME: MAX_XMIT_CONNS

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_participant;
struct ddsi_topic;
struct ddsi_writer;
struct ddsi_reader;
struct ddsi_rsample_info;
struct ddsi_rdata;
struct ddsi_plist;
struct ddsi_xevent;
struct ddsi_xpack;
struct ddsi_domaingv;

/** @component discovery */
int ddsi_builtins_dqueue_handler (const struct ddsi_rsample_info *sampleinfo, const struct ddsi_rdata *fragchain, const ddsi_guid_t *rdguid, void *qarg);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DISCOVERY_H */
