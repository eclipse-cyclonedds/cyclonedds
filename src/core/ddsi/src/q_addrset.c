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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/ddsi_domaingv.h" /* gv.mattr */
#include "dds/ddsi/ddsi_udp.h" /* nn_mc4gen_address_t */

/* So what does one do with const & mutexes? I need to take lock in a
   pure function just in case some other thread is trying to change
   something. Arguably, that means the thing isn't const; but one
   could just as easily argue that "const" means "this call won't
   change it". If it is globally visible before the call, it may
   change anyway.

   Today, I'm taking the latter interpretation. But all the
   const-discarding casts get moved into LOCK/UNLOCK macros. */
#define LOCK(as) (ddsrt_mutex_lock (&((struct addrset *) (as))->lock))
#define TRYLOCK(as) (ddsrt_mutex_trylock (&((struct addrset *) (as))->lock))
#define UNLOCK(as) (ddsrt_mutex_unlock (&((struct addrset *) (as))->lock))

static int compare_xlocators_vwrap (const void *va, const void *vb);

static const ddsrt_avl_ctreedef_t addrset_treedef =
  DDSRT_AVL_CTREEDEF_INITIALIZER (offsetof (struct addrset_node, avlnode), offsetof (struct addrset_node, loc), compare_xlocators_vwrap, 0);

static int add_addresses_to_addrset_1 (const struct ddsi_domaingv *gv, struct addrset *as, ddsi_locator_t *loc, int port_mode, const char *msgtag)
{
  char buf[DDSI_LOCSTRLEN];
  int32_t maxidx;

  // check whether port number, address type and mode make sense, and prepare the
  // locator by patching the first port number to use if none is given
  if (loc->port != NN_LOCATOR_PORT_INVALID)
  {
    if (port_mode >= 0 && loc->port != (uint32_t) port_mode)
    {
      GVERROR ("%s: %s: port mismatch (expecting no port or %d)\n", msgtag, ddsi_locator_to_string (buf, sizeof(buf), loc), port_mode);
      return -1;
    }
    maxidx = 0;
  }
  else if (port_mode >= 0)
  {
    loc->port = (uint32_t) port_mode;
    maxidx = 0;
  }
  else if (ddsi_is_mcaddr (gv, loc))
  {
    loc->port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, 0);
    maxidx = 0;
  }
  else
  {
    loc->port = ddsi_get_port (&gv->config, DDSI_PORT_UNI_DISC, 0);
    maxidx = gv->config.maxAutoParticipantIndex;
  }

  GVLOG (DDS_LC_CONFIG, "%s: add %s", msgtag, ddsi_locator_to_string (buf, sizeof (buf), loc));
  add_locator_to_addrset (gv, as, loc);
  for (int32_t i = 1; i < maxidx; i++)
  {
    loc->port = ddsi_get_port (&gv->config, DDSI_PORT_UNI_DISC, i);
    GVLOG (DDS_LC_CONFIG, ", :%"PRIu32, loc->port);
    add_locator_to_addrset (gv, as, loc);
  }
  GVLOG (DDS_LC_CONFIG, "\n");
  return 0;
}

int add_addresses_to_addrset (const struct ddsi_domaingv *gv, struct addrset *as, const char *addrs, int port_mode, const char *msgtag, int req_mc)
{
  /* port_mode: -1  => take from string, if 0 & unicast, add for a range of participant indices;
     port_mode >= 0 => always set port to port_mode
  */
  DDSRT_WARNING_MSVC_OFF(4996);
  char *addrs_copy, *cursor, *a;
  int retval = -1;
  addrs_copy = ddsrt_strdup (addrs);
  cursor = addrs_copy;
  while ((a = ddsrt_strsep (&cursor, ",")) != NULL)
  {
    ddsi_locator_t loc;
    char buf[DDSI_LOCSTRLEN];

    switch (ddsi_locator_from_string (gv, &loc, a, gv->m_factory))
    {
      case AFSR_OK:
        break;
      case AFSR_INVALID:
        GVERROR ("%s: %s: not a valid address\n", msgtag, a);
        goto error;
      case AFSR_UNKNOWN:
        GVERROR ("%s: %s: unknown address\n", msgtag, a);
        goto error;
      case AFSR_MISMATCH:
        GVERROR ("%s: %s: address family mismatch\n", msgtag, a);
        goto error;
    }

    if (req_mc && !ddsi_is_mcaddr (gv, &loc))
    {
      GVERROR ("%s: %s: not a multicast address\n", msgtag, ddsi_locator_to_string_no_port (buf, sizeof(buf), &loc));
      goto error;
    }

    if (add_addresses_to_addrset_1 (gv, as, &loc, port_mode, msgtag) < 0)
    {
      goto error;
    }
  }
  retval = 0;
 error:
  ddsrt_free (addrs_copy);
  return retval;
  DDSRT_WARNING_MSVC_ON(4996);
}

