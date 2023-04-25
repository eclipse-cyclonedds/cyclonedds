// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "access_control_utils.h"

#define SEQ_ERR -1
#define SEQ_NOMATCH 0
#define SEQ_MATCH 1

static bool load_X509_certificate_from_bio (BIO *bio, X509 **x509Cert, DDS_Security_SecurityException *ex)
{
  assert(x509Cert);
  if (!(*x509Cert = PEM_read_bio_X509(bio, NULL, NULL, NULL)))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CERTIFICATE_CODE, 0, DDS_SECURITY_ERR_INVALID_CERTICICATE_MESSAGE ": ");
    return false;
  }
  return true;
}

static BIO *load_file_into_BIO (const char *filename, DDS_Security_SecurityException *ex)
{
  // File BIOs exist so one doesn't have to do all this, but it requires an application
  // on Windows that linked OpenSSL via a DLL to incorporate OpenSSL's applink.c, and
  // that is too onerous a requirement.
  BIO *bio;
  FILE *fp;
  size_t remain, n;
  char tmp[512];

  // One can fopen() a directory, after which the calls to fread() fail and the function
  // sets an error code different from the expected one in `ex`.  This check solves that,
  // and given that it gets us the size, we can use that afterward to limit how much we
  // try to read.
  if ((remain = ac_regular_file_size(filename)) == 0)
  {
    DDS_Security_Exception_set (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE, DDS_SECURITY_VALIDATION_FAILED, "load_file_into_BIO: " DDS_SECURITY_ERR_INVALID_FILE_PATH_MESSAGE, filename);
    return NULL;
  }

  if ((bio = BIO_new (BIO_s_mem ())) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ALLOCATION_FAILED_CODE, DDS_SECURITY_VALIDATION_FAILED, "load_file_into_BIO: BIO_new_mem (BIO_s_mem ()): ");
    return NULL;
  }

  DDSRT_WARNING_MSVC_OFF(4996);
  if ((fp = fopen(filename, "r")) == NULL)
  {
    DDS_Security_Exception_set (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE, DDS_SECURITY_VALIDATION_FAILED, "load_file_into_BIO: " DDS_SECURITY_ERR_INVALID_FILE_PATH_MESSAGE, filename);
    goto err_fopen;
  }
  DDSRT_WARNING_MSVC_ON(4996);

  // Try reading before testing remain: that way the EOF flag will always get set
  // if indeed we do read to the end of the file
  while ((n = fread (tmp, 1, sizeof (tmp), fp)) > 0 && remain > 0)
  {
    if (!BIO_write (bio, tmp, (int) n))
    {
      DDS_Security_Exception_set_with_openssl_error (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "load_X509_certificate_from_file: failed to append data to BIO: ");
      goto err_bio_write;
    }
    // protect against truncation while reading
    remain -= (n <= remain) ? n : remain;
  }
  if (!feof (fp))
  {
    DDS_Security_Exception_set (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "load_X509_certificate_from_file: read from file failed");
    goto err_fread;
  }
  fclose (fp);
  return bio;

 err_fread:
 err_bio_write:
  fclose (fp);
 err_fopen:
  BIO_free (bio);
  return NULL;
}

bool ac_X509_certificate_from_data(const char *data, int len, X509 **x509Cert, DDS_Security_SecurityException *ex)
{
  BIO *bio;
  assert(data);
  assert(len >= 0);
  assert(x509Cert);
  if (!(bio = BIO_new_mem_buf((void *)data, len)))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED, "BIO_new_mem_buf failed: ");
    return false;
  }
  else
  {
    const bool result = load_X509_certificate_from_bio (bio, x509Cert, ex);
    BIO_free(bio);
    return result;
  }
}

static bool ac_X509_certificate_from_file(const char *filename, X509 **x509Cert, DDS_Security_SecurityException *ex)
{
  assert(filename);
  assert(x509Cert);

  BIO *bio;
  if ((bio = load_file_into_BIO (filename, ex)) == NULL)
    return false;
  else
  {
    const bool result = load_X509_certificate_from_bio (bio, x509Cert, ex);
    BIO_free(bio);
    return result;
  }
}

bool ac_X509_certificate_read(const char *data, X509 **x509Cert, DDS_Security_SecurityException *ex)
{
  bool result = false;
  char *contents = NULL;
  assert(data);
  assert(x509Cert);

  switch (DDS_Security_get_conf_item_type(data, &contents))
  {
  case DDS_SECURITY_CONFIG_ITEM_PREFIX_FILE:
    result = ac_X509_certificate_from_file(contents, x509Cert, ex);
    break;
  case DDS_SECURITY_CONFIG_ITEM_PREFIX_DATA:
    result = ac_X509_certificate_from_data(contents, (int)strlen(contents), x509Cert, ex);
    break;
  case DDS_SECURITY_CONFIG_ITEM_PREFIX_PKCS11:
    DDS_Security_Exception_set(
        ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CERTIFICATE_TYPE_NOT_SUPPORTED_CODE, 0,
        DDS_SECURITY_ERR_CERTIFICATE_TYPE_NOT_SUPPORTED_MESSAGE " (pkcs11)");
    break;
  default:
    DDS_Security_Exception_set(
        ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CERTIFICATE_TYPE_NOT_SUPPORTED_CODE, 0,
        DDS_SECURITY_ERR_CERTIFICATE_TYPE_NOT_SUPPORTED_MESSAGE);
    break;
  }
  ddsrt_free(contents);
  return result;
}

