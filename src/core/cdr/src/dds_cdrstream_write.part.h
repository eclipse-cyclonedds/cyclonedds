// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

static const uint32_t *dds_stream_write_implBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind);

static inline bool dds_stream_write_bool_valueBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint8_t val)
{
  dds_os_put1BO (os, allocator, val != 0);
  return true;
}

static bool dds_stream_write_enum_valueBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, uint32_t insn, uint32_t val, uint32_t max)
{
  if (val > max)
    return false;
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1:
      dds_os_put1BO (os, allocator, (uint8_t) val);
      break;
    case 2:
      dds_os_put2BO (os, allocator, (uint16_t) val);
      break;
    case 4:
      dds_os_put4BO (os, allocator, val);
      break;
    default:
      abort ();
  }
  return true;
}

static bool dds_stream_write_bitmask_valueBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, uint32_t insn, const void *addr, uint32_t bits_h, uint32_t bits_l)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      const uint8_t *ptr = (const uint8_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put1BO (os, allocator, *ptr);
      break;
    }
    case 2: {
      const uint16_t *ptr = (const uint16_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put2BO (os, allocator, *ptr);
      break;
    }
    case 4: {
      const uint32_t *ptr = (const uint32_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put4BO (os, allocator, *ptr);
      break;
    }
    case 8: {
      const uint64_t *ptr = (const uint64_t *) addr;
      if (!bitmask_value_valid (*ptr, bits_h, bits_l))
        return false;
      dds_os_put8BO (os, allocator, *ptr);
      break;
    }
    default:
      abort ();
  }
  return true;
}

static void dds_stream_write_stringBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const char *val)
{
  uint32_t size = val ? (uint32_t) strlen (val) + 1 : 1;
  dds_os_put4BO (os, allocator, size);
  if (val)
    dds_os_put_bytes_base (&os->x, allocator, val, size);
  else
    dds_os_put1BO (os, allocator, 0);
}

static void dds_stream_write_wstringBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const wchar_t *val)
{
  if (val == NULL)
    dds_os_put4BO (os, allocator, 0);
  else
  {
    const uint32_t n = (uint32_t) wstring_utf16_len (val);
    dds_os_put4BO (os, allocator, 2 * n);
    dds_cdr_resize (&os->x, allocator, 2 * n);
    uint16_t * const dst = (uint16_t *) (os->x.m_buffer + os->x.m_index);
    uint32_t di = 0;
    for (uint32_t i = 0; val[i] != L'\0'; i++)
    {
      assert (di < n);
      if ((uint32_t) val[i] < 0x10000)
        dst[di++] = (uint16_t) val[i];
      else
      {
        const uint32_t u = (uint32_t) val[i] - 0x10000;
        const uint16_t w1 = (uint16_t) (0xd800 + ((u >> 10) & 0x3ff));
        const uint16_t w2 = (uint16_t) (0xdc00 + (u & 0x3ff));
        dst[di++] = w1;
        assert (di < n);
        dst[di++] = w2;
      }
    }
    os->x.m_index += 2 * n;
  }
}

ddsrt_attribute_warn_unused_result
static bool dds_stream_write_wcharBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const wchar_t val)
{
  // surrogates are forbidden
  if ((uint32_t) val >= 0xd800 && (uint32_t) val < 0xe000)
    return false;
  else if ((uint32_t) val > 0xffff) // out-of-range
    return false;
  dds_os_put2BO (os, allocator, (uint16_t) val);
  return true;
}

static bool dds_stream_write_bool_arrBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint8_t *addr, uint32_t num)
{
  for (uint32_t i = 0; i < num; i++)
  {
    if (!dds_stream_write_bool_valueBO (os, allocator, addr[i]))
      return false;
  }
  return true;
}

