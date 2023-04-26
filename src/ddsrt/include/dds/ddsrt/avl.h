// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_AVL_H
#define DDSRT_AVL_H

/** \file avl.h
  The tree library never performs memory allocations or deallocations internally.

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
   a "summary" of the subtree -- currently only used in DDSI, in one
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
   memory and updates.
*/

#include <stdint.h>
#include <stdlib.h>

#include "dds/export.h"
#include "dds/ddsrt/attributes.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DDSRT_AVL_MAX_TREEHEIGHT (12 * sizeof (void *))

/**
 * @brief User defined compare function.
 * 
 * Input arguments are expected to be pointers to the keys.
 * The expected return values are as follows:
 * - value < 0 to indicate key a is less than key b
 * - value > 0 to indicate key a is greater than key b
 * - value == 0 to indicate key a is equal to key b
 * @see ddsrt_avl_compare_r_t
 */
typedef int (*ddsrt_avl_compare_t) (const void *a, const void *b);

/** @brief Same as @ref ddsrt_avl_compare_t, but with extra argument for state information. */
typedef int (*ddsrt_avl_compare_r_t) (const void *a, const void *b, void *arg);

/**
 * @brief User defined augment function.
 * 
 * If provided, it is called whenever the structure of the tree changes,
 * or when explicitly calling @ref ddsrt_avl_augment_update.
 * It is called for each node on the path to (and inclusive of) the root.
 * It is intended to update user data in nodes when a change occurs in one or both child nodes,
 * or when a the change occurs in the node itself.
 */
typedef void (*ddsrt_avl_augment_t) (void *node, const void *left, const void *right);

/**
 * @brief User defined walk function to perform an action on each node in the tree.
 */
typedef void (*ddsrt_avl_walk_t) (void *node, void *arg);

/**
 * @brief Like @ref ddsrt_avl_walk_t, but works on const nodes.
 */
typedef void (*ddsrt_avl_const_walk_t) (const void *node, void *arg);

/// @brief The avl node is the basic element of the avl tree structure.
/// 
/// To store user data in the tree, the avl node must be embedded in a user node, which is a struct containing the user data.
/// All API calls with a node as input or output, deal with the user node. Example:
/// @code{.c}
/// typedef struct num_s{ // user node
///   ddsrt_avl_node_t node;
///   uint64_t val; // user data
/// }num_t;
/// @endcode
typedef struct ddsrt_avl_node {
    struct ddsrt_avl_node *cs[2]; ///< contains pointers to the left and right child node
    struct ddsrt_avl_node *parent; ///< points to the parent node
    int height; ///< is the height of the subtree starting at this node
} ddsrt_avl_node_t;

/**
 * Save a reference/dereference when using an indirect key. For details, see @ref ddsrt_avl_treedef_init.
 */
#define DDSRT_AVL_TREEDEF_FLAG_INDKEY 1
#define DDSRT_AVL_TREEDEF_FLAG_R 2 /**< Flag used internally by @ref ddsrt_avl_treedef_init_r : compare function has extra argument */
#define DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS 4 /**< Flag for @ref ddsrt_avl_treedef_init : multiple nodes with the same key are allowed */

/**
 * @brief The tree definition.
 * 
 * The treedef stores settings for the tree, but does not hold the actual tree itself. This implies that:
 * - A single treedef can be used for multiple trees of the same type.
 * - Most tree operations require you to provide both the treedef and the @ref ddsrt_avl_tree.
 * 
 * To initialize it, use @ref ddsrt_avl_treedef_init, @ref ddsrt_avl_treedef_init_r,
 * or one of the 'DDSRT_AVL_TREEDEF_INITIALIZER' macros.
 */
typedef struct ddsrt_avl_treedef {
#if defined (__cplusplus)
    ddsrt_avl_treedef() {}
#endif
    size_t avlnodeoffset;
    size_t keyoffset;
    union {
        ddsrt_avl_compare_t comparekk;
        ddsrt_avl_compare_r_t comparekk_r;
    } u;
    ddsrt_avl_augment_t augment;
    uint32_t flags;
    void *cmp_arg; /* for _r variant */
} ddsrt_avl_treedef_t;

