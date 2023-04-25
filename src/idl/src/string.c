// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

/*
 * Locale-independent C runtime functions, like strtoull_l and strtold_l, are
 * available on modern operating systems albeit with some quirks.
 *
 * Linux exports newlocale and freelocale from locale.h if _GNU_SOURCE is
 * defined. strtoull_l and strtold_l are exported from stdlib.h, again if
 * _GNU_SOURCE is defined.
 *
 * freeBSD and macOS export newlocale and freelocale from xlocale.h and
 * export strtoull_l and strtold_l from xlocale.h if stdlib.h is included
 * before.
 *
 * Windows exports _create_locale and _free_locale from locale.h and exports
 * _strtoull_l and _strtold_l from stdlib.h.
 */
#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define _GNU_SOURCE
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(__USE_GNU) || !defined(__APPLE__) || !defined(__FreeBSD__)
  #define __MUSL__
#endif

#if defined _WIN32
# include <locale.h>
typedef _locale_t locale_t;
#else
# include <pthread.h>
# include <strings.h>
# if __APPLE__ || __FreeBSD__
#   include <xlocale.h>
# else
#   include <locale.h>
# endif
#endif

#include "idl/stream.h"
#include "idl/heap.h"
#include "idl/string.h"

static locale_t posix_locale(void);

#if _WIN32
int idl_isalnum(int c) { return _isalnum_l(c, posix_locale()); }
int idl_isalpha(int c) { return _isalpha_l(c, posix_locale()); }
//int idl_isblank(int c) { return _isblank_l(c, posix_locale()); }
int idl_iscntrl(int c) { return _iscntrl_l(c, posix_locale()); }
int idl_isgraph(int c) { return _isgraph_l(c, posix_locale()); }
int idl_islower(int c) { return _islower_l(c, posix_locale()); }
int idl_isprint(int c) { return _isprint_l(c, posix_locale()); }
int idl_ispunct(int c) { return _ispunct_l(c, posix_locale()); }
int idl_isspace(int c) { return _isspace_l(c, posix_locale()); }
int idl_isupper(int c) { return _isupper_l(c, posix_locale()); }
int idl_toupper(int c) { return _toupper_l(c, posix_locale()); }
int idl_tolower(int c) { return _tolower_l(c, posix_locale()); }
#else
int idl_isalnum(int c) { return isalnum_l(c, posix_locale()); }
int idl_isalpha(int c) { return isalpha_l(c, posix_locale()); }
int idl_isblank(int c) { return isblank_l(c, posix_locale()); }
int idl_iscntrl(int c) { return iscntrl_l(c, posix_locale()); }
int idl_isgraph(int c) { return isgraph_l(c, posix_locale()); }
int idl_islower(int c) { return islower_l(c, posix_locale()); }
int idl_isprint(int c) { return isprint_l(c, posix_locale()); }
int idl_ispunct(int c) { return ispunct_l(c, posix_locale()); }
int idl_isspace(int c) { return isspace_l(c, posix_locale()); }
int idl_isupper(int c) { return isupper_l(c, posix_locale()); }
int idl_toupper(int c) { return toupper_l(c, posix_locale()); }
int idl_tolower(int c) { return tolower_l(c, posix_locale()); }
#endif

int idl_isdigit(int chr, int base)
{
  int num = -1;
  assert(base > 0 && base < 36);
  if (chr >= '0' && chr <= '9')
    num = chr - '0';
  else if (chr >= 'a' && chr <= 'z')
    num = chr - 'a';
  else if (chr >= 'A' && chr <= 'Z')
    num = chr - 'A';
  return num != -1 && num < base ? num : -1;
}

int idl_strcasecmp(const char *s1, const char *s2)
{
  assert(s1);
  assert(s2);
#if _WIN32
  return _stricmp_l(s1, s2, posix_locale());
#else
  return strcasecmp_l(s1, s2, posix_locale());
#endif
}

int idl_strncasecmp(const char *s1, const char *s2, size_t n)
{
  assert(s1);
  assert(s2);
#if _WIN32
  return _strnicmp_l(s1, s2, n, posix_locale());
#else
  return strncasecmp_l(s1, s2, n, posix_locale());
#endif
}

char *idl_strdup(const char *str)
{
#if _WIN32
  return _strdup(str);
#else
  return strdup(str);
#endif
}

char *idl_strndup(const char *str, size_t len)
{
  char *s;
  size_t n;
  for (n=0; n < len && str[n]; n++) ;
  assert(n <= len);
  if (!(s = idl_malloc(n + 1)))
    return NULL;
  memmove(s, str, n);
  s[n] = '\0';
  return s;
}

