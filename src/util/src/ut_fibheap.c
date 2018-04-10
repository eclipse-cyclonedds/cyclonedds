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
#include <stddef.h>
#include <limits.h>
#include <assert.h>

#include "util/ut_fibheap.h"

/* max degree: n >= F_{d+2} >= \phi^d ==> d <= log_\phi n, where \phi
   (as usual) is the golden ratio ~= 1.618.  We know n <= (size of
   address space) / sizeof (fh_node), log_\phi 2 ~= 1.44, sizeof
   (fh_node) >= 4, therefore max degree < log_2 (size of address
   space). */
#define MAX_DEGREE ((unsigned) (sizeof (void *) * CHAR_BIT - 1))

static int cmp (const ut_fibheapDef_t *fhdef, const ut_fibheapNode_t *a, const ut_fibheapNode_t *b)
{
    return fhdef->cmp ((const char *) a - fhdef->offset, (const char *) b - fhdef->offset);
}

void ut_fibheapDefInit (ut_fibheapDef_t *fhdef, uintptr_t offset, int (*cmp) (const void *va, const void *vb))
{
    fhdef->offset = offset;
    fhdef->cmp = cmp;
}

void ut_fibheapInit (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh)
{
    OS_UNUSED_ARG(fhdef);
    fh->roots = NULL;
}

void *ut_fibheapMin (const ut_fibheapDef_t *fhdef, const ut_fibheap_t *fh)
{
    if (fh->roots) {
        return (void *) ((char *) fh->roots - fhdef->offset);
    } else {
        return NULL;
    }
}

static void ut_fibheap_merge_nonempty_list (ut_fibheapNode_t **markptr, ut_fibheapNode_t *list)
{
    assert (list != NULL);

    if (*markptr == NULL) {
        *markptr = list;
    } else {
        ut_fibheapNode_t * const mark = *markptr;
        ut_fibheapNode_t * const old_mark_next = mark->next;
        ut_fibheapNode_t * const old_list_prev = list->prev;
        mark->next = list;
        old_mark_next->prev = old_list_prev;
        list->prev = mark;
        old_list_prev->next = old_mark_next;
    }
}

static void ut_fibheap_merge_into (const ut_fibheapDef_t *fhdef, ut_fibheap_t *a, ut_fibheapNode_t * const br)
{
    if (br == NULL) {
        return;
    } else if (a->roots == NULL) {
        a->roots = br;
    } else {
        const int c = cmp (fhdef, br, a->roots);
        ut_fibheap_merge_nonempty_list (&a->roots, br);
        if (c < 0)
            a->roots = br;
    }
}

void ut_fibheapMerge (const ut_fibheapDef_t *fhdef, ut_fibheap_t *a, ut_fibheap_t *b)
{
    /* merges nodes from b into a, thereafter, b is empty */
    ut_fibheap_merge_into (fhdef, a, b->roots);
    b->roots = NULL;
}

void ut_fibheapInsert (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh, const void *vnode)
{
    /* fibheap node is opaque => nothing in node changes as far as
     caller is concerned => declare as const argument, then drop the
     const qualifier */
    ut_fibheapNode_t *node = (ut_fibheapNode_t *) ((char *) vnode + fhdef->offset);

    /* new heap of degree 0 (i.e., only containing NODE) */
    node->parent = node->children = NULL;
    node->prev = node->next = node;
    node->mark = 0;
    node->degree = 0;

    /* then merge it in */
    ut_fibheap_merge_into (fhdef, fh, node);
}

static void ut_fibheap_add_as_child (ut_fibheapNode_t *parent, ut_fibheapNode_t *child)
{
    parent->degree++;
    child->parent = parent;
    child->prev = child->next = child;
    ut_fibheap_merge_nonempty_list (&parent->children, child);
}

static void ut_fibheap_delete_one_from_list (ut_fibheapNode_t **markptr, ut_fibheapNode_t *node)
{
    if (node->next == node) {
        *markptr = NULL;
    } else {
        ut_fibheapNode_t * const node_prev = node->prev;
        ut_fibheapNode_t * const node_next = node->next;
        node_prev->next = node_next;
        node_next->prev = node_prev;
        if (*markptr == node) {
            *markptr = node_next;
        }
    }
}

