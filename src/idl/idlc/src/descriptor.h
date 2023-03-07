/*
 * Copyright(c) 2021 to 2022 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef DESCRIPTOR_H
#define DESCRIPTOR_H

#include "idl/processor.h"

#define MAX_KEY_OFFS (255)

/* The name of the member that is added as the first member in a struct to
   include its base type in case of inheritance */
#define STRUCT_BASE_MEMBER_NAME "parent"

/* store each instruction separately for easy post processing and reduced
   complexity. arrays and sequences introduce a new scope and the relative
   offset to the next field is stored with the instructions for the respective
   field. this requires the generator to revert its position. using separate
   streams intruduces too much complexity. the table is also used to generate
   a key offset table after the fact */
struct instruction {
  enum {
    OPCODE,
    OFFSET,                 /* offsetof(type, member) */
    MEMBER_SIZE,
    CONSTANT,
    COUPLE,
    SINGLE,
    ELEM_OFFSET,            /* lower 16 bits have the offset of an external type (for EXT instruction), higher 16 bits offset to next instruction */
    JEQ_OFFSET,             /* JEQ for union case */
    MEMBER_OFFSET,          /* PLM with offset to the member instruction within the current type */
    BASE_MEMBERS_OFFSET,    /* PLM with FLAG_BASE set, to jump to PLM list of base type */
    KEY_OFFSET,             /* KOF instruction, lower 16 bits have the number of offsets that follow this instruction */
    KEY_OFFSET_VAL          /* follows KOF, and has the offset to instruction of a key part */
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
  uint32_t offset;
  uint32_t order;
  uint32_t dims;
  uint32_t size;
  bool dheader;
  uint32_t align;
  struct constructed_type_key *sub;
};

struct key_offs {
  uint16_t val[MAX_KEY_OFFS];
  uint32_t order[MAX_KEY_OFFS];
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

struct key_meta_data {
  char *name;
  uint32_t inst_offs;
  uint32_t n_order;
  uint32_t *order;
  uint32_t key_idx;
  uint32_t dims;
  uint32_t size;
  bool dheader;
  uint32_t align;
};

struct descriptor {
  const idl_node_t *topic;
  const struct alignment *alignment; /**< alignment of topic type */
  uint32_t n_keys; /**< number of keys in topic */
  struct key_meta_data *keys; /**< key meta-data */
  uint32_t n_opcodes; /**< number of opcodes in descriptor */
  uint32_t flags; /**< topic descriptor flag values */
  uint32_t data_representations; /**< restrict data representations for top-level type */
  uint32_t keysz_xcdr1; /**< size of the XCDR1 serialized key (or set to MAX_FIXED_KEY + 1 if more than MAX_FIXED_KEY) */
  uint32_t keysz_xcdr2; /**< size of the XCDR2 serialized key (or set to MAX_FIXED_KEY + 1 if more than MAX_FIXED_KEY) */
  struct stack_type *type_stack;
  struct constructed_type *constructed_types;
  struct instructions key_offsets;
};

void
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

#endif /* DESCRIPTOR_H */
