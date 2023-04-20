// Copyright(c) 2022 ZettaScale Technology BV
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/environ.h"
#include "dds__entity.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "ddsi__lease.h"
#include "ddsi__entity_index.h"
#include "dds/dds.h"

#include "test_common.h"

struct guidstr { char s[4*8+4]; };

static char *guidstr (struct guidstr *dst, const dds_guid_t *g)
{
  const uint8_t *v = g->v;
  snprintf (dst->s, sizeof (dst->s), "%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x:%02x%02x%02x%02x",
            v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], v[12], v[13], v[14], v[15]);
  return dst->s;
}

static void participant_lease_duration_make_doms (dds_duration_t ldur2)
{
  const char *conf_fmt = "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<Discovery>\
  <Tag>${CYCLONEDDS_PID}</Tag>\
  <ExternalDomainId>0</ExternalDomainId>\
  <LeaseDuration>%"PRId64"ns</LeaseDuration>\
</Discovery>";
  char *conf_ld_def, *conf_ld_alt;
  (void) ddsrt_asprintf (&conf_ld_def, conf_fmt, DDS_SECS (10));
  (void) ddsrt_asprintf (&conf_ld_alt, conf_fmt, ldur2);
  char *xconf_ld_def = ddsrt_expand_envvars (conf_ld_def, DDS_DOMAIN_DEFAULT);
  const dds_entity_t dom0 = dds_create_domain (0, xconf_ld_def);
  CU_ASSERT_FATAL (dom0 > 0);
  const dds_entity_t dom1 = dds_create_domain (1, xconf_ld_def);
  CU_ASSERT_FATAL (dom1 > 0);
  char *xconf_ld_alt = ddsrt_expand_envvars (conf_ld_alt, DDS_DOMAIN_DEFAULT);
  const dds_entity_t dom2 = dds_create_domain (2, xconf_ld_alt);
  CU_ASSERT_FATAL (dom2 > 0);
  ddsrt_free (xconf_ld_def);
  ddsrt_free (xconf_ld_alt);
  ddsrt_free (conf_ld_def);
  ddsrt_free (conf_ld_alt);
}

static void check_lease_duration (const dds_qos_t *qos, dds_duration_t exp_ldur)
{
  dds_liveliness_kind_t kind;
  dds_duration_t ldur;
  const bool lpresent = dds_qget_liveliness (qos, &kind, &ldur);
  CU_ASSERT_FATAL (lpresent);
  CU_ASSERT_FATAL (kind == DDS_LIVELINESS_AUTOMATIC);
  CU_ASSERT_FATAL (ldur == exp_ldur);
}

static void check_lease_duration_pp (dds_entity_t pp, dds_duration_t exp_ldur)
{
  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_return_t ret = dds_get_qos (pp, qos);
  CU_ASSERT_FATAL (ret == 0);
  check_lease_duration (qos, exp_ldur);
  dds_delete_qos (qos);
}

