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
#include "CUnit/Runner.h"
#include "os/os.h"

#ifndef WINCE
#include <fcntl.h>
#endif

#if (defined WIN32 || defined WIN64)
#include <direct.h>
#include <Windows.h>
#endif

static const os_time wait_time_out = { 1, 0 };
static FILE *file;

#define ENABLE_TRACING 0

#define FLOCKFILE_THREAD1_INPUT1 "thread1_flockfile_proc: *** input 1 ***"
#define FLOCKFILE_THREAD1_INPUT3 "thread1_flockfile_proc: *** input 3 ***"
#define FLOCKFILE_THREAD2_INPUT2 "thread2_flockfile_proc: *** input 2 ***"

#define defSignal(signal) \
	static os_cond signal;\
	static bool signal##_set = false;

#define initSignal(signal, mutex) \
		os_condInit(&signal, &mutex);\
		signal##_set = false;

#define sendSignal(signal, mutex) \
		os_mutexLock(&mutex);\
		/* signaling */ \
		signal##_set = true; \
		os_condSignal(&signal);\
		os_mutexUnlock(&mutex);

#define waitForSignal(signal, mutex) \
		os_mutexLock(&mutex);\
		while(!signal##_set) { \
			/* waiting for signal */ \
			os_condWait(&signal, &mutex);\
			/* received */ \
		} /* else already signal received */ \
		os_mutexUnlock(&mutex);

#define timedWaitSignal(signal, mutex, time) \
		{ \
			os_time duration = time; \
			os_time startTime, currentTime; \
			os_result rc; \
			os_mutexLock(&mutex); \
			startTime = os_timeGetElapsed(); \
			while(!signal##_set) { \
				/* waiting for signal */ \
				rc = os_condTimedWait(&signal, &mutex, &duration); \
				/* signal received or timeout */ \
				if(rc == os_resultTimeout) { \
					break; \
				} else { \
					currentTime = os_timeGetElapsed(); \
					if(os_timeCompare(os_timeSub(currentTime, startTime), wait_time_out) >= 0) { \
						break; \
					} \
					duration = os_timeSub(wait_time_out, os_timeSub(currentTime, startTime)); \
				} \
			} /* else already signal received */ \
			os_mutexUnlock(&mutex);\
		}

static os_mutex mutex;
static bool do_locking;

/* signals set by threads */
defSignal(thread1_started);
defSignal(thread2_started);
defSignal(action1_done);
defSignal(action2_done);

/* signals set by the test orchestrator (doFlockfileTest) */
defSignal(do_action1);
defSignal(do_action2);
defSignal(do_action3);

static uint32_t thread1_flockfile_proc(void* args) {
	int result = 0;

	/* thread1: start */
	sendSignal(thread1_started, mutex);

	waitForSignal(do_action1, mutex);
	if(do_locking) os_flockfile(file);
	/* Thread1: writing input 1 to the file */
	result = fputs(FLOCKFILE_THREAD1_INPUT1, file);
	CU_ASSERT(result >= 0);

	sendSignal(action1_done, mutex);

	waitForSignal(do_action3, mutex);
	/* Thread1: writing input 3 to the file */
	result = fputs(FLOCKFILE_THREAD1_INPUT3, file);
	CU_ASSERT(result >= 0);
	if(do_locking) os_funlockfile(file);
	/* thread1: end */

	return 0;
}

static uint32_t thread2_flockfile_proc(void* args) {
	int result = 0;

	/* thread2: start */
	sendSignal(thread2_started, mutex);

	waitForSignal(do_action2, mutex);
	/* Thread2: writing input 2 to the file */
	result = fputs(FLOCKFILE_THREAD2_INPUT2, file);
	CU_ASSERT(result >= 0);

	sendSignal(action2_done, mutex);
	/* thread2: end */

	return 0;
}

