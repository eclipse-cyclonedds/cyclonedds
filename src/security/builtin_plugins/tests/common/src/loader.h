// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef _DBT_SECURITY_PLUGINS_LOADER_H_
#define _DBT_SECURITY_PLUGINS_LOADER_H_

#include "dds/security/dds_security_api.h"

struct plugins_hdl;

/** load some or all security plugins for testing

 @param[out] ac      where to store address of loaded access control plugin, or NULL if not to be loaded
 @param[out] auth    where to store address of loaded authentication plugin, or NULL if not to be loaded
 @param[out] crypto  where to store address of loaded cryptography plugin, or NULL if not to be loaded
 @param[in]  init_gv initial value for DDSI globals that plugins happen to rely on, or NULL if no special
                     values need to be set.  "xevents" is always set in load_plugins

 @returns pointer to opaque handle for unloading the plugins, or NULL on failure
 */
struct plugins_hdl*
load_plugins(
        dds_security_access_control **ac,
        dds_security_authentication **auth,
        dds_security_cryptography   **crypto,
        const struct ddsi_domaingv *init_gv);

void
unload_plugins(
        struct plugins_hdl *plugins);

char*
load_file_contents(
        const char *filename);

#endif
