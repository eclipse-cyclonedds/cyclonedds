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
/** \file os/posix/code/os_thread.c
 *  \brief Posix thread management
 *
 * Implements thread management for POSIX
 */

#include "os/os.h"

#include <sys/types.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
/* TODO: should introduce a HAVE_PRCTL define rather than blacklisting some platforms */
#if !defined __VXWORKS__ && !defined __APPLE__
#include <sys/prctl.h>
#endif
#include <limits.h>

typedef struct {
    char *threadName;
    void *arguments;
    uint32_t (*startRoutine)(void *);
} os_threadContext;

static pthread_key_t os_threadNameKey;
static pthread_key_t os_threadMemKey;

static sigset_t os_threadBlockAllMask;

/** \brief Initialize the thread private memory array
 *
 * \b os_threadMemInit initializes the thread private memory array
 *    for the calling thread
 *
 * pre condition:
 *    os_threadMemKey is initialized
 *
 * post condition:
 *       pthreadMemArray is initialized
 *    or
 *       an appropriate error report is generated
 */
static void
os_threadMemInit (
    void)
{
    void *pthreadMemArray;

    pthreadMemArray = os_malloc (sizeof(void *) * OS_THREAD_MEM_ARRAY_SIZE);
    if (pthreadMemArray != NULL) {
        memset (pthreadMemArray, 0, sizeof(void *) * OS_THREAD_MEM_ARRAY_SIZE);
        if (pthread_setspecific (os_threadMemKey, pthreadMemArray) == EINVAL) {
            OS_ERROR("os_threadMemInit", 4,
                         "pthread_setspecific failed with error EINVAL (%d), "
                         "invalid threadMemKey value", EINVAL);
            os_free(pthreadMemArray);
        }
    } else {
        OS_ERROR("os_threadMemInit", 3, "Out of heap memory");
    }
}

/** \brief Initialize the thread private memory array
 *
 * \b os_threadMemInit releases the thread private memory array
 *    for the calling thread, the allocated private memory areas
 *    referenced by the array are also freed.
 *
 * pre condition:
 *    os_threadMemKey is initialized
 *
 * post condition:
 *       pthreadMemArray is released
 *    or
 *       an appropriate error report is generated
 */
static void
os_threadMemExit(
    void)
{
    void **pthreadMemArray;
    int32_t i;

    pthreadMemArray = pthread_getspecific (os_threadMemKey);
    if (pthreadMemArray != NULL) {
        for (i = 0; i < OS_THREAD_MEM_ARRAY_SIZE; i++) {
            if (pthreadMemArray[i] != NULL) {
                os_free (pthreadMemArray[i]);
            }
        }
        os_free (pthreadMemArray);
        pthreadMemArray = NULL;
        if (pthread_setspecific (os_threadMemKey, pthreadMemArray) == EINVAL) {
            OS_ERROR("os_threadMemExit", 4, "pthread_setspecific failed with error %d", EINVAL);
        }
    }
}

/** \brief Initialize the thread module
 *
 * \b os_threadModuleInit initializes the thread module for the
 *    calling process
 */
void
os_threadModuleInit (
    void)
{
    pthread_key_create (&os_threadNameKey, NULL);
    pthread_key_create (&os_threadMemKey, NULL);

    sigfillset(&os_threadBlockAllMask);

    os_threadMemInit();
}

/** \brief Deinitialize the thread module
 *
 * \b os_threadModuleExit deinitializes the thread module for the
 *    calling process
 */
void
os_threadModuleExit(void)
{
    os_threadMemExit();

    pthread_key_delete(os_threadNameKey);
    pthread_key_delete(os_threadMemKey);
}

/** \brief Wrap thread start routine
 *
 * \b os_startRoutineWrapper wraps a threads starting routine.
 * before calling the user routine, it sets the threads name
 * in the context of the thread. With \b pthread_getspecific,
 * the name can be retreived for different purposes.
 */
