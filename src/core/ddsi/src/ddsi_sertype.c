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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_freelist.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"
#include "dds/ddsi/ddsi_domaingv.h"

#ifndef _WIN32
void ddsi_sertype_v0 (struct ddsi_sertype_v0 *dummy)
{
  (void) dummy;
}
#endif

bool ddsi_sertype_equal (const struct ddsi_sertype *a, const struct ddsi_sertype *b)
{
  if (strcmp (a->type_name, b->type_name) != 0)
    return false;
  if (a->serdata_basehash != b->serdata_basehash)
    return false;
  if (a->ops != b->ops)
    return false;
  if (a->serdata_ops != b->serdata_ops)
    return false;
  if (a->typekind_no_key != b->typekind_no_key)
    return false;
  return a->ops->equal (a, b);
}

uint32_t ddsi_sertype_hash (const struct ddsi_sertype *a)
{
  uint32_t h;
  h = ddsrt_mh3 (a->type_name, strlen (a->type_name), a->serdata_basehash);
  h ^= a->serdata_basehash ^ (uint32_t) a->typekind_no_key;
  return h ^ a->ops->hash (a);
}

struct ddsi_sertype *ddsi_sertype_ref (const struct ddsi_sertype *sertype_const)
{
  struct ddsi_sertype *sertype = (struct ddsi_sertype *) sertype_const;
  ddsrt_atomic_inc32 (&sertype->flags_refc);
  return sertype;
}

struct ddsi_sertype *ddsi_sertype_lookup_locked (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype_template)
{
  struct ddsi_sertype *sertype = ddsrt_hh_lookup (gv->sertypes, sertype_template);
#ifndef NDEBUG
  if (sertype != NULL)
    assert ((ddsrt_atomic_ld32 (&sertype->flags_refc) & DDSI_SERTYPE_REFC_MASK) > 0);
#endif
  return sertype ? ddsi_sertype_ref (sertype) : NULL;
}

void ddsi_sertype_register_locked (struct ddsi_domaingv *gv, struct ddsi_sertype *sertype)
{
  assert (!(ddsrt_atomic_ld32 (&sertype->flags_refc) & DDSI_SERTYPE_REGISTERING));
  assert (ddsrt_atomic_ldvoidp (&sertype->gv) == NULL);

  uint32_t refc, refc1;
  do {
    refc = ddsrt_atomic_ld32 (&sertype->flags_refc);
    refc1 = (refc | DDSI_SERTYPE_REGISTERING) + 1;
  } while (!ddsrt_atomic_cas32 (&sertype->flags_refc, refc, refc1));
  ddsrt_atomic_fence_stst ();
  ddsrt_atomic_stvoidp (&sertype->gv, gv);
  ddsrt_atomic_fence_stst ();
  ddsrt_atomic_or32 (&sertype->flags_refc, DDSI_SERTYPE_REGISTERED);
  ddsrt_hh_add_absent (gv->sertypes, sertype);
}

void ddsi_sertype_unref_locked (struct ddsi_domaingv * const gv, struct ddsi_sertype *sertype)
{
  const uint32_t flags_refc1 = ddsrt_atomic_dec32_nv (&sertype->flags_refc);
  assert (!(flags_refc1 & DDSI_SERTYPE_REGISTERED) || gv == ddsrt_atomic_ldvoidp (&sertype->gv));
  if ((flags_refc1 & DDSI_SERTYPE_REFC_MASK) == 0)
  {
    if (sertype->base_sertype)
    {
      ddsi_sertype_unref_locked (gv, (struct ddsi_sertype *) sertype->base_sertype);
      ddsrt_free (sertype);
    }
    else
    {
      if (flags_refc1 & DDSI_SERTYPE_REGISTERED)
        ddsrt_hh_remove_present (gv->sertypes, sertype);
      ddsi_sertype_free (sertype);
    }
  }
}

static void ddsi_sertype_unref_registered_unlocked (struct ddsi_sertype *sertype)
{
  // last reference & registered: load-load barrier to ensure loading a valid gv
  ddsrt_atomic_fence_ldld ();
  struct ddsi_domaingv * const gv = ddsrt_atomic_ldvoidp (&sertype->gv);
  assert (gv != NULL);
  ddsrt_mutex_lock (&gv->sertypes_lock);
  ddsi_sertype_unref_locked (gv, sertype);
  ddsrt_mutex_unlock (&gv->sertypes_lock);
}

