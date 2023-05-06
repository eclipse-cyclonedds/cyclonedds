// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_HOPSCOTCH_H
#define DDSRT_HOPSCOTCH_H

#include <stdint.h>

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** \file hopscotch.h
 * The hopscotch hash table is implemented in three versions:
 * - Standard version
 * - Concurrent version (for shared use by multiple threads)
 * - Embedded data version (stores a copy of the data)
 * 
 * With exception of the embedded data version, the hash table stores pointers to user data,
 * but doesn't manage the memory of the data itself.
 * 
 * Note that elements in a hash table are unordered. If an ordered set is desired,
 * then a different data structure such as a tree should be used.
 */

/**
 * @brief User defined hash function.
 * 
 * The hopscotch hash table is dependent on a proper functioning hash.
 * If the hash function generates a lot of hash collisions, then it will
 * not be able to handle that by design.
 * It is capable of handling some collisions, but not more than 32 per
 * bucket (less, when other hash values are clustered around the
 * collision value).
 * When proper distributed hash values are generated, then hopscotch
 * works nice and quickly.
 */
typedef uint32_t (*ddsrt_hh_hash_fn) (const void *a);

/**
 * @brief User defined equals function.
 * 
 * Hopscotch needs to be able to compare two elements to confirm it has found the correct one
 * (equal hashes doesn't guarantee that the keys are equal).
 * The input arguments are pointers to the user data objects to compare.
 * Returns int meant to be interpreted as bool (1 is match, 0 is no match)
 */
typedef int (*ddsrt_hh_equals_fn) (const void *a, const void *b);

/**
 * @brief User defined garbage collection function for buckets (only used for concurrent version).
 * 
 * Hopscotch will resize its internal buckets list when needed. It will
 * call this garbage collection function with the old buckets list. The
 * caller has to delete the list when it deems it safe to do so.
 */
typedef void (*ddsrt_hh_buckets_gc_fn) (void *bs, void *arg);

/* Sequential version */
/**
 * @brief The hopscotch hash table.
 * @see @ref ddsrt_hh_new
 * @see @ref ddsrt_hh_free
 */
struct ddsrt_hh;

/**
 * @brief Iter object for the iterator to store its progress and know where to go next.
 * @see @ref ddsrt_hh_iter_first
 */
struct ddsrt_hh_iter {
    struct ddsrt_hh *hh;
    uint32_t cursor;
};

/**
 * @brief Create a hopscotch hash table.
 * 
 * @param[in] init_size the minimum initial size
 * @param[in] hash the hash function, see @ref ddsrt_hh_hash_fn
 * @param[in] equals the equals function, see @ref ddsrt_hh_equals_fn
 * @return pointer to the hopscotch hash table
 * @see @ref ddsrt_hh_free
 */
DDS_EXPORT struct ddsrt_hh *ddsrt_hh_new (uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals) ddsrt_nonnull_all;

/**
 * @brief Destroy a hopscotch hash table.
 * 
 * It destroys the hash table and its buckets, but not the user data in the buckets.
 * The user data in the table must be deleted prior to calling this,
 * which can be done by using @ref ddsrt_hh_enum with a free function.
 * 
 * @param[in,out] hh the hash table to destroy
 * @see @ref ddsrt_hh_new
 */
DDS_EXPORT void ddsrt_hh_free (struct ddsrt_hh * __restrict hh) ddsrt_nonnull_all;

/**
 * @brief Lookup an element in the hash table.
 * 
 * Despite the name, the parameter 'keyobject' is a pointer to the whole user data object, not just the key,
 * so you need a dummy object to do the lookup. However, if the user data object is defined such that the key is its first element,
 * then it is also possible to do the lookup by making just a key object and passing a pointer to that key.
 * 
 * @param[in] rt the hash table
 * @param[in] keyobject is the object with which to do the lookup
 * @return pointer to the matching element, NULL if failed
 */
DDS_EXPORT void *ddsrt_hh_lookup (const struct ddsrt_hh * __restrict rt, const void * __restrict keyobject) ddsrt_nonnull_all;

/**
 * @brief Add an element to the hash table.
 * 
 * Attempting to add a duplicate to the hash table results in failure i.e. the element is not added,
 * and the function returns with failure.
 * 
 * @param[in,out] rt the hash table
 * @param[in] data user data to add
 * @return int meant to be interpreted as bool (1 is success, element added; 0 is failure, element not added)
 * @see @ref ddsrt_hh_remove
 */
