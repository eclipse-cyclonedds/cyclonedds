// Copyright(c) 2021 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "idl/processor.h"
#include "idl/heap.h"
#include "symbol.h"

const idl_location_t *idl_location(const void *symbol)
{
  return symbol ? &((const idl_symbol_t *)symbol)->location : NULL;
}

void idl_delete_name(idl_name_t *name)
{
  if (name) {
    if (name->identifier)
      idl_free(name->identifier);
    idl_free(name);
  }
}

idl_retcode_t
idl_create_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  char *identifier,
  bool is_annotation,
  idl_name_t **namep)
{
  idl_name_t *name;

  (void)pstate;
  if (!(name = idl_malloc(sizeof(*name))))
    return IDL_RETCODE_NO_MEMORY;
  name->symbol.location = *location;
  name->identifier = identifier;
  name->is_annotation = is_annotation;
  *namep = name;
  return IDL_RETCODE_OK;
}

void idl_delete_scoped_name(idl_scoped_name_t *scoped_name)
{
  if (scoped_name) {
    idl_free(scoped_name->identifier);
    for (size_t i=0; i < scoped_name->length; i++)
      idl_delete_name(scoped_name->names[i]);
    idl_free(scoped_name->names);
    idl_free(scoped_name);
  }
}

idl_retcode_t
idl_create_scoped_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  bool absolute,
  idl_scoped_name_t **scoped_namep)
{
  idl_scoped_name_t *scoped_name;
  char *str;
  size_t len, off;
  const char *root = absolute ? "::" : "";

  if (!name)
    return IDL_RETCODE_SYNTAX_ERROR;

  (void)pstate;
  assert(name->identifier);
  if (!(scoped_name = idl_calloc(1, sizeof(*scoped_name))))
    goto err_alloc;
  if (!(scoped_name->names = idl_calloc(1, sizeof(idl_name_t *))))
    goto err_alloc;
  off = strlen(root);
  len = strlen(name->identifier);
  if (!(str = idl_malloc(off + len + 1)))
    goto err_alloc;
  memcpy(str, root, off);
  memcpy(str+off, name->identifier, len);
  str[off + len] = '\0';
  scoped_name->symbol.location.first = location->first;
  scoped_name->symbol.location.last = name->symbol.location.last;
  scoped_name->absolute = absolute;
  scoped_name->length = 1;
  scoped_name->names[0] = name;
  scoped_name->identifier = str;
  *scoped_namep = scoped_name;
  return IDL_RETCODE_OK;
err_alloc:
  if (scoped_name && scoped_name->names)
    idl_free(scoped_name->names);
  if (scoped_name)
    idl_free(scoped_name);
  return IDL_RETCODE_NO_MEMORY;
}

idl_retcode_t
idl_push_scoped_name(
  idl_pstate_t *pstate,
  idl_scoped_name_t *scoped_name,
  idl_name_t *name)
{
  char *str;
  size_t len, off;
  size_t size;
  static const char *sep = "::";
  idl_name_t **names;

  (void)pstate;
  assert(scoped_name);
  assert(scoped_name->length >= 1);
  assert(name);

  len = strlen(name->identifier);
  off = strlen(scoped_name->identifier);
  if (!(str = idl_malloc(off + strlen(sep) + len + 1)))
    goto err_alloc;
  memcpy(str, scoped_name->identifier, off);
  memcpy(str+off, sep, strlen(sep));
  off += strlen(sep);
  memcpy(str+off, name->identifier, len);
  str[off + len] = '\0';
  size = (scoped_name->length + 1) * sizeof(idl_name_t*);
  if (!(names = idl_realloc(scoped_name->names, size)))
    goto err_alloc;
  scoped_name->symbol.location.last = name->symbol.location.last;
  scoped_name->names = names;
  scoped_name->names[scoped_name->length++] = name;
  idl_free(scoped_name->identifier);
  scoped_name->identifier = str;
  return IDL_RETCODE_OK;
err_alloc:
  if (str) idl_free(str);
  return IDL_RETCODE_NO_MEMORY;
}

void idl_delete_field_name(idl_field_name_t *field_name)
{
  if (field_name) {
    if (field_name->identifier)
      idl_free(field_name->identifier);
    for (size_t i=0; i < field_name->length; i++)
      idl_delete_name(field_name->names[i]);
    idl_free(field_name->names);
    idl_free(field_name);
  }
}

idl_retcode_t
idl_create_field_name(
  idl_pstate_t *pstate,
  const idl_location_t *location,
  idl_name_t *name,
  idl_field_name_t **field_namep)
{
  char *str;
  size_t len;
  idl_field_name_t *field_name;

  (void)pstate;
  assert(name);
  assert(name->identifier);
  if (!(field_name = idl_calloc(1, sizeof(*field_name))))
    goto err_alloc;
  if (!(field_name->names = idl_calloc(1, sizeof(idl_name_t*))))
    goto err_alloc;
  len = strlen(name->identifier);
  if (!(str = idl_malloc(len + 1)))
    goto err_alloc;
  memcpy(str, name->identifier, len);
  str[len] = '\0';
  field_name->symbol.location = *location;
  field_name->symbol.location.last = name->symbol.location.last;
  field_name->length = 1;
  field_name->names[0] = name;
  field_name->identifier = str;
  *field_namep = field_name;
  return IDL_RETCODE_OK;
err_alloc:
  if (field_name && field_name->names)
    idl_free(field_name->names);
  if (field_name)
    idl_free(field_name);
  return IDL_RETCODE_NO_MEMORY;
}

idl_retcode_t
idl_push_field_name(
  idl_pstate_t *pstate,
  idl_field_name_t *field_name,
  idl_name_t *name)
{
  char *str;
  size_t len, off;
  size_t size;
  const char *sep = ".";
  idl_name_t **names;

  (void)pstate;
  assert(field_name);
  assert(field_name->length >= 1);
  assert(name);

  off = strlen(field_name->identifier);
  len = strlen(name->identifier);
  if (!(str = idl_malloc(off + strlen(sep) + len + 1)))
    goto err_alloc;
  memcpy(str, field_name->identifier, off);
  memcpy(str+off, sep, strlen(sep));
  off += strlen(sep);
  memcpy(str+off, name->identifier, len);
  str[off + len] = '\0';
  size = (field_name->length + 1) * sizeof(idl_name_t*);
  if (!(names = idl_realloc(field_name->names, size)))
    goto err_alloc;
  field_name->symbol.location.last = name->symbol.location.last;
  field_name->names = names;
  field_name->names[field_name->length++] = name;
  idl_free(field_name->identifier);
  field_name->identifier = str;
  return IDL_RETCODE_OK;
err_alloc:
  if (str) idl_free(str);
  return IDL_RETCODE_NO_MEMORY;
}
