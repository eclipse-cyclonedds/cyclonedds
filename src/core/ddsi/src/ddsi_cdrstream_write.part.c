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

static const uint32_t *dds_stream_write_implBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops, bool is_mutable_member);

static inline bool dds_stream_write_bool_valueBO (DDS_OSTREAM_T * __restrict os, const uint8_t val)
{
  if (val > 1)
    return false;
  dds_os_put1BO (os, val);
  return true;
}

static bool dds_stream_write_enum_valueBO (DDS_OSTREAM_T * __restrict os, uint32_t insn, uint32_t val, uint32_t max)
{
  if (val > max)
    return false;
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1:
      dds_os_put1BO (os, (uint8_t) val);
      break;
    case 2:
      dds_os_put2BO (os, (uint16_t) val);
      break;
    case 4:
      dds_os_put4BO (os, val);
      break;
    default:
      abort ();
  }
  return true;
}

static bool dds_stream_write_bitmask_valueBO (DDS_OSTREAM_T * __restrict os, uint32_t insn, const void * __restrict addr, uint32_t bits_h, uint32_t bits_l)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      const uint8_t *ptr = (const uint8_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put1BO (os, *ptr);
      break;
    }
    case 2: {
      const uint16_t *ptr = (const uint16_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put2BO (os, *ptr);
      break;
    }
    case 4: {
      const uint32_t *ptr = (const uint32_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put4BO (os, *ptr);
      break;
    }
    case 8: {
      const uint64_t *ptr = (const uint64_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put8BO (os, *ptr);
      break;
    }
    default:
      abort ();
  }
  return true;
}

static void dds_stream_write_stringBO (DDS_OSTREAM_T * __restrict os, const char * __restrict val)
{
  uint32_t size = val ? (uint32_t) strlen (val) + 1 : 1;
  dds_os_put4BO (os, size);
  if (val)
    dds_os_put_bytes ((struct dds_ostream *)os, val, size);
  else
    dds_os_put1BO (os, 0);
}

static bool dds_stream_write_bool_arrBO (DDS_OSTREAM_T * __restrict os, const uint8_t * __restrict addr, uint32_t num)
{
  for (uint32_t i = 0; i < num; i++)
  {
    if (!dds_stream_write_bool_valueBO (os, addr[i]))
      return false;
  }
  return true;
}

static bool dds_stream_write_enum_arrBO (DDS_OSTREAM_T * __restrict os, uint32_t insn, const uint32_t * __restrict addr, uint32_t num, uint32_t max)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1:
      for (uint32_t i = 0; i < num; i++)
      {
        if (addr[i] > max)
          return false;
        dds_os_put1BO (os, (uint8_t) addr[i]);
      }
      break;
    case 2:
      for (uint32_t i = 0; i < num; i++)
      {
        if (addr[i] > max)
          return false;
        dds_os_put2BO (os, (uint16_t) addr[i]);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < num; i++)
      {
        if (addr[i] > max)
          return false;
        dds_os_put4BO (os, addr[i]);
      }
      break;
    default:
      abort ();
  }
  return true;
}

static bool dds_stream_write_bitmask_arrBO (DDS_OSTREAM_T * __restrict os, uint32_t insn, const void * __restrict addr, uint32_t num, uint32_t bits_h, uint32_t bits_l)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      const uint8_t *ptr = (const uint8_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put1BO (os, ptr[i]);
      }
      break;
    }
    case 2: {
      const uint16_t *ptr = (const uint16_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put2BO (os, ptr[i]);
      }
      break;
    }
    case 4: {
      const uint32_t *ptr = (const uint32_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put4BO (os, ptr[i]);
      }
      break;
    }
    case 8: {
      const uint64_t *ptr = (const uint64_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put8BO (os, ptr[i]);
      }
      break;
    }
    default:
      abort ();
  }
  return true;
}

