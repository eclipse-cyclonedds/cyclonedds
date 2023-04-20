// Copyright(c) 2023 ZettaScale Technology and others
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
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "ddsi__dynamic_type.h"
#include "ddsi__xt_impl.h"
#include "test_util.h"
#include "Space.h"

static dds_entity_t domain = 0, participant = 0;

static void dynamic_type_init(void)
{
  domain = dds_create_domain (0, NULL);
  CU_ASSERT_FATAL (domain >= 0);
  participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (participant >= 0);
}

static void dynamic_type_fini(void)
{
  dds_return_t ret = dds_delete (participant);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
}

static void do_test (dds_dynamic_type_t *dtype)
{
  dds_return_t ret;
  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (dtype, &type_info);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *descriptor;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, participant, type_info, 0, &descriptor);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  char topic_name[100];
  create_unique_topic_name ("ddsc_dynamic_type", topic_name, sizeof (topic_name));
  dds_entity_t topic = dds_create_topic (participant, descriptor, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic >= 0);

  dds_free_typeinfo (type_info);
  dds_delete_topic_descriptor (descriptor);
  dds_dynamic_type_unref (dtype);
}

CU_Test (ddsc_dynamic_type, basic, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant,
    (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));
  do_test (&dstruct);
}

CU_Test (ddsc_dynamic_type, entity_kinds, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  char name[100];
  dds_entity_t publisher = dds_create_publisher (participant, NULL, NULL);
  dds_entity_t topic = dds_create_topic (participant, &Space_Type1_desc, create_unique_topic_name("ddsc_dynamic_type_test", name, sizeof name), NULL, NULL);

  const struct {
    dds_entity_t entity;
    dds_return_t ret;
  } tests[] = {
    { DDS_CYCLONEDDS_HANDLE, DDS_RETCODE_BAD_PARAMETER },
    { domain, DDS_RETCODE_OK },
    { participant, DDS_RETCODE_OK },
    { publisher, DDS_RETCODE_OK },
    { topic, DDS_RETCODE_OK }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_dynamic_type_t dstruct = dds_dynamic_type_create (tests[i].entity,
      (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
    CU_ASSERT_EQUAL_FATAL (dstruct.ret, tests[i].ret);
    if (tests[i].ret == DDS_RETCODE_OK)
    {
      dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));
      do_test (&dstruct);
    }
  }
}

/* Copy of the DDS_DYNAMIC_TYPE_SPEC_PRIM macro, without the explicit cast because
   that causes a build error on MSVC when used in a designated initializer. */
#define TYPE_SPEC_PRIM_NC(p) { .kind = DDS_DYNAMIC_TYPE_KIND_PRIMITIVE, .type.primitive = (p) }

CU_Test (ddsc_dynamic_type, type_create, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  static const uint32_t bounds[] = { 10 };
  static const struct {
    dds_dynamic_type_descriptor_t desc;
    dds_return_t ret;
  } tests[] = {
    { { .kind = DDS_DYNAMIC_NONE, .name = "t" }, DDS_RETCODE_BAD_PARAMETER },
    { { .kind = DDS_DYNAMIC_BOOLEAN, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_BYTE, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_INT16, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_INT32, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_INT64, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_UINT16, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_UINT32, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_UINT64, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_FLOAT32, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_FLOAT64, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_FLOAT128, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_INT8, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_UINT8, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_CHAR8, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_CHAR16, .name = "t" }, DDS_RETCODE_UNSUPPORTED },
    { { .kind = DDS_DYNAMIC_STRING8, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_STRING16, .name = "t" }, DDS_RETCODE_UNSUPPORTED },
    { { .kind = DDS_DYNAMIC_ENUMERATION, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_BITMASK, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_ALIAS, .name = "t", .base_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32) }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_ARRAY, .name = "t", .element_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32), .bounds = bounds, .num_bounds = 1 }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_SEQUENCE, .name = "t", .element_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32) }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_MAP, .name = "t" }, DDS_RETCODE_UNSUPPORTED },
    { { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_UNION, .name = "t", .discriminator_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32) }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_BITSET, .name = "t" }, DDS_RETCODE_UNSUPPORTED }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_dynamic_type_t dtype = dds_dynamic_type_create (participant, tests[i].desc);
    printf("create type kind %u, return code %d\n", tests[i].desc.kind, dtype.ret);
    CU_ASSERT_EQUAL_FATAL (dtype.ret, tests[i].ret);
    if (tests[i].ret == DDS_RETCODE_OK)
      dds_dynamic_type_unref (&dtype);
  }
}