CU_Test(ddsc_participant_lease_duration, invalid_setting)
{
  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_entity_t pp;
  dds_qset_liveliness (qos, DDS_LIVELINESS_AUTOMATIC, -1);
  pp = dds_create_participant (DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL (pp == DDS_RETCODE_BAD_PARAMETER);
  dds_qset_liveliness (qos, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 1);
  pp = dds_create_participant (DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL (pp == DDS_RETCODE_BAD_PARAMETER);
  dds_qset_liveliness (qos, DDS_LIVELINESS_MANUAL_BY_TOPIC, 1);
  pp = dds_create_participant (DDS_DOMAIN_DEFAULT, qos, NULL);
  CU_ASSERT_FATAL (pp == DDS_RETCODE_BAD_PARAMETER);
  dds_delete_qos (qos);
}

static void participant_lease_duration_make_pps (dds_entity_t pp[3], dds_guid_t ppg[3], const dds_duration_t ldur[3])
{
  participant_lease_duration_make_doms (ldur[2]);

  dds_qos_t *qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  // pp[0] is our "primary" participant, pp[1] and pp[2] are for checking
  // network-related things; pp[1] has lease dur set via QoS, pp[2] via config
  for (int i = 0; i < 3; i++)
  {
    dds_qset_liveliness (qos, DDS_LIVELINESS_AUTOMATIC, ldur[i]);
    pp[i] = dds_create_participant ((dds_domainid_t) i, (i == 2) ? NULL : qos, NULL);
    CU_ASSERT_FATAL (pp[i] > 0);
    dds_return_t ret = dds_get_guid (pp[i], &ppg[i]);
    CU_ASSERT_FATAL (ret == 0);
    check_lease_duration_pp (pp[i], ldur[i]);
  }
  dds_delete_qos (qos);
}

CU_Test(ddsc_participant_lease_duration, builtin_topic)
{
  dds_entity_t pp[3];
  dds_guid_t ppg[3];
  dds_entity_t rd;
  dds_return_t ret;
  const dds_duration_t ldur[3] = { 999999937, 1000000007, 1000000009 };
  participant_lease_duration_make_pps (pp, ppg, ldur);

  rd = dds_create_reader (pp[0], DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);
  ret = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (ret == 0);

  dds_entity_t ws = dds_create_waitset (pp[0]);
  CU_ASSERT_FATAL (ws > 0);
  ret = dds_waitset_attach (ws, rd, 0);
  CU_ASSERT_FATAL (ret == 0);

  int nseen = 0;
  bool seen[3] = { false };
  const dds_time_t abstimeout = dds_time () + DDS_SECS (5);
  while (nseen != 3 && (ret = dds_waitset_wait_until (ws, NULL, 0, abstimeout) > 0))
  {
    dds_sample_info_t si;
    int32_t n;
    void *raw = NULL;
    while ((n = dds_take (rd, &raw, &si, 1, 1)) == 1)
    {
      dds_builtintopic_participant_t const * const s = raw;
      int i;
      for (i = 0; i < 3; i++)
        if (memcmp (&ppg[i], &s->key, sizeof (s->key)) == 0)
          break;
      CU_ASSERT_FATAL (i < 3); // thanks to domain tag
      assert (i < 3); // Clang static analyzer doesn't get CU_ASSERT_FATAL
      if (!si.valid_data)
        continue;
      nseen += !seen[i];
      seen[i] = true;

      check_lease_duration (s->qos, ldur[i]);
      ret = dds_return_loan (rd, &raw, 1);
      CU_ASSERT_FATAL (ret == 0);
    }
    CU_ASSERT_FATAL (n == 0);
  }
  CU_ASSERT_FATAL (ret >= 0);
  CU_ASSERT_FATAL (nseen == 3);
  ret = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ret == 0);
}

static int64_t sub_tref_et (int64_t t_v, ddsrt_etime_t tref)
{
  return (t_v == INT64_MIN) ? t_v : t_v - tref.v;
}

static bool make_pp0_deaf (const dds_entity_t pp[3], const dds_guid_t ppg[3], const ddsrt_etime_t tref_et)
{
  const ddsrt_etime_t tdeaf_et = ddsrt_time_elapsed ();
  dds_return_t ret;
  // renew the leases for the proxy participants so we know more about when the lease will expire
  struct dds_entity *ppe;
  bool lax_check = false;
  ret = dds_entity_pin (pp[0], &ppe);
  CU_ASSERT_FATAL (ret == 0);
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &ppe->m_domain->gv);
  for (int i = 1; i < 3; i++)
  {
    DDSRT_STATIC_ASSERT (sizeof (dds_guid_t) == sizeof (ddsi_guid_t));
    ddsi_guid_t tmp;
    memcpy (&tmp, &ppg[i], sizeof (tmp));
    tmp = ddsi_ntoh_guid (tmp);
    struct ddsi_proxy_participant *proxypp = ddsi_entidx_lookup_proxy_participant_guid (ppe->m_domain->gv.entity_index, &tmp);
    if (proxypp == NULL) {
      // there's always the possibility that adverse timing means it expired just now
      lax_check = true;
    } else {
      struct ddsi_lease *lease;
      if ((lease = ddsrt_atomic_ldvoidp (&proxypp->minl_auto)) != NULL)
      {
        const int64_t old_tend = sub_tref_et ((int64_t) ddsrt_atomic_ld64 (&lease->tend), tref_et);
        const int64_t old_tsched_unsafe = sub_tref_et (((volatile ddsrt_etime_t *) &lease->tsched)->v, tref_et);
        ddsi_lease_renew (lease, tdeaf_et);
        const int64_t new_tend = sub_tref_et ((int64_t) ddsrt_atomic_ld64 (&lease->tend), tref_et);
        const int64_t new_tsched_unsafe = sub_tref_et (((volatile ddsrt_etime_t *) &lease->tsched)->v, tref_et);
        struct guidstr gs;
        tprintf ("%d renewing proxy %s lease (end %"PRId64", sched %"PRId64") -> (%"PRId64", %"PRId64")\n",
                 i, guidstr (&gs, &ppg[i]), old_tend, old_tsched_unsafe, new_tend, new_tsched_unsafe);
      }
    }
  }
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  dds_entity_unpin (ppe);
  // make pp[0] deaf
  tprintf ("making pp0 deaf @ %"PRId64"\n", sub_tref_et (tdeaf_et.v, tref_et));
  ret = dds_domain_set_deafmute (pp[0], true, false, DDS_INFINITY);
  CU_ASSERT_FATAL (ret == 0);
  return lax_check;
}

