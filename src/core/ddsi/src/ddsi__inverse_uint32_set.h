// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__INVERSE_UINT32_SET_H
#define DDSI__INVERSE_UINT32_SET_H

#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_inverse_uint32_set.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_inverse_uint32_set_node {
  ddsrt_avl_node_t avlnode;
  uint32_t min, max;
};

/** @component inverset_set */
void ddsi_inverse_uint32_set_init(struct ddsi_inverse_uint32_set *set, uint32_t min, uint32_t max);

/** @component inverset_set */
void ddsi_inverse_uint32_set_fini(struct ddsi_inverse_uint32_set *set);

/** @component inverset_set */
int ddsi_inverse_uint32_set_alloc(uint32_t * const id, struct ddsi_inverse_uint32_set *set);

/** @component inverset_set */
void ddsi_inverse_uint32_set_free(struct ddsi_inverse_uint32_set *set, uint32_t id);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__INVERSE_UINT32_SET_H */
