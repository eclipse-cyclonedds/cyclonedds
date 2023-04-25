// Copyright(c) 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__DELIVER_LOCALLY_H
#define DDSI__DELIVER_LOCALLY_H

#include <stdint.h>
#include <stdbool.h>

#include "dds/export.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_deliver_locally.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_domaingv;
struct ddsi_entity_common;
struct ddsi_writer_info;
struct ddsi_deliver_locally_ops;

/** @component local_delivery */
dds_return_t ddsi_deliver_locally_one (struct ddsi_domaingv *gv, struct ddsi_entity_common *source_entity, bool source_entity_locked, const ddsi_guid_t *rdguid,
    const struct ddsi_writer_info *wrinfo, const struct ddsi_deliver_locally_ops * __restrict ops, void *vsourceinfo);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__DELIVER_LOCALLY_H */
