// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_INVERSE_UINT32_SET_H
#define DDSI_INVERSE_UINT32_SET_H

#include "dds/ddsrt/avl.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_inverse_uint32_set {
  ddsrt_avl_tree_t ids;
  uint32_t cursor;
  uint32_t min, max;
};

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_INVERSE_UINT32_SET_H */
