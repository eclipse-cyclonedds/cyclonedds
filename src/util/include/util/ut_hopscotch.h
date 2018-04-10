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
#ifndef UT_HOPSCOTCH_H
#define UT_HOPSCOTCH_H

#include "os/os.h"
#include "util/ut_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

#if __STDC_VERSION__ >= 199901L
#define UT_HH_RESTRICT restrict
#else
#define UT_HH_RESTRICT
#endif

/* Concurrent version */
struct ut_chh;
struct ut_chhBucket;
struct ut_chhIter {
  struct ut_chhBucket *bs;
  uint32_t size;
  uint32_t cursor;
};

/*
 * The hopscotch hash table is dependent on a proper functioning hash.
 * If the hash function generates a lot of hash collisions, then it will
 * not be able to handle that by design.
 * It is capable of handling some collisions, but not more than 32 per
 * bucket (less, when other hash values are clustered around the
 * collision value).
 * When proper distributed hash values are generated, then hopscotch
 * works nice and quickly.
 */
typedef uint32_t (*ut_hhHash_fn) (const void *);

/*
 * Hopscotch needs to be able to compare two elements.
 * Returns 0 when not equal.
 */
typedef int (*ut_hhEquals_fn) (const void *, const void *);

/*
 * Hopscotch is will resize its internal buckets list when needed. It will
 * call this garbage collection function with the old buckets list. The
 * caller has to delete the list when it deems it safe to do so.
 */
typedef void (*ut_hhBucketsGc_fn) (void *);

UTIL_EXPORT struct ut_chh *ut_chhNew (uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals, ut_hhBucketsGc_fn gc_buckets);
UTIL_EXPORT void ut_chhFree (struct ut_chh * UT_HH_RESTRICT hh);
UTIL_EXPORT void *ut_chhLookup (struct ut_chh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template);
UTIL_EXPORT int ut_chhAdd (struct ut_chh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT data);
UTIL_EXPORT int ut_chhRemove (struct ut_chh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template);
UTIL_EXPORT void ut_chhEnumUnsafe (struct ut_chh * UT_HH_RESTRICT rt, void (*f) (void *a, void *f_arg), void *f_arg); /* may delete a */
void *ut_chhIterFirst (struct ut_chh * UT_HH_RESTRICT rt, struct ut_chhIter *it);
void *ut_chhIterNext (struct ut_chhIter *it);

/* Sequential version */
struct ut_hh;

struct ut_hhIter {
    struct ut_hh *hh;
    uint32_t cursor;
};

UTIL_EXPORT struct ut_hh *ut_hhNew (uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals);
UTIL_EXPORT void ut_hhFree (struct ut_hh * UT_HH_RESTRICT hh);
UTIL_EXPORT void *ut_hhLookup (const struct ut_hh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template);
UTIL_EXPORT int ut_hhAdd (struct ut_hh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT data);
UTIL_EXPORT int ut_hhRemove (struct ut_hh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template);
UTIL_EXPORT void ut_hhEnum (struct ut_hh * UT_HH_RESTRICT rt, void (*f) (void *a, void *f_arg), void *f_arg); /* may delete a */
UTIL_EXPORT void *ut_hhIterFirst (struct ut_hh * UT_HH_RESTRICT rt, struct ut_hhIter * UT_HH_RESTRICT iter); /* may delete nodes */
UTIL_EXPORT void *ut_hhIterNext (struct ut_hhIter * UT_HH_RESTRICT iter);

/* Sequential version, embedded data */
struct ut_ehh;

struct ut_ehhIter {
    struct ut_ehh *hh;
    uint32_t cursor;
};

UTIL_EXPORT struct ut_ehh *ut_ehhNew (size_t elemsz, uint32_t init_size, ut_hhHash_fn hash, ut_hhEquals_fn equals);
UTIL_EXPORT void ut_ehhFree (struct ut_ehh * UT_HH_RESTRICT hh);
UTIL_EXPORT void *ut_ehhLookup (const struct ut_ehh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template);
UTIL_EXPORT int ut_ehhAdd (struct ut_ehh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT data);
UTIL_EXPORT int ut_ehhRemove (struct ut_ehh * UT_HH_RESTRICT rt, const void * UT_HH_RESTRICT template);
UTIL_EXPORT void ut_ehhEnum (struct ut_ehh * UT_HH_RESTRICT rt, void (*f) (void *a, void *f_arg), void *f_arg); /* may delete a */
UTIL_EXPORT void *ut_ehhIterFirst (struct ut_ehh * UT_HH_RESTRICT rt, struct ut_ehhIter * UT_HH_RESTRICT iter); /* may delete nodes */
UTIL_EXPORT void *ut_ehhIterNext (struct ut_ehhIter * UT_HH_RESTRICT iter);

#if defined (__cplusplus)
}
#endif

#endif
