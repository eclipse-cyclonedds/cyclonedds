// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__HBCONTROL_H
#define DDSI__HBCONTROL_H

#include "dds/features.h"
#include "dds/ddsi/ddsi_hbcontrol.h"

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsi_writer;
struct ddsi_whc_state;
struct ddsi_proxy_reader;

/** @component outgoing_rtps */
void ddsi_writer_hbcontrol_init (struct ddsi_hbcontrol *hbc);

/** @component outgoing_rtps */
int64_t ddsi_writer_hbcontrol_intv (const struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, ddsrt_mtime_t tnow);

/** @component outgoing_rtps */
void ddsi_writer_hbcontrol_note_asyncwrite (struct ddsi_writer *wr, ddsrt_mtime_t tnow);

/** @component outgoing_rtps */
int ddsi_writer_hbcontrol_ack_required (const struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, ddsrt_mtime_t tnow);

/** @component outgoing_rtps */
struct ddsi_xmsg *ddsi_writer_hbcontrol_piggyback (struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, ddsrt_mtime_t tnow, uint32_t packetid, int *hbansreq);

/** @component outgoing_rtps */
int ddsi_writer_hbcontrol_must_send (const struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, ddsrt_mtime_t tnow);

/** @component outgoing_rtps */
struct ddsi_xmsg *ddsi_writer_hbcontrol_create_heartbeat (struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, ddsrt_mtime_t tnow, int hbansreq, int issync);

#ifdef DDS_HAS_SECURITY
/** @component outgoing_rtps */
struct ddsi_xmsg *ddsi_writer_hbcontrol_p2p(struct ddsi_writer *wr, const struct ddsi_whc_state *whcst, int hbansreq, struct ddsi_proxy_reader *prd);
#endif

struct ddsi_heartbeat_xevent_cb_arg {
  ddsi_guid_t wr_guid;
};

void ddsi_heartbeat_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow);

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__HBCONTROL_H */
