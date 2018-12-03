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
#include <assert.h>
#include <string.h>
#include "dds__alloc.h"
#include "dds__stream.h"
#include "os/os_heap.h"
#include "ddsi/q_config.h"

/*
#define OP_DEBUG_FREE 1
*/

#if defined OP_DEBUG_FREE
static const char * stream_op_type[11] =
{
  NULL, "1Byte", "2Byte", "4Byte", "8Byte", "String",
  "BString", "Sequence", "Array", "Union", "Struct"
};
#endif

static dds_allocator_t dds_allocator_fns = { os_malloc, os_realloc, os_free };

void * dds_alloc (size_t size)
{
  void * ret = (dds_allocator_fns.malloc) (size);
  if (ret)
  {
    memset (ret, 0, size);
  }
  else 
  {
    DDS_FAIL ("dds_alloc");
  }
  return ret;
}

void * dds_realloc (void * ptr, size_t size)
{
  void * ret = (dds_allocator_fns.realloc) (ptr, size);
  if (ret == NULL) DDS_FAIL ("dds_realloc");
  return ret;
}

void * dds_realloc_zero (void * ptr, size_t size)
{
  void * ret = dds_realloc (ptr, size);
  if (ret)
  {
    memset (ret, 0, size);
  }
  return ret;
}

void dds_free (void * ptr)
{
  if (ptr) (dds_allocator_fns.free) (ptr);
}

char * dds_string_alloc (size_t size)
{
  return (char*) dds_alloc (size + 1);
}

char * dds_string_dup (const char * str)
{
  char * ret = NULL;
  if (str)
  {
    size_t sz = strlen (str) + 1;
    ret = dds_alloc (sz);
    memcpy (ret, str, sz);
  }
  return ret;
}

void dds_string_free (char * str)
{
  dds_free (str);
}

