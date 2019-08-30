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
#ifndef DDSRT_STRING_H
#define DDSRT_STRING_H

#include "dds/export.h"
#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/retcode.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Compare two strings ignoring case.
 *
 * @param[in]  s1  String to compare.
 * @param[in]  s2  String to compare.
 *
 * @returns Zero if @s1 and @s2 match, a negative integer if @s1 is less than
 *          @s2 or a positive integer if @s1 is greater than @s2.
 */
DDS_EXPORT int
ddsrt_strcasecmp(
  const char *s1,
  const char *s2)
ddsrt_nonnull_all;

/**
 * @brief Compare two strings ignoring case, but no more than @n bytes.
 *
 * @param[in]  s1  String to compare.
 * @param[in]  s2  String to compare.
 * @param[in]  n   Maximum number of bytes to compare.
 *
 * @returns Zero if @s1 and @s2 match, a negative integer if @s1 is less than
 *          @s2 or a positive integer if @s1 is greater than @s2.
 */
DDS_EXPORT int
ddsrt_strncasecmp(
  const char *s1,
  const char *s2,
  size_t n)
ddsrt_nonnull((1,2));

/**
 * @brief Extract token from string.
 *
 * Finds the first token in @stringp delimited by one of the characters in
 * @delim. The delimiter is overwritten with a null byte, terminating the
 * token and @stringp is updated to point past the delimiter.
 *
 * @param[in,out] stringp  String to extract token from.
 * @param[in]     delim    Characters that delimit a token.
 *
 * @returns The original value of @stringp.
 */
DDS_EXPORT char *
ddsrt_strsep(
  char **stringp,
  const char *delim);

/**
 * @brief Duplicate block of memory.
 *
 * Copy a block of memory into a newly allocated block of memory. The memory
 * is obtained with @ddsrt_malloc and must be freed with @ddsrt_free when it
 * is no longer used.
 *
 * @param[in]  ptr  Pointer to block of memory to duplicate.
 * @param[in]  len  Number of bytes to copy into newly allocated buffer.
 *
 * @returns A new block of memory that is a duplicate of the block pointed to
 *          by @ptr or NULL if not enough memory was available.
 */
DDS_EXPORT void *
ddsrt_memdup(
  const void *ptr,
  size_t len)
ddsrt_nonnull((1))
ddsrt_attribute_malloc;

/**
 * @brief Duplicate string.
 *
 * Copy string into a newly allocated block of memory. The memory is obtained
 * with @ddsrt_malloc and must freed with @ddsrt_free when it is no longer
 * used.
 *
 * @param[in]  str  String to duplicate.
 *
 * @returns A new string that is a duplicate of @str or NULL if not enough
 *          memory was available.
 */
DDS_EXPORT char *
ddsrt_strdup(
  const char *str)
ddsrt_nonnull_all
ddsrt_attribute_malloc;

/**
 * @brief Copy string.
 *
 * Copy string to buffer with specified size. The string is truncated if there
 * is not enough space. The resulting string is guaranteed to be null
 * terminated if there is space.
 *
 * @param[out]  dest  Destination buffer.
 * @param[in]   src   Null terminated string to copy to dest.
 * @param[in]   size  Number of bytes available in dest.
 *
 * @returns Number of characters copied to dest (excluding the null byte), or
 *          the number of characters that would have been copied if dest is not
 *          sufficiently large enough.
 */
DDS_EXPORT
size_t
ddsrt_strlcpy(
  char * __restrict dest,
  const char * __restrict src,
  size_t size)
ddsrt_nonnull((1,2));

/**
 * @brief Concatenate strings.
 *
 * Append the string specified by src to the string specified by dest. The
 * terminating null byte at the end of dest is overwritten. The resulting
 * string is truncated if there is not enough space. The resulting string
 * guaranteed to be null terminated if there is space.
 *
 * @param[in,out] dest  Destination buffer.
 * @param[in]     src   Null terminated string to append to dest.
 * @param[in]     size  Number of bytes available in dest.
 *
 * @returns Number of characters copied to dest (excluding the null byte), or
 *          the number of characters that would have been copied if dest is not
 *          sufficiently large enough.
 */
DDS_EXPORT
size_t
ddsrt_strlcat(
  char * __restrict dest,
  const char * __restrict src,
  size_t size)
ddsrt_nonnull((1,2));

/**
 * @brief Get description for specified system error number.
 *
 * @param[in]  errnum  System error number.
 * @param[in]  buf     Buffer where description is copied to.
 * @param[in]  buflen  Number of bytes available in @buf.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Description for @errnum was successfully copied to @buf.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Unknown error number.
 * @retval DDS_RETCODE_NOT_ENOUGH_SPACE
 *             Buffer was not large enough to hold the description.
 */
DDS_EXPORT dds_return_t
ddsrt_strerror_r(
    int errnum,
    char *buf,
    size_t buflen);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_STRING_H */
