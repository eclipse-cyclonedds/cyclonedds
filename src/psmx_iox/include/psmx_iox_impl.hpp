// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdbool.h>

#if defined (__cplusplus)
extern "C" {
#endif

#include "dds/dds.h"
#include "dds/ddsc/dds_loaned_sample.h"
#include "dds/ddsc/dds_psmx.h"
#include "psmx_iox_export.h"

DDS_PSMX_IOX_EXPORT dds_return_t iox_create_psmx (struct dds_psmx **psmx, dds_psmx_instance_id_t instance_id, const char *config);

#if defined (__cplusplus)
}
#endif
