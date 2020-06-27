/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef _DDS_STATISTICS_IMPL_H_
#define _DDS_STATISTICS_IMPL_H_

#include "dds/ddsc/dds_statistics.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct dds_stat_keyvalue_descriptor {
  const char *name;
  enum dds_stat_kind kind;
};

struct dds_stat_descriptor {
  size_t count;
  const struct dds_stat_keyvalue_descriptor *kv;
};

struct dds_statistics *dds_alloc_statistics (const struct dds_entity *e, const struct dds_stat_descriptor *d);

#if defined (__cplusplus)
}
#endif
#endif /* _DDS_STATISTICS_IMPL_H_ */
