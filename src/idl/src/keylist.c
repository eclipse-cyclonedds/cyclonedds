// Copyright(c) 2021 ZettaScale Technology and others
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
#include "idl/heap.h"
#include "idl/misc.h"
#include "idl/string.h"
#include "idl/vector.h"
#include "keylist.h"

struct key_field {
  const idl_declarator_t *key_declarator; /* declarator node, used for finding an entry in the key_containers key_fields */
  idl_boxed_vector_t parent_paths; /* stores the paths to this key field, as string with dot separated field names, e.g. { "field1.f2.id", "field2.f2.id" } */
};

struct key_container {
  const idl_node_t *node;
  idl_boxed_vector_t key_fields;
};

struct key_container_iter {
  idl_boxed_vector_iter_t x;
};

struct key_containers {
  idl_boxed_vector_t key_containers;
};

struct key_containers_iter {
  idl_boxed_vector_iter_t x;
};

/* ======= new/free/iterators/... wrappers for the above ======== */

static void *boxed_vector_init_with_failure_action (idl_boxed_vector_t *v, void (*failure_action) (void *c), void *c)
{
  if (idl_boxed_vector_init (v))
    return c;
  failure_action (c);
  return NULL;
}

static void *boxed_vector_append_with_failure_action (idl_boxed_vector_t *v, void *x, void (*failure_action) (void *x))
{
  if (idl_boxed_vector_append (v, x))
    return x;
  else
  {
    failure_action (x);
    return NULL;
  }
}

static struct key_field *key_field_new (const idl_declarator_t *key_declarator)
{
  struct key_field *kf;
  if ((kf = idl_malloc (sizeof (*kf))) == NULL)
    return NULL;
  kf->key_declarator = key_declarator;
  return boxed_vector_init_with_failure_action (&kf->parent_paths, idl_free, kf);
}

static void key_field_free (struct key_field *kf) {
  idl_boxed_vector_fini (&kf->parent_paths, idl_free);
  idl_free (kf);
}
static void key_field_free_wrapper (void *vkf) {
  key_field_free (vkf);
}

static bool key_field_append (struct key_field *kf, char *parent_path)
{
  return idl_boxed_vector_append (&kf->parent_paths, parent_path);
}

static struct key_container *key_container_new (const idl_node_t *node)
{
  struct key_container *kc;
  if ((kc = idl_malloc (sizeof (*kc))) == NULL)
    return NULL;
  kc->node = node;
  return boxed_vector_init_with_failure_action (&kc->key_fields, idl_free, kc);
}

static void key_container_free (struct key_container *kc) {
  idl_boxed_vector_fini (&kc->key_fields, key_field_free_wrapper);
  idl_free (kc);
}
static void key_container_free_wrapper (void *vkc) {
  key_container_free (vkc);
}

static struct key_field *key_container_append (struct key_container *kc, const idl_declarator_t *declarator)
{
  struct key_field *kf;
  if ((kf = key_field_new (declarator)) == NULL)
    return NULL;
  return boxed_vector_append_with_failure_action (&kc->key_fields, kf, key_field_free_wrapper);
}

static const struct key_field *key_container_first_c (const struct key_container *kc, struct key_container_iter *it) {
  return idl_boxed_vector_first_c (&kc->key_fields, &it->x);
}
static const struct key_field *key_container_next_c (struct key_container_iter *it) {
  return idl_boxed_vector_next_c (&it->x);
}
static struct key_field *key_container_first (struct key_container *kc, struct key_container_iter *it) {
  return idl_boxed_vector_first (&kc->key_fields, &it->x);
}
static struct key_field *key_container_next (struct key_container_iter *it) {
  return idl_boxed_vector_next (&it->x);
}

static struct key_containers *key_containers_new (void)
{
  struct key_containers *kcs;
  if ((kcs = idl_malloc (sizeof (*kcs))) == NULL)
    return NULL;
  return boxed_vector_init_with_failure_action (&kcs->key_containers, idl_free, kcs);
}

static void key_containers_free (struct key_containers *kcs)
{
  idl_boxed_vector_fini (&kcs->key_containers, key_container_free_wrapper);
  idl_free (kcs);
}

static struct key_container *key_containers_append (struct key_containers *kcs, const idl_node_t *key_type_node)
{
  struct key_container *kc;
  if ((kc = key_container_new (key_type_node)) == NULL)
    return NULL;
  return boxed_vector_append_with_failure_action (&kcs->key_containers, kc, key_container_free_wrapper);
}