int compare_locators (const ddsi_locator_t *a, const ddsi_locator_t *b)
{
  int c;
  if (a->kind != b->kind)
    return (int) (a->kind - b->kind);
  else if ((c = memcmp (a->address, b->address, sizeof (a->address))) != 0)
    return c;
  else if (a->port != b->port)
    return (int) (a->port - b->port);
  else
    return 0;
}

int compare_xlocators (const ddsi_xlocator_t *a, const ddsi_xlocator_t *b)
{
  int c;
  if ((c = compare_locators (&a->c, &b->c)) != 0)
    return c;
  else
  {
    const uintptr_t ac = (uintptr_t) a->conn;
    const uintptr_t bc = (uintptr_t) b->conn;
    return (ac == bc) ? 0 : (ac < bc) ? -1 : 1;
  }
}

static int compare_xlocators_vwrap (const void *va, const void *vb)
{
  return compare_xlocators (va, vb);
}

struct addrset *new_addrset (void)
{
  struct addrset *as = ddsrt_malloc (sizeof (*as));
  ddsrt_atomic_st32 (&as->refc, 1);
  ddsrt_mutex_init (&as->lock);
  ddsrt_avl_cinit (&addrset_treedef, &as->ucaddrs);
  ddsrt_avl_cinit (&addrset_treedef, &as->mcaddrs);
  return as;
}

struct addrset *ref_addrset (struct addrset *as)
{
  if (as != NULL)
  {
    ddsrt_atomic_inc32 (&as->refc);
  }
  return as;
}

void unref_addrset (struct addrset *as)
{
  if ((as != NULL) && (ddsrt_atomic_dec32_ov (&as->refc) == 1))
  {
    ddsrt_avl_cfree (&addrset_treedef, &as->ucaddrs, ddsrt_free);
    ddsrt_avl_cfree (&addrset_treedef, &as->mcaddrs, ddsrt_free);
    ddsrt_mutex_destroy (&as->lock);
    ddsrt_free (as);
  }
}

void set_unspec_locator (ddsi_locator_t *loc)
{
  loc->kind = NN_LOCATOR_KIND_INVALID;
  loc->port = NN_LOCATOR_PORT_INVALID;
  memset (loc->address, 0, sizeof (loc->address));
}

void set_unspec_xlocator (ddsi_xlocator_t *loc)
{
  loc->conn = NULL;
  set_unspec_locator (&loc->c);
}

int is_unspec_locator (const ddsi_locator_t *loc)
{
  static const ddsi_locator_t zloc = { .kind = 0 };
  return (loc->kind == NN_LOCATOR_KIND_INVALID &&
          loc->port == NN_LOCATOR_PORT_INVALID &&
          memcmp (&zloc.address, loc->address, sizeof (zloc.address)) == 0);
}

int is_unspec_xlocator (const ddsi_xlocator_t *loc)
{
  return is_unspec_locator (&loc->c);
}

#ifdef DDS_HAS_SSM
int addrset_contains_ssm (const struct ddsi_domaingv *gv, const struct addrset *as)
{
  struct addrset_node *n;
  ddsrt_avl_citer_t it;
  LOCK (as);
  for (n = ddsrt_avl_citer_first (&addrset_treedef, &as->mcaddrs, &it); n; n = ddsrt_avl_citer_next (&it))
  {
    if (ddsi_is_ssm_mcaddr (gv, &n->loc.c))
    {
      UNLOCK (as);
      return 1;
    }
  }
  UNLOCK (as);
  return 0;
}

int addrset_any_ssm (const struct ddsi_domaingv *gv, const struct addrset *as, ddsi_xlocator_t *dst)
{
  struct addrset_node *n;
  ddsrt_avl_citer_t it;
  LOCK (as);
  for (n = ddsrt_avl_citer_first (&addrset_treedef, &as->mcaddrs, &it); n; n = ddsrt_avl_citer_next (&it))
  {
    if (ddsi_is_ssm_mcaddr (gv, &n->loc.c))
    {
      *dst = n->loc;
      UNLOCK (as);
      return 1;
    }
  }
  UNLOCK (as);
  return 0;
}

