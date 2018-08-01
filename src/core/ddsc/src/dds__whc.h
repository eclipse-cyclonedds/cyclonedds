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

#include "ddsi/q_whc.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct whc *whc_new (int is_transient_local, unsigned hdepth, unsigned tldepth);

#if defined (__cplusplus)
}
#endif

#endif /* Q_WHC_H */
