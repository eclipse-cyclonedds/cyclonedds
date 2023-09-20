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
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__loaned_sample.h"
#include "dds__entity.h"

DDS_EXPORT extern inline void dds_loaned_sample_ref (dds_loaned_sample_t *loaned_sample);
DDS_EXPORT extern inline void dds_loaned_sample_unref (dds_loaned_sample_t *loaned_sample);

static dds_return_t loan_pool_expand_cap (dds_loan_pool_t *pool, uint32_t n)
  ddsrt_nonnull_all;

static dds_return_t loan_pool_expand_cap (dds_loan_pool_t *pool, uint32_t n)
{
  assert (n <= UINT32_MAX - pool->n_samples_cap);
  uint32_t newcap = pool->n_samples_cap + n;
  dds_loaned_sample_t **newarray = NULL;
  newarray = ddsrt_realloc (pool->samples, sizeof (*newarray) * newcap);
  if (newarray == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  memset (newarray + pool->n_samples_cap, 0, sizeof (*newarray) * n);
  pool->samples = newarray;
  pool->n_samples_cap = newcap;
  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_pool_create (dds_loan_pool_t **ppool, uint32_t initial_cap)
{
  dds_loan_pool_t *pool;
  if ((pool = *ppool = ddsrt_malloc (sizeof (*pool))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  pool->n_samples = 0;
  pool->n_samples_cap = initial_cap;
  if (initial_cap == 0) {
    // it makes sense to optimise this: most applications presumably will never use
    // them on the writer, and there will probably also be subscribers that don't
    // need them
    pool->samples = NULL;
  } else if ((pool->samples = ddsrt_malloc (pool->n_samples_cap * sizeof (*pool->samples))) == NULL) {
    ddsrt_free (pool);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  } else {
    memset (pool->samples, 0, pool->n_samples_cap * sizeof (*pool->samples));
  }
  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_pool_free (dds_loan_pool_t *pool)
{
  for (uint32_t i = 0; i < pool->n_samples; i++)
  {
    dds_loaned_sample_t *s = pool->samples[i];
    assert (s != NULL);
    dds_loaned_sample_unref (s);
  }
#ifndef NDEBUG
  for (uint32_t i = pool->n_samples; i < pool->n_samples_cap; i++)
    assert (pool->samples[i] == NULL);
#endif
  ddsrt_free (pool->samples);
  ddsrt_free (pool);
  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_pool_add_loan (dds_loan_pool_t *pool, dds_loaned_sample_t *loaned_sample)
{
  dds_return_t ret;
  if (pool->n_samples == pool->n_samples_cap)
  {
    uint32_t cap = pool->n_samples_cap;
    uint32_t newcap;
    if (cap == 0)
      newcap = 1;
    else if (cap <= UINT32_MAX / 2)
      newcap = cap * 2;
    else if (cap == UINT32_MAX)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    else
      newcap = UINT32_MAX;
    if ((ret = loan_pool_expand_cap (pool, newcap - cap)) != DDS_RETCODE_OK)
      return ret;
  }
  pool->samples[pool->n_samples++] = loaned_sample;
  return DDS_RETCODE_OK;
}

dds_loaned_sample_t *dds_loan_pool_find_and_remove_loan (dds_loan_pool_t *pool, const void *sample_ptr)
{
  for (uint32_t i = 0; i < pool->n_samples; i++)
  {
    if (pool->samples[i]->sample_ptr == sample_ptr)
    {
      dds_loaned_sample_t * const ls = pool->samples[i];
      assert (pool->n_samples > 0);
      if (i < --pool->n_samples)
        pool->samples[i] = pool->samples[pool->n_samples];
      pool->samples[pool->n_samples] = NULL;
      return ls;
    }
  }
  return NULL;
}

dds_loaned_sample_t *dds_loan_pool_get_loan (dds_loan_pool_t *pool)
{
  if (pool->n_samples == 0)
    return NULL;
  --pool->n_samples;
  dds_loaned_sample_t * const ls = pool->samples[pool->n_samples];
  assert (ls != NULL);
  pool->samples[pool->n_samples] = NULL;
  return ls;
}
