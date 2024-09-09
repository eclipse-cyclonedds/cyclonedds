// Copyright(c) 2024 ZettaScale Technology BV
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
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/process.h"
#include "dds__entity.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "ddsi__lease.h"
#include "ddsi__entity_index.h"
#include "ddsi__misc.h"
#include "dds/dds.h"

#include "test_common.h"

typedef struct { char str[DDSI_LOCSTRLEN]; } locstr_t;

static void get_localhost_address (locstr_t *str)
{
  const char *cyclonedds_uri = "";
  (void) ddsrt_getenv ("CYCLONEDDS_URI", &cyclonedds_uri);
  dds_entity_t eh = dds_create_domain (0, cyclonedds_uri);
  CU_ASSERT_FATAL (eh > 0);
  const struct ddsi_domaingv *gv = get_domaingv (eh);
  CU_ASSERT_FATAL (gv != NULL);
  ddsi_locator_to_string_no_port (str->str, sizeof (str->str), &gv->interfaces[0].loc);
  dds_return_t rc = dds_delete (eh);
  CU_ASSERT_FATAL (rc == 0);
}

static void get_nonexist_address (locstr_t *str)
{
  const char *cyclonedds_uri = "";
  (void) ddsrt_getenv ("CYCLONEDDS_URI", &cyclonedds_uri);
  dds_entity_t eh = dds_create_domain (0, cyclonedds_uri);
  CU_ASSERT_FATAL (eh > 0);
  const struct ddsi_domaingv *gv = get_domaingv (eh);
  CU_ASSERT_FATAL (gv != NULL);
  // No guarantee that this really yields a locator that doesn't point to an existing machine
  // running DDSI at the port number we use, but in combination with a random domain tag,
  // it hopefully probably works out ok
  ddsi_locator_t loc = gv->interfaces[0].loc;
  assert (loc.kind == DDSI_LOCATOR_KIND_UDPv4);
  if (loc.address[15] == 254)
    loc.address[15] = 1;
  else
    loc.address[15]++;
  ddsi_locator_to_string_no_port (str->str, sizeof (str->str), &loc);
  dds_return_t rc = dds_delete (eh);
  CU_ASSERT_FATAL (rc == 0);
}

