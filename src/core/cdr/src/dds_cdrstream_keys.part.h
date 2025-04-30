// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

ddsrt_attribute_warn_unused_result
static bool dds_stream_write_keyBO_impl (RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint32_t *ops, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  uint32_t insn = *ops;
  assert (DDS_OP (insn) == DDS_OP_ADR);
  void *addr = (char *) src + ops[1];

  if (op_type_external (insn) || DDS_OP_TYPE (insn) == DDS_OP_VAL_STR || DDS_OP_TYPE (insn) == DDS_OP_VAL_WSTR)
  {
    addr = *(char **) addr;
    if (addr == NULL && DDS_OP_TYPE (insn) != DDS_OP_VAL_STR && DDS_OP_TYPE (insn) != DDS_OP_VAL_WSTR)
      return false;
  }

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN: dds_os_put1BO (os, allocator, *((uint8_t *) addr) != 0); break;
    case DDS_OP_VAL_1BY: dds_os_put1BO (os, allocator, *((uint8_t *) addr)); break;
    case DDS_OP_VAL_2BY: dds_os_put2BO (os, allocator, *((uint16_t *) addr)); break;
    case DDS_OP_VAL_4BY: dds_os_put4BO (os, allocator, *((uint32_t *) addr)); break;
    case DDS_OP_VAL_8BY: dds_os_put8BO (os, allocator, *((uint64_t *) addr)); break;
    case DDS_OP_VAL_ENU:
      if (!dds_stream_write_enum_valueBO (os, allocator, insn, *((uint32_t *) addr), ops[2]))
        return false;
      break;
    case DDS_OP_VAL_BMK:
      if (!dds_stream_write_bitmask_valueBO (os, allocator, insn, addr, ops[2], ops[3]))
        return false;
      break;
    case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, allocator, addr); break;
    case DDS_OP_VAL_WSTR: dds_stream_write_wstringBO (os, allocator, (const wchar_t *) addr); break;
    case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, allocator, addr); break;
    case DDS_OP_VAL_BWSTR: dds_stream_write_wstringBO (os, allocator, (const wchar_t *) addr); break;
    case DDS_OP_VAL_WCHAR:
      if (!dds_stream_write_wcharBO (os, allocator, *(wchar_t *) addr))
        return false;
      break;
    case DDS_OP_VAL_ARR:
      if (!dds_stream_write_arrBO (os, allocator, addr, ops, insn, CDR_KIND_KEY))
        return false;
      break;
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      if (!dds_stream_write_keyBO_impl (os, allocator, jsr_ops, addr, --key_offset_count, ++key_offset_insn))
        return false;
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: {
      if (!dds_stream_write_seqBO (os, allocator, addr, ops, insn, CDR_KIND_KEY))
        return false;
      break;
    }
    case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      // FIXME: implement support for unions as part of the key
      abort ();
      break;
    }
  }
  return true;
}

static bool dds_stream_write_keyBO_restrict (RESTRICT_OSTREAM_T *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const char *sample, const struct dds_cdrstream_desc *desc)
{
  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM) && ser_kind == DDS_CDR_KEY_SERIALIZATION_SAMPLE)
  {
    /* For types with key fields in aggregated types with appendable or mutable
       extensibility, write the key CDR using the regular write functions */
    if (dds_stream_write_implBO (os, allocator, sample, desc->ops.ops, false, CDR_KIND_KEY) == NULL)
      return false;
  }
  else
  {
    /* Optimized implementation to write key in case all key members are in an aggregated
       type with final extensibility: iterate over keys in key descriptor. Depending on the output
       kind (for a key-only sample or keyhash), use the specific key-list from the descriptor. */

    /* FIXME: a known issue in this implementation is that in case of key-hash CDR output, for key
       members that are of a collection type (array/seq), and the element type is an aggregated type,
       the members are not ordered by their member ID, but included in definition order. As a result,
       the CDR used for calculating the key-hash may be incorrect. */
    bool use_memberid_order = (ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH && os->x.m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
    struct dds_cdrstream_desc_key *keylist = use_memberid_order ? desc->keys.keys : desc->keys.keys_definition_order;
    for (uint32_t i = 0; i < desc->keys.nkeys; i++)
    {
      const uint32_t *insnp = desc->ops.ops + keylist[i].ops_offs;
      switch (DDS_OP (*insnp))
      {
        case DDS_OP_KOF: {
          uint16_t n_offs = DDS_OP_LENGTH (*insnp);
          assert (n_offs > 0);
          if (!dds_stream_write_keyBO_impl (os, allocator, desc->ops.ops + insnp[1], sample, --n_offs, insnp + 2))
            return false;
          break;
        }
        case DDS_OP_ADR: {
          if (!dds_stream_write_keyBO_impl (os, allocator, insnp, sample, 0, NULL))
            return false;
          break;
        }
        default:
          abort ();
          break;
      }
    }
  }
  return true;
}

