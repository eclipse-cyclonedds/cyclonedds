// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef _TEST_COMMON_H_
#define _TEST_COMMON_H_

#include <stdint.h>
#include <stddef.h>

#include "CUnit/Test.h"
#include "CUnit/Theory.h"

#include "dds/ddsrt/heap.h"
#include "dds/cdr/dds_cdrstream.h"
#include "test_util.h"

#include "Space.h"
#include "RoundTrip.h"

void xcdr2_ser (const void *obj, const dds_topic_descriptor_t *desc, dds_ostream_t *os);
void xcdr2_deser (const unsigned char *buf, uint32_t sz, void **obj, const dds_topic_descriptor_t *desc);

#endif /* _TEST_COMMON_H_ */