// returns domain handle
static dds_entity_t make_domain_and_participant (uint32_t domainid, int base_port, enum ddsi_boolean_default allow_multicast, const char *spdp_address, const char *participant_index, bool add_localhost, const locstr_t *peer_address)
{
  const char *cyclonedds_uri = "";
  (void) ddsrt_getenv ("CYCLONEDDS_URI", &cyclonedds_uri);
  
  char *peers = NULL;
  if (add_localhost || peer_address)
  {
    ddsrt_asprintf (&peers, "\
<Peers %s>\
  %s%s%s\
</Peers>", 
                    add_localhost ? "addlocalhost=\"true\"" : "",
                    peer_address ? "<Peer address=\"" : "",
                    peer_address ? peer_address->str : "",
                    peer_address ? "\"/>" : "");
  }
  else
  {
    peers = ddsrt_strdup ("");
  }

  // MaxAutoParticipantIndex = 1: we never do more than 2 participants in this test, so this
  // results in one additional index that won't be used.  With the base port and unicast offsets
  // that also means a 10-port offset between tests makes each test use unique port numbers
  // outside the usual range.
  char *config = NULL;
  ddsrt_asprintf (&config, "%s,\
<Tracing>\
  <Category>trace</Category>\
</Tracing>\
<General>\
  <AllowMulticast>%s</>\
</General>\
<Discovery>\
  <Tag>%d</>\
  <Ports>\
    <Base>%d</>\
    <UnicastMetaOffset>2</>\
    <UnicastDataOffset>3</>\
  </>\
  <ExternalDomainId>0</>\
  <SPDPInterval>0.5s</>\
  <SPDPMulticastAddress>%s</>\
  <LeaseDuration>2s</>\
  <ParticipantIndex>%s</>\
  <MaxAutoParticipantIndex>2</>\
  <InitialLocatorPruneDelay>2s</>\
  <DiscoveredLocatorPruneDelay>2s</>\
  %s\
</Discovery>",
                  cyclonedds_uri,
                  (allow_multicast == DDSI_BOOLDEF_DEFAULT) ? "default" : (allow_multicast == DDSI_BOOLDEF_TRUE) ? "true" : "false",
                  (int) ddsrt_getpid (),
                  base_port,
                  spdp_address,
                  participant_index,
                  peers);
  ddsrt_free (peers);
  //printf ("%s\n", config);

  const dds_entity_t dom = dds_create_domain (domainid, config);
  CU_ASSERT_FATAL (dom > 0);
  ddsrt_free (config);
  const dds_entity_t pp = dds_create_participant (domainid, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  return dom;
}

struct logger_pat {
  const char *pat;
  bool present;
};
#define MAX_PATS 4
struct logger_arg {
  struct logger_pat expected[2][MAX_PATS]; // one for each domain, max 4 patterns per domain
  uint32_t found[2];
};

static void logger (void *varg, const dds_log_data_t *data)
{
  struct logger_arg * const arg = varg;
  fputs (data->message - data->hdrsize, stdout);
  if (data->domid == 0 || data->domid == 1)
  {
    for (uint32_t i = 0; i < MAX_PATS && arg->expected[data->domid][i].pat != NULL; i++)
      if (ddsi_patmatch (arg->expected[data->domid][i].pat, data->message))
        arg->found[data->domid] |= (uint32_t)(1 << i);
  }
}

struct one {
  enum ddsi_boolean_default allowmc;
  const char *spdp_address;
  const char *participant_index;
  bool add_localhost;
  const locstr_t *peer;
};
struct cfg {
  struct one one[2];
};

// Operations for constructing different tests, there's always an implied shutting down
// of all domains after the last operation
enum oper {
  SLEEP_1,
  SLEEP_3,
  SLEEP_5,
  SHUTDOWN_0,
  SHUTDOWN_1,
  KILL_0,
  KILL_1
};

static void run_one (int base_port, const struct cfg *cfg, const struct logger_arg *larg_in, size_t nopers, const enum oper opers[])
{
  assert (larg_in->found[0] == 0 && larg_in->found[1] == 0);
  struct logger_arg larg = *larg_in;
  dds_set_log_mask (DDS_LC_ALL);
  dds_set_log_sink (&logger, &larg);
  dds_set_trace_sink (&logger, &larg);
    
  dds_entity_t dom[2];
  for (uint32_t d = 0; d < 2; d++)
  {
    dom[d] = make_domain_and_participant (d, base_port, cfg->one[d].allowmc, cfg->one[d].spdp_address, cfg->one[d].participant_index, cfg->one[d].add_localhost, cfg->one[d].peer);
  }
  for (size_t i = 0; i < nopers;i ++)
  {
    switch (opers[i])
    {
      case SLEEP_1:
        dds_sleepfor (DDS_SECS (1));
        break;
      case SLEEP_3:
        dds_sleepfor (DDS_SECS (3));
        break;
      case SLEEP_5:
        dds_sleepfor (DDS_SECS (5));
        break;
      case SHUTDOWN_0:
        dds_delete (dom[0]);
        break;
      case SHUTDOWN_1:
        dds_delete (dom[1]);
        break;
      case KILL_0:
        // making it deafmute, then shutting it down has the same effect
        // on the remote as killing it
        dds_domain_set_deafmute (dom[0], true, true, DDS_INFINITY);
        dds_delete (dom[0]);
        break;
      case KILL_1:
        dds_domain_set_deafmute (dom[1], true, true, DDS_INFINITY);
        dds_delete (dom[1]);
        break;
    }
  }
  for (int d = 0; d < 2; d++)
  {
    // don't check returns: we may have deleted the domain already
    dds_delete (dom[d]);
  }
    
  dds_set_log_mask (0);
  dds_set_log_sink (NULL, NULL);
  dds_set_trace_sink (NULL, NULL);
  fflush (stdout);
  
  bool all_ok = true;
  for (uint32_t d = 0; d < 2; d++)
  {
    for (uint32_t i = 0; i < MAX_PATS && larg.expected[d][i].pat != NULL; i++)
    {
      if (((larg.found[d] & (1u << i)) != 0) != larg.expected[d][i].present)
      {
        printf ("dom %"PRIu32" pattern %s: %s\n",
                d, larg.expected[d][i].pat,
                larg.expected[d][i].present ? "missing" : "present unexpectedly");
        all_ok = false;
      }
    }
  }
  CU_ASSERT_FATAL (all_ok);
}

static const struct logger_arg larg_mut_disc = {
  .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 }
    },
    [1] = {
      { "*SPDP*NEW*", 1 }
    }
  }
};
static const struct logger_arg larg_mut_nodisc = {
  .expected = {
    [0] = {
      { "*SPDP*NEW*", 0 }
    },
    [1] = {
      { "*SPDP*NEW*", 0 }
    }
  }
};

// assume MC works on test platform (true for CI)
// using non-standard SPDP address for no reason at all, I think

