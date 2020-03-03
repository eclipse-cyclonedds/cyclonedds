/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef SECURITY_CORE_TEST_SECURITY_CONFIG_TEST_UTILS_H_
#define SECURITY_CORE_TEST_SECURITY_CONFIG_TEST_UTILS_H_

#include <stdlib.h>
#include "dds/ddsrt/environ.h"

struct kvp {
  const char *key;
  const char *value;
  int32_t count;
};

const char * expand_lookup_vars (const char *name, void * data);
const char * expand_lookup_vars_env (const char *name, void * data);
int32_t expand_lookup_unmatched (const struct kvp * lookup_table);

char * get_governance_config (struct kvp *config_vars);

#endif /* SECURITY_CORE_TEST_SECURITY_CONFIG_TEST_UTILS_H_ */