/** @brief Counted version of @ref ddsrt_avl_treedef */
typedef struct ddsrt_avl_ctreedef {
    ddsrt_avl_treedef_t t;
} ddsrt_avl_ctreedef_t;

/**
 * @brief The avl tree.
 * 
 * The tree only knows the root @ref ddsrt_avl_node. The settings for the tree are stored separately in the @ref ddsrt_avl_treedef.
 * As a result, most tree operations require you to provide both the treedef and the tree.
 */
typedef struct ddsrt_avl_tree {
    ddsrt_avl_node_t *root; ///< is the root node of the tree
} ddsrt_avl_tree_t;

/** @brief Counted version of @ref ddsrt_avl_tree */
typedef struct ddsrt_avl_ctree {
    ddsrt_avl_tree_t t;
    size_t count;
} ddsrt_avl_ctree_t;

typedef struct ddsrt_avl_path {
    int depth; /* total depth of path */
    int pnodeidx;
    ddsrt_avl_node_t *parent; /* (nodeidx == 0 ? NULL : *(path[nodeidx-1])) */
    ddsrt_avl_node_t **pnode[DDSRT_AVL_MAX_TREEHEIGHT];
} ddsrt_avl_path_t;

/**
 * @brief Path for inserting a node.
 * @see ddsrt_avl_lookup_ipath
 * @see ddsrt_avl_insert_ipath
 */
typedef struct ddsrt_avl_ipath {
    ddsrt_avl_path_t p;
} ddsrt_avl_ipath_t;

/**
 * @brief Path for removing a node.
 * @see ddsrt_avl_lookup_dpath
 * @see ddsrt_avl_delete_dpath
 */
typedef struct ddsrt_avl_dpath {
    ddsrt_avl_path_t p;
} ddsrt_avl_dpath_t;

/**
 * @brief Iter object for the iterator to store its progress and know where to go next.
 */
typedef struct ddsrt_avl_iter {
    const ddsrt_avl_treedef_t *td;
    ddsrt_avl_node_t *right;
    ddsrt_avl_node_t **todop;
    ddsrt_avl_node_t *todo[1+DDSRT_AVL_MAX_TREEHEIGHT];
} ddsrt_avl_iter_t;

/** @brief Counted version of @ref ddsrt_avl_iter*/
typedef struct ddsrt_avl_citer {
    ddsrt_avl_iter_t t;
} ddsrt_avl_citer_t;

/* avlnodeoffset and keyoffset must both be in [0,2**31-1] */
#define DDSRT_AVL_TREEDEF_INITIALIZER(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), 0, 0 }
#define DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_INDKEY, 0 }
#define DDSRT_AVL_TREEDEF_INITIALIZER_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS, 0 }
#define DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, augment) { (avlnodeoffset), (keyoffset), { .comparekk = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_INDKEY|DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS, 0 }
#define DDSRT_AVL_TREEDEF_INITIALIZER_R(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_R, (cmparg) }
#define DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY_R(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_INDKEY|DDSRT_AVL_TREEDEF_FLAG_R, (cmparg) }
#define DDSRT_AVL_TREEDEF_INITIALIZER_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_R|DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS, (cmparg) }
#define DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk_, cmparg, augment) { (avlnodeoffset), (keyoffset), { .comparekk_r = (comparekk_) }, (augment), DDSRT_AVL_TREEDEF_FLAG_INDKEY|DDSRT_AVL_TREEDEF_FLAG_R|DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS, (cmparg) }

/* Not maintaining # nodes */

