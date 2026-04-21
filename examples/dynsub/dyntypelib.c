/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#define _CRT_SECURE_NO_WARNINGS // sscanf

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "dds/dds.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"

#include "domtree.h"
#include "dyntypelib.h"

dds_return_t dtl_set_error (struct dyntypelib_error *err, const struct elem *elem, const char *fmt, ...)
{
  int cnt = 0;
  if (elem)
  {
    cnt += snprintf (err->errmsg + cnt, sizeof (err->errmsg) - (size_t) cnt, "%s:%d: ", elem->file, elem->line);
    if (cnt < 0)
      return DDS_RETCODE_ERROR;
  }
  va_list ap;
  va_start (ap, fmt);
  vsnprintf (err->errmsg + cnt, sizeof (err->errmsg) - (size_t) cnt, fmt, ap);
  va_end (ap);
  return DDS_RETCODE_ERROR;
}

struct make_context {
  const char *file;
  dds_entity_t dp;
  struct dyntypelib *dtl;
};

static dds_return_t make_types (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err);

static dds_return_t make_module (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err)
{
  const char *name = getattr (elem, "name");
  if (name == NULL)
    return dtl_set_error (err, elem, "module is missing name\n");
  char *newns;
  dds_return_t rc = DDS_RETCODE_OK;
  ddsrt_asprintf (&newns, "%s::%s", ns, name);
  for (struct elem *e = elem->children; e; e = e->next)
    if ((rc = make_types (ctxt, e, newns, err)) != 0)
      break;
  ddsrt_free (newns);
  return rc;
}

static struct dyntype *lookup_type (const struct make_context *ctxt, const char *ns, const char *nbtype)
{
  struct dyntype *t;
  if (strncmp (nbtype, "::", 2) == 0)
  {
    //printf ("fqname lookup %s\n", nbtype);
    t = ddsrt_hh_lookup (ctxt->dtl->typelib, &(struct dyntype){ .name = (char *) nbtype });
  }
  else
  {
    char *nscopy = ddsrt_strdup (ns);
    char *colon = strrchr (nscopy, ':');
    char *fqnbtype;
    ddsrt_asprintf (&fqnbtype, "%s::%s", nscopy, nbtype);
    //printf ("name lookup %s\n", fqnbtype);
    while ((t = ddsrt_hh_lookup (ctxt->dtl->typelib, &(struct dyntype){ .name = fqnbtype })) == NULL && colon != NULL)
    {
      memmove (fqnbtype + (colon - 1 - nscopy), nbtype, strlen (nbtype) + 1);
      colon = (colon >= nscopy + 2) ? strrchr (colon - 2, ':') : NULL;
      //printf ("name lookup %s\n", fqnbtype);
    }
    ddsrt_free (fqnbtype);
    ddsrt_free (nscopy);
  }
  return t;
}

static dds_return_t register_type (const struct make_context *ctxt, const struct elem *elem, dds_dynamic_type_t *dtype, char *fqname, struct dyntypelib_error *err)
{
  struct ddsi_typeinfo *typeinfo = NULL;
  dds_return_t rc = dds_dynamic_type_register (dtype, &typeinfo);
  if (rc != DDS_RETCODE_OK)
    return dtl_set_error (err, elem, "dynamic_type_register %s failed: %s\n", fqname, dds_strretcode (rc));

  struct dyntype *t = ddsrt_malloc (sizeof (*t));
  t->name = fqname;
  t->dtype = dtype;
  t->typeinfo = typeinfo;

  DDS_XTypes_TypeInformation const * const xti = (const DDS_XTypes_TypeInformation *) typeinfo;
  dds_typeid_t const * const ti = (const dds_typeid_t *) &xti->complete.typeid_with_size.type_id;
  dds_typeobj_t *typeobj;
  if ((rc = dds_get_typeobj (ctxt->dp, ti, 0, &typeobj)) < 0)
    return dtl_set_error (err, elem, "dds_get_typeobj %s failed to get typeobj: %s\n", fqname, dds_strretcode (rc));
  t->typeobj = (DDS_XTypes_TypeObject *) typeobj;

  if (!ddsrt_hh_add (ctxt->dtl->typelib, t))
    return dtl_set_error (err, elem, "hh_add failed for %s\n", t->name);

  assert (xti->complete.typeid_with_size.type_id._d == DDS_XTypes_EK_COMPLETE);
  struct type_hashid_map *info = ddsrt_malloc (sizeof (*info));
  memcpy (info->id, &xti->complete.typeid_with_size.type_id._u.equivalence_hash, sizeof (info->id));
  info->typeobj = t->typeobj;
  info->lineno = 0;
  type_hashid_map_add (ctxt->dtl->typecache, info);

  //printf ("added %s\n", t->name);
  return DDS_RETCODE_OK;
}

