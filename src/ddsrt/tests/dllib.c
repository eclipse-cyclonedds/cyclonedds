/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "lib_test_export.h"

static int g_val = -1;

LIB_TEST_EXPORT void set_int(int val)
{
  g_val = val;
}

LIB_TEST_EXPORT int get_int(void)
{
  return g_val;
}

