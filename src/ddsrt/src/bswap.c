// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/export.h"
#include "dds/ddsrt/bswap.h"

DDS_EXPORT extern inline uint16_t ddsrt_bswap2u (uint16_t x);
DDS_EXPORT extern inline uint32_t ddsrt_bswap4u (uint32_t x);
DDS_EXPORT extern inline uint64_t ddsrt_bswap8u (uint64_t x);
DDS_EXPORT extern inline int16_t ddsrt_bswap2 (int16_t x);
DDS_EXPORT extern inline int32_t ddsrt_bswap4 (int32_t x);
DDS_EXPORT extern inline int64_t ddsrt_bswap8 (int64_t x);