bool dds_stream_write_keyBO (DDS_OSTREAM_T *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const char *sample, const struct dds_cdrstream_desc *desc)
{
  return dds_stream_write_keyBO_restrict ((RESTRICT_OSTREAM_T *) os, ser_kind, allocator, sample, desc);
}

static const uint32_t *dds_stream_extract_keyBO_from_data_adr (uint32_t insn, dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint32_t * const op0, const uint32_t *ops, bool mutable_member, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * restrict keys_remaining)
{
  assert (insn == *ops);
  assert (DDS_OP (insn) == DDS_OP_ADR);
  const enum dds_stream_typecode type = DDS_OP_TYPE (insn);
  const bool is_key = (insn & DDS_OP_FLAG_KEY) && (os != NULL);
  if (!stream_is_member_present (insn, is, mutable_member))
  {
    assert (!is_key);
    return dds_stream_skip_adr (insn, ops);
  }

  if (type == DDS_OP_VAL_EXT)
  {
    const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
    const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);

    /* skip DLC instruction for base type, handle as if it is final because the base type's
        members follow the derived types members without an extra DHEADER */
    if (op_type_base (insn) && jsr_ops[0] == DDS_OP_DLC)
      jsr_ops++;

    /* only in case the ADR|EXT has the key flag set, pass the actual ostream, otherwise skip the EXT type by passing NULL for ostream */
    (void) dds_stream_extract_keyBO_from_data1 (is, is_key ? os : NULL, allocator, op0, jsr_ops, false, mutable_member_or_parent, n_keys, keys_remaining);
    ops += jmp ? jmp : 3;
  }
  else
  {
    if (is_key)
    {
      assert (*keys_remaining > 0);
      assert (os != NULL);
      dds_stream_extract_keyBO_from_key_prim_op (is, os, allocator, ops, 0, NULL);
      ops = dds_stream_skip_adr (insn, ops);
      (*keys_remaining)--;
    }
    else
      ops = dds_stream_extract_key_from_data_skip_adr (is, ops, type);
  }
  return ops;
}

static const uint32_t *dds_stream_extract_keyBO_from_data_delimited (dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint32_t * const op0, const uint32_t *ops, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * restrict keys_remaining)
{
  uint32_t delimited_sz_is = dds_is_get4 (is), delimited_offs_is = is->m_index, insn;

  uint32_t delimited_offs_os = 0;
  if (os != NULL)
  {
    /* At this point we can safely assume that at least one of the members
      of this aggregated type is part of the key, so we need to add the dheader */
    delimited_offs_os = dds_os_reserve4BO (os, allocator);
  }

  ops++;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        /* skip fields that are not in serialized data for appendable type */
        ops = (is->m_index - delimited_offs_is < delimited_sz_is) ?
          dds_stream_extract_keyBO_from_data_adr (insn, is, os, allocator, op0, ops, false, mutable_member_or_parent, n_keys, keys_remaining) : dds_stream_skip_adr (insn, ops);
        break;
      case DDS_OP_JSR:
        (void) dds_stream_extract_keyBO_from_data1 (is, os, allocator, op0, ops + DDS_OP_JUMP (insn), false, mutable_member_or_parent, n_keys, keys_remaining);
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
    }
  }

  /* Skip remainder of serialized data for this appendable type */
  if (delimited_sz_is > is->m_index - delimited_offs_is)
    is->m_index += delimited_sz_is - (is->m_index - delimited_offs_is);

  /* if not in skip mode: add dheader in os */
  if (os != NULL)
  {
    assert (delimited_sz_is == is->m_index - delimited_offs_is);
    *((uint32_t *) (os->x.m_buffer + delimited_offs_os - 4)) = to_BO4u (os->x.m_index - delimited_offs_os);
  }

  return ops;
}

