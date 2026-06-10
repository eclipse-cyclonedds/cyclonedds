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
#include "ddsi__typelib.h"
#include "ddsi__xt_impl.h"
#include "test_util.h"
#include "Space.h"
#include "DynamicTypeTypes.h"

static dds_entity_t domain = 0, participant = 0;

static void dynamic_type_init(void)
{
  domain = dds_create_domain (0, NULL);
  CU_ASSERT_GEQ_FATAL (domain, 0);
  participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (participant, 0);
}

static void dynamic_type_no_recursive_init(void)
{
  domain = dds_create_domain (0, "<Compatibility><AllowRecursiveTypes>false</AllowRecursiveTypes></Compatibility>");
  CU_ASSERT_GEQ_FATAL (domain, 0);
  participant = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (participant, 0);
}

static void dynamic_type_fini(void)
{
  dds_return_t ret = dds_delete (participant);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (domain);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
}

static void do_test (dds_dynamic_type_t *dtype)
{
  dds_return_t ret;
  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *descriptor;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, participant, type_info, 0, &descriptor);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  char topic_name[100];
  create_unique_topic_name ("ddsc_dynamic_type", topic_name, sizeof (topic_name));
  dds_entity_t topic = dds_create_topic (participant, descriptor, topic_name, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (topic, 0);

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
    CU_ASSERT_EQ_FATAL (dstruct.ret, tests[i].ret);
    if (tests[i].ret == DDS_RETCODE_OK)
    {
      dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));
      do_test (&dstruct);
    }
  }
}

static struct ddsi_type * get_ddsi_type (dds_dynamic_type_t *dtype)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  dds_typeinfo_t *type_info;
  dds_return_t ret = dds_dynamic_type_register (dtype, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  const ddsi_typeid_t *type_id = ddsi_typeinfo_complete_typeid (type_info);
  struct ddsi_type *type = ddsi_type_lookup (gv, type_id);
  CU_ASSERT_NEQ_FATAL (type, NULL);
  dds_free_typeinfo (type_info);
  return type;
}

/* Copy of the DDS_DYNAMIC_TYPE_SPEC_PRIM macro, without the explicit cast because
   that causes a build error on MSVC when used in a designated initializer. */
#define TYPE_SPEC_PRIM_NC(p) { .kind = DDS_DYNAMIC_TYPE_KIND_PRIMITIVE, .type.primitive = (p) }

typedef void (*type_init_fn) (dds_dynamic_type_t *);

static void tcr_init_enum (dds_dynamic_type_t *dtype) { dds_dynamic_type_add_enum_literal (dtype, "V1", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false); }
static void tcr_init_bitmask (dds_dynamic_type_t *dtype) { dds_dynamic_type_add_bitmask_field (dtype, "B1", DDS_DYNAMIC_BITMASK_POSITION_AUTO); }
static void tcr_init_struct (dds_dynamic_type_t *dtype) { dds_dynamic_type_add_member (dtype, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_INT32, "s1")); }
static void tcr_init_union (dds_dynamic_type_t *dtype) { dds_dynamic_type_add_member (dtype, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "u1", 1, ((int32_t[]) { 1 }))); }

