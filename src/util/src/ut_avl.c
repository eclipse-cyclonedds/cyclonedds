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
#include <limits.h>
#include <assert.h>
#include <string.h>

#include "os/os.h"
#include "util/ut_avl.h"

#define LOAD_DIRKEY(avlnode, tree) (((char *) (avlnode)) - (tree)->avlnodeoffset + (tree)->keyoffset)
#define LOAD_INDKEY(avlnode, tree) (*((char **) (((char *) (avlnode)) - (tree)->avlnodeoffset + (tree)->keyoffset)))

static int comparenk (const ut_avlTreedef_t *td, const ut_avlNode_t *a, const void *b)
{
    const void *ka;
    if (td->flags & UT_AVL_TREEDEF_FLAG_INDKEY) {
        ka = LOAD_INDKEY (a, td);
    } else {
        ka = LOAD_DIRKEY (a, td);
    }
    if (td->flags & UT_AVL_TREEDEF_FLAG_R) {
        return td->u.comparekk_r (ka, b, td->cmp_arg);
    } else {
        return td->u.comparekk (ka, b);
    }
}

#if 0
static int comparenn_direct (const ut_avlTreedef_t *td, const ut_avlNode_t *a, const ut_avlNode_t *b)
{
    return td->comparekk (LOAD_DIRKEY (a, td), LOAD_DIRKEY (b, td));
}

static int comparenn_indirect (const ut_avlTreedef_t *td, const ut_avlNode_t *a, const ut_avlNode_t *b)
{
    return td->comparekk (LOAD_INDKEY (a, td), LOAD_INDKEY (b, td));
}

static int comparenn (const ut_avlTreedef_t *td, const ut_avlNode_t *a, const ut_avlNode_t *b)
{
    if (IS_INDKEY (td->keyoffset)) {
        return comparenn_indirect (td, a, b);
    } else {
        return comparenn_direct (td, a, b);
    }
}
#endif

static ut_avlNode_t *node_from_onode (const ut_avlTreedef_t *td, char *onode)
{
    if (onode == NULL) {
        return NULL;
    } else {
        return (ut_avlNode_t *) (onode + td->avlnodeoffset);
    }
}

static const ut_avlNode_t *cnode_from_onode (const ut_avlTreedef_t *td, const char *onode)
{
    if (onode == NULL) {
        return NULL;
    } else {
        return (const ut_avlNode_t *) (onode + td->avlnodeoffset);
    }
}

static char *onode_from_node (const ut_avlTreedef_t *td, ut_avlNode_t *node)
{
    if (node == NULL) {
        return NULL;
    } else {
        return (char *) node - td->avlnodeoffset;
    }
}

static const char *conode_from_node (const ut_avlTreedef_t *td, const ut_avlNode_t *node)
{
    if (node == NULL) {
        return NULL;
    } else {
        return (const char *) node - td->avlnodeoffset;
    }
}

static void treedef_init_common (ut_avlTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, ut_avlAugment_t augment, uint32_t flags)
{
    assert (avlnodeoffset <= 0x7fffffff);
    assert (keyoffset <= 0x7fffffff);
    td->avlnodeoffset = avlnodeoffset;
    td->keyoffset = keyoffset;
    td->augment = augment;
    td->flags = flags;
}

void ut_avlTreedefInit (_Out_ ut_avlTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_t comparekk, _In_opt_ ut_avlAugment_t augment, uint32_t flags)
{
    treedef_init_common (td, avlnodeoffset, keyoffset, augment, flags);
    td->u.comparekk = comparekk;
}

void ut_avlTreedefInit_r (_Out_ ut_avlTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_r_t comparekk_r, _Inout_opt_ void *cmp_arg, ut_avlAugment_t augment, uint32_t flags)
{
    treedef_init_common (td, avlnodeoffset, keyoffset, augment, flags | UT_AVL_TREEDEF_FLAG_R);
    td->cmp_arg = cmp_arg;
    td->u.comparekk_r = comparekk_r;
}

void ut_avlInit (_In_ const ut_avlTreedef_t *td, _Out_ ut_avlTree_t *tree)
{
    tree->root = NULL;
    (void) td;
}

static void treedestroy (const ut_avlTreedef_t *td, ut_avlNode_t *n, void (*freefun) (void *node))
{
    if (n) {
        n->parent = NULL;
        treedestroy (td, n->cs[0], freefun);
        treedestroy (td, n->cs[1], freefun);
        n->cs[0] = NULL;
        n->cs[1] = NULL;
        freefun (onode_from_node (td, n));
    }
}

static void treedestroy_arg (const ut_avlTreedef_t *td, ut_avlNode_t *n, void (*freefun) (void *node, void *arg), void *arg)
{
    if (n) {
        n->parent = NULL;
        treedestroy_arg (td, n->cs[0], freefun, arg);
        treedestroy_arg (td, n->cs[1], freefun, arg);
        n->cs[0] = NULL;
        n->cs[1] = NULL;
        freefun (onode_from_node (td, n), arg);
    }
}

