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

#if DDSRT_WITH_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
typedef TaskHandle_t ddsrt_pid_t; /* typedef void *TaskHandle_t */
#define PRIdPID "p"
#define DDSRT_HAVE_MULTI_PROCESS 0
/* DDSRT_WITH_FREERTOS */
#elif defined(_WIN32)
typedef DWORD ddsrt_pid_t;
#define PRIdPID "u"
#define DDSRT_HAVE_MULTI_PROCESS 1
/* _WIN32 */
#else
#include <unistd.h>
#if defined(_WRS_KERNEL)
typedef RTP_ID ddsrt_pid_t; /* typedef struct wind_rtp *RTP_ID */
#define PRIdPID PRIuPTR
#define DDSRT_HAVE_MULTI_PROCESS 0
#else
typedef pid_t ddsrt_pid_t;
#define PRIdPID "d"
#define DDSRT_HAVE_MULTI_PROCESS 1
#endif
#endif


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

#if DDSRT_HAVE_MULTI_PROCESS

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
 * @returns A dds_return_t indicating success or failure.
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
DDS_EXPORT dds_return_t
ddsrt_proc_create(
  const char *executable,
  char *const argv[],
  ddsrt_pid_t *pid);

/**
 * @brief Wait for a specific child process to have finished.
 *
 * This function takes a process id and will wait until the related process
 * has finished or the timeout is reached.
 *
 * If the process finished, then the exit code of that process will be copied
 * into the given 'code' argument.
 *
 * Internally, the timeout can be round-up to the nearest milliseconds or
 * seconds, depending on the platform.
 *
 * See ddsrt_proc_waitpids() for waiting on all child processes.
 *
 * @param[in]   pid            Process ID (PID) to get the exit code from.
 * @param[in]   timeout        Time within the process is expected to finish.
 * @param[out]  code           The exit code of the process.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Process has terminated and its exit code has been captured.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             Process is still alive (only when timeout == 0).
 * @retval DDS_RETCODE_TIMEOUT
 *             Process is still alive (even after the timeout).
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Negative timeout.
 * @retval DDS_RETCODE_NOT_FOUND
 *             Process unknown.
 * @retval DDS_RETCODE_ERROR
 *             Getting the exit code failed for an unknown reason.
 */
DDS_EXPORT dds_return_t
ddsrt_proc_waitpid(
  ddsrt_pid_t pid,
  dds_duration_t timeout,
  int32_t *code);

/**
 * @brief Wait for a random child process to have finished.
 *
 * This function will wait until anyone of the child processes has
 * finished or the timeout is reached.
 *
 * If a process finished, then the exit code of that process will be
 * copied into the given 'code' argument. The pid of the process will
 * be put in the 'pid' argument.
 *
 * Internally, the timeout can be round-up to the nearest milliseconds or
 * seconds, depending on the platform.
 *
 * See ddsrt_proc_waitpid() for waiting on a specific child process.
 *
 * @param[in]   timeout        Time within a process is expected to finish.
 * @param[out]  pid            Process ID (PID) of the finished process.
 * @param[out]  code           The exit code of the process.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             A process has terminated.
 *             Its exit code and pid have been captured.
 * @retval DDS_RETCODE_PRECONDITION_NOT_MET
 *             All child processes are still alive (only when timeout == 0).
 * @retval DDS_RETCODE_TIMEOUT
 *             All child processes are still alive (even after the timeout).
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Negative timeout.
 * @retval DDS_RETCODE_NOT_FOUND
 *             There are no processes to wait for.
 * @retval DDS_RETCODE_ERROR
 *             Getting the exit code failed for an unknown reason.
 */
DDS_EXPORT dds_return_t
ddsrt_proc_waitpids(
  dds_duration_t timeout,
  ddsrt_pid_t *pid,
  int32_t *code);

/**
 * @brief Checks if a process exists.
 *
 * @param[in]   pid            Process ID (PID) to check if it exists.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             The process exists.
 * @retval DDS_RETCODE_NOT_FOUND
 *             The process does not exist.
 * @retval DDS_RETCODE_ERROR
 *             Determining if a process exists or not, failed.
 */
DDS_EXPORT dds_return_t
ddsrt_proc_exists(
  ddsrt_pid_t pid);

/**
 * @brief Kill a process.
 *
 * This function will try to forcefully kill the process (identified
 * by pid).
 *
 * When DDS_RETCODE_OK is returned, it doesn't mean that the process
 * was actually killed. It only indicates that the process was
 * 'told' to terminate. Call ddsrt_proc_exists() to know
 * for sure if the process was killed.
 *
 * @param[in]   pid       Process ID (PID) of the process to terminate.
 *
 * @returns A dds_return_t indicating success or failure.
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
DDS_EXPORT dds_return_t
ddsrt_proc_kill(
  ddsrt_pid_t pid);

#endif /* DDSRT_HAVE_MULTI_PROCESS */

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_PROCESS_H */