static dds_return_t get_max_length (const struct elem *elem, const char *name, uint32_t def, uint32_t *max_length, struct dyntypelib_error *err)
{
  const char *str = getattr (elem, name);
  if (str == NULL)
    *max_length = def;
  else
  {
    int tmp, pos;
    if (sscanf (str, "%d%n", &tmp, &pos) != 1 || str[pos] != 0 || tmp < -1)
      return dtl_set_error (err, elem, "unexpected value for %s: %s\n", name, str);
    *max_length = (tmp <= 0) ? UINT32_MAX : (uint32_t) tmp;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t set_member_flag (dds_dynamic_type_t *dtype, const struct elem *m, const char *name, dds_return_t (*setter) (dds_dynamic_type_t *type, uint32_t member_id, bool is_must_understand), struct dyntypelib_error *err)
{
  const char *flagstr = getattr (m, name);
  if (flagstr != NULL)
  {
    bool flag = false;
    if (strcmp (flagstr, "true") == 0)
      flag = true;
    else if (strcmp (flagstr, "false") != 0)
      return dtl_set_error (err, m, "unsupported value for %s: %s\n", name, flagstr);
    dds_return_t rc;
    rc = setter (dtype, DDS_DYNAMIC_MEMBER_ID_AUTO, flag);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, m, "set_flag failed for %s: %s\n", name, dds_strretcode (rc));
  }
  return DDS_RETCODE_OK;
}

static dds_return_t get_try_construct (const struct elem *m, const char *name, enum dds_dynamic_type_try_construct *tc, struct dyntypelib_error *err)
{
  const char *tcstr = getattr (m, name);
  *tc = DDS_DYNAMIC_MEMBER_TRY_CONSTRUCT_DISCARD;
  if (tcstr == NULL || strcmp (tcstr, "discard") == 0)
    *tc = DDS_DYNAMIC_MEMBER_TRY_CONSTRUCT_DISCARD;
  else if (strcmp (tcstr, "") == 0 || strcmp (tcstr, "use_default") == 0)
    *tc = DDS_DYNAMIC_MEMBER_TRY_CONSTRUCT_USE_DEFAULT;
  else if (strcmp (tcstr, "trim") == 0)
    *tc = DDS_DYNAMIC_MEMBER_TRY_CONSTRUCT_TRIM;
  else
    return dtl_set_error (err, m, "unknown try_construct %s\n", tcstr);
  return DDS_RETCODE_OK;
}

static dds_return_t get_typespec (const struct make_context *ctxt, const struct elem *m, const char *ns, dds_dynamic_type_spec_t *typespec, struct dyntypelib_error *err)
{
  dds_return_t rc;
  const char *type = getattr (m, "type");
  if (type == NULL)
    return dtl_set_error (err, m, "type missing\n");

  dds_dynamic_type_spec_t mtspec;
  if (strcmp (type, "int8") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT8);
  else if (strcmp (type, "uint8") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_UINT8);
  else if (strcmp (type, "int16") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT16);
  else if (strcmp (type, "uint16") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_UINT16);
  else if (strcmp (type, "int32") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT32);
  else if (strcmp (type, "uint32") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_UINT32);
  else if (strcmp (type, "int64") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_INT64);
  else if (strcmp (type, "uint64") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_UINT64);
  else if (strcmp (type, "boolean") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_BOOLEAN);
  else if (strcmp (type, "float32") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_FLOAT32);
  else if (strcmp (type, "float64") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_FLOAT64);
  else if (strcmp (type, "float128") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_FLOAT128);
  else if (strcmp (type, "byte") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_BYTE);
  else if (strcmp (type, "char8") == 0)
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_CHAR8);
  else if (strcmp (type, "char16") == 0) // missing in testsuite, it seems
    mtspec = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_CHAR16);
  else if (strcmp (type, "string") == 0 || strcmp (type, "wstring") == 0)
  {
    dds_dynamic_type_descriptor_t desc = {
      .kind = (strcmp (type, "string") == 0) ? DDS_DYNAMIC_STRING8 : DDS_DYNAMIC_STRING16
    };
    uint32_t strmaxlength = 0;
    if ((rc = get_max_length (m, "stringMaxLength", UINT32_MAX, &strmaxlength, err)) != 0)
      return rc;
    if (strmaxlength != UINT32_MAX)
    {
      desc.bounds = &strmaxlength;
      desc.num_bounds = 1;
    }
    mtspec = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_create (ctxt->dp, desc));
  }
  else if (strcmp (type, "nonBasic") == 0)
  {
    const char *nbtype = getattr (m, "nonBasicTypeName");
    if (nbtype == NULL)
      return dtl_set_error (err, m, "non-basic type, but nonBasicTypeName missing\n");
    mtspec = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_ref (lookup_type (ctxt, ns, nbtype)->dtype));
  }
  else
  {
    return dtl_set_error (err, m, "unsupported type %s\n", type);
  }

  // anonymous sequence
  uint32_t seqmaxlength = 0;
  if ((rc = get_max_length (m, "sequenceMaxLength", 0, &seqmaxlength, err)) != 0)
    return rc;
  if (seqmaxlength != 0)
  {
    char *seqname;
    ddsrt_asprintf (&seqname, "dds_sequence_%s", type);
    for (char *c = seqname; *c; c++)
      *c = (*c == ':') ? '_': *c;
    dds_dynamic_type_t dseq = dds_dynamic_type_create (ctxt->dp, (dds_dynamic_type_descriptor_t) {
      .kind = DDS_DYNAMIC_SEQUENCE,
      .name = seqname,
      .element_type = mtspec,
      .bounds = (uint32_t[]) { seqmaxlength },
      .num_bounds = (seqmaxlength == UINT32_MAX) ? 0 : 1
    });
    // FIXME: spec doesn't define a way to specify try-construct for a sequence element, this is my own hack
    enum dds_dynamic_type_try_construct tc;
    if ((rc = get_try_construct (m, "elementTryConstruct", &tc, err)) != 0)
    {
      ddsrt_free (seqname);
      return rc;
    }
    rc = dds_dynamic_type_set_try_construct (&dseq, tc);
    ddsrt_free (seqname);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, m, "set_try_construct failed: %s\n", dds_strretcode (rc));
    mtspec = DDS_DYNAMIC_TYPE_SPEC (dseq);
  }

  // anonymous array (not sure it if sequence/array can be combined)
  const char *arydimstr = getattr (m, "arrayDimensions");
  if (arydimstr)
  {
#define MAXDIMS 10
    uint32_t ndims = 0, dims[MAXDIMS];
    char *aryname; // Should I bake this name?
    ddsrt_asprintf (&aryname, "dds_array_%s", type);
    for (char *c = aryname; *c; c++)
      *c = (*c == ':') ? '_': *c;
    while (*arydimstr)
    {
      char *endptr;
      dims[ndims++] = (uint32_t) strtoul (arydimstr, &endptr, 0);
      if (*endptr != 0 && *endptr != ',')
      {
        ddsrt_free (aryname);
        return dtl_set_error (err, m, "unsupported content in arrayDimensions\n");
      }
      arydimstr = endptr + (*endptr == ',');
    } while (*arydimstr);
    if (ndims == 0)
    {
      ddsrt_free (aryname);
      return dtl_set_error (err, m, "arrayDimensions is empty\n");
    }
    dds_dynamic_type_t dary = dds_dynamic_type_create (ctxt->dp, (dds_dynamic_type_descriptor_t) {
      .kind = DDS_DYNAMIC_ARRAY,
      .name = aryname,
      .element_type = mtspec,
      .bounds = dims,
      .num_bounds = ndims
    });
    ddsrt_free (aryname);
    mtspec = DDS_DYNAMIC_TYPE_SPEC (dary);
#undef MAXDIMS
  }

  *typespec = mtspec;
  return DDS_RETCODE_OK;
}

