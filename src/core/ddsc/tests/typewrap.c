// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/dds.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "ddsi__dynamic_type.h"
#include "ddsi__typelib.h"
#include "ddsi__typewrap.h"
#include "ddsi__xt_impl.h"
#include "test_util.h"

static dds_entity_t domain = 0, participant = 0;

static void typewrap_init (void)
{
  domain = dds_create_domain (0, NULL);
  CU_ASSERT_GEQ_FATAL (domain, 0);
  participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (participant, 0);
}

static void typewrap_fini (void)
{
  dds_return_t ret = dds_delete (participant);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (domain);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
}

struct enum_literal {
  const char *name;
  int32_t value;
};

struct bitflag {
  const char *name;
  uint16_t position;
};

struct struct_member {
  const char *name;
  uint32_t id;
};

struct union_member {
  const char *name;
  uint32_t id;
  int32_t label;
};

static void check_typeobject (
    const struct DDS_XTypes_TypeObject *typeobj,
    dds_return_t expected_ret)
{
  ddsi_typeid_t typeid;
  dds_return_t ret = ddsi_typeobj_get_hash_id (typeobj, &typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type *type = NULL;
  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = ddsi_type_ref_id_locked (gv, &type, &typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (ret == DDS_RETCODE_OK)
  {
    ret = ddsi_type_add_typeobj (gv, type, typeobj);
    CU_ASSERT_EQ (ret, expected_ret);
    ddsi_type_unref_locked (gv, type);
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

static void check_enum_typeobject (
    const char *name,
    uint16_t bit_bound,
    const struct enum_literal *literals,
    uint32_t n_literals,
    dds_return_t expected_ret)
{
  struct DDS_XTypes_CompleteEnumeratedLiteral literal_buf[5] = {0};
  CU_ASSERT_LEQ_FATAL (n_literals, sizeof (literal_buf) / sizeof (literal_buf[0]));
  for (uint32_t n = 0; n < n_literals; n++)
  {
    literal_buf[n].common.value = literals[n].value;
    ddsrt_strlcpy (literal_buf[n].detail.name, literals[n].name, sizeof (literal_buf[n].detail.name));
  }

  struct DDS_XTypes_TypeObject typeobj = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_ENUM,
      ._u.enumerated_type = {
        .enum_flags = DDS_XTypes_IS_FINAL,
        .header = { .common = { .bit_bound = bit_bound } },
        .literal_seq = {
          ._maximum = n_literals,
          ._length = n_literals,
          ._buffer = literal_buf,
          ._release = false
        }
      }
    }
  };
  ddsrt_strlcpy (typeobj._u.complete._u.enumerated_type.header.detail.type_name, name,
      sizeof (typeobj._u.complete._u.enumerated_type.header.detail.type_name));

  check_typeobject (&typeobj, expected_ret);
}

static void check_bitmask_typeobject (
    const char *name,
    uint16_t bit_bound,
    const struct bitflag *bitflags,
    uint32_t n_bitflags,
    dds_return_t expected_ret)
{
  struct DDS_XTypes_CompleteBitflag bitflag_buf[5] = {0};
  CU_ASSERT_LEQ_FATAL (n_bitflags, sizeof (bitflag_buf) / sizeof (bitflag_buf[0]));
  for (uint32_t n = 0; n < n_bitflags; n++)
  {
    bitflag_buf[n].common.position = bitflags[n].position;
    ddsrt_strlcpy (bitflag_buf[n].detail.name, bitflags[n].name, sizeof (bitflag_buf[n].detail.name));
  }

  struct DDS_XTypes_TypeObject typeobj = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_BITMASK,
      ._u.bitmask_type = {
        .bitmask_flags = DDS_XTypes_IS_FINAL,
        .header = { .common = { .bit_bound = bit_bound } },
        .flag_seq = {
          ._maximum = n_bitflags,
          ._length = n_bitflags,
          ._buffer = bitflag_buf,
          ._release = false
        }
      }
    }
  };
  ddsrt_strlcpy (typeobj._u.complete._u.bitmask_type.header.detail.type_name, name,
      sizeof (typeobj._u.complete._u.bitmask_type.header.detail.type_name));

  check_typeobject (&typeobj, expected_ret);
}

