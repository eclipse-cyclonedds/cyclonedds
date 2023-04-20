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
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds__serdata_default.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_xmsg.h"
#endif

static bool sertype_default_equal (const struct ddsi_sertype *acmn, const struct ddsi_sertype *bcmn)
{
  const struct dds_sertype_default *a = (struct dds_sertype_default *) acmn;
  const struct dds_sertype_default *b = (struct dds_sertype_default *) bcmn;
  if (a->encoding_format != b->encoding_format)
    return false;
  if (a->type.size != b->type.size)
    return false;
  if (a->type.align != b->type.align)
    return false;
  if (a->type.flagset != b->type.flagset)
    return false;
  if (a->type.keys.nkeys != b->type.keys.nkeys)
    return false;
  if (
    (a->type.keys.nkeys > 0) &&
    memcmp (a->type.keys.keys, b->type.keys.keys, a->type.keys.nkeys * sizeof (*a->type.keys.keys)) != 0)
    return false;
  if (a->type.ops.nops != b->type.ops.nops)
    return false;
  if (
    (a->type.ops.nops > 0) &&
    memcmp (a->type.ops.ops, b->type.ops.ops, a->type.ops.nops * sizeof (*a->type.ops.ops)) != 0)
    return false;
  assert (a->type.opt_size_xcdr1 == b->type.opt_size_xcdr1);
  assert (a->type.opt_size_xcdr2 == b->type.opt_size_xcdr2);
  return true;
}

#ifdef DDS_HAS_TYPE_DISCOVERY

static ddsi_typeid_t * sertype_default_typeid (const struct ddsi_sertype *tpcmn, ddsi_typeid_kind_t kind)
{
  assert (tpcmn);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  const struct dds_sertype_default *tp = (struct dds_sertype_default *) tpcmn;
  ddsi_typeinfo_t *type_info = ddsi_typeinfo_deser (tp->typeinfo_ser.data, tp->typeinfo_ser.sz);
  if (type_info == NULL)
    return NULL;
  ddsi_typeid_t *type_id = ddsi_typeinfo_typeid (type_info, kind);
  ddsi_typeinfo_fini (type_info);
  ddsrt_free (type_info);
  return type_id;
}

static ddsi_typemap_t * sertype_default_typemap (const struct ddsi_sertype *tpcmn)
{
  assert (tpcmn);
  const struct dds_sertype_default *tp = (struct dds_sertype_default *) tpcmn;
  return ddsi_typemap_deser (tp->typemap_ser.data, tp->typemap_ser.sz);
}

static ddsi_typeinfo_t *sertype_default_typeinfo (const struct ddsi_sertype *tpcmn)
{
  assert (tpcmn);
  const struct dds_sertype_default *tp = (struct dds_sertype_default *) tpcmn;
  return ddsi_typeinfo_deser (tp->typeinfo_ser.data, tp->typeinfo_ser.sz);
}

#endif /* DDS_HAS_TYPE_DISCOVERY */

static uint32_t sertype_default_hash (const struct ddsi_sertype *tpcmn)
{
  assert (tpcmn);
  const struct dds_sertype_default *tp = (struct dds_sertype_default *) tpcmn;
  unsigned char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->c.type_name, (uint32_t) strlen (tp->c.type_name));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->encoding_format, sizeof (tp->encoding_format));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.size, sizeof (tp->type.size));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.align, sizeof (tp->type.align));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) &tp->type.flagset, sizeof (tp->type.flagset));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->type.keys.keys, (uint32_t) (tp->type.keys.nkeys * sizeof (*tp->type.keys.keys)));
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) tp->type.ops.ops, (uint32_t) (tp->type.ops.nops * sizeof (*tp->type.ops.ops)));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  return *(uint32_t *) buf;
}

static void sertype_default_free (struct ddsi_sertype *tpcmn)
{
  struct dds_sertype_default *tp = (struct dds_sertype_default *) tpcmn;
  ddsrt_free (tp->type.keys.keys);
  ddsrt_free (tp->type.ops.ops);
  if (tp->typeinfo_ser.data != NULL)
    ddsrt_free (tp->typeinfo_ser.data);
  if (tp->typemap_ser.data != NULL)
    ddsrt_free (tp->typemap_ser.data);
  ddsi_sertype_fini (&tp->c);
  ddsrt_free (tp);
}

