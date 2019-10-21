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
#include "CUnit/Test.h"
#include "cyclonedds/ddsrt/atomics.h"

uint32_t _osuint32 = 0;
uint64_t _osuint64 = 0;
uintptr_t _osaddress = 0;
ptrdiff_t _ptrdiff = 0;
void * _osvoidp = (uintptr_t *)0;

CU_Test(ddsrt_atomics, load_store)
{
  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(5);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(5);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(5);
  volatile ddsrt_atomic_voidp_t voidp = DDSRT_ATOMIC_VOIDP_INIT((uintptr_t)5);

  /* Test uint32 LD-ST */
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 5); /* Returns contents of uint32 */
  ddsrt_atomic_st32 (&uint32, _osuint32); /* Writes os_uint32 into uint32 */
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == _osuint32);

  /* Test uint64 LD-ST */
#if DDSRT_HAVE_ATOMIC64
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 5);
  ddsrt_atomic_st64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == _osuint64);
#endif

  /* Test uintptr LD-ST */
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 5);
  ddsrt_atomic_stptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == _osaddress);

  /* Test uintvoidp LD-ST */
  CU_ASSERT (ddsrt_atomic_ldvoidp (&voidp) == (uintptr_t*)5);
  ddsrt_atomic_stvoidp (&voidp, _osvoidp);
  CU_ASSERT (ddsrt_atomic_ldvoidp (&voidp) == (uintptr_t*)_osvoidp);
}

CU_Test(ddsrt_atomics, compare_and_swap)
{
  /* Compare and Swap if (ptr == expected) { ptr = newval; } */
  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(0);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(0);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(0);
  volatile ddsrt_atomic_voidp_t uintvoidp = DDSRT_ATOMIC_VOIDP_INIT((uintptr_t)0);
  _osuint32 = 1;
  _osuint64 = 1;
  _osaddress = 1;
  _osvoidp = (uintptr_t *)1;
  uint32_t expected = 0, newval = 5;
  uintptr_t addr_expected = 0, addr_newval = 5;
  void *void_expected = (uintptr_t*)0;
  void *void_newval = (uintptr_t*)5;
  int ret = 0;

  /* Test ddsrt_atomic_cas32 */
  ret = ddsrt_atomic_cas32 (&uint32, expected, newval);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == newval && ret == 1);
  ddsrt_atomic_st32 (&uint32, _osuint32);
  ret = ddsrt_atomic_cas32 (&uint32, expected, newval);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) != newval && ret == 0);

  /* Test ddsrt_atomic_cas64 */
#if DDSRT_HAVE_ATOMIC64
  ret = ddsrt_atomic_cas64 (&uint64, expected, newval);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == newval && ret == 1);
  ddsrt_atomic_st64 (&uint64, _osuint64);
  ret = ddsrt_atomic_cas64 (&uint64, expected, newval);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) != newval && ret == 0);
#endif

  /* Test ddsrt_atomic_casptr */
  ret = ddsrt_atomic_casptr (&uintptr, addr_expected, addr_newval);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == addr_newval && ret == 1);
  ddsrt_atomic_stptr (&uintptr, _osaddress);
  ret = ddsrt_atomic_casptr (&uintptr, addr_expected, addr_newval);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) != addr_newval && ret == 0);

  /* Test ddsrt_atomic_casvoidp */
  ret = ddsrt_atomic_casvoidp (&uintvoidp, void_expected, void_newval);
  CU_ASSERT (ddsrt_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)void_newval && ret == 1);
  ddsrt_atomic_stvoidp (&uintvoidp, _osvoidp);
  ret = ddsrt_atomic_casvoidp (&uintvoidp, void_expected, void_newval);
  CU_ASSERT (ddsrt_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)1 && ret == 0);
}

CU_Test(ddsrt_atomics, increment)
{
  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(0);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(0);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(0);
  _osuint32 = 0;
  _osuint64 = 0;
  _osaddress = 0;
  _osvoidp = (uintptr_t *)0;

  /* Test os_inc32 */
  ddsrt_atomic_inc32 (&uint32);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 1);

  /* Test os_inc64 */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_inc64 (&uint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 1);
#endif

  /* Test os_incptr */
  ddsrt_atomic_incptr (&uintptr);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 1);

  /* Test ddsrt_atomic_inc32_nv */
  ddsrt_atomic_st32 (&uint32, _osuint32);
  CU_ASSERT (ddsrt_atomic_inc32_nv (&uint32) == 1);

  /* Test ddsrt_atomic_inc64_nv */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_st64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_inc64_nv (&uint64) == 1);
