/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <string.h>
#include "dds/ddsrt/sync.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_statistics.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_radmin.h"

void ddsi_get_writer_stats (struct writer *wr, uint64_t * __restrict rexmit_bytes, uint32_t * __restrict throttle_count, uint64_t * __restrict time_throttled, uint64_t * __restrict time_retransmit)
{
  ddsrt_mutex_lock (&wr->e.lock);
  *rexmit_bytes = wr->rexmit_bytes;
  *throttle_count = wr->throttle_count;
  *time_throttled = wr->time_throttled;
  *time_retransmit = wr->time_retransmit;
  ddsrt_mutex_unlock (&wr->e.lock);
}

void ddsi_get_reader_stats (struct reader *rd, uint64_t * __restrict discarded_bytes)
{
  struct rd_pwr_match *m;
  ddsi_guid_t pwrguid;
  memset (&pwrguid, 0, sizeof (pwrguid));
  assert (thread_is_awake ());

  *discarded_bytes = 0;

  // collect for all matched proxy writers
  ddsrt_mutex_lock (&rd->e.lock);
  while ((m = ddsrt_avl_lookup_succ (&rd_writers_treedef, &rd->writers, &pwrguid)) != NULL)
  {
    struct proxy_writer *pwr;
    pwrguid = m->pwr_guid;
    ddsrt_mutex_unlock (&rd->e.lock);
    if ((pwr = entidx_lookup_proxy_writer_guid (rd->e.gv->entity_index, &pwrguid)) != NULL)
    {
      uint64_t disc_frags, disc_samples;
      ddsrt_mutex_lock (&pwr->e.lock);
      struct pwr_rd_match *x = ddsrt_avl_lookup (&pwr_readers_treedef, &pwr->readers, &rd->e.guid);
      if (x != NULL)
      {
        nn_defrag_stats (pwr->defrag, &disc_frags);
        if (x->in_sync != PRMSS_OUT_OF_SYNC && !x->filtered)
          nn_reorder_stats (pwr->reorder, &disc_samples);
        else
          nn_reorder_stats (x->u.not_in_sync.reorder, &disc_samples);
        *discarded_bytes += disc_frags + disc_samples;
      }
      ddsrt_mutex_unlock (&pwr->e.lock);
    }
    ddsrt_mutex_lock (&rd->e.lock);
  }
  ddsrt_mutex_unlock (&rd->e.lock);
}
