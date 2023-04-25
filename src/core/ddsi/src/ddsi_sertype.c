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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_freelist.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__plist_generic.h"
#include "ddsi__serdata_pserop.h"
#include "dds/cdr/dds_cdrstream.h"

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

static void ddsi_sertype_unref_locked (struct ddsi_domaingv * const gv, struct ddsi_sertype *sertype)
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
  tp->allowed_data_representation = DDS_DATA_REPRESENTATION_RESTRICT_DEFAULT;
  tp->base_sertype = NULL;
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

uint16_t ddsi_sertype_get_native_enc_identifier (uint32_t enc_version, uint32_t enc_format)
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
    case DDSI_RTPS_CDR_ENC_VERSION_1:
      if (enc_format == DDSI_RTPS_CDR_ENC_FORMAT_PL)
        return CONCAT(DDSI_RTPS_PL_CDR, SUFFIX);
      return CONCAT(DDSI_RTPS_CDR, SUFFIX);
    case DDSI_RTPS_CDR_ENC_VERSION_2:
      if (enc_format == DDSI_RTPS_CDR_ENC_FORMAT_PL)
        return CONCAT(DDSI_RTPS_PL_CDR2, SUFFIX);
      if (enc_format == DDSI_RTPS_CDR_ENC_FORMAT_DELIMITED)
        return CONCAT(DDSI_RTPS_D_CDR2, SUFFIX);
      return CONCAT(DDSI_RTPS_CDR2, SUFFIX);
    default:
      abort (); /* unsupported */
  }
#undef SUFFIX
#undef CONCAT
#undef CONCAT_
}

uint16_t ddsi_sertype_extensibility_enc_format (enum dds_cdr_type_extensibility type_extensibility)
{
  switch (type_extensibility)
  {
    case DDS_CDR_TYPE_EXT_FINAL:
      return DDSI_RTPS_CDR_ENC_FORMAT_PLAIN;
    case DDS_CDR_TYPE_EXT_APPENDABLE:
      return DDSI_RTPS_CDR_ENC_FORMAT_DELIMITED;
    case DDS_CDR_TYPE_EXT_MUTABLE:
      return DDSI_RTPS_CDR_ENC_FORMAT_PL;
    default:
      abort ();
  }
}

uint32_t ddsi_sertype_enc_id_xcdr_version (uint16_t cdr_identifier)
{
  switch (cdr_identifier)
  {
    case DDSI_RTPS_CDR_LE: case DDSI_RTPS_CDR_BE:
      return DDSI_RTPS_CDR_ENC_VERSION_1;
    case DDSI_RTPS_CDR2_LE: case DDSI_RTPS_CDR2_BE:
    case DDSI_RTPS_D_CDR2_LE: case DDSI_RTPS_D_CDR2_BE:
    case DDSI_RTPS_PL_CDR2_LE: case DDSI_RTPS_PL_CDR2_BE:
      return DDSI_RTPS_CDR_ENC_VERSION_2;
    default:
      return DDSI_RTPS_CDR_ENC_VERSION_UNDEF;
  }
}

uint32_t ddsi_sertype_enc_id_enc_format (uint16_t cdr_identifier)
{
  switch (cdr_identifier)
  {
    case DDSI_RTPS_CDR_LE: case DDSI_RTPS_CDR_BE:
    case DDSI_RTPS_CDR2_LE: case DDSI_RTPS_CDR2_BE:
      return DDSI_RTPS_CDR_ENC_FORMAT_PLAIN;
    case DDSI_RTPS_D_CDR2_LE: case DDSI_RTPS_D_CDR2_BE:
      return DDSI_RTPS_CDR_ENC_FORMAT_DELIMITED;
    case DDSI_RTPS_PL_CDR2_LE: case DDSI_RTPS_PL_CDR2_BE:
      return DDSI_RTPS_CDR_ENC_FORMAT_PL;
    default:
      abort ();
  }
}

DDS_EXPORT extern inline void ddsi_sertype_free (struct ddsi_sertype *tp);
DDS_EXPORT extern inline void ddsi_sertype_zero_samples (const struct ddsi_sertype *tp, void *samples, size_t count);
DDS_EXPORT extern inline void ddsi_sertype_realloc_samples (void **ptrs, const struct ddsi_sertype *tp, void *old, size_t oldcount, size_t count);
DDS_EXPORT extern inline void ddsi_sertype_free_samples (const struct ddsi_sertype *tp, void **ptrs, size_t count, dds_free_op_t op);
DDS_EXPORT extern inline void ddsi_sertype_zero_sample (const struct ddsi_sertype *tp, void *sample);
DDS_EXPORT extern inline void *ddsi_sertype_alloc_sample (const struct ddsi_sertype *tp);
DDS_EXPORT extern inline void ddsi_sertype_free_sample (const struct ddsi_sertype *tp, void *sample, dds_free_op_t op);
DDS_EXPORT extern inline ddsi_typeid_t * ddsi_sertype_typeid (const struct ddsi_sertype *tp, ddsi_typeid_kind_t kind);
DDS_EXPORT extern inline ddsi_typemap_t * ddsi_sertype_typemap (const struct ddsi_sertype *tp);
DDS_EXPORT extern inline ddsi_typeinfo_t * ddsi_sertype_typeinfo (const struct ddsi_sertype *tp);
DDS_EXPORT extern inline struct ddsi_sertype * ddsi_sertype_derive_sertype (const struct ddsi_sertype *base_sertype, dds_data_representation_id_t data_representation, dds_type_consistency_enforcement_qospolicy_t tce_qos);

extern inline size_t ddsi_sertype_get_serialized_size(const struct ddsi_sertype *tp, const void *sample);
extern inline bool ddsi_sertype_serialize_into(const struct ddsi_sertype *tp, const void *sample, void *dst_buffer, size_t dst_size);
