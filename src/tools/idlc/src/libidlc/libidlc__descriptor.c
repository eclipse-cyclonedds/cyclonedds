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
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "dds/features.h"
#include "idl/heap.h"
#include "idl/misc.h"
#include "idl/print.h"
#include "idl/processor.h"
#include "idl/stream.h"
#include "idl/string.h"

#include "libidlc__generator.h"
#include "libidlc__descriptor.h"
#include "hashid.h"
#ifdef DDS_HAS_TYPELIB
#include "idl/descriptor_type_meta.h"
#endif

#include "dds/ddsc/dds_opcodes.h"

#define TYPE (16)
#define SUBTYPE (8)

struct visited_ctype {
  struct constructed_type *ctype;
  struct visited_ctype *next;
};

static const uint16_t nop = UINT16_MAX;

static idl_retcode_t push_field(
  struct descriptor *descriptor, const void *node, struct field **fieldp)
{
  struct stack_type *stype;
  struct field *field;
  assert(descriptor);
  assert(idl_is_declarator(node) ||
         idl_is_switch_type_spec(node) ||
         idl_is_case(node) ||
         idl_is_inherit_spec(node));
  stype = descriptor->type_stack;
  assert(stype);
  if (!(field = idl_calloc(1, sizeof(*field))))
    return IDL_RETCODE_NO_MEMORY;
  field->previous = stype->fields;
  field->node = node;
  stype->fields = field;
  if (fieldp)
    *fieldp = field;
  return IDL_RETCODE_OK;
}

static void pop_field(struct descriptor *descriptor)
{
  struct field *field;
  struct stack_type *stype;
  assert(descriptor);
  stype = descriptor->type_stack;
  assert(stype);
  field = stype->fields;
  assert(field);
  stype->fields = field->previous;
 idl_free (field);
}

static idl_retcode_t push_type(
  struct descriptor *descriptor, const void *node, struct constructed_type *ctype, struct stack_type **typep)
{
  struct stack_type *stype;
  assert(descriptor);
  assert(ctype);
  assert(idl_is_struct(node) ||
         idl_is_union(node) ||
         idl_is_sequence(node) ||
         idl_is_declarator(node));
  if (!(stype = idl_calloc(1, sizeof(*stype))))
    return IDL_RETCODE_NO_MEMORY;
  stype->previous = descriptor->type_stack;
  stype->node = node;
  stype->ctype = ctype;
  descriptor->type_stack = stype;
  if (typep)
    *typep = stype;
  return IDL_RETCODE_OK;
}

static void pop_type(struct descriptor *descriptor)
{
  struct stack_type *stype;
  assert(descriptor);
  assert(descriptor->type_stack);
  stype = descriptor->type_stack;
  descriptor->type_stack = stype->previous;
  assert(!stype->fields || (stype->previous && stype->fields == stype->previous->fields));
 idl_free (stype);
}

