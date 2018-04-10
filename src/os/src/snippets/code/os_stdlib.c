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

os_result
os_access(
    const char *file_path,
    int32_t permission)
{
    os_result result;
#ifdef VXWORKS_RTP
    /* The access function is broken for vxworks RTP for some filesystems
       so best ignore the result, and assume the user has correct permissions */
    (void) file_path;
    (void) permission;
    result = os_resultSuccess;
#else
    if (access (file_path, permission) == 0) {
        result = os_resultSuccess;
    } else {
        result = os_resultFail;
    }
#endif
    return result;
}

char *
os_rindex(
    const char *s,
    int c)
{
    char *last = NULL;

    while (*s) {
        if (*s == c) {
            last = (char *)s;
        }
        s++;
    }
    return last;
}

_Ret_z_
_Check_return_
char *
os_strdup(
    _In_z_ const char *s1)
{
    size_t len;
    char *dup;

    len = strlen(s1) + 1;
    dup = os_malloc(len);
    memcpy(dup, s1, len);

    return dup;
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

int
os_strcasecmp(
    const char *s1,
    const char *s2)
{
    int cr;

    while (*s1 && *s2) {
        cr = tolower(*s1) - tolower(*s2);
        if (cr) {
            return cr;
        }
        s1++;
        s2++;
    }
    cr = tolower(*s1) - tolower(*s2);
    return cr;
}

int
os_strncasecmp(
    const char *s1,
    const char *s2,
    size_t n)
{
    int cr = 0;

    while (*s1 && *s2 && n) {
        cr = tolower(*s1) - tolower(*s2);
        if (cr) {
            return cr;
        }
        s1++;
        s2++;
        n--;
    }
    if (n) {
        cr = tolower(*s1) - tolower(*s2);
    }
    return cr;
}

os_result
os_stat(
    const char *path,
    struct os_stat *buf)
{
    os_result result;
    struct stat _buf;
    int r;

    r = stat(path, &_buf);
    if (r == 0) {
        buf->stat_mode = _buf.st_mode;
        buf->stat_size = (size_t) _buf.st_size;
        buf->stat_mtime.tv_sec = (os_timeSec) _buf.st_mtime;
        buf->stat_mtime.tv_nsec = 0;
        result = os_resultSuccess;
    } else {
        result = os_resultFail;
    }

    return result;
}

os_result os_remove (const char *pathname)
{
    return (remove (pathname) == 0) ? os_resultSuccess : os_resultFail;
}

os_result os_rename (const char *oldpath, const char *newpath)
{
    return (rename (oldpath, newpath) == 0) ? os_resultSuccess : os_resultFail;
}

/* The result of os_fileNormalize should be freed with os_free */
char *
os_fileNormalize(
    const char *filepath)
{
    char *norm;
    const char *fpPtr;
    char *normPtr;

    norm = NULL;
    if ((filepath != NULL) && (*filepath != '\0')) {
        norm = os_malloc(strlen(filepath) + 1);
        /* replace any / or \ by OS_FILESEPCHAR */
        fpPtr = (char *) filepath;
        normPtr = norm;
        while (*fpPtr != '\0') {
            *normPtr = *fpPtr;
            if ((*fpPtr == '/') || (*fpPtr == '\\')) {
                *normPtr = OS_FILESEPCHAR;
                normPtr++;
            } else {
                if (*fpPtr != '\"') {
                    normPtr++;
                }
            }
            fpPtr++;
        }
        *normPtr = '\0';
    }

    return norm;
}

os_result
os_fsync(
    FILE *fHandle)
{
    os_result r;

    if (fsync(fileno(fHandle)) == 0) {
        r = os_resultSuccess;
    } else {
        r = os_resultFail;
    }

    return r;
}

_Ret_opt_z_ const char *
os_getTempDir(void)
{
    const char * dir_name = NULL;

    dir_name = os_getenv("OSPL_TEMP");

    /* if OSPL_TEMP is not defined the default is /tmp */
    if (dir_name == NULL || (strcmp (dir_name, "") == 0)) {
       dir_name = "/tmp";
    }

    return dir_name;
}

ssize_t os_write(int fd, const void *buf, size_t count)
{
    return write(fd, buf, count);
}

void os_flockfile(FILE *file)
{
	/* flockfile is not supported on the VxWorks DKM platform.
	 * Therefore, this function block is empty on the VxWorks platform. */
#ifndef _WRS_KERNEL
	flockfile (file);
#endif
}

void os_funlockfile(FILE *file)
{
	/* funlockfile is not supported on the VxWorks DKM platform.
	 * Therefore, this function block is empty on the VxWorks platform. */
#ifndef _WRS_KERNEL
	funlockfile (file);
#endif
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