static int32_t read_with_timeout1 (dds_entity_t rd, void *raw[], dds_sample_info_t si[], uint32_t maxn, int32_t n, dds_instance_state_t reqistate)
{
  if (n > 0)
    (void) dds_return_loan (rd, raw, n);
  n = dds_read_mask (rd, raw, si, maxn, maxn, reqistate | DDS_ANY_VIEW_STATE | DDS_ANY_SAMPLE_STATE);
  CU_ASSERT_FATAL (n >= 0);
  return n;
}

static int32_t countinst (const dds_sample_info_t *si, int32_t n)
{
  if (n == 0)
    return 0;
  int32_t ninst = 1;
  for (int32_t i = 1; i < n; i++)
    ninst += (si[i].instance_handle != si[i-1].instance_handle);
  return ninst;
}

struct read_with_timeout_result {
  bool no_timeout;
  int32_t nsamples;
  int32_t ninstances;
};

static struct read_with_timeout_result read_with_timeout (dds_entity_t rd, void **raw, dds_sample_info_t *si, uint32_t maxn, int32_t minninst, dds_instance_state_t reqistate, dds_duration_t abstimeout)
{
  // it doesn't make sense to wait for "read" to return an empty set, that's not how waitsets work
  assert (minninst > 0);
  const char *whatstr = (reqistate == DDS_ALIVE_INSTANCE_STATE) ? "alive" : "not-alive";

  dds_return_t ret;
  const dds_entity_t ws = dds_create_waitset (dds_get_participant (rd));
  CU_ASSERT_FATAL (ws > 0);
  const dds_entity_t rdcond = dds_create_readcondition (rd, reqistate | DDS_ANY_VIEW_STATE | DDS_NOT_READ_SAMPLE_STATE);
  CU_ASSERT_FATAL (rdcond > 0);
  ret = dds_waitset_attach (ws, rdcond, 0);
  CU_ASSERT_FATAL (ret == 0);

