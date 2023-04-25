// Copyright(c) 2006 to 2021 ZettaScale Technology and others
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
#include "dds/ddsrt/io.h"

dds_return_t ddsrt_opendir(const char *name, ddsrt_dir_handle_t *dir)
{
    dds_return_t result;

    TCHAR szDir[DDSRT_PATH_MAX + 1];
    WIN32_FIND_DATA FileData;
    HANDLE hList;

    result = DDS_RETCODE_ERROR;
    if (dir) {
        snprintf(szDir, DDSRT_PATH_MAX, "%s\\*", name);
        hList = FindFirstFile(szDir, &FileData);

        if (hList != INVALID_HANDLE_VALUE) {
            *dir = hList;
            result = DDS_RETCODE_OK;
        }
    }

    return result;
}

dds_return_t ddsrt_readdir(ddsrt_dir_handle_t d, struct ddsrt_dirent *direntp)
{
    dds_return_t result;
    WIN32_FIND_DATA FileData;
    BOOL r;

    if (direntp) {
        r = FindNextFile(d, &FileData);
        if (r) {
            ddsrt_strlcpy(direntp->d_name, FileData.cFileName, sizeof(direntp->d_name));
            result = DDS_RETCODE_OK;
        } else {
            result = DDS_RETCODE_ERROR;
        }
    } else {
        result = DDS_RETCODE_ERROR;
    }

    return result;
}

dds_return_t ddsrt_closedir(ddsrt_dir_handle_t d)
{
    FindClose(d);

    return DDS_RETCODE_OK;
}

dds_return_t ddsrt_stat(const char *path, struct ddsrt_stat *buf)
{
    dds_return_t result;
    struct _stat _buf;
    int r;

    r = _stat(path, &_buf);
    if (r == 0) {
        buf->stat_mode = _buf.st_mode;
        buf->stat_size = (size_t)_buf.st_size;
        buf->stat_mtime = DDS_SECS(_buf.st_mtime);;
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
    if ((filepath != NULL) && (*filepath != '\0')) {
        norm = ddsrt_malloc(strlen(filepath) + 1);
        /* replace any / or \ by DDSRT_FILESEPCHAR */
        fpPtr = (char *) filepath;
        normPtr = norm;
        while (*fpPtr != '\0') {
            *normPtr = *fpPtr;
            if ((*fpPtr == '/') || (*fpPtr == '\\')) {
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
    return "\\";
}
