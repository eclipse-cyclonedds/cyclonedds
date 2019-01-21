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
#if !defined __VXWORKS__ && !defined __APPLE__ && !defined __sun
#include <sys/prctl.h>
#endif
#include <limits.h>

typedef struct {
  char *threadName;
  void *arguments;
  uint32_t (*startRoutine) (void *);
} os_threadContext;

static pthread_key_t os_threadNameKey;
static pthread_key_t os_threadMemKey;
static pthread_key_t cleanup_key;
static pthread_once_t cleanup_once = PTHREAD_ONCE_INIT;

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
static void os_threadMemInit (void)
{
  const size_t sz = sizeof(void *) * OS_THREAD_MEM_ARRAY_SIZE;
  void *pthreadMemArray;
  int ret;
  if ((pthreadMemArray = os_malloc (sz)) == NULL)
    DDS_FATAL ("os_threadMemInit: out of memory\n");
  memset (pthreadMemArray, 0, sz);
  if ((ret = pthread_setspecific (os_threadMemKey, pthreadMemArray)) != 0)
    DDS_FATAL ("pthread_setspecific failed with error (%d), invalid threadMemKey value\n", ret);
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
static void os_threadMemExit (void)
{
  void **pthreadMemArray = pthread_getspecific (os_threadMemKey);
  if (pthreadMemArray != NULL)
  {
    int ret;
    for (int i = 0; i < OS_THREAD_MEM_ARRAY_SIZE; i++)
      if (pthreadMemArray[i] != NULL)
        os_free (pthreadMemArray[i]);
    os_free (pthreadMemArray);
    if ((ret = pthread_setspecific (os_threadMemKey, NULL)) != 0)
      DDS_FATAL ("pthread_setspecific failed with error %d\n", ret);
  }
}

/** \brief Initialize the thread module
 *
 * \b os_threadModuleInit initializes the thread module for the
 *    calling process
 */
void os_threadModuleInit (void)
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
void os_threadModuleExit (void)
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
static void *os_startRoutineWrapper (void *threadContext)
{
  os_threadContext *context = threadContext;
  uintptr_t resultValue;
  int ret;

#if !defined(__VXWORKS__) && !defined(__APPLE__) && !defined(__sun)
  /* FIXME: Switch to use pthread_setname_np in the future.
   * Linux: pthread_setname_np(pthread_t, const char *)
   * macOS: pthread_setname_np(const char *)
   * FreeBSD: pthread_set_name_np(pthread_t, const char *) */
  prctl(PR_SET_NAME, context->threadName);
#endif

  /* store the thread name with the thread via thread specific data */
  if ((ret = pthread_setspecific (os_threadNameKey, context->threadName)) != 0)
    DDS_FATAL ("pthread_setspecific failed with error %d, invalid os_threadNameKey value\n", ret);

  /* allocate an array to store thread private memory references */
  os_threadMemInit ();

  /* Call the user routine */
  resultValue = context->startRoutine (context->arguments);

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
os_result os_threadCreate (os_threadId *threadId, const char *name, const os_threadAttr *threadAttr, uint32_t (*start_routine) (void *), void *arg)
{
  pthread_attr_t attr;
  os_threadContext *threadContext;
  os_threadAttr tattr;
  int result, create_ret;

  assert (threadId != NULL);
  assert (name != NULL);
  assert (threadAttr != NULL);
  assert (start_routine != NULL);
  tattr = *threadAttr;

  if (pthread_attr_init (&attr) != 0)
    return os_resultFail;

#ifdef __VXWORKS__
  /* PR_SET_NAME is not available on VxWorks. Use pthread_attr_setname instead (proprietary VxWorks extension) */
  (void) pthread_attr_setname (&attr, name);
#endif

  if (pthread_attr_setscope (&attr, PTHREAD_SCOPE_SYSTEM) != 0 ||
      pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_JOINABLE) != 0)
    goto err;

  if (tattr.stackSize != 0)
  {
#ifdef PTHREAD_STACK_MIN
    if (tattr.stackSize < PTHREAD_STACK_MIN)
      tattr.stackSize = PTHREAD_STACK_MIN;
#endif
    if ((result = pthread_attr_setstacksize (&attr, tattr.stackSize)) != 0)
    {
      DDS_ERROR ("os_threadCreate(%s): pthread_attr_setstacksize(%"PRIu32") failed with error %d\n", name, tattr.stackSize, result);
      goto err;
    }
  }

  if (tattr.schedClass == OS_SCHED_DEFAULT)
  {
    if (tattr.schedPriority != 0)
    {
      /* If caller doesn't set the class, he must not try to set the priority, which we
       approximate by expecting a 0. FIXME: should do this as part of config validation */
      DDS_ERROR("os_threadCreate(%s): schedClass DEFAULT but priority != 0 is unsupported\n", name);
      goto err;
    }
  }
  else
  {
    int policy;
    struct sched_param sched_param;
    if ((result = pthread_getschedparam (pthread_self (), &policy, &sched_param) != 0) != 0)
    {
      DDS_ERROR("os_threadCreate(%s): pthread_attr_getschedparam(self) failed with error %d\n", name, result);
      goto err;
    }
    switch (tattr.schedClass)
    {
      case OS_SCHED_DEFAULT:
        assert (0);
        break;
    case OS_SCHED_REALTIME:
        policy = SCHED_FIFO;
        break;
      case OS_SCHED_TIMESHARE:
        policy = SCHED_OTHER;
        break;
    }
    if ((result = pthread_attr_setschedpolicy (&attr, policy)) != 0)
    {
      DDS_ERROR("os_threadCreate(%s): pthread_attr_setschedpolicy(%d) failed with error %d\n", name, policy, result);
      goto err;
    }
    sched_param.sched_priority = tattr.schedPriority;
    if ((result = pthread_attr_setschedparam (&attr, &sched_param)) != 0)
    {
      DDS_ERROR("os_threadCreate(%s): pthread_attr_setschedparam(priority = %d) failed with error %d\n", name, tattr.schedPriority, result);
      goto err;
    }
    if ((result = pthread_attr_setinheritsched (&attr, PTHREAD_EXPLICIT_SCHED)) != 0)
    {
      DDS_ERROR("os_threadCreate(%s): pthread_attr_setinheritsched(EXPLICIT) failed with error %d\n", name, result);
      goto err;
    }
  }

  /* Construct context structure & start thread */
  threadContext = os_malloc (sizeof (os_threadContext));
  threadContext->threadName = os_malloc (strlen (name) + 1);
  strcpy (threadContext->threadName, name);
  threadContext->startRoutine = start_routine;
  threadContext->arguments = arg;
  if ((create_ret = pthread_create (&threadId->v, &attr, os_startRoutineWrapper, threadContext)) != 0)
  {
    DDS_ERROR ("os_threadCreate(%s): pthread_create failed with error %d\n", name, create_ret);
    goto err_create;
  }
  pthread_attr_destroy (&attr);
  return os_resultSuccess;

err_create:
  os_free (threadContext->threadName);
  os_free (threadContext);
err:
  pthread_attr_destroy (&attr);
  return os_resultFail;
}

/** \brief Return the integer representation of the given thread ID
 *
 * Possible Results:
 * - returns the integer representation of the given thread ID
 */
uintmax_t os_threadIdToInteger (os_threadId id)
{
  return (uintmax_t) ((uintptr_t) id.v);
}

/** \brief Return the thread ID of the calling thread
 *
 * \b os_threadIdSelf determines the own thread ID by
 * calling \b pthread_self.
 */
os_threadId os_threadIdSelf (void)
{
  os_threadId id = { .v = pthread_self () };
  return id;
}

int32_t os_threadGetThreadName (char *buffer, uint32_t length)
{
  char *name;
  if ((name = pthread_getspecific (os_threadNameKey)) == NULL)
    name = "";
  return snprintf (buffer, length, "%s", name);
}

/** \brief Wait for the termination of the identified thread
 *
 * \b os_threadWaitExit wait for the termination of the
 * thread \b threadId by calling \b pthread_join. The return
 * value of the thread is passed via \b thread_result.
 */
os_result os_threadWaitExit (os_threadId threadId, uint32_t *thread_result)
{
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
  if (pthread_getschedparam (threadId.v, &policy, &sched_param) == 0) {
    max = sched_get_priority_max (policy);
    if (max != -1) {
      (void) pthread_setschedprio (threadId.v, max);
    }
  }
#endif

  if ((result = pthread_join (threadId.v, &vthread_result)) != 0)
  {
    DDS_TRACE ("pthread_join(0x%"PRIxMAX") failed with error %d\n", os_threadIdToInteger (threadId), result);
    return os_resultFail;
  }

  if (thread_result)
    *thread_result = (uint32_t) ((uintptr_t) vthread_result);
  return os_resultSuccess;
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
void *os_threadMemMalloc (int32_t index, size_t size)
{
  void **pthreadMemArray;

  assert (0 <= index && index < OS_THREAD_MEM_ARRAY_SIZE);
  if ((pthreadMemArray = pthread_getspecific (os_threadMemKey)) == NULL)
  {
    os_threadMemInit ();
    pthreadMemArray = pthread_getspecific (os_threadMemKey);
  }
  assert (pthreadMemArray[index] == NULL);

  pthreadMemArray[index] = os_malloc (size);
  return pthreadMemArray[index];
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
void os_threadMemFree (int32_t index)
{
  assert (0 <= index && index < OS_THREAD_MEM_ARRAY_SIZE);
  void **pthreadMemArray = pthread_getspecific (os_threadMemKey);
  if (pthreadMemArray != NULL && pthreadMemArray[index] != NULL)
  {
    os_free (pthreadMemArray[index]);
    pthreadMemArray[index] = NULL;
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
void *os_threadMemGet (int32_t index)
{
  assert (0 <= index && index < OS_THREAD_MEM_ARRAY_SIZE);
  void **pthreadMemArray = pthread_getspecific (os_threadMemKey);
  return (pthreadMemArray != NULL) ? pthreadMemArray[index] : NULL;
}

static void os_threadCleanupFini (void *data)
{
  if (data == NULL)
    return;

  os_iter *itr = data;
  os_threadCleanup *obj;
  for (obj = os_iterTake (itr, -1); obj != NULL; obj = os_iterTake (itr, -1))
  {
    assert (obj->func != NULL);
    obj->func (obj->data);
    os_free (obj);
  }
  os_iterFree (itr, NULL);
}

static void os_threadCleanupInit (void)
{
  int ret;
  if ((ret = pthread_key_create (&cleanup_key, &os_threadCleanupFini)) != 0)
    DDS_FATAL ("os_threadCleanupInit: pthread_key_create failed with error %d\n", ret);
}

/* os_threadCleanupPush and os_threadCleanupPop are mapped onto a destructor
 registered with pthread_key_create in stead of being mapped directly onto
 pthread_cleanup_push/pthread_cleanup_pop because the cleanup routines could
 otherwise be popped of the stack by the user */
void os_threadCleanupPush (void (*func) (void *), void *data)
{
  os_iter *itr;
  os_threadCleanup *obj;

  assert (func != NULL);

  (void) pthread_once (&cleanup_once, &os_threadCleanupInit);
  if ((itr = pthread_getspecific (cleanup_key)) == NULL)
  {
    int ret;
    itr = os_iterNew ();
    assert (itr != NULL);
    if ((ret = pthread_setspecific (cleanup_key, itr)) != 0)
      DDS_FATAL ("os_threadCleanupPush: pthread_setspecific failed with error %d\n", ret);
  }

  obj = os_malloc (sizeof (*obj));
  obj->func = func;
  obj->data = data;
  os_iterAppend (itr, obj);
}

void os_threadCleanupPop (int execute)
{
  os_iter *itr;

  (void) pthread_once (&cleanup_once, &os_threadCleanupInit);
  if ((itr = pthread_getspecific (cleanup_key)) != NULL)
  {
    os_threadCleanup *obj;
    if ((obj = os_iterTake (itr, -1)) != NULL)
    {
      if (execute)
        obj->func(obj->data);
      os_free(obj);
    }
  }
}
