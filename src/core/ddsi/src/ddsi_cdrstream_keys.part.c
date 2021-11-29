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
static void dds_stream_write_keyBO_impl (DDS_OSTREAM_T * __restrict os, const uint32_t *insnp, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn);
static void dds_stream_write_keyBO_impl (DDS_OSTREAM_T * __restrict os, const uint32_t *insnp, const void *src, uint16_t key_offset_count, const uint32_t * key_offset_insn)
{
  assert (DDS_OP (*insnp) == DDS_OP_ADR);
  assert (insn_key_ok_p (*insnp));
  const void *addr = (char *) src + insnp[1];
  switch (DDS_OP_TYPE (*insnp))
  {
    case DDS_OP_VAL_1BY: dds_os_put1BO (os, *((uint8_t *) addr)); break;
    case DDS_OP_VAL_2BY: dds_os_put2BO (os, *((uint16_t *) addr)); break;
    case DDS_OP_VAL_4BY: case DDS_OP_VAL_ENU: dds_os_put4BO (os, *((uint32_t *) addr)); break;
    case DDS_OP_VAL_8BY: dds_os_put8BO (os, *((uint64_t *) addr)); break;
    case DDS_OP_VAL_STR: dds_stream_write_stringBO (os, *(char **) addr); break;
    case DDS_OP_VAL_BST: dds_stream_write_stringBO (os, addr); break;
    case DDS_OP_VAL_ARR: {
      const uint32_t elem_size = get_type_size (DDS_OP_SUBTYPE (*insnp));
      const uint32_t num = insnp[2];
      dds_cdr_alignto_clear_and_resizeBO (os, elem_size, num * elem_size);
      void * const dst = ((struct dds_ostream *)os)->m_buffer + ((struct dds_ostream *)os)->m_index;
      dds_os_put_bytes ((struct dds_ostream *)os, addr, num * elem_size);
      dds_stream_swap_if_needed_insituBO (dst, elem_size, num);
      break;
    }
    case DDS_OP_VAL_EXT: {
      assert (key_offset_count > 0);
      const uint32_t *jsr_ops = insnp + DDS_OP_ADR_JSR (insnp[2]) + *key_offset_insn;
      dds_stream_write_keyBO_impl (os, jsr_ops, addr, --key_offset_count, ++key_offset_insn);
      break;
    }
    case DDS_OP_VAL_SEQ: case DDS_OP_VAL_UNI: case DDS_OP_VAL_STU: {
      // FIXME: implement support for sequences and unions as part of the key
      abort ();
      break;
    }
  }
}