static idl_retcode_t
stash_instruction(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const struct instruction *inst)
{
  if (instructions->count >= INT16_MAX) {
    idl_error(pstate, NULL, "Maximum number of serializer instructions reached");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  /* make more slots available as necessary */
  assert(instructions->count <= instructions->size);
  if (instructions->count == instructions->size) {
    uint32_t size = instructions->size + 100;
    struct instruction *table = instructions->table;
    if (!(table = idl_realloc(table, size * sizeof(*table))))
      return IDL_RETCODE_NO_MEMORY;
    instructions->size = size;
    instructions->table = table;
  }

  if (index >= instructions->count) {
    index = instructions->count;
  } else {
    struct instruction *table = instructions->table;
    for (uint32_t i = instructions->count; i > index; i--) {
      table[i] = table[i - 1];
      if (table[i].type == ELEM_OFFSET || table[i].type == JEQ_OFFSET || table[i].type == MEMBER_OFFSET || table[i].type == BASE_MEMBERS_OFFSET)
        table[i].data.inst_offset.addr_offs++;
    }
  }

  instructions->table[index] = *inst;
  instructions->count++;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
stash_opcode(
  const idl_pstate_t *pstate, struct descriptor *descriptor, struct instructions *instructions, uint32_t index, uint32_t code, uint32_t order)
{
  struct instruction inst = { OPCODE, { .opcode = { .code = code, .order = order } } };
  descriptor->n_opcodes++;
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const struct field *field)
{
  size_t cnt, pos, len, levels;
  const char *ident;
  const struct field *fld;
  struct instruction inst = { OFFSET, { .offset = { NULL, NULL, 0 } } };

  if (!field)
    return stash_instruction(pstate, instructions, index, &inst);

  assert(field);

  len = 0;
  for (fld = field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else if (idl_is_inherit_spec(fld->node))
      ident = STRUCT_BASE_MEMBER_NAME;
    else
      ident = idl_identifier(fld->node);
    len += strlen(ident);
    if (!fld->previous)
      break;
    len += strlen(".");
  }

  pos = len;
  if (!(inst.data.offset.member = idl_malloc(len + 1)))
    goto err_member;

  inst.data.offset.member[pos] = '\0';
  for (fld=field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else if (idl_is_inherit_spec(fld->node))
      ident = STRUCT_BASE_MEMBER_NAME;
    else
      ident = idl_identifier(fld->node);
    cnt = strlen(ident);
    assert(pos >= cnt);
    pos -= cnt;
    memcpy(inst.data.offset.member + pos, ident, cnt);
    if (!fld->previous)
      break;
    assert(pos > 1);
    pos -= 1;
    inst.data.offset.member[pos] = '.';
  }
  assert(pos == 0);

  levels = idl_is_declarator(fld->node) != 0;
  if (IDL_PRINT(&inst.data.offset.type, print_type, idl_ancestor(fld->node, levels)) < 0)
    goto err_type;

  if (idl_is_declarator(fld->node))
    inst.data.offset.member_id = ((idl_declarator_t *)fld->node)->id.value;

  if (stash_instruction(pstate, instructions, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
 idl_free (inst.data.offset.type);
err_type:
 idl_free (inst.data.offset.member);
err_member:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_key_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, char *key_name, uint16_t length)
{
  struct instruction inst = { KEY_OFFSET, { .key_offset = { .len = length } } };
  if (!(inst.data.key_offset.key_name = idl_strdup(key_name)))
    return IDL_RETCODE_NO_MEMORY;
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_key_offset_val(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, uint16_t offset, uint32_t order)
{
  struct instruction inst = { KEY_OFFSET_VAL, { .key_offset_val = { .offs = offset, .order = order } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_element_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const idl_node_t *node, uint16_t high, int16_t addr_offs)
{
  struct instruction inst = { ELEM_OFFSET, { .inst_offset = { .node = node, .inst.high = high, .addr_offs = addr_offs, .elem_offs = 0 } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_jeq_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const idl_node_t *node, uint32_t opcode, int16_t addr_offs)
{
  struct instruction inst = { JEQ_OFFSET, { .inst_offset = { .node = node, .inst.opcode = opcode, .addr_offs = addr_offs, .elem_offs = 0 } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_mutable_member_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, uint32_t opcode, int16_t addr_offs)
{
  struct instruction inst = { MEMBER_OFFSET, { .inst_offset = { .inst.opcode = opcode, .addr_offs = addr_offs } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_base_members_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const idl_node_t *node)
{
  /* Use index (which is a position in the mutable types parameter list) as the addr_offs to calculate
     the relative offset for the target type */
  struct instruction inst = { BASE_MEMBERS_OFFSET, { .inst_offset = { .node = node, .addr_offs = (int16_t)index, .elem_offs = 0 } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_member_size(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const void *node, bool ext)
{
  const idl_type_spec_t *type_spec;
  struct instruction inst = { MEMBER_SIZE, { .size = { NULL } } };

  if (idl_is_sequence(node) || ext) {
    type_spec = idl_type_spec(node);

    if (idl_is_bounded_string(type_spec)) {
      uint32_t dims = ((const idl_string_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "char[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_unbounded_string(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("char *")))
        goto err_type;
    } else if (idl_is_bounded_wstring(type_spec)) {
      uint32_t dims = ((const idl_wstring_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "wchar_t[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_unbounded_wstring(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("wchar_t *")))
        goto err_type;
    } else {
      if (IDL_PRINT(&inst.data.size.type, print_type, type_spec) < 0)
        goto err_type;
    }
  } else {
    const idl_type_spec_t *array = NULL;
    bool arr_of_seq = false;

    type_spec = idl_type_spec(node);
    while (idl_is_alias(type_spec)) {
      if (idl_is_array(type_spec))
        array = type_spec;
      type_spec = idl_type_spec(type_spec);
    }

    if (array) {
      type_spec = idl_type_spec(array);
      /* sequences are special if non-implicit, because no implicit sequence
         is generated for typedefs of a sequence with a complex declarator */
      if (idl_is_sequence(type_spec)) {
        type_spec = array;
        arr_of_seq = true;
      }
    } else {
      assert(idl_is_array(node));
      type_spec = idl_type_spec(node);
    }

    if (idl_is_bounded_string(type_spec)) {
      uint32_t dims = ((const idl_string_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "char[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_unbounded_string(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("char *")))
        goto err_type;
    } else if (idl_is_bounded_wstring(type_spec)) {
      uint32_t dims = ((const idl_wstring_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "wchar_t[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_unbounded_wstring(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("wchar_t *")))
        goto err_type;
    } else if (idl_is_array(type_spec)) {
      char *typestr = NULL;
      size_t len, pos;
      const idl_const_expr_t *const_expr;

      if (IDL_PRINT(&typestr, print_type, type_spec) < 0)
        goto err_type;

      if (arr_of_seq) {
        /* We're dealing with a typedef of an array of sequences, and therefore
           sizeof (array[0]) won't work. The generated type for this is
           typedef struct decl { .. seq type members .. } decl[n]; so we can
           use sizeof (struct decl) to get size of the array elements. */
        const char * _struct = "struct ";
        size_t sz = strlen(typestr) + strlen(_struct) + 1;
        inst.data.size.type = idl_malloc(sz);
        if (inst.data.size.type) {
          idl_strlcpy(inst.data.size.type, _struct, sz);
          idl_strlcpy(inst.data.size.type + strlen(_struct), typestr, sz - strlen(_struct));
        }
       idl_free (typestr);
        if (!inst.data.size.type)
          goto err_type;
      } else {
        len = pos = strlen(typestr);
        const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
        assert(const_expr);
        for (; const_expr; const_expr = idl_next(const_expr), len += 3)
          /* do nothing */;

        inst.data.size.type = idl_malloc(len + 1);
        if (inst.data.size.type)
          memcpy(inst.data.size.type, typestr, pos);
       idl_free (typestr);
        if (!inst.data.size.type)
          goto err_type;

        const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
        assert(const_expr);
        for (; const_expr; const_expr = idl_next(const_expr), pos += 3)
          memmove(inst.data.size.type + pos, "[0]", 3);
        inst.data.size.type[pos] = '\0';
      }
    } else {
      if (IDL_PRINT(&inst.data.size.type, print_type, type_spec) < 0)
        goto err_type;
    }
  }

  if (stash_instruction(pstate, instructions, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
 idl_free (inst.data.size.type);
err_type:
  return IDL_RETCODE_NO_MEMORY;
}

/* used to stash case labels. no need to take into account strings etc */
static idl_retcode_t
stash_constant(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const idl_const_expr_t *const_expr)
{
  int cnt = 0;
  struct instruction inst = { CONSTANT, { .constant = { NULL } } };
  char **strp = &inst.data.constant.value;

  if (idl_is_enumerator(const_expr)) {
    cnt = IDL_PRINT(strp, print_type, const_expr);
  } else if (const_expr) {
    const idl_literal_t *literal = const_expr;

    switch (idl_type(const_expr)) {
      case IDL_CHAR:
        if (isprint ((unsigned char) literal->value.chr))
          cnt = idl_asprintf(strp, "'%c'", literal->value.chr);
        else
          cnt = idl_asprintf(strp, "'\\%03o'", literal->value.chr);
        break;
      case IDL_BOOL:
        cnt = idl_asprintf(strp, "%s", literal->value.bln ? "true" : "false");
        break;
      case IDL_INT8:
        cnt = idl_asprintf(strp, "%" PRId8, literal->value.int8);
        break;
      case IDL_OCTET:
      case IDL_UINT8:
        cnt = idl_asprintf(strp, "%" PRIu8, literal->value.uint8);
        break;
      case IDL_SHORT:
      case IDL_INT16:
        cnt = idl_asprintf(strp, "%" PRId16, literal->value.int16);
        break;
      case IDL_USHORT:
      case IDL_UINT16:
        cnt = idl_asprintf(strp, "%" PRIu16, literal->value.uint16);
        break;
      case IDL_LONG:
      case IDL_INT32:
        cnt = idl_asprintf(strp, "%" PRId32, literal->value.int32);
        break;
      case IDL_ULONG:
      case IDL_UINT32:
        cnt = idl_asprintf(strp, "%" PRIu32, literal->value.uint32);
        break;
      case IDL_LLONG:
      case IDL_INT64:
        cnt = idl_asprintf(strp, "%" PRId64, literal->value.int64);
        break;
      case IDL_ULLONG:
      case IDL_UINT64:
        cnt = idl_asprintf(strp, "%" PRIu64, literal->value.uint64);
        break;
      default:
        break;
    }
  }

  if (!strp || cnt < 0)
    goto err_value;
  if (stash_instruction(pstate, instructions, index, &inst))
    goto err_stash;
  return IDL_RETCODE_OK;
err_stash:
 idl_free (inst.data.constant.value);
err_value:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_couple(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, uint16_t high, uint16_t low)
{
  struct instruction inst = { COUPLE, { .couple = { high, low } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_single(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, uint32_t single)
{
  struct instruction inst = { SINGLE, { .single = single } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_bitmask_bits(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const idl_bitmask_t * bitmask)
{
  idl_retcode_t ret;
  uint64_t bits = 0;
  idl_bit_value_t *bitv;

  IDL_FOREACH(bitv, bitmask->bit_values) {
    bits |= 1llu << bitv->position.value;
  }

  if ((ret = stash_single (pstate, instructions, index, (uint32_t) (bits >> 32))) != IDL_RETCODE_OK)
    return ret;
  return stash_single (pstate, instructions, index, (uint32_t) (bits & 0xffffffffu));
}

static idl_retcode_t add_typecode(const idl_pstate_t *pstate, const idl_type_spec_t *type_spec, uint32_t shift, bool struct_union_ext, uint32_t *add_to)
{
  assert(add_to && (shift == 8 || shift == 16));
  if (idl_is_array(type_spec)) {
    *add_to |= ((uint32_t)DDS_OP_VAL_ARR << shift);
    return IDL_RETCODE_OK;
  }
  type_spec = idl_strip(type_spec, IDL_STRIP_ALIASES|IDL_STRIP_FORWARD);
  assert(!idl_is_typedef(type_spec) && !idl_is_forward(type_spec));
  switch (idl_type(type_spec)) {
    case IDL_CHAR:
      *add_to |= ((uint32_t)DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
      break;
    case IDL_WCHAR:
      *add_to |= ((uint32_t)DDS_OP_VAL_WCHAR << shift);
      break;
    case IDL_BOOL:
      *add_to |=  ((uint32_t)DDS_OP_VAL_BLN << shift);
      break;
    case IDL_INT8:
      *add_to |= ((uint32_t)DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
      break;
    case IDL_OCTET:
    case IDL_UINT8:
      *add_to |= ((uint32_t)DDS_OP_VAL_1BY << shift);
      break;
    case IDL_SHORT:
    case IDL_INT16:
      *add_to |= ((uint32_t)DDS_OP_VAL_2BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
      break;
    case IDL_USHORT:
    case IDL_UINT16:
      *add_to |= ((uint32_t)DDS_OP_VAL_2BY << shift);
      break;
    case IDL_LONG:
    case IDL_INT32:
      *add_to |= ((uint32_t)DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
      break;
    case IDL_ULONG:
    case IDL_UINT32:
      *add_to |= ((uint32_t)DDS_OP_VAL_4BY << shift);
      break;
    case IDL_LLONG:
    case IDL_INT64:
      *add_to |= ((uint32_t)DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
      break;
    case IDL_ULLONG:
    case IDL_UINT64:
      *add_to |= ((uint32_t)DDS_OP_VAL_8BY << shift);
      break;
    case IDL_FLOAT:
      *add_to |= ((uint32_t)DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
      break;
    case IDL_DOUBLE:
      *add_to |= ((uint32_t)DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
      break;
    case IDL_LDOUBLE:
      idl_error (pstate, type_spec, "Long doubles are currently unsupported");
      return IDL_RETCODE_UNSUPPORTED;
      break;
    case IDL_STRING:
      if (idl_is_bounded(type_spec))
        *add_to |= ((uint32_t)DDS_OP_VAL_BST << shift);
      else
        *add_to |= ((uint32_t)DDS_OP_VAL_STR << shift);
      break;
    case IDL_WSTRING:
      if (idl_is_bounded(type_spec))
        *add_to |= ((uint32_t)DDS_OP_VAL_BWSTR << shift);
      else
        *add_to |= ((uint32_t)DDS_OP_VAL_WSTR << shift);
      break;
    case IDL_SEQUENCE:
      if (idl_is_bounded(type_spec))
        *add_to |= ((uint32_t)DDS_OP_VAL_BSQ << shift);
      else
        *add_to |= ((uint32_t)DDS_OP_VAL_SEQ << shift);
      break;
    case IDL_ENUM: {
      *add_to |= ((uint32_t)DDS_OP_VAL_ENU << shift);
      uint32_t bit_bound = idl_bound(type_spec);
      assert (bit_bound > 0 && bit_bound <= 32);
      if (bit_bound > 16)
        *add_to |= 2 << DDS_OP_FLAG_SZ_SHIFT;
      else if (bit_bound > 8)
        *add_to |= 1 << DDS_OP_FLAG_SZ_SHIFT;
      break;
    }
    case IDL_UNION:
      *add_to |= ((uint32_t)(struct_union_ext ? DDS_OP_VAL_EXT : DDS_OP_VAL_UNI) << shift);
      break;
    case IDL_STRUCT:
      *add_to |= ((uint32_t)(struct_union_ext ? DDS_OP_VAL_EXT : DDS_OP_VAL_STU) << shift);
      break;
    case IDL_BITMASK:
    {
      *add_to |= ((uint32_t)DDS_OP_VAL_BMK << shift);
      uint32_t bit_bound = idl_bound(type_spec);
      assert (bit_bound > 0 && bit_bound <= 64);
      if (bit_bound > 32)
        *add_to |= 3 << DDS_OP_FLAG_SZ_SHIFT;
      else if (bit_bound > 16)
        *add_to |= 2 << DDS_OP_FLAG_SZ_SHIFT;
      else if (bit_bound > 8)
        *add_to |= 1 << DDS_OP_FLAG_SZ_SHIFT;
      break;
    }
      break;
    default:
      idl_error (pstate, type_spec, "Unsupported type for opcode generation");
      return IDL_RETCODE_UNSUPPORTED;
  }
  return IDL_RETCODE_OK;
}

static struct constructed_type *
find_ctype(const struct descriptor *descriptor, const void *node)
{
  struct constructed_type *ctype = descriptor->constructed_types;
  const void *node1;
  if (idl_is_forward(node))
    node1 = ((const idl_forward_t *)node)->type_spec;
  else
    node1 = node;
  while (ctype && ctype->node != node1)
    ctype = ctype->next;
  return ctype;
}

static idl_retcode_t
add_ctype(struct descriptor *descriptor, const idl_scope_t *scope, const void *node, struct constructed_type **ctype)
{
  struct constructed_type *ctype1;

  if (!(ctype1 = idl_calloc(1, sizeof (*ctype1))))
    goto err_ctype;
  ctype1->node = node;
  ctype1->name = idl_name(node);
  ctype1->scope = scope;

  if (!descriptor->constructed_types)
    descriptor->constructed_types = ctype1;
  else {
    struct constructed_type *tmp = descriptor->constructed_types;
    while (tmp->next)
      tmp = tmp->next;
    tmp->next = ctype1;
  }
  if (ctype)
    *ctype = ctype1;
  return IDL_RETCODE_OK;

err_ctype:
  return IDL_RETCODE_NO_MEMORY;
}

static void
shift_plm_list_offsets(struct constructed_type *ctype)
{
  /* update offset for previous members for this ctype */
  assert(idl_is_extensible(ctype->node, IDL_MUTABLE));
  struct instruction *table = ctype->instructions.table;
  for (uint32_t i = 1; i < ctype->pl_offset - 2; i += 2) {
    if (table[i].type == MEMBER_OFFSET) /* not for BASE_MEMBERS_OFFSET */
      table[i].data.inst_offset.addr_offs = (int16_t)(table[i].data.inst_offset.addr_offs + 2);
  }
}

static idl_retcode_t
add_mutable_member_offset(
  const idl_pstate_t *pstate,
  struct constructed_type *ctype,
  uint32_t id)
{
  idl_retcode_t ret;
  assert(idl_is_extensible(ctype->node, IDL_MUTABLE));

  /* add member offset for declarators of mutable types */
  uint32_t opcode = DDS_OP_PLM;
  assert(ctype->instructions.count <= INT16_MAX);
  int16_t addr_offs = (int16_t)(ctype->instructions.count
      - ctype->pl_offset /* offset of first op after PLC */
      + 2 /* skip this JEQ and member id */
      + 1 /* skip RTS (of the PLC list) */);
  if ((ret = stash_mutable_member_offset(pstate, &ctype->instructions, ctype->pl_offset++, opcode, addr_offs)) ||
      (ret = stash_single(pstate, &ctype->instructions, ctype->pl_offset++, id)))
    return ret;

  shift_plm_list_offsets(ctype);

  return IDL_RETCODE_OK;
}

static idl_retcode_t
add_mutable_member_base(
  const idl_pstate_t *pstate,
  struct constructed_type *ctype,
  const struct constructed_type *base_ctype)
{
  idl_retcode_t ret;
  assert(idl_is_extensible(ctype->node, IDL_MUTABLE));
  if ((ret = stash_base_members_offset(pstate, &ctype->instructions, ctype->pl_offset++, base_ctype->node)))
    return ret;
  if ((ret = stash_single(pstate, &ctype->instructions, ctype->pl_offset++, 0u)))
    return ret;
  shift_plm_list_offsets(ctype);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
close_mutable_member(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  struct constructed_type *ctype)
{
  idl_retcode_t ret;
  assert(idl_is_extensible(ctype->node, IDL_MUTABLE));

  if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
stash_member_id(
  const idl_pstate_t *pstate, struct descriptor *descriptor, struct instructions *instructions, uint32_t index, int16_t offs, const char *type, const char *member)
{
  struct instruction inst = { MEMBER_ID, { .member_id = { .addr_offs = offs, .type = type, .member = member } } };
  descriptor->n_opcodes++;
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
add_member_id_entry(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor,
  struct constructed_type_memberid *mid)
{
  idl_retcode_t ret;
  assert(descriptor->member_ids.count <= INT16_MAX);
  assert(mid->ctype->offset <= INT32_MAX);
  assert(mid->ctype->offset < (uint32_t) (INT32_MAX - mid->rel_offs));
  int32_t offs = ((int32_t)mid->ctype->offset + mid->rel_offs);
  assert(offs >= INT16_MIN);
  assert(offs <= INT16_MAX);
  if ((ret = stash_member_id(pstate, descriptor, &descriptor->member_ids, nop, (int16_t) offs, mid->type, mid->member)) ||
      (ret = stash_single(pstate, &descriptor->member_ids, nop, mid->value)))
    return ret;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
close_member_id_table(
  const idl_pstate_t *pstate,
  struct descriptor *descriptor)
{
  return stash_opcode(pstate, descriptor, &descriptor->member_ids, nop, DDS_OP_RTS, 0u);
}


static idl_retcode_t
emit_case(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;

  (void)path;
  if (revisit) {
    /* close inline case */
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
      return ret;
    pop_field(descriptor);
  } else {
    enum {
      INLINE,     /* the instructions for the member are inlined with the ADR
                      e.g.:
                        DDS_OP_JEQ4 | DDS_OP_TYPE_4BY, ... */

      IN_UNION,   /* the DDS_OP_JEQ4 has an offset to the member instructions:
                      e.g.:
                        DDS_OP_JEQ4 | DDS_OP_TYPE_SEQ | offs, ...
                      which are embedded in the union instructions, after the last
                      member's instructions, for example:
                        DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_BST, ... */

      EXTERNAL    /* the member is of an aggregated type that is defined outside the
                      current union, the JEQ4 has an offset to these instructions,
                      e.g.:
                        DDS_OP_JEQ4 | DDS_OP_TYPE_EXT | DDS_OP_TYPE_STU | offs, ... */
    } case_type;
    uint32_t off, cnt;
    uint32_t opcode = DDS_OP_JEQ4;
    const idl_case_t *_case = node;
    const idl_case_label_t *label;
    const idl_type_spec_t *type_spec;

    if (_case->try_construct.annotation)
      idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node), "The @try_construct annotation is not supported yet in the C generator, the default try-construct behavior will be used");

    type_spec = idl_strip(idl_type_spec(node), IDL_STRIP_ALIASES|IDL_STRIP_FORWARD);
    if (idl_is_external(node) && !idl_is_unbounded_xstring(type_spec))
      opcode |= DDS_OP_FLAG_EXT;
    if (idl_is_array(_case->declarator)) {
      opcode |= DDS_OP_TYPE_ARR;
      case_type = IN_UNION;
    } else {
      if ((ret = add_typecode(pstate, type_spec, TYPE, false, &opcode)))
        return ret;
      if (idl_is_struct(type_spec) || idl_is_union(type_spec))
        case_type = EXTERNAL;
      else if (idl_is_array(type_spec) || idl_is_bounded_xstring(type_spec) || idl_is_sequence(type_spec) || idl_is_bitmask(type_spec))
        case_type = IN_UNION;
      else {
        assert (idl_is_base_type(type_spec) || idl_is_unbounded_xstring(type_spec) || idl_is_bitmask(type_spec) || idl_is_enum(type_spec));
        case_type = INLINE;
      }
    }

    if ((ret = push_field(descriptor, _case, NULL)))
      return ret;
    if ((ret = push_field(descriptor, _case->declarator, NULL)))
      return ret;

    const idl_union_t *union_spec = idl_parent(node);
    assert(idl_is_union(union_spec));
    bool union_discr_enum = idl_is_enum(idl_type_spec(union_spec->switch_type_spec));

    /* Note: this function currently only outputs JEQ4 ops and does not use JEQ where
       that would be possible (in case it is not type ENU, or an @external member). This
       could be optimized to save some instructions in the descriptor. */
    cnt = ctype->instructions.count + (stype->labels - stype->label) * 4;
    for (label = _case->labels; label; label = idl_next(label)) {
      off = stype->offset + 2 + (union_discr_enum ? 1 : 0) + (stype->label * 4);

      bool has_size = false;
      if (case_type == INLINE || case_type == IN_UNION) {
        uint32_t label_opcode = opcode;
        /* update offset to first instruction for in-union cases */
        if (!idl_is_enum(type_spec))
          label_opcode &= (DDS_OP_MASK | DDS_OP_TYPE_FLAGS_MASK | DDS_OP_TYPE_MASK);
        if (case_type == IN_UNION)
          label_opcode |= (cnt - off);
        if (idl_is_external(node) && !idl_is_unbounded_xstring(type_spec)) {
          label_opcode |= DDS_OP_FLAG_EXT;
          has_size = idl_is_array(type_spec) || idl_is_sequence(type_spec);
        }
        /* generate union case opcode */
        if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, off++, label_opcode, 0u)))
          return ret;
      } else { /* EXTERNAL */
        assert(off <= INT16_MAX);
        if (idl_is_external(node))
          has_size = true;
        stash_jeq_offset(pstate, &ctype->instructions, off, type_spec, opcode, (int16_t)off);
        descriptor->n_opcodes++; /* this stashes an opcode, but is using stash_jeq_offset so we'll increase the opcode count here */
        off++;
      }
      /* generate union case discriminator, use 0 for default case */
      if ((ret = stash_constant(pstate, &ctype->instructions, off++, idl_is_default_case_label(label) || idl_is_implicit_default_case_label(label) ? 0 : label->const_expr)))
        return ret;
      /* generate union case member (address) offset; use offset 0 for empty types,
         as these members are not generated and no offset can be calculated */
      if ((ret = stash_offset(pstate, &ctype->instructions, off++, idl_is_empty(type_spec) ? NULL : stype->fields)))
        return ret;
      /* For @external union members include the size of the member to allow the
         serializer to allocate memory when deserializing. Stash 0 in case
         no size is required (not an external member), so that the size of a JEQ4
         instruction with parameters remains 4. */
      if (has_size) {
        assert (case_type != INLINE);
        if ((ret = stash_member_size(pstate, &ctype->instructions, off++, node, true)))
          return ret;
      } else if (idl_is_enum(type_spec) && case_type == INLINE) {
        // only add max for inline ENU, not for IN_UNION array of enum
        if ((ret = stash_single(pstate, &ctype->instructions, off++, idl_enum_max_value(type_spec))))
          return ret;
      } else {
        if ((ret = stash_single(pstate, &ctype->instructions, off++, 0)))
          return ret;
      }
      stype->label++;
    }

    pop_field(descriptor); /* field readded by declarator for complex types */
    if (case_type == INLINE || case_type == EXTERNAL) {
      pop_field(descriptor); /* field readded by declarator for complex types */
      return (case_type == INLINE) ? IDL_VISIT_DONT_RECURSE : IDL_VISIT_RECURSE;
    }

    return IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_switch_type_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  uint32_t opcode, order;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;
  struct constructed_type *ctype = descriptor->type_stack->ctype;
  struct field *field = NULL;

  (void)revisit;

  type_spec = idl_strip(idl_type_spec(node), IDL_STRIP_ALIASES);
  assert(!idl_is_typedef(type_spec) && !idl_is_array(type_spec));
  const idl_union_t *union_spec = idl_parent(node);
  assert(idl_is_union(union_spec));

  if ((ret = push_field(descriptor, node, &field)))
    return ret;

  opcode = DDS_OP_ADR | DDS_OP_TYPE_UNI;
  if ((ret = add_typecode(pstate, type_spec, SUBTYPE, false, &opcode)))
    return ret;

  // XTypes spec 7.2.2.4.4.4.6: In a union type, the discriminator member shall always have the 'must understand' attribute set to true.
  opcode |= DDS_OP_FLAG_MU;
  if (idl_is_topic_key(descriptor->topic, (pstate->config.flags & IDL_FLAG_KEYLIST) != 0, path, &order) != IDL_KEYTYPE_NONE) {
    opcode |= DDS_OP_FLAG_KEY;
    ctype->has_key_member = true;
  }
  if (idl_is_default_case(idl_parent(union_spec->default_case)) && !idl_is_implicit_default_case(idl_parent(union_spec->default_case)))
    opcode |= DDS_OP_FLAG_DEF;
  if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
    return ret;
  if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
    return ret;
  if (idl_is_enum(type_spec)) {
    if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_enum_max_value(type_spec))))
      return ret;
  }
  pop_field(descriptor);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_union(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  const idl_union_t *union_spec = (const idl_union_t *)node;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype;
  (void)pstate;
  (void)path;
  if (revisit) {
    uint32_t cnt;
    ctype = stype->ctype;
    assert(stype->label <= stype->labels);
    cnt = (ctype->instructions.count - stype->offset) + 2;
    if ((ret = stash_single(pstate, &ctype->instructions, stype->offset + 2, stype->label)))  // not stype->labels, as labels with empty declarator type will be left out
      return ret;
    bool union_discr_enum = idl_is_enum(idl_type_spec(union_spec->switch_type_spec));
    if ((ret = stash_couple(pstate, &ctype->instructions, stype->offset + 3, (uint16_t)cnt, 4u + (union_discr_enum ? 1 : 0))))
      return ret;
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
      return ret;
    pop_type(descriptor);
  } else {
    const idl_case_t *_case;
    const idl_case_label_t *label;

    if (find_ctype(descriptor, node))
      return IDL_RETCODE_OK | IDL_VISIT_DONT_RECURSE;
    if ((ret = add_ctype(descriptor, idl_scope(node), node, &ctype)))
      return ret;

    if (idl_is_extensible(node, IDL_APPENDABLE)) {
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_DLC, 0u)))
        return ret;
    } else if (idl_is_extensible(node, IDL_MUTABLE)) {
      idl_error(pstate, idl_location(node), "Mutable unions are not supported yet");
      return IDL_RETCODE_UNSUPPORTED;
    }

    if ((ret = push_type(descriptor, node, ctype, &stype)))
      return ret;

    stype->offset = ctype->instructions.count;
    stype->labels = stype->label = 0;

    /* determine total number of case labels as opcodes for complex elements
       are stored after case label opcodes */
    _case = union_spec->cases;
    for (; _case; _case = idl_next(_case)) {
      for (label = _case->labels; label; label = idl_next(label))
        stype->labels++;
    }

    ret = IDL_VISIT_REVISIT;
    /* For a topic, only its top-level type should be visited, not the other
       (non-related) types in the idl */
    if (node == descriptor->topic)
      ret |= IDL_VISIT_DONT_ITERATE;
    return ret;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_forward(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)pstate;
  (void)revisit;
  (void)path;
  (void)node;
  (void)user_data;
  return IDL_VISIT_TYPE_SPEC;
}

static idl_retcode_t
emit_inherit_spec(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)path;

  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  const idl_inherit_spec_t *inherit_spec = (const idl_inherit_spec_t *)node;
  struct constructed_type *ctype = find_ctype(descriptor, inherit_spec->node.parent);
  assert(ctype);

  if (revisit) {
    struct constructed_type *base_ctype = find_ctype(descriptor, inherit_spec->base);
    assert(base_ctype);

    if (idl_is_extensible(ctype->node, IDL_MUTABLE)) {
      if ((ret = add_mutable_member_base(pstate, ctype, base_ctype)))
        return ret;
    } else {
      // final and appendable structs
      int16_t addr_offs = (int16_t)ctype->instructions.count;

      /* generate data field opcode */
      uint32_t opcode = DDS_OP_ADR;
      if ((ret = add_typecode(pstate, inherit_spec->base, TYPE, true, &opcode)))
        return ret;
      opcode |= DDS_OP_FLAG_BASE;
      if (base_ctype->has_key_member) {
        opcode |= DDS_OP_FLAG_KEY;
        ctype->has_key_member = true;
      }
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, 0u)))
        return ret;

      /* generate 'parent' field offset (which should always be 0) */
      struct field *field = NULL;
      if ((ret = push_field(descriptor, node, &field)))
        return ret;
      if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
        return ret;
      pop_field(descriptor);

      /* generate offset to base type */
      if ((ret = stash_element_offset(pstate, &ctype->instructions, nop, inherit_spec->base, 3, addr_offs)))
        return ret;
    }

    return IDL_RETCODE_OK;
  } else {
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }
}

static idl_retcode_t
emit_struct(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct constructed_type *ctype;
  (void)pstate;
  (void)path;
  if (revisit) {
    ctype = find_ctype(descriptor, node);
    assert(ctype);
    /* generate return from subroutine */
    uint32_t off = idl_is_extensible(node, IDL_MUTABLE) ? ctype->pl_offset : nop;
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, off, DDS_OP_RTS, 0u)))
      return ret;
    pop_type(descriptor);
  } else {
    if (find_ctype(descriptor, node))
      return IDL_RETCODE_OK | IDL_VISIT_DONT_RECURSE;
    if ((ret = add_ctype(descriptor, idl_scope(node), node, &ctype)))
      return ret;

    if (idl_is_extensible(node, IDL_APPENDABLE)) {
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_DLC, 0u)))
        return ret;
    } else if (idl_is_extensible(node, IDL_MUTABLE)) {
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_PLC, 0u)))
        return ret;
      ctype->pl_offset = ctype->instructions.count;
    }

    if (!(ret = push_type(descriptor, node, ctype, NULL))) {
      ret = IDL_VISIT_REVISIT;
      /* For a topic, only its top-level type should be visited, not the other
        (non-related) types in the idl */
      if (node == descriptor->topic)
        ret |= IDL_VISIT_DONT_ITERATE;
    }
    return ret;
  }
  return IDL_RETCODE_OK;
}

static bool nested_collection_key(const struct stack_type *stype, const idl_path_t *path)
{
  bool is_key_member = false;
  if (idl_is_sequence(stype->node) || idl_is_array(stype->node))
  {
    size_t i = path->length - 1;
    bool member_found = false;
    do
    {
      if (idl_is_member(path->nodes[i]))
      {
        member_found = true;
        if (((idl_member_t *) path->nodes[i])->key.value)
          is_key_member = true;
      }
      i--;
    } while (!member_found && i > 0); // index 0 cannot be a member, because it must have a parent struct/union
    assert (member_found);
  }
  return is_key_member;
}

static idl_retcode_t
emit_sequence(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;
  const idl_type_spec_t *type_spec;

  (void)pstate;
  (void)path;

  /* resolve non-array aliases */
  type_spec = idl_strip(idl_type_spec(node), IDL_STRIP_ALIASES|IDL_STRIP_FORWARD);

  if (revisit) {
    uint32_t off, cnt;
    uint16_t bound_op = idl_is_bounded(node) ? 1 : 0;
    off = stype->offset;
    cnt = ctype->instructions.count;
    /* generate data field [elem-size] */
    if ((ret = stash_member_size(pstate, &ctype->instructions, off + 2 + bound_op, node, false)))
      return ret;
    /* generate data field [next-insn, elem-insn] */
    if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      assert(cnt <= INT16_MAX);
      int16_t addr_offs = (int16_t)(cnt - 2 - bound_op); /* minus 2 or 3 for the opcode, offset and bound (if applicable) ops that are already stashed for this sequence */
      if ((ret = stash_element_offset(pstate, &ctype->instructions, off + 3 + bound_op, type_spec, (uint16_t)(4u + bound_op), addr_offs)))
        return ret;
    } else {
      if ((ret = stash_couple(pstate, &ctype->instructions, off + 3 + bound_op, (uint16_t)((cnt - off) + 3u), (uint16_t)(4u + bound_op))))
        return ret;
      /* generate return from subroutine */
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
        return ret;
    }
    pop_type(descriptor);
  } else {
    uint32_t off;
    uint32_t opcode = DDS_OP_ADR;
    uint32_t order;
    struct field *field = NULL;

    opcode |= idl_is_bounded(node) ? DDS_OP_TYPE_BSQ : DDS_OP_TYPE_SEQ;
    if ((ret = add_typecode(pstate, type_spec, SUBTYPE, false, &opcode)))
      return ret;
    idl_keytype_t keytype;
    if (idl_is_struct(stype->ctype->node))
    {
      if (nested_collection_key (stype, path))
        opcode |= DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU;
    }
    if ((keytype = idl_is_topic_key(descriptor->topic, (pstate->config.flags & IDL_FLAG_KEYLIST) != 0, path, &order)) != IDL_KEYTYPE_NONE) {
      opcode |= DDS_OP_FLAG_KEY | ((keytype == IDL_KEYTYPE_EXPLICIT) ? DDS_OP_FLAG_MU : 0u);
      ctype->has_key_member = true;
    }

    if (idl_is_struct(stype->node)) {
      field = stype->fields;

      /* Determine if the member is external: use field->node and not the parent of the current node,
         because the latter could be a typedef in case of an aliased type */
      idl_node_t *member_node = idl_parent(field->node);
      assert(idl_is_member(member_node));
      if (idl_is_external(member_node))
        opcode |= DDS_OP_FLAG_EXT;

      if (((idl_member_t *)member_node)->key.value)
      {
        opcode |= DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU;
        ctype->has_key_member = true;
      }

      /* Add FLAG_OPT, and add FLAG_EXT, because an optional field is represented in the same way as
         external fields */
      if (idl_is_optional(member_node))
        opcode |= DDS_OP_FLAG_OPT | DDS_OP_FLAG_EXT;
      if (idl_is_must_understand(member_node))
        opcode |= DDS_OP_FLAG_MU;
    }

    off = ctype->instructions.count;
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
      return ret;
    if (idl_is_bounded(node)) {
      /* generate seq bound field */
      if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_bound(node))))
        return ret;
    }
    if (idl_is_enum(type_spec)) {
      if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_enum_max_value(type_spec))))
        return ret;
    } else if (idl_is_bitmask(type_spec)) {
      if ((ret = stash_bitmask_bits(pstate, &ctype->instructions, nop, (const idl_bitmask_t *)(type_spec))))
        return ret;
    }

    /* short-circuit on simple types */
    if (idl_is_xstring(type_spec) || idl_is_base_type(type_spec) || idl_is_bitmask(type_spec) || idl_is_enum(type_spec)) {
      if (idl_is_bounded(type_spec)) {
        if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_bound(type_spec) + 1)))
          return ret;
      }
      return IDL_RETCODE_OK;
    }

    struct stack_type *seq_stype;
    if ((ret = push_type(descriptor, node, stype->ctype, &seq_stype)))
      return ret;
    seq_stype->offset = off;
    /* For array element type, don't let the visit function unalias the type spec, because this is part
       of the actual type and will be unaliased in emit_declarator for this type */
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT | (idl_is_array(type_spec) ? 0 : IDL_VISIT_UNALIAS_TYPE_SPEC);
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_array(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;
  const idl_type_spec_t *type_spec;
  bool simple = false;
  uint32_t dims = 1;

  if (idl_is_array(node)) {
    dims = idl_array_size(node);
    type_spec = idl_strip(idl_type_spec(node), IDL_STRIP_FORWARD);
  } else {
    type_spec = idl_strip(idl_type_spec(node), IDL_STRIP_ALIASES|IDL_STRIP_FORWARD);
    assert(idl_is_array(type_spec));
    dims = idl_array_size(type_spec);
    type_spec = idl_type_spec(type_spec);
  }

  /* resolve aliases, squash multi-dimensional arrays */
  for (; idl_is_alias(type_spec); type_spec = idl_type_spec(type_spec))
    if (idl_is_array(type_spec))
      dims *= idl_array_size(type_spec);

  simple = (idl_mask(type_spec) & (IDL_BASE_TYPE|IDL_STRING|IDL_WSTRING|IDL_ENUM|IDL_BITMASK)) != 0;

  if (revisit) {
    uint32_t off, cnt;
    off = stype->offset;
    cnt = ctype->instructions.count;
    /* generate data field [next-insn, elem-insn] */
    if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      assert(cnt <= INT16_MAX);
      int16_t addr_offs = (int16_t)(cnt - 3); /* minus 2 for the opcode and offset ops that are already stashed for this array */
      if ((ret = stash_element_offset(pstate, &ctype->instructions, off + 3, type_spec, 5u, addr_offs)))
        return ret;
      /* generate data field [elem-size] */
      if ((ret = stash_member_size(pstate, &ctype->instructions, off + 4, node, false)))
        return ret;
    } else {
      if ((ret = stash_couple(pstate, &ctype->instructions, off + 3, (uint16_t)((cnt - off) + 3u), 5u)))
        return ret;
      /* generate data field [elem-size] */
      if ((ret = stash_member_size(pstate, &ctype->instructions, off + 4, node, false)))
        return ret;
      /* generate return from subroutine */
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
        return ret;
    }

    pop_type(descriptor);
    stype = descriptor->type_stack;
    if (!idl_is_alias(node) && idl_is_struct(stype->node))
      pop_field(descriptor);
  } else {
    uint32_t off;
    uint32_t opcode = DDS_OP_ADR | DDS_OP_TYPE_ARR;
    uint32_t order;
    struct field *field = NULL;

    /* type definitions do not introduce a field */
    if (idl_is_alias(node))
      assert(idl_is_sequence(stype->node));
    else if (idl_is_struct(stype->node) && (ret = push_field(descriptor, node, &field)))
      return ret;

    if ((ret = add_typecode(pstate, type_spec, SUBTYPE, false, &opcode)))
      return ret;
    idl_keytype_t keytype;
    if (idl_is_struct(stype->ctype->node))
    {
      if (nested_collection_key(stype, path))
        opcode |= DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU;
    }
    if ((keytype = idl_is_topic_key(descriptor->topic, (pstate->config.flags & IDL_FLAG_KEYLIST) != 0, path, &order)) != IDL_KEYTYPE_NONE) {
      opcode |= DDS_OP_FLAG_KEY | (keytype == IDL_KEYTYPE_EXPLICIT ? DDS_OP_FLAG_MU : 0u);
      ctype->has_key_member = true;
    }

    /* Array node is the declarator node, so its parent is the member in case
       we're processing a struct type, and this can be used to determine if its
       an external member */
    if (idl_is_struct(stype->node)) {
      idl_node_t *parent = idl_parent(node);
      assert(idl_is_member(parent));

      if (idl_is_external(parent))
        opcode |= DDS_OP_FLAG_EXT;

      if (((idl_member_t *)parent)->key.value)
      {
        opcode |= DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU;
        ctype->has_key_member = true;
      }

      /* Add FLAG_OPT, and add FLAG_EXT, because an optional field is represented in the same way as
         external fields */
      if (idl_is_optional(parent))
        opcode |= DDS_OP_FLAG_OPT | DDS_OP_FLAG_EXT;
      if (idl_is_must_understand(parent))
        opcode |= DDS_OP_FLAG_MU;
    }

    off = ctype->instructions.count;
    /* generate data field opcode */
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    /* generate data field offset */
    if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
      return ret;
    /* generate data field alen */
    if ((ret = stash_single(pstate, &ctype->instructions, nop, dims)))
      return ret;

    if (idl_is_enum(type_spec)) {
      if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_enum_max_value(type_spec))))
        return ret;
    } else if (idl_is_bitmask(type_spec)) {
      if ((ret = stash_bitmask_bits(pstate, &ctype->instructions, nop, (const idl_bitmask_t *)(type_spec))))
        return ret;
    }

    /* short-circuit on simple types */
    if (simple) {
      if (idl_is_bounded_xstring(type_spec)) {
        /* generate data field noop [next-insn, elem-insn] */
        if ((ret = stash_single(pstate, &ctype->instructions, nop, 0)))
          return ret;
        /* generate data field bound */
        if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_bound(type_spec)+1)))
          return ret;
      }
      if (!idl_is_alias(node) && idl_is_struct(stype->node))
        pop_field(descriptor);
      /* visit type-spec for bitmask and enum, so that emit function is triggered
         and some additional checks are done. */
      if (idl_is_bitmask(type_spec) || idl_is_enum(type_spec))
        return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC;
      return IDL_RETCODE_OK;
    }

    struct stack_type *array_stype;
    if ((ret = push_type(descriptor, node, stype->ctype, &array_stype)))
      return ret;
    array_stype->offset = off;
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_member(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)revisit;
  (void)path;
  (void)user_data;
  const idl_member_t *member = (const idl_member_t *)node;
  if (member->value.annotation)
    idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node), "Explicit defaults are not supported yet in the C generator, the value from the @default annotation will not be used");
  if (member->try_construct.annotation)
    idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node), "The @try_construct annotation is not supported yet in the C generator, the default try-construct behavior will be used");
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_bitmask(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)revisit;
  (void)path;
  (void)user_data;
  const idl_bitmask_t *bitmask = (const idl_bitmask_t *)node;
  if (bitmask->extensibility.annotation && bitmask->extensibility.value != IDL_FINAL)
    idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node), "Extensibility appendable and mutable for bitmask type are not yet supported in the C generator, the extensibility will not be used");
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_enum(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  (void)revisit;
  (void)path;
  (void)user_data;
  const idl_enum_t *_enum = (const idl_enum_t *)node;
  uint32_t value = 0, value_c = 0;
  if (_enum->default_enumerator && idl_mask(_enum->default_enumerator) == IDL_DEFAULT_ENUMERATOR)
    idl_warning(pstate, IDL_WARN_UNSUPPORTED_ANNOTATIONS, idl_location(node), "The @default_literal annotation is not supported yet in the C generator and will not be used");
  for (idl_enumerator_t *e1 = _enum->enumerators; e1; e1 = idl_next(e1), value++) {
    if (e1->value.annotation)
      value = e1->value.value;
    if (value != value_c++) {
      idl_warning(pstate, IDL_WARN_ENUM_CONSECUTIVE, idl_location(e1),
        "Warning: values for literals of this enumerator are not consecutive or not starting from zero. The serializer currently does not support checking for valid values for incoming and outgoing data for enums using non-consecutive literal values.");
      break;
    }
  }
  return IDL_RETCODE_OK;
}

static idl_retcode_t
emit_declarator(
  const idl_pstate_t *pstate,
  bool revisit,
  const idl_path_t *path,
  const void *node,
  void *user_data)
{
  idl_retcode_t ret;
  const idl_type_spec_t *type_spec;
  struct descriptor *descriptor = user_data;
  struct stack_type *stype = descriptor->type_stack;
  struct constructed_type *ctype = stype->ctype;
  idl_node_t *parent = idl_parent(node);
  bool mutable_aggr_type_member = idl_is_extensible(ctype->node, IDL_MUTABLE) &&
    (idl_is_member(idl_parent(node)) || idl_is_case(idl_parent(node)));

  type_spec = idl_strip(idl_type_spec(node), IDL_STRIP_ALIASES|IDL_STRIP_FORWARD);

  /* delegate array type specifiers or declarators */
  if (idl_is_array(node) || idl_is_array(type_spec)) {
    if (!revisit && mutable_aggr_type_member) {
      if ((ret = add_mutable_member_offset(pstate, ctype, ((idl_declarator_t *)node)->id.value)))
        return ret;
    }

    if ((ret = emit_array(pstate, revisit, path, node, user_data)) < 0)
      return ret;

    /* in case there is no revisit required (array has simple element type) we have
       to close the mutable member immediately, otherwise close it when revisiting */
    if (mutable_aggr_type_member && (!(ret & IDL_VISIT_REVISIT) || revisit)) {
      idl_retcode_t ret2;
      if ((ret2 = close_mutable_member(pstate, descriptor, ctype)) < 0)
        ret = ret2;
    }
    return ret;
  }

  /* close the mutable member when revisiting */
  if (revisit) {
    if (!idl_is_alias(node) && idl_is_struct(stype->node))
      pop_field(descriptor);
    if (mutable_aggr_type_member) {
      if ((ret = close_mutable_member(pstate, descriptor, ctype)))
        return ret;
    }
  } else {
    uint32_t opcode;
    uint32_t order = 0;
    struct field *field = NULL;

    if (mutable_aggr_type_member && (ret = add_mutable_member_offset(pstate, ctype, ((idl_declarator_t *)node)->id.value)))
      return ret;

    if (!idl_is_alias(node) && idl_is_struct(stype->node)) {
      if ((ret = push_field(descriptor, node, &field)))
        return ret;
    }

    if (idl_is_sequence(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;

    /* inline the type spec for struct/union declarators in a union */
    if (idl_is_union(ctype->node)) {
      if (idl_is_union(type_spec) || idl_is_struct(type_spec))
        return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;
    }

    assert(ctype->instructions.count <= INT16_MAX);
    int16_t addr_offs = (int16_t)ctype->instructions.count;
    bool has_size = false;
    bool keylist = (pstate->config.flags & IDL_FLAG_KEYLIST) != 0;
    opcode = DDS_OP_ADR;
    if ((ret = add_typecode(pstate, type_spec, TYPE, true, &opcode)))
      return ret;

    /* Mark this DDS_OP_ADR as key if @key annotation is present, even in case the referring
        member is not part of the key (which resulted in idl_is_topic_key returning false).
        The reason for adding the key flag here, is that if any other member (that is a key)
        refers to this type, it will require the key flag. */
    idl_keytype_t keytype;
    if ((keytype = idl_is_topic_key(descriptor->topic, keylist, path, &order)) != IDL_KEYTYPE_NONE)
    {
      opcode |= DDS_OP_FLAG_KEY | (keytype == IDL_KEYTYPE_EXPLICIT ? DDS_OP_FLAG_MU : 0u);
      ctype->has_key_member = true;
    }
    else if (idl_is_member(parent) && ((idl_member_t *)parent)->key.value)
    {
      opcode |= DDS_OP_FLAG_KEY | DDS_OP_FLAG_MU;
      ctype->has_key_member = true;
    }
    if (idl_is_struct(stype->node) && (idl_is_external(parent) || idl_is_optional(parent))) {
      if (idl_is_external(parent) && !idl_is_unbounded_xstring(type_spec))
        opcode |= DDS_OP_FLAG_EXT;
      /* For optional field of non-string (or bounded string) types, add EXT flag because
         an optional field is represented in the same way as external fields */
      if (idl_is_optional(parent))
        opcode |= DDS_OP_FLAG_OPT | (idl_is_unbounded_xstring(type_spec) ? 0 : DDS_OP_FLAG_EXT);
      /* For @external and @optional fields of type OP_TYPE_EXT include the size of the field to
         allow the serializer to allocate memory for this field when deserializing. */
      if (DDS_OP_TYPE(opcode) == DDS_OP_VAL_EXT)
        has_size = true;
    }
    if (idl_is_must_understand(parent))
      opcode |= DDS_OP_FLAG_MU;

    /* use member id for key ordering */
    order = ((idl_declarator_t *)node)->id.value;

    /* generate data field opcode */
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    /* generate data field offset; for empty types the offset cannot be used
       because the member is commented-out, so set to 0 in that case */
    if ((ret = stash_offset(pstate, &ctype->instructions, nop, idl_is_empty(type_spec) ? NULL : field)))
      return ret;
    /* generate data field bound */
    if (idl_is_bounded_xstring(type_spec)) {
      if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_bound(type_spec)+1)))
        return ret;
    } else if (idl_is_enum(type_spec)) {
      if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_enum_max_value(type_spec))))
        return ret;
    } else if (idl_is_bitmask(type_spec)) {
      if ((ret = stash_bitmask_bits(pstate, &ctype->instructions, nop, (const idl_bitmask_t *)(type_spec))))
        return ret;
    } else if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      if ((ret = stash_element_offset(pstate, &ctype->instructions, nop, type_spec, (uint16_t) 3 + (has_size ? 1 : 0), addr_offs)))
        return ret;
    }
    /* generate data field element size */
    if (has_size) {
      if ((ret = stash_member_size(pstate, &ctype->instructions, nop, node, true)))
        return ret;
    }

    /* Type spec in case of aggregated type needs to be visited, to generate
       the serializer ops for these types. For bitmask type, also visit
       the type-spec, so that emit function is triggered that checks for
       unsupported extensibility. For enum types, the emit function is called
       to check for non-consecutive values. */
    if (idl_is_union(type_spec) || idl_is_struct(type_spec) || idl_is_bitmask(type_spec) || idl_is_enum(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_UNALIAS_TYPE_SPEC | IDL_VISIT_REVISIT;

    return IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

static int print_opcode(FILE *fp, const struct instruction *inst)
{
  char buf[32];
  const char *vec[10];
  size_t len = 0;
  enum dds_stream_opcode opcode;

  assert(inst->type == OPCODE);

  opcode = DDS_OP(inst->data.opcode.code);

  switch (opcode) {
    case DDS_OP_DLC:
      vec[len++] = "DDS_OP_DLC";
      break;
    case DDS_OP_PLC:
      vec[len++] = "DDS_OP_PLC";
      break;
    case DDS_OP_RTS:
      vec[len++] = "DDS_OP_RTS";
      break;
    case DDS_OP_KOF:
      vec[len++] = "DDS_OP_KOF";
      idl_snprintf(buf, sizeof(buf), " | %u", DDS_OP_LENGTH(inst->data.opcode.code));
      vec[len++] = buf;
      break;
    case DDS_OP_PLM:
      vec[len++] = "DDS_OP_PLM";
      break;
    case DDS_OP_JEQ4:
      vec[len++] = "DDS_OP_JEQ4";
      break;
    case DDS_OP_MID:
      vec[len++] = "DDS_OP_MID";
      break;
    default:
      assert(opcode == DDS_OP_ADR);
      vec[len++] = "DDS_OP_ADR";
      break;
  }

  if (opcode == DDS_OP_ADR) {
    /* FLAG_BASE to indicate EXT 'parent' field */
    if (inst->data.opcode.code & DDS_OP_FLAG_BASE)
      vec[len++] = " | DDS_OP_FLAG_BASE";
    if (inst->data.opcode.code & DDS_OP_FLAG_KEY)
      vec[len++] = " | DDS_OP_FLAG_KEY";
    if (inst->data.opcode.code & DDS_OP_FLAG_MU)
      vec[len++] = " | DDS_OP_FLAG_MU";
    if (inst->data.opcode.code & DDS_OP_FLAG_OPT)
      vec[len++] = " | DDS_OP_FLAG_OPT";
  } else if (opcode == DDS_OP_PLM) {
    /* FLAG_BASE to indicate inheritance in PLM list */
    if (DDS_PLM_FLAGS(inst->data.opcode.code) & DDS_OP_FLAG_BASE)
      vec[len++] = " | (DDS_OP_FLAG_BASE << 16)";
  }

  if (opcode == DDS_OP_ADR || opcode == DDS_OP_JEQ4) {
    if (inst->data.opcode.code & DDS_OP_FLAG_EXT)
      vec[len++] = " | DDS_OP_FLAG_EXT";
    switch (DDS_OP_TYPE(inst->data.opcode.code)) {
      case DDS_OP_VAL_BLN: vec[len++] = " | DDS_OP_TYPE_BLN"; break;
      case DDS_OP_VAL_1BY: vec[len++] = " | DDS_OP_TYPE_1BY"; break;
      case DDS_OP_VAL_2BY: vec[len++] = " | DDS_OP_TYPE_2BY"; break;
      case DDS_OP_VAL_4BY: vec[len++] = " | DDS_OP_TYPE_4BY"; break;
      case DDS_OP_VAL_8BY: vec[len++] = " | DDS_OP_TYPE_8BY"; break;
      case DDS_OP_VAL_STR: vec[len++] = " | DDS_OP_TYPE_STR"; break;
      case DDS_OP_VAL_BST: vec[len++] = " | DDS_OP_TYPE_BST"; break;
      case DDS_OP_VAL_SEQ: vec[len++] = " | DDS_OP_TYPE_SEQ"; break;
      case DDS_OP_VAL_BSQ: vec[len++] = " | DDS_OP_TYPE_BSQ"; break;
      case DDS_OP_VAL_ARR: vec[len++] = " | DDS_OP_TYPE_ARR"; break;
      case DDS_OP_VAL_UNI: vec[len++] = " | DDS_OP_TYPE_UNI"; break;
      case DDS_OP_VAL_STU: vec[len++] = " | DDS_OP_TYPE_STU"; break;
      case DDS_OP_VAL_ENU: vec[len++] = " | DDS_OP_TYPE_ENU"; break;
      case DDS_OP_VAL_BMK: vec[len++] = " | DDS_OP_TYPE_BMK"; break;
      case DDS_OP_VAL_EXT: vec[len++] = " | DDS_OP_TYPE_EXT"; break;
      case DDS_OP_VAL_WSTR: vec[len++] = " | DDS_OP_TYPE_WSTR"; break;
      case DDS_OP_VAL_BWSTR: vec[len++] = " | DDS_OP_TYPE_BWSTR"; break;
      case DDS_OP_VAL_WCHAR: vec[len++] = " | DDS_OP_TYPE_WCHAR"; break;
    }
  }

  if (opcode == DDS_OP_PLM) {
    /* lower 16 bits contain an offset */
    idl_snprintf(buf, sizeof(buf), " | %u", (uint16_t) DDS_OP_JUMP (inst->data.opcode.code));
    vec[len++] = buf;
  } else if (opcode == DDS_OP_MID) {
    /* lower 16 bits contain an offset */
    idl_snprintf(buf, sizeof(buf), " | %u", (uint16_t) DDS_OP_JUMP (inst->data.member_id.addr_offs));
    vec[len++] = buf;
  } else if (opcode == DDS_OP_JEQ4) {
    enum dds_stream_typecode type = DDS_OP_TYPE(inst->data.opcode.code);
    if (type == DDS_OP_VAL_ENU) {
      idl_snprintf(buf, sizeof(buf), " | (%u << DDS_OP_FLAG_SZ_SHIFT)", (inst->data.opcode.code & DDS_OP_FLAG_SZ_MASK) >> DDS_OP_FLAG_SZ_SHIFT);
      vec[len++] = buf;
    } else if (type != DDS_OP_VAL_BLN && type != DDS_OP_VAL_1BY && type != DDS_OP_VAL_2BY && type != DDS_OP_VAL_4BY && type != DDS_OP_VAL_8BY && type != DDS_OP_VAL_STR && type != DDS_OP_VAL_WSTR) {
      /* lower 16 bits contain an offset */
      idl_snprintf(buf, sizeof(buf), " | %u", (uint16_t) DDS_OP_JUMP (inst->data.opcode.code));
      vec[len++] = buf;
    } else {
      assert ((uint16_t) DDS_OP_JUMP (inst->data.opcode.code) == 0);
    }
  } else if (opcode == DDS_OP_ADR) {
    enum dds_stream_typecode type = DDS_OP_TYPE(inst->data.opcode.code);
    enum dds_stream_typecode subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
    assert((type == DDS_OP_VAL_SEQ || type == DDS_OP_VAL_ARR || type == DDS_OP_VAL_UNI || type == DDS_OP_VAL_STU || type == DDS_OP_VAL_BSQ) == (subtype != 0));
    switch (subtype) {
      case DDS_OP_VAL_BLN: vec[len++] = " | DDS_OP_SUBTYPE_BLN"; break;
      case DDS_OP_VAL_1BY: vec[len++] = " | DDS_OP_SUBTYPE_1BY"; break;
      case DDS_OP_VAL_2BY: vec[len++] = " | DDS_OP_SUBTYPE_2BY"; break;
      case DDS_OP_VAL_4BY: vec[len++] = " | DDS_OP_SUBTYPE_4BY"; break;
      case DDS_OP_VAL_8BY: vec[len++] = " | DDS_OP_SUBTYPE_8BY"; break;
      case DDS_OP_VAL_STR: vec[len++] = " | DDS_OP_SUBTYPE_STR"; break;
      case DDS_OP_VAL_BST: vec[len++] = " | DDS_OP_SUBTYPE_BST"; break;
      case DDS_OP_VAL_SEQ: vec[len++] = " | DDS_OP_SUBTYPE_SEQ"; break;
      case DDS_OP_VAL_BSQ: vec[len++] = " | DDS_OP_SUBTYPE_BSQ"; break;
      case DDS_OP_VAL_ARR: vec[len++] = " | DDS_OP_SUBTYPE_ARR"; break;
      case DDS_OP_VAL_UNI: vec[len++] = " | DDS_OP_SUBTYPE_UNI"; break;
      case DDS_OP_VAL_STU: vec[len++] = " | DDS_OP_SUBTYPE_STU"; break;
      case DDS_OP_VAL_ENU: vec[len++] = " | DDS_OP_SUBTYPE_ENU"; break;
      case DDS_OP_VAL_BMK: vec[len++] = " | DDS_OP_SUBTYPE_BMK"; break;
      case DDS_OP_VAL_WSTR: vec[len++] = " | DDS_OP_SUBTYPE_WSTR"; break;
      case DDS_OP_VAL_BWSTR: vec[len++] = " | DDS_OP_SUBTYPE_BWSTR"; break;
      case DDS_OP_VAL_WCHAR: vec[len++] = " | DDS_OP_SUBTYPE_WCHAR"; break;
      case DDS_OP_VAL_EXT: abort(); break;
    }

    if (type == DDS_OP_VAL_ENU || subtype == DDS_OP_VAL_ENU || type == DDS_OP_VAL_BMK || subtype == DDS_OP_VAL_BMK) {
      idl_snprintf(buf, sizeof(buf), " | (%u << DDS_OP_FLAG_SZ_SHIFT)", (inst->data.opcode.code & DDS_OP_FLAG_SZ_MASK) >> DDS_OP_FLAG_SZ_SHIFT);
      vec[len++] = buf;
    }
    if (type == DDS_OP_VAL_UNI && (inst->data.opcode.code & DDS_OP_FLAG_DEF))
      vec[len++] = " | DDS_OP_FLAG_DEF";
    else if (inst->data.opcode.code & DDS_OP_FLAG_FP)
      vec[len++] = " | DDS_OP_FLAG_FP";
    if (inst->data.opcode.code & DDS_OP_FLAG_SGN)
      vec[len++] = " | DDS_OP_FLAG_SGN";
  }

  for (size_t cnt=0; cnt < len; cnt++) {
    if (fputs(vec[cnt], fp) < 0)
      return -1;
  }
  return 0;
}

static int print_offset(FILE *fp, const struct instruction *inst)
{
  const char *type, *member;
  assert(inst->type == OFFSET);
  type = inst->data.offset.type;
  member = inst->data.offset.member;
  assert((!type && !member) || (type && member));
  if (!type)
    return fputs("0u", fp);
  else
    return idl_fprintf(fp, "offsetof (%s, %s)", type, member);
}

static int print_size(FILE *fp, const struct instruction *inst)
{
  const char *type;
  assert(inst->type == MEMBER_SIZE);
  type = inst->data.offset.type;
  return idl_fprintf(fp, "sizeof (%s)", type) < 0 ? -1 : 0;
}

static int print_constant(FILE *fp, const struct instruction *inst)
{
  const char *value;
  value = inst->data.constant.value ? inst->data.constant.value : "0";
  return fputs(value, fp);
}

static int print_couple(FILE *fp, const struct instruction *inst)
{
  uint16_t high, low;
  assert(inst->type == COUPLE);
  high = inst->data.couple.high;
  low = inst->data.couple.low;
  return idl_fprintf(fp, "(%"PRIu16"u << 16u) + %"PRIu16"u", high, low);
}

static int print_single(FILE *fp, const struct instruction *inst)
{
  assert(inst->type == SINGLE);
  return idl_fprintf(fp, "%"PRIu32"u", inst->data.single);
}

static int print_opcodes(FILE *fp, const struct descriptor *descriptor, uint32_t *kof_offs, uint32_t *mid_table_offs)
{
  const struct instruction *inst;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode optype, subtype;
  char *type = NULL;
  const char *seps[] = { ", ", ",\n  " };
  uint32_t cnt = 0;

  if (IDL_PRINTA(&type, print_type, descriptor->topic) < 0)
    return -1;
  if (idl_fprintf(fp, "static const uint32_t %s_ops [] =\n{\n", type) < 0)
    return -1;

  for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
    if (ctype != descriptor->constructed_types)
      if (fputs(",\n\n", fp) < 0)
        return -1;

    if (idl_fprintf(fp, "  /* %s */\n", idl_identifier(ctype->node)) < 0)
      return -1;
    for (size_t op = 0, brk = 0; op < ctype->instructions.count; op++) {
      inst = &ctype->instructions.table[op];
      const char *sep = seps[op == brk];
      switch (inst->type) {
        case OPCODE:
          sep = op ? seps[1] : "  "; /* indent, always */
          /* determine when to break line */
          opcode = DDS_OP(inst->data.opcode.code);
          optype = DDS_OP_TYPE(inst->data.opcode.code);
          if (opcode == DDS_OP_RTS || opcode == DDS_OP_DLC || opcode == DDS_OP_PLC)
            brk = op + 1;
          else if (opcode == DDS_OP_JEQ4)
            brk = op + 4;
          else if (opcode == DDS_OP_PLM)
            brk = op + 3;
          else if (optype == DDS_OP_VAL_BST || optype == DDS_OP_VAL_BWSTR)
            brk = op + 3;
          else if (optype == DDS_OP_VAL_EXT)
            brk = op + 3;
          else if (optype == DDS_OP_VAL_ARR || optype == DDS_OP_VAL_SEQ || optype == DDS_OP_VAL_BSQ) {
            subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
            brk = op + (optype == DDS_OP_VAL_SEQ ? 2 : 3);
            switch (subtype) {
              case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
              case DDS_OP_VAL_STR: case DDS_OP_VAL_WSTR:
                break;
              case DDS_OP_VAL_BST: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_ENU:
                brk++;
                break;
              default:
                brk += 2;
                break;
            }
            if (optype == DDS_OP_VAL_ARR && (subtype == DDS_OP_VAL_BST || subtype == DDS_OP_VAL_BWSTR)) brk++;
          } else if (optype == DDS_OP_VAL_UNI) {
            brk = op + 4;
            subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
            if (subtype == DDS_OP_VAL_ENU)
              brk++;
          } else if (optype == DDS_OP_VAL_ENU)
            brk = op + 3;
          else if (optype == DDS_OP_VAL_BMK)
            brk = op + 4;
          else
            brk = op + 2;
          if (inst->data.opcode.code & (DDS_OP_FLAG_EXT | DDS_OP_FLAG_OPT))
            brk++;
          if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
            return -1;
          break;
        case OFFSET:
          if (fputs(sep, fp) < 0 || print_offset(fp, inst) < 0)
            return -1;
          break;
        case MEMBER_SIZE:
          if (fputs(sep, fp) < 0 || print_size(fp, inst) < 0)
            return -1;
          break;
        case CONSTANT:
          if (fputs(sep, fp) < 0 || print_constant(fp, inst) < 0)
            return -1;
          break;
        case COUPLE:
          if (fputs(sep, fp) < 0 || print_couple(fp, inst) < 0)
            return -1;
          break;
        case SINGLE:
          if (fputs(sep, fp) < 0 || print_single(fp, inst) < 0)
            return -1;
          break;
        case ELEM_OFFSET:
        {
          const struct instruction inst_couple = { COUPLE, { .couple = { .high = inst->data.inst_offset.inst.high & 0xffffu, .low = (uint16_t)inst->data.inst_offset.elem_offs } } };
          if (fputs(sep, fp) < 0 || print_couple(fp, &inst_couple) < 0 || idl_fprintf(fp, " /* %s */", idl_identifier(inst->data.inst_offset.node)) < 0)
            return -1;
          break;
        }
        case JEQ_OFFSET:
        {
          const struct instruction inst_op = { OPCODE, { .opcode = { .code = (inst->data.inst_offset.inst.opcode & (DDS_OP_MASK | DDS_OP_TYPE_FLAGS_MASK | DDS_OP_TYPE_MASK)) | (uint16_t)inst->data.inst_offset.elem_offs, .order = 0 } } };
          if (fputs(sep, fp) < 0 || print_opcode(fp, &inst_op) < 0 || idl_fprintf(fp, " /* %s */", idl_identifier(inst->data.inst_offset.node)) < 0)
            return -1;
          brk = op + 4;
          break;
        }
        case MEMBER_OFFSET:
        {
          const struct instruction inst_op = { OPCODE, { .opcode = { .code = (inst->data.inst_offset.inst.opcode & (DDS_OP_MASK | DDS_PLM_FLAGS_MASK)) | (uint16_t)inst->data.inst_offset.addr_offs, .order = 0 } } };
          if (fputs(sep, fp) < 0 || print_opcode(fp, &inst_op) < 0)
            return -1;
          brk = op + 2;
          break;
        }
        case BASE_MEMBERS_OFFSET:
        {
          const struct instruction inst_op = { OPCODE, { .opcode = { .code = ((DDS_OP_PLM | (DDS_OP_FLAG_BASE << 16)) & (DDS_OP_MASK | DDS_PLM_FLAGS_MASK)) | (uint16_t)inst->data.inst_offset.elem_offs } } };
          if (fputs(sep, fp) < 0 || print_opcode(fp, &inst_op) < 0 || idl_fprintf(fp, " /* %s */", idl_identifier(inst->data.inst_offset.node)) < 0)
            return -1;
          brk = op + 2;
          break;
        }
        case KEY_OFFSET:
        case KEY_OFFSET_VAL:
        case MEMBER_ID:
          return -1;
      }
      cnt++;
    }
  }

  if (kof_offs)
    *kof_offs = cnt;

  for (size_t op = 0, brk = 0; op < descriptor->key_offsets.count; op++) {
    inst = &descriptor->key_offsets.table[op];
    const char *sep = seps[op == brk];
    switch (inst->type) {
      case KEY_OFFSET:
      {
        const struct instruction inst_op = { OPCODE, { .opcode = { .code = (DDS_OP_KOF & DDS_OP_MASK) | (inst->data.key_offset.len & DDS_KOF_OFFSET_MASK), .order = 0 } } };
        if (fputs(sep, fp) < 0 || idl_fprintf(fp, "\n  /* key: %s */\n  ", inst->data.key_offset.key_name) < 0 || print_opcode(fp, &inst_op) < 0)
          return -1;
        brk = op + 1 + inst->data.key_offset.len;
        break;
      }
      case KEY_OFFSET_VAL: {
        const struct instruction inst_single = { SINGLE, { .single = inst->data.key_offset_val.offs } };
        if (fputs(sep, fp) < 0 || print_single(fp, &inst_single) < 0 || idl_fprintf(fp, " /* order: %"PRIu32" */", inst->data.key_offset_val.order) < 0)
          return -1;
        break;
      }
      case OPCODE:
        opcode = DDS_OP(inst->data.opcode.code);
        assert (opcode == DDS_OP_RTS);
        if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
          return -1;
        break;
      default:
        return -1;
    }
    cnt++;
  }

  if (mid_table_offs)
    *mid_table_offs = descriptor->member_ids.count > 0 ? cnt : 0;

  for (size_t op = 0, brk = 0; op < descriptor->member_ids.count; op++) {
    inst = &descriptor->member_ids.table[op];
    const char *sep = seps[op == brk];
    switch (inst->type)
    {
      case MEMBER_ID:
      {
        const struct instruction inst_op = { OPCODE, { .opcode = { .code = (DDS_OP_MID & DDS_OP_MASK) | (inst->data.member_id.addr_offs & DDS_KOF_OFFSET_MASK), .order = 0 } } };
        if (fputs(sep, fp) < 0 || (op == 0 && idl_fprintf(fp, "\n  /* member ID list */\n  ") < 0) || print_opcode(fp, &inst_op) < 0 || idl_fprintf(fp, " /* %s.%s */", inst->data.member_id.type, inst->data.member_id.member) < 0)
          return -1;
        brk = op + 2;
        break;
      }
      case SINGLE:
        if (fputs(sep, fp) < 0 || print_single(fp, inst) < 0)
          return -1;
        break;
      case OPCODE:
        opcode = DDS_OP(inst->data.opcode.code);
        assert (opcode == DDS_OP_RTS);
        if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
          return -1;
        break;
      default:
        return -1;
    }
    cnt++;
  }

  if (fputs("\n};\n\n", fp) < 0)
    return -1;
  return 0;
}

static void free_ctype_keys(struct constructed_type_key *key)
{
  struct constructed_type_key *tmp = key, *tmp1;
  while (tmp) {
    if (tmp->name)
     idl_free (tmp->name);
    if (tmp->sub)
      free_ctype_keys(tmp->sub);
    tmp1 = tmp;
    tmp = tmp->next;
   idl_free (tmp1);
  }
}

static idl_retcode_t get_ctype_keys(const idl_pstate_t *pstate, struct descriptor *descriptor, struct constructed_type *ctype, struct constructed_type_key **keys, uint32_t *n_keys, bool parent_is_key, uint32_t base_type_ops_offs, bool in_collection);


static idl_retcode_t get_ctype_keys_adr(const idl_pstate_t *pstate, struct descriptor *descriptor, uint32_t offs, uint32_t base_type_ops_offs, struct instruction *inst, struct constructed_type *ctype, uint32_t *n_keys, struct constructed_type_key **ctype_keys, bool in_collection)
{
  idl_retcode_t ret;

  assert(n_keys != NULL || in_collection);
  assert(ctype_keys != NULL || in_collection);
  assert(DDS_OP(inst->data.opcode.code) == DDS_OP_ADR);

  /* get the name of the field from the offset instruction, which is the first after the ADR */
  const struct instruction *inst_offs = &ctype->instructions.table[offs + 1];
  assert(inst_offs->type == OFFSET);
  char *type = inst_offs->data.offset.type;
  char *member = inst_offs->data.offset.member;
  assert((!type && !member) || (type && member));
  (void) member;

  if (!in_collection && type == NULL)
  {
    /* Additional ADR for nested list-types (e.g. seq of seqs, seq of arrays, etc) are processed with their main ADR entry,
       but because get_ctype_keys iterates over all instructions, it will also hit the additional ADRs and we need to skip
       them here. */
    assert (DDS_OP_TYPE(inst->data.opcode.code) == DDS_OP_VAL_ARR || DDS_OP_TYPE(inst->data.opcode.code) == DDS_OP_VAL_SEQ || DDS_OP_TYPE(inst->data.opcode.code) == DDS_OP_VAL_BSQ);
    return IDL_RETCODE_OK;
  }

  /* ADR entries must have the KEY flag set at this point, as a result of a key annotation or set because the ADR is
     implicitly a key field. For nested list-types, the KEY flag is set before the current function is called recursively. */
  assert((DDS_OP_FLAGS(inst->data.opcode.code) & DDS_OP_FLAG_KEY));

  /* Members of aggregated types that are wrapped in a collection are not added as separate key entries
     in the top-level type, so when inside a collection, no key meta-data is collected */
  struct constructed_type_key *key = NULL;
  if (!in_collection)
  {
    key = idl_calloc (1, sizeof(*key));
    if (!key)
      return IDL_RETCODE_NO_MEMORY;

    if (*ctype_keys == NULL)
      *ctype_keys = key;
    else {
      struct constructed_type_key *last = *ctype_keys;
      while (last->next)
        last = last->next;
      last->next = key;
    }

    key->ctype = ctype;
    key->offset = offs + base_type_ops_offs;
    key->order = inst->data.opcode.order;
    if (!(key->name = idl_strdup(inst_offs->data.offset.member)))
      return IDL_RETCODE_NO_MEMORY;
  }

  switch (DDS_OP_TYPE(inst->data.opcode.code)) {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_WSTR:
      if (!in_collection)
        (*n_keys)++;
      break;

    case DDS_OP_VAL_EXT: {
      assert(ctype->instructions.table[offs + 2].type == ELEM_OFFSET);
      const idl_node_t *node = ctype->instructions.table[offs + 2].data.inst_offset.node;
      struct constructed_type *csubtype = find_ctype(descriptor, node);
      assert(csubtype);
      if ((ret = get_ctype_keys(pstate, descriptor, csubtype, in_collection ? NULL : &key->sub, n_keys, true, 0, in_collection)))
        return ret;
      break;
    }

    case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
      switch (subtype) {
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_WSTR:
          break;
        case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
          // For a nested collection, jump to ADR for element type
          uint32_t elem_offs = offs + 3 + ((DDS_OP_TYPE(inst->data.opcode.code) == DDS_OP_VAL_BSQ) ? 1 : 0);
          assert(ctype->instructions.table[elem_offs].type == COUPLE);
          uint32_t elem_insn_offs = ctype->instructions.table[elem_offs].data.couple.low;

          // Nested ADRs for collection element types also need a KEY flag
          struct instruction *elem_insn = &ctype->instructions.table[offs + elem_insn_offs];
          if (!(DDS_OP_FLAGS(elem_insn->data.opcode.code) & DDS_OP_FLAG_KEY))
            elem_insn->data.opcode.code |= DDS_OP_FLAG_KEY;

          if ((ret = get_ctype_keys_adr(pstate, descriptor, offs + elem_insn_offs, base_type_ops_offs, elem_insn, ctype, NULL, NULL, true)))
            return ret;
          break;
        }
        case DDS_OP_VAL_STU: {
          // For collection of structs, recurse into the struct type so that the implicit KEY flag is set
          uint32_t elem_offs = offs + 3 + ((DDS_OP_TYPE(inst->data.opcode.code) == DDS_OP_VAL_BSQ) ? 1 : 0);
          assert(ctype->instructions.table[elem_offs].type == ELEM_OFFSET);
          const idl_node_t *node = ctype->instructions.table[elem_offs].data.inst_offset.node;
          struct constructed_type *csubtype = find_ctype(descriptor, node);
          assert(csubtype);
          if ((ret = get_ctype_keys(pstate, descriptor, csubtype, NULL, NULL, true, 0, true)))
            return ret;
          break;
        }
        case DDS_OP_VAL_UNI:
          idl_error (pstate, ctype->node, "Using an array or sequence with a union element type as part of the key is currently unsupported");
          return IDL_RETCODE_UNSUPPORTED;
        case DDS_OP_VAL_EXT:
          abort ();
          return IDL_RETCODE_BAD_PARAMETER;
      }

      // Only when not handling a type wrapped in a collection, increase the key count
      if (!in_collection)
        (*n_keys)++;
      break;
    }

    case DDS_OP_VAL_UNI:
        idl_error (pstate, ctype->node, "Using union type as part of the key is currently unsupported");
        return IDL_RETCODE_UNSUPPORTED;

    case DDS_OP_VAL_STU:
      abort ();
      return IDL_RETCODE_BAD_PARAMETER;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t get_ctype_keys(const idl_pstate_t *pstate, struct descriptor *descriptor, struct constructed_type *ctype, struct constructed_type_key **keys, uint32_t *n_keys, bool parent_is_key, uint32_t base_type_ops_offs, bool in_collection)
{
  idl_retcode_t ret;
  assert(n_keys != NULL || in_collection);
  assert(keys != NULL || in_collection);
  struct constructed_type_key *ctype_keys = NULL;

  for (uint32_t offs = 0; offs < ctype->instructions.count; offs++) {
    struct instruction *inst = &ctype->instructions.table[offs];
    switch (inst->type) {
      case BASE_MEMBERS_OFFSET: {
        struct constructed_type *cbasetype = find_ctype(descriptor, inst->data.inst_offset.node);
        assert (cbasetype);
        /* Get the offset that will be used for ops in the basetype (calculated by adding the base-type offset
           to the current PLM instruction offset, which is within its ctype). A derived type cannot add keys
           (not allowed in IDL) and therefore is not in the offset list, the offset in this list is the offset
           of the key field in the base type. */
        uint32_t ops_offs = (uint32_t) base_type_ops_offs + (uint32_t) inst->data.inst_offset.addr_offs + (uint32_t) inst->data.inst_offset.elem_offs;
        if ((ret = get_ctype_keys(pstate, descriptor, cbasetype, &ctype_keys, n_keys, false, ops_offs, in_collection)) != IDL_RETCODE_OK)
          goto err;
        break;
      }

      case OPCODE:
        if (DDS_OP(inst->data.opcode.code) == DDS_OP_ADR) {
          /* If the parent is a @key member and there are no specific key members in this ctype,
            add the key flag to all members in this type. The serializer will only use these
            key flags in case the top-level member (which is referring to this member)
            also has the key flag set. */
          if (parent_is_key && !ctype->has_key_member)
            inst->data.opcode.code |= DDS_OP_FLAG_KEY;
          if (inst->data.opcode.code & DDS_OP_FLAG_KEY) {
            if (inst->data.opcode.code & DDS_OP_FLAG_OPT) {
              idl_error (pstate, ctype->node, "A member that is part of a key (possibly nested inside a struct) cannot be optional");
              ret = IDL_RETCODE_SYNTAX_ERROR;
              goto err;
            }
            if ((ret = get_ctype_keys_adr(pstate, descriptor, offs, base_type_ops_offs, inst, ctype, n_keys, &ctype_keys, in_collection)) != IDL_RETCODE_OK)
              goto err;
          }
        }
        break;

      default:
        break;
    }

  } /* end for loop through all the ctype's instructions */

  if (!in_collection)
    *keys = ctype_keys;
  return IDL_RETCODE_OK;

err:
  free_ctype_keys(ctype_keys);
  return ret;
}

static idl_retcode_t descriptor_add_key_recursive(const idl_pstate_t *pstate, struct descriptor *descriptor, const struct constructed_type *ctype_top_level, struct constructed_type_key *key, char *name, bool keylist, struct key_offs *offs)
{
  idl_retcode_t ret;
  if (offs->n >= MAX_KEY_OFFS)
    return -1;

  char *name1;
  while (key) {
    if (idl_asprintf(&name1, "%s%s%s", name ? name : "", name ? "." : "", key->name) == -1) {
      ret = IDL_RETCODE_NO_MEMORY;
      goto err;
    }
    offs->val[offs->n] = (uint16_t)key->offset;
    offs->order[offs->n] = key->order;
    offs->n++;
    if (key->sub) {
      if ((ret = descriptor_add_key_recursive(pstate, descriptor, ctype_top_level, key->sub, name1, keylist, offs)) < 0)
        goto err_stash;
    } else {
      uint32_t i = descriptor->n_keys;
      descriptor->keys[i].name = idl_strdup(name1);

      /* Use the key order stored in the constructed_type_key object, which is the member
         id of this key member in its parent constructed_type. */
      if ((ret = stash_key_offset(pstate, &descriptor->key_offsets, nop, name1, offs->n)) < 0)
        goto err_stash;
      for (uint32_t n = 0; n < offs->n; n++) {
        if ((ret = stash_key_offset_val(pstate, &descriptor->key_offsets, nop, offs->val[n], offs->order[n])) < 0)
          goto err_stash;
      }
      descriptor->n_keys++;
    }
    offs->n--;
   idl_free (name1);
    key = key->next;
  }
  return IDL_RETCODE_OK;
err_stash:
 idl_free (name1);
err:
  return ret;
}


static int key_meta_data_cmp (const void *va, const void *vb)
{
  const struct key_meta_data *a = va;
  const struct key_meta_data *b = vb;
  for (uint32_t i = 0; i < a->n_order; i++) {
    assert (i < b->n_order);
    if (a->order[i] != b->order[i])
      return a->order[i] < b->order[i] ? -1 : 1;
  }
  assert (b->n_order == a->n_order);
  return 0;
}

static idl_retcode_t descriptor_add_keys(const idl_pstate_t *pstate, struct constructed_type *ctype, struct constructed_type_key *ctype_keys, struct descriptor *descriptor, uint32_t n_keys, bool keylist)
{
  idl_retcode_t ret;
  struct key_offs offs = { .val = { 0 }, .order = { 0 }, .n = 0 };
  if ((ret = descriptor_add_key_recursive(pstate, descriptor, ctype, ctype_keys, NULL, keylist, &offs)) < 0)
    return ret;
  assert(descriptor->n_keys == n_keys);
  (void) n_keys;
  return IDL_RETCODE_OK;
}

static idl_retcode_t descriptor_init_keys(const idl_pstate_t *pstate, struct constructed_type *ctype, struct constructed_type_key *ctype_keys, struct descriptor *descriptor, uint32_t n_keys, bool keylist)
{
  idl_retcode_t ret;
  uint32_t key_index = 0, offs_len = 0;

  assert(ctype);
  if (n_keys == 0)
    return IDL_RETCODE_OK;
  if (!(descriptor->keys = idl_calloc(n_keys, sizeof(*descriptor->keys))))
    return IDL_RETCODE_NO_MEMORY;

  if ((ret = descriptor_add_keys(pstate, ctype, ctype_keys, descriptor, n_keys, keylist)) < 0) {
   idl_free (descriptor->keys);
    return ret;
  }

  for (uint32_t i = 0; i < descriptor->key_offsets.count; i++) {
    const struct instruction *inst = &descriptor->key_offsets.table[i];
    if (inst->type == KEY_OFFSET) {
      offs_len = inst->data.key_offset.len;
      assert(key_index < descriptor->n_keys);
      assert(descriptor->keys[key_index].name);
      assert(!strcmp(descriptor->keys[key_index].name, inst->data.key_offset.key_name));
      descriptor->keys[key_index].inst_offs = i;
      descriptor->keys[key_index].order = idl_calloc(offs_len, sizeof (*descriptor->keys[key_index].order));
      descriptor->keys[key_index].n_order = offs_len;
      descriptor->keys[key_index].key_idx = key_index;
      key_index++;
    } else {
      assert(inst->type == KEY_OFFSET_VAL);
      assert(offs_len > 0);
      assert(key_index > 0);
      uint32_t order = inst->data.key_offset_val.order;
      descriptor->keys[key_index - 1].order[descriptor->keys[key_index - 1].n_order - offs_len] = order;
      offs_len--;
    }
  }
  assert (key_index == descriptor->n_keys);

  // sort keys by member id (scoped within the containing aggregated type)
  qsort(descriptor->keys, descriptor->n_keys, sizeof (*descriptor->keys), key_meta_data_cmp);

  return IDL_RETCODE_OK;
}

static void descriptor_keys_free(struct key_meta_data *keys, uint32_t n_keys)
{
  for (uint32_t k = 0; k < n_keys; k++) {
    if (keys[k].order) {
     idl_free (keys[k].name);
     idl_free (keys[k].order);
      keys[k].order = NULL;
    }
  }
 idl_free (keys);
}

static int print_keys(FILE *fp, struct descriptor *descriptor, uint32_t offset)
{
  char *typestr = NULL;
  const char *fmt;

  if (descriptor->n_keys == 0)
    return 0;
  if (IDL_PRINT(&typestr, print_type, descriptor->topic) < 0)
    goto err_type;

  fmt = "static const dds_key_descriptor_t %s_keys[%"PRIu32"] =\n{\n";
  if (idl_fprintf(fp, fmt, typestr, descriptor->n_keys) < 0)
    goto err_print;
  const char *sep = "";
  fmt = "%s  { \"%s\", %"PRIu32", %"PRIu32" }";
  for (uint32_t k=0; k < descriptor->n_keys; k++) {
    if (idl_fprintf(fp, fmt, sep, descriptor->keys[k].name, offset + descriptor->keys[k].inst_offs, descriptor->keys[k].key_idx) < 0)
      goto err_print;
    sep = ",\n";
  }
  if (fputs("\n};\n\n", fp) < 0)
    goto err_print;
 idl_free (typestr);
  return 0;

err_print:
 idl_free (typestr);
err_type:
  return -1;
}


static void free_ctype_memberids(struct constructed_type_memberid *mids)
{
  struct constructed_type_memberid *tmp = mids, *tmp1;
  while (tmp) {
    tmp1 = tmp;
    tmp = tmp->next;
    idl_free (tmp1);
  }
}

static idl_retcode_t get_ctype_memberids(const idl_pstate_t *pstate, struct descriptor *descriptor, struct constructed_type *ctype, struct constructed_type_memberid **ctype_mids, struct visited_ctype *visited_ctypes);

static idl_retcode_t get_ctype_memberids_adr(const idl_pstate_t *pstate, struct descriptor *descriptor, uint32_t offs, struct instruction *inst, struct constructed_type *ctype, struct constructed_type_memberid **ctype_mids, struct visited_ctype *visited_ctypes)
{
  idl_retcode_t ret;

  assert(DDS_OP(inst->data.opcode.code) == DDS_OP_ADR);

  /* get the name of the field from the offset instruction, which is the first after the ADR */
  const struct instruction *inst_offs = &ctype->instructions.table[offs + 1];
  assert(inst_offs->type == OFFSET);

  if (inst->data.opcode.code & DDS_OP_FLAG_OPT)
  {
    struct constructed_type_memberid *mid = idl_calloc (1, sizeof(*mid));
    if (mid == NULL)
      return IDL_RETCODE_NO_MEMORY;
    mid->ctype = ctype;
    mid->rel_offs = (int16_t) offs;
    mid->value = inst_offs->data.offset.member_id;
    mid->type = inst_offs->data.offset.type;
    mid->member = inst_offs->data.offset.member;

    if (*ctype_mids == NULL)
    {
      *ctype_mids = mid;
    }
    else
    {
      struct constructed_type_memberid *last = *ctype_mids;
      while (!(last->ctype == mid->ctype && last->value == mid->value) && last->next)
        last = last->next;
      if (last->next == NULL)
        last->next = mid;
      else
        idl_free (mid);
    }
  }

  const enum dds_stream_typecode type = DDS_OP_TYPE(inst->data.opcode.code);
  switch (type) {
    case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
    case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_WSTR:
      break;

    case DDS_OP_VAL_EXT: {
      assert(ctype->instructions.table[offs + 2].type == ELEM_OFFSET);
      const idl_node_t *node = ctype->instructions.table[offs + 2].data.inst_offset.node;
      struct constructed_type *csubtype = find_ctype(descriptor, node);
      assert(csubtype);
      if ((ret = get_ctype_memberids(pstate, descriptor, csubtype, ctype_mids, visited_ctypes)))
        return ret;
      break;
    }

    case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
      const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
      uint32_t offs_insn_offs = 3;
      if (type == DDS_OP_VAL_BSQ)
        offs_insn_offs++;
      switch (subtype) {
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_WCHAR: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY:
        case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: case DDS_OP_VAL_BST: case DDS_OP_VAL_STR: case DDS_OP_VAL_BWSTR: case DDS_OP_VAL_WSTR:
          break;
        case DDS_OP_VAL_ARR: case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
          assert(ctype->instructions.table[offs + offs_insn_offs].type == COUPLE);
          uint16_t elem_addr_offs = ctype->instructions.table[offs + offs_insn_offs].data.couple.low;
          struct instruction *elem_inst = &ctype->instructions.table[offs + elem_addr_offs];
          if ((ret = get_ctype_memberids_adr(pstate, descriptor, offs + elem_addr_offs, elem_inst, ctype, ctype_mids, visited_ctypes)))
            return ret;
          break;
        }
        case DDS_OP_VAL_STU: case DDS_OP_VAL_UNI: {
          assert(ctype->instructions.table[offs + offs_insn_offs].type == ELEM_OFFSET);
          const idl_node_t *node = ctype->instructions.table[offs + offs_insn_offs].data.inst_offset.node;
          struct constructed_type *csubtype = find_ctype(descriptor, node);
          assert(csubtype);
          if ((ret = get_ctype_memberids(pstate, descriptor, csubtype, ctype_mids, visited_ctypes)))
            return ret;
          break;
        }
        case DDS_OP_VAL_EXT:
          abort ();
          return IDL_RETCODE_BAD_PARAMETER;
      }
      break;
    }

    case DDS_OP_VAL_UNI:
      assert(ctype->instructions.table[offs + 2].type == SINGLE);
      assert(ctype->instructions.table[offs + 3].type == COUPLE);
      const uint32_t num_cases = ctype->instructions.table[offs + 2].data.single;
      uint32_t jeq_offs = offs + ctype->instructions.table[offs + 3].data.couple.low;
      for (uint32_t c = 0; c < num_cases; c++)
      {
        if (ctype->instructions.table[jeq_offs].type == OPCODE)
        {
          // FIXME: SEQ/BSQ/ARR
        }
        else if (ctype->instructions.table[jeq_offs].type == JEQ_OFFSET)
        {
          const enum dds_stream_typecode subtype = DDS_OP_TYPE(ctype->instructions.table[jeq_offs].data.inst_offset.inst.opcode);
          assert (subtype == DDS_OP_VAL_STU || subtype == DDS_OP_VAL_UNI);
          const idl_node_t *node = ctype->instructions.table[jeq_offs].data.inst_offset.node;
          struct constructed_type *csubtype = find_ctype(descriptor, node);
          assert(csubtype);
          if ((ret = get_ctype_memberids(pstate, descriptor, csubtype, ctype_mids, visited_ctypes)))
            return ret;
        }
        else
          abort ();

        // move to next JEQ4
        jeq_offs += 4;
      }
      break;
    case DDS_OP_VAL_STU:
      return IDL_RETCODE_BAD_PARAMETER;
  }

  return IDL_RETCODE_OK;
}


