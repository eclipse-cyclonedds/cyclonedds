// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__entity.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__addrset.h"
#include "ddsi__bitset.h"
#include "ddsi__entity_index.h"
#include "ddsi__wraddrset.h"
#include "ddsi__tran.h"
#include "ddsi__udp.h" /* ddsi_mc4gen_address_t */

// For each (reader, locator) pair, the coverage map gives:
// INT32_MIN if the reader isn't covered by this locator, >= INT32_MIN+1 if it is
//
// If covered, the value depends on the kind of locator:
// - for regular locators: 0
// - for MCGEN locators:   bit position in address
typedef uint8_t cover_info_t;
typedef char rdname_t[3];

struct cover {
  int nreaders;
  int nlocs;
  rdname_t *rdnames;
  cover_info_t m[]; // [nreaders][nlocs]
};

static size_t cover_size (int nreaders, int nlocs)
{
  return sizeof (struct cover) + (uint32_t) nlocs * (uint32_t) nreaders * sizeof (cover_info_t);
}

static struct cover *cover_new (int nreaders, int nlocs, bool want_rdnames)
{
  struct cover *c = ddsrt_malloc (cover_size (nreaders, nlocs));
  c->nreaders = nreaders;
  c->nlocs = nlocs;
  if (want_rdnames)
    c->rdnames = ddsrt_malloc ((size_t) nreaders * sizeof (*c->rdnames));
  else
    c->rdnames = NULL;
  for (int i = 0; i < nreaders; i++)
    for (int j = 0; j < nlocs; j++)
      c->m[i * nlocs + j] = 0xff;
  return c;
}

static void cover_makeroom (struct cover **c, int rdidx)
{
  if (rdidx == (*c)->nreaders)
  {
    // why 60? why not ... we only get here if the redundant networking trick is enabled
    const int chunk = 60;
    (*c) = ddsrt_realloc (*c, cover_size ((*c)->nreaders + chunk, (*c)->nlocs));
    (*c)->nreaders += chunk;
    if ((*c)->rdnames)
      (*c)->rdnames = ddsrt_realloc ((*c)->rdnames, (size_t) (*c)->nreaders * sizeof (*(*c)->rdnames));
  }
}

static void cover_update_nreaders (struct cover *c, int nreaders)
{
  // Coverage matrix initially gets created based on the number of matching proxy readers
  // but by the time we get around to populating it, some of them may no longer be around
  // (that is, the GUID lookup fails).  This function exists to allow reducing the number
  // of readers to what actually is used in computing the address set.  That way the need
  // to carry the count separately disappears.
  assert (nreaders <= c->nreaders);
  c->nreaders = nreaders;
}

static int cover_get_nreaders (const struct cover *c)
{
  return c->nreaders;
}

static int cover_get_nlocs (const struct cover *c)
{
  return c->nlocs;
}

static void cover_free (struct cover *c)
{
  if (c->rdnames)
    ddsrt_free (c->rdnames);
  ddsrt_free (c);
}

static void cover_set (struct cover *c, int rdidx, int lidx, cover_info_t v)
{
  assert (rdidx < c->nreaders && lidx < c->nlocs);
  c->m[rdidx * c->nlocs + lidx] = v;
}

static cover_info_t cover_get (const struct cover *c, int rdidx, int lidx)
{
  assert (rdidx < c->nreaders && lidx < c->nlocs);
  return c->m[rdidx * c->nlocs + lidx];
}

typedef int32_t cost_t;
typedef int32_t delta_cost_t;

typedef struct ddsi_readercount_cost {
  uint32_t nrds;
  cost_t cost;
} readercount_cost_t;

struct costmap {
  int nlocs;
  readercount_cost_t m[];
};

static struct costmap *costmap_new (int nlocs)
{
  struct costmap *wm = ddsrt_malloc (sizeof (*wm) + (uint32_t) nlocs * sizeof (*wm->m));
  wm->nlocs = nlocs;
  for (int j = 0; j < nlocs; j++)
  {
    wm->m[j].nrds = 0;
    wm->m[j].cost = 0;
  }
  return wm;
}

static void costmap_free (struct costmap *wm)
{
  ddsrt_free (wm);
}

static void costmap_set (struct costmap *wm, int lidx, readercount_cost_t v)
{
  assert (lidx < wm->nlocs);
  wm->m[lidx] = v;
}