void dds_sample_free_contents (char * data, const uint32_t * ops)
{
  uint32_t op;
  uint32_t type;
  uint32_t num;
  uint32_t subtype;
  char * addr;

  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP_MASK & op)
    {
      case DDS_OP_ADR:
      {
        type = DDS_OP_TYPE (op);
#ifdef OP_DEBUG_FREE
        DDS_TRACE("F-ADR: %s offset %d\n", stream_op_type[type], ops[1]);
#endif
        addr = data + ops[1];
        ops += 2;
        switch (type)
        {
          case DDS_OP_VAL_1BY:
          case DDS_OP_VAL_2BY:
          case DDS_OP_VAL_4BY:
          case DDS_OP_VAL_8BY:
          {
            break;
          }
          case DDS_OP_VAL_STR:
          {
#ifdef OP_DEBUG_FREE
            DDS_TRACE("F-STR: @ %p %s\n", addr, *((char**) addr));
#endif
            dds_free (*((char**) addr));
            *((char**) addr) = NULL;
            break;
          }
          case DDS_OP_VAL_SEQ:
          {
            dds_sequence_t * seq = (dds_sequence_t*) addr;
            subtype = DDS_OP_SUBTYPE (op);
            num = (seq->_maximum > seq->_length) ? seq->_maximum : seq->_length;

#ifdef OP_DEBUG_FREE
            DDS_TRACE("F-SEQ: of %s\n", stream_op_type[subtype]);
#endif
            if ((seq->_release && num) || (subtype > DDS_OP_VAL_STR))
            {
              switch (subtype)
              {
                case DDS_OP_VAL_1BY:
                case DDS_OP_VAL_2BY:
                case DDS_OP_VAL_4BY:
                case DDS_OP_VAL_8BY:
                {
                  break;
                }
                case DDS_OP_VAL_BST:
                {
                  ops++;
                  break;
                }
                case DDS_OP_VAL_STR:
                {
                  char ** ptr = (char**) seq->_buffer;
                  while (num--)
                  {
                    dds_free (*ptr++);
                  }
                  break;
                }
                default:
                {
                  const uint32_t elem_size = *ops++;
                  const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                  const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                  char * ptr = (char*) seq->_buffer;
 
                  while (num--)
                  {
                    dds_sample_free_contents (ptr, jsr_ops);
                    ptr += elem_size;
                  }
                  ops += jmp ? (jmp - 3) : 1;
                  break;
                }
              }
            }
            if (seq->_release)
            {
              dds_free (seq->_buffer);
              seq->_buffer = NULL;
            }
            break;
          }
          case DDS_OP_VAL_ARR:
          {
            subtype = DDS_OP_SUBTYPE (op);
            num = *ops++;

#ifdef OP_DEBUG_FREE
            DDS_TRACE("F-ARR: of %s size %d\n", stream_op_type[subtype], num);
#endif
            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              case DDS_OP_VAL_2BY:
              case DDS_OP_VAL_4BY:
              case DDS_OP_VAL_8BY:
              {
                break;
              }
              case DDS_OP_VAL_STR:
              {
                char ** ptr = (char**) addr;
                while (num--)
                {
                  dds_free (*ptr++);
                }
                break;
              }
              case DDS_OP_VAL_BST:
              {
                ops += 2;
                break;
              }
              default:
              {
                const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (*ops) - 3;
                const uint32_t jmp = DDS_OP_ADR_JMP (*ops);
                const uint32_t elem_size = ops[1];

                while (num--)
                {
                  dds_sample_free_contents (addr, jsr_ops);
                  addr += elem_size;
                }
                ops += jmp ? (jmp - 3) : 2;
                break;
              }
            }
            break;
          }
          case DDS_OP_VAL_UNI:
          {
            const bool has_default = op & DDS_OP_FLAG_DEF;
            subtype = DDS_OP_SUBTYPE (op);
            num = ops[0];
            const uint32_t * jeq_op = ops + DDS_OP_ADR_JSR (ops[1]) - 2;
            uint32_t disc = 0;

            assert (subtype <= DDS_OP_VAL_4BY);

#ifdef OP_DEBUG_FREE
            DDS_TRACE("F-UNI: switch %s cases %d\n", stream_op_type[subtype], num);
#endif
            /* Get discriminant */

            switch (subtype)
            {
              case DDS_OP_VAL_1BY:
              {
                disc = *((uint8_t*) addr);
                break;
              }
              case DDS_OP_VAL_2BY:
              {
                disc = *((uint16_t*) addr);
                break;
              }
              case DDS_OP_VAL_4BY:
              {
                disc = *((uint32_t*) addr);
                break;
              }
              default: assert (0);
            }

            /* Free case matching discriminant */

            while (num--)
            {
              assert ((DDS_OP_MASK & jeq_op[0]) == DDS_OP_JEQ);
              if ((jeq_op[1] == disc) || (has_default && (num == 0)))
              {
                subtype = DDS_JEQ_TYPE (jeq_op[0]);
                addr = data + jeq_op[2];

                switch (subtype)
                {
                  case DDS_OP_VAL_1BY:
                  case DDS_OP_VAL_2BY:
                  case DDS_OP_VAL_4BY:
                  case DDS_OP_VAL_8BY:
                  case DDS_OP_VAL_BST:
                  {
                    break;
                  }
                  case DDS_OP_VAL_STR:
                  {
                    dds_free (*((char**) addr));
                    *((char**) addr) = NULL;
                    break;
                  }
                  default:
                  {
                    dds_sample_free_contents (addr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
                    break;
                  }
                }
                break;
              }
              jeq_op += 3;
            }

            /* Jump to next instruction */

            ops += DDS_OP_ADR_JMP (ops[1]) - 2;
            break;
          }
          case DDS_OP_VAL_BST:
          {
            ops++;
            break;
          }
          default: assert (0);
        }
        break;
      }
      case DDS_OP_JSR: /* Implies nested type */
      {
#ifdef OP_DEBUG_FREE
        DDS_TRACE("F-JSR: %d\n", DDS_OP_JUMP (op));
#endif
        dds_sample_free_contents (data, ops + DDS_OP_JUMP (op));
        ops++;
        break;
      }
      default: assert (0);
    }
  }
#ifdef OP_DEBUG_FREE
  DDS_TRACE("F-RTS:\n");
#endif
}

static void dds_sample_free_key (char * sample, const struct dds_topic_descriptor * desc)
{
  uint32_t i;
  const uint32_t * op;

  for (i = 0; i < desc->m_nkeys; i++)
  {
    op = desc->m_ops + desc->m_keys[i].m_index;
    if (DDS_OP_TYPE (*op) == DDS_OP_VAL_STR)
    {
      dds_free (*(char**)(sample + op[1]));
    }
  }
}

void dds_sample_free (void * sample, const struct dds_topic_descriptor * desc, dds_free_op_t op)
{
  assert (desc);

  if (sample)
  {
    if (op & DDS_FREE_CONTENTS_BIT)
    {
      dds_sample_free_contents ((char*) sample, desc->m_ops);
    }
    else if (op & DDS_FREE_KEY_BIT)
    {
      dds_sample_free_key ((char*) sample, desc);
    }
    if (op & DDS_FREE_ALL_BIT)
    {
      dds_free (sample);
    }
  }
}