char *ac_get_certificate_subject_name(X509 *cert, DDS_Security_SecurityException *ex)
{
  X509_NAME *name;
  BIO *bio;
  char *subject = NULL;
  char *pmem;
  size_t sz;
  assert(cert);
  if (!(bio = BIO_new(BIO_s_mem())))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ALLOCATION_FAILED_CODE, 0, DDS_SECURITY_ERR_ALLOCATION_FAILED_MESSAGE ": ");
    goto err_bio_alloc;
  }
  if (!(name = X509_get_subject_name(cert)))
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_CODE, 0, DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_MESSAGE ": ");
    goto err_get_subject;
  }

  /* TODO: check if this is the correct format of the subject name: check spec */
  X509_NAME_print_ex(bio, name, 0, XN_FLAG_RFC2253);

  sz = (size_t) BIO_get_mem_data(bio, &pmem);
  subject = ddsrt_malloc(sz + 1);

  if (BIO_gets(bio, subject, (int)sz + 1) < 0)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_CODE, 0, DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_MESSAGE ": ");
    ddsrt_free(subject);
    subject = NULL;
  }
  BIO_free(bio);
  return subject;

err_get_subject:
  BIO_free(bio);
err_bio_alloc:
  return NULL;
}

static bool PKCS7_document_from_data(const char *data, size_t len, PKCS7 **p7, BIO **bcont, DDS_Security_SecurityException *ex)
{
  BIO *bio;
  assert(data);
  assert(p7);
  assert(bcont);

  *bcont = NULL;
  assert (len < INT32_MAX);
  if ((bio = BIO_new_mem_buf((void *)data, (int)len)) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ALLOCATION_FAILED_CODE, 0, DDS_SECURITY_ERR_ALLOCATION_FAILED_MESSAGE ": ");
    return false;
  }
  if ((*p7 = SMIME_read_PKCS7(bio, bcont)) == NULL)
  {
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE, 0, DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_MESSAGE ": ");
    BIO_free(bio);
    return false;
  }
  BIO_free(bio);
  return true;
}

static bool PKCS7_document_verify(PKCS7 *p7, X509 *cert, BIO *inbio, BIO **outbio, DDS_Security_SecurityException *ex)
{
  bool result = false;
  STACK_OF(X509) *certStack = NULL;

  assert(p7);
  assert(cert);
  assert(inbio);
  assert(outbio);

  if ((*outbio = BIO_new(BIO_s_mem())) == NULL)
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ALLOCATION_FAILED_CODE, 0, DDS_SECURITY_ERR_ALLOCATION_FAILED_MESSAGE ": ");
  else if ((certStack = sk_X509_new_null()) == NULL)
    DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ALLOCATION_FAILED_CODE, 0, DDS_SECURITY_ERR_ALLOCATION_FAILED_MESSAGE ": ");
  else
  {
    sk_X509_push(certStack, cert);
    if (PKCS7_verify(p7, certStack, NULL, inbio, *outbio, PKCS7_TEXT | PKCS7_NOVERIFY | PKCS7_NOINTERN) != 1)
      DDS_Security_Exception_set_with_openssl_error(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_CODE, 0, DDS_SECURITY_ERR_INVALID_SMIME_DOCUMENT_MESSAGE ": ");
    else
      result = true;
  }
  if (certStack)
      sk_X509_free(certStack);
  if (!result && *outbio)
  {
    BIO_free(*outbio);
    *outbio = NULL;
  }
  return result;
}

bool ac_PKCS7_document_check(const char *data, size_t len, X509 *cert, char **document, DDS_Security_SecurityException *ex)
{
  bool result = false;
  PKCS7 *p7;
  BIO *bcont, *bdoc;
  char *pmem;
  size_t sz;

  assert(data);
  assert(cert);
  assert(document);

  if (!PKCS7_document_from_data(data, len, &p7, &bcont, ex))
    goto err_read_data;

  if (!PKCS7_document_verify(p7, cert, bcont, &bdoc, ex))
    goto err_verify;

  sz = (size_t) BIO_get_mem_data(bdoc, &pmem);
  *document = ddsrt_malloc(sz + 1);
  memcpy(*document, pmem, sz);
  (*document)[sz] = '\0';
  result = true;
  BIO_free(bdoc);

err_verify:
  PKCS7_free(p7);
  BIO_free(bcont);
err_read_data:
  return result;
}

