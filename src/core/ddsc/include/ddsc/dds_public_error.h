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

/** @file
 *
 * @brief DDS C Error API
 *
 * This header file defines the public API of error values and convenience
 * functions in the CycloneDDS C language binding.
 */
#ifndef DDS_ERROR_H
#define DDS_ERROR_H

#include "os/os_public.h"
#include "ddsc/dds_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* Error masks for returned status values */

#define DDS_ERR_NR_MASK       0x000000ff
#define DDS_ERR_LINE_MASK     0x003fff00
#define DDS_ERR_FILE_ID_MASK  0x7fc00000


/*
  State is unchanged following a function call returning an error
  other than UNSPECIFIED, OUT_OF_RESOURCES and ALREADY_DELETED.

  Error handling functions. Three components to returned int status value.

  1 - The DDS_ERR_xxx error number
  2 - The file identifier
  3 - The line number

  All functions return >= 0 on success, < 0 on error
*/
/** @name Return codes
  @{**/
#define DDS_RETCODE_OK                   0 /**< Success */
#define DDS_RETCODE_ERROR                1 /**< Non specific error */
#define DDS_RETCODE_UNSUPPORTED          2 /**< Feature unsupported */
#define DDS_RETCODE_BAD_PARAMETER        3 /**< Bad parameter value */
#define DDS_RETCODE_PRECONDITION_NOT_MET 4 /**< Precondition for operation not met */
#define DDS_RETCODE_OUT_OF_RESOURCES     5 /**< When an operation fails because of a lack of resources */
#define DDS_RETCODE_NOT_ENABLED          6 /**< When a configurable feature is not enabled */
#define DDS_RETCODE_IMMUTABLE_POLICY     7 /**< When an attempt is made to modify an immutable policy */
#define DDS_RETCODE_INCONSISTENT_POLICY  8 /**< When a policy is used with inconsistent values */
#define DDS_RETCODE_ALREADY_DELETED      9 /**< When an attempt is made to delete something more than once */
#define DDS_RETCODE_TIMEOUT              10 /**< When a timeout has occurred */
#define DDS_RETCODE_NO_DATA              11 /**< When expected data is not provided */
#define DDS_RETCODE_ILLEGAL_OPERATION    12 /**< When a function is called when it should not be */
#define DDS_RETCODE_NOT_ALLOWED_BY_SECURITY 13 /**< When credentials are not enough to use the function */


/** @}*/

/* For backwards compatability */

#define DDS_SUCCESS DDS_RETCODE_OK

/** @name DDS_Error_Type
  @{**/
#define DDS_CHECK_REPORT 0x01
#define DDS_CHECK_FAIL 0x02
#define DDS_CHECK_EXIT 0x04
/** @}*/

/* Error code handling functions */

/** @name Macros for error handling
  @{**/
#define DDS_TO_STRING(n) #n
#define DDS_INT_TO_STRING(n) DDS_TO_STRING(n)
/** @}*/

/** Macro to extract error number */
#define dds_err_nr(e) ((-(e)) & DDS_ERR_NR_MASK)

/** Macro to extract line number */
#define dds_err_line(e) (((-(e)) & DDS_ERR_LINE_MASK) >> 8)

/** Macro to extract file identifier */
#define dds_err_file_id(e) (((-(e)) & DDS_ERR_FILE_ID_MASK) >> 22)

/**
 * @brief Takes the error value and outputs a string corresponding to it.
 *
 * @param[in]  err  Error value to be converted to a string
 * @returns  String corresponding to the error value
 */
DDS_EXPORT const char * dds_err_str (dds_return_t err);

/**
 * @brief Takes the error number, error type and filename and line number and formats it to
 * a string which can be used for debugging.
 *
 * @param[in]  err    Error value
 * @param[in]  flags  Indicates Fail, Exit or Report
 * @param[in]  where  File and line number
 * @returns  true - True
 * @returns  false - False
 */

DDS_EXPORT bool dds_err_check (dds_return_t err, unsigned flags, const char * where);

/** Macro that defines dds_err_check function */
#define DDS_ERR_CHECK(e, f) (dds_err_check ((e), (f), __FILE__ ":" DDS_INT_TO_STRING(__LINE__)))

/* Failure handling */

/** Failure handler */
typedef void (*dds_fail_fn) (const char *, const char *);

/** Macro that defines dds_fail function */
#define DDS_FAIL(m) (dds_fail (m, __FILE__ ":" DDS_INT_TO_STRING (__LINE__)))

/**
 * @brief Set the failure function
 *
 * @param[in]  fn  Function to invoke on failure
 */
DDS_EXPORT void dds_fail_set (dds_fail_fn fn);

/**
 * @brief Get the failure function
 *
 * @returns Failure function
 */
DDS_EXPORT dds_fail_fn dds_fail_get (void);

/**
 * @brief Handles failure through an installed failure handler
 *
 * @params[in]  msg  String containing failure message
 * @params[in]  where  String containing file and location
 */
DDS_EXPORT void dds_fail (const char * msg, const char * where);


#if defined (__cplusplus)
}
#endif
#endif
