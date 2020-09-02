/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
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
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "tree.h"
#include "hashid.h"
#include "idl/processor.h"

#define IDL_ANNOTATION_APPL_HASHID (1u<<0)
#define IDL_ANNOTATION_APPL_ID (1u<<1)
#define IDL_ANNOTATION_APPL_AUTOID (1u<<2)
#define IDL_ANNOTATION_APPL_EXTENSIBILITY (1u<<3)
#define IDL_ANNOTATION_APPL_FINAL (1u<<4)
#define IDL_ANNOTATION_APPL_APPENDABLE (1u<<5)
#define IDL_ANNOTATION_APPL_MUTABLE (1u<<6)
#define IDL_ANNOTATION_APPL_KEY (1u<<7)

static idl_retcode_t
annotate_hashid(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  const char *name = NULL;

  if (*seen & (IDL_ANNOTATION_APPL_ID | IDL_ANNOTATION_APPL_HASHID)) {
    idl_error(proc, idl_location(annotation),
      "@hashid conflicts with earlier @id or @hashid annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_MEMBER)) {
    idl_error(proc, idl_location(annotation),
      "@hashid can only be applied to struct member declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (idl_next(((idl_member_t*)node)->declarators)) {
    idl_error(proc, idl_location(annotation),
      "@hashid can only be applied to unique struct members declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (annotation->parameters) {
    if (!idl_is_masked(annotation->parameters, IDL_STRING_LITERAL)) {
      idl_error(proc, idl_location(annotation->parameters),
        "@hashid takes a string literal constant expression");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    name = ((idl_literal_t *)annotation->parameters)->value.str;
  }

  /* member ID must be computed from the member name if the annotation is used
     without any parameter or with the empty string as a parameter */
  if (!name || strcmp(name, "") == 0)
    name = ((idl_member_t *)node)->declarators->identifier;

  *seen |= IDL_ANNOTATION_APPL_HASHID;
  ((idl_member_t *)node)->node.mask |= IDL_ID;
  ((idl_member_t *)node)->id = idl_hashid(name);

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_id(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  uint64_t id;

  if (*seen & (IDL_ANNOTATION_APPL_ID | IDL_ANNOTATION_APPL_HASHID)) {
    idl_error(proc, idl_location(annotation),
      "@id conflicts with earlier @id or @hashid annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_MEMBER)) {
    idl_error(proc, idl_location(annotation),
      "@id can only be applied to struct member declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (idl_next(((idl_member_t*)node)->declarators)) {
    idl_error(proc, idl_location(annotation),
      "@id can only be applied to unique struct member declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(annotation->parameters, IDL_INTEGER_LITERAL)) {
    idl_error(proc, idl_location(annotation->parameters),
      "@id takes an integer literal constant expression");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  id = ((idl_literal_t *)annotation->parameters)->value.ullng;
  if (id > UINT32_MAX) {
    idl_error(proc, &node->location, "@id exceeds maximum");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *seen |= IDL_ANNOTATION_APPL_ID;
  ((idl_member_t *)node)->node.mask |= IDL_ID;
  ((idl_member_t *)node)->id = (uint32_t)id;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_autoid(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  idl_autoid_t autoid = IDL_AUTOID_SEQUENTIAL;
  const char *str = NULL;

  if (*seen & IDL_ANNOTATION_APPL_AUTOID) {
    idl_error(proc, idl_location(annotation),
      "@autoid conflicts with earlier @autoid annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_STRUCT)) {
    idl_error(proc, idl_location(annotation),
      "@autoid can only be applied to struct declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (idl_is_masked(annotation->parameters, IDL_STRING_LITERAL))
    str = (const char *)((idl_literal_t *)annotation->parameters)->value.str;
  else if (!annotation->parameters)
    str = "SEQUENTIAL";

  if (str && strcmp(str, "HASH") == 0) {
    autoid = IDL_AUTOID_HASH;
  } else if (str && strcmp(str, "SEQUENTIAL") == 0) {
    autoid = IDL_AUTOID_SEQUENTIAL;
  } else {
    idl_error(proc, idl_location(annotation),
      "@autoid requires an AutoidKind literal (%s or %s)",
      "HASH", "SEQUENTIAL");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *seen |= IDL_ANNOTATION_APPL_AUTOID;
  ((idl_struct_t*)node)->autoid = autoid;
  return IDL_RETCODE_OK;
}

static uint32_t extensibility_flags =
  IDL_ANNOTATION_APPL_EXTENSIBILITY |
  IDL_ANNOTATION_APPL_FINAL |
  IDL_ANNOTATION_APPL_APPENDABLE |
  IDL_ANNOTATION_APPL_MUTABLE;

static idl_retcode_t
annotate_extensibility(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  idl_extensibility_t extensibility = IDL_EXTENSIBILITY_FINAL;
  const char *str = NULL;

  if (*seen & extensibility_flags) {
    idl_error(proc, idl_location(annotation),
      "@extensibility conflicts with earlier extensibility annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_STRUCT)) {
    idl_error(proc, idl_location(annotation),
      "@extensibility can only be applied to struct declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(annotation->parameters, IDL_STRING_LITERAL)) {
    idl_error(proc, idl_location(annotation),
      "@extensibility takes an ExtensibilityKind literal (%s, %s or %s)",
      "FINAL", "APPENDABLE", "MUTABLE");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  str = ((idl_literal_t *)annotation->parameters)->value.str;
  if (str && strcmp(str, "FINAL") == 0) {
    extensibility = IDL_EXTENSIBILITY_FINAL;
  } else if (str && strcmp(str, "APPENDABLE") == 0) {
    extensibility = IDL_EXTENSIBILITY_APPENDABLE;
  } else if (str && strcmp(str, "MUTABLE") == 0) {
    extensibility = IDL_EXTENSIBILITY_MUTABLE;
  } else {
    idl_error(proc, idl_location(annotation->parameters),
      "@extensibility takes an ExtensibilityKind literal (%s, %s or %s)",
      "FINAL", "APPENDABLE", "MUTABLE");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *seen |= IDL_ANNOTATION_APPL_EXTENSIBILITY;
  ((idl_struct_t*)node)->extensibility = extensibility;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_final(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  if (*seen & extensibility_flags) {
    idl_error(proc, idl_location(annotation),
      "@final conflicts with earlier extensibility annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_STRUCT)) {
    idl_error(proc, idl_location(annotation),
      "@final can only be applied to struct declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (annotation->parameters) {
    idl_error(proc, idl_location(annotation->parameters),
      "@final does not take any parameters");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *seen |= IDL_ANNOTATION_APPL_FINAL;
  ((idl_struct_t*)node)->extensibility = IDL_EXTENSIBILITY_FINAL;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_appendable(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  if (*seen & extensibility_flags) {
    idl_error(proc, idl_location(annotation),
      "@appendable conflicts with earlier extensibility annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_STRUCT)) {
    idl_error(proc, idl_location(annotation),
      "@appendable can only be applied to struct declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (annotation->parameters) {
    idl_error(proc, idl_location(annotation->parameters),
      "@appendable does not take any parameters");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *seen |= IDL_ANNOTATION_APPL_APPENDABLE;
  ((idl_struct_t*)node)->extensibility = IDL_EXTENSIBILITY_APPENDABLE;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_mutable(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  if (*seen & extensibility_flags) {
    idl_error(proc, idl_location(annotation),
      "@mutable conflicts with earlier extensibility annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!idl_is_masked(node, IDL_STRUCT)) {
    idl_error(proc, idl_location(annotation),
      "@mutable can only be applied to struct declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (annotation->parameters) {
    idl_error(proc, idl_location(annotation->parameters),
      "@mutable does not take any parameters");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *seen |= IDL_ANNOTATION_APPL_MUTABLE;
  ((idl_struct_t*)node)->extensibility = IDL_EXTENSIBILITY_MUTABLE;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_key(
  idl_processor_t *proc,
  idl_node_t *node,
  idl_annotation_appl_t *annotation,
  uint32_t *seen)
{
  if (!idl_is_masked(node, IDL_MEMBER)) {
    idl_error(proc, &annotation->node.location,
      "@key can only be applied to struct member declarations");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  if (annotation) {
    if (!idl_is_masked(annotation->parameters, IDL_BOOLEAN_LITERAL)) {
      idl_error(proc, &annotation->node.location,
        "@key requires a boolean constant expression");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  *seen |= IDL_ANNOTATION_APPL_KEY;
  node->mask |= IDL_KEY;
  return IDL_RETCODE_OK;
}

static const struct {
  const char *name;
  idl_retcode_t(*annotate)(idl_processor_t *, idl_node_t *, idl_annotation_appl_t *, uint32_t *);
} builtin_annotations[] = {
  /* DDS-XTypes 1.3 (7.3.1.2.1.1) Member IDs */
  { "hashid", annotate_hashid },
  /* IDL 4.2 (8.3.1) Group of Annotations: General Purpose */
  { "id", annotate_id },
  { "autoid", annotate_autoid },
  { "extensibility", annotate_extensibility },
  { "final", annotate_final },
  { "appendable", annotate_appendable },
  { "mutable", annotate_mutable },
  /* IDL 4.2 (8.3.2) Group of Annotations: Data Modeling */
  { "key", annotate_key },
  { NULL, 0 }
};

idl_retcode_t
idl_annotate(
  idl_processor_t *proc,
  void *node,
  idl_annotation_appl_t *annotations)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  uint32_t seen = 0;

  assert(proc);
  assert(proc->flags & IDL_FLAG_ANNOTATIONS);
  assert(node);

  for (idl_annotation_appl_t *n = annotations; n && ret == IDL_RETCODE_OK; n = (idl_annotation_appl_t*)n->node.next) {
    unsigned i;
    for (i = 0; builtin_annotations[i].name; i++) {
      if (strcmp(n->scoped_name, builtin_annotations[i].name) == 0) {
        ret = builtin_annotations[i].annotate(proc, node, n, &seen);
        break;
      }
    }
  }

  return ret;
}