static bool dds_stream_extract_keyBO_from_data_pl_member (dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, uint32_t m_id, const uint32_t * const op0, const uint32_t *ops, uint32_t n_keys, uint32_t * restrict keys_remaining)
{
  uint32_t insn, ops_csr = 0;
  bool found = false;

  while (*keys_remaining > 0 && !found && (insn = ops[ops_csr]) != DDS_OP_RTS)
  {
    assert (DDS_OP (insn) == DDS_OP_PLM);
    uint32_t flags = DDS_PLM_FLAGS (insn);
    const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_PLM (insn);
    if (flags & DDS_OP_FLAG_BASE)
    {
      assert (DDS_OP (plm_ops[0]) == DDS_OP_PLC);
      plm_ops++; /* skip PLC to go to first PLM from base type */
      found = dds_stream_extract_keyBO_from_data_pl_member (is, os, allocator, m_id, op0, plm_ops, n_keys, keys_remaining);
    }
    else if (ops[ops_csr + 1] == m_id)
    {
      uint32_t lc = get_length_code (plm_ops);
      assert (lc <= LENGTH_CODE_ALSO_NEXTINT8);
      uint32_t data_offs = (lc != LENGTH_CODE_NEXTINT) ? dds_os_reserve4BO (os, allocator) : dds_os_reserve8BO (os, allocator);

      (void) dds_stream_extract_keyBO_from_data1 (is, os, allocator, op0, plm_ops, true, true, n_keys, keys_remaining);

      /* add emheader with data length code and flags and optionally the serialized size of the data */
      uint32_t em_hdr = 0;
      em_hdr |= EMHEADER_FLAG_MUSTUNDERSTAND;
      em_hdr |= lc << 28;
      em_hdr |= m_id & EMHEADER_MEMBERID_MASK;

      uint32_t *em_hdr_ptr = (uint32_t *) (os->x.m_buffer + data_offs - (lc == LENGTH_CODE_NEXTINT ? 8 : 4));
      em_hdr_ptr[0] = to_BO4u (em_hdr);
      if (lc == LENGTH_CODE_NEXTINT)
        em_hdr_ptr[1] = to_BO4u (os->x.m_index - data_offs);  /* member size in next_int field in emheader */

      found = true;
      break;
    }
    ops_csr += 2;
  }
  return found;
}

static const uint32_t *dds_stream_extract_keyBO_from_data_pl (dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint32_t * const op0, const uint32_t *ops, uint32_t n_keys, uint32_t * restrict keys_remaining)
{
  /* skip PLC op */
  ops++;

  /* read DHEADER */
  uint32_t pl_sz = dds_is_get4 (is), pl_offs = is->m_index;

  /* At least one of the members of this aggregated type is part of the key,
     so we need to add the dheader for this mutable type */
  uint32_t delimited_offs_os = 0;
  if (os != NULL)
    dds_os_reserve4BO (os, allocator);

  while (is->m_index - pl_offs < pl_sz)
  {
    /* read EMHEADER and next_int */
    uint32_t em_hdr = dds_is_get4 (is);
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), m_id = EMHEADER_MEMBERID (em_hdr), msz;
    switch (lc)
    {
      case LENGTH_CODE_1B: case LENGTH_CODE_2B: case LENGTH_CODE_4B: case LENGTH_CODE_8B:
        msz = 1u << lc;
        break;
      case LENGTH_CODE_NEXTINT:
        /* read NEXTINT */
        msz = dds_is_get4 (is);
        break;
      case LENGTH_CODE_ALSO_NEXTINT: case LENGTH_CODE_ALSO_NEXTINT4: case LENGTH_CODE_ALSO_NEXTINT8:
        /* length is part of serialized data */
        msz = dds_is_peek4 (is);
        if (lc > LENGTH_CODE_ALSO_NEXTINT)
          msz <<= (lc - 4);
        break;
      default:
        abort ();
        break;
    }

    /* If in skip-mode, member not found or in case no more keys remaining to be found, skip the member
       in the input stream */
    if (os == NULL || !dds_stream_extract_keyBO_from_data_pl_member (is, os, allocator, m_id, op0, ops, n_keys, keys_remaining))
    {
      is->m_index += msz;
      if (lc >= LENGTH_CODE_ALSO_NEXTINT)
        is->m_index += 4; /* length embedded in member does not include it's own 4 bytes */
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  /* add dheader in os */
  if (os != NULL)
    *((uint32_t *) (os->x.m_buffer + delimited_offs_os - 4)) = to_BO4u (os->x.m_index - delimited_offs_os);

  return ops;
}

static const uint32_t *dds_stream_extract_keyBO_from_data1 (dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const uint32_t * const op0, const uint32_t *ops, bool mutable_member, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * restrict keys_remaining)
{
  uint32_t insn;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR:
        ops = dds_stream_extract_keyBO_from_data_adr (insn, is, os, allocator, op0, ops, mutable_member, mutable_member_or_parent, n_keys, keys_remaining);
        break;
      case DDS_OP_JSR:
        (void) dds_stream_extract_keyBO_from_data1 (is, os, allocator, op0, ops + DDS_OP_JUMP (insn), mutable_member, mutable_member_or_parent, n_keys, keys_remaining);
        ops++;
        break;
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: case DDS_OP_MID:
        abort ();
        break;
      case DDS_OP_DLC:
        ops = dds_stream_extract_keyBO_from_data_delimited (is, os, allocator, op0, ops, mutable_member_or_parent, n_keys, keys_remaining);
        break;
      case DDS_OP_PLC:
        ops = dds_stream_extract_keyBO_from_data_pl (is, os, allocator, op0, ops, n_keys, keys_remaining);
        break;
    }
  }
  return ops;
}

