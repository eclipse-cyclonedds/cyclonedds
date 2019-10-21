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
#ifndef _DDSI_SSL_H_
#define _DDSI_SSL_H_

#ifdef DDSI_INCLUDE_SSL

#ifdef _WIN32
/* supposedly WinSock2 must be included before openssl headers otherwise winsock will be used */
#include <WinSock2.h>
#endif
#include <openssl/ssl.h>

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_ssl_plugins;
void ddsi_ssl_config_plugin (struct ddsi_ssl_plugins *plugin);

#if defined (__cplusplus)
}
#endif

#endif
#endif
