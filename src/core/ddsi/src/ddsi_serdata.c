// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stddef.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "dds/export.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "ddsi__radmin.h"

void ddsi_serdata_init (struct ddsi_serdata *d, const struct ddsi_sertype *tp, enum ddsi_serdata_kind kind)
{
  d->type = tp;
  d->ops = tp->serdata_ops;
  d->kind = kind;
  d->hash = 0;
  d->statusinfo = 0;
  d->timestamp.v = INT64_MIN;
  d->twrite.v = INT64_MIN;
#ifdef DDS_HAS_SHM
  d->iox_chunk = NULL;
  d->iox_subscriber = NULL;
#endif
  ddsrt_atomic_st32 (&d->refc, 1);
}

struct ddsi_serdata *ddsi_serdata_ref_as_type (const struct ddsi_sertype *type, struct ddsi_serdata *serdata)
{
  if (serdata->type == type)
    return ddsi_serdata_ref (serdata);
  else
  {
    /* ouch ... convert a serdata from one sertype to another ... */
    struct ddsi_serdata *converted;
    ddsrt_iovec_t iov;
    uint32_t size = ddsi_serdata_size (serdata);
    (void) ddsi_serdata_to_ser_ref (serdata, 0, size, &iov);
    if ((converted = ddsi_serdata_from_ser_iov (type, serdata->kind, 1, &iov, size)) != NULL)
    {
      converted->statusinfo = serdata->statusinfo;
      converted->timestamp = serdata->timestamp;
    }
    ddsi_serdata_to_ser_unref (serdata, &iov);
    return converted;
  }
}

const ddsi_keyhash_t *ddsi_serdata_keyhash_from_fragchain (const struct ddsi_rdata *fragchain)
{
  if (fragchain->keyhash_zoff == 0)
    return NULL;
  else
    return (const ddsi_keyhash_t *) DDSI_RMSG_PAYLOADOFF (fragchain->rmsg, DDSI_RDATA_KEYHASH_OFF (fragchain));
}

DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_ref (const struct ddsi_serdata *serdata_const);
DDS_EXPORT extern inline void ddsi_serdata_unref (struct ddsi_serdata *serdata);
DDS_EXPORT extern inline uint32_t ddsi_serdata_size (const struct ddsi_serdata *d);
DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_from_ser (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size);
DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_from_ser_iov (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, ddsrt_msg_iovlen_t niov, const ddsrt_iovec_t *iov, size_t size);
DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_from_keyhash (const struct ddsi_sertype *type, const struct ddsi_keyhash *keyhash);
DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_from_sample (const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, const void *sample);
DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_to_untyped (const struct ddsi_serdata *d);
DDS_EXPORT extern inline void ddsi_serdata_to_ser (const struct ddsi_serdata *d, size_t off, size_t sz, void *buf);
DDS_EXPORT extern inline struct ddsi_serdata *ddsi_serdata_to_ser_ref (const struct ddsi_serdata *d, size_t off, size_t sz, ddsrt_iovec_t *ref);
DDS_EXPORT extern inline void ddsi_serdata_to_ser_unref (struct ddsi_serdata *d, const ddsrt_iovec_t *ref);
DDS_EXPORT extern inline bool ddsi_serdata_to_sample (const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);
DDS_EXPORT extern inline bool ddsi_serdata_untyped_to_sample (const struct ddsi_sertype *type, const struct ddsi_serdata *d, void *sample, void **bufptr, void *buflim);
DDS_EXPORT extern inline bool ddsi_serdata_eqkey (const struct ddsi_serdata *a, const struct ddsi_serdata *b);
DDS_EXPORT extern inline bool ddsi_serdata_print (const struct ddsi_serdata *d, char *buf, size_t size);
DDS_EXPORT extern inline bool ddsi_serdata_print_untyped (const struct ddsi_sertype *type, const struct ddsi_serdata *d, char *buf, size_t size);
DDS_EXPORT extern inline void ddsi_serdata_get_keyhash (const struct ddsi_serdata *d, struct ddsi_keyhash *buf, bool force_md5);
#ifdef DDS_HAS_SHM
DDS_EXPORT extern inline uint32_t ddsi_serdata_iox_size(const struct ddsi_serdata* d);
DDS_EXPORT extern inline struct ddsi_serdata* ddsi_serdata_from_iox(const struct ddsi_sertype* type, enum ddsi_serdata_kind kind, void* sub, void* iox_buffer);
DDS_EXPORT extern inline struct ddsi_serdata* ddsi_serdata_from_loaned_sample(const struct ddsi_sertype *type, enum ddsi_serdata_kind kind, const char *sample);
#endif