size_t idl_strlcpy(char * __restrict dest, const char * __restrict src, size_t size)
{
  size_t srclen;

  assert(dest != NULL);
  assert(src != NULL);

  /* strlcpy must return the number of bytes that (would) have been written, i.e. the length of src. */
  srclen = strlen(src);
  if (size > 0) {
    size_t len = srclen;
    if (size <= srclen)
      len = size - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
  }

  return srclen;
}


int idl_vsnprintf(char *str, size_t size, const char *fmt, va_list ap)
{
#if _WIN32
#if _MSC_VER
__pragma(warning(push))
__pragma(warning(disable: 4996))
#endif
  int ret;
  va_list ap2;
  /* _vsprintf_p_l supports positional parameters */
  va_copy(ap2, ap);
  if ((ret = _vsprintf_p_l(str, size, fmt, posix_locale(), ap)) < 0)
    ret = _vscprintf_p_l(fmt, posix_locale(), ap2);
  va_end(ap2);
  return ret;
#if _MSC_VER
__pragma(warning(pop))
#endif
#elif __APPLE__ || __FreeBSD__
  return vsnprintf_l(str, size, posix_locale(), fmt, ap);
#else
  int ret;
  locale_t loc, posixloc = posix_locale();
  loc = uselocale(posixloc);
  ret = vsnprintf(str, size, fmt, ap);
  loc = uselocale(loc);
  assert(loc == posixloc);
  return ret;
#endif
}

int idl_snprintf(char *str, size_t size, const char *fmt, ...)
{
  int ret;
  va_list ap;

  va_start(ap, fmt);
  ret = idl_vsnprintf(str, size, fmt, ap);
  va_end(ap);
  return ret;
}

int idl_asprintf(char **strp, const char *fmt, ...)
{
  int ret;
  unsigned int len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list ap1, ap2;

  assert(strp != NULL);
  assert(fmt != NULL);

  va_start(ap1, fmt);
  va_copy(ap2, ap1); /* va_list cannot be reused */

  if ((ret = idl_vsnprintf(buf, sizeof(buf), fmt, ap1)) >= 0) {
    len = (unsigned int)ret; /* +1 for null byte */
    if ((str = idl_malloc(len + 1)) == NULL) {
      ret = -1;
    } else if ((ret = idl_vsnprintf(str, len + 1, fmt, ap2)) >= 0) {
      assert(((unsigned int)ret) == len);
      *strp = str;
    } else {
      idl_free(str);
    }
  }

  va_end(ap1);
  va_end(ap2);

  return ret;
}
int idl_vasprintf(char **strp, const char *fmt, va_list ap)
{
  int ret;
  unsigned int len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list ap2;

  assert(strp != NULL);
  assert(fmt != NULL);

  va_copy(ap2, ap); /* va_list cannot be reused */

  if ((ret = idl_vsnprintf(buf, sizeof(buf), fmt, ap)) >= 0) {
    len = (unsigned int)ret;
    if ((str = idl_malloc(len + 1)) == NULL) {
      ret = -1;
    } else if ((ret = idl_vsnprintf(str, len + 1, fmt, ap2)) >= 0) {
      assert(((unsigned int)ret) == len);
      *strp = str;
    } else {
      idl_free(str);
    }
  }

  va_end(ap2);

  return ret;
}

unsigned long long idl_strtoull(const char *str, char **endptr, int base)
{
  assert(str);
  assert(base >= 0 && base <= 36);
#ifdef __MUSL__
  return strtoull(str, endptr, base);
#elif _WIN32
#if __GNUC__
  return strtoull(str, endptr, base);
#else
  return _strtoull_l(str, endptr, base, posix_locale());
#endif
#else
  return strtoull_l(str, endptr, base, posix_locale());
#endif
}

long double idl_strtold(const char *str, char **endptr)
{
  assert(str);
#if _WIN32
#if __GNUC__
  return strtold(str, endptr);
#else
  return _strtold_l(str, endptr, posix_locale());
#endif
#else
  return strtold_l(str, endptr, posix_locale());
#endif
}

char *idl_strtok_r(char *str, const char *delim, char **saveptr)
{
#if _WIN32
  return strtok_s(str, delim, saveptr);
#else
  return strtok_r(str, delim, saveptr);
#endif
}

