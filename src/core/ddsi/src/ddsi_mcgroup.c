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
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include "os/os.h"
#include "os/os_atomics.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_mcgroup.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "util/ut_avl.h"

struct nn_group_membership_node {
  ut_avlNode_t avlnode;
  ddsi_tran_conn_t conn;
  nn_locator_t srcloc;
  nn_locator_t mcloc;
  unsigned count;
};

struct nn_group_membership {
  os_mutex lock;
  ut_avlTree_t mships;
};

static int locator_compare_no_port (const nn_locator_t *as, const nn_locator_t *bs)
{
  if (as->kind != bs->kind)
    return (as->kind < bs->kind) ? -1 : 1;
  else
    return memcmp (as->address, bs->address, 16);
}

static int cmp_group_membership (const void *va, const void *vb)
{
  const struct nn_group_membership_node *a = va;
  const struct nn_group_membership_node *b = vb;
  int c;
  if (a->conn < b->conn)
    return -1;
  else if (a->conn > b->conn)
    return 1;
  else if ((c = locator_compare_no_port (&a->srcloc, &b->srcloc)) != 0)
    return c;
  else if ((c = locator_compare_no_port (&a->mcloc, &b->mcloc)) != 0)
    return c;
  else
    return 0;
}

static ut_avlTreedef_t mship_td = UT_AVL_TREEDEF_INITIALIZER(offsetof (struct nn_group_membership_node, avlnode), 0, cmp_group_membership, 0);

struct nn_group_membership *new_group_membership (void)
{
  struct nn_group_membership *mship = os_malloc (sizeof (*mship));
  os_mutexInit (&mship->lock);
  ut_avlInit (&mship_td, &mship->mships);
  return mship;
}

void free_group_membership (struct nn_group_membership *mship)
{
  ut_avlFree (&mship_td, &mship->mships, os_free);
  os_mutexDestroy (&mship->lock);
  os_free (mship);
}

static int reg_group_membership (struct nn_group_membership *mship, ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc)
{
  struct nn_group_membership_node key, *n;
  ut_avlIPath_t ip;
  int isnew;
  key.conn = conn;
  if (srcloc)
    key.srcloc = *srcloc;
  else
    memset (&key.srcloc, 0, sizeof (key.srcloc));
  key.mcloc = *mcloc;
  if ((n = ut_avlLookupIPath (&mship_td, &mship->mships, &key, &ip)) != NULL) {
    isnew = 0;
    n->count++;
  } else {
    isnew = 1;
    n = os_malloc (sizeof (*n));
    n->conn = conn;
    n->srcloc = key.srcloc;
    n->mcloc = key.mcloc;
    n->count = 1;
    ut_avlInsertIPath (&mship_td, &mship->mships, n, &ip);
  }
  return isnew;
}

static int unreg_group_membership (struct nn_group_membership *mship, ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc)
{
  struct nn_group_membership_node key, *n;
  ut_avlDPath_t dp;
  int mustdel;
  key.conn = conn;
  if (srcloc)
    key.srcloc = *srcloc;
  else
    memset (&key.srcloc, 0, sizeof (key.srcloc));
  key.mcloc = *mcloc;
  n = ut_avlLookupDPath (&mship_td, &mship->mships, &key, &dp);
  assert (n != NULL);
  assert (n->count > 0);
  if (--n->count > 0)
    mustdel = 0;
  else
  {
    mustdel = 1;
    ut_avlDeleteDPath (&mship_td, &mship->mships, n, &dp);
    os_free (n);
  }
  return mustdel;
}

static char *make_joinleave_msg (char *buf, size_t bufsz, ddsi_tran_conn_t conn, int join, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf, int err)
{
  char mcstr[DDSI_LOCSTRLEN], interfstr[DDSI_LOCSTRLEN];
  char srcstr[DDSI_LOCSTRLEN] = { '*', '\0' };
  int n;
#ifdef DDSI_INCLUDE_SSM
  if (srcloc) {
    ddsi_locator_to_string_no_port(srcstr, sizeof(srcstr), srcloc);
  }
#else
  OS_UNUSED_ARG (srcloc);
#endif
  ddsi_locator_to_string_no_port (mcstr, sizeof(mcstr), mcloc);
  if (interf)
    ddsi_locator_to_string_no_port(interfstr, sizeof(interfstr), &interf->loc);
  else
    (void) snprintf (interfstr, sizeof (interfstr), "(default)");
  n = err ? snprintf (buf, bufsz, "error %d in ", err) : 0;
  if ((size_t) n  < bufsz)
    snprintf (buf + n, bufsz - (size_t) n, "%s conn %p for (%s, %s) interface %s", join ? "join" : "leave", (void *) conn, mcstr, srcstr, interfstr);
  return buf;
}

static int joinleave_mcgroup (ddsi_tran_conn_t conn, int join, const nn_locator_t *srcloc, const nn_locator_t *mcloc, const struct nn_interface *interf)
{
  char buf[256];
  int err;
  DDS_TRACE("%s\n", make_joinleave_msg (buf, sizeof(buf), conn, join, srcloc, mcloc, interf, 0));
  if (join)
    err = ddsi_conn_join_mc(conn, srcloc, mcloc, interf);
  else
    err = ddsi_conn_leave_mc(conn, srcloc, mcloc, interf);
  if (err)
    DDS_WARNING("%s\n", make_joinleave_msg (buf, sizeof(buf), conn, join, srcloc, mcloc, interf, err));
  return err ? -1 : 0;
}