static const uint32_t *dds_stream_write_seqBO (DDS_OSTREAM_T * __restrict os, const char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const dds_sequence_t * const seq = (const dds_sequence_t *) addr;
  uint32_t offs = 0, xcdrv = ((struct dds_ostream *)os)->m_xcdr_version;

  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  uint32_t bound = bound_op ? ops[2] : 0;

  if (is_dheader_needed (subtype, xcdrv))
  {
    /* reserve space for DHEADER */
    dds_os_reserve4BO (os);
    offs = ((struct dds_ostream *)os)->m_index;
  }

  const uint32_t num = seq->_length;
  if (bound && num > bound)
  {
    dds_ostreamBO_fini (os);
    return NULL;
  }

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
      case DDS_OP_VAL_BLN:
        if (!dds_stream_write_bool_arrBO (os, (const uint8_t *) seq->_buffer, num))
          return NULL;
        ops += 2 + bound_op;
        break;
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
        const uint32_t elem_size = get_primitive_size (subtype);
        const align_t align = get_align (xcdrv, elem_size);
        void * dst;
        /* Combining put bytes and swap into a single step would improve the performance
           of writing data in non-native endianess. But in most cases the data will
           be written in native endianess, and in that case the swap is a no-op (for writing
           keys a separate function is used). */
        dds_os_put_bytes_aligned ((struct dds_ostream *)os, seq->_buffer, num, elem_size, align, &dst);
        dds_stream_to_BO_insitu (dst, elem_size, num);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_ENU:
        if (!dds_stream_write_enum_arrBO (os, insn, (const uint32_t *) seq->_buffer, num, ops[2 + bound_op]))
          return NULL;
        ops += 3 + bound_op;
        break;
      case DDS_OP_VAL_BMK: {
        if (!dds_stream_write_bitmask_arrBO (os, insn, seq->_buffer, num, ops[2 + bound_op], ops[3 + bound_op]))
          return NULL;
        ops += 4 + bound_op;
        break;
      }
      case DDS_OP_VAL_STR: {
        const char **ptr = (const char **) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_stringBO (os, ptr[i]);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_BST: {
        const char *ptr = (const char *) seq->_buffer;
        const uint32_t elem_size = ops[2 + bound_op];
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_stringBO (os, ptr + i * elem_size);
        ops += 3 + bound_op;
        break;
      }
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t elem_size = ops[2 + bound_op];
        const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
        uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
        const char *ptr = (const char *) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          if (!dds_stream_write_implBO (os, ptr + i * elem_size, jsr_ops, false))
            return NULL;
        ops += (jmp ? jmp : (4 + bound_op)); /* FIXME: why would jmp be 0? */
        break;
      }
      case DDS_OP_VAL_EXT:
        abort (); /* op type EXT as sequence subtype not supported */
        return NULL;
    }
  }

  /* write DHEADER */
  if (is_dheader_needed (subtype, xcdrv))
    *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = to_BO4u(((struct dds_ostream *)os)->m_index - offs);

  return ops;
}

static const uint32_t *dds_stream_write_arrBO (DDS_OSTREAM_T * __restrict os, const char * __restrict addr, const uint32_t * __restrict ops, uint32_t insn)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t offs = 0, xcdrv = ((struct dds_ostream *)os)->m_xcdr_version;
  if (is_dheader_needed (subtype, xcdrv))
  {
    /* reserve space for DHEADER */
    dds_os_reserve4BO (os);
    offs = ((struct dds_ostream *)os)->m_index;
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN:
      if (!dds_stream_write_bool_arrBO (os, (const uint8_t *) addr, num))
        return NULL;
      ops += 3;
      break;
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      const align_t align = get_align (xcdrv, elem_size);
      void * dst;
      /* See comment for stream_write_seq, swap is a no-op in most cases */
      dds_os_put_bytes_aligned ((struct dds_ostream *)os, addr, num, elem_size, align, &dst);
      dds_stream_to_BO_insitu (dst, elem_size, num);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_ENU:
      if (!dds_stream_write_enum_arrBO (os, insn, (const uint32_t *) addr, num, ops[3]))
        return NULL;
      ops += 4;
      break;
    case DDS_OP_VAL_BMK:
      if (!dds_stream_write_bitmask_arrBO (os, insn, addr, num, ops[3], ops[4]))
        return NULL;
      ops += 5;
      break;
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
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!dds_stream_write_implBO (os, addr + i * elem_size, jsr_ops, false))
          return NULL;
      ops += (jmp ? jmp : 5);
      break;
    }
    case DDS_OP_VAL_EXT:
      abort (); /* op type EXT as array subtype not supported */
      break;
  }

  /* write DHEADER */
  if (is_dheader_needed (subtype, xcdrv))
    *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = to_BO4u(((struct dds_ostream *)os)->m_index - offs);

  return ops;
}

