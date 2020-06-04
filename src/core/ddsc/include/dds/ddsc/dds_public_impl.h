/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
/* TODO: do we really need to expose all of this as an API? maybe some, but all? */

/** @file
 *
 * @brief DDS C Implementation API
 *
 * This header file defines the public API for all kinds of things in the
 * Eclipse Cyclone DDS C language binding.
 */
#ifndef DDS_IMPL_H
#define DDS_IMPL_H

#include <stdint.h>
#include <stdbool.h>
#include "dds/export.h"
#include "dds/ddsc/dds_public_alloc.h"

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct dds_sequence
{
  uint32_t _maximum;
  uint32_t _length;
  uint8_t * _buffer;
  bool _release;
}
dds_sequence_t;

typedef struct dds_key_descriptor
{
  const char * m_name;
  uint32_t m_index;
}
dds_key_descriptor_t;

/*
  Topic definitions are output by a preprocessor and have an
  implementation-private definition. The only thing exposed on the
  API is a pointer to the "topic_descriptor_t" struct type.
*/

typedef struct dds_topic_descriptor
{
  const uint32_t m_size;               /* Size of topic type */
  const uint32_t m_align;              /* Alignment of topic type */
  const uint32_t m_flagset;            /* Flags */
  const uint32_t m_nkeys;              /* Number of keys (can be 0) */
  const char * m_typename;             /* Type name */
  const dds_key_descriptor_t * m_keys; /* Key descriptors (NULL iff m_nkeys 0) */
  const uint32_t m_nops;               /* Number of ops in m_ops */
  const uint32_t * m_ops;              /* Marshalling meta data */
  const char * m_meta;                 /* XML topic description meta data */
}
dds_topic_descriptor_t;

/* Topic descriptor flag values */

#define DDS_TOPIC_NO_OPTIMIZE 0x0001
#define DDS_TOPIC_FIXED_KEY 0x0002
#define DDS_TOPIC_CONTAINS_UNION 0x0004

/*
  Masks for read condition, read, take: there is only one mask here,
  which combines the sample, view and instance states.
*/

#define DDS_READ_SAMPLE_STATE 1u
#define DDS_NOT_READ_SAMPLE_STATE 2u
#define DDS_ANY_SAMPLE_STATE (1u | 2u)

#define DDS_NEW_VIEW_STATE 4u
#define DDS_NOT_NEW_VIEW_STATE 8u
#define DDS_ANY_VIEW_STATE (4u | 8u)

#define DDS_ALIVE_INSTANCE_STATE 16u
#define DDS_NOT_ALIVE_DISPOSED_INSTANCE_STATE 32u
#define DDS_NOT_ALIVE_NO_WRITERS_INSTANCE_STATE 64u
#define DDS_ANY_INSTANCE_STATE (16u | 32u | 64u)

#define DDS_ANY_STATE (DDS_ANY_SAMPLE_STATE | DDS_ANY_VIEW_STATE | DDS_ANY_INSTANCE_STATE)

#define DDS_DOMAIN_DEFAULT ((uint32_t) 0xffffffffu)
#define DDS_HANDLE_NIL 0
#define DDS_ENTITY_NIL 0

typedef enum dds_entity_kind
{
  DDS_KIND_DONTCARE,
  DDS_KIND_TOPIC,
  DDS_KIND_PARTICIPANT,
  DDS_KIND_READER,
  DDS_KIND_WRITER,
  DDS_KIND_SUBSCRIBER,
  DDS_KIND_PUBLISHER,
  DDS_KIND_COND_READ,
  DDS_KIND_COND_QUERY,
  DDS_KIND_COND_GUARD,
  DDS_KIND_WAITSET,
  DDS_KIND_DOMAIN,
  DDS_KIND_CYCLONEDDS
} dds_entity_kind_t;
#define DDS_KIND_MAX DDS_KIND_CYCLONEDDS

/* Handles are opaque pointers to implementation types */
typedef uint64_t dds_instance_handle_t;
typedef uint32_t dds_domainid_t;


/* Topic encoding instruction types */

enum dds_stream_opcode {
  /* return from subroutine, exits top-level
     [RTS,   0,   0, 0] */
  DDS_OP_RTS = 0x00 << 24,
  /* data field
     [ADR, nBY,   0, k] [offset]
     [ADR, STR,   0, k] [offset]
     [ADR, BST,   0, k] [offset] [bound]
     [ADR, SEQ, nBY, 0] [offset]
     [ADR, SEQ, STR, 0] [offset]
     [ADR, SEQ, BST, 0] [offset] [bound]
     [ADR, SEQ,   s, 0] [offset] [elem-size] [next-insn, elem-insn]
       where s = {SEQ,ARR,UNI,STU}
     [ADR, ARR, nBY, k] [offset] [alen]
     [ADR, ARR, STR, 0] [offset] [alen]
     [ADR, ARR, BST, 0] [offset] [alen] [0] [bound]
     [ADR, ARR,   s, 0] [offset] [alen] [next-insn, elem-insn] [elem-size]
         where s = {SEQ,ARR,UNI,STU}
     [ADR, UNI,   d, z] [offset] [alen] [next-insn, cases]
       where
         d = discriminant type of {1BY,2BY,4BY}
         z = default present/not present (DDS_OP_FLAG_DEF)
         offset = discriminant offset
       followed by alen case labels: in JEQ format
     note: [ADR, STU, ...] is illegal
   where
     s            = subtype
     k            = key/not key (DDS_OP_FLAG_KEY)
     [offset]     = field offset from start of element in memory
     [elem-size]  = element size in memory
     [bound]      = string bound + 1
     [alen]       = array length, number of cases
     [next-insn]  = (unsigned 16 bits) offset to instruction for next field, from start of insn
     [elem-insn]  = (unsigned 16 bits) offset to first instruction for element, from start of insn
     [cases]      = (unsigned 16 bits) offset to first case label, from start of insn
   */
  DDS_OP_ADR = 0x01 << 24,
  /* jump-to-subroutine (apparently not used at the moment)
     [JSR,   0, e]
       where
         e = (signed 16 bits) offset to first instruction in subroutine, from start of insn
             instruction sequence must end in RTS, execution resumes at instruction
             following JSR */
  DDS_OP_JSR = 0x02 << 24,
  /* union case
     [JEQ, nBY, 0] [disc] [offset]
     [JEQ, STR, 0] [disc] [offset]
     [JEQ,   s, e] [disc] [offset]
       where
         s  = subtype other than {nBY,STR}
         e  = (unsigned 16 bits) offset to first instruction for case, from start of insn
              instruction sequence must end in RTS, at which point executes continues
              at the next field's instruction as specified by the union */
  DDS_OP_JEQ = 0x03 << 24
};

