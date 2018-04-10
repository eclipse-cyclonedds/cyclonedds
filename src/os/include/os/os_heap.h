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
 * Interface definition for OS layer heap memory managment      *
 ****************************************************************/

/** \file os_heap.h
 *  \brief Heap memory management
 *
 * os_heap.h provides abstraction to heap memory management functions.
 */

#ifndef OS_HEAP_H
#define OS_HEAP_H

#if defined (__cplusplus)
extern "C" {
#endif

    /* !!!!!!!!NOTE From here no more includes are allowed!!!!!!! */

    /** \brief Allocate memory from heap
     *
     * Allocate memory from heap with the identified size. The returned pointer must be free'd with
     * os_free. If size is 0, os_malloc_s will still return a non-NULL pointer which must be free'd
     * with os_free.
     *
     * Possible Results:
     * - returns pointer to allocated memory, abort() if out of memory detected
     */
    _Check_return_ _Ret_bytecap_(size)
    OSAPI_EXPORT void * os_malloc(_In_ size_t size);

    /** \brief Allocate memory from heap
     *
     * Allocate memory from heap with the identified size. The returned pointer must be free'd with
     * os_free. If size is 0, os_malloc_s will still return a non-NULL pointer which must be free'd
     * with os_free.
     *
     * Possible Results:
     * - returns pointer to allocated memory, or null if out of memory
     */
    _Check_return_ _Ret_opt_bytecap_(size)
    OSAPI_EXPORT void * os_malloc_s(_In_ size_t size);

    /** \brief Allocate zeroed memory from heap
     *
     * Allocate memory from heap with the identified size and initialized to zero. The returned
     * pointer must be free'd with os_free. If size is 0, os_malloc_0_s will still return a non-NULL
     * pointer which must be free'd with os_free.
     *
     * Possible Results:
     * - returns pointer to allocated zeroed memory, abort() if out of memory detected
     */
    _Check_return_
    _Ret_bytecount_(size)
    OSAPI_EXPORT void *
    os_malloc_0(_In_ size_t size)
       __attribute_malloc__
       __attribute_alloc_size__((1));

    /** \brief Allocate zeroed memory from heap
     *
     * Allocate memory from heap with the identified size and initialized to zero. The returned
     * pointer must be free'd with os_free. If size is 0, os_malloc_0_s will still return a non-NULL
     * pointer which must be free'd with os_free.
     *
     * Possible Results:
     * - returns pointer to allocated zeroed memory, or null if out of memory
     */
    _Check_return_
    _Ret_opt_bytecount_(size)
    OSAPI_EXPORT void *
    os_malloc_0_s(_In_ size_t size)
       __attribute_malloc__
       __attribute_alloc_size__((1));

    /** \brief Allocate memory from heap for an array with count elements of size size
     *
     * The allocated memory is initialized to zero. The returned pointer must be free'd
     * with os_free. If size and/or count is 0, os_calloc will still return a non-NULL
     * pointer which must be free'd with os_free.
     *
     * Possible Results:
     * - returns pointer to allocated and zeroed memory, abort() if out of memory detected
     */
    _Check_return_
    _Ret_bytecount_(count * size)
    OSAPI_EXPORT void *
    os_calloc(_In_ size_t count,
              _In_ size_t size)
    __attribute_malloc__;

    /** \brief Allocate memory from heap for an array with count elements of size size
     *
     * The allocated memory is initialized to zero. The returned pointer must be free'd
     * with os_free. If size and/or count is 0, os_calloc_s will still return a non-NULL
     * pointer which must be free'd with os_free.
     *
     * Possible Results:
     * - returns pointer to allocated and zeroed memory, or null if out of memory
     */
    _Check_return_
    _Ret_opt_bytecount_(count * size)
    OSAPI_EXPORT void *
    os_calloc_s(_In_ size_t count,
                _In_ size_t size)
        __attribute_malloc__;

    /** \brief Reallocate memory from heap
     *
     * Reallocate memory from heap. If memblk is NULL the function returns os_malloc_s(size). If
     * size is 0, os_realloc_s free's the memory pointed to by memblk and returns a pointer as if
     * os_malloc_s(0) was invoked. The returned pointer must be free'd with os_free.
     * Possible Results:
     * - return pointer to reallocated memory, abort() if out of memory detected
     */
    _Check_return_
    _Ret_bytecap_(size)
    OSAPI_EXPORT void *
    os_realloc(
            _Pre_maybenull_ _Post_ptr_invalid_ void *memblk,
            _In_ size_t size)
        __attribute_malloc__
        __attribute_alloc_size__((1));

    /** \brief Reallocate memory from heap
     *
     * Reallocate memory from heap. If memblk is NULL the function returns os_malloc_s(size). If
     * size is 0, os_realloc_s free's the memory pointed to by memblk and returns a pointer as if
     * os_malloc_s(0) was invoked. The returned pointer must be free'd with os_free.
     * Possible Results:
     * - return pointer to reallocated memory, or null if out of memory .
     */
    _Success_(return != NULL)
    _Check_return_
    _Ret_opt_bytecap_(size)
    OSAPI_EXPORT void *
    os_realloc_s(
            _Pre_maybenull_ _Post_ptr_invalid_ void *memblk,
            _In_ size_t size)
        __attribute_malloc__
        __attribute_alloc_size__((1));

    /** \brief Free allocated memory and return it to heap
     *
     * Free the allocated memory pointed to by \b ptr
     * and release it to the heap. When \b ptr is NULL,
     * os_free will return without doing any action.
     */
    OSAPI_EXPORT void
    os_free(_Pre_maybenull_ _Post_ptr_invalid_ void *ptr);

#if defined (__cplusplus)
}
#endif

#endif /* OS_HEAP_H */