CU_Test(ddsc_spdp, I1_mc)
{
  const int baseport = 7000;
  // default: expect discovery
  struct logger_arg larg = larg_mut_disc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_DEFAULT, "239.255.0.2", "default", false, NULL },
    { DDSI_BOOLDEF_DEFAULT, "239.255.0.2", "default", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I2_uc_lhost)
{
  const int baseport = 7010;
  // no mc, localhost as peer: expect discovery
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = larg_mut_disc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.2", "default", false, &localhost },
    { DDSI_BOOLDEF_FALSE, "239.255.0.2", "default", false, &localhost } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I3_mc_spdp0_no_lhost)
{
  const int baseport = 7020;
  // all mc but SPDP address = 0.0.0.0: no discovery without peers, known port
  struct logger_arg larg = larg_mut_nodisc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_TRUE, "0.0.0.0", "default", false, NULL },
    { DDSI_BOOLDEF_TRUE, "0.0.0.0", "default", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I4_mc_spdp0_lhost_in_one)
{
  const int baseport = 7030;
  // no discovery happens: first one says peers defined => well-known ports, pings localhost
  // second says: MC supported/allowed, no peers defined => port none
  // a bit weird, but not wrong
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = larg_mut_nodisc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_TRUE, "0.0.0.0", "default", false, &localhost },
    { DDSI_BOOLDEF_TRUE, "0.0.0.0", "default", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I5_mc_spdp0_lhost_one_addlhost_other)
{
  const int baseport = 7040;
  // first one says peers defined => well-known ports, pings localhost
  // second says: peers defined => well-known ports
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = larg_mut_disc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_TRUE, "0.0.0.0", "default", false, &localhost },
    { DDSI_BOOLDEF_TRUE, "0.0.0.0", "default", true, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I6_mc_one_uc_lhost_other)
{
  const int baseport = 7050;
  // no discovery happens: first one says MC works => no localhost, random port
  // second says: peers defined => no MC, well-known ports
  // but that doesn't allow them to discover each other
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = larg_mut_nodisc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_TRUE, "239.255.0.2", "default", false, NULL },
    { DDSI_BOOLDEF_FALSE, "0.0.0.0", "default", true, &localhost } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I7_mc_autoidx_one_uc_other)
{
  const int baseport = 7060;
  struct logger_arg larg = larg_mut_disc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_TRUE, "239.255.0.2", "auto", false, NULL },
    { DDSI_BOOLDEF_FALSE, "0.0.0.0", "default", true, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I8_uc_lhost_one_mc_other)
{
  const int baseport = 7070;
  // fails to discover: second uses random port
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = larg_mut_nodisc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.2", "default", false, &localhost },
    { DDSI_BOOLDEF_TRUE, "239.255.0.1", "default", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

CU_Test(ddsc_spdp, I9_uc_lhost_one_mc_autoidx_other)
{
  const int baseport = 7080;
  // setting participant index to auto makes I8 work
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = larg_mut_disc;
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.2", "default", false, &localhost },
    { DDSI_BOOLDEF_TRUE, "239.255.0.1", "auto", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_1 });
}

// pruning of useless initial locators
// prune delays are 2s, spdp interval = 0.5s

CU_Test(ddsc_spdp, II1_mc_nonexist_peer_in_one)
{
  const int baseport = 7090;
  locstr_t nonexist;
  get_nonexist_address (&nonexist);
  char prunemsg[200];
  const int prunemsglen = snprintf (prunemsg, sizeof (prunemsg), "*spdp: prune loc*%s*", nonexist.str);
  assert (prunemsglen < (int) sizeof (prunemsg));
  (void) prunemsglen;
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg, 1 }
    },
    [1] = {
      { "*SPDP*NEW*", 1 }
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_TRUE, "239.255.0.1", "default", false, &nonexist },
    { DDSI_BOOLDEF_TRUE, "239.255.0.1", "default", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_3 });
}

CU_Test(ddsc_spdp, II2_uc_prune_only_existing)
{
  const int baseport = 7100;
  locstr_t localhost;
  get_localhost_address (&localhost);
  char prunemsg[2][2][200];
  const int prunemsglen = snprintf (prunemsg[0][0], sizeof (prunemsg[0][0]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 4);
  assert (prunemsglen < (int) sizeof (prunemsg[0][0]));
  (void) prunemsglen;
  snprintf (prunemsg[0][1], sizeof (prunemsg[0][1]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 6);
  snprintf (prunemsg[1][0], sizeof (prunemsg[1][0]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 2);
  snprintf (prunemsg[1][1], sizeof (prunemsg[1][1]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 6);
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg[0][0], 0 }, // mustn't prune discovered peer
      { prunemsg[0][1], 1 }, // must prune non-existent one
    },
    [1] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg[1][0], 0 }, // mustn't prune discovered peer
      { prunemsg[1][1], 1 }, // must prune non-existent one
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "0", true, NULL },
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "1", true, NULL } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_3 });
}