static void *
os_startRoutineWrapper (
    void *threadContext)
{
    os_threadContext *context = threadContext;
    uintptr_t resultValue;
    os_threadId id;

    resultValue = 0;

#if !defined(__VXWORKS__) && !defined(__APPLE__)
    prctl(PR_SET_NAME, context->threadName);
#endif

    /* store the thread name with the thread via thread specific data; failure isn't  */
    if (pthread_setspecific (os_threadNameKey, context->threadName) == EINVAL) {
        OS_WARNING("os_startRoutineWrapper", 0,
                     "pthread_setspecific failed with error EINVAL (%d), "
                     "invalid os_threadNameKey value", EINVAL);
    }

    /* allocate an array to store thread private memory references */
    os_threadMemInit ();

    id.v = pthread_self();
    /* Call the user routine */
    resultValue = context->startRoutine (context->arguments);

    os_report_stack_free();

    /* Free the thread context resources, arguments is responsibility */
    /* for the caller of os_procCreate                                */
    os_free (context->threadName);
    os_free (context);

    /* deallocate the array to store thread private memory references */
    os_threadMemExit ();

    /* return the result of the user routine */
    return (void *)resultValue;
}

/** \brief Create a new thread
 *
 * \b os_threadCreate creates a thread by calling \b pthread_create.
 * But first it processes all thread attributes in \b threadAttr and
 * sets the scheduling properties with \b pthread_attr_setscope
 * to create a bounded thread, \b pthread_attr_setschedpolicy to
 * set the scheduling class and \b pthread_attr_setschedparam to
 * set the scheduling priority.
 * \b pthread_attr_setdetachstate is called with parameter
 * \PTHREAD_CREATE_JOINABLE to make the thread joinable, which
 * is needed to be able to wait for the threads termination
 * in \b os_threadWaitExit.
 */