static struct ddsi_type * get_ddsi_type (dds_dynamic_type_t *dtype)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  dds_typeinfo_t *type_info;
  dds_return_t ret = dds_dynamic_type_register (dtype, &type_info);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  const ddsi_typeid_t *type_id = ddsi_typeinfo_complete_typeid (type_info);
  struct ddsi_type *type = ddsi_type_lookup (gv, type_id);
  CU_ASSERT_FATAL (type != NULL);
  dds_free_typeinfo (type_info);
  return type;
}

CU_Test (ddsc_dynamic_type, struct_member_id, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct;

  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_set_autoid (&dstruct, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID_PRIM(DDS_DYNAMIC_UINT16, "m2", 123));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m3"));
  dds_dynamic_type_add_member (&dstruct, ((dds_dynamic_member_descriptor_t) {
      .type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_UINT16),
      .name = "m0",
      .id = DDS_DYNAMIC_MEMBER_ID_AUTO,
      .index = DDS_DYNAMIC_MEMBER_INDEX_START
  }));

  struct ddsi_type *type = get_ddsi_type (&dstruct);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.length, 4);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.seq[0].id, ddsi_dynamic_type_member_hashid ("m0"));
  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.seq[1].id, ddsi_dynamic_type_member_hashid ("m1"));
  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.seq[2].id, 123);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.seq[3].id, ddsi_dynamic_type_member_hashid ("m3"));

  dds_dynamic_type_unref (&dstruct);
}

CU_Test (ddsc_dynamic_type, extensibility_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_return_t ret;

  // Type parameter NULL
  ret = dds_dynamic_type_set_extensibility (NULL, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Invalid type
  dds_dynamic_type_t dbool = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BOOLEAN });
  ret = dds_dynamic_type_set_extensibility (&dbool, DDS_DYNAMIC_TYPE_EXT_FINAL);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbool);

  // Invalid extensibility value
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_set_extensibility (&dstruct, (enum dds_dynamic_type_extensibility) 99);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Type may not have members
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  ret = dds_dynamic_type_set_extensibility (&dstruct, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dstruct);

  // Type must be in constructing state
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  (void) get_ddsi_type (&dstruct);
  ret = dds_dynamic_type_set_extensibility (&dstruct, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dstruct);
}

CU_Test (ddsc_dynamic_type, extensibility_valid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  static const struct {
    bool default_ext;
    enum dds_dynamic_type_extensibility dyn_ext;
    uint16_t xt_ext;
  } tests[] = {
    { true, 0, DDS_XTypes_IS_FINAL },
    { false, DDS_DYNAMIC_TYPE_EXT_FINAL, DDS_XTypes_IS_FINAL },
    { false, DDS_DYNAMIC_TYPE_EXT_APPENDABLE, DDS_XTypes_IS_APPENDABLE },
    { false, DDS_DYNAMIC_TYPE_EXT_MUTABLE, DDS_XTypes_IS_MUTABLE }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
    if (!tests[i].default_ext)
      dds_dynamic_type_set_extensibility (&dstruct, tests[i].dyn_ext);
    dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));

    struct ddsi_type *type = get_ddsi_type (&dstruct);
    uint16_t exp_xt_ext = tests[i].default_ext ? DDS_XTypes_IS_FINAL : tests[i].xt_ext;
    CU_ASSERT_FATAL (type->xt._u.structure.flags & exp_xt_ext);

    dds_dynamic_type_unref (&dstruct);
  }
}

CU_Test (ddsc_dynamic_type, bit_bound, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_return_t ret;

  // Type parameter NULL
  ret = dds_dynamic_type_set_bit_bound (NULL, 16);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Invalid type kind
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_set_bit_bound (NULL, 16);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Invalid bit-bound value
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 0);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 65);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  ret = dds_dynamic_type_set_bit_bound (&denum, 0);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  ret = dds_dynamic_type_set_bit_bound (&denum, 33);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);
  dds_dynamic_type_unref (&denum);

  // Type may not have members
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 1);
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 16);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dbitmask);

  // Type must be in constructing state
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 1);
  (void) get_ddsi_type (&dbitmask);
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 16);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dbitmask);
}

