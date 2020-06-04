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
#ifndef DDSI_SERDATA_PLIST_H
#define DDSI_SERDATA_PLIST_H

#include "dds/ddsi/q_protocol.h" /* for nn_parameterid_t */
#include "dds/ddsi/ddsi_keyhash.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertopic.h"

#include "dds/dds.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* There is an alignment requirement on the raw data (it must be at
   offset mod 8 for the conversion to/from a dds_stream to work).
   So we define two types: one without any additional padding, and
   one where the appropriate amount of padding is inserted */
#define DDSI_SERDATA_PLIST_PREPAD     \
  struct ddsi_serdata c;              \
  uint32_t pos;                       \
  uint32_t size;                      \
  nn_vendorid_t vendorid;             \
  nn_protocol_version_t protoversion; \
  ddsi_keyhash_t keyhash
#define DDSI_SERDATA_PLIST_POSTPAD    \
  uint16_t identifier;                \
  uint16_t options;                   \
  char data[]

struct ddsi_serdata_plist_unpadded {
  DDSI_SERDATA_PLIST_PREPAD;
  DDSI_SERDATA_PLIST_POSTPAD;
};

#ifdef __GNUC__
#define DDSI_SERDATA_PLIST_PAD(n) ((n) % 8)
#else
#define DDSI_SERDATA_PLIST_PAD(n) (n)
#endif

struct ddsi_serdata_plist {
  DDSI_SERDATA_PLIST_PREPAD;
  char pad[DDSI_SERDATA_PLIST_PAD (8 - (offsetof (struct ddsi_serdata_plist_unpadded, data) % 8))];
  DDSI_SERDATA_PLIST_POSTPAD;
};

#undef DDSI_SERDATA_PLIST_PAD
#undef DDSI_SERDATA_PLIST_POSTPAD
#undef DDSI_SERDATA_PLIST_PREPAD

struct ddsi_sertopic_plist {
  struct ddsi_sertopic c;
  uint16_t native_encoding_identifier; /* PL_CDR_(LE|BE) */
  nn_parameterid_t keyparam;
};

extern DDS_EXPORT const struct ddsi_sertopic_ops ddsi_sertopic_ops_plist;
extern DDS_EXPORT const struct ddsi_serdata_ops ddsi_serdata_ops_plist;

#if defined (__cplusplus)
}
#endif

#endif
