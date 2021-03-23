/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef _TEST_COMMON_H_
#define _TEST_COMMON_H_

#include <stdint.h>
#include <stddef.h>

#include "CUnit/Test.h"
#include "CUnit/Theory.h"

#include "test_common.h"

#include "Space.h"
#include "RoundTrip.h"

/* Get unique g_topic name on each invocation. */
char *create_unique_topic_name (const char *prefix, char *name, size_t size);

/* Sync the reader to the writer and writer to reader */
void sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer);

/* Wait for all (proxy and local) writers for the reader are in fastpath mode (fastpath param = true),
   or reset to non-fastpath (fastpath param = false). Assert nwr writers (local + proxy) */
void waitfor_or_reset_fastpath (dds_entity_t rdhandle, bool fastpath, size_t nwr);

#endif /* _TEST_COMMON_H_ */