/// @brief Initialize the @ref ddsrt_avl_treedef
/// 
/// Different types of trees are supported. This function defines which type to use.
/// Given a node defined as:
/// @code{.c}
/// typedef struct num_s{ // user node
///   ddsrt_avl_node_t node;
///   uint64_t val; // user data
/// }num_t;
/// @endcode
/// 
/// For a treedef initialized with default settings:
/// @code{.c}
/// ddsrt_avl_treedef_t td;
/// ddsrt_avl_treedef_init(&td, offsetof(num_t, node), offsetof(num_t, val), num_cmp, 0, 0);
/// @endcode
/// 
/// A lookup is performed as:
/// @code{.c}
/// uint64_t key = 7;
/// num_t* num_lookup = ddsrt_avl_lookup(&td, &tree, &key);
/// @endcode
/// 
/// By using keyoffset == 0, you can do the lookup using a dummy node:
/// @code{.c}
/// num_t dummy;
/// dummy.val = 7;
/// num_t* num_lookup = ddsrt_avl_lookup(&td, &tree, &dummy);
/// @endcode
/// 
/// Regarding the flags:
/// - The flag @ref DDSRT_AVL_TREEDEF_FLAG_INDKEY affects functions like @ref ddsrt_avl_compare_t and @ref ddsrt_avl_lookup.
///   These functions expect their 'key' argument to be a pointer, so by default you need to pass a reference to the key.
///   Alternatively, by using @ref DDSRT_AVL_TREEDEF_FLAG_INDKEY you indicate that the key is already a pointer, allowing you to pass the key directly.
///   As an example in the case of a C string, using the INDKEY flag means you can provide the key directly (char*), rather than a reference (char**).
///   @code{.c}
///   typedef struct name_s{ // user node
///     ddsrt_avl_node_t node;
///     char* val; // user data
///   }name_t;
///   @endcode
/// 
///   If using the default flag:
///   @code{.c}
///   ddsrt_avl_treedef_t td;
///   ddsrt_avl_treedef_init(&td, offsetof(name_t, node), offsetof(name_t, val), name_cmp_default, 0, 0);
///   @endcode
///
///   Then a lookup is done as:
///   @code{.c}
///   char* key = strdup("John");
///   name_t* name_lookup = ddsrt_avl_lookup(&td, &tree, &key); // Using default flag
///   @endcode
///
///   If using the INDKEY flag:
///   @code{.c}
///   ddsrt_avl_treedef_t td;
///   ddsrt_avl_treedef_init(&td, offsetof(name_t, node), offsetof(name_t, val), name_cmp_indkey, 0, DDSRT_AVL_TREEDEF_FLAG_INDKEY);
///   @endcode
/// 
///   Then a lookup is done as:
///   @code{.c}
///   char* key = strdup("John");
///   name_t* name_lookup = ddsrt_avl_lookup(&td, &tree, key); // Using INDKEY flag
///   @endcode
/// 
/// - Multiple nodes with the same key are not allowed, unless using @ref DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS.
/// 
/// To set multiple flags, use the bitwise or.
/// 
/// @param[out] td the treedef to initialize
/// @param[in] avlnodeoffset offset of @ref ddsrt_avl_node relative to the user node.
/// @param[in] keyoffset offset of the key relative to the user node
/// @param[in] comparekk a key compare function, see @ref ddsrt_avl_compare_t
/// @param[in] augment optional function @ref ddsrt_avl_augment_t, provide NULL if not used
/// @param[in] flags optional flags, use 0 for default
/// @see ddsrt_avl_treedef_init_r
/// @see ddsrt_avl_init
DDS_EXPORT void ddsrt_avl_treedef_init (ddsrt_avl_treedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_t comparekk, ddsrt_avl_augment_t augment, uint32_t flags) ddsrt_nonnull((1,4));

/**
 * @brief Like @ref ddsrt_avl_treedef_init, but there is an extra argument for the compare function.
 * 
 * @param[out] td the treedef to initialize
 * @param[in] avlnodeoffset offset of @ref ddsrt_avl_node relative to the user node.
 * @param[in] keyoffset offset of the key relative to the user node
 * @param[in] comparekk_r a key compare function, see @ref ddsrt_avl_compare_r_t
 * @param[in] cmp_arg is fed to the compare function
 * @param[in] augment optional function @ref ddsrt_avl_augment_t, provide NULL if not used
 * @param[in] flags optional flags, use 0 for default
 * @see ddsrt_avl_treedef_init
 */
DDS_EXPORT void ddsrt_avl_treedef_init_r (ddsrt_avl_treedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_r_t comparekk_r, void *cmp_arg, ddsrt_avl_augment_t augment, uint32_t flags) ddsrt_nonnull((1,4));

