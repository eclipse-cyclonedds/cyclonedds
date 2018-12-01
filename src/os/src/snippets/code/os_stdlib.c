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
#ifndef PIKEOS_POSIX
#include <strings.h>
#endif
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

const char *
os_fileSep(void)
{
    return "/";
}

const char *
os_pathSep(void)
{
    return ":";
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

int
os_vfprintfnosigpipe(
    FILE *file,
    const char *format,
    va_list args)
{
   int result;

   sigset_t sset_before, sset_omask, sset_pipe, sset_after;

   sigemptyset(&sset_pipe);
   sigaddset(&sset_pipe, SIGPIPE);
   sigpending(&sset_before);
   pthread_sigmask(SIG_BLOCK, &sset_pipe, &sset_omask);
   result = vfprintf(file, format, args);
   sigpending(&sset_after);
   if (!sigismember(&sset_before, SIGPIPE) && sigismember(&sset_after, SIGPIPE)) {
       /* sigtimedwait appears to be fairly well supported, just not by Mac OS. If other platforms prove to be a problem, we can do a proper indication of platform support in the os defs. The advantage of sigtimedwait is that it protects against a deadlock when SIGPIPE is sent from outside the program and all threads have it blocked. In any case, when SIGPIPE is sent in this manner and we consume the signal here, the signal is lost. Nobody should be abusing system-generated signals in this manner. */
#ifndef __APPLE__
       struct timespec timeout = { 0, 0 };
       sigtimedwait(&sset_pipe, NULL, &timeout);
#else
       int sig;
       sigwait(&sset_pipe, &sig);
#endif
#ifndef NDEBUG
       sigpending(&sset_after);
       assert(!sigismember(&sset_after, SIGPIPE));
#endif
       os_setErrno(EPIPE);
       result = -1;
   }
   pthread_sigmask(SIG_SETMASK, &sset_omask, NULL);
   return result;
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
