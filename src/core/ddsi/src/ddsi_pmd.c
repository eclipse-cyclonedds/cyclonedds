/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>
#include <string.h>
#include "dds/ddsi/ddsi_pmd.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_xmsg.h"

#include "dds/ddsi/sysdeps.h"

/* note: treating guid prefix + kind as if it were a GUID because that matches
   the octet-sequence/sequence-of-uint32 distinction between the specified wire
   representation and the internal representation */
const enum pserop participant_message_data_ops[] = { XG, XO, XSTOP };
size_t participant_message_data_nops = sizeof (participant_message_data_ops) / sizeof (participant_message_data_ops[0]);
const enum pserop participant_message_data_ops_key[] = { XG, XSTOP };
size_t participant_message_data_nops_key = sizeof (participant_message_data_ops_key) / sizeof (participant_message_data_ops_key[0]);

void write_pmd_message_guid (struct ddsi_domaingv * const gv, struct ddsi_guid *pp_guid, unsigned pmd_kind)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct lease *lease;
  thread_state_awake (ts1, gv);
  struct participant *pp = entidx_lookup_participant_guid (gv->entity_index, pp_guid);
  if (pp == NULL)
    GVTRACE ("write_pmd_message("PGUIDFMT") - builtin pmd writer not found\n", PGUID (*pp_guid));
  else
  {
    if ((lease = ddsrt_atomic_ldvoidp (&pp->minl_man)) != NULL)
      lease_renew (lease, ddsrt_time_elapsed());
    write_pmd_message (ts1, NULL, pp, pmd_kind);
  }
  thread_state_asleep (ts1);
}

void write_pmd_message (struct thread_state1 * const ts1, struct nn_xpack *xp, struct participant *pp, unsigned pmd_kind)
{
#define PMD_DATA_LENGTH 1
  struct ddsi_domaingv * const gv = pp->e.gv;
  struct writer *wr;
  unsigned char data[PMD_DATA_LENGTH] = { 0 };
  ParticipantMessageData_t pmd;
  struct ddsi_serdata *serdata;
  struct ddsi_tkmap_instance *tk;

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER)) == NULL)
  {
    GVTRACE ("write_pmd_message("PGUIDFMT") - builtin pmd writer not found\n", PGUID (pp->e.guid));
    return;
  }

  pmd.participantGuidPrefix = pp->e.guid.prefix;
  pmd.kind = pmd_kind;
  pmd.value.length = (uint32_t) sizeof (data);
  pmd.value.value = data;
  serdata = ddsi_serdata_from_sample (gv->pmd_type, SDK_DATA, &pmd);
  serdata->timestamp = ddsrt_time_wallclock ();

  tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_nogc (ts1, xp, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
#undef PMD_DATA_LENGTH
}

void handle_pmd_message (const struct receiver_state *rst, struct ddsi_serdata *sample_common)
{
  /* use sample with knowledge of internal representation: there's a deserialized sample inside already */
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  struct proxy_participant *proxypp;
  ddsi_guid_t ppguid;
  struct lease *l;
  RSTTRACE (" PMD ST%"PRIx32, sample->c.statusinfo);
  switch (sample->c.statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
  {
    case 0: {
      const ParticipantMessageData_t *pmd = sample->sample;
      RSTTRACE (" pp %"PRIx32":%"PRIx32":%"PRIx32" kind %"PRIu32" data %"PRIu32, PGUIDPREFIX (pmd->participantGuidPrefix), pmd->kind, pmd->value.length);
      ppguid.prefix = pmd->participantGuidPrefix;
      ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
      if ((proxypp = entidx_lookup_proxy_participant_guid (rst->gv->entity_index, &ppguid)) == NULL)
        RSTTRACE (" PPunknown");
      else if (pmd->kind == PARTICIPANT_MESSAGE_DATA_KIND_MANUAL_LIVELINESS_UPDATE &&
               (l = ddsrt_atomic_ldvoidp (&proxypp->minl_man)) != NULL)
      {
        /* Renew lease for entity with shortest manual-by-participant lease */
        lease_renew (l, ddsrt_time_elapsed ());
      }
      break;
    }

    case NN_STATUSINFO_DISPOSE:
    case NN_STATUSINFO_UNREGISTER:
    case NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER: {
      const ParticipantMessageData_t *pmd = sample->sample;
      ppguid.prefix = pmd->participantGuidPrefix;
      ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
      if (delete_proxy_participant_by_guid (rst->gv, &ppguid, sample->c.timestamp, 0) < 0)
        RSTTRACE (" unknown");
      else
        RSTTRACE (" delete");
      break;
    }
  }
  RSTTRACE ("\n");
}