static bool dds_stream_write_enum_arrBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, uint32_t insn, const uint32_t *addr, uint32_t num, uint32_t max)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1:
      for (uint32_t i = 0; i < num; i++)
      {
        if (addr[i] > max)
          return false;
        dds_os_put1BO (os, allocator, (uint8_t) addr[i]);
      }
      break;
    case 2:
      for (uint32_t i = 0; i < num; i++)
      {
        if (addr[i] > max)
          return false;
        dds_os_put2BO (os, allocator, (uint16_t) addr[i]);
      }
      break;
    case 4:
      for (uint32_t i = 0; i < num; i++)
      {
        if (addr[i] > max)
          return false;
        dds_os_put4BO (os, allocator, addr[i]);
      }
      break;
    default:
      abort ();
  }
  return true;
}

static bool dds_stream_write_bitmask_arrBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, uint32_t insn, const void *addr, uint32_t num, uint32_t bits_h, uint32_t bits_l)
{
  switch (DDS_OP_TYPE_SZ (insn))
  {
    case 1: {
      const uint8_t *ptr = (const uint8_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put1BO (os, allocator, ptr[i]);
      }
      break;
    }
    case 2: {
      const uint16_t *ptr = (const uint16_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put2BO (os, allocator, ptr[i]);
      }
      break;
    }
    case 4: {
      const uint32_t *ptr = (const uint32_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put4BO (os, allocator, ptr[i]);
      }
      break;
    }
    case 8: {
      const uint64_t *ptr = (const uint64_t *) addr;
      for (uint32_t i = 0; i < num; i++)
      {
        if (!bitmask_value_valid (ptr[i], bits_h, bits_l))
          return false;
        dds_os_put8BO (os, allocator, ptr[i]);
      }
      break;
    }
    default:
      abort ();
  }
  return true;
}