int addrset_any_non_ssm_mc (const struct ddsi_domaingv *gv, const struct addrset *as, ddsi_xlocator_t *dst)
{
  struct addrset_node *n;
  ddsrt_avl_citer_t it;
  LOCK (as);
  for (n = ddsrt_avl_citer_first (&addrset_treedef, &as->mcaddrs, &it); n; n = ddsrt_avl_citer_next (&it))
  {
    if (!ddsi_is_ssm_mcaddr (gv, &n->loc.c))
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
  ddsrt_avl_cfree (&addrset_treedef, &as->ucaddrs, ddsrt_free);
  ddsrt_avl_cfree (&addrset_treedef, &as->mcaddrs, ddsrt_free);
  UNLOCK (as);
  return 0;
}

static void add_xlocator_to_addrset_impl (const struct ddsi_domaingv *gv, struct addrset *as, const ddsi_xlocator_t *loc)
{
  assert (!is_unspec_locator (&loc->c));
  assert (loc->conn != NULL);
  ddsrt_avl_ipath_t path;
  ddsrt_avl_ctree_t *tree = ddsi_is_mcaddr (gv, &loc->c) ? &as->mcaddrs : &as->ucaddrs;
  LOCK (as);
  if (ddsrt_avl_clookup_ipath (&addrset_treedef, tree, loc, &path) == NULL)
  {
    struct addrset_node *n = ddsrt_malloc (sizeof (*n));
    n->loc = *loc;
    ddsrt_avl_cinsert_ipath (&addrset_treedef, tree, n, &path);
  }
  UNLOCK (as);
}

void add_xlocator_to_addrset (const struct ddsi_domaingv *gv, struct addrset *as, const ddsi_xlocator_t *loc)
{
  if (is_unspec_locator (&loc->c))
    return;
  add_xlocator_to_addrset_impl (gv, as, loc);
}

void add_locator_to_addrset (const struct ddsi_domaingv *gv, struct addrset *as, const ddsi_locator_t *loc)
{
  if (is_unspec_locator (loc))
    return;
  if (ddsi_is_mcaddr (gv, loc))
  {
    // multicast: use all transmit connections
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (ddsi_factory_supports (gv->xmit_conns[i]->m_factory, loc->kind))
        add_xlocator_to_addrset_impl (gv, as, &(const ddsi_xlocator_t) {
          .conn = gv->xmit_conns[i],
          .c = *loc });
    }
  }
  else
  {
    // unicast: assume the kernel knows how to route it from any connection
    // if it doesn't match a local interface
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (!ddsi_factory_supports (gv->xmit_conns[i]->m_factory, loc->kind))
        continue;
      switch (ddsi_is_nearby_address (gv, loc, (size_t) gv->n_interfaces, gv->interfaces, NULL))
      {
        case DNAR_LOCAL:
          add_xlocator_to_addrset_impl (gv, as, &(const ddsi_xlocator_t) {
            .conn = gv->xmit_conns[i],
            .c = *loc });
          return;
        case DNAR_DISTANT:
          break;
      }
    }
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (!ddsi_factory_supports (gv->xmit_conns[i]->m_factory, loc->kind))
        continue;
      add_xlocator_to_addrset_impl (gv, as, &(const ddsi_xlocator_t) {
        .conn = gv->xmit_conns[i],
        .c = *loc });
      break;
    }
  }
}

void remove_from_addrset (const struct ddsi_domaingv *gv, struct addrset *as, const ddsi_xlocator_t *loc)
{
  ddsrt_avl_dpath_t path;
  ddsrt_avl_ctree_t *tree = ddsi_is_mcaddr (gv, &loc->c) ? &as->mcaddrs : &as->ucaddrs;
  struct addrset_node *n;
  LOCK (as);
  if ((n = ddsrt_avl_clookup_dpath (&addrset_treedef, tree, loc, &path)) != NULL)
  {
    ddsrt_avl_cdelete_dpath (&addrset_treedef, tree, n, &path);
    ddsrt_free (n);
  }
  UNLOCK (as);
}

void copy_addrset_into_addrset_uc (const struct ddsi_domaingv *gv, struct addrset *as, const struct addrset *asadd)
{
  struct addrset_node *n;
  ddsrt_avl_citer_t it;
  LOCK (asadd);
  for (n = ddsrt_avl_citer_first (&addrset_treedef, &asadd->ucaddrs, &it); n; n = ddsrt_avl_citer_next (&it))
    add_xlocator_to_addrset_impl (gv, as, &n->loc);
  UNLOCK (asadd);
}

