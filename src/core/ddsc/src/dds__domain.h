/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef _DDS_DOMAIN_H_
#define _DDS_DOMAIN_H_

#include "dds__types.h"

#if defined (__cplusplus)
extern "C" {
#endif

extern DDS_EXPORT const ut_avlTreedef_t dds_domaintree_def;

DDS_EXPORT dds_domain * dds_domain_create (dds_domainid_t id);
DDS_EXPORT void dds_domain_free (dds_domain * domain);
DDS_EXPORT dds_domain * dds_domain_find_locked (dds_domainid_t id);

#if defined (__cplusplus)
}
#endif
#endif
