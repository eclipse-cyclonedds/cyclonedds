// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <string.h>
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_log.h"
#include "ddsi__pmd.h"
#include "ddsi__serdata_pserop.h"
#include "ddsi__entity.h"
#include "ddsi__entity_index.h"
#include "ddsi__participant.h"
#include "ddsi__lease.h"
#include "ddsi__misc.h"
#include "ddsi__protocol.h"
#include "ddsi__radmin.h"
#include "ddsi__transmit.h"
#include "ddsi__xmsg.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__xevent.h"

/* note: treating guid prefix + kind as if it were a GUID because that matches
   the octet-sequence/sequence-of-uint32 distinction between the specified wire
   representation and the internal representation */
const enum ddsi_pserop ddsi_participant_message_data_ops[] = { XG, XO, XSTOP };
size_t ddsi_participant_message_data_nops = sizeof (ddsi_participant_message_data_ops) / sizeof (ddsi_participant_message_data_ops[0]);
const enum ddsi_pserop ddsi_participant_message_data_ops_key[] = { XG, XSTOP };
size_t ddsi_participant_message_data_nops_key = sizeof (ddsi_participant_message_data_ops_key) / sizeof (ddsi_participant_message_data_ops_key[0]);

void ddsi_write_pmd_message_guid (struct ddsi_domaingv * const gv, struct ddsi_guid *pp_guid, unsigned pmd_kind)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  struct ddsi_lease *lease;
  ddsi_thread_state_awake (thrst, gv);
  struct ddsi_participant *pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, pp_guid);
  if (pp == NULL)
    GVTRACE ("ddsi_write_pmd_message ("PGUIDFMT") - builtin pmd writer not found\n", PGUID (*pp_guid));
  else
  {
    if ((lease = ddsrt_atomic_ldvoidp (&pp->minl_man)) != NULL)
      ddsi_lease_renew (lease, ddsrt_time_elapsed());
    ddsi_write_pmd_message (thrst, NULL, pp, pmd_kind);
  }
  ddsi_thread_state_asleep (thrst);
}

void ddsi_write_pmd_message (struct ddsi_thread_state * const thrst, struct ddsi_xpack *xp, struct ddsi_participant *pp, unsigned pmd_kind)
{
#define PMD_DATA_LENGTH 1
  struct ddsi_domaingv * const gv = pp->e.gv;
  struct ddsi_writer *wr;
  unsigned char data[PMD_DATA_LENGTH] = { 0 };
  ddsi_participant_message_data_t pmd;
  struct ddsi_serdata *serdata;
  struct ddsi_tkmap_instance *tk;

  if ((wr = ddsi_get_builtin_writer (pp, DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER)) == NULL)
  {
    GVTRACE ("ddsi_write_pmd_message ("PGUIDFMT") - builtin pmd writer not found\n", PGUID (pp->e.guid));
    return;
  }

  pmd.participantGuidPrefix = pp->e.guid.prefix;
  pmd.kind = pmd_kind;
  pmd.value.length = (uint32_t) sizeof (data);
  pmd.value.value = data;
  serdata = ddsi_serdata_from_sample (gv->pmd_type, SDK_DATA, &pmd);
  serdata->timestamp = ddsrt_time_wallclock ();

  tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  ddsi_write_sample_nogc (thrst, xp, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
#undef PMD_DATA_LENGTH
}

void ddsi_handle_pmd_message (const struct ddsi_receiver_state *rst, struct ddsi_serdata *sample_common)
{
  /* use sample with knowledge of internal representation: there's a deserialized sample inside already */
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  struct ddsi_proxy_participant *proxypp;
  ddsi_guid_t ppguid;
  struct ddsi_lease *l;
  RSTTRACE (" PMD ST%"PRIx32, sample->c.statusinfo);
  switch (sample->c.statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER))
  {
    case 0: {
      const ddsi_participant_message_data_t *pmd = sample->sample;
      RSTTRACE (" pp %"PRIx32":%"PRIx32":%"PRIx32" kind %"PRIu32" data %"PRIu32, PGUIDPREFIX (pmd->participantGuidPrefix), pmd->kind, pmd->value.length);
      ppguid.prefix = pmd->participantGuidPrefix;
      ppguid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
      if ((proxypp = ddsi_entidx_lookup_proxy_participant_guid (rst->gv->entity_index, &ppguid)) == NULL)
        RSTTRACE (" PPunknown");
      else if (pmd->kind == DDSI_PARTICIPANT_MESSAGE_DATA_KIND_MANUAL_LIVELINESS_UPDATE &&
               (l = ddsrt_atomic_ldvoidp (&proxypp->minl_man)) != NULL)
      {
        /* Renew lease for entity with shortest manual-by-participant lease */
        ddsi_lease_renew (l, ddsrt_time_elapsed ());
      }
      break;
    }

    case DDSI_STATUSINFO_DISPOSE:
    case DDSI_STATUSINFO_UNREGISTER:
    case DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER: {
      const ddsi_participant_message_data_t *pmd = sample->sample;
      ppguid.prefix = pmd->participantGuidPrefix;
      ppguid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
      if (ddsi_delete_proxy_participant_by_guid (rst->gv, &ppguid, sample->c.timestamp, 0) < 0)
        RSTTRACE (" unknown");
      else
        RSTTRACE (" delete");
      break;
    }
  }
  RSTTRACE ("\n");
}

void ddsi_write_pmd_message_xevent_cb (struct ddsi_domaingv *gv, struct ddsi_xevent *ev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  struct ddsi_write_pmd_message_xevent_cb_arg const * const arg = varg;
  struct ddsi_participant *pp;
  dds_duration_t intv;
  ddsrt_mtime_t tnext;

  if ((pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, &arg->pp_guid)) == NULL)
  {
    return;
  }

  ddsi_write_pmd_message (thrst, xp, pp, DDSI_PARTICIPANT_MESSAGE_DATA_KIND_AUTOMATIC_LIVELINESS_UPDATE);

  intv = ddsi_participant_get_pmd_interval (pp);
  if (intv < 0 || intv == DDS_INFINITY)
  {
    tnext.v = DDS_NEVER;
    GVTRACE ("resched pmd("PGUIDFMT"): never\n", PGUID (pp->e.guid));
  }
  else
  {
    /* schedule next when 80% of the interval has elapsed, or 2s
       before the lease ends, whichever comes first */
    if (intv >= DDS_SECS (10))
      tnext.v = tnow.v + intv - DDS_SECS (2);
    else
      tnext.v = tnow.v + 4 * intv / 5;
    GVTRACE ("resched pmd("PGUIDFMT"): %gs\n", PGUID (pp->e.guid), (double)(tnext.v - tnow.v) / 1e9);
  }

  (void) ddsi_resched_xevent_if_earlier (ev, tnext);
}