DDS_EXPORT int ddsrt_hh_add (struct ddsrt_hh * __restrict rt, void * __restrict data) ddsrt_nonnull_all;

/**
 * @brief Remove an element from the hash table.
 * 
 * It only uses the key of the passed object and as such it will also work if you pass a dummy object.
 * However, you still need to free the memory of the object after removing it from the hash table.
 * Since this means you need to have the pointer to the actual object anyway, you might as well use that.
 * 
 * @param[in,out] rt the hash table
 * @param[in] keyobject user data to remove
 * @return int meant to be interpreted as bool (1 is success, 0 is failure)
 * @see @ref ddsrt_hh_add
 */
DDS_EXPORT int ddsrt_hh_remove (struct ddsrt_hh * __restrict rt, const void * __restrict keyobject) ddsrt_nonnull_all;

/**
 * @brief Like @ref ddsrt_hh_add, but without returning success/failure result.
 * 
 * Only use if you're certain that the element doesn't already exist.
 * 
 * @param[in,out] rt the hash table
 * @param[in] data user data to add
 * @see @ref ddsrt_hh_remove
 */
DDS_EXPORT void ddsrt_hh_add_absent (struct ddsrt_hh * __restrict rt, void * __restrict data) ddsrt_nonnull_all;

/**
 * @brief Like @ref ddsrt_hh_remove, but without returning success/failure result.
 * 
 * Only use if you're certain that the element exists.
 * 
 * @param[in,out] rt the hash table
 * @param[in] keyobject user data to remove
 * @see @ref ddsrt_hh_add_absent
 */
DDS_EXPORT void ddsrt_hh_remove_present (struct ddsrt_hh * __restrict rt, void * __restrict keyobject) ddsrt_nonnull_all;

/**
 * @brief Walk the hash table and apply a user defined function to each node.
 * 
 * The walk function 'f' receives in parameter 'a' the pointer to user data, and has 'f_arg' as an optional extra argument.
 * It is allowed to modify the user data. The @ref ddsrt_hh_enum is useful to free user data
 * (by passing a function 'f' that frees memory pointed to by 'a') prior to calling @ref ddsrt_hh_free.
 * 
 * @param[in,out] rt the hash table
 * @param[in] f user defined walk function to apply to each element
 * @param[in] f_arg extra argument for walk function
 * @see @ref ddsrt_hh_iter_next
 */
DDS_EXPORT void ddsrt_hh_enum (struct ddsrt_hh * __restrict rt, void (*f) (void *a, void *f_arg), void *f_arg) ddsrt_nonnull ((1, 2));

/**
 * @brief Initialize the iterator and get the first element.
 * 
 * @param[in] rt the hash table
 * @param[out] iter iterator object
 * @return pointer to first element
 * @see @ref ddsrt_hh_iter_next
 */
DDS_EXPORT void *ddsrt_hh_iter_first (struct ddsrt_hh * __restrict rt, struct ddsrt_hh_iter * __restrict iter) ddsrt_nonnull_all;

/**
 * @brief Use the iterator to get the next element
 * 
 * @param[in,out] iter iterator object
 * @return pointer to next element
 * @see @ref ddsrt_hh_iter_first
 * @see @ref ddsrt_hh_enum
 */
DDS_EXPORT void *ddsrt_hh_iter_next (struct ddsrt_hh_iter * __restrict iter) ddsrt_nonnull_all;

/* Concurrent version */
/**
 * @brief The concurrent hopscotch hash table.
 * @see @ref ddsrt_chh_new
 * @see @ref ddsrt_chh_free
 */
struct ddsrt_chh;
struct ddsrt_chh_bucket;

/**
 * @brief Embedded data version of @ref ddsrt_hh_iter.
 * @see @ref ddsrt_chh_iter_first
 */
#if ! ddsrt_has_feature_thread_sanitizer
struct ddsrt_chh_iter {
  struct ddsrt_chh_bucket *bs;
  uint32_t size;
  uint32_t cursor;
};
#else
struct ddsrt_chh_iter {
  struct ddsrt_chh *chh;
  struct ddsrt_hh_iter it;
};
#endif