os_result
os_threadCreate (
    os_threadId *threadId,
    const char *name,
    const os_threadAttr *threadAttr,
    uint32_t (* start_routine)(void *),
    void *arg)
{
    pthread_attr_t attr;
    struct sched_param sched_param;
    os_result rv = os_resultSuccess;
    os_threadContext *threadContext;
    os_threadAttr tattr;
    int result, create_ret;
    int policy;

    assert (threadId != NULL);
    assert (name != NULL);
    assert (threadAttr != NULL);
    assert (start_routine != NULL);
    tattr = *threadAttr;

    if (tattr.schedClass == OS_SCHED_DEFAULT) {
#if 0 /* FIXME! */
        tattr.schedClass = os_procAttrGetClass ();
        tattr.schedPriority = os_procAttrGetPriority ();
#endif
    }
    if (pthread_attr_init (&attr) != 0)
    {
       rv = os_resultFail;
    }
    else
    {
#ifdef __VXWORKS__
       /* PR_SET_NAME is not available on VxWorks. Use pthread_attr_setname
          instead (proprietary VxWorks extension) */
       (void)pthread_attr_setname(&attr, name);
#endif
       if (pthread_getschedparam(pthread_self(), &policy, &sched_param) != 0 ||
#if !defined (OS_RTEMS_DEFS_H) && !defined (PIKEOS_POSIX)
           pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM) != 0 ||
#endif
           pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE) != 0 ||
           pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED) != 0)
       {
          rv = os_resultFail;
       }
       else
       {
          if (tattr.stackSize != 0) {
#ifdef PTHREAD_STACK_MIN
             if ( tattr.stackSize < PTHREAD_STACK_MIN ) {
                tattr.stackSize = PTHREAD_STACK_MIN;
             }
#endif
#ifdef OSPL_STACK_MAX
             if ( tattr.stackSize > OSPL_STACK_MAX ) {
                tattr.stackSize = OSPL_STACK_MAX;
             }
#endif
             if (pthread_attr_setstacksize (&attr, tattr.stackSize) != 0) {
                rv = os_resultFail;
             }
          }
       }

       if (rv == os_resultSuccess) {
          if (tattr.schedClass == OS_SCHED_REALTIME) {
             result = pthread_attr_setschedpolicy (&attr, SCHED_FIFO);

             if (result != 0) {
                char errmsg[64];
                (void)os_strerror_r(result, errmsg, sizeof(errmsg));
                OS_WARNING("os_threadCreate", 2,
                             "pthread_attr_setschedpolicy failed for SCHED_FIFO with "\
                             "error %d (%s) for thread '%s', reverting to SCHED_OTHER.",
                             result, errmsg, name);

                result = pthread_attr_setschedpolicy (&attr, SCHED_OTHER);
                if (result != 0) {
                   OS_WARNING("os_threadCreate", 2, "pthread_attr_setschedpolicy failed with error %d (%s)", result, name);
                }
             }
          } else {
             result = pthread_attr_setschedpolicy (&attr, SCHED_OTHER);

             if (result != 0) {
                OS_WARNING("os_threadCreate", 2,
                             "pthread_attr_setschedpolicy failed with error %d (%s)",
                             result, name);
             }
          }
          pthread_attr_getschedpolicy(&attr, &policy);

          if ((tattr.schedPriority < sched_get_priority_min(policy)) ||
              (tattr.schedPriority > sched_get_priority_max(policy))) {
             OS_WARNING("os_threadCreate", 2,
                          "scheduling priority outside valid range for the policy "\
                          "reverted to valid value (%s)", name);
             sched_param.sched_priority = (sched_get_priority_min(policy) +
                                           sched_get_priority_max(policy)) / 2;
          } else {
             sched_param.sched_priority = tattr.schedPriority;
          }
          /* Take over the thread context: name, start routine and argument */
          threadContext = os_malloc (sizeof (os_threadContext));
          threadContext->threadName = os_malloc (strlen (name)+1);
          strncpy (threadContext->threadName, name, strlen (name)+1);
          threadContext->startRoutine = start_routine;
          threadContext->arguments = arg;

          /* start the thread */
          result = pthread_attr_setschedparam (&attr, &sched_param);
          if (result != 0) {
             OS_WARNING("os_threadCreate", 2,
                          "pthread_attr_setschedparam failed with error %d (%s)",
                          result, name);
          }

          create_ret = pthread_create(&threadId->v, &attr, os_startRoutineWrapper,
                                      threadContext);
          if (create_ret != 0) {
             /* In case real-time thread creation failed due to a lack
              * of permissions, try reverting to time-sharing and continue.
              */
             if((create_ret == EPERM) && (tattr.schedClass == OS_SCHED_REALTIME))
             {
                OS_WARNING("os_threadCreate", 2,
                             "pthread_create failed with SCHED_FIFO "     \
                             "for thread '%s', reverting to SCHED_OTHER.",
                             name);
                (void) pthread_attr_setschedpolicy (&attr, SCHED_OTHER); /* SCHED_OTHER is always supported */
                pthread_attr_getschedpolicy(&attr, &policy);

                if ((tattr.schedPriority < sched_get_priority_min(policy)) ||
                    (tattr.schedPriority > sched_get_priority_max(policy)))
                {
                   OS_WARNING("os_threadCreate", 2,
                                "scheduling priority outside valid range for the " \
                                "policy reverted to valid value (%s)", name);
                   sched_param.sched_priority =
                   (sched_get_priority_min(policy) +
                    sched_get_priority_max(policy)) / 2;
                } else {
                   sched_param.sched_priority = tattr.schedPriority;
                }

                result = pthread_attr_setschedparam (&attr, &sched_param);
                if (result != 0) {
                   OS_WARNING("os_threadCreate", 2,
                                "pthread_attr_setschedparam failed "      \
                                "with error %d (%s)", result, name);
                } else {
                   create_ret = pthread_create(&threadId->v, &attr,
                                               os_startRoutineWrapper, threadContext);
                }
             }
          } else {
             rv = os_resultSuccess;
          }
          if(create_ret != 0){
             os_free (threadContext->threadName);
             os_free (threadContext);
             OS_WARNING("os_threadCreate", 2, "pthread_create failed with error %d (%s)", create_ret, name);
             rv = os_resultFail;
          }
       }
       pthread_attr_destroy (&attr);
    }
    return rv;
}

/** \brief Return the integer representation of the given thread ID
 *
 * Possible Results:
 * - returns the integer representation of the given thread ID
 */
uintmax_t
os_threadIdToInteger(os_threadId id)
{
   return (uintmax_t)(uintptr_t)id.v;
}