static dds_return_t add_member (const struct make_context *ctxt, struct dds_dynamic_type *dtype, const struct elem *m, const char *ns, uint32_t nlabels, const int32_t *labels, bool isdefault, struct dyntypelib_error *err)
{
  dds_return_t rc;
  const char *mname = getattr (m, "name");
  if (mname == NULL)
    return dtl_set_error (err, m, "name missing\n");

  dds_dynamic_type_spec_t mtspec;
  if ((rc = get_typespec (ctxt, m, ns, &mtspec, err)) != 0)
    return rc;
  uint32_t id = DDS_DYNAMIC_MEMBER_ID_AUTO;
  const char *idstr = getattr (m, "id");
  if (idstr)
  {
    int pos;
    if (sscanf (idstr, "%"SCNu32"%n", &id, &pos) != 1 || idstr[pos] != 0 || id >= 0x0fffffff)
      return dtl_set_error (err, m, "id attribute empty, contains non-digits or out-of-range\n");
  }

  rc = dds_dynamic_type_add_member (dtype, ((dds_dynamic_member_descriptor_t) {
    .name = mname,
    .id = id,
    .type = mtspec,
    .index = DDS_DYNAMIC_MEMBER_INDEX_END,
    .num_labels = nlabels,
    .labels = (int32_t *) labels,
    .default_label = isdefault
  }));
  if (rc != DDS_RETCODE_OK)
    return dtl_set_error (err, m, "add_member failed: %s\n", dds_strretcode (rc));

  const char *hashidstr = getattr (m, "hashid");
  if (hashidstr)
    rc = dds_dynamic_member_set_hashid (dtype, DDS_DYNAMIC_MEMBER_ID_AUTO, hashidstr);
  if (rc != DDS_RETCODE_OK)
    return dtl_set_error (err, m, "set_hashid failed: %s\n", dds_strretcode (rc));

  if ((rc = set_member_flag (dtype, m, "key", dds_dynamic_member_set_key, err)) != 0)
    return rc;
  if ((rc = set_member_flag (dtype, m, "mustUnderstand", dds_dynamic_member_set_must_understand, err)) != 0)
    return rc;

  enum dds_dynamic_type_try_construct tc;
  if ((rc = get_try_construct (m, "tryConstruct", &tc, err)) != 0)
    return rc;
  rc = dds_dynamic_member_set_try_construct (dtype, DDS_DYNAMIC_MEMBER_ID_AUTO, tc);
  if (rc != DDS_RETCODE_OK)
    return dtl_set_error (err, m, "set_try_construct failed: %s\n", dds_strretcode (rc));

  // not in test suite, they are just guesses:
  if ((rc = set_member_flag (dtype, m, "optional", dds_dynamic_member_set_optional, err)) != 0)
    return rc;
  if ((set_member_flag (dtype, m, "external", dds_dynamic_member_set_external, err)) != 0)
    return rc;
  return DDS_RETCODE_OK;
}

