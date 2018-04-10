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
#include "ddsi/ddsi_ssl.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "os/os_heap.h"
#include "ddsi/ddsi_tcp.h"

#ifdef DDSI_INCLUDE_SSL

#include <openssl/rand.h>
#include <openssl/err.h>

static SSL_CTX * ddsi_ssl_ctx = NULL;

static SSL * ddsi_ssl_new (void)
{
  return SSL_new (ddsi_ssl_ctx);
}

static void ddsi_ssl_error (SSL * ssl, const char * str, int err)
{
  char buff [128];
  ERR_error_string ((unsigned) SSL_get_error (ssl, err), buff);
  nn_log (LC_ERROR, "tcp/ssl %s %s %d\n", str, buff, err);
}

static int ddsi_ssl_verify (int ok, X509_STORE_CTX * store)
{
  if (!ok)
  {
    char issuer[256];
    X509 * cert = X509_STORE_CTX_get_current_cert (store);
    int err = X509_STORE_CTX_get_error (store);

    /* Check if allowing self-signed certificates */

    if
    (
      config.ssl_self_signed &&
      ((err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) ||
      (err == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN))
    )
    {
      ok = 1;
    }
    else
    {
      X509_NAME_oneline (X509_get_issuer_name (cert), issuer, sizeof (issuer));
      nn_log
      (
        LC_ERROR,
        "tcp/ssl failed to verify certificate from %s : %s\n",
        issuer,
        X509_verify_cert_error_string (err)
      );
    }
  }
  return ok;
}

static os_ssize_t ddsi_ssl_read (SSL * ssl, void * buf, os_size_t len, int * err)
{
  int ret;

  assert (len <= INT32_MAX);

  if (SSL_get_shutdown (ssl) != 0)
  {
    return -1;
  }

  /* Returns -1 on error or 0 on shutdown */

  ret = SSL_read (ssl, buf, (int) len);
  switch (SSL_get_error (ssl, ret))
  {
    case SSL_ERROR_NONE:
    {
      /* Success */

      break;
    }
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    {
      *err = os_sockEAGAIN;
      ret = -1;
      break;
    }
    case SSL_ERROR_ZERO_RETURN:
    default:
    {
      /* Connection closed or error */

      *err = os_getErrno ();
      ret = -1;
      break;
    }
  }

  return ret;
}

static os_ssize_t ddsi_ssl_write (SSL * ssl, const void * buf, os_size_t len, int * err)
{
  int ret;

  assert(len <= INT32_MAX);

  if (SSL_get_shutdown (ssl) != 0)
  {
    return -1;
  }

  /* Returns -1 on error or 0 on shutdown */

  ret = SSL_write (ssl, buf, (int) len);
  switch (SSL_get_error (ssl, ret))
  {
    case SSL_ERROR_NONE:
    {
      /* Success */

      break;
    }
    case SSL_ERROR_WANT_READ:
    case SSL_ERROR_WANT_WRITE:
    {
      *err = os_sockEAGAIN;
      ret = -1;
      break;
    }
    case SSL_ERROR_ZERO_RETURN:
    default:
    {
      /* Connection closed or error */

      *err = os_getErrno ();
      ret = -1;
      break;
    }
  }

  return ret;
}

/* Standard OpenSSL init and thread support routines. See O'Reilly. */

static unsigned long ddsi_ssl_id (void)
{
  return os_threadIdToInteger (os_threadIdSelf ());
}

typedef struct CRYPTO_dynlock_value
{
  os_mutex m_mutex;
}
CRYPTO_dynlock_value;

static CRYPTO_dynlock_value * ddsi_ssl_locks = NULL;

static void ddsi_ssl_dynlock_lock (int mode, CRYPTO_dynlock_value * lock, const char * file, int line)
{
  (void) file;
  (void) line;
  if (mode & CRYPTO_LOCK)
  {
    os_mutexLock (&lock->m_mutex);
  }
  else
  {
    os_mutexUnlock (&lock->m_mutex);
  }
}

static void ddsi_ssl_lock (int mode, int n, const char * file, int line)
{
  ddsi_ssl_dynlock_lock (mode, &ddsi_ssl_locks[n], file, line);
}

static CRYPTO_dynlock_value * ddsi_ssl_dynlock_create (const char * file, int line)
{
  CRYPTO_dynlock_value * val = os_malloc (sizeof (*val));

  (void) file;
  (void) line;
  os_mutexInit (&val->m_mutex);
  return val;
}

static void ddsi_ssl_dynlock_destroy (CRYPTO_dynlock_value * lock, const char * file, int line)
{
  (void) file;
  (void) line;
  os_mutexDestroy (&lock->m_mutex);
  os_free (lock);
}

static int ddsi_ssl_password (char * buf, int num, int rwflag, void * udata)
{
  (void) rwflag;
  (void) udata;
  if ((unsigned int) num < strlen (config.ssl_key_pass) + 1)
  {
    return (0);
  }
  strcpy (buf, config.ssl_key_pass);
  return (int) strlen (config.ssl_key_pass);
}

