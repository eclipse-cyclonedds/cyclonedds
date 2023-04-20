// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "config_env.h"
#include "dds/version.h"
#include "dds/ddsrt/environ.h"


CU_Test(ddsc_participant, create_and_delete) {

  dds_entity_t participant, participant2, participant3;

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant2 > 0);

  dds_delete (participant);
  dds_delete (participant2);

  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant3 > 0);

  dds_delete (participant3);

}


/* Test for creating participant with no configuration file  */
CU_Test(ddsc_participant, create_with_no_conf_no_env)
{
  dds_entity_t participant2, participant3;
  dds_return_t status;
  dds_domainid_t domain_id;
  dds_domainid_t valid_domain=3;

  status = ddsrt_unsetenv("CYCLONEDDS_URI");
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  //valid specific domain value
  participant2 = dds_create_participant (valid_domain, NULL, NULL);
  CU_ASSERT_FATAL(participant2 > 0);
  status = dds_get_domainid(participant2, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(domain_id, valid_domain);

  //DDS_DOMAIN_DEFAULT from user
  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant3 > 0);
  status = dds_get_domainid(participant3, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(domain_id, valid_domain);

  dds_delete(participant2);
  dds_delete(participant3);
}


/* Test for creating participants in multiple domains with no configuration file  */
CU_Test(ddsc_participant, create_multiple_domains)
{
  dds_entity_t participant1, participant2;
  dds_return_t status;
  dds_domainid_t domain_id;

  ddsrt_setenv("CYCLONEDDS_URI", "<Tracing><Verbosity>finest</><OutputFile>multi-domain-1.log</></>");

  //valid specific domain value
  participant1 = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL(participant1 > 0);
  status = dds_get_domainid(participant1, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(domain_id, 1);

  ddsrt_setenv("CYCLONEDDS_URI", "<Tracing><Verbosity>finest</><OutputFile>multi-domain-2.log</></>");

  //DDS_DOMAIN_DEFAULT from user
  participant2 = dds_create_participant (2, NULL, NULL);
  CU_ASSERT_FATAL(participant2 > 0);
  status = dds_get_domainid(participant2, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL_FATAL(domain_id, 2);

  dds_delete(participant1);
  dds_delete(participant2);
}


////WITH CONF

/* Test for creating participant with valid configuration file  */
CU_Test(ddsc_participant, create_with_conf_no_env) {
    dds_entity_t participant2, participant3;
    dds_return_t status;
    dds_domainid_t domain_id;
    dds_domainid_t valid_domain=3;

    ddsrt_setenv("CYCLONEDDS_URI", CONFIG_ENV_SIMPLE_UDP);
    ddsrt_setenv("MAX_PARTICIPANTS", CONFIG_ENV_MAX_PARTICIPANTS);

    const char * env_uri = NULL;
    ddsrt_getenv("CYCLONEDDS_URI", &env_uri);
    CU_ASSERT_PTR_NOT_EQUAL_FATAL(env_uri, NULL);

    //valid specific domain value
    participant2 = dds_create_participant (valid_domain, NULL, NULL);
    CU_ASSERT_FATAL(participant2 > 0);
    status = dds_get_domainid(participant2, &domain_id);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(domain_id, valid_domain);


    //DDS_DOMAIN_DEFAULT from the user
    participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant3 > 0);
    status = dds_get_domainid(participant3, &domain_id);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(domain_id, valid_domain);

    dds_delete(participant2);
    dds_delete(participant3);
}

CU_Test(ddsc_participant_lookup, one) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 1);
  CU_ASSERT_EQUAL_FATAL(participants[0], participant);

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
  CU_ASSERT_FATAL(participant > 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant2 > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 2);
  CU_ASSERT_FATAL(participants[0] == participant || participants[0] == participant2);
  CU_ASSERT_FATAL(participants[1] == participant || participants[1] == participant2);
  CU_ASSERT_NOT_EQUAL_FATAL(participants[0], participants[1]);

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
  CU_ASSERT_FATAL(participant > 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant2 > 0);

  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant3 > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 3);
  CU_ASSERT_FATAL(participants[0] == participant || participants[0] == participant2 || participants[0] == participant3);
  CU_ASSERT_FATAL(participants[1] == participant || participants[1] == participant2 || participants[1] == participant3);
  CU_ASSERT_NOT_EQUAL_FATAL(participants[0], participants[1]);

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
  CU_ASSERT_FATAL(participant > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, NULL, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 1);

  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, null_nonzero){

  dds_entity_t participant;
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  num_of_found_pp = dds_lookup_participant( domain_id, NULL, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, DDS_RETCODE_BAD_PARAMETER);

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
  CU_ASSERT_FATAL(participant > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
  domain_id ++;

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 0);

  dds_delete (participant);
}

CU_Test(ddsc_participant_lookup, none) {

  dds_entity_t participants[2];
  dds_return_t num_of_found_pp;
  size_t size = 2;

  num_of_found_pp = dds_lookup_participant( 0, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 0);
}

CU_Test(ddsc_participant_lookup, no_more) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  dds_delete (participant);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 0);
}

CU_Test(ddsc_participant_lookup, deleted) {

  dds_entity_t participant, participant2;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL(participant2 > 0);

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

  dds_delete (participant2);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  CU_ASSERT_EQUAL_FATAL(num_of_found_pp, 1);
  CU_ASSERT_FATAL(participants[0] == participant);

  dds_delete (participant);
}