/**
 * @brief Initialize the tree.
 * 
 * The treedef must have been initialized prior to calling this.
 * 
 * @param[in] td treedef of the tree
 * @param[out] tree the avl tree
 * @see ddsrt_avl_free
 * @see ddsrt_avl_treedef_init
 */
DDS_EXPORT void ddsrt_avl_init (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree) ddsrt_nonnull_all;

/**
 * @brief Destroy the tree
 * 
 * Applies user defined free function to each node, and initializes the tree again.
 * The free function is optional, but if not provided, the user is responsible for freeing the memory.
 * 
 * @param[in] td treedef of the tree
 * @param[in,out] tree the avl tree
 * @param[in] freefun optional user defined free function to use on nodes, NULL if not used
 * @see ddsrt_avl_init
 */
DDS_EXPORT void ddsrt_avl_free (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void (*freefun) (void *node)) ddsrt_nonnull((1,2));

/**
 * @brief Same as @ref ddsrt_avl_free, but with an extra argument for the free function.
 * 
 * Extra parameter:
 * @param[in] arg is for the user defined free function
 */
DDS_EXPORT void ddsrt_avl_free_arg (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void (*freefun) (void *node, void *arg), void *arg) ddsrt_nonnull((1,2));

/**
 * @brief Get the root node
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @returns pointer to root node, NULL if tree is empty
 */
DDS_EXPORT void *ddsrt_avl_root (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree) ddsrt_nonnull_all;

/** Same as @ref ddsrt_avl_root, but has undefined behavior in case the tree is empty. */
DDS_EXPORT void *ddsrt_avl_root_non_empty (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree) ddsrt_nonnull_all ddsrt_attribute_returns_nonnull;

/**
 * @brief Lookup a node based on the key
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @return pointer to the node satisfying (node.key == key), NULL if failed
 */