/* requires posix_locale */
int idl_vfprintf(FILE *fp, const char *fmt, va_list ap)
{
  assert(fp);
  assert(fmt);

#if _WIN32
  /* _vfprintf_p_l supports positional parameters */
  return _vfprintf_p_l(fp, fmt, posix_locale(), ap);
#elif __APPLE__ || __FreeBSD__
  return vfprintf_l(fp, posix_locale(), fmt, ap);
#else
  int ret;
  locale_t loc, posixloc = posix_locale();
  loc = uselocale(posixloc);
  ret = vfprintf(fp, fmt, ap);
  loc = uselocale(loc);
  assert(loc == posixloc);
  return ret;
#endif
}

int idl_fprintf(FILE *fp, const char *fmt, ...)
{
  int ret;
  va_list ap;

  assert(fp);
  assert(fmt);

  va_start(ap, fmt);
  ret = idl_vfprintf(fp, fmt, ap);
  va_end(ap);

  return ret;
}

FILE *idl_fopen(const char *pathname, const char *mode)
{
#if _MSC_VER
  FILE *fp = NULL;

  if (fopen_s(&fp, pathname, mode) != 0)
    return NULL;
  return fp;
#else
  return fopen(pathname, mode);
#endif
}

int idl_fclose(FILE *fp)
{
  return fclose(fp);
}


#if defined _WIN32
static DWORD locale = TLS_OUT_OF_INDEXES;

#if defined __MINGW32__
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wmissing-prototypes\"")
#endif
void WINAPI idl_cdtor(PVOID handle, DWORD reason, PVOID reserved)
{
  locale_t loc;

  (void)handle;
  (void)reason;
  (void)reserved;
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      if ((locale = TlsAlloc()) == TLS_OUT_OF_INDEXES)
        goto err_alloc;
      if (!(loc = _create_locale(LC_ALL, "C")))
        goto err_locale;
      if (TlsSetValue(locale, loc))
        return;
      _free_locale(loc);
err_locale:
      TlsFree(locale);
err_alloc:
      abort();
      /* never reached */
    case DLL_THREAD_ATTACH:
      assert(locale != TLS_OUT_OF_INDEXES);
      if (!(loc = _create_locale(LC_ALL, "C")))
        abort();
      if (TlsSetValue(locale, loc))
        return;
      _free_locale(loc);
      abort();
      break;
    case DLL_THREAD_DETACH:
      assert(locale != TLS_OUT_OF_INDEXES);
      loc = TlsGetValue(locale);
      if (loc && TlsSetValue(locale, NULL))
        _free_locale(loc);
      break;
    case DLL_PROCESS_DETACH:
      assert(locale != TLS_OUT_OF_INDEXES);
      loc = TlsGetValue(locale);
      if (loc)
        _free_locale(loc);
      TlsSetValue(locale, NULL);
      TlsFree(locale);
      locale = TLS_OUT_OF_INDEXES;
      break;
    default:
      break;
  }
}
#if defined __MINGW32__
_Pragma("GCC diagnostic pop")
#endif

#if defined __MINGW32__
  PIMAGE_TLS_CALLBACK __crt_xl_tls_callback__ __attribute__ ((section(".CRT$XLZ"))) = idl_cdtor;
#elif defined _WIN64
  #pragma comment (linker, "/INCLUDE:_tls_used")
  #pragma comment (linker, "/INCLUDE:tls_callback_func")
  #pragma const_seg(".CRT$XLZ")
  EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = idl_cdtor;
  #pragma const_seg()
#else
  #pragma comment (linker, "/INCLUDE:__tls_used")
  #pragma comment (linker, "/INCLUDE:_tls_callback_func")
  #pragma data_seg(".CRT$XLZ")
  EXTERN_C PIMAGE_TLS_CALLBACK tls_callback_func = idl_cdtor;
  #pragma data_seg()
#endif /* _WIN32 */

static locale_t posix_locale(void)
{
  return TlsGetValue(locale);
}
#else /* _WIN32 */
static pthread_key_t key;
static pthread_once_t once = PTHREAD_ONCE_INIT;

static void free_locale(void *ptr)
{
  freelocale((locale_t)ptr);
}

static void make_key(void)
{
  (void)pthread_key_create(&key, free_locale);
}

static locale_t posix_locale(void)
{
  locale_t locale;
  (void)pthread_once(&once, make_key);
  if ((locale = pthread_getspecific(key)))
    return locale;
#if __APPLE__ || __FreeBSD__
  locale = newlocale(LC_ALL_MASK, NULL, NULL);
#else
  locale = newlocale(LC_ALL, "C", (locale_t)0);
#endif
  pthread_setspecific(key, locale);
  return locale;
}
#endif /* _WIN32 */
