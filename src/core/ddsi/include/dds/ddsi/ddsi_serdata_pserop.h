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
#ifndef DDSI_SERDATA_PSEROP_H
#define DDSI_SERDATA_PSEROP_H

#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertopic.h"
#include "dds/ddsi/ddsi_plist_generic.h"

#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* There is an alignment requirement on the raw data (it must be at
   offset mod 8 for the conversion to/from a dds_stream to work).
   So we define two types: one without any additional padding, and
   one where the appropriate amount of padding is inserted */
#define DDSI_SERDATA_PSEROP_PREPAD    \
  struct ddsi_serdata c;              \
  void *sample;                       \
  bool keyless; /*cached from topic*/ \
  uint32_t pos;                       \
  uint32_t size
#define DDSI_SERDATA_PSEROP_POSTPAD   \
  uint16_t identifier;                \
  uint16_t options;                   \
  char data[]

struct ddsi_serdata_pserop_unpadded {
  DDSI_SERDATA_PSEROP_PREPAD;
  DDSI_SERDATA_PSEROP_POSTPAD;
};

#ifdef __GNUC__
#define DDSI_SERDATA_PSEROP_PAD(n) ((n) % 8)
#else
#define DDSI_SERDATA_PSEROP_PAD(n) (n)
#endif

struct ddsi_serdata_pserop {
  DDSI_SERDATA_PSEROP_PREPAD;
  char pad[DDSI_SERDATA_PSEROP_PAD (8 - (offsetof (struct ddsi_serdata_pserop_unpadded, data) % 8))];
  DDSI_SERDATA_PSEROP_POSTPAD;
};

#undef DDSI_SERDATA_PSEROP_PAD
#undef DDSI_SERDATA_PSEROP_POSTPAD
#undef DDSI_SERDATA_PSEROP_PREPAD

struct ddsi_sertopic_pserop {
  struct ddsi_sertopic c;
  uint16_t native_encoding_identifier; /* CDR_(LE|BE) */
  size_t memsize;
  size_t nops;
  const enum pserop *ops;
  size_t nops_key;
  const enum pserop *ops_key; /* NULL <=> no key; != NULL <=> 16-byte key at offset 0 */
};

extern DDS_EXPORT const struct ddsi_sertopic_ops ddsi_sertopic_ops_pserop;
extern DDS_EXPORT const struct ddsi_serdata_ops ddsi_serdata_ops_pserop;

#if defined (__cplusplus)
}
#endif

#endif
