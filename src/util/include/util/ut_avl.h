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
#ifndef UT_AVL_H
#define UT_AVL_H

/* The tree library never performs memory allocations or deallocations internally.

   - Treedef_t: defines the properties of the tree, offsets,
     comparison functions, augmented structures, flags -- these are
     related to the code/data structure in which the tree is embedded,
     and in nearly all cases known at compile time.
   - avlTree_t: represents the tree, i.e., pointer to the root.
   - avlNode_t: contains the administrative data for a single node in
     the tree.

   For a tree node:
     struct T {
       avlNode_t avlnode;
       int key;
     };
   by definition, avlnodeoffset == offsetof(struct T, avlnode) and
   keyoffset = offsetof(struct T, key). The user of the library only
   ever deals in pointers to (in this case) struct T, never with
   pointers to the avlNode_t, and the compare function operations on
   pointers to keys, in this case pointers to "int"s. If you wish, you
   can also do: keyoffset = 0, in which case the compare function
   would be operating on struct T's.

   The compare function is assumed to behave just like all compare
   functions in the C library: < 0, =0, >0 for left argument less
   than, equal to or greater than the right argument.

   The "augment" function is automatically called whenever some of the
   children of a node change, as well as when the "augment" function
   has been called on some of the children. It allows you to maintain
   a "summary" of the subtree -- currently only used in ddsi2e, in one
   spot.

   Trees come in various "variants", configured through "treedef"
   flags:
   - direct/indirect key: direct meaning the key value is embedded in
     the structure containing the avlNode_t, indirect meaning a
     pointer to the key value is. The compare function doesn't deal
     with tree nodes, but with key values.
   - re-entrant: in the style of the C library, meaning, the
     comparison function gets a user-supplied 3rd argument (in
     particular used by mmstat).
   - unique keys/duplicate keys: when keys must be unique, some
     optimizations apply; it is up to the caller to ensure one doesn't
     violate the uniqueness of the keys (it'll happily crash in insert
     if you don't); when duplicate keys are allowed, a forward scan of
     the tree will visit them in the order of insertion.

   For a tree node:
     struct T {
       avlnode_t avlnode;
      char *key;
     };
   you could set the "indirect" flag, and then you simply use
   strcmp(), avoiding the need for passing templates in looking up key
   values. Much nicer.

   There is also an orthogonal variant that is enforced through the
   type system -- note that would be possible for all of the above as
   well, but the number of cases simply explodes and none of the above
   flags affects the dynamically changing data structures (just the
   tree definition), unlike this one.

   - the "C" variant keeps track of the number of nodes in the tree to
     support a "count" operation in O(1) time, but is otherwise
     identical.

   The various initializer macros and TreedefInit functions should
   make sense with this.

   All functions for looking up nodes return NULL if there is no node
   satisfying the requirements.

   - Init: initializes a tree (really just: root = NULL, perhaps count = 0)
   - Free: calls "freefun" on each node, which may free the node
   - FreeArg: as "Free", but with an extra, user-supplied, argument
   - Root: returns the root node
   - Lookup: returns a node with key value "key" (ref allowdups flag)
   - LookupIPath: like Lookup, but also filling an IPath_t structure
     for efficient insertion in case of a failed lookup (or inserting
     duplicates)
   - LookupDPath: like Lookup, but also filling a DPath_t structure
     that helps with deleting a node
   - LookupPredEq: locates the node with the greatest key value <= "key"
   - LookupSuccEq: similar, but smallest key value >= "key"
   - LookupPred: similar, < "key"
   - LookupSucc: similar, > "key"
   - Insert: convenience function: LookupIPath ; InsertIPath
   - Delete: convenience function: LookupDPath ; DeleteDPath
   - InsertIPath: insert node based on the "path" obtained from LookupIPath
   - DeleteDPath: delete node, using information in "path" to do so efficiently
   - SwapNode: replace "oldn" by "newn" without modifying the tree
     structure (the key need not be equal, but must be
     FindPred(oldn).key < newn.key < FindSucc(oldn).key, where a
     non-existing predecessor has key -inf and a non-existing
     successor has key +inf, and where it is understood that the <
     operator becomes <= if allowdups is set
   - AugmentUpdate: to be called when something in "node" changes that
     affects the subtree "summary" computed by the configured
     "augment" function
   - IsEmpty: returns 1 if tree is empty, 0 if not
   - IsSingleton: returns 1 if tree contains exactly one node, 0 if not
   - FindMin: returns the node with the smallest key value in the tree
   - FindMax: similar, largest key value
   - FindPred: preceding node in in-order treewalk
   - FindSucc: similar, following node

   - Walk: calls "f" with user-supplied argument "a" once for each
     node, starting at FindMin and ending at FindMax
   - ConstWalk: same, but with a const tree
   - WalkRange: like Walk, but only visiting nodes with key values in
     range [min,max] (that's inclusive)
   - ConstWalkRange: same, but with a const tree
   - WalkRangeReverse: like WalkRange, but in the reverse direction
   - ConstWalkRangeReverse: same, but with a const tree
   - IterFirst: starts forward iteration, starting at (and returning) FindMin
   - IterSuccEq: similar, starting at LookupSuccEq
   - IterSucc: similar, starting at LookupSucc
   - IterNext: returns FindSucc(last returned node); may not be called
     if preceding IterXXX call on same "iter" returned NULL

   That's all there is to it.

   Note that all calls to Walk(f,a) can be rewritten as:
     for(n=IterFirst(&it); n; n=IterNext(&it)) { f(n,a) }
   or as
     for(n=FindMin(); n; n=FindSucc(n)) { f(n,a) }

   The walk functions and iterators may not alter the tree
   structure. If that is desired, the latter can easily be rewritten
   as:
     n=FindMin() ; while(n) { nn=FindSucc(n); f(n,a); n=nn }
   because FindMin/FindSucc doesn't store any information to allow
   fast processing. That'll allow every operation, with the obvious
   exception of f(n) calling Delete(FindSucc(n)).

   Currently, all trees maintain parent pointers, but it may be worth
   doing a separate set without it, as it reduces the size of
   avlNode_t. But in that case, the FindMin/FindSucc option would no
   longer be a reasonable option because it would be prohibitively
   expensive, whereas the IterFirst/IterNext option are alway
   efficiently. If one were to do a threaded tree variant, the
   implemetantion of IterFirst/IterNext would become absolute trivial
   and faster still, but at the cost of significantly more overhead in
   memory and updates. */