/**
 * @brief Concurrent version of @ref ddsrt_hh_new
 * 
 * @param[in] init_size the minimum initial size
 * @param[in] hash the hash function, see @ref ddsrt_hh_hash_fn
 * @param[in] equals the equals function, see @ref ddsrt_hh_equals_fn
 * @param[in] gc_buckets user defined garbage collection function, see @ref ddsrt_hh_buckets_gc_fn
 * @param[in] gc_buckets_arg extra argument for @ref ddsrt_hh_buckets_gc_fn
 * @return pointer to the hopscotch hash table
 * @see @ref ddsrt_chh_free
 */
struct ddsrt_chh *ddsrt_chh_new (uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals, ddsrt_hh_buckets_gc_fn gc_buckets, void *gc_buckets_arg);

/**
 * @brief Concurrent version of @ref ddsrt_hh_free
 * 
 * @param[in,out] hh the hash table to destroy
 * @see @ref ddsrt_chh_new
 */
void ddsrt_chh_free (struct ddsrt_chh * __restrict hh);

/**
 * @brief Concurrent version of @ref ddsrt_hh_lookup
 * 
 * The lookup is lock-free and wait-free, and an element that exists in the hash table is guaranteed to be found.
 * Note that wait-free doesn't mean the operation is unimpeded by actions of other threads,
 * but rather it means that the operation will complete within a bounded number of steps.
 * 
 * It can work in parallel to @ref ddsrt_chh_remove,
 * but a successful lookup doesn't guarantee that the element hasn't been removed in the mean time.
 * 
 * @param[in] rt the hash table
 * @param[in] keyobject is the object with which to do the lookup
 * @return pointer to the matching element, NULL if failed
 */
void *ddsrt_chh_lookup (struct ddsrt_chh * __restrict rt, const void * __restrict keyobject);

/**
 * @brief Concurrent version of @ref ddsrt_hh_add
 * 
 * @param[in,out] rt the hash table
 * @param[in] data user data to add
 * @return int meant to be interpreted as bool (1 is success, 0 is failure)
 * @see @ref ddsrt_chh_remove
 */
int ddsrt_chh_add (struct ddsrt_chh * __restrict rt, void * __restrict data);

/**
 * @brief Concurrent version of @ref ddsrt_hh_remove
 * 
 * It can work in parallel to @ref ddsrt_chh_lookup, but in that case the looked up element may not be
 * part of the hash table anymore by the time the caller of @ref ddsrt_chh_lookup accesses the element.
 * 
 * @param[in,out] rt the hash table
 * @param[in] keyobject user data to remove
 * @return int meant to be interpreted as bool (1 is success, 0 is failure)
 * @see @ref ddsrt_chh_add
 */
int ddsrt_chh_remove (struct ddsrt_chh * __restrict rt, const void * __restrict keyobject);

/**
 * @brief Concurrent version of @ref ddsrt_hh_enum
 * 
 * Called unsafe because:
 * - if another thread is removing an element, then there is no guarantee that the element for which 'f' is invoked
 *   is still in the table at the time of invocation (similar to a @ref ddsrt_chh_remove and @ref ddsrt_chh_lookup)
 * - if another thread is adding an element, there is no guarantee whether that new element will, or will not, be visited
 * - if another thread is adding an element, there is no guarantee that an element won't be visited multiple times
 * 
 * @param[in,out] rt the hash table
 * @param[in] f user defined walk function to apply to each element
 * @param[in] f_arg extra argument for walk function
 * @see @ref ddsrt_chh_iter_next
 */
void ddsrt_chh_enum_unsafe (struct ddsrt_chh * __restrict rt, void (*f) (void *a, void *f_arg), void *f_arg); /* may delete a */

/**
 * @brief Concurrent version of @ref ddsrt_hh_iter_first
 * 
 * @param[in] rt the hash table
 * @param[out] it iterator object
 * @return pointer to first element
 * @see @ref ddsrt_chh_iter_next
 */
void *ddsrt_chh_iter_first (struct ddsrt_chh * __restrict rt, struct ddsrt_chh_iter *it);

/**
 * @brief Concurrent version of @ref ddsrt_hh_iter_next
 * 
 * @param[in,out] it iterator object
 * @return pointer to next element
 * @see @ref ddsrt_chh_iter_first
 * @see @ref ddsrt_chh_enum_unsafe
 */
void *ddsrt_chh_iter_next (struct ddsrt_chh_iter *it);


/* Sequential version, embedded data */