static const uint32_t *dds_stream_write_seqBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *addr, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
{
  const dds_sequence_t * const seq = (const dds_sequence_t *) addr;
  uint32_t offs = 0, xcdrv = os->x.m_xcdr_version;

  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t bound_op = seq_is_bounded (DDS_OP_TYPE (insn)) ? 1 : 0;
  uint32_t bound = bound_op ? ops[2] : 0;

  if (is_dheader_needed (subtype, xcdrv))
  {
    /* reserve space for DHEADER */
    dds_os_reserve4BO (os, allocator);
    offs = os->x.m_index;
  }

  const uint32_t num = seq->_length;
  if (bound && num > bound)
    return NULL;
  if (num > 0 && seq->_buffer == NULL)
    return NULL;

  dds_os_put4BO (os, allocator, num);

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
        if (!dds_stream_write_bool_arrBO (os, allocator, (const uint8_t *) seq->_buffer, num))
          return NULL;
        ops += 2 + bound_op;
        break;
      case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
        const uint32_t elem_size = get_primitive_size (subtype);
        const align_t cdr_align = dds_cdr_get_align (xcdrv, elem_size);
        void * dst;
        /* Combining put bytes and swap into a single step would improve the performance
           of writing data in non-native endianess. But in most cases the data will
           be written in native endianess, and in that case the swap is a no-op (for writing
           keys a separate function is used). */
        dds_os_put_bytes_aligned_base (&os->x, allocator, seq->_buffer, num, elem_size, cdr_align, &dst);
        dds_stream_to_BO_insitu (dst, elem_size, num);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_ENU:
        if (!dds_stream_write_enum_arrBO (os, allocator, insn, (const uint32_t *) seq->_buffer, num, ops[2 + bound_op]))
          return NULL;
        ops += 3 + bound_op;
        break;
      case DDS_OP_VAL_BMK: {
        if (!dds_stream_write_bitmask_arrBO (os, allocator, insn, seq->_buffer, num, ops[2 + bound_op], ops[3 + bound_op]))
          return NULL;
        ops += 4 + bound_op;
        break;
      }
      case DDS_OP_VAL_STR: {
        const char **ptr = (const char **) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_stringBO (os, allocator, ptr[i]);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_WSTR: {
        const wchar_t **ptr = (const wchar_t **) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_wstringBO (os, allocator, ptr[i]);
        ops += 2 + bound_op;
        break;
      }
      case DDS_OP_VAL_BST: {
        const char *ptr = (const char *) seq->_buffer;
        const uint32_t elem_size = ops[2 + bound_op];
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_stringBO (os, allocator, ptr + i * elem_size);
        ops += 3 + bound_op;
        break;
      }
      case DDS_OP_VAL_BWSTR: {
        const wchar_t *ptr = (const wchar_t *) seq->_buffer;
        const uint32_t elem_size = ops[2 + bound_op];
        for (uint32_t i = 0; i < num; i++)
          dds_stream_write_wstringBO (os, allocator, ptr + i * elem_size);
        ops += 3 + bound_op;
        break;
      }
      case DDS_OP_VAL_WCHAR: {
        const wchar_t *ptr = (const wchar_t *) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          if (!dds_stream_write_wcharBO (os, allocator, ptr[i]))
            return NULL;
        ops += 2 + bound_op;
        break;
      }
     case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
        const uint32_t elem_size = ops[2 + bound_op];
        const uint32_t jmp = DDS_OP_ADR_JMP (ops[3 + bound_op]);
        uint32_t const * const jsr_ops = ops + DDS_OP_ADR_JSR (ops[3 + bound_op]);
        const char *ptr = (const char *) seq->_buffer;
        for (uint32_t i = 0; i < num; i++)
          if (!dds_stream_write_implBO (os, allocator, mid_table, ptr + i * elem_size, jsr_ops, false, cdr_kind))
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
    *((uint32_t *) (os->x.m_buffer + offs - 4)) = to_BO4u(os->x.m_index - offs);

  return ops;
}

static const uint32_t *dds_stream_write_arrBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *addr, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
{
  const enum dds_stream_typecode subtype = DDS_OP_SUBTYPE (insn);
  uint32_t offs = 0, xcdrv = os->x.m_xcdr_version;
  if (is_dheader_needed (subtype, xcdrv))
  {
    /* reserve space for DHEADER */
    dds_os_reserve4BO (os, allocator);
    offs = os->x.m_index;
  }
  const uint32_t num = ops[2];
  switch (subtype)
  {
    case DDS_OP_VAL_BLN:
      if (!dds_stream_write_bool_arrBO (os, allocator, (const uint8_t *) addr, num))
        return NULL;
      ops += 3;
      break;
    case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
      const uint32_t elem_size = get_primitive_size (subtype);
      const align_t cdr_align = dds_cdr_get_align (xcdrv, elem_size);
      void * dst;
      /* See comment for stream_write_seq, swap is a no-op in most cases */
      dds_os_put_bytes_aligned_base (&os->x, allocator, addr, num, elem_size, cdr_align, &dst);
      dds_stream_to_BO_insitu (dst, elem_size, num);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_ENU:
      if (!dds_stream_write_enum_arrBO (os, allocator, insn, (const uint32_t *) addr, num, ops[3]))
        return NULL;
      ops += 4;
      break;
    case DDS_OP_VAL_BMK:
      if (!dds_stream_write_bitmask_arrBO (os, allocator, insn, addr, num, ops[3], ops[4]))
        return NULL;
      ops += 5;
      break;
    case DDS_OP_VAL_STR: {
      const char **ptr = (const char **) addr;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_stringBO (os, allocator, ptr[i]);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_WSTR: {
      const wchar_t **ptr = (const wchar_t **) addr;
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_wstringBO (os, allocator, ptr[i]);
      ops += 3;
      break;
    }
    case DDS_OP_VAL_BST: {
      const char *ptr = (const char *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_stringBO (os, allocator, ptr + i * elem_size);
      ops += 5;
      break;
    }
    case DDS_OP_VAL_BWSTR: {
      const wchar_t *ptr = (const wchar_t *) addr;
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        dds_stream_write_wstringBO (os, allocator, ptr + i * elem_size);
      ops += 5;
      break;
    }
    case DDS_OP_VAL_WCHAR: {
      const wchar_t *ptr = (const wchar_t *) addr;
      for (uint32_t i = 0; i < num; i++)
        if (!dds_stream_write_wcharBO (os, allocator, ptr[i]))
          return NULL;
      ops += 3;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      const uint32_t * jsr_ops = ops + DDS_OP_ADR_JSR (ops[3]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[3]);
      const uint32_t elem_size = ops[4];
      for (uint32_t i = 0; i < num; i++)
        if (!dds_stream_write_implBO (os, allocator, mid_table, addr + i * elem_size, jsr_ops, false, cdr_kind))
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
    *((uint32_t *) (os->x.m_buffer + offs - 4)) = to_BO4u(os->x.m_index - offs);

  return ops;
}

static bool dds_stream_write_union_discriminantBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, uint32_t insn, const void *addr, uint32_t *disc)
{
  assert (disc);
  enum dds_stream_typecode type = DDS_OP_SUBTYPE (insn);
  assert (type == DDS_OP_VAL_BLN || type == DDS_OP_VAL_1BY || type == DDS_OP_VAL_2BY || type == DDS_OP_VAL_4BY || type == DDS_OP_VAL_ENU);
  switch (type)
  {
    case DDS_OP_VAL_BLN:
      *disc = *((const uint8_t *) addr) != 0;
      if (!dds_stream_write_bool_valueBO (os, allocator, (uint8_t) *disc))
        return false;
      break;
    case DDS_OP_VAL_1BY:
      *disc = *((const uint8_t *) addr);
      dds_os_put1BO (os, allocator, (uint8_t) *disc);
      break;
    case DDS_OP_VAL_2BY:
      *disc = *((const uint16_t *) addr);
      dds_os_put2BO (os, allocator, (uint16_t) *disc);
      break;
    case DDS_OP_VAL_4BY:
      *disc = *((const uint32_t *) addr);
      dds_os_put4BO (os, allocator, *disc);
      break;
    case DDS_OP_VAL_ENU:
      *disc = *((const uint32_t *) addr);
      if (!dds_stream_write_enum_valueBO (os, allocator, insn, *disc, ops[4]))
        return false;
      break;
    default:
      abort ();
  }
  return true;
}

static const uint32_t *dds_stream_write_uniBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *discaddr, const char *baseaddr, const uint32_t *ops, uint32_t insn, enum cdr_data_kind cdr_kind)
{
  uint32_t disc;
  if (!dds_stream_write_union_discriminantBO (os, allocator, ops, insn, discaddr, &disc))
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
    if (op_type_external (jeq_op[0]) && valtype != DDS_OP_VAL_STR && valtype != DDS_OP_VAL_WSTR)
    {
      assert (DDS_OP (jeq_op[0]) == DDS_OP_JEQ4);
      valaddr = *(char **) valaddr;
      if (valaddr == NULL)
        return NULL;
    }

    switch (valtype)
    {
      case DDS_OP_VAL_BLN:
        if (!dds_stream_write_bool_valueBO (os, allocator, *(const uint8_t *) valaddr))
          return NULL;
        break;
      case DDS_OP_VAL_1BY: dds_os_put1BO (os, allocator, *(const uint8_t *) valaddr); break;
      case DDS_OP_VAL_2BY: dds_os_put2BO (os, allocator, *(const uint16_t *) valaddr); break;
      case DDS_OP_VAL_4BY: dds_os_put4BO (os, allocator, *(const uint32_t *) valaddr); break;
      case DDS_OP_VAL_8BY: dds_os_put8BO (os, allocator, *(const uint64_t *) valaddr); break;
      case DDS_OP_VAL_ENU:
        if (!dds_stream_write_enum_valueBO (os, allocator, jeq_op[0], *((const uint32_t *) valaddr), jeq_op[3]))
          return NULL;
        break;
      case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, allocator, *(const char **) valaddr); break;
      case DDS_OP_VAL_WSTR: dds_stream_write_wstringBO (os, allocator, *(const wchar_t **) valaddr); break;
      case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, allocator, (const char *) valaddr); break;
      case DDS_OP_VAL_BWSTR: dds_stream_write_wstringBO (os, allocator, (const wchar_t *) valaddr); break;
      case DDS_OP_VAL_WCHAR:
        if (!dds_stream_write_wcharBO (os, allocator, *((const wchar_t *) valaddr)))
          return NULL;
        break;
      case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_ARR: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: case DDS_OP_VAL_BMK:
        if (!dds_stream_write_implBO (os, allocator, mid_table, valaddr, jeq_op + DDS_OP_ADR_JSR (jeq_op[0]), false, cdr_kind))
          return NULL;
        break;
      case DDS_OP_VAL_EXT:
        abort (); /* op type EXT as union subtype not supported */
        break;
    }
  }
  return ops;
}