CU_Test (ddsc_dynamic_type, type_create, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  static const uint32_t bounds[] = { 10 };
  static const struct {
    dds_dynamic_type_descriptor_t desc;
    dds_return_t ret;
    DDS_XTypes_TypeKind xt_type_kind;
    type_init_fn init_fn;
  } tests[] = {
    { { .kind = DDS_DYNAMIC_NONE, .name = "t" }, DDS_RETCODE_BAD_PARAMETER, DDS_XTypes_TK_NONE, NULL },
    { { .kind = DDS_DYNAMIC_BOOLEAN, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_BOOLEAN, NULL },
    { { .kind = DDS_DYNAMIC_BYTE, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_BYTE, NULL },
    { { .kind = DDS_DYNAMIC_INT16, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_INT16, NULL },
    { { .kind = DDS_DYNAMIC_INT32, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_INT32, NULL },
    { { .kind = DDS_DYNAMIC_INT64, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_INT64, NULL },
    { { .kind = DDS_DYNAMIC_UINT16, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_UINT16, NULL },
    { { .kind = DDS_DYNAMIC_UINT32, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_UINT32, NULL },
    { { .kind = DDS_DYNAMIC_UINT64, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_UINT64, NULL },
    { { .kind = DDS_DYNAMIC_FLOAT32, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_FLOAT32, NULL },
    { { .kind = DDS_DYNAMIC_FLOAT64, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_FLOAT64, NULL },
    { { .kind = DDS_DYNAMIC_FLOAT128, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_FLOAT128, NULL },
    { { .kind = DDS_DYNAMIC_INT8, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_INT8, NULL },
    { { .kind = DDS_DYNAMIC_UINT8, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_UINT8, NULL },
    { { .kind = DDS_DYNAMIC_CHAR8, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_CHAR8, NULL },
    { { .kind = DDS_DYNAMIC_CHAR16, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_CHAR16, NULL },
    { { .kind = DDS_DYNAMIC_STRING8, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_STRING8, NULL },
    { { .kind = DDS_DYNAMIC_STRING16, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_STRING16, NULL },
    { { .kind = DDS_DYNAMIC_ENUMERATION, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_ENUM, tcr_init_enum },
    { { .kind = DDS_DYNAMIC_BITMASK, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_BITMASK, tcr_init_bitmask },
    { { .kind = DDS_DYNAMIC_ALIAS, .name = "t", .base_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32) }, DDS_RETCODE_OK, DDS_XTypes_TK_ALIAS, NULL },
    { { .kind = DDS_DYNAMIC_ARRAY, .name = "t", .element_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32), .bounds = bounds, .num_bounds = 1 }, DDS_RETCODE_OK, DDS_XTypes_TK_ARRAY, NULL },
    { { .kind = DDS_DYNAMIC_SEQUENCE, .name = "t", .element_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32) }, DDS_RETCODE_OK, DDS_XTypes_TK_SEQUENCE, NULL },
    { { .kind = DDS_DYNAMIC_MAP, .name = "t" }, DDS_RETCODE_UNSUPPORTED, DDS_XTypes_TK_NONE, NULL },
    { { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" }, DDS_RETCODE_OK, DDS_XTypes_TK_STRUCTURE, tcr_init_struct },
    { { .kind = DDS_DYNAMIC_UNION, .name = "t", .discriminator_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32) }, DDS_RETCODE_OK, DDS_XTypes_TK_UNION, tcr_init_union },
    { { .kind = DDS_DYNAMIC_BITSET, .name = "t" }, DDS_RETCODE_UNSUPPORTED, DDS_XTypes_TK_NONE, NULL }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_dynamic_type_t dtype = dds_dynamic_type_create (participant, tests[i].desc);
    tprintf("create type kind %u, return code %d\n", tests[i].desc.kind, dtype.ret);
    CU_ASSERT_EQ_FATAL (dtype.ret, tests[i].ret);
    if (tests[i].ret == DDS_RETCODE_OK)
    {
      dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });

      // add primitive type using macro
      if (tests[i].desc.kind <= DDS_DYNAMIC_CHAR16)
        dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(tests[i].desc.kind, "mp"));

      // add elements for types that need at least one
      if (tests[i].init_fn != NULL)
        tests[i].init_fn (&dtype);

      // add member, also re-add for primitives
      dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dtype, "m"));

      // check that member(s) have expected type
      struct ddsi_type *type = get_ddsi_type (&dstruct);
      CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[0].type->xt._d, tests[i].xt_type_kind);
      if (tests[i].desc.kind <= DDS_DYNAMIC_CHAR16)
        CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[1].type->xt._d, tests[i].xt_type_kind);
      dds_dynamic_type_unref (&dstruct);
    }
  }
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
  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.length, 4);
  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[0].id, ddsi_dynamic_type_member_hashid ("m0"));
  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[1].id, ddsi_dynamic_type_member_hashid ("m1"));
  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[2].id, 123);
  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[3].id, ddsi_dynamic_type_member_hashid ("m3"));

  dds_dynamic_type_unref (&dstruct);
}

CU_Test (ddsc_dynamic_type, extensibility_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_return_t ret;

  // Type parameter NULL
  ret = dds_dynamic_type_set_extensibility (NULL, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Invalid type
  dds_dynamic_type_t dbool = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BOOLEAN });
  ret = dds_dynamic_type_set_extensibility (&dbool, DDS_DYNAMIC_TYPE_EXT_FINAL);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbool);

  // Invalid extensibility value
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  union { enum dds_dynamic_type_extensibility dte; int i; } invalid_dte = { .i = 99 };
  ret = dds_dynamic_type_set_extensibility (&dstruct, invalid_dte.dte);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Type may not have members
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  ret = dds_dynamic_type_set_extensibility (&dstruct, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dstruct);

  // Type must be in constructing state
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  (void) get_ddsi_type (&dstruct);
  ret = dds_dynamic_type_set_extensibility (&dstruct, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
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
    CU_ASSERT_NEQ_FATAL (type->xt._u.structure.flags & exp_xt_ext, 0);

    dds_dynamic_type_unref (&dstruct);
  }
}

CU_Test (ddsc_dynamic_type, bit_bound, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_return_t ret;

  // Type parameter NULL
  ret = dds_dynamic_type_set_bit_bound (NULL, 16);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Invalid type kind
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_set_bit_bound (NULL, 16);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Invalid bit-bound value
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 0);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 65);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  ret = dds_dynamic_type_set_bit_bound (&denum, 0);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  ret = dds_dynamic_type_set_bit_bound (&denum, 33);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);
  dds_dynamic_type_unref (&denum);

  // Type may not have members
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 1);
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 16);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dbitmask);

  // Type must be in constructing state
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 1);
  (void) get_ddsi_type (&dbitmask);
  ret = dds_dynamic_type_set_bit_bound (&dbitmask, 16);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_PRECONDITION_NOT_MET);
  dds_dynamic_type_unref (&dbitmask);
}

CU_Test (ddsc_dynamic_type, bitmask, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_return_t ret = dds_dynamic_type_set_bit_bound (&dbitmask, 16);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_auto0", DDS_DYNAMIC_BITMASK_POSITION_AUTO);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_auto1", DDS_DYNAMIC_BITMASK_POSITION_AUTO);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_10", 10);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b_5", 5);

  struct ddsi_type *type = get_ddsi_type (&dbitmask);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bit_bound, 16);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bitflags.length, 4);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bitflags.seq[0].position, 0);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bitflags.seq[1].position, 1);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bitflags.seq[2].position, 10);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bitflags.seq[3].position, 5);

  dds_dynamic_type_unref (&dbitmask);
}

CU_Test (ddsc_dynamic_type, bitmask_field_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  // Bitmask type NULL
  dds_return_t ret = dds_dynamic_type_add_bitmask_field (NULL, "b1", 1);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Name property missing
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  ret = dds_dynamic_type_add_bitmask_field (&dbitmask, "", 1);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);

  // Position in use
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 1);
  ret = dds_dynamic_type_add_bitmask_field (&dbitmask, "b2", 1);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);

  // Invalid position for bit-bound
  dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "b" });
  dds_dynamic_type_set_bit_bound (&dbitmask, 2);
  ret = dds_dynamic_type_add_bitmask_field (&dbitmask, "b1", 2);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dbitmask);
}

CU_Test (ddsc_dynamic_type, enum_type, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_return_t ret = dds_dynamic_type_set_bit_bound (&denum, 31);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_dynamic_type_add_enum_literal (&denum, "e_auto0", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false);
  dds_dynamic_type_add_enum_literal (&denum, "e_auto1", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, true);
  dds_dynamic_type_add_enum_literal (&denum, "e_31", DDS_DYNAMIC_ENUM_LITERAL_VALUE ((1u << 31) - 1), false);
  dds_dynamic_type_add_enum_literal (&denum, "e_2", DDS_DYNAMIC_ENUM_LITERAL_VALUE (2), false);

  struct ddsi_type *type = get_ddsi_type (&denum);
  CU_ASSERT_EQ_FATAL (type->xt._u.bitmask.bit_bound, 31);
  CU_ASSERT_EQ_FATAL (type->xt._u.enum_type.literals.length, 4);

  CU_ASSERT_EQ_FATAL (type->xt._u.enum_type.literals.seq[0].value, 0);
  CU_ASSERT_FATAL (!(type->xt._u.enum_type.literals.seq[0].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQ_FATAL (type->xt._u.enum_type.literals.seq[1].value, 1);
  CU_ASSERT_NEQ_FATAL (type->xt._u.enum_type.literals.seq[1].flags & DDS_XTypes_IS_DEFAULT, 0);

  CU_ASSERT_EQ_FATAL (type->xt._u.enum_type.literals.seq[2].value, (1u << 31) - 1);
  CU_ASSERT_FATAL (!(type->xt._u.enum_type.literals.seq[2].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQ_FATAL (type->xt._u.enum_type.literals.seq[3].value, 2);
  CU_ASSERT_FATAL (!(type->xt._u.enum_type.literals.seq[3].flags & DDS_XTypes_IS_DEFAULT));

  dds_dynamic_type_unref (&denum);
}

CU_Test (ddsc_dynamic_type, enum_literal_invalid, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  // Enum type NULL
  dds_return_t ret = dds_dynamic_type_add_enum_literal (NULL, "b1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);

  // Name property missing
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  ret = dds_dynamic_type_add_enum_literal (&denum, "", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Value in use
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e2", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Name in use
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), false);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (2), false);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Multiple default values
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (1), true);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e2", DDS_DYNAMIC_ENUM_LITERAL_VALUE (2), true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);

  // Invalid value for bit-bound
  denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "e" });
  dds_dynamic_type_set_bit_bound (&denum, 2);
  ret = dds_dynamic_type_add_enum_literal (&denum, "e1", DDS_DYNAMIC_ENUM_LITERAL_VALUE (4), false);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&denum);
}

