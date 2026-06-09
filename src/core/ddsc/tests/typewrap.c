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

static void typewrap_no_recursive_init (void)
{
  domain = dds_create_domain (0, "<Compatibility><AllowRecursiveTypes>false</AllowRecursiveTypes></Compatibility>");
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

static void init_test_hash_id (ddsi_typeid_t *typeid, uint8_t discriminator)
{
  memset (typeid, 0, sizeof (*typeid));
  typeid->x._d = DDS_XTypes_EK_COMPLETE;
  typeid->x._u.equivalence_hash[0] = discriminator;
}

static void init_struct_dependency_typeobject (
    struct DDS_XTypes_TypeObject *typeobj,
    struct DDS_XTypes_CompleteStructMember *member,
    const char *type_name,
    const char *member_name,
    uint32_t member_id,
    const ddsi_typeid_t *member_typeid)
{
  memset (member, 0, sizeof (*member));
  member->common.member_id = member_id;
  member->common.member_flags = DDS_XTypes_TRY_CONSTRUCT1;
  member->common.member_type_id = member_typeid->x;
  ddsrt_strlcpy (member->detail.name, member_name, sizeof (member->detail.name));

  *typeobj = (struct DDS_XTypes_TypeObject) {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_STRUCTURE,
      ._u.struct_type = {
        .struct_flags = DDS_XTypes_IS_FINAL,
        .member_seq = {
          ._maximum = 1,
          ._length = 1,
          ._buffer = member,
          ._release = false
        }
      }
    }
  };
  ddsrt_strlcpy (typeobj->_u.complete._u.struct_type.header.detail.type_name, type_name,
      sizeof (typeobj->_u.complete._u.struct_type.header.detail.type_name));
}

