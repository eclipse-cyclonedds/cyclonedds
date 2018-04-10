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
#ifndef _DDS_KEY_H_
#define _DDS_KEY_H_

#include "dds__types.h"

struct dds_key_hash;

#if defined (__cplusplus)
extern "C" {
#endif

void dds_key_md5 (struct dds_key_hash * kh);

void dds_key_gen
(
  const dds_topic_descriptor_t * const desc,
  struct dds_key_hash * kh,
  const char * sample
);

#if defined (__cplusplus)
}
#endif
#endif