CU_Test (ddsc_dynamic_type, struct_member_prop, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_set_autoid (&dstruct, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m2"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m3"));

  dds_return_t ret = dds_dynamic_member_set_key (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_external (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_member_set_hashid (&dstruct, ddsi_dynamic_type_member_hashid ("m2"), "m2_name");
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  // Because of the set_hashid, from this point the member has a different id
  ret = dds_dynamic_member_set_must_understand (&dstruct, ddsi_dynamic_type_member_hashid ("m2_name"), true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  // Optional and key can't be set to the same member
  ret = dds_dynamic_member_set_optional (&dstruct, ddsi_dynamic_type_member_hashid ("m3"), true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_type *type = get_ddsi_type (&dstruct);
  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.length, 3);

  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[0].id, ddsi_dynamic_type_member_hashid ("m1"));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_KEY));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_OPTIONAL));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[0].flags & DDS_XTypes_IS_MUST_UNDERSTAND));

  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[1].id, ddsi_dynamic_type_member_hashid ("m2_name"));
  CU_ASSERT_NEQ_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_KEY, 0);
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_OPTIONAL));
  CU_ASSERT_NEQ_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_EXTERNAL, 0);
  CU_ASSERT_NEQ_FATAL (type->xt._u.structure.members.seq[1].flags & DDS_XTypes_IS_MUST_UNDERSTAND, 0);

  CU_ASSERT_EQ_FATAL (type->xt._u.structure.members.seq[2].id, ddsi_dynamic_type_member_hashid ("m3"));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[2].flags & DDS_XTypes_IS_KEY));
  CU_ASSERT_NEQ_FATAL (type->xt._u.structure.members.seq[2].flags & DDS_XTypes_IS_OPTIONAL, 0);
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[2].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_FATAL (!(type->xt._u.structure.members.seq[2].flags & DDS_XTypes_IS_MUST_UNDERSTAND));

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
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Empty member name
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, ""));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dstruct);

  // Non-primitive type member, re-used member id
  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "s" });
  dds_dynamic_type_add_member (&dsubstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "s1"));

  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID_PRIM(DDS_DYNAMIC_INT32, "m1", 1));
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID(dsubstruct, "m2", 1));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
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
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  // Because of the set_hashid, from this point the member has a different id
  ret = dds_dynamic_member_set_external (&dunion, ddsi_dynamic_type_member_hashid ("m2_name"), true);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_type *type = get_ddsi_type (&dunion);
  CU_ASSERT_EQ_FATAL (type->xt._u.union_type.members.length, 3);

  CU_ASSERT_EQ_FATAL (type->xt._u.union_type.members.seq[0].id, ddsi_dynamic_type_member_hashid ("m1"));
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[0].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[0].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQ_FATAL (type->xt._u.union_type.members.seq[1].id, ddsi_dynamic_type_member_hashid ("m2_name"));
  CU_ASSERT_NEQ_FATAL (type->xt._u.union_type.members.seq[1].flags & DDS_XTypes_IS_EXTERNAL, 0);
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[1].flags & DDS_XTypes_IS_DEFAULT));

  CU_ASSERT_EQ_FATAL (type->xt._u.union_type.members.seq[2].id, ddsi_dynamic_type_member_hashid ("md"));
  CU_ASSERT_FATAL (!(type->xt._u.union_type.members.seq[2].flags & DDS_XTypes_IS_EXTERNAL));
  CU_ASSERT_NEQ_FATAL (type->xt._u.union_type.members.seq[2].flags & DDS_XTypes_IS_DEFAULT, 0);

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
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);

  // Re-used label
  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m1", 1, ((int32_t[]) { 1 })));
  ret = dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m2", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);

  // Multiple default
  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(DDS_DYNAMIC_INT32, "m1"));
  ret = dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(DDS_DYNAMIC_INT32, "m2"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);

  // Non-primitive type member, re-used label
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "m1"));

  dunion = dds_dynamic_type_create (participant, desc);
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER(dstruct, "m1", 1, ((int32_t[]) { 1 })));
  ret = dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT16, "m2", 1, ((int32_t[]) { 1 })));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);
}

CU_Test (ddsc_dynamic_type, no_members, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_typeinfo_t *type_info;
  dds_return_t ret;

  // Struct without members
  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_register (&dstruct, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dstruct);

  // Struct with basetype without members
  dds_dynamic_type_t dbasestruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "b" });
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t", .base_type = DDS_DYNAMIC_TYPE_SPEC (dbasestruct) });
  CU_ASSERT_EQ_FATAL (dstruct.ret, DDS_RETCODE_OK);
  dds_dynamic_type_unref (&dstruct);

  // Struct with substruct without members
  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "s" });
  dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "t" });
  ret = dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsubstruct, "m1"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_dynamic_type_unref (&dstruct);

  // Union without members
  dds_dynamic_type_t dunion = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t)
      { .kind = DDS_DYNAMIC_UNION, .name = "u", .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32) });
  ret = dds_dynamic_type_register (&dunion, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_BAD_PARAMETER);
  dds_dynamic_type_unref (&dunion);
}

#ifdef DDS_HAS_TYPE_DISCOVERY
static void create_type_topic_wr (dds_entity_t pp, const char *topic_name, ddsi_typeid_t **type_id)
{
  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (pp, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_substruct" });
  dds_dynamic_type_add_member (&dsubstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "submember_uint32"));

  dds_dynamic_type_t dstruct = dds_dynamic_type_create (pp, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "member_uint16"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsubstruct, "member_struct"));

  dds_typeinfo_t *type_info;
  dds_return_t ret = dds_dynamic_type_register (&dstruct, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_topic_descriptor_t *descriptor;
  ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, pp, type_info, 0, &descriptor);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_entity_t topic = dds_create_topic (pp, descriptor, topic_name, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (topic, 0);
  dds_entity_t writer = dds_create_writer (pp, topic, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (writer, 0);

  *type_id = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info));
  dds_free_typeinfo (type_info);
  dds_delete_topic_descriptor (descriptor);
  dds_dynamic_type_unref (&dstruct);
}
#endif /* DDS_HAS_TYPE_DISCOVERY */

