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

#include "dds/ddsrt/md5.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_serdata.h"

void ddsi_serdata_init (struct ddsi_serdata *d, const struct ddsi_sertopic *tp, enum ddsi_serdata_kind kind)
{
  d->topic = tp;
  d->ops = tp->serdata_ops;
  d->kind = kind;
  d->hash = 0;
  d->statusinfo = 0;
  d->timestamp.v = INT64_MIN;
  d->twrite.v = INT64_MIN;
  ddsrt_atomic_st32 (&d->refc, 1);
}

struct ddsi_serdata *ddsi_serdata_ref_as_topic (const struct ddsi_sertopic *topic, struct ddsi_serdata *serdata)
{
  if (serdata->topic == topic)
    return ddsi_serdata_ref (serdata);
  else
  {
    /* ouch ... convert a serdata from one sertopic to another ... */
    struct ddsi_serdata *converted;
    ddsrt_iovec_t iov;
    uint32_t size = ddsi_serdata_size (serdata);
    (void) ddsi_serdata_to_ser_ref (serdata, 0, size, &iov);
    if ((converted = ddsi_serdata_from_ser_iov (topic, serdata->kind, 1, &iov, size)) != NULL)
    {
      converted->statusinfo = serdata->statusinfo;
      converted->timestamp = serdata->timestamp;
    }
    ddsi_serdata_to_ser_unref (serdata, &iov);
    return converted;
  }
}

extern inline struct ddsi_serdata *ddsi_serdata_ref (const struct ddsi_serdata *serdata_const);
extern inline void ddsi_serdata_unref (struct ddsi_serdata *serdata);
extern inline uint32_t ddsi_serdata_size (const struct ddsi_serdata *d);
extern inline struct ddsi_serdata *ddsi_serdata_from_ser (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const struct nn_rdata *fragchain, size_t size);
extern inline struct ddsi_serdata *ddsi_serdata_from_ser_iov (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size);
extern inline struct ddsi_serdata *ddsi_serdata_from_keyhash (const struct ddsi_sertopic *topic, const struct ddsi_keyhash *keyhash);
extern inline struct ddsi_serdata *ddsi_serdata_from_sample (const struct ddsi_sertopic *topic, enum ddsi_serdata_kind kind, const void *sample);
extern inline struct ddsi_serdata *ddsi_serdata_to_topicless (const struct ddsi_serdata *d);
extern inline void ddsi_serdata_to_ser (const struct ddsi_serdata *d, size_t off, size_t sz, void *buf);
extern inline struct ddsi_serdata *ddsi_serdata_to_ser_ref (const struct ddsi_serdata *d, size_t off, size_t sz, ddsrt_iovec_t *ref);
extern inline void ddsi_serdata_to_ser_unref (struct ddsi_serdata *d, const ddsrt_iovec_t *ref);
extern inline bool ddsi_serdata_to_sample (const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);
extern inline bool ddsi_serdata_topicless_to_sample (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);
extern inline bool ddsi_serdata_eqkey (const struct ddsi_serdata *a, const struct ddsi_serdata *b);
extern inline bool ddsi_serdata_print (const struct ddsi_serdata *d, char *buf, size_t size);
extern inline bool ddsi_serdata_print_topicless (const struct ddsi_sertopic *topic, const struct ddsi_serdata *d, char *buf, size_t size);
extern inline void ddsi_serdata_get_keyhash (const struct ddsi_serdata *d, struct ddsi_keyhash *buf, bool force_md5);