DDS_EXPORT void *ddsrt_avl_lookup (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;

/**
 * @brief Same as @ref ddsrt_avl_lookup, but also initializes a @ref ddsrt_avl_ipath.
 * 
 * The path initialized is intended for use with @ref ddsrt_avl_insert_ipath.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @param[out] path is the insert path
 * @return pointer to the node satisfying (node.key == key), NULL if failed
 * @see ddsrt_avl_lookup_dpath
 */
DDS_EXPORT void *ddsrt_avl_lookup_ipath (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key, ddsrt_avl_ipath_t *path) ddsrt_nonnull_all;

/**
 * @brief Same as @ref ddsrt_avl_lookup, but also initializes a @ref ddsrt_avl_dpath.
 * 
 * The path initialized is intended for use with @ref ddsrt_avl_delete_dpath.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @param[out] path is the delete path
 * @return pointer to the node satisfying (node.key == key), NULL if failed
 * @see ddsrt_avl_lookup_ipath
 */
DDS_EXPORT void *ddsrt_avl_lookup_dpath (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key, ddsrt_avl_dpath_t *path) ddsrt_nonnull_all;

/**
 * @brief Like @ref ddsrt_avl_lookup, but match greatest node satisfying (node.key <= key)
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @return pointer to the node satisfying (node.key <= key), NULL if failed
 */
DDS_EXPORT void *ddsrt_avl_lookup_pred_eq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;

/**
 * @brief Like @ref ddsrt_avl_lookup, but match smallest node satisfying (node.key >= key)
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @return pointer to the node satisfying (node.key >= key), NULL if failed
 */
DDS_EXPORT void *ddsrt_avl_lookup_succ_eq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;

/**
 * @brief Like @ref ddsrt_avl_lookup, but match greatest node satisfying (node.key < key)
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @return pointer to the node satisfying (node.key < key), NULL if failed
 */
DDS_EXPORT void *ddsrt_avl_lookup_pred (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;

/**
 * @brief Like @ref ddsrt_avl_lookup, but match smallest node satisfying (node.key > key)
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] key key to search for
 * @return pointer to the node satisfying (node.key > key), NULL if failed
 */
DDS_EXPORT void *ddsrt_avl_lookup_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;

/**
 * @brief Insert a node into the tree.
 * 
 * There is no protection against duplicates (which causes a crash if the treedef says duplicates are not allowed).
 * If you want a safe insert, you should use @ref ddsrt_avl_lookup_ipath to check for existence, then insert using @ref ddsrt_avl_insert_ipath.
 * 
 * @param[in] td treedef of the tree
 * @param[in,out] tree the avl tree
 * @param[in] node the node to insert
 * @see ddsrt_avl_delete
 */
DDS_EXPORT void ddsrt_avl_insert (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *node) ddsrt_nonnull_all;

/**
 * @brief Remove a node from the tree.
 * 
 * Despite the name 'delete', it only removes the node from the tree. To fully delete it, you still need to free the memory associated with the node.
 * There is no protection against a crash due to trying to remove a nonexisting node.
 * If you want a safe remove, you should use @ref ddsrt_avl_lookup_dpath to check for existence, then remove using @ref ddsrt_avl_delete_dpath.
 * 
 * @param[in] td treedef of the tree
 * @param[in,out] tree the avl tree
 * @param[in] node the node to remove
 * @see ddsrt_avl_insert
 */
DDS_EXPORT void ddsrt_avl_delete (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *node) ddsrt_nonnull_all;

/**
 * @brief Same as @ref ddsrt_avl_insert, but also requires a @ref ddsrt_avl_ipath.
 * 
 * The path required can be initialized by @ref ddsrt_avl_lookup_ipath.
 * 
 * @param[in] td treedef of the tree
 * @param[in,out] tree the avl tree
 * @param[in] node the node to insert
 * @param[in] path is the insert path
 * @see ddsrt_avl_delete_dpath
 */
DDS_EXPORT void ddsrt_avl_insert_ipath (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *node, ddsrt_avl_ipath_t *path) ddsrt_nonnull_all;

/**
 * @brief Same as @ref ddsrt_avl_delete, but also requires a @ref ddsrt_avl_dpath.
 * 
 * The path required can be initialized by @ref ddsrt_avl_lookup_dpath.
 * 
 * @param[in] td treedef of the tree
 * @param[in,out] tree the avl tree
 * @param[in] node the node to remove
 * @param[in] path is the delete path
 * @see ddsrt_avl_insert_ipath
 */
DDS_EXPORT void ddsrt_avl_delete_dpath (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *node, ddsrt_avl_dpath_t *path) ddsrt_nonnull_all;

/**
 * @brief Replace a node in the tree.
 * 
 * Replace a node in the tree without using @ref ddsrt_avl_delete and @ref ddsrt_avl_insert.
 * This operation can invalidate the tree as it doesn't check whether the key of the new node preserves the ordering of the set.
 * 
 * @param[in] td treedef of the tree
 * @param[in,out] tree the avl tree
 * @param[in] oldn the node to replace
 * @param[in] newn the node to take the old node's place
 * @see ddsrt_avl_insert
 * @see ddsrt_avl_delete
 */
DDS_EXPORT void ddsrt_avl_swap_node (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *oldn, void *newn) ddsrt_nonnull_all;

/**
 * @brief Call the user defined augment function @ref ddsrt_avl_augment_t on a node and its parents up to the root.
 * 
 * It is intended to update user data in nodes when a change occurs in one or both child nodes, or when a the change occurs in the node itself.
 * Note that @ref ddsrt_avl_augment_t is called automatically whenever the structure of the tree changes, such as in rotations.
 * It is called for each node on the path to (and inclusive of) the root.
 * 
 * @param[in] td treedef of the tree
 * @param[in] node is the node from which to start the augmentation
 */
DDS_EXPORT void ddsrt_avl_augment_update (const ddsrt_avl_treedef_t *td, void *node) ddsrt_nonnull_all;

/**
 * @brief Check whether the tree is empty or not.
 * 
 * @param[in] tree the avl tree
 * @return int meant to be interpreted as bool (1 is true, 0 is false)
 */
DDS_EXPORT int ddsrt_avl_is_empty (const ddsrt_avl_tree_t *tree) ddsrt_nonnull_all;

/**
 * @brief Check whether the tree contains exactly one node.
 * 
 * @param[in] tree the avl tree
 * @return int meant to be interpreted as bool (1 is true, 0 is false)
 */
DDS_EXPORT int ddsrt_avl_is_singleton (const ddsrt_avl_tree_t *tree) ddsrt_nonnull_all;

/**
 * @brief Lookup node with smallest key.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @return pointer to the node
 * @see ddsrt_avl_find_max
 */
DDS_EXPORT void *ddsrt_avl_find_min (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree) ddsrt_nonnull_all;

/**
 * @brief Lookup node with greatest key.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @return pointer to the node
 * @see ddsrt_avl_find_min
 */
DDS_EXPORT void *ddsrt_avl_find_max (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree) ddsrt_nonnull_all;

/**
 * @brief Get the previous node in the ordered set.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] vnode the current node
 * @return pointer to the node
 * @see ddsrt_avl_find_succ
 */
DDS_EXPORT void *ddsrt_avl_find_pred (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *vnode) ddsrt_nonnull((1,2));

/**
 * @brief Get the next node in in the ordered set
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] vnode the current node
 * @return pointer to the node
 * @see ddsrt_avl_find_pred
 * @see ddsrt_avl_iter_next
 */
DDS_EXPORT void *ddsrt_avl_find_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *vnode) ddsrt_nonnull((1,2));

/**
 * @brief Walk the tree and apply a user defined function @ref ddsrt_avl_walk_t to each node.
 * 
 * Functionally the same as doing a loop using @ref ddsrt_avl_find_succ or @ref ddsrt_avl_iter_next, and calling the function on each node.
 * Modifying the tree structure is not allowed when using the walk function. If that is desired, use @ref ddsrt_avl_find_succ
 * (or @ref ddsrt_avl_find_pred for the reverse walk).
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] f is the user defined walk function
 * @param[in,out] a is an argument for the user defined function
 * @see ddsrt_avl_find_succ
 * @see ddsrt_avl_iter_next
 */
DDS_EXPORT void ddsrt_avl_walk (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, ddsrt_avl_walk_t f, void *a) ddsrt_nonnull((1,2,3));

/**
 * @brief Like @ref ddsrt_avl_walk, but the user defined function @ref ddsrt_avl_const_walk_t works on const nodes.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] f is the user defined walk function
 * @param[in,out] a is an argument for the user defined function
 */
DDS_EXPORT void ddsrt_avl_const_walk (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_const_walk_t f, void *a) ddsrt_nonnull((1,2,3));

/**
 * @brief Like @ref ddsrt_avl_walk, but walk within a restricted range defined by [min, max].
 * 
 * - If min is smaller than the smallest value, it starts at the smallest value.
 * - If max is larger than the largest value, it finishes at the largest value.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] min The smallest value to start at
 * @param[in] max The largest value to finish at (inclusive)
 * @param[in] f is the user defined walk function
 * @param[in,out] a is an argument for the user defined function
 */
DDS_EXPORT void ddsrt_avl_walk_range (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a)  ddsrt_nonnull((1,2,3,4,5));

/**
 * @brief Like @ref ddsrt_avl_walk_range, but the user defined function @ref ddsrt_avl_const_walk_t works on const nodes.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] min The smallest value to start at
 * @param[in] max The largest value to finish at (inclusive)
 * @param[in] f is the user defined walk function
 * @param[in,out] a is an argument for the user defined function
*/
DDS_EXPORT void ddsrt_avl_const_walk_range (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a) ddsrt_nonnull((1,2,3,4,5));

/**
 * @brief Like @ref ddsrt_avl_walk_range, but walks in the reverse direction.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] min The smallest value to finish at (inclusive)
 * @param[in] max The largest value to start at
 * @param[in] f is the user defined walk function
 * @param[in,out] a is an argument for the user defined function
 */
DDS_EXPORT void ddsrt_avl_walk_range_reverse (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a) ddsrt_nonnull((1,2,3));

/**
 * @brief Like @ref ddsrt_avl_walk_range_reverse, but the user defined function @ref ddsrt_avl_const_walk_t works on const nodes.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[in] min The smallest value to finish at (inclusive)
 * @param[in] max The largest value to start at
 * @param[in] f is the user defined walk function
 * @param[in,out] a is an argument for the user defined function
 */
DDS_EXPORT void ddsrt_avl_const_walk_range_reverse (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a) ddsrt_nonnull((1,2,3));

/**
 * @brief Get the first node in in the ordered set, and initialize the iterator @ref ddsrt_avl_iter_t.
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[out] iter the iterator
 * @return pointer to the node
 * @see ddsrt_avl_iter_next
 * @see ddsrt_avl_find_min
 */
DDS_EXPORT void *ddsrt_avl_iter_first (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_iter_t *iter) ddsrt_nonnull_all;

/**
 * @brief Similar to @ref ddsrt_avl_iter_first, but start at smallest node satisfying (node.key >= key)
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[out] iter the iterator
 * @param[in] key the key with which to do the lookup
 * @return pointer to the node
 */
DDS_EXPORT void *ddsrt_avl_iter_succ_eq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_iter_t *iter, const void *key) ddsrt_nonnull_all;

