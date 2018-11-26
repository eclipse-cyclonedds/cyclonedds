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
/** \file os/win32/code/os_thread.c
 *  \brief WIN32 thread management
 *
 * Implements thread management for WIN32
 */

#include "os/os.h"

#include <assert.h>

typedef struct {
    char *threadName;
    void *arguments;
    os_threadRoutine startRoutine;
} os_threadContext;

static DWORD tlsIndex;

static os_result
os_threadMemInit(void)
{
    void **tlsMemArray;
    BOOL result;

    tlsMemArray = os_malloc (sizeof(void *) * OS_THREAD_MEM_ARRAY_SIZE);
    memset(tlsMemArray, 0, sizeof(void *) * OS_THREAD_MEM_ARRAY_SIZE);
    result = TlsSetValue(tlsIndex, tlsMemArray);
    if (!result) {
        //OS_INIT_FAIL("os_threadMemInit: failed to set TLS");
        goto err_setTls;
    }
    return os_resultSuccess;

err_setTls:
    os_free(tlsMemArray);
    return os_resultFail;
}

static void
os_threadMemExit(void)
{
    void **tlsMemArray;
    int i;

    tlsMemArray = (void **)TlsGetValue(tlsIndex);
    if (tlsMemArray != NULL) {
/*The compiler doesn't realize that tlsMemArray has always size OS_THREAD_MEM_ARRAY_SIZE. */
#pragma warning(push)
#pragma warning(disable: 6001)
        for (i = 0; i < OS_THREAD_MEM_ARRAY_SIZE; i++) {
            if (tlsMemArray[i] != NULL) {
                os_free(tlsMemArray[i]);
            }
        }
#pragma warning(pop)
        os_free(tlsMemArray);
        TlsSetValue(tlsIndex, NULL);
    }
}

/** \brief Initialize the thread module
 *
 * \b os_threadModuleInit initializes the thread module for the
 *    calling process
 */
os_result
os_threadModuleInit(void)
{
    if ((tlsIndex = TlsAlloc()) == TLS_OUT_OF_INDEXES) {
        //OS_INIT_FAIL("os_threadModuleInit: could not allocate thread-local memory (System Error Code: %i)", os_getErrno());
        goto err_tlsAllocFail;
    }

    return os_resultSuccess;

err_tlsAllocFail:
    return os_resultFail;
}

/** \brief Deinitialize the thread module
 *
 * \b os_threadModuleExit deinitializes the thread module for the
 *    calling process
 */
void
os_threadModuleExit(void)
{
    void **tlsMemArray = TlsGetValue(tlsIndex);

    if (tlsMemArray != NULL) {
        os_free(tlsMemArray);
    }

    TlsFree(tlsIndex);
}

const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   /** Must be 0x1000. */
   DWORD dwType;
   /** Pointer to name (in user addr space). */
   LPCSTR szName;
   /** Thread ID (-1=caller thread). */
   DWORD dwThreadID;
   /**  Reserved for future use, must be zero. */
   DWORD dwFlags;
} THREADNAME_INFO;
#pragma pack(pop)

/**
* Usage: os_threadSetThreadName (-1, "MainThread");
* @pre ::
* @see http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx
* @param dwThreadID The thread ID that is to be named, -1 for 'self'
* @param threadName The name to apply.
*/
void os_threadSetThreadName( DWORD dwThreadID, char* threadName)
{
   char* tssThreadName;
#ifndef WINCE /* When we merge the code, this first bit won't work there */
   THREADNAME_INFO info;
   info.dwType = 0x1000;
   info.szName = threadName;
   info.dwThreadID = dwThreadID;
   info.dwFlags = 0;

/* Empty try/except that catches everything is done on purpose to set the
 * thread name. This code equals the official example on msdn, including
 * the warning suppressions. */
#pragma warning(push)
#pragma warning(disable: 6320 6322)
   __try
   {
      RaiseException( MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info );
   }
   __except(EXCEPTION_EXECUTE_HANDLER)
   {
   }
#pragma warning(pop)
#endif /* No reason why the restshouldn't though */

    tssThreadName = (char *)os_threadMemGet(OS_THREAD_NAME);
    if (tssThreadName == NULL)
    {
        tssThreadName = (char *)os_threadMemMalloc(OS_THREAD_NAME, (strlen(threadName) + 1));
        strcpy(tssThreadName, threadName);
    }
}

