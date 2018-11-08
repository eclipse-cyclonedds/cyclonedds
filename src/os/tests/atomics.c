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
#include "os/os.h"
#include "CUnit/Test.h"

uint32_t _osuint32 = 0;
uint64_t _osuint64 = 0;
// os_address is uintptr_t
uintptr_t _osaddress = 0;
ptrdiff_t _ptrdiff = 0;
void * _osvoidp = (uintptr_t *)0;

CU_Test(os_atomics, load_store)
{
   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(5);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(5);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(5);
   volatile os_atomic_voidp_t voidp = OS_ATOMIC_VOIDP_INIT((uintptr_t)5);

   /* Test uint32 LD-ST */
   printf ("Starting os_atomics_load_store_001\n");
   CU_ASSERT (os_atomic_ld32 (&uint32) == 5); /* Returns contents of uint32 */
   os_atomic_st32 (&uint32, _osuint32); /* Writes os_uint32 into uint32 */
   CU_ASSERT (os_atomic_ld32 (&uint32) == _osuint32);

   /* Test uint64 LD-ST */
   printf ("Starting os_atomics_load_store_002\n");
#if OS_ATOMIC64_SUPPORT
   CU_ASSERT (os_atomic_ld64 (&uint64) == 5);
   os_atomic_st64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == _osuint64);
#endif

   /* Test uintptr LD-ST */
   printf ("Starting os_atomics_load_store_003\n");
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 5);
   os_atomic_stptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == _osaddress);

   /* Test uintvoidp LD-ST */
   printf ("Starting os_atomics_load_store_004\n");
   CU_ASSERT (os_atomic_ldvoidp (&voidp) == (uintptr_t*)5);
   os_atomic_stvoidp (&voidp, _osvoidp);
   CU_ASSERT (os_atomic_ldvoidp (&voidp) == (uintptr_t*)_osvoidp);

   printf ("Ending atomics_load_store\n");
}

CU_Test(os_atomics, compare_and_swap)
{
   /* Compare and Swap
    * if (ptr == expected) { ptr = newval; }
    */
   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(0);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(0);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(0);
   volatile os_atomic_voidp_t uintvoidp = OS_ATOMIC_VOIDP_INIT((uintptr_t)0);
   _osuint32 = 1;
   _osuint64 = 1;
   _osaddress = 1;
   _osvoidp = (uintptr_t *)1;
   uint32_t expected = 0, newval = 5;
   uintptr_t addr_expected = 0, addr_newval = 5;
   void *void_expected = (uintptr_t*)0;
   void *void_newval = (uintptr_t*)5;
   int ret = 0;

   /* Test os_atomic_cas32 */
   printf ("Starting os_atomics_compare_and_swap_001\n");
   ret = os_atomic_cas32 (&uint32, expected, newval);
   CU_ASSERT (os_atomic_ld32 (&uint32) == newval && ret == 1);
   os_atomic_st32 (&uint32, _osuint32);
   ret = os_atomic_cas32 (&uint32, expected, newval);
   CU_ASSERT (os_atomic_ld32 (&uint32) != newval && ret == 0);

   /* Test os_atomic_cas64 */
   printf ("Starting os_atomics_compare_and_swap_002\n");
#if OS_ATOMIC64_SUPPORT
   ret = os_atomic_cas64 (&uint64, expected, newval);
   CU_ASSERT (os_atomic_ld64 (&uint64) == newval && ret == 1);
   os_atomic_st64 (&uint64, _osuint64);
   ret = os_atomic_cas64 (&uint64, expected, newval);
   CU_ASSERT (os_atomic_ld64 (&uint64) != newval && ret == 0);
#endif

   /* Test os_atomic_casptr */
   printf ("Starting os_atomics_compare_and_swap_003\n");
   ret = os_atomic_casptr (&uintptr, addr_expected, addr_newval);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == addr_newval && ret == 1);
   os_atomic_stptr (&uintptr, _osaddress);
   ret = os_atomic_casptr (&uintptr, addr_expected, addr_newval);
   CU_ASSERT (os_atomic_ldptr (&uintptr) != addr_newval && ret == 0);

   /* Test os_atomic_casvoidp */
   printf ("Starting os_atomics_compare_and_swap_003\n");
   ret = os_atomic_casvoidp (&uintvoidp, void_expected, void_newval);
   CU_ASSERT (os_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)void_newval && ret == 1);
   os_atomic_stvoidp (&uintvoidp, _osvoidp);
   ret = os_atomic_casvoidp (&uintvoidp, void_expected, void_newval);
   CU_ASSERT (os_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)1 && ret == 0);

   printf ("Ending atomics_compare_and_swap\n");
}