void copy_addrset_into_addrset_mc (const struct ddsi_domaingv *gv, struct addrset *as, const struct addrset *asadd)
{
  struct addrset_node *n;
  ddsrt_avl_citer_t it;
  LOCK (asadd);
  for (n = ddsrt_avl_citer_first (&addrset_treedef, &asadd->mcaddrs, &it); n; n = ddsrt_avl_citer_next (&it))
    add_xlocator_to_addrset_impl (gv, as, &n->loc);
  UNLOCK (asadd);
}

void copy_addrset_into_addrset (const struct ddsi_domaingv *gv, struct addrset *as, const struct addrset *asadd)
{
  copy_addrset_into_addrset_uc (gv, as, asadd);
  copy_addrset_into_addrset_mc (gv, as, asadd);
}

#ifdef DDS_HAS_SSM
void copy_addrset_into_addrset_no_ssm_mc (const struct ddsi_domaingv *gv, struct addrset *as, const struct addrset *asadd)
{
  struct addrset_node *n;
  ddsrt_avl_citer_t it;
  LOCK (asadd);
  for (n = ddsrt_avl_citer_first (&addrset_treedef, &asadd->mcaddrs, &it); n; n = ddsrt_avl_citer_next (&it))
  {
    if (!ddsi_is_ssm_mcaddr (gv, &n->loc.c))
      add_xlocator_to_addrset_impl (gv, as, &n->loc);
  }
  UNLOCK (asadd);

}

void copy_addrset_into_addrset_no_ssm (const struct ddsi_domaingv *gv, struct addrset *as, const struct addrset *asadd)
{
  copy_addrset_into_addrset_uc (gv, as, asadd);
  copy_addrset_into_addrset_no_ssm_mc (gv, as, asadd);
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
    count = ddsrt_avl_ccount (&as->ucaddrs) + ddsrt_avl_ccount (&as->mcaddrs);
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
    count = ddsrt_avl_ccount (&as->ucaddrs);
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
    count = ddsrt_avl_ccount (&as->mcaddrs);
    UNLOCK (as);
    return count;
  }
}

int addrset_empty_uc (const struct addrset *as)
{
  int isempty;
  LOCK (as);
  isempty = ddsrt_avl_cis_empty (&as->ucaddrs);
  UNLOCK (as);
  return isempty;
}

int addrset_empty_mc (const struct addrset *as)
{
  int isempty;
  LOCK (as);
  isempty = ddsrt_avl_cis_empty (&as->mcaddrs);
  UNLOCK (as);
  return isempty;
}

int addrset_empty (const struct addrset *as)
{
  int isempty;
  LOCK (as);
  isempty = ddsrt_avl_cis_empty (&as->ucaddrs) && ddsrt_avl_cis_empty (&as->mcaddrs);
  UNLOCK (as);
  return isempty;
}

int addrset_any_uc (const struct addrset *as, ddsi_xlocator_t *dst)
{
  LOCK (as);
  if (ddsrt_avl_cis_empty (&as->ucaddrs))
  {
    UNLOCK (as);
    return 0;
  }
  else
  {
    const struct addrset_node *n = ddsrt_avl_croot_non_empty (&addrset_treedef, &as->ucaddrs);
    *dst = n->loc;
    UNLOCK (as);
    return 1;
  }
}

int addrset_any_mc (const struct addrset *as, ddsi_xlocator_t *dst)
{
  LOCK (as);
  if (ddsrt_avl_cis_empty (&as->mcaddrs))
  {
    UNLOCK (as);
    return 0;
  }
  else
  {
    const struct addrset_node *n = ddsrt_avl_croot_non_empty (&addrset_treedef, &as->mcaddrs);
    *dst = n->loc;
    UNLOCK (as);
    return 1;
  }
}

