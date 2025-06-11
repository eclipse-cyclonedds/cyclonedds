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

#include "dds/features.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "ddsi__serdata_cdr.h"
#include "dds/cdr/dds_cdrstream.h"

static bool sertype_cdr_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  const struct ddsi_sertype_cdr *a = (struct ddsi_sertype_cdr *) acmn;
  const struct ddsi_sertype_cdr *b = (struct ddsi_sertype_cdr *) bcmn;
  if (a->encoding_format != b->encoding_format)
    return false;
  if (a->type.size != b->type.size)
    return false;
  if (a->type.align != b->type.align)
    return false;
  if (a->type.flagset != b->type.flagset)
    return false;
  if (a->type.ops.nops != b->type.ops.nops)
    return false;
  if (a->type.ops.nops > 0 && memcmp (a->type.ops.ops, b->type.ops.ops, a->type.ops.nops * sizeof (*a->type.ops.ops)))
    return false;
  assert (a->type.opt_size_xcdr2 == b->type.opt_size_xcdr2);
  return true;
}

static uint32_t sertype_cdr_hash (const struct ddsi_sertype *tpcmn)
{
  assert (tpcmn);
  const struct ddsi_sertype_cdr *tp = (struct ddsi_sertype_cdr *) tpcmn;
  unsigned char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->c.type_name, (uint32_t) strlen (tp->c.type_name));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->encoding_format, sizeof (tp->encoding_format));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.size, sizeof (tp->type.size));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.align, sizeof (tp->type.align));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.flagset, sizeof (tp->type.flagset));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->type.ops.ops, (uint32_t) (tp->type.ops.nops * sizeof (*tp->type.ops.ops)));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  return *(uint32_t *) buf;
}

static void sertype_cdr_free (struct ddsi_sertype *tpcmn)
{
  struct ddsi_sertype_cdr *tp = (struct ddsi_sertype_cdr *) tpcmn;
  ddsrt_free (tp->type.ops.ops);
  ddsi_sertype_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertype_cdr_zero_samples (const struct ddsi_sertype *sertype_common, void *sample, size_t count)
{
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *) sertype_common;
  memset (sample, 0, tp->type.size * count);
}

static void sertype_cdr_realloc_samples (void **ptrs, const struct ddsi_sertype *sertype_common, void *old, size_t oldcount, size_t count)
{
  const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *) sertype_common;
  const size_t size = tp->type.size;
  char *new = (oldcount == count) ? old : dds_realloc (old, size * count);
  if (new && count > oldcount)
    memset (new + size * oldcount, 0, size * (count - oldcount));
  for (size_t i = 0; i < count; i++)
  {
    void *ptr = (char *) new + i * size;
    ptrs[i] = ptr;
  }
}

static bool type_may_contain_ptr (const struct ddsi_sertype_cdr *tp)
{
  /* In case the optimized size for XCDR 2 is 0 (not optimized), there may
     be a pointer in the type and free_samples needs to do a full
     dds_stream_free_sample for each sample. */
  /* TODO: improve this check so that it only returns true in case the type
     really contains a pointer, by inspection of the serializer ops */
  return tp->type.opt_size_xcdr2 == 0;
}

static void sertype_cdr_free_samples (const struct ddsi_sertype *sertype_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct ddsi_sertype_cdr *tp = (const struct ddsi_sertype_cdr *) sertype_common;
    const struct dds_cdrstream_desc *type = &tp->type;
    const size_t size = type->size;
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *) ptrs[i] == (char *) ptrs[0] + off);
#endif
    if (type_may_contain_ptr (tp))
    {
      char *ptr = ptrs[0];
      for (size_t i = 0; i < count; i++)
      {
        dds_stream_free_sample (ptr, &dds_cdrstream_default_allocator, type->ops.ops);
        ptr += size;
      }
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (ptrs[0]);
    }
  }
}

const struct ddsi_sertype_ops ddsi_sertype_ops_cdr = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertype_cdr_equal,
  .hash = sertype_cdr_hash,
  .free = sertype_cdr_free,
  .zero_samples = sertype_cdr_zero_samples,
  .realloc_samples = sertype_cdr_realloc_samples,
  .free_samples = sertype_cdr_free_samples,
  .type_id = 0,
  .type_map = 0,
  .type_info = 0,
  .derive_sertype = 0,
  .get_serialized_size = 0,
  .serialize_into = 0
};

dds_return_t ddsi_sertype_cdr_init (const struct ddsi_domaingv *gv, struct ddsi_sertype_cdr *st, const dds_topic_descriptor_t *desc)
{
  /* Get the extensility of the outermost object in the type used for the topic. Note that the
     outermost type can have a different extensibility than nested types used in this type;
     the extensibility that is returned here is used to set the CDR encapsulation identifier,
     but nested types can use a different data representation format (not version) */
  enum dds_cdr_type_extensibility type_ext;
  if (!dds_stream_extensibility (desc->m_ops, &type_ext))
    return DDS_RETCODE_BAD_PARAMETER;

  ddsi_sertype_init_props (&st->c, desc->m_typename, &ddsi_sertype_ops_cdr, &ddsi_serdata_ops_cdr, desc->m_size, dds_stream_data_types (desc->m_ops), DDS_DATA_REPRESENTATION_FLAG_XCDR2, 0);

  st->encoding_format = ddsi_sertype_extensibility_enc_format (type_ext);

  dds_cdrstream_desc_init (&st->type, &dds_cdrstream_default_allocator, desc->m_size, desc->m_align, desc->m_flagset, desc->m_ops, desc->m_keys, desc->m_nkeys, desc->m_mid_table_offs);

  if (dds_stream_type_nesting_depth (desc->m_ops) > DDS_CDRSTREAM_MAX_NESTING_DEPTH)
  {
    ddsi_sertype_unref (&st->c);
    GVTRACE ("Serializer ops for type %s has unsupported nesting depth (max %u)\n", desc->m_typename, DDS_CDRSTREAM_MAX_NESTING_DEPTH);
    return DDS_RETCODE_BAD_PARAMETER;
  }

  st->type.opt_size_xcdr2 = dds_stream_check_optimize (&st->type, DDSI_RTPS_CDR_ENC_VERSION_2);
  if (st->type.opt_size_xcdr2 > 0)
    GVTRACE ("Marshalling XCDR2 for type: %s is %soptimised\n", st->c.type_name, st->type.opt_size_xcdr2 ? "" : "not ");

  return DDS_RETCODE_OK;
}