CU_Test (ddsc_dynamic_type, existing, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
#ifdef DDS_HAS_TYPE_DISCOVERY
  dds_return_t ret;
  char topic_name[100];
  create_unique_topic_name ("ddsc_dynamic_type", topic_name, sizeof (topic_name));

  // Create participant2 with writer
  dds_entity_t domain2 = dds_create_domain (1, "<Discovery><ExternalDomainId>0</ExternalDomainId></Discovery>");
  CU_ASSERT_GEQ_FATAL (domain2, 0);
  dds_entity_t participant2 = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (participant2, 0);

  ddsi_typeid_t *type_id, *type_id2;
  create_type_topic_wr (participant2, topic_name, &type_id2);

  // Read DCPS Publication and find participant2 writer
  dds_entity_t pub_rd = dds_create_reader (participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (pub_rd, 0);
  ret = dds_set_status_mask (pub_rd, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_EQ_FATAL (ret, 0);
  dds_entity_t ws = dds_create_waitset (participant);
  CU_ASSERT_GEQ_FATAL (ws, 0);
  ret = dds_waitset_attach (ws, pub_rd, 0);
  CU_ASSERT_EQ_FATAL (ret, 0);

  bool done = false;
  while (!done)
  {
    ret = dds_waitset_wait (ws, NULL, 0, DDS_INFINITY);
    CU_ASSERT_GEQ_FATAL (ret, 0);

    void *samples[1];
    dds_sample_info_t si;
    samples[0] = NULL;
    while (!done && dds_take (pub_rd, samples, &si, 1, 1) == 1)
    {
      const dds_builtintopic_endpoint_t *sample = samples[0];
      done = si.valid_data && si.instance_state == DDS_ALIVE_INSTANCE_STATE && !strcmp (sample->topic_name, topic_name);
    }
    dds_return_loan (pub_rd, samples, 1);
  }

  /* Now that we have discovered the writer from participant2, its types should be
     in the type library in unresolved state (in participant 1 context!). */
  struct ddsi_type *type, *type2;
  struct ddsi_domaingv *gv = get_domaingv (participant);
  type2 = ddsi_type_lookup_locked (gv, type_id2);
  CU_ASSERT_NEQ_FATAL (type2, NULL);
  bool resolved = ddsi_type_resolved_locked (gv, type2, DDSI_TYPE_IGNORE_DEPS);
  CU_ASSERT_FATAL (!resolved);

  /* Create the same type for a local writer and confirm that the type
     id is the same and the type is resolved. */
  create_type_topic_wr (participant, topic_name, &type_id);
  CU_ASSERT_EQ_FATAL (ddsi_typeid_compare (type_id, type_id2), 0);
  type = ddsi_type_lookup_locked (gv, type_id);
  CU_ASSERT_NEQ_FATAL (type, NULL);
  resolved = ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS);
  CU_ASSERT_FATAL (resolved);

  // Clean-up
  ddsi_typeid_fini (type_id);
  ddsrt_free (type_id);
  ddsi_typeid_fini (type_id2);
  ddsrt_free (type_id2);

  dds_delete (domain2);
#endif /* DDS_HAS_TYPE_DISCOVERY */
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
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  ret = dds_dynamic_type_register (&dstruct1, &type_info1);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  ddsi_typeid_t *type_id1, *type_id2;
  type_id1 = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info1));
  type_id2 = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info2));
  CU_ASSERT_EQ_FATAL (ddsi_typeid_compare (type_id1, type_id2), 0);

  ddsi_typeid_fini (type_id1);
  ddsrt_free (type_id1);
  ddsi_typeid_fini (type_id2);
  ddsrt_free (type_id2);

  dds_free_typeinfo (type_info1);
  dds_free_typeinfo (type_info2);
  dds_dynamic_type_unref (&dstruct1);
  dds_dynamic_type_unref (&dstruct2);
}

CU_Test (ddsc_dynamic_type, existing_nested_constructing, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dstruct_existing = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_nested_struct" });
  dds_dynamic_type_add_member (&dstruct_existing, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));

  dds_typeinfo_t *type_info_existing;
  dds_return_t ret = dds_dynamic_type_register (&dstruct_existing, &type_info_existing);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dstruct_nested = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_nested_struct" });
  dds_dynamic_type_add_member (&dstruct_nested, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT32, "member_uint32"));

  dds_dynamic_type_t dstruct_parent = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_parent_struct" });
  dds_dynamic_type_add_member (&dstruct_parent, DDS_DYNAMIC_MEMBER (dds_dynamic_type_ref (&dstruct_nested), "member_struct"));

  dds_typeinfo_t *type_info_parent;
  ret = dds_dynamic_type_register (&dstruct_parent, &type_info_parent);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info_nested;
  ret = dds_dynamic_type_register (&dstruct_nested, &type_info_nested);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (ddsi_typeid_compare (ddsi_typeinfo_complete_typeid (type_info_existing), ddsi_typeinfo_complete_typeid (type_info_nested)), 0);

  dds_free_typeinfo (type_info_existing);
  dds_free_typeinfo (type_info_parent);
  dds_free_typeinfo (type_info_nested);
  dds_dynamic_type_unref (&dstruct_existing);
  dds_dynamic_type_unref (&dstruct_parent);
  dds_dynamic_type_unref (&dstruct_nested);
}

