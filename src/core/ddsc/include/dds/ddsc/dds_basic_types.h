/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 * Copyright(c) 2021 Apex.AI, Inc
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef _DDS_BASIC_TYPES_H_
#define _DDS_BASIC_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * Handle to an entity. A valid entity handle will always have a positive
 * integer value. Should the value be negative, it is one of the DDS_RETCODE_*
 * error codes.
 */
typedef int32_t dds_entity_t;

#define DDS_MIN_PSEUDO_HANDLE ((dds_entity_t)0x7fff0000)

/** Special handle representing the entity corresponding to the CycloneDDS
 * library itself */
#define DDS_CYCLONEDDS_HANDLE ((dds_entity_t)(DDS_MIN_PSEUDO_HANDLE + 256))

#endif /*_DDS_PUBLIC_TYPES_H_*/
