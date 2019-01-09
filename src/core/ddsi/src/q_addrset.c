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
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "os/os.h"

#include "util/ut_avl.h"
#include "ddsi/q_log.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_config.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_globals.h" /* gv.mattr */

/* So what does one do with const & mutexes? I need to take lock in a
   pure function just in case some other thread is trying to change
   something. Arguably, that means the thing isn't const; but one
   could just as easily argue that "const" means "this call won't
   change it". If it is globally visible before the call, it may
   change anyway.

   Today, I'm taking the latter interpretation. But all the
   const-discarding casts get moved into LOCK/UNLOCK macros. */
#define LOCK(as) (os_mutexLock (&((struct addrset *) (as))->lock))
#define TRYLOCK(as) (os_mutexTryLock (&((struct addrset *) (as))->lock))
#define UNLOCK(as) (os_mutexUnlock (&((struct addrset *) (as))->lock))

static int compare_locators_vwrap (const void *va, const void *vb);

static const ut_avlCTreedef_t addrset_treedef =
  UT_AVL_CTREEDEF_INITIALIZER (offsetof (struct addrset_node, avlnode), offsetof (struct addrset_node, loc), compare_locators_vwrap, 0);

static int add_addresses_to_addrset_1 (struct addrset *as, const char *ip, int port_mode, const char *msgtag, int req_mc, int mcgen_base, int mcgen_count, int mcgen_idx)
{
  char buf[DDSI_LOCSTRLEN];
  nn_locator_t loc;

  switch (ddsi_locator_from_string(&loc, ip))
  {
    case AFSR_OK:
      break;
    case AFSR_INVALID:
      DDS_ERROR("%s: %s: not a valid address\n", msgtag, ip);
      return -1;
    case AFSR_UNKNOWN:
      DDS_ERROR("%s: %s: unknown address\n", msgtag, ip);
      return -1;
    case AFSR_MISMATCH:
      DDS_ERROR("%s: %s: address family mismatch\n", msgtag, ip);
      return -1;
  }

  if (req_mc && !ddsi_is_mcaddr (&loc))
  {
    DDS_ERROR ("%s: %s: not a multicast address\n", msgtag, ip);
    return -1;
  }

  if (mcgen_base == -1 && mcgen_count == -1 && mcgen_idx == -1)
    ;
  else if (loc.kind == NN_LOCATOR_KIND_UDPv4 && ddsi_is_mcaddr(&loc) && mcgen_base >= 0 && mcgen_count > 0 && mcgen_base + mcgen_count < 28 && mcgen_idx >= 0 && mcgen_idx < mcgen_count)
  {
    nn_udpv4mcgen_address_t x;
    memset(&x, 0, sizeof(x));
    memcpy(&x.ipv4, loc.address + 12, 4);
    x.base = (unsigned char) mcgen_base;
    x.count = (unsigned char) mcgen_count;
    x.idx = (unsigned char) mcgen_idx;
    memset(loc.address, 0, sizeof(loc.address));
    memcpy(loc.address, &x, sizeof(x));
    loc.kind = NN_LOCATOR_KIND_UDPv4MCGEN;
  }
  else
  {
    DDS_ERROR("%s: %s,%d,%d,%d: IPv4 multicast address generator invalid or out of place\n",
              msgtag, ip, mcgen_base, mcgen_count, mcgen_idx);
    return -1;
  }

  if (port_mode >= 0)
  {
    loc.port = (unsigned) port_mode;
    DDS_LOG(DDS_LC_CONFIG, "%s: add %s", msgtag, ddsi_locator_to_string(buf, sizeof(buf), &loc));
    add_to_addrset (as, &loc);
  }
  else
  {
    DDS_LOG(DDS_LC_CONFIG, "%s: add ", msgtag);
    if (!ddsi_is_mcaddr (&loc))
    {
      int i;
      for (i = 0; i <= config.maxAutoParticipantIndex; i++)
      {
        int port = config.port_base + config.port_dg * config.domainId.value + i * config.port_pg + config.port_d1;
        loc.port = (unsigned) port;
        if (i == 0)
          DDS_LOG(DDS_LC_CONFIG, "%s", ddsi_locator_to_string(buf, sizeof(buf), &loc));
        else
          DDS_LOG(DDS_LC_CONFIG, ", :%d", port);
        add_to_addrset (as, &loc);
      }
    }
    else
    {
      int port = port_mode;
      if (port == -1)
        port = config.port_base + config.port_dg * config.domainId.value + config.port_d0;
      loc.port = (unsigned) port;
      DDS_LOG(DDS_LC_CONFIG, "%s", ddsi_locator_to_string(buf, sizeof(buf), &loc));
      add_to_addrset (as, &loc);
    }
  }

  DDS_LOG(DDS_LC_CONFIG, "\n");
  return 0;
}