static void costmap_adjust (struct costmap *wm, int lidx, delta_cost_t v)
{
  assert (lidx < wm->nlocs);
  if (wm->m[lidx].cost == INT32_MAX)
    return;
  assert (wm->m[lidx].nrds > 0);
  if (--wm->m[lidx].nrds == 0)
    wm->m[lidx].cost = INT32_MAX;
  else
    wm->m[lidx].cost += v;
}

static readercount_cost_t costmap_get (const struct costmap *wm, int lidx)
{
  assert (lidx < wm->nlocs);
  return wm->m[lidx];
}

struct locset {
  int nlocs;
  ddsi_xlocator_t locs[];
};

static struct locset *locset_new (int nlocs)
{
  struct locset *ls = ddsrt_malloc (sizeof (*ls) + (uint32_t) nlocs * sizeof (*ls->locs));
  ls->nlocs = nlocs;
  for (int j = 0; j < nlocs; j++)
    ddsi_set_unspec_xlocator (&ls->locs[j]);
  return ls;
}

static void locset_free (struct locset *ls)
{
  ddsrt_free (ls);
}

static struct ddsi_addrset *wras_collect_all_locs (const struct ddsi_writer *wr)
{
  struct ddsi_entity_index * const gh = wr->e.gv->entity_index;
  struct ddsi_addrset *all_addrs = ddsi_new_addrset ();
  struct ddsi_wr_prd_match *m;
  ddsrt_avl_iter_t it;
  for (m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_reader *prd;
    if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
      continue;
    ddsi_copy_addrset_into_addrset (wr->e.gv, all_addrs, prd->c.as);
  }
  if (!ddsi_addrset_empty (all_addrs))
  {
#ifdef DDS_HAS_SSM
    if (wr->supports_ssm && wr->ssm_as)
      ddsi_copy_addrset_into_addrset_mc (wr->e.gv, all_addrs, wr->ssm_as);
#endif
  }
  return all_addrs;
}

struct rebuild_flatten_locs_helper_arg {
  ddsi_xlocator_t *locs;
  int idx;
#ifndef NDEBUG
  int size;
#endif
};

static void wras_flatten_locs_helper (const ddsi_xlocator_t *loc, void *varg)
{
  struct rebuild_flatten_locs_helper_arg *arg = varg;
  assert(arg->idx < arg->size);
  arg->locs[arg->idx++] = *loc;
}

static void wras_flatten_locs_prealloc (struct locset *ls, struct ddsi_addrset *addrs)
{
  struct rebuild_flatten_locs_helper_arg flarg;
  flarg.locs = ls->locs;
  flarg.idx = 0;
#ifndef NDEBUG
  flarg.size = ls->nlocs;
#endif
  ddsi_addrset_forall (addrs, wras_flatten_locs_helper, &flarg);
  ls->nlocs = flarg.idx;
}

static struct locset *wras_flatten_locs (struct ddsi_addrset *all_addrs)
{
  const int nin = (int) ddsi_addrset_count (all_addrs);
  struct locset *ls = locset_new (nin);
  wras_flatten_locs_prealloc (ls, all_addrs);
  assert (ls->nlocs == nin);
  return ls;
}

static int wras_compare_locs (const void *va, const void *vb)
{
  // Each machine has a slightly UDPv4MCGEN locator because the address
  // contains the index of the machine the bitmask, but the point of them
  // is to treat them the same and calculate the actual address to use
  // once we know all the readers it addresses.  So for those, erase the
  // index component before comparing.
  const ddsi_xlocator_t *a = va;
  const ddsi_xlocator_t *b = vb;
  if (a->c.kind != b->c.kind || a->c.kind != DDSI_LOCATOR_KIND_UDPv4MCGEN)
    return ddsi_compare_xlocators (a, b);
  else
  {
    ddsi_xlocator_t u = *a, v = *b;
    ddsi_udpv4mcgen_address_t *u1 = (ddsi_udpv4mcgen_address_t *) u.c.address;
    ddsi_udpv4mcgen_address_t *v1 = (ddsi_udpv4mcgen_address_t *) v.c.address;
    u1->idx = v1->idx = 0;
    return ddsi_compare_xlocators (&u, &v);
  }
}

