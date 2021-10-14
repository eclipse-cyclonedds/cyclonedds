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
#include <stdint.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "dds/features.h"
#include "idl/misc.h"
#include "idl/print.h"
#include "idl/processor.h"
#include "idl/stream.h"
#include "idl/string.h"

#include "generator.h"
#include "descriptor.h"
#include "dds/ddsc/dds_opcodes.h"

#define TYPE (16)
#define SUBTYPE (8)

#define MAX_SIZE (16)

static const uint16_t nop = UINT16_MAX;

struct alignment {
  int value;
  int ordering;
  const char *rendering;
};

static const struct alignment alignments[] = {
#define ALIGNMENT_1BY (&alignments[0])
  { 1, 0, "1u" },
#define ALIGNMENT_2BY (&alignments[1])
  { 2, 2, "2u" },
#define ALIGNMENT_4BY (&alignments[2])
  { 4, 4, "4u" },
#define ALIGNMENT_PTR (&alignments[3])
  { 0, 6, "sizeof (char *)" },
#define ALIGNMENT_8BY (&alignments[4])
  { 8, 8, "8u" }
};

static const struct alignment *
max_alignment(const struct alignment *a, const struct alignment *b)
{
  if (!a)
    return b;
  if (!b)
    return a;
  return b->ordering > a->ordering ? b : a;
}

static idl_retcode_t push_field(
  struct descriptor *descriptor, const void *node, struct field **fieldp)
{
  struct stack_type *stype;
  struct field *field;
  assert(descriptor);
  assert(idl_is_declarator(node) ||
         idl_is_switch_type_spec(node) ||
         idl_is_case(node));
  stype = descriptor->type_stack;
  assert(stype);
  if (!(field = calloc(1, sizeof(*field))))
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
  free(field);
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
  if (!(stype = calloc(1, sizeof(*stype))))
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
  free(stype);
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
    if (!(table = realloc(table, size * sizeof(*table))))
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
      if (table[i].type == ELEM_OFFSET || table[i].type == JEQ_OFFSET || table[i].type == MEMBER_OFFSET)
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
  uint32_t typecode = 0;
  struct instruction inst = { OPCODE, { .opcode = { .code = code, .order = order } } };
  const struct alignment *alignment = NULL;

  descriptor->n_opcodes++;
  switch (DDS_OP(code)) {
    case DDS_OP_ADR:
    case DDS_OP_JEQ:
    case DDS_OP_JEQ4:
      typecode = DDS_OP_TYPE(code);
      if (typecode == DDS_OP_VAL_ARR)
        typecode = DDS_OP_SUBTYPE(code);
      break;
    default:
      return stash_instruction(pstate, instructions, index, &inst);
  }

  switch (typecode) {
    case DDS_OP_VAL_STR:
    case DDS_OP_VAL_SEQ:
      alignment = ALIGNMENT_PTR;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_BST:
      alignment = ALIGNMENT_1BY;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_EXT:
      alignment = ALIGNMENT_1BY;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE;
      break;
    case DDS_OP_VAL_8BY:
      alignment = ALIGNMENT_8BY;
      break;
    case DDS_OP_VAL_4BY:
      alignment = ALIGNMENT_4BY;
      break;
    case DDS_OP_VAL_2BY:
      alignment = ALIGNMENT_2BY;
      break;
    case DDS_OP_VAL_1BY:
      alignment = ALIGNMENT_1BY;
      break;
    case DDS_OP_VAL_UNI:
      /* strictly speaking a topic with a union can be optimized if all
         members have the same size, and if the non-basetype members are all
         optimizable themselves, and the alignment of the discriminant is not
         less than the alignment of the members */
      alignment = ALIGNMENT_1BY;
      descriptor->flags |= DDS_TOPIC_NO_OPTIMIZE | DDS_TOPIC_CONTAINS_UNION;
      break;
    default:
      break;
  }

