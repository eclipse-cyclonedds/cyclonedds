// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_COUNTARGS_H
#define DDSRT_COUNTARGS_H

/** @file countargs.h
  @brief This header provides a macro @ref DDSRT_COUNT_ARGS that returns the number of arguments provided
  (the other macros are just helper macros for @ref DDSRT_COUNT_ARGS)
*/

#define DDSRT_COUNT_ARGS_MSVC_WORKAROUND(x) x

/** @brief Returns the number of arguments provided (at most 20) */
#define DDSRT_COUNT_ARGS(...) DDSRT_COUNT_ARGS1 (__VA_ARGS__, 20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)
#define DDSRT_COUNT_ARGS1(...) DDSRT_COUNT_ARGS_MSVC_WORKAROUND (DDSRT_COUNT_ARGS_ARGN (__VA_ARGS__))
#define DDSRT_COUNT_ARGS_ARGN(a,b,c,d,e,f,g,h,i,j, k,l,m,n,o,p,q,r,s,t, N,...) N

#endif