static idl_retcode_t get_ctype_memberids(const idl_pstate_t *pstate, struct descriptor *descriptor, struct constructed_type *ctype, struct constructed_type_memberid **ctype_mids, struct visited_ctype *visited_ctypes)
{
  idl_retcode_t ret;
  struct visited_ctype *vc = visited_ctypes;
  while (true)
  {
    if (vc->ctype == ctype)
      return IDL_RETCODE_OK;
    else if (vc->next != NULL)
      vc = vc->next;
    else
    {
      vc->next = idl_calloc (1, sizeof (*vc->next));
      vc->next->ctype = ctype;
      break;
    }
  }

  for (uint32_t offs = 0; offs < ctype->instructions.count; offs++)
  {
    struct instruction *inst = &ctype->instructions.table[offs];
    if (inst->type == OPCODE && DDS_OP(inst->data.opcode.code) == DDS_OP_ADR)
    {
      if ((ret = get_ctype_memberids_adr(pstate, descriptor, offs, inst, ctype, ctype_mids, visited_ctypes)) != IDL_RETCODE_OK)
        goto err;
    }
  }
  return IDL_RETCODE_OK;

err:
  free_ctype_memberids(*ctype_mids);
  return ret;
}



#define MAX_FLAGS 30
static int print_flags(FILE *fp, struct descriptor *descriptor, bool type_info)
{
  const char *fmt;
  const char *vec[MAX_FLAGS] = { NULL };
  size_t cnt, len = 0;

  if (descriptor->flags & DDS_TOPIC_RESTRICT_DATA_REPRESENTATION)
    vec[len++] = "DDS_TOPIC_RESTRICT_DATA_REPRESENTATION";

  bool fixed_size = true;
  for (struct constructed_type *ctype = descriptor->constructed_types; ctype && fixed_size; ctype = ctype->next) {
    for (uint32_t op = 0; op < ctype->instructions.count && fixed_size; op++) {
      struct instruction i = ctype->instructions.table[op];
      if (i.type != OPCODE)
        continue;

      uint32_t typecode = DDS_OP_TYPE(i.data.opcode.code);
      if (typecode == DDS_OP_VAL_STR || typecode == DDS_OP_VAL_BST ||
          typecode == DDS_OP_VAL_WSTR || typecode == DDS_OP_VAL_BWSTR ||
          typecode == DDS_OP_VAL_SEQ || typecode == DDS_OP_VAL_BSQ)
        fixed_size = false;
      if (typecode == DDS_OP_VAL_ARR)
      {
        uint32_t subtypecode = DDS_OP_SUBTYPE(i.data.opcode.code);
        if (subtypecode == DDS_OP_VAL_STR || subtypecode == DDS_OP_VAL_BST ||
            subtypecode == DDS_OP_VAL_WSTR || subtypecode == DDS_OP_VAL_BWSTR ||
            subtypecode == DDS_OP_VAL_SEQ || subtypecode == DDS_OP_VAL_BSQ)
          fixed_size = false;
      }
    }
  }

  if (fixed_size)
    vec[len++] = "DDS_TOPIC_FIXED_SIZE";

#ifdef DDS_HAS_TYPELIB
  if (type_info)
    vec[len++] = "DDS_TOPIC_XTYPES_METADATA";
#else
  assert(!type_info);
  (void) type_info;
#endif

  if (!len)
    vec[len++] = "0u";

  for (cnt=0, fmt="%s"; cnt < len; cnt++, fmt=" | %s") {
    if (idl_fprintf(fp, fmt, vec[cnt]) < 0)
      return -1;
  }

  return fputs(",\n", fp) < 0 ? -1 : 0;
}