static const struct key_container *key_containers_first_c (const struct key_containers *kcs, struct key_containers_iter *it) {
  return idl_boxed_vector_first_c (&kcs->key_containers, &it->x);
}
static const struct key_container *key_containers_next_c (struct key_containers_iter *it) {
  return idl_boxed_vector_next_c (&it->x);
}
static struct key_container *key_containers_first (struct key_containers *kcs, struct key_containers_iter *it) {
  return idl_boxed_vector_first (&kcs->key_containers, &it->x);
}
static struct key_container *key_containers_next (struct key_containers_iter *it) {
  return idl_boxed_vector_next (&it->x);
}

/* ======= end of new/free/iterators/... wrappers ======== */

static int cmp_parent_path (const void *a, const void *b)
{
  return strcmp (a, b);
}

static void sort_parent_paths (struct key_containers *key_containers)
{
  struct key_containers_iter it;
  for (struct key_container *cntr = key_containers_first (key_containers, &it); cntr; cntr = key_containers_next (&it))
  {
    struct key_container_iter kcit;
    for (struct key_field *fld = key_container_first (cntr, &kcit); fld; fld = key_container_next (&kcit))
      idl_boxed_vector_sort (&fld->parent_paths, cmp_parent_path);
  }
}

static bool check_for_conflicting_keys(struct key_containers *key_containers, const idl_node_t **node)
{
  assert (node);
  sort_parent_paths (key_containers);
  struct key_containers_iter it;
  for (const struct key_container *cntr = key_containers_first_c (key_containers, &it); cntr; cntr = key_containers_next_c (&it))
  {
    /* For all keys in a key_container (struct), check if the set of parent nodes is
       equal. The parent path of a key is the list of fields that form the key,
       except the key field itself (the last field).
       For example in this keylist:
          #pragma keylist my_type f1.a, f1.b, f2.c.a
       the parent paths for the key fields are:
          f1, f1, f2.c
       In case field c has the same type 'inner_type' as f1, the paths to key field
       inner_type.a are: f1 and f2.c. For key field inner_type.b the parent path is
       only f1. So the parent paths for alls keys in inner_type are not equal, and
       therefore the keylist has conflicting keys (and cannot be represented with
       @key annotations). */
    struct key_container_iter kcit;
    for (const struct key_field *fld = key_container_first_c (cntr, &kcit); fld; fld = key_container_next_c (&kcit))
    {
      struct key_container_iter kcit1;
      for (const struct key_field *fld1 = key_container_first_c (cntr, &kcit1); fld1; fld1 = key_container_next_c (&kcit1))
      {
        idl_boxed_vector_iter_t ppit, ppit1;
        const char *pp = idl_boxed_vector_first_c (&fld->parent_paths, &ppit);
        const char *pp1 = idl_boxed_vector_first_c (&fld1->parent_paths, &ppit1);
        while (pp && pp1 && strcmp (pp, pp1) == 0)
        {
          pp = idl_boxed_vector_next (&ppit);
          pp1 = idl_boxed_vector_next (&ppit1);
        }
        if (pp || pp1)
        {
          *node = cntr->node;
          return true;
        }
      }
    }
  }
  return false;
}

static struct key_container *get_key_container (const idl_node_t *key_type_node, struct key_containers *key_containers)
{
  struct key_containers_iter it;
  for (struct key_container *kc = key_containers_first (key_containers, &it); kc; kc = key_containers_next (&it))
    if (kc->node == key_type_node)
      return kc;
  return key_containers_append (key_containers, key_type_node);
}

static struct key_field *get_key_field (const idl_declarator_t *declarator, struct key_container *key_container)
{
  struct key_container_iter it;
  for (struct key_field *kf = key_container_first (key_container, &it); kf; kf = key_container_next (&it))
    if (kf->key_declarator == declarator)
      return kf;
  return key_container_append (key_container, declarator);
}

static bool add_parent_path (struct key_field *key_field, idl_field_name_t *key)
{
  assert (key->length);
  if (key->length == 1)
    return true;

  size_t parent_path_len = strlen(key->identifier) - strlen(key->names[key->length - 1]->identifier) - 1;
  assert (parent_path_len > 0);
  char *pp = idl_malloc (parent_path_len + 1);
  if (pp == NULL)
    return false;
  memcpy (pp, key->identifier, parent_path_len);
  pp[parent_path_len] = '\0';
  return key_field_append (key_field, pp);
}