  int32_t n = 0, ninst = 0; // not equal to reqn
  dds_return_t wret = 1; // no timeout occurred yet
  while (wret != 0 && ninst < minninst)
  {
    n = read_with_timeout1 (rd, raw, si, maxn, n, reqistate);
    if ((ninst = countinst (si, n)) < minninst)
    {
      tprintf ("%s %"PRId32" samples %"PRId32" instances; still waiting\n", whatstr, n, ninst);
      wret = dds_waitset_wait_until (ws, NULL, 0, abstimeout);
      CU_ASSERT_FATAL (wret >= 0);
    }
  }
  if (wret == 0)
  {
    ddsrt_log_cfg_t logcfg;
    tprintf ("%s timed out\n", whatstr);
    dds_log_cfg_init (&logcfg, 0, ~0u, stdout, stdout);
    ddsi_log_stack_traces (&logcfg, NULL);
    n = read_with_timeout1 (rd, raw, si, maxn, n, reqistate);
    ninst = countinst (si, n);
  }

  ret = dds_delete (ws);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_delete (rdcond);
  CU_ASSERT_FATAL (ret == 0);

  tprintf ("%s %"PRId32" samples %"PRId32" instances\n", whatstr, n, ninst);
  for (int32_t i = 0; i < n; i++)
  {
    struct dds_builtintopic_participant const * const s = raw[i];
    struct guidstr gs;
    tprintf ("%"PRId32": %s iid %016"PRIx64" valid %d\n", i, guidstr (&gs, &s->key), si[i].instance_handle, si[i].valid_data);
  }
  return (struct read_with_timeout_result){ .no_timeout = (wret != 0), .nsamples = n, .ninstances = ninst };
}

CU_Test(ddsc_participant_lease_duration, expiry)
{
  dds_entity_t pp[3];
  dds_guid_t ppg[3];
  dds_entity_t rd;
  dds_return_t ret;
  const dds_duration_t ldur[3] = { 999999937, 1000000007, 1000000009 };
  participant_lease_duration_make_pps (pp, ppg, ldur);
  for (int i = 0; i < 3; i++)
  {
    struct guidstr gs;
    tprintf ("%d: %s\n", i, guidstr (&gs, &ppg[i]));
  }

  rd = dds_create_reader (pp[0], DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  // only have 3 participants, domain tag ensures no interference from other tests,
  // but making room for 4 means we can be more confident we ran in isolation
  // also need to allow for invalid samples
  void *raw[7] = { NULL };
  dds_sample_info_t si[7];
  const dds_time_t abstimeout = dds_time () + DDS_SECS (8);
  struct read_with_timeout_result rret;
  rret = read_with_timeout (rd, raw, si, 7, 3, DDS_ALIVE_INSTANCE_STATE, abstimeout);
  // all three should be alive now, no invalid samples
  CU_ASSERT_FATAL (rret.no_timeout && rret.ninstances == 3);
  ret = dds_return_loan (rd, raw, rret.nsamples);
  CU_ASSERT_FATAL (ret == 0);

  // make pp[0] deaf after forcing lease renewal
  const ddsrt_etime_t tref_et = ddsrt_time_elapsed ();
  tprintf ("tref_et = %"PRId64"\n", tref_et.v);
  const dds_time_t tdeaf = dds_time ();
  const bool lax_check = make_pp0_deaf (pp, ppg, tref_et);

  // wait for the two remote participants to go missing: we expect invalid samples for the state change
  // because we made sure to read all of them before
  rret = read_with_timeout (rd, raw, si, 7, 2, DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE, abstimeout);
  if (rret.ninstances != 2)
  {
    tprintf ("calling make_pp0_deaf for its debug output on lease expiry\n");
    make_pp0_deaf (pp, ppg, tref_et);
  }
  CU_ASSERT_FATAL (rret.no_timeout && rret.ninstances == 2);
  const dds_time_t texpire = dds_time ();
  ret = dds_return_loan (rd, raw, rret.nsamples);
  CU_ASSERT_FATAL (ret == 0);

  // must not expire too soon (unless lax_check says we really don't know)
  assert (ldur[1] <= ldur[2]);
  CU_ASSERT_FATAL (lax_check || texpire - tdeaf > ldur[1]);
  // must not have taken ridiculously long either (100ms margin is not enough on CI)
  CU_ASSERT_FATAL (texpire - tdeaf < ldur[2] + DDS_MSECS (300));
  ret = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (ret == 0);
}