static bool dds_stream_extract_keyBO_from_data_restrict (dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  bool ret = true;
  uint32_t keys_remaining = desc->keys.nkeys;
  if (keys_remaining == 0)
    return ret;

  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM))
  {
    /* In case the type or any subtype has non-final extensibility, read the sample
       and write the key-only CDR for this sample */
    void *sample = allocator->malloc (desc->size);
    memset (sample, 0, desc->size);
    (void) dds_stream_read (is, sample, allocator, desc->ops.ops);
    if (!dds_stream_write_keyBO_restrict (os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, allocator, sample, desc))
    {
      // can't happen given normalized input (and it has to be normalized, else dds_stream_read may not be used)
      abort ();
    }
    dds_stream_free_sample (sample, allocator, desc->ops.ops);
    allocator->free (sample);
  }
  else
  {
    /* optimized solution for keys in type with final extensibility */
    uint32_t *op0 = desc->ops.ops;
    (void) dds_stream_extract_keyBO_from_data1 (is, os, allocator, op0, desc->ops.ops, false, false, desc->keys.nkeys, &keys_remaining);

    /* FIXME: stream_normalize should check for missing keys by implementing the
        must_understand annotation, so the check keys_remaining > 0 can become an assert. */
    ret = (keys_remaining == 0);
  }
  return ret;
}

bool dds_stream_extract_keyBO_from_data (dds_istream_t *is, DDS_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  return dds_stream_extract_keyBO_from_data_restrict (is, (RESTRICT_OSTREAM_T *) os, allocator, desc);
}

static void dds_stream_extract_keyBO_from_key_impl (dds_istream_t *is, RESTRICT_OSTREAM_T *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  /* The type or any subtype has non-final extensibility, so read a key sample
     and write the key-only CDR for this sample */
  void *sample = allocator->malloc (desc->size);
  memset (sample, 0, desc->size);
  (void) dds_stream_read_impl (is, sample, allocator, desc->ops.ops, false, CDR_KIND_KEY, SAMPLE_DATA_INITIALIZED);
  if (!dds_stream_write_keyBO_restrict (os, ser_kind, allocator, sample, desc))
  {
    // input must have been proven correct (using normalize), so write can't run into invalid data
    abort ();
  }
  dds_stream_free_sample (sample, allocator, desc->ops.ops);
  allocator->free (sample);
}

static void dds_stream_extract_keyBO_from_key_optimized (dds_istream_t *is, RESTRICT_OSTREAM_T *os, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    uint32_t const * const op = desc->ops.ops + desc->keys.keys_definition_order[i].ops_offs;
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        assert (n_offs > 0);
        dds_stream_extract_keyBO_from_key_prim_op (is, os, allocator, desc->ops.ops + op[1], --n_offs, op + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_extract_keyBO_from_key_prim_op (is, os, allocator, op, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}

/* This function is used to create a serialized key in order to create a keyhash (big-endian) and to translate XCDR1 key CDR into XCDR2
   representation (native endianess). The former is not used regularly by Cyclone, and the latter is only used when receiving a key sample,
   e.g. a dispose. For this reason, we use a (performance wise) sub-optimal approach of going through the entire CDR for every key field.
   Optimizations is possible but would result in more complex code. */
void dds_stream_extract_keyBO_from_key (dds_istream_t *is, DDS_OSTREAM_T *os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator *allocator, const struct dds_cdrstream_desc *desc)
{
  assert (ser_kind == DDS_CDR_KEY_SERIALIZATION_SAMPLE || ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH);

  /* This assumes that the key fields in the input CDR are in definition order.
     In case any key field is in an appendable or mutable type, or in case a serialized
     key for a keyhash is required (in member-id order), extract and write the key
     in two steps. Otherwise, extract the output CDR in a single step. */
  if ((desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE | DDS_TOPIC_KEY_SEQUENCE | DDS_TOPIC_KEY_ARRAY_NONPRIM)) || ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH)
    dds_stream_extract_keyBO_from_key_impl (is, (RESTRICT_OSTREAM_T *) os, ser_kind, allocator, desc);
  else
    dds_stream_extract_keyBO_from_key_optimized (is, (RESTRICT_OSTREAM_T *) os, allocator, desc);
}
