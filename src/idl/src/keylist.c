/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "idl/misc.h"
#include "idl/string.h"
#include "keylist.h"

struct key_field {
  const idl_declarator_t *key_declarator; /* declarator node, used for finding an entry in the key_containers key_fields */
  char **parent_paths;    /* stores the paths to this key field, as string with dot separated field names, e.g. { "field1.f2.id", "field2.f2.id" } */
  size_t n_parent_paths;  /* number of paths in parent_paths */
};

struct key_container {
  const idl_node_t *node;
  struct key_field *key_fields;
  size_t n_key_fields;
};

static int cmp_parent_path(const void *a, const void *b)
{
  return strcmp(*(char **)a, *(char **)b);
}

static bool parent_path_sorted_list_equal(char **a, char **b, size_t len)
{
  for (size_t n = 0; n < len; n++)
    if (strcmp(a[n], b[n]))
      return false;
  return true;
}

static bool has_conflicting_keys(const struct key_container *key_containers, size_t n_key_containers, const idl_node_t **node)
{
  assert(node);
  for (size_t c = 0; c < n_key_containers; c++) {
    const struct key_container *cntr = &key_containers[c];

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
    for (size_t k = 0; k < cntr->n_key_fields; k++) {
      const struct key_field *fld = &cntr->key_fields[k];
      /* parent_paths == NULL && n_parent_paths == 0 happens for keys in the outer
         struct but qsort() requires a non-null pointer */
      if (fld->n_parent_paths > 1) {
        qsort(fld->parent_paths, fld->n_parent_paths,
          sizeof(*fld->parent_paths), cmp_parent_path);
      }
    }
    for (size_t k = 0; k < cntr->n_key_fields; k++) {
      const struct key_field *fld = &cntr->key_fields[k];
      for (size_t k1 = 0; k1 < cntr->n_key_fields; k1++) {
        const struct key_field *fld1 = &cntr->key_fields[k1];
        if (fld->n_parent_paths != fld1->n_parent_paths
          || !parent_path_sorted_list_equal(fld->parent_paths, fld1->parent_paths, fld->n_parent_paths)
        ) {
          *node = cntr->node;
          return true;
        }
      }
    }
  }
  return false;
}

static struct key_container *get_key_container(const idl_node_t *key_type_node, struct key_container **key_containers, size_t *n_key_containers)
{
  for (size_t n=0; n < *n_key_containers; n++) {
    if ((*key_containers)[n].node == key_type_node)
      return &(*key_containers)[n];
  }

  // not found => add it
  (*n_key_containers)++;
  struct key_container *tmp = realloc(*key_containers, *n_key_containers * sizeof(**key_containers));
  if (tmp == NULL) {
    free (*key_containers);
    *key_containers = NULL;
    return NULL;
  }
  *key_containers = tmp;
  (*key_containers)[*n_key_containers - 1] = (struct key_container) { key_type_node, NULL, 0 };
  return &(*key_containers)[*n_key_containers - 1];
}

static struct key_field *get_key_field(const idl_declarator_t *declarator, struct key_container *key_container)
{
  for (size_t n=0; n < key_container->n_key_fields; n++) {
    if (key_container->key_fields[n].key_declarator == declarator)
      return &key_container->key_fields[n];
  }

  // not found => add it
  key_container->n_key_fields++;
  struct key_field *tmp = realloc(key_container->key_fields, key_container->n_key_fields * sizeof(*key_container->key_fields));
  if (tmp == NULL) {
    free (key_container->key_fields);
    key_container->key_fields = NULL;
    return NULL;
  }
  key_container->key_fields = tmp;
  key_container->key_fields[key_container->n_key_fields - 1] = (struct key_field) { declarator, NULL, 0 };
  return &key_container->key_fields[key_container->n_key_fields - 1];
}

static idl_retcode_t add_parent_path(struct key_field *key_field, idl_field_name_t *key)
{
  assert(key->length);
  if (key->length == 1)
    return IDL_RETCODE_OK;
  key_field->n_parent_paths++;
  char **tmp;
  if (!(tmp = realloc(key_field->parent_paths, key_field->n_parent_paths * sizeof(*key_field->parent_paths))))
  {
    free (key_field->parent_paths);
    key_field->parent_paths = NULL;
    return IDL_RETCODE_NO_MEMORY;
  }
  key_field->parent_paths = tmp;

  size_t parent_path_len = strlen(key->identifier) - strlen(key->names[key->length - 1]->identifier) - 1;
  assert(parent_path_len > 0);
  if (!(key_field->parent_paths[key_field->n_parent_paths - 1] = malloc(parent_path_len + 1)))
    return IDL_RETCODE_NO_MEMORY;
  memcpy(key_field->parent_paths[key_field->n_parent_paths - 1], key->identifier, parent_path_len);
  key_field->parent_paths[key_field->n_parent_paths - 1][parent_path_len] = '\0';
  return IDL_RETCODE_OK;
}

