// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_FIBHEAP_H
#define DDSRT_FIBHEAP_H

/** @file fibheap.h
  The main purpose of a fibonacci heap is for use as a priority queue i.e. repeatedly extracting the minimum of the set.
  Certain operations i.e. insert, merge, decrease_key are very efficient in that they have O(1) time complexity,
  but they they degrade the shape of the heap, which requires rebalancing to fix.
  The necessary rebalancing is delayed until the next call to extract_min.

  For example, after lots of calls to insert and/or decrease_key, the next call to extract_min is very expensive
  since it has to rebalance, and this could take as much as O(N) in the worst case. This is where you pay off the debt
  for procrastinating on rebalancing. Once balanced, following calls to extract_min will be O(log(N)).
*/

#include <stdint.h>

#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/// @brief The fibheap node is the basic element of the fibonacci heap.
/// 
/// - children: Rather than have a pointer to each child, it has one pointer to a representative child.
///   The other children can then be reached through the child's prev/next.
/// - prev/next: Siblings form a circular doubly linked list, so all can be reached going either direction.
///   A node without siblings will point prev/next to itself.
/// - mark: Is either 0 (unmarked) or 1 (marked). It is marked to indicate the node has lost a child since the last time
///   it was made the child of another node. Newly created nodes are unmarked.
///   A node becomes unmarked whenever it is made the child of another node.
///   The mark flag is of importance for @ref ddsrt_fibheap_decrease_key and @ref ddsrt_fibheap_delete.
/// 
/// To store user data in the heap, the node must be embedded in a user node, which is a struct containing the user data e.g.:
/// @code{.c}
/// typedef struct num_s{ // user node
///   uint64_t val; // user data
///   ddsrt_fibheap_node node;
/// }num_t;
/// @endcode
/// 
/// The offset obtained as 'offsetof(num_t, node)' is needed for @ref ddsrt_fibheap_def_init
typedef struct ddsrt_fibheap_node {
    struct ddsrt_fibheap_node *parent; ///< The node's parent
    struct ddsrt_fibheap_node *children; ///< The node's representative child.
    struct ddsrt_fibheap_node *prev; ///< The node's left sibling.
    struct ddsrt_fibheap_node *next; ///< The node’s right sibling.
    unsigned mark: 1; ///< Indicates whether the node has lost a child since the last time it was made the child of another node.
    unsigned degree: 31; ///< The number of children the node has
} ddsrt_fibheap_node_t;

/**
 * @brief The fibonacci heap definition for the @ref ddsrt_fibheap
 * 
 * Regarding the compare function, input arguments are pointers to the user nodes.
 * The expected return values are as follows:
 * - value < 0 to indicate va is less than vb
 * - value > 0 to indicate va is greater than vb
 * - value == 0 to indicate va is equal to vb
 * 
 * See @ref ddsrt_fibheap_def_init, @ref DDSRT_FIBHEAPDEF_INITIALIZER
 */
typedef struct ddsrt_fibheap_def {
    uintptr_t offset; ///< The offset of the @ref ddsrt_fibheap_node with respect to the user node
    int (*cmp) (const void *va, const void *vb); ///< compare function for user nodes
} ddsrt_fibheap_def_t;

/** @brief The fibonacci heap */
typedef struct ddsrt_fibheap {
    ddsrt_fibheap_node_t *roots; ///< points to root node with minimum key value
} ddsrt_fibheap_t;

/**
 * @brief Macro to initialize @ref ddsrt_fibheap_def
 * 
 * See @ref ddsrt_fibheap_def_init
 */
#define DDSRT_FIBHEAPDEF_INITIALIZER(offset, cmp) { (offset), (cmp) }

/**
 * @brief Initialize the @ref ddsrt_fibheap_def
 * 
 * @param[out] fhdef the fibonacci heap definition to initialize
 * @param[in] offset the offset
 * @param[in] cmp the compare function
 * 
 * See @ref ddsrt_fibheap_init, @ref DDSRT_FIBHEAPDEF_INITIALIZER
 */
DDS_EXPORT void ddsrt_fibheap_def_init (ddsrt_fibheap_def_t *fhdef, uintptr_t offset, int (*cmp) (const void *va, const void *vb));

/**
 * @brief Initialize the @ref ddsrt_fibheap
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[out] fh the fibonacci heap
 * 
 * See @ref ddsrt_fibheap_def_init
 */
DDS_EXPORT void ddsrt_fibheap_init (const ddsrt_fibheap_def_t *fhdef, ddsrt_fibheap_t *fh);

/**
 * @brief Access the minimum node
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[in] fh the fibonacci heap
 * @return pointer to the minimum user node, NULL if empty
 * 
 * See @ref ddsrt_fibheap_extract_min
 */
DDS_EXPORT void *ddsrt_fibheap_min (const ddsrt_fibheap_def_t *fhdef, const ddsrt_fibheap_t *fh);

/**
 * @brief Merge two fibonacci heaps
 * 
 * Contents of heap b are merged into heap a, emptying heap b.
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[in,out] a target fibonacci heap
 * @param[in,out] b source fibonacci heap
 * 
 * See @ref ddsrt_fibheap_insert
 */
DDS_EXPORT void ddsrt_fibheap_merge (const ddsrt_fibheap_def_t *fhdef, ddsrt_fibheap_t *a, ddsrt_fibheap_t *b);

/**
 * @brief Insert a node into a fibonacci heap
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[in,out] fh the fibonacci heap
 * @param[in] vnode user node to insert
 * 
 * See @ref ddsrt_fibheap_extract_min, @ref ddsrt_fibheap_delete, @ref ddsrt_fibheap_merge
 */
DDS_EXPORT void ddsrt_fibheap_insert (const ddsrt_fibheap_def_t *fhdef, ddsrt_fibheap_t *fh, const void *vnode);

/**
 * @brief Remove a node from a fibonacci heap
 * 
 * Despite the name 'delete', it only removes the node from the heap.
 * To fully delete it, you still need to free the memory associated with the node.
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[in,out] fh the fibonacci heap
 * @param[in] vnode user node to delete
 * 
 * See @ref ddsrt_fibheap_extract_min, @ref ddsrt_fibheap_insert
 */
DDS_EXPORT void ddsrt_fibheap_delete (const ddsrt_fibheap_def_t *fhdef, ddsrt_fibheap_t *fh, const void *vnode);

/**
 * @brief Take the minimum node from a fibonacci heap
 * 
 * The user is responsible for freeing the memory associated with the node.
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[in,out] fh the fibonacci heap
 * @return pointer to the minimum user node, NULL if empty
 * 
 * See @ref ddsrt_fibheap_min, @ref ddsrt_fibheap_delete, @ref ddsrt_fibheap_insert
 */
DDS_EXPORT void *ddsrt_fibheap_extract_min (const ddsrt_fibheap_def_t *fhdef, ddsrt_fibheap_t *fh);

/**
 * @brief Reposition a node in the fibonacci heap to account for a decrease of its key
 * 
 * Despite the name 'decrease_key', the function doesn't decrease the key.
 * It assumes the key has already been decreased, and repositions the node such as to maintain a valid heap structure.
 * 
 * @param[in] fhdef the fibonacci heap definition
 * @param[in,out] fh the fibonacci heap
 * @param[in] vnode user node to reposition
 */
DDS_EXPORT void ddsrt_fibheap_decrease_key (const ddsrt_fibheap_def_t *fhdef, ddsrt_fibheap_t *fh, const void *vnode);

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_FIBHEAP_H */