static int print_descriptor(FILE *fp, struct descriptor *descriptor, bool type_info, uint32_t mid_table_offs)
{
  char *name, *type;
  const char *fmt;

  if (IDL_PRINTA(&name, print_scoped_name, descriptor->topic) < 0)
    return -1;
  if (IDL_PRINTA(&type, print_type, descriptor->topic) < 0)
    return -1;
  fmt = "const dds_topic_descriptor_t %1$s_desc =\n{\n"
        "  .m_size = sizeof (%1$s),\n" /* size of type */
        "  .m_align = dds_alignof (%1$s),\n" /* alignment */
        "  .m_flagset = ";
  if (idl_fprintf(fp, fmt, type) < 0)
    return -1;
  if (print_flags(fp, descriptor, type_info) < 0)
    return -1;
  fmt = "  .m_nkeys = %1$"PRIu32"u,\n" /* number of keys */
        "  .m_typename = \"%2$s\",\n"; /* fully qualified name in IDL */
  if (idl_fprintf(fp, fmt, descriptor->n_keys, name) < 0)
    return -1;

  /* key array */
  if (descriptor->n_keys)
    fmt = "  .m_keys = %1$s_keys,\n";
  else
    fmt = "  .m_keys = NULL,\n";
  if (idl_fprintf(fp, fmt, type) < 0)
    return -1;

  fmt = "  .m_nops = %1$"PRIu32",\n" /* number of ops */
        "  .m_ops = %2$s_ops,\n" /* ops array */
        "  .m_meta = \"\""; /* OpenSplice metadata */
  if (idl_fprintf(fp, fmt, descriptor->n_opcodes, type) < 0)
    return -1;

#ifdef DDS_HAS_TYPELIB
  if (type_info) {
    fmt = ",\n"
          "  .type_information = { .data = TYPE_INFO_CDR_%1$s, .sz = TYPE_INFO_CDR_SZ_%1$s },\n" /* CDR serialized XTypes TypeInformation object */
          "  .type_mapping = { .data = TYPE_MAP_CDR_%1$s, .sz = TYPE_MAP_CDR_SZ_%1$s }"; /* CDR serialized type id to type object mapping */
    if (idl_fprintf(fp, fmt, type) < 0)
      return -1;
  }
#endif

  if (descriptor->flags & DDS_TOPIC_RESTRICT_DATA_REPRESENTATION) {
    if (idl_fprintf(fp, ",\n  .restrict_data_representation = ") < 0)
      return -1;
    bool first = true;
    if (descriptor->data_representations & IDL_DATAREPRESENTATION_FLAG_XCDR1) {
      if (idl_fprintf(fp, "(1u << DDS_DATA_REPRESENTATION_XCDR1)") < 0)
        return -1;
      first = false;
    }
    if (descriptor->data_representations & IDL_DATAREPRESENTATION_FLAG_XCDR2) {
      if (!first && idl_fprintf(fp, " | ") < 0)
        return -1;
      if (idl_fprintf(fp, "(1u << DDS_DATA_REPRESENTATION_XCDR2)") < 0)
        return -1;
    }
  }

  fmt = ",\n"
        "  .m_mid_table_offs = %u";
  idl_fprintf (fp, fmt, mid_table_offs);

  if (idl_fprintf(fp, "\n};\n\n") < 0)
    return -1;

  return 0;
}