static void check_struct_typeobject (
    const char *name,
    const struct struct_member *members,
    uint32_t n_members,
    dds_return_t expected_ret)
{
  struct DDS_XTypes_CompleteStructMember member_buf[5] = {0};
  CU_ASSERT_LEQ_FATAL (n_members, sizeof (member_buf) / sizeof (member_buf[0]));
  for (uint32_t n = 0; n < n_members; n++)
  {
    member_buf[n].common.member_id = members[n].id;
    member_buf[n].common.member_flags = DDS_XTypes_TRY_CONSTRUCT1;
    member_buf[n].common.member_type_id._d = DDS_XTypes_TK_INT32;
    ddsrt_strlcpy (member_buf[n].detail.name, members[n].name, sizeof (member_buf[n].detail.name));
  }

  struct DDS_XTypes_TypeObject typeobj = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_STRUCTURE,
      ._u.struct_type = {
        .struct_flags = DDS_XTypes_IS_FINAL,
        .member_seq = {
          ._maximum = n_members,
          ._length = n_members,
          ._buffer = member_buf,
          ._release = false
        }
      }
    }
  };
  ddsrt_strlcpy (typeobj._u.complete._u.struct_type.header.detail.type_name, name,
      sizeof (typeobj._u.complete._u.struct_type.header.detail.type_name));

  check_typeobject (&typeobj, expected_ret);
}

static void check_union_typeobject (
    const char *name,
    const struct union_member *members,
    uint32_t n_members,
    dds_return_t expected_ret)
{
  int32_t label_buf[5] = {0};
  struct DDS_XTypes_CompleteUnionMember member_buf[5] = {0};
  CU_ASSERT_LEQ_FATAL (n_members, sizeof (member_buf) / sizeof (member_buf[0]));
  for (uint32_t n = 0; n < n_members; n++)
  {
    label_buf[n] = members[n].label;
    member_buf[n].common.member_id = members[n].id;
    member_buf[n].common.member_flags = DDS_XTypes_TRY_CONSTRUCT1;
    member_buf[n].common.type_id._d = DDS_XTypes_TK_INT32;
    member_buf[n].common.label_seq._maximum = 1;
    member_buf[n].common.label_seq._length = 1;
    member_buf[n].common.label_seq._buffer = &label_buf[n];
    member_buf[n].common.label_seq._release = false;
    ddsrt_strlcpy (member_buf[n].detail.name, members[n].name, sizeof (member_buf[n].detail.name));
  }

  struct DDS_XTypes_TypeObject typeobj = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_UNION,
      ._u.union_type = {
        .union_flags = DDS_XTypes_IS_FINAL,
        .discriminator = {
          .common = {
            .member_flags = DDS_XTypes_TRY_CONSTRUCT1,
            .type_id = { ._d = DDS_XTypes_TK_INT32 }
          }
        },
        .member_seq = {
          ._maximum = n_members,
          ._length = n_members,
          ._buffer = member_buf,
          ._release = false
        }
      }
    }
  };
  ddsrt_strlcpy (typeobj._u.complete._u.union_type.header.detail.type_name, name,
      sizeof (typeobj._u.complete._u.union_type.header.detail.type_name));

  check_typeobject (&typeobj, expected_ret);
}

