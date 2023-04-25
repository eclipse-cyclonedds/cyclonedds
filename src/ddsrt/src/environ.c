// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "dds/ddsrt/expand_vars.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/process.h"

struct expand_env_data
{
    uint32_t domid;
    char idstr[20];
};

static const char * expand_lookup_env(const char *name, void * data)
{
    const char *env = NULL;
    struct expand_env_data * env_data = data;

    if (ddsrt_getenv (name, &env) == DDS_RETCODE_OK) {
        /* ok */
    } else if (strcmp (name, "$") == 0 || strcmp (name, "CYCLONEDDS_PID") == 0) {
        (void) snprintf (env_data->idstr, sizeof (env_data->idstr), "%"PRIdPID, ddsrt_getpid ());
        env = env_data->idstr;
    } else if (strcmp (name, "CYCLONEDDS_DOMAIN_ID") == 0 && env_data->domid != UINT32_MAX) {
        (void) snprintf (env_data->idstr, sizeof (env_data->idstr), "%"PRIu32, env_data->domid);
        env = env_data->idstr;
    }
    return env;
}

char *ddsrt_expand_envvars_sh (const char *src0, uint32_t domid)
{
    struct expand_env_data env = { .domid = domid, .idstr = "" };
    return ddsrt_expand_vars_sh(src0, &expand_lookup_env, &env);
}

char *ddsrt_expand_envvars (const char *src0, uint32_t domid)
{
    struct expand_env_data env = { .domid = domid, .idstr = "" };
    return ddsrt_expand_vars(src0, &expand_lookup_env, &env);
}

