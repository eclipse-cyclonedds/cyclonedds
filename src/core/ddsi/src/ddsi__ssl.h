// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__SSL_H
#define DDSI__SSL_H

#include "dds/features.h"

#ifdef DDS_HAS_TCP_TLS

#ifdef _WIN32
/* supposedly WinSock2 must be included before openssl headers otherwise winsock will be used */
#include <WinSock2.h>
#endif
#include <openssl/ssl.h>

#if defined (__cplusplus)
extern "C" {
#endif

/** @brief Read bytes from an SSL connection
 * @param[in,out] ssl SSL connection
 * @param[out] buf buffer of at least len bytes to store received bytes
 * @param[in] len size of buffer pointed to by buf
 * @param[out] bytes_read number of bytes read on successful completion, set to 0 in all other cases
 * @return return code indicating success or failure
 * @retval `DDS_RETCODE_OK` bytes read or EOF, `bytes_read` is 0 on EOF else in [1,len]
 * @retval `DDS_RETCODE_TRY_AGAIN` no bytes available but not EOF either
 * @retval `DDS_RETCODE_ERROR` unspecified error */
typedef dds_return_t (*ddsi_ssl_plugin_read_t) (SSL *ssl, void *buf, size_t len, size_t *bytes_read)
  ddsrt_nonnull((1, 2, 4)) ddsrt_attribute_warn_unused_result;

/** @brief Write bytes to an SSL connection
 * @param[in,out] ssl SSL connection
 * @param[in] msg data to write
 * @param[in] len number of bytes in msg
 * @param[out] bytes_written optional, number of bytes written on successful completion, undefined in all other cases
 * @return return code indicating success or failure
 * @retval `DDS_RETCODE_OK` bytes written or EOF, `bytes_written` is 0 on EOF else in [1,len]
 * @retval `DDS_RETCODE_TRY_AGAIN` no bytes written but not EOF either
 * @retval `DDS_RETCODE_ERROR` unspecified error */
typedef dds_return_t (*ddsi_ssl_plugin_write_t) (SSL *ssl, const void *msg, size_t len, size_t *bytes_written)
  ddsrt_nonnull((1, 2));

struct ddsi_ssl_plugins
{
  bool (*init) (struct ddsi_domaingv *gv);
  void (*fini) (void);
  void (*ssl_free) (SSL *ssl);
  void (*bio_vfree) (BIO *bio);
  ddsi_ssl_plugin_read_t read;
  ddsi_ssl_plugin_write_t write;
  SSL * (*connect) (const struct ddsi_domaingv *gv, ddsrt_socket_t sock);
  BIO * (*listen) (ddsrt_socket_t sock);
  SSL * (*accept) (const struct ddsi_domaingv *gv, BIO *bio, ddsrt_socket_t *sock);
};

/** @component legacy_ssl */
void ddsi_ssl_config_plugin (struct ddsi_ssl_plugins *plugin);

#if defined (__cplusplus)
}
#endif

#endif /* DDS_HAS_TCP_TLS */
#endif /* DDSI__SSL_H */
