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
#ifndef DDSRT_PROCESS_H
#define DDSRT_PROCESS_H

#include "dds/export.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/retcode.h"

#if defined(_WIN32)
typedef DWORD ddsrt_pid_t;
#define PRIdPID "u"
#else /* _WIN32 */
#include <unistd.h>
#if defined(_WRS_KERNEL)
typedef RTP_ID ddsrt_pid_t; /* typedef struct wind_rtp *RTP_ID */
#define PRIdPID PRIuPTR
#else
typedef pid_t ddsrt_pid_t;
#define PRIdPID "d"
#endif
#endif /* _WIN32 */


#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Return process ID (PID) of the calling process.
 *
 * @returns The process ID of the calling process.
 */
DDS_EXPORT ddsrt_pid_t
ddsrt_getpid(void);


/**
 * @brief Create new process.
 *
 * Creates a new process using the provided executable file. It will have
 * default priority and scheduling.
 *
 * Process arguments are represented by argv, which can be null. If argv is
 * not null, then the array must be null terminated. The argv array only has
 * to contain the arguments, the executable filename doesn't have to be in
 * the first element, which is normally the convention.
 *
 * @param[in]   executable     Executable file name.
 * @param[in]   argv           Arguments array.
 * @param[out]  pid            ID of the created process.
 *
 * @returns A dds_retcode_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Process successfully created.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Provided file is not available or not executable.
 * @retval DDS_RETCODE_NOT_ALLOWED
 *             Caller is not permitted to start the process.
 * @retval DDS_RETCODE_OUT_OF_RESOURCES
 *             Not enough resources to start the process.
 * @retval DDS_RETCODE_ERROR
 *             Process could not be created.
 */
DDS_EXPORT dds_retcode_t
ddsrt_proc_create(
  const char *executable,
  char *const argv[],
  ddsrt_pid_t *pid);

/**
 * @brief Get the exit code of a process.
 *
 * This can only be used on child processes on some platforms.
 *
 * @param[in]   pid            Process ID (PID) to get the exit code from.
 * @param[out]  code           The exit code of the process.
 *
 * @returns A dds_retcode_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Process has terminated and its exit code has been captured.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             Process is still alive.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Process unknown.
 * @retval DDS_RETCODE_ERROR
 *             Getting the exit code failed for an unknown reason.
 */
DDS_EXPORT dds_retcode_t
ddsrt_proc_get_exit_code(
  ddsrt_pid_t pid,
  int32_t *code);

/**
 * @brief Checks if a process exists.
 *
 * @param[in]   pid            Process ID (PID) to check if it exists.
 *
 * @returns A dds_retcode_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The process exists.
 * @retval DDS_RETCODE_NOT_FOUND
 *             The process does not exist.
 * @retval DDS_RETCODE_ERROR
 *             Determining if a process exists or not, failed.
 */
DDS_EXPORT dds_retcode_t
ddsrt_proc_exists(
  ddsrt_pid_t pid);

/**
 * @brief Terminate a process.
 *
 * This function will try to gracefully terminate the process (identified
 * by pid).
 *
 * When DDS_RETCODE_OK is returned, it doesn't mean that the process
 * has actually terminated. It only indicates that the process was
 * 'told' to terminate. Call ddsrt_proc_exists() to know
 * for sure if the process has terminated.
 *
 * See also ddsrt_proc_kill();
 *
 * @param[in]   pid       Process ID (PID) of the process to terminate.
 *
 * @returns A dds_retcode_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Process was told to terminate.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Process unknown.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             Caller is not allowed to terminate the process.
 * @retval DDS_RETCODE_ERROR
 *             Termination failed for an unknown reason.
 */
DDS_EXPORT dds_retcode_t
ddsrt_proc_term(
  ddsrt_pid_t pid);

/**
 * @brief Kill a process.
 *
 * This function will try to forcefully kill the process (identified
 * by pid).
 *
 * When DDS_RETCODE_OK is returned, it doesn't mean that the process
 * has actually killed. It only indicates that the process was
 * 'told' to terminate. Call ddsrt_proc_exists() to know
 * for sure if the process was killed.
 *
 * See also ddsrt_proc_term();
 *
 * @param[in]   pid       Process ID (PID) of the process to terminate.
 *
 * @returns A dds_retcode_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Kill attempt has been started.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Process unknown.
 * @retval DDS_RETCODE_ILLEGAL_OPERATION
 *             Caller is not allowed to kill the process.
 * @retval DDS_RETCODE_ERROR
 *             Kill failed for an unknown reason.
 */
DDS_EXPORT dds_retcode_t
ddsrt_proc_kill(
  ddsrt_pid_t pid);


#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_PROCESS_H */