void ut_avlFree (_In_ const ut_avlTreedef_t *td, _Inout_ _Post_invalid_ ut_avlTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node))
{
    ut_avlNode_t *n = tree->root;
    tree->root = NULL;
    if (freefun) {
        treedestroy (td, n, freefun);
    }
}

void ut_avlFreeArg (_In_ const ut_avlTreedef_t *td, _Inout_ _Post_invalid_ ut_avlTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node, _Inout_opt_ void *arg), _Inout_opt_ void *arg)
{
    ut_avlNode_t *n = tree->root;
    tree->root = NULL;
    if (freefun) {
        treedestroy_arg (td, n, freefun, arg);
    }
}

static void augment (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlNode_t *n)
{
    td->augment (onode_from_node (td, n), conode_from_node (td, n->cs[0]), conode_from_node (td, n->cs[1]));
}

static ut_avlNode_t *rotate_single (const ut_avlTreedef_t *td, ut_avlNode_t **pnode, ut_avlNode_t *node, int dir)
{
    /* rotate_single(N, dir) performs one rotation, e.g., for dir=1, a
       right rotation as depicted below, for dir=0 a left rotation:

                     N                      _ND'
            _ND          v     ==>    u             N'
         u    _ND_D                          _ND_D     v

       Since a right rotation is only ever done with the left side is
       overweight, _ND must be != NULL.  */
    ut_avlNode_t * const parent = node->parent;
    ut_avlNode_t * const node_ND = node->cs[1-dir];
    ut_avlNode_t * const node_ND_D = node_ND->cs[dir];
    assert (node_ND);
    node_ND->cs[dir] = node;
    node_ND->parent = parent;
    node->parent = node_ND;
    node->cs[1-dir] = node_ND_D;
    if (node_ND_D) {
        node_ND_D->parent = node;
    }
    node->height = node_ND_D ? (1 + node_ND_D->height) : 1;
    node_ND->height = node->height + 1;
    *pnode = node_ND;
    if (td->augment) {
        augment (td, node);
        augment (td, node_ND);
    }
    return parent;
}

static ut_avlNode_t *rotate_double (const ut_avlTreedef_t *td, ut_avlNode_t **pnode, ut_avlNode_t *node, int dir)
{
    /* rotate_double() performs one double rotation, which is slightly
       faster when written out than the equivalent:

         rotate_single(N->cs[1-dir], 1-dir)
         rotate_single(N, dir)

       A right double rotation therefore means:

                    N                    N'            _ND_D''
            _ND         v  =>    _ND_D'     v  =>   _ND''     N''
         u    _ND_D            _ND'    y           u    x    y   v
             x     y         u     x

       Since a right double rotation is only ever done with the N is
       overweight on the left side and _ND is overweight on the right
       side, both _ND and _ND_D must be != NULL. */
    ut_avlNode_t * const parent = node->parent;
    ut_avlNode_t * const node_ND = node->cs[1-dir];
    ut_avlNode_t * const node_ND_D = node_ND->cs[dir];
    assert (node_ND);
    assert (node_ND_D);
    node_ND->cs[dir] = node_ND_D->cs[1-dir];
    if (node_ND->cs[dir]) {
        node_ND->cs[dir]->parent = node_ND;
    }
    node->cs[1-dir] = node_ND_D->cs[dir];
    if (node->cs[1-dir]) {
        node->cs[1-dir]->parent = node;
    }
    node_ND_D->cs[1-dir] = node_ND;
    node_ND_D->cs[dir] = node;
    node_ND->parent = node_ND_D;
    node->parent = node_ND_D;
    node_ND_D->parent = parent;
    *pnode = node_ND_D;
    {
        const int h = node_ND->height;
        node->height = node_ND_D->height;
        node_ND->height = node_ND_D->height;
        node_ND_D->height = h;
    }
    if (td->augment) {
        augment (td, node);
        augment (td, node_ND);
        augment (td, node_ND_D);
    }
    return parent;
}

static ut_avlNode_t *rotate (const ut_avlTreedef_t *td, ut_avlNode_t **pnode, ut_avlNode_t *node, int dir)
{
    /* _D => child in the direction of rotation (1 for right, 0 for
       left); _ND => child in the opposite direction.  So in a right
       rotation, _ND_D means the grandchild that is the right child of
       the left child. */
    ut_avlNode_t * const node_ND = node->cs[1-dir];
    assert (node_ND != NULL);
    ut_avlNode_t * const node_ND_ND = node_ND->cs[1-dir];
    ut_avlNode_t * const node_ND_D = node_ND->cs[dir];
    int height_ND_ND, height_ND_D;
    assert (dir == !!dir);
    height_ND_ND = node_ND_ND ? node_ND_ND->height : 0;
    height_ND_D = node_ND_D ? node_ND_D->height : 0;
    if (height_ND_ND < height_ND_D) {
        return rotate_double (td, pnode, node, dir);
    } else {
        return rotate_single (td, pnode, node, dir);
    }
}

