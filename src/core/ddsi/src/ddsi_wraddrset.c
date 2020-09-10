/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include <assert.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_bitset.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_wraddrset.h"

#include "dds/ddsi/ddsi_udp.h" /* nn_mc4gen_address_t */

// For each (reader, locator) pair, the coverage map gives:
// -1 if the reader isn't covered by this locator, >= 0 if it is
//
// If covered, the value depends on the kind of locator:
// - for regular locators: 0
// - for MCGEN locators:   bit position in address
typedef uint8_t cover_info_t;

struct cover {
  int nreaders;
  int nlocs;
  char rdnames[250][3];
  cover_info_t m[]; // [nreaders][nlocs]
};

static struct cover *cover_new (int nreaders, int nlocs)
{
  struct cover *c = ddsrt_malloc (sizeof (*c) + (uint32_t) nlocs * (uint32_t) nreaders * sizeof (*c->m));
  assert (nreaders <= 250); // because of rdnames hack
  c->nreaders = nreaders;
  c->nlocs = nlocs;
  for (int i = 0; i < nreaders; i++)
    for (int j = 0; j < nlocs; j++)
      c->m[i * nlocs + j] = 0xff;
  return c;
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

typedef int32_t weight_t;

struct weightmap {
  int nlocs;
  weight_t m[];
};

static struct weightmap *weightmap_new (int nlocs)
{
  struct weightmap *wm = ddsrt_malloc (sizeof (*wm) + (uint32_t) nlocs * sizeof (*wm->m));
  wm->nlocs = nlocs;
  for (int j = 0; j < nlocs; j++)
    wm->m[j] = 0;
  return wm;
}

static void weightmap_free (struct weightmap *wm)
{
  ddsrt_free (wm);
}

static void weightmap_set (struct weightmap *wm, int lidx, weight_t v)
{
  assert (lidx < wm->nlocs);
  wm->m[lidx] = v;
}

#if 0
static void weightmap_dec (struct weightmap *wm, int lidx)
{
  assert (lidx < wm->nlocs);
  assert (wm->m[lidx] > 0);
  wm->m[lidx]--;
}
#endif

static weight_t weightmap_get (const struct weightmap *wm, int lidx)
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
    set_unspec_xlocator (&ls->locs[j]);
  return ls;
}

static void locset_free (struct locset *ls)
{
  ddsrt_free (ls);
}