/** \brief Return the thread ID of the calling thread
 *
 * \b os_threadIdSelf determines the own thread ID by
 * calling \b pthread_self.
 */
os_threadId
os_threadIdSelf (void)
{
    os_threadId id = {.v = pthread_self ()};
    return id;
}

/** \brief Figure out the identity of the current thread
 *
 * \b os_threadFigureIdentity determines the numeric identity
 * of a thread. POSIX does not identify threads by name,
 * therefore only the numeric identification is returned,
 */
int32_t
os_threadFigureIdentity (
    char *threadIdentity,
    uint32_t threadIdentitySize)
{
    int size;
    char *threadName;

    threadName = pthread_getspecific (os_threadNameKey);
    if (threadName != NULL) {
        size = snprintf (threadIdentity, threadIdentitySize, "%s 0x%"PRIxMAX, threadName, (uintmax_t)pthread_self ());
    } else {
        size = snprintf (threadIdentity, threadIdentitySize, "0x%"PRIxMAX, (uintmax_t)pthread_self ());
    }
    return size;
}

int32_t
os_threadGetThreadName (
    char *buffer,
    uint32_t length)
{
    char *name;

    assert (buffer != NULL);

    if ((name = pthread_getspecific (os_threadNameKey)) == NULL) {
        name = "";
    }

    return snprintf (buffer, length, "%s", name);
}

/** \brief Wait for the termination of the identified thread
 *
 * \b os_threadWaitExit wait for the termination of the
 * thread \b threadId by calling \b pthread_join. The return
 * value of the thread is passed via \b thread_result.
 */