CU_Test (ddsc_typewrap, invalid_enum_typeobject, .init = typewrap_init, .fini = typewrap_fini)
{
  const struct enum_literal duplicate_adjacent[] = {
    { "DuplicateAdjacentA", 1 },
    { "DuplicateAdjacentB", 1 },
    { "DuplicateAdjacentC", 2 }
  };
  check_enum_typeobject ("DuplicateAdjacent", 4, duplicate_adjacent,
      sizeof (duplicate_adjacent) / sizeof (duplicate_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_unsorted[] = {
    { "DuplicateUnsortedA", 0 },
    { "DuplicateUnsortedB", 3 },
    { "DuplicateUnsortedC", 12 },
    { "DuplicateUnsortedD", 3 }
  };
  check_enum_typeobject ("DuplicateUnsorted", 5, duplicate_unsorted,
      sizeof (duplicate_unsorted) / sizeof (duplicate_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_at_min[] = {
    { "DuplicateAtMinA", 7 },
    { "DuplicateAtMinB", 0 },
    { "DuplicateAtMinC", 5 },
    { "DuplicateAtMinD", 0 }
  };
  check_enum_typeobject ("DuplicateAtMinAfterSort", 4, duplicate_at_min,
      sizeof (duplicate_at_min) / sizeof (duplicate_at_min[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_at_max[] = {
    { "DuplicateAtMaxA", 0 },
    { "DuplicateAtMaxB", 7 },
    { "DuplicateAtMaxC", 1 },
    { "DuplicateAtMaxD", 7 }
  };
  check_enum_typeobject ("DuplicateAtMaxAfterSort", 4, duplicate_at_max,
      sizeof (duplicate_at_max) / sizeof (duplicate_at_max[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_run[] = {
    { "DuplicateRunA", 6 },
    { "DuplicateRunB", 2 },
    { "DuplicateRunC", 2 },
    { "DuplicateRunD", 1 },
    { "DuplicateRunE", 2 }
  };
  check_enum_typeobject ("DuplicateRunAfterSort", 4, duplicate_run,
      sizeof (duplicate_run) / sizeof (duplicate_run[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_name_adjacent[] = {
    { "DuplicateNameAdjacentA", 0 },
    { "DuplicateNameAdjacentA", 1 },
    { "DuplicateNameAdjacentC", 2 }
  };
  check_enum_typeobject ("DuplicateNameAdjacent", 4, duplicate_name_adjacent,
      sizeof (duplicate_name_adjacent) / sizeof (duplicate_name_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_name_unsorted[] = {
    { "DuplicateNameUnsortedA", 3 },
    { "DuplicateNameUnsortedB", 1 },
    { "DuplicateNameUnsortedA", 2 },
    { "DuplicateNameUnsortedD", 4 }
  };
  check_enum_typeobject ("DuplicateNameUnsorted", 4, duplicate_name_unsorted,
      sizeof (duplicate_name_unsorted) / sizeof (duplicate_name_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal duplicate_name_run[] = {
    { "DuplicateNameRunA", 5 },
    { "DuplicateNameRunB", 3 },
    { "DuplicateNameRunB", 0 },
    { "DuplicateNameRunD", 2 },
    { "DuplicateNameRunB", 4 }
  };
  check_enum_typeobject ("DuplicateNameRun", 4, duplicate_name_run,
      sizeof (duplicate_name_run) / sizeof (duplicate_name_run[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal too_large_two_bits[] = {
    { "TwoBitsMin", 0 },
    { "TwoBitsMax", 3 },
    { "TwoBitsTooLarge", 4 }
  };
  check_enum_typeobject ("TooLargeForTwoBits", 2, too_large_two_bits,
      sizeof (too_large_two_bits) / sizeof (too_large_two_bits[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal too_large_eight_bits[] = {
    { "EightBitsMin", 0 },
    { "EightBitsMax", 255 },
    { "EightBitsTooLarge", 256 }
  };
  check_enum_typeobject ("TooLargeForEightBits", 8, too_large_eight_bits,
      sizeof (too_large_eight_bits) / sizeof (too_large_eight_bits[0]), DDS_RETCODE_BAD_PARAMETER);

  check_enum_typeobject ("NoSymbols", 8, NULL, 0, DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal negative_values[] = {
    { "NegativeMin", -1 },
    { "NegativeMid", 0 }
  };
  check_enum_typeobject ("NegativeValues", 1, negative_values,
      sizeof (negative_values) / sizeof (negative_values[0]), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test (ddsc_typewrap, invalid_bitmask_typeobject, .init = typewrap_init, .fini = typewrap_fini)
{
  const struct bitflag duplicate_adjacent[] = {
    { "DuplicateAdjacentA", 1 },
    { "DuplicateAdjacentB", 1 },
    { "DuplicateAdjacentC", 2 }
  };
  check_bitmask_typeobject ("DuplicateAdjacentBitflag", 8, duplicate_adjacent,
      sizeof (duplicate_adjacent) / sizeof (duplicate_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag duplicate_unsorted[] = {
    { "DuplicateUnsortedA", 7 },
    { "DuplicateUnsortedB", 2 },
    { "DuplicateUnsortedC", 5 },
    { "DuplicateUnsortedD", 2 }
  };
  check_bitmask_typeobject ("DuplicateUnsortedBitflag", 8, duplicate_unsorted,
      sizeof (duplicate_unsorted) / sizeof (duplicate_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag duplicate_run[] = {
    { "DuplicateRunA", 6 },
    { "DuplicateRunB", 2 },
    { "DuplicateRunC", 2 },
    { "DuplicateRunD", 5 },
    { "DuplicateRunE", 2 }
  };
  check_bitmask_typeobject ("DuplicateRunBitflag", 8, duplicate_run,
      sizeof (duplicate_run) / sizeof (duplicate_run[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag duplicate_name_adjacent[] = {
    { "DuplicateNameAdjacentA", 0 },
    { "DuplicateNameAdjacentA", 1 },
    { "DuplicateNameAdjacentC", 2 }
  };
  check_bitmask_typeobject ("DuplicateNameAdjacentBitflag", 8, duplicate_name_adjacent,
      sizeof (duplicate_name_adjacent) / sizeof (duplicate_name_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag duplicate_name_unsorted[] = {
    { "DuplicateNameUnsortedA", 0 },
    { "DuplicateNameUnsortedB", 2 },
    { "DuplicateNameUnsortedA", 4 },
    { "DuplicateNameUnsortedD", 6 }
  };
  check_bitmask_typeobject ("DuplicateNameUnsortedBitflag", 8, duplicate_name_unsorted,
      sizeof (duplicate_name_unsorted) / sizeof (duplicate_name_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag too_large_eight_bits[] = {
    { "EightBitsFirst", 0 },
    { "EightBitsLast", 7 },
    { "EightBitsTooLarge", 8 }
  };
  check_bitmask_typeobject ("TooLargeForEightBitBitmask", 8, too_large_eight_bits,
      sizeof (too_large_eight_bits) / sizeof (too_large_eight_bits[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag too_large_sixty_four_bits[] = {
    { "SixtyFourBitsFirst", 0 },
    { "SixtyFourBitsLast", 63 },
    { "SixtyFourBitsTooLarge", 64 }
  };
  check_bitmask_typeobject ("TooLargeForSixtyFourBitBitmask", 64, too_large_sixty_four_bits,
      sizeof (too_large_sixty_four_bits) / sizeof (too_large_sixty_four_bits[0]), DDS_RETCODE_BAD_PARAMETER);

  check_bitmask_typeobject ("NoBitflags", 8, NULL, 0, DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag valid_positions[] = {
    { "ValidFirst", 0 },
    { "ValidMiddle", 31 },
    { "ValidLast", 63 }
  };
  check_bitmask_typeobject ("ValidSixtyFourBitBitmask", 64, valid_positions,
      sizeof (valid_positions) / sizeof (valid_positions[0]), DDS_RETCODE_OK);
}

CU_Test (ddsc_typewrap, invalid_struct_typeobject, .init = typewrap_init, .fini = typewrap_fini)
{
  const struct struct_member duplicate_adjacent[] = {
    { "duplicate_adjacent_a", 1 },
    { "duplicate_adjacent_b", 1 },
    { "duplicate_adjacent_c", 2 }
  };
  check_struct_typeobject ("DuplicateAdjacentStructMember", duplicate_adjacent,
      sizeof (duplicate_adjacent) / sizeof (duplicate_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct struct_member duplicate_unsorted[] = {
    { "duplicate_unsorted_a", 7 },
    { "duplicate_unsorted_b", 2 },
    { "duplicate_unsorted_c", 5 },
    { "duplicate_unsorted_d", 2 }
  };
  check_struct_typeobject ("DuplicateUnsortedStructMember", duplicate_unsorted,
      sizeof (duplicate_unsorted) / sizeof (duplicate_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct struct_member duplicate_run[] = {
    { "duplicate_run_a", 6 },
    { "duplicate_run_b", 2 },
    { "duplicate_run_c", 2 },
    { "duplicate_run_d", 5 },
    { "duplicate_run_e", 2 }
  };
  check_struct_typeobject ("DuplicateRunStructMember", duplicate_run,
      sizeof (duplicate_run) / sizeof (duplicate_run[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct struct_member valid_member_ids[] = {
    { "valid_first", 1 },
    { "valid_middle", 12 },
    { "valid_last", 27 }
  };
  check_struct_typeobject ("ValidStructMemberIds", valid_member_ids,
      sizeof (valid_member_ids) / sizeof (valid_member_ids[0]), DDS_RETCODE_OK);
}

CU_Test (ddsc_typewrap, invalid_union_typeobject, .init = typewrap_init, .fini = typewrap_fini)
{
  const struct union_member duplicate_adjacent[] = {
    { "duplicate_adjacent_a", 1, 1 },
    { "duplicate_adjacent_b", 1, 2 },
    { "duplicate_adjacent_c", 2, 3 }
  };
  check_union_typeobject ("DuplicateAdjacentUnionMember", duplicate_adjacent,
      sizeof (duplicate_adjacent) / sizeof (duplicate_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member duplicate_unsorted[] = {
    { "duplicate_unsorted_a", 7, 1 },
    { "duplicate_unsorted_b", 2, 2 },
    { "duplicate_unsorted_c", 5, 3 },
    { "duplicate_unsorted_d", 2, 4 }
  };
  check_union_typeobject ("DuplicateUnsortedUnionMember", duplicate_unsorted,
      sizeof (duplicate_unsorted) / sizeof (duplicate_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member duplicate_run[] = {
    { "duplicate_run_a", 6, 1 },
    { "duplicate_run_b", 2, 2 },
    { "duplicate_run_c", 2, 3 },
    { "duplicate_run_d", 5, 4 },
    { "duplicate_run_e", 2, 5 }
  };
  check_union_typeobject ("DuplicateRunUnionMember", duplicate_run,
      sizeof (duplicate_run) / sizeof (duplicate_run[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member valid_member_ids[] = {
    { "valid_first", 1, 1 },
    { "valid_middle", 12, 2 },
    { "valid_last", 27, 3 }
  };
  check_union_typeobject ("ValidUnionMemberIds", valid_member_ids,
      sizeof (valid_member_ids) / sizeof (valid_member_ids[0]), DDS_RETCODE_OK);
}