static ut_avlNode_t *rebalance_one (const ut_avlTreedef_t *td, ut_avlNode_t **pnode, ut_avlNode_t *node)
{
    ut_avlNode_t *node_L = node->cs[0];
    ut_avlNode_t *node_R = node->cs[1];
    int height_L = node_L ? node_L->height : 0;
    int height_R = node_R ? node_R->height : 0;
    if (height_L > height_R + 1) {
        return rotate (td, pnode, node, 1);
    } else if (height_L < height_R - 1) {
        return rotate (td, pnode, node, 0);
    } else {
        /* Rebalance happens only on insert & delete, and augment needs to
           be called during rotations.  Therefore, rebalancing integrates
           calling augment, and because of that, includes running all the
           way up to the root even when not needed for the rebalancing
           itself. */
        int height = (height_L < height_R ? height_R : height_L) + 1;
        if (td->augment == 0 && height == node->height) {
            return NULL;
        } else {
            node->height = height;
            if (td->augment) {
                augment (td, node);
            }
            return node->parent;
        }
    }
}

static void rebalance_path (const ut_avlTreedef_t *td, ut_avlPath_t *path, ut_avlNode_t *node)
{
    while (node) {
        assert (*path->pnode[path->depth] == node);
        node = rebalance_one (td, path->pnode[path->depth], node);
        path->depth--;
    }
}

static ut_avlNode_t **nodeptr_from_node (ut_avlTree_t *tree, ut_avlNode_t *node)
{
    ut_avlNode_t *parent = node->parent;
    return (parent == NULL) ? &tree->root
        : (node == parent->cs[0]) ? &parent->cs[0]
        : &parent->cs[1];
}

static void rebalance_nopath (const ut_avlTreedef_t *td, ut_avlTree_t *tree, ut_avlNode_t *node)
{
    while (node) {
        ut_avlNode_t **pnode = nodeptr_from_node (tree, node);
        node = rebalance_one (td, pnode, node);
    }
}

_Check_return_ _Ret_maybenull_ void *ut_avlLookup (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key)
{
    const ut_avlNode_t *cursor = tree->root;
    int c;
    while (cursor && (c = comparenk (td, cursor, key)) != 0) {
        const int dir = (c <= 0);
        cursor = cursor->cs[dir];
    }
    return (void *) conode_from_node (td, cursor);
}

static const ut_avlNode_t *lookup_path (_In_ const ut_avlTreedef_t *td, const ut_avlTree_t *tree, const void *key, ut_avlPath_t *path)
{
    const ut_avlNode_t *cursor = tree->root;
    const ut_avlNode_t *prev = NULL;
    int c;
    path->depth = 0;
    path->pnode[0] = (ut_avlNode_t **) &tree->root;
    while (cursor && (c = comparenk (td, cursor, key)) != 0) {
        const int dir = (c <= 0);
        prev = cursor;
        path->pnode[++path->depth] = (ut_avlNode_t **) &cursor->cs[dir];
        cursor = cursor->cs[dir];
    }
    path->pnodeidx = path->depth;
    path->parent = (ut_avlNode_t *) prev;
    return cursor;
}

_Ret_maybenull_ void *ut_avlLookupDPath (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key, _Out_ ut_avlDPath_t *path)
{
    const ut_avlNode_t *node = lookup_path (td, tree, key, &path->p);
    return (void *) conode_from_node (td, node);
}