static struct addrset *wras_collect_all_locs (const struct writer *wr)
{
  struct entity_index * const gh = wr->e.gv->entity_index;
  struct addrset *all_addrs = new_addrset ();
  struct wr_prd_match *m;
  ddsrt_avl_iter_t it;
  for (m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct proxy_reader *prd;
    if ((prd = entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
      continue;
    copy_addrset_into_addrset (wr->e.gv, all_addrs, prd->c.as);
  }
  if (!addrset_empty (all_addrs))
  {
#ifdef DDS_HAS_SSM
    if (wr->supports_ssm && wr->ssm_as)
      copy_addrset_into_addrset_mc (wr->e.gv, all_addrs, wr->ssm_as);
#endif
    return all_addrs;
  }
  else
  {
    unref_addrset (all_addrs);
    return NULL;
  }
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

static void wras_flatten_locs_prealloc (struct locset *ls, struct addrset *addrs)
{
  struct rebuild_flatten_locs_helper_arg flarg;
  flarg.locs = ls->locs;
  flarg.idx = 0;
#ifndef NDEBUG
  flarg.size = ls->nlocs;
#endif
  addrset_forall (addrs, wras_flatten_locs_helper, &flarg);
  ls->nlocs = flarg.idx;
}

static struct locset *wras_flatten_locs (struct addrset *all_addrs)
{
  const int nin = (int) addrset_count (all_addrs);
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
  const ddsi_locator_t *a = va;
  const ddsi_locator_t *b = vb;
  if (a->kind != b->kind || a->kind != NN_LOCATOR_KIND_UDPv4MCGEN)
    return compare_locators (a, b);
  else
  {
    ddsi_locator_t u = *a, v = *b;
    nn_udpv4mcgen_address_t *u1 = (nn_udpv4mcgen_address_t *) u.address;
    nn_udpv4mcgen_address_t *v1 = (nn_udpv4mcgen_address_t *) v.address;
    u1->idx = v1->idx = 0;
    return compare_locators (&u, &v);
  }
}

static struct locset *wras_calc_locators (const struct ddsrt_log_cfg *logcfg, struct addrset *all_addrs)
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
#define CI_MULTICAST_MASK 0xf8 // 0x8 if regular multicast, (index+1) in MCGEN
#define CI_MULTICAST_SHIFT   3

#define CI_ICEORYX        0xfc // FIXME: this is a hack

static weight_t sat_weight_add (weight_t x, int32_t a)
{
  DDSRT_STATIC_ASSERT (sizeof (weight_t) == sizeof (int32_t));
  if (a >= 0)
    return (x > INT32_MAX - a) ? INT32_MAX : x + a;
  else
    return (x < INT32_MIN - a) ? INT32_MIN : x + a;
}

static weight_t calc_locator_weight (const struct cover *c, int lidx, bool prefer_multicast)
{
  const int32_t cost_uc = prefer_multicast ? 1000000 : 2;
  const int32_t cost_mc = prefer_multicast ? 0 : 3;
  const int32_t cost_discarded = 1;
  const int32_t cost_delivered = -1;
  const int32_t cost_non_loopback = 2;
  weight_t weight = 0;
  bool valid_choice = false;

  // FIXME: should associate costs with interfaces, so that, e.g., iceoryx << loopback < GbE < WiFi without any details needed here

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
    return INT32_MAX;

  if ((ci & ~CI_STATUS_MASK) == CI_ICEORYX)
    weight = INT32_MIN;
  else if ((ci & CI_MULTICAST_MASK) == 0)
    weight += cost_uc;
  else
    weight += cost_mc;
  if (!(ci & CI_LOOPBACK))
    weight += cost_non_loopback;

  for (; rdidx < c->nreaders; rdidx++)
  {
    ci = cover_get (c, rdidx, lidx);
    if ((ci & CI_STATUS_MASK) == CI_NOMATCH)
      continue;

    if ((ci & CI_STATUS_MASK) == CI_INCLUDED)
      weight = sat_weight_add (weight, cost_discarded); // FIXME: need addressed hosts, addressed processes
    else
    {
      assert ((ci & CI_STATUS_MASK) == CI_REACHABLE);
      weight = sat_weight_add (weight, cost_delivered);
      valid_choice = true;
    }
  }
  if (weight == INT32_MAX)
    weight = INT32_MAX - 1;
  return valid_choice ? weight : INT32_MAX;
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

static bool locator_is_iceoryx (const ddsi_xlocator_t *l)
{
#ifdef DDS_HAS_SHM
  return l->loc.kind == NN_LOCATOR_KIND_SHEM;
#else
  (void) l;
  return false;
#endif
}

static void wras_cover_locatorset (struct ddsi_domaingv const * const gv, struct cover *cov, const struct locset *locs, const struct locset *work_locs, int rdidx, int nloopback, int first, int last)
{
  for (int j = first; j <= last; j++)
  {
    /* all addresses should be in the combined set of addresses -- FIXME: this doesn't hold if the address sets can change */
    const ddsi_xlocator_t *l = bsearch (&work_locs->locs[j], locs->locs, (size_t) locs->nlocs, sizeof (*locs->locs), wras_compare_locs);
    cover_info_t x;
    assert (l != NULL);
    int lidx = (int) (l - locs->locs);
    if (locator_is_iceoryx (l)) // FIXME: a gross hack
    {
      x = CI_ICEORYX;
    }
    else if (l->loc.kind == NN_LOCATOR_KIND_UDPv4MCGEN)
    {
      const nn_udpv4mcgen_address_t *l1 = (const nn_udpv4mcgen_address_t *) l->loc.address;
      assert (l1->base + l1->idx <= 30);
      x = (cover_info_t) ((1 + l1->base + l1->idx) << CI_MULTICAST_SHIFT);
    }
    else
    {
      x = 0;
      if (j < nloopback)
        x |= CI_LOOPBACK;
      if (ddsi_is_mcaddr (gv, &l->loc))
        x |= 1 << CI_MULTICAST_SHIFT;
    }
    assert (x != 0xff);
    assert (cover_get (cov, rdidx, lidx) == 0xff);
    cover_set (cov, rdidx, lidx, x);
  }
}

static struct cover *wras_calc_cover (const struct writer *wr, const struct locset *locs)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  struct entity_index * const gh = gv->entity_index;
  ddsrt_avl_iter_t it;
  struct cover *cov = cover_new (3 * (int) wr->num_readers, locs->nlocs); // FIXME: *3 allows up to 3 i/f per reader for redundant networking, but should count properly
  struct locset *work_locs = locset_new (locs->nlocs);
  int rdidx = 0;
  char rdletter = 'a', rddigit = '0';
  for (struct wr_prd_match *m = ddsrt_avl_iter_first (&wr_readers_treedef, &wr->readers, &it); m; m = ddsrt_avl_iter_next (&it))
  {
    struct proxy_reader *prd;
    struct addrset *ass[] = { NULL, NULL, NULL };
    bool increment_rdidx = true;
    assert (rdidx < 3 * (int) wr->num_readers); // FIXME: *3 hack
    if ((prd = entidx_lookup_proxy_reader_guid (gh, &m->prd_guid)) == NULL)
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
        for (int j = 0; j < work_locs->nlocs; j++)
          wras_cover_locatorset (gv, cov, locs, work_locs, rdidx, nloopback, j, j);
      }
      else
      {
        int j = nloopback;
        while (j < work_locs->nlocs)
        {
          if (nloopback > 0)
            wras_cover_locatorset (gv, cov, locs, work_locs, rdidx, nloopback, 0, nloopback - 1);
          int k = j + 1;
          while (k < work_locs->nlocs && work_locs->locs[j].conn == work_locs->locs[k].conn)
            k++;
          GVTRACE ("j = %d, k = %d\n", j, k);
          wras_cover_locatorset (gv, cov, locs, work_locs, rdidx, nloopback, j, k - 1);
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
    return NULL;
  }
  else
  {
    cover_update_nreaders (cov, rdidx);
    return cov;
  }
}

static struct weightmap *wras_calc_weightmap (const struct cover *covered, bool prefer_multicast)
{
  const int nlocs = cover_get_nlocs (covered);
  struct weightmap *wm = weightmap_new (nlocs);
  for (int i = 0; i < nlocs; i++)
    weightmap_set (wm, i, calc_locator_weight (covered, i, prefer_multicast));
  return wm;
}

static void wras_trace_cover (const struct ddsi_domaingv *gv, const struct locset *locs, const struct weightmap *wm, const struct cover *covered)
{
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
    GVLOGDISC ("  loc %2d = %-40s%11"PRId32" {", i, buf, weightmap_get (wm, i));
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

static int wras_choose_locator (const struct ddsi_domaingv *gv, const struct locset *locs, const struct weightmap *wm, bool prefer_multicast)
{
  (void) gv; (void) prefer_multicast; // FIXME: do the right thing rather ignore the warning
#if 0
  int i, j;
  if (locs->nlocs == 0)
    return -1;
  for (j = 0, i = 1; i < locs->nlocs; i++) {
    const weight_t w_i = weightmap_get (wm, i);
    const weight_t w_j = weightmap_get (wm, j);
    if (prefer_multicast && w_i > 0 && ddsi_is_mcaddr(gv, &locs->locs[i]) && !ddsi_is_mcaddr(gv, &locs->locs[j]))
      j = i; /* obviously first step must be to try and avoid unicast if configuration says so */
    else if (w_i > w_j)
      j = i; /* better coverage */
    else if (w_i == w_j)
    {
      if (w_i == 1 && !ddsi_is_mcaddr(gv, &locs->locs[i]))
        j = i; /* prefer unicast for single nodes */
#if DDS_HAS_SSM
      else if (ddsi_is_ssm_mcaddr(gv, &locs->locs[i]))
        j = i; /* "reader favours SSM": all else being equal, use SSM */
#endif
    }
  }
  return (weightmap_get (wm, j) > 0) ? j : -1;
#else
  int i, j;
  if (locs->nlocs == 0)
    return -1;
  for (j = 0, i = 1; i < locs->nlocs; i++)
  {
    const weight_t w_i = weightmap_get (wm, i);
    const weight_t w_j = weightmap_get (wm, j);
    if (w_i < w_j)
      j = i;
  }
  return (weightmap_get (wm, j) != INT32_MAX) ? j : -1;
#endif
}

static void wras_add_locator (const struct ddsi_domaingv *gv, struct addrset *newas, int locidx, const struct locset *locs, const struct cover *covered)
{
  ddsi_xlocator_t tmploc;
  char str[DDSI_LOCSTRLEN];
  const char *kindstr;
  const ddsi_xlocator_t *locp;

  if (locs->locs[locidx].loc.kind != NN_LOCATOR_KIND_UDPv4MCGEN)
  {
    locp = &locs->locs[locidx];
    kindstr = "simple";
  }
  else /* convert MC gen to the correct multicast address */
  {
    const int nreaders = cover_get_nreaders (covered);
    nn_udpv4mcgen_address_t l1;
    uint32_t iph, ipn;
    int i;
    tmploc = locs->locs[locidx];
    memcpy (&l1, tmploc.loc.address, sizeof (l1));
    tmploc.loc.kind = NN_LOCATOR_KIND_UDPv4;
    memset (tmploc.loc.address, 0, 12);
    iph = ntohl (l1.ipv4.s_addr);
    for (i = 0; i < nreaders; i++)
    {
      cover_info_t ci = cover_get (covered, i, locidx);
      if ((ci & CI_STATUS_MASK) == CI_REACHABLE)
        iph |= 1u << ((ci >> CI_MULTICAST_SHIFT) - 1);
    }
    ipn = htonl (iph);
    memcpy (tmploc.loc.address + 12, &ipn, 4);
    locp = &tmploc;
    kindstr = "mcgen";
  }

  GVLOGDISC ("  %s %s\n", kindstr, ddsi_xlocator_to_string (str, sizeof(str), locp));
  add_xlocator_to_addrset (gv, newas, locp);
}

static void wras_drop_covered_readers (int locidx, struct weightmap *wm, struct cover *covered, bool prefer_multicast)
{
  /* readers covered by this locator no longer matter */
  const int nreaders = cover_get_nreaders (covered);
  const int nlocs = cover_get_nlocs (covered);
  for (int i = 0; i < nreaders; i++)
  {
    if ((cover_get (covered, i, locidx) & CI_STATUS_MASK) != CI_REACHABLE)
      continue;
    for (int j = 0; j < nlocs; j++)
    {
      cover_info_t ci = cover_get (covered, i, j);
      if ((ci & CI_STATUS_MASK) == CI_REACHABLE)
        cover_set (covered, i, j, (cover_info_t) ((ci & ~CI_STATUS_MASK) | CI_INCLUDED));
    }
  }
  for (int j = 0; j < nlocs; j++)
  {
    weightmap_set (wm, j, calc_locator_weight (covered, j, prefer_multicast));
  }
}

static void wras_setcover (struct addrset *newas, const struct writer *wr)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  const bool prefer_multicast = gv->config.prefer_multicast;
  struct addrset *all_addrs;
  struct locset *locs;
  struct weightmap *wm;
  struct cover *covered;
  int best;
  if ((all_addrs = wras_collect_all_locs (wr)) == NULL)
    return;
  nn_log_addrset (gv, DDS_LC_DISCOVERY, "setcover: all_addrs", all_addrs);
  ELOGDISC (wr, "\n");
  locs = wras_calc_locators (&gv->logconfig, all_addrs);
  unref_addrset (all_addrs);
  if ((covered = wras_calc_cover (wr, locs)) == NULL)
    goto done;
  wm = wras_calc_weightmap (covered, prefer_multicast);
  while ((best = wras_choose_locator (gv, locs, wm, prefer_multicast)) >= 0)
  {
    wras_trace_cover (gv, locs, wm, covered);
    ELOGDISC (wr, "  best = %d\n", best);
    wras_add_locator (gv, newas, best, locs, covered);
    wras_drop_covered_readers (best, wm, covered, prefer_multicast);
  }
  weightmap_free (wm);
 done:
  locset_free (locs);
  cover_free (covered);
}

struct addrset *compute_writer_addrset (const struct writer *wr)
{
  struct addrset *as = new_addrset ();
  wras_setcover (as, wr);
  return as;
}
