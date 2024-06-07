// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

static void dds_stream_write_keyBO_impl (DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t *ops, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn);
static void dds_stream_write_keyBO_impl (DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const uint32_t *ops, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  uint32_t insn = *ops;
  assert (DDS_OP (insn) == DDS_OP_ADR);
  assert (insn_key_ok_p (insn));
  void *addr = (char *) src + ops[1];

  if (op_type_external (insn))
    addr = *((char **) addr);

  switch (DDS_OP_TYPE (insn))
  {
    case DDS_OP_VAL_BLN:
    case DDS_OP_VAL_1BY: dds_os_put1BO (os, allocator, *((uint8_t *) addr)); break;
    case DDS_OP_VAL_2BY: dds_os_put2BO (os, allocator, *((uint16_t *) addr)); break;
    case DDS_OP_VAL_4BY: dds_os_put4BO (os, allocator, *((uint32_t *) addr)); break;
    case DDS_OP_VAL_8BY: dds_os_put8BO (os, allocator, *((uint64_t *) addr)); break;
    case DDS_OP_VAL_ENU:
      (void) dds_stream_write_enum_valueBO (os, allocator, insn, *((uint32_t *) addr), ops[2]);
      break;
    case DDS_OP_VAL_BMK:
      (void) dds_stream_write_bitmask_valueBO (os, allocator, insn, addr, ops[2], ops[3]);
      break;
    case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, allocator, *(char **) addr); break;
    case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, allocator, addr); break;
    case DDS_OP_VAL_ARR: {
      const uint32_t num = ops[2];
      switch (DDS_OP_SUBTYPE (insn))
      {
        case DDS_OP_VAL_BLN: case DDS_OP_VAL_1BY: case DDS_OP_VAL_2BY: case DDS_OP_VAL_4BY: case DDS_OP_VAL_8BY: {
          const uint32_t elem_size = get_primitive_size (DDS_OP_SUBTYPE (insn));
          const align_t cdr_align = dds_cdr_get_align (((struct dds_ostream *)os)->m_xcdr_version, elem_size);
          dds_cdr_alignto_clear_and_resizeBO (os, allocator, cdr_align, num * elem_size);
          void * const dst = ((struct dds_ostream *)os)->m_buffer + ((struct dds_ostream *)os)->m_index;
          dds_os_put_bytes ((struct dds_ostream *)os, allocator, addr, num * elem_size);
          dds_stream_swap_if_needed_insituBO (dst, elem_size, num);
          break;
        }
        case DDS_OP_VAL_ENU: case DDS_OP_VAL_BMK: {
          uint32_t offs = 0, xcdrv = ((struct dds_ostream *)os)->m_xcdr_version;
          if (xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2)
          {
            /* reserve space for DHEADER */
            dds_os_reserve4BO (os, allocator);
            offs = ((struct dds_ostream *)os)->m_index;
          }
          if (DDS_OP_SUBTYPE (insn) == DDS_OP_VAL_ENU)
            (void) dds_stream_write_enum_arrBO (os, allocator, insn, (const uint32_t *) addr, num, ops[3]);
          else
            (void) dds_stream_write_bitmask_arrBO (os, allocator, insn, (const uint32_t *) addr, num, ops[3], ops[4]);
          /* write DHEADER */
          if (xcdrv == DDSI_RTPS_CDR_ENC_VERSION_2)
            *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + offs - 4)) = to_BO4u(((struct dds_ostream *)os)->m_index - offs);
          break;
        }
        default:
          abort ();
      }
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]) + *key_offset_insn;
      dds_stream_write_keyBO_impl (os, allocator, jsr_ops, addr, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_BSQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      // FIXME: implement support for sequences and unions as part of the key
      abort ();
      break;
    }
  }
}

void dds_stream_write_keyBO (DDS_OSTREAM_T * __restrict os, enum dds_cdr_key_serialization_kind ser_kind, const struct dds_cdrstream_allocator * __restrict allocator, const char * __restrict sample, const struct dds_cdrstream_desc * __restrict desc)
{
  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE) && ser_kind == DDS_CDR_KEY_SERIALIZATION_SAMPLE)
  {
    /* For types with key fields in aggregated types with appendable or mutable
       extensibility, write the key CDR using the regular write functions */
    (void) dds_stream_write_implBO (os, allocator, sample, desc->ops.ops, false, CDR_KIND_KEY);
  }
  else
  {
    /* Optimized implementation to write key in case all key members are in an aggregated
       type with final extensibility: iterate over keys in key descriptor. Depending on the output
       kind (for a key-only sample or keyhash), use the specific key-list from the descriptor. */
    bool use_memberid_order = (ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH && ((struct dds_ostream *) os)->m_xcdr_version == DDSI_RTPS_CDR_ENC_VERSION_2);
    struct dds_cdrstream_desc_key *keylist = use_memberid_order ? desc->keys.keys : desc->keys.keys_definition_order;
    for (uint32_t i = 0; i < desc->keys.nkeys; i++)
    {
      const uint32_t *insnp = desc->ops.ops + keylist[i].ops_offs;
      switch (DDS_OP (*insnp))
      {
        case DDS_OP_KOF: {
          uint16_t n_offs = DDS_OP_LENGTH (*insnp);
          assert (n_offs > 0);
          dds_stream_write_keyBO_impl (os, allocator, desc->ops.ops + insnp[1], sample, --n_offs, insnp + 2);
          break;
        }
        case DDS_OP_ADR: {
          dds_stream_write_keyBO_impl (os, allocator, insnp, sample, 0, NULL);
          break;
        }
        default:
          abort ();
          break;
      }
    }
  }
}

