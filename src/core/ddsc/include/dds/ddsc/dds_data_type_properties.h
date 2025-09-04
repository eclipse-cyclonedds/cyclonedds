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
 *
 * Bits 0 .. 16 are kinda reserved for presence of a variety of types, but its really not worth the bother.
 * The old names are left in for backwards source compatibility with other sertype implementations.
 * Eventually they can be reused, if necessary.
 */
#define DDS_DATA_TYPE_CONTAINS_UNION              (0ull)
#define DDS_DATA_TYPE_CONTAINS_BITMASK            (0ull)
#define DDS_DATA_TYPE_CONTAINS_ENUM               (0ull)
#define DDS_DATA_TYPE_CONTAINS_STRUCT             (0ull)
#define DDS_DATA_TYPE_CONTAINS_STRING             (0ull)
#define DDS_DATA_TYPE_CONTAINS_BSTRING            (0ull)
#define DDS_DATA_TYPE_CONTAINS_WSTRING            (0ull)
#define DDS_DATA_TYPE_CONTAINS_SEQUENCE           (0ull)
#define DDS_DATA_TYPE_CONTAINS_BSEQUENCE          (0ull)
#define DDS_DATA_TYPE_CONTAINS_ARRAY              (0ull)
#define DDS_DATA_TYPE_CONTAINS_OPTIONAL           (0x1ull << 10)
#define DDS_DATA_TYPE_CONTAINS_EXTERNAL           (0ull)
#define DDS_DATA_TYPE_CONTAINS_BWSTRING           (0ull)
#define DDS_DATA_TYPE_CONTAINS_WCHAR              (0ull)
#define DDS_DATA_TYPE_CONTAINS_APPENDABLE         (0x1ull << 15)
#define DDS_DATA_TYPE_CONTAINS_MUTABLE            (0x1ull << 16)

// Bits 10, 15 and 16 used to be optional, appendable and mutable, respectively: and if some of these
// were set, the default was XCDR2. Now we set only bit 10, but check for any of the 3 bits.
#define DDS_DATA_TYPE_DEFAULTS_TO_XCDR2           (DDS_DATA_TYPE_CONTAINS_OPTIONAL)
#define DDS_DATA_TYPE_DEFAULTS_TO_XCDR2_MASK      (DDS_DATA_TYPE_CONTAINS_OPTIONAL | DDS_DATA_TYPE_CONTAINS_APPENDABLE | DDS_DATA_TYPE_CONTAINS_MUTABLE)
#define DDS_DATA_TYPE_CONTAINS_KEY                (0x1ull << 12)
#define DDS_DATA_TYPE_IS_MEMCPY_SAFE              (0x1ull << 63)


typedef uint64_t dds_data_type_properties_t;

#if defined (__cplusplus)
}
#endif

#endif
