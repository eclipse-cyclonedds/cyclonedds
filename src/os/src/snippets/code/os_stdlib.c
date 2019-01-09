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
#include "os/os.h"

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>

#include "os_stdlib_strsep.c"
#include "os_stdlib_memdup.c"
#include "os_stdlib_rindex.c"
#include "os_stdlib_asprintf.c"
#include "os_stdlib_strcasecmp.c"
#include "os_stdlib_strncasecmp.c"
#include "os_stdlib_strdup.c"

_Ret_opt_z_ const char *
os_getenv(
    _In_z_ const char *variable)
{
    return getenv(variable);
}

os_result
os_putenv(
    char *variable_definition)
{
    os_result result;

    if (putenv (variable_definition) == 0) {
        result = os_resultSuccess;
    } else {
        result = os_resultFail;
    }
    return result;
}

int
os_vsnprintf(
   char *str,
   size_t size,
   const char *format,
   va_list args)
{
   return vsnprintf(str, size, format, args);
}

ssize_t os_write(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

int os_getopt(int argc, char **argv, const char *opts)
{
        return getopt(argc, argv, opts);
}

void os_set_opterr(int err)
{
        opterr = err;
}

int os_get_opterr(void)
{
        return opterr;
}

void os_set_optind(int index)
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
