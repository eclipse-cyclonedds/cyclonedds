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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <pthread.h>
#endif /* __APPLE__ */

#include "CUnit/Test.h"
#include "os/os.h"

static FILE *fh = NULL;

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>

/* Windows does not support opening a stream to a buffer like fmemopen on
 * Linux does. A temporary file that will never be flushed to disk is created
 * instead. See the link below for more detail.
 *
 * https://blogs.msdn.microsoft.com/larryosterman/2004/04/19/its-only-temporary/
 */

FILE *fmemopen(void *buf, size_t size, const char *mode)
{
    int err = 0;
    int fd = -1;
    DWORD ret;
    FILE *fh = NULL;
    HANDLE hdl = INVALID_HANDLE_VALUE;
    /* GetTempFileName will fail if the directory is be longer than MAX_PATH-14
       characters */
    char tmpdir[(MAX_PATH + 1) - 14];
    char tmpfile[MAX_PATH + 1];
    static const int max = 1000;
    static const char pfx[] = "cyclone"; /* Up to first three are used. */

    (void)buf;
    (void)size;

    ret = GetTempPath(sizeof(tmpdir), tmpdir);
    if (ret == 0) {
        err = GetLastError();
    } else if (ret > sizeof(tmpdir)) {
        err = ENOMEM;
    }

    if (GetTempFileName(tmpdir, pfx, 0, tmpfile) == 0) {
        err = GetLastError();
        assert(err != ERROR_BUFFER_OVERFLOW);
    } else {
        /* The combination of FILE_ATTRIBUTE_TEMPORARY and
           FILE_FLAG_DELETE_ON_CLOSE hints to the filesystem that the file
           should never be flushed to disk. */
        hdl = CreateFile(
            tmpfile,
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            CREATE_ALWAYS,
            FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY,
            NULL);
        if (hdl == INVALID_HANDLE_VALUE) {
            err = GetLastError();
        }
    }

    if (err) {
        errno = err;
    } else {
        OS_WARNING_MSVC_OFF(4996);
        if ((fd = _open_osfhandle((intptr_t)hdl, _O_APPEND)) == -1) {
            /* errno set by _open_osfhandle. */
            CloseHandle(hdl);
        } else if ((fh = fdopen(fd, mode)) == NULL) {
            /* errno set by fdopen. */
            _close(fd); /* Automatically closes underlying handle. */
        } else {
            return fh;
        }
        OS_WARNING_MSVC_ON(4996);
    }

    return NULL;
}
#endif /* _WIN32 */

static void count(void *ptr, const dds_log_data_t *data)
{
    (void)data;
    *(int *)ptr += 1;
}

static void copy(void *ptr, const dds_log_data_t *data)
{
    *(char **)ptr = os_strdup(data->message);
}

static void setup(void)
{
    fh = fmemopen(NULL, 1024, "wb+");
    CU_ASSERT_PTR_NOT_NULL_FATAL(fh);
}

static void teardown(void)
{
    (void)fclose(fh);
}

/* By default only DDS_LC_FATAL and DDS_LC_ERROR are set. This means setting a
   trace sink should not have any effect, because no trace categories are
   enabled. The message should end up in the log file. */
CU_Test(dds_log, only_log_file, .init=setup, .fini=teardown)
{
    char buf[1024], *ptr;
    int cnt = 0;
    size_t nbytes;

    dds_set_log_file(fh);
    dds_set_trace_sink(&count, &cnt);
    DDS_ERROR("foo%s\n", "bar");
    (void)fseek(fh, 0L, SEEK_SET);
    nbytes = fread(buf, 1, sizeof(buf) - 1, fh);
    /* At least foobar should have been printed to the log file. */
    CU_ASSERT_FATAL(nbytes > 6);
    buf[nbytes] = '\0';
    ptr = strstr(buf, "foobar\n");
    CU_ASSERT_PTR_NOT_NULL(ptr);
    /* No trace categories are enabled by default, verify trace callback was
       not invoked. */
    CU_ASSERT_EQUAL(cnt, 0);
}