void ddsi_sertype_unref (struct ddsi_sertype *sertype)
{
  uint32_t flags_refc, flags_refc1;
  do {
    flags_refc = ddsrt_atomic_ld32 (&sertype->flags_refc);
    assert ((flags_refc & DDSI_SERTYPE_REFC_MASK) >= 1);
    flags_refc1 = flags_refc - 1;
    if ((flags_refc & DDSI_SERTYPE_REGISTERING) && !(flags_refc & DDSI_SERTYPE_REGISTERED))
    {
      // while registering, there's another thread holding a reference so refcount must necessarily still be larger than 1
      assert ((flags_refc1 & DDSI_SERTYPE_REFC_MASK) >= 1);
    }
    if (flags_refc & DDSI_SERTYPE_REGISTERED)
    {
      // registered sertypes refcounts manipulated while holding gv->sertypes_lock
      // so that refcount -> 0 can be done atomically with removing it from gv->sertypes
      //
      // it can only transition to REGISTERED once, then it stays that way until it is
      // dropped entirely
      ddsi_sertype_unref_registered_unlocked (sertype);
      return;
    }
  } while (!ddsrt_atomic_cas32 (&sertype->flags_refc, flags_refc, flags_refc1));

  if ((flags_refc1 & DDSI_SERTYPE_REFC_MASK) == 0)
  {
    /* If base_type is set this, is a derived sertype, which is a shallow copy of its base
       sertype. So don't free anything for the derived sertype, just unref the base sertype */
    if (sertype->base_sertype)
    {
      ddsi_sertype_unref ((struct ddsi_sertype *) sertype->base_sertype);
      ddsrt_free (sertype);
    }
    else
    {
      assert (!(flags_refc1 & DDSI_SERTYPE_REGISTERING));
      ddsi_sertype_free (sertype);
    }
  }
}

struct sertype_ser
{
  char *type_name;
  bool typekind_no_key;
};

const enum pserop sertype_ser_ops[] = { XS, Xb, XSTOP };

bool ddsi_sertype_serialize (const struct ddsi_sertype *tp, size_t *dst_sz, unsigned char **dst_buf)
{
  struct sertype_ser d = { tp->type_name, tp->typekind_no_key };
  size_t dst_pos = 0;

  if (!tp->ops->serialized_size || !tp->ops->serialize)
    return false;
  *dst_sz = 0;
  plist_ser_generic_size_embeddable (dst_sz, &d, 0, sertype_ser_ops);
  tp->ops->serialized_size (tp, dst_sz);
  *dst_buf = ddsrt_malloc (*dst_sz);
  if (plist_ser_generic_embeddable ((char *) *dst_buf, &dst_pos, &d, 0, sertype_ser_ops, DDSRT_BOSEL_LE) < 0) // xtypes spec (7.3.4.5) requires LE encoding for type serialization
    return false;
  if (!tp->ops->serialize (tp, &dst_pos, *dst_buf))
    return false;
  assert (dst_pos == *dst_sz);
  return true;
}

bool ddsi_sertype_deserialize (struct ddsi_domaingv *gv, struct ddsi_sertype *tp, const struct ddsi_sertype_ops *sertype_ops, size_t sz, unsigned char *serdata)
{
  struct sertype_ser d;
  size_t srcoff = 0;
  if (!sertype_ops->deserialize)
    return false;
  DDSRT_WARNING_MSVC_OFF(6326)
  if (plist_deser_generic_srcoff (&d, serdata, sz, &srcoff, DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN, sertype_ser_ops) < 0)
    return false;
  DDSRT_WARNING_MSVC_ON(6326)
  ddsrt_atomic_st32 (&tp->flags_refc, 1);
  tp->ops = sertype_ops;
  tp->type_name = ddsrt_strdup (d.type_name);
  tp->typekind_no_key = d.typekind_no_key;
  tp->min_xcdrv = 0u;
  if (!tp->ops->deserialize (gv, tp, sz, serdata, &srcoff))
  {
    ddsrt_free (tp->type_name);
    return false;
  }
  tp->serdata_basehash = ddsi_sertype_compute_serdata_basehash (tp->serdata_ops);
  ddsrt_atomic_stvoidp (&tp->gv, NULL);
  return true;
}