static int interface_in_recvips_p (const struct nn_interface *interf)
{
  struct config_in_addr_node *nodeaddr;
  for (nodeaddr = gv.recvips; nodeaddr; nodeaddr = nodeaddr->next)
  {
    if (locator_compare_no_port(&nodeaddr->loc, &interf->loc) == 0)
      return 1;
  }
  return 0;
}

static int joinleave_mcgroups (ddsi_tran_conn_t conn, int join, const nn_locator_t *srcloc, const nn_locator_t *mcloc)
{
  int rc;
  switch (gv.recvips_mode)
  {
    case RECVIPS_MODE_NONE:
      break;
    case RECVIPS_MODE_ANY:
      /* User has specified to use the OS default interface */
      if ((rc = joinleave_mcgroup (conn, join, srcloc, mcloc, NULL)) < 0)
        return rc;
      break;
    case RECVIPS_MODE_PREFERRED:
      if (gv.interfaces[gv.selected_interface].mc_capable)
        return joinleave_mcgroup (conn, join, srcloc, mcloc, &gv.interfaces[gv.selected_interface]);
      return 0;
    case RECVIPS_MODE_ALL:
    case RECVIPS_MODE_SOME:
    {
      int i, fails = 0, oks = 0;
      for (i = 0; i < gv.n_interfaces; i++)
      {
        if (gv.interfaces[i].mc_capable)
        {
          if (gv.recvips_mode == RECVIPS_MODE_ALL || interface_in_recvips_p (&gv.interfaces[i]))
          {
            if ((rc = joinleave_mcgroup (conn, join, srcloc, mcloc, &gv.interfaces[i])) < 0)
              fails++;
            else
              oks++;
          }
        }
      }
      if (fails > 0)
      {
        if (oks > 0)
          DDS_TRACE("multicast join failed for some but not all interfaces, proceeding\n");
        else
          return -2;
      }
    }
      break;
  }
  return 0;
}

int ddsi_join_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc)
{
  int ret;
  os_mutexLock (&gv.mship->lock);
  if (!reg_group_membership (gv.mship, conn, srcloc, mcloc))
  {
    char buf[256];
    DDS_TRACE("%s: already joined\n", make_joinleave_msg (buf, sizeof(buf), conn, 1, srcloc, mcloc, NULL, 0));
    ret = 0;
  }
  else
  {
    ret = joinleave_mcgroups (conn, 1, srcloc, mcloc);
  }
  os_mutexUnlock (&gv.mship->lock);
  return ret;
}

int ddsi_leave_mc (ddsi_tran_conn_t conn, const nn_locator_t *srcloc, const nn_locator_t *mcloc)
{
  int ret;
  os_mutexLock (&gv.mship->lock);
  if (!unreg_group_membership (gv.mship, conn, srcloc, mcloc))
  {
    char buf[256];
    DDS_TRACE("%s: not leaving yet\n", make_joinleave_msg (buf, sizeof(buf), conn, 0, srcloc, mcloc, NULL, 0));
    ret = 0;
  }
  else
  {
    ret = joinleave_mcgroups (conn, 0, srcloc, mcloc);
  }
  os_mutexUnlock (&gv.mship->lock);
  return ret;
}

void ddsi_transfer_group_membership (ddsi_tran_conn_t conn, ddsi_tran_conn_t newconn)
{
  struct nn_group_membership_node *n, min, max;
  memset(&min, 0, sizeof(min));
  memset(&max, 0xff, sizeof(max));
  min.conn = max.conn = conn;
  /* ordering is on socket, then src IP, then mc IP; IP compare checks family first and AF_INET, AF_INET6
   are neither 0 nor maximum representable, min and max define the range of key values that relate to
   oldsock */
  os_mutexLock (&gv.mship->lock);
  n = ut_avlLookupSuccEq (&mship_td, &gv.mship->mships, &min);
  while (n != NULL && cmp_group_membership (n, &max) <= 0)
  {
    struct nn_group_membership_node * const nn = ut_avlFindSucc (&mship_td, &gv.mship->mships, n);
    ut_avlDelete (&mship_td, &gv.mship->mships, n);
    n->conn = newconn;
    ut_avlInsert (&mship_td, &gv.mship->mships, n);
    n = nn;
  }
  os_mutexUnlock (&gv.mship->lock);
}

int ddsi_rejoin_transferred_mcgroups (ddsi_tran_conn_t conn)
{
  struct nn_group_membership_node *n, min, max;
  ut_avlIter_t it;
  int ret = 0;
  memset(&min, 0, sizeof(min));
  memset(&max, 0xff, sizeof(max));
  min.conn = max.conn = conn;
  os_mutexLock (&gv.mship->lock);
  for (n = ut_avlIterSuccEq (&mship_td, &gv.mship->mships, &it, &min); n != NULL && ret >= 0 && cmp_group_membership(n, &max) <= 0; n = ut_avlIterNext (&it))
  {
    int have_srcloc = (memcmp(&n->srcloc, &min.srcloc, sizeof(n->srcloc)) != 0);
    assert (n->conn == conn);
    ret = joinleave_mcgroups (conn, 1, have_srcloc ? &n->srcloc : NULL, &n->mcloc);
  }
  os_mutexUnlock (&gv.mship->lock);
  return ret;
}