#endif

  /* Test ddsrt_atomic_incptr_nv */
  ddsrt_atomic_stptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_incptr_nv(&uintptr) == 1);
}

CU_Test(ddsrt_atomics, decrement)
{
  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(1);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(1);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(1);
  _osuint32 = 1;
  _osuint64 = 1;
  _osaddress = 1;
  _osvoidp = (uintptr_t *)1;

  /* Test ddsrt_atomic_dec32 */
  ddsrt_atomic_dec32 (&uint32);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 0);

  /* Test ddsrt_atomic_dec64 */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_dec64 (&uint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 0);
#endif

  /* Test ddsrt_atomic_decptr */
  ddsrt_atomic_decptr (&uintptr);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 0);

  /* Test ddsrt_atomic_dec32_nv */
  ddsrt_atomic_st32 (&uint32, _osuint32);
  CU_ASSERT (ddsrt_atomic_dec32_nv (&uint32) == 0);

  /* Test ddsrt_atomic_dec64_nv */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_st64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_dec64_nv (&uint64) == 0);
#endif

  /* Test ddsrt_atomic_decptr_nv */
  ddsrt_atomic_stptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_decptr_nv(&uintptr) == 0);
}

CU_Test(ddsrt_atomics, add)
{
  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(1);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(1);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(1);
  volatile ddsrt_atomic_voidp_t uintvoidp = DDSRT_ATOMIC_VOIDP_INIT((uintptr_t)1);
  _osuint32 = 2;
  _osuint64 = 2;
  _osaddress = 2;
  _ptrdiff = 2;

  /* Test ddsrt_atomic_add32 */
  ddsrt_atomic_add32 (&uint32, _osuint32);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 3);

  /* Test ddsrt_atomic_add64 */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_add64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 3);
#endif

  /* Test ddsrt_atomic_addptr */
  ddsrt_atomic_addptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 3);

  /* Test ddsrt_atomic_addvoidp */
  ddsrt_atomic_addvoidp (&uintvoidp, _ptrdiff);
  CU_ASSERT (ddsrt_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)3);

  /* Test ddsrt_atomic_add32_nv */
  ddsrt_atomic_st32 (&uint32, 1);
  CU_ASSERT (ddsrt_atomic_add32_nv (&uint32, _osuint32) == 3);

  /* Test ddsrt_atomic_add64_nv */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_st64 (&uint64, 1);
  CU_ASSERT (ddsrt_atomic_add64_nv (&uint64, _osuint64) == 3);
#endif

  /* Test ddsrt_atomic_addptr_nv */
  ddsrt_atomic_stptr (&uintptr, 1);
  CU_ASSERT (ddsrt_atomic_addptr_nv (&uintptr, _osaddress) == 3);

  /* Test ddsrt_atomic_addvoidp_nv */
  ddsrt_atomic_stvoidp (&uintvoidp, (uintptr_t*)1);
  CU_ASSERT (ddsrt_atomic_addvoidp_nv (&uintvoidp, _ptrdiff) == (uintptr_t*)3);
}

CU_Test(ddsrt_atomics, subtract)
{
  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(5);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(5);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(5);
  volatile ddsrt_atomic_voidp_t uintvoidp = DDSRT_ATOMIC_VOIDP_INIT((uintptr_t)5);
  _osuint32 = 2;
  _osuint64 = 2;
  _osaddress = 2;
  _ptrdiff = 2;

  /* Test ddsrt_atomic_sub32 */
  ddsrt_atomic_sub32 (&uint32, _osuint32);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 3);

  /* Test ddsrt_atomic_sub64 */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_sub64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 3);
#endif

  /* Test ddsrt_atomic_subptr */
  ddsrt_atomic_subptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 3);

  /* Test ddsrt_atomic_subvoidp */
  ddsrt_atomic_subvoidp (&uintvoidp, _ptrdiff);
  CU_ASSERT (ddsrt_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)3);

  /* Test ddsrt_atomic_sub32_nv */
  ddsrt_atomic_st32 (&uint32, 5);
  CU_ASSERT (ddsrt_atomic_sub32_nv (&uint32, _osuint32) == 3);

  /* Test ddsrt_atomic_sub64_nv */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_st64 (&uint64, 5);
  CU_ASSERT (ddsrt_atomic_sub64_nv (&uint64, _osuint64) == 3);