void *ut_fibheapExtractMin (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh)
{
    ut_fibheapNode_t *roots[MAX_DEGREE + 1];
    ut_fibheapNode_t * const min = fh->roots;
    unsigned min_degree_noninit = 0;

    /* empty heap => return that, alternative would be to require the
       heap to contain at least one element, but this is probably nicer
       in practice */
    if (min == NULL) {
        return NULL;
    }

    /* singleton heap => no work remaining */
    if (min->next == min && min->children == NULL) {
        fh->roots = NULL;
        return (void *) ((char *) min - fhdef->offset);
    }

    /* remove min from fh->roots */
    ut_fibheap_delete_one_from_list (&fh->roots, min);

    /* FIXME: can speed up by combining a few things & improving
       locality of reference by scanning lists only once */

    /* insert min'schildren as new roots -- must fix parent pointers,
       and reset marks because roots are always unmarked */
    if (min->children) {
        ut_fibheapNode_t * const mark = min->children;
        ut_fibheapNode_t *n = mark;
        do {
            n->parent = NULL;
            n->mark = 0;
            n = n->next;
        } while (n != mark);

        ut_fibheap_merge_nonempty_list (&fh->roots, min->children);
    }

    /* iteratively merge roots of equal degree, completely messing up
       fh->roots, ... */
    {
        ut_fibheapNode_t *const mark = fh->roots;
        ut_fibheapNode_t *n = mark;
        do {
            ut_fibheapNode_t * const n1 = n->next;

            /* if n is first root with this high a degree, there's certainly
               not going to be another root to merge with yet */
            while (n->degree < min_degree_noninit && roots[n->degree]) {
                unsigned const degree = n->degree;
                ut_fibheapNode_t *u, *v;

                if (cmp (fhdef, roots[degree], n) < 0) {
                    u = roots[degree]; v = n;
                } else {
                    u = n; v = roots[degree];
                }
                roots[degree] = NULL;
                ut_fibheap_add_as_child (u, v);
                n = u;
            }

            /* n may have changed, hence need to retest whether or not
               enough of roots has been initialised -- note that
               initialising roots[n->degree] is unnecessary, but easier */
            assert (n->degree <= MAX_DEGREE);
            while (min_degree_noninit <= n->degree) {
                roots[min_degree_noninit++] = NULL;
            }
            roots[n->degree] = n;
            n = n1;
        } while (n != mark);
    }

    /* ... but we don't mind because we have roots[], we can scan linear
       memory at an astonishing rate, and we need to compare the root
       keys anyway to find the minimum */
    {
        ut_fibheapNode_t *mark, *cursor, *newmin;
        unsigned i;
        for (i = 0; roots[i] == NULL; i++) {
            assert (i+1 < min_degree_noninit);
        }
        newmin = roots[i];
        assert (newmin != NULL);
        mark = cursor = roots[i];
        for (++i; i < min_degree_noninit; i++) {
            if (roots[i]) {
                ut_fibheapNode_t * const r = roots[i];
                if (cmp (fhdef, r, newmin) < 0)
                    newmin = r;
                r->prev = cursor;
                cursor->next = r;
                cursor = r;
            }
        }
        mark->prev = cursor;
        cursor->next = mark;

        fh->roots = newmin;
    }

    return (void *) ((char *) min - fhdef->offset);
}

static void ut_fibheap_cutnode (ut_fibheap_t *fh, ut_fibheapNode_t *node)
{
    /* by marking the node, we ensure it gets cut */
    node->mark = 1;

    /* traverse towards the root, cutting marked nodes on the way */
    while (node->parent && node->mark) {
        ut_fibheapNode_t *parent = node->parent;

        assert (parent->degree > 0);
        ut_fibheap_delete_one_from_list (&parent->children, node);
        parent->degree--;

        node->mark = 0;
        node->parent = NULL;
        node->next = node->prev = node;

        /* we assume heap properties haven't been violated, and therefore
           none of the nodes we cut can become the new minimum */
        ut_fibheap_merge_nonempty_list (&fh->roots, node);

        node = parent;
    }

    /* if we stopped because we hit an unmarked interior node, we must
       mark it */
    if (node->parent) {
        node->mark = 1;
    }
}

void ut_fibheapDecreaseKey (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh, const void *vnode)
{
    /* fibheap node is opaque => nothing in node changes as far as
       caller is concerned => declare as const argument, then drop the
       const qualifier */
    ut_fibheapNode_t *node = (ut_fibheapNode_t *) ((char *) vnode + fhdef->offset);

    if (node->parent && cmp (fhdef, node->parent, node) <= 0) {
        /* heap property not violated, do nothing */
    } else {
        if (node->parent) {
            /* heap property violated by decreasing the key, but we cut it
               pretending nothing has happened yet, then fix up the minimum if
               this node is the new minimum */
            ut_fibheap_cutnode (fh, node);
        }
        if (cmp (fhdef, node, fh->roots) < 0) {
            fh->roots = node;
        }
    }
}

void ut_fibheapDelete (const ut_fibheapDef_t *fhdef, ut_fibheap_t *fh, const void *vnode)
{
    /* fibheap node is opaque => nothing in node changes as far as
       caller is concerned => declare as const argument, then drop the
       const qualifier */
    ut_fibheapNode_t *node = (ut_fibheapNode_t *) ((char *) vnode + fhdef->offset);
    
    /* essentially decreasekey(node);extractmin while pretending the
       node key is -infinity.  That means we can't directly call
       decreasekey, because it considers the actual value of the key. */
    if (node->parent != NULL) {
        ut_fibheap_cutnode (fh, node);
    }
    fh->roots = node;
    (void) ut_fibheapExtractMin (fhdef, fh);
}