  descriptor->alignment = max_alignment(descriptor->alignment, alignment);
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const struct field *field)
{
  size_t cnt, pos, len, levels;
  const char *ident;
  const struct field *fld;
  struct instruction inst = { OFFSET, { .offset = { NULL, NULL } } };

  if (!field)
    return stash_instruction(pstate, instructions, index, &inst);

  assert(field);

  len = 0;
  for (fld = field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
    else
      ident = idl_identifier(fld->node);
    len += strlen(ident);
    if (!fld->previous)
      break;
    len += strlen(".");
  }

  pos = len;
  if (!(inst.data.offset.member = malloc(len + 1)))
    goto err_member;

  inst.data.offset.member[pos] = '\0';
  for (fld=field; fld; fld = fld->previous) {
    if (idl_is_switch_type_spec(fld->node))
      ident = "_d";
    else if (idl_is_case(fld->node))
      ident = "_u";
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

  if (stash_instruction(pstate, instructions, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.offset.type);
err_type:
  free(inst.data.offset.member);
err_member:
  return IDL_RETCODE_NO_MEMORY;
}

static idl_retcode_t
stash_key_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, char *key_name, uint16_t length, uint16_t key_size)
{
  struct instruction inst = { KEY_OFFSET, { .key_offset = { .len = length, .key_size = key_size } } };
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
stash_member_offset(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, int16_t addr_offs)
{
  struct instruction inst = { MEMBER_OFFSET, { .inst_offset = { .addr_offs = addr_offs } } };
  return stash_instruction(pstate, instructions, index, &inst);
}

static idl_retcode_t
stash_size(
  const idl_pstate_t *pstate, struct instructions *instructions, uint32_t index, const void *node, bool ext)
{
  const idl_type_spec_t *type_spec;
  struct instruction inst = { SIZE, { .size = { NULL } } };

  if (idl_is_sequence(node) || ext) {
    type_spec = idl_type_spec(node);

    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t dims = ((const idl_string_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "char[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_string(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("char *")))
        goto err_type;
    } else {
      if (IDL_PRINT(&inst.data.size.type, print_type, type_spec) < 0)
        goto err_type;
    }
  } else {
    const idl_type_spec_t *array = NULL;

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
      if (idl_is_sequence(type_spec))
        type_spec = array;
    } else {
      assert(idl_is_array(node));
      type_spec = idl_type_spec(node);
    }

    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      uint32_t dims = ((const idl_string_t *)type_spec)->maximum;
      if (idl_asprintf(&inst.data.size.type, "char[%"PRIu32"]", dims) == -1)
        goto err_type;
    } else if (idl_is_string(type_spec)) {
      if (!(inst.data.size.type = idl_strdup("char *")))
        goto err_type;
    } else if (idl_is_array(type_spec)) {
      char *typestr = NULL;
      size_t len, pos;
      const idl_const_expr_t *const_expr;

      if (IDL_PRINT(&typestr, print_type, type_spec) < 0)
        goto err_type;

      len = pos = strlen(typestr);
      const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
      assert(const_expr);
      for (; const_expr; const_expr = idl_next(const_expr), len += 3)
        /* do nothing */;

      inst.data.size.type = malloc(len + 1);
      if (inst.data.size.type)
        memcpy(inst.data.size.type, typestr, pos);
      free(typestr);
      if (!inst.data.size.type)
        goto err_type;

      const_expr = ((const idl_declarator_t *)type_spec)->const_expr;
      assert(const_expr);
      for (; const_expr; const_expr = idl_next(const_expr), pos += 3)
        memmove(inst.data.size.type + pos, "[0]", 3);
      inst.data.size.type[pos] = '\0';
    } else {
      if (IDL_PRINT(&inst.data.size.type, print_type, type_spec) < 0)
        goto err_type;
    }
  }

  if (stash_instruction(pstate, instructions, index, &inst))
    goto err_stash;

  return IDL_RETCODE_OK;
err_stash:
  free(inst.data.size.type);
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
  } else {
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
  free(inst.data.constant.value);
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

static uint32_t typecode(const idl_type_spec_t *type_spec, uint32_t shift, bool struct_union_ext)
{
  assert(shift == 8 || shift == 16);
  if (idl_is_array(type_spec))
    return ((uint32_t)DDS_OP_VAL_ARR << shift);
  type_spec = idl_unalias(type_spec, 0u);
  if (idl_is_forward(type_spec))
    type_spec = ((const idl_forward_t *)type_spec)->definition;
  assert(!idl_is_typedef(type_spec));
  switch (idl_type(type_spec)) {
    case IDL_CHAR:
      return ((uint32_t)DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_BOOL:
      return ((uint32_t)DDS_OP_VAL_1BY << shift);
    case IDL_INT8:
      return ((uint32_t)DDS_OP_VAL_1BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_OCTET:
    case IDL_UINT8:
      return ((uint32_t)DDS_OP_VAL_1BY << shift);
    case IDL_SHORT:
    case IDL_INT16:
      return ((uint32_t)DDS_OP_VAL_2BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_USHORT:
    case IDL_UINT16:
      return ((uint32_t)DDS_OP_VAL_2BY << shift);
    case IDL_LONG:
    case IDL_INT32:
      return ((uint32_t)DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_ULONG:
    case IDL_UINT32:
      return ((uint32_t)DDS_OP_VAL_4BY << shift);
    case IDL_LLONG:
    case IDL_INT64:
      return ((uint32_t)DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_SGN;
    case IDL_ULLONG:
    case IDL_UINT64:
      return ((uint32_t)DDS_OP_VAL_8BY << shift);
    case IDL_FLOAT:
      return ((uint32_t)DDS_OP_VAL_4BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
    case IDL_DOUBLE:
      return ((uint32_t)DDS_OP_VAL_8BY << shift) | (uint32_t)DDS_OP_FLAG_FP;
    case IDL_LDOUBLE:
      /* long doubles are not supported (yet) */
      abort();
    case IDL_STRING:
      if (idl_is_bounded(type_spec))
        return ((uint32_t)DDS_OP_VAL_BST << shift);
      return ((uint32_t)DDS_OP_VAL_STR << shift);
    case IDL_SEQUENCE:
      /* bounded sequences are not supported (yet) */
      if (idl_is_bounded(type_spec))
        abort();
      return ((uint32_t)DDS_OP_VAL_SEQ << shift);
    case IDL_ENUM:
      return ((uint32_t)DDS_OP_VAL_4BY << shift);
    case IDL_UNION:
      return ((uint32_t)(struct_union_ext ? DDS_OP_VAL_EXT : DDS_OP_VAL_UNI) << shift);
    case IDL_STRUCT:
      return ((uint32_t)(struct_union_ext ? DDS_OP_VAL_EXT : DDS_OP_VAL_STU) << shift);
    case IDL_BITMASK:
    {
      uint32_t bit_bound = idl_bound(type_spec);
      if (bit_bound <= 8)
        return ((uint32_t)DDS_OP_VAL_1BY << shift);
      else if (bit_bound <= 16)
        return ((uint32_t)DDS_OP_VAL_2BY << shift);
      else if (bit_bound <= 32)
        return ((uint32_t)DDS_OP_VAL_4BY << shift);
      else
        return ((uint32_t)DDS_OP_VAL_8BY << shift);
    }
    default:
      abort ();
      break;
  }
  return 0u;
}

static struct constructed_type *
find_ctype(const struct descriptor *descriptor, const void *node)
{
  struct constructed_type *ctype = descriptor->constructed_types;
  const void *node1;
  if (idl_is_forward(node))
    node1 = ((const idl_forward_t *)node)->definition;
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

  if (!(ctype1 = calloc(1, sizeof (*ctype1))))
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

  (void)pstate;
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

    type_spec = idl_unalias(idl_type_spec(node), 0u);
    if (idl_is_forward(type_spec))
      type_spec = ((const idl_forward_t *)type_spec)->definition;

    if (idl_is_empty(type_spec)) {
      /* In case of an empty type (a struct without members), stash no-ops for the
         case labels so that offset to type ops for non-simple inline cases is correct.
         FIXME: This needs a better solution... */
      for (label = _case->labels; label; label = idl_next(label)) {
        off = stype->offset + 2 + (stype->label * 4);
        if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, off++, DDS_OP_RTS, 0u))
            || (ret = stash_opcode(pstate, descriptor, &ctype->instructions, off++, DDS_OP_RTS, 0u))
            || (ret = stash_opcode(pstate, descriptor, &ctype->instructions, off++, DDS_OP_RTS, 0u))
            || (ret = stash_opcode(pstate, descriptor, &ctype->instructions, off, DDS_OP_RTS, 0u)))
          return ret;
      }
      return IDL_RETCODE_OK;
    }

    if (idl_is_array(_case->declarator)) {
      opcode |= DDS_OP_TYPE_ARR;
      case_type = IN_UNION;
    } else {
      opcode |= typecode(type_spec, TYPE, false);
      if (idl_is_struct(type_spec) || idl_is_union(type_spec))
        case_type = EXTERNAL;
      else if (idl_is_array(type_spec) || (idl_is_string(type_spec) && idl_is_bounded(type_spec)) || idl_is_sequence(type_spec) || idl_is_enum(type_spec))
        case_type = IN_UNION;
      else {
        assert (idl_is_base_type(type_spec) || (idl_is_string(type_spec) && !idl_is_bounded(type_spec)) || idl_is_bitmask(type_spec));
        case_type = INLINE;
      }
    }

    if ((ret = push_field(descriptor, _case, NULL)))
      return ret;
    if ((ret = push_field(descriptor, _case->declarator, NULL)))
      return ret;

    /* FIXME: see above.
       For labels that are omitted because of empty target struct/union type, dummy ops
       will be stashed, so that we can safely assume that offset of type instructions is
       after last label */
    /* Note: this function currently only outputs JEQ4 ops and does not use JEQ where
       that would be possibly (in case it is not type ENU, or an @external member). This
       could be optimized to save some instructions in the descriptor. */
    cnt = ctype->instructions.count + (stype->labels - stype->label) * 4;
    for (label = _case->labels; label; label = idl_next(label)) {
      bool has_size = false;
      off = stype->offset + 2 + (stype->label * 4);
      if (case_type == INLINE || case_type == IN_UNION) {
        /* update offset to first instruction for inline non-simple cases */
        opcode &= (DDS_OP_MASK | DDS_OP_TYPE_FLAGS_MASK | DDS_OP_TYPE_MASK);
        if (case_type == IN_UNION)
          opcode |= (cnt - off);
        /* generate union case opcode */
        if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, off++, opcode, 0u)))
          return ret;
      } else {
        assert(off <= INT16_MAX);
        if (idl_is_external(node)) {
          opcode |= DDS_OP_FLAG_EXT;
          /* For @external union members include the size of the member to allow the
             serializer to allocate memory when deserializing. */
          has_size = true;
        }
        stash_jeq_offset(pstate, &ctype->instructions, off, type_spec, opcode, (int16_t)off);
        off++;
      }
      /* generate union case discriminator */
      if ((ret = stash_constant(pstate, &ctype->instructions, off++, label->const_expr)))
        return ret;
      /* generate union case offset */
      if ((ret = stash_offset(pstate, &ctype->instructions, off++, stype->fields)))
        return ret;
      /* Stash data field for the size of the type for this member. Stash 0 in case
         no size is required (not an external member), so that the size of a JEQ4
         instruction with parameters remains 4. */
      if (has_size) {
        if ((ret = stash_size(pstate, &ctype->instructions, off++, node, true)))
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

  type_spec = idl_unalias(idl_type_spec(node), 0u);
  assert(!idl_is_typedef(type_spec) && !idl_is_array(type_spec));
  const idl_union_t *union_spec = idl_parent(node);
  assert(idl_is_union(union_spec));

  if ((ret = push_field(descriptor, node, &field)))
    return ret;

  opcode = DDS_OP_ADR | DDS_OP_TYPE_UNI | typecode(type_spec, SUBTYPE, false);
  if (idl_is_topic_key(descriptor->topic, (pstate->flags & IDL_FLAG_KEYLIST) != 0, path, &order)) {
    opcode |= DDS_OP_FLAG_KEY;
    ctype->has_key_member = true;
  }
  if (idl_is_default_case(idl_parent(union_spec->default_case)) && !idl_is_implicit_default_case(idl_parent(union_spec->default_case)))
    opcode |= DDS_OP_FLAG_DEF;
  if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
    return ret;
  if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
    return ret;
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
    if ((ret = stash_couple(pstate, &ctype->instructions, stype->offset + 3, (uint16_t)cnt, 4u)))
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

    switch (((idl_union_t *)node)->extensibility.value) {
      case IDL_APPENDABLE:
        stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_DLC, 0u);
        break;
      case IDL_MUTABLE:
        idl_error(pstate, idl_location(node), "Mutable unions are not supported yet");
        // stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_PLC, 0u);
        return IDL_RETCODE_UNSUPPORTED;
      case IDL_FINAL:
        break;
    }

    if ((ret = push_type(descriptor, node, ctype, &stype)))
      return ret;

    stype->offset = ctype->instructions.count;
    stype->labels = stype->label = 0;

    /* determine total number of case labels as opcodes for complex elements
       are stored after case label opcodes */
    _case = ((const idl_union_t *)node)->cases;
    for (; _case; _case = idl_next(_case)) {
      for (label = _case->labels; label; label = idl_next(label))
        stype->labels++;
    }

    ret = IDL_VISIT_REVISIT;
    /* For a topic, only its top-level type should be visited, not the other
       (non-related) types in the idl */
    if (path->length == 1)
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
  return IDL_VISIT_TYPE_SPEC | IDL_VISIT_FWD_DECL_TARGET;
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

    switch (((idl_struct_t *)node)->extensibility.value) {
      case IDL_APPENDABLE:
        stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_DLC, 0u);
        break;
      case IDL_MUTABLE:
        stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_PLC, 0u);
        ctype->pl_offset = ctype->instructions.count;
        break;
      case IDL_FINAL:
        break;
    }

    if (!(ret = push_type(descriptor, node, ctype, NULL))) {
      ret = IDL_VISIT_REVISIT;
      /* For a topic, only its top-level type should be visited, not the other
        (non-related) types in the idl */
      if (path->length == 1)
        ret |= IDL_VISIT_DONT_ITERATE;
    }
    return ret;
  }
  return IDL_RETCODE_OK;
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
  type_spec = idl_unalias(idl_type_spec(node), 0u);
  if (idl_is_forward(type_spec))
    type_spec = ((const idl_forward_t *)type_spec)->definition;

  if (revisit) {
    uint32_t off, cnt;
    off = stype->offset;
    cnt = ctype->instructions.count;
    /* generate data field [elem-size] */
    if ((ret = stash_size(pstate, &ctype->instructions, off + 2, node, false)))
      return ret;
    /* generate data field [next-insn, elem-insn] */
    if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      assert(cnt <= INT16_MAX);
      int16_t addr_offs = (int16_t)(cnt - 2); /* minus 2 for the opcode and offset ops that are already stashed for this sequence */
      if ((ret = stash_element_offset(pstate, &ctype->instructions, off + 3, type_spec, 4u, addr_offs)))
        return ret;
    } else {
      if ((ret = stash_couple(pstate, &ctype->instructions, off + 3, (uint16_t)((cnt - off) + 3u), 4u)))
        return ret;
      /* generate return from subroutine */
      if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, DDS_OP_RTS, 0u)))
        return ret;
    }
    pop_type(descriptor);
  } else {
    uint32_t off;
    uint32_t opcode = DDS_OP_ADR | DDS_OP_TYPE_SEQ;
    uint32_t order;
    struct field *field = NULL;

    opcode |= typecode(type_spec, SUBTYPE, false);
    if (idl_is_topic_key(descriptor->topic, (pstate->flags & IDL_FLAG_KEYLIST) != 0, path, &order)) {
      opcode |= DDS_OP_FLAG_KEY;
      ctype->has_key_member = true;
    }

    off = ctype->instructions.count;
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    if (idl_is_struct(stype->node))
      field = stype->fields;
    if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
      return ret;

    /* short-circuit on simple types */
    if (idl_is_string(type_spec) || idl_is_base_type(type_spec) || idl_is_bitmask(type_spec)) {
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
    return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
add_mutable_member_offset(
  const idl_pstate_t *pstate,
  struct constructed_type *ctype,
  const idl_declarator_t *decl)
{
  idl_retcode_t ret;
  assert(idl_is_extensible(ctype->node, IDL_MUTABLE));

  /* add member offset for declarators of mutable types */
  assert(ctype->instructions.count <= INT16_MAX);
  int16_t addr_offs = (int16_t)(ctype->instructions.count
      - ctype->pl_offset /* offset of first op after PLC */
      + 2 /* skip this JEQ and member id */
      + 1 /* skip RTS (of the PLC list) */);
  if ((ret = stash_member_offset(pstate, &ctype->instructions, ctype->pl_offset++, addr_offs)))
    return ret;
  stash_single(pstate, &ctype->instructions, ctype->pl_offset++, decl->id.value);

  /* update offset for previous members for this ctype */
  struct instruction *table = ctype->instructions.table;
  for (uint32_t i = 1; i < ctype->pl_offset - 2; i += 2) {
    assert (table[i].type == MEMBER_OFFSET);
    table[i].data.inst_offset.addr_offs = (int16_t)(table[i].data.inst_offset.addr_offs + 2);
  }

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
    type_spec = idl_type_spec(node);
    if (idl_is_forward(type_spec))
      type_spec = ((const idl_forward_t *)type_spec)->definition;
  } else {
    type_spec = idl_unalias(idl_type_spec(node), 0u);
    assert(idl_is_array(type_spec));
    dims = idl_array_size(type_spec);
    type_spec = idl_type_spec(type_spec);
  }

  /* resolve aliases, squash multi-dimensional arrays */
  for (; idl_is_alias(type_spec); type_spec = idl_type_spec(type_spec))
    if (idl_is_array(type_spec))
      dims *= idl_array_size(type_spec);

  simple = (idl_mask(type_spec) & (IDL_BASE_TYPE|IDL_STRING|IDL_ENUM)) != 0;

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
      if ((ret = stash_size(pstate, &ctype->instructions, off + 4, node, false)))
        return ret;
    } else {
      if ((ret = stash_couple(pstate, &ctype->instructions, off + 3, (uint16_t)((cnt - off) + 3u), 5u)))
        return ret;
      /* generate data field [elem-size] */
      if ((ret = stash_size(pstate, &ctype->instructions, off + 4, node, false)))
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

    opcode |= typecode(type_spec, SUBTYPE, false);
    if (idl_is_topic_key(descriptor->topic, (pstate->flags & IDL_FLAG_KEYLIST) != 0, path, &order)) {
      opcode |= DDS_OP_FLAG_KEY;
      ctype->has_key_member = true;
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

    /* short-circuit on simple types */
    if (simple) {
      if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
        /* generate data field noop [next-insn, elem-insn] */
        if ((ret = stash_single(pstate, &ctype->instructions, nop, 0)))
          return ret;
        /* generate data field bound */
        if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_bound(type_spec)+1)))
          return ret;
      }
      if (!idl_is_alias(node) && idl_is_struct(stype->node))
        pop_field(descriptor);
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
    idl_warning(pstate, idl_location(node), "Explicit defaults are not supported yet in the C generator, the value from the @default annotation will not be used");
  if (member->optional.annotation)
    idl_warning(pstate, idl_location(node), "Optional members are not supported yet in the C generator, the @optional annotation will be ignored");
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
    idl_warning(pstate, idl_location(node), "Extensibility appendable and mutable are not yet supported in the C generator, the extensibility will not be used");
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
  bool mutable_aggr_type_member = idl_is_extensible(ctype->node, IDL_MUTABLE) &&
    (idl_is_member(idl_parent(node)) || idl_is_case(idl_parent(node)));

  type_spec = idl_unalias(idl_type_spec(node), 0u);
  if (idl_is_forward(type_spec))
    type_spec = ((const idl_forward_t *)type_spec)->definition;

  /* delegate array type specifiers or declarators */
  if (idl_is_array(node) || idl_is_array(type_spec)) {
    if (!revisit && mutable_aggr_type_member) {
      if ((ret = add_mutable_member_offset(pstate, ctype, (idl_declarator_t *)node)))
        return ret;
    }

    if ((ret = emit_array(pstate, revisit, path, node, user_data)))
      return ret;

    /* in case there is no revisit required (array has simple element type) we have
       to close the mutable member immediately, otherwise close it when revisiting */
    if (mutable_aggr_type_member && (!(ret & IDL_VISIT_REVISIT) || revisit)) {
      if ((ret = close_mutable_member(pstate, descriptor, ctype)))
        return ret;
    }
    return IDL_RETCODE_OK;
  }

  if (idl_is_empty(type_spec))
    return IDL_RETCODE_OK | IDL_VISIT_REVISIT;

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

    if (mutable_aggr_type_member && (ret = add_mutable_member_offset(pstate, ctype, (idl_declarator_t *)node)))
      return ret;

    if (!idl_is_alias(node) && idl_is_struct(stype->node)) {
      if ((ret = push_field(descriptor, node, &field)))
        return ret;
    }

    if (idl_is_sequence(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;

    /* inline the type spec for seq/struct/union declarators in a union */
    if (idl_is_union(ctype->node)) {
      if (idl_is_sequence(type_spec) || idl_is_union(type_spec) || idl_is_struct(type_spec))
        return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;
    }

    assert(ctype->instructions.count <= INT16_MAX);
    int16_t addr_offs = (int16_t)ctype->instructions.count;
    bool has_size = false;
    idl_node_t *parent = idl_parent(node);
    bool keylist = (pstate->flags & IDL_FLAG_KEYLIST) != 0;
    opcode = DDS_OP_ADR | typecode(type_spec, TYPE, true);
    if (idl_is_topic_key(descriptor->topic, keylist, path, &order)) {
      opcode |= DDS_OP_FLAG_KEY;
      ctype->has_key_member = true;
    } else if (idl_is_member(parent) && ((idl_member_t *)parent)->key.value) {
      /* Mark this DDS_OP_ADR as key if @key annotation is present, even in case the referring
         member is not part of the key (which resulted in idl_is_topic_key returning false).
         The reason for adding the key flag here, is that if any other member (that is a key)
         refers to this type, it will require the key flag. */
      opcode |= DDS_OP_FLAG_KEY;
      ctype->has_key_member = true;
    }
    if (idl_is_external(parent))
    {
      opcode |= DDS_OP_FLAG_EXT;
      /* For @external fields include the size of the field to allow the serializer to allocate
         memory for this field when deserializing. */
      has_size = true;
    }

    /* use member id for key ordering */
    order = ((idl_declarator_t *)node)->id.value;

    /* generate data field opcode */
    if ((ret = stash_opcode(pstate, descriptor, &ctype->instructions, nop, opcode, order)))
      return ret;
    /* generate data field offset */
    if ((ret = stash_offset(pstate, &ctype->instructions, nop, field)))
      return ret;
    /* generate data field bound */
    if (idl_is_string(type_spec) && idl_is_bounded(type_spec)) {
      if ((ret = stash_single(pstate, &ctype->instructions, nop, idl_bound(type_spec)+1)))
        return ret;
    } else if (idl_is_struct(type_spec) || idl_is_union(type_spec)) {
      if ((ret = stash_element_offset(pstate, &ctype->instructions, nop, type_spec, 3 + (has_size ? 1 : 0), addr_offs)))
        return ret;
    }
    /* generate data field element size */
    if (has_size) {
      if ((ret = stash_size(pstate, &ctype->instructions, nop, node, true)))
        return ret;
    }

    if (idl_is_union(type_spec) || idl_is_struct(type_spec) || idl_is_bitmask(type_spec))
      return IDL_VISIT_TYPE_SPEC | IDL_VISIT_REVISIT;

    return IDL_VISIT_REVISIT;
  }
  return IDL_RETCODE_OK;
}

