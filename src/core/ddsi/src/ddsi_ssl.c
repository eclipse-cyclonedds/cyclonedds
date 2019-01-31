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
#include "os/os.h"
#include "ddsi/ddsi_tcp.h"
#include "ddsi/ddsi_ssl.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"

#ifdef DDSI_INCLUDE_SSL

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/opensslconf.h>

static SSL_CTX *ddsi_ssl_ctx = NULL;

static SSL *ddsi_ssl_new (void)
{
  return SSL_new (ddsi_ssl_ctx);
}

static void ddsi_ssl_error (SSL *ssl, const char *str, int err)
{
  char buff[128];
  ERR_error_string ((unsigned) SSL_get_error (ssl, err), buff);
  DDS_ERROR ("tcp/ssl %s %s %d\n", str, buff, err);
}

static int ddsi_ssl_verify (int ok, X509_STORE_CTX *store)
{
  if (!ok)
  {
    char issuer[256];
    X509 *cert = X509_STORE_CTX_get_current_cert (store);
    int err = X509_STORE_CTX_get_error (store);

    /* Check if allowing self-signed certificates */
    if (config.ssl_self_signed && ((err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) || (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN)))
      ok = 1;
    else
    {
      X509_NAME_oneline (X509_get_issuer_name (cert), issuer, sizeof (issuer));
      DDS_ERROR ("tcp/ssl failed to verify certificate from %s: %s\n", issuer, X509_verify_cert_error_string (err));
    }
  }
  return ok;
}

static ssize_t ddsi_ssl_read (SSL *ssl, void *buf, size_t len, int *err)
{
  assert (len <= INT32_MAX);
  if (SSL_get_shutdown (ssl) != 0)
    return -1;

  /* Returns -1 on error or 0 on shutdown */
  const int ret = SSL_read (ssl, buf, (int) len);
  switch (SSL_get_error (ssl, ret))
  {
    case SSL_ERROR_NONE:
      return ret;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      *err = os_sockEAGAIN;
      return -1;
    case SSL_ERROR_ZERO_RETURN:
    default:
      /* Connection closed or error */
      *err = os_getErrno ();
      return -1;
  }
}

static ssize_t ddsi_ssl_write (SSL *ssl, const void *buf, size_t len, int *err)
{
  assert(len <= INT32_MAX);

  if (SSL_get_shutdown (ssl) != 0)
    return -1;

  /* Returns -1 on error or 0 on shutdown */
  const int ret = SSL_write (ssl, buf, (int) len);
  switch (SSL_get_error (ssl, ret))
  {
    case SSL_ERROR_NONE:
      return ret;
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
      *err = os_sockEAGAIN;
      return -1;
    case SSL_ERROR_ZERO_RETURN:
    default:
      /* Connection closed or error */
      *err = os_getErrno ();
      return -1;
  }
}

/* Standard OpenSSL init and thread support routines. See O'Reilly. */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
static unsigned long ddsi_ssl_id (void)
{
  return (unsigned long) os_threadIdToInteger (os_threadIdSelf ());
}

typedef struct CRYPTO_dynlock_value {
  os_mutex m_mutex;
} CRYPTO_dynlock_value;

static CRYPTO_dynlock_value *ddsi_ssl_locks = NULL;

static void ddsi_ssl_dynlock_lock (int mode, CRYPTO_dynlock_value *lock, const char *file, int line)
{
  (void) file;
  (void) line;
  if (mode & CRYPTO_LOCK)
    os_mutexLock (&lock->m_mutex);
  else
    os_mutexUnlock (&lock->m_mutex);
}

static void ddsi_ssl_lock (int mode, int n, const char *file, int line)
{
  ddsi_ssl_dynlock_lock (mode, &ddsi_ssl_locks[n], file, line);
}

static CRYPTO_dynlock_value *ddsi_ssl_dynlock_create (const char *file, int line)
{
  (void) file;
  (void) line;
  CRYPTO_dynlock_value *val = os_malloc (sizeof (*val));
  os_mutexInit (&val->m_mutex);
  return val;
}

static void ddsi_ssl_dynlock_destroy (CRYPTO_dynlock_value *lock, const char *file, int line)
{
  (void) file;
  (void) line;
  os_mutexDestroy (&lock->m_mutex);
  os_free (lock);
}
#endif