OS_WARNING_MSVC_OFF(4996);
int add_addresses_to_addrset (struct addrset *as, const char *addrs, int port_mode, const char *msgtag, int req_mc)
{
  /* port_mode: -1  => take from string, if 0 & unicast, add for a range of participant indices;
     port_mode >= 0 => always set port to port_mode
  */
  char *addrs_copy, *ip, *cursor, *a;
  int retval = -1;
  addrs_copy = os_strdup (addrs);
  ip = os_malloc (strlen (addrs) + 1);
  cursor = addrs_copy;
  while ((a = os_strsep (&cursor, ",")) != NULL)
  {
    int port = 0, pos;
    int mcgen_base = -1, mcgen_count = -1, mcgen_idx = -1;
    if (config.transport_selector == TRANS_UDP || config.transport_selector == TRANS_TCP)
    {
      if (port_mode == -1 && sscanf (a, "%[^:]:%d%n", ip, &port, &pos) == 2 && a[pos] == 0)
        ; /* XYZ:PORT */
      else if (sscanf (a, "%[^;];%d;%d;%d%n", ip, &mcgen_base, &mcgen_count, &mcgen_idx, &pos) == 4 && a[pos] == 0)
        port = port_mode; /* XYZ;BASE;COUNT;IDX for IPv4 MC address generators */
      else if (sscanf (a, "%[^:]%n", ip, &pos) == 1 && a[pos] == 0)
        port = port_mode; /* XYZ */
      else { /* XY:Z -- illegal, but conversion routine should flag it */
        strcpy (ip, a);
        port = 0;
      }
    }
    else
    {
      if (port_mode == -1 && sscanf (a, "[%[^]]]:%d%n", ip, &port, &pos) == 2 && a[pos] == 0)
        ; /* [XYZ]:PORT */
      else if (sscanf (a, "[%[^]]]%n", ip, &pos) == 1 && a[pos] == 0)
        port = port_mode; /* [XYZ] */
      else { /* XYZ -- let conversion routines handle errors */
        strcpy (ip, a);
        port = 0;
      }
    }

    if ((port > 0 && port <= 65535) || (port_mode == -1 && port == -1)) {
      if (add_addresses_to_addrset_1 (as, ip, port, msgtag, req_mc, mcgen_base, mcgen_count, mcgen_idx) < 0)
        goto error;
    } else {
      DDS_ERROR("%s: %s: port %d invalid\n", msgtag, a, port);
    }
  }
  retval = 0;
 error:
  os_free (ip);
  os_free (addrs_copy);
  return retval;
}
OS_WARNING_MSVC_ON(4996);

int compare_locators (const nn_locator_t *a, const nn_locator_t *b)
{
  int c;
  if (a->kind != b->kind)
    return (int) (a->kind - b->kind);
  else if ((c = memcmp (a->address, b->address, sizeof (a->address))) != 0)
    return c;
  else
    return (int) (a->port - b->port);
}

static int compare_locators_vwrap (const void *va, const void *vb)
{
  return compare_locators (va, vb);
}

struct addrset *new_addrset (void)
{
  struct addrset *as = os_malloc (sizeof (*as));
  os_atomic_st32 (&as->refc, 1);
  os_mutexInit (&as->lock);
  ut_avlCInit (&addrset_treedef, &as->ucaddrs);
  ut_avlCInit (&addrset_treedef, &as->mcaddrs);
  return as;
}

struct addrset *ref_addrset (struct addrset *as)
{
  if (as != NULL)
  {
    os_atomic_inc32 (&as->refc);
  }
  return as;
}

void unref_addrset (struct addrset *as)
{
  if ((as != NULL) && (os_atomic_dec32_ov (&as->refc) == 1))
  {
    ut_avlCFree (&addrset_treedef, &as->ucaddrs, os_free);
    ut_avlCFree (&addrset_treedef, &as->mcaddrs, os_free);
    os_mutexDestroy (&as->lock);
    os_free (as);
  }
}

