// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "dds/ddsrt/io.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/strtod.h"

// Determine the maximum size that a string should have to be
// able to contain a double.
// See the following site for the calculation explanation:
// http://stackoverflow.com/questions/1701055/what-is-the-maximum-length-in-chars-needed-to-represent-any-double-value

#include <float.h>
#define DOUBLE_STRING_MAX_LENGTH (3 + DBL_MANT_DIG - DBL_MIN_EXP)

/*
 * VALID_DOUBLE_CHAR(c) is used to determine if the given char
 * can be valid when it appears in a string that should represent
 * a double.
 * It is used to detect the end of a double string representation.
 * Because it doesn't consider context, it is possible that more
 * characters are detected after the double (fi. when a few white
 * spaces tail the double). This isn't that bad, because the call
 * to strtod itself will handle these extra characters properly.
 */
#define VALID_DOUBLE_CHAR(c) ( (isspace((unsigned char)(c)))  || /* (leading) whitespaces   */ \
                               (isxdigit((unsigned char)(c))) || /* (hexa)decimal digits    */ \
                               ((c) == '.')                   || /* ospl LC_NUMERIC         */ \
                               ((c) == os_lcNumericGet())     || /* locale LC_NUMERIC       */ \
                               ((c) == '+') || ((c) == '-')   || /* signs                   */ \
                               ((c) == 'x') || ((c) == 'X')   || /* hexadecimal indication  */ \
                               ((c) == 'e') || ((c) == 'E')   || /* exponent chars          */ \
                               ((c) == 'p') || ((c) == 'P')   || /* binary exponent chars   */ \
                               ((c) == 'a') || ((c) == 'A')   || /* char for NaN            */ \
                               ((c) == 'n') || ((c) == 'N')   || /* char for NaN & INFINITY */ \
                               ((c) == 'i') || ((c) == 'I')   || /* char for INFINITY       */ \
                               ((c) == 'f') || ((c) == 'F')   || /* char for INFINITY       */ \
                               ((c) == 't') || ((c) == 'T')   || /* char for INFINITY       */ \
                               ((c) == 'y') || ((c) == 'Y'))     /* char for INFINITY       */



/** \brief Detect and return the LC_NUMERIC char of the locale.
 */
static char
os_lcNumericGet(void)
{
  static char lcNumeric = ' ';

  /* Detect lcNumeric only once. */
  if (lcNumeric == ' ') {
    /* There could be multiple threads here, but it is still save and works.
     * Only side effect is that possibly multiple os_reports are traced. */
    char num[] = { '\0', '\0', '\0', '\0' };
    (void) snprintf(num, 4, "%3.1f", 2.2);
    lcNumeric = num [1];
    if (lcNumeric != '.') {
      DDS_WARNING("Locale with LC_NUMERIC \'%c\' detected, which is not '.'. "
                  "This can decrease performance.", lcNumeric);
    }
  }

  return lcNumeric;
}


/** \brief Replace lcNumeric char with '.' in floating point string when locale dependent
 *      functions use a non '.' LC_NUMERIC, while we want locale indepenent '.'.
 */
static void
os_lcNumericReplace(char *str) {
  /* We only need to replace when standard functions
   * did not put a '.' in the result string. */
  char lcNumeric = os_lcNumericGet();
  if (lcNumeric != '.') {
    str = strchr(str, lcNumeric);
    if (str != NULL) {
      *str = '.';
    }
  }
}

/** @brief Delocalize a floating point string to use '.' as its decimal separator.
 *  @param[in] nptr the localized floating point string.
 *  @param[out] nptrCopy the delocalized floating point string (needs to have a length of DOUBLE_STRING_MAX_LENGTH).
 *  @param[out] nptrCopyEnd a pointer to the end of the delocalized floating point string.
 */