void addrset_any_uc_else_mc_nofail (const struct addrset *as, ddsi_xlocator_t *dst)
{
  LOCK (as);
  if (!ddsrt_avl_cis_empty (&as->ucaddrs))
  {
    const struct addrset_node *n = ddsrt_avl_croot_non_empty (&addrset_treedef, &as->ucaddrs);
    *dst = n->loc;
  }
  else
  {
    assert (!ddsrt_avl_cis_empty (&as->mcaddrs));
    const struct addrset_node *n = ddsrt_avl_croot_non_empty (&addrset_treedef, &as->mcaddrs);
    *dst = n->loc;
  }
  UNLOCK (as);
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
  ddsrt_avl_cwalk (&addrset_treedef, &as->mcaddrs, addrset_forall_helper, &arg1);
  ddsrt_avl_cwalk (&addrset_treedef, &as->ucaddrs, addrset_forall_helper, &arg1);
  count = ddsrt_avl_ccount (&as->ucaddrs) + ddsrt_avl_ccount (&as->mcaddrs);
  UNLOCK (as);
  return count;
}

void addrset_forall (struct addrset *as, addrset_forall_fun_t f, void *arg)
{
  (void) addrset_forall_count (as, f, arg);
}

size_t addrset_forall_uc_else_mc_count (struct addrset *as, addrset_forall_fun_t f, void *arg)
{
  struct addrset_forall_helper_arg arg1;
  size_t count;
  arg1.f = f;
  arg1.arg = arg;
  LOCK (as);
  if (!ddsrt_avl_cis_empty (&as->ucaddrs))
  {
    ddsrt_avl_cwalk (&addrset_treedef, &as->ucaddrs, addrset_forall_helper, &arg1);
    count = ddsrt_avl_ccount (&as->ucaddrs);
  }
  else
  {
    ddsrt_avl_cwalk (&addrset_treedef, &as->mcaddrs, addrset_forall_helper, &arg1);
    count = ddsrt_avl_ccount (&as->mcaddrs);
  }
  UNLOCK (as);
  return count;
}

size_t addrset_forall_mc_count (struct addrset *as, addrset_forall_fun_t f, void *arg)
{
  struct addrset_forall_helper_arg arg1;
  size_t count;
  arg1.f = f;
  arg1.arg = arg;
  LOCK (as);
  ddsrt_avl_cwalk (&addrset_treedef, &as->mcaddrs, addrset_forall_helper, &arg1);
  count = ddsrt_avl_ccount (&as->mcaddrs);
  UNLOCK (as);
  return count;
}

int addrset_forone (struct addrset *as, addrset_forone_fun_t f, void *arg)
{
  addrset_node_t n;
  ddsrt_avl_ctree_t *trees[2];
  ddsrt_avl_citer_t iter;

  trees[0] = &as->mcaddrs;
  trees[1] = &as->ucaddrs;
  for (int i = 0; i < 2; i++)
  {
    n = (addrset_node_t) ddsrt_avl_citer_first (&addrset_treedef, trees[i], &iter);
    while (n)
    {
      if ((f) (&n->loc, arg) > 0)
      {
        return 0;
      }
      n = (addrset_node_t) ddsrt_avl_citer_next (&iter);
    }
  }
  return -1;
}

struct log_addrset_helper_arg
{
  uint32_t tf;
  struct ddsi_domaingv *gv;
};

static void log_addrset_helper (const ddsi_xlocator_t *n, void *varg)
{
  const struct log_addrset_helper_arg *arg = varg;
  const struct ddsi_domaingv *gv = arg->gv;
  char buf[DDSI_LOCSTRLEN];
  if (gv->logconfig.c.mask & arg->tf)
    GVLOG (arg->tf, " %s", ddsi_xlocator_to_string (buf, sizeof(buf), n));
}

void nn_log_addrset (struct ddsi_domaingv *gv, uint32_t tf, const char *prefix, const struct addrset *as)
{
  if (gv->logconfig.c.mask & tf)
  {
    struct log_addrset_helper_arg arg;
    arg.tf = tf;
    arg.gv = gv;
    GVLOG (tf, "%s", prefix);
    addrset_forall ((struct addrset *) as, log_addrset_helper, &arg); /* drop const, we know it is */
  }
}

static int addrset_eq_onesidederr1 (const ddsrt_avl_ctree_t *at, const ddsrt_avl_ctree_t *bt)
{
  /* Just checking the root */
  if (ddsrt_avl_cis_empty (at) && ddsrt_avl_cis_empty (bt)) {
    return 1;
  } else if (ddsrt_avl_cis_singleton (at) && ddsrt_avl_cis_singleton (bt)) {
    const struct addrset_node *a = ddsrt_avl_croot_non_empty (&addrset_treedef, at);
    const struct addrset_node *b = ddsrt_avl_croot_non_empty (&addrset_treedef, bt);
    return compare_xlocators (&a->loc, &b->loc) == 0;
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
  if (TRYLOCK (b))
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