#endif

  /* Test ddsrt_atomic_subptr_nv */
  ddsrt_atomic_stptr (&uintptr, 5);
  CU_ASSERT (ddsrt_atomic_subptr_nv (&uintptr, _osaddress) == 3);

  /* Test ddsrt_atomic_subvoidp_nv */
  ddsrt_atomic_stvoidp (&uintvoidp, (uintptr_t*)5);
  CU_ASSERT (ddsrt_atomic_subvoidp_nv (&uintvoidp, _ptrdiff) == (void *)3);
}

CU_Test(ddsrt_atomics, and)
{
  /* AND Operation:

     150  010010110
     500  111110100

     148  010010100 */

  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(150);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(150);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(150);
  _osuint32 = 500;
  _osuint64 = 500;
  _osaddress = 500;

  /* Test ddsrt_atomic_and32 */
  ddsrt_atomic_and32 (&uint32, _osuint32);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 148);

  /* Test ddsrt_atomic_and64 */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_and64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 148);
#endif

  /* Test ddsrt_atomic_andptr */
  ddsrt_atomic_andptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 148);

  /* Test ddsrt_atomic_and32_ov */
  CU_ASSERT (ddsrt_atomic_and32_ov (&uint32, _osuint32) == 148);

  /* Test ddsrt_atomic_and64_ov */
#if DDSRT_HAVE_ATOMIC64
  CU_ASSERT (ddsrt_atomic_and64_ov (&uint64, _osuint64) == 148);
#endif

  /* Test ddsrt_atomic_andptr_ov */
  CU_ASSERT (ddsrt_atomic_andptr_ov (&uintptr, _osaddress) == 148);

  /* Test ddsrt_atomic_and32_nv */
  CU_ASSERT (ddsrt_atomic_and32_nv (&uint32, _osuint32) == 148);

  /* Test ddsrt_atomic_and64_nv */
#if DDSRT_HAVE_ATOMIC64
  CU_ASSERT (ddsrt_atomic_and64_nv (&uint64, _osuint64) == 148);
 #endif

  /* Test ddsrt_atomic_andptr_nv */
  CU_ASSERT (ddsrt_atomic_andptr_nv (&uintptr, _osaddress) == 148);
}

CU_Test(ddsrt_atomics, or)
{
  /* OR Operation:

     150  010010110
     500  111110100

     502  111110110 */

  volatile ddsrt_atomic_uint32_t uint32 = DDSRT_ATOMIC_UINT32_INIT(150);
#if DDSRT_HAVE_ATOMIC64
  volatile ddsrt_atomic_uint64_t uint64 = DDSRT_ATOMIC_UINT64_INIT(150);
#endif
  volatile ddsrt_atomic_uintptr_t uintptr = DDSRT_ATOMIC_UINTPTR_INIT(150);
  _osuint32 = 500;
  _osuint64 = 500;
  _osaddress = 500;

  /* Test ddsrt_atomic_or32 */
  ddsrt_atomic_or32 (&uint32, _osuint32);
  CU_ASSERT (ddsrt_atomic_ld32 (&uint32) == 502);

  /* Test ddsrt_atomic_or64 */
#if DDSRT_HAVE_ATOMIC64
  ddsrt_atomic_or64 (&uint64, _osuint64);
  CU_ASSERT (ddsrt_atomic_ld64 (&uint64) == 502);
#endif

  /* Test ddsrt_atomic_orptr */
  ddsrt_atomic_orptr (&uintptr, _osaddress);
  CU_ASSERT (ddsrt_atomic_ldptr (&uintptr) == 502);

  /* Test ddsrt_atomic_or32_ov */
  CU_ASSERT (ddsrt_atomic_or32_ov (&uint32, _osuint32) == 502);

  /* Test ddsrt_atomic_or64_ov */
#if DDSRT_HAVE_ATOMIC64
  CU_ASSERT (ddsrt_atomic_or64_ov (&uint64, _osuint64) == 502);
#endif

  /* Test ddsrt_atomic_orptr_ov */
  CU_ASSERT (ddsrt_atomic_orptr_ov (&uintptr, _osaddress) == 502);

  /* Test ddsrt_atomic_or32_nv */
  CU_ASSERT (ddsrt_atomic_or32_nv (&uint32, _osuint32) == 502);

  /* Test ddsrt_atomic_or64_nv */
#if DDSRT_HAVE_ATOMIC64
  CU_ASSERT (ddsrt_atomic_or64_nv (&uint64, _osuint64) == 502);
#endif

  /* Test ddsrt_atomic_orptr_nv */
  CU_ASSERT (ddsrt_atomic_orptr_nv (&uintptr, _osaddress) == 502);
}
