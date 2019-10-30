/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef _DBT_SECURITY_PLUGINS_LOADER_H_
#define _DBT_SECURITY_PLUGINS_LOADER_H_

#include "dds/security/dds_security_api.h"

struct plugins_hdl;

struct plugins_hdl*
load_plugins(
        dds_security_access_control **ac,
        dds_security_authentication **auth,
        dds_security_cryptography   **crypto);

void
unload_plugins(
        struct plugins_hdl *plugins);

char*
load_file_contents(
        const char *filename);

#endif