CU_Test(os_atomics, increment)
{
   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(0);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(0);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(0);
   _osuint32 = 0;
   _osuint64 = 0;
   _osaddress = 0;
   _osvoidp = (uintptr_t *)0;

   /* Test os_inc32 */
   printf ("Starting os_atomics_increment_001\n");
   os_atomic_inc32 (&uint32);
   CU_ASSERT (os_atomic_ld32 (&uint32) == 1);

   /* Test os_inc64 */
   printf ("Starting os_atomics_increment_002\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_inc64 (&uint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == 1);
#endif

   /* Test os_incptr */
   printf ("Starting os_atomics_increment_003\n");
   os_atomic_incptr (&uintptr);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 1);

   /* Test os_atomic_inc32_nv */
   printf ("Starting os_atomics_increment_004\n");
   os_atomic_st32 (&uint32, _osuint32);
   CU_ASSERT (os_atomic_inc32_nv (&uint32) == 1);

   /* Test os_atomic_inc64_nv */
   printf ("Starting os_atomics_increment_005\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_st64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_inc64_nv (&uint64) == 1);
#endif

   /* Test os_atomic_incptr_nv */
   printf ("Starting os_atomics_increment_006\n");
   os_atomic_stptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_incptr_nv(&uintptr) == 1);

   printf ("Ending atomics_increment\n");
}

CU_Test(os_atomics, decrement)
{
   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(1);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(1);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(1);
   _osuint32 = 1;
   _osuint64 = 1;
   _osaddress = 1;
   _osvoidp = (uintptr_t *)1;

   /* Test os_atomic_dec32 */
   printf ("Starting os_atomics_decrement_001\n");
   os_atomic_dec32 (&uint32);
   CU_ASSERT (os_atomic_ld32 (&uint32) == 0);

   /* Test os_atomic_dec64 */
   printf ("Starting os_atomics_decrement_002\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_dec64 (&uint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == 0);
#endif

   /* Test os_atomic_decptr */
   printf ("Starting os_atomics_decrement_003\n");
   os_atomic_decptr (&uintptr);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 0);

   /* Test os_atomic_dec32_nv */
   printf ("Starting os_atomics_decrement_004\n");
   os_atomic_st32 (&uint32, _osuint32);
   CU_ASSERT (os_atomic_dec32_nv (&uint32) == 0);

   /* Test os_atomic_dec64_nv */
   printf ("Starting os_atomics_decrement_005\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_st64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_dec64_nv (&uint64) == 0);
#endif

   /* Test os_atomic_decptr_nv */
   printf ("Starting os_atomics_decrement_006\n");
   os_atomic_stptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_decptr_nv(&uintptr) == 0);

   printf ("Ending atomics_decrement\n");
}