void dds_stream_write_keyBO (DDS_OSTREAM_T * __restrict os, const char * __restrict sample, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    const uint32_t *insnp = desc->ops.ops + desc->keys.keys[i].ops_offs;
    switch (DDS_OP (*insnp))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*insnp);
        dds_stream_write_keyBO_impl (os, desc->ops.ops + insnp[1], sample, --n_offs, insnp + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_write_keyBO_impl (os, insnp, sample, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}

static const uint32_t *dds_stream_extract_keyBO_from_data_delimited (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const uint32_t * __restrict ops,
  uint32_t n_keys, uint32_t * __restrict keys_remaining, const ddsi_sertype_default_desc_key_t * __restrict key, struct key_off_info * __restrict key_offs)
{
  uint32_t delimited_sz = dds_is_get4 (is), delimited_offs = is->m_index, insn;
  ops++;
  while ((insn = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (insn))
    {
      case DDS_OP_ADR: {
        /* skip fields that are not in serialized data for appendable type */
        const uint32_t type = DDS_OP_TYPE (insn);
        ops = (is->m_index - delimited_offs < delimited_sz) ?
          dds_stream_extract_keyBO_from_data1 (is, os, ops, n_keys, keys_remaining, key, key_offs) : dds_stream_extract_key_from_data_skip_adr (is, ops, type);
        break;
      }
      case DDS_OP_JSR: {
        (void) dds_stream_extract_keyBO_from_data1 (is, os, ops + DDS_OP_JUMP (insn), n_keys, keys_remaining, key, key_offs);
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_DLC: case DDS_OP_PLC: case DDS_OP_PLM: {
        abort ();
        break;
      }
    }
  }
  /* Skip remainder of serialized data for this appendable type */
  if (delimited_sz > is->m_index - delimited_offs)
    is->m_index += delimited_sz - (is->m_index - delimited_offs);
  return ops;
}

static const uint32_t *dds_stream_extract_keyBO_from_data_pl (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const uint32_t * __restrict ops,
  uint32_t n_keys, uint32_t * __restrict keys_remaining, const ddsi_sertype_default_desc_key_t * __restrict key, struct key_off_info * __restrict key_offs)
{
  /* skip PLC op */
  ops++;

  /* read DHEADER */
  uint32_t pl_sz = dds_is_get4 (is), pl_offs = is->m_index;
  while (is->m_index - pl_offs < pl_sz)
  {
    /* read EMHEADER and next_int */
    uint32_t em_hdr = dds_is_get4 (is);
    uint32_t lc = EMHEADER_LENGTH_CODE (em_hdr), mid = EMHEADER_MEMBERID (em_hdr), msz;
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

    /* find member and deserialize */
    uint32_t insn, ops_csr = 0;
    bool found = false;

    /* FIXME: continue finding the member in the ops member list starting from the last
       found one, because in many cases the members will be in the data sequentially */
    while (!found && (insn = ops[ops_csr]) != DDS_OP_RTS)
    {
      assert (DDS_OP (insn) == DDS_OP_PLM);
      if (ops[ops_csr + 1] == mid)
      {
        const uint32_t *plm_ops = ops + ops_csr + DDS_OP_ADR_JSR (insn);
        (void) dds_stream_extract_keyBO_from_data1 (is, os, plm_ops, n_keys, keys_remaining, key, key_offs);
        found = true;
        break;
      }
      ops_csr += 2;
    }

    if (!found)
    {
      is->m_index += msz;
      if (lc >= LENGTH_CODE_ALSO_NEXTINT)
        is->m_index += 4; /* length embedded in member does not include it's own 4 bytes */
    }
  }

  /* skip all PLM-memberid pairs */
  while (ops[0] != DDS_OP_RTS)
    ops += 2;

  return ops;
}

static const uint32_t *dds_stream_extract_keyBO_from_data1 (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const uint32_t * __restrict ops,
  uint32_t n_keys, uint32_t * __restrict keys_remaining, const ddsi_sertype_default_desc_key_t * __restrict key, struct key_off_info * __restrict key_offs)
{
  uint32_t op;
  while ((op = *ops) != DDS_OP_RTS)
  {
    switch (DDS_OP (op))
    {
      case DDS_OP_ADR: {
        const uint32_t type = DDS_OP_TYPE (op);
        const bool is_key = (op & DDS_OP_FLAG_KEY) && (os != NULL);
        if (type == DDS_OP_VAL_EXT)
        {
          const uint32_t *jsr_ops = ops + DDS_OP_ADR_JSR (ops[2]);
          const uint32_t jmp = DDS_OP_ADR_JMP (ops[2]);
          /* only in case the ADR|EXT has the key flag set, pass the actual ostream, otherwise skip the EXT type by passing NULL for ostream */
          (void) dds_stream_extract_keyBO_from_data1 (is, is_key ? os : NULL, jsr_ops, n_keys, keys_remaining, key, key_offs);
          ops += jmp ? jmp : 3;
        }
        else if (is_key)
        {
          assert (*keys_remaining <= n_keys);
          const uint32_t idx = key[n_keys - *keys_remaining].idx;
          key_offs[idx].src_off = is->m_index;
          key_offs[idx].op_off = ops;
          ops = dds_stream_extract_key_from_data_skip_adr (is, ops, type);
          assert (*keys_remaining > 0);
          if (--(*keys_remaining) == 0)
            return ops;
        }
        else
        {
          ops = dds_stream_extract_key_from_data_skip_adr (is, ops, type);
        }
        break;
      }
      case DDS_OP_JSR: { /* Implies nested type */
        ops += 2;
        dds_stream_extract_keyBO_from_data1 (is, os, ops + DDS_OP_JUMP (op), n_keys, keys_remaining, key, key_offs);
        if (*keys_remaining == 0)
          return ops;
        ops++;
        break;
      }
      case DDS_OP_RTS: case DDS_OP_JEQ: case DDS_OP_JEQ4: case DDS_OP_KOF: case DDS_OP_PLM: {
        abort ();
        break;
      }
      case DDS_OP_DLC: {
        ops = dds_stream_extract_keyBO_from_data_delimited (is, os, ops, n_keys, keys_remaining, key, key_offs);
        if (*keys_remaining == 0)
          return ops;
        break;
      }
      case DDS_OP_PLC: {
        ops = dds_stream_extract_keyBO_from_data_pl (is, os, ops, n_keys, keys_remaining, key, key_offs);
        if (*keys_remaining == 0)
          return ops;
        break;
      }
    }
  }
  return ops;
}

void dds_stream_extract_keyBO_from_data (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  uint32_t keys_remaining = desc->keys.nkeys;
  if (keys_remaining == 0)
    return;

#define MAX_ST_KEYS 16
  struct key_off_info st_key_offs[MAX_ST_KEYS];
  struct key_off_info * const key_offs =
    (desc->keys.nkeys <= MAX_ST_KEYS) ? st_key_offs : ddsrt_malloc (desc->keys.nkeys * sizeof (*key_offs));

  (void) dds_stream_extract_keyBO_from_data1 (is, os, desc->ops.ops, desc->keys.nkeys, &keys_remaining, desc->keys.keys, key_offs);

  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    is->m_index = key_offs[i].src_off;
    dds_stream_extract_keyBO_from_key_prim_op (is, os, key_offs[i].op_off, 0, NULL);
  }

  if (desc->keys.nkeys > MAX_ST_KEYS)
    ddsrt_free (key_offs);
#undef MAX_ST_KEYS
}

/* This function is used to create a serialized key in order to create a keyhash (big-endian) and to translate XCDR1 key CDR into XCDR2
   representation (native endianess). The former is not used regularly by Cyclone, and the latter is only used when receiving a key sample,
   e.g. a dispose. For this reason, we use a (performance wise) sub-optimal approach of going through the entire CDR for every key field.
   Optimizations is possible but would result in more complex code. */
void dds_stream_extract_keyBO_from_key (dds_istream_t * __restrict is, DDS_OSTREAM_T * __restrict os, const struct ddsi_sertype_default * __restrict type)
{
  const struct ddsi_sertype_default_desc *desc = &type->type;
  for (uint32_t i = 0; i < desc->keys.nkeys; i++)
  {
    uint32_t const * const op = desc->ops.ops + desc->keys.keys[i].ops_offs;
    switch (DDS_OP (*op))
    {
      case DDS_OP_KOF: {
        uint16_t n_offs = DDS_OP_LENGTH (*op);
        dds_stream_extract_keyBO_from_key_prim_op (is, os, desc->ops.ops + op[1], --n_offs, op + 2);
        break;
      }
      case DDS_OP_ADR: {
        dds_stream_extract_keyBO_from_key_prim_op (is, os, op, 0, NULL);
        break;
      }
      default:
        abort ();
        break;
    }
  }
}
