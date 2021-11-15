/*
 * Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

static void dds_stream_write_stringBO (DDS_OSTREAM_T * __restrict os, const char * __restrict val)
{
  uint32_t size = val ? (uint32_t) strlen (val) + 1 : 1;
  dds_os_put4BO (os, size);
  if (val)
    dds_os_put_bytes ((struct dds_ostream *)os, val, size);
  else
    dds_os_put1BO (os, 0);
}

static const uint32_t *dds_stream_write_seqBO (DDS_OSTREAM_T * __restrict os, const char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const dds_sequence_t * const seq = (const dds_sequence_t *) addr;
  uint32_t offs = 0;
  bool is_xcdr2 = ((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2;

  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  if (subtype > DDS_OP_VAL_8BY && is_xcdr2)
  {
    /* reserve space for DHEADER */
    dds_os_reserve4BO (os);
    offs = ((struct dds_ostream *)os)->m_index;
  }

  const uint32_t num = seq->_length;
  dds_os_put4BO (os, num);

  if (num == 0)
  {
    ops = skip_sequence_insns (insn, ops);
  }
  else
  {
    /* following length, stream is aligned to mod 4 */
    switch (subtype)
    {
      case DDS_OP_VAL_ENU:
        ops++;
        /* fall through */
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
        const uint32_t elem_size = get_type_size (subtype);
        const uint32_t align = is_xcdr2 && subtype == DDS_OP_VAL_8BY ? 4 : elem_size;
        void * dst;
        /* Combining put bytes and swap into a single step would improve the performance
           of writing data in non-native endianess. But in most cases the data will
           be written in native endianess, and in that case the swap is a no-op (for writing
           keys a separate function is used). */
        dds_os_put_bytes_aligned ((struct dds_ostream *)os, seq->_buffer, num, elem_size, align, &dst);
        dds_stream_to_BO_insitu (dst, elem_size, num);
        ops += 2;
        break;
      }
      case DDS_OP_VAL_STR: {
        const char **ptr = (const char **) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_stringBO (os, ptr[i]);
        ops += 2;
        break;
      }
      case DDS_OP_VAL_BST: {
        const char *ptr = (const char *) seq->_buffer;
        const uint32_t elem_size = ops[2];
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_stringBO (os, ptr + i * elem_size);
        ops += 3;
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t elem_size = ops[2];
        const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
        uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
        const char *ptr = (const char *) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          (void) dds_stream_writeBO (os, ptr + i * elem_size, jsr_ops);
        ops += (jmp ? jmp : 4); /* FIXME: why would jmp be 0? */
        break;
      }
      case DDS_OP_VAL_EXT: {
        abort (); /* op type EXT as sequence subtype not supported */
        return NULL;
      }
    }
  }

  if (subtype > DDS_OP_VAL_8BY && is_xcdr2)
  {
    /* write DHEADER */
    *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = to_BO4u(((struct dds_ostream *)os)->m_index - offs);
  }

  return ops;
}

