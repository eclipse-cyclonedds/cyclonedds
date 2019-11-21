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
#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"

int
ddsrt_strcasecmp(
  const char *s1,
  const char *s2)
{
  int cr;

  while (*s1 && *s2) {
    cr = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (cr) {
      return cr;
    }
    s1++;
    s2++;
  }
  cr = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
  return cr;
}

int
ddsrt_strncasecmp(
  const char *s1,
  const char *s2,
  size_t n)
{
  int cr = 0;

  assert(s1 != NULL);
  assert(s2 != NULL);

  while (*s1 && *s2 && n) {
    cr = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    if (cr) {
      return cr;
    }
    s1++;
    s2++;
    n--;
  }
  if (n) {
    cr = tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
  }
  return cr;
}

char *
ddsrt_strsep(char **str, const char *sep)
{
  char *ret;
  if (**str == '\0')
    return 0;
  ret = *str;
  while (**str && strchr (sep, **str) == 0)
    (*str)++;
  if (**str != '\0')
  {
    **str = '\0';
    (*str)++;
  }
  return ret;
}

size_t
ddsrt_strlcpy(
  char * __restrict dest,
  const char * __restrict src,
  size_t size)
{
  size_t srclen = 0;

  assert(dest != NULL);
  assert(src != NULL);

  /* strlcpy must return the number of bytes that (would) have been written,
     i.e. the length of src. */
  srclen = strlen(src);
  if (size > 0) {
    size_t len = srclen;
    if (size <= srclen) {
      len = size - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
  }

  return srclen;
}

/* NOTE: ddsrt_strlcat does not forward to strlcat too avoid a bug in the macOS
         implementation where it does not return the right result if dest
         contains more characters than the size specified if size is either
         0 or 1. */
size_t
ddsrt_strlcat(
  char * __restrict dest,
  const char * __restrict src,
  size_t size)
{
  size_t destlen, srclen;

  assert(dest != NULL);
  assert(src != NULL);

  /* strlcat must return the number of bytes that (would) have been written,
     i.e. the length of dest plus the length of src. */
  destlen = strlen(dest);
  srclen = strlen(src);
  if (SIZE_MAX == destlen) {
    srclen = 0;
  } else if ((SIZE_MAX - destlen) <= srclen) {
    srclen = (SIZE_MAX - destlen) - 1;
  }
  if (size > 0 && --size > destlen) {
    size_t len = srclen;
    size -= destlen;
    if (size <= srclen) {
      len = size;
    }
    memcpy(dest + destlen, src, len);
    dest[destlen + len] = '\0';
  }

  return destlen + srclen;
}

void *
ddsrt_memdup(const void *src, size_t n)
{
  void *dest = NULL;

  if (n != 0 && (dest = ddsrt_malloc_s(n)) != NULL) {
    memcpy(dest, src, n);
  }

  return dest;
}

char *
ddsrt_strdup(
  const char *str)
{
  assert(str != NULL);

  return ddsrt_memdup(str, strlen(str) + 1);
}

char *
ddsrt_str_replace(
    const char *str,
    const char *srch,
    const char *subst,
    size_t max)
{
  char *r, *cur = (char *)str, *tmp;
  size_t lstr, lsrch, lsubst, cnt, offset;

  if (!str || !srch || !subst || !(lsrch = strlen(srch)))
    return NULL;
  lstr = strlen(str);
  lsubst = strlen(subst);
  for (cnt = 0; (cur < str + lstr) && (tmp = strstr(cur, srch)) && (max == 0 || cnt < max); cnt++)
    cur = tmp + lsrch;
  if (!(tmp = r = ddsrt_malloc(lstr + cnt * (lsubst - lsrch) + 1)))
    return NULL;
  while (cnt--)
  {
    cur = strstr(str, srch);
    offset = (size_t)(cur - str);
    strncpy(tmp, str, offset);
    tmp += offset;
    strcpy(tmp, subst);
    tmp += lsubst;
    str += offset + lsrch;
  }
  strcpy(tmp, str);
  return r;
}