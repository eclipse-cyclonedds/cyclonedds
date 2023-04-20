// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__DOMAIN_H
#define DDS__DOMAIN_H

#include "dds__types.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component domain */
dds_entity_t dds_domain_create_internal (dds_domain **domain_out, dds_domainid_t id, bool implicit, const char *config) ddsrt_nonnull((1,4));

/** @component domain */
dds_domain *dds_domain_find_locked (dds_domainid_t id);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__DOMAIN_H */
