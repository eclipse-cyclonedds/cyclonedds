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
#ifndef _DDS_TOPIC_H_
#define _DDS_TOPIC_H_

#include "dds__types.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define dds_topic_lock(hdl, obj) dds_entity_lock(hdl, DDS_KIND_TOPIC, (dds_entity**)obj)
#define dds_topic_unlock(obj)    dds_entity_unlock((dds_entity*)obj);

extern struct sertopic * dds_topic_lookup (dds_domain * domain, const char * name);
extern void dds_topic_free (dds_domainid_t domainid, struct sertopic * st);

#ifndef DDS_TOPIC_INTERN_FILTER_FN_DEFINED
#define DDS_TOPIC_INTERN_FILTER_FN_DEFINED
typedef bool (*dds_topic_intern_filter_fn) (const void * sample, void *ctx);
#endif

DDS_EXPORT void dds_topic_set_filter_with_ctx
  (dds_entity_t topic, dds_topic_intern_filter_fn filter, void *ctx);

DDS_EXPORT dds_topic_intern_filter_fn dds_topic_get_filter_with_ctx
  (dds_entity_t topic);

#if defined (__cplusplus)
}
#endif
#endif
