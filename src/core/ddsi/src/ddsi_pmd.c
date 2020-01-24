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
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_xmsg.h"

#include "dds/ddsi/sysdeps.h"

static void debug_print_rawdata (const struct q_globals *gv, const char *msg, const void *data, size_t len)
{
  const unsigned char *c = data;
  size_t i;
  GVTRACE ("%s<", msg);
  for (i = 0; i < len; i++)
  {
    if (32 < c[i] && c[i] <= 127)
      GVTRACE ("%s%c", (i > 0 && (i%4) == 0) ? " " : "", c[i]);
    else
      GVTRACE ("%s\\x%02x", (i > 0 && (i%4) == 0) ? " " : "", c[i]);
  }
  GVTRACE (">");
}

void write_pmd_message_guid (struct q_globals * const gv, struct ddsi_guid *pp_guid, unsigned pmd_kind)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  thread_state_awake (ts1, gv);
  struct participant *pp = entidx_lookup_participant_guid (gv->entity_index, pp_guid);
  if (pp == NULL)
    GVTRACE ("write_pmd_message("PGUIDFMT") - builtin pmd writer not found\n", PGUID (*pp_guid));
  else
    write_pmd_message (ts1, NULL, pp, pmd_kind);
  thread_state_asleep (ts1);
}

void write_pmd_message (struct thread_state1 * const ts1, struct nn_xpack *xp, struct participant *pp, unsigned pmd_kind)
{
#define PMD_DATA_LENGTH 1
  struct q_globals * const gv = pp->e.gv;
  struct writer *wr;
  union {
    ParticipantMessageData_t pmd;
    char pad[offsetof (ParticipantMessageData_t, value) + PMD_DATA_LENGTH];
  } u;
  struct ddsi_serdata *serdata;
  struct ddsi_tkmap_instance *tk;

  if ((wr = get_builtin_writer (pp, NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER)) == NULL)
  {
    GVTRACE ("write_pmd_message("PGUIDFMT") - builtin pmd writer not found\n", PGUID (pp->e.guid));
    return;
  }

  u.pmd.participantGuidPrefix = nn_hton_guid_prefix (pp->e.guid.prefix);
  u.pmd.kind = ddsrt_toBE4u (pmd_kind);
  u.pmd.length = PMD_DATA_LENGTH;
  memset (u.pmd.value, 0, u.pmd.length);

  struct ddsi_rawcdr_sample raw = {
    .blob = &u,
    .size = offsetof (ParticipantMessageData_t, value) + PMD_DATA_LENGTH,
    .key = &u.pmd,
    .keysize = 16
  };
  serdata = ddsi_serdata_from_sample (gv->rawcdr_topic, SDK_DATA, &raw);
  serdata->timestamp = now ();

  tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_nogc (ts1, xp, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
#undef PMD_DATA_LENGTH
}

void handle_pmd_message (const struct receiver_state *rst, nn_wctime_t timestamp, uint32_t statusinfo, const void *vdata, uint32_t len)
{
  const struct CDRHeader *data = vdata; /* built-ins not deserialized (yet) */
  const int bswap = (data->identifier == CDR_LE) ^ (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
  struct proxy_participant *proxypp;
  ddsi_guid_t ppguid;
  struct lease *l;
  RSTTRACE (" PMD ST%x", statusinfo);
  if (data->identifier != CDR_LE && data->identifier != CDR_BE)
  {
    RSTTRACE (" PMD data->identifier %u !?\n", ntohs (data->identifier));
    return;
  }

  switch (statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER))
  {
    case 0:
      if (offsetof (ParticipantMessageData_t, value) > len - sizeof (struct CDRHeader))
        debug_print_rawdata (rst->gv, " SHORT1", data, len);
      else
      {
        const ParticipantMessageData_t *pmd = (ParticipantMessageData_t *) (data + 1);
        ddsi_guid_prefix_t p = nn_ntoh_guid_prefix (pmd->participantGuidPrefix);
        uint32_t kind = ntohl (pmd->kind);
        uint32_t length = bswap ? ddsrt_bswap4u (pmd->length) : pmd->length;
        RSTTRACE (" pp %"PRIx32":%"PRIx32":%"PRIx32" kind %u data %u", p.u[0], p.u[1], p.u[2], kind, length);
        if (len - sizeof (struct CDRHeader) - offsetof (ParticipantMessageData_t, value) < length)
          debug_print_rawdata (rst->gv, " SHORT2", pmd->value, len - sizeof (struct CDRHeader) - offsetof (ParticipantMessageData_t, value));
        else
          debug_print_rawdata (rst->gv, "", pmd->value, length);
        ppguid.prefix = p;
        ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
        if ((proxypp = entidx_lookup_proxy_participant_guid (rst->gv->entity_index, &ppguid)) == NULL)
          RSTTRACE (" PPunknown");
        else if (kind == PARTICIPANT_MESSAGE_DATA_KIND_MANUAL_LIVELINESS_UPDATE &&
                 (l = ddsrt_atomic_ldvoidp (&proxypp->minl_man)) != NULL)
        {
          /* Renew lease for entity with shortest manual-by-participant lease */
          lease_renew (l, now_et ());
        }
      }
      break;

    case NN_STATUSINFO_DISPOSE:
    case NN_STATUSINFO_UNREGISTER:
    case NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER:
      /* Serialized key; BE or LE doesn't matter as both fields are
         defined as octets.  */
      if (len < sizeof (struct CDRHeader) + sizeof (ddsi_guid_prefix_t))
        debug_print_rawdata (rst->gv, " SHORT3", data, len);
      else
      {
        ppguid.prefix = nn_ntoh_guid_prefix (*((ddsi_guid_prefix_t *) (data + 1)));
        ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;
        if (delete_proxy_participant_by_guid (rst->gv, &ppguid, timestamp, 0) < 0)
          RSTTRACE (" unknown");
        else
          RSTTRACE (" delete");
      }
      break;
  }
  RSTTRACE ("\n");
}
