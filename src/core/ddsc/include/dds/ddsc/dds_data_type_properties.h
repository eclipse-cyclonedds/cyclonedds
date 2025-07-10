// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS_DATA_TYPE_PROPERTIES_H
#define DDS_DATA_TYPE_PROPERTIES_H

#include <stdint.h>

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Flags that are used to indicate the types used in a data type
 */
#define DDS_DATA_TYPE_CONTAINS_UNION              (0x1ull << 0)
#define DDS_DATA_TYPE_CONTAINS_BITMASK            (0x1ull << 1)
#define DDS_DATA_TYPE_CONTAINS_ENUM               (0x1ull << 2)
#define DDS_DATA_TYPE_CONTAINS_STRUCT             (0x1ull << 3)
#define DDS_DATA_TYPE_CONTAINS_STRING             (0x1ull << 4)
#define DDS_DATA_TYPE_CONTAINS_BSTRING            (0x1ull << 5)
#define DDS_DATA_TYPE_CONTAINS_WSTRING            (0x1ull << 6)
#define DDS_DATA_TYPE_CONTAINS_SEQUENCE           (0x1ull << 7)
#define DDS_DATA_TYPE_CONTAINS_BSEQUENCE          (0x1ull << 8)
#define DDS_DATA_TYPE_CONTAINS_ARRAY              (0x1ull << 9)
#define DDS_DATA_TYPE_CONTAINS_OPTIONAL           (0x1ull << 10)
#define DDS_DATA_TYPE_CONTAINS_EXTERNAL           (0x1ull << 11)
#define DDS_DATA_TYPE_CONTAINS_KEY                (0x1ull << 12)
#define DDS_DATA_TYPE_CONTAINS_BWSTRING           (0x1ull << 13)
#define DDS_DATA_TYPE_CONTAINS_WCHAR              (0x1ull << 14)
#define DDS_DATA_TYPE_CONTAINS_APPENDABLE         (0x1ull << 15)
#define DDS_DATA_TYPE_CONTAINS_MUTABLE            (0x1ull << 16)

#define DDS_DATA_TYPE_IS_MEMCPY_SAFE              (0x1ull << 63)

typedef uint64_t dds_data_type_properties_t;

#if defined (__cplusplus)
}
#endif

#endif
