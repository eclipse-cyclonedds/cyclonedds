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
#include <assert.h>
#include <stdlib.h>

#if WIN32
#define _CRT_RAND_S
#include <errno.h>

long random (void)
{
    /* rand() is a really terribly bad PRNG */
    /* FIXME: Indeed (especially if not seeded), use rand_s instead. */
    union { long x; unsigned char c[4]; } t;
    int i;
    for (i = 0; i < 4; i++)
        t.c[i] = (unsigned char) ((rand () >> 4) & 0xff);
#if RAND_MAX == INT32_MAX || RAND_MAX == 0x7fff
    t.x &= RAND_MAX;
#elif RAND_MAX <= 0x7ffffffe
    t.x %= (RAND_MAX+1);
#else
#error "RAND_MAX out of range"
#endif
  return t.x;
}
#endif

long os_random(void)
{
    /* FIXME: Not MT-safe, should use random_r (or a real PRNG) instead. */
    return random();
}