CU_Test(os_atomics, add)
{
   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(1);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(1);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(1);
   volatile os_atomic_voidp_t uintvoidp = OS_ATOMIC_VOIDP_INIT((uintptr_t)1);
   _osuint32 = 2;
   _osuint64 = 2;
   _osaddress = 2;
   _ptrdiff = 2;

   /* Test os_atomic_add32 */
   printf ("Starting os_atomics_add_001\n");
   os_atomic_add32 (&uint32, _osuint32);
   CU_ASSERT (os_atomic_ld32 (&uint32) == 3);

   /* Test os_atomic_add64 */
   printf ("Starting os_atomics_add_002\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_add64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == 3);
#endif

   /* Test os_atomic_addptr */
   printf ("Starting os_atomics_add_003\n");
   os_atomic_addptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 3);

   /* Test os_atomic_addvoidp */
   printf ("Starting os_atomics_add_004\n");
   os_atomic_addvoidp (&uintvoidp, _ptrdiff);
   CU_ASSERT (os_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)3);

   /* Test os_atomic_add32_nv */
   printf ("Starting os_atomics_add_005\n");
   os_atomic_st32 (&uint32, 1);
   CU_ASSERT (os_atomic_add32_nv (&uint32, _osuint32) == 3);

   /* Test os_atomic_add64_nv */
   printf ("Starting os_atomics_add_006\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_st64 (&uint64, 1);
   CU_ASSERT (os_atomic_add64_nv (&uint64, _osuint64) == 3);
#endif

   /* Test os_atomic_addptr_nv */
   printf ("Starting os_atomics_add_007\n");
   os_atomic_stptr (&uintptr, 1);
   CU_ASSERT (os_atomic_addptr_nv (&uintptr, _osaddress) == 3);

   /* Test os_atomic_addvoidp_nv */
   printf ("Starting os_atomics_add_008\n");
   os_atomic_stvoidp (&uintvoidp, (uintptr_t*)1);
   CU_ASSERT (os_atomic_addvoidp_nv (&uintvoidp, _ptrdiff) == (uintptr_t*)3);

   printf ("Ending atomics_add\n");
}