static const uint32_t *dds_stream_write_arrBO (DDS_OSTREAM_T * __restrict os, const char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t offs = 0;
  bool is_xcdr2 = ((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2;
  if (subtype > DDS_OP_VAL_8BY && ((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2)
  {
    /* reserve space for DHEADER */
    dds_os_reserve4BO (os);
    offs = ((struct dds_ostream *)os)->m_index;
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_ENU:
      ops++;
      /* fall through */
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_type_size (subtype);
      const uint32_t align = is_xcdr2 && subtype == DDS_OP_VAL_8BY ? 4 : elem_size;
      void * dst;
      /* See comment for stream_write_seq, swap is a no-op in most cases */
      dds_os_put_bytes_aligned ((struct dds_ostream *)os, addr, num, elem_size, align, &dst);
      dds_stream_to_BO_insitu (dst, elem_size, num);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_STR: {
      const char **ptr = (const char **) addr;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_stringBO (os, ptr[i]);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BST: {
      const char *ptr = (const char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_stringBO (os, ptr + i * elem_size);
      ops += 5;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        (void) dds_stream_writeBO (os, addr + i * elem_size, jsr_ops);
      ops += (jmp ? jmp : 5);
      break;
    }
    case DDS_OP_VAL_EXT: {
      abort (); /* op type EXT as array subtype not supported */
      break;
    }
  }

  if (subtype > DDS_OP_VAL_8BY && is_xcdr2)
  {
    /* write DHEADER */
    *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = to_BO4u(((struct dds_ostream *)os)->m_index - offs);
  }

  return ops;
}

static uint32_t write_union_discriminantBO (DDS_OSTREAM_T * __restrict os, enum dds_stream_typecode type, const void * __restrict addr)
{
  assert (type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_ENU);
  switch (type)
  {
    case DDS_OP_VAL_1BY: { uint8_t  d8  = *((const uint8_t *) addr); dds_os_put1BO (os, d8); return d8; }
    case DDS_OP_VAL_2BY: { uint16_t d16 = *((const uint16_t *) addr); dds_os_put2BO (os, d16); return d16; }
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: { uint32_t d32 = *((const uint32_t *) addr); dds_os_put4BO (os, d32); return d32; }
    default: return 0;
  }
}

static const uint32_t *dds_stream_write_uniBO (DDS_OSTREAM_T * __restrict os, const char * __restrict discaddr, const char * __restrict baseaddr, const uint32_t * __restrict ops, uint32_t insn)
{
  const uint32_t disc = write_union_discriminantBO (os, DDS_OP_SUBTYPE (insn), discaddr);
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    const void *valaddr = baseaddr + jeq_op[2];

    if (op_type_external (jeq_op[0]) && valtype != DDS_OP_VAL_STR)
    {
      assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
      valaddr = *(char **) valaddr;
      assert (valaddr);
    }

    switch (valtype)
    {
      case DDS_OP_VAL_1BY: dds_os_put1BO (os, *(const uint8_t *) valaddr); break;
      case DDS_OP_VAL_2BY: dds_os_put2BO (os, *(const uint16_t *) valaddr); break;
      case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: dds_os_put4BO (os, *(const uint32_t *) valaddr); break;
      case DDS_OP_VAL_8BY: dds_os_put8BO (os, *(const uint64_t *) valaddr); break;
      case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, *(const char **) valaddr); break;
      case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, (const char *) valaddr); break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_ARR:
        (void) dds_stream_writeBO (os, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]));
        break;
      case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t *jsr_ops = jeq_op + DDS_OP_ADR_JSR (jeq_op[0]);
        (void) dds_stream_writeBO (os, valaddr, jsr_ops);
        break;
      }
      case DDS_OP_VAL_EXT: {
        abort (); /* op type EXT as union subtype not supported */
        break;
      }
    }
  }
  return ops;
}

static const uint32_t *dds_stream_write_delimitedBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t offs = dds_os_reserve4BO (os);
  ops = dds_stream_writeBO (os, data, ops + 1);
  *((uint32_t *) (os->x.m_buffer + offs - 4)) = to_BO4u (os->x.m_index - offs);
  return ops;
}

static void dds_stream_write_pl_memberBO (bool must_understand, uint32_t mid, DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  assert (!(mid & ~EMHEADER_MEMBERID_MASK));
  uint32_t lc = get_length_code (ops);
  assert (lc < LENGTH_CODE_ALSO_NEXTINT8);
  uint32_t data_offs = (lc != LENGTH_CODE_NEXTINT) ? dds_os_reserve4BO (os) : dds_os_reserve8BO (os);
  (void) dds_stream_writeBO (os, data, ops);

  uint32_t em_hdr = 0;
  if (must_understand)
    em_hdr |= EMHEADER_FLAG_MUSTUNDERSTAND;
  em_hdr |= lc << 28;
  em_hdr |= mid & EMHEADER_MEMBERID_MASK;

  uint32_t *em_hdr_ptr = (uint32_t *) (os->x.m_buffer + data_offs - (lc == LENGTH_CODE_NEXTINT ? 8 : 4));
  em_hdr_ptr[0] = to_BO4u (em_hdr);
  if (lc == LENGTH_CODE_NEXTINT)
    em_hdr_ptr[1] = to_BO4u (os->x.m_index - data_offs);  /* member size in next_int field in emheader */
}

