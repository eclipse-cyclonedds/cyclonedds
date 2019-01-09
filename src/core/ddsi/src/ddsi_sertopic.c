/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "os/os.h"
#include "ddsi/q_md5.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_config.h"
#include "ddsi/q_freelist.h"
#include "ddsi/ddsi_sertopic.h"
#include "ddsi/ddsi_serdata.h"
#include "ddsi/q_md5.h"

struct ddsi_sertopic *ddsi_sertopic_ref (const struct ddsi_sertopic *sertopic_const)
{
  struct ddsi_sertopic *sertopic = (struct ddsi_sertopic *)sertopic_const;
  if (sertopic)
    os_atomic_inc32 (&sertopic->refc);
  return sertopic;
}

void ddsi_sertopic_unref (struct ddsi_sertopic *sertopic)
{
  if (sertopic)
  {
    if (os_atomic_dec32_ov (&sertopic->refc) == 1)
    {
      ddsi_sertopic_deinit (sertopic);
      os_free (sertopic->name_typename);
      os_free (sertopic->name);
      os_free (sertopic->typename);
      os_free (sertopic);
    }
  }
}

uint32_t ddsi_sertopic_compute_serdata_basehash (const struct ddsi_serdata_ops *ops)
{
  md5_state_t md5st;
  md5_byte_t digest[16];
  uint32_t res;
  md5_init (&md5st);
  md5_append (&md5st, (const md5_byte_t *) &ops, sizeof (ops));
  md5_append (&md5st, (const md5_byte_t *) ops, sizeof (*ops));
  md5_finish (&md5st, digest);
  memcpy (&res, digest, sizeof (res));
  return res;
}

extern inline void ddsi_sertopic_deinit (struct ddsi_sertopic *tp);
extern inline void ddsi_sertopic_zero_samples (const struct ddsi_sertopic *tp, void *samples, size_t count);
extern inline void ddsi_sertopic_realloc_samples (void **ptrs, const struct ddsi_sertopic *tp, void *old, size_t oldcount, size_t count);
extern inline void ddsi_sertopic_free_samples (const struct ddsi_sertopic *tp, void **ptrs, size_t count, dds_free_op_t op);
extern inline void ddsi_sertopic_zero_sample (const struct ddsi_sertopic *tp, void *sample);
extern inline void ddsi_sertopic_free_sample (const struct ddsi_sertopic *tp, void *sample, dds_free_op_t op);
extern inline void *ddsi_sertopic_alloc_sample (const struct ddsi_sertopic *tp);