void ddsi_sertype_init_flags (struct ddsi_sertype *tp, const char *type_name, const struct ddsi_sertype_ops *sertype_ops, const struct ddsi_serdata_ops *serdata_ops, uint32_t flags)
{
  // only one version for now
  assert (sertype_ops->version == ddsi_sertype_v0);
  assert ((flags & ~(uint32_t)DDSI_SERTYPE_FLAG_MASK) == 0);

  ddsrt_atomic_st32 (&tp->flags_refc, 1);
  tp->type_name = ddsrt_strdup (type_name);
  tp->ops = sertype_ops;
  tp->serdata_ops = serdata_ops;
  tp->serdata_basehash = ddsi_sertype_compute_serdata_basehash (tp->serdata_ops);
  tp->typekind_no_key = (flags & DDSI_SERTYPE_FLAG_TOPICKIND_NO_KEY) ? 1u : 0u;
  tp->request_keyhash = (flags & DDSI_SERTYPE_FLAG_REQUEST_KEYHASH) ? 1u : 0u;
  tp->fixed_size = (flags & DDSI_SERTYPE_FLAG_FIXED_SIZE) ? 1u : 0u;
  tp->min_xcdrv = CDR_ENC_VERSION_1;
  tp->base_sertype = NULL;
  tp->wrapped_sertopic = NULL;
#ifdef DDS_HAS_SHM
  tp->iox_size = 0;
#endif
  ddsrt_atomic_stvoidp (&tp->gv, NULL);
}

void ddsi_sertype_init (struct ddsi_sertype *tp, const char *type_name, const struct ddsi_sertype_ops *sertype_ops, const struct ddsi_serdata_ops *serdata_ops, bool typekind_no_key)
{
  ddsi_sertype_init_flags (tp, type_name, sertype_ops, serdata_ops, typekind_no_key ? DDSI_SERTYPE_FLAG_TOPICKIND_NO_KEY : 0);
}

void ddsi_sertype_fini (struct ddsi_sertype *tp)
{
  assert ((ddsrt_atomic_ld32 (&tp->flags_refc) & DDSI_SERTYPE_REFC_MASK) == 0);
  ddsrt_free (tp->type_name);
}

uint32_t ddsi_sertype_compute_serdata_basehash (const struct ddsi_serdata_ops *ops)
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

uint16_t ddsi_sertype_get_native_encoding_identifier (uint32_t enc_version, uint32_t enc_format)
{
#define CONCAT_(a,b) (a ## b)
#define CONCAT(id,suffix) CONCAT_(id,suffix)

#if (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN)
#define SUFFIX _LE
#else
#define SUFFIX _BE
#endif

  switch (enc_version)
  {
    case CDR_ENC_VERSION_1:
      if (enc_format == CDR_ENC_FORMAT_PL)
        return CONCAT(PL_CDR, SUFFIX);
      return CONCAT(CDR, SUFFIX);
    case CDR_ENC_VERSION_2:
      if (enc_format == CDR_ENC_FORMAT_PL)
        return CONCAT(PL_CDR2, SUFFIX);
      if (enc_format == CDR_ENC_FORMAT_DELIMITED)
        return CONCAT(D_CDR2, SUFFIX);
      return CONCAT(CDR2, SUFFIX);
    default:
      abort (); /* unsupported */
  }
#undef SUFFIX
#undef CONCAT
#undef CONCAT_
}

uint16_t ddsi_sertype_get_encoding_format (enum ddsi_sertype_extensibility type_extensibility)
{
  switch (type_extensibility)
  {
    case DDSI_SERTYPE_EXT_FINAL:
      return CDR_ENC_FORMAT_PLAIN;
    case DDSI_SERTYPE_EXT_APPENDABLE:
      return CDR_ENC_FORMAT_DELIMITED;
    case DDSI_SERTYPE_EXT_MUTABLE:
      return CDR_ENC_FORMAT_PL;
    default:
      abort ();
  }
}