static bool string_to_properties(const char *str, DDS_Security_PropertySeq *properties)
{
  char *copy = ddsrt_strdup (str), *cursor = copy, *tok;
  while ((tok = ddsrt_strsep (&cursor, ",/|")) != NULL)
  {
    if (strlen(tok) == 0)
      continue;
    char *name = ddsrt_strsep (&tok, "=");
    if (name == NULL || tok == NULL || properties->_length >= properties->_maximum)
    {
      ddsrt_free (copy);
      return false;
    }
    properties->_buffer[properties->_length].name = ddsrt_strdup(name);
    properties->_buffer[properties->_length].value = ddsrt_strdup(tok);
    properties->_length++;
  }
  ddsrt_free (copy);
  return true;
}

bool ac_check_subjects_are_equal(const char *permissions_sn, const char *identity_sn)
{
  bool result = false;
  char *copy_idsn = ddsrt_strdup (identity_sn), *cursor_idsn = copy_idsn, *tok_idsn;
  DDS_Security_PropertySeq prop_pmsn;
  prop_pmsn._length = 0;
  prop_pmsn._maximum = 20;
  prop_pmsn._buffer = ddsrt_malloc(prop_pmsn._maximum * sizeof(DDS_Security_Property_t));

  if (!string_to_properties(permissions_sn, &prop_pmsn))
    goto check_subj_equal_failed;

  while ((tok_idsn = ddsrt_strsep (&cursor_idsn, ",/|")) != NULL)
  {
    char *value_pmsn;
    char *name_idsn = ddsrt_strsep (&tok_idsn, "=");
    if (name_idsn == NULL || tok_idsn == NULL)
      goto check_subj_equal_failed;
    value_pmsn = DDS_Security_Property_get_value(&prop_pmsn, name_idsn);
    if (value_pmsn == NULL || strcmp(tok_idsn, value_pmsn) != 0)
    {
      ddsrt_free(value_pmsn);
      goto check_subj_equal_failed;
    }
    ddsrt_free(value_pmsn);
  }
  result = true;

check_subj_equal_failed:
  ddsrt_free(copy_idsn);
  DDS_Security_PropertySeq_deinit(&prop_pmsn);
  return result;
}

size_t ac_regular_file_size(const char *filename)
{
  if (filename)
  {
#if _WIN32
    struct _stat stat_info;
    if (_stat (filename, &stat_info) == 0)
      if (stat_info.st_mode & _S_IFREG)
        return (size_t) stat_info.st_size;
#else
    struct stat stat_info;
    if (stat (filename, &stat_info) == 0)
      if (S_ISREG(stat_info.st_mode))
        return (size_t) stat_info.st_size;
#endif
  }
  return 0;
}

static int sequencematch(const char *pat, char c, char **new_pat)
{
  char patc = *pat;
  char rpatc;
  const bool neg = (patc == '!');
  bool m = false;

  if (neg)
    ++pat;
  for (patc = *pat; patc != ']'; pat++)
  {
    patc = *pat;
    if (patc == '\0')
      return SEQ_ERR;
    if (*(pat + 1) == '-')
    {
      rpatc = *(pat + 2);
      if (rpatc == '\0' || rpatc == ']')
        return SEQ_ERR;
      if ((uint8_t)patc <= (uint8_t)c && (uint8_t)c <= (uint8_t)rpatc)
        m = true;
      pat += 2;
    }
    else if (patc == c)
      m = true;
  }
  *new_pat = (char *) pat;
  return (m != neg) ? SEQ_MATCH : SEQ_NOMATCH;
}

bool ac_fnmatch(const char* pat, const char* str)
{
  char patc;
  bool ret;
  char *new_pat;

  assert(pat != NULL);
  assert(str != NULL);

  for (;;)
  {
    switch (patc = *pat++)
    {
    case '\0':
      return (*str == '\0');
    case '?':
      if (*str == '\0')
        return false;
      ++str;
      break;
    case '*':
      patc = *pat;
      while (patc == '*')
        patc = *++pat;
      if (patc == '\0')
        return true;
      while (*str != '\0')
      {
        ret = ac_fnmatch(pat, str);
        if (ret)
          return true;
        ++str;
      }
      return false;
      break;
    case '[':
      if (*str == '\0')
        return false;
      switch (sequencematch(pat, *str, &new_pat))
      {
      case SEQ_MATCH:
        pat = new_pat;
        ++str;
        break;
      case SEQ_NOMATCH:
      case SEQ_ERR:
        return false;
      }
      break;
    default: /* Regular character */
      if (*str != patc)
        return false;
      str++;
      break;
    }
  }
}