CU_Test (ddsc_dynamic_type, recursive_struct_disabled, .init = dynamic_type_no_recursive_init, .fini = dynamic_type_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  CU_ASSERT_EQ_FATAL (gv->config.allow_recursive_types, 0);

  dds_dynamic_type_t dnode = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "NodeDisabled" });
  CU_ASSERT_EQ_FATAL (dnode.ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dseq = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_SEQUENCE,
    .name = "NodeDisabledSeq",
    .element_type = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_ref (&dnode))
  });
  CU_ASSERT_EQ_FATAL (dseq.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&dnode, DDS_DYNAMIC_MEMBER_PRIM (DDS_DYNAMIC_INT32, "value"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dnode, DDS_DYNAMIC_MEMBER (dseq, "children"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info = NULL;
  ret = dds_dynamic_type_register (&dnode, &type_info);
  CU_ASSERT_NEQ (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ (type_info, NULL);

  dds_dynamic_type_unref (&dnode);
}

CU_Test (ddsc_dynamic_type, recursive_struct_disabled_nested_cycle, .init = dynamic_type_no_recursive_init, .fini = dynamic_type_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  CU_ASSERT_EQ_FATAL (gv->config.allow_recursive_types, 0);

  dds_dynamic_type_t droot = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "RootDisabled" });
  CU_ASSERT_EQ_FATAL (droot.ret, DDS_RETCODE_OK);
  dds_dynamic_type_t da = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "ADisabled" });
  CU_ASSERT_EQ_FATAL (da.ret, DDS_RETCODE_OK);
  dds_dynamic_type_t da_ref = dds_dynamic_type_ref (&da);
  CU_ASSERT_EQ_FATAL (da_ref.ret, DDS_RETCODE_OK);
  dds_dynamic_type_t db = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "BDisabled" });
  CU_ASSERT_EQ_FATAL (db.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&db, DDS_DYNAMIC_MEMBER (dds_dynamic_type_ref (&da), "a"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dseqb = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_SEQUENCE,
    .name = "BDisabledSeq",
    .element_type = DDS_DYNAMIC_TYPE_SPEC (db)
  });
  CU_ASSERT_EQ_FATAL (dseqb.ret, DDS_RETCODE_OK);

  ret = dds_dynamic_type_add_member (&da, DDS_DYNAMIC_MEMBER (dseqb, "bs"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&droot, DDS_DYNAMIC_MEMBER (da, "a"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info = NULL;
  ret = dds_dynamic_type_register (&droot, &type_info);
  CU_ASSERT_NEQ (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ (droot.ret, ret);
  CU_ASSERT_EQ (type_info, NULL);

  ret = dds_dynamic_type_register (&da_ref, &type_info);
  CU_ASSERT_NEQ (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ (da_ref.ret, ret);
  CU_ASSERT_EQ (type_info, NULL);

  dds_dynamic_type_unref (&droot);
  dds_dynamic_type_unref (&da_ref);
}

static void recursive_import_expect (const ddsi_typeinfo_t *type_info, const ddsi_typemap_t *type_map, dds_domainid_t domainid, dds_return_t expected)
{
  dds_entity_t import_domain = dds_create_domain (domainid, NULL);
  CU_ASSERT_GEQ_FATAL (import_domain, 0);
  dds_entity_t import_participant = dds_create_participant (domainid, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (import_participant, 0);

  struct ddsi_domaingv *import_gv = get_domaingv (import_participant);
  struct ddsi_type *imported_minimal = NULL, *imported_complete = NULL;
  dds_return_t ret = ddsi_type_add (import_gv, &imported_minimal, &imported_complete, type_info, type_map);
  CU_ASSERT_EQ_FATAL (ret, expected);
  ddsrt_mutex_lock (&import_gv->typelib_lock);
  if (ret == DDS_RETCODE_OK)
  {
    if (imported_minimal)
      ddsi_type_unref_locked (import_gv, imported_minimal);
    if (imported_complete)
      ddsi_type_unref_locked (import_gv, imported_complete);
  }
  else
  {
    CU_ASSERT_EQ (imported_minimal, NULL);
    CU_ASSERT_EQ (imported_complete, NULL);
    CU_ASSERT_EQ (ddsi_type_lookup_locked (import_gv, ddsi_typeinfo_minimal_typeid (type_info)), NULL);
    CU_ASSERT_EQ (ddsi_type_lookup_locked (import_gv, ddsi_typeinfo_complete_typeid (type_info)), NULL);
  }
  ddsrt_mutex_unlock (&import_gv->typelib_lock);

  ret = dds_delete (import_participant);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (import_domain);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
}

static void mutate_recursive_scc_typeobject (ddsi_typemap_t *type_map)
{
  DDS_XTypes_TypeIdentifierTypeObjectPair *min_pair = &type_map->x.identifier_object_pair_minimal._buffer[0];
  CU_ASSERT_EQ_FATAL (min_pair->type_identifier._d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (min_pair->type_object._d, DDS_XTypes_EK_MINIMAL);
  CU_ASSERT_EQ_FATAL (min_pair->type_object._u.minimal._d, DDS_XTypes_TK_STRUCTURE);
  CU_ASSERT_FATAL (min_pair->type_object._u.minimal._u.struct_type.member_seq._length > 0);
  min_pair->type_object._u.minimal._u.struct_type.member_seq._buffer[0].detail.name_hash[0] ^= 1;

  DDS_XTypes_TypeIdentifierTypeObjectPair *complete_pair = &type_map->x.identifier_object_pair_complete._buffer[0];
  CU_ASSERT_EQ_FATAL (complete_pair->type_identifier._d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (complete_pair->type_object._d, DDS_XTypes_EK_COMPLETE);
  CU_ASSERT_EQ_FATAL (complete_pair->type_object._u.complete._d, DDS_XTypes_TK_STRUCTURE);
  complete_pair->type_object._u.complete._u.struct_type.header.detail.type_name[0] ^= 1;
}

static void mutate_recursive_complete_scc_typeobject (ddsi_typemap_t *type_map)
{
  DDS_XTypes_TypeIdentifierTypeObjectPair *complete_pair = &type_map->x.identifier_object_pair_complete._buffer[0];
  CU_ASSERT_EQ_FATAL (complete_pair->type_identifier._d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (complete_pair->type_object._d, DDS_XTypes_EK_COMPLETE);
  CU_ASSERT_EQ_FATAL (complete_pair->type_object._u.complete._d, DDS_XTypes_TK_STRUCTURE);
  complete_pair->type_object._u.complete._u.struct_type.header.detail.type_name[0] ^= 1;
}

static void mutate_recursive_scc_pair_index_out_of_bounds (ddsi_typemap_t *type_map)
{
  DDS_XTypes_TypeIdentifier *min_id = &type_map->x.identifier_object_pair_minimal._buffer[0].type_identifier;
  DDS_XTypes_TypeIdentifier *complete_id = &type_map->x.identifier_object_pair_complete._buffer[0].type_identifier;
  CU_ASSERT_EQ_FATAL (min_id->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (complete_id->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  min_id->_u.sc_component_id.scc_index = min_id->_u.sc_component_id.scc_length + 1;
  complete_id->_u.sc_component_id.scc_index = complete_id->_u.sc_component_id.scc_length + 1;
}

static void mutate_recursive_scc_length (ddsi_typeinfo_t *type_info, ddsi_typemap_t *type_map, int32_t length)
{
  DDS_XTypes_TypeIdentifier *min_ti = &type_info->x.minimal.typeid_with_size.type_id;
  DDS_XTypes_TypeIdentifier *complete_ti = &type_info->x.complete.typeid_with_size.type_id;
  DDS_XTypes_TypeIdentifier *min_map = &type_map->x.identifier_object_pair_minimal._buffer[0].type_identifier;
  DDS_XTypes_TypeIdentifier *complete_map = &type_map->x.identifier_object_pair_complete._buffer[0].type_identifier;
  CU_ASSERT_EQ_FATAL (min_ti->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (complete_ti->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (min_map->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (complete_map->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  min_ti->_u.sc_component_id.scc_length = length;
  complete_ti->_u.sc_component_id.scc_length = length;
  min_map->_u.sc_component_id.scc_length = length;
  complete_map->_u.sc_component_id.scc_length = length;
}

static void duplicate_first_scc_pair (dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *seq)
{
  CU_ASSERT_EQ_FATAL (seq->_length, 1);
  CU_ASSERT_EQ_FATAL (seq->_maximum, 1);
  CU_ASSERT_NEQ_FATAL (seq->_buffer, NULL);
  CU_ASSERT_EQ_FATAL (seq->_buffer[0].type_identifier._d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);

  DDS_XTypes_TypeIdentifierTypeObjectPair *buf = ddsrt_calloc (2, sizeof (*buf));
  CU_ASSERT_NEQ_FATAL (buf, NULL);
  buf[0] = seq->_buffer[0];
  buf[1].type_identifier = buf[0].type_identifier;
  buf[1].type_object._d = buf[0].type_object._d;
  ddsrt_free (seq->_buffer);
  seq->_buffer = buf;
  seq->_length = 2;
  seq->_maximum = 2;
  seq->_release = true;
}

static void mutate_recursive_scc_pair_duplicate_index (ddsi_typeinfo_t *type_info, ddsi_typemap_t *type_map)
{
  mutate_recursive_scc_length (type_info, type_map, 2);
  duplicate_first_scc_pair (&type_map->x.identifier_object_pair_minimal);
  duplicate_first_scc_pair (&type_map->x.identifier_object_pair_complete);
}

static void mutate_recursive_scc_wrong_equivalence_kind (ddsi_typemap_t *type_map)
{
  DDS_XTypes_TypeIdentifier *min_id = &type_map->x.identifier_object_pair_minimal._buffer[0].type_identifier;
  DDS_XTypes_TypeIdentifier *complete_id = &type_map->x.identifier_object_pair_complete._buffer[0].type_identifier;
  CU_ASSERT_EQ_FATAL (min_id->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (complete_id->_d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_EQ_FATAL (min_id->_u.sc_component_id.sc_component_id._d, DDS_XTypes_EK_MINIMAL);
  CU_ASSERT_EQ_FATAL (complete_id->_u.sc_component_id.sc_component_id._d, DDS_XTypes_EK_COMPLETE);
  min_id->_u.sc_component_id.sc_component_id._d = DDS_XTypes_EK_COMPLETE;
  complete_id->_u.sc_component_id.sc_component_id._d = DDS_XTypes_EK_MINIMAL;
}

CU_Test (ddsc_dynamic_type, recursive_struct, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dnode = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "Node" });
  CU_ASSERT_EQ_FATAL (dnode.ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dseq = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_SEQUENCE,
    .name = "NodeSeq",
    .element_type = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_ref (&dnode))
  });
  CU_ASSERT_EQ_FATAL (dseq.ret, DDS_RETCODE_OK);

  dds_return_t ret = dds_dynamic_type_add_member (&dnode, DDS_DYNAMIC_MEMBER_PRIM (DDS_DYNAMIC_INT32, "value"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&dnode, DDS_DYNAMIC_MEMBER (dseq, "children"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_typeinfo_t *type_info;
  ret = dds_dynamic_type_register (&dnode, &type_info);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_typeinfo *dti = (struct ddsi_typeinfo *) type_info;
  const ddsi_typeid_t *type_id = ddsi_typeinfo_complete_typeid (dti);
  CU_ASSERT_EQ_FATAL (type_id->x._d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  const ddsi_typeid_t *minimal_type_id = ddsi_typeinfo_minimal_typeid (dti);
  CU_ASSERT_EQ_FATAL (minimal_type_id->x._d, DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  CU_ASSERT_NEQ_FATAL (dnode.x[1], NULL);

  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type *type = ddsi_type_lookup (gv, type_id);
  CU_ASSERT_NEQ_FATAL (type, NULL);
  CU_ASSERT_EQ_FATAL (type->state, DDSI_TYPE_RESOLVED);

  unsigned char *typemap_ser;
  uint32_t typemap_ser_sz;
  ret = ddsi_type_get_typemap_ser (gv, type, &typemap_ser, &typemap_ser_sz);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  unsigned char *typeinfo_ser;
  uint32_t typeinfo_ser_sz;
  ret = ddsi_type_get_typeinfo_ser (gv, type, &typeinfo_ser, &typeinfo_ser_sz);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  ddsi_typemap_t *type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (type_map, NULL);

  dds_entity_t import_domain = dds_create_domain (1, NULL);
  CU_ASSERT_GEQ_FATAL (import_domain, 0);
  dds_entity_t import_participant = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_GEQ_FATAL (import_participant, 0);

  struct ddsi_domaingv *import_gv = get_domaingv (import_participant);
  struct ddsi_type *unresolved_minimal = NULL;
  ddsrt_mutex_lock (&import_gv->typelib_lock);
  ret = ddsi_type_ref_id_locked (import_gv, &unresolved_minimal, minimal_type_id);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (unresolved_minimal->state, DDSI_TYPE_UNRESOLVED);
  ddsrt_mutex_unlock (&import_gv->typelib_lock);

  struct ddsi_type *imported_minimal = NULL, *imported_complete = NULL;
  ret = ddsi_type_add (import_gv, &imported_minimal, &imported_complete, dti, type_map);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ddsi_type_resolved (import_gv, imported_minimal, DDSI_TYPE_IGNORE_DEPS));
  CU_ASSERT_FATAL (ddsi_type_resolved (import_gv, imported_complete, DDSI_TYPE_IGNORE_DEPS));
  CU_ASSERT_NEQ_FATAL (imported_minimal->scc, NULL);
  CU_ASSERT_EQ_FATAL (imported_minimal->scc->n_wire_types, 1);
  CU_ASSERT_FATAL (imported_minimal->scc->types[0] == imported_minimal);
  CU_ASSERT_EQ_FATAL (imported_minimal->scc->n_types, 2);
  CU_ASSERT_EQ_FATAL (imported_minimal->xt._u.structure.members.seq[1].type->xt._d, DDS_XTypes_TK_SEQUENCE);
  CU_ASSERT_FATAL (imported_minimal->scc->types[1] == imported_minimal->xt._u.structure.members.seq[1].type);
  CU_ASSERT_NEQ_FATAL (imported_complete->scc, NULL);
  CU_ASSERT_EQ_FATAL (imported_complete->scc->n_wire_types, 1);
  CU_ASSERT_FATAL (imported_complete->scc->types[0] == imported_complete);
  CU_ASSERT_EQ_FATAL (imported_complete->scc->n_types, 2);
  CU_ASSERT_EQ_FATAL (imported_complete->xt._u.structure.members.seq[1].type->xt._d, DDS_XTypes_TK_SEQUENCE);
  CU_ASSERT_FATAL (imported_complete->scc->types[1] == imported_complete->xt._u.structure.members.seq[1].type);

  ddsrt_mutex_lock (&import_gv->typelib_lock);
  ddsi_type_unref_locked (import_gv, unresolved_minimal);
  ddsi_type_unref_locked (import_gv, imported_minimal);
  ddsi_type_unref_locked (import_gv, imported_complete);
  ddsrt_mutex_unlock (&import_gv->typelib_lock);

  ret = dds_delete (import_participant);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_delete (import_domain);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  ddsi_typeinfo_t *bad_type_info;
  ddsi_typemap_t *bad_type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_map, NULL);
  mutate_recursive_scc_typeobject (bad_type_map);
  recursive_import_expect (dti, bad_type_map, 2, DDS_RETCODE_BAD_PARAMETER);
  ddsi_typemap_fini (bad_type_map);
  ddsrt_free (bad_type_map);

  bad_type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_map, NULL);
  mutate_recursive_complete_scc_typeobject (bad_type_map);
  recursive_import_expect (dti, bad_type_map, 3, DDS_RETCODE_BAD_PARAMETER);
  ddsi_typemap_fini (bad_type_map);
  ddsrt_free (bad_type_map);

  bad_type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_map, NULL);
  mutate_recursive_scc_pair_index_out_of_bounds (bad_type_map);
  recursive_import_expect (dti, bad_type_map, 4, DDS_RETCODE_BAD_PARAMETER);
  ddsi_typemap_fini (bad_type_map);
  ddsrt_free (bad_type_map);

  bad_type_info = ddsi_typeinfo_deser (typeinfo_ser, typeinfo_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_info, NULL);
  bad_type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_map, NULL);
  mutate_recursive_scc_pair_duplicate_index (bad_type_info, bad_type_map);
  recursive_import_expect (bad_type_info, bad_type_map, 5, DDS_RETCODE_BAD_PARAMETER);
  ddsi_typeinfo_fini (bad_type_info);
  ddsrt_free (bad_type_info);
  ddsi_typemap_fini (bad_type_map);
  ddsrt_free (bad_type_map);

  bad_type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_map, NULL);
  mutate_recursive_scc_wrong_equivalence_kind (bad_type_map);
  recursive_import_expect (dti, bad_type_map, 6, DDS_RETCODE_BAD_PARAMETER);
  ddsi_typemap_fini (bad_type_map);
  ddsrt_free (bad_type_map);

  bad_type_info = ddsi_typeinfo_deser (typeinfo_ser, typeinfo_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_info, NULL);
  bad_type_map = ddsi_typemap_deser (typemap_ser, typemap_ser_sz);
  CU_ASSERT_NEQ_FATAL (bad_type_map, NULL);
  mutate_recursive_scc_length (bad_type_info, bad_type_map, 2);
  recursive_import_expect (bad_type_info, bad_type_map, 7, DDS_RETCODE_BAD_PARAMETER);
  ddsi_typeinfo_fini (bad_type_info);
  ddsrt_free (bad_type_info);
  ddsi_typemap_fini (bad_type_map);
  ddsrt_free (bad_type_map);

  ddsi_typemap_fini (type_map);
  ddsrt_free (type_map);
  ddsrt_free (typeinfo_ser);
  ddsrt_free (typemap_ser);

  dds_free_typeinfo (type_info);
  dds_dynamic_type_unref (&dnode);
}

CU_Test (ddsc_dynamic_type, recursive_two_slot_duplicate_sequence_register_all_reverse, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t da = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "DupSeqAllRevA" });
  CU_ASSERT_EQ_FATAL (da.ret, DDS_RETCODE_OK);
  dds_return_t ret = dds_dynamic_type_set_extensibility (&da, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dseq_a1 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_SEQUENCE,
    .name = "dds_sequence_nonBasic",
    .element_type = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_ref (&da))
  });
  CU_ASSERT_EQ_FATAL (dseq_a1.ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&da, DDS_DYNAMIC_MEMBER (dseq_a1, "as"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t db = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "DupSeqAllRevB" });
  CU_ASSERT_EQ_FATAL (db.ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_set_extensibility (&db, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_dynamic_type_t dseq_a2 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_SEQUENCE,
    .name = "dds_sequence_nonBasic",
    .element_type = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_ref (&da))
  });
  CU_ASSERT_EQ_FATAL (dseq_a2.ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&db, DDS_DYNAMIC_MEMBER (dseq_a2, "as"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_dynamic_type_t dseq_b = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_SEQUENCE,
    .name = "dds_sequence_nonBasic",
    .element_type = DDS_DYNAMIC_TYPE_SPEC (dds_dynamic_type_ref (&db))
  });
  CU_ASSERT_EQ_FATAL (dseq_b.ret, DDS_RETCODE_OK);
  ret = dds_dynamic_type_add_member (&da, DDS_DYNAMIC_MEMBER (dseq_b, "bs"));
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  /* This registration order reproduces the xmltype/dyntypelib order that exposed the leak. */
  dds_typeinfo_t *type_info_b;
  ret = dds_dynamic_type_register (&db, &type_info_b);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  dds_typeinfo_t *type_info_a;
  ret = dds_dynamic_type_register (&da, &type_info_a);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  dds_free_typeinfo (type_info_a);
  dds_free_typeinfo (type_info_b);
  dds_dynamic_type_unref (&da);
  dds_dynamic_type_unref (&db);
}

CU_Test (ddsc_dynamic_type, type_info, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  dds_dynamic_type_t dsub1 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dsub1" });
  dds_dynamic_type_add_member (&dsub1, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_INT32, "m_int32"));

  dds_dynamic_type_t dsubsub1 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dsubsub1" });
  dds_dynamic_type_add_member (&dsubsub1, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_BOOLEAN, "m_bool"));

  dds_dynamic_type_t dsubsub2 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dsubsub2" });
  dds_dynamic_type_add_member (&dsubsub2, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_BOOLEAN, "m_bool"));

  dds_dynamic_type_t dsub2 = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dsub2" });
  dds_dynamic_type_add_member (&dsub2, DDS_DYNAMIC_MEMBER(dds_dynamic_type_ref (&dsubsub1), "m_subsub1")); // increase ref because type is re-used below
  dds_dynamic_type_add_member (&dsub2, DDS_DYNAMIC_MEMBER(dds_dynamic_type_ref (&dsubsub2), "m_subsub2")); // increase ref because type is re-used below

  dds_dynamic_type_t dseq = dds_dynamic_type_create (participant,
      (dds_dynamic_type_descriptor_t) {
        .kind = DDS_DYNAMIC_SEQUENCE,
        .name = "dseq",
        .element_type = DDS_DYNAMIC_TYPE_SPEC(dds_dynamic_type_ref (&dsub2)),
        .num_bounds = 0
      });

  static const uint32_t bounds[] = { 10 };
  dds_dynamic_type_t darr = dds_dynamic_type_create (participant,
      (dds_dynamic_type_descriptor_t) {
        .kind = DDS_DYNAMIC_ARRAY,
        .name = "darr",
        .element_type = DDS_DYNAMIC_TYPE_SPEC(dds_dynamic_type_ref (&dsub2)),
        .num_bounds = 1,
        .bounds = bounds
      });

  dds_dynamic_type_t dalias = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
      .kind = DDS_DYNAMIC_ALIAS,
      .base_type = DDS_DYNAMIC_TYPE_SPEC(dsubsub1),
      .name = "dalias"
  });

  dds_dynamic_type_t dunion = dds_dynamic_type_create (participant,
      (dds_dynamic_type_descriptor_t) {
        .kind = DDS_DYNAMIC_UNION,
        .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32),
        .name = "dunion"
      });
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER_ID_PRIM(DDS_DYNAMIC_FLOAT64, "um_f", 100 /* has specific member id */, 2, ((int32_t[]) { 9, 10 })));
  dds_dynamic_type_add_member (&dunion, DDS_DYNAMIC_UNION_MEMBER(dds_dynamic_type_ref (&dsub1) /* increase ref because type is re-used */, "um_sub1", 2, ((int32_t[]) { 15, 16 })));

  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "denum" });
  dds_dynamic_type_add_enum_literal (&denum, "DE1", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false);
  dds_dynamic_type_add_enum_literal (&denum, "DE2", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false);

  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dstruct" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_BOOLEAN, "m_bool"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dseq, "m_seq"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(darr, "m_arr"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsub1, "m_sub1"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsub2, "m_sub2"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsubsub2, "m_subsub2"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dalias, "m_alias"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dunion, "m_union"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(denum, "m_enum"));

  dds_typeinfo_t *type_info;
  dds_return_t ret = dds_dynamic_type_register (&dstruct, &type_info);
  if (ret != DDS_RETCODE_OK)
    DDS_FATAL ("dds_dynamic_type_register: %s\n", dds_strretcode (-ret));
  struct ddsi_typeinfo *dti = (struct ddsi_typeinfo *) type_info;

  struct ddsi_domaingv *gv = get_domaingv (participant);
  const ddsi_typeid_t *tid = ddsi_typeinfo_complete_typeid (dti);
  struct ddsi_type *t = ddsi_type_lookup (gv, tid);
  unsigned char *data;
  uint32_t sz;
  ret = ddsi_type_get_typemap_ser (gv, t, &data, &sz);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ddsi_typemap_t *dtypemap = ddsi_typemap_deser (data, sz);
  ddsi_typemap_t *typemap = ddsi_typemap_deser (dstruct_desc.type_mapping.data, dstruct_desc.type_mapping.sz);
  ddsrt_free (data);

  if (!ddsi_typemap_equal (dtypemap, typemap))
    CU_FAIL ("typemap not equal");
  ddsi_typemap_fini (dtypemap);
  ddsi_typemap_fini (typemap);
  ddsrt_free (dtypemap);
  ddsrt_free (typemap);

  ddsi_typeinfo_t *ti = ddsi_typeinfo_deser (dstruct_desc.type_information.data, dstruct_desc.type_information.sz);
  if (!ddsi_typeinfo_equal (dti, ti, DDSI_TYPE_INCLUDE_DEPS))
    CU_FAIL ("typeinfo not equal");

  ddsi_typeinfo_fini (ti);
  ddsrt_free (ti);
  dds_dynamic_type_unref (&dstruct);
  dds_free_typeinfo (type_info);
}

