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
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__loaned_sample.h"
#include "dds__entity.h"

static dds_return_t loan_pool_remove_loan_locked (dds_loaned_sample_t *loaned_sample);

static dds_return_t loaned_sample_free_locked (dds_loaned_sample_t *loaned_sample)
{
  assert (loaned_sample);
  assert (ddsrt_atomic_ld32 (&loaned_sample->refc) == 0);

  if (loaned_sample->ops.free)
    loaned_sample->ops.free (loaned_sample);

  return DDS_RETCODE_OK;
}

dds_return_t dds_loaned_sample_free (dds_loaned_sample_t *loaned_sample)
{
  if (loaned_sample == NULL || ddsrt_atomic_ld32 (&loaned_sample->refc) > 0 || loaned_sample->loan_pool == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret;
  ddsrt_mutex_lock (&loaned_sample->loan_pool->mutex);
  ret = loaned_sample_free_locked (loaned_sample);
  ddsrt_mutex_unlock (&loaned_sample->loan_pool->mutex);
  return ret;
}

dds_return_t dds_loaned_sample_ref (dds_loaned_sample_t *loaned_sample)
{
  if (loaned_sample == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_atomic_inc32 (&loaned_sample->refc);
  return DDS_RETCODE_OK;
}

dds_return_t dds_loaned_sample_unref (dds_loaned_sample_t *loaned_sample)
{
  if (loaned_sample == NULL || ddsrt_atomic_ld32 (&loaned_sample->refc) == 0)
    return DDS_RETCODE_BAD_PARAMETER;

  assert (loaned_sample);
  assert (ddsrt_atomic_ld32 (&loaned_sample->refc) > 0);

  dds_return_t ret = DDS_RETCODE_OK;
  if (ddsrt_atomic_dec32_nv (&loaned_sample->refc) == 0)
  {
    assert (loaned_sample->loan_pool == NULL);
    ret = loaned_sample_free_locked (loaned_sample);
  }

  return ret;
}

dds_return_t dds_loaned_sample_reset_sample (dds_loaned_sample_t *loaned_sample)
{
  assert(loaned_sample && ddsrt_atomic_ld32 (&loaned_sample->refc));
  if (loaned_sample->ops.reset)
    loaned_sample->ops.reset (loaned_sample);
  return DDS_RETCODE_OK;
}

static dds_return_t loan_pool_expand_cap_locked (dds_loan_pool_t *pool, uint32_t n)
{
  if (pool == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if (n > UINT32_MAX - pool->n_samples_cap)
    return DDS_RETCODE_OUT_OF_RANGE;

  uint32_t newcap = pool->n_samples_cap + n;
  dds_loaned_sample_t **newarray = NULL;
  if (newcap > 0)
  {
    newarray = dds_realloc (pool->samples, sizeof (**newarray) * newcap);
    if (newarray == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    memset (newarray + pool->n_samples_cap, 0, sizeof (**newarray) * n);
  }
  pool->samples = newarray;
  pool->n_samples_cap = newcap;

  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_pool_create (dds_loan_pool_t **pool, uint32_t initial_cap)
{
  if (pool == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  if ((*pool = dds_alloc (sizeof (**pool))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  memset (*pool, 0, sizeof (**pool));
  if ((ret = loan_pool_expand_cap_locked (*pool, initial_cap)) != DDS_RETCODE_OK)
    dds_free (*pool);
  ddsrt_mutex_init (&(*pool)->mutex);
  return ret;
}

dds_return_t dds_loan_pool_free (dds_loan_pool_t *pool)
{
  if (pool == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  for (uint32_t i = 0; i < pool->n_samples_cap; i++)
  {
    dds_loaned_sample_t *s = pool->samples[i];
    if (s == NULL)
      continue;
    (void) dds_loan_pool_remove_loan (s);
    (void) dds_loaned_sample_unref (s);
  }

  ddsrt_mutex_destroy (&pool->mutex);
  dds_free (pool->samples);
  dds_free (pool);
  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_pool_add_loan (dds_loan_pool_t *pool, dds_loaned_sample_t *loaned_sample)
{
  dds_return_t ret;
  if (pool == NULL || loaned_sample == NULL || loaned_sample->loan_pool != NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_mutex_lock (&pool->mutex);
  if (pool->n_samples == pool->n_samples_cap)
  {
    uint32_t cap = pool->n_samples_cap;
    uint32_t newcap = cap ? cap * 2 : 1;
    if ((ret = loan_pool_expand_cap_locked (pool, newcap - cap)) != DDS_RETCODE_OK)
    {
      ddsrt_mutex_unlock (&pool->mutex);
      return ret;
    }
  }

  for (uint32_t i = 0; i < pool->n_samples_cap; i++)
  {
    if (!pool->samples[i])
    {
      loaned_sample->loan_idx = i;
      pool->samples[i] = loaned_sample;
      break;
    }
  }
  loaned_sample->loan_pool = pool;
  pool->n_samples++;
  ddsrt_mutex_unlock (&pool->mutex);

  return DDS_RETCODE_OK;
}

static dds_return_t loan_pool_remove_loan_locked (dds_loaned_sample_t *loaned_sample)
{
  assert (loaned_sample);
  assert (loaned_sample->loan_pool);
  dds_loan_pool_t *mgr = loaned_sample->loan_pool;
  if (mgr->n_samples == 0 ||
      loaned_sample->loan_idx >= mgr->n_samples_cap ||
      loaned_sample != mgr->samples[loaned_sample->loan_idx])
  {
    return DDS_RETCODE_BAD_PARAMETER;
  }
  else
  {
    mgr->samples[loaned_sample->loan_idx] = NULL;
    mgr->n_samples--;
    loaned_sample->loan_idx = UINT32_MAX;
    loaned_sample->loan_pool = NULL;
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_loan_pool_remove_loan (dds_loaned_sample_t *loaned_sample)
{
  if (loaned_sample == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_loan_pool_t *mgr = loaned_sample->loan_pool;
  if (!mgr)
    return DDS_RETCODE_OK;

  dds_return_t ret;
  ddsrt_mutex_lock (&mgr->mutex);
  ret = loan_pool_remove_loan_locked (loaned_sample);
  ddsrt_mutex_unlock (&mgr->mutex);
  return ret;
}

dds_loaned_sample_t *dds_loan_pool_find_loan (dds_loan_pool_t *pool, const void *sample_ptr)
{
  if (pool == NULL)
    return NULL;

  dds_loaned_sample_t *ls = NULL;
  ddsrt_mutex_lock (&pool->mutex);
  for (uint32_t i = 0; ls == NULL && i < pool->n_samples_cap && sample_ptr; i++)
  {
    if (pool->samples[i] && pool->samples[i]->sample_ptr == sample_ptr)
      ls = pool->samples[i];
  }
  ddsrt_mutex_unlock (&pool->mutex);
  return ls;
}

dds_loaned_sample_t *dds_loan_pool_get_loan (dds_loan_pool_t *pool)
{
  if (pool == NULL || pool->samples == NULL)
    return NULL;

  dds_loaned_sample_t *ls = NULL;
  ddsrt_mutex_lock (&pool->mutex);
  for (uint32_t i = 0; i < pool->n_samples_cap && ls == NULL; i++)
  {
    if (pool->samples[i])
      ls = pool->samples[i];
  }
  if (ls != NULL)
    loan_pool_remove_loan_locked (ls);
  ddsrt_mutex_unlock (&pool->mutex);
  return ls;
}