bool doFlockfileTest(bool lock) {
	bool testPassed = true;
	bool strcmpResult = true;
	os_result result;
	os_threadAttr threadAttr;
	os_threadId thread1;
	os_threadId thread2;
	int FLOCKFILE_INPUT_MAX = sizeof(FLOCKFILE_THREAD1_INPUT1);

	do_locking = lock;

	char *buffer = os_malloc(sizeof(char) * FLOCKFILE_INPUT_MAX);

	file = tmpfile();

	os_mutexInit(&mutex);

	/* initialize all signal conditions */
	os_mutexLock(&mutex);
	initSignal(thread1_started, mutex);
	initSignal(thread2_started, mutex);
	initSignal(action1_done, mutex);
	initSignal(action2_done, mutex);

	initSignal(do_action1, mutex);
	initSignal(do_action2, mutex);
	initSignal(do_action3, mutex);
	os_mutexUnlock(&mutex);

	/* create threads... */
	os_threadAttrInit(&threadAttr);

	result = os_threadCreate(
	            &thread1,
	            "thread 1",
				&threadAttr,
				thread1_flockfile_proc,
	            NULL);
	CU_ASSERT(result == os_resultSuccess);

	result = os_threadCreate(
	            &thread2,
	            "thread 2",
				&threadAttr,
				thread2_flockfile_proc,
	            NULL);
	CU_ASSERT(result == os_resultSuccess);

	/* wait for threads to start */
	waitForSignal(thread1_started, mutex);
	waitForSignal(thread2_started, mutex);

	/* get thread one to do its first thing */
	sendSignal(do_action1, mutex);

	/* wait for thread 1 to acknowledge */
	timedWaitSignal(action1_done, mutex, wait_time_out);

	/* kick thead 2 */
	sendSignal(do_action2, mutex);

	/* wait for thread 2 to acknowledge */
	timedWaitSignal(action2_done, mutex, wait_time_out);

	/* kick thread 1, again */
	sendSignal(do_action3, mutex);

	/* wait for threads to shutdown */
	result = os_threadWaitExit(thread1,NULL);
	CU_ASSERT(result == os_resultSuccess);

	result = os_threadWaitExit(thread2,NULL);
	CU_ASSERT(result == os_resultSuccess);

	/* if lock then Expected action order: 1 3 2
	* else Expected action order: 1 2 3  */

	rewind(file);

	if(lock) {
		if(fgets(buffer, FLOCKFILE_INPUT_MAX, file) > 0) {
			strcmpResult = (strcmp(buffer, FLOCKFILE_THREAD1_INPUT1) == 0);
			CU_ASSERT(strcmpResult);
			testPassed = testPassed && strcmpResult; /* update flag indicating overall test state */
		}
		if(fgets(buffer, FLOCKFILE_INPUT_MAX, file) > 0) {
			strcmpResult = (strcmp(buffer, FLOCKFILE_THREAD1_INPUT3) == 0);
			CU_ASSERT(strcmpResult);
			testPassed = testPassed && strcmpResult; /* update flag indicating overall test state */
		}
		if(fgets(buffer, FLOCKFILE_INPUT_MAX, file) > 0) {
			strcmpResult = (strcmp(buffer, FLOCKFILE_THREAD2_INPUT2) == 0);
			CU_ASSERT(strcmpResult);
			testPassed = testPassed && strcmpResult; /* update flag indicating overall test state */
		}
	} else {
		if(fgets(buffer, FLOCKFILE_INPUT_MAX, file) > 0) {
			strcmpResult = (strcmp(buffer, FLOCKFILE_THREAD1_INPUT1) == 0);
			CU_ASSERT(strcmpResult);
			testPassed = testPassed && strcmpResult; /* update flag indicating overall test state */
		}
		if(fgets(buffer, FLOCKFILE_INPUT_MAX, file) > 0) {
			strcmpResult = (strcmp(buffer, FLOCKFILE_THREAD2_INPUT2) == 0);
			CU_ASSERT(strcmpResult);
			testPassed = testPassed && strcmpResult; /* update flag indicating overall test state */
		}
		if(fgets(buffer, FLOCKFILE_INPUT_MAX, file) > 0) {
			strcmpResult = (strcmp(buffer, FLOCKFILE_THREAD1_INPUT3) == 0);
			CU_ASSERT(strcmpResult);
			testPassed = testPassed && strcmpResult; /* update flag indicating overall test state */
		}
	}

	/* cleanup */
	os_free(buffer);
	fclose(file);

	os_mutexLock(&mutex);
	os_condDestroy(&do_action1);
	os_condDestroy(&do_action2);
	os_condDestroy(&do_action3);

	os_condDestroy(&thread1_started);
	os_condDestroy(&thread2_started);
	os_condDestroy(&action1_done);
	os_condDestroy(&action2_done);
	os_mutexUnlock(&mutex);
	os_mutexDestroy(&mutex);
	/* doFlockfileTest */
	return testPassed;
}