static int print_cdrstream_descriptor(FILE *fp, struct descriptor *descriptor, uint32_t kof_offs)
{
  char *name, *type;
  const char *fmt;

  if (IDL_PRINTA(&name, print_scoped_name, descriptor->topic) < 0)
    return -1;
  if (IDL_PRINTA(&type, print_type, descriptor->topic) < 0)
    return -1;
  fmt = "const struct dds_cdrstream_desc %1$s_cdrstream_desc =\n{\n"
        "  .size = sizeof (%1$s),\n" /* size of type */
        "  .align = dds_alignof (%1$s),\n" /* alignment */
        "  .flagset = ";
  if (idl_fprintf(fp, fmt, type) < 0)
    return -1;
  if (print_flags(fp, descriptor, false) < 0)
    return -1;
  fmt = "  .keys = {\n"
        "    .nkeys = %1$"PRIu32"u,\n";
  if (idl_fprintf(fp, fmt, descriptor->n_keys) < 0)
    return -1;
  if (descriptor->n_keys == 0) {
    if (idl_fprintf(fp, "    .keys = NULL\n") < 0)
      return -1;
  } else {
    if (idl_fprintf(fp, "    .keys = (struct dds_cdrstream_desc_key[]){\n") < 0)
      return -1;
    const char *sep = "";
    const char *keyfmt = "%s      { %"PRIu32", %"PRIu32" }";
    for (uint32_t k=0; k < descriptor->n_keys; k++) {
      if (idl_fprintf(fp, keyfmt, sep, kof_offs + descriptor->keys[k].inst_offs, descriptor->keys[k].key_idx) < 0)
        return -1;
      sep = ",\n";
    }
    if (idl_fprintf(fp, "\n    }\n") < 0)
      return -1;
  }
  if (idl_fprintf(fp, "  },\n") < 0)
    return -1;
  fmt = "  .ops = {\n"
        "    .nops = (uint32_t) (sizeof(%1$s_ops) / sizeof(%1$s_ops[0])),\n"
        "    .ops = (uint32_t *) %1$s_ops\n"
        "  },\n";
  if (idl_fprintf(fp, fmt, type) < 0)
    return -1;
  // opt_size_xcdr... = 0 means the serializer won't use memcpy, this is not
  // a problem for our purpose and avoids making the output dependent on
  // platform-specific details (such as alignment)
  fmt = "  .opt_size_xcdr1 = 0,\n"
        "  .opt_size_xcdr2 = 0\n"
        "};\n\n";
  if (idl_fprintf(fp, "%s", fmt) < 0)
    return -1;
  return 0;
}