void set_unspec_locator (nn_locator_t *loc)
{
  loc->kind = NN_LOCATOR_KIND_INVALID;
  loc->port = NN_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, sizeof (loc->address));
}

int is_unspec_locator (const nn_locator_t *loc)
{
  static const nn_locator_t zloc = { .kind = 0 };
  return (loc->kind == NN_LOCATOR_KIND_INVALID &&
          loc->port == NN_LOCATOR_PORT_INVALID &&
          memcmp (&zloc.address, loc->address, sizeof (zloc.address)) == 0);
}

#ifdef DDSI_INCLUDE_SSM
int addrset_contains_ssm (const struct addrset *as)
{
  struct addrset_node *n;
  ut_avlCIter_t it;
  LOCK (as);
  for (n = ut_avlCIterFirst (&addrset_treedef, &as->mcaddrs, &it); n; n = ut_avlCIterNext (&it))
  {
    if (ddsi_is_ssm_mcaddr (&n->loc))
    {
      UNLOCK (as);
      return 1;
    }
  }
  UNLOCK (as);
  return 0;
}

int addrset_any_ssm (const struct addrset *as, nn_locator_t *dst)
{
  struct addrset_node *n;
  ut_avlCIter_t it;
  LOCK (as);
  for (n = ut_avlCIterFirst (&addrset_treedef, &as->mcaddrs, &it); n; n = ut_avlCIterNext (&it))
  {
    if (ddsi_is_ssm_mcaddr (&n->loc))
    {
      *dst = n->loc;
      UNLOCK (as);
      return 1;
    }
  }
  UNLOCK (as);
  return 0;
}

int addrset_any_non_ssm_mc (const struct addrset *as, nn_locator_t *dst)
{
  struct addrset_node *n;
  ut_avlCIter_t it;
  LOCK (as);
  for (n = ut_avlCIterFirst (&addrset_treedef, &as->mcaddrs, &it); n; n = ut_avlCIterNext (&it))
  {
    if (!ddsi_is_ssm_mcaddr (&n->loc))
    {
      *dst = n->loc;
      UNLOCK (as);
      return 1;
    }
  }
  UNLOCK (as);
  return 0;
}
#endif

int addrset_purge (struct addrset *as)
{
  LOCK (as);
  ut_avlCFree (&addrset_treedef, &as->ucaddrs, os_free);
  ut_avlCFree (&addrset_treedef, &as->mcaddrs, os_free);
  UNLOCK (as);
  return 0;
}

void add_to_addrset (struct addrset *as, const nn_locator_t *loc)
{
  if (!is_unspec_locator (loc))
  {
    ut_avlIPath_t path;
    ut_avlCTree_t *tree = ddsi_is_mcaddr (loc) ? &as->mcaddrs : &as->ucaddrs;
    LOCK (as);
    if (ut_avlCLookupIPath (&addrset_treedef, tree, loc, &path) == NULL)
    {
      struct addrset_node *n = os_malloc (sizeof (*n));
      n->loc = *loc;
      ut_avlCInsertIPath (&addrset_treedef, tree, n, &path);
    }
    UNLOCK (as);
  }
}

void remove_from_addrset (struct addrset *as, const nn_locator_t *loc)
{
  ut_avlDPath_t path;
  ut_avlCTree_t *tree = ddsi_is_mcaddr (loc) ? &as->mcaddrs : &as->ucaddrs;
  struct addrset_node *n;
  LOCK (as);
  if ((n = ut_avlCLookupDPath (&addrset_treedef, tree, loc, &path)) != NULL)
  {
    ut_avlCDeleteDPath (&addrset_treedef, tree, n, &path);
    os_free (n);
  }
  UNLOCK (as);
}

void copy_addrset_into_addrset_uc (struct addrset *as, const struct addrset *asadd)
{
  struct addrset_node *n;
  ut_avlCIter_t it;
  LOCK (asadd);
  for (n = ut_avlCIterFirst (&addrset_treedef, &asadd->ucaddrs, &it); n; n = ut_avlCIterNext (&it))
    add_to_addrset (as, &n->loc);
  UNLOCK (asadd);
}