static void dds_stream_write_xcdr1_paramheaderBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, bool flag_mu, uint32_t member_id, uint32_t *param_length_offset, bool *alignment_offset_by_4)
{
  // Always using long PL encoding
  uint16_t phdr = DDS_XCDR1_PL_SHORT_FLAG_MU | DDS_XCDR1_PL_SHORT_PID_EXTENDED; // support for FLAG_IMPL_EXT not implemented
  uint16_t slen = DDS_XCDR1_PL_SHORT_PID_EXT_LEN;
  uint32_t pid = (flag_mu ? DDS_XCDR1_PL_LONG_FLAG_MU : 0) | (member_id & DDS_XCDR1_PL_LONG_MID_MASK);

  dds_cdr_alignto_clear_and_resize_base (&os->x, allocator, dds_cdr_get_align (os->x.m_xcdr_version, 4), 4);
  dds_os_put2BO (os, allocator, phdr);
  dds_os_put2BO (os, allocator, slen);
  dds_os_put4BO (os, allocator, pid);
  *param_length_offset = dds_os_reserve4BO (os, allocator);
  // Ghastly XCDR1 alignment rules for mutable encoding and the encoding of optionals in non-mutable structs
  // require us to pretend that the value starts at an offset 0 mod 8. The caller is expected to undo the
  // change to os->x.m_align_off
  if ((os->x.m_index % 8) == 0)
    *alignment_offset_by_4 = false;
  else
  {
    assert ((os->x.m_index % 4) == 0);
    assert (os->x.m_index >= 4);
    os->x.m_align_off += 4;
    *alignment_offset_by_4 = true;
  }
}