CU_Test(ddsc_spdp, II3_uc_prune_only_existing)
{
  const int baseport = 7110;
  locstr_t localhost;
  get_localhost_address (&localhost);
  char prunemsg[2][2][200];
  const int prunemsglen = snprintf (prunemsg[0][0], sizeof (prunemsg[0][0]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 4);
  assert (prunemsglen < (int) sizeof (prunemsg[0][0]));
  (void) prunemsglen;
  snprintf (prunemsg[0][1], sizeof (prunemsg[0][1]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 6);
  snprintf (prunemsg[1][0], sizeof (prunemsg[1][0]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 2);
  snprintf (prunemsg[1][1], sizeof (prunemsg[1][1]), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 6);
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg[0][0], 0 }, // mustn't prune discovered peer
      { prunemsg[0][1], 1 }, // must prune non-existent one
    },
    [1] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg[1][0], 0 }, // mustn't prune discovered peer
      { prunemsg[1][1], 1 }, // must prune non-existent one
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "0", false, &localhost },
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "1", false, &localhost } }
  };
  run_one (baseport, &cfg, &larg, 1, (enum oper[]){ SLEEP_3 });
}

CU_Test(ddsc_spdp, II4_uc_other_disc_address_of_one)
{
  const int baseport = 7120;
  // first stops early, second discovered the address
  // 2s until pruning
  // but only a 1s wait -> can't have pruned based on time yet
  // clean termination: should've been dropped (not pruned)
  locstr_t localhost;
  get_localhost_address (&localhost);
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 }
    },
    [1] = {
      { "*SPDP*NEW*", 1 },
      { "*SPDP*ST3*", 1 }, // shutdown termination detected
      { "*spdp: drop live loc*", 1 }
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "0", false, &localhost },
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "1", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 3, (enum oper[]){ SLEEP_3, SHUTDOWN_0, SLEEP_1 });
}

CU_Test(ddsc_spdp, II5_uc_one_init_address_of_other)
{
  const int baseport = 7130;
  // second stops early, first simply uses initial locator
  // 2s until pruning
  // but only a 1s wait -> can't have pruned based on time yet
  // clean termination: should've been dropped (not pruned)
  locstr_t localhost;
  get_localhost_address (&localhost);
  char prunemsg[200], dropmsg[200];
  const int prunemsglen = snprintf (prunemsg, sizeof (prunemsg), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 6);
  assert (prunemsglen < (int) sizeof (prunemsg));
  (void) prunemsglen;
  const int dropmsglen = snprintf (dropmsg, sizeof (dropmsg), "*spdp: drop live loc*%s:%d*", localhost.str, baseport + 4);
  assert (dropmsglen < (int) sizeof (dropmsg));
  (void) dropmsglen;
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 },
      { "*SPDP*ST3*", 1 }, // shutdown termination detected
      { prunemsg, 1 }, // enough time to do pruning of unused locators
      // locator was in initial set
      // locator kept alive by existing participant
      // clean termination -> may drop it
      // initial prune delay passed, so dropped immediately
      { dropmsg, 1 }
    },
    [1] = {
      { "*SPDP*NEW*", 1 },
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "0", false, &localhost },
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "1", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 3, (enum oper[]){ SLEEP_3, SHUTDOWN_1, SLEEP_1 });
}


CU_Test(ddsc_spdp, II6_pruning_lease_exp)
{
  const int baseport = 7140;
  // 2s until lease expiry
  // 2s until pruning
  // 3s wait -> not pruned yet
  locstr_t localhost;
  get_localhost_address (&localhost);
  char prunemsg[200];
  const int prunemsglen = snprintf (prunemsg, sizeof (prunemsg), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 2);
  assert (prunemsglen < (int) sizeof (prunemsg));
  (void) prunemsglen;
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 }
    },
    [1] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg, 0 }
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "0", false, &localhost },
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "1", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 3, (enum oper[]){ SLEEP_3, KILL_0, SLEEP_3 });
}

CU_Test(ddsc_spdp, II7_pruning_lease_exp)
{
  const int baseport = 7150;
  // 2s until lease expiry
  // 2s until pruning
  // 5s wait -> should be pruned
  locstr_t localhost;
  get_localhost_address (&localhost);
  char prunemsg[200];
  const int prunemsglen = snprintf (prunemsg, sizeof (prunemsg), "*spdp: prune loc*%s:%d*", localhost.str, baseport + 2);
  assert (prunemsglen < (int) sizeof (prunemsg));
  (void) prunemsglen;
  struct logger_arg larg = { .expected = {
    [0] = {
      { "*SPDP*NEW*", 1 }
    },
    [1] = {
      { "*SPDP*NEW*", 1 },
      { prunemsg, 1 }
    }
  } };
  struct cfg cfg = { {
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "0", false, &localhost },
    { DDSI_BOOLDEF_FALSE, "239.255.0.1", "1", false, NULL } }
  };
  run_one (baseport, &cfg, &larg, 3, (enum oper[]){ SLEEP_3, KILL_0, SLEEP_5 });
}
