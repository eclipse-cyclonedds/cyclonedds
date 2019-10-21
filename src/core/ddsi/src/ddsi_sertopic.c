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

#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/md5.h"
#include "cyclonedds/ddsrt/string.h"
#include "cyclonedds/ddsi/q_bswap.h"
#include "cyclonedds/ddsi/q_config.h"
#include "cyclonedds/ddsi/q_freelist.h"
#include "cyclonedds/ddsi/ddsi_iid.h"
#include "cyclonedds/ddsi/ddsi_sertopic.h"
#include "cyclonedds/ddsi/ddsi_serdata.h"

struct ddsi_sertopic *ddsi_sertopic_ref (const struct ddsi_sertopic *sertopic_const)
{
  struct ddsi_sertopic *sertopic = (struct ddsi_sertopic *)sertopic_const;
  if (sertopic)
    ddsrt_atomic_inc32 (&sertopic->refc);
  return sertopic;
}

void ddsi_sertopic_unref (struct ddsi_sertopic *sertopic)
{
  if (sertopic)
  {
    if (ddsrt_atomic_dec32_ov (&sertopic->refc) == 1)
    {
      ddsi_sertopic_free (sertopic);
    }
  }
}

void ddsi_sertopic_init (struct ddsi_sertopic *tp, const char *name, const char *type_name, const struct ddsi_sertopic_ops *sertopic_ops, const struct ddsi_serdata_ops *serdata_ops, bool topickind_no_key)
{
  ddsrt_atomic_st32 (&tp->refc, 1);
  tp->iid = ddsi_iid_gen ();
  tp->name = ddsrt_strdup (name);
  tp->type_name = ddsrt_strdup (type_name);
  size_t ntn_sz = strlen (tp->name) + 1 + strlen (tp->type_name) + 1;
  tp->name_type_name = ddsrt_malloc (ntn_sz);
  (void) snprintf (tp->name_type_name, ntn_sz, "%s/%s", tp->name, tp->type_name);
  tp->ops = sertopic_ops;
  tp->serdata_ops = serdata_ops;
  tp->serdata_basehash = ddsi_sertopic_compute_serdata_basehash (tp->serdata_ops);
  tp->topickind_no_key = topickind_no_key;
}

void ddsi_sertopic_init_anon (struct ddsi_sertopic *tp, const struct ddsi_sertopic_ops *sertopic_ops, const struct ddsi_serdata_ops *serdata_ops, bool topickind_no_key)
{
  ddsrt_atomic_st32 (&tp->refc, 1);
  tp->iid = ddsi_iid_gen ();
  tp->name = NULL;
  tp->type_name = NULL;
  tp->name_type_name = NULL;
  tp->ops = sertopic_ops;
  tp->serdata_ops = serdata_ops;
  tp->serdata_basehash = ddsi_sertopic_compute_serdata_basehash (tp->serdata_ops);
  tp->topickind_no_key = topickind_no_key;
}

void ddsi_sertopic_fini (struct ddsi_sertopic *tp)
{
  ddsrt_free (tp->name);
  ddsrt_free (tp->type_name);
  ddsrt_free (tp->name_type_name);
}

uint32_t ddsi_sertopic_compute_serdata_basehash (const struct ddsi_serdata_ops *ops)
{
  ddsrt_md5_state_t md5st;
  ddsrt_md5_byte_t digest[16];
  uint32_t res;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *) &ops, sizeof (ops));
  ddsrt_md5_append (&md5st, (const ddsrt_md5_byte_t *) ops, sizeof (*ops));
  ddsrt_md5_finish (&md5st, digest);
  memcpy (&res, digest, sizeof (res));
  return res;
}

extern inline void ddsi_sertopic_free (struct ddsi_sertopic *tp);
extern inline void ddsi_sertopic_zero_samples (const struct ddsi_sertopic *tp, void *samples, size_t count);
extern inline void ddsi_sertopic_realloc_samples (void **ptrs, const struct ddsi_sertopic *tp, void *old, size_t oldcount, size_t count);
extern inline void ddsi_sertopic_free_samples (const struct ddsi_sertopic *tp, void **ptrs, size_t count, dds_free_op_t op);
extern inline void ddsi_sertopic_zero_sample (const struct ddsi_sertopic *tp, void *sample);
extern inline void ddsi_sertopic_free_sample (const struct ddsi_sertopic *tp, void *sample, dds_free_op_t op);
extern inline void *ddsi_sertopic_alloc_sample (const struct ddsi_sertopic *tp);