CU_Test (ddsc_dynamic_type, bitmask, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_return_t ret = dds_dynamic_type_set_bit_bound (&dbitmask, 16);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_auto0", DDS_DYNAMIC_BITMASK_POSITION_AUTO);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_auto1", DDS_DYNAMIC_BITMASK_POSITION_AUTO);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_10", 10);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_5", 5);

  struct ddsi_type *type = get_ddsi_type (&dbitmask);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bit_bound, 16);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bitflags.length, 4);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bitflags.seq[0].position, 0);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bitflags.seq[1].position, 1);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bitflags.seq[2].position, 10);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bitflags.seq[3].position, 5);

  dds_dynamic_type_unref (&dbitmask);
}

CU_Test (ddsc_dynamic_type, bitmask_field_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  // Bitmask type NULL
  dds_return_t ret = dds_dynamic_type_add_bitmask_field (NULL, "b1", 1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Name property missing
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  ret = dds_dynamic_type_add_bitmask_field (&dbitmask, "", 1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);

  // Position in use
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 1);
  ret = dds_dynamic_type_add_bitmask_field (&dbitmask, "b2", 1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);

  // Invalid position for bit-bound
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_set_bit_bound (&dbitmask, 2);
  ret = dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 2);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);
}

CU_Test (ddsc_dynamic_type, enum_type, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_return_t ret = dds_dynamic_type_set_bit_bound (&denum, 31);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_dynamic_type_add_enum_literal (&denum, "e_auto0", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false);
  dds_dynamic_type_add_enum_literal (&denum, "e_auto1", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, true);
  dds_dynamic_type_add_enum_literal (&denum, "e_31", DDS_DYNAMIC_ENUM_LITERAL_VALUE ((1u << 31) - 1), false);
  dds_dynamic_type_add_enum_literal (&denum, "e_2", DDS_DYNAMIC_ENUM_LITERAL_VALUE (2), false);

  struct ddsi_type *type = get_ddsi_type (&denum);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.bitmask.bit_bound, 31);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.enum_type.literals.length, 4);

  CU_ASSERT_EQUAL_FATAL (type->xt._u.enum_type.literals.seq[0].value, 0);
  CU_ASSERT_FATAL (!(type->xt._u.enum_type.literals.seq[0].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQUAL_FATAL (type->xt._u.enum_type.literals.seq[1].value, 1);
  CU_ASSERT_FATAL (type->xt._u.enum_type.literals.seq[1].flags & DDS_XTypes_IS_DEFAULT);

  CU_ASSERT_EQUAL_FATAL (type->xt._u.enum_type.literals.seq[2].value, (1u << 31) - 1);
  CU_ASSERT_FATAL (!(type->xt._u.enum_type.literals.seq[2].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQUAL_FATAL (type->xt._u.enum_type.literals.seq[3].value, 2);
  CU_ASSERT_FATAL (!(type->xt._u.enum_type.literals.seq[3].flags & DDS_XTypes_IS_DEFAULT));

  dds_dynamic_type_unref (&denum);
}

CU_Test (ddsc_dynamic_type, enum_literal_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  // Enum type NULL
  dds_return_t ret = dds_dynamic_type_add_enum_literal (NULL, "b1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Name property missing
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  ret = dds_dynamic_type_add_enum_literal (&denum, "", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Value in use
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e2", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Name in use
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (2), false);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Multiple default values
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), true);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e2", DDS_DYNAMIC_ENUM_LITERAL_VALUE (2), true);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Invalid value for bit-bound
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_set_bit_bound (&denum, 2);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (4), false);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);
}

CU_Test (ddsc_dynamic_type, struct_member_prop, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_set_autoid (&dstruct, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m2"));

  dds_return_t ret = dds_dynamic_member_set_key (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), true);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_optional (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), true);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_external (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), true);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_hashid (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), "m2_name");
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  // Because of the set_hashid, from this point the member has a different id
  ret = dds_dynamic_member_set_must_understand (&dstruct, ddsi_dynamic_type_member_hashid ("m2_name"), true);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_type *type = get_ddsi_type (&dstruct);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.length, 2);

  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.seq[0].id, ddsi_dynamic_type_member_hashid ("m1"));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_KEY));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_OPTIONAL));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_MUST_UNDERSTAND));

  CU_ASSERT_EQUAL_FATAL (type->xt._u.structure.members.seq[1].id, ddsi_dynamic_type_member_hashid ("m2_name"));
  CU_ASSERT_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_KEY);
  CU_ASSERT_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_OPTIONAL);
  CU_ASSERT_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_EXTERNAL);
  CU_ASSERT_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_MUST_UNDERSTAND);

  dds_dynamic_type_unref (&dstruct);
}