static idl_retcode_t
resolve_offsets(struct descriptor *descriptor)
{
  /* set instruction offset for each type in descriptor */
  {
    uint32_t offs = 0;
    for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
      /* confirm that type is complete */
      assert (ctype->node);
      ctype->offset = offs;
      offs += ctype->instructions.count;
    }
  }

  /* set offset for each ELEM_OFFSET, JEQ_OFFSET and BASE_MEMBERS_OFFSET instruction */
  for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
    for (size_t op = 0; op < ctype->instructions.count; op++) {
      if (ctype->instructions.table[op].type == ELEM_OFFSET || ctype->instructions.table[op].type == JEQ_OFFSET || ctype->instructions.table[op].type == BASE_MEMBERS_OFFSET)
      {
        struct instruction *inst = &ctype->instructions.table[op];
        struct constructed_type *ctype1 = find_ctype(descriptor, inst->data.inst_offset.node);
        assert (ctype1);
        assert(ctype1->offset <= INT32_MAX);
        int32_t offs = (int32_t)ctype1->offset - ((int32_t)ctype->offset + inst->data.inst_offset.addr_offs);
        assert(offs >= INT16_MIN);
        assert(offs <= INT16_MAX);
        inst->data.inst_offset.elem_offs = (int16_t)offs;
      }
    }
  }
  return IDL_RETCODE_OK;
}