static int ddsi_ssl_password (char *buf, int num, int rwflag, void *udata)
{
  (void) rwflag;
  (void) udata;
  if (num < 0 || (size_t) num < strlen (config.ssl_key_pass) + 1)
    return 0;
  OS_WARNING_MSVC_OFF(4996);
  strcpy (buf, config.ssl_key_pass);
  OS_WARNING_MSVC_ON(4996);
  return (int) strlen (config.ssl_key_pass);
}

static SSL_CTX *ddsi_ssl_ctx_init (void)
{
  SSL_CTX *ctx = SSL_CTX_new (SSLv23_method ());
  unsigned disallow_TLSv1_2;

  /* Load certificates */
  if (! SSL_CTX_use_certificate_file (ctx, config.ssl_keystore, SSL_FILETYPE_PEM))
  {
    DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl failed to load certificate from file: %s\n", config.ssl_keystore);
    goto fail;
  }

  /* Set password and callback */
  SSL_CTX_set_default_passwd_cb (ctx, ddsi_ssl_password);

  /* Get private key */
  if (! SSL_CTX_use_PrivateKey_file (ctx, config.ssl_keystore, SSL_FILETYPE_PEM))
  {
    DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl failed to load private key from file: %s\n", config.ssl_keystore);
    goto fail;
  }

  /* Load CAs */
  if (! SSL_CTX_load_verify_locations (ctx, config.ssl_keystore, 0))
  {
    DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl failed to load CA from file: %s\n", config.ssl_keystore);
    goto fail;
  }

  /* Set ciphers */
  if (! SSL_CTX_set_cipher_list (ctx, config.ssl_ciphers))
  {
    DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl failed to set ciphers: %s\n", config.ssl_ciphers);
    goto fail;
  }

  /* Load randomness from file (optional) */
  if (config.ssl_rand_file[0] != '\0')
  {
    if (! RAND_load_file (config.ssl_rand_file, 4096))
    {
      DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl failed to load random seed from file: %s\n", config.ssl_rand_file);
      goto fail;
    }
  }

  /* Set certificate verification policy from configuration */
  if (!config.ssl_verify)
    SSL_CTX_set_verify (ctx, SSL_VERIFY_NONE, NULL);
  else
  {
    int i = SSL_VERIFY_PEER;
    if (config.ssl_verify_client)
      i |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    SSL_CTX_set_verify (ctx, i, ddsi_ssl_verify);
  }
  switch (config.ssl_min_version.major)
  {
    case 1:
      switch (config.ssl_min_version.minor)
      {
        case 2:
          disallow_TLSv1_2 = 0;
          break;
        case 3:
#ifdef SSL_OP_NO_TLSv1_2
          disallow_TLSv1_2 = SSL_OP_NO_TLSv1_2;
#else
          DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl: openssl version does not support disabling TLSv1.2 as required by config\n");
          goto fail;
#endif
          break;
        default:
          DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl: can't set minimum requested TLS version to %d.%d\n", config.ssl_min_version.major, config.ssl_min_version.minor);
          goto fail;
      }
      break;
    default:
      DDS_LOG (DDS_LC_ERROR | DDS_LC_CONFIG, "tcp/ssl: can't set minimum requested TLS version to %d.%d\n", config.ssl_min_version.major, config.ssl_min_version.minor);
      goto fail;
  }
  SSL_CTX_set_options (ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1 | disallow_TLSv1_2);
  return ctx;

fail:
  SSL_CTX_free (ctx);
  return NULL;
}

static void dds_report_tls_version (const SSL *ssl, const char *oper)
{
  if (ssl)
  {
    char issuer[256], subject[256];
    X509_NAME_oneline (X509_get_issuer_name (SSL_get_peer_certificate (ssl)), issuer, sizeof (issuer));
    X509_NAME_oneline (X509_get_subject_name (SSL_get_peer_certificate (ssl)), subject, sizeof (subject));
    DDS_TRACE("tcp/ssl %s %s issued by %s [%s]\n", oper, subject, issuer, SSL_get_version (ssl));
  }
}