CU_Test (ddsc_dynamic_type, struct_member_prop_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct;
  dds_return_t ret;

  // Re-used member hash-name
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_set_autoid (&dstruct, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m2"));
  ret = dds_dynamic_member_set_hashid (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), "m1");
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Empty member name
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, ""));
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Non-primitive type member, re-used member id
  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "s" });
  dds_dynamic_type_add_member (&dsubstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "s1"));

  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID_PRIM(DDS_DYNAMIC_INT32, "m1", 1));
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID(dsubstruct, "m2", 1));
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);
}

CU_Test (ddsc_dynamic_type, union_member_prop, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dunion = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_UNION,
    .name = "u",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32)
  });
  dds_dynamic_type_set_autoid (&dunion, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m1", 2, ((int32_t[]) { 1, 2 })));
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m2", 1, ((int32_t[]) { 5 })));
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(DDS_DYNAMIC_BOOLEAN, "md"));

  dds_return_t ret = dds_dynamic_member_set_hashid (&dunion, ddsi_dynamic_type_member_hashid ("m2"), "m2_name");
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  // Because of the set_hashid, from this point the member has a different id
  ret = dds_dynamic_member_set_external (&dunion, ddsi_dynamic_type_member_hashid ("m2_name"), true);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_type *type = get_ddsi_type (&dunion);
  CU_ASSERT_EQUAL_FATAL (type->xt._u.union_type.members.length, 3);

  CU_ASSERT_EQUAL_FATAL (type->xt._u.union_type.members.seq[0].id, ddsi_dynamic_type_member_hashid ("m1"));
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[0].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[0].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQUAL_FATAL (type->xt._u.union_type.members.seq[1].id, ddsi_dynamic_type_member_hashid ("m2_name"));
  CU_ASSERT_FATAL (type->xt._u.union_type.members.seq[1].flags & DDS_XTypes_IS_EXTERNAL);
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[1].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQUAL_FATAL (type->xt._u.union_type.members.seq[2].id, ddsi_dynamic_type_member_hashid ("md"));
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[2].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_FATAL (type->xt._u.union_type.members.seq[2].flags & DDS_XTypes_IS_DEFAULT);

  dds_dynamic_type_unref (&dunion);
}

CU_Test (ddsc_dynamic_type, union_member_prop_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dunion;
  dds_return_t ret;
  dds_dynamic_type_descriptor_t desc = {
    .kind = DDS_DYNAMIC_UNION,
    .name = "u",
    .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32)
  };

  // Existing hash name
  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_set_autoid (&dunion, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m1", 1, ((int32_t[]) { 1 })));
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m2", 1, ((int32_t[]) { 2 })));
  ret = dds_dynamic_member_set_hashid (&dunion, ddsi_dynamic_type_member_hashid ("m2"), "m1");
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);

  // Re-used label
  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m1", 1, ((int32_t[]) { 1 })));
  ret = dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m2", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);

  // Multiple default
  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(DDS_DYNAMIC_INT32, "m1"));
  ret = dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(DDS_DYNAMIC_INT32, "m2"));
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);

  // Non-primitive type member, re-used label
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));

  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER(dstruct, "m1", 1, ((int32_t[]) { 1 })));
  ret = dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT16, "m2", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);
}

CU_Test (ddsc_dynamic_type, no_members, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_typeinfo_t *type_info;
  dds_return_t ret;

  // Struct without members
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_register (&dstruct, &type_info);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Struct with basetype without members
  dds_dynamic_type_t dbasestruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "b" });
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t", .base_type = DDS_DYNAMIC_TYPE_SPEC (dbasestruct) });
  CU_ASSERT_EQUAL_FATAL (dstruct.ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbasestruct);

  // Struct with substruct without members
  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "s" });
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsubstruct, "m1"));
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Union without members
  dds_dynamic_type_t dunion = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t)
      { .kind = DDS_DYNAMIC_UNION, .name = "u", .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32) });
  ret = dds_dynamic_type_register (&dunion, &type_info);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);
}

