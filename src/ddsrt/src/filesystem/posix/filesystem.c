// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "dds/ddsrt/filesystem.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"

dds_return_t ddsrt_opendir(const char *name, ddsrt_dir_handle_t *dir)
{
    dds_return_t result = DDS_RETCODE_ERROR;
    DIR *d;
    if (dir) {
        d = opendir(name);
        if (d) {
            *dir = d;
            result = DDS_RETCODE_OK;
        }
    }
    return result;
}

dds_return_t ddsrt_readdir(ddsrt_dir_handle_t d, struct ddsrt_dirent *direntp)
{
    dds_return_t result;
    struct dirent *d_entry;

    result = DDS_RETCODE_ERROR;
    if (d && direntp) {
        d_entry = readdir(d);
        if (d_entry) {
            ddsrt_strlcpy(direntp->d_name, d_entry->d_name, sizeof(direntp->d_name));
            result = DDS_RETCODE_OK;
        }
    }

    return result;
}

dds_return_t ddsrt_closedir(ddsrt_dir_handle_t d)
{
    dds_return_t result;

    result = DDS_RETCODE_ERROR;
    if (d) {
        if (closedir(d) == 0) {
            result = DDS_RETCODE_OK;
        }
    }

    return result;
}

dds_return_t ddsrt_stat(const char *path, struct ddsrt_stat *buf)
{
    dds_return_t result;
    struct stat _buf;
    int r;

    r = stat(path, &_buf);
    if (r == 0) {
        buf->stat_mode = _buf.st_mode;
        buf->stat_size = (size_t) _buf.st_size;
        buf->stat_mtime = DDS_SECS(_buf.st_mtime);
        result = DDS_RETCODE_OK;
    } else {
        result = DDS_RETCODE_ERROR;
    }

    return result;
}

char * ddsrt_file_normalize(const char *filepath)
{
    char *norm;
    const char *fpPtr;
    char *normPtr;

    norm = NULL;
    if (filepath != NULL) {
        norm = ddsrt_malloc (strlen (filepath) + 1);
        /* replace any / or \ by DDSRT_FILESEPCHAR */
        fpPtr = (char *) filepath;
        normPtr = norm;
        while (*fpPtr != '\0') {
            *normPtr = *fpPtr;
            if (*fpPtr == '/' || *fpPtr == '\\') {
                *normPtr = DDSRT_FILESEPCHAR;
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

const char *ddsrt_file_sep(void)
{
    return "/";
}