/** \brief Wrap thread start routine
 *
 * \b os_startRoutineWrapper wraps a threads starting routine.
 * before calling the user routine. It tries to set a thread name
 * that will be visible if the process is running under the MS
 * debugger.
 */
static uint32_t
os_startRoutineWrapper(
    _In_ _Post_invalid_ void *threadContext)
{
    os_threadContext *context = threadContext;
    uint32_t resultValue = 0;
    os_threadId id;

    /* allocate an array to store thread private memory references */
    os_threadMemInit();

    /* Set a thread name that will take effect if the process is running under a debugger */
    os_threadSetThreadName(-1, context->threadName);

    id.threadId = GetCurrentThreadId();
    id.handle = GetCurrentThread();

    /* Call the user routine */
    resultValue = context->startRoutine(context->arguments);

    /* Free the thread context resources, arguments is responsibility */
    /* for the caller of os_threadCreate                                */
    os_free (context->threadName);
    os_free (context);

    /* deallocate the array to store thread private memory references */
    os_threadMemExit ();

    /* return the result of the user routine */
    return resultValue;
}

/** \brief Create a new thread
 *
 * \b os_threadCreate creates a thread by calling \b CreateThread.
 */
os_result
os_threadCreate(
    _Out_ os_threadId *threadId,
    _In_z_ const char *name,
    _In_ const os_threadAttr *threadAttr,
    _In_ os_threadRoutine start_routine,
    _In_opt_ void *arg)
{
    HANDLE threadHandle;
    DWORD threadIdent;
    os_threadContext *threadContext;

    int32_t effective_priority;

    assert(threadId != NULL);
    assert(name != NULL);
    assert(threadAttr != NULL);
    assert(start_routine != NULL);

    /* Take over the thread context: name, start routine and argument */
    threadContext = os_malloc(sizeof (*threadContext));
    threadContext->threadName = os_strdup(name);
    threadContext->startRoutine = start_routine;
    threadContext->arguments = arg;
    threadHandle = CreateThread(NULL,
        (SIZE_T)threadAttr->stackSize,
        (LPTHREAD_START_ROUTINE)os_startRoutineWrapper,
        (LPVOID)threadContext,
        (DWORD)0, &threadIdent);
    if (threadHandle == 0) {
        DDS_WARNING("Failed with System Error Code: %i\n", os_getErrno ());
        return os_resultFail;
    }

    fflush(stdout);

    threadId->handle   = threadHandle;
    threadId->threadId = threadIdent;

    /*  #642 fix (JCM)
     *  Windows thread priorities are in the range below :
    -15 : THREAD_PRIORITY_IDLE
    -2  : THREAD_PRIORITY_LOWEST
    -1  : THREAD_PRIORITY_BELOW_NORMAL
     0  : THREAD_PRIORITY_NORMAL
     1  : THREAD_PRIORITY_ABOVE_NORMAL
     2  : THREAD_PRIORITY_HIGHEST
    15  : THREAD_PRIORITY_TIME_CRITICAL
    For realtime threads additional values are allowed : */

    /* PROCESS_QUERY_INFORMATION rights required
     * to call GetPriorityClass
     * Ensure that priorities are effectively in the allowed range depending
     * on GetPriorityClass result */
    effective_priority = threadAttr->schedPriority;
    if (GetPriorityClass(GetCurrentProcess()) == REALTIME_PRIORITY_CLASS) {
        if (threadAttr->schedPriority < -7) {
            effective_priority = THREAD_PRIORITY_IDLE;
        }
        if (threadAttr->schedPriority > 6) {
            effective_priority = THREAD_PRIORITY_TIME_CRITICAL;
        }
    } else {
        if (threadAttr->schedPriority < THREAD_PRIORITY_LOWEST) {
            effective_priority = THREAD_PRIORITY_IDLE;
        }
        if (threadAttr->schedPriority > THREAD_PRIORITY_HIGHEST) {
            effective_priority = THREAD_PRIORITY_TIME_CRITICAL;
        }
    }
    if (SetThreadPriority (threadHandle, effective_priority) == 0) {
        DDS_INFO("SetThreadPriority failed with %i\n", os_getErrno());
    }

   /* ES: dds2086: Close handle should not be performed here. Instead the handle
    * should not be closed until the os_threadWaitExit(...) call is called.
    * CloseHandle (threadHandle);
    */
   return os_resultSuccess;
}