#include "os/os.h"
#include "util/ut_export.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define UT_AVL_MAX_TREEHEIGHT (12 * sizeof (void *))

typedef int (*ut_avlCompare_t) (_In_ const void *a, _In_ const void *b);
typedef int (*ut_avlCompare_r_t) (_In_ const void *a, _In_ const void *b, _In_opt_ void *arg);
typedef void (*ut_avlAugment_t) (_Inout_ void *node, _In_opt_ const void *left, _In_opt_ const void *right);
typedef void (*ut_avlWalk_t) (_Inout_ void *node, _Inout_opt_ void *arg);
typedef void (*ut_avlConstWalk_t) (_In_ const void *node, _Inout_opt_ void *arg);

typedef struct ut_avlNode {
    struct ut_avlNode *cs[2]; /* 0 = left, 1 = right */
    struct ut_avlNode *parent;
    int height;
} ut_avlNode_t;

#define UT_AVL_TREEDEF_FLAG_INDKEY 1
#define UT_AVL_TREEDEF_FLAG_R 2
#define UT_AVL_TREEDEF_FLAG_ALLOWDUPS 4

typedef struct ut_avlTreedef {
#if defined (__cplusplus)
    ut_avlTreedef() {}
#endif
    size_t avlnodeoffset;
    size_t keyoffset;
    union {
        ut_avlCompare_t comparekk;
        ut_avlCompare_r_t comparekk_r;
    } u;
    ut_avlAugment_t augment;
    uint32_t flags;
    void *cmp_arg; /* for _r variant */
} ut_avlTreedef_t;