static struct locset *wras_calc_locators (const struct ddsrt_log_cfg *logcfg, struct ddsi_addrset *all_addrs)
{
  struct locset *ls = wras_flatten_locs (all_addrs);
  int i, j;
  /* We want MC gens just once for each IP,BASE,COUNT pair, not once for each node */
  i = 0; j = 1;
  qsort (ls->locs, (size_t) ls->nlocs, sizeof (*ls->locs), wras_compare_locs);
  while (j < ls->nlocs)
  {
    if (wras_compare_locs (&ls->locs[i], &ls->locs[j]) != 0)
      ls->locs[++i] = ls->locs[j];
    j++;
  }
  ls->nlocs = i+1;
  DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "reduced nlocs=%d\n", ls->nlocs);
  return ls;
}

#define CI_STATUS_MASK     0x3
#define CI_REACHABLE       0x0 // reachable via this locator
#define CI_INCLUDED        0x1 // reachable, already included in selected locators
#define CI_NOMATCH         0x2 // not reached by this locator
#define CI_LOOPBACK        0x4 // is a loopback locator (set for entire row)
#define CI_MULTICAST_MASK 0xf8 // 0: no, 1: ASM, 2: SSM, (index+3) if MCGEN
#define CI_MULTICAST_SHIFT   3
#define CI_MULTICAST_ASM          1
#define CI_MULTICAST_SSM          2
#define CI_MULTICAST_MCGEN_OFFSET 3

// Cost associated with delivering another time to a reader that has
// already been covered by previously selected locators
static const int32_t cost_discarded = 1;

// Cost associated with delivering another time to a reader that has
// already been covered by a (selected) Iceoryx locator.  Currently,
// it is quite painful when this happens because it can lead to user
// observable stuttering
static const int32_t cost_redundant_iceoryx = 1000000;

// Cost associated with delivering data for the first time (slightly
// negative cost makes it possible to give a slightly higher initial
// cost to multicasts and switch over from unicast to multicast once
// several readers can be addressed simultaneously)
static const int32_t cost_delivered = -1;

#define CI_ICEORYX        0xfc // FIXME: this is a hack

static cost_t sat_cost_add (cost_t x, int32_t a)
{
  DDSRT_STATIC_ASSERT (sizeof (cost_t) == sizeof (int32_t) && (cost_t) ((uint32_t)1 << 31) < 0);
  if (a >= 0)
    return (x > INT32_MAX - a) ? INT32_MAX : x + a;
  else
    return (x < INT32_MIN - a) ? INT32_MIN : x + a;
}

static readercount_cost_t calc_locator_cost (const struct locset *locs, const struct cover *c, int lidx, dds_locator_mask_t ignore)
{
  const int32_t cost_uc  = locs->locs[lidx].conn->m_interf->prefer_multicast ? 1000000 : 2;
  const int32_t cost_mc  = locs->locs[lidx].conn->m_interf->prefer_multicast ? 1 : 3;
  const int32_t cost_ssm = locs->locs[lidx].conn->m_interf->prefer_multicast ? 0 : 2;
  readercount_cost_t x = { .nrds = 0, .cost = - locs->locs[lidx].conn->m_interf->priority };

  // Find first reader that this locator addresses so we actually know something
  // about the locator.  There should be at least one, but if none were to be there
  // we already know this is an invalid choice.
  int rdidx;
  cover_info_t ci = 0; // clang (at least) can't figure out that init is unnecessary
  for (rdidx = 0; rdidx < c->nreaders; rdidx++)
  {
    ci = cover_get (c, rdidx, lidx);
    if ((ci & CI_STATUS_MASK) != CI_NOMATCH)
      break;
  }
  if (rdidx == c->nreaders)
    goto no_readers;

  if ((ci & ~CI_STATUS_MASK) == CI_ICEORYX)
  {
    if (0 == (ignore & DDSI_LOCATOR_KIND_SHEM))
      x.cost = INT32_MIN;
    else
      goto no_readers;
  }
  else if ((ci & CI_MULTICAST_MASK) == 0)
    x.cost += cost_uc;
  else if (((ci & CI_MULTICAST_MASK) >> CI_MULTICAST_SHIFT) == CI_MULTICAST_SSM)
    x.cost += cost_ssm;
  else
    x.cost += cost_mc;

  for (; rdidx < c->nreaders; rdidx++)
  {
    ci = cover_get (c, rdidx, lidx);
    if ((ci & CI_STATUS_MASK) == CI_NOMATCH)
      continue;

#if 0
    // this is nice for checking the incremental work done in wras_drop_covered_readers,
    // but that is only possible if the cost_redundant_iceoryx == cost_discarded
    if ((ci & CI_STATUS_MASK) == CI_INCLUDED)
    {
      // FIXME: need addressed hosts, addressed processes; those change when nodes come/go
      x.cost = sat_cost_add (x.cost, cost_discarded);
    }
    else
#endif
    {
      assert ((ci & CI_STATUS_MASK) == CI_REACHABLE);
      x.cost = sat_cost_add (x.cost, cost_delivered);
      x.nrds++;
    }
  }
  if (x.cost == INT32_MAX)
    x.cost = INT32_MAX - 1;

no_readers:
  if (x.nrds == 0)
    x.cost = INT32_MAX;
  return x;
}

