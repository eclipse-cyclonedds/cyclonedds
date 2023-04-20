// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__tran.h"
#include "ddsi__mcgroup.h"

struct ddsi_mcgroup_membership_node {
  ddsrt_avl_node_t avlnode;
  struct ddsi_tran_conn * conn;
  ddsi_locator_t srcloc;
  ddsi_locator_t mcloc;
  unsigned count;
};

struct ddsi_mcgroup_membership {
  ddsrt_mutex_t lock;
  ddsrt_avl_tree_t mships;
};

static int locator_compare_no_port (const ddsi_locator_t *as, const ddsi_locator_t *bs)
{
  if (as->kind != bs->kind)
    return (as->kind < bs->kind) ? -1 : 1;
  else
    return memcmp (as->address, bs->address, 16);
}

static int cmp_group_membership (const void *va, const void *vb)
{
  const struct ddsi_mcgroup_membership_node *a = va;
  const struct ddsi_mcgroup_membership_node *b = vb;
  int c;

#if !defined(DDSRT_MCGROUP_JOIN_ONCE)
  if (a->conn < b->conn)
    return -1;
  else if (a->conn > b->conn)
    return 1;
#endif
  if ((c = locator_compare_no_port (&a->srcloc, &b->srcloc)) != 0)
    return c;
  else if ((c = locator_compare_no_port (&a->mcloc, &b->mcloc)) != 0)
    return c;
  else
    return 0;
}

static ddsrt_avl_treedef_t mship_td = DDSRT_AVL_TREEDEF_INITIALIZER(offsetof (struct ddsi_mcgroup_membership_node, avlnode), 0, cmp_group_membership, 0);

struct ddsi_mcgroup_membership *ddsi_new_mcgroup_membership (void)
{
  struct ddsi_mcgroup_membership *mship = ddsrt_malloc (sizeof (*mship));
  ddsrt_mutex_init (&mship->lock);
  ddsrt_avl_init (&mship_td, &mship->mships);
  return mship;
}

void ddsi_free_mcgroup_membership (struct ddsi_mcgroup_membership *mship)
{
  ddsrt_avl_free (&mship_td, &mship->mships, ddsrt_free);
  ddsrt_mutex_destroy (&mship->lock);
  ddsrt_free (mship);
}

static int reg_group_membership (struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc)
{
  struct ddsi_mcgroup_membership_node key, *n;
  ddsrt_avl_ipath_t ip;
  int isnew;
  key.conn = conn;
  if (srcloc)
    key.srcloc = *srcloc;
  else
    memset (&key.srcloc, 0, sizeof (key.srcloc));
  key.mcloc = *mcloc;
  if ((n = ddsrt_avl_lookup_ipath (&mship_td, &mship->mships, &key, &ip)) != NULL) {
    isnew = 0;
    n->count++;
  } else {
    isnew = 1;
    n = ddsrt_malloc (sizeof (*n));
    n->conn = conn;
    n->srcloc = key.srcloc;
    n->mcloc = key.mcloc;
    n->count = 1;
    ddsrt_avl_insert_ipath (&mship_td, &mship->mships, n, &ip);
  }
  return isnew;
}

static int unreg_group_membership (struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc)
{
  struct ddsi_mcgroup_membership_node key, *n;
  ddsrt_avl_dpath_t dp;
  int mustdel;
  key.conn = conn;
  if (srcloc)
    key.srcloc = *srcloc;
  else
    memset (&key.srcloc, 0, sizeof (key.srcloc));
  key.mcloc = *mcloc;
  n = ddsrt_avl_lookup_dpath (&mship_td, &mship->mships, &key, &dp);
  assert (n != NULL);
  assert (n->count > 0);
  if (--n->count > 0)
    mustdel = 0;
  else
  {
    mustdel = 1;
    ddsrt_avl_delete_dpath (&mship_td, &mship->mships, n, &dp);
    ddsrt_free (n);
  }
  return mustdel;
}

static char *make_joinleave_msg (char *buf, size_t bufsz, struct ddsi_tran_conn * conn, int join, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf, int err)
{
  char mcstr[DDSI_LOCSTRLEN], interfstr[DDSI_LOCSTRLEN];
  char srcstr[DDSI_LOCSTRLEN] = { '*', '\0' };
  int n;
#ifdef DDS_HAS_SSM
  if (srcloc) {
    ddsi_locator_to_string_no_port(srcstr, sizeof(srcstr), srcloc);
  }
#else
  DDSRT_UNUSED_ARG (srcloc);
#endif
  ddsi_locator_to_string_no_port (mcstr, sizeof(mcstr), mcloc);
  if (interf)
    ddsi_locator_to_string_no_port(interfstr, sizeof(interfstr), &interf->loc);
  else
    (void) snprintf (interfstr, sizeof (interfstr), "(default)");
  n = err ? snprintf (buf, bufsz, "error %d in ", err) : 0;
  if ((size_t) n  < bufsz)
    (void) snprintf (buf + n, bufsz - (size_t) n, "%s conn %p for (%s, %s) interface %s", join ? "join" : "leave", (void *) conn, mcstr, srcstr, interfstr);
  return buf;
}