_Ret_maybenull_ void *ut_avlLookupIPath (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key, _Out_ ut_avlIPath_t *path)
{
    const ut_avlNode_t *node = lookup_path (td, tree, key, &path->p);
    /* If no duplicates allowed, path may not be used for insertion,
       and hence there is no need to continue descending the tree.
       Since we can invalidate it very efficiently, do so. */
    if (node) {
        if (!(td->flags & UT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
            path->p.pnode[path->p.depth] = NULL;
        } else {
            const ut_avlNode_t *cursor = node;
            const ut_avlNode_t *prev;
            int c, dir;
            do {
                c = comparenk (td, cursor, key);
                dir = (c <= 0);
                prev = cursor;
                path->p.pnode[++path->p.depth] = (ut_avlNode_t **) &cursor->cs[dir];
                cursor = cursor->cs[dir];
            } while (cursor);
            path->p.parent = (ut_avlNode_t *) prev;
        }
    }
    return (void *) conode_from_node (td, node);
}

void ut_avlInsertIPath (_In_ const ut_avlTreedef_t *td, ut_avlTree_t *tree, _Inout_ void *vnode, _Inout_ _Post_invalid_ ut_avlIPath_t *path)
{
    ut_avlNode_t *node = node_from_onode (td, vnode);
    (void) tree;
    node->cs[0] = NULL;
    node->cs[1] = NULL;
    node->parent = path->p.parent;
    node->height = 1;
    if (td->augment) {
        augment (td, node);
    }
    assert (path->p.pnode[path->p.depth]);
    assert ((*path->p.pnode[path->p.depth]) == NULL);
    *path->p.pnode[path->p.depth] = node;
    path->p.depth--;
    rebalance_path (td, &path->p, node->parent);
}

void ut_avlInsert (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *vnode)
{
    const void *node = cnode_from_onode (td, vnode);
    const void *key;
    ut_avlIPath_t path;
    if (td->flags & UT_AVL_TREEDEF_FLAG_INDKEY) {
        key = LOAD_INDKEY (node, td);
    } else {
        key = LOAD_DIRKEY (node, td);
    }
    ut_avlLookupIPath (td, tree, key, &path);
    ut_avlInsertIPath (td, tree, vnode, &path);
}

static void delete_generic (_In_ const ut_avlTreedef_t *td, ut_avlTree_t *tree, void *vnode, ut_avlDPath_t *path)
{
    ut_avlNode_t *node = node_from_onode (td, vnode);
    ut_avlNode_t **pnode;
    ut_avlNode_t *whence;

    if (path) {
        assert (tree == NULL);
        pnode = path->p.pnode[path->p.pnodeidx];
    } else {
        pnode = nodeptr_from_node (tree, node);
    }

    if (node->cs[0] == NULL)
    {
        if (node->cs[1]) {
            node->cs[1]->parent = node->parent;
        }
        *pnode = node->cs[1];
        whence = node->parent;
    }
    else if (node->cs[1] == NULL)
    {
        node->cs[0]->parent = node->parent;
        *pnode = node->cs[0];
        whence = node->parent;
    }
    else
    {
        ut_avlNode_t *subst;

        assert (node->cs[0] != NULL);
        assert (node->cs[1] != NULL);

        /* Use predecessor as substitute */
        if (path) {
            path->p.pnode[++path->p.depth] = &node->cs[0];
        }
        subst = node->cs[0];
        if (subst->cs[1] == NULL) {
            whence = subst;
        } else {
            do {
                if (path) {
                    path->p.pnode[++path->p.depth] = &subst->cs[1];
                }
                subst = subst->cs[1];
            } while (subst->cs[1]);
            whence = subst->parent;

            whence->cs[1] = subst->cs[0];
            if (whence->cs[1]) {
                whence->cs[1]->parent = whence;
            }
            subst->cs[0] = node->cs[0];
            if (subst->cs[0]) {
                subst->cs[0]->parent = subst;
            }
            if (path) {
                path->p.pnode[path->p.pnodeidx+1] = &subst->cs[0];
            }
        }

        subst->parent = node->parent;
        subst->height = node->height;
        subst->cs[1] = node->cs[1];
        if (subst->cs[1]) {
            subst->cs[1]->parent = subst;
        }
        *pnode = subst;
    }

    if (td->augment && whence) {
        augment (td, whence);
    }
    if (path) {
        path->p.depth--;
        rebalance_path (td, &path->p, whence);
    } else {
        rebalance_nopath (td, tree, whence);
    }
}

void ut_avlDeleteDPath (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *vnode, _Inout_ _Post_invalid_ ut_avlDPath_t *path)
{
    (void) tree;
    delete_generic (td, NULL, vnode, path);
}

void ut_avlDelete (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *vnode)
{
    delete_generic (td, tree, vnode, NULL);
}

void ut_avlSwapNode (_In_ const ut_avlTreedef_t *td, _Inout_ ut_avlTree_t *tree, _Inout_ void *vold, _Inout_ void *vnew)
{
    ut_avlNode_t *old = node_from_onode (td, vold);
    ut_avlNode_t *new = node_from_onode (td, vnew);
    *nodeptr_from_node (tree, old) = new;
    /* use memmove so partially overlap between old & new is allowed
       (yikes!, but exploited by the memory allocator, and not a big
       deal to get right) */
    memmove (new, old, sizeof (*new));
    if (new->cs[0]) {
        new->cs[0]->parent = new;
    }
    if (new->cs[1]) {
        new->cs[1]->parent = new;
    }
    if (td->augment) {
        augment (td, new);
    }
}

static ut_avlNode_t *find_neighbour (const ut_avlNode_t *n, int dir)
{
    /* dir = 0 => pred; dir = 1 = succ */
    if (n->cs[dir]) {
        n = n->cs[dir];
        while (n->cs[1-dir]) {
            n = n->cs[1-dir];
        }
        return (ut_avlNode_t *) n;
    } else {
        const ut_avlNode_t *p = n->parent;
        while (p && n == p->cs[dir]) {
            n = p;
            p = p->parent;
        }
        return (ut_avlNode_t *) p;
    }
}

static ut_avlNode_t *find_extremum (const ut_avlTree_t *tree, int dir)
{
    const ut_avlNode_t *n = tree->root;
    if (n) {
        while (n->cs[dir]) {
            n = n->cs[dir];
        }
    }
    return (ut_avlNode_t *) n;
}

_Check_return_ _Ret_maybenull_ void *ut_avlFindMin (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree)
{
    return (void *) conode_from_node (td, find_extremum (tree, 0));
}

_Check_return_ _Ret_maybenull_ void *ut_avlFindMax (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree)
{
    return (void *) conode_from_node (td, find_extremum (tree, 1));
}

_Check_return_ _Ret_maybenull_ void *ut_avlFindPred (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_opt_ const void *vnode)
{
    const ut_avlNode_t *n = cnode_from_onode (td, vnode);
    if (n == NULL) {
        return ut_avlFindMax (td, tree);
    } else {
        return (void *) conode_from_node (td, find_neighbour (n, 0));
    }
}

_Check_return_ _Ret_maybenull_ void *ut_avlFindSucc (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_opt_ const void *vnode)
{
    const ut_avlNode_t *n = cnode_from_onode (td, vnode);
    if (n == NULL) {
        return ut_avlFindMin (td, tree);
    } else {
        return (void *) conode_from_node (td, find_neighbour (n, 1));
    }
}

static void avl_iter_downleft (ut_avlIter_t *iter)
{
    if (*iter->todop) {
        ut_avlNode_t *n;
        n = (*iter->todop)->cs[0];
        while (n) {
            assert ((int) (iter->todop - iter->todo) < (int) (sizeof (iter->todo) / sizeof (*iter->todo)));
            *++iter->todop = n;
            n = n->cs[0];
        }
        iter->right = (*iter->todop)->cs[1];
    }
}

_Check_return_ _Ret_maybenull_ void *ut_avlIterFirst (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter)
{
    iter->td = td;
    iter->todop = iter->todo+1;
    *iter->todop = (ut_avlNode_t *) tree->root;
    avl_iter_downleft (iter);
    return onode_from_node (td, *iter->todop);
}

_Check_return_ _Ret_maybenull_ void *ut_avlIterNext (_Inout_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter)
{
    if (iter->todop-- > iter->todo+1 && iter->right == NULL) {
        iter->right = (*iter->todop)->cs[1];
    } else {
        assert ((int) (iter->todop - iter->todo) < (int) (sizeof (iter->todo) / sizeof (*iter->todo)));
        *++iter->todop = iter->right;
        avl_iter_downleft (iter);
    }
    return onode_from_node (iter->td, *iter->todop);
}

void ut_avlWalk (_In_ const ut_avlTreedef_t *td, _In_ ut_avlTree_t *tree, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)
{
    const ut_avlNode_t *todo[1+UT_AVL_MAX_TREEHEIGHT];
    const ut_avlNode_t **todop = todo+1;
    *todop = tree->root;
    while (*todop) {
        ut_avlNode_t *right, *n;
        /* First locate the minimum value in this subtree */
        n = (*todop)->cs[0];
        while (n) {
            *++todop = n;
            n = n->cs[0];
        }
        /* Then process it and its parents until a node N is hit that has
           a right subtree, with (by definition) key values in between N
           and the parent of N */
        do {
            right = (*todop)->cs[1];
            f ((void *) conode_from_node (td, *todop), a);
        } while (todop-- > todo+1 && right == NULL);
        /* Continue with right subtree rooted at 'right' before processing
           the parent node of the last node processed in the loop above */
        *++todop = right;
    }
}

void ut_avlConstWalk (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a)
{
    ut_avlWalk (td, (ut_avlTree_t *) tree, (ut_avlWalk_t) f, a);
}

int ut_avlIsEmpty (_In_ const ut_avlTree_t *tree)
{
    return tree->root == NULL;
}

int ut_avlIsSingleton (_In_ const ut_avlTree_t *tree)
{
    int r = (tree->root && tree->root->height == 1);
    assert (!r || (tree->root->cs[0] == NULL && tree->root->cs[1] == NULL));
    return r;
}

void ut_avlAugmentUpdate (_In_ const ut_avlTreedef_t *td, _Inout_ void *vnode)
{
    if (td->augment) {
        ut_avlNode_t *node = node_from_onode (td, vnode);
        while (node) {
            augment (td, node);
            node = node->parent;
        }
    }
}

static const ut_avlNode_t *fixup_predsucceq (_In_ const ut_avlTreedef_t *td, const void *key, const ut_avlNode_t *tmp, const ut_avlNode_t *cand, int dir)
{
    if (tmp == NULL) {
        return cand;
    } else if (!(td->flags & UT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
        return tmp;
    } else {
        /* key exists - but it there's no guarantee we hit the first
           one in the in-order enumeration of the tree */
        cand = tmp;
        tmp = tmp->cs[1-dir];
        while (tmp) {
            if (comparenk (td, tmp, key) != 0) {
                tmp = tmp->cs[dir];
            } else {
                cand = tmp;
                tmp = tmp->cs[1-dir];
            }
        }
        return cand;
    }
}

static const ut_avlNode_t *lookup_predeq (_In_ const ut_avlTreedef_t *td, const ut_avlTree_t *tree, const void *key)
{
    const ut_avlNode_t *tmp = tree->root;
    const ut_avlNode_t *cand = NULL;
    int c;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c < 0) {
            cand = tmp;
            tmp = tmp->cs[1];
        } else {
            tmp = tmp->cs[0];
        }
    }
    return fixup_predsucceq (td, key, tmp, cand, 0);
}