static dds_return_t add_struct_member (const struct make_context *ctxt, struct dds_dynamic_type *dtype, const struct elem *m, const char *ns, struct dyntypelib_error *err)
{
  return add_member (ctxt, dtype, m, ns, 0, NULL, false, err);
}

static dds_return_t set_extensibility (dds_dynamic_type_t *dtype, const struct elem *elem, struct dyntypelib_error *err)
{
  const char *ext = getattr (elem, "extensibility");
  if (ext)
  {
    dds_return_t rc;
    if (strcmp (ext, "final") == 0)
      rc = dds_dynamic_type_set_extensibility (dtype, DDS_DYNAMIC_TYPE_EXT_FINAL);
    else if (strcmp (ext, "appendable") == 0)
      rc = dds_dynamic_type_set_extensibility (dtype, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
    else if (strcmp (ext, "mutable") == 0)
      rc = dds_dynamic_type_set_extensibility (dtype, DDS_DYNAMIC_TYPE_EXT_MUTABLE);
    else
      return dtl_set_error (err, elem, "unknown extensibility %s\n", ext);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, elem, "set_extensibility failed: %s\n", dds_strretcode (rc));
  }
  return DDS_RETCODE_OK;
}

static dds_return_t set_autoid (dds_dynamic_type_t *dtype, const struct elem *elem, struct dyntypelib_error *err)
{
  const char *autoid = getattr (elem, "autoid");
  if (autoid)
  {
    dds_return_t rc;
    if (strcmp (autoid, "sequential") == 0)
      rc = dds_dynamic_type_set_autoid (dtype, DDS_DYNAMIC_TYPE_AUTOID_SEQUENTIAL);
    else if (strcmp (autoid, "hash") == 0)
      rc = dds_dynamic_type_set_autoid (dtype, DDS_DYNAMIC_TYPE_AUTOID_HASH);
    else
      return dtl_set_error (err, elem, "unknown autoid %s\n", autoid);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, elem, "set_autoid failed: %s\n", dds_strretcode (rc));
  }
  return DDS_RETCODE_OK;
}