static void sertype_default_zero_samples (const struct ddsi_sertype *sertype_common, void *sample, size_t count)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)sertype_common;
  memset (sample, 0, tp->type.size * count);
}

static void sertype_default_realloc_samples (void **ptrs, const struct ddsi_sertype *sertype_common, void *old, size_t oldcount, size_t count)
{
  const struct dds_sertype_default *tp = (const struct dds_sertype_default *)sertype_common;
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

static bool type_may_contain_ptr (const struct dds_sertype_default *tp)
{
  /* In case the optimized size for both XCDR1 and 2 is 0 (not optimized),
     there may be a pointer in the type and free_samples needs to do a full
     dds_stream_free_sample for each sample. */
  /* TODO: improve this check so that it only returns true in case the type
     really contains a pointer, by inspection of the serializer ops */
  return tp->type.opt_size_xcdr1 == 0 || tp->type.opt_size_xcdr2 == 0;
}

static void sertype_default_free_samples (const struct ddsi_sertype *sertype_common, void **ptrs, size_t count, dds_free_op_t op)
{
  if (count > 0)
  {
    const struct dds_sertype_default *tp = (const struct dds_sertype_default *)sertype_common;
    const struct dds_cdrstream_desc *type = &tp->type;
    const size_t size = type->size;
#ifndef NDEBUG
    for (size_t i = 0, off = 0; i < count; i++, off += size)
      assert ((char *)ptrs[i] == (char *)ptrs[0] + off);
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

static struct ddsi_sertype * sertype_default_derive_sertype (const struct ddsi_sertype *base_sertype, dds_data_representation_id_t data_representation, dds_type_consistency_enforcement_qospolicy_t tce_qos)
{
  const struct dds_sertype_default *base_sertype_default = (const struct dds_sertype_default *) base_sertype;
  struct dds_sertype_default *derived_sertype = NULL;
  const struct ddsi_serdata_ops *required_ops;

  assert (base_sertype);

  // FIXME: implement using options from the type consistency enforcement qos policy in (de)serializer
  (void) tce_qos;

  if (data_representation == DDS_DATA_REPRESENTATION_XCDR1)
    required_ops = base_sertype->typekind_no_key ? &dds_serdata_ops_cdr_nokey : &dds_serdata_ops_cdr;
  else if (data_representation == DDS_DATA_REPRESENTATION_XCDR2)
    required_ops = base_sertype->typekind_no_key ? &dds_serdata_ops_xcdr2_nokey : &dds_serdata_ops_xcdr2;
  else
    abort ();

  if (base_sertype->serdata_ops == required_ops)
    derived_sertype = (struct dds_sertype_default *) base_sertype_default;
  else
  {
    derived_sertype = ddsrt_memdup (base_sertype_default, sizeof (*derived_sertype));
    uint32_t refc = ddsrt_atomic_ld32 (&derived_sertype->c.flags_refc);
    ddsrt_atomic_st32 (&derived_sertype->c.flags_refc, refc & ~DDSI_SERTYPE_REFC_MASK);
    derived_sertype->c.base_sertype = ddsi_sertype_ref (base_sertype);
    derived_sertype->c.serdata_ops = required_ops;
    derived_sertype->write_encoding_version = data_representation == DDS_DATA_REPRESENTATION_XCDR1 ? DDSI_RTPS_CDR_ENC_VERSION_1 : DDSI_RTPS_CDR_ENC_VERSION_2;
  }

  return (struct ddsi_sertype *) derived_sertype;
}

// move to cdr_stream?
static dds_ostream_t ostream_from_buffer(void *buffer, size_t size, uint16_t write_encoding_version)
{
  dds_ostream_t os;
  os.m_buffer = buffer;
  os.m_size = (uint32_t) size;
  os.m_index = 0;
  os.m_xcdr_version = write_encoding_version;
  return os;
}

// placeholder implementation
// TODO: implement efficiently (we now actually serialize to get the size)
//       This is similar to serializing but instead counting bytes instead of writing
//       data to a stream.
//       This should be (almost...) O(1), there may be issues with
//       sequences of nontrivial types where it will depend on the number of elements.
static size_t sertype_default_get_serialized_size (const struct ddsi_sertype *type, const void *sample)
{
  // We do not count the CDR header here.
  // TODO Do we want to include CDR header into the serialization used by iceoryx?
  //      If the endianness does not change, it appears not to be necessary (maybe for
  //      XTypes)
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample(type, SDK_DATA, sample);
  size_t serialized_size = ddsi_serdata_size(serdata) - sizeof(struct dds_cdr_header);
  ddsi_serdata_unref(serdata);
  return serialized_size;
}

static bool sertype_default_serialize_into (const struct ddsi_sertype *type, const void *sample, void* dst_buffer, size_t dst_size)
{
  const struct dds_sertype_default *type_default = (const struct dds_sertype_default *)type;
  dds_ostream_t os = ostream_from_buffer(dst_buffer, dst_size, type_default->write_encoding_version);
  return dds_stream_write_sample(&os, &dds_cdrstream_default_allocator, sample, &type_default->type);
}

const struct ddsi_sertype_ops dds_sertype_ops_default = {
  .version = ddsi_sertype_v0,
  .arg = 0,
  .equal = sertype_default_equal,
  .hash = sertype_default_hash,
  .free = sertype_default_free,
  .zero_samples = sertype_default_zero_samples,
  .realloc_samples = sertype_default_realloc_samples,
  .free_samples = sertype_default_free_samples,
#ifdef DDS_HAS_TYPE_DISCOVERY
  .type_id = sertype_default_typeid,
  .type_map = sertype_default_typemap,
  .type_info = sertype_default_typeinfo,
#else
  .type_id = 0,
  .type_map = 0,
  .type_info = 0,
#endif
  .derive_sertype = sertype_default_derive_sertype,
  .get_serialized_size = sertype_default_get_serialized_size,
  .serialize_into = sertype_default_serialize_into
};

dds_return_t dds_sertype_default_init (const struct dds_domain *domain, struct dds_sertype_default *st, const dds_topic_descriptor_t *desc, uint16_t min_xcdrv, dds_data_representation_id_t data_representation)
{
  const struct ddsi_domaingv *gv = &domain->gv;
  const struct ddsi_serdata_ops *serdata_ops;
  switch (data_representation)
  {
    case DDS_DATA_REPRESENTATION_XCDR1:
      serdata_ops = desc->m_nkeys ? &dds_serdata_ops_cdr : &dds_serdata_ops_cdr_nokey;
      break;
    case DDS_DATA_REPRESENTATION_XCDR2:
      serdata_ops = desc->m_nkeys ? &dds_serdata_ops_xcdr2 : &dds_serdata_ops_xcdr2_nokey;
      break;
    default:
      abort ();
  }

  /* Get the extensility of the outermost object in the type used for the topic. Note that the
     outermost type can have a different extensibility than nested types used in this type;
     the extensibility that is returned here is used to set the CDR encapsulation identifier,
     but nested types can use a different data representation format (not version) */
  enum dds_cdr_type_extensibility type_ext;
  if (!dds_stream_extensibility (desc->m_ops, &type_ext))
    return DDS_RETCODE_BAD_PARAMETER;

  ddsi_sertype_init (&st->c, desc->m_typename, &dds_sertype_ops_default, serdata_ops, (desc->m_nkeys == 0));
#ifdef DDS_HAS_SHM
  st->c.iox_size = desc->m_size;
#endif
  st->c.fixed_size = (st->c.fixed_size || (desc->m_flagset & DDS_TOPIC_FIXED_SIZE)) ? 1u : 0u;
  st->c.allowed_data_representation = desc->m_flagset & DDS_TOPIC_RESTRICT_DATA_REPRESENTATION ?
      desc->restrict_data_representation : DDS_DATA_REPRESENTATION_RESTRICT_DEFAULT;
  if (min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2)
    st->c.allowed_data_representation &= ~DDS_DATA_REPRESENTATION_FLAG_XCDR1;
  st->encoding_format = ddsi_sertype_extensibility_enc_format (type_ext);
  /* Store the encoding version used for writing data using this sertype. When reading data,
     the encoding version from the encapsulation header in the CDR is used */
  st->write_encoding_version = data_representation == DDS_DATA_REPRESENTATION_XCDR1 ? DDSI_RTPS_CDR_ENC_VERSION_1 : DDSI_RTPS_CDR_ENC_VERSION_2;
  st->serpool = domain->serpool;
  st->type.size = desc->m_size;
  st->type.align = desc->m_align;
  st->type.flagset = desc->m_flagset;
  st->type.keys.nkeys = desc->m_nkeys;
  st->type.keys.keys = ddsrt_malloc (st->type.keys.nkeys  * sizeof (*st->type.keys.keys));
  for (uint32_t i = 0; i < st->type.keys.nkeys; i++)
  {
    st->type.keys.keys[i].ops_offs = desc->m_keys[i].m_offset;
    st->type.keys.keys[i].idx = desc->m_keys[i].m_idx;
  }
  st->type.ops.nops = dds_stream_countops (desc->m_ops, desc->m_nkeys, desc->m_keys);
  st->type.ops.ops = ddsrt_memdup (desc->m_ops, st->type.ops.nops * sizeof (*st->type.ops.ops));

  if (min_xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2 && dds_stream_type_nesting_depth (desc->m_ops) > DDS_CDRSTREAM_MAX_NESTING_DEPTH)
  {
    ddsi_sertype_unref (&st->c);
    GVTRACE ("Serializer ops for type %s has unsupported nesting depth (max %u)\n", desc->m_typename, DDS_CDRSTREAM_MAX_NESTING_DEPTH);
    return DDS_RETCODE_BAD_PARAMETER;
  }

  if (desc->m_flagset & DDS_TOPIC_XTYPES_METADATA)
  {
    if (desc->type_information.sz == 0 || desc->type_information.data == NULL
      || desc->type_mapping.sz == 0 || desc->type_mapping.data == NULL)
    {
      ddsi_sertype_unref (&st->c);
      GVTRACE ("Flag DDS_TOPIC_XTYPES_METADATA set for type %s but topic descriptor does not contains type information\n", desc->m_typename);
      return DDS_RETCODE_BAD_PARAMETER;
    }
    st->typeinfo_ser.data = ddsrt_memdup (desc->type_information.data, desc->type_information.sz);
    st->typeinfo_ser.sz = desc->type_information.sz;
    st->typemap_ser.data = ddsrt_memdup (desc->type_mapping.data, desc->type_mapping.sz);
    st->typemap_ser.sz = desc->type_mapping.sz;
  }
  else
  {
    st->typeinfo_ser.data = NULL;
    st->typeinfo_ser.sz = 0;
    st->typemap_ser.data = NULL;
    st->typemap_ser.sz = 0;
  }

  st->type.opt_size_xcdr1 = (st->c.allowed_data_representation & DDS_DATA_REPRESENTATION_FLAG_XCDR1) ? dds_stream_check_optimize (&st->type, DDSI_RTPS_CDR_ENC_VERSION_1) : 0;
  if (st->type.opt_size_xcdr1 > 0)
    GVTRACE ("Marshalling XCDR1 for type: %s is %soptimised\n", st->c.type_name, st->type.opt_size_xcdr1 ? "" : "not ");

  st->type.opt_size_xcdr2 = (st->c.allowed_data_representation & DDS_DATA_REPRESENTATION_FLAG_XCDR2) ? dds_stream_check_optimize (&st->type, DDSI_RTPS_CDR_ENC_VERSION_2) : 0;
  if (st->type.opt_size_xcdr2 > 0)
    GVTRACE ("Marshalling XCDR2 for type: %s is %soptimised\n", st->c.type_name, st->type.opt_size_xcdr2 ? "" : "not ");

  return DDS_RETCODE_OK;
}