/* Messages must be printed to the trace file if at least one trace category
   is enabled. Messages must not be written twice if the trace file is the
   same as the log file. */
CU_Test(dds_log, same_file, .init=setup, .fini=teardown)
{
    char buf[1024], *ptr;
    size_t nbytes;

    dds_set_log_mask(DDS_LC_ALL);
    dds_set_log_file(fh);
    dds_set_trace_file(fh);
    DDS_ERROR("foo%s\n", "bar");
    (void)fseek(fh, 0L, SEEK_SET);
    nbytes = fread(buf, 1, sizeof(buf) - 1, fh);
    /* At least foobar should have been written to the trace file. */
    CU_ASSERT_FATAL(nbytes > 6);
    buf[nbytes] = '\0';
    ptr = strstr(buf, "foobar\n");
    CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
    /* The message should only have been printed once, verify foobar does not
       occur again. */
    ptr = strstr(ptr + 1, "foobar\n");
    CU_ASSERT_PTR_NULL(ptr);
}

/* The sinks are considered to be the same only if the callback and userdata
   both are an exact match. If the userdata is different, the function should
   be called twice for log messages. */
CU_Test(dds_log, same_sink_function)
{
    int log_cnt = 0, trace_cnt = 0;

    dds_set_log_mask(DDS_LC_ALL);
    dds_set_log_sink(&count, &log_cnt);
    dds_set_trace_sink(&count, &trace_cnt);
    DDS_ERROR("foo%s\n", "bar");
    CU_ASSERT_EQUAL(log_cnt, 1);
    CU_ASSERT_EQUAL(trace_cnt, 1);
}

CU_Test(dds_log, exact_same_sink)
{
    int cnt = 0;

    dds_set_log_mask(DDS_LC_ALL);
    dds_set_log_sink(&count, &cnt);
    dds_set_trace_sink(&count, &cnt);
    DDS_ERROR("foo%s\n", "bar");
    CU_ASSERT_EQUAL(cnt, 1);
}

/* The log file must be restored if the sink is unregistered, verify the log
   file is not used while the sink is registered. Verify use of the log file is
   restored again when the sink is unregistered. */
CU_Test(dds_log, no_sink, .init=setup, .fini=teardown)
{
    int ret;
    char buf[1024], *ptr = NULL;
    size_t cnt[2] = {0, 0};

    /* Set the destination log file and verify the message is written. */
    dds_set_log_file(fh);
    DDS_ERROR("foobar\n");
    ret = fseek(fh, 0L, SEEK_SET);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    buf[0] = '\0';
    cnt[0] = fread(buf, 1, sizeof(buf) - 1, fh);
    buf[cnt[0]] = '\0';
    ptr = strstr(buf, "foobar\n");
    CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);

    /* Register a custom sink and verify it receives the message. */
    ptr = NULL;
    dds_set_log_sink(&copy, &ptr);
    DDS_ERROR("foobaz\n");
    CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
    CU_ASSERT(strcmp(ptr, "foobaz\n") == 0);
    os_free(ptr);
    ptr = NULL;
    /* Verify it has not been written to the stream. */
    ret = fseek(fh, 0L, SEEK_SET);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    buf[0] = '\0';
    cnt[1] = fread(buf, 1, sizeof(buf) - 1, fh);
    buf[cnt[1]] = '\0';
    ptr = strstr(buf, "foobaz\n");
    CU_ASSERT_PTR_NULL_FATAL(ptr);

    /* Unregister the custom sink and verify the default is restored. */
    dds_set_log_sink(0, NULL);
    ret = fseek(fh, 0, SEEK_SET);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
    ptr = NULL;
    DDS_ERROR("foobaz\n");
    ret = fseek(fh, 0, SEEK_SET);
    CU_ASSERT_PTR_NULL(ptr);
    if (ptr != NULL) {
        os_free(ptr);
        ptr = NULL;
    }
    buf[0]= '\0';
    cnt[1] = fread(buf, 1, sizeof(buf) - 1, fh);