void copy_addrset_into_addrset_mc (struct addrset *as, const struct addrset *asadd)
{
  struct addrset_node *n;
  ut_avlCIter_t it;
  LOCK (asadd);
  for (n = ut_avlCIterFirst (&addrset_treedef, &asadd->mcaddrs, &it); n; n = ut_avlCIterNext (&it))
    add_to_addrset (as, &n->loc);
  UNLOCK (asadd);
}

void copy_addrset_into_addrset (struct addrset *as, const struct addrset *asadd)
{
  copy_addrset_into_addrset_uc (as, asadd);
  copy_addrset_into_addrset_mc (as, asadd);
}

#ifdef DDSI_INCLUDE_SSM
void copy_addrset_into_addrset_no_ssm_mc (struct addrset *as, const struct addrset *asadd)
{
  struct addrset_node *n;
  ut_avlCIter_t it;
  LOCK (asadd);
  for (n = ut_avlCIterFirst (&addrset_treedef, &asadd->mcaddrs, &it); n; n = ut_avlCIterNext (&it))
  {
    if (!ddsi_is_ssm_mcaddr (&n->loc))
      add_to_addrset (as, &n->loc);
  }
  UNLOCK (asadd);

}

void copy_addrset_into_addrset_no_ssm (struct addrset *as, const struct addrset *asadd)
{
  copy_addrset_into_addrset_uc (as, asadd);
  copy_addrset_into_addrset_no_ssm_mc (as, asadd);
}

void addrset_purge_ssm (struct addrset *as)
{
  struct addrset_node *n;
  LOCK (as);
  n = ut_avlCFindMin (&addrset_treedef, &as->mcaddrs);
  while (n)
  {
    struct addrset_node *n1 = n;
    n = ut_avlCFindSucc (&addrset_treedef, &as->mcaddrs, n);
    if (ddsi_is_ssm_mcaddr (&n1->loc))
    {
      ut_avlCDelete (&addrset_treedef, &as->mcaddrs, n1);
      os_free (n1);
    }
  }
  UNLOCK (as);
}
#endif

size_t addrset_count (const struct addrset *as)
{
  if (as == NULL)
    return 0;
  else
  {
    size_t count;
    LOCK (as);
    count = ut_avlCCount (&as->ucaddrs) + ut_avlCCount (&as->mcaddrs);
    UNLOCK (as);
    return count;
  }
}

size_t addrset_count_uc (const struct addrset *as)
{
  if (as == NULL)
    return 0;
  else
  {
    size_t count;
    LOCK (as);
    count = ut_avlCCount (&as->ucaddrs);
    UNLOCK (as);
    return count;
  }
}

size_t addrset_count_mc (const struct addrset *as)
{
  if (as == NULL)
    return 0;
  else
  {
    size_t count;
    LOCK (as);
    count = ut_avlCCount (&as->mcaddrs);
    UNLOCK (as);
    return count;
  }
}

int addrset_empty_uc (const struct addrset *as)
{
  int isempty;
  LOCK (as);
  isempty = ut_avlCIsEmpty (&as->ucaddrs);
  UNLOCK (as);
  return isempty;
}

int addrset_empty_mc (const struct addrset *as)
{
  int isempty;
  LOCK (as);
  isempty = ut_avlCIsEmpty (&as->mcaddrs);
  UNLOCK (as);
  return isempty;
}

int addrset_empty (const struct addrset *as)
{
  int isempty;
  LOCK (as);
  isempty = ut_avlCIsEmpty (&as->ucaddrs) && ut_avlCIsEmpty (&as->mcaddrs);
  UNLOCK (as);
  return isempty;
}

int addrset_any_uc (const struct addrset *as, nn_locator_t *dst)
{
  LOCK (as);
  if (ut_avlCIsEmpty (&as->ucaddrs))
  {
    UNLOCK (as);
    return 0;
  }
  else
  {
    const struct addrset_node *n = ut_avlCRootNonEmpty (&addrset_treedef, &as->ucaddrs);
    *dst = n->loc;
    UNLOCK (as);
    return 1;
  }
}

int addrset_any_mc (const struct addrset *as, nn_locator_t *dst)
{
  LOCK (as);
  if (ut_avlCIsEmpty (&as->mcaddrs))
  {
    UNLOCK (as);
    return 0;
  }
  else
  {
    const struct addrset_node *n = ut_avlCRootNonEmpty (&addrset_treedef, &as->mcaddrs);
    *dst = n->loc;
    UNLOCK (as);
    return 1;
  }
}