static void
instructions_fini(struct instructions *instructions)
{
  for (size_t i = 0; i < instructions->count; i++) {
    struct instruction *inst = &instructions->table[i];
    assert(inst);
    /* MSVC incorrectly reports this as uninitialised variables */
    IDL_WARNING_MSVC_OFF (6001);
    switch (inst->type) {
      case OFFSET:
        if (inst->data.offset.member)
         idl_free (inst->data.offset.member);
        if (inst->data.offset.type)
         idl_free (inst->data.offset.type);
        break;
      case MEMBER_SIZE:
        if (inst->data.size.type)
         idl_free (inst->data.size.type);
        break;
      case CONSTANT:
        if (inst->data.constant.value)
         idl_free (inst->data.constant.value);
        break;
      case KEY_OFFSET:
        if (inst->data.key_offset.key_name)
         idl_free (inst->data.key_offset.key_name);
      default:
        break;
    }
    IDL_WARNING_MSVC_ON (6001);
  }
}

static void
ctype_fini(struct constructed_type *ctype)
{
  instructions_fini(&ctype->instructions);
  if (ctype->instructions.table)
   idl_free (ctype->instructions.table);
}

void
descriptor_fini(struct descriptor *descriptor)
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 6001)
#endif
  struct constructed_type *ctype1, *ctype = descriptor->constructed_types;
  while (ctype) {
    ctype_fini(ctype);
    ctype1 = ctype->next;
   idl_free (ctype);
    ctype = ctype1;
  }
  instructions_fini(&descriptor->key_offsets);
  descriptor_keys_free(descriptor->keys, descriptor->n_keys);
  idl_free (descriptor->key_offsets.table);
  idl_free (descriptor->member_ids.table);
  assert(!descriptor->type_stack);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}

