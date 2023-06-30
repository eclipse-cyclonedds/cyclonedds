// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

// WARNING This file is only needed for the work around for https://github.com/eclipse-cyclonedds/cyclonedds/issues/74
// Do not include this file in an application! Once issue #74 is solved this header file should be removed.

#ifndef DDS_INTERNAL_API_H
#define DDS_INTERNAL_API_H

#include "dds/export.h"
#include "dds/cdr/dds_cdrstream.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @ingroup internal
 * @component topic
 * @unstable
 * @brief Gets a CDR stream serializer type descriptor from a topic descriptor
 *
 * @param[out] desc Pointer to the target struct that can be filled with the CDR stream topic descriptor
 * @param[in] topic_desc The source topic descriptor
 *
*/
DDS_EXPORT void
dds_cdrstream_desc_from_topic_desc (struct dds_cdrstream_desc *desc, const dds_topic_descriptor_t *topic_desc);

#if defined (__cplusplus)
}
#endif
#endif