static void delocalize_floating_point_str(const char *nptr, char *nptrCopy, char **nptrCopyEnd)
{
    /* The current locale uses ',', so we can not use the standard functions as
       is, but have to do extra work because ospl uses "x.x" doubles (notice
       the dot). Just copy the string and replace the LC_NUMERIC. */
    char *nptrCopyIdx;
    char *nptrIdx;

    /* It is possible that the given nptr just starts with a double
       representation but continues with other data. To be able to copy not too
       much and not too little, we have to scan across nptr until we detect the
       doubles' end. */
    nptrIdx = (char*)nptr;
    nptrCopyIdx = nptrCopy;
    *nptrCopyEnd = nptrCopyIdx + DOUBLE_STRING_MAX_LENGTH - 1;
    while (VALID_DOUBLE_CHAR(*nptrIdx) && (nptrCopyIdx < *nptrCopyEnd)) {
      if (*nptrIdx == '.') {
        /* Replace '.' with locale LC_NUMERIC to get strtod to work. */
        *nptrCopyIdx = os_lcNumericGet();
      } else {
        *nptrCopyIdx = *nptrIdx;
      }
      nptrIdx++;
      nptrCopyIdx++;
    }
    *nptrCopyIdx = '\0';
}

dds_return_t
ddsrt_strtod(const char *nptr, char **endptr, double *dblptr)
{
  double dbl;
  int orig_errno;
  dds_return_t ret = DDS_RETCODE_OK;
  char *string_end = NULL;
  bool successfully_parsed = false;

  assert(nptr != NULL);
  assert(dblptr != NULL); 

  orig_errno = errno;
  if (os_lcNumericGet() == '.') {
    errno = 0;
    /* The current locale uses '.', so strtod can be used as is. */
    dbl = strtod(nptr, &string_end);

    /* Check that something was parsed */
    if (nptr != string_end) {
      successfully_parsed = true;
    }
    
    /* Set the proper end char when needed. */
    if (endptr != NULL) {
      *endptr = string_end;
    }
  } else {
    /* The current locale has to be normalized to use '.' for the floating
       point string. */
    char  nptrCopy[DOUBLE_STRING_MAX_LENGTH];
    delocalize_floating_point_str(nptr, nptrCopy, &string_end);

    /* Now that we have a copy with the proper locale LC_NUMERIC, we can use
       strtod() for the conversion. */
    errno = 0;
    dbl = strtod(nptrCopy, &string_end);

    /* Check that something was parsed */
    if (nptrCopy != string_end) {
      successfully_parsed = true;
    }

    /* Calculate the proper end char when needed. */
    if (endptr != NULL) {
      *endptr = (char*)nptr + (string_end - nptrCopy);
    }
  }

  // There are two erroring scenarios from `strtod`.
  //
  // 1. The floating point value to be parsed is too large:
  //    In this case `strtod` sets `errno` to `ERANGE` and will return a
  //    parsed value of either `-HUGE_VAL` or `HUGE_VAL` depending on the
  //    initial sign present in the string.
  //
  // 2. The string contains a non-numeric prefix:
  //    In the case junk is passed in `strtod` parses nothing. As a result,
  //    the value that is returned corresponds to `0.0`. To differentiate
  //    between the parsing scenarios of junk being passed in versus a valid
  //    floating point string that parses to 0 (such as "0.0") `strtod` also
  //    ensures that the provided end pointer == start pointer in the case
  //    that a junk prefix is encountered.
  //
  // The two other scenarios that we want to reject are:
  //
  // 3. The value being parsed results in `-nan` or `nan`.
  // 4. The value being parsed results in `-inf` or `inf`.
  if (errno == ERANGE || !successfully_parsed
      || isnan(dbl) || isinf(dbl)) {
    ret = DDS_RETCODE_OUT_OF_RANGE;
  } else {
    errno = orig_errno;
  }

  *dblptr = dbl;

  return ret;
}

int
ddsrt_dtostr(double src, char *str, size_t size)
{
  int i;

  assert(str != NULL);

  /* Use locale dependent standard function. */
  i = snprintf(str, size, "%0.15g", src);
  /* Make sure the result is a locale independent "x.x" double. */
  os_lcNumericReplace(str);
  return i;
}

int
ddsrt_ftostr(float src, char *str, size_t size)
{
  int i;

  assert(str != NULL);

  /* Use locale dependent standard function. */
  i = snprintf(str, size, "%0.7g", src);
  /* Make sure the result is a locale independent "x.x" float. */
  os_lcNumericReplace(str);
  return i;
}
