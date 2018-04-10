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
#include "ddsc/dds.h"
#include <criterion/criterion.h>
#include <os/os.h>
#include "config_env.h"
#include "ddsc/ddsc_project.h"


#define cr_assert_status_eq(s1, s2, ...) cr_assert_eq(dds_err_nr(s1), s2, __VA_ARGS__)


Test(ddsc_participant, create_and_delete) {

  dds_entity_t participant, participant2, participant3;

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant2, 0, "dds_participant_create");

  dds_delete (participant);
  dds_delete (participant2);

  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant3, 0, "dds_participant_create");

  dds_delete (participant3);

}


/* Test for creating participant with no configuration file  */
Test(ddsc_participant, create_with_no_conf_no_env) {
  dds_entity_t participant, participant2, participant3;
  dds_return_t status;
  dds_domainid_t domain_id;
  dds_domainid_t valid_domain=0;

  const char * env_uri = os_getenv(DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI");
  cr_assert_eq(env_uri, NULL, DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI must be NULL");

  //invalid domain
  participant = dds_create_participant (1, NULL, NULL);
  cr_assert_lt(participant, 0, "Error must be received for invalid domain value");

  //valid specific domain value
  participant2 = dds_create_participant (valid_domain, NULL, NULL);
  cr_assert_gt(participant2, 0, "Valid participant must be received for valid specific domain value");
  status = dds_get_domainid(participant2, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");
  cr_assert_eq(domain_id, valid_domain, "Retrieved domain ID must be valid");

  //DDS_DOMAIN_DEFAULT from user
  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant3, 0, "Valid participant must be received for DDS_DOMAIN_DEFAULT");
  status = dds_get_domainid(participant3, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");
  cr_assert_eq(domain_id, valid_domain, "Retrieved domain ID must be valid");

  dds_delete(participant2);
  dds_delete(participant3);


}


////WITH CONF

/* Test for creating participant with valid configuration file  */
Test(ddsc_participant, create_with_conf_no_env) {
  dds_entity_t participant, participant2, participant3;
  dds_return_t status;
  dds_domainid_t domain_id;
  dds_domainid_t valid_domain=3;

  static char env_uri_str[1000];
  (void) sprintf(env_uri_str, "%s=%s", DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI", CONFIG_ENV_SIMPLE_UDP);
  os_putenv(env_uri_str);

  static char env_mp_str[100];
  (void) sprintf(env_mp_str, "%s=%s", "MAX_PARTICIPANTS", CONFIG_ENV_MAX_PARTICIPANTS);
  os_putenv(env_mp_str);


  const char * env_uri = os_getenv(DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI");
  cr_assert_neq(env_uri, NULL, DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI must be set");

  //invalid domain
  participant = dds_create_participant (1, NULL, NULL);
  cr_assert_lt(participant, 0, "Error must be received for invalid domain value");

  //valid specific domain value
  participant2 = dds_create_participant (valid_domain, NULL, NULL);
  cr_assert_gt(participant2, 0, "Valid participant must be received for valid specific domain value");
  status = dds_get_domainid(participant2, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");
  cr_assert_eq(domain_id, valid_domain, "Retrieved domain ID must be valid");


  //DDS_DOMAIN_DEFAULT from the user
  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant3, 0, "Valid participant must be received for DDS_DOMAIN_DEFAULT");
  status = dds_get_domainid(participant3, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");
  cr_assert_eq(domain_id, valid_domain, "Retrieved domain ID must be valid");

  dds_delete(participant2);
  dds_delete(participant3);


}

Test(ddsc_participant_lookup, one) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  cr_assert_eq(num_of_found_pp, 1, "dds_lookup_participant(domain_id, participants, size)");
  cr_assert_eq(participants[0], participant,"dds_lookup_participant did not return the participant");

  dds_delete (participant);
}

Test(ddsc_participant_lookup, multiple) {

  dds_entity_t participant, participant2;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant2, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  cr_assert_eq(num_of_found_pp, 2, "dds_lookup_participant(domain_id, participants, size)");
  cr_assert(participants[0] == participant || participants[0] == participant2,"ddsc_participant_lookup");
  cr_assert(participants[1] == participant || participants[1] == participant2,"ddsc_participant_lookup");
  cr_assert_neq(participants[0], participants[1], "dds_lookup_participant returned a participant twice");

  dds_delete (participant2);
  dds_delete (participant);
}

Test(ddsc_participant_lookup, array_too_small) {

  dds_entity_t participant, participant2, participant3;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant2, 0, "dds_participant_create");

  participant3 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant3, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  cr_assert_eq(num_of_found_pp, 3, "dds_lookup_participant(domain_id, participants, size)");
  cr_assert(participants[0] == participant || participants[0] == participant2 || participants[0] == participant3,"ddsc_participant_lookup");
  cr_assert(participants[1] == participant || participants[1] == participant2 || participants[1] == participant3,"ddsc_participant_lookup");
  cr_assert_neq(participants[0], participants[1], "dds_lookup_participant returned a participant twice");

  dds_delete (participant3);
  dds_delete (participant2);
  dds_delete (participant);
}

Test(ddsc_participant_lookup, null_zero){

  dds_entity_t participant;
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 0;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  num_of_found_pp = dds_lookup_participant( domain_id, NULL, size);
  cr_assert_eq(num_of_found_pp, 1, "dds_lookup_participant(domain_id, participants, size)");

  dds_delete (participant);
}

Test(ddsc_participant_lookup, null_nonzero){

  dds_entity_t participant;
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  num_of_found_pp = dds_lookup_participant( domain_id, NULL, size);
  cr_assert_status_eq(num_of_found_pp, DDS_RETCODE_BAD_PARAMETER, "dds_lookup_participant did not return bad parameter");

  dds_delete (participant);
}

Test(ddsc_participant_lookup, unknown_id) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");
  domain_id ++;

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  cr_assert_eq(num_of_found_pp, 0, "dds_lookup_participant(domain_id, participants, size)");

  dds_delete (participant);
}

Test(ddsc_participant_lookup, none) {

  dds_entity_t participants[2];
  dds_return_t num_of_found_pp;
  size_t size = 2;

  num_of_found_pp = dds_lookup_participant( 0, participants, size);
  cr_assert_eq(num_of_found_pp, 0, "dds_lookup_participant did not return 0");
}

Test(ddsc_participant_lookup, no_more) {

  dds_entity_t participant;
  dds_entity_t participants[3];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 3;

  /* Create a participant */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  dds_delete (participant);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  cr_assert_eq(num_of_found_pp, 0, "dds_lookup_participant did not return 0");
}

Test(ddsc_participant_lookup, deleted) {

  dds_entity_t participant, participant2;
  dds_entity_t participants[2];
  dds_domainid_t domain_id;
  dds_return_t status, num_of_found_pp;
  size_t size = 2;

  /* Create participants */
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant, 0, "dds_participant_create");

  participant2 = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  cr_assert_gt(participant2, 0, "dds_participant_create");

  /* Get domain id */
  status = dds_get_domainid(participant, &domain_id);
  cr_assert_status_eq(status, DDS_RETCODE_OK, "dds_get_domainid(participant, domain_id)");

  dds_delete (participant2);

  num_of_found_pp = dds_lookup_participant( domain_id, participants, size);
  cr_assert_eq(num_of_found_pp, 1, "dds_lookup_participant did not return one participant");
  cr_assert(participants[0] == participant,"ddsc_participant_lookup");

  dds_delete (participant);
}