static int
vsnprintfTest(
              const char *format,
              ...)
{
    va_list varargs;
    int result = 0;
    char description[10];
    va_start(varargs, format);
    memset(description, 0, sizeof(description));

    result = os_vsnprintf(description, sizeof(description)-1, format, varargs);
    va_end(varargs);
    return result;
}

CUnit_Suite_Initialize(os_stdlib)
{
    int result = 0;
    os_osInit();
    return result;
}

CUnit_Suite_Cleanup(os_stdlib)
{
  /* Remove files used to test permissions */
  remove ("exec_only");
  remove ("read_exec");
  remove ("read_only");
  remove ("read_write_exec");
  remove ("write_only");
  remove ("existing_file");
  os_osExit();
  return 0;
}

CUnit_Test(os_stdlib, strcasecmp)
{
    int res;
    char *s1, *s2;

    s1 = "a";
    s2 = "a";
    printf ("Starting os_stdlib_strcasecmp_001\n");
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res == 0);

    printf ("Starting os_stdlib_strcasecmp_002\n");
    s1 = "aa";
    s2 = "a";
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res > 0);

    printf ("Starting os_stdlib_strcasecmp_003\n");
    s1 = "a";
    s2 = "aa";
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res < 0);

    printf ("Starting os_stdlib_strcasecmp_004\n");
    s1 = "a";
    s2 = "A";
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res == 0);

    printf ("Starting os_stdlib_strcasecmp_005\n");
    s1 = "A";
    s2 = "a";
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res == 0);

    printf ("Starting os_stdlib_strcasecmp_006\n");
    s1 = "a";
    s2 = "b";
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res < 0);

    printf ("Starting os_stdlib_strcasecmp_007\n");
    s1 = "b";
    s2 = "a";
    res = os_strcasecmp (s1,s2);
    CU_ASSERT (res > 0);

    printf ("Ending os_stdlib_strcasecmp\n");
}

