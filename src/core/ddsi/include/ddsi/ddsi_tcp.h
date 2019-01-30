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
#ifndef _DDSI_TCP_H_
#define _DDSI_TCP_H_

#include "ddsi/ddsi_tran.h"

#ifdef DDSI_INCLUDE_SSL

#include "ddsi/ddsi_ssl.h"

struct ddsi_ssl_plugins
{
  bool (*init) (void);
  void (*fini) (void);
  void (*ssl_free) (SSL *ssl);
  void (*bio_vfree) (BIO *bio);
  ssize_t (*read) (SSL *ssl, void *buf, size_t len, int *err);
  ssize_t (*write) (SSL *ssl, const void *msg, size_t len, int *err);
  SSL * (*connect) (os_socket sock);
  BIO * (*listen) (os_socket sock);
  SSL * (*accept) (BIO *bio, os_socket *sock);
};

#endif

int ddsi_tcp_init (void);

#endif