CU_Test (ddsc_typewrap, recursive_type_validation, .init = typewrap_init, .fini = typewrap_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);

  struct ddsi_type self_alias = {0};
  init_test_hash_id (&self_alias.xt.id, 1);
  self_alias.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  self_alias.xt._d = DDS_XTypes_TK_ALIAS;
  self_alias.xt._u.alias.related_type = &self_alias;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &self_alias), DDS_RETCODE_BAD_PARAMETER);

  struct ddsi_type direct_recursive_struct = {0};
  struct xt_struct_member direct_recursive_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &direct_recursive_struct
  };
  init_test_hash_id (&direct_recursive_struct.xt.id, 2);
  direct_recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  direct_recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  direct_recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  direct_recursive_struct.xt._u.structure.members.length = 1;
  direct_recursive_struct.xt._u.structure.members.seq = &direct_recursive_member;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &direct_recursive_struct), DDS_RETCODE_BAD_PARAMETER);

  struct ddsi_type sequence_recursive_struct = {0}, sequence = {0};
  struct xt_struct_member sequence_recursive_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &sequence
  };
  init_test_hash_id (&sequence_recursive_struct.xt.id, 3);
  sequence_recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  sequence_recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  sequence_recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  sequence_recursive_struct.xt._u.structure.members.length = 1;
  sequence_recursive_struct.xt._u.structure.members.seq = &sequence_recursive_member;
  init_test_hash_id (&sequence.xt.id, 4);
  sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  sequence.xt._u.seq.c.element_type = &sequence_recursive_struct;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &sequence_recursive_struct), DDS_RETCODE_OK);

  struct ddsi_type alias_element_recursive_struct = {0}, alias_element_sequence = {0}, alias_element = {0};
  struct xt_struct_member alias_element_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &alias_element_sequence
  };
  init_test_hash_id (&alias_element_recursive_struct.xt.id, 5);
  alias_element_recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  alias_element_recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  alias_element_recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  alias_element_recursive_struct.xt._u.structure.members.length = 1;
  alias_element_recursive_struct.xt._u.structure.members.seq = &alias_element_member;
  init_test_hash_id (&alias_element_sequence.xt.id, 6);
  alias_element_sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  alias_element_sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  alias_element_sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  alias_element_sequence.xt._u.seq.c.element_type = &alias_element;
  init_test_hash_id (&alias_element.xt.id, 7);
  alias_element.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  alias_element.xt._d = DDS_XTypes_TK_ALIAS;
  alias_element.xt._u.alias.related_type = &alias_element_recursive_struct;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &alias_element_recursive_struct), DDS_RETCODE_OK);

  struct ddsi_type top_alias = {0}, top_alias_recursive_struct = {0}, top_alias_sequence = {0};
  struct xt_struct_member top_alias_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &top_alias_sequence
  };
  init_test_hash_id (&top_alias.xt.id, 8);
  top_alias.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  top_alias.xt._d = DDS_XTypes_TK_ALIAS;
  top_alias.xt._u.alias.related_type = &top_alias_recursive_struct;
  init_test_hash_id (&top_alias_recursive_struct.xt.id, 9);
  top_alias_recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  top_alias_recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  top_alias_recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  top_alias_recursive_struct.xt._u.structure.members.length = 1;
  top_alias_recursive_struct.xt._u.structure.members.seq = &top_alias_member;
  init_test_hash_id (&top_alias_sequence.xt.id, 10);
  top_alias_sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  top_alias_sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  top_alias_sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  top_alias_sequence.xt._u.seq.c.element_type = &top_alias;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &top_alias), DDS_RETCODE_OK);

  /* Start at the typedef-like alias to verify cycle validity is independent of
     the root and of which edge closes the cycle. */
  struct ddsi_type sequence_alias = {0}, alias_sequence = {0}, alias_sequence_recursive_struct = {0};
  struct xt_struct_member sequence_alias_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &sequence_alias
  };
  init_test_hash_id (&sequence_alias.xt.id, 11);
  sequence_alias.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  sequence_alias.xt._d = DDS_XTypes_TK_ALIAS;
  sequence_alias.xt._u.alias.related_type = &alias_sequence;
  init_test_hash_id (&alias_sequence.xt.id, 12);
  alias_sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  alias_sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  alias_sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  alias_sequence.xt._u.seq.c.element_type = &alias_sequence_recursive_struct;
  init_test_hash_id (&alias_sequence_recursive_struct.xt.id, 13);
  alias_sequence_recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  alias_sequence_recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  alias_sequence_recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  alias_sequence_recursive_struct.xt._u.structure.members.length = 1;
  alias_sequence_recursive_struct.xt._u.structure.members.seq = &sequence_alias_member;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &sequence_alias), DDS_RETCODE_OK);

  struct ddsi_type int32_type = { .xt = { ._d = DDS_XTypes_TK_INT32 } };
  struct ddsi_type optional_child = {0}, key_context_parent = {0};
  struct xt_struct_member optional_child_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_IS_OPTIONAL,
    .type = &int32_type
  };
  struct xt_struct_member key_context_members[] = {
    { .id = 1, .flags = DDS_XTypes_TRY_CONSTRUCT1, .type = &optional_child },
    { .id = 2, .flags = DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_IS_KEY, .type = &optional_child }
  };
  init_test_hash_id (&optional_child.xt.id, 14);
  optional_child.state = DDSI_TYPE_RESOLVED;
  optional_child.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  optional_child.xt._d = DDS_XTypes_TK_STRUCTURE;
  optional_child.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  optional_child.xt._u.structure.members.length = 1;
  optional_child.xt._u.structure.members.seq = &optional_child_member;
  init_test_hash_id (&key_context_parent.xt.id, 15);
  key_context_parent.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  key_context_parent.xt._d = DDS_XTypes_TK_STRUCTURE;
  key_context_parent.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  key_context_parent.xt._u.structure.members.length = sizeof (key_context_members) / sizeof (key_context_members[0]);
  key_context_parent.xt._u.structure.members.seq = key_context_members;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &key_context_parent), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test (ddsc_typewrap, recursive_type_validation_disabled, .init = typewrap_no_recursive_init, .fini = typewrap_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  CU_ASSERT_EQ_FATAL (gv->config.allow_recursive_types, 0);

  struct ddsi_type recursive_struct = {0}, sequence = {0};
  struct xt_struct_member recursive_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &sequence
  };
  init_test_hash_id (&recursive_struct.xt.id, 1);
  recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  recursive_struct.xt._u.structure.members.length = 1;
  recursive_struct.xt._u.structure.members.seq = &recursive_member;
  init_test_hash_id (&sequence.xt.id, 2);
  sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  sequence.xt._u.seq.c.element_type = &recursive_struct;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &recursive_struct), DDS_RETCODE_BAD_PARAMETER);

  struct ddsi_type int32_type = { .xt = { ._d = DDS_XTypes_TK_INT32 } };
  struct ddsi_type non_recursive_struct = {0};
  struct xt_struct_member non_recursive_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &int32_type
  };
  init_test_hash_id (&non_recursive_struct.xt.id, 3);
  non_recursive_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  non_recursive_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  non_recursive_struct.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  non_recursive_struct.xt._u.structure.members.length = 1;
  non_recursive_struct.xt._u.structure.members.seq = &non_recursive_member;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &non_recursive_struct), DDS_RETCODE_OK);
}