enum dds_stream_typecode {
  DDS_OP_VAL_1BY = 0x01, /* one byte simple type (char, octet, boolean) */
  DDS_OP_VAL_2BY = 0x02, /* two byte simple type ((unsigned) short) */
  DDS_OP_VAL_4BY = 0x03, /* four byte simple type ((unsigned) long, enums, float) */
  DDS_OP_VAL_8BY = 0x04, /* eight byte simple type ((unsigned) long long, double) */
  DDS_OP_VAL_STR = 0x05, /* string */
  DDS_OP_VAL_BST = 0x06, /* bounded string */
  DDS_OP_VAL_SEQ = 0x07, /* sequence */
  DDS_OP_VAL_ARR = 0x08, /* array */
  DDS_OP_VAL_UNI = 0x09, /* union */
  DDS_OP_VAL_STU = 0x0a  /* struct */
};

/* primary type code for DDS_OP_ADR, DDS_OP_JEQ */
enum dds_stream_typecode_primary {
  DDS_OP_TYPE_1BY = DDS_OP_VAL_1BY << 16,
  DDS_OP_TYPE_2BY = DDS_OP_VAL_2BY << 16,
  DDS_OP_TYPE_4BY = DDS_OP_VAL_4BY << 16,
  DDS_OP_TYPE_8BY = DDS_OP_VAL_8BY << 16,
  DDS_OP_TYPE_STR = DDS_OP_VAL_STR << 16,
  DDS_OP_TYPE_BST = DDS_OP_VAL_BST << 16,
  DDS_OP_TYPE_SEQ = DDS_OP_VAL_SEQ << 16,
  DDS_OP_TYPE_ARR = DDS_OP_VAL_ARR << 16,
  DDS_OP_TYPE_UNI = DDS_OP_VAL_UNI << 16,
  DDS_OP_TYPE_STU = DDS_OP_VAL_STU << 16
};
#define DDS_OP_TYPE_BOO DDS_OP_TYPE_1BY

/* sub-type code:
   - encodes element type for DDS_OP_TYPE_{SEQ,ARR},
   - discriminant type for DDS_OP_TYPE_UNI */
enum dds_stream_typecode_subtype {
  DDS_OP_SUBTYPE_1BY = DDS_OP_VAL_1BY << 8,
  DDS_OP_SUBTYPE_2BY = DDS_OP_VAL_2BY << 8,
  DDS_OP_SUBTYPE_4BY = DDS_OP_VAL_4BY << 8,
  DDS_OP_SUBTYPE_8BY = DDS_OP_VAL_8BY << 8,
  DDS_OP_SUBTYPE_STR = DDS_OP_VAL_STR << 8,
  DDS_OP_SUBTYPE_BST = DDS_OP_VAL_BST << 8,
  DDS_OP_SUBTYPE_SEQ = DDS_OP_VAL_SEQ << 8,
  DDS_OP_SUBTYPE_ARR = DDS_OP_VAL_ARR << 8,
  DDS_OP_SUBTYPE_UNI = DDS_OP_VAL_UNI << 8,
  DDS_OP_SUBTYPE_STU = DDS_OP_VAL_STU << 8
};
#define DDS_OP_SUBTYPE_BOO DDS_OP_SUBTYPE_1BY

#define DDS_OP_FLAG_KEY 0x01 /* key field: applicable to {1,2,4,8}BY, STR, BST, ARR-of-{1,2,4,8}BY */
#define DDS_OP_FLAG_DEF 0x02 /* union has a default case (for DDS_OP_ADR | DDS_OP_TYPE_UNI) */

/* For a union: (1) the discriminator may be a key field; (2) there may be a default value;
   and (3) the discriminator can be an integral type (or enumerated - here treated as equivalent).
   What it can't be is a floating-point type. So DEF and FP need never be set at the same time.
   There are only a few flag bits, so saving one is not such a bad idea. */
#define DDS_OP_FLAG_FP  0x02 /* floating-point: applicable to {4,8}BY and arrays, sequences of them */
#define DDS_OP_FLAG_SGN 0x04 /* signed: applicable to {1,2,4,8}BY and arrays, sequences of them */

/**
 * Description : Enable or disable write batching. Overrides default configuration
 * setting for write batching (Internal/WriteBatch).
 *
 * Arguments :
 *   -# enable Enables or disables write batching for all writers.
 */
DDS_EXPORT void dds_write_set_batch (bool enable);

#if defined (__cplusplus)
}
#endif
#endif
