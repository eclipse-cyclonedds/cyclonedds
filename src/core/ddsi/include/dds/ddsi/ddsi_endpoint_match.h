// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_ENDPOINT_MATCH_H
#define DDSI_ENDPOINT_MATCH_H

#include "dds/export.h"
#include "dds/features.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsi/ddsi_lat_estim.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_writer;
struct ddsi_reader;
struct ddsi_entity_common;
struct dds_qos;

/** @component endpoint_matching */
dds_return_t ddsi_writer_get_matched_subscriptions (struct ddsi_writer *wr, uint64_t *rds, size_t nrds);

/** @component endpoint_matching */
dds_return_t ddsi_reader_get_matched_publications (struct ddsi_reader *rd, uint64_t *wrs, size_t nwrs);

/** @brief Lookup the instance handle of a matching reader and return it, it's qos and participant
 * @component endpoint_matching
 *
 * @note Thread state must be "awake", returned pointers are aliases; lifetime ends on leaving the "awake" state; contents may not be modified.
 *
 * @param[in] wr writer for which to lookup ih in the setting of matching readers
 * @param[in] ih instance handle of a reader
 * @param[out] rdc pointer to the common entity data of the reader
 * @param[out] rdqos pointer to the QoS of the reader
 * @param[out] ppc participant, may be null for an orphan reader
 * @return true iff ih corresponds to a matched reader, output parameters are undefined if not
 */
bool ddsi_writer_find_matched_reader (struct ddsi_writer *wr, uint64_t ih, const struct ddsi_entity_common **rdc, const struct dds_qos **rdqos, const struct ddsi_entity_common **ppc);

/** @brief Lookup the instance handle of a matching writer and return it, it's qos and participant
 * @component endpoint_matching
 *
 * @note Thread state must be "awake", returned pointers are aliases; lifetime ends on leaving the "awake" state; contents may not be modified.
 *
 * @param[in] rd reader for which to lookup ih in the setting of matching writers
 * @param[in] ih instance handle of a writer
 * @param[out] wrc pointer to the common entity data of the writer
 * @param[out] wrqos pointer to the QoS of the writer
 * @param[out] ppc participant, may be null for an orphan writer
 * @return true iff ih corresponds to a matched writer, output parameters are undefined if not
 */
bool ddsi_reader_find_matched_writer (struct ddsi_reader *rd, uint64_t ih, const struct ddsi_entity_common **wrc, const struct dds_qos **wrqos, const struct ddsi_entity_common **ppc);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_ENDPOINT_MATCH_H */