static dds_return_t make_struct (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err)
{
  const char *name = getattr (elem, "name");
  if (name == NULL)
    return dtl_set_error (err, elem, "name missing\n");
  char *fqname;
  ddsrt_asprintf (&fqname, "%s::%s", ns, name);
  dds_dynamic_type_t *dstruct = ddsrt_malloc (sizeof (*dstruct));
  *dstruct = dds_dynamic_type_create (ctxt->dp, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_STRUCTURE, .name = fqname
  });
  dds_return_t rc;
  if ((rc = set_extensibility (dstruct, elem, err)) != 0)
    return rc;
  if ((rc = set_autoid (dstruct, elem, err)) != 0)
    return rc;
  for (const struct elem *m = elem->children; m; m = m->next)
    if ((rc = add_struct_member (ctxt, dstruct, m, ns, err)) != 0)
      return rc;
  return register_type (ctxt, elem, dstruct, fqname, err);
}

static dds_return_t make_union (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err)
{
  dds_return_t rc;
  const char *name = getattr (elem, "name");
  if (name == NULL)
    return dtl_set_error (err, elem, "name missing\n");
  char *fqname;
  ddsrt_asprintf (&fqname, "%s::%s", ns, name);

  // We require the discriminator type at the time of creating the union, so go look for it
  const struct elem *discriminator_elem = NULL;
  bool discriminator_is_key = false;
  dds_dynamic_type_spec_t discts = DDS_DYNAMIC_TYPE_SPEC_PRIM (DDS_DYNAMIC_BOOLEAN);
  {
    bool discts_set = false;
    for (const struct elem *c = elem->children; c; c = c->next)
    {
      if (strcmp (c->name, "discriminator") != 0)
        continue;
      discriminator_elem = c;
      if ((rc = get_typespec (ctxt, c, ns, &discts, err)) != 0)
        return rc;
      discts_set = true;
      const char *keystr = getattr (c, "key");
      if (keystr) {
        if (strcmp (keystr, "false") == 0)
          discriminator_is_key = false;
        else if (strcmp (keystr, "true") == 0)
          discriminator_is_key = true;
        else
          return dtl_set_error (err, elem, "disciminator key attribute has invalid value: %s\n", keystr);
      }
      break;
    }
    if (!discts_set)
      return dtl_set_error (err, elem, "discriminator missing\n");
  }

  dds_dynamic_type_t *dunion = ddsrt_malloc (sizeof (*dunion));
  *dunion = dds_dynamic_type_create (ctxt->dp, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION, .name = fqname, .discriminator_type = discts
  });

  if ((rc = set_extensibility (dunion, elem, err)) != 0)
    return rc;
  if ((rc = set_autoid (dunion, elem, err)) != 0)
    return rc;
  if ((rc = dds_dynamic_member_set_key (dunion, 0, discriminator_is_key)) != 0)
    return dtl_set_error (err, elem, "failed to set key attribute for union type: %s\n", dds_strretcode (rc));

  for (const struct elem *c = elem->children; c; c = c->next)
  {
    if (strcmp (c->name, "case") != 0)
      continue; // presumably "discriminator"

    uint32_t nlabs = 0;
    int32_t labs[10];
    bool isdefault = false;

    for (const struct elem *m = c->children; m; m = m->next)
    {
      if (strcmp (m->name, "caseDiscriminator") != 0) // presumably "member"
        continue;
      const char *valstr = getattr (m, "value");
      if (valstr == NULL)
        return dtl_set_error (err, m, "no value in caseDiscriminator\n");
      if (strcmp (valstr, "default") == 0)
        isdefault = true;
      else if (*valstr == 0)
        return dtl_set_error (err, m, "empty value in caseDiscriminator\n");
      else if (nlabs == sizeof (labs) / sizeof (labs[0]))
        return dtl_set_error (err, m, "too many labels\n");
      else
      {
        char *endptr;
        labs[nlabs++] = (int32_t) strtol (valstr, &endptr, 0);
        if (*endptr)
        {
          if (strcmp(getattr(discriminator_elem, "type"), "nonBasic") != 0)
            return dtl_set_error (err, m, "junk at end of value\n");
          struct dyntype* d_enum = lookup_type(ctxt, ns, getattr(discriminator_elem, "nonBasicTypeName"));
          if (d_enum->typeobj->_u.complete._d != DDS_XTypes_TK_ENUM)
            return dtl_set_error (err, m, "Non eunm type for literal values\n");
          const DDS_XTypes_CompleteEnumeratedType *c_enum = &d_enum->typeobj->_u.complete._u.enumerated_type;
          bool enum_contains_value = false;
          for (uint32_t i = 0; i < c_enum->literal_seq._length; i++)
          {
            if (strcmp(valstr, c_enum->literal_seq._buffer[i].detail.name) != 0)
              continue;
            enum_contains_value = true;
            labs[nlabs - 1] = c_enum->literal_seq._buffer[i].common.value;
            break;
          }
          if (!enum_contains_value)
            return dtl_set_error (err, m, "Enum does not contain value\n");
        }
      }
    }

    const struct elem *m;
    for (m = c->children; m; m = m->next)
      if (strcmp (m->name, "member") == 0)
        break;
    if (m == NULL)
      return dtl_set_error (err, c, "member missing in case\n");
    if ((rc = add_member (ctxt, dunion, m, ns, nlabs, labs, isdefault, err)) != 0)
      return rc;
  }

  return register_type (ctxt, elem, dunion, fqname, err);
}