os_result
os_threadWaitExit (
    os_threadId threadId,
    uint32_t *thread_result)
{
    os_result rv;
    int result;
    void *vthread_result;

    assert (threadId.v);

#if defined(__VXWORKS__) && !defined(_WRS_KERNEL)
    struct sched_param sched_param;
    int max, policy = 0;

    /* There is a known issue in pthread_join on VxWorks 6.x RTP mode.

       WindRiver: When pthread_join returns, it does not indicate end of a
       thread in 100% of the situations. If the thread that calls pthread_join
       has a higher priority than the thread that is currently terminating,
       pthread_join could return before pthread_exit has finished. This
       conflicts with the POSIX specification that dictates that pthread_join
       must only return when the thread is really terminated. The workaround
       suggested by WindRiver support is to increase the priority of the thread
       (task) to be terminated before handing back the semaphore to ensure the
       thread exits before pthread_join returns.

       This bug was submitted to WindRiver as TSR 815826. */

    /* Note that any possible errors raised here are not terminal since the
       thread may have exited at this point anyway. */
    if (pthread_getschedparam(threadId.v, &policy, &sched_param) == 0) {
        max = sched_get_priority_max(policy);
        if (max != -1) {
            (void)pthread_setschedprio(threadId.v, max);
        }
    }
#endif

    result = pthread_join (threadId.v, &vthread_result);
    if (result != 0) {
        /* NOTE: The below report actually is a debug output; makes no sense from
         * a customer perspective. Made OS_INFO for now. */
        OS_INFO("os_threadWaitExit", 2, "pthread_join(0x%"PRIxMAX") failed with error %d", os_threadIdToInteger(threadId), result);
        rv = os_resultFail;
    } else {
        rv = os_resultSuccess;
    }
    if(thread_result){
        uintptr_t res = (uintptr_t)vthread_result;
        *thread_result = (uint32_t)res;
    }
    return rv;
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
os_threadMemMalloc (
    int32_t index,
    size_t size)
{
    void **pthreadMemArray;
    void *threadMemLoc = NULL;

    if ((0 <= index) && (index < OS_THREAD_MEM_ARRAY_SIZE)) {
        pthreadMemArray = pthread_getspecific (os_threadMemKey);
        if (pthreadMemArray == NULL) {
            os_threadMemInit ();
            pthreadMemArray = pthread_getspecific (os_threadMemKey);
        }
        if (pthreadMemArray != NULL) {
            if (pthreadMemArray[index] == NULL) {
                threadMemLoc = os_malloc (size);
                if (threadMemLoc != NULL) {
                    pthreadMemArray[index] = threadMemLoc;
                }
            }
        }
    }
    return threadMemLoc;
}

/** \brief Free thread private memory
 *
 * Free the memory referenced by the thread reference
 * array indexed location. If this reference is NULL,
 * no action is taken. The reference is set to NULL
 * after freeing the heap memory.
 *
 * Postcondition:
 * - os_threadMemGet (index) = NULL and allocated
 *   heap memory is freed
 */
void
os_threadMemFree (
    int32_t index)
{
    void **pthreadMemArray;
    void *threadMemLoc = NULL;

    if ((0 <= index) && (index < OS_THREAD_MEM_ARRAY_SIZE)) {
        pthreadMemArray = pthread_getspecific (os_threadMemKey);
        if (pthreadMemArray != NULL) {
            threadMemLoc = pthreadMemArray[index];
            if (threadMemLoc != NULL) {
                pthreadMemArray[index] = NULL;
                os_free (threadMemLoc);
            }
        }
    }
}

/** \brief Get thread private memory
 *
 * Possible Results:
 * - returns NULL if
 *     0 < index <= OS_THREAD_MEM_ARRAY_SIZE
 * - returns NULL if
 *     No heap memory is related to the thread for
 *     the specified index
 * - returns a reference to the allocated memory
 */
void *
os_threadMemGet (
    int32_t index)
{
    void **pthreadMemArray;
    void *threadMemLoc = NULL;

    if ((0 <= index) && (index < OS_THREAD_MEM_ARRAY_SIZE)) {
        pthreadMemArray = pthread_getspecific (os_threadMemKey);
        if (pthreadMemArray != NULL) {
            threadMemLoc = pthreadMemArray[index];
        }
    }
    return threadMemLoc;
}


static pthread_key_t cleanup_key;
static pthread_once_t cleanup_once = PTHREAD_ONCE_INIT;

static void
os_threadCleanupFini(
    void *data)
{
    os_iter *itr;
    os_threadCleanup *obj;

    if (data != NULL) {
        itr = (os_iter *)data;
        for (obj = (os_threadCleanup *)os_iterTake(itr, -1);
             obj != NULL;
             obj = (os_threadCleanup *)os_iterTake(itr, -1))
        {
            assert(obj->func != NULL);
            obj->func(obj->data);
            os_free(obj);
        }
        os_iterFree(itr, NULL);
    }
}

static void
os_threadCleanupInit(
    void)
{
    (void)pthread_key_create(&cleanup_key, &os_threadCleanupFini);
}

/* os_threadCleanupPush and os_threadCleanupPop are mapped onto a destructor
   registered with pthread_key_create in stead of being mapped directly onto
   pthread_cleanup_push/pthread_cleanup_pop because the cleanup routines could
   otherwise be popped of the stack by the user */
void
os_threadCleanupPush(
    void (*func)(void*),
    void *data)
{
    os_iter *itr;
    os_threadCleanup *obj;

    assert(func != NULL);

    (void)pthread_once(&cleanup_once, &os_threadCleanupInit);
    itr = (os_iter *)pthread_getspecific(cleanup_key);
    if (itr == NULL) {
        itr = os_iterNew();
        assert(itr != NULL);
        if (pthread_setspecific(cleanup_key, itr) == EINVAL) {
            OS_WARNING(OS_FUNCTION, 0, "pthread_setspecific failed with error EINVAL (%d)", EINVAL);
            os_iterFree(itr, NULL);
            itr = NULL;
        }
    }

    if(itr) {
        obj = os_malloc(sizeof(*obj));
        obj->func = func;
        obj->data = data;
        os_iterAppend(itr, obj);
    }
}

void
os_threadCleanupPop(
    int execute)
{
    os_iter *itr;
    os_threadCleanup *obj;

    (void)pthread_once(&cleanup_once, &os_threadCleanupInit);
    if ((itr = (os_iter *)pthread_getspecific(cleanup_key)) != NULL) {
        obj = (os_threadCleanup *)os_iterTake(itr, -1);
        if (obj != NULL) {
            if (execute) {
                obj->func(obj->data);
            }
            os_free(obj);
        }
    }
}
