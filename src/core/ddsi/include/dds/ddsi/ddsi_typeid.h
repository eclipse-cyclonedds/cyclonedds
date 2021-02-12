/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DDSI_TYPEID_H
#define DDSI_TYPEID_H

#include "dds/features.h"
#ifdef DDS_HAS_TYPE_DISCOVERY

#include <stdint.h>
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_guid.h"

#define PFMT4B "%02x%02x%02x%02x"
#define PTYPEIDFMT PFMT4B "-" PFMT4B "-" PFMT4B "-" PFMT4B

#define PTYPEID4B(x, n) ((x).hash[n]), ((x).hash[n + 1]), ((x).hash[n + 2]), ((x).hash[n + 3])
#define PTYPEID(x) PTYPEID4B(x, 0), PTYPEID4B(x, 4), PTYPEID4B(x, 8), PTYPEID4B(x, 12)

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_sertype;

typedef struct type_identifier {
  unsigned char hash[16];
} type_identifier_t;

typedef struct type_identifier_seq {
  uint32_t n;
  type_identifier_t *type_ids;
} type_identifier_seq_t;

typedef struct type_object {
  uint32_t length;
  unsigned char *value;
} type_object_t;

typedef struct type_identifier_type_object_pair {
  type_identifier_t type_identifier;
  type_object_t type_object;
} type_identifier_type_object_pair_t;

typedef struct type_identifier_type_object_pair_seq {
  uint32_t n;
  type_identifier_type_object_pair_t *types;
} type_identifier_type_object_pair_seq_t;


DDS_EXPORT type_identifier_t * ddsi_typeid_from_sertype (const struct ddsi_sertype * type);
DDS_EXPORT type_identifier_t * ddsi_typeid_dup (const type_identifier_t *type_id);
DDS_EXPORT bool ddsi_typeid_equal (const type_identifier_t *a, const type_identifier_t *b);
DDS_EXPORT bool ddsi_typeid_none (const type_identifier_t *type_id);

#if defined (__cplusplus)
}
#endif
#endif /* DDS_HAS_TYPE_DISCOVERY */
#endif /* DDSI_TYPEID_H */