static void create_type_topic_wr (dds_entity_t pp, const char *topic_name, ddsi_typeid_t **type_id)
{
  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (pp, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_substruct" });
  dds_dynamic_type_add_member (&dsubstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "submember_uint32"));

  dds_dynamic_type_t dstruct = dds_dynamic_type_create (pp, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "member_uint16"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsubstruct, "member_struct"));

  dds_typeinfo_t *type_info;
  dds_return_t ret = dds_dynamic_type_register (&dstruct, &type_info);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *descriptor;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, pp, type_info, 0, &descriptor);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_entity_t topic = dds_create_topic (pp, descriptor, topic_name, NULL, NULL);
  CU_ASSERT_FATAL (topic >= 0);
  dds_entity_t writer = dds_create_writer (pp, topic, NULL, NULL);
  CU_ASSERT_FATAL (writer >= 0);

  *type_id = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info));
  dds_free_typeinfo (type_info);
  dds_delete_topic_descriptor (descriptor);
  dds_dynamic_type_unref (&dstruct);
}

CU_Test (ddsc_dynamic_type, existing, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_return_t ret;
  char topic_name[100];
  create_unique_topic_name ("ddsc_dynamic_type", topic_name, sizeof (topic_name));

  // Create participant2 with writer
  dds_entity_t domain2 = dds_create_domain (1, "<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>");
  CU_ASSERT_FATAL (domain2 >= 0);
  dds_entity_t participant2 = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL (participant2 >= 0);

  ddsi_typeid_t *type_id, *type_id2;
  create_type_topic_wr (participant2, topic_name, &type_id2);

  // Read DCPS Publication and find participant2 writer
  dds_entity_t pub_rd = dds_create_reader (participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL (pub_rd >= 0);
  ret = dds_set_status_mask (pub_rd, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_FATAL (ret == 0);
  dds_entity_t ws = dds_create_waitset (participant);
  CU_ASSERT_FATAL (ws >= 0);
  ret = dds_waitset_attach (ws, pub_rd, 0);
  CU_ASSERT_FATAL (ret == 0);

  bool done = false;
  while (!done)
  {
    ret = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    CU_ASSERT_FATAL (ret >= 0);

    void *samples[1];
    dds_sample_info_t si;
    samples[0] = NULL;
    while (!done && dds_take (pub_rd, samples, &si, 1, 1) == 1)
    {
      const dds_builtintopic_endpoint_t *sample = samples[0];
      done = si.valid_data && si.instance_state == DDS_IST_ALIVE && !strcmp (sample->topic_name, topic_name);
    }
    dds_return_loan (pub_rd, samples, 1);
  }

  /* Now that we have discovered the writer from participant2, its types should be
     in the type library in unresolved state (in participant 1 context!). */
  struct ddsi_type *type, *type2;
  struct ddsi_domaingv *gv = get_domaingv (participant);
  type2 = ddsi_type_lookup_locked (gv, type_id2);
  CU_ASSERT_FATAL (type2 != NULL);
  bool resolved = ddsi_type_resolved_locked (gv, type2, DDSI_TYPE_IGNORE_DEPS);
  CU_ASSERT_FATAL (!resolved);

  /* Create the same type for a local writer and confirm that the type
     id is the same and the type is resolved. */
  create_type_topic_wr (participant, topic_name, &type_id);
  CU_ASSERT_FATAL (ddsi_typeid_compare (type_id, type_id2) == 0);
  type = ddsi_type_lookup_locked (gv, type_id);
  CU_ASSERT_FATAL (type != NULL);
  resolved = ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS);
  CU_ASSERT_FATAL (resolved);

  // Clean-up
  ddsi_typeid_fini (type_id);
  ddsrt_free (type_id);
  ddsi_typeid_fini (type_id2);
  ddsrt_free (type_id2);

  dds_delete (domain2);
}

CU_Test (ddsc_dynamic_type, existing_constructing, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct1 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
  dds_dynamic_type_add_member (&dstruct1, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));

  dds_dynamic_type_t dstruct2 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
  dds_dynamic_type_add_member (&dstruct2, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));

  dds_typeinfo_t *type_info1, *type_info2;
  dds_return_t ret;
  ret = dds_dynamic_type_register (&dstruct2, &type_info2);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  ret = dds_dynamic_type_register (&dstruct1, &type_info1);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);

  ddsi_typeid_t *type_id1, *type_id2;
  type_id1 = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info1));
  type_id2 = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info2));
  CU_ASSERT_FATAL (ddsi_typeid_compare (type_id1, type_id2) == 0);

  ddsi_typeid_fini (type_id1);
  ddsrt_free (type_id1);
  ddsi_typeid_fini (type_id2);
  ddsrt_free (type_id2);

  dds_free_typeinfo (type_info1);
  dds_free_typeinfo (type_info2);
  dds_dynamic_type_unref (&dstruct1);
  dds_dynamic_type_unref (&dstruct2);
}