static const ut_avlNode_t *lookup_succeq (_In_ const ut_avlTreedef_t *td, const ut_avlTree_t *tree, const void *key)
{
    const ut_avlNode_t *tmp = tree->root;
    const ut_avlNode_t *cand = NULL;
    int c;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c > 0) {
            cand = tmp;
            tmp = tmp->cs[0];
        } else {
            tmp = tmp->cs[1];
        }
    }
    return fixup_predsucceq (td, key, tmp, cand, 1);
}

static const ut_avlNode_t *fixup_predsucc (_In_ const ut_avlTreedef_t *td, const void *key, const ut_avlNode_t *tmp, const ut_avlNode_t *cand, int dir)
{
    /* dir=0: pred, dir=1: succ */
    if (tmp == NULL || tmp->cs[dir] == NULL) {
        return cand;
    } else if (!(td->flags & UT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
        /* No duplicates, therefore the extremum in the subtree */
        tmp = tmp->cs[dir];
        while (tmp->cs[1-dir]) {
            tmp = tmp->cs[1-dir];
        }
        return tmp;
    } else {
        /* Duplicates allowed, therefore: if tmp has no right subtree,
           cand else scan the right subtree for the minimum larger
           than key, return cand if none can be found */
        tmp = tmp->cs[dir];
        while (tmp) {
            if (comparenk (td, tmp, key) != 0) {
                cand = tmp;
                tmp = tmp->cs[1-dir];
            } else {
                tmp = tmp->cs[dir];
            }
        }
        return cand;
    }
}

static const ut_avlNode_t *lookup_pred (_In_ const ut_avlTreedef_t *td, const ut_avlTree_t *tree, const void *key)
{
    const ut_avlNode_t *tmp = tree->root;
    const ut_avlNode_t *cand = NULL;
    int c;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c < 0) {
            cand = tmp;
            tmp = tmp->cs[1];
        } else {
            tmp = tmp->cs[0];
        }
    }
    return fixup_predsucc (td, key, tmp, cand, 0);
}