uint32_t get_xcdr_version (uint16_t cdr_identifier)
{
  switch (cdr_identifier)
  {
    case CDR_LE: case CDR_BE:
      return CDR_ENC_VERSION_1;
    case CDR2_LE: case CDR2_BE:
    case D_CDR2_LE: case D_CDR2_BE:
    case PL_CDR2_LE: case PL_CDR2_BE:
      return CDR_ENC_VERSION_2;
    default:
      return CDR_ENC_VERSION_UNDEF;
  }
}

static bool sertype_is_default (const struct ddsi_sertype *sertype)
{
  return (sertype->serdata_ops == &ddsi_serdata_ops_cdr
      || sertype->serdata_ops == &ddsi_serdata_ops_cdr_nokey
      || sertype->serdata_ops == &ddsi_serdata_ops_xcdr2
      || sertype->serdata_ops == &ddsi_serdata_ops_xcdr2_nokey);
}

struct ddsi_sertype *ddsi_sertype_data_representation (const struct ddsi_sertype *base_sertype, dds_data_representation_id_t data_representation)
{
  struct ddsi_sertype_default *derived_sertype = NULL;
  if (sertype_is_default (base_sertype))
  {
    const struct ddsi_serdata_ops *required_ops;
    if (data_representation == DDS_DATA_REPRESENTATION_XCDR1)
      required_ops = base_sertype->typekind_no_key ? &ddsi_serdata_ops_cdr_nokey : &ddsi_serdata_ops_cdr;
    else if (data_representation == DDS_DATA_REPRESENTATION_XCDR2)
      required_ops = base_sertype->typekind_no_key ? &ddsi_serdata_ops_xcdr2_nokey : &ddsi_serdata_ops_xcdr2;
    else
      abort ();

    if (base_sertype->serdata_ops != required_ops)
    {
      derived_sertype = (struct ddsi_sertype_default *) ddsi_sertype_derive_sertype (base_sertype);
      derived_sertype->c.serdata_ops = required_ops;
      derived_sertype->encoding_version = data_representation == DDS_DATA_REPRESENTATION_XCDR1 ? CDR_ENC_VERSION_1 : CDR_ENC_VERSION_2;
    }
  }
  return derived_sertype != NULL ? (struct ddsi_sertype *) derived_sertype : (struct ddsi_sertype *) base_sertype;
}

DDS_EXPORT extern inline void ddsi_sertype_free (struct ddsi_sertype *tp);
DDS_EXPORT extern inline void ddsi_sertype_zero_samples (const struct ddsi_sertype *tp, void *samples, size_t count);
DDS_EXPORT extern inline void ddsi_sertype_realloc_samples (void **ptrs, const struct ddsi_sertype *tp, void *old, size_t oldcount, size_t count);
DDS_EXPORT extern inline void ddsi_sertype_free_samples (const struct ddsi_sertype *tp, void **ptrs, size_t count, dds_free_op_t op);
DDS_EXPORT extern inline void ddsi_sertype_zero_sample (const struct ddsi_sertype *tp, void *sample);
DDS_EXPORT extern inline void *ddsi_sertype_alloc_sample (const struct ddsi_sertype *tp);
DDS_EXPORT extern inline void ddsi_sertype_free_sample (const struct ddsi_sertype *tp, void *sample, dds_free_op_t op);
DDS_EXPORT extern inline bool ddsi_sertype_typeid_hash (const struct ddsi_sertype *tp, unsigned char *buf);
DDS_EXPORT extern inline bool ddsi_sertype_assignable_from (const struct ddsi_sertype *type_a, const struct ddsi_sertype *type_b);
DDS_EXPORT extern inline struct ddsi_sertype * ddsi_sertype_derive_sertype (const struct ddsi_sertype *base_sertype);

DDS_EXPORT extern inline size_t ddsi_sertype_get_serialized_size(const struct ddsi_sertype *tp, const void *sample);
DDS_EXPORT extern inline bool ddsi_sertype_serialize_into(const struct ddsi_sertype *tp, const void *sample, void *dst_buffer, size_t dst_size);