static dds_return_t make_enum (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err)
{
  dds_return_t rc;
  const char *name = getattr (elem, "name");
  if (name == NULL)
    return dtl_set_error (err, elem, "name missing\n");
  char *fqname;
  ddsrt_asprintf (&fqname, "%s::%s", ns, name);
  dds_dynamic_type_t *denum = ddsrt_malloc (sizeof (*denum));
  *denum = dds_dynamic_type_create (ctxt->dp, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_ENUMERATION, .name = fqname
  });

  if ((rc = set_extensibility (denum, elem, err)) != 0)
    return rc;

  const char *bitboundstr = getattr (elem, "bitBound");
  if (bitboundstr != NULL)
  {
    uint16_t bitbound;
    int pos;
    if (sscanf (bitboundstr, "%"SCNu16"%n", &bitbound, &pos) != 1 || bitboundstr[pos] != 0)
      return dtl_set_error (err, elem, "invalid bitbound: %s\n", bitboundstr);
    rc = dds_dynamic_type_set_bit_bound (denum, bitbound);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, elem, "set_bit_bound failed: %s\n", dds_strretcode (rc));
  }

  for (const struct elem *m = elem->children; m; m = m->next)
  {
    if (strcmp (m->name, "enumerator") != 0)
      return dtl_set_error (err, m, "expected \"enumerator\", got \"%s\"\n", m->name);

    const char *mname = getattr (m, "name");
    if (mname == NULL)
      return dtl_set_error (err, m, "name missing\n");
    const char *valuestr = getattr (m, "value");
    if (valuestr == NULL)
      return dtl_set_error (err, m, "value missing\n");
    int32_t value;
    int pos;
    if (sscanf (valuestr, "%"SCNd32"%n", &value, &pos) != 1 || valuestr[pos] != 0)
      return dtl_set_error (err, m, "value not a plain integer %s\n", valuestr);

    rc = dds_dynamic_type_add_enum_literal (denum, mname, DDS_DYNAMIC_ENUM_LITERAL_VALUE(value), false);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, m, "add_enum_literal failed: %s\n",  dds_strretcode (rc));
  }

  return register_type (ctxt, elem, denum, fqname, err);
}