CU_Test(os_atomics, subtract)
{
   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(5);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(5);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(5);
   volatile os_atomic_voidp_t uintvoidp = OS_ATOMIC_VOIDP_INIT((uintptr_t)5);
   _osuint32 = 2;
   _osuint64 = 2;
   _osaddress = 2;
   _ptrdiff = 2;

   /* Test os_atomic_sub32 */
   printf ("Starting os_atomics_subtract_001\n");
   os_atomic_sub32 (&uint32, _osuint32);
   CU_ASSERT (os_atomic_ld32 (&uint32) == 3);

   /* Test os_atomic_sub64 */
   printf ("Starting os_atomics_subtract_002\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_sub64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == 3);
#endif

   /* Test os_atomic_subptr */
   printf ("Starting os_atomics_subtract_003\n");
   os_atomic_subptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 3);

   /* Test os_atomic_subvoidp */
   printf ("Starting os_atomics_subtract_004\n");
   os_atomic_subvoidp (&uintvoidp, _ptrdiff);
   CU_ASSERT (os_atomic_ldvoidp (&uintvoidp) == (uintptr_t*)3);

   /* Test os_atomic_sub32_nv */
   printf ("Starting os_atomics_subtract_005\n");
   os_atomic_st32 (&uint32, 5);
   CU_ASSERT (os_atomic_sub32_nv (&uint32, _osuint32) == 3);

   /* Test os_atomic_sub64_nv */
   printf ("Starting os_atomics_subtract_006\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_st64 (&uint64, 5);
   CU_ASSERT (os_atomic_sub64_nv (&uint64, _osuint64) == 3);
#endif

   /* Test os_atomic_subptr_nv */
   printf ("Starting os_atomics_subtract_007\n");
   os_atomic_stptr (&uintptr, 5);
   CU_ASSERT (os_atomic_subptr_nv (&uintptr, _osaddress) == 3);

   /* Test os_atomic_subvoidp_nv */
   printf ("Starting os_atomics_subtract_008\n");
   os_atomic_stvoidp (&uintvoidp, (uintptr_t*)5);
   CU_ASSERT (os_atomic_subvoidp_nv (&uintvoidp, _ptrdiff) == (void *)3);

   printf ("Ending atomics_subtract\n");
}

CU_Test(os_atomics, and)
{
   /* AND Operation:

     150  010010110
     500  111110100

     148  010010100 */

   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(150);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(150);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(150);
   _osuint32 = 500;
   _osuint64 = 500;
   _osaddress = 500;

   /* Test os_atomic_and32 */
   printf ("Starting os_atomics_and_001\n");
   os_atomic_and32 (&uint32, _osuint32);
   CU_ASSERT (os_atomic_ld32 (&uint32) == 148);

   /* Test os_atomic_and64 */
   printf ("Starting os_atomics_and_002\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_and64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == 148);
#endif

   /* Test os_atomic_andptr */
   printf ("Starting os_atomics_and_003\n");
   os_atomic_andptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 148);

   /* Test os_atomic_and32_ov */
   printf ("Starting os_atomics_and_004\n");
   CU_ASSERT (os_atomic_and32_ov (&uint32, _osuint32) == 148);

   /* Test os_atomic_and64_ov */
   printf ("Starting os_atomics_and_005\n");
#if OS_ATOMIC64_SUPPORT
   CU_ASSERT (os_atomic_and64_ov (&uint64, _osuint64) == 148);
#endif

   /* Test os_atomic_andptr_ov */
   printf ("Starting os_atomics_and_006\n");
   CU_ASSERT (os_atomic_andptr_ov (&uintptr, _osaddress) == 148);

   /* Test os_atomic_and32_nv */
   printf ("Starting os_atomics_and_007\n");
   CU_ASSERT (os_atomic_and32_nv (&uint32, _osuint32) == 148);

   /* Test os_atomic_and64_nv */
   printf ("Starting os_atomics_and_008\n");
#if OS_ATOMIC64_SUPPORT
   CU_ASSERT (os_atomic_and64_nv (&uint64, _osuint64) == 148);
 #endif

   /* Test os_atomic_andptr_nv */
   printf ("Starting os_atomics_and_009\n");
   CU_ASSERT (os_atomic_andptr_nv (&uintptr, _osaddress) == 148);

   printf ("Ending atomics_and\n");
}

CU_Test(os_atomics, or)
{
   /* OR Operation:

     150  010010110
     500  111110100

     502  111110110 */

   volatile os_atomic_uint32_t uint32 = OS_ATOMIC_UINT32_INIT(150);
#if OS_ATOMIC64_SUPPORT
   volatile os_atomic_uint64_t uint64 = OS_ATOMIC_UINT64_INIT(150);
#endif
   volatile os_atomic_uintptr_t uintptr = OS_ATOMIC_UINTPTR_INIT(150);
   _osuint32 = 500;
   _osuint64 = 500;
   _osaddress = 500;

   /* Test os_atomic_or32 */
   printf ("Starting os_atomics_or_001\n");
   os_atomic_or32 (&uint32, _osuint32);
   CU_ASSERT (os_atomic_ld32 (&uint32) == 502);

   /* Test os_atomic_or64 */
   printf ("Starting os_atomics_or_002\n");
#if OS_ATOMIC64_SUPPORT
   os_atomic_or64 (&uint64, _osuint64);
   CU_ASSERT (os_atomic_ld64 (&uint64) == 502);
#endif

   /* Test os_atomic_orptr */
   printf ("Starting os_atomics_or_003\n");
   os_atomic_orptr (&uintptr, _osaddress);
   CU_ASSERT (os_atomic_ldptr (&uintptr) == 502);

   /* Test os_atomic_or32_ov */
   printf ("Starting os_atomics_or_004\n");
   CU_ASSERT (os_atomic_or32_ov (&uint32, _osuint32) == 502);

   /* Test os_atomic_or64_ov */
   printf ("Starting os_atomics_or_005\n");
#if OS_ATOMIC64_SUPPORT
   CU_ASSERT (os_atomic_or64_ov (&uint64, _osuint64) == 502);
#endif

   /* Test os_atomic_orptr_ov */
   printf ("Starting os_atomics_or_006\n");
   CU_ASSERT (os_atomic_orptr_ov (&uintptr, _osaddress) == 502);

   /* Test os_atomic_or32_nv */
   printf ("Starting os_atomics_or_007\n");
   CU_ASSERT (os_atomic_or32_nv (&uint32, _osuint32) == 502);

   /* Test os_atomic_or64_nv */
   printf ("Starting os_atomics_or_008\n");
#if OS_ATOMIC64_SUPPORT
   CU_ASSERT (os_atomic_or64_nv (&uint64, _osuint64) == 502);
#endif

   /* Test os_atomic_orptr_nv */
   printf ("Starting os_atomics_or_009\n");
   CU_ASSERT (os_atomic_orptr_nv (&uintptr, _osaddress) == 502);

   printf ("Ending atomics_or\n");
}

