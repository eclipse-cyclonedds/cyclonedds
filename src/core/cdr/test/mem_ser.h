// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_TEST_MEM_SER_H
#define DDSI_TEST_MEM_SER_H

#include "dds/ddsrt/endian.h"

#define SER8(v) ((unsigned char) (v))
#if DDSRT_ENDIAN == DDSRT_BIG_ENDIAN
#define SER16(v) \
  (unsigned char)(((uint16_t)(v) >>  8) & 0xff), \
  (unsigned char)( (uint16_t)(v)        & 0xff)
#define SER16BE(v) SER16(v)
#define SER32(v) \
  (unsigned char)( (uint32_t)(v) >> 24        ), \
  (unsigned char)(((uint32_t)(v) >> 16) & 0xff), \
  (unsigned char)(((uint32_t)(v) >>  8) & 0xff), \
  (unsigned char)( (uint32_t)(v)        & 0xff)
#define SER32BE(v) SER32(v)
#define SER64(v) \
  (unsigned char)( (uint64_t)(v) >> 56),         \
  (unsigned char)(((uint64_t)(v) >> 48) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 40) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 32) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 24) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 16) & 0xff), \
  (unsigned char)(((uint64_t)(v) >>  8) & 0xff), \
  (unsigned char)( (uint64_t)(v)        & 0xff)
#define SER64BE(v) SER64(v)
#else
#define SER16(v) \
  (unsigned char)( (uint16_t)(v)        & 0xff), \
  (unsigned char)(((uint16_t)(v) >>  8) & 0xff)
#define SER16BE(v) \
  (unsigned char)(((uint16_t)(v) >>  8) & 0xff), \
  (unsigned char)( (uint16_t)(v)        & 0xff)
#define SER32(v) \
  (unsigned char)( (uint32_t)(v)        & 0xff), \
  (unsigned char)(((uint32_t)(v) >>  8) & 0xff), \
  (unsigned char)(((uint32_t)(v) >> 16) & 0xff), \
  (unsigned char)( (uint32_t)(v) >> 24        )
#define SER32BE(v) \
  (unsigned char)( (uint32_t)(v) >> 24        ), \
  (unsigned char)(((uint32_t)(v) >> 16) & 0xff), \
  (unsigned char)(((uint32_t)(v) >>  8) & 0xff), \
  (unsigned char)( (uint32_t)(v)        & 0xff)
#define SER64(v) \
  (unsigned char)( (uint64_t)(v)        & 0xff), \
  (unsigned char)(((uint64_t)(v) >>  8) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 16) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 24) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 32) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 40) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 48) & 0xff), \
  (unsigned char)( (uint64_t)(v) >> 56)
#define SER64BE(v) \
  (unsigned char)( (uint64_t)(v) >> 56        ), \
  (unsigned char)(((uint64_t)(v) >> 48) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 40) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 32) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 24) & 0xff), \
  (unsigned char)(((uint64_t)(v) >> 16) & 0xff), \
  (unsigned char)(((uint64_t)(v) >>  8) & 0xff), \
  (unsigned char)( (uint64_t)(v)        & 0xff)
#endif

#define SER_DHEADER(l) SER32(l)
#define SER_EMHEADER(mu,lc,mid) SER32(((mu) ? (1u << 31) : 0) + ((uint32_t) (lc) << 28) + ((uint32_t) (mid) & 0x0fffffff))
#define SER_NEXTINT(l) SER32(l)

#endif /* DDSI_TEST_MEM_SER_H */