static dds_return_t make_bitmask (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err)
{
  dds_return_t rc;
  const char *name = getattr (elem, "name");
  if (name == NULL)
    return dtl_set_error (err, elem, "name missing\n");
  char *fqname;
  ddsrt_asprintf (&fqname, "%s::%s", ns, name);
  dds_dynamic_type_t *dbitmask = ddsrt_malloc (sizeof (*dbitmask));
  *dbitmask = dds_dynamic_type_create (ctxt->dp, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_BITMASK, .name = fqname
  });

  if ((rc = set_extensibility (dbitmask, elem, err)) != 0)
    return rc;

  const char *bitboundstr = getattr (elem, "bitBound");
  if (bitboundstr != NULL)
  {
    uint16_t bitbound;
    int pos;
    if (sscanf (bitboundstr, "%"SCNu16"%n", &bitbound, &pos) != 1 || bitboundstr[pos] != 0)
      return dtl_set_error (err, elem, "invalid bitbound: %s\n", bitboundstr);
    rc = dds_dynamic_type_set_bit_bound (dbitmask, bitbound);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, elem, "set_bit_bound failed: %s\n", dds_strretcode (rc));
  }

  for (const struct elem *m = elem->children; m; m = m->next)
  {
    if (strcmp (m->name, "flag") != 0)
      return dtl_set_error (err, m, "expected \"flag\", got \"%s\"\n", m->name);

    const char *mname = getattr (m, "name");
    if (mname == NULL)
      return dtl_set_error (err, m, "name missing\n");
    const char *valuestr = getattr (m, "position"); // XSD says "position"
    if (valuestr == NULL)
      valuestr = getattr (m, "value"); // interop test uses "value" ...
    if (valuestr == NULL)
      return dtl_set_error (err, m, "value missing\n");
    int32_t value;
    int pos;
    if (sscanf (valuestr, "%"SCNd32"%n", &value, &pos) != 1 || valuestr[pos] != 0)
      return dtl_set_error (err, m, "value not a plain integer %s\n", valuestr);

    rc = dds_dynamic_type_add_bitmask_field (dbitmask, mname, (uint16_t) value);
    if (rc != DDS_RETCODE_OK)
      return dtl_set_error (err, m, "add_enum_literal failed: %s\n", dds_strretcode (rc));
  }

  return register_type (ctxt, elem, dbitmask, fqname, err);
}

static dds_return_t make_types (const struct make_context *ctxt, const struct elem *elem, const char *ns, struct dyntypelib_error *err)
{
  if (strcmp (elem->name, "module") == 0)
    return make_module (ctxt, elem, ns, err);
  else if (strcmp (elem->name, "struct") == 0)
    return make_struct (ctxt, elem, ns, err);
  else if (strcmp (elem->name, "union") == 0)
    return make_union (ctxt, elem, ns, err);
  else if (strcmp (elem->name, "enum") == 0)
    return make_enum (ctxt, elem, ns, err);
  else if (strcmp (elem->name, "bitmask") == 0)
    return make_bitmask (ctxt, elem, ns, err);
  else
    return dtl_set_error (err, elem, "unrecognized element %s\n", elem->name);
}

static uint32_t namehash (const void *va)
{
  const struct dyntype *a = va;
  ddsrt_md5_state_t st;
  ddsrt_md5_init (&st);
  ddsrt_md5_append (&st, (const ddsrt_md5_byte_t *) a->name, (unsigned) (strlen (a->name) + 1));
  ddsrt_md5_byte_t digest[16];
  ddsrt_md5_finish (&st, digest);
  uint32_t hash;
  memcpy (&hash, digest, sizeof (hash));
  return hash;
}