static const uint32_t *dds_stream_extract_keyBO_from_data_adr (uint32_t insn, dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator,
  const uint32_t * const __restrict op0, const uint32_t * __restrict ops, bool mutable_member, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * __restrict keys_remaining)
{
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

static const uint32_t *dds_stream_extract_keyBO_from_data_delimited (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator,
  const uint32_t * const __restrict op0, const uint32_t * __restrict ops, bool mutable_member_or_parent, uint32_t n_keys, uint32_t * __restrict keys_remaining)
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
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM:
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
    *((uint32_t *) (((struct dds_ostream *)os)->m_buffer + delimited_offs_os - 4)) = to_BO4u (((struct dds_ostream *)os)->m_index - delimited_offs_os);
  }

  return ops;
}

static bool dds_stream_extract_keyBO_from_data_pl_member (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, uint32_t m_id,
  const uint32_t * const __restrict op0, const uint32_t * __restrict ops, uint32_t n_keys, uint32_t * __restrict keys_remaining)
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

      uint32_t *em_hdr_ptr = (uint32_t *) (((struct dds_ostream *)os)->m_buffer + data_offs - (lc == LENGTH_CODE_NEXTINT ? 8 : 4));
      em_hdr_ptr[0] = to_BO4u (em_hdr);
      if (lc == LENGTH_CODE_NEXTINT)
        em_hdr_ptr[1] = to_BO4u (((struct dds_ostream *)os)->m_index - data_offs);  /* member size in next_int field in emheader */

      found = true;
      break;
    }
    ops_csr += 2;
  }
  return found;
}

static const uint32_t *dds_stream_extract_keyBO_from_data_pl (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator,
  const uint32_t * const __restrict op0, const uint32_t * __restrict ops, uint32_t n_keys, uint32_t * __restrict keys_remaining)
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
    *((uint32_t *) (((struct dds_ostream *) os)->m_buffer + delimited_offs_os - 4)) = to_BO4u (((struct dds_ostream *) os)->m_index - delimited_offs_os);

  return ops;
}

static const uint32_t *dds_stream_extract_keyBO_from_data1 (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator,
  const uint32_t * const __restrict op0, const uint32_t * __restrict ops, bool mutable_member, bool mutable_member_or_parent,
  uint32_t n_keys, uint32_t * __restrict keys_remaining)
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
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM:
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

bool dds_stream_extract_keyBO_from_data (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
{
  bool ret = true;
  uint32_t keys_remaining = desc->keys.nkeys;
  if (keys_remaining == 0)
    return ret;

  if (desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE))
  {
    /* In case the type or any subtype has non-final extensibility, read the sample
       and write the key-only CDR for this sample */
    void *sample = allocator->malloc (desc->size);
    memset (sample, 0, desc->size);
    (void) dds_stream_read (is, sample, allocator, desc->ops.ops);
    dds_stream_write_keyBO (os, DDS_CDR_KEY_SERIALIZATION_SAMPLE, allocator, sample, desc);
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

static void dds_stream_extract_keyBO_from_key_impl (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, enum dds_cdr_key_serialization_kind ser_kind,
    const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
{
  /* The type or any subtype has non-final extensibility, so read a key sample
     and write the key-only CDR for this sample */
  void *sample = allocator->malloc (desc->size);
  memset (sample, 0, desc->size);
  (void) dds_stream_read_impl (is, sample, allocator, desc->ops.ops, false, CDR_KIND_KEY, SAMPLE_DATA_INITIALIZED);
  if (ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH)
    dds_stream_write_keyBE ((dds_ostreamBE_t *) os, ser_kind, allocator, sample, desc);
  else
    dds_stream_write_keyBO (os, ser_kind, allocator, sample, desc);
  dds_stream_free_sample (sample, allocator, desc->ops.ops);
  allocator->free (sample);
}

static void dds_stream_extract_keyBO_from_key_optimized (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os,
    const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
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
void dds_stream_extract_keyBO_from_key (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, enum dds_cdr_key_serialization_kind ser_kind,
    const struct dds_cdrstream_allocator * __restrict allocator, const struct dds_cdrstream_desc * __restrict desc)
{
  assert (ser_kind == DDS_CDR_KEY_SERIALIZATION_SAMPLE || ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH);

  /* This assumes that the key fields in the input CDR are in definition order.
     In case any key field is in an appendable or mutable type, or in case a serialized
     key for a keyhash is required (in member-id order), extract and write the key
     in two steps. Otherwise, extract the output CDR in a single step. */
  if ((desc->flagset & (DDS_TOPIC_KEY_APPENDABLE | DDS_TOPIC_KEY_MUTABLE)) || ser_kind == DDS_CDR_KEY_SERIALIZATION_KEYHASH)
    dds_stream_extract_keyBO_from_key_impl (is, os, ser_kind, allocator, desc);
  else
    dds_stream_extract_keyBO_from_key_optimized (is, os, allocator, desc);
}