static int joinleave_mcgroup (struct ddsi_tran_conn * conn, int join, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc, const struct ddsi_network_interface *interf)
{
  if (interf && mcloc->kind != interf->loc.kind)
  {
    // quietly ignore attempts at joining/leaving multicast groups on interfaces of a different kind
    // if interf == NULL, the user selected "any" for the interface, which means the kernel's default
    // which means we can only hope for the best (I don't think "any" is ever, or should ever, be
    // used).
    return 0;
  }
  else
  {
    char buf[256];
    int err;
    DDS_CTRACE(&conn->m_base.gv->logconfig, "%s\n", make_joinleave_msg (buf, sizeof(buf), conn, join, srcloc, mcloc, interf, 0));
    if (join)
      err = ddsi_conn_join_mc(conn, srcloc, mcloc, interf);
    else
      err = ddsi_conn_leave_mc(conn, srcloc, mcloc, interf);
    if (err)
      DDS_CWARNING(&conn->m_base.gv->logconfig, "%s\n", make_joinleave_msg (buf, sizeof(buf), conn, join, srcloc, mcloc, interf, err));
    return err ? -1 : 0;
  }
}

static int interface_in_recvips_p (const struct ddsi_config_in_addr_node *recvips, const struct ddsi_network_interface *interf)
{
  const struct ddsi_config_in_addr_node *nodeaddr;
  for (nodeaddr = recvips; nodeaddr; nodeaddr = nodeaddr->next)
  {
    if (locator_compare_no_port(&nodeaddr->loc, &interf->loc) == 0)
      return 1;
  }
  return 0;
}

static int joinleave_mcgroups (const struct ddsi_domaingv *gv, struct ddsi_tran_conn * conn, int join, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc)
{
  int rc;
  switch (gv->recvips_mode)
  {
    case DDSI_RECVIPS_MODE_NONE:
      break;
    case DDSI_RECVIPS_MODE_ANY:
      /* User has specified to use the OS default interface */
      if ((rc = joinleave_mcgroup (conn, join, srcloc, mcloc, NULL)) < 0)
        return rc;
      break;
    case DDSI_RECVIPS_MODE_ALL:
    case DDSI_RECVIPS_MODE_SOME:
    case DDSI_RECVIPS_MODE_PREFERRED: {
      int i, fails = 0, oks = 0;
      for (i = 0; i < gv->n_interfaces; i++)
      {
        if (gv->interfaces[i].mc_capable)
        {
          if (gv->recvips_mode == DDSI_RECVIPS_MODE_ALL || gv->recvips_mode == DDSI_RECVIPS_MODE_PREFERRED || interface_in_recvips_p (gv->recvips, &gv->interfaces[i]))
          {
            if (joinleave_mcgroup (conn, join, srcloc, mcloc, &gv->interfaces[i]) < 0)
              fails++;
            else
              oks++;
          }
        }
      }
      if (fails > 0)
      {
        if (oks > 0)
          GVTRACE("multicast join failed for some but not all interfaces, proceeding\n");
        else
          return -2;
      }
      break;
    }
  }
  return 0;
}

int ddsi_join_mc (const struct ddsi_domaingv *gv, struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc)
{
  /* FIXME: gv to be reduced; perhaps mship, recvips, interfaces, ownloc should be combined into a single struct */
  int ret;
  ddsrt_mutex_lock (&mship->lock);
  if (!reg_group_membership (mship, conn, srcloc, mcloc))
  {
    char buf[256];
    GVTRACE("%s: already joined\n", make_joinleave_msg (buf, sizeof(buf), conn, 1, srcloc, mcloc, NULL, 0));
    ret = 0;
  }
  else
  {
    ret = joinleave_mcgroups (gv, conn, 1, srcloc, mcloc);
  }
  ddsrt_mutex_unlock (&mship->lock);
  return ret;
}

int ddsi_leave_mc (const struct ddsi_domaingv *gv, struct ddsi_mcgroup_membership *mship, struct ddsi_tran_conn * conn, const ddsi_locator_t *srcloc, const ddsi_locator_t *mcloc)
{
  int ret;
  ddsrt_mutex_lock (&mship->lock);
  if (!unreg_group_membership (mship, conn, srcloc, mcloc))
  {
    char buf[256];
    GVTRACE("%s: not leaving yet\n", make_joinleave_msg (buf, sizeof(buf), conn, 0, srcloc, mcloc, NULL, 0));
    ret = 0;
  }
  else
  {
    ret = joinleave_mcgroups (gv, conn, 0, srcloc, mcloc);
  }
  ddsrt_mutex_unlock (&mship->lock);
  return ret;
}

