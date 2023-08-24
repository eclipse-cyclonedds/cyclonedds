// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__loaned_sample.h"
#include "dds__heap_loan.h"
#include "dds__entity.h"

typedef struct dds_heap_loan {
  dds_loaned_sample_t c;
  const struct ddsi_sertype *m_stype;
} dds_heap_loan_t;

static void heap_loan_free (dds_loaned_sample_t *loaned_sample)
  ddsrt_nonnull_all;

static void heap_loan_free (dds_loaned_sample_t *loaned_sample)
{
  assert (loaned_sample);
  dds_heap_loan_t *hl = (dds_heap_loan_t *) loaned_sample;
  dds_free (hl->c.metadata);
  if (hl->c.sample_ptr != NULL)
    ddsi_sertype_free_sample (hl->m_stype, hl->c.sample_ptr, DDS_FREE_ALL);
  dds_free (hl);
}

static void heap_loan_reset (dds_loaned_sample_t *loaned_sample)
{
  assert (loaned_sample);
  dds_heap_loan_t *hl = (dds_heap_loan_t *) loaned_sample;
  memset (hl->c.metadata, 0, sizeof (*(hl->c.metadata)));
  ddsi_sertype_free_sample (hl->m_stype, hl->c.sample_ptr, DDS_FREE_CONTENTS);
  ddsi_sertype_zero_sample (hl->m_stype, hl->c.sample_ptr);
}

const dds_loaned_sample_ops_t dds_loan_heap_ops = {
  .free = heap_loan_free,
  .reset = heap_loan_reset
};

dds_return_t dds_heap_loan (const struct ddsi_sertype *type, dds_loaned_sample_state_t sample_state, dds_loaned_sample_t **loaned_sample)
{
  assert (sample_state == DDS_LOANED_SAMPLE_STATE_RAW_KEY || sample_state == DDS_LOANED_SAMPLE_STATE_RAW_DATA);

  dds_heap_loan_t *s = dds_alloc (sizeof (*s));
  if (s == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  if ((s->c.metadata = dds_alloc (sizeof (*s->c.metadata))) == NULL)
  {
    dds_free (s);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  s->c.ops = dds_loan_heap_ops;
  s->m_stype = type;
  if ((s->c.sample_ptr = ddsi_sertype_alloc_sample (type)) == NULL)
  {
    dds_free (s);
    dds_free (s->c.metadata);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }

  s->c.metadata->sample_state = sample_state;
  s->c.metadata->cdr_identifier = DDSI_RTPS_SAMPLE_NATIVE;
  s->c.metadata->cdr_options = 0;
  s->c.loan_origin.origin_kind = DDS_LOAN_ORIGIN_KIND_HEAP;
  s->c.loan_origin.psmx_endpoint = NULL;
  ddsrt_atomic_st32 (&s->c.refc, 1);

  *loaned_sample = (dds_loaned_sample_t *) s;

  return DDS_RETCODE_OK;
}