typedef struct ut_avlCTreedef {
    ut_avlTreedef_t t;
} ut_avlCTreedef_t;

typedef struct ut_avlTree {
    ut_avlNode_t *root;
} ut_avlTree_t;

typedef struct ut_avlCTree {
    ut_avlTree_t t;
    size_t count;
} ut_avlCTree_t;

typedef struct ut_avlPath {
    int depth; /* total depth of path */
    int pnodeidx;
    ut_avlNode_t *parent; /* (nodeidx == 0 ? NULL : *(path[nodeidx-1])) */
    ut_avlNode_t **pnode[UT_AVL_MAX_TREEHEIGHT];
} ut_avlPath_t;

typedef struct ut_avlIPath {
    ut_avlPath_t p;
} ut_avlIPath_t;

typedef struct ut_avlDPath {
    ut_avlPath_t p;
} ut_avlDPath_t;

typedef struct ut_avlIter {
    const ut_avlTreedef_t *td;
    ut_avlNode_t *right;
    ut_avlNode_t **todop;
    ut_avlNode_t *todo[1+UT_AVL_MAX_TREEHEIGHT];
} ut_avlIter_t;

typedef struct ut_avlCIter {
    ut_avlIter_t t;
} ut_avlCIter_t;

/* avlnodeoffset and keyoffset must both be in [0,2**31-1] */
#define UT_AVL_TREEDEF_INITIALIZER(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), 0, 0 }
#define UT_AVL_TREEDEF_INITIALIZER_INDKEY(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_INDKEY, 0 }
#define UT_AVL_TREEDEF_INITIALIZER_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_ALLOWDUPS, 0 }
#define UT_AVL_TREEDEF_INITIALIZER_INDKEY_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_INDKEY|UT_AVL_TREEDEF_FLAG_ALLOWDUPS, 0 }
#define UT_AVL_TREEDEF_INITIALIZER_R(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_R, (cmparg) }
#define UT_AVL_TREEDEF_INITIALIZER_INDKEY_R(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_INDKEY|UT_AVL_TREEDEF_FLAG_R, (cmparg) }
#define UT_AVL_TREEDEF_INITIALIZER_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_R|UT_AVL_TREEDEF_FLAG_ALLOWDUPS, (cmparg) }
#define UT_AVL_TREEDEF_INITIALIZER_INDKEY_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), UT_AVL_TREEDEF_FLAG_INDKEY|UT_AVL_TREEDEF_FLAG_R|UT_AVL_TREEDEF_FLAG_ALLOWDUPS, (cmparg) }

/* Not maintaining # nodes */

UTIL_EXPORT void ut_avlTreedefInit (_Out_ ut_avlTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_t comparekk, _In_opt_ ut_avlAugment_t augment, uint32_t flags) __nonnull((1,4));
UTIL_EXPORT void ut_avlTreedefInit_r (_Out_ ut_avlTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_r_t comparekk_r, _Inout_opt_ void *cmp_arg, ut_avlAugment_t augment, uint32_t flags) __nonnull((1,4));

UTIL_EXPORT void ut_avlInit (_In_ const ut_avlTreedef_t *td, _Out_ ut_avlTree_t *tree) __nonnull_all__;
UTIL_EXPORT void ut_avlFree (_In_ const ut_avlTreedef_t *td, _Inout_ _Post_invalid_ ut_avlTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node)) __nonnull((1,2));
UTIL_EXPORT void ut_avlFreeArg (_In_ const ut_avlTreedef_t *td, _Inout_ _Post_invalid_ ut_avlTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node, _Inout_opt_ void *arg), _Inout_opt_ void *arg) __nonnull((1,2));

