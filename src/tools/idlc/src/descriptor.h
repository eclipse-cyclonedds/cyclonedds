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
#include "idl/processor.h"

#define MAX_KEY_OFFS (255)

/* store each instruction separately for easy post processing and reduced
   complexity. arrays and sequences introduce a new scope and the relative
   offset to the next field is stored with the instructions for the respective
   field. this requires the generator to revert its position. using separate
   streams intruduces too much complexity. the table is also used to generate
   a key offset table after the fact */
struct instruction {
  enum {
    OPCODE,
    OFFSET,
    SIZE,
    CONSTANT,
    COUPLE,
    SINGLE,
    ELEM_OFFSET,
    JEQ_OFFSET,
    MEMBER_OFFSET,
    KEY_OFFSET,
    KEY_OFFSET_VAL
  } type;
  union {
    struct {
      uint32_t code;
      uint32_t order; /**< key order if DDS_OP_FLAG_KEY */
    } opcode;
    struct {
      char *type;
      char *member;
    } offset; /**< name of type and member to generate offsetof */
    struct {
      char *type;
    } size; /**< name of type to generate sizeof */
    struct {
      char *value;
    } constant;
    struct {
      uint16_t high;
      uint16_t low;
    } couple;
    uint32_t single;
    struct {
      const idl_node_t *node;
      union {
        uint32_t opcode;
        uint16_t high;
      } inst;
      int16_t addr_offs;
      int16_t elem_offs;
    } inst_offset;
    struct {
      char *key_name;
      uint16_t len;
      uint16_t key_size;
    } key_offset;
    struct {
      uint16_t offs;
      uint32_t order; /**< in xcdr2 the member id is used for ordening keys */
    } key_offset_val;
  } data;
};

struct instructions {
  uint32_t size;        /**< available number of instructions */
  uint32_t count;       /**< used number of instructions */
  uint32_t offset;      /**< absolute offset in descriptor instructions array */
  struct instruction *table;
};

struct constructed_type {
  struct constructed_type *next;
  const void *node;
  const idl_name_t *name;
  const idl_scope_t *scope;
  bool has_key_member;
  uint32_t offset;        /**< offset for the instructions of this type in the topic descriptor instruction array */
  uint32_t pl_offset;     /**< current offset in parameter list for mutable types */
  struct instructions instructions;
};

struct constructed_type_key {
  const struct constructed_type *ctype;
  struct constructed_type_key *next;
  char *name;
  uint32_t size;
  uint32_t offset;
  uint32_t order;
  struct constructed_type_key *sub;
};

struct key_offs {
  uint16_t val[MAX_KEY_OFFS];
  uint16_t order[MAX_KEY_OFFS];
  uint16_t n;
};

struct field {
  struct field *previous;
  const void *node;
};

struct stack_type {
  struct stack_type *previous;
  struct field *fields;
  const void *node;
  struct constructed_type *ctype;
  uint32_t offset;
  uint32_t label, labels;
};

struct descriptor {
  const idl_node_t *topic;
  const struct alignment *alignment; /**< alignment of topic type */
  uint32_t n_keys; /**< number of keys in topic */
  uint32_t n_opcodes; /**< number of opcodes in descriptor */
  uint32_t flags; /**< topic descriptor flag values */
  struct stack_type *type_stack;
  struct constructed_type *constructed_types;
  struct instructions key_offsets;
};

struct key_print_meta {
  const char *name;
  uint32_t inst_offs;
  uint32_t n_order;
  uint32_t *order;
  uint32_t size;
  uint32_t key_idx;
};

struct key_print_meta *
key_print_meta_init(
  struct descriptor *descriptor,
  uint32_t *sz);

void
key_print_meta_free(
  struct key_print_meta *keys,
  uint32_t n_keys);

idl_retcode_t
descriptor_fini(
  struct descriptor *descriptor);

idl_retcode_t
generate_descriptor_impl(
  const idl_pstate_t *pstate,
  const idl_node_t *topic_node,
  struct descriptor *descriptor);

idl_retcode_t
emit_topic_descriptor(
  const idl_pstate_t *pstate,
  const idl_node_t *node,
  void *user_data);
