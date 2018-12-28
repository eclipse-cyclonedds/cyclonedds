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
/****************************************************************
 * Interface definition for standard operating system features  *
 ****************************************************************/

/** \file os_stdlib.h
 *  \brief standard operating system features
 */

#ifndef OS_STDLIB_H
#define OS_STDLIB_H

#include <stdio.h>
#include <stdarg.h>
#include "os/os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif
    /* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

    /** \brief Get host or processor name
     *
     * Possible Results:
     * - assertion failure: hostname = NULL
     * - returns os_resultSuccess if
     *     hostname correctly identifies the name of the host
     * - returns os_resultFail if
     *     actual hostname is longer than buffersize
     */
    OSAPI_EXPORT os_result
    os_gethostname(
                   char *hostname,
                   size_t buffersize);

    /** \brief Get environment variable definition
     *
     * Postcondition:
     * - the pointer returned is only to be accessed readonly
     *
     * Possible Results:
     * - assertion failure: variable = NULL
     * - returns pointer to value of variable if
     *     variable is found
     * - returns NULL if
     *     variable is not found
     *
     * TODO CHAM-379 : Coverity generates a tainted string.
     * For now, the Coverity warning reported intentional in Coverity.
     */
    OSAPI_EXPORT _Ret_opt_z_ const char *
    os_getenv(
              _In_z_ const char *variable);

    /** \brief Set environment variable definition
     *
     * Precondition:
     * - variable_definition follows the format "<variable>=<value>"
     *
     * Possible Results:
     * - assertion failure: variable_definition = NULL
     * - returns os_resultSuccess if
     *     environment variable is set according the variable_definition
     * - returns os_resultFail if
     *     environment variable could not be set according the
     *     variable_definition
     * @deprecated This function is a thin, bordering on anorexic, wrapper to
     * putenv. putenv behaviour varies from unsafe to leaky across different
     * platforms. Use ::os_setenv instead.
     */
    OSAPI_EXPORT os_result
    os_putenv(
              char *variable_definition);

    /** \brief rindex wrapper
     *
     * because not all operating systems have
     * interfaces to rindex a wrapper is made
     *
     * Precondition:
     *   None
     *
     * Possible results:
     * - return NULL if
     *     char c is not found in string s
     * - return address of last occurance of c in s
     */
    OSAPI_EXPORT char *
    os_rindex(
              const char *s,
              int c);

    /** \brief index wrapper
     *
     * because not all operating systems have
     * interfaces to index a wrapper is made
     *
     * Precondition:
     *   None
     *
     * Possible results:
     * - return NULL if
     *     char c is not found in string s
     * - return address of first occurance of c in s
     */
    OSAPI_EXPORT char *
    os_index(
             const char *s,
             int c);

    /** \brief strdup wrapper
     *
     * because not all operating systems have
     * interfaces to strdup a wrapper is made
     *
     * Precondition:
     *   None
     * Postcondition:
     *   The allocated string must be freed using os_free
     *
     * Possible results:
     * - return duplicate of the string s1 allocated via
     *     os_malloc
     */
    _Ret_z_
    _Check_return_
    OSAPI_EXPORT char *
    os_strdup(
              _In_z_ const char *s1) __nonnull_all__
    __attribute_malloc__
    __attribute_returns_nonnull__
    __attribute_warn_unused_result__;

    void *
    os_memdup(const void *src, size_t n);

    /** \brief os_strsep wrapper
     *
     * See strsep()
     */
    OSAPI_EXPORT char *
    os_strsep(
              char **stringp,
              const char *delim);

    /** \brief write a formatted string to a newly allocated buffer
     */
    OSAPI_EXPORT int
    os_asprintf(
        char **strp,
        const char *fmt,
        ...);

    /** \brief os_vsnprintf wrapper
     *
     * Microsoft generates deprected warnings for vsnprintf,
     * wrapper removes warnings for win32
     *
     * Precondition:
     *   None
     * Postcondition:
     *   None
     *
     * Possible results:
     * - os_vsnprintf() does not write  more than size bytes (including the trailing '\0').
     *   If the output was truncated due to this limit then the return value is the
     *   number of  characters (not including the trailing '\0') which would have been
     *   written to the final string if enough space had been  available.
     *   Thus, a return value of size or more means that the output was truncated.
     */

    OSAPI_EXPORT int
    os_vsnprintf(
                 char *str,
                 size_t size,
                 const char *format,
                 va_list args);

    /** \brief strtoll wrapper
     *
     * Translate string str to long long value considering base,
     * and sign. If base is 0, base is determined from
     * str ([1-9]+ base = 10, 0x[0-9]+ base = 16,
     * 0X[0-9]+ base = 16, 0[0-9] base = 8).
     *
     * Precondition:
     *   errno is set to 0
     *
     * Possible results:
     * - return 0 and errno == EINVAL in case of conversion error
     * - return OS_LLONG_MIN and errno == ERANGE
     * - return OS_LLONG_MAX and errno == ERANGE
     * - return value(str)
     */
    OSAPI_EXPORT long long
    os_strtoll(
               const char *str,
               char **endptr,
               int32_t base);

    /** \brief strtoull wrapper
     *
     * Translate string str to unsigned long long value considering
     * base. If base is 0, base is determined from
     * str ([1-9]+ base = 10, 0x[0-9]+ base = 16,
     * 0X[0-9]+ base = 16, 0[0-9] base = 8).
     *
     * Precondition:
     *   errno is set to 0
     *
     * Possible results:
     * - return 0 and errno == EINVAL in case of conversion error
     * - return OS_ULLONG_MIN and errno == ERANGE
     * - return OS_ULLONG_MAX and errno == ERANGE
     * - return value(str)
     */
    OSAPI_EXPORT unsigned long long
    os_strtoull(
                const char *str,
                char **endptr,
                int32_t base);

    /** \brief atoll wrapper
     *
     * Translate string to long long value considering base 10.
     *
     * Precondition:
     *   errno is set to 0
     *
     * Possible results:
     * - return os_strtoll(str, 10)
     */
    OSAPI_EXPORT long long
    os_atoll(
             const char *str);

    /** \brief atoull wrapper
     *
     * Translate string to unsigned long long value considering base 10.
     *
     * Precondition:
     *   errno is set to 0
     *
     * Possible results:
     * - return os_strtoll(str, 10)
     */
    OSAPI_EXPORT unsigned long long
    os_atoull(
              const char *str);

    /** \brief lltostr wrapper
     *
     * Translate long long value into string representation based
     * on radix 10.
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return 0 and errno == EINVAL in case of conversion error
     * - return value(str)
     */
    OSAPI_EXPORT char *
    os_lltostr(
               long long num,
               char *str,
               size_t len,
               char **endptr);

    /** \brief ulltostr wrapper
     *
     * Translate unsigned long long value into string representation based
     * on radix 10.
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return 0 and errno == EINVAL in case of conversion error
     * - return value(str)
     */
    OSAPI_EXPORT char *
    os_ulltostr(
                unsigned long long num,
                char *str,
                size_t len,
                char **endptr);

    /** \brief strtod wrapper
     *
     * Translate string to double value considering base 10.
     *
     * Normal strtod is locale dependent, meaning that if you
     * provide "2.2" and lc_numeric is ',', then the result
     * would be 2. This function makes sure that both "2.2"
     * (which is mostly used) and "2,2" (which could be provided
     * by applications) are translated to 2.2 when the locale
     * indicates that lc_numeric is ','.
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return value(str)
     */
    OSAPI_EXPORT double
    os_strtod(const char *nptr, char **endptr);

    /** \brief strtof wrapper
     *
     * Translate string to float value considering base 10.
     *
     * Normal strtof is locale dependent, meaning that if you
     * provide "2.2" and lc_numeric is ',', then the result
     * would be 2. This function makes sure that both "2.2"
     * (which is mostly used) and "2,2" (which could be provided
     * by applications) are translated to 2.2 when the locale
     * indicates that lc_numeric is ','.
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return value(str)
     */
    OSAPI_EXPORT float
    os_strtof(const char *nptr, char **endptr);

    /** \brief os_strtod mirror
     *
     * Translate double to string considering base 10.
     *
     * The function dtostr doesn't exists and can be done by
     *     snprintf((char*)dst, "%f", (double)src).
     * But like strtod, snprint is locale dependent, meaning
     * that if you provide 2.2 and lc_numeric is ',' then the
     * result would be "2,2". This comma can cause problems
     * when serializing data to other nodes with different
     * locales.
     * This function makes sure that 2.2 is translated into
     * "2.2", whatever lc_numeric is set ('.' or ',').
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return value(str)
     */
    OSAPI_EXPORT int
    os_dtostr(double src, char *str, size_t size);

    /** \brief os_strtof mirror
     *
     * Translate float to string considering base 10.
     *
     * The function ftostr doesn't exists and can be done by
     *     snprintf((char*)dst, "%f", (float)src).
     * But like strtof, snprint is locale dependent, meaning
     * that if you provide 2.2 and lc_numeric is ',' then the
     * result would be "2,2". This comma can cause problems
     * when serializing data to other nodes with different
     * locales.
     * This function makes sure that 2.2 is translated into
     * "2.2", whatever lc_numeric is set ('.' or ',').
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return value(str)
     */
    OSAPI_EXPORT int
    os_ftostr(float src, char *str, size_t size);

    /** \brief strcasecm wrapper
     *
     * Compare 2 strings conform strcasecmp
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return 0 and s1 equals s2
     * - return <0 and s1 is less than s2
     * - return >0 and s1 is greater than s2
     */
    OSAPI_EXPORT int
    os_strcasecmp(
                  const char *s1,
                  const char *s2);

    /** \brief strncasecm wrapper
     *
     * Compare 2 strings conform strncasecmp
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return 0 and s1 equals s2 (maximum the first n characters)
     * - return <0 and s1 is less than s2 (maximum the first n characters)
     * - return >0 and s1 is greater than s2 (maximum the first n characters)
     */
    OSAPI_EXPORT int
    os_strncasecmp(
                   const char *s1,
                   const char *s2,
                   size_t n);

    /** \brief strtok_r wrapper
     *
     * Tokenise strings based on delim
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - return ptr to next token or NULL if there are no tokens left.
     */
    OSAPI_EXPORT char *
    os_strtok_r(char *str, const char *delim, char **saveptr);

    /**
     * \brief writes up to count bytes from the buffer pointed buf to the file referred to by the file descriptor fd.
     *
     *
     * Precondition:
     *   none
     *
     * Possible results:
     * - On success, the number of bytes written is returned (zero indicates
     * nothing was written). On error, -1 is returned
     *
     */
    OSAPI_EXPORT ssize_t os_write(
        _In_ int fd,
        _In_reads_bytes_(count) void const* buf,
        _In_ size_t count);

    /**
     * \brief binary search algorithm on an already sorted list.
     *
     *
     * Precondition:
     *   list must be sorted in ascending order
     *
     * Possible results:
     * - On success, the number the matching item is returned.  When the item
     * does not exist, NULL is returned.
     *
     */
    OSAPI_EXPORT void *
    os_bsearch(const void *key, const void *base, size_t nmemb, size_t size,
               int (*compar) (const void *, const void *));

    /**
	 * \brief the os_getopt function gets the next option argument from the argument
	 * list specified by the argv and argc arguments. Normally these values come
	 * directly from the arguments received by main.
	 *
	 * The opts argument is a string that specifies the option characters that are
	 * valid for this program. An option character in this string can be followed
	 * by a colon (‘:’) to indicate that it takes a required argument. If an option
	 * character is followed by two colons (‘::’), its argument is optional;
	 *
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - If an option was successfully found, then os_getopt() returns the option
	 * character.  If all command-line options have been parsed, then
	 * os_getopt() returns -1.  If os_getopt() encounters an option character that
	 * was not in opts, then '?' is returned.  If os_getopt() encounters
	 * an option with a missing argument, then the return value depends on
	 * the first character in opts: if it is ':', then ':' is returned;
	 * otherwise '?' is returned.
	 *
	 */
	OSAPI_EXPORT int
	os_getopt(
			_In_range_(0, INT_MAX) int argc,
			_In_reads_z_(argc) char **argv,
			_In_z_ const char *opts);

	/**
	 * \brief the os_set_opterr function sets the value of the opterr variable.
	 * opterr is used as a flag to suppress an error message generated by
	 * getopt. When opterr is set to 0, it suppresses the error message generated
	 * by getopt when that function does not recognize an option character.
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - No error information is returned.
	 * Set the value of the opterr variable.
	 *
	 */
	OSAPI_EXPORT void
	os_set_opterr(_In_range_(0, INT_MAX) int err);

	/**
	 * \brief the os_get_opterr returns the value of the opterr variable.
	 * opterr is used as a flag to suppress an error message generated by
	 * getopt. When opterr is set to 0, it suppresses the error message generated
	 * by getopt when that function does not recognize an option character.
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - return the value of the opterr variable.
	 *
	 */
	OSAPI_EXPORT int
	os_get_opterr(void);

	/**
	 * \brief the os_set_optind function sets the value of the optind variable.
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - No error information is returned.
	 * Set the value of the optind variable.
	 *
	 */
	OSAPI_EXPORT void
	os_set_optind(_In_range_(0, INT_MAX) int index);

	/**
	 * \brief the os_get_optind function returns the value of the optind variable.
	 * optind is set by getopt to the index of the next element of the argv
	 * array to be processed. Once getopt has found all of the option arguments,
	 * this variable can be used to determine where the remaining non-option
	 * arguments begin. The initial value of this variable is 1.
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - return the value of the optind variable.
	 *
	 */
	OSAPI_EXPORT int
	os_get_optind(void);

	/**
	 * \brief the os_get_optopt function returns the value of the optopt variable.
	 * optopt holds the unknown option character when that option
	 * character is not recognized by getopt.
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - return the value of the optopt variable.
	 *
	 */
	OSAPI_EXPORT int
	os_get_optopt(void);

	/**
	 * \brief the os_get_optarg function returns the value of the optarg variable.
	 * optarg is set by getopt to point at the value of the option
	 * argument, for those options that accept arguments.
	 *
	 * Precondition:
	 *   none
	 *
	 * Possible results:
	 * - return the value of the optarg variable.
	 *
	 */
	OSAPI_EXPORT char *
	os_get_optarg(void);


#if defined (__cplusplus)
}
#endif

#endif /* OS_STDLIB_H */
