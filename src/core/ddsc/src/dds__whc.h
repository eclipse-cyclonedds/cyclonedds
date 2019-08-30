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
#ifndef DDS__WHC_H
#define DDS__WHC_H

#include "dds/ddsi/q_whc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct q_globals;
struct whc *whc_new (struct q_globals *gv, int is_transient_local, uint32_t hdepth, uint32_t tldepth);

#if defined (__cplusplus)
}
#endif

#endif /* Q_WHC_H */
