// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <limits.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/avl.h"

#define LOAD_DIRKEY(avlnode, tree) (((char *) (avlnode)) - (tree)->avlnodeoffset + (tree)->keyoffset)
#define LOAD_INDKEY(avlnode, tree) (*((char **) (((char *) (avlnode)) - (tree)->avlnodeoffset + (tree)->keyoffset)))

static int comparenk (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *a, const void *b)
{
    const void *ka;
    if (td->flags & DDSRT_AVL_TREEDEF_FLAG_INDKEY) {
        ka = LOAD_INDKEY (a, td);
    } else {
        ka = LOAD_DIRKEY (a, td);
    }
    if (td->flags & DDSRT_AVL_TREEDEF_FLAG_R) {
        return td->u.comparekk_r (ka, b, td->cmp_arg);
    } else {
        return td->u.comparekk (ka, b);
    }
}

#if 0
static int comparenn_direct (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *a, const ddsrt_avl_node_t *b)
{
    return td->comparekk (LOAD_DIRKEY (a, td), LOAD_DIRKEY (b, td));
}

static int comparenn_indirect (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *a, const ddsrt_avl_node_t *b)
{
    return td->comparekk (LOAD_INDKEY (a, td), LOAD_INDKEY (b, td));
}

static int comparenn (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *a, const ddsrt_avl_node_t *b)
{
    if (IS_INDKEY (td->keyoffset)) {
        return comparenn_indirect (td, a, b);
    } else {
        return comparenn_direct (td, a, b);
    }
}
#endif

#if 0
static ddsrt_avl_node_t *node_from_onode (const ddsrt_avl_treedef_t *td, char *onode)
{
    if (onode == NULL) {
        return NULL;
    } else {
        return (ddsrt_avl_node_t *) (onode + td->avlnodeoffset);
    }
}
#endif

static const ddsrt_avl_node_t *cnode_from_onode (const ddsrt_avl_treedef_t *td, const char *onode)
{
    if (onode == NULL) {
        return NULL;
    } else {
        return (const ddsrt_avl_node_t *) (onode + td->avlnodeoffset);
    }
}

static char *onode_from_node (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *node)
{
    if (node == NULL) {
        return NULL;
    } else {
        return (char *) node - td->avlnodeoffset;
    }
}

static const char *conode_from_node (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *node)
{
    if (node == NULL) {
        return NULL;
    } else {
        return (const char *) node - td->avlnodeoffset;
    }
}

static ddsrt_avl_node_t *node_from_onode_nonnull (const ddsrt_avl_treedef_t *td, char *onode) ddsrt_nonnull_all ddsrt_attribute_returns_nonnull;
static ddsrt_avl_node_t *node_from_onode_nonnull (const ddsrt_avl_treedef_t *td, char *onode)
{
  return (ddsrt_avl_node_t *) (onode + td->avlnodeoffset);
}

static const ddsrt_avl_node_t *cnode_from_onode_nonnull (const ddsrt_avl_treedef_t *td, const char *onode) ddsrt_nonnull_all ddsrt_attribute_returns_nonnull;
static const ddsrt_avl_node_t *cnode_from_onode_nonnull (const ddsrt_avl_treedef_t *td, const char *onode)
{
  return (const ddsrt_avl_node_t *) (onode + td->avlnodeoffset);
}

static char *onode_from_node_nonnull (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *node) ddsrt_nonnull_all ddsrt_attribute_returns_nonnull;
static char *onode_from_node_nonnull (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *node)
{
  return (char *) node - td->avlnodeoffset;
}

static const char *conode_from_node_nonnull (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *node) ddsrt_nonnull_all ddsrt_attribute_returns_nonnull;
static const char *conode_from_node_nonnull (const ddsrt_avl_treedef_t *td, const ddsrt_avl_node_t *node)
{
  return (const char *) node - td->avlnodeoffset;
}