/** \brief Return the integer representation of the given thread ID
 *
 * Possible Results:
 * - returns the integer representation of the given thread ID
 */
uintmax_t
os_threadIdToInteger(os_threadId id)
{
   return id.threadId;
}

/** \brief Return the thread ID of the calling thread
 *
 * \b os_threadIdSelf determines the own thread ID by
 * calling \b GetCurrentThreadId ().
 */
os_threadId
os_threadIdSelf(
    void)
{
   os_threadId id;
   id.threadId = GetCurrentThreadId();
   id.handle = GetCurrentThread();   /* pseudo HANDLE, no need to close it */

   return id;
}

/** \brief Wait for the termination of the identified thread
 *
 * \b os_threadWaitExit wait for the termination of the
 * thread \b threadId by calling \b pthread_join. The return
 * value of the thread is passed via \b thread_result.
 */
os_result
os_threadWaitExit(
    _In_ os_threadId threadId,
    _Out_opt_ uint32_t *thread_result)
{
    DWORD tr;
    DWORD err;
    DWORD waitres;
    BOOL status;

    if(threadId.handle == NULL){
        //OS_DEBUG("os_threadWaitExit", "Parameter threadId is null");
        return os_resultFail;
    }

    waitres = WaitForSingleObject(threadId.handle, INFINITE);
    if (waitres != WAIT_OBJECT_0) {
        err = os_getErrno();
        //OS_DEBUG_1("os_threadWaitExit", "WaitForSingleObject Failed %d", err);
        return os_resultFail;
    }

    status = GetExitCodeThread(threadId.handle, &tr);
    if (!status) {
       err = os_getErrno();
       //OS_DEBUG_1("os_threadWaitExit", "GetExitCodeThread Failed %d", err);
       return os_resultFail;
    }

    assert(tr != STILL_ACTIVE);
    if (thread_result) {
        *thread_result = tr;
    }
    CloseHandle(threadId.handle);

    return os_resultSuccess;
}

int
os_threadGetThreadName(
    char *buffer,
    uint32_t length)
{
    char *name;

    assert (buffer != NULL);

    if ((name = os_threadMemGet(OS_THREAD_NAME)) == NULL) {
        name = "";
    }

    return snprintf (buffer, length, "%s", name);
}

/** \brief Allocate thread private memory
 *
 * Allocate heap memory of the specified \b size and
 * relate it to the thread by storing the memory
 * reference in an thread specific reference array
 * indexed by \b index. If the indexed thread reference
 * array location already contains a reference, no
 * memory will be allocated and NULL is returned.
 *
 * Possible Results:
 * - returns NULL if
 *     index < 0 || index >= OS_THREAD_MEM_ARRAY_SIZE
 * - returns NULL if
 *     no sufficient memory is available on heap
 * - returns NULL if
 *     os_threadMemGet (index) returns != NULL
 * - returns reference to allocated heap memory
 *     of the requested size if
 *     memory is successfully allocated
 */
void *
os_threadMemMalloc(
    int32_t index,
    size_t size)
{
   void **tlsMemArray;
   void *threadMemLoc = NULL;

    if ((0 <= index) && (index < OS_THREAD_MEM_ARRAY_SIZE)) {
        tlsMemArray = (void **)TlsGetValue(tlsIndex);
        if (tlsMemArray == NULL) {
            os_threadMemInit ();
            tlsMemArray = (void **)TlsGetValue(tlsIndex);
        }
        if (tlsMemArray != NULL) {
            if (tlsMemArray[index] == NULL) {
                threadMemLoc = os_malloc(size);
                tlsMemArray[index] = threadMemLoc;
            }
        }
    }
    return threadMemLoc;
}

/** \brief Free thread private memory
 *
 * Free the memory referenced by the thread reference
 * array indexed location. If this reference is NULL,
 * or index is invalid, no action is taken.
 * The reference is set to NULL after freeing the
 * heap memory.
 *
 * Postcondition:
 * - os_threadMemGet (index) = NULL and allocated
 *   heap memory is freed
 */