static const ut_avlNode_t *lookup_succ (_In_ const ut_avlTreedef_t *td, const ut_avlTree_t *tree, const void *key)
{
    const ut_avlNode_t *tmp = tree->root;
    const ut_avlNode_t *cand = NULL;
    int c;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c > 0) {
            cand = tmp;
            tmp = tmp->cs[0];
        } else {
            tmp = tmp->cs[1];
        }
    }
    return fixup_predsucc (td, key, tmp, cand, 1);
}

_Check_return_ _Ret_maybenull_ void *ut_avlLookupSuccEq (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key)
{
    return (void *) conode_from_node (td, lookup_succeq (td, tree, key));
}

_Check_return_ _Ret_maybenull_ void *ut_avlLookupPredEq (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key)
{
    return (void *) conode_from_node (td, lookup_predeq (td, tree, key));
}

_Check_return_ _Ret_maybenull_ void *ut_avlLookupSucc (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key)
{
    return (void *) conode_from_node (td, lookup_succ (td, tree, key));
}

_Check_return_ _Ret_maybenull_ void *ut_avlLookupPred (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *key)
{
    return (void *) conode_from_node (td, lookup_pred (td, tree, key));
}

_Check_return_ _Ret_maybenull_ void *ut_avlIterSuccEq (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter, _In_ const void *key)
{
    const ut_avlNode_t *tmp = tree->root;
    int c;
    iter->td = td;
    iter->todop = iter->todo;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c > 0) {
            *++iter->todop = (ut_avlNode_t *) tmp;
            tmp = tmp->cs[0];
        } else {
            tmp = tmp->cs[1];
        }
    }
    if (tmp != NULL) {
        *++iter->todop = (ut_avlNode_t *) tmp;
        if (td->flags & UT_AVL_TREEDEF_FLAG_ALLOWDUPS) {
            /* key exists - but it there's no guarantee we hit the
               first one in the in-order enumeration of the tree */
            tmp = tmp->cs[0];
            while (tmp) {
                if (comparenk (td, tmp, key) != 0) {
                    tmp = tmp->cs[1];
                } else {
                    *++iter->todop = (ut_avlNode_t *) tmp;
                    tmp = tmp->cs[0];
                }
            }
        }
    }
    if (iter->todop == iter->todo) {
        return NULL;
    } else {
        iter->right = (*iter->todop)->cs[1];
        return onode_from_node (td, *iter->todop);
    }
}