static bool isloopback (struct ddsi_domaingv const * const gv, const ddsi_xlocator_t *loc)
{
  for (int k = 0; k < gv->n_interfaces; k++)
    if (loc->conn == gv->xmit_conns[k] && gv->interfaces[k].loopback)
      return true;
  return false;
}

static int wras_compare_by_interface (const void *va, const void *vb)
{
  const ddsi_xlocator_t *a = va;
  const ddsi_xlocator_t *b = vb;
  if ((uintptr_t) a->conn == (uintptr_t) b->conn)
    return 0;
  else if ((uintptr_t) a->conn < (uintptr_t) b->conn)
    return -1;
  else
    return 1;
}

static int move_loopback_forward (struct ddsi_domaingv const * const gv, struct locset *ls)
{
  // note: not a stable sort
  int i = 0, j = ls->nlocs;
  while (i < j)
  {
    // isloopback(ls->locs[k]) = true for all k < i, false for all k >= j; i < nlocs
    if (isloopback (gv, &ls->locs[i]))
      i++;
    else
    {
      ddsi_xlocator_t tmp = ls->locs[i];
      ls->locs[i] = ls->locs[--j];
      ls->locs[j] = tmp;
    }
  }
  // i <= nlocs
  qsort (ls->locs + i, (size_t) (ls->nlocs - i), sizeof (ls->locs[0]), wras_compare_by_interface);
  return i;
}


static unsigned multicast_indicator (struct ddsi_domaingv const * const gv, const ddsi_xlocator_t *l)
{
#if DDS_HAS_SSM
  if (ddsi_is_ssm_mcaddr (gv, &l->c))
    return CI_MULTICAST_SSM;
#endif
  if (ddsi_is_mcaddr (gv, &l->c))
    return CI_MULTICAST_ASM;
  return 0;
}

static bool locator_is_iceoryx (const ddsi_xlocator_t *l)
{
#ifdef DDS_HAS_SHM
  return l->c.kind == DDSI_LOCATOR_KIND_SHEM;
#else
  (void) l;
  return false;
#endif
}

static bool wras_cover_locatorset (struct ddsi_domaingv const * const gv, struct cover *cov, const struct locset *locs, const struct locset *work_locs, int rdidx, int nloopback, int first, int last) ddsrt_attribute_warn_unused_result;

static bool wras_cover_locatorset (struct ddsi_domaingv const * const gv, struct cover *cov, const struct locset *locs, const struct locset *work_locs, int rdidx, int nloopback, int first, int last)
{
  for (int j = first; j <= last; j++)
  {
    /* all addresses should be in the combined set of addresses, unless the address sets change on the fly:
       in that case, restart */
    const ddsi_xlocator_t *l = bsearch (&work_locs->locs[j], locs->locs, (size_t) locs->nlocs, sizeof (*locs->locs), wras_compare_locs);
    if (l == NULL)
      return false;
    cover_info_t x;
    int lidx = (int) (l - locs->locs);
    if (locator_is_iceoryx (l)) // FIXME: a gross hack
    {
      x = CI_ICEORYX;
    }
    else if (l->c.kind == DDSI_LOCATOR_KIND_UDPv4MCGEN)
    {
      const ddsi_udpv4mcgen_address_t *l1 = (const ddsi_udpv4mcgen_address_t *) l->c.address;
      assert (l1->base + l1->idx <= 31 - CI_MULTICAST_MCGEN_OFFSET);
      x = (cover_info_t) ((CI_MULTICAST_MCGEN_OFFSET + l1->base + l1->idx) << CI_MULTICAST_SHIFT);
    }
    else
    {
      x = 0;
      if (j < nloopback)
        x |= CI_LOOPBACK;
      x |= (cover_info_t) (multicast_indicator (gv, l) << CI_MULTICAST_SHIFT);
    }
    char buf[200];
    GVTRACE ("rdidx %u lidx %s %u -> %x\n", rdidx, ddsi_xlocator_to_string(buf, sizeof(buf), &work_locs->locs[j]), lidx, x);
    assert (x != 0xff);
    assert (cover_get (cov, rdidx, lidx) == 0xff);
    cover_set (cov, rdidx, lidx, x);
  }
  return true;
}

