// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI_TRANSMIT_H
#define DDSI_TRANSMIT_H

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_xpack;
struct ddsi_writer;
struct ddsi_serdata;
struct ddsi_tkmap_instance;
struct ddsi_thread_state;

/**
 * @component outgoing_rtps
 *
 * Writing new data; serdata_twrite (serdata) is assumed to be really recentish; serdata is unref'd.
 * If xp == NULL, data is queued, else packed. GC may occur, which means the writer history and watermarks
 * can be anything. This must be used for all application data.
 *
 * @param thrst     Thread state
 * @param xp        xpack
 * @param wr        writer
 * @param serdata   serialized sample data
 * @param tk        key-instance map instance
 * @return int
 */
int ddsi_write_sample_gc (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_writer *wr, struct ddsi_serdata *serdata, struct ddsi_tkmap_instance *tk);

/**
 * @component outgoing_rtps
 *
 * @remark wr->lock must be held
 *
 * @param gv        domain globals
 * @param wr_guid   writer guid
 * @param xp        xpack
 * @return dds_return_t
 */
dds_return_t ddsi_write_hb_liveliness (struct ddsi_domaingv * const gv, struct ddsi_guid *wr_guid, struct ddsi_xpack *xp);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI_TRANSMIT_H */