/**
 * @brief Similar to @ref ddsrt_avl_iter_first, but start at smallest node satisfying (node.key > key)
 * 
 * @param[in] td treedef of the tree
 * @param[in] tree the avl tree
 * @param[out] iter the iterator
 * @param[in] key the key with which to do the lookup
 * @return pointer to the node
 */
DDS_EXPORT void *ddsrt_avl_iter_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_iter_t *iter, const void *key) ddsrt_nonnull_all;

/**
 * @brief Get the next node in in the ordered set.
 * 
 * Modifying the tree structure is not allowed when using the iterator. If that is desired, use @ref ddsrt_avl_find_succ.
 * 
 * @param[in,out] iter the iterator
 * @return pointer to the node
 * @see ddsrt_avl_iter_first
 * @see ddsrt_avl_find_succ
 */
DDS_EXPORT void *ddsrt_avl_iter_next (ddsrt_avl_iter_t *iter) ddsrt_nonnull_all;

/* Maintaining # nodes */

#define DDSRT_AVL_CTREEDEF_INITIALIZER(avlnodeoffset, keyoffset, comparekk, augment) { DDSRT_AVL_TREEDEF_INITIALIZER (avlnodeoffset, keyoffset, comparekk, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_INDKEY(avlnodeoffset, keyoffset, comparekk, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY (avlnodeoffset, keyoffset, comparekk, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_INDKEY_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_R(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_R (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_INDKEY_R(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY_R (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_R_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }
#define DDSRT_AVL_CTREEDEF_INITIALIZER_INDKEY_R_ALLOWDUPS(avlnodeoffset, keyoffset, comparekk, cmparg, augment) { DDSRT_AVL_TREEDEF_INITIALIZER_INDKEY_R_ALLOWDUPS (avlnodeoffset, keyoffset, comparekk, cmparg, augment) }

