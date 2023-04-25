// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dl.h"

/* source used for the test library */
static int g_val = -1;

#if _WIN32
__declspec(dllexport)
#endif
void set_int(int val);

#if _WIN32
__declspec(dllexport)
#endif
int get_int(void);

void set_int(int val) { g_val = val; }

int get_int(void) { return g_val; }