static idl_retcode_t get_keylist_key_paths_struct_key(idl_pstate_t *pstate, idl_struct_t *struct_node, const idl_key_t *key_node, struct key_containers *key_containers)
{
  /* Create a shallow copy of the key names and unalias the identifier, so that
     this copy can be used to find fields for all parts of the key, by removing
     the last name-part in each iteration. */
  idl_field_name_t key_names = *key_node->field_name;
  if ((key_names.identifier = idl_strdup(key_node->field_name->identifier)) == NULL)
    return IDL_RETCODE_NO_MEMORY;

  idl_scope_t *scope = struct_node->node.declaration->scope;
  assert(scope);
  for (uint32_t k = 0; k < key_node->field_name->length; k++)
  {
    const idl_declaration_t *declaration = idl_find_field_name(pstate, scope, &key_names, 0u);
    assert(declaration && declaration->node);
    const idl_declarator_t *declarator = (const idl_declarator_t *)declaration->node;
    /* Find the struct node that contains this key field */
    const idl_node_t *key_parent = &declarator->node;
    /* Keylist can only be used for struct types, not for keys in unions,
       so traverse up the tree until a struct is found */
    do {
      key_parent = idl_parent(key_parent);
      assert(key_parent);
    } while (!idl_is_struct(key_parent));

    struct key_container *key_container;
    struct key_field *key_field;
    if (!(key_container = get_key_container(key_parent, key_containers)))
      break;
    else if (!(key_field = get_key_field(declarator, key_container)))
      break;
    else if (!add_parent_path(key_field, &key_names))
      break;
    else
    {
      if (key_names.length > 1)
        key_names.identifier[strlen(key_names.identifier) - strlen(key_names.names[key_names.length - 1]->identifier) - 1] = '\0';
      key_names.length--;
    }
  }
  idl_free(key_names.identifier);
  return (key_names.length == 0) ? IDL_RETCODE_OK : IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t get_keylist_key_paths_struct(idl_pstate_t *pstate, idl_struct_t *struct_node, struct key_containers *key_containers)
{
  if (struct_node->keylist != NULL) {
    for (const idl_key_t *key_node = struct_node->keylist->keys; key_node; key_node = idl_next(key_node)) {
      idl_retcode_t ret;
      if ((ret = get_keylist_key_paths_struct_key(pstate, struct_node, key_node, key_containers)) != IDL_RETCODE_OK)
        return ret;
    }
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t get_keylist_key_paths(idl_pstate_t *pstate, void *list, struct key_containers *key_containers)
{
  /* Build tree of key_containers, key fields and paths to these key fields */
  idl_retcode_t ret = IDL_RETCODE_OK;
  for ( ; ret == IDL_RETCODE_OK && list; list = idl_next(list)) {
    if (idl_mask(list) == IDL_MODULE) {
      idl_module_t *node = list;
      ret = get_keylist_key_paths(pstate, node->definitions, key_containers);
    } else if (idl_mask(list) == IDL_STRUCT) {
      idl_struct_t *struct_node = list;
      ret = get_keylist_key_paths_struct(pstate, struct_node, key_containers);
    }
  }
  return ret;
}

idl_retcode_t idl_validate_keylists(idl_pstate_t *pstate)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  struct key_containers *key_containers;
  if ((key_containers = key_containers_new ()) == NULL)
    return IDL_RETCODE_NO_MEMORY;

  assert(pstate);
  if ((ret = get_keylist_key_paths (pstate, pstate->root, key_containers) != IDL_RETCODE_OK))
    goto err_create_key;

  /* Check for conflicting keys */
  const idl_node_t *conflicting_type;
  if (check_for_conflicting_keys (key_containers, &conflicting_type))
  {
    idl_error(pstate, idl_location(conflicting_type),
              "Conflicting keys in keylist directive for type '%s'", idl_identifier(conflicting_type));
    ret = IDL_RETCODE_SEMANTIC_ERROR;
  }

err_create_key:
  key_containers_free (key_containers);
  return ret;
}

void idl_set_keylist_key_flags(idl_pstate_t *pstate, void *list)
{
  assert(pstate);

  for ( ; list; list = idl_next(list)) {
    if (idl_mask(list) == IDL_MODULE) {
      idl_module_t *node = list;
      idl_set_keylist_key_flags(pstate, node->definitions);
    } else if (idl_mask(list) == IDL_STRUCT) {
      idl_struct_t *struct_node = list;
      if (struct_node->keylist) {
        idl_scope_t *scope = struct_node->node.declaration->scope;
        for (idl_key_t *key_node = struct_node->keylist->keys; key_node; key_node = idl_next(key_node)) {
          /* Create a shallow copy of the key name parts, so that the length
             can be decreased and all members in the path of the key have the key
             flag set to true. For the loop use the length from key_node->field_name,
             not key_names.length which is decreased in the loop. */
          idl_field_name_t key_names = *key_node->field_name;
          for (uint32_t k = 0; k < key_node->field_name->length; k++) {
            const idl_declaration_t *declaration = idl_find_field_name(pstate, scope, &key_names, 0u);
            assert(declaration);
            const idl_declarator_t *declarator = (const idl_declarator_t *)declaration->node;
            assert(declarator);
            idl_member_t *m = idl_parent(&declarator->node);
            assert(idl_is_member(m));
            m->key.value = true;
            m->key.annotation = NULL;
            key_names.length--;
          }
        }
      }
    }
  }
}