static SSL *ddsi_ssl_connect (os_socket sock)
{
  SSL *ssl;
  int err;

  /* Connect SSL over connected socket; on Win64 a SOCKET is 64-bit type is forced into an int by
     the OpenSSL API. Lots of software does use openssl on Win64, so it appears that it is
     safe to do so, and moreover, that it will remain safe to do so, given Microsoft's track
     record of maintaining backwards compatibility. The SSL API is in the wrong of course ... */
  ssl = ddsi_ssl_new ();
  OS_WARNING_MSVC_OFF(4244);
  SSL_set_fd (ssl, sock);
  OS_WARNING_MSVC_ON(4244);
  err = SSL_connect (ssl);
  if (err != 1)
  {
    ddsi_ssl_error (ssl, "connect failed", err);
    SSL_free (ssl);
    ssl = NULL;
  }
  dds_report_tls_version (ssl, "connected to");
  return ssl;
}

static BIO *ddsi_ssl_listen (os_socket sock)
{
  /* See comment in ddsi_ssl_connect concerning casting the socket to an int */
  BIO * bio = BIO_new (BIO_s_accept ());
  OS_WARNING_MSVC_OFF(4244);
  BIO_set_fd (bio, sock, BIO_NOCLOSE);
  OS_WARNING_MSVC_ON(4244);
  return bio;
}

static SSL *ddsi_ssl_accept (BIO *bio, os_socket *sock)
{
  SSL *ssl = NULL;
  BIO *nbio;
  int err;

  if (BIO_do_accept (bio) > 0)
  {
    nbio = BIO_pop (bio);
    *sock = (os_socket) BIO_get_fd (nbio, NULL);
    ssl = ddsi_ssl_new ();
    SSL_set_bio (ssl, nbio, nbio);
    err = SSL_accept (ssl);
    if (err <= 0)
    {
      SSL_free (ssl);
      *sock = OS_INVALID_SOCKET;
      ssl = NULL;
    }
  }
  dds_report_tls_version (ssl, "accepted from");
  return ssl;
}

static bool ddsi_ssl_init (void)
{
  ERR_load_BIO_strings ();
  SSL_load_error_strings ();
  SSL_library_init ();
  OpenSSL_add_all_algorithms ();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  {
    const int locks = CRYPTO_num_locks ();
    assert (locks >= 0);
    ddsi_ssl_locks = os_malloc (sizeof (CRYPTO_dynlock_value) * (size_t) locks);
    for (int i = 0; i < locks; i++)
      os_mutexInit (&ddsi_ssl_locks[i].m_mutex);
  }
#endif
  /* Leave these in place: OpenSSL 1.1 defines them as no-op macros that not even reference the symbol,
     therefore leaving them in means we get compile time errors if we the library expects the callbacks
     to be defined and we somehow failed to detect that previously */
  CRYPTO_set_id_callback (ddsi_ssl_id);
  CRYPTO_set_locking_callback (ddsi_ssl_lock);
  CRYPTO_set_dynlock_create_callback (ddsi_ssl_dynlock_create);
  CRYPTO_set_dynlock_lock_callback (ddsi_ssl_dynlock_lock);
  CRYPTO_set_dynlock_destroy_callback (ddsi_ssl_dynlock_destroy);
  ddsi_ssl_ctx = ddsi_ssl_ctx_init ();

  return (ddsi_ssl_ctx != NULL);
}

static void ddsi_ssl_fini (void)
{
  SSL_CTX_free (ddsi_ssl_ctx);
  CRYPTO_set_id_callback (0);
  CRYPTO_set_locking_callback (0);
  CRYPTO_set_dynlock_create_callback (0);
  CRYPTO_set_dynlock_lock_callback (0);
  CRYPTO_set_dynlock_destroy_callback (0);
  ERR_free_strings ();
  EVP_cleanup ();

#if OPENSSL_VERSION_NUMBER < 0x10100000L
  {
    const int locks = CRYPTO_num_locks ();
    for (int i = 0; i < locks; i++)
      os_mutexDestroy (&ddsi_ssl_locks[i].m_mutex);
    os_free (ddsi_ssl_locks);
  }
#endif
}

void ddsi_ssl_config_plugin (struct ddsi_ssl_plugins *plugin)
{
  plugin->init = ddsi_ssl_init;
  plugin->fini = ddsi_ssl_fini;
  plugin->ssl_free = SSL_free;
  plugin->bio_vfree = BIO_vfree;
  plugin->read = ddsi_ssl_read;
  plugin->write = ddsi_ssl_write;
  plugin->connect = ddsi_ssl_connect;
  plugin->listen = ddsi_ssl_listen;
  plugin->accept = ddsi_ssl_accept;
}

#endif /* DDSI_INCLUDE_SSL */
