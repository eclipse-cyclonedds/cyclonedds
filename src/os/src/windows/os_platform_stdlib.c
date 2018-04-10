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


static int32_t
os__ensurePathExists(
        _In_z_ const char* dir_name);

/**
*  \brief create a directory with default
*  security descriptor.  The mode parameter
*  is ignored for this Operating System.
*
*/
int32_t
os_mkdir(
        const char *path,
        os_mode_t mode)
{
        int32_t result = 0;

        if (CreateDirectory(path, NULL)) {
                result = 0;
        }
        else {
                result = -1;
        }
        return result;
}

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
                OS_FATAL("os_gethostname", 0, "WSAStartup failed, no compatible socket implementation available");
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

const char *
os_fileSep(void)
{
        return "\\";
}

const char *
os_pathSep(void)
{
        return ";";
}

os_result
os_access(
        const char *file_path,
        int32_t permission)
{
        struct _stat statbuf;
        os_result result;

        result = os_resultFail;
        if (file_path) {
                if (_stat(file_path, &statbuf) == 0) {
                        if ((statbuf.st_mode & permission) == permission) {
                                result = os_resultSuccess;
                        }
                }
        }

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

char *
os_strsep(char **str, const char *sep)
{
        char *ret;
        if (**str == '\0')
                return 0;
        ret = *str;
        while (**str && strchr(sep, **str) == 0)
                (*str)++;
        if (**str != '\0')
        {
                **str = '\0';
                (*str)++;
        }
        return ret;
}

#pragma warning( disable : 4996 )
int
os_vfprintfnosigpipe(
        FILE *file,
        const char *format,
        va_list args)
{
        return vfprintf(file, format, args);
}

#pragma warning( default : 4996 )
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
        struct _stat32 _buf;
        int r;

        r = _stat32(path, &_buf);
        if (r == 0) {
                buf->stat_mode = _buf.st_mode;
                buf->stat_size = _buf.st_size;
                buf->stat_mtime.tv_sec = _buf.st_mtime;
                buf->stat_mtime.tv_nsec = 0;
                result = os_resultSuccess;
        }
        else {
                result = os_resultFail;
        }

        return result;
}

os_result os_remove(const char *pathname)
{
        return (remove(pathname) == 0) ? os_resultSuccess : os_resultFail;
}

os_result os_rename(const char *oldpath, const char *newpath)
{
        (void)os_remove(newpath);
        return (rename(oldpath, newpath) == 0) ? os_resultSuccess : os_resultFail;
}

/* The result of os_fileNormalize should be freed with os_free */
_Ret_z_
_Must_inspect_result_
char *
os_fileNormalize(
        _In_z_ const char *filepath)
{
    char *norm;
    const char *fpPtr;
    char *normPtr;

    norm = os_malloc(strlen(filepath) + 1);
    fpPtr = filepath;
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

    return norm;
}

os_result
os_fsync(
        FILE *fHandle)
{
        os_result r;

        if (FlushFileBuffers((HANDLE)fHandle)) {
                r = os_resultSuccess;
        }
        else {
                r = os_resultFail;
        }
        return r;
}

_Ret_opt_z_ const char *
os_getTempDir(void)
{
        const char * dir_name = NULL;

        dir_name = os_getenv("OSPL_TEMP");

        /* if OSPL_TEMP is not defined use the TEMP variable */
        if (dir_name == NULL || (strcmp(dir_name, "") == 0)) {
                dir_name = os_getenv("TEMP");
        }

        /* if TEMP is not defined use the TMP variable */
        if (dir_name == NULL || (strcmp(dir_name, "") == 0)) {
                dir_name = os_getenv("TMP");
        }

        /* Now we need to verify if we found a temp directory path, and if we did
        * we have to make sure all the (sub)directories in the path exist.
        * This is needed  to prevent any other operations from using the directory
        * path while it doesn't exist and therefore running into errors.
        */
        if (dir_name == NULL || (strcmp(dir_name, "") == 0)) {
                OS_ERROR("os_getTempDir", 0,
                        "Could not retrieve temporary directory path - "
                        "neither of environment variables TEMP, TMP, OSPL_TEMP were set");
        }
        else if (os__ensurePathExists(dir_name) != 0)
        {
                OS_ERROR("os_getTempDir", 0,
                        "Could not ensure all (sub)directories of the temporary directory\n"
                        "path '%s' exist.\n"
                        "This has consequences for the ability of OpenSpliceDDS to run\n"
                        "properly, as the directory path must be accessible to create\n"
                        "database and key files in. Without this ability OpenSpliceDDS can\n"
                        "not start.\n",
                        dir_name);
        }

        return dir_name;
}

int32_t
os__ensurePathExists(
    _In_z_ const char* dir_name)
{
    char* tmp;
    char* ptr;
    char ptrTmp;
    struct os_stat statBuf;
    os_result status;
    int32_t result = 0;
    int32_t cont = 1;

    tmp = os_strdup(dir_name);

    for (ptr = tmp; cont; ptr++)
    {
        if (*ptr == '\\' || *ptr == '/' || *ptr == '\0')
        {
            ptrTmp = ptr[0];
            ptr[0] = '\0';
            status = os_stat(tmp, &statBuf);

            if (status != os_resultSuccess)
            {
                os_mkdir(tmp, 0);
                status = os_stat(tmp, &statBuf);
            }

            if (!OS_ISDIR(statBuf.stat_mode))
            {
                if ((strlen(tmp) == 2) && (tmp[1] == ':')) {
                    /*This is a device like for instance: 'C:'*/
                }
                else
                {
                    OS_ERROR("os_ensurePathExists", 0,
                        "Unable to create directory '%s' within path '%s'. Errorcode: %d",
                        tmp,
                        dir_name,
                        os_getErrno());
                    result = -1;
                }
            }
            ptr[0] = ptrTmp;
        }
        if (*ptr == '\0' || result == -1)
        {
            cont = 0;
        }
    }
    os_free(tmp);

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

void os_flockfile(FILE *file)
{
	_lock_file (file);
}

void os_funlockfile(FILE *file)
{
	_unlock_file (file);
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
