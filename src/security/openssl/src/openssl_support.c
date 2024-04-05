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

void dds_openssl_init (void)
{
  // nothing needed for OpenSSL 1.1.0 and later
}

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