static SSL_CTX * ddsi_ssl_ctx_init (void)
{
  int i;
  SSL_CTX * ctx = SSL_CTX_new (TLSv1_method ());

  /* Load certificates */

  if (! SSL_CTX_use_certificate_file (ctx, config.ssl_keystore, SSL_FILETYPE_PEM))
  {
    nn_log
    (
      LC_ERROR | LC_CONFIG,
      "tcp/ssl failed to load certificate from file: %s\n",
      config.ssl_keystore
    );
    goto fail;
  }

  /* Set password and callback */

  SSL_CTX_set_default_passwd_cb (ctx, ddsi_ssl_password);

  /* Get private key */

  if (! SSL_CTX_use_PrivateKey_file (ctx, config.ssl_keystore, SSL_FILETYPE_PEM))
  {
    nn_log
    (
      LC_ERROR | LC_CONFIG,
      "tcp/ssl failed to load private key from file: %s\n",
      config.ssl_keystore
    );
    goto fail;
  }

  /* Load CAs */

  if (! SSL_CTX_load_verify_locations (ctx, config.ssl_keystore, 0))
  {
    nn_log
    (
      LC_ERROR | LC_CONFIG,
      "tcp/ssl failed to load CA from file: %s\n",
      config.ssl_keystore
    );
    goto fail;
  }

  /* Set ciphers */

  if (! SSL_CTX_set_cipher_list (ctx, config.ssl_ciphers))
  {
    nn_log
    (
      LC_ERROR | LC_CONFIG,
      "tcp/ssl failed to set ciphers: %s\n",
      config.ssl_ciphers
    );
    goto fail;
  }

  /* Load randomness from file (optional) */

  if (config.ssl_rand_file[0] != '\0')
  {
    if (! RAND_load_file (config.ssl_rand_file, 4096))
    {
      nn_log
      (
        LC_ERROR | LC_CONFIG,
        "tcp/ssl failed to load random seed from file: %s\n",
        config.ssl_rand_file
      );
      goto fail;
    }
  }

  /* Set certificate verification policy from configuration */

  if (config.ssl_verify)
  {
    i = SSL_VERIFY_PEER;
    if (config.ssl_verify_client)
    {
      i |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    }
    SSL_CTX_set_verify (ctx, i, ddsi_ssl_verify);
  }
  else
  {
    SSL_CTX_set_verify (ctx, SSL_VERIFY_NONE, NULL);
  }
  SSL_CTX_set_options (ctx, SSL_OP_ALL | SSL_OP_NO_SSLv2);

  return ctx;

fail:

  SSL_CTX_free (ctx);
  return NULL;
}

static SSL * ddsi_ssl_connect (os_socket sock)
{
  SSL * ssl;
  int err;

  /* Connect SSL over connected socket */

  ssl = ddsi_ssl_new ();
  SSL_set_fd (ssl, sock);
  err = SSL_connect (ssl);
  if (err != 1)
  {
    ddsi_ssl_error (ssl, "connect failed", err);
    SSL_free (ssl);
    ssl = NULL;
  }
  return ssl;
}

static BIO * ddsi_ssl_listen (os_socket sock)
{
  BIO * bio = BIO_new (BIO_s_accept ());
  BIO_set_fd (bio, sock, BIO_NOCLOSE);
  return bio;
}

static SSL * ddsi_ssl_accept (BIO * bio, os_socket * sock)
{
  SSL * ssl = NULL;
  BIO * nbio;
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
      *sock = Q_INVALID_SOCKET;
      ssl = NULL;
    }
  }
  return ssl;
}

static c_bool ddsi_ssl_init (void)
{
  unsigned locks = (unsigned) CRYPTO_num_locks ();
  unsigned i;

  ddsi_ssl_locks = os_malloc (sizeof (CRYPTO_dynlock_value) * locks);
  for (i = 0; i < locks; i++)
  {
    os_mutexInit (&ddsi_ssl_locks[i].m_mutex);
  }
  ERR_load_BIO_strings ();
  SSL_load_error_strings ();
  SSL_library_init ();
  OpenSSL_add_all_algorithms ();
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
  unsigned locks = (unsigned) CRYPTO_num_locks ();
  unsigned i;

  SSL_CTX_free (ddsi_ssl_ctx);
  CRYPTO_set_id_callback (NULL);
  CRYPTO_set_locking_callback (NULL);
  CRYPTO_set_dynlock_create_callback (NULL);
  CRYPTO_set_dynlock_lock_callback (NULL);
  CRYPTO_set_dynlock_destroy_callback (NULL);
  ERR_free_strings ();
  EVP_cleanup ();
  for (i = 0; i < locks; i++)
  {
    os_mutexDestroy (&ddsi_ssl_locks[i].m_mutex);
  }
  os_free (ddsi_ssl_locks);
}

static void ddsi_ssl_config (void)
{
  if (config.ssl_enable)
  {
    ddsi_tcp_ssl_plugin.init = ddsi_ssl_init;
    ddsi_tcp_ssl_plugin.fini = ddsi_ssl_fini;
    ddsi_tcp_ssl_plugin.ssl_free = SSL_free;
    ddsi_tcp_ssl_plugin.bio_vfree = BIO_vfree;
    ddsi_tcp_ssl_plugin.read = ddsi_ssl_read;
    ddsi_tcp_ssl_plugin.write = ddsi_ssl_write;
    ddsi_tcp_ssl_plugin.connect = ddsi_ssl_connect;
    ddsi_tcp_ssl_plugin.listen = ddsi_ssl_listen;
    ddsi_tcp_ssl_plugin.accept = ddsi_ssl_accept;
  }
}

void ddsi_ssl_plugin (void)
{
  ddsi_tcp_ssl_plugin.config = ddsi_ssl_config;
}

#endif /* DDSI_INCLUDE_SSL */