static bool dds_stream_write_union_discriminantBO (DDS_OSTREAM_T * __restrict os, const uint32_t * __restrict ops, uint32_t insn, const void * __restrict addr, uint32_t *disc)
{
  assert (disc);
  enum dds_stream_typecode type = DDS_OP_SUBTYPE (insn);
  assert (type == DDS_OP_VAL_BLN || type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_ENU);
  switch (type)
  {
    case DDS_OP_VAL_BLN:
      *disc = *((const uint8_t *) addr);
      if (!dds_stream_write_bool_valueBO (os, (uint8_t) *disc))
        return false;
      break;
    case DDS_OP_VAL_1BY:
      *disc = *((const uint8_t *) addr);
      dds_os_put1BO (os, (uint8_t) *disc);
      break;
    case DDS_OP_VAL_2BY:
      *disc = *((const uint16_t *) addr);
      dds_os_put2BO (os, (uint16_t) *disc);
      break;
    case DDS_OP_VAL_4BY:
      *disc = *((const uint32_t *) addr);
      dds_os_put4BO (os, *disc);
      break;
    case DDS_OP_VAL_ENU:
      *disc = *((const uint32_t *) addr);
      if (!dds_stream_write_enum_valueBO (os, insn, *disc, ops[4]))
        return false;
      break;
    default:
      abort ();
  }
  return true;
}