/** @brief Counted version of @ref ddsrt_avl_treedef_init*/
DDS_EXPORT void ddsrt_avl_ctreedef_init (ddsrt_avl_ctreedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_t comparekk, ddsrt_avl_augment_t augment, uint32_t flags) ddsrt_nonnull((1,4));

/** @brief Counted version of @ref ddsrt_avl_treedef_init_r*/
DDS_EXPORT void ddsrt_avl_ctreedef_init_r (ddsrt_avl_ctreedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_r_t comparekk_r, void *cmp_arg, ddsrt_avl_augment_t augment, uint32_t flags) ddsrt_nonnull((1,4));

/** @brief Counted version of @ref ddsrt_avl_init*/
DDS_EXPORT void ddsrt_avl_cinit (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_free*/
DDS_EXPORT void ddsrt_avl_cfree (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void (*freefun) (void *node)) ddsrt_nonnull((1,2));

/** @brief Counted version of @ref ddsrt_avl_free_arg*/
DDS_EXPORT void ddsrt_avl_cfree_arg (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void (*freefun) (void *node, void *arg), void *arg) ddsrt_nonnull((1,2));

/** @brief Counted version of @ref ddsrt_avl_root*/
DDS_EXPORT void *ddsrt_avl_croot (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_root_non_empty*/
DDS_EXPORT void *ddsrt_avl_croot_non_empty (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup*/
DDS_EXPORT void *ddsrt_avl_clookup (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup_ipath*/
DDS_EXPORT void *ddsrt_avl_clookup_ipath (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key, ddsrt_avl_ipath_t *path) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup_dpath*/
DDS_EXPORT void *ddsrt_avl_clookup_dpath (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key, ddsrt_avl_dpath_t *path) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup_pred_eq*/
DDS_EXPORT void *ddsrt_avl_clookup_pred_eq (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup_succ_eq*/
DDS_EXPORT void *ddsrt_avl_clookup_succ_eq (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup_pred*/
DDS_EXPORT void *ddsrt_avl_clookup_pred (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_lookup_succ*/
DDS_EXPORT void *ddsrt_avl_clookup_succ (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_insert*/
DDS_EXPORT void ddsrt_avl_cinsert (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_delete*/
DDS_EXPORT void ddsrt_avl_cdelete (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_insert_ipath*/
DDS_EXPORT void ddsrt_avl_cinsert_ipath (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node, ddsrt_avl_ipath_t *path) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_delete_dpath*/
DDS_EXPORT void ddsrt_avl_cdelete_dpath (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node, ddsrt_avl_dpath_t *path) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_swap_node*/
DDS_EXPORT void ddsrt_avl_cswap_node (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *oldn, void *newn) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_augment_update*/
DDS_EXPORT void ddsrt_avl_caugment_update (const ddsrt_avl_ctreedef_t *td, void *node) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_is_empty*/
DDS_EXPORT int ddsrt_avl_cis_empty (const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_is_singleton*/
DDS_EXPORT int ddsrt_avl_cis_singleton (const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_count*/
DDS_EXPORT size_t ddsrt_avl_ccount (const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_find_min*/
DDS_EXPORT void *ddsrt_avl_cfind_min (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_find_max*/
DDS_EXPORT void *ddsrt_avl_cfind_max (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_find_pred*/
DDS_EXPORT void *ddsrt_avl_cfind_pred (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *vnode) ddsrt_nonnull((1,2));

/** @brief Counted version of @ref ddsrt_avl_find_succ*/
DDS_EXPORT void *ddsrt_avl_cfind_succ (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *vnode) ddsrt_nonnull((1,2));

/** @brief Counted version of @ref ddsrt_avl_walk*/
DDS_EXPORT void ddsrt_avl_cwalk (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, ddsrt_avl_walk_t f, void *a) ddsrt_nonnull((1,2,3));

/** @brief Counted version of @ref ddsrt_avl_const_walk*/
DDS_EXPORT void ddsrt_avl_cconst_walk (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_const_walk_t f, void *a) ddsrt_nonnull((1,2,3));

/** @brief Counted version of @ref ddsrt_avl_walk_range*/
DDS_EXPORT void ddsrt_avl_cwalk_range (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a) ddsrt_nonnull((1,2,3,4,5));

/** @brief Counted version of @ref ddsrt_avl_const_walk_range*/
DDS_EXPORT void ddsrt_avl_cconst_walk_range (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a) ddsrt_nonnull((1,2,3,4,5));

/** @brief Counted version of @ref ddsrt_avl_walk_range_reverse*/
DDS_EXPORT void ddsrt_avl_cwalk_range_reverse (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a) ddsrt_nonnull((1,2,3,4,5));

/** @brief Counted version of @ref ddsrt_avl_const_walk_range_reverse*/
DDS_EXPORT void ddsrt_avl_cconst_walk_range_reverse (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a) ddsrt_nonnull((1,2,3,4,5));

/** @brief Counted version of @ref ddsrt_avl_iter_first*/
DDS_EXPORT void *ddsrt_avl_citer_first (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_citer_t *iter) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_iter_succ_eq*/
DDS_EXPORT void *ddsrt_avl_citer_succ_eq (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_citer_t *iter, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_iter_succ*/
DDS_EXPORT void *ddsrt_avl_citer_succ (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_citer_t *iter, const void *key) ddsrt_nonnull_all;

/** @brief Counted version of @ref ddsrt_avl_iter_next*/
DDS_EXPORT void *ddsrt_avl_citer_next (ddsrt_avl_citer_t *iter) ddsrt_nonnull_all;

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_AVL_H */
