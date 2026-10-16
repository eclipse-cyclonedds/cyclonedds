// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <zephyr/random/random.h>

#include "dds/ddsrt/random.h"

bool ddsrt_prng_makeseed (struct ddsrt_prng_seed *seed)
{
  sys_rand_get (seed->key, sizeof (seed->key));
  return true;
}
