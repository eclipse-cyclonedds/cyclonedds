// Copyright(c) 2020 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef _TEST_UTIL_H_
#define _TEST_UTIL_H_

#include <stdint.h>
#include <stddef.h>
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/dds.h"

/* Get unique g_topic name on each invocation. */
char *create_unique_topic_name (const char *prefix, char *name, size_t size);

/* Sync the reader to the writer and writer to reader */
void sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer);

/* Try to sync the reader to the writer and writer to reader, expect to fail */
void no_sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer, dds_duration_t timeout);

/* Print message preceded by time stamp */
void tprintf (const char *msg, ...)
  ddsrt_attribute_format_printf (1, 2);

/* Get gv from the provided entity */
struct ddsi_domaingv *get_domaingv (dds_entity_t handle);

/* Generate a guid */
void gen_test_guid (struct ddsi_domaingv *gv, ddsi_guid_t *guid, uint32_t entity_id);

#endif /* _TEST_UTIL_H_ */
