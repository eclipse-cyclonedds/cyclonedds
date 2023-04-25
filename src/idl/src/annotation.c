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
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include "idl/processor.h"
#include "idl/string.h"
#include "annotation.h"
#include "expression.h"
#include "hashid.h"
#include "fieldid.h"
#include "tree.h"

static idl_retcode_t
annotate_id(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_literal_t *literal;
  idl_declarator_t *decl = NULL;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  literal = (idl_literal_t *)annotation_appl->parameters->const_expr;
  assert(idl_type(literal) == IDL_ULONG);

  if (idl_is_member(node)) {
    decl = ((idl_member_t *)node)->declarators;
  } else if (idl_is_case(node)) {
    decl = ((idl_case_t*)node)->declarator;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@id cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  assert(decl);

  if (idl_next(decl)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@id cannot be applied to multiple declarators simultaneously");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if ((literal->value.uint32 & ~IDL_FIELDID_MASK) != 0) {
    idl_error(pstate, idl_location(annotation_appl),
      "@id '0x%"PRIx32"' is out of valid range", literal->value.uint32);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (decl->id.annotation) {
    idl_error(pstate, idl_location(annotation_appl),
      "@id conflicts with earlier annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  decl->id.annotation = annotation_appl;
  decl->id.value = (literal->value.uint32 & IDL_FIELDID_MASK);

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_hashid(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const char *name = NULL;
  idl_declarator_t *decl = NULL;

  assert(annotation_appl);
  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_type(literal) == IDL_STRING);
    name = literal->value.str;
  }

  if (idl_is_member(node)) {
    decl = ((idl_member_t *)node)->declarators;
  } else if (idl_is_case(node)) {
    decl = ((idl_case_t*)node)->declarator;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@hashid cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  assert(decl);

  if (idl_next(decl)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@hashid cannot be applied to multiple declarators simultaneously");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (decl->id.annotation) {
    idl_error(pstate, idl_location(annotation_appl),
      "@hashid conflicts with earlier annotation");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (!name)
    name = decl->name->identifier;

  decl->id.annotation = annotation_appl;
  decl->id.value = idl_hashid(name);

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_autoid(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_autoid_t autoid = IDL_HASH;
  const char *enumerator;

  assert(annotation_appl);
  if (annotation_appl->parameters) {
    enumerator = idl_identifier(annotation_appl->parameters->const_expr);
    if (strcmp(enumerator, "HASH") == 0) {
      autoid = IDL_HASH;
    } else {
      assert (strcmp(enumerator, "SEQUENTIAL") == 0);
      autoid = IDL_SEQUENTIAL;
    }
  }

  if (idl_is_struct(node)) {
    idl_struct_t *str = (idl_struct_t *)node;
    str->autoid.annotation = annotation_appl;
    str->autoid.value = autoid;
  } else if (idl_is_union(node)) {
    idl_union_t *u = (idl_union_t *)node;
    u->autoid.annotation = annotation_appl;
    u->autoid.value = autoid;
  } else if (idl_is_module(node)) {
    idl_module_t *mod = (idl_module_t *)node;
    mod->autoid.annotation = annotation_appl;
    mod->autoid.value = autoid;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@autoid cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_optional(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const idl_const_expr_t *const_expr;
  idl_member_t *mem = (idl_member_t*)node;
  bool value = true;

  assert(pstate);
  assert(annotation_appl);

  if (annotation_appl->parameters) {
    const_expr = annotation_appl->parameters->const_expr;
    assert(idl_type(const_expr) == IDL_BOOL);
    value = ((const idl_literal_t*)const_expr)->value.bln;
  }

  if (!idl_is_member(node)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@optional can only be assigned to members");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (value && mem->key.value) {
    idl_error(pstate, idl_location(annotation_appl),
      "@optional cannot be set to true on key members");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (value && mem->value.annotation) {
    idl_error(pstate, idl_location(annotation_appl),
      "@optional cannot be set to true on members with explicit default values");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  mem->optional.annotation = annotation_appl;
  mem->optional.value = value;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_value(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_type_t type;
  const idl_const_expr_t *const_expr;
  uint32_t value;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  const_expr = annotation_appl->parameters->const_expr;

  if (!idl_is_enumerator(node)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@value cannot be applied to '%s' elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  type = idl_type(const_expr);
  if (type == IDL_OCTET || (type & IDL_INTEGER_TYPE)) {
    idl_intval_t intval = idl_intval(const_expr);

    if ((intval.type & IDL_UNSIGNED) && intval.value.ullng > UINT32_MAX) {
      idl_error(pstate, idl_location(annotation_appl),
        "@value(%" PRIu64 ") cannot be applied to '%s' element",
        intval.value.ullng, idl_construct(node));
      return IDL_RETCODE_SEMANTIC_ERROR;
    } else if (!(intval.type & IDL_UNSIGNED) && intval.value.llng < 0) {
      idl_error(pstate, idl_location(annotation_appl),
        "@value(%" PRId64 ") cannot be applied to '%s' element",
        intval.value.llng, idl_construct(node));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }

    value = (uint32_t)intval.value.ullng;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@value with %s cannot be applied to '%s' element",
      idl_construct(const_expr), idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  ((idl_enumerator_t *)node)->value.annotation = annotation_appl;
  ((idl_enumerator_t *)node)->value.value = value;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_extensibility(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const idl_annotation_appl_t **annotationp = NULL;
  idl_extensibility_t *extensibilityp = NULL, extensibility = IDL_FINAL;
  const char *annotation;

  assert(annotation_appl);
  annotation = idl_identifier(annotation_appl);
  assert(annotation);
  if (idl_strcasecmp(annotation, "extensibility") == 0) {
    assert(annotation_appl->parameters);
    assert(annotation_appl->parameters->const_expr);
    annotation = idl_identifier(annotation_appl->parameters->const_expr);
    assert(annotation);
  }

  if (idl_is_struct(node)) {
    annotationp = &((idl_struct_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_struct_t *)node)->extensibility.value;
  } else if (idl_is_union(node)) {
    annotationp = &((idl_union_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_union_t *)node)->extensibility.value;
  } else if (idl_is_enum(node)) {
    annotationp = &((idl_enum_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_enum_t *)node)->extensibility.value;
  } else if (idl_is_bitmask(node)) {
    annotationp = &((idl_bitmask_t *)node)->extensibility.annotation;
    extensibilityp = &((idl_bitmask_t *)node)->extensibility.value;
  }

  assert(!annotationp == !extensibilityp);

  if (!annotationp) {
    idl_error(pstate, idl_location(annotation_appl),
      "@%s can only be applied to constructed types", annotation);
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (*annotationp) {
    idl_error(pstate, idl_location(annotation_appl),
      "@%s clashes with an earlier annotation", annotation);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (idl_strcasecmp(annotation, "appendable") == 0) {
    extensibility = IDL_APPENDABLE;
  } else if (idl_strcasecmp(annotation, "mutable") == 0) {
    extensibility = IDL_MUTABLE;
  } else {
    assert(idl_strcasecmp(annotation, "final") == 0);
    extensibility = IDL_FINAL;
  }

  if (idl_is_struct(node)) {
    idl_struct_t* _struct = (idl_struct_t*)node;
    if (_struct->inherit_spec) {
      idl_struct_t *base_struct = (idl_struct_t*)_struct->inherit_spec->base;
      if (base_struct->extensibility.value != extensibility) {
        idl_error(pstate, idl_location(annotation_appl),
          "Extensibility of inherited type does not match that of base");
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
  }

  if (idl_is_enum(node) || idl_is_bitmask(node)) {
    if (extensibility == IDL_MUTABLE) {
      idl_error(pstate, idl_location(annotation_appl),
        "Extensibility 'mutable' not allowed for enumerated types");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  //check that extensibility does not clash with datarepresentation value
  if (IDL_FINAL != extensibility && !(idl_allowable_data_representations(node) & IDL_DATAREPRESENTATION_FLAG_XCDR2)) {
    idl_error(pstate, idl_location(annotation_appl),
      "Non-final extensibility set while datarepresentation requires XCDR2");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *annotationp = annotation_appl;
  *extensibilityp = extensibility;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_key(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool key = true;

  assert(pstate);
  assert(annotation_appl);

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_type(literal) == IDL_BOOL);
    key = literal->value.bln;
  }

  if (idl_mask(node) & IDL_MEMBER) {
    idl_member_t *mem = (idl_member_t *)node;
    if (key) {
      if (mem->optional.value) {
        idl_error(pstate, idl_location(annotation_appl),
          "@key cannot be set to true on optional members");
        return IDL_RETCODE_SEMANTIC_ERROR;
      } else if (mem->must_understand.annotation && !mem->must_understand.value) {
        idl_error(pstate, idl_location(annotation_appl),
          "@key cannot be set to true on members with must_understand set to false");
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
    }
    mem->key.annotation = annotation_appl;
    mem->key.value = key;
  } else if (idl_mask(node) & IDL_SWITCH_TYPE_SPEC) {
    ((idl_switch_type_spec_t *)node)->key.annotation = annotation_appl;
    ((idl_switch_type_spec_t *)node)->key.value = key;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@key can only be applied to members of constructed types");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
set_nested(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node,
  bool nested)
{
  const idl_annotation_appl_t **annotationp = NULL;
  bool *nestedp = NULL;
  const char *annotation;

  annotation = idl_identifier(annotation_appl->annotation);
  assert(annotation);

  if (idl_is_struct(node)) {
    annotationp = &((idl_struct_t *)node)->nested.annotation;
    nestedp = &((idl_struct_t *)node)->nested.value;
  } else if (idl_is_union(node)) {
    annotationp = &((idl_union_t *)node)->nested.annotation;
    nestedp = &((idl_union_t *)node)->nested.value;
  }

  if (!annotationp) {
    idl_error(pstate, idl_location(annotation_appl),
      "@%s cannot be applied to %s elements", annotation, idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (*annotationp) {
    if (annotation_appl->annotation == (*annotationp)->annotation) {
      idl_error(pstate, idl_location(annotation_appl),
        "@%s clashes with an earlier annotation", annotation);
      return IDL_RETCODE_SEMANTIC_ERROR;
    /* @topic overrides @nested, short-circuit on @nested */
    } else if (idl_identifier_is(annotation_appl->annotation, "nested")) {
      return IDL_RETCODE_OK;
    }
  }

  *annotationp = annotation_appl;
  *nestedp = nested;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_default(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_const_expr_t *value;
  idl_member_t *mem = (idl_member_t*)node;
  idl_type_spec_t *mem_spec = mem->type_spec;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  value = annotation_appl->parameters->const_expr;

  if (!idl_is_member(node)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@default can only be assigned to members");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (mem->optional.value) {
    idl_error(pstate, idl_location(annotation_appl),
      "@default cannot be set on optional members");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if (!(idl_mask(value) & IDL_BASE_TYPE) && !(idl_mask(value) & IDL_STRING)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@default can only set primitive types");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  /*check whether type of literal matches and falls inside spec of member*/
  idl_type_t mem_type = idl_type(mem_spec);
  if (mem_type != idl_type(value)) {
    idl_retcode_t ret = IDL_RETCODE_OK;
    idl_literal_t *literal = NULL;
    if ((ret = idl_evaluate(pstate, value, mem_type, &literal)))
      return ret;

    assert(literal);
    annotation_appl->parameters->const_expr = literal;
    literal->node.parent = (idl_node_t*)annotation_appl->parameters;
  }

  ((idl_member_t *)node)->value.annotation = annotation_appl;
  ((idl_member_t *)node)->value.value = annotation_appl->parameters->const_expr;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_default_literal(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  assert(pstate);

  if (!idl_is_enumerator(node)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@default_literal can only be assigned to enumerators");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  node->mask |= IDL_DEFAULT_ENUMERATOR;

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_must_understand(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_member_t *mem = (idl_member_t *)node;
  bool must_understand = true;

  assert(pstate);
  assert(annotation_appl);

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_type(literal) == IDL_BOOL);
    must_understand = literal->value.bln;
  }

  if (!idl_is_member(node)) {
    idl_error(pstate, idl_location(annotation_appl),
      "@must_understand can only be assigned to members");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }else if (!must_understand && mem->key.value) {
    idl_error(pstate, idl_location(annotation_appl),
      "@must_understand can not be set to false on key members");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  mem->must_understand.annotation = annotation_appl;
  mem->must_understand.value = must_understand;

  return IDL_RETCODE_OK;
}

static idl_floatval_t idl_arithmetic_to_double(const idl_literal_t *lit) {
  if (idl_type(lit) & IDL_INTEGER_TYPE) {
    idl_intval_t val = idl_intval(lit);
    if (IDL_UNSIGNED & val.type)
      return (idl_floatval_t)val.value.ullng;
    else
      return (idl_floatval_t)val.value.llng;
  } else if (idl_type(lit) & IDL_FLOATING_PT_TYPE) {
    return idl_floatval(lit);
  } else {
    assert(0);
  }
  return 0;
}

static idl_retcode_t check_and_attach_minmax(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *parent,
  idl_annotation_appl_param_t *param,
  idl_node_t *node,
  bool to_max)
{
  idl_type_t param_type, field_type;
  const idl_const_expr_t *const_expr;
  const idl_annotation_appl_t *existing_for_field;

  assert(param);
  const_expr = param->const_expr;

  assert(idl_is_literal(const_expr));
  param_type = idl_type(const_expr);

  if (idl_is_member(node)) {
    if (to_max)
      existing_for_field = ((idl_member_t*)node)->max.annotation;
    else
      existing_for_field = ((idl_member_t*)node)->min.annotation;
    field_type = idl_type(((idl_member_t*)node)->type_spec);
  } else if (idl_is_case(node)) {
    if (to_max)
      existing_for_field = ((idl_case_t*)node)->max.annotation;
    else
      existing_for_field = ((idl_case_t*)node)->min.annotation;
    field_type = idl_type(((idl_case_t*)node)->type_spec);
  } else {
    idl_error(pstate, idl_location(param),
      "@max/@min/@range values can only be assigned to members and cases");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (existing_for_field) {
    idl_error(pstate, idl_location(param),
      "attempting to overwrite existing limit value");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (!(field_type & IDL_INTEGER_TYPE) && !(field_type & IDL_FLOATING_PT_TYPE) && field_type != IDL_OCTET) {
    idl_error(pstate, idl_location(param),
      "@max/@min/@range field type mismatch: attempt to set limits on non-arithmetic field");
    return IDL_RETCODE_SEMANTIC_ERROR;
  } else if ((param_type & IDL_FLOATING_PT_TYPE) && (field_type & IDL_INTEGER_TYPE)) {
    idl_error(pstate, idl_location(param),
      "@max/@min/@range field type mismatch: attempt to set floating point limit on integer field");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  const idl_literal_t *lit = (const idl_literal_t *)const_expr;
  if (param_type == IDL_OCTET || (param_type & IDL_INTEGER_TYPE)) {
    //check whether range of value falls inside type
    idl_intval_t intval = idl_intval(lit);
    bool is_unsigned = intval.type & IDL_UNSIGNED;
    uint64_t uval = intval.value.ullng;
    int64_t sval = intval.value.llng;

    if (!is_unsigned && sval < 0 && (field_type & IDL_UNSIGNED) && !(field_type & IDL_FLOATING_PT_TYPE)) {
      idl_error(pstate, idl_location(param),
        "@max/@min/@range field type mismatch: attempt to set limit < 0 on unsigned field");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }

    bool range_error = false;
    switch (field_type) {
      case IDL_INT8:
        range_error =
          (is_unsigned && uval > INT8_MAX) ||
          (!is_unsigned && (sval > INT8_MAX || sval < INT8_MIN));
        break;
      case IDL_UINT8:
      case IDL_OCTET:
        range_error =
          (is_unsigned && uval > UINT8_MAX) ||
          (!is_unsigned && (sval > UINT8_MAX || sval < 0));
        break;
      case IDL_SHORT:
      case IDL_INT16:
        range_error =
          (is_unsigned && uval > INT16_MAX) ||
          (!is_unsigned && (sval > INT16_MAX || sval < INT16_MIN));
        break;
      case IDL_UINT16:
      case IDL_USHORT:
        range_error =
          (is_unsigned && uval > UINT16_MAX) ||
          (!is_unsigned && (sval > UINT16_MAX || sval < 0));
        break;
      case IDL_INT32:
      case IDL_LONG:
        range_error =
          (is_unsigned && uval > INT32_MAX) ||
          (!is_unsigned && (sval > INT32_MAX || sval < INT32_MIN));
        break;
      case IDL_UINT32:
      case IDL_ULONG:
        range_error =
          (is_unsigned && uval > UINT32_MAX) ||
          (!is_unsigned && (sval > UINT32_MAX || sval < 0));
        break;
      case IDL_LLONG:
      case IDL_INT64:
        range_error = is_unsigned && uval > INT64_MAX;
        break;
      case IDL_ULLONG:
      case IDL_UINT64:
        range_error = !is_unsigned && sval < 0;
        break;
      default:
        //boundary checking done on setting int limits on floating point fields
        break;
    }

    if (range_error) {
      idl_error(pstate, idl_location(const_expr),
        "@max/@min/@range limit range error");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  } else if (param_type & IDL_FLOATING_PT_TYPE) {
    if (!(field_type & IDL_FLOATING_PT_TYPE)) {
      idl_error(pstate, idl_location(const_expr),
        "@max/@min/@range error: floating point limit on non-floating point field");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  } else {
    idl_error(pstate, idl_location(param),
      "@max/@min/@range with %s cannot be applied to '%s' element",
      idl_construct(const_expr), idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  //check that min is not larger than max
  const idl_literal_t *min_lit = NULL,
                      *max_lit = NULL;
  if (idl_is_member(node)) {
    if (to_max) {
      ((idl_member_t*)node)->max.annotation = parent;
      ((idl_member_t*)node)->max.value = lit;
    } else {
      ((idl_member_t*)node)->min.annotation = parent;
      ((idl_member_t*)node)->min.value = lit;
    }
    min_lit = ((idl_member_t*)node)->min.value;
    max_lit = ((idl_member_t*)node)->max.value;
  } else if (idl_is_case(node)) {
    if (to_max) {
      ((idl_case_t*)node)->max.annotation = parent;
      ((idl_case_t*)node)->max.value = lit;
    } else {
      ((idl_case_t*)node)->min.annotation = parent;
      ((idl_case_t*)node)->min.value = lit;
    }
    min_lit = ((idl_case_t*)node)->min.value;
    max_lit = ((idl_case_t*)node)->max.value;
  }

  if (min_lit && max_lit) {
    bool range_error = false;
    if (idl_is_integer_type(min_lit) && idl_is_integer_type(max_lit)) {
      //both are integers, check that min >= max
      idl_intval_t minval = idl_intval(min_lit), maxval = idl_intval(max_lit);

      if (minval.type & IDL_UNSIGNED) {
        if (maxval.type & IDL_UNSIGNED) {
          range_error = minval.value.ullng > maxval.value.ullng;
        } else {
          if (minval.value.ullng > INT64_MAX)
            range_error = true;
          else
            range_error = minval.value.llng > maxval.value.llng;
        }
      } else {
        if (maxval.type & IDL_UNSIGNED) {
          if (minval.value.llng > 0 && maxval.value.ullng < INT64_MAX)
            range_error = minval.value.llng > maxval.value.llng;
        } else {
          range_error = minval.value.llng > maxval.value.llng;
        }
      }
    } else {
      if (idl_is_integer_type(min_lit) || idl_is_integer_type(max_lit)) {
        //one is an integer, check that the double cast does not alias away significant bits
        idl_intval_t ival;
        if (idl_is_integer_type(min_lit))
          ival = idl_intval(min_lit);
        else
          ival = idl_intval(max_lit);

        //9007199254740992 (2^53) is the largest integer that can be accurately represented as a double precision float
        if (((ival.type & IDL_UNSIGNED) && ival.value.ullng > 9007199254740992) ||
                         (!(ival.type & IDL_UNSIGNED) && (ival.value.llng > 9007199254740992 || ival.value.llng < -9007199254740992))) {
          idl_error(pstate, idl_location(param),
            "@range/@min/@max parameter error: unreliable min to max comparison due to limited floating point precision");
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
      }

      range_error = idl_arithmetic_to_double(min_lit) > idl_arithmetic_to_double(max_lit);
    }

    if (range_error) {
      idl_error(pstate, idl_location(param),
        "@range/@min/@max parameter error: minimum larger than maximum for field");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }

  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_range(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_retcode_t ret = IDL_RETCODE_OK;
  idl_annotation_appl_param_t *par = NULL;

  IDL_FOREACH(par,annotation_appl->parameters) {
    //are the parameters sorted, or in the order of declaration of the file
    if (ret)
      break;
    if (idl_strcasecmp(par->member->declarator->name->identifier, "min") == 0) {
      ret = check_and_attach_minmax(pstate, annotation_appl, par, node, false);
    } else if (idl_strcasecmp(par->member->declarator->name->identifier, "max") == 0) {
      ret = check_and_attach_minmax(pstate, annotation_appl, par, node, true);
    }
  }

  return ret;

}

static idl_retcode_t
annotate_min(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  return check_and_attach_minmax(pstate, annotation_appl, annotation_appl->parameters, node, false);
}

static idl_retcode_t
annotate_max(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  return check_and_attach_minmax(pstate, annotation_appl, annotation_appl->parameters, node, true);
}

static idl_retcode_t
annotate_unit(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const char *name = NULL;
  idl_literal_t *literal = NULL;
  assert(annotation_appl);
  assert(annotation_appl->parameters);
  literal = annotation_appl->parameters->const_expr;
  assert(idl_type(literal) == IDL_STRING);
  name = literal->value.str;

  if (idl_is_member(node)) {
    ((idl_member_t*)node)->unit.annotation = annotation_appl;
    ((idl_member_t*)node)->unit.value = name;
  } else if (idl_is_case(node)) {
    ((idl_case_t*)node)->unit.annotation = annotation_appl;
    ((idl_case_t*)node)->unit.value = name;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@unit can only be assigned to members and cases");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }


  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_datarepresentation(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  assert(annotation_appl);
  assert(annotation_appl->parameters);
  idl_literal_t *literal = annotation_appl->parameters->const_expr;
  assert(idl_type(literal) == IDL_BITMASK);
  allowable_data_representations_t val = (allowable_data_representations_t)literal->value.uint32;  //native type of datarepresentation is uint32_t

  if (0 == val) {
    idl_error(pstate, idl_location(annotation_appl),
      "no viable datarepresentations for type %s", idl_name(node)->identifier);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (idl_is_module(node)) {
    ((idl_module_t*)node)->data_representation.annotation = annotation_appl;
    ((idl_module_t*)node)->data_representation.value = val;
  } else if (idl_is_struct(node)) {
    if (IDL_FINAL != ((idl_struct_t*)node)->extensibility.value && !(val & IDL_DATAREPRESENTATION_FLAG_XCDR2)) {
      idl_error(pstate, idl_location(annotation_appl),
        "Datarepresentation does not support XCDR2, but non-final extensibility set.");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    ((idl_struct_t*)node)->data_representation.annotation = annotation_appl;
    ((idl_struct_t*)node)->data_representation.value = val;
  } else if (idl_is_union(node)) {
    if (IDL_FINAL != ((idl_union_t*)node)->extensibility.value && !(val & IDL_DATAREPRESENTATION_FLAG_XCDR2)) {
      idl_error(pstate, idl_location(annotation_appl),
        "Datarepresentation does not support XCDR2, but non-final extensibility set.");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    ((idl_union_t*)node)->data_representation.annotation = annotation_appl;
    ((idl_union_t*)node)->data_representation.value = val;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@data_representation can only be assigned to modules, structs and unions");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_nested(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool nested = true;

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_mask(literal) == (IDL_LITERAL|IDL_BOOL));
    nested = literal->value.bln;
  }

  return set_nested(pstate, annotation_appl, node, nested);
}

static idl_retcode_t
annotate_topic(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  const char *platform = "*"; /* default is any platform */
  const idl_annotation_appl_param_t *p;

  for (p = annotation_appl->parameters; p; p = idl_next(p)) {
    if (idl_identifier_is(p->member, "platform")) {
      assert(idl_mask(p->const_expr) == (IDL_LITERAL|IDL_STRING));
      platform = ((idl_literal_t *)p->const_expr)->value.str;
    } else if (idl_identifier_is(p->member, "name")) {
      idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node),
        "@topic::name parameter is currently ignored.");
    }
  }

  /* only apply topic annotation if platform equals "*" or "DDS" */
  if (strcmp(platform, "*") != 0 && strcmp(platform, "DDS") != 0) {
    idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node),
      "@topic::platform parameter only supports \"*\" and \"DDS\", \"%s\" is not supported and this annotation is therefore ignored.", platform);
    return IDL_RETCODE_OK;
  }

  return set_nested(pstate, annotation_appl, node, false);
}

static idl_retcode_t
set_implicitly_nested(void *node)
{
  idl_retcode_t ret = IDL_RETCODE_OK;

  for (; ret == IDL_RETCODE_OK && node; node = idl_next(node)) {
    if (idl_is_struct(node)) {
      /* skip if annotated with @nested or @topic before */
      if (!((idl_struct_t *)node)->nested.annotation)
        ((idl_struct_t *)node)->nested.value = true;
    } else if (idl_is_union(node)) {
      /* skip if annotated with @nested or @topic before */
      if (!((idl_union_t *)node)->nested.annotation)
        ((idl_union_t *)node)->nested.value = true;
    } else if (idl_is_module(node)) {
      idl_module_t *module = node;
      /* skip if annotated with @default_nested before */
      if (module->default_nested.annotation)
        continue;
      module->default_nested.value = true;
      ret = set_implicitly_nested(module->definitions);
    }
  }

  return ret;
}

static idl_retcode_t
annotate_default_nested(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool default_nested = true;

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_type(literal) == IDL_BOOL);
    default_nested = literal->value.bln;
  }

  if ((idl_mask(node) & IDL_MODULE)) {
    idl_module_t *module = (idl_module_t *)node;
    if (!module->default_nested.annotation) {
      module->default_nested.annotation = annotation_appl;
      module->default_nested.value = default_nested;
      if (!module->default_nested.value)
        return IDL_RETCODE_OK;
      /* annotate all embedded structs and unions implicitly nested unless
         explicitly annotated with either @nested or @topic */
      return set_implicitly_nested(module->definitions);
    } else {
      idl_error(pstate, idl_location(annotation_appl),
        "@default_nested conflicts with earlier annotation");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@default_nested cannot be applied to %s elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
}

static idl_retcode_t
annotate_bit_bound(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_literal_t *literal;
  uint16_t value;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  literal = (idl_literal_t *)annotation_appl->parameters->const_expr;
  assert(idl_type(literal) == IDL_USHORT);
  value = literal->value.uint16;

  if (idl_is_bitmask(node)) {
    if (value > 64) {
      idl_error(pstate, idl_location(annotation_appl),
        "@bit_bound for bitmask must be <= 64");
      return IDL_RETCODE_OUT_OF_RANGE;
    }
    idl_bitmask_t *bm = (idl_bitmask_t *)node;
    bm->bit_bound.annotation = annotation_appl;
    bm->bit_bound.value = value;
  } else if (idl_is_enum(node)) {
    if (value == 0 || value > 32) {
      idl_error(pstate, idl_location(annotation_appl),
        "@bit_bound for enum must be greater than zero and no greater than 32");
      return IDL_RETCODE_OUT_OF_RANGE;
    }
    idl_enum_t *_enum = (idl_enum_t *)node;
    _enum->bit_bound.annotation = annotation_appl;
    _enum->bit_bound.value = value;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@bit_bound can only be applied to enum and bitmask types");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_external(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  bool external = true;
  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_type(literal) == IDL_BOOL);
    external = literal->value.bln;
  }

  if (idl_mask(node) & IDL_MEMBER) {
    idl_member_t *member = (idl_member_t *)node;
    member->external.annotation = annotation_appl;
    member->external.value = external;
  } else if (idl_mask(node) & IDL_CASE) {
    idl_case_t *_case = (idl_case_t *)node;
    _case->external.annotation = annotation_appl;
    _case->external.value = external;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@external can only be applied to members of constructed types");
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_position(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  idl_literal_t *literal;

  assert(annotation_appl);
  assert(annotation_appl->parameters);
  literal = (idl_literal_t *)annotation_appl->parameters->const_expr;
  assert(idl_type(literal) == IDL_USHORT);

  if (idl_is_bit_value(node)) {
    idl_bit_value_t *bit_value = (idl_bit_value_t *)node;
    bit_value->position.annotation = annotation_appl;
    bit_value->position.value = literal->value.uint16;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@position cannot be applied to %s elements", idl_construct(node));
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
annotate_try_construct(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  assert(annotation_appl);

  const char *str = NULL;
  const idl_type_spec_t *ts = NULL;
  const idl_annotation_appl_t **annotation_appl_p = NULL;
  idl_try_construct_t *try_construct_p = NULL;

  if (annotation_appl->parameters) {
    idl_literal_t *literal = annotation_appl->parameters->const_expr;
    assert(idl_type(literal) == IDL_ENUM);
    str = idl_identifier(literal);
  }

  if (idl_is_member(node)) {
    idl_member_t *mem = (idl_member_t*)node;
    ts = mem->type_spec;
    try_construct_p = &(mem->try_construct.value);
    annotation_appl_p = &(mem->try_construct.annotation);
  } else if (idl_is_case(node)) {
    idl_case_t *cs = (idl_case_t*)node;
    ts = cs->type_spec;
    try_construct_p = &(cs->try_construct.value);
    annotation_appl_p = &(cs->try_construct.annotation);
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "@try_construct can only be applied to struct members and union cases");
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  if (NULL == str || idl_strcasecmp(str, "use_default") == 0) {
    /*USE_DEFAULT is the annotation default, NOT the default
      try_construct action in absence of an annotation, that
      is DISCARD*/
    *try_construct_p = IDL_USE_DEFAULT;
  } else if (idl_strcasecmp(str, "discard") == 0) {
    *try_construct_p = IDL_DISCARD;
  } else if (idl_strcasecmp(str, "trim") == 0) {
    /*TRIM can only be used on bounded sequences/strings*/
    if (!idl_is_bounded(ts)) {
      idl_error(pstate, idl_location(annotation_appl),
      "@try_construct(%s) can not be applied to types which cannot have bounds", str);
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    *try_construct_p = IDL_TRIM;
  } else {
    idl_error(pstate, idl_location(annotation_appl),
      "%s is not a supported value for annotation try_construct", str);
    assert(0);
    return IDL_RETCODE_SEMANTIC_ERROR;
  }

  *annotation_appl_p = annotation_appl;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
warn_unsupported_annotation(
  idl_pstate_t *pstate,
  idl_annotation_appl_t *annotation_appl,
  idl_node_t *node)
{
  (void)node;

  assert(annotation_appl);
  assert(annotation_appl->annotation);
  assert(annotation_appl->annotation->name);

  const char *identifier = annotation_appl->annotation->name->identifier;
  if (strcmp(identifier, "verbatim") == 0 ||
      strcmp(identifier, "service") == 0 ||
      strcmp(identifier, "oneway") == 0 ||
      strcmp(identifier, "ami") == 0 ||
      strcmp(identifier, "ignore_literal_names") == 0 ||
      strcmp(identifier, "non_serialized") == 0) {
    idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node),
      "@%s is currently not supported, and will be skipped in parsing", identifier);
  }

  return IDL_RETCODE_OK;
}

static const idl_builtin_annotation_t annotations[] = {
  /* general purpose */
  { .syntax = "@annotation id { unsigned long value; };",
    .summary =
      "<p>Assign a 32-bit unsigned integer identifier to an element, with "
      "the underlying assumption that an identifier should be unique inside "
      "its scope of application. The precise scope of uniqueness depends on "
      "the type of specification making use of it.</p>",
    .callback = &annotate_id },
  { .syntax =
      "@annotation autoid {\n"
      "  enum AutoidKind { SEQUENTIAL, HASH };\n"
      "  AutoidKind value default HASH;\n"
      "};",
    .summary =
      "<p>Complements @id and is applicable to any set containing elements "
      "to which allocating 32-bit unsigned identifiers makes sense. @autoid "
      "instructs to automatically allocate identifiers to elements that have "
      "not been assigned a 32-bit unsigned identifiers explicitly.</p>",
    .callback = &annotate_autoid },
  { .syntax = "@annotation optional { boolean value default TRUE; };",
    .summary =
      "<p>Indicates that the annotated member may be in a NULL state, not"
      "containing any value.</p>",
    .callback = &annotate_optional },
  { .syntax = "@annotation value { any value; };",
    .summary =
      "<p>Set a constant value to any element that may be given a constant "
      "value. More versatile than @id as it may carry any constant value, "
      "but does not carry any concept of uniqueness among a set.</p>",
    .callback = annotate_value },
  { .syntax =
      "@annotation extensibility {\n"
      "  enum ExtensibilityKind { FINAL, APPENDABLE, MUTABLE };\n"
      "  ExtensibilityKind value;\n"
      "};",
    .summary =
      "<p>Specify a given constructed element, such as a data type or an "
      "interface, is allowed to evolve.</p>",
    .callback = annotate_extensibility },
  { .syntax = "@annotation final { };",
    .summary = "<p>Shorthand for @extensibility(FINAL).</p>",
    .callback = annotate_extensibility },
  { .syntax = "@annotation appendable { };",
    .summary = "<p>Shorthand for @extensibility(APPENDABLE).</p>",
    .callback = annotate_extensibility },
  { .syntax = "@annotation mutable { };",
    .summary = "<p>Shorthand for @extensibility(MUTABLE).</p>",
    .callback = annotate_extensibility },
  /* data modeling */
  { .syntax = "@annotation key { boolean value default TRUE; };",
    .summary =
      "<p>Specify a data member is part of the key for the object whose type "
      "is the constructed data type owning this element.</p>",
    .callback = annotate_key },
  { .syntax = "@annotation default { any value; };",
    .summary =
      "<p>Specify the value with which the annotated member should be default"
      "initialized.</p>",
    .callback = annotate_default },
  { .syntax = "@annotation default_literal { };",
    .summary =
      "<p>Explicity sets the default value for an enum to the annotated enumerator"
      "instead of the first entry.</p>",
    .callback = annotate_default_literal },
  { .syntax = "@annotation must_understand { boolean value default TRUE; };",
    .summary =
      "<p>Specify the data member must be understood by any application "
      "making use of that piece of data.</p>",
    .callback = annotate_must_understand },
  { .syntax = "@annotation verbatim {"
              "enum PlacementKind {"
              "BEGIN_FILE,"
              "BEFORE_DECLARATION,"
              "BEGIN_DECLARATION,"
              "END_DECLARATION,"
              "AFTER_DECLARATION,"
              "END_FILE"
              "};"
              "string language default \"*\";"
              "PlacementKind placement default BEFORE_DECLARATION;"
              "string text;"
              "};",
    .summary =
      "<p>Insert the code verbatim.</p>",
    .callback = warn_unsupported_annotation },
  { .syntax = "@annotation service {string platform default \"*\"; };",
    .summary =
      "<p>Indicate that an interface is to be treated as a service.</p>",
    .callback = warn_unsupported_annotation },
  { .syntax = "@annotation ami { boolean value default TRUE; };",
    .summary =
      "<p>Indicate that an interface or an operation is to be made callable asynchronously.</p>",
    .callback = warn_unsupported_annotation },
  { .syntax = "@annotation oneway { boolean value default TRUE; };",
    .summary =
      "<p>Indicate that an operation is one way only.</p>",
    .callback = warn_unsupported_annotation },
  /* units and ranges */
  { .syntax = "@annotation range { any min; any max; };",
    .summary =
      "<p>Specify a range of allowed value for the annotated element.</p>",
    .callback = annotate_range },
  { .syntax = "@annotation min { any value; };",
    .summary =
      "<p>Specify a minimum value for the annotated element.</p>",
    .callback = annotate_min },
  { .syntax = "@annotation max { any value; };",
    .summary =
      "<p>Specify a maximum value for the annotated element.</p>",
    .callback = annotate_max },
  { .syntax = "@annotation unit { string value; };",
    .summary =
      "<p>Specify a unit of measurement for the annotated element.</p>",
    .callback = annotate_unit },
  { .syntax =
      "@bit_bound(32) bitmask DataRepresentationMask {\n"
      "@position(0) XCDR1,\n"
      "@position(1) XML,\n"
      "@position(2) XCDR2\n"
      "};\n"
      "@annotation data_representation {\n"
      "DataRepresentationMask allowed_kinds;\n"
      "};\n",
    .summary =
      "<p>Restricts the allowed datarepresentions to be used for this type.</p>",
    .callback = annotate_datarepresentation },
  /* extensible and dynamic topic types */
  { .syntax = "@annotation nested { boolean value default TRUE; };",
    .summary =
      "<p>Specify annotated element is never used as top-level object.</p>",
    .callback = annotate_nested },
  { .syntax = "@annotation hashid { string value default \"\"; };",
    .summary =
      "<p>Assign a 32-bit unsigned integer identifier to an element "
      "derived from the member name or a string specified in the "
      "annotation parameter.</p>",
    .callback = annotate_hashid },
  { .syntax =
      "@annotation topic {\n"
      "  string name default \"\";\n"
      "  string platform default \"*\";\n"
      "};",
    .summary =
      "<p>Specify annotated element is used as top-level object.</p>",
    .callback = annotate_topic },
  { .syntax = "@annotation default_nested { boolean value default TRUE; };",
    .summary =
      "<p>Change aggregated types contained in annotated module are "
      "considered nested unless otherwise annotated by @topic or @nested.</p>",
    .callback = annotate_default_nested },
  { .syntax = "@annotation bit_bound { unsigned short value default 32; };",
    .summary =
      "<p>This annotation allows setting a size (expressed in bits) to "
      "an element or a group of elements.</p>",
    .callback = annotate_bit_bound },
  { .syntax = "@annotation external { boolean value default TRUE; };",
    .summary =
      "<p>A member declared as external within an aggregated type indicates "
      "that it is desirable for the implementation to store the member in "
      "storage external to the enclosing aggregated type object.</p>",
    .callback = annotate_external },
  { .syntax = "@annotation position { unsigned short value; };",
    .summary =
      "<p>This annotation allows setting a position to an element or a group of elements.</p>",
    .callback = annotate_position },
  { .syntax = "@annotation try_construct {\n"
              "enum TryConstructFailAction { DISCARD, USE_DEFAULT, TRIM };\n"
              "TryConstructFailAction value default USE_DEFAULT;\n"
              "};",
    .summary =
      "<p>This annotation allows setting the (fallback)behaviour when constructing fails.</p>",
    .callback = annotate_try_construct },
  { .syntax = "@annotation ignore_literal_names { boolean value default TRUE; };",
    .summary =
      "<p>Allows ignoring literal names in assignability checking of enumerated types.</p>",
    .callback = warn_unsupported_annotation },
  { .syntax = "@annotation non_serialized { boolean value default TRUE; };",
    .summary =
      "<p>Allows ignoring fields during serialization.</p>",
    .callback = warn_unsupported_annotation },
  { .syntax = NULL, .summary = NULL, .callback = 0 }
};

const idl_builtin_annotation_t *builtin_annotations = annotations;

static idl_annotation_appl_param_t *
find(
  const idl_annotation_appl_t *annotation_appl,
  const idl_annotation_member_t *annotation_member)
{
  const idl_annotation_appl_param_t *ap;

  for (ap = annotation_appl->parameters; ap; ap = idl_next(ap)) {
    if (ap->member == annotation_member)
      return (idl_annotation_appl_param_t *)ap;
  }

  return NULL;
}

static idl_retcode_t
eval(idl_pstate_t *pstate, void *node, idl_annotation_appl_t *appls)
{
  idl_retcode_t ret;

  (void)node;
  for (idl_annotation_appl_t *a = appls; a; a = idl_next(a)) {
    assert(a->annotation);
    idl_annotation_appl_param_t *ap;
    for (ap = a->parameters; ap; ap = idl_next(ap)) {
      idl_type_t type;
      idl_literal_t *literal = NULL;
      assert(ap->member);
      assert(ap->member->type_spec);
      type = idl_type(ap->member->type_spec);
      if ((ret = idl_evaluate(pstate, ap->const_expr, type, &literal)))
        return ret;
      assert(literal);
      if (!idl_scope(literal))
        ((idl_node_t *)literal)->parent = (idl_node_t *)ap;
      ap->const_expr = literal;
    }
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
dedup(idl_pstate_t *pstate, void *node, idl_annotation_appl_t *appls)
{
  (void)node;

  /* deduplicate parameters for annotations */
  for (idl_annotation_appl_t *a = appls; a; a = idl_next(a)) {
    idl_definition_t *d;
    idl_annotation_member_t *m;
    idl_annotation_appl_param_t *ap;
    assert(a->annotation);
    for (ap = a->parameters; ap; ap = idl_next(ap)) {
      assert(ap->member);
      idl_annotation_appl_param_t *cap, *nap;
      for (cap = idl_next(ap); cap; cap = nap) {
        nap = idl_next(cap);
        if (cap->member != ap->member)
          continue;
        if (idl_compare(cap->const_expr, ap->const_expr) != 0) {
          idl_error(pstate, idl_location(cap),
            "Incompatible assignment of '%s' in application of @%s",
            idl_identifier(cap->member->declarator),
            idl_identifier(a->annotation));
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
        idl_unreference_node(cap);
      }
    }
    /* ensure all required parameters are provided */
    for (d = a->annotation->definitions; d; d = idl_next(d)) {
      if (!idl_is_annotation_member(d))
        continue;
      m = (idl_annotation_member_t *)d;
      if (m->const_expr || find(a, m))
        continue;
      idl_error(pstate, idl_location(a),
        "Missing assignment of '%s' in application of @%s",
        idl_identifier(m->declarator),
        idl_identifier(a->annotation));
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  /* deduplication */
  for (idl_annotation_appl_t *a = appls; a; a = idl_next(a)) {
    idl_definition_t *d;
    idl_annotation_member_t *m;
    assert(a->annotation);
    for (idl_annotation_appl_t *na, *ca = idl_next(a); ca; ca = na) {
      na = idl_next(ca);
      if (ca->annotation != a->annotation)
        continue;
      for (d = a->annotation->definitions; d; d = idl_next(d)) {
        /* skip non-members */
        if (!idl_is_annotation_member(d))
          continue;
        m = (idl_annotation_member_t *)d;
        idl_const_expr_t *lval, *rval;
        idl_annotation_appl_param_t *ap;
        ap = find(a, m);
        lval = ap && ap->const_expr ? ap->const_expr : m->const_expr;
        ap = find(ca, m);
        rval = ap && ap->const_expr ? ap->const_expr : m->const_expr;
        if (lval != rval && idl_compare(lval, rval) != 0) {
          idl_error(pstate, idl_location(ap),
            "Incompatible reapplication of @%s",
            idl_identifier(a->annotation));
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
      }
      idl_unreference_node(ca);
    }
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_annotate(
  idl_pstate_t *pstate,
  void *node,
  idl_annotation_appl_t *annotation_appls)
{
  idl_retcode_t ret;

  assert(pstate);

  /* FIXME: in case of multiple forward declators for a type, only the first
     one gets a node, and the subsequent ones don't have a node. Therefore
     these cannot get an annotation (which may not be a problem, as the spec
     is not clear if annotations on forward declarations are allowed). */
  // assert(node);
  if (!node)
    return IDL_RETCODE_OK;

  /* evaluate constant expressions */
  if ((ret = eval(pstate, node, annotation_appls)))
    return ret;
  /* discard redundant annotations and annotation parameters */
  if ((ret = dedup(pstate, node, annotation_appls)))
    return ret;

  for (idl_annotation_appl_t *a = annotation_appls; a; a = idl_next(a)) {
    idl_annotation_callback_t callback = a->annotation->callback;
    if (callback && (ret = callback(pstate, a, node)))
      return ret;
    a->node.parent = node;
  }
  ((idl_node_t *)node)->annotations = annotation_appls;
  return IDL_RETCODE_OK;
}
