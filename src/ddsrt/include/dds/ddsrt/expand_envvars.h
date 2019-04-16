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
#ifndef DDSRT_EXPAND_ENVVARS_H
#define DDSRT_EXPAND_ENVVARS_H

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

    /* Expands ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms, but not $X */
    DDS_EXPORT char *ddsrt_expand_envvars(const char *string);

    /* Expands $X, ${X}, ${X:-Y}, ${X:+Y}, ${X:?Y} forms, $ and \ can be escaped with \ */
    DDS_EXPORT char *ddsrt_expand_envvars_sh(const char *string);

#if defined (__cplusplus)
}
#endif

#endif
