// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_CIRCLIST_H
#define DDSRT_CIRCLIST_H

/**
 * @file circlist.h
 * 
 * Circular doubly linked list implementation.
 */

#include <stdbool.h>
#include <stdint.h>
#include "dds/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Macro to get the pointer to the user node, see @ref ddsrt_circlist_elem for an example.
 */
#define DDSRT_FROM_CIRCLIST(typ_, member_, cle_) ((typ_ *) ((char *) (cle_) - offsetof (typ_, member_)))

/** @brief the circular doubly linked list */
struct ddsrt_circlist {
  struct ddsrt_circlist_elem *latest; /**< pointer to latest inserted element */
};

/// @brief The circlist element is the basic element of circular doubly linked list structure.
/// 
/// To store user data in the list, the element must be embedded in a user element, which is a struct containing the user data.
/// All API calls with an element as input or output, deal with the circlist element. Example:
/// @code{.c}
/// typedef struct num_s{ // user element
///   uint64_t val; // user data
///   ddsrt_circlist_elem elem;
/// }num_t;
/// @endcode
/// 
/// For functions like @ref ddsrt_circlist_latest, this means that to get a pointer to the user element, you can use the macro @ref DDSRT_FROM_CIRCLIST :
/// @code{.c}
/// num_t* num = DDSRT_FROM_CIRCLIST(num_t, elem, ddsrt_circlist_latest(listptr));
/// @endcode
struct ddsrt_circlist_elem {
  struct ddsrt_circlist_elem *next;
  struct ddsrt_circlist_elem *prev;
};

/**
 * @brief initialize the list
 * 
 * @param[out] list the circular list
 */
void ddsrt_circlist_init (struct ddsrt_circlist *list);

/**
 * @brief check whether the list is empty
 * 
 * @param[in] list the circular list
 * @return true if empty, false if not empty
 */
bool ddsrt_circlist_isempty (const struct ddsrt_circlist *list);

/**
 * @brief append an element to the list
 * 
 * @param[in,out] list the circular list
 * @param[in] elem the element to append
 * 
 * See @ref ddsrt_circlist_remove
 */
void ddsrt_circlist_append (struct ddsrt_circlist *list, struct ddsrt_circlist_elem *elem);

/**
 * @brief remove an element from the list
 * 
 * The user is still responsible for freeing the memory associated with the element.
 * 
 * @param[in,out] list the circular list
 * @param[in] elem the element to append
 * 
 * See @ref ddsrt_circlist_append
 */
void ddsrt_circlist_remove (struct ddsrt_circlist *list, struct ddsrt_circlist_elem *elem);

/**
 * @brief get the oldest (earliest appended) element
 * 
 * @param[in] list the circular list
 * @return pointer to the oldest element
 * 
 * See @ref ddsrt_circlist_latest, @ref ddsrt_circlist_elem
 */
struct ddsrt_circlist_elem *ddsrt_circlist_oldest (const struct ddsrt_circlist *list);

/**
 * @brief get the latest (most recently appended) element
 * 
 * @param[in] list the circular list
 * @return pointer to the latest element
 * 
 * See @ref ddsrt_circlist_oldest, @ref ddsrt_circlist_elem
 */
struct ddsrt_circlist_elem *ddsrt_circlist_latest (const struct ddsrt_circlist *list);

#if defined(__cplusplus)
}
#endif

#endif /* DDSRT_CIRCLIST_H */