static void treedef_init_common (ddsrt_avl_treedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_augment_t augment, uint32_t flags)
{
    assert (avlnodeoffset <= 0x7fffffff);
    assert (keyoffset <= 0x7fffffff);
    td->avlnodeoffset = avlnodeoffset;
    td->keyoffset = keyoffset;
    td->augment = augment;
    td->flags = flags;
}

void ddsrt_avl_treedef_init (ddsrt_avl_treedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_t comparekk, ddsrt_avl_augment_t augment, uint32_t flags)
{
    treedef_init_common (td, avlnodeoffset, keyoffset, augment, flags);
    td->u.comparekk = comparekk;
}

void ddsrt_avl_treedef_init_r (ddsrt_avl_treedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_r_t comparekk_r, void *cmp_arg, ddsrt_avl_augment_t augment, uint32_t flags)
{
    treedef_init_common (td, avlnodeoffset, keyoffset, augment, flags | DDSRT_AVL_TREEDEF_FLAG_R);
    td->cmp_arg = cmp_arg;
    td->u.comparekk_r = comparekk_r;
}

void ddsrt_avl_init (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree)
{
    tree->root = NULL;
    (void) td;
}

static void treedestroy (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *n, void (*freefun) (void *node))
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

static void treedestroy_arg (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *n, void (*freefun) (void *node, void *arg), void *arg)
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

void ddsrt_avl_free (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void (*freefun) (void *node))
{
    ddsrt_avl_node_t *n = tree->root;
    tree->root = NULL;
    if (freefun) {
        treedestroy (td, n, freefun);
    }
}

void ddsrt_avl_free_arg (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void (*freefun) (void *node, void *arg), void *arg)
{
    ddsrt_avl_node_t *n = tree->root;
    tree->root = NULL;
    if (freefun) {
        treedestroy_arg (td, n, freefun, arg);
    }
}

static void augment (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *n) ddsrt_nonnull_all;
static void augment (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t *n)
{
    td->augment (onode_from_node_nonnull (td, n), conode_from_node (td, n->cs[0]), conode_from_node (td, n->cs[1]));
}