CU_Test (ddsc_dynamic_type, struct_member_key, .init = dynamic_type_init, .fini = dynamic_type_fini)
{
  static const struct {
    dds_dynamic_type_descriptor_t member_type;
    dds_return_t ret;
  } tests[] = {
    { { .kind = DDS_DYNAMIC_INT32, .name = "m" }, DDS_RETCODE_OK },
    { { .kind = DDS_DYNAMIC_SEQUENCE, .name = "m", .element_type = TYPE_SPEC_PRIM_NC(DDS_DYNAMIC_INT32), .num_bounds = 0 }, DDS_RETCODE_OK }
  };

  for (uint32_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    dds_dynamic_type_t dtype = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dstruct" });
    CU_ASSERT_EQ_FATAL (dtype.ret, DDS_RETCODE_OK);
    dds_dynamic_type_t dm = dds_dynamic_type_create (participant, tests[i].member_type);
    dds_dynamic_type_add_member (&dtype, DDS_DYNAMIC_MEMBER(dm, "m"));
    dds_return_t ret = dds_dynamic_member_set_key (&dtype, 0, true);
    CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

    dds_typeinfo_t *type_info;
    ret = dds_dynamic_type_register (&dtype, &type_info);
    CU_ASSERT_EQ_FATAL (ret, tests[i].ret);

    dds_topic_descriptor_t *descriptor;
    ret = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, participant, type_info, 0, &descriptor);
    CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

    char topic_name[100];
    create_unique_topic_name ("ddsc_dynamic_type", topic_name, sizeof (topic_name));
    dds_entity_t topic = dds_create_topic (participant, descriptor, topic_name, NULL, NULL);
    CU_ASSERT_GEQ_FATAL (topic, 0);

    dds_delete_topic_descriptor (descriptor);
    dds_free_typeinfo (type_info);
    dds_dynamic_type_unref (&dtype);
  }
}
