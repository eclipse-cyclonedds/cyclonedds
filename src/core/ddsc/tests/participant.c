// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdint.h>
#include <stdlib.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "config_env.h"
#include "dds/version.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/process.h"
#include "test_util.h"


CU_Test(ddsc_participant, create_and_delete) {

  dds_entity_t participant, participant2, participant3;

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant2, 0);

  dds_delete (participant);
  dds_delete (participant2);

  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant3, 0);

  dds_delete (participant3);

}


/* Test for creating participant with no configuration file  */
CU_Test(ddsc_participant, create_with_no_conf_no_env)
{
  dds_entity_t participant2, participant3;
  dds_return_t status;
  dds_domainid_t valid_domain=3;
  dds_domainid_t domain_id2 = UINT32_MAX;
  dds_domainid_t domain_id3 = UINT32_MAX;
  dds_return_t status2 = DDS_RETCODE_ERROR;
  dds_return_t status3 = DDS_RETCODE_ERROR;
  struct test_saved_envvar saved_uri;

  status = test_save_envvar (&saved_uri, "CYCLONEDDS_URI");
  if (status == DDS_RETCODE_OK && !test_config_inherits_fakeudp ())
    status = ddsrt_setenv("CYCLONEDDS_URI", "");
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  //valid specific domain value
  participant2 = dds_create_participant (valid_domain, NULL, NULL);
  if (participant2 > 0)
    status2 = dds_get_domainid(participant2, &domain_id2);

  //DDS_DOMAIN_DEFAULT from user
  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant3 > 0)
    status3 = dds_get_domainid(participant3, &domain_id3);

  if (participant2 > 0)
    dds_delete(participant2);
  if (participant3 > 0)
    dds_delete(participant3);
  const dds_return_t restore_status = test_restore_envvar (&saved_uri);

  CU_ASSERT_EQ_FATAL (restore_status, DDS_RETCODE_OK);
  CU_ASSERT_GT_FATAL (participant2, 0);
  CU_ASSERT_EQ_FATAL (status2, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (domain_id2, valid_domain);
  CU_ASSERT_GT_FATAL (participant3, 0);
  CU_ASSERT_EQ_FATAL (status3, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (domain_id3, valid_domain);
}


/* Test for creating participants in multiple domains with no configuration file  */
CU_Test(ddsc_participant, create_multiple_domains)
{
  dds_entity_t participant1, participant2;
  dds_return_t status;
  dds_domainid_t domain_id1 = UINT32_MAX;
  dds_domainid_t domain_id2 = UINT32_MAX;
  dds_return_t status1 = DDS_RETCODE_ERROR;
  dds_return_t status2 = DDS_RETCODE_ERROR;
  struct test_saved_envvar saved_uri;

  status = test_save_envvar (&saved_uri, "CYCLONEDDS_URI");
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
  char *config = test_config_from_env ("<Tracing><Verbosity>finest</><OutputFile>multi-domain-1.log</></>", 1);
  status = ddsrt_setenv("CYCLONEDDS_URI", config);
  ddsrt_free (config);

  //valid specific domain value
  participant1 = status == DDS_RETCODE_OK ? dds_create_participant (1, NULL, NULL) : DDS_RETCODE_ERROR;
  if (participant1 > 0)
    status1 = dds_get_domainid(participant1, &domain_id1);

  config = test_config_from_env ("<Tracing><Verbosity>finest</><OutputFile>multi-domain-2.log</></>", 2);
  if (status == DDS_RETCODE_OK)
    status = ddsrt_setenv("CYCLONEDDS_URI", config);
  ddsrt_free (config);

  //DDS_DOMAIN_DEFAULT from user
  participant2 = status == DDS_RETCODE_OK ? dds_create_participant (2, NULL, NULL) : DDS_RETCODE_ERROR;
  if (participant2 > 0)
    status2 = dds_get_domainid(participant2, &domain_id2);

  if (participant1 > 0)
    dds_delete(participant1);
  if (participant2 > 0)
    dds_delete(participant2);
  const dds_return_t restore_status = test_restore_envvar (&saved_uri);

  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (restore_status, DDS_RETCODE_OK);
  CU_ASSERT_GT_FATAL (participant1, 0);
  CU_ASSERT_EQ_FATAL (status1, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (domain_id1, 1);
  CU_ASSERT_GT_FATAL (participant2, 0);
  CU_ASSERT_EQ_FATAL (status2, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (domain_id2, 2);
}

CU_Test(ddsc_participant, auto_participant_index_zero)
{
  dds_return_t status;
  char *config = NULL;
  const unsigned port_base = 20000u + (unsigned) ((uintptr_t) ddsrt_getpid () % 1000u) * 20u;

  (void) ddsrt_asprintf (&config,
    "<General>"
    "  <AllowMulticast>false</AllowMulticast>"
    "</General>"
    "<Discovery>"
    "  <ParticipantIndex>auto</ParticipantIndex>"
    "  <MaxAutoParticipantIndex>0</MaxAutoParticipantIndex>"
    "  <Ports>"
    "    <Base>%u</Base>"
    "  </Ports>"
    "</Discovery>",
    port_base);

  char *expanded = test_config_from_env (config, 0);
  ddsrt_free (config);
  dds_entity_t domain = dds_create_domain (0, expanded);
  ddsrt_free (expanded);
  CU_ASSERT_GT_FATAL (domain, 0);

  const struct ddsi_domaingv *gv = get_domaingv (domain);
  CU_ASSERT_NEQ_FATAL (gv, NULL);
  CU_ASSERT_EQ_FATAL (gv->config.participantIndex, 0);

  status = dds_delete (domain);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
}


////WITH CONF

/* Test for creating participant with valid configuration file  */
CU_Test(ddsc_participant, create_with_conf_no_env) {
    dds_entity_t participant2, participant3;
    dds_return_t status;
    dds_domainid_t valid_domain=3;
    dds_domainid_t domain_id2 = UINT32_MAX;
    dds_domainid_t domain_id3 = UINT32_MAX;
    dds_return_t status2 = DDS_RETCODE_ERROR;
    dds_return_t status3 = DDS_RETCODE_ERROR;
    struct test_saved_envvar saved_uri, saved_max_participants;

    status = test_save_envvar (&saved_uri, "CYCLONEDDS_URI");
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
    status = test_save_envvar (&saved_max_participants, "MAX_PARTICIPANTS");
    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
    char *config = test_config_from_env (CONFIG_ENV_SIMPLE_UDP, valid_domain);
    status = ddsrt_setenv("CYCLONEDDS_URI", config);
    ddsrt_free (config);
    if (status == DDS_RETCODE_OK)
      status = ddsrt_setenv("MAX_PARTICIPANTS", CONFIG_ENV_MAX_PARTICIPANTS);

    const char * env_uri = NULL;
    if (status == DDS_RETCODE_OK)
      ddsrt_getenv("CYCLONEDDS_URI", &env_uri);

    //valid specific domain value
    participant2 = status == DDS_RETCODE_OK ? dds_create_participant (valid_domain, NULL, NULL) : DDS_RETCODE_ERROR;
    if (participant2 > 0)
      status2 = dds_get_domainid(participant2, &domain_id2);


    //DDS_DOMAIN_DEFAULT from the user
    participant3 = status == DDS_RETCODE_OK ? dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL) : DDS_RETCODE_ERROR;
    if (participant3 > 0)
      status3 = dds_get_domainid(participant3, &domain_id3);

    if (participant2 > 0)
      dds_delete(participant2);
    if (participant3 > 0)
      dds_delete(participant3);
    const dds_return_t restore_max_participants_status = test_restore_envvar (&saved_max_participants);
    const dds_return_t restore_uri_status = test_restore_envvar (&saved_uri);

    CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
    CU_ASSERT_NEQ_FATAL (env_uri, NULL);
    CU_ASSERT_EQ_FATAL (restore_max_participants_status, DDS_RETCODE_OK);
    CU_ASSERT_EQ_FATAL (restore_uri_status, DDS_RETCODE_OK);
    CU_ASSERT_GT_FATAL (participant2, 0);
    CU_ASSERT_EQ_FATAL (status2, DDS_RETCODE_OK);
    CU_ASSERT_EQ_FATAL (domain_id2, valid_domain);
    CU_ASSERT_GT_FATAL (participant3, 0);
    CU_ASSERT_EQ_FATAL (status3, DDS_RETCODE_OK);
    CU_ASSERT_EQ_FATAL (domain_id3, valid_domain);
}

CU_Test(ddsc_participant_lookup, one) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 1);
  CU_ASSERT_EQ_FATAL (participants[0], participant);

  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, multiple) {

  dds_entity_t participant, participant2;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant2, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 2);
  CU_ASSERT_FATAL (participants[0] == participant || participants[0] == participant2);
  CU_ASSERT_FATAL (participants[1] == participant || participants[1] == participant2);
  CU_ASSERT_NEQ_FATAL (participants[0], participants[1]);

  dds_delete (participant2);
  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, array_too_small) {

  dds_entity_t participant, participant2, participant3;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant2, 0);

  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant3, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 3);
  CU_ASSERT_FATAL (participants[0] == participant || participants[0] == participant2 || participants[0] == participant3);
  CU_ASSERT_FATAL (participants[1] == participant || participants[1] == participant2 || participants[1] == participant3);
  CU_ASSERT_NEQ_FATAL (participants[0], participants[1]);

  dds_delete (participant3);
  dds_delete (participant2);
  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, null_zero){

  dds_entity_t participant;
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 0;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, NULL, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 1);

  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, null_nonzero){

  dds_entity_t participant;
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, NULL, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, DDS_RETCODE_BAD_PARAMETER);

  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, unknown_id) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);
  domain_id ++;

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 0);

  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, none) {

  dds_entity_t participants[2];
  dds_return_t num_of_found_pp;
  size_t size = 2;

  num_of_found_pp = dds_lookup_participant( 0, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 0);
}

CU_Test(ddsc_participant_lookup, no_more) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  dds_delete (participant);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 0);
}

CU_Test(ddsc_participant_lookup, deleted) {

  dds_entity_t participant, participant2;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant2, 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQ_FATAL (status, DDS_RETCODE_OK);

  dds_delete (participant2);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQ_FATAL (num_of_found_pp, 1);
  CU_ASSERT_EQ_FATAL (participants[0], participant);

  dds_delete (participant);
}
