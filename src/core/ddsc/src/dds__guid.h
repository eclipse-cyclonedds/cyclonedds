// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__GUID_H
#define DDS__GUID_H

#include "dds/ddsc/dds_basic_types.h"
#include "dds/ddsi/ddsi_guid.h"

ddsi_guid_t dds_guid_to_ddsi_guid (dds_guid_t g);
dds_guid_t dds_guid_from_ddsi_guid (ddsi_guid_t gi);

#endif
