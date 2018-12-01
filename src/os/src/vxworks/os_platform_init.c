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
/****************************************************************
 * Initialization / Deinitialization                            *
 ****************************************************************/

/** \file os/vxworks/os_platform_init.c
 *  \brief Initialization / Deinitialization
 */

#include <assert.h>

#include "os/os.h"


#if defined(OS_USE_ALLIGNED_MALLOC) && !defined(NDEBUG)
#include "os/os_atomics.h"
extern atomic_t os__reallocdoublecopycount ;
#endif

/** \brief Counter that keeps track of number of times os-layer is initialized */
static os_atomic_uint32_t _ospl_osInitCount = OS_ATOMIC_UINT32_INIT(0);

/* Check expression is true at compile time ( no code generated )
     - will get negative sized array on failure */
#define EXPRCHECK(expr) (void)sizeof(char[1-(signed int)(2*(unsigned int)!(expr))]);


/** \brief OS layer initialization
 *
 * \b os_osInit calls:
 * - \b os_sharedMemoryInit
 * - \b os_threadInit
 */
void os_osInit (void)
{
    uint32_t initCount;

    /* Sanity checks, don't remove, they don't generate any code anyway. */
    EXPRCHECK(sizeof(char)==1)
    EXPRCHECK(sizeof(unsigned char)==1)
    EXPRCHECK(sizeof(short)==2)
    EXPRCHECK(sizeof(unsigned short)==2)
    EXPRCHECK(sizeof(int32_t)==4)
    EXPRCHECK(sizeof(uint32_t)==4)
    EXPRCHECK(sizeof(int64_t)==8)
    EXPRCHECK(sizeof(uint64_t)==8)
    EXPRCHECK(sizeof(float)==4)
    EXPRCHECK(sizeof(double)==8)

#ifdef OS_USE_ALLIGNED_MALLOC
    /* Check for heap realignment code which needs types below being 32bit */
    EXPRCHECK(sizeof(size_t)==4)
    EXPRCHECK(sizeof(void *)==4)
#endif

    initCount = os_atomic_inc32_nv(&_ospl_osInitCount);

    if (initCount == 1) {
        /* init for programs using data base threads */
        os_threadModuleInit();
        os_reportInit(false);
#ifdef _WRS_KERNEL
        os_stdlibInitialize();
#endif
    }

    return;
}

/** \brief OS layer deinitialization
 */
void os_osExit (void)
{
    uint32_t initCount;

#if defined(OS_USE_ALLIGNED_MALLOC) && !defined(NDEBUG)
    DDS_INFO("count=%d\n", vxAtomicGet(&os__reallocdoublecopycount));
#endif
    initCount = os_atomic_dec32_nv(&_ospl_osInitCount);

    if (initCount == 0) {
        os_reportExit();
        os_threadModuleExit();
        os_syncModuleExit();
    } else if ((initCount + 1) < initCount){
        /* The 0 boundary is passed, so os_osExit is called more often than
         * os_osInit. Therefore undo decrement as nothing happened and warn. */
        initCount = os_atomic_inc32_nv(&_ospl_osInitCount);
        DDS_WARNING("OS-layer not initialized\n");
        /* Fail in case of DEV, as it is incorrect API usage */
        assert(0);
    }
    return;
}

#ifdef _WRS_KERNEL
/* This constructor is invoked when the library is loaded into a process. */
void __attribute__ ((constructor))
os__osInit(
    void)
{
    os_osInit();
}

/* This destructor is invoked when the library is unloaded from a process. */
void __attribute__ ((destructor))
os__osExit(
    void)
{
    os_osExit();
}
#else
/* This constructor is invoked when the library is loaded into a process. */
_WRS_CONSTRUCTOR(os__osInit, 100) {
    os_osInit();
    atexit(&os_osExit); /* There is no _WRS_DESTRUCTOR, use atexit instead */
    return;
}
#endif
