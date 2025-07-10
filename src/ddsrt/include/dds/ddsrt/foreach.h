// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_FOREACH_H
#define DDSRT_FOREACH_H

/** @file foreach.h
  @brief This header provides macros for operating on each argument provided
*/

#include "dds/ddsrt/countargs.h"

#define DDSRT_FOREACH_3_MSVC_WORKAROUND(x) x
#define DDSRT_FOREACH_3_1(f, sep, x)       f(x)
#define DDSRT_FOREACH_3_2(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_1(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_3(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_2(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_4(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_3(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_5(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_4(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_6(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_5(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_7(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_6(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_8(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_7(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_9(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_8(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_10(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_9(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_11(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_10(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_12(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_11(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_13(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_12(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_14(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_13(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_15(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_14(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_16(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_15(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_17(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_16(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_18(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_17(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_19(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_18(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_3_20(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_19(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_2(n, f, sep, ...) DDSRT_FOREACH_3_MSVC_WORKAROUND(DDSRT_FOREACH_3_##n(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_1(n, f, sep, ...) DDSRT_FOREACH_2(n, f, sep, __VA_ARGS__)
#define DDSRT_FOREACH(f, sep, ...) DDSRT_FOREACH_1(DDSRT_COUNT_ARGS(__VA_ARGS__), f, sep,  __VA_ARGS__)
#define DDSRT_FOREACH_WRAP(f, sep, ...) DDSRT_FOREACH(f, sep, __VA_ARGS__)

// 2nd set of DDSRT_FOREACH macros because we need two levels of DDSRT_FOREACH's but the
// C preprocessor doesn't allow recursive macro expansion
#define DDSRT_FOREACH_B_3_MSVC_WORKAROUND(x) x
#define DDSRT_FOREACH_B_3_1(f, sep, x)       f(x)
#define DDSRT_FOREACH_B_3_2(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_1(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_3(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_2(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_4(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_3(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_5(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_4(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_6(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_5(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_7(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_6(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_8(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_7(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_9(f, sep, x, ...)  f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_8(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_10(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_9(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_11(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_10(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_12(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_11(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_13(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_12(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_14(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_13(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_15(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_14(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_16(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_15(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_17(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_16(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_18(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_17(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_19(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_18(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_3_20(f, sep, x, ...) f(x) sep() DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_19(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_2(n, f, sep, ...) DDSRT_FOREACH_B_3_MSVC_WORKAROUND(DDSRT_FOREACH_B_3_##n(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_B_1(n, f, sep, ...) DDSRT_FOREACH_B_2(n, f, sep, __VA_ARGS__)
#define DDSRT_FOREACH_B(f, sep, ...) DDSRT_FOREACH_B_1(DDSRT_COUNT_ARGS(__VA_ARGS__), f, sep, __VA_ARGS__)
#define DDSRT_FOREACH_B_WRAP(f, sep, ...) DDSRT_FOREACH_B(f, sep, __VA_ARGS__)

/* Operating on pairs of arguments */
#define DDSRT_COUNT_PAIRS_MSVC_WORKAROUND(x) x
#define DDSRT_COUNT_PAIRS(...) DDSRT_COUNT_PAIRS1 (__VA_ARGS__, 24,24,23,23,22,22,21,21,20,20,19,19,18,18,17,17,16,16,15,15,14,14,13,13,12,12,11,11,10,10,9,9,8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0)
#define DDSRT_COUNT_PAIRS1(...) DDSRT_COUNT_PAIRS_MSVC_WORKAROUND (DDSRT_COUNT_PAIRS_ARGN (__VA_ARGS__))
#define DDSRT_COUNT_PAIRS_ARGN(a,a1,b,b1,c,c1,d,d1,e,e1,f,f1,g,g1,h,h1,i,i1,j,j1, k,k1,l,l1,m,m1,n,n1,o,o1,p,p1,q,q1,r,r1,s,s1,t,t1, u,u1,v,v1,w,w1,x,x1, N,...) N

#define DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(x) x
#define DDSRT_FOREACH_PAIR_3_1(f, sep, x,y)       f(x,y)
#define DDSRT_FOREACH_PAIR_3_2(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_1(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_3(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_2(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_4(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_3(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_5(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_4(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_6(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_5(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_7(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_6(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_8(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_7(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_9(f, sep, x,y, ...)  f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_8(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_10(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_9(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_11(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_10(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_12(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_11(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_13(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_12(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_14(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_13(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_15(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_14(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_16(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_15(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_17(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_16(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_18(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_17(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_19(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_18(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_20(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_19(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_21(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_20(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_22(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_21(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_23(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_22(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_24(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_23(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_3_25(f, sep, x,y, ...) f(x,y) sep() DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_24(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_2(n, f, sep, ...) DDSRT_FOREACH_PAIR_3_MSVC_WORKAROUND(DDSRT_FOREACH_PAIR_3_##n(f, sep, __VA_ARGS__))
#define DDSRT_FOREACH_PAIR_1(n, f, sep, ...) DDSRT_FOREACH_PAIR_2(n, f, sep, __VA_ARGS__)
#define DDSRT_FOREACH_PAIR(f, sep, ...) DDSRT_FOREACH_PAIR_1(DDSRT_COUNT_PAIRS(__VA_ARGS__), f, sep,  __VA_ARGS__)
#define DDSRT_FOREACH_PAIR_WRAP(f, sep, ...) DDSRT_FOREACH_PAIR(f, sep, __VA_ARGS__)

#endif