#ifdef _WIN32
    /* Write on Windows appends. */
    CU_ASSERT_EQUAL(cnt[1], cnt[0] * 2);
#else
    CU_ASSERT_EQUAL(cnt[1], cnt[0]);
#endif
    buf[cnt[1]] = '\0';
    ptr = strstr(buf, "foobaz\n");
    CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
}

/* A newline terminates the message. Until that a newline is encountered, the
   messages must be concatenated in the buffer. The newline is replaced by a
   NULL byte if it is flushed to a sink. */
CU_Test(dds_log, newline_terminates)
{
    char *msg = NULL;

    dds_set_log_sink(&copy, &msg);
    DDS_ERROR("foo");
    CU_ASSERT_PTR_NULL_FATAL(msg);
    DDS_ERROR("bar");
    CU_ASSERT_PTR_NULL_FATAL(msg);
    DDS_ERROR("baz\n");
    CU_ASSERT_PTR_NOT_NULL_FATAL(msg);
    CU_ASSERT(strcmp(msg, "foobarbaz\n") == 0);
    os_free(msg);
}

/* Nothing must be written unless a category is enabled. */
CU_Test(dds_log, disabled_categories_discarded)
{
    char *msg = NULL;

    dds_set_log_sink(&copy, &msg);
    DDS_INFO("foobar\n");
    CU_ASSERT_PTR_NULL_FATAL(msg);
    dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_INFO);
    DDS_INFO("foobar\n");
    CU_ASSERT_PTR_NOT_NULL_FATAL(msg);
    CU_ASSERT(strcmp(msg, "foobar\n") == 0);
    os_free(msg);
}


static os_cond cond;
static os_mutex mutex;

struct arg {
    os_cond *cond;
    os_mutex *mutex;
    os_time stamp;
    os_time pause;
};

static void dummy(void *ptr, const dds_log_data_t *data)
{
    (void)ptr;
    (void)data;
}

static void block(void *ptr, const dds_log_data_t *data)
{
    (void)data;
    struct arg *arg = (struct arg *)ptr;
    os_mutexLock(arg->mutex);
    arg->stamp = os_timeGet();
    os_condBroadcast(arg->cond);
    os_mutexUnlock(arg->mutex);

    os_nanoSleep(arg->pause);
}

static uint32_t run(void *ptr)
{
    (void)ptr;

    DDS_ERROR("foobar\n");

    return 0;
}

/* Log and trace sinks can be changed at runtime. However, the operation must
   be synchronous! Verify the dds_set_log_sink blocks while other threads
   reside in the log or trace sinks. */
CU_Test(dds_log, synchronous_sink_changes)
{
    struct arg arg;
    os_time diff, stamp;
    os_threadId tid;
    os_threadAttr tattr;
    os_result res;

    os_mutexInit(&mutex);
    os_condInit(&cond, &mutex);
    (void)memset(&arg, 0, sizeof(arg));
    arg.mutex = &mutex;
    arg.cond = &cond;
    arg.pause.tv_sec = 0;
    arg.pause.tv_nsec = 1000000;

    os_mutexLock(&mutex);
    dds_set_log_sink(&block, &arg);
    os_threadAttrInit(&tattr);
    res = os_threadCreate(&tid, "foobar", &tattr, &run, &arg);
    CU_ASSERT_EQUAL_FATAL(res, os_resultSuccess);
    os_condWait(&cond, &mutex);
    dds_set_log_sink(dummy, NULL);
    stamp = os_timeGet();

    CU_ASSERT(os_timeCompare(arg.stamp, stamp) == -1);
    diff = os_timeSub(stamp, arg.stamp);
    CU_ASSERT(os_timeCompare(arg.pause, diff) == -1);
}