static void key_containers_fini(struct key_container *key_containers, size_t n_key_containers)
{
  IDL_WARNING_MSVC_OFF (6001);
  if (!key_containers)
    return;
  for (size_t c = 0; c < n_key_containers; c++) {
    if (!key_containers[c].key_fields)
      continue;
    for (size_t f = 0; f < key_containers[c].n_key_fields; f++) {
      if (!key_containers[c].key_fields[f].parent_paths)
        continue;
      for (size_t p = 0; p < key_containers[c].key_fields[f].n_parent_paths; p++) {
        if (!key_containers[c].key_fields[f].parent_paths)
          continue;
        free(key_containers[c].key_fields[f].parent_paths[p]);
        key_containers[c].key_fields[f].parent_paths[p] = NULL;
      }
      free(key_containers[c].key_fields[f].parent_paths);
      key_containers[c].key_fields[f].parent_paths = NULL;
    }
    free(key_containers[c].key_fields);
    key_containers[c].key_fields = NULL;
  }
  free(key_containers);
  IDL_WARNING_MSVC_ON (6001);
}

static idl_retcode_t get_keylist_key_paths_struct_key(idl_pstate_t *pstate, idl_struct_t *struct_node, const idl_key_t *key_node, struct key_container **key_containers, size_t *n_key_containers)
{

  /* Create a shallow copy of the key names and unalias the identifier, so that
     this copy can be used to find fields for all parts of the key, by removing
     the last name-part in each iteration. */
  idl_field_name_t key_names = *key_node->field_name;
  if ((key_names.identifier = idl_strdup(key_node->field_name->identifier)) == NULL)
    return IDL_RETCODE_NO_MEMORY;

  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_scope_t *scope = struct_node->node.declaration->scope;
  assert(scope);
  for (uint32_t k = 0; ret == IDL_RETCODE_OK && k < key_node->field_name->length; k++)
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
    if (!(key_container = get_key_container(key_parent, key_containers, n_key_containers)))
      ret = IDL_RETCODE_NO_MEMORY;
    else if (!(key_field = get_key_field(declarator, key_container)))
      ret = IDL_RETCODE_NO_MEMORY;
    else
      ret = add_parent_path(key_field, &key_names);
    if (ret == IDL_RETCODE_OK)
    {
      if (key_names.length > 1)
        key_names.identifier[strlen(key_names.identifier) - strlen(key_names.names[key_names.length - 1]->identifier) - 1] = '\0';
      key_names.length--;
    }
  }
  assert(key_names.length == 0 || ret != IDL_RETCODE_OK);
  free(key_names.identifier);
  return ret;
}

static idl_retcode_t get_keylist_key_paths_struct(idl_pstate_t *pstate, idl_struct_t *struct_node, struct key_container **key_containers, size_t *n_key_containers)
{
  if (struct_node->keylist != NULL) {
    for (const idl_key_t *key_node = struct_node->keylist->keys; key_node; key_node = idl_next(key_node)) {
      idl_retcode_t ret;
      if ((ret = get_keylist_key_paths_struct_key(pstate, struct_node, key_node, key_containers, n_key_containers)) != IDL_RETCODE_OK)
        return ret;
    }
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t get_keylist_key_paths(idl_pstate_t *pstate, void *list, struct key_container **key_containers, size_t *n_key_containers)
{
  /* Build tree of key_containers, key fields and paths to these key fields */
  idl_retcode_t ret = IDL_RETCODE_OK;
  for ( ; ret == IDL_RETCODE_OK && list; list = idl_next(list)) {
    if (idl_mask(list) == IDL_MODULE) {
      idl_module_t *node = list;
      ret = get_keylist_key_paths(pstate, node->definitions, key_containers, n_key_containers);
    } else if (idl_mask(list) == IDL_STRUCT) {
      idl_struct_t *struct_node = list;
      ret = get_keylist_key_paths_struct(pstate, struct_node, key_containers, n_key_containers);
    }
  }
  return ret;
}

idl_retcode_t idl_validate_keylists(idl_pstate_t *pstate)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  struct key_container *key_containers = NULL;
  size_t nkc = 0;

  assert(pstate);
  if ((ret = get_keylist_key_paths(pstate, pstate->root, &key_containers, &nkc) != IDL_RETCODE_OK))
    goto err_create_key;

  /* Check for conflicting keys */
  if (nkc > 0) {
    const idl_node_t *conflicting_type;
    if (has_conflicting_keys(key_containers, nkc, &conflicting_type))
    {
      idl_error(pstate, idl_location(conflicting_type),
        "Conflicting keys in keylist directive for type '%s'", idl_identifier(conflicting_type));
      ret = IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

err_create_key:
  key_containers_fini(key_containers, nkc);
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