static const uint32_t *dds_stream_write_uniBO (DDS_OSTREAM_T * __restrict os, const char * __restrict discaddr, const char * __restrict baseaddr, const uint32_t * __restrict ops, uint32_t insn)
{
  uint32_t disc;
  if (!dds_stream_write_union_discriminantBO (os, ops, insn, discaddr, &disc))
    return NULL;
  uint32_t const * const jeq_op = find_union_case (ops, disc);
  ops += DDS_OP_ADR_JMP (ops[3]);
  if (jeq_op)
  {
    const enum dds_stream_typecode valtype = DDS_JEQ_TYPE (jeq_op[0]);
    const void *valaddr = baseaddr + jeq_op[2];

    /* Union members cannot be optional, only external. For string types, the pointer
       is dereferenced below (and there is no extra pointer indirection when using
       @external for STR types) */
    if (op_type_external (jeq_op[0]) && valtype != DDS_OP_VAL_STR)
    {
      assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
      valaddr = *(char **) valaddr;
      assert (valaddr);
    }

    switch (valtype)
    {
      case DDS_OP_VAL_BLN:
        if (!dds_stream_write_bool_valueBO (os, *(const uint8_t *) valaddr))
          return NULL;
        break;
      case DDS_OP_VAL_1BY: dds_os_put1BO (os, *(const uint8_t *) valaddr); break;
      case DDS_OP_VAL_2BY: dds_os_put2BO (os, *(const uint16_t *) valaddr); break;
      case DDS_OP_VAL_4BY: dds_os_put4BO (os, *(const uint32_t *) valaddr); break;
      case DDS_OP_VAL_8BY: dds_os_put8BO (os, *(const uint64_t *) valaddr); break;
      case DDS_OP_VAL_ENU:
        if (!dds_stream_write_enum_valueBO (os, jeq_op[0], *((const uint32_t *) valaddr), jeq_op[3]))
          return NULL;
        break;
      case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, *(const char **) valaddr); break;
      case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, (const char *) valaddr); break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        if (!dds_stream_write_implBO (os, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false))
          return NULL;
        break;
      case DDS_OP_VAL_EXT:
        abort (); /* op type EXT as union subtype not supported */
        break;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_write_adrBO (uint32_t insn, DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops, bool is_mutable_member)
{
  const void *addr = data + ops[1];
  if (op_type_external (insn) || op_type_optional (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR)
    addr = *(char **) addr;
  if (op_type_optional (insn))
  {
    if (!is_mutable_member)
      dds_os_put1BO (os, addr ? 1 : 0);
    if (!addr)
      return dds_stream_skip_adr (insn, ops);
  }
  assert (addr || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
      if (!dds_stream_write_bool_valueBO (os, *((const uint8_t *) addr)))
        return NULL;
      ops += 2;
      break;
    case DDS_OP_VAL_1BY: dds_os_put1BO (os, *((const uint8_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_2BY: dds_os_put2BO (os, *((const uint16_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_4BY: dds_os_put4BO (os, *((const uint32_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_8BY: dds_os_put8BO (os, *((const uint64_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_ENU:
      if (!dds_stream_write_enum_valueBO (os, insn, *((const uint32_t *) addr), ops[2]))
        return NULL;
      ops += 3;
      break;
    case DDS_OP_VAL_BMK:
      if (!dds_stream_write_bitmask_valueBO (os, insn, addr, ops[2], ops[3]))
        return NULL;
      ops += 4;
      break;
    case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, (const char *) addr); ops += 2; break;
    case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, (const char *) addr); ops += 3; break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_write_seqBO (os, addr, ops, insn); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_write_arrBO (os, addr, ops, insn); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_write_uniBO (os, addr, data, ops, insn); break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

      /* skip DLC instruction for base type, so that the DHEADER is not
          serialized for base types */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;

      /* don't forward is_mutable_member, subtype can have other extensibility */
      if (!dds_stream_write_implBO (os, addr, jsr_ops, false))
        return NULL;
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU: abort (); break; /* op type STU only supported as subtype */
  }
  return ops;
}

static const uint32_t *dds_stream_write_delimitedBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t offs = dds_os_reserve4BO (os);
  if (!(ops = dds_stream_write_implBO (os, data, ops + 1, false)))
    return NULL;

  /* add dheader, which is the serialized size of the data */
  *((uint32_t *) (os->x.m_buffer + offs - 4)) = to_BO4u (os->x.m_index - offs);
  return ops;
}

static bool dds_stream_write_pl_memberBO (uint32_t mid, DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  assert (!(mid & ~EMHEADER_MEMBERID_MASK));
  uint32_t lc = get_length_code (ops);
  assert (lc <= LENGTH_CODE_ALSO_NEXTINT8);
  uint32_t data_offs = (lc != LENGTH_CODE_NEXTINT) ? dds_os_reserve4BO (os) : dds_os_reserve8BO (os);
  if (!(dds_stream_write_implBO (os, data, ops, true)))
    return false;

  /* get must-understand flag from first member op */
  uint32_t flags = DDS_OP_FLAGS (ops[0]);
  bool must_understand = flags & DDS_OP_FLAG_MU;

  /* add emheader with data length code and flags and optionally the serialized size of the data */
  uint32_t em_hdr = 0;
  if (must_understand)
    em_hdr |= EMHEADER_FLAG_MUSTUNDERSTAND;
  em_hdr |= lc << 28;
  em_hdr |= mid & EMHEADER_MEMBERID_MASK;

  uint32_t *em_hdr_ptr = (uint32_t *) (os->x.m_buffer + data_offs - (lc == LENGTH_CODE_NEXTINT ? 8 : 4));
  em_hdr_ptr[0] = to_BO4u (em_hdr);
  if (lc == LENGTH_CODE_NEXTINT)
    em_hdr_ptr[1] = to_BO4u (os->x.m_index - data_offs);  /* member size in next_int field in emheader */
  return true;
}

static const uint32_t *dds_stream_write_pl_memberlistBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  uint32_t insn;
  while (ops && (insn = *ops) != DDS_OP_RTS)
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
          if (!dds_stream_write_pl_memberlistBO (os, data, plm_ops))
            return NULL;
        }
        else if (is_member_present (data, plm_ops))
        {
          uint32_t member_id = ops[1];
          if (!dds_stream_write_pl_memberBO (member_id, os, data, plm_ops))
            return NULL;
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

static const uint32_t *dds_stream_write_implBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops, bool is_mutable_member)
{
  uint32_t insn;
  while (ops && (insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_write_adrBO (insn, os, data, ops, is_mutable_member);
        break;
      case DDS_OP_JSR:
        if (!dds_stream_write_implBO (os, data, ops + DDS_OP_JUMP (insn), is_mutable_member))
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
        abort ();
        break;
      case DDS_OP_DLC:
        assert (((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = dds_stream_write_delimitedBO (os, data, ops);
        break;
      case DDS_OP_PLC:
        assert (((struct dds_ostream *)os)->m_xcdr_version == CDR_ENC_VERSION_2);
        ops = dds_stream_write_plBO (os, data, ops);
        break;
    }
  }
  return ops;
}

const uint32_t *dds_stream_writeBO (DDS_OSTREAM_T * __restrict os, const char * __restrict data, const uint32_t * __restrict ops)
{
  return dds_stream_write_implBO (os, data, ops, false);
}