static bool nameequal (const void *va, const void *vb)
{
  const struct dyntype *a = va;
  const struct dyntype *b = vb;
  return strcmp (a->name, b->name) == 0;
}

struct dyntypelib *dtl_new (dds_entity_t dp)
{
  struct dyntypelib *dtl = ddsrt_malloc (sizeof (*dtl));
  dtl->dp = dp;
  dtl->print_types = false;
  dtl->typelib = ddsrt_hh_new (32, namehash, nameequal);
  dtl->typecache = type_cache_new ();
  ppc_init (&dtl->ppc);
  return dtl;
}

void dtl_set_print_types (struct dyntypelib *dtl, bool print_types)
{
  dtl->print_types = print_types;
}

dds_return_t dtl_add_xml_type_library (struct dyntypelib *dtl, const char *xml_type_lib, struct dyntypelib_error *err)
{
  struct elem *root = domtree_from_file (xml_type_lib);
  if (root == NULL)
  {
    dtl_set_error (err, NULL, "failed to load %s", xml_type_lib);
    return DDS_RETCODE_BAD_PARAMETER;
  }

  //domtree_print (root);

  if (strcmp (root->name, "dds") != 0 || root->children == NULL || strcmp (root->children->name, "types") != 0)
  {
    // FIXME: free domtree "root"
    dtl_set_error (err, NULL, "expected /dds/types");
    return DDS_RETCODE_BAD_PARAMETER;
  }

  struct make_context ctxt = {
    .file = xml_type_lib,
    .dp = dtl->dp,
    .dtl = dtl
  };
  return make_types (&ctxt, root->children->children, "", err);
}

dds_return_t dtl_add_typeid (struct dyntypelib *dtl, const dds_typeinfo_t *typeinfo, const DDS_XTypes_TypeObject **typeobj, struct dyntypelib_error *err)
{
  const DDS_XTypes_TypeObject *to;
  if ((to = load_type_with_deps (dtl->typecache, dtl->dp, typeinfo, dtl->print_types ? &dtl->ppc : NULL)) == NULL)
  {
    dtl_set_error (err, NULL, "failed to load type with dependencies");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  if (*typeobj)
    *typeobj = to;
  return DDS_RETCODE_OK;
}

struct dyntype *dtl_lookup_typename (struct dyntypelib *dtl, const char *name)
{
  if (*name == ':') // fq name: hash lookup
    return ddsrt_hh_lookup (dtl->typelib, &(struct dyntype){ .name = (char *) name });
  else // non-fq name: pick the shortest match (breaking ties arbitrarily)
  {
    struct ddsrt_hh_iter it;
    size_t matchlen = SIZE_MAX;
    for (struct dyntype *t = ddsrt_hh_iter_first (dtl->typelib, &it); t; t = ddsrt_hh_iter_next (&it))
    {
      size_t arglen = strlen (name);
      size_t len = strlen (t->name);
      if (len < matchlen
          && len >= arglen + 2
          && strncmp (t->name + len - arglen - 2, "::", 2) == 0
          && strcmp (t->name + len - arglen, name) == 0)
      {
        return t;
      }
    }
  }
  return NULL;
}

void dtl_free (struct dyntypelib *dtl)
{
  type_cache_free (dtl->typecache);

  struct ddsrt_hh_iter it;
  for (struct dyntype *t = ddsrt_hh_iter_first (dtl->typelib, &it); t; t = ddsrt_hh_iter_next (&it))
  {
    //printf ("free %s\n", t->name);
    dds_free_typeobj ((dds_typeobj_t *) t->typeobj);
    dds_dynamic_type_unref (t->dtype);
    dds_free_typeinfo (t->typeinfo);
    ddsrt_free (t->dtype);
    ddsrt_free (t->name);
    ddsrt_free (t);
  }
  ddsrt_hh_free (dtl->typelib);
  ddsrt_free (dtl);
}
