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
#include "dds__loan.h"
#include "dds__entity.h"

static dds_return_t loan_manager_remove_loan_locked (dds_loaned_sample_t *loaned_sample);

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
  if (loaned_sample == NULL || ddsrt_atomic_ld32 (&loaned_sample->refc) > 0 || loaned_sample->manager == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret;
  ddsrt_mutex_lock (&loaned_sample->manager->mutex);
  ret = loaned_sample_free_locked (loaned_sample);
  ddsrt_mutex_unlock (&loaned_sample->manager->mutex);
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
    assert (loaned_sample->manager == NULL);
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

static dds_return_t loan_manager_expand_cap_locked (dds_loan_manager_t *manager, uint32_t n)
{
  if (manager == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  if (n > UINT32_MAX - manager->n_samples_cap)
    return DDS_RETCODE_OUT_OF_RANGE;

  uint32_t newcap = manager->n_samples_cap + n;
  dds_loaned_sample_t **newarray = NULL;
  if (newcap > 0)
  {
    newarray = dds_realloc (manager->samples, sizeof (**newarray) * newcap);
    if (newarray == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    memset (newarray + manager->n_samples_cap, 0, sizeof (**newarray) * n);
  }
  manager->samples = newarray;
  manager->n_samples_cap = newcap;

  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_manager_create (dds_loan_manager_t **manager, uint32_t initial_cap)
{
  if (manager == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  if ((*manager = dds_alloc (sizeof (**manager))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  memset (*manager, 0, sizeof (**manager));
  if ((ret = loan_manager_expand_cap_locked (*manager, initial_cap)) != DDS_RETCODE_OK)
    dds_free (*manager);
  ddsrt_mutex_init (&(*manager)->mutex);
  return ret;
}

dds_return_t dds_loan_manager_free (dds_loan_manager_t *manager)
{
  if (manager == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  for (uint32_t i = 0; i < manager->n_samples_cap; i++)
  {
    dds_loaned_sample_t *s = manager->samples[i];
    if (s == NULL)
      continue;
    (void) dds_loan_manager_remove_loan (s);
    (void) dds_loaned_sample_unref (s);
  }

  ddsrt_mutex_destroy (&manager->mutex);
  dds_free (manager->samples);
  dds_free (manager);
  return DDS_RETCODE_OK;
}

dds_return_t dds_loan_manager_add_loan (dds_loan_manager_t *manager, dds_loaned_sample_t *loaned_sample)
{
  dds_return_t ret;
  if (manager == NULL || loaned_sample == NULL || loaned_sample->manager != NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_mutex_lock (&manager->mutex);
  if (manager->n_samples_managed == manager->n_samples_cap)
  {
    uint32_t cap = manager->n_samples_cap;
    uint32_t newcap = cap ? cap * 2 : 1;
    if ((ret = loan_manager_expand_cap_locked (manager, newcap - cap)) != DDS_RETCODE_OK)
    {
      ddsrt_mutex_unlock (&manager->mutex);
      return ret;
    }
  }

  for (uint32_t i = 0; i < manager->n_samples_cap; i++)
  {
    if (!manager->samples[i])
    {
      loaned_sample->loan_idx = i;
      manager->samples[i] = loaned_sample;
      break;
    }
  }
  loaned_sample->manager = manager;
  manager->n_samples_managed++;
  ddsrt_mutex_unlock (&manager->mutex);

  return DDS_RETCODE_OK;
}

static dds_return_t loan_manager_remove_loan_locked (dds_loaned_sample_t *loaned_sample)
{
  assert (loaned_sample);
  assert (loaned_sample->manager);
  dds_loan_manager_t *mgr = loaned_sample->manager;
  if (mgr->n_samples_managed == 0 ||
      loaned_sample->loan_idx >= mgr->n_samples_cap ||
      loaned_sample != mgr->samples[loaned_sample->loan_idx])
  {
    return DDS_RETCODE_BAD_PARAMETER;
  }
  else
  {
    mgr->samples[loaned_sample->loan_idx] = NULL;
    mgr->n_samples_managed--;
    loaned_sample->loan_idx = UINT32_MAX;
    loaned_sample->manager = NULL;
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_loan_manager_remove_loan (dds_loaned_sample_t *loaned_sample)
{
  if (loaned_sample == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_loan_manager_t *mgr = loaned_sample->manager;
  if (!mgr)
    return DDS_RETCODE_OK;

  dds_return_t ret;
  ddsrt_mutex_lock (&mgr->mutex);
  ret = loan_manager_remove_loan_locked (loaned_sample);
  ddsrt_mutex_unlock (&mgr->mutex);
  return ret;
}

dds_loaned_sample_t *dds_loan_manager_find_loan (dds_loan_manager_t *manager, const void *sample_ptr)
{
  if (manager == NULL)
    return NULL;

  dds_loaned_sample_t *ls = NULL;
  ddsrt_mutex_lock (&manager->mutex);
  for (uint32_t i = 0; ls == NULL && i < manager->n_samples_cap && sample_ptr; i++)
  {
    if (manager->samples[i] && manager->samples[i]->sample_ptr == sample_ptr)
      ls = manager->samples[i];
  }
  ddsrt_mutex_unlock (&manager->mutex);
  return ls;
}

dds_loaned_sample_t *dds_loan_manager_get_loan (dds_loan_manager_t *manager)
{
  if (manager == NULL || manager->samples == NULL)
    return NULL;

  dds_loaned_sample_t *ls = NULL;
  ddsrt_mutex_lock (&manager->mutex);
  for (uint32_t i = 0; i < manager->n_samples_cap && ls == NULL; i++)
  {
    if (manager->samples[i])
      ls = manager->samples[i];
  }
  if (ls != NULL)
    loan_manager_remove_loan_locked (ls);
  ddsrt_mutex_unlock (&manager->mutex);
  return ls;
}