_Check_return_ _Ret_maybenull_ void *ut_avlIterSucc (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlIter_t *iter, _In_ const void *key)
{
    const ut_avlNode_t *tmp = tree->root;
    int c;
    iter->td = td;
    iter->todop = iter->todo;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c > 0) {
            *++iter->todop = (ut_avlNode_t *) tmp;
            tmp = tmp->cs[0];
        } else {
            tmp = tmp->cs[1];
        }
    }
    if (tmp != NULL) {
      if (!(td->flags & UT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
            tmp = tmp->cs[1];
            if (tmp) {
                do {
                    *++iter->todop = (ut_avlNode_t *) tmp;
                    tmp = tmp->cs[0];
                } while (tmp);
            }
        } else {
            tmp = tmp->cs[1];
            while (tmp) {
                if (comparenk (td, tmp, key) != 0) {
                    *++iter->todop = (ut_avlNode_t *) tmp;
                    tmp = tmp->cs[0];
                } else {
                    tmp = tmp->cs[1];
                }
            }
        }
    }
    if (iter->todop == iter->todo) {
        return NULL;
    } else {
        iter->right = (*iter->todop)->cs[1];
        return onode_from_node (td, *iter->todop);
    }
}

void ut_avlWalkRange (_In_ const ut_avlTreedef_t *td, _In_ ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)
{
    ut_avlNode_t *n, *nn;
    n = (ut_avlNode_t *) lookup_succeq (td, tree, min);
    while (n && comparenk (td, n, max) <= 0) {
        nn = find_neighbour (n, 1);
        f (onode_from_node (td, n), a);
        n = nn;
    }
}

void ut_avlConstWalkRange (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a)
{
    ut_avlWalkRange (td, (ut_avlTree_t *) tree, min, max, (ut_avlWalk_t) f, a);
}

void ut_avlWalkRangeReverse (_In_ const ut_avlTreedef_t *td, ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)
{
    ut_avlNode_t *n, *nn;
    n = (ut_avlNode_t *) lookup_predeq (td, tree, max);
    while (n && comparenk (td, n, min) >= 0) {
        nn = find_neighbour (n, 0);
        f (onode_from_node (td, n), a);
        n = nn;
    }
}

void ut_avlConstWalkRangeReverse (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a)
{
    ut_avlWalkRangeReverse (td, (ut_avlTree_t *) tree, min, max, (ut_avlWalk_t) f, a);
}

_Check_return_ _Ret_maybenull_ void *ut_avlRoot (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree)
{
    return (void *) conode_from_node (td, tree->root);
}

_Ret_notnull_ void *ut_avlRootNonEmpty (_In_ const ut_avlTreedef_t *td, _In_ const ut_avlTree_t *tree)
{
    assert (tree->root);
    return (void *) conode_from_node (td, tree->root);
}

/**************************************************************************************
 ****
 ****  Wrappers for counting trees
 ****
 **************************************************************************************/

void ut_avlCTreedefInit (_Out_ ut_avlCTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_t comparekk, _In_opt_ ut_avlAugment_t augment, uint32_t flags)
{
    treedef_init_common (&td->t, avlnodeoffset, keyoffset, augment, flags);
    td->t.u.comparekk = comparekk;
}

void ut_avlCTreedefInit_r (_Out_ ut_avlCTreedef_t *td, size_t avlnodeoffset, size_t keyoffset, _In_ ut_avlCompare_r_t comparekk_r, _Inout_opt_ void *cmp_arg, _In_opt_ ut_avlAugment_t augment, uint32_t flags)
{
    treedef_init_common (&td->t, avlnodeoffset, keyoffset, augment, flags | UT_AVL_TREEDEF_FLAG_R);
    td->t.cmp_arg = cmp_arg;
    td->t.u.comparekk_r = comparekk_r;
}

void ut_avlCInit (_In_ const ut_avlCTreedef_t *td, _Out_ ut_avlCTree_t *tree)
{
    ut_avlInit (&td->t, &tree->t);
    tree->count = 0;
}

void ut_avlCFree (_In_ const ut_avlCTreedef_t *td, _Inout_ _Post_invalid_ ut_avlCTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node))
{
    tree->count = 0;
    ut_avlFree (&td->t, &tree->t, freefun);
}