CUnit_Test(os_stdlib, strncasecmp)
{
    int res;
    char *s1, *s2;

    s1 = "a";
    s2 = "a";
    printf ("Starting os_stdlib_strncasecmp_001\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res == 0);

    s1 = "aa";
    s2 = "a";
    printf ("Starting os_stdlib_strncasecmp_002\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res > 0);

    s1 = "a";
    s2 = "aa";
    printf ("Starting os_stdlib_strncasecmp_003\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res < 0);

    s1 = "a";
    s2 = "A";
    printf ("Starting os_stdlib_strncasecmp_004\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res == 0);

    s1 = "A";
    s2 = "a";
    printf ("Starting os_stdlib_strncasecmp_005\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res == 0);

    s1 = "a";
    s2 = "b";
    printf ("Starting os_stdlib_strncasecmp_006\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res < 0);

    s1 = "b";
    s2 = "a";
    printf ("Starting os_stdlib_strncasecmp_007\n");
    res = os_strncasecmp (s1,s2,2);
    CU_ASSERT (res > 0);

    s1 = "abcdefghijkl";
    s2 = "AbCdEaGhIjKl";
    printf ("Starting os_stdlib_strncasecmp_008\n");
    res = os_strncasecmp (s1,s2,5);
    CU_ASSERT (res == 0);

    s1 = "abcdefghijkl";
    s2 = "AbCdEaGhIjKl";
    printf ("Starting os_stdlib_strncasecmp_009\n");
    res = os_strncasecmp (s1,s2,6);
    CU_ASSERT (res > 0);

    printf ("Ending os_stdlib_strncasecmp\n");
}

CUnit_Test(os_stdlib, gethostname)
{
    int res;
    os_result os_res;
    char os_cpu[200];
    char cpu[200];

    printf ("Starting os_stdlib_gethostname_001\n");
    os_cpu[0] = '\0';
    os_res = os_gethostname (os_cpu, sizeof(os_cpu));
    CU_ASSERT (os_res == os_resultSuccess);

    cpu[0] = '\0';
    res = gethostname (cpu, sizeof(cpu));
    CU_ASSERT (res == 0);

    printf ("Starting os_stdlib_gethostname_002\n");
    os_res = os_gethostname (os_cpu, strlen(os_cpu)-1);
    CU_ASSERT (os_res == os_resultFail);
    printf ("Ending os_stdlib_gethostname\n");
}

CUnit_Test(os_stdlib, putenv)
{
    os_result os_res;

    printf ("Starting os_stdlib_putenv_001\n");
    os_res = os_putenv ("ABCDE=FGHIJ");
    CU_ASSERT (os_res == os_resultSuccess);
    CU_ASSERT (strcmp (os_getenv("ABCDE"), "FGHIJ") == 0);
    printf ("Ending os_stdlib_putenv\n");
}

CUnit_Test(os_stdlib, getenv)
{
    const char *env;
    os_result res;

    printf ("Starting os_stdlib_getenv_001\n");

    res = os_putenv("ABCDE=FGHIJ");
    CU_ASSERT(res == os_resultSuccess);

    env = os_getenv("ABCDE");
    CU_ASSERT(env != NULL);
    if (env != NULL) {
        CU_ASSERT(strcmp(env, "FGHIJ") == 0);
    }
    printf ("Starting os_stdlib_getenv_002\n");
    CU_ASSERT (os_getenv("XXABCDEXX") == NULL );
    printf ("Ending os_stdlib_getenv\n");
}

CUnit_Test(os_stdlib, fileSep)
{
  #if defined WIN32
    const char *wanted= "\\";
  #else
    const char *wanted= "/";
  #endif
    printf ("Starting os_stdlib_fileSep_001\n");
    CU_ASSERT (strcmp(os_fileSep(), wanted) == 0);
    printf ("Ending os_stdlib_fileSep\n");
}

CUnit_Test(os_stdlib, access)
{
    os_result os_res;
    os_result wanted;
    int fh;

    /* Check correct functioning of os_access, non existing file read access */
    printf ("Starting os_stdlib_access_001\n");
#if defined VXWORKS_RTP || defined _WRS_KERNEL
    printf ("N.A - Not tested for vxworks.\n");
#else
    os_res = os_access("non_existing_file", OS_ROK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access, non existing file write access */
    printf ("Starting os_stdlib_access_002\n");
    os_res = os_access("non_existing_file", OS_WOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access, non existing file execute access */
    printf ("Starting os_stdlib_access_003\n");
    os_res = os_access("non_existing_file", OS_XOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access, non existing file existence */
    printf ("Starting os_stdlib_access_004\n");
    os_res = os_access("non_existing_file", OS_FOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access, existing file with no
       permissions read access */
    printf ("Starting os_stdlib_access_005\n");
  #ifdef WIN32
    fh= _creat("existing_file", 0000); /* Note always has read & execute */
    if (fh != -1)
        _close(fh);
    wanted = os_resultSuccess;
  #else
    fh= creat("existing_file", 0000);
    if (fh != -1)
        close(fh);
     wanted = os_resultFail;
  #endif /* WIN32 */
    os_res = os_access("existing_file", OS_ROK);
    CU_ASSERT (os_res == wanted);

    /* Check correct functioning of os_access, existing file with no
       permissions write access */
    printf ("Starting os_stdlib_access_006\n");
    os_res = os_access("existing_file", OS_WOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access, existing file with no
       permissions execute access */
    printf ("Starting os_stdlib_access_007\n");
    os_res = os_access("existing_file", OS_XOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access, existing file with no permissions existence */
    printf ("Starting os_stdlib_access_008\n");
    os_res = os_access("existing_file", OS_FOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read permissions read access */
    printf ("Starting os_stdlib_access_009\n");
  #ifdef WIN32
    fh= _creat("read_only", _S_IREAD); /* Note always has read & execute */
    if (fh != -1)
        _close(fh);
  #else
    fh= creat("read_only", S_IRUSR);
    if (fh != -1)
        close(fh);
  #endif /* WIN32 */
    os_res = os_access("read_only", OS_ROK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read permissions write access */
    printf ("Starting os_stdlib_access_010\n");
    os_res = os_access("read_only", OS_WOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access,
       existing file with read permissions execute access */
    printf ("Starting os_stdlib_access_011\n");
    os_res = os_access("read_only", OS_XOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access,
       existing file with read permissions existence */
    printf ("Starting os_stdlib_access_012\n");
    os_res = os_access("read_only", OS_FOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with write permissions read access */
    printf ("Starting os_stdlib_access_013\n");
  #ifdef WIN32
    fh= _creat("write_only", _S_IWRITE); /* Note windows automatically has read access can't have write only */
    if (fh != -1)
        _close(fh);
    wanted = os_resultSuccess;
  #else
    fh= creat("write_only", S_IWUSR);
    if (fh != -1)
        close(fh);
    wanted = os_resultFail;
  #endif /* WIN32 */
    os_res = os_access("write_only", OS_ROK);
    CU_ASSERT (os_res == wanted);

    /* Check correct functioning of os_access,
       existing file with write permissions write access */
    printf ("Starting os_stdlib_access_014\n");
    os_res = os_access("write_only", OS_WOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with write permissions execute access */
    printf ("Starting os_stdlib_access_015\n");
    os_res = os_access("write_only", OS_XOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access,
       existing file with write permissions existence */
    printf ("Starting os_stdlib_access_016\n");
    os_res = os_access("write_only", OS_FOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with exec permissions read access */
    printf ("Starting os_stdlib_access_017\n");
  #ifdef WIN32
    fh= _creat("exec_only" OS_OS_EXESUFFIX, _S_IREAD); /* Windows always has read and can't do execute (that's based upon filename ext only) */
    if (fh != -1)
        _close(fh);
    wanted = os_resultSuccess;
  #else
    fh= creat("exec_only" OS_OS_EXESUFFIX, S_IXUSR);
    if (fh != -1)
        close(fh);
    wanted = os_resultFail;
  #endif /* WIN32 */
    os_res = os_access("exec_only" OS_OS_EXESUFFIX, OS_ROK);
    CU_ASSERT (os_res == wanted);

    /* Check correct functioning of os_access,
       existing file with exec permissions write access */
    printf ("Starting os_stdlib_access_018\n");
    os_res = os_access("exec_only" OS_OS_EXESUFFIX, OS_WOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access,
       existing file with exec permissions execute access */
    printf ("Starting os_stdlib_access_019\n");
    os_res = os_access("exec_only" OS_OS_EXESUFFIX, OS_XOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with exec permissions existence */
    printf ("Starting os_stdlib_access_020\n");
    os_res = os_access("exec_only" OS_OS_EXESUFFIX, OS_FOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read/write/exec permissions read access */
    printf ("Starting os_stdlib_access_021\n");
  #ifdef WIN32
    fh= _creat("read_write_exec" OS_OS_EXESUFFIX, _S_IREAD | _S_IWRITE); /* Windows always has read and can't do execute (that's based upon filename ext only) */
    if (fh != -1)
        _close(fh);
  #else
    fh= creat("read_write_exec" OS_OS_EXESUFFIX, S_IRUSR | S_IWUSR | S_IXUSR);
    if (fh != -1)
        close(fh);
  #endif /* WIN32 */
    os_res = os_access("read_write_exec" OS_OS_EXESUFFIX, OS_ROK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read/write/exec permissions write access */
    printf ("Starting os_stdlib_access_022\n");
    os_res = os_access("read_write_exec" OS_OS_EXESUFFIX, OS_WOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read/write/exec permissions execute access */
    printf ("Starting os_stdlib_access_023\n");
    os_res = os_access("read_write_exec" OS_OS_EXESUFFIX, OS_XOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read/write/exec permissions existence */
    printf ("Starting os_stdlib_access_024\n");
    os_res = os_access("read_write_exec" OS_OS_EXESUFFIX, OS_FOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read/exec permissions read+write access */
    printf ("Starting os_stdlib_access_025\n");
  #ifdef WIN32
    fh= _creat("read_exec" OS_OS_EXESUFFIX, _S_IREAD); /* Windows always has read and can't do execute (that's based upon filename ext only) */
    if (fh != -1)
        _close(fh);
  #else
    fh= creat("read_exec" OS_OS_EXESUFFIX, S_IRUSR | S_IXUSR);
    if (fh != -1)
        close(fh);
  #endif /* WIN32 */
    os_res = os_access("read_exec" OS_OS_EXESUFFIX, OS_ROK|OS_WOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access,
       existing file with read/exec permissions write+exec access */
    printf ("Starting os_stdlib_access_026\n");
    os_res = os_access("read_exec" OS_OS_EXESUFFIX, OS_WOK|OS_XOK);
    CU_ASSERT (os_res == os_resultFail);

    /* Check correct functioning of os_access,
       existing file with read/exec permissions read+exec access */
    printf ("Starting os_stdlib_access_027\n");
    os_res = os_access("read_exec" OS_OS_EXESUFFIX, OS_ROK|OS_XOK);
    CU_ASSERT (os_res == os_resultSuccess);

    /* Check correct functioning of os_access,
       existing file with read/exec permissions read+exec+existence */
    printf ("Starting os_stdlib_access_028\n");
    os_res = os_access("read_exec" OS_OS_EXESUFFIX, OS_ROK|OS_XOK|OS_FOK);
    CU_ASSERT (os_res == os_resultSuccess);
#endif /* VXWORKS */

    printf ("Ending stdlib_os_access\n");
}

CUnit_Test(os_stdlib, vsnprintf)
{
    printf ("Starting os_stdlib_vsnprintf_001\n");
    CU_ASSERT (vsnprintfTest("%s","test") == 4);
    CU_ASSERT (vsnprintfTest("%d",12) == 2);
    CU_ASSERT (vsnprintfTest("hello %s","world") == 11);

    printf ("Ending os_stdlib_vsnprintf\n");
}

CUnit_Test(os_stdlib, strtok_r)
{
    char * res;
    char *strtok_r_ts1;
    char *saveptr;

    printf ("Starting os_stdlib_strtok_r_001\n");
     strtok_r_ts1= os_strdup("123,234");
     res = os_strtok_r( strtok_r_ts1, ",", &saveptr );
     CU_ASSERT (strcmp(res, "123") == 0);

    printf ("Starting os_stdlib_strtok_r_002\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "234") == 0);

    printf ("Starting os_stdlib_strtok_r_003\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (res == NULL);
    os_free(strtok_r_ts1);

    printf ("Starting os_stdlib_strtok_r_004\n");
    strtok_r_ts1= os_strdup(",;,123abc,,456,:,");
    res = os_strtok_r( strtok_r_ts1, ",;", &saveptr );
    CU_ASSERT (strcmp(res, "123abc") == 0);

    printf ("Starting os_stdlib_strtok_r_005\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "456") == 0);

    printf ("Starting os_stdlib_strtok_r_006\n");
    res = os_strtok_r( NULL, ",:", &saveptr );
    CU_ASSERT (res == NULL);
    free(strtok_r_ts1);

    printf ("Starting os_stdlib_strtok_r_007\n");
    strtok_r_ts1= os_strdup(",,,123,,456,789,,,");
    res = os_strtok_r( strtok_r_ts1, ",", &saveptr );
    CU_ASSERT (strcmp(res, "123") == 0);

    printf ("Starting os_stdlib_strtok_r_008\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "456") == 0);

    printf ("Starting os_stdlib_strtok_r_009\n");
    res = os_strtok_r( NULL, ",", &saveptr );
    CU_ASSERT (strcmp(res, "789") == 0);

    printf ("Starting os_stdlib_strtok_r_010\n");
    res = os_strtok_r( NULL, ",:", &saveptr );
    CU_ASSERT (res == NULL);
    free(strtok_r_ts1);

    printf ("Ending os_stdlib_strtok_r\n");
}

CUnit_Test(os_stdlib, index)
{
    char * res;
    char *index_ts1;
    printf ("Starting os_stdlib_index_001\n");
    index_ts1 = "abc";
    res = os_index( index_ts1, 'a' );
    CU_ASSERT (res == index_ts1);

    printf ("Starting os_stdlib_index_002\n");
    res = os_index( index_ts1, 'c' );
    CU_ASSERT (res == &index_ts1[2]);

    printf ("Starting os_stdlib_index_003\n");
    index_ts1 = "abcdefghij";
    res = os_index( index_ts1, 'f' );
    CU_ASSERT (res == &index_ts1[5]);

    printf ("Starting os_stdlib_index_004\n");
    res = os_index( index_ts1, 'k' );
    CU_ASSERT (res == NULL);

    printf ("Ending os_stdlib_index\n");
}

CUnit_Test(os_stdlib, flockfile)
{
	bool result = false;

	os_osInit();

	/* Check writing in a FILE from multiple threads without using os_flockfile. */
	printf ("Starting os_stdlib_flockfile_001\n");
	result = doFlockfileTest(false);
	CU_ASSERT (result);

	/* Check writing in a FILE from multiple threads using os_flockfile in the first thread. */
	printf ("Starting os_stdlib_flockfile_002\n");
	result = doFlockfileTest(true);
	CU_ASSERT (result);

	printf ("Ending os_stdlib_flockfile\n");

	os_osExit();
}

CUnit_Test(os_stdlib, getopt)
{
	int c = 0;
	int argc = 3;
	char *argv001[] = {"", "-a", "-b"};
	char *argv002[] = {"", "-c", "foo"};
	char *argv003[] = {"", "-d"};

	/* Check correct functioning of os_getopt */
	printf ("Starting os_stdlib_getopt_001\n");
	c = os_getopt(argc, argv001, "abc:");
	CU_ASSERT (c == 'a');
	c = os_getopt(argc, argv001, "abc:");
	CU_ASSERT (c == 'b');
	c = os_getopt(argc, argv001, "abc:");
	CU_ASSERT (c == -1);

	/* Check correct functioning of os_set_optind and os_get_optind */
	printf ("Starting os_stdlib_getopt_002\n");
	os_set_optind(1);
	CU_ASSERT (os_get_optind() == 1);

	/* Check correct functioning of os_get_optarg */
	printf ("Starting os_stdlib_getopt_003\n");
	c = os_getopt (argc, argv002, "c:");
	CU_ASSERT (c == 'c');
	CU_ASSERT (strcmp(os_get_optarg(), "foo") == 0);
	c = os_getopt(argc, argv002, "c:");
	CU_ASSERT (c == -1);

	/* Check correct functioning of os_set_opterr, os_get_opterr and os_get_optopt */
	printf ("Starting os_stdlib_getopt_004\n");
	argc = 2;
	os_set_optind(1);
	os_set_opterr(0);
	CU_ASSERT(os_get_opterr() == 0)
	c = os_getopt (argc, argv003, "c:");
	CU_ASSERT (c == '?');
	CU_ASSERT (os_get_optopt() == 'd');

	printf ("Ending os_stdlib_getopt\n");
}