static ddsrt_avl_node_t *rotate_single (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node, int dir) ddsrt_nonnull_all;
static ddsrt_avl_node_t *rotate_single (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node, int dir)
{
    /* rotate_single(N, dir) performs one rotation, e.g., for dir=1, a
       right rotation as depicted below, for dir=0 a left rotation:

                     N                      _ND'
            _ND          v     ==>    u             N'
         u    _ND_D                          _ND_D     v

       Since a right rotation is only ever done with the left side is
       overweight, _ND must be != NULL.  */
    ddsrt_avl_node_t * const parent = node->parent;
    ddsrt_avl_node_t * const node_ND = node->cs[1-dir];
    ddsrt_avl_node_t * const node_ND_D = node_ND->cs[dir];
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

static ddsrt_avl_node_t *rotate_double (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node, int dir) ddsrt_nonnull_all;
static ddsrt_avl_node_t *rotate_double (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node, int dir)
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
    ddsrt_avl_node_t * const parent = node->parent;
    ddsrt_avl_node_t * const node_ND = node->cs[1-dir];
    ddsrt_avl_node_t * const node_ND_D = node_ND->cs[dir];
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

static ddsrt_avl_node_t *rotate (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node, int dir) ddsrt_nonnull_all;
static ddsrt_avl_node_t *rotate (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node, int dir)
{
    /* _D => child in the direction of rotation (1 for right, 0 for
       left); _ND => child in the opposite direction.  So in a right
       rotation, _ND_D means the grandchild that is the right child of
       the left child. */
    ddsrt_avl_node_t * const node_ND = node->cs[1-dir];
    assert (node_ND != NULL);
    ddsrt_avl_node_t * const node_ND_ND = node_ND->cs[1-dir];
    ddsrt_avl_node_t * const node_ND_D = node_ND->cs[dir];
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

static ddsrt_avl_node_t *rebalance_one (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node) ddsrt_nonnull_all;
static ddsrt_avl_node_t *rebalance_one (const ddsrt_avl_treedef_t *td, ddsrt_avl_node_t **pnode, ddsrt_avl_node_t *node)
{
    ddsrt_avl_node_t *node_L = node->cs[0];
    ddsrt_avl_node_t *node_R = node->cs[1];
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

static ddsrt_avl_node_t **nodeptr_from_node (ddsrt_avl_tree_t *tree, ddsrt_avl_node_t *node) ddsrt_nonnull ((1, 2));
static void rebalance_path (const ddsrt_avl_treedef_t *td, ddsrt_avl_path_t *path, ddsrt_avl_node_t *node)
{
    while (node) {
        assert (*path->pnode[path->depth] == node);
        node = rebalance_one (td, path->pnode[path->depth], node);
        path->depth--;
    }
}

static ddsrt_avl_node_t **nodeptr_from_node (ddsrt_avl_tree_t *tree, ddsrt_avl_node_t *node) ddsrt_nonnull_all;
static ddsrt_avl_node_t **nodeptr_from_node (ddsrt_avl_tree_t *tree, ddsrt_avl_node_t *node)
{
    ddsrt_avl_node_t *parent = node->parent;
    return (parent == NULL) ? &tree->root
        : (node == parent->cs[0]) ? &parent->cs[0]
        : &parent->cs[1];
}

static void rebalance_nopath (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, ddsrt_avl_node_t *node) ddsrt_nonnull ((1, 2));
static void rebalance_nopath (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, ddsrt_avl_node_t *node)
{
    while (node) {
        ddsrt_avl_node_t **pnode = nodeptr_from_node (tree, node);
        node = rebalance_one (td, pnode, node);
    }
}

void *ddsrt_avl_lookup (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    const ddsrt_avl_node_t *cursor = tree->root;
    int c;
    while (cursor && (c = comparenk (td, cursor, key)) != 0) {
        const int dir = (c <= 0);
        cursor = cursor->cs[dir];
    }
    return (void *) conode_from_node (td, cursor);
}

static const ddsrt_avl_node_t *lookup_path (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key, ddsrt_avl_path_t *path) ddsrt_nonnull_all;
static const ddsrt_avl_node_t *lookup_path (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key, ddsrt_avl_path_t *path)
{
    const ddsrt_avl_node_t *cursor = tree->root;
    const ddsrt_avl_node_t *prev = NULL;
    int c;
    path->depth = 0;
    path->pnode[0] = (ddsrt_avl_node_t **) &tree->root;
    while (cursor && (c = comparenk (td, cursor, key)) != 0) {
        const int dir = (c <= 0);
        prev = cursor;
        path->pnode[++path->depth] = (ddsrt_avl_node_t **) &cursor->cs[dir];
        cursor = cursor->cs[dir];
    }
    path->pnodeidx = path->depth;
    path->parent = (ddsrt_avl_node_t *) prev;
    return cursor;
}

void *ddsrt_avl_lookup_dpath (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key, ddsrt_avl_dpath_t *path)
{
    const ddsrt_avl_node_t *node = lookup_path (td, tree, key, &path->p);
    return (void *) conode_from_node (td, node);
}

void *ddsrt_avl_lookup_ipath (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key, ddsrt_avl_ipath_t *path)
{
    const ddsrt_avl_node_t *node = lookup_path (td, tree, key, &path->p);
    /* If no duplicates allowed, path may not be used for insertion,
       and hence there is no need to continue descending the tree.
       Since we can invalidate it very efficiently, do so. */
    if (node) {
        if (!(td->flags & DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
            path->p.pnode[path->p.depth] = NULL;
        } else {
            const ddsrt_avl_node_t *cursor = node;
            const ddsrt_avl_node_t *prev;
            int c, dir;
            do {
                c = comparenk (td, cursor, key);
                dir = (c <= 0);
                prev = cursor;
                path->p.pnode[++path->p.depth] = (ddsrt_avl_node_t **) &cursor->cs[dir];
                cursor = cursor->cs[dir];
            } while (cursor);
            path->p.parent = (ddsrt_avl_node_t *) prev;
        }
    }
    return (void *) conode_from_node (td, node);
}

void ddsrt_avl_insert_ipath (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vnode, ddsrt_avl_ipath_t *path)
{
    ddsrt_avl_node_t *node = node_from_onode_nonnull (td, vnode);
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

void ddsrt_avl_insert (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vnode)
{
    const void *node = cnode_from_onode_nonnull (td, vnode);
    const void *key;
    ddsrt_avl_ipath_t path;
    if (td->flags & DDSRT_AVL_TREEDEF_FLAG_INDKEY) {
        key = LOAD_INDKEY (node, td);
    } else {
        key = LOAD_DIRKEY (node, td);
    }
    ddsrt_avl_lookup_ipath (td, tree, key, &path);
    ddsrt_avl_insert_ipath (td, tree, vnode, &path);
}

static void delete_generic (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vnode, ddsrt_avl_dpath_t *path) ddsrt_nonnull ((1, 3));
static void delete_generic (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vnode, ddsrt_avl_dpath_t *path)
{
    ddsrt_avl_node_t *node = node_from_onode_nonnull (td, vnode);
    ddsrt_avl_node_t **pnode;
    ddsrt_avl_node_t *whence;

    if (path) {
        assert (tree == NULL);
        pnode = path->p.pnode[path->p.pnodeidx];
    } else {
        assert (tree != NULL);
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
        ddsrt_avl_node_t *subst;

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

void ddsrt_avl_delete_dpath (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vnode, ddsrt_avl_dpath_t *path)
{
    (void) tree;
    delete_generic (td, NULL, vnode, path);
}

void ddsrt_avl_delete (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vnode)
{
    delete_generic (td, tree, vnode, NULL);
}

void ddsrt_avl_swap_node (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, void *vold, void *vnew)
{
    ddsrt_avl_node_t *old = node_from_onode_nonnull (td, vold);
    ddsrt_avl_node_t *new = node_from_onode_nonnull (td, vnew);
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

static ddsrt_avl_node_t *find_neighbour (const ddsrt_avl_node_t *n, int dir)
{
    /* dir = 0 => pred; dir = 1 = succ */
    if (n->cs[dir]) {
        n = n->cs[dir];
        while (n->cs[1-dir]) {
            n = n->cs[1-dir];
        }
        return (ddsrt_avl_node_t *) n;
    } else {
        const ddsrt_avl_node_t *p = n->parent;
        while (p && n == p->cs[dir]) {
            n = p;
            p = p->parent;
        }
        return (ddsrt_avl_node_t *) p;
    }
}

static ddsrt_avl_node_t *find_extremum (const ddsrt_avl_tree_t *tree, int dir)
{
    const ddsrt_avl_node_t *n = tree->root;
    if (n) {
        while (n->cs[dir]) {
            n = n->cs[dir];
        }
    }
    return (ddsrt_avl_node_t *) n;
}

void *ddsrt_avl_find_min (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree)
{
    return (void *) conode_from_node (td, find_extremum (tree, 0));
}

void *ddsrt_avl_find_max (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree)
{
    return (void *) conode_from_node (td, find_extremum (tree, 1));
}

void *ddsrt_avl_find_pred (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *vnode)
{
    const ddsrt_avl_node_t *n = cnode_from_onode (td, vnode);
    if (n == NULL) {
        return ddsrt_avl_find_max (td, tree);
    } else {
        return (void *) conode_from_node (td, find_neighbour (n, 0));
    }
}

void *ddsrt_avl_find_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *vnode)
{
    const ddsrt_avl_node_t *n = cnode_from_onode (td, vnode);
    if (n == NULL) {
        return ddsrt_avl_find_min (td, tree);
    } else {
        return (void *) conode_from_node (td, find_neighbour (n, 1));
    }
}

static void avl_iter_downleft (ddsrt_avl_iter_t *iter)
{
    if (*iter->todop) {
        ddsrt_avl_node_t *n;
        n = (*iter->todop)->cs[0];
        while (n) {
            assert ((int) (iter->todop - iter->todo) < (int) (sizeof (iter->todo) / sizeof (*iter->todo)));
            *++iter->todop = n;
            n = n->cs[0];
        }
        iter->right = (*iter->todop)->cs[1];
    }
}

void *ddsrt_avl_iter_first (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_iter_t *iter)
{
    iter->td = td;
    iter->todop = iter->todo+1;
    *iter->todop = (ddsrt_avl_node_t *) tree->root;
    avl_iter_downleft (iter);
    return onode_from_node (td, *iter->todop);
}

void *ddsrt_avl_iter_next (ddsrt_avl_iter_t *iter)
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

void ddsrt_avl_walk (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, ddsrt_avl_walk_t f, void *a)
{
    const ddsrt_avl_node_t *todo[1+DDSRT_AVL_MAX_TREEHEIGHT];
    const ddsrt_avl_node_t **todop = todo+1;
    *todop = tree->root;
    while (*todop) {
        ddsrt_avl_node_t *right, *n;
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
            f ((void *) conode_from_node_nonnull (td, *todop), a);
        } while (todop-- > todo+1 && right == NULL);
        /* Continue with right subtree rooted at 'right' before processing
           the parent node of the last node processed in the loop above */
        *++todop = right;
    }
}

void ddsrt_avl_const_walk (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_const_walk_t f, void *a)
{
    ddsrt_avl_walk (td, (ddsrt_avl_tree_t *) tree, (ddsrt_avl_walk_t) f, a);
}

int ddsrt_avl_is_empty (const ddsrt_avl_tree_t *tree)
{
    return tree->root == NULL;
}

int ddsrt_avl_is_singleton (const ddsrt_avl_tree_t *tree)
{
    int r = (tree->root && tree->root->height == 1);
    assert (!r || (tree->root->cs[0] == NULL && tree->root->cs[1] == NULL));
    return r;
}

void ddsrt_avl_augment_update (const ddsrt_avl_treedef_t *td, void *vnode)
{
    if (td->augment) {
        ddsrt_avl_node_t *node = node_from_onode_nonnull (td, vnode);
        while (node) {
            augment (td, node);
            node = node->parent;
        }
    }
}

static const ddsrt_avl_node_t *fixup_predsucceq (const ddsrt_avl_treedef_t *td, const void *key, const ddsrt_avl_node_t *tmp, const ddsrt_avl_node_t *cand, int dir)
{
    if (tmp == NULL) {
        return cand;
    } else if (!(td->flags & DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
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

static const ddsrt_avl_node_t *lookup_predeq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;
static const ddsrt_avl_node_t *lookup_predeq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    const ddsrt_avl_node_t *tmp = tree->root;
    const ddsrt_avl_node_t *cand = NULL;
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

static const ddsrt_avl_node_t *lookup_succeq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;
static const ddsrt_avl_node_t *lookup_succeq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    const ddsrt_avl_node_t *tmp = tree->root;
    const ddsrt_avl_node_t *cand = NULL;
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

static const ddsrt_avl_node_t *fixup_predsucc (const ddsrt_avl_treedef_t *td, const void *key, const ddsrt_avl_node_t *tmp, const ddsrt_avl_node_t *cand, int dir)
{
    /* dir=0: pred, dir=1: succ */
    if (tmp == NULL || tmp->cs[dir] == NULL) {
        return cand;
    } else if (!(td->flags & DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
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

static const ddsrt_avl_node_t *lookup_pred (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;
static const ddsrt_avl_node_t *lookup_pred (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    const ddsrt_avl_node_t *tmp = tree->root;
    const ddsrt_avl_node_t *cand = NULL;
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

static const ddsrt_avl_node_t *lookup_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key) ddsrt_nonnull_all;
static const ddsrt_avl_node_t *lookup_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    const ddsrt_avl_node_t *tmp = tree->root;
    const ddsrt_avl_node_t *cand = NULL;
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

void *ddsrt_avl_lookup_succ_eq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    return (void *) conode_from_node (td, lookup_succeq (td, tree, key));
}

void *ddsrt_avl_lookup_pred_eq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    return (void *) conode_from_node (td, lookup_predeq (td, tree, key));
}

void *ddsrt_avl_lookup_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    return (void *) conode_from_node (td, lookup_succ (td, tree, key));
}

void *ddsrt_avl_lookup_pred (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *key)
{
    return (void *) conode_from_node (td, lookup_pred (td, tree, key));
}

void *ddsrt_avl_iter_succ_eq (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_iter_t *iter, const void *key)
{
    const ddsrt_avl_node_t *tmp = tree->root;
    int c;
    iter->td = td;
    iter->todop = iter->todo;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c > 0) {
            *++iter->todop = (ddsrt_avl_node_t *) tmp;
            tmp = tmp->cs[0];
        } else {
            tmp = tmp->cs[1];
        }
    }
    if (tmp != NULL) {
        *++iter->todop = (ddsrt_avl_node_t *) tmp;
        if (td->flags & DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS) {
            /* key exists - but it there's no guarantee we hit the
               first one in the in-order enumeration of the tree */
            tmp = tmp->cs[0];
            while (tmp) {
                if (comparenk (td, tmp, key) != 0) {
                    tmp = tmp->cs[1];
                } else {
                    *++iter->todop = (ddsrt_avl_node_t *) tmp;
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

void *ddsrt_avl_iter_succ (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, ddsrt_avl_iter_t *iter, const void *key)
{
    const ddsrt_avl_node_t *tmp = tree->root;
    int c;
    iter->td = td;
    iter->todop = iter->todo;
    while (tmp && (c = comparenk (td, tmp, key)) != 0) {
        if (c > 0) {
            *++iter->todop = (ddsrt_avl_node_t *) tmp;
            tmp = tmp->cs[0];
        } else {
            tmp = tmp->cs[1];
        }
    }
    if (tmp != NULL) {
      if (!(td->flags & DDSRT_AVL_TREEDEF_FLAG_ALLOWDUPS)) {
            tmp = tmp->cs[1];
            if (tmp) {
                do {
                    *++iter->todop = (ddsrt_avl_node_t *) tmp;
                    tmp = tmp->cs[0];
                } while (tmp);
            }
        } else {
            tmp = tmp->cs[1];
            while (tmp) {
                if (comparenk (td, tmp, key) != 0) {
                    *++iter->todop = (ddsrt_avl_node_t *) tmp;
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

void ddsrt_avl_walk_range (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a)
{
    ddsrt_avl_node_t *n, *nn;
    n = (ddsrt_avl_node_t *) lookup_succeq (td, tree, min);
    while (n && comparenk (td, n, max) <= 0) {
        nn = find_neighbour (n, 1);
        f (onode_from_node_nonnull (td, n), a);
        n = nn;
    }
}

void ddsrt_avl_const_walk_range (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a)
{
    ddsrt_avl_walk_range (td, (ddsrt_avl_tree_t *) tree, min, max, (ddsrt_avl_walk_t) f, a);
}

void ddsrt_avl_walk_range_reverse (const ddsrt_avl_treedef_t *td, ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a)
{
    ddsrt_avl_node_t *n, *nn;
    n = (ddsrt_avl_node_t *) lookup_predeq (td, tree, max);
    while (n && comparenk (td, n, min) >= 0) {
        nn = find_neighbour (n, 0);
        f (onode_from_node_nonnull (td, n), a);
        n = nn;
    }
}

void ddsrt_avl_const_walk_range_reverse (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a)
{
    ddsrt_avl_walk_range_reverse (td, (ddsrt_avl_tree_t *) tree, min, max, (ddsrt_avl_walk_t) f, a);
}

void *ddsrt_avl_root (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree)
{
    return (void *) conode_from_node (td, tree->root);
}

void *ddsrt_avl_root_non_empty (const ddsrt_avl_treedef_t *td, const ddsrt_avl_tree_t *tree)
{
    assert (tree->root);
    return (void *) conode_from_node_nonnull (td, tree->root);
}

/**************************************************************************************
 ****
 ****  Wrappers for counting trees
 ****
 **************************************************************************************/

void ddsrt_avl_ctreedef_init (ddsrt_avl_ctreedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_t comparekk, ddsrt_avl_augment_t augment, uint32_t flags)
{
    treedef_init_common (&td->t, avlnodeoffset, keyoffset, augment, flags);
    td->t.u.comparekk = comparekk;
}

void ddsrt_avl_ctreedef_init_r (ddsrt_avl_ctreedef_t *td, size_t avlnodeoffset, size_t keyoffset, ddsrt_avl_compare_r_t comparekk_r, void *cmp_arg, ddsrt_avl_augment_t augment, uint32_t flags)
{
    treedef_init_common (&td->t, avlnodeoffset, keyoffset, augment, flags | DDSRT_AVL_TREEDEF_FLAG_R);
    td->t.cmp_arg = cmp_arg;
    td->t.u.comparekk_r = comparekk_r;
}

void ddsrt_avl_cinit (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree)
{
    ddsrt_avl_init (&td->t, &tree->t);
    tree->count = 0;
}

void ddsrt_avl_cfree (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void (*freefun) (void *node))
{
    tree->count = 0;
    ddsrt_avl_free (&td->t, &tree->t, freefun);
}

void ddsrt_avl_cfree_arg (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void (*freefun) (void *node, void *arg), void *arg)
{
    tree->count = 0;
    ddsrt_avl_free_arg (&td->t, &tree->t, freefun, arg);
}

void *ddsrt_avl_croot (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree)
{
    return ddsrt_avl_root (&td->t, &tree->t);
}

void *ddsrt_avl_croot_non_empty (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree)
{
    return ddsrt_avl_root_non_empty (&td->t, &tree->t);
}

void *ddsrt_avl_clookup (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key)
{
    return ddsrt_avl_lookup (&td->t, &tree->t, key);
}

void *ddsrt_avl_clookup_ipath (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key, ddsrt_avl_ipath_t *path)
{
    return ddsrt_avl_lookup_ipath (&td->t, &tree->t, key, path);
}

void *ddsrt_avl_clookup_dpath (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key, ddsrt_avl_dpath_t *path)
{
    return ddsrt_avl_lookup_dpath (&td->t, &tree->t, key, path);
}

void *ddsrt_avl_clookup_pred_eq (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key)
{
    return ddsrt_avl_lookup_pred_eq (&td->t, &tree->t, key);
}

void *ddsrt_avl_clookup_succ_eq (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key)
{
    return ddsrt_avl_lookup_succ_eq (&td->t, &tree->t, key);
}

void *ddsrt_avl_clookup_pred (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key)
{
    return ddsrt_avl_lookup_pred (&td->t, &tree->t, key);
}

void *ddsrt_avl_clookup_succ (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *key)
{
    return ddsrt_avl_lookup_succ (&td->t, &tree->t, key);
}

void ddsrt_avl_cinsert (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node)
{
    tree->count++;
    ddsrt_avl_insert (&td->t, &tree->t, node);
}

void ddsrt_avl_cdelete (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node)
{
    assert (tree->count > 0);
    tree->count--;
    ddsrt_avl_delete (&td->t, &tree->t, node);
}

void ddsrt_avl_cinsert_ipath (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node, ddsrt_avl_ipath_t *path)
{
    tree->count++;
    ddsrt_avl_insert_ipath (&td->t, &tree->t, node, path);
}

void ddsrt_avl_cdelete_dpath (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *node, ddsrt_avl_dpath_t *path)
{
    assert (tree->count > 0);
    tree->count--;
    ddsrt_avl_delete_dpath (&td->t, &tree->t, node, path);
}

void ddsrt_avl_cswap_node (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, void *old, void *new)
{
    ddsrt_avl_swap_node (&td->t, &tree->t, old, new);
}

void ddsrt_avl_caugment_update (const ddsrt_avl_ctreedef_t *td, void *node)
{
    ddsrt_avl_augment_update (&td->t, node);
}

int ddsrt_avl_cis_empty (const ddsrt_avl_ctree_t *tree)
{
    return ddsrt_avl_is_empty (&tree->t);
}

int ddsrt_avl_cis_singleton (const ddsrt_avl_ctree_t *tree)
{
    return ddsrt_avl_is_singleton (&tree->t);
}

size_t ddsrt_avl_ccount (const ddsrt_avl_ctree_t *tree)
{
    return tree->count;
}

void *ddsrt_avl_cfind_min (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree)
{
    return ddsrt_avl_find_min (&td->t, &tree->t);
}

void *ddsrt_avl_cfind_max (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree)
{
    return ddsrt_avl_find_max (&td->t, &tree->t);
}

void *ddsrt_avl_cfind_pred (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *vnode)
{
    return ddsrt_avl_find_pred (&td->t, &tree->t, vnode);
}

void *ddsrt_avl_cfind_succ (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *vnode)
{
    return ddsrt_avl_find_succ (&td->t, &tree->t, vnode);
}

void ddsrt_avl_cwalk (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, ddsrt_avl_walk_t f, void *a)
{
    ddsrt_avl_walk (&td->t, &tree->t, f, a);
}

void ddsrt_avl_cconst_walk (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_const_walk_t f, void *a)
{
    ddsrt_avl_const_walk (&td->t, &tree->t, f, a);
}

void ddsrt_avl_cwalk_range (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a)
{
    ddsrt_avl_walk_range (&td->t, &tree->t, min, max, f, a);
}

void ddsrt_avl_cconst_walk_range (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a)
{
    ddsrt_avl_const_walk_range (&td->t, &tree->t, min, max, f, a);
}

void ddsrt_avl_cwalk_range_reverse (const ddsrt_avl_ctreedef_t *td, ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_walk_t f, void *a)
{
    ddsrt_avl_walk_range_reverse (&td->t, &tree->t, min, max, f, a);
}

void ddsrt_avl_cconst_walk_range_reverse (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, const void *min, const void *max, ddsrt_avl_const_walk_t f, void *a)
{
    ddsrt_avl_const_walk_range_reverse (&td->t, &tree->t, min, max, f, a);
}

void *ddsrt_avl_citer_first (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_citer_t *iter)
{
    return ddsrt_avl_iter_first (&td->t, &tree->t, &iter->t);
}

void *ddsrt_avl_citer_succ_eq (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_citer_t *iter, const void *key)
{
    return ddsrt_avl_iter_succ_eq (&td->t, &tree->t, &iter->t, key);
}

void *ddsrt_avl_citer_succ (const ddsrt_avl_ctreedef_t *td, const ddsrt_avl_ctree_t *tree, ddsrt_avl_citer_t *iter, const void *key)
{
    return ddsrt_avl_iter_succ (&td->t, &tree->t, &iter->t, key);
}

void *ddsrt_avl_citer_next (ddsrt_avl_citer_t *iter)
{
    /* Added this in-between t variable to satisfy SAL. */
    ddsrt_avl_iter_t *t = &(iter->t);
    return ddsrt_avl_iter_next(t);
}