static const uint32_t *dds_stream_write_pl_memberlistBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_PLM: {
        uint32_t flags = DDS_PLM_FLAGS (insn);
        const uint32_t *plm_ops = ops + DDS_OP_ADR_PLM (insn);
        if (flags & DDS_OP_FLAG_BASE)
        {
          assert (plm_ops[0] == DDS_OP_PLC);
          plm_ops++; /* skip PLC op to go to first PLM for the base type */
          (void) dds_stream_write_pl_memberlistBO (os, data, plm_ops);
        }
        else
        {
          uint32_t member_id = ops[1];
          dds_stream_write_pl_memberBO (flags & DDS_OP_FLAG_MU, member_id, os, data, plm_ops);
        }
        ops += 2;
        break;
      }
      default:
        abort (); /* other ops not supported at this point */
        break;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_write_plBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  /* skip PLC op */
  ops++;

  /* alloc space for dheader */
  dds_os_reserve4BO (os);
  uint32_t data_offs = os->x.m_index;

  /* write members, including members from base types */
  ops = dds_stream_write_pl_memberlistBO (os, data, ops);

  /* write serialized size in dheader */
  *((uint32_t *) (os->x.m_buffer + data_offs - 4)) = to_BO4u (os->x.m_index - data_offs);
  return ops;
}

const uint32_t *dds_stream_writeBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        const void *addr = data + ops[1];
        if (op_type_external (insn) && DDS_OP_TYPE (insn) != DDS_OP_VAL_STR)
        {
          addr = *(char **) addr;
          assert (addr);
        }

        switch (DDS_OP_TYPE (insn))
        {
          case DDS_OP_VAL_1BY: dds_os_put1BO (os, *((const uint8_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_2BY: dds_os_put2BO (os, *((const uint16_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_4BY: dds_os_put4BO (os, *((const uint32_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_8BY: dds_os_put8BO (os, *((const uint64_t *) addr)); ops += 2; break;
          case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, *((const char **) addr)); ops += 2; break;
          case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, (const char *) addr); ops += 3; break;
          case DDS_OP_VAL_SEQ: ops = dds_stream_write_seqBO (os, addr, ops, insn); break;
          case DDS_OP_VAL_ARR: ops = dds_stream_write_arrBO (os, addr, ops, insn); break;
          case DDS_OP_VAL_UNI: ops = dds_stream_write_uniBO (os, addr, data, ops, insn); break;
          case DDS_OP_VAL_ENU: dds_os_put4BO (os, *((const uint32_t *) addr)); ops += 3; break;
          case DDS_OP_VAL_EXT: {
            const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
            const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

            /* skip DLC instruction for base type, so that the DHEADER is not
               serialized for base types */
            if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
              jsr_ops++;

            (void) dds_stream_writeBO (os, addr, jsr_ops);
            ops += jmp ? jmp : 3;
            break;
          }
          case DDS_OP_VAL_STU: abort (); break; /* op type STU only supported as subtype */
        }
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_writeBO (os, data, ops + DDS_OP_JUMP (insn));
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        assert (((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = dds_stream_write_delimitedBO (os, data, ops);
        break;
      }
      case DDS_OP_PLC: {
        assert (((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = dds_stream_write_plBO (os, data, ops);
        break;
      }
    }
  }
  return ops;
}
