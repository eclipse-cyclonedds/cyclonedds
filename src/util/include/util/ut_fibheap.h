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
#ifndef UT_FIBHEAP_H
#define UT_FIBHEAP_H

#include "os/os.h"
#include "util/ut_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct ut_fibheapNode {
  struct ut_fibheapNode *parent, *children;
  struct ut_fibheapNode *prev, *next;
  unsigned mark: 1;
  unsigned degree: 31;
} ut_fibheapNode_t;

typedef struct ut_fibheapDef {
    uintptr_t offset;
    int (*cmp) (const void *va, const void *vb);
} ut_fibheapDef_t;

typedef struct ut_fibheap {
  ut_fibheapNode_t *roots; /* points to root with min key value */
} ut_fibheap_t;

#define UT_FIBHEAPDEF_INITIALIZER(offset, cmp) { (offset), (cmp) }

UTIL_EXPORT void ut_fibheapDefInit (ut_fibheapDef_t *fhdef, uintptr_t offset, int (*cmp) (const void *va, const void *vb));
UTIL_EXPORT void ut_fibheapInit (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh);
UTIL_EXPORT void *ut_fibheapMin (const ut_fibheapDef_t *fhdef, const ut_fibheap_t *fh);
UTIL_EXPORT void ut_fibheapMerge (const ut_fibheapDef_t *fhdef, ut_fibheap_t *a, ut_fibheap_t *b);
UTIL_EXPORT void ut_fibheapInsert (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh, const void *vnode);
UTIL_EXPORT void ut_fibheapDelete (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh, const void *vnode);
UTIL_EXPORT void *ut_fibheapExtractMin (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh);
UTIL_EXPORT void ut_fibheapDecreaseKey (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh, const void *vnode); /* to be called AFTER decreasing the key */

#if defined (__cplusplus)
}
#endif

#endif /* UT_FIBHEAP_H */