CU_Test (ddsc_typewrap, recursive_type_assignability, .init = typewrap_init, .fini = typewrap_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type rd_struct = { .refc = 1 }, rd_sequence = { .refc = 1 },
    wr_struct = { .refc = 1 }, wr_sequence = { .refc = 1 };
  struct xt_struct_member rd_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &rd_sequence
  };
  struct xt_struct_member wr_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &wr_sequence
  };
  init_test_hash_id (&rd_struct.xt.id, 10);
  rd_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  rd_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  rd_struct.xt._u.structure.flags = DDS_XTypes_IS_MUTABLE;
  rd_struct.xt._u.structure.members.length = 1;
  rd_struct.xt._u.structure.members.seq = &rd_member;
  init_test_hash_id (&rd_sequence.xt.id, 11);
  rd_sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  rd_sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  rd_sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  rd_sequence.xt._u.seq.c.element_type = &rd_struct;

  init_test_hash_id (&wr_struct.xt.id, 12);
  wr_struct.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  wr_struct.xt._d = DDS_XTypes_TK_STRUCTURE;
  wr_struct.xt._u.structure.flags = DDS_XTypes_IS_MUTABLE;
  wr_struct.xt._u.structure.members.length = 1;
  wr_struct.xt._u.structure.members.seq = &wr_member;
  init_test_hash_id (&wr_sequence.xt.id, 13);
  wr_sequence.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  wr_sequence.xt._d = DDS_XTypes_TK_SEQUENCE;
  wr_sequence.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  wr_sequence.xt._u.seq.c.element_type = &wr_struct;

  dds_type_consistency_enforcement_qospolicy_t tce = {
    .kind = DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION,
    .ignore_sequence_bounds = true,
    .ignore_string_bounds = true
  };
  struct ddsi_non_assignability_reason reason;
  CU_ASSERT_EQ_FATAL (ddsi_xt_validate (gv, &rd_struct), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (ddsi_xt_validate (gv, &wr_struct), DDS_RETCODE_OK);
  CU_ASSERT (ddsi_xt_is_assignable_from (gv, &rd_struct.xt, &wr_struct.xt, &tce, &reason));

  struct ddsi_type rd_sequence_a = {0}, rd_sequence_b = {0}, wr_sequence_a = {0}, wr_sequence_b = {0};
  init_test_hash_id (&rd_sequence_a.xt.id, 14);
  rd_sequence_a.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  rd_sequence_a.xt._d = DDS_XTypes_TK_SEQUENCE;
  rd_sequence_a.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  rd_sequence_a.xt._u.seq.c.element_type = &rd_sequence_b;
  init_test_hash_id (&rd_sequence_b.xt.id, 15);
  rd_sequence_b.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  rd_sequence_b.xt._d = DDS_XTypes_TK_SEQUENCE;
  rd_sequence_b.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  rd_sequence_b.xt._u.seq.c.element_type = &rd_sequence_a;

  init_test_hash_id (&wr_sequence_a.xt.id, 16);
  wr_sequence_a.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  wr_sequence_a.xt._d = DDS_XTypes_TK_SEQUENCE;
  wr_sequence_a.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  wr_sequence_a.xt._u.seq.c.element_type = &wr_sequence_b;
  init_test_hash_id (&wr_sequence_b.xt.id, 17);
  wr_sequence_b.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  wr_sequence_b.xt._d = DDS_XTypes_TK_SEQUENCE;
  wr_sequence_b.xt._u.seq.c.element_flags = DDS_XTypes_TRY_CONSTRUCT1;
  wr_sequence_b.xt._u.seq.c.element_type = &wr_sequence_a;

  CU_ASSERT_EQ (ddsi_xt_validate (gv, &rd_sequence_a), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &wr_sequence_a), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test (ddsc_typewrap, alias_base_validation, .init = typewrap_init, .fini = typewrap_fini)
{
  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type int32_type = { .xt = { ._d = DDS_XTypes_TK_INT32 } };
  struct ddsi_type base = {0}, base_alias = {0}, derived = {0};
  struct xt_struct_member base_member = {
    .id = 1,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &int32_type
  };
  struct xt_struct_member derived_member = {
    .id = 2,
    .flags = DDS_XTypes_TRY_CONSTRUCT1,
    .type = &int32_type
  };

  init_test_hash_id (&base.xt.id, 7);
  base.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  base.xt._d = DDS_XTypes_TK_STRUCTURE;
  base.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  base.xt._u.structure.members.length = 1;
  base.xt._u.structure.members.seq = &base_member;

  init_test_hash_id (&base_alias.xt.id, 8);
  base_alias.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  base_alias.xt._d = DDS_XTypes_TK_ALIAS;
  base_alias.xt._u.alias.related_type = &base;

  init_test_hash_id (&derived.xt.id, 9);
  derived.xt.kind = DDSI_TYPEID_KIND_COMPLETE;
  derived.xt._d = DDS_XTypes_TK_STRUCTURE;
  derived.xt._u.structure.flags = DDS_XTypes_IS_FINAL;
  derived.xt._u.structure.base_type = &base_alias;
  derived.xt._u.structure.members.length = 1;
  derived.xt._u.structure.members.seq = &derived_member;

  CU_ASSERT_EQ (ddsi_xt_validate (gv, &derived), DDS_RETCODE_OK);
  derived_member.id = base_member.id;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &derived), DDS_RETCODE_BAD_PARAMETER);

  derived_member.id = 2;
  base.state = DDSI_TYPE_RESOLVED;
  base.xt._u.structure.base_type = &int32_type;
  CU_ASSERT_EQ (ddsi_xt_validate (gv, &derived), DDS_RETCODE_BAD_PARAMETER);
}