idl_retcode_t
generate_descriptor_impl(
  const idl_pstate_t *pstate,
  const idl_node_t *topic_node,
  struct descriptor *descriptor)
{
  idl_retcode_t ret;
  idl_visitor_t visitor;

  /* must be invoked for topics only, so structs and unions */
  assert(idl_is_struct(topic_node) || idl_is_union(topic_node));

  memset(descriptor, 0, sizeof(*descriptor));
  descriptor->topic = topic_node;

  memset(&visitor, 0, sizeof(visitor));
  visitor.visit = IDL_DECLARATOR | IDL_SEQUENCE | IDL_STRUCT | IDL_UNION | IDL_SWITCH_TYPE_SPEC | IDL_CASE | IDL_FORWARD | IDL_MEMBER | IDL_BITMASK | IDL_ENUM | IDL_INHERIT_SPEC;
  visitor.accept[IDL_ACCEPT_SEQUENCE] = &emit_sequence;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_CASE] = &emit_case;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;
  visitor.accept[IDL_ACCEPT_FORWARD] = &emit_forward;
  visitor.accept[IDL_ACCEPT_MEMBER] = &emit_member;
  visitor.accept[IDL_ACCEPT_BITMASK] = &emit_bitmask;
  visitor.accept[IDL_ACCEPT_ENUM] = &emit_enum;
  visitor.accept[IDL_ACCEPT_INHERIT_SPEC] = &emit_inherit_spec;

  if ((ret = idl_visit(pstate, descriptor->topic, &visitor, descriptor))) {
    /* Clear the type stack in case an error occured during visit, so that the check
      for an empty type stack in descriptor_fini will pass */
    while (descriptor->type_stack) {
      while (descriptor->type_stack->fields)
        pop_field(descriptor);
      pop_type(descriptor);
    }
    goto err;
  }
  if ((ret = resolve_offsets(descriptor)) < 0)
    goto err;

  struct constructed_type_key *ctype_keys;
  struct constructed_type *ctype = find_ctype(descriptor, descriptor->topic);
  assert(ctype);
  uint32_t n_keys = 0;
  if ((ret = get_ctype_keys(pstate, descriptor, ctype, &ctype_keys, &n_keys, false, 0, false)) != IDL_RETCODE_OK)
    goto err;
  if ((ret = descriptor_init_keys(pstate, ctype, ctype_keys, descriptor, n_keys, (pstate->config.flags & IDL_FLAG_KEYLIST) != 0)) < 0)
    goto err;
  free_ctype_keys(ctype_keys);

  struct visited_ctype visited_ctypes = { NULL, NULL };
  struct constructed_type_memberid *ctype_mids = NULL;
  if ((ret = get_ctype_memberids(pstate, descriptor, ctype, &ctype_mids, &visited_ctypes)) != IDL_RETCODE_OK)
    goto err;
  for (struct constructed_type_memberid *mid = ctype_mids; mid != NULL; mid = mid->next)
  {
    if ((ret = add_member_id_entry (pstate, descriptor, mid)) < 0)
      goto err;
  }
  if (ctype_mids != NULL)
  {
    free_ctype_memberids(ctype_mids);
    if ((ret = close_member_id_table (pstate, descriptor)) < 0)
      goto err;
  }
  struct visited_ctype *vc = visited_ctypes.next;
  while (vc != NULL)
  {
    struct visited_ctype *vcn = vc->next;
    idl_free (vc);
    vc = vcn;
  }

  /* set data representation restriction flag and mask (ignore unsupported data representations) */
  allowable_data_representations_t dr = idl_allowable_data_representations(descriptor->topic);
  if (dr != IDL_ALLOWABLE_DATAREPRESENTATION_DEFAULT && (dr & (IDL_DATAREPRESENTATION_FLAG_XCDR1 | IDL_DATAREPRESENTATION_FLAG_XCDR2))) {
    descriptor->flags |= DDS_TOPIC_RESTRICT_DATA_REPRESENTATION;
    descriptor->data_representations = dr;
  }

err:
  if (ret < 0)
    descriptor_fini(descriptor);
  return ret;
}

idl_retcode_t
generate_descriptor(
  const idl_pstate_t *pstate,
  struct generator *generator,
  const idl_node_t *node)
{
  idl_retcode_t ret;
  struct descriptor descriptor;
  uint32_t kof_offs, mid_table_offs;

  if ((ret = generate_descriptor_impl(pstate, node, &descriptor)) < 0)
    goto err_gen;
  if (print_opcodes(generator->source.handle, &descriptor, &kof_offs, &mid_table_offs) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
  if (print_keys(generator->source.handle, &descriptor, kof_offs) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
#ifdef DDS_HAS_TYPELIB
  if (generator->config.c.generate_type_info && print_type_meta_ser(generator->source.handle, pstate, node) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
  if (print_descriptor(generator->source.handle, &descriptor, generator->config.c.generate_type_info, mid_table_offs) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
#else
  if (print_descriptor(generator->source.handle, &descriptor, false, mid_table_offs) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
#endif
  if (generator->config.generate_cdrstream_desc && print_cdrstream_descriptor(generator->source.handle, &descriptor, kof_offs) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }

err_print:
  descriptor_fini(&descriptor);
err_gen:
  return ret;
}
