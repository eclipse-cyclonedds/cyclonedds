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
#include "CUnit/Test.h"
#include "os/os.h"
#include "config_env.h"
#include "ddsc/ddsc_project.h"

#define FORCE_ENV

#define URI_VARIABLE DDSC_PROJECT_NAME_NOSPACE_CAPS"_URI"
#define MAX_PARTICIPANTS_VARIABLE "MAX_PARTICIPANTS"

static void config__check_env(
    _In_z_ const char * env_variable,
    _In_z_ const char * expected_value)
{
    const char * env_uri = os_getenv(env_variable);
#if 0
    const char * const env_not_set = "Environment variable '%s' isn't set. This needs to be set to '%s' for this test to run.";
    const char * const env_not_as_expected = "Environment variable '%s' has an unexpected value: '%s' (expected: '%s')";
#endif

#ifdef FORCE_ENV
    {
        bool env_ok;

        if ( env_uri == NULL ) {
            env_ok = false;
        } else if ( strncmp(env_uri, expected_value, strlen(expected_value)) != 0 ) {
            env_ok = false;
        } else {
            env_ok = true;
        }

        if ( !env_ok ) {
            os_result r;
            char *envstr;
            size_t len = strlen(env_variable) + strlen("=") + strlen(expected_value) + 1;

            envstr = os_malloc(len);
            (void)snprintf(envstr, len, "%s=%s", env_variable, expected_value);

            r = os_putenv(envstr);
            CU_ASSERT_EQUAL_FATAL(r, os_resultSuccess);

            os_free(envstr);
        }
    }
#else
    CU_ASSERT_PTR_NOT_NULL_FATAL(env_uri);
    CU_ASSERT_STRING_EQUAL_FATAL(env_uri, expected_value);
#endif /* FORCE_ENV */

}

CU_Test(ddsc_config, simple_udp, .init = os_osInit, .fini = os_osExit) {

    dds_entity_t participant;

    config__check_env(URI_VARIABLE, CONFIG_ENV_SIMPLE_UDP);
    config__check_env(MAX_PARTICIPANTS_VARIABLE, CONFIG_ENV_MAX_PARTICIPANTS);

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);

    CU_ASSERT_FATAL(participant> 0);

    dds_delete(participant);
}