CU_Test (ddsc_typewrap, invalid_dependency_state, .init = typewrap_init, .fini = typewrap_fini)
{
  struct DDS_XTypes_TypeObject invalid_dep_typeobj = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_ENUM,
      ._u.enumerated_type = {
        .enum_flags = DDS_XTypes_IS_FINAL,
        .header = { .common = { .bit_bound = 8 } }
      }
    }
  };
  ddsrt_strlcpy (invalid_dep_typeobj._u.complete._u.enumerated_type.header.detail.type_name, "InvalidDep",
      sizeof (invalid_dep_typeobj._u.complete._u.enumerated_type.header.detail.type_name));

  ddsi_typeid_t dep_typeid, parent_typeid, late_parent_typeid;
  dds_return_t ret = ddsi_typeobj_get_hash_id (&invalid_dep_typeobj, &dep_typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct DDS_XTypes_TypeObject parent_typeobj, late_parent_typeobj;
  struct DDS_XTypes_CompleteStructMember parent_member, late_parent_member;
  init_struct_dependency_typeobject (&parent_typeobj, &parent_member, "InvalidDepParent", "dep", 1, &dep_typeid);
  init_struct_dependency_typeobject (&late_parent_typeobj, &late_parent_member, "LateInvalidDepParent", "late_dep", 2, &dep_typeid);
  ret = ddsi_typeobj_get_hash_id (&parent_typeobj, &parent_typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = ddsi_typeobj_get_hash_id (&late_parent_typeobj, &late_parent_typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type *dep = NULL, *parent = NULL, *late_parent = NULL;
  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = ddsi_type_ref_id_locked (gv, &dep, &dep_typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  ret = ddsi_type_ref_id_locked (gv, &parent, &parent_typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (dep != NULL && parent != NULL)
  {
    ret = ddsi_type_add_typeobj (gv, parent, &parent_typeobj);
    CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
    CU_ASSERT_NEQ (parent->state, DDSI_TYPE_INVALID);

    ret = ddsi_type_add_typeobj (gv, dep, &invalid_dep_typeobj);
    CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
    CU_ASSERT_EQ (dep->state, DDSI_TYPE_INVALID);
    CU_ASSERT_EQ (parent->state, DDSI_TYPE_INVALID);
  }

  ret = ddsi_type_ref_id_locked (gv, &late_parent, &late_parent_typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (late_parent != NULL)
  {
    ret = ddsi_type_add_typeobj (gv, late_parent, &late_parent_typeobj);
    CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
    CU_ASSERT_EQ (late_parent->state, DDSI_TYPE_INVALID);
  }
  if (late_parent != NULL)
    ddsi_type_unref_locked (gv, late_parent);
  if (parent != NULL)
    ddsi_type_unref_locked (gv, parent);
  if (dep != NULL)
    ddsi_type_unref_locked (gv, dep);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  ddsi_typeid_fini (&late_parent_typeid);
  ddsi_typeid_fini (&parent_typeid);
  ddsi_typeid_fini (&dep_typeid);
}

static void init_scc_typeid (ddsi_typeid_t *typeid, int32_t length, int32_t index)
{
  memset (typeid, 0, sizeof (*typeid));
  typeid->x._d = DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
  typeid->x._u.sc_component_id.sc_component_id._d = DDS_XTypes_EK_COMPLETE;
  typeid->x._u.sc_component_id.sc_component_id._u.hash[0] = 1;
  typeid->x._u.sc_component_id.scc_length = length;
  typeid->x._u.sc_component_id.scc_index = index;
}

static void fill_equivalence_hash (DDS_XTypes_EquivalenceHash hash, uint8_t seed)
{
  for (size_t n = 0; n < sizeof (DDS_XTypes_EquivalenceHash); n++)
    hash[n] = (uint8_t) (seed + n);
}

CU_Test (ddsc_typewrap, scc_identifier_hashes_slot_and_component_identity)
{
  ddsi_typeid_t slot1, slot2, different_length, different_component;
  init_scc_typeid (&slot1, 2, 1);
  fill_equivalence_hash (slot1.x._u.sc_component_id.sc_component_id._u.hash, 0x10);
  slot2 = slot1;
  slot2.x._u.sc_component_id.scc_index = 2;
  different_length = slot1;
  different_length.x._u.sc_component_id.scc_length = 3;
  different_component = slot1;
  different_component.x._u.sc_component_id.sc_component_id._u.hash[0] ^= 0x80;

  CU_ASSERT_NEQ (ddsi_typeid_compare (&slot1, &slot2), 0);
  CU_ASSERT_NEQ (ddsi_typeid_hash (&slot1), ddsi_typeid_hash (&slot2));
  CU_ASSERT (ddsi_type_scc_id_same_component_impl (
    &slot1.x._u.sc_component_id, &slot2.x._u.sc_component_id));
  CU_ASSERT_EQ (ddsi_type_scc_id_component_hash_impl (&slot1.x._u.sc_component_id),
                ddsi_type_scc_id_component_hash_impl (&slot2.x._u.sc_component_id));

  CU_ASSERT (!ddsi_type_scc_id_same_component_impl (
    &slot1.x._u.sc_component_id, &different_length.x._u.sc_component_id));
  CU_ASSERT (!ddsi_type_scc_id_same_component_impl (
    &slot1.x._u.sc_component_id, &different_component.x._u.sc_component_id));
}

CU_Test (ddsc_typewrap, equivalence_hash_getter_handles_scc_identifier)
{
  ddsi_typeid_t scc_typeid;
  DDS_XTypes_EquivalenceHash expected_scc_hash, actual_hash;
  init_scc_typeid (&scc_typeid, 2, 1);
  fill_equivalence_hash (expected_scc_hash, 0x20);
  memcpy (scc_typeid.x._u.sc_component_id.sc_component_id._u.hash,
          expected_scc_hash, sizeof (expected_scc_hash));
  ddsi_typeid_get_equivalence_hash (&scc_typeid, &actual_hash);
  CU_ASSERT_MEMEQ (&actual_hash, sizeof (actual_hash), &expected_scc_hash, sizeof (expected_scc_hash));

  ddsi_typeid_t complete_typeid = { .x = { ._d = DDS_XTypes_EK_COMPLETE } };
  DDS_XTypes_EquivalenceHash expected_complete_hash;
  fill_equivalence_hash (expected_complete_hash, 0x40);
  memcpy (complete_typeid.x._u.equivalence_hash,
          expected_complete_hash, sizeof (expected_complete_hash));
  ddsi_typeid_get_equivalence_hash (&complete_typeid, &actual_hash);
  CU_ASSERT_MEMEQ (&actual_hash, sizeof (actual_hash), &expected_complete_hash, sizeof (expected_complete_hash));
}

CU_Test (ddsc_typewrap, scc_identifier_bounds, .init = typewrap_init, .fini = typewrap_fini)
{
  ddsi_typeid_t typeid;
  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type *type = NULL;

  ddsrt_mutex_lock (&gv->typelib_lock);
  init_scc_typeid (&typeid, 1, 2);
  dds_return_t ret = ddsi_type_ref_id_locked (gv, &type, &typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (type, NULL);

  init_scc_typeid (&typeid, 101, 1);
  ret = ddsi_type_ref_id_locked (gv, &type, &typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (type, NULL);

  init_scc_typeid (&typeid, 2, 1);
  ret = ddsi_type_ref_id_locked (gv, &type, &typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL (type, NULL);
  CU_ASSERT_NEQ_FATAL (type->scc, NULL);
  CU_ASSERT_EQ (type->scc->n_wire_types, 2);
  CU_ASSERT_FATAL (type->scc->types[0] == type);
  CU_ASSERT_EQ (type->scc->types[1], NULL);

  struct DDS_XTypes_CompleteStructMember member;
  struct DDS_XTypes_TypeObject typeobj;
  const ddsi_typeid_t int32_typeid = { .x = { ._d = DDS_XTypes_TK_INT32 } };
  init_struct_dependency_typeobject (&typeobj, &member, "IncompleteScc", "value", 1, &int32_typeid);
  ret = ddsi_type_add_typeobj (gv, type, &typeobj);
  CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (type->state, DDSI_TYPE_UNRESOLVED);

  ddsi_type_unref_locked (gv, type);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

CU_Test (ddsc_typewrap, scc_component_must_be_strongly_connected, .init = typewrap_init, .fini = typewrap_fini)
{
  const ddsi_typeid_t int32_typeid = { .x = { ._d = DDS_XTypes_TK_INT32 } };
  ddsi_typeid_t scc_ref_typeid = {
    .x = {
      ._d = DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT,
      ._u.sc_component_id = {
        .sc_component_id = { ._d = DDS_XTypes_EK_COMPLETE },
        .scc_length = 2,
        .scc_index = 2
      }
    }
  };
  struct DDS_XTypes_CompleteStructMember member_a, member_b;
  struct DDS_XTypes_TypeObject typeobjs[2];
  init_struct_dependency_typeobject (&typeobjs[0], &member_a, "SccButNotStrongA", "next", 1, &scc_ref_typeid);
  init_struct_dependency_typeobject (&typeobjs[1], &member_b, "SccButIndependentB", "value", 1, &int32_typeid);

  DDS_XTypes_EquivalenceHash hash;
  dds_return_t ret = ddsi_typeobj_get_scc_hash (hash, typeobjs, 2);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  memcpy (member_a.common.member_type_id._u.sc_component_id.sc_component_id._u.hash, hash, sizeof (hash));

  DDS_XTypes_TypeIdentifierTypeObjectPair pairs[2] = { 0 };
  dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair pairseq = {
    ._maximum = 2,
    ._length = 2,
    ._buffer = pairs,
    ._release = false
  };
  for (uint32_t n = 0; n < 2; n++)
  {
    pairs[n].type_identifier._d = DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
    pairs[n].type_identifier._u.sc_component_id.sc_component_id._d = DDS_XTypes_EK_COMPLETE;
    memcpy (pairs[n].type_identifier._u.sc_component_id.sc_component_id._u.hash, hash, sizeof (hash));
    pairs[n].type_identifier._u.sc_component_id.scc_length = 2;
    pairs[n].type_identifier._u.sc_component_id.scc_index = (int32_t) n + 1;
    pairs[n].type_object = typeobjs[n];
  }

  struct ddsi_domaingv *gv = get_domaingv (participant);
  bool complete = false;
  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = ddsi_type_add_scc_typeobjs_locked (gv, &pairseq, &pairs[0].type_identifier, true, &complete);
  CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT (!complete);
  CU_ASSERT_EQ (ddsi_type_lookup_locked_impl (gv, &pairs[0].type_identifier), NULL);
  CU_ASSERT_EQ (ddsi_type_lookup_locked_impl (gv, &pairs[1].type_identifier), NULL);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

CU_Test (ddsc_typewrap, large_map_kind_uses_key_identifier)
{
  struct DDS_XTypes_TypeIdentifier key_typeid = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.equivalence_hash = { 1 }
  };
  struct DDS_XTypes_TypeIdentifier element_typeid = { ._d = DDS_XTypes_TK_INT32 };
  ddsi_typeid_t map_typeid = {
    .x = {
      ._d = DDS_XTypes_TI_PLAIN_MAP_LARGE,
      ._u.map_ldefn = {
        .header = { .equiv_kind = DDS_XTypes_EK_COMPLETE },
        .bound = 1000,
        .element_identifier = &element_typeid,
        .key_identifier = &key_typeid
      }
    }
  };
  CU_ASSERT_EQ (ddsi_typeid_kind (&map_typeid), DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE);
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
    uint8_t discriminator_type,
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
            .type_id = { ._d = discriminator_type }
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

static void check_union_discriminator_typeobject (
    const char *name,
    const struct DDS_XTypes_TypeIdentifier *discriminator_type,
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
            .type_id = *discriminator_type
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

static void check_union_enum_typeobject (
    const char *name,
    const char *enum_name,
    uint16_t bit_bound,
    const struct enum_literal *literals,
    uint32_t n_literals,
    const struct union_member *members,
    uint32_t n_members,
    dds_return_t expected_ret)
{
  struct DDS_XTypes_CompleteEnumeratedLiteral literal_buf[5] = {0};
  CU_ASSERT_LEQ_FATAL (n_literals, sizeof (literal_buf) / sizeof (literal_buf[0]));
  for (uint32_t n = 0; n < n_literals; n++)
  {
    literal_buf[n].common.value = literals[n].value;
    ddsrt_strlcpy (literal_buf[n].detail.name, literals[n].name, sizeof (literal_buf[n].detail.name));
  }

  struct DDS_XTypes_TypeObject enum_typeobj = {
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
  ddsrt_strlcpy (enum_typeobj._u.complete._u.enumerated_type.header.detail.type_name, enum_name,
      sizeof (enum_typeobj._u.complete._u.enumerated_type.header.detail.type_name));

  ddsi_typeid_t enum_typeid;
  dds_return_t ret = ddsi_typeobj_get_hash_id (&enum_typeobj, &enum_typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

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
            .type_id = enum_typeid.x
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

  ddsi_typeid_t typeid;
  ret = ddsi_typeobj_get_hash_id (&typeobj, &typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type *enum_type = NULL, *type = NULL;
  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = ddsi_type_ref_id_locked (gv, &enum_type, &enum_typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (ret == DDS_RETCODE_OK)
  {
    ret = ddsi_type_add_typeobj (gv, enum_type, &enum_typeobj);
    CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  }

  ret = ddsi_type_ref_id_locked (gv, &type, &typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (ret == DDS_RETCODE_OK)
  {
    ret = ddsi_type_add_typeobj (gv, type, &typeobj);
    CU_ASSERT_EQ (ret, expected_ret);
  }
  ddsi_type_unref_locked (gv, type);
  ddsi_type_unref_locked (gv, enum_type);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

static void check_union_bitmask_typeobject (
    const char *name,
    const char *bitmask_name,
    uint16_t bit_bound,
    const struct bitflag *bitflags,
    uint32_t n_bitflags,
    const struct union_member *members,
    uint32_t n_members,
    dds_return_t expected_ret)
{
  struct DDS_XTypes_CompleteBitflag bitflag_buf[5] = {0};
  CU_ASSERT_LEQ_FATAL (n_bitflags, sizeof (bitflag_buf) / sizeof (bitflag_buf[0]));
  for (uint32_t n = 0; n < n_bitflags; n++)
  {
    bitflag_buf[n].common.position = bitflags[n].position;
    ddsrt_strlcpy (bitflag_buf[n].detail.name, bitflags[n].name, sizeof (bitflag_buf[n].detail.name));
  }

  struct DDS_XTypes_TypeObject bitmask_typeobj = {
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
  ddsrt_strlcpy (bitmask_typeobj._u.complete._u.bitmask_type.header.detail.type_name, bitmask_name,
      sizeof (bitmask_typeobj._u.complete._u.bitmask_type.header.detail.type_name));

  ddsi_typeid_t bitmask_typeid;
  dds_return_t ret = ddsi_typeobj_get_hash_id (&bitmask_typeobj, &bitmask_typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

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
            .type_id = bitmask_typeid.x
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

  ddsi_typeid_t typeid;
  ret = ddsi_typeobj_get_hash_id (&typeobj, &typeid);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);

  struct ddsi_domaingv *gv = get_domaingv (participant);
  struct ddsi_type *bitmask_type = NULL, *type = NULL;
  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = ddsi_type_ref_id_locked (gv, &bitmask_type, &bitmask_typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (ret == DDS_RETCODE_OK)
  {
    ret = ddsi_type_add_typeobj (gv, bitmask_type, &bitmask_typeobj);
    CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  }

  ret = ddsi_type_ref_id_locked (gv, &type, &typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  if (ret == DDS_RETCODE_OK)
  {
    ret = ddsi_type_add_typeobj (gv, type, &typeobj);
    CU_ASSERT_EQ (ret, expected_ret);
  }
  ddsi_type_unref_locked (gv, type);
  ddsi_type_unref_locked (gv, bitmask_type);
  ddsrt_mutex_unlock (&gv->typelib_lock);
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

CU_Test (ddsc_typewrap, invalid_bitset_typeobject_hash)
{
  struct DDS_XTypes_TypeObject typeobj = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.complete = {
      ._d = DDS_XTypes_TK_BITSET,
      ._u.bitset_type = {
        .bitset_flags = 0x8000u
      }
    }
  };
  ddsrt_strlcpy (typeobj._u.complete._u.bitset_type.header.detail.type_name, "InvalidBitsetHash",
      sizeof (typeobj._u.complete._u.bitset_type.header.detail.type_name));

  ddsi_typeid_t typeid;
  dds_return_t ret = ddsi_typeobj_get_hash_id (&typeobj, &typeid);
  CU_ASSERT_EQ (ret, DDS_RETCODE_BAD_PARAMETER);
  if (ret == DDS_RETCODE_OK)
    ddsi_typeid_fini (&typeid);
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
  check_union_typeobject ("DuplicateAdjacentUnionMember", DDS_XTypes_TK_INT32, duplicate_adjacent,
      sizeof (duplicate_adjacent) / sizeof (duplicate_adjacent[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member duplicate_unsorted[] = {
    { "duplicate_unsorted_a", 7, 1 },
    { "duplicate_unsorted_b", 2, 2 },
    { "duplicate_unsorted_c", 5, 3 },
    { "duplicate_unsorted_d", 2, 4 }
  };
  check_union_typeobject ("DuplicateUnsortedUnionMember", DDS_XTypes_TK_INT32, duplicate_unsorted,
      sizeof (duplicate_unsorted) / sizeof (duplicate_unsorted[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member duplicate_run[] = {
    { "duplicate_run_a", 6, 1 },
    { "duplicate_run_b", 2, 2 },
    { "duplicate_run_c", 2, 3 },
    { "duplicate_run_d", 5, 4 },
    { "duplicate_run_e", 2, 5 }
  };
  check_union_typeobject ("DuplicateRunUnionMember", DDS_XTypes_TK_INT32, duplicate_run,
      sizeof (duplicate_run) / sizeof (duplicate_run[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member overlapping_labels[] = {
    { "overlapping_label_a", 1, 1 },
    { "overlapping_label_b", 2, 1 },
    { "overlapping_label_c", 3, 3 }
  };
  check_union_typeobject ("OverlappingUnionLabels", DDS_XTypes_TK_INT32, overlapping_labels,
      sizeof (overlapping_labels) / sizeof (overlapping_labels[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct union_member label_outside_discriminator_range[] = {
    { "label_range_a", 1, INT8_MIN },
    { "label_range_b", 2, INT8_MAX },
    { "label_range_c", 3, (int32_t) INT8_MAX + 1 }
  };
  check_union_typeobject ("LabelOutsideDiscriminatorRange", DDS_XTypes_TK_INT8, label_outside_discriminator_range,
      sizeof (label_outside_discriminator_range) / sizeof (label_outside_discriminator_range[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct enum_literal sparse_enum[] = {
    { "sparse_a", 1 },
    { "sparse_b", 3 },
    { "sparse_c", 7 }
  };
  const struct union_member valid_enum_labels[] = {
    { "valid_enum_a", 1, 1 },
    { "valid_enum_b", 2, 3 },
    { "valid_enum_c", 3, 7 }
  };
  check_union_enum_typeobject ("ValidEnumDiscriminatorLabels", "ValidEnumDiscriminator", 4, sparse_enum,
      sizeof (sparse_enum) / sizeof (sparse_enum[0]), valid_enum_labels,
      sizeof (valid_enum_labels) / sizeof (valid_enum_labels[0]), DDS_RETCODE_OK);

  const struct union_member undefined_enum_label[] = {
    { "undefined_enum_a", 1, 1 },
    { "undefined_enum_b", 2, 2 },
    { "undefined_enum_c", 3, 7 }
  };
  check_union_enum_typeobject ("UndefinedEnumDiscriminatorLabel", "UndefinedEnumDiscriminator", 4, sparse_enum,
      sizeof (sparse_enum) / sizeof (sparse_enum[0]), undefined_enum_label,
      sizeof (undefined_enum_label) / sizeof (undefined_enum_label[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag sparse_bitmask[] = {
    { "sparse_bit_a", 0 },
    { "sparse_bit_b", 2 },
    { "sparse_bit_c", 4 }
  };
  const struct union_member valid_bitmask_labels[] = {
    { "valid_bitmask_a", 1, 1 },
    { "valid_bitmask_b", 2, 4 },
    { "valid_bitmask_c", 3, 17 }
  };
  check_union_bitmask_typeobject ("ValidBitmaskDiscriminatorLabels", "ValidBitmaskDiscriminator", 5, sparse_bitmask,
      sizeof (sparse_bitmask) / sizeof (sparse_bitmask[0]), valid_bitmask_labels,
      sizeof (valid_bitmask_labels) / sizeof (valid_bitmask_labels[0]), DDS_RETCODE_OK);

  const struct union_member undefined_bitmask_bit[] = {
    { "undefined_bitmask_a", 1, 1 },
    { "undefined_bitmask_b", 2, 2 },
    { "undefined_bitmask_c", 3, 16 }
  };
  check_union_bitmask_typeobject ("UndefinedBitmaskDiscriminatorBit", "UndefinedBitmaskDiscriminator", 5, sparse_bitmask,
      sizeof (sparse_bitmask) / sizeof (sparse_bitmask[0]), undefined_bitmask_bit,
      sizeof (undefined_bitmask_bit) / sizeof (undefined_bitmask_bit[0]), DDS_RETCODE_BAD_PARAMETER);

  const struct bitflag high_bitmask[] = {
    { "high_bit_a", 0 },
    { "high_bit_b", 31 }
  };
  const struct union_member high_bitmask_labels[] = {
    { "high_bitmask_a", 1, 1 },
    { "high_bitmask_b", 2, INT32_MIN }
  };
  check_union_bitmask_typeobject ("HighBitmaskDiscriminatorLabels", "HighBitmaskDiscriminator", 32, high_bitmask,
      sizeof (high_bitmask) / sizeof (high_bitmask[0]), high_bitmask_labels,
      sizeof (high_bitmask_labels) / sizeof (high_bitmask_labels[0]), DDS_RETCODE_OK);

  const struct DDS_XTypes_TypeIdentifier unresolved_enum_discriminator = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.equivalence_hash = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 }
  };
  const struct union_member unresolved_enum_labels[] = {
    { "unresolved_enum_a", 1, 1 },
    { "unresolved_enum_b", 2, 2 },
    { "unresolved_enum_c", 3, 7 }
  };
  check_union_discriminator_typeobject ("UnresolvedEnumDiscriminatorLabels", &unresolved_enum_discriminator,
      unresolved_enum_labels, sizeof (unresolved_enum_labels) / sizeof (unresolved_enum_labels[0]), DDS_RETCODE_OK);

  const struct DDS_XTypes_TypeIdentifier unresolved_bitmask_discriminator = {
    ._d = DDS_XTypes_EK_COMPLETE,
    ._u.equivalence_hash = { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 }
  };
  const struct union_member unresolved_bitmask_labels[] = {
    { "unresolved_bitmask_a", 1, 1 },
    { "unresolved_bitmask_b", 2, 2 },
    { "unresolved_bitmask_c", 3, INT32_MIN }
  };
  check_union_discriminator_typeobject ("UnresolvedBitmaskDiscriminatorLabels", &unresolved_bitmask_discriminator,
      unresolved_bitmask_labels, sizeof (unresolved_bitmask_labels) / sizeof (unresolved_bitmask_labels[0]), DDS_RETCODE_OK);

  const struct union_member valid_member_ids[] = {
    { "valid_first", 1, 1 },
    { "valid_middle", 12, 2 },
    { "valid_last", 27, 3 }
  };
  check_union_typeobject ("ValidUnionMemberIds", DDS_XTypes_TK_INT32, valid_member_ids,
      sizeof (valid_member_ids) / sizeof (valid_member_ids[0]), DDS_RETCODE_OK);
}
