// Copyright(c) 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__STATISTICS_H
#define DDS__STATISTICS_H

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

/** @component statistics */
struct dds_statistics *dds_alloc_statistics (const struct dds_entity *e, const struct dds_stat_descriptor *d);

#if defined (__cplusplus)
}
#endif
#endif /* DDS__STATISTICS_H */