static int print_opcode(FILE *fp, const struct instruction *inst)
{
  char buf[16];
  const char *vec[10];
  size_t len = 0;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode type, subtype;

  assert(inst->type == OPCODE);

  opcode = DDS_OP(inst->data.opcode.code);

  switch (opcode) {
    case DDS_OP_DLC:
      vec[len++] = "DDS_OP_DLC";
      goto print;
    case DDS_OP_PLC:
      vec[len++] = "DDS_OP_PLC";
      goto print;
    case DDS_OP_RTS:
      vec[len++] = "DDS_OP_RTS";
      goto print;
    case DDS_OP_KOF:
      vec[len++] = "DDS_OP_KOF";
      /* lower 16 bits contains length */
      idl_snprintf(buf, sizeof(buf), " | %u", DDS_OP_LENGTH(inst->data.opcode.code));
      vec[len++] = buf;
      goto print;
    case DDS_OP_PLM:
      vec[len++] = "DDS_OP_PLM";
      break;
    case DDS_OP_JEQ:
      vec[len++] = "DDS_OP_JEQ";
      break;
    case DDS_OP_JEQ4:
      vec[len++] = "DDS_OP_JEQ4";
      break;
    default:
      assert(opcode == DDS_OP_ADR);
      vec[len++] = "DDS_OP_ADR";
      break;
  }

  if (inst->data.opcode.code & DDS_OP_FLAG_EXT)
    vec[len++] = " | DDS_OP_FLAG_EXT";

  type = DDS_OP_TYPE(inst->data.opcode.code);
  assert(opcode == DDS_OP_PLM || type);
  switch (type) {
    case DDS_OP_VAL_1BY: vec[len++] = " | DDS_OP_TYPE_1BY"; break;
    case DDS_OP_VAL_2BY: vec[len++] = " | DDS_OP_TYPE_2BY"; break;
    case DDS_OP_VAL_4BY: vec[len++] = " | DDS_OP_TYPE_4BY"; break;
    case DDS_OP_VAL_8BY: vec[len++] = " | DDS_OP_TYPE_8BY"; break;
    case DDS_OP_VAL_STR: vec[len++] = " | DDS_OP_TYPE_STR"; break;
    case DDS_OP_VAL_BST: vec[len++] = " | DDS_OP_TYPE_BST"; break;
    case DDS_OP_VAL_BSP: vec[len++] = " | DDS_OP_TYPE_BSP"; break;
    case DDS_OP_VAL_SEQ: vec[len++] = " | DDS_OP_TYPE_SEQ"; break;
    case DDS_OP_VAL_ARR: vec[len++] = " | DDS_OP_TYPE_ARR"; break;
    case DDS_OP_VAL_UNI: vec[len++] = " | DDS_OP_TYPE_UNI"; break;
    case DDS_OP_VAL_STU: vec[len++] = " | DDS_OP_TYPE_STU"; break;
    case DDS_OP_VAL_ENU: vec[len++] = " | DDS_OP_TYPE_ENU"; break;
    case DDS_OP_VAL_EXT: vec[len++] = " | DDS_OP_TYPE_EXT"; break;
  }
  if (opcode == DDS_OP_JEQ || opcode == DDS_OP_JEQ4 || opcode == DDS_OP_PLM) {
    /* lower 16 bits contain offset to next instruction */
    idl_snprintf(buf, sizeof(buf), " | %u", (uint16_t) DDS_OP_JUMP (inst->data.opcode.code));
    vec[len++] = buf;
  } else {
    subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
    assert(( subtype &&  (type == DDS_OP_VAL_SEQ ||
                          type == DDS_OP_VAL_ARR ||
                          type == DDS_OP_VAL_UNI ||
                          type == DDS_OP_VAL_STU))
        || (!subtype && !(type == DDS_OP_VAL_SEQ ||
                          type == DDS_OP_VAL_ARR ||
                          type == DDS_OP_VAL_UNI ||
                          type == DDS_OP_VAL_STU)));
    switch (subtype) {
      case DDS_OP_VAL_1BY: vec[len++] = " | DDS_OP_SUBTYPE_1BY"; break;
      case DDS_OP_VAL_2BY: vec[len++] = " | DDS_OP_SUBTYPE_2BY"; break;
      case DDS_OP_VAL_4BY: vec[len++] = " | DDS_OP_SUBTYPE_4BY"; break;
      case DDS_OP_VAL_8BY: vec[len++] = " | DDS_OP_SUBTYPE_8BY"; break;
      case DDS_OP_VAL_STR: vec[len++] = " | DDS_OP_SUBTYPE_STR"; break;
      case DDS_OP_VAL_BST: vec[len++] = " | DDS_OP_SUBTYPE_BST"; break;
      case DDS_OP_VAL_BSP: vec[len++] = " | DDS_OP_SUBTYPE_BSP"; break;
      case DDS_OP_VAL_SEQ: vec[len++] = " | DDS_OP_SUBTYPE_SEQ"; break;
      case DDS_OP_VAL_ARR: vec[len++] = " | DDS_OP_SUBTYPE_ARR"; break;
      case DDS_OP_VAL_UNI: vec[len++] = " | DDS_OP_SUBTYPE_UNI"; break;
      case DDS_OP_VAL_STU: vec[len++] = " | DDS_OP_SUBTYPE_STU"; break;
      case DDS_OP_VAL_ENU: vec[len++] = " | DDS_OP_SUBTYPE_ENU"; break;
      case DDS_OP_VAL_EXT: abort(); break;
    }

    if (type == DDS_OP_VAL_UNI && (inst->data.opcode.code & DDS_OP_FLAG_DEF))
      vec[len++] = " | DDS_OP_FLAG_DEF";
    else if (inst->data.opcode.code & DDS_OP_FLAG_FP)
      vec[len++] = " | DDS_OP_FLAG_FP";
    if (inst->data.opcode.code & DDS_OP_FLAG_SGN)
      vec[len++] = " | DDS_OP_FLAG_SGN";
    if (inst->data.opcode.code & DDS_OP_FLAG_KEY)
      vec[len++] = " | DDS_OP_FLAG_KEY";
  }

print:
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
  assert(inst->type == SIZE);
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

static int print_opcodes(FILE *fp, const struct descriptor *descriptor, uint32_t *kof_offs)
{
  const struct instruction *inst;
  enum dds_stream_opcode opcode;
  enum dds_stream_typecode optype, subtype;
  char *type = NULL;
  const char *seps[] = { ", ", ",\n  " };
  const char *sep = "  ";
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
      sep = seps[op == brk];
      switch (inst->type) {
        case OPCODE:
          sep = op ? seps[1] : "  "; /* indent, always */
          /* determine when to break line */
          opcode = DDS_OP(inst->data.opcode.code);
          optype = DDS_OP_TYPE(inst->data.opcode.code);
          if (opcode == DDS_OP_RTS || opcode == DDS_OP_DLC || opcode == DDS_OP_PLC)
            brk = op + 1;
          else if (opcode == DDS_OP_JEQ)
            brk = op + 3;
          else if (opcode == DDS_OP_JEQ4)
            brk = op + 4;
          else if (opcode == DDS_OP_PLM)
            brk = op + 3;
          else if (optype == DDS_OP_VAL_BST)
            brk = op + 3;
          else if (optype == DDS_OP_VAL_EXT) {
            brk = op + 3;
            if (inst->data.opcode.code & DDS_OP_FLAG_EXT)
              brk++;
          }
          else if (optype == DDS_OP_VAL_ARR || optype == DDS_OP_VAL_SEQ) {
            subtype = DDS_OP_SUBTYPE(inst->data.opcode.code);
            brk = op + (optype == DDS_OP_VAL_SEQ ? 2 : 3);
            if (subtype > DDS_OP_VAL_8BY)
              brk += 2;
          } else if (optype == DDS_OP_VAL_UNI)
            brk = op + 4;
          else
            brk = op + 2;
          if (fputs(sep, fp) < 0 || print_opcode(fp, inst) < 0)
            return -1;
          break;
        case OFFSET:
          if (fputs(sep, fp) < 0 || print_offset(fp, inst) < 0)
            return -1;
          break;
        case SIZE:
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
          const struct instruction inst_op = { OPCODE, { .opcode = { .code = (DDS_OP_PLM & (DDS_OP_MASK | DDS_PLM_FLAGS_MASK)) | (uint16_t)inst->data.inst_offset.addr_offs, .order = 0 } } };
          if (fputs(sep, fp) < 0 || print_opcode(fp, &inst_op) < 0)
            return -1;
          brk = op + 2;
          break;
        }
        case KEY_OFFSET:
        case KEY_OFFSET_VAL:
          return -1;
      }
      cnt++;
    }
  }

  if (kof_offs)
    *kof_offs = cnt;

  for (size_t op = 0, brk = 0; op < descriptor->key_offsets.count; op++) {
    inst = &descriptor->key_offsets.table[op];
    sep = seps[op == brk];
    switch (inst->type) {
      case KEY_OFFSET:
      {
        const struct instruction inst_op = { OPCODE, { .opcode = { .code = (DDS_OP_KOF & DDS_OP_MASK) | (inst->data.key_offset.len & DDS_KOF_OFFSET_MASK), .order = 0 } } };
        if (fputs(sep, fp) < 0 || idl_fprintf(fp, "\n  /* key: %s (size: %u) */\n  ", inst->data.key_offset.key_name, inst->data.key_offset.key_size) < 0 || print_opcode(fp, &inst_op) < 0)
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
      free(tmp->name);
    if (tmp->sub)
      free_ctype_keys(tmp->sub);
    tmp1 = tmp;
    tmp = tmp->next;
    free(tmp1);
  }
}

static idl_retcode_t get_ctype_keys(struct descriptor *descriptor, struct constructed_type *ctype, struct constructed_type_key **keys, bool parent_is_key)
{
  idl_retcode_t ret;
  assert(keys);
  struct constructed_type_key *ctype_keys = NULL;
  for (uint32_t i = 0; i < ctype->instructions.count; i++) {
    struct instruction *inst = &ctype->instructions.table[i];
    uint32_t code, typecode, size = 0, dims = 1, align = 0;

    if (inst->type != OPCODE)
      continue;
    code = inst->data.opcode.code;
    if (DDS_OP(code) != DDS_OP_ADR)
      continue;
    /* If the parent is a @key member and there are no specific key members in this ctype,
       add the key flag to all members in this type. The serializer will only use these
       key flags in case the top-level member (which is referring this this member)
       also has the key flag set. */
    if (parent_is_key && !ctype->has_key_member)
      code = (inst->data.opcode.code |= DDS_OP_FLAG_KEY);
    if (!(code & DDS_OP_FLAG_KEY))
      continue;

    struct constructed_type_key *key = calloc (1, sizeof(*key)), *tmp;
    if (!key)
      goto err_no_memory;
    if (ctype_keys == NULL)
      ctype_keys = key;
    else {
      tmp = ctype_keys;
      while (tmp->next)
        tmp = tmp->next;
      tmp->next = key;
    }

    key->ctype = ctype;
    key->offset = i;
    key->order = inst->data.opcode.order;

    const struct instruction *inst2 = &ctype->instructions.table[i + 2];
    if (DDS_OP_TYPE(code) == DDS_OP_VAL_EXT) {
      assert(inst2->type == ELEM_OFFSET);
      const idl_node_t *node = inst2->data.inst_offset.node;
      struct constructed_type *csubtype = find_ctype(descriptor, node);
      assert(csubtype);
      if ((ret = get_ctype_keys(descriptor, csubtype, &key->sub, true)))
        goto err;
    } else {
      descriptor->n_keys++;
      if (DDS_OP_TYPE(code) == DDS_OP_VAL_ARR) {
        assert(i + 2 < ctype->instructions.count);
        assert(inst2->type == SINGLE);
        dims = inst2->data.single;
        typecode = DDS_OP_SUBTYPE(code);
      } else {
        typecode = DDS_OP_TYPE(code);
      }

      switch (typecode) {
        case DDS_OP_VAL_1BY: size = align = 1; break;
        case DDS_OP_VAL_2BY: size = align = 2; break;
        case DDS_OP_VAL_4BY: size = align = 4; break;
        case DDS_OP_VAL_8BY: size = align = 8; break;
        case DDS_OP_VAL_BST: {
          assert(i+2 < ctype->instructions.count);
          assert(ctype->instructions.table[i+2].type == SINGLE);
          align = 4;
          size = 5 + ctype->instructions.table[i+2].data.single;
        }
        break;
        default:
          key->size = MAX_SIZE + 1;
          break;
      }
      if (size > MAX_SIZE || dims > MAX_SIZE || (size * dims) + key->size > MAX_SIZE)
        key->size = MAX_SIZE + 1;
      else
      {
        if ((key->size % align) != 0)
          key->size += align - (key->size % align);
        key->size += size * dims;
      }
    }

    const struct instruction *inst1 = &ctype->instructions.table[i + 1];
    assert(inst1->type == OFFSET);
    assert(inst1->data.offset.type);
    assert(inst1->data.offset.member);
    if (!(key->name = idl_strdup(inst1->data.offset.member)))
      goto err_no_memory;
  }
  *keys = ctype_keys;
  return IDL_RETCODE_OK;

err_no_memory:
  ret = IDL_RETCODE_NO_MEMORY;
err:
  free_ctype_keys(ctype_keys);
  return ret;
}

static int add_key_offset(const idl_pstate_t *pstate, struct descriptor *descriptor, const struct constructed_type *ctype_top_level, struct constructed_type_key *key, char *name, bool keylist, struct key_offs *offs)
{
  if (offs->n >= MAX_KEY_OFFS)
    return -1;

  char *name1;
  while (key) {
    if (idl_asprintf(&name1, "%s%s%s", name ? name : "", name ? "." : "", key->name) == -1)
      goto err;
    offs->val[offs->n] = (uint16_t)key->offset;
    offs->order[offs->n] = (uint16_t)key->order;
    offs->n++;
    if (key->sub) {
      if (add_key_offset(pstate, descriptor, ctype_top_level, key->sub, name1, keylist, offs))
        goto err_stash;
    } else {
      /* For both @key and pragma keylist, use the key order stored in the constructed_type_key
         object, which is the member id of this key member in its parent constructed_type. */
      if (stash_key_offset(pstate, &descriptor->key_offsets, nop, name1, offs->n, (uint16_t)key->size) < 0)
        goto err_stash;
      for (uint32_t n = 0; n < offs->n; n++) {
        if (stash_key_offset_val(pstate, &descriptor->key_offsets, nop, offs->val[n], offs->order[n]))
          goto err_stash;
      }
    }
    offs->n--;
    free(name1);
    key = key->next;
  }
  return 0;
err_stash:
  free(name1);
err:
  return -1;
}

static int add_key_offset_list(const idl_pstate_t *pstate, struct descriptor *descriptor, bool keylist)
{
  struct constructed_type *ctype = find_ctype(descriptor, descriptor->topic);
  assert(ctype);
  struct constructed_type_key *keys;
  if (get_ctype_keys(descriptor, ctype, &keys, false))
    return -1;
  struct key_offs offs = { .val = { 0 }, .order = { 0 }, .n = 0 };
  add_key_offset(pstate, descriptor, ctype, keys, NULL, keylist, &offs);
  free_ctype_keys(keys);
  return 0;
}

static int sort_keys_cmp (const void *va, const void *vb)
{
  const struct key_print_meta *a = va;
  const struct key_print_meta *b = vb;
  for (uint32_t i = 0; i < a->n_order; i++) {
    if (b->n_order - 1 < i)
      return 1;
    if (a->order[i] != b->order[i])
      return a->order[i] < b->order[i] ? -1 : 1;
  }
  if (b->n_order > a->n_order)
    return -1;
  return 0;
}

struct key_print_meta *key_print_meta_init(struct descriptor *descriptor, uint32_t *sz)
{
  struct key_print_meta *keys;
  uint32_t key_index = 0, offs_len = 0;

  assert (sz);
  if (!(keys = calloc(descriptor->n_keys, sizeof(*keys))))
    return NULL;
  for (uint32_t i = 0; i < descriptor->key_offsets.count; i++) {
    const struct instruction *inst = &descriptor->key_offsets.table[i];
    if (inst->type == KEY_OFFSET) {
      (*sz) += inst->data.key_offset.key_size;
      offs_len = inst->data.key_offset.len;
      assert(key_index < descriptor->n_keys);
      keys[key_index].name = inst->data.key_offset.key_name;
      keys[key_index].inst_offs = i;
      keys[key_index].order = calloc(offs_len, sizeof (*keys[key_index].order));
      keys[key_index].n_order = offs_len;
      keys[key_index].size = inst->data.key_offset.key_size;
      keys[key_index].key_idx = key_index;
      key_index++;
    } else {
      assert(inst->type == KEY_OFFSET_VAL);
      assert(offs_len > 0);
      assert(key_index > 0);
      uint32_t order = inst->data.key_offset_val.order;
      keys[key_index - 1].order[keys[key_index - 1].n_order - offs_len] = order;
      offs_len--;
    }
  }
  assert(key_index == descriptor->n_keys);
  qsort(keys, descriptor->n_keys, sizeof (*keys), sort_keys_cmp);

  return keys;
}

void key_print_meta_free(struct key_print_meta *keys, uint32_t n_keys)
{
  for (uint32_t k = 0; k < n_keys; k++) {
    if (keys[k].order) {
      free(keys[k].order);
      keys[k].order = NULL;
    }
  }
  free(keys);
}

static int print_keys(FILE *fp, struct descriptor *descriptor, uint32_t offset)
{
  char *typestr = NULL;
  const char *fmt, *sep="";
  struct key_print_meta *keys;
  uint32_t sz = 0;

  if (descriptor->n_keys == 0)
    return 0;
  if (!(keys = key_print_meta_init(descriptor, &sz)))
    goto err_keys;
  if (IDL_PRINT(&typestr, print_type, descriptor->topic) < 0)
    goto err_type;

  assert(sz);
  if (sz <= MAX_SIZE)
    descriptor->flags |= DDS_TOPIC_FIXED_KEY;

  fmt = "static const dds_key_descriptor_t %s_keys[%"PRIu32"] =\n{\n";
  if (idl_fprintf(fp, fmt, typestr, descriptor->n_keys) < 0)
    goto err_print;
  sep = "";
  fmt = "%s  { \"%s\", %"PRIu32", %"PRIu32" }";
  for (uint32_t k=0; k < descriptor->n_keys; k++) {
    if (idl_fprintf(fp, fmt, sep, keys[k].name, offset + keys[k].inst_offs, keys[k].key_idx) < 0)
      goto err_print;
    sep = ",\n";
  }
  if (fputs("\n};\n\n", fp) < 0)
    goto err_print;
  free(typestr);
  key_print_meta_free(keys, descriptor->n_keys);
  return 0;

err_print:
  free(typestr);
err_type:
  key_print_meta_free(keys, descriptor->n_keys);
err_keys:
  return -1;
}

static int print_flags(FILE *fp, struct descriptor *descriptor)
{
  const char *fmt;
  const char *vec[4] = { NULL };
  size_t cnt, len = 0;

  if (descriptor->flags & DDS_TOPIC_NO_OPTIMIZE)
    vec[len++] = "DDS_TOPIC_NO_OPTIMIZE";
  if (descriptor->flags & DDS_TOPIC_CONTAINS_UNION)
    vec[len++] = "DDS_TOPIC_CONTAINS_UNION";
  if (descriptor->flags & DDS_TOPIC_FIXED_KEY)
    vec[len++] = "DDS_TOPIC_FIXED_KEY";

  bool fixed_size = true;
  for (struct constructed_type *ctype = descriptor->constructed_types; ctype && fixed_size; ctype = ctype->next) {
    for (uint32_t op = 0; op < ctype->instructions.count && fixed_size; op++) {
      struct instruction i = ctype->instructions.table[op];
      if (i.type != OPCODE)
        continue;

      uint32_t typecode = DDS_OP_TYPE(i.data.opcode.code);
      if (typecode == DDS_OP_VAL_STR || typecode == DDS_OP_VAL_BST || typecode == DDS_OP_VAL_BSP ||typecode == DDS_OP_VAL_SEQ)
        fixed_size = false;
    }
  }

  if (fixed_size)
    vec[len++] = "DDS_TOPIC_FIXED_SIZE";

  if (!len)
    vec[len++] = "0u";

  for (cnt=0, fmt="%s"; cnt < len; cnt++, fmt=" | %s") {
    if (idl_fprintf(fp, fmt, vec[cnt]) < 0)
      return -1;
  }

  return fputs(",\n", fp) < 0 ? -1 : 0;
}

static int print_descriptor(
    FILE *fp,
    struct descriptor *descriptor
)
{
  char *name, *type;
  const char *fmt;

  if (IDL_PRINTA(&name, print_scoped_name, descriptor->topic) < 0)
    return -1;
  if (IDL_PRINTA(&type, print_type, descriptor->topic) < 0)
    return -1;
  fmt = "const dds_topic_descriptor_t %1$s_desc =\n{\n"
        "  .m_size = sizeof (%1$s),\n" /* size of type */
        "  .m_align = %2$s,\n" /* alignment */
        "  .m_flagset = ";
  if (idl_fprintf(fp, fmt, type, descriptor->alignment->rendering) < 0)
    return -1;
  if (print_flags(fp, descriptor) < 0)
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

  if (idl_fprintf(fp, "\n};\n\n") < 0)
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
      if (!ctype->node) {
        // FIXME: this shouldn't happen, can we make it an assert?
        return IDL_RETCODE_SEMANTIC_ERROR;
      }
      ctype->offset = offs;
      offs += ctype->instructions.count;
    }
  }

  /* set offset for each ELEM_OFFSET instruction */
  for (struct constructed_type *ctype = descriptor->constructed_types; ctype; ctype = ctype->next) {
    for (size_t op = 0; op < ctype->instructions.count; op++) {
      if (ctype->instructions.table[op].type == ELEM_OFFSET || ctype->instructions.table[op].type == JEQ_OFFSET)
      {
        struct instruction *inst = &ctype->instructions.table[op];
        struct constructed_type *ctype1 = find_ctype(descriptor, inst->data.inst_offset.node);
        if (ctype1) {
          assert(ctype1->offset <= INT32_MAX);
          int32_t offs = (int32_t)ctype1->offset - ((int32_t)ctype->offset + inst->data.inst_offset.addr_offs);
          assert(offs >= INT16_MIN);
          assert(offs <= INT16_MAX);
          inst->data.inst_offset.elem_offs = (int16_t)offs;
        } else {
          // FIXME: this shouldn't happen, can we make it an assert?
          return IDL_RETCODE_SEMANTIC_ERROR;
        }
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
    DDSRT_WARNING_MSVC_OFF (6001);
    switch (inst->type) {
      case OFFSET:
        if (inst->data.offset.member)
          free(inst->data.offset.member);
        if (inst->data.offset.type)
          free(inst->data.offset.type);
        break;
      case SIZE:
        if (inst->data.size.type)
          free(inst->data.size.type);
        break;
      case CONSTANT:
        if (inst->data.constant.value)
          free(inst->data.constant.value);
        break;
      case KEY_OFFSET:
        if (inst->data.key_offset.key_name)
          free(inst->data.key_offset.key_name);
      default:
        break;
    }
    DDSRT_WARNING_MSVC_ON (6001);
  }
}

static void
ctype_fini(struct constructed_type *ctype)
{
  instructions_fini(&ctype->instructions);
  if (ctype->instructions.table)
    free(ctype->instructions.table);
}

idl_retcode_t generate_descriptor(const idl_pstate_t *pstate, struct generator *generator, const idl_node_t *node);

idl_retcode_t
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
    free(ctype);
    ctype = ctype1;
  }
  instructions_fini(&descriptor->key_offsets);
  free(descriptor->key_offsets.table);
  assert(!descriptor->type_stack);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
  return IDL_RETCODE_OK;
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
  visitor.visit = IDL_DECLARATOR | IDL_SEQUENCE | IDL_STRUCT | IDL_UNION | IDL_SWITCH_TYPE_SPEC | IDL_CASE | IDL_FORWARD | IDL_MEMBER | IDL_BITMASK;
  visitor.accept[IDL_ACCEPT_SEQUENCE] = &emit_sequence;
  visitor.accept[IDL_ACCEPT_UNION] = &emit_union;
  visitor.accept[IDL_ACCEPT_SWITCH_TYPE_SPEC] = &emit_switch_type_spec;
  visitor.accept[IDL_ACCEPT_CASE] = &emit_case;
  visitor.accept[IDL_ACCEPT_STRUCT] = &emit_struct;
  visitor.accept[IDL_ACCEPT_DECLARATOR] = &emit_declarator;
  visitor.accept[IDL_ACCEPT_FORWARD] = &emit_forward;
  visitor.accept[IDL_ACCEPT_MEMBER] = &emit_member;
  visitor.accept[IDL_ACCEPT_BITMASK] = &emit_bitmask;

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
  if ((ret = add_key_offset_list(pstate, descriptor, (pstate->flags & IDL_FLAG_KEYLIST) != 0)) < 0)
    goto err;

err:
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
  uint32_t inst_count;

  if ((ret = generate_descriptor_impl(pstate, node, &descriptor)) < 0)
    goto err_gen;
  if (print_opcodes(generator->source.handle, &descriptor, &inst_count) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
  if (print_keys(generator->source.handle, &descriptor, inst_count) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }
  if (print_descriptor(generator->source.handle, &descriptor) < 0)
    { ret = IDL_RETCODE_NO_MEMORY; goto err_print; }

err_print:
err_gen:
  descriptor_fini(&descriptor);
  return ret;
}
