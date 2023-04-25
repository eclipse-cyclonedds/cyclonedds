// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>

/** \file os/vxworks/os_platform_heap.c
 *  \brief VxWorks heap memory management
 *
 * Implements heap memory management for VxWorks
 * by including the common implementation
 */
#if defined(OS_USE_ALLIGNED_MALLOC)
#ifndef NDEBUG
#include "os/os.h"
atomic_t os__reallocdoublecopycount = 0;
#endif

void *
ddsrt_malloc(
    size_t size)
{
    void *ptr;
    void *origptr;

    origptr = malloc(size+12);
    ptr=origptr;
    if (!ptr)
    {
        return NULL;
    }
    assert ( ((char *)ptr - (char *)0) % 4 == 0 );

    if ( ((char *)ptr - (char *)0) % 8 != 0 )
    {
        /* malloc returned memory not 8 byte aligned */
        /* move pointer by 4 so that it will be aligned again */
        ptr=((uint32_t *)ptr) + 1;
    }

    /* Store requested size */
    /* 8 bytes before the pointer we return */
    *(((size_t *)ptr)) = size;
    ptr=((size_t *)ptr) + 1;

    /* Store pointer to result of malloc "before" the allocation */
    /* 4 bytes before the pointer we return */
    *((void **)ptr)= origptr;
    ptr=((uint32_t *)ptr) + 1;

    assert ( ((char *)ptr - (char *)0) % 8 == 0 );
    return ptr;
}

void *ddsrt_realloc(
    void *ptr,
    size_t size)
{
    void *newptr;  /* Address returned from system realloc */
    void *origptr; /* Address returned by previous *alloc */
    void *olddata; /* Address of original user data immediately after realloc. */
    void *newdata; /* Address user data will be at on return. */
    size_t origsize; /* Size before realloc */

    if ( ptr == NULL )
    {
        return (ddsrt_malloc(size));
    }

    assert ( ((char *)ptr - (char *)0) % 8 == 0 );

    origptr = *(((void **)ptr)-1);
    if ( size == 0 )
    {
        /* really a free */
        realloc(origptr, size);
        return NULL;
    }

    origsize = *(((size_t *)ptr)-2);
    newptr = realloc(origptr, size+12);
    if ( newptr == NULL )
    {
        /* realloc failed, everything is left untouched */
        return NULL;
    }
    olddata = (char *)newptr + ((char *)ptr - (char *)origptr);

    assert ( ((char *)newptr - (char *)0) % 4 == 0 );

    if ( ((char *)newptr - (char *)0) % 8 == 0 )
    {
        /* Allow space for size and pointer */
        newdata = ((uint32_t *)newptr)+2;
    }
    else
    {
        /* malloc returned memory not 8 byte aligned */
        /* realign, and Allow space for size and pointer */
        newdata = ((uint32_t *)newptr)+3;
    }

    assert ( ((char *)newdata - (char *)0) % 8 == 0 );

    if ( (((char *)newptr - (char *)0) % 8) != (((char *)origptr - (char *)0) % 8) )
    {
        /* realloc returned memory with different alignment */
        assert (  ((char *)newdata)+4 == ((char *)olddata)
             ||((char *)olddata)+4 == ((char *)newdata));
#ifndef NDEBUG
        vxAtomicInc( &os__reallocdoublecopycount);
#endif
        memmove(newdata, olddata, origsize < size ? origsize : size);
    }

    /* Store requested size */
    /* 8 bytes before the pointer we return */
    *(((size_t *)newdata)-2) = size;

    /* Store pointer to result of realloc "before" the allocation */
    /* 4 bytes before the pointer we return */
    *(((void **)newdata)-1) = newptr;

    return newdata;
}

void
ddsrt_free(
    void *ptr)
{
    assert ( ((char *)ptr - (char *)0) % 8 == 0 );
    free(*(((void **)ptr)-1));
}
#else
/* For 64bit use the native ops align to 8 bytes */
#include "../snippets/code/os_heap.c"

#endif /* OS_USE_ALLIGNED_MALLOC */

