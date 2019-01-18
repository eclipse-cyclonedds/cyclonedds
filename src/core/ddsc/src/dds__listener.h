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
#ifndef _DDS_LISTENER_H_
#define _DDS_LISTENER_H_

#include "dds__types.h"
#include "dds/ddsc/dds_public_listener.h"

#if defined (__cplusplus)
extern "C" {
#endif

void dds_override_inherited_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src);
void dds_inherit_listener (dds_listener_t * __restrict dst, const dds_listener_t * __restrict src);

#if defined (__cplusplus)
}
#endif
#endif