static const uint32_t *dds_stream_write_adrBO (uint32_t insn, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, bool is_mutable_member, enum cdr_data_kind cdr_kind)
{
  // When writing key CDR, don't require an external member to be malloc'ed
  // and initialized (see also comment in dds_stream_read_adr)
  const bool is_key = (insn & DDS_OP_FLAG_KEY);
  if (cdr_kind == CDR_KIND_KEY && !is_key)
    return dds_stream_skip_adr (insn, ops);

  const void *addr = data + ops[1];
  if (op_type_external (insn) || op_type_optional (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR)
  {
    addr = *(char **) addr;
    if (addr == NULL && !(op_type_optional (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR))
      return NULL;
  }

  uint32_t param_length_offs = 0;
  bool alignment_offset_by_4 = false; // for XCDR1 alignment rules
  if (op_type_optional (insn) && !is_mutable_member)
  {
    bool present = (addr != NULL);
    if (os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1)
    {
      uint32_t flags = DDS_OP_FLAGS (insn);
      bool must_understand = flags & (DDS_OP_FLAG_MU | DDS_OP_FLAG_KEY);
      uint32_t member_id;
      if (!find_member_id (mid_table, ops, &member_id))
        return NULL;
      dds_stream_write_xcdr1_paramheaderBO (os, allocator, must_understand, member_id, &param_length_offs, &alignment_offset_by_4);
      if (!present)
      {
        *((uint32_t *) (os->x.m_buffer + param_length_offs - 4)) = to_BO4u (0);
        if (alignment_offset_by_4)
        {
          assert (os->x.m_align_off >= 4);
          os->x.m_align_off -= 4;
        }
      }
    }
    else // DDSI_RTPS_CDR_ENC_VERSION_2
    {
      dds_os_put1BO (os, allocator, present ? 1 : 0);
    }
    if (!present)
      return dds_stream_skip_adr (insn, ops);
  }
  assert (addr || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
      if (!dds_stream_write_bool_valueBO (os, allocator, *((const uint8_t *) addr)))
        return NULL;
      ops += 2;
      break;
    case DDS_OP_VAL_1BY: dds_os_put1BO (os, allocator, *((const uint8_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_2BY: dds_os_put2BO (os, allocator, *((const uint16_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_4BY: dds_os_put4BO (os, allocator, *((const uint32_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_8BY: dds_os_put8BO (os, allocator, *((const uint64_t *) addr)); ops += 2; break;
    case DDS_OP_VAL_ENU:
      if (!dds_stream_write_enum_valueBO (os, allocator, insn, *((const uint32_t *) addr), ops[2]))
        return NULL;
      ops += 3;
      break;
    case DDS_OP_VAL_BMK:
      if (!dds_stream_write_bitmask_valueBO (os, allocator, insn, addr, ops[2], ops[3]))
        return NULL;
      ops += 4;
      break;
    case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, allocator, (const char *) addr); ops += 2; break;
    case DDS_OP_VAL_WSTR: dds_stream_write_wstringBO (os, allocator, (const wchar_t *) addr); ops += 2; break;
    case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, allocator, (const char *) addr); ops += 3; break;
    case DDS_OP_VAL_BWSTR: dds_stream_write_wstringBO (os, allocator, (const wchar_t *) addr); ops += 3; break;
    case DDS_OP_VAL_WCHAR:
      if (!dds_stream_write_wcharBO (os, allocator, *((const wchar_t *) addr)))
        return NULL;
      ops += 2;
      break;
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: ops = dds_stream_write_seqBO (os, allocator, mid_table, addr, ops, insn, cdr_kind); break;
    case DDS_OP_VAL_ARR: ops = dds_stream_write_arrBO (os, allocator, mid_table, addr, ops, insn, cdr_kind); break;
    case DDS_OP_VAL_UNI: ops = dds_stream_write_uniBO (os, allocator, mid_table, addr, data, ops, insn, cdr_kind); break;
    case DDS_OP_VAL_EXT: {
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
      const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

      /* skip DLC instruction for base type, so that the DHEADER is not
          serialized for base types */
      if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
        jsr_ops++;

      /* don't forward is_mutable_member, subtype can have other extensibility */
      if (!dds_stream_write_implBO (os, allocator, mid_table, addr, jsr_ops, false, cdr_kind))
        return NULL;
      ops += jmp ? jmp : 3;
      break;
    }
    case DDS_OP_VAL_STU: abort (); break; /* op type STU only supported as subtype */
  }

  // In case XCDR1, optional member of a non-mutable type: set the parameter length in the (extended) parameter header
  if (os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_1 && op_type_optional (insn) && !is_mutable_member)
  {
    *((uint32_t *) (os->x.m_buffer + param_length_offs - 4)) = to_BO4u (os->x.m_index - param_length_offs);
    if (alignment_offset_by_4)
    {
      assert (os->x.m_align_off >= 4);
      os->x.m_align_off -= 4;
    }
  }
  return ops;
}

static const uint32_t *dds_stream_write_delimitedBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, enum cdr_data_kind cdr_kind)
{
  uint32_t offs = dds_os_reserve4BO (os, allocator);
  if (!(ops = dds_stream_write_implBO (os, allocator, mid_table, data, ops + 1, false, cdr_kind)))
    return NULL;

  /* add dheader, which is the serialized size of the data */
  *((uint32_t *) (os->x.m_buffer + offs - 4)) = to_BO4u (os->x.m_index - offs);
  return ops;
}

static bool dds_stream_write_xcdr2_pl_memberBO (uint32_t mid, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, enum cdr_data_kind cdr_kind)
{
  assert (!(mid & ~EMHEADER_MEMBERID_MASK));

  /* get flags from first member op */
  uint32_t flags = DDS_OP_FLAGS (ops[0]);
  bool is_key = flags & (DDS_OP_FLAG_MU | DDS_OP_FLAG_KEY);
  bool must_understand = flags & (DDS_OP_FLAG_MU | DDS_OP_FLAG_KEY);

  if (cdr_kind == CDR_KIND_KEY && !is_key)
    return true;

  uint32_t lc = get_length_code (ops);
  assert (lc <= LENGTH_CODE_ALSO_NEXTINT8);
  uint32_t data_offs = (lc != LENGTH_CODE_NEXTINT) ? dds_os_reserve4BO (os, allocator) : dds_os_reserve8BO (os, allocator);
  if (!(dds_stream_write_implBO (os, allocator, mid_table, data, ops, true, cdr_kind)))
    return false;

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

static const uint32_t *dds_stream_write_xcdr2_pl_memberlistBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, enum cdr_data_kind cdr_kind)
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
          if (!dds_stream_write_xcdr2_pl_memberlistBO (os, allocator, mid_table, data, plm_ops, cdr_kind))
            return NULL;
        }
        else if (is_member_present (data, plm_ops))
        {
          uint32_t member_id = ops[1];
          if (!dds_stream_write_xcdr2_pl_memberBO (member_id, os, allocator, mid_table, data, plm_ops, cdr_kind))
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

static const uint32_t *dds_stream_write_xcdr2_plBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops, enum cdr_data_kind cdr_kind)
{
  /* skip PLC op */
  ops++;

  /* alloc space for dheader */
  dds_os_reserve4BO (os, allocator);
  uint32_t data_offs = os->x.m_index;

  /* write members, including members from base types */
  ops = dds_stream_write_xcdr2_pl_memberlistBO (os, allocator, mid_table, data, ops, cdr_kind);

  /* write serialized size in dheader */
  *((uint32_t *) (os->x.m_buffer + data_offs - 4)) = to_BO4u (os->x.m_index - data_offs);
  return ops;
}

static const uint32_t *dds_stream_write_implBO (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops0, bool is_mutable_member, enum cdr_data_kind cdr_kind)
{
  const uint32_t *ops = ops0; // clang warns about testing a non-null pointer otherwise
  uint32_t insn;
  while (ops && (insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_write_adrBO (insn, os, allocator, mid_table, data, ops, is_mutable_member, cdr_kind);
        break;
      case DDS_OP_JSR:
        if (!dds_stream_write_implBO (os, allocator, mid_table, data, ops + DDS_OP_JUMP (insn), is_mutable_member, cdr_kind))
          return NULL;
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        if (os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2)
          ops = dds_stream_write_delimitedBO (os, allocator, mid_table, data, ops, cdr_kind);
        else
          ops = dds_stream_write_implBO (os, allocator, mid_table, data, ops + 1, false, cdr_kind);
        break;
      case DDS_OP_PLC:
        assert (os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
        ops = dds_stream_write_xcdr2_plBO (os, allocator, mid_table, data, ops, cdr_kind);
        break;
    }
  }
  return ops;
}

const uint32_t *dds_stream_write_with_midBO (DDS_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc_mid_table *mid_table, const char *data, const uint32_t *ops)
{
  const struct dds_cdrstream_desc_mid_table empty_mid_table = { .table = (struct ddsrt_hh *) &ddsrt_hh_empty, .op0 = ops };
  RESTRICT_OSTREAM_T ros;
  memcpy (&ros, os, sizeof (*os));
  ros.x.m_align_off = 0;
  const uint32_t *ret = dds_stream_write_implBO (&ros, allocator, mid_table ? mid_table : &empty_mid_table, data, ops, false, CDR_KIND_DATA);
  memcpy (os, &ros, sizeof (*os));
  return ret;
}

const uint32_t *dds_stream_writeBO (DDS_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const char *data, const uint32_t *ops)
{
  return dds_stream_write_with_midBO (os, allocator, NULL, data, ops);
}