void ut_avlCFreeArg (_In_ const ut_avlCTreedef_t *td, _Inout_ _Post_invalid_ ut_avlCTree_t *tree, _In_opt_ void (*freefun) (_Inout_ void *node, _Inout_opt_ void *arg), _Inout_opt_ void *arg)
{
    tree->count = 0;
    ut_avlFreeArg (&td->t, &tree->t, freefun, arg);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCRoot (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree)
{
    return ut_avlRoot (&td->t, &tree->t);
}

_Ret_notnull_  void *ut_avlCRootNonEmpty (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree)
{
    return ut_avlRootNonEmpty (&td->t, &tree->t);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCLookup (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key)
{
    return ut_avlLookup (&td->t, &tree->t, key);
}

_Ret_maybenull_ void *ut_avlCLookupIPath (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key, _Out_ ut_avlIPath_t *path)
{
    return ut_avlLookupIPath (&td->t, &tree->t, key, path);
}

_Ret_maybenull_ void *ut_avlCLookupDPath (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key, _Out_ ut_avlDPath_t *path)
{
    return ut_avlLookupDPath (&td->t, &tree->t, key, path);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCLookupPredEq (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key)
{
    return ut_avlLookupPredEq (&td->t, &tree->t, key);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCLookupSuccEq (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key)
{
    return ut_avlLookupSuccEq (&td->t, &tree->t, key);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCLookupPred (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key)
{
    return ut_avlLookupPred (&td->t, &tree->t, key);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCLookupSucc (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *key)
{
    return ut_avlLookupSucc (&td->t, &tree->t, key);
}

void ut_avlCInsert (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node)
{
    tree->count++;
    ut_avlInsert (&td->t, &tree->t, node);
}

void ut_avlCDelete (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node)
{
    assert (tree->count > 0);
    tree->count--;
    ut_avlDelete (&td->t, &tree->t, node);
}

void ut_avlCInsertIPath (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node, _Inout_ _Post_invalid_ ut_avlIPath_t *path)
{
    tree->count++;
    ut_avlInsertIPath (&td->t, &tree->t, node, path);
}

void ut_avlCDeleteDPath (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *node, _Inout_ _Post_invalid_ ut_avlDPath_t *path)
{
    assert (tree->count > 0);
    tree->count--;
    ut_avlDeleteDPath (&td->t, &tree->t, node, path);
}

void ut_avlCSwapNode (_In_ const ut_avlCTreedef_t *td, _Inout_ ut_avlCTree_t *tree, _Inout_ void *old, _Inout_ void *new)
{
    ut_avlSwapNode (&td->t, &tree->t, old, new);
}

void ut_avlCAugmentUpdate (_In_ const ut_avlCTreedef_t *td, _Inout_ void *node)
{
    ut_avlAugmentUpdate (&td->t, node);
}

int ut_avlCIsEmpty (_In_ const ut_avlCTree_t *tree)
{
    return ut_avlIsEmpty (&tree->t);
}

int ut_avlCIsSingleton (_In_ const ut_avlCTree_t *tree)
{
    return ut_avlIsSingleton (&tree->t);
}

size_t ut_avlCCount (_In_ const ut_avlCTree_t *tree)
{
    return tree->count;
}

_Check_return_ _Ret_maybenull_ void *ut_avlCFindMin (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree)
{
    return ut_avlFindMin (&td->t, &tree->t);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCFindMax (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree)
{
    return ut_avlFindMax (&td->t, &tree->t);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCFindPred (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *vnode)
{
    return ut_avlFindPred (&td->t, &tree->t, vnode);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCFindSucc (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *vnode)
{
    return ut_avlFindSucc (&td->t, &tree->t, vnode);
}

void ut_avlCWalk (_In_ const ut_avlCTreedef_t *td, _In_ ut_avlCTree_t *tree, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)
{
    ut_avlWalk (&td->t, &tree->t, f, a);
}

void ut_avlCConstWalk (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a)
{
    ut_avlConstWalk (&td->t, &tree->t, f, a);
}

void ut_avlCWalkRange (_In_ const ut_avlCTreedef_t *td, _In_ ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)
{
    ut_avlWalkRange (&td->t, &tree->t, min, max, f, a);
}

void ut_avlCConstWalkRange (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a)
{
    ut_avlConstWalkRange (&td->t, &tree->t, min, max, f, a);
}

void ut_avlCWalkRangeReverse (_In_ const ut_avlCTreedef_t *td, _In_ ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlWalk_t f, _Inout_opt_ void *a)
{
    ut_avlWalkRangeReverse (&td->t, &tree->t, min, max, f, a);
}

void ut_avlCConstWalkRangeReverse (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _In_ const void *min, _In_ const void *max, _In_ ut_avlConstWalk_t f, _Inout_opt_ void *a)
{
    ut_avlConstWalkRangeReverse (&td->t, &tree->t, min, max, f, a);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCIterFirst (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter)
{
    return ut_avlIterFirst (&td->t, &tree->t, &iter->t);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCIterSuccEq (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter, _In_ const void *key)
{
    return ut_avlIterSuccEq (&td->t, &tree->t, &iter->t, key);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCIterSucc (_In_ const ut_avlCTreedef_t *td, _In_ const ut_avlCTree_t *tree, _Out_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter, _In_ const void *key)
{
    return ut_avlIterSucc (&td->t, &tree->t, &iter->t, key);
}

_Check_return_ _Ret_maybenull_ void *ut_avlCIterNext (_Inout_ _When_ (return == 0, _Post_invalid_) ut_avlCIter_t *iter)
{
    /* Added this in-between t variable to satisfy SAL. */
    ut_avlIter_t *t = &(iter->t);
    return ut_avlIterNext(t);
}