void
os_threadMemFree(
    int32_t index)
{
    void **tlsMemArray;
    void *threadMemLoc = NULL;

    if ((0 <= index) && (index < OS_THREAD_MEM_ARRAY_SIZE)) {
        tlsMemArray = (void **)TlsGetValue(tlsIndex);
        if (tlsMemArray != NULL) {
            threadMemLoc = tlsMemArray[index];
            if (threadMemLoc != NULL) {
                tlsMemArray[index] = NULL;
                os_free(threadMemLoc);
            }
        }
    }
}

/** \brief Get thread private memory
 *
 * Possible Results:
 * - returns NULL if
 *     No heap memory is related to the thread for
 *     the specified index
 * - returns a reference to the allocated memory
 */
void *
os_threadMemGet(
    int32_t index)
{
    void **tlsMemArray;
    void *data;

    data = NULL;
    if ((0 <= index) && (index < OS_THREAD_MEM_ARRAY_SIZE)) {
        tlsMemArray = TlsGetValue(tlsIndex);
        if (tlsMemArray != NULL) {
            data = tlsMemArray[index];
        }
    }

    return data;
}


static os_threadLocal os_iter *cleanup_funcs;

/* executed before dllmain within the context of the thread itself */
void NTAPI
os_threadCleanupFini(
    PVOID handle,
    DWORD reason,
    PVOID reserved)
{
    os_threadCleanup *obj;

    switch(reason) {
        case DLL_PROCESS_DETACH: /* specified when main thread exits */
        case DLL_THREAD_DETACH: /* specified when thread exits */
            if (cleanup_funcs != NULL) {
                for (obj = (os_threadCleanup *)os_iterTake(cleanup_funcs, -1);
                     obj != NULL;
                     obj = (os_threadCleanup *)os_iterTake(cleanup_funcs, -1))
                {
                    assert(obj->func != NULL);
                    obj->func(obj->data);
                    os_free(obj);
                }
                os_iterFree(cleanup_funcs, NULL);
            }
            cleanup_funcs = NULL;
            break;
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        default:
            /* do nothing */
            break;
    }

    (void)handle;
    (void)reserved;
}

/* These instructions are very specific to the Windows platform. They register
   a function (or multiple) as a TLS initialization function. TLS initializers
   are executed when a thread (or program) attaches or detaches. In contrast to
   DllMain, a TLS initializer is also executed when the library is linked
   statically. TLS initializers are always executed before DllMain (both when
   the library is attached and detached). See http://www.nynaeve.net/?p=190,
   for a detailed explanation on TLS initializers. Boost and/or POSIX Threads
   for Windows code bases may also form good sources of information on this
   subject.

   These instructions could theoretically be hidden in the build system, but
   doing so would be going a bit overboard as only Windows offers (and
   requires) this type of functionality/initialization. Apart from that the
   logic isn't exactly as trivial as for example determining the endianness of
   a platform, so keeping this close to the implementation is probably wise. */
#ifdef _WIN64
    #pragma comment (linker, "/INCLUDE:_tls_used")
    #pragma comment (linker, "/INCLUDE:tls_callback_func")
    #pragma const_seg(".CRT$XLZ")
    EXTERN_C const PIMAGE_TLS_CALLBACK tls_callback_func = os_threadCleanupFini;
    #pragma const_seg()
#else
    #pragma comment (linker, "/INCLUDE:__tls_used")
    #pragma comment (linker, "/INCLUDE:_tls_callback_func")
    #pragma data_seg(".CRT$XLZ")
    EXTERN_C PIMAGE_TLS_CALLBACK tls_callback_func = os_threadCleanupFini;
    #pragma data_seg()
#endif

void
os_threadCleanupPush(
    void (*func)(void *),
    void *data)
{
    os_threadCleanup *obj;

    assert(func != NULL);

    if (cleanup_funcs == NULL) {
        cleanup_funcs = os_iterNew();
        assert(cleanup_funcs != NULL);
    }

    obj = os_malloc(sizeof(*obj));
    assert(obj != NULL);
    obj->func = func;
    obj->data = data;
    (void)os_iterAppend(cleanup_funcs, obj);
}

void
os_threadCleanupPop(
    int execute)
{
    os_threadCleanup *obj;

    if (cleanup_funcs != NULL) {
        obj = os_iterTake(cleanup_funcs, -1);
        if (obj != NULL) {
            if (execute) {
                obj->func(obj->data);
            }
            os_free(obj);
        }
    }
}