static bool wras_calc_cover (const struct ddsi_writer *wr, const struct locset *locs, struct cover **pcov) ddsrt_attribute_warn_unused_result;

static bool wras_calc_cover (const struct ddsi_writer *wr, const struct locset *locs, struct cover **pcov)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  struct ddsi_entity_index * const gh = gv->entity_index;
  ddsrt_avl_iter_t it;
  const bool want_rdnames = true;
  // allocate cover matrix, it needs to be grow if there are readers requesting redundant delivery
  // (but that's rare enough to be ok with reallocating it for now)
  struct cover *cov = cover_new ((int) wr->num_readers, locs->nlocs, want_rdnames);
  struct locset *work_locs = locset_new (locs->nlocs);
  int rdidx = 0;
  char rdletter = 'a', rddigit = '0';
  for (struct ddsi_wr_prd_match *m = ddsrt_avl_iter_first (&ddsi_wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_proxy_reader *prd;
    struct ddsi_addrset *ass[] = { NULL, NULL, NULL };
    bool increment_rdidx = true;
    if ((prd = ddsi_entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
      continue;
    ass[0] = prd->c.as;
#ifdef DDS_HAS_SSM
    if (prd->favours_ssm && wr->supports_ssm)
      ass[1] = wr->ssm_as;
#endif
    for (int i = 0; ass[i]; i++)
    {
      work_locs->nlocs = locs->nlocs;
      wras_flatten_locs_prealloc (work_locs, ass[i]);
      const int nloopback = move_loopback_forward (gv, work_locs);
      GVTRACE ("nloopback = %d, nlocs = %d, redundant_networking = %d\n", nloopback, work_locs->nlocs, prd->redundant_networking);
      if (!prd->redundant_networking || nloopback == work_locs->nlocs)
      {
        cover_makeroom (&cov, rdidx);
        for (int j = 0; j < work_locs->nlocs; j++)
        {
          if (!wras_cover_locatorset (gv, cov, locs, work_locs, rdidx, nloopback, j, j))
            goto addrset_changed;
        }
      }
      else
      {
        int j = nloopback;
        while (j < work_locs->nlocs)
        {
          cover_makeroom (&cov, rdidx);
          if (nloopback > 0)
          {
            if (!wras_cover_locatorset (gv, cov, locs, work_locs, rdidx, nloopback, 0, nloopback - 1))
              goto addrset_changed;
          }
          int k = j + 1;
          while (k < work_locs->nlocs && work_locs->locs[j].conn == work_locs->locs[k].conn)
            k++;
          GVTRACE ("j = %d, k = %d\n", j, k);
          if (!wras_cover_locatorset (gv, cov, locs, work_locs, rdidx, nloopback, j, k - 1))
            goto addrset_changed;
          j = k;
          for (int l = 0; l < cov->nlocs; l++)
            if (cover_get (cov, rdidx, l) == 0xff)
              cover_set (cov, rdidx, l, CI_NOMATCH);
          cov->rdnames[rdidx][0] = rdletter;
          cov->rdnames[rdidx][1] = rddigit++;
          cov->rdnames[rdidx][2] = 0;
          rdidx++;
          increment_rdidx = false;
        }
      }
    }
    if (increment_rdidx)
    {
      cover_makeroom (&cov, rdidx);
      for (int i = 0; i < cov->nlocs; i++)
        if (cover_get (cov, rdidx, i) == 0xff)
          cover_set (cov, rdidx, i, CI_NOMATCH);
      cov->rdnames[rdidx][0] = rdletter;
      cov->rdnames[rdidx][1] = 0;
      rdidx++;
    }
    if (++rdletter == 'z')
      rdletter = 'a';
    rddigit = '0';
  }
  locset_free (work_locs);
  if (rdidx == 0)
  {
    cover_free (cov);
    *pcov = NULL;
  }
  else
  {
    cover_update_nreaders (cov, rdidx);
    *pcov = cov;
  }
  return true;

addrset_changed:
  locset_free (work_locs);
  cover_free (cov);
  return false;
}

static struct costmap *wras_calc_costmap (const struct locset *locs, const struct cover *covered, dds_locator_mask_t ignore)
{
  const int nlocs = cover_get_nlocs (covered);
  struct costmap *wm = costmap_new (nlocs);
  for (int i = 0; i < nlocs; i++)
    costmap_set (wm, i, calc_locator_cost (locs, covered, i, ignore));
  return wm;
}

static void wras_trace_cover (const struct ddsi_domaingv *gv, const struct locset *locs, const struct costmap *wm, const struct cover *covered)
{
  if (!(gv->logconfig.c.mask & DDS_LC_DISCOVERY))
    return;
  const int nreaders = cover_get_nreaders (covered);
  const int nlocs = cover_get_nlocs (covered);
  assert (nlocs == locs->nlocs);
  GVLOGDISC ("  %61s", "");
  for (int i = 0; i < nreaders; i++)
    GVLOGDISC (" %3s", covered->rdnames[i]);
  GVLOGDISC ("\n");
  for (int i = 0; i < nlocs; i++)
  {
    char buf[DDSI_LOCSTRLEN];
    ddsi_xlocator_to_string (buf, sizeof(buf), &locs->locs[i]);
    GVLOGDISC ("  loc %2d = %-40s%11"PRId32" {", i, buf, costmap_get (wm, i).cost);
    for (int j = 0; j < nreaders; j++)
    {
      cover_info_t ci = cover_get (covered, j, i);
      if ((ci & CI_STATUS_MASK) == CI_NOMATCH)
        GVLOGDISC ("  ..");
      else
      {
        assert ((ci & CI_STATUS_MASK) == CI_INCLUDED || (ci & CI_STATUS_MASK) == CI_REACHABLE);
        if ((ci & CI_STATUS_MASK) == CI_INCLUDED)
          GVLOGDISC (" *");
        else
          GVLOGDISC (" +");
        if ((ci & ~CI_STATUS_MASK) == CI_ICEORYX)
          GVLOGDISC ("I ");
        else
        {
          if ((ci & CI_MULTICAST_MASK) == 0)
            GVLOGDISC ("u");
          else
            GVLOGDISC ("%d", ci >> CI_MULTICAST_SHIFT);
          GVLOGDISC ("%c", (ci & CI_LOOPBACK) ? 'l' : ' ');
        }
      }
    }
    GVLOGDISC (" }\n");
  }
}

static int wras_choose_locator (const struct locset *locs, const struct costmap *wm)
{
  // general preference for unicast is by having a larger base cost for a multicast
  // general preference for loopback is by having a cost for non-loopback interfaces
  // prefer_multicast: done by assigning much greater cost to unicast than to multicast
  // "reader favours SSM": slightly lower cost than ASM (it only "favours" it, after all)
  if (locs->nlocs == 0)
    return INT32_MIN;
  int best = 0;
  readercount_cost_t w_best = costmap_get (wm, best);
  for (int i = 1; i < locs->nlocs; i++)
  {
    const readercount_cost_t w_i = costmap_get (wm, i);
    if (w_i.cost < w_best.cost || (w_i.cost == w_best.cost && w_i.nrds > w_best.nrds))
    {
      best = i;
      w_best = w_i;
    }
  }
  return (w_best.cost != INT32_MAX) ? best : INT32_MIN;
}

static void wras_add_locator (const struct ddsi_domaingv *gv, struct ddsi_addrset *newas, int locidx, const struct locset *locs, const struct cover *covered)
{
  ddsi_xlocator_t tmploc;
  char str[DDSI_LOCSTRLEN];
  const char *kindstr;
  const ddsi_xlocator_t *locp;

  if (locs->locs[locidx].c.kind != DDSI_LOCATOR_KIND_UDPv4MCGEN)
  {
    locp = &locs->locs[locidx];
    kindstr = "simple";
  }
  else /* convert MC gen to the correct multicast address */
  {
    const int nreaders = cover_get_nreaders (covered);
    ddsi_udpv4mcgen_address_t l1;
    uint32_t iph, ipn;
    int i;
    tmploc = locs->locs[locidx];
    memcpy (&l1, tmploc.c.address, sizeof (l1));
    tmploc.c.kind = DDSI_LOCATOR_KIND_UDPv4;
    memset (tmploc.c.address, 0, 12);
    iph = ntohl (l1.ipv4.s_addr);
    for (i = 0; i < nreaders; i++)
    {
      cover_info_t ci = cover_get (covered, i, locidx);
      if ((ci & CI_STATUS_MASK) == CI_REACHABLE)
        iph |= 1u << ((ci >> CI_MULTICAST_SHIFT) - CI_MULTICAST_MCGEN_OFFSET);
    }
    ipn = htonl (iph);
    memcpy (tmploc.c.address + 12, &ipn, 4);
    locp = &tmploc;
    kindstr = "mcgen";
  }

  GVLOGDISC ("  %s %s\n", kindstr, ddsi_xlocator_to_string (str, sizeof(str), locp));
  if (locp->c.kind != DDSI_LOCATOR_KIND_SHEM)
  {
    // Iceoryx offload occurs above the RTPS stack, adding it to the address only means
    // samples get packed into RTPS messages and the transmit path is traversed without
    // actually sending any packet.  It should be generalized to handle various pub/sub
    // providers.
    ddsi_add_xlocator_to_addrset (gv, newas, locp);
  }
}

static void wras_drop_covered_readers (int locidx, struct costmap *wm, struct cover *covered)
{
  /* readers covered by this locator no longer matter */
  const int nreaders = cover_get_nreaders (covered);
  const int nlocs = cover_get_nlocs (covered);
  for (int i = 0; i < nreaders; i++)
  {
    const cover_info_t ci_rd_loc = cover_get (covered, i, locidx);
    if ((ci_rd_loc & CI_STATUS_MASK) != CI_REACHABLE)
      continue;
    for (int j = 0; j < nlocs; j++)
    {
      cover_info_t ci = cover_get (covered, i, j);
      if ((ci & CI_STATUS_MASK) == CI_REACHABLE)
      {
        cover_set (covered, i, j, (cover_info_t) ((ci & ~CI_STATUS_MASK) | CI_INCLUDED));
        // from reachable to included -> cost goes from "delivered" to "discarded"
        const int32_t cost =
          ((ci_rd_loc & ~CI_STATUS_MASK) == CI_ICEORYX) ? cost_redundant_iceoryx : cost_discarded;
        costmap_adjust (wm, j, cost - cost_delivered);
      }
    }
  }
}

struct ddsi_addrset *ddsi_compute_writer_addrset (const struct ddsi_writer *wr)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  struct locset *locs;
  struct cover *covered;
  struct ddsi_addrset *newas;

  // Gather all addresses, using an addrset means no need to worry about
  // duplicates. If no addresses found it is trivial.
  {
    struct ddsi_addrset *all_addrs = wras_collect_all_locs (wr);
    if (ddsi_addrset_empty (all_addrs))
      return all_addrs;
    ddsi_log_addrset (gv, DDS_LC_DISCOVERY, "setcover: all_addrs", all_addrs);
    ELOGDISC (wr, "\n");
    locs = wras_calc_locators (&gv->logconfig, all_addrs);
    ddsi_unref_addrset (all_addrs);
  }

  if (!wras_calc_cover (wr, locs, &covered))
  {
    // Addrset computation fails when some proxy reader's address can't be found in all_addrs,
    // which means its address set changed while we were working.  In that case, the change
    // will trigger a recalculation and we can just return the old one.
    //
    // FIXME: copying it is a bit excessive (a little rework and refcount manipulation suffices)
    newas = ddsi_ref_addrset (wr->as);
  }
  else if (covered == NULL)
  {
    // No readers, no need to do anything else
    newas = ddsi_new_addrset ();
  }
  else
  {
    assert(wr->xqos->present & DDSI_QP_LOCATOR_MASK);
    struct costmap *wm = wras_calc_costmap (locs, covered, wr->xqos->ignore_locator_type);
    int best;
    newas = ddsi_new_addrset ();
    while ((best = wras_choose_locator (locs, wm)) > INT32_MIN)
    {
      wras_trace_cover (gv, locs, wm, covered);
      ELOGDISC (wr, "  best = %d\n", best);
      wras_add_locator (gv, newas, best, locs, covered);
      wras_drop_covered_readers (best, wm, covered);
    }
    costmap_free (wm);
    cover_free (covered);
  }
  locset_free (locs);
  return newas;
}