struct addrset_forall_helper_arg
{
  addrset_forall_fun_t f;
  void * arg;
};

static void addrset_forall_helper (void *vnode, void *varg)
{
  const struct addrset_node *n = vnode;
  struct addrset_forall_helper_arg *arg = varg;
  arg->f (&n->loc, arg->arg);
}

size_t addrset_forall_count (struct addrset *as, addrset_forall_fun_t f, void *arg)
{
  struct addrset_forall_helper_arg arg1;
  size_t count;
  arg1.f = f;
  arg1.arg = arg;
  LOCK (as);
  ut_avlCWalk (&addrset_treedef, &as->mcaddrs, addrset_forall_helper, &arg1);
  ut_avlCWalk (&addrset_treedef, &as->ucaddrs, addrset_forall_helper, &arg1);
  count = ut_avlCCount (&as->ucaddrs) + ut_avlCCount (&as->mcaddrs);
  UNLOCK (as);
  return count;
}

void addrset_forall (struct addrset *as, addrset_forall_fun_t f, void *arg)
{
  (void) addrset_forall_count (as, f, arg);
}

int addrset_forone (struct addrset *as, addrset_forone_fun_t f, void *arg)
{
  unsigned i;
  addrset_node_t n;
  ut_avlCTree_t *trees[2];
  ut_avlCIter_t iter;

  trees[0] = &as->mcaddrs;
  trees[1] = &as->ucaddrs;

  for (i = 0; i < 2u; i++)
  {
    n = (addrset_node_t) ut_avlCIterFirst (&addrset_treedef, trees[i], &iter);
    while (n)
    {
      if ((f) (&n->loc, arg) > 0)
      {
        return 0;
      }
      n = (addrset_node_t) ut_avlCIterNext (&iter);
    }
  }
  return -1;
}

struct log_addrset_helper_arg
{
  uint32_t tf;
};

static void log_addrset_helper (const nn_locator_t *n, void *varg)
{
  const struct log_addrset_helper_arg *arg = varg;
  char buf[DDSI_LOCSTRLEN];
  if (dds_get_log_mask() & arg->tf)
    DDS_LOG(arg->tf, " %s", ddsi_locator_to_string(buf, sizeof(buf), n));
}

void nn_log_addrset (uint32_t tf, const char *prefix, const struct addrset *as)
{
  if (dds_get_log_mask() & tf)
  {
    struct log_addrset_helper_arg arg;
    arg.tf = tf;
    DDS_LOG(tf, "%s", prefix);
    addrset_forall ((struct addrset *) as, log_addrset_helper, &arg); /* drop const, we know it is */
  }
}

static int addrset_eq_onesidederr1 (const ut_avlCTree_t *at, const ut_avlCTree_t *bt)
{
  /* Just checking the root */
  if (ut_avlCIsEmpty (at) && ut_avlCIsEmpty (bt)) {
    return 1;
  } else if (ut_avlCIsSingleton (at) && ut_avlCIsSingleton (bt)) {
    const struct addrset_node *a = ut_avlCRootNonEmpty (&addrset_treedef, at);
    const struct addrset_node *b = ut_avlCRootNonEmpty (&addrset_treedef, bt);
    return compare_locators (&a->loc, &b->loc) == 0;
  } else {
    return 0;
  }
}

int addrset_eq_onesidederr (const struct addrset *a, const struct addrset *b)
{
  int iseq;
  if (a == b)
    return 1;
  if (a == NULL || b == NULL)
    return 0;
  LOCK (a);
  if (TRYLOCK (b) == os_resultSuccess)
  {
    iseq =
      addrset_eq_onesidederr1 (&a->ucaddrs, &b->ucaddrs) &&
      addrset_eq_onesidederr1 (&a->mcaddrs, &b->mcaddrs);
    UNLOCK (b);
  }
  else
  {
    /* We could try <lock b ; trylock(a)>, in a loop, &c. Or we can
       just decide it isn't worth the bother. Which it isn't because
       it doesn't have to be an exact check on equality. A possible
       improvement would be to use an rwlock. */
    iseq = 0;
  }
  UNLOCK (a);
  return iseq;
}
