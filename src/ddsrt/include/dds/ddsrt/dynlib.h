
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
#ifndef DDSRT_LIBRARY_H
#define DDSRT_LIBRARY_H

#include "dds/export.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/attributes.h"

#if !DDSRT_WITH_FREERTOS
#define DDSRT_HAVE_DYNLIB (1)
#else
#define DDSRT_HAVE_DYNLIB (0)
#endif

#if DDSRT_HAVE_DYNLIB

#if defined (__cplusplus)
extern "C" {
#endif


//typedef void *ddsrt_dynlib_t;
typedef struct ddsrt_dynlib *ddsrt_dynlib_t;


/**
 * @brief Load a dynamic shared library.
 *
 * The function ddsrt_dlopen() loads the dynamic shared object (shared library)
 * file, identified by 'name', sets the handle parameter for the loaded library and
 * returns the result with dds return code.
 *
 * If the 'translate' boolean is true, this function will first try to open the
 * library with a translated 'name'. Translated in this context means that if
 * "mylibrary" is provided, it will be translated into libmylibrary.so,
 * libmylibrary.dylib or mylibrary.dll depending on the platform.
 * This translation only happens when the given name does not contain
 * a directory.
 * If the function isn't able to load the library with the translated name, it
 * will still try the given name.
 *
 * @param[in]   name        Library file name.
 * @param[in]   translate   Automatic name translation on/off.
 * @param[out]  handle      Library handle that will be assigned after successfull operation. It is assigned to NULL if loading fails.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Library handle was successfully loaded.
 * @retval DDS_RETCODE_BAD_PARAM
 *             There is an invalid input in the parameter list
 * @retval DDS_RETCODE_ERROR
 *             Loading failed.
 *             Use ddsrt_dlerror() to diagnose the failure.
 */
DDS_EXPORT dds_return_t
ddsrt_dlopen(
    const char *name,
    bool translate,
    ddsrt_dynlib_t *handle) ddsrt_nonnull_all;

/**
 * @brief Close the library.
 *
 * The function ddsrt_dlclose() informs the system that the
 * library, identified by 'handle', is no longer needed.
 * will get the memory address of a symbol,
 * identified by 'symbol', from a loaded library 'handle'.
 *
 * @param[in]   handle      Library handle.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Library handle was successfully closed.
 * @retval DDS_RETCODE_ERROR
 *             Library closing failed.
 *             Use ddsrt_dlerror() to diagnose the failure.
 */
DDS_EXPORT dds_return_t
ddsrt_dlclose(
    ddsrt_dynlib_t handle);

/**
 * @brief Get the memory address of a symbol.
 *
 * The function ddsrt_dlsym() will get the memory address of a symbol,
 * identified by 'symbol', from a loaded library 'handle'.
 *
 * @param[in]   handle      Library handle.
 * @param[in]   symbol      Symbol name.
 * @param[out]  address     The memory address of the loaded symbol (void*).
 *
 * @returns  A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Symbol was found in the loaded library.
 *             Address parameter is ready to use.
 * @retval DDS_RETCODE_ERROR
 *             Symbol was not found.
 *             Use ddsrt_dlerror() to diagnose the failure.
 */
DDS_EXPORT dds_return_t
ddsrt_dlsym(
    ddsrt_dynlib_t handle,
    const char *symbol,
    void **address);

/**
 * @brief Get the most recent library related error.
 *
 * The function ddsrt_dlerror() will return the most recent error of the operating system
 * in human readable form.
 *
 * If no error was found, it's either due to the fact that there
 * actually was no error since init or last ddsrt_dlerror() call,
 * or due to an unknown unrelated error.
 *
 * As error reporting function can be used for different purposes, dssrt_dlerror
 * function should be called immediately after calling ddsrt_dlopen or ddsrt_dlsym
 * function.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Most recent library related error returned.
 * @retval DDS_RETCODE_NOT_FOUND
 *             No library related error found.
 */
DDS_EXPORT dds_return_t
ddsrt_dlerror(
    char *buf,
    size_t buflen);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_HAVE_DYNLIB */

#endif /* DDSRT_LIBRARY_H */
