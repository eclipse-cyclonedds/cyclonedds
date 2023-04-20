// Copyright(c) 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/atomics.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static unsigned long ssl_id (void)
{
  return (unsigned long) ddsrt_gettid ();
}

typedef struct CRYPTO_dynlock_value {
  ddsrt_mutex_t m_mutex;
} CRYPTO_dynlock_value;

CRYPTO_dynlock_value *dds_openssl102_ssl_locks = NULL;

static void ssl_dynlock_lock (int mode, CRYPTO_dynlock_value *lock, const char *file, int line)
{
  (void) file;
  (void) line;
  if (mode & CRYPTO_LOCK)
    ddsrt_mutex_lock (&lock->m_mutex);
  else
    ddsrt_mutex_unlock (&lock->m_mutex);
}

static void ssl_lock (int mode, int n, const char *file, int line)
{
  ssl_dynlock_lock (mode, &dds_openssl102_ssl_locks[n], file, line);
}

static CRYPTO_dynlock_value *ssl_dynlock_create (const char *file, int line)
{
  (void) file;
  (void) line;
  CRYPTO_dynlock_value *val = ddsrt_malloc (sizeof (*val));
  ddsrt_mutex_init (&val->m_mutex);
  return val;
}

static void ssl_dynlock_destroy (CRYPTO_dynlock_value *lock, const char *file, int line)
{
  (void) file;
  (void) line;
  ddsrt_mutex_destroy (&lock->m_mutex);
  ddsrt_free (lock);
}

void dds_openssl_init (void)
{
  // This is terribly fragile and broken-by-design, but with OpenSSL sometimes
  // linked dynamically and sometimes linked statically, with Windows and Unix
  // in the mix, this appears to be the compromise that makes it work reliably
  // enough ...
  if (CRYPTO_get_id_callback () == 0)
  {
    CRYPTO_set_id_callback (ssl_id);
    CRYPTO_set_locking_callback (ssl_lock);
    CRYPTO_set_dynlock_create_callback (ssl_dynlock_create);
    CRYPTO_set_dynlock_lock_callback (ssl_dynlock_lock);
    CRYPTO_set_dynlock_destroy_callback (ssl_dynlock_destroy);

    if (dds_openssl102_ssl_locks == NULL)
    {
      const int locks = CRYPTO_num_locks ();
      assert (locks >= 0);
      dds_openssl102_ssl_locks = ddsrt_malloc (sizeof (CRYPTO_dynlock_value) * (size_t) locks);
      for (int i = 0; i < locks; i++)
        ddsrt_mutex_init (&dds_openssl102_ssl_locks[i].m_mutex);
    }

    OpenSSL_add_all_algorithms ();
    OpenSSL_add_all_ciphers ();
    OpenSSL_add_all_digests ();
    ERR_load_BIO_strings ();
    ERR_load_crypto_strings ();
  }
}
#else
void dds_openssl_init (void)
{
  // nothing needed for OpenSSL 1.1.0 and later
}
#endif

void DDS_Security_Exception_set_with_openssl_error (DDS_Security_SecurityException *ex, const char *context, int code, int minor_code, const char *error_area)
{
  BIO *bio;
  assert (context);
  assert (error_area);
  assert (ex);
  DDSRT_UNUSED_ARG (context);

  if ((bio = BIO_new (BIO_s_mem ()))) {
    ERR_print_errors (bio);
    char *buf = NULL;
    size_t len = (size_t) BIO_get_mem_data (bio, &buf);
    size_t exception_msg_len = len + strlen (error_area) + 1;
    char *str = ddsrt_malloc (exception_msg_len);
    ddsrt_strlcpy (str, error_area, exception_msg_len);
    if (len > 0) {
      memcpy (str + strlen (error_area), buf, len);
    }
    str[exception_msg_len - 1] = '\0';
    ex->message = str;
    ex->code = code;
    ex->minor_code = minor_code;
    BIO_free (bio);
  } else {
    DDS_Security_Exception_set (ex, context, code, minor_code, "BIO_new failed");
  }
}
