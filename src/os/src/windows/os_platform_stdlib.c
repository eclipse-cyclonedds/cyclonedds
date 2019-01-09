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

#include "os/os.h"

#include "../snippets/code/os_stdlib_getopt.c"
#include "../snippets/code/os_stdlib_bsearch.c"
#include "../snippets/code/os_stdlib_strtod.c"
#include "../snippets/code/os_stdlib_strtol.c"
#include "../snippets/code/os_stdlib_strtok_r.c"
#include "../snippets/code/os_stdlib_strsep.c"
#include "../snippets/code/os_stdlib_strdup.c"
#include "../snippets/code/os_stdlib_memdup.c"
#include "../snippets/code/os_stdlib_asprintf.c"
#include "../snippets/code/os_stdlib_rindex.c"
#include "../snippets/code/os_stdlib_strcasecmp.c"
#include "../snippets/code/os_stdlib_strncasecmp.c"

os_result
os_gethostname(
        char *hostname,
        size_t buffersize)
{
        os_result result;
        char hostnamebuf[MAXHOSTNAMELEN];
        WORD wVersionRequested;
        WSADATA wsaData;
        int err;

        wVersionRequested = MAKEWORD(OS_SOCK_VERSION, OS_SOCK_REVISION);

        err = WSAStartup(wVersionRequested, &wsaData);
        if (err != 0) {
                DDS_FATAL("WSAStartup failed, no compatible socket implementation available\n");
                /* Tell the user that we could not find a usable */
                /* WinSock DLL.                                  */
                return os_resultFail;
        }
        if (gethostname(hostnamebuf, MAXHOSTNAMELEN) == 0) {
                if ((strlen(hostnamebuf) + 1) > (size_t)buffersize) {
                        result = os_resultFail;
                }
                else {
                        strcpy(hostname, hostnamebuf);
                        result = os_resultSuccess;
                }
        }
        else {
                result = os_resultFail;
        }
        return result;
}

#pragma warning( disable : 4996 )
_Ret_opt_z_ const char *
os_getenv(
        _In_z_ const char *variable)
{
        const char * result;
        result = getenv(variable);

        return result;
}
#pragma warning( default : 4996 )

os_result
os_putenv(
        char *variable_definition)
{
        os_result result;

        if (_putenv(variable_definition) == 0) {
                result = os_resultSuccess;
        }
        else {
                result = os_resultFail;
        }
        return result;
}

#pragma warning( disable : 4996 )
int
os_vsnprintf(
        char *str,
        size_t size,
        const char *format,
        va_list args)
{
        int result;

        /* Return-values of _vsnprintf don't match the output on posix platforms,
        * so this extra code is needed to bring it in accordance. It is made to
        * behave as follows (copied from printf man-pages):
        * Upon successful return, this function returns the number of characters
        * printed (not including the trailing '\0' used to end output to strings).
        * The function does not write more than size bytes (including the trailing
        * '\0').  If the output was truncated due to this limit then the return
        * value is the number of characters (not including the trailing '\0') which
        * would have been written to the final string if enough space had been
        * available. Thus, a return value of size or more means that the output was
        * truncated. If an output error is encountered, a negative value is
        * returned. */

        result = _vsnprintf(str, size, format, args);

        if (result == -1) {
                /* vsnprintf will return the length that would be written for the given
                * formatting arguments if invoked with NULL and 0 as the first two arguments.
                */
                result = _vsnprintf(NULL, 0, format, args);
        }

        /* Truncation occurred, so we need to guarantee that the string is NULL-
        * terminated. */
        if ((size_t)result >= size) {
                str[size - 1] = '\0';
        }
        return result;
}
#pragma warning( default : 4996 )

#if _MSC_VER < 1900
int
snprintf(
        char *s,
        size_t size,
        const char *format,
        ...)
{
        int result;
        va_list args;

        va_start(args, format);

        result = os_vsnprintf(s, size, format, args);

        va_end(args);

        return result;
}
#endif

ssize_t os_write(
        _In_ int fd,
        _In_reads_bytes_(count) void const* buf,
        _In_ size_t count)
{
        return _write(fd, buf, (unsigned int)count); /* Type casting is done for the warning of conversion from 'size_t' to 'unsigned int', which may cause possible loss of data */
}

int os_getopt(
		_In_range_(0, INT_MAX) int argc,
		_In_reads_z_(argc) char **argv,
		_In_z_ const char *opts)
{
	return getopt(argc, argv, opts);
}

void os_set_opterr(_In_range_(0, INT_MAX) int err)
{
	opterr = err;
}

int os_get_opterr(void)
{
	return opterr;
}

void os_set_optind(_In_range_(0, INT_MAX) int index)
{
	optind = index;
}

int os_get_optind(void)
{
	return optind;
}

int os_get_optopt(void)
{
	return optopt;
}

char * os_get_optarg(void)
{
	return optarg;
}