/**
 * @brief The embedded data hopscotch hash table.
 * @see @ref ddsrt_ehh_new
 * @see @ref ddsrt_ehh_free
 */
struct ddsrt_ehh;

/**
 * @brief Embedded data version of @ref ddsrt_hh_iter.
 * @see @ref ddsrt_ehh_iter_first
 */
struct ddsrt_ehh_iter {
    struct ddsrt_ehh *hh;
    uint32_t cursor;
};

/**
 * @brief Embedded data version of @ref ddsrt_hh_new.
 * 
 * Each element will use the same fixed size of embedded data. This means that for data that can vary in size (such as strings),
 * you'll have to use the worst case size and thus waste memory. It is also not recommended to use the embedded data version
 * with a large data size: It will hurt performance because each time the hash table is resized, all of the data is copied.
 * 
 * @param[in] elemsz size (in bytes) of the embedded data element
 * @param[in] init_size the minimum initial size
 * @param[in] hash the hash function, see @ref ddsrt_hh_hash_fn
 * @param[in] equals the equals function, see @ref ddsrt_hh_equals_fn
 * @return pointer to the hopscotch hash table
 * @see @ref ddsrt_ehh_free
 */
struct ddsrt_ehh *ddsrt_ehh_new (size_t elemsz, uint32_t init_size, ddsrt_hh_hash_fn hash, ddsrt_hh_equals_fn equals);

/**
 * @brief Embedded data version of @ref ddsrt_hh_free
 * 
 * Since the memory of the data is part of the bucket, it is also freed.
 * 
 * @param[in,out] hh the hash table to destroy
 * @see @ref ddsrt_ehh_new
 */
void ddsrt_ehh_free (struct ddsrt_ehh * __restrict hh);

/**
 * @brief Embedded data version of @ref ddsrt_hh_lookup
 * 
 * @param[in] rt the hash table
 * @param[in] keyobject is the object with which to do the lookup
 * @return pointer to the matching element, NULL if failed
 */
void *ddsrt_ehh_lookup (const struct ddsrt_ehh * __restrict rt, const void * __restrict keyobject);

/**
 * @brief Embedded data version of @ref ddsrt_hh_add
 * 
 * The data is copied into a bucket of the hash table.
 * 
 * @param[in,out] rt the hash table
 * @param[in] data user data to add
 * @return int meant to be interpreted as bool (1 is success, 0 is failure)
 * @see @ref ddsrt_ehh_remove
 */
int ddsrt_ehh_add (struct ddsrt_ehh * __restrict rt, const void * __restrict data);

/**
 * @brief Embedded data version of @ref ddsrt_hh_remove
 * 
 * There is no issue with memory management here. The bucket is simply tagged as not in use.
 * 
 * @param[in,out] rt the hash table
 * @param[in] keyobject user data to remove
 * @return int meant to be interpreted as bool (1 is success, 0 is failure)
 * @see @ref ddsrt_ehh_add
 */
int ddsrt_ehh_remove (struct ddsrt_ehh * __restrict rt, const void * __restrict keyobject);

/**
 * @brief Embedded data version of @ref ddsrt_hh_enum
 * 
 * @param[in,out] rt the hash table
 * @param[in] f user defined walk function to apply to each element
 * @param[in] f_arg extra argument for walk function
 * @see @ref ddsrt_ehh_iter_next
 */
void ddsrt_ehh_enum (struct ddsrt_ehh * __restrict rt, void (*f) (void *a, void *f_arg), void *f_arg); /* may delete a */

/**
 * @brief Embedded data version of @ref ddsrt_hh_iter_first
 * 
 * @param[in] rt the hash table
 * @param[out] iter iterator object
 * @return pointer to first element
 * @see @ref ddsrt_ehh_iter_next
 */
void *ddsrt_ehh_iter_first (struct ddsrt_ehh * __restrict rt, struct ddsrt_ehh_iter * __restrict iter); /* may delete nodes */

/**
 * @brief Embedded data version of @ref ddsrt_hh_iter_next
 * 
 * @param[in,out] iter iterator object
 * @return pointer to next element
 * @see @ref ddsrt_ehh_iter_first
 * @see @ref ddsrt_ehh_enum
 */
void *ddsrt_ehh_iter_next (struct ddsrt_ehh_iter * __restrict iter);

#if defined (__cplusplus)
}
#endif

#endif