_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlRoot (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree) __nonnull_all__;
_Ret_notnull_ UTIL_EXPORT void *ut_avlRootNonEmpty (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookup (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key) __nonnull_all__;
               _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookupIPath (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key, _Out_ ut_avlIPath_t *path) __nonnull_all__;
               _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookupDPath (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key, _Out_ ut_avlDPath_t *path) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookupPredEq (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookupSuccEq (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookupPred (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlLookupSucc (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key) __nonnull_all__;

UTIL_EXPORT void ut_avlInsert (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *node) __nonnull_all__;
UTIL_EXPORT void ut_avlDelete (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *node) __nonnull_all__;
UTIL_EXPORT void ut_avlInsertIPath (_In_ const ut_avlTreedef_t *td, ut_avlTree_t *tree, _Inout_ void *node, _Inout_ _Post_invalid_ ut_avlIPath_t *path) __nonnull_all__;
UTIL_EXPORT void ut_avlDeleteDPath (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *node, _Inout_ _Post_invalid_ ut_avlDPath_t *path) __nonnull_all__;
UTIL_EXPORT void ut_avlSwapNode (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *oldn, _Inout_ void *newn) __nonnull_all__;
UTIL_EXPORT void ut_avlAugmentUpdate (_In_ const ut_avlTreedef_t *td, _Inout_ void *node) __nonnull_all__;

UTIL_EXPORT int ut_avlIsEmpty (_In_ const ut_avlTree_t *tree) __nonnull_all__;
UTIL_EXPORT int ut_avlIsSingleton (_In_ const ut_avlTree_t *tree) __nonnull_all__;

_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlFindMin (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlFindMax (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlFindPred (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_opt_ const void *vnode) __nonnull((1,2));
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlFindSucc (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_opt_ const void *vnode) __nonnull((1,2));

UTIL_EXPORT void ut_avlWalk (_In_ const ut_avlTreedef_t *td, _In_ ut_avlTree_t *tree, _In_ ut_avlWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3));
UTIL_EXPORT void ut_avlConstWalk (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3));
UTIL_EXPORT void ut_avlWalkRange (_In_ const ut_avlTreedef_t *td, _In_ ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)  __nonnull((1,2,3,4,5));
UTIL_EXPORT void ut_avlConstWalkRange (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3,4,5));
UTIL_EXPORT void ut_avlWalkRangeReverse (_In_ const ut_avlTreedef_t *td, ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3));
UTIL_EXPORT void ut_avlConstWalkRangeReverse (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3));

_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlIterFirst (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlIterSuccEq (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlIterSucc (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlIterNext (_Inout_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter) __nonnull_all__;

/* Maintaining # nodes */

#define UT_AVL_CTREEDEF_INITIALIZER(avlnodeoffset, keyoffset, comparekk, augment) { UT_AVL_TREEDEF_INITIALIZER (avlnodeoffset, keyoffset, comparekk, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_INDKEY(avlnodeoffset, keyoffset, comparekk, augment) { UT_AVL_TREEDEF_INITIALIZER_INDKEY (avlnodeoffset, keyoffset, comparekk, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, augment) { UT_AVL_TREEDEF_INITIALIZER_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_INDKEY_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, augment) { UT_AVL_TREEDEF_INITIALIZER_INDKEY_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_R(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { UT_AVL_TREEDEF_INITIALIZER_R (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_INDKEY_R(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { UT_AVL_TREEDEF_INITIALIZER_INDKEY_R (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { UT_AVL_TREEDEF_INITIALIZER_R_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }
#define UT_AVL_CTREEDEF_INITIALIZER_INDKEY_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { UT_AVL_TREEDEF_INITIALIZER_INDKEY_R_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }

UTIL_EXPORT void ut_avlCTreedefInit (_Out_ ut_avlCTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_t comparekk, _In_opt_ ut_avlAugment_t augment, uint32_t flags) __nonnull((1,4));
UTIL_EXPORT void ut_avlCTreedefInit_r (_Out_ ut_avlCTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_r_t comparekk_r, _Inout_opt_ void *cmp_arg, _In_opt_ ut_avlAugment_t augment, uint32_t flags) __nonnull((1,4));

UTIL_EXPORT void ut_avlCInit (_In_ const ut_avlCTreedef_t *td, _Out_ ut_avlCTree_t *tree) __nonnull_all__;
UTIL_EXPORT void ut_avlCFree (_In_ const ut_avlCTreedef_t *td, _Inout_ _Post_invalid_ ut_avlCTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node)) __nonnull((1,2));
UTIL_EXPORT void ut_avlCFreeArg (_In_ const ut_avlCTreedef_t *td, _Inout_ _Post_invalid_ ut_avlCTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node, _Inout_opt_ void *arg), _Inout_opt_ void *arg) __nonnull((1,2));

_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCRoot (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree) __nonnull_all__;
_Ret_notnull_ UTIL_EXPORT void *ut_avlCRootNonEmpty (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookup (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key) __nonnull_all__;
               _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookupIPath (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key, _Out_ ut_avlIPath_t *path) __nonnull_all__;
               _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookupDPath (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key, _Out_ ut_avlDPath_t *path) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookupPredEq (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookupSuccEq (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookupPred (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCLookupSucc (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key) __nonnull_all__;

UTIL_EXPORT void ut_avlCInsert (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node) __nonnull_all__;
UTIL_EXPORT void ut_avlCDelete (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node) __nonnull_all__;
UTIL_EXPORT void ut_avlCInsertIPath (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node, _Inout_ _Post_invalid_ ut_avlIPath_t *path) __nonnull_all__;
UTIL_EXPORT void ut_avlCDeleteDPath (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node, _Inout_ _Post_invalid_ ut_avlDPath_t *path) __nonnull_all__;
UTIL_EXPORT void ut_avlCSwapNode (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *oldn, _Inout_ void *newn) __nonnull_all__;
UTIL_EXPORT void ut_avlCAugmentUpdate (_In_ const ut_avlCTreedef_t *td, _Inout_ void *node) __nonnull_all__;

UTIL_EXPORT int ut_avlCIsEmpty (_In_ const ut_avlCTree_t *tree) __nonnull_all__;
UTIL_EXPORT int ut_avlCIsSingleton (_In_ const ut_avlCTree_t *tree) __nonnull_all__;
UTIL_EXPORT size_t ut_avlCCount (_In_ const ut_avlCTree_t *tree) __nonnull_all__;

_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCFindMin (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCFindMax (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCFindPred (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *vnode) __nonnull((1,2));
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCFindSucc (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *vnode) __nonnull((1,2));

UTIL_EXPORT void ut_avlCWalk (_In_ const ut_avlCTreedef_t *td, _In_ ut_avlCTree_t *tree, _In_ ut_avlWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3));
UTIL_EXPORT void ut_avlCConstWalk (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3));
UTIL_EXPORT void ut_avlCWalkRange (_In_ const ut_avlCTreedef_t *td, _In_ ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3,4,5));
UTIL_EXPORT void ut_avlCConstWalkRange (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3,4,5));
UTIL_EXPORT void ut_avlCWalkRangeReverse (_In_ const ut_avlCTreedef_t *td, _In_ ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3,4,5));
UTIL_EXPORT void ut_avlCConstWalkRangeReverse (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a) __nonnull((1,2,3,4,5));

_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCIterFirst (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCIterSuccEq (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCIterSucc (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter, _In_ const void *key) __nonnull_all__;
_Check_return_ _Ret_maybenull_ UTIL_EXPORT void *ut_avlCIterNext (_Inout_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter) __nonnull_all__;

#if defined (__cplusplus)
}
#endif

#endif /* UT_AVL_H */
