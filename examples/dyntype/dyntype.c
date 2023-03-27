/*
 * Copyright(c) 2023 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "dds/dds.h"
#include <stdio.h>
#include <stdlib.h>

int main (int argc, char ** argv)
{
  (void) argc;
  (void) argv;

  dds_return_t rc;
  dds_entity_t participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  if (participant < 0)
    DDS_FATAL("dds_create_participant: %s\n", dds_strretcode (-participant));

  dds_dynamic_type_t dsubstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_substruct" });
  dds_dynamic_type_set_extensibility (&dsubstruct, DDS_DYNAMIC_TYPE_EXT_APPENDABLE);
  dds_dynamic_type_set_autoid (&dsubstruct, DDS_DYNAMIC_TYPE_AUTOID_HASH);
  dds_dynamic_type_add_member (&dsubstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_UINT16, "submember_uint16"));

  dds_dynamic_type_t dsubunion = dds_dynamic_type_create (participant,
      (dds_dynamic_type_descriptor_t) {
        .kind = DDS_DYNAMIC_UNION,
        .discriminator_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32),
        .name = "dynamic_subunion"
      });
  dds_dynamic_type_add_member (&dsubunion, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_INT32, "member_int32", 2, ((int32_t[]) { 1, 2 })));
  dds_dynamic_type_add_member (&dsubunion, DDS_DYNAMIC_UNION_MEMBER_ID_PRIM(DDS_DYNAMIC_FLOAT64, "member_float64", 100 /* has specific member id */, 2, ((int32_t[]) { 9, 10 })));
  dds_dynamic_type_add_member (&dsubunion, DDS_DYNAMIC_UNION_MEMBER(dds_dynamic_type_ref (&dsubstruct) /* increase ref because type is re-used */, "submember_substruct", 2, ((int32_t[]) { 15, 16 })));
  dds_dynamic_type_add_member (&dsubunion, DDS_DYNAMIC_UNION_MEMBER_DEFAULT_PRIM(DDS_DYNAMIC_BOOLEAN, "submember_default"));

  dds_dynamic_type_t dsubunion2 = dds_dynamic_type_dup (&dsubunion);
  dds_dynamic_type_add_member (&dsubunion2, DDS_DYNAMIC_UNION_MEMBER_PRIM(DDS_DYNAMIC_BOOLEAN, "submember_bool", 1, ((int32_t[]) { 5 })));

  dds_dynamic_type_t dsubsubstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_subsubstruct" });
  dds_dynamic_type_add_member (&dsubsubstruct, DDS_DYNAMIC_MEMBER(dsubunion2, "subsubmember_union"));

  // Sequences
  dds_dynamic_type_t dseq = dds_dynamic_type_create (participant,
      (dds_dynamic_type_descriptor_t) {
        .kind = DDS_DYNAMIC_SEQUENCE,
        .name = "dynamic_seq",
        .element_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_INT32),
        .bounds = (uint32_t[]) { 10 },
        .num_bounds = 1
      });
  dds_dynamic_type_t dseq2 = dds_dynamic_type_create (participant,
      (dds_dynamic_type_descriptor_t) {
        .kind = DDS_DYNAMIC_SEQUENCE,
        .name = "dynamic_seq2",
        .element_type = DDS_DYNAMIC_TYPE_SPEC(dds_dynamic_type_ref (&dsubstruct)) /* increase ref because type is re-used */,
        .num_bounds = 0
      });

  // Arrays
  dds_dynamic_type_t darr = dds_dynamic_type_create (participant,
    (dds_dynamic_type_descriptor_t) {
      .kind = DDS_DYNAMIC_ARRAY,
      .name = "dynamic_array",
      .element_type = DDS_DYNAMIC_TYPE_SPEC_PRIM(DDS_DYNAMIC_FLOAT64),
      .bounds = (uint32_t[]) { 5, 99 },
      .num_bounds = 2
    });

  // Enum
  dds_dynamic_type_t denum = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_ENUMERATION, .name = "dynamic_enum" });
  dds_dynamic_type_add_enum_literal (&denum, "DYNAMIC_ENUM_VALUE1", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false);
  dds_dynamic_type_add_enum_literal (&denum, "DYNAMIC_ENUM_VALUE2", DDS_DYNAMIC_ENUM_LITERAL_VALUE_AUTO, false);
  dds_dynamic_type_add_enum_literal (&denum, "DYNAMIC_ENUM_VALUE5", DDS_DYNAMIC_ENUM_LITERAL_VALUE(5), false);

  // Bitmask
  dds_dynamic_type_t dbitmask = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_BITMASK, .name = "dynamic_bitmask" });
  dds_dynamic_type_add_bitmask_field (&dbitmask, "DYNAMIC_BITMASK_1", DDS_DYNAMIC_BITMASK_POSITION_AUTO);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "DYNAMIC_BITMASK_5", 5);
  dds_dynamic_type_add_bitmask_field (&dbitmask, "DYNAMIC_BITMASK_6", DDS_DYNAMIC_BITMASK_POSITION_AUTO);

  // Alias
  dds_dynamic_type_t dalias = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
      .kind = DDS_DYNAMIC_ALIAS,
      .base_type = DDS_DYNAMIC_TYPE_SPEC(dds_dynamic_type_ref (&dseq2)), /* increase ref because type is re-used */
      .name = "dynamic_alias"
  });

  // String types
  dds_dynamic_type_t dstring = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRING8 });
  dds_dynamic_type_t dstring_bounded = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) {
    .kind = DDS_DYNAMIC_STRING8,
    .bounds = (uint32_t[]) { 100 },
    .num_bounds = 1
  });

  dds_dynamic_type_t dstruct = dds_dynamic_type_create (participant, (dds_dynamic_type_descriptor_t) { .kind = DDS_DYNAMIC_STRUCTURE, .name = "dynamic_struct" });
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_INT32, "member_int32"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID_PRIM(DDS_DYNAMIC_FLOAT64, "member_float64", 10 /* has specific member id */));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_PRIM(DDS_DYNAMIC_BOOLEAN, "member_bool"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dds_dynamic_type_ref (&dsubstruct) /* increase ref because type is re-used */, "member_struct"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dsubstruct, "member_struct2"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER_ID(dsubunion, "member_union", 20 /* has specific member id */));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dseq, "member_seq"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(darr, "member_array"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(denum, "member_enum"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dbitmask, "member_bitmask"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dalias, "member_alias"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dstring, "member_string8"));
  dds_dynamic_type_add_member (&dstruct, DDS_DYNAMIC_MEMBER(dstring_bounded, "member_string8_bounded"));

  // Add members at specific index
  dds_dynamic_type_add_member (&dstruct, (dds_dynamic_member_descriptor_t) {
      .type = DDS_DYNAMIC_TYPE_SPEC(dseq2),
      .name = "member_seq2",
      .index = 0,
      .id = 999
  });
  dds_dynamic_type_add_member (&dstruct, (dds_dynamic_member_descriptor_t) {
      .type = DDS_DYNAMIC_TYPE_SPEC(dsubsubstruct),
      .name = "member_substruct",
      .index = 3,
      .id = DDS_DYNAMIC_MEMBER_ID_AUTO
  });

  // Register type and create topic
  dds_typeinfo_t *type_info;
  rc = dds_dynamic_type_register (&dstruct, &type_info);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL ("dds_dynamic_type_register: %s\n", dds_strretcode (-rc));

  dds_topic_descriptor_t *descriptor;
  rc = dds_create_topic_descriptor (DDS_FIND_SCOPE_LOCAL_DOMAIN, participant, type_info, 0, &descriptor);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL ("dds_create_topic_descriptor: %s\n", dds_strretcode (-rc));

  dds_entity_t topic = dds_create_topic (participant, descriptor, "dynamictopic", NULL, NULL);
  if (topic < 0)
    DDS_FATAL ("dds_create_topic: %s\n", dds_strretcode (-topic));

  dds_entity_t writer = dds_create_writer (participant, topic, NULL, NULL);
  if (writer < 0)
    DDS_FATAL ("dds_create_writer: %s\n", dds_strretcode (-writer));

  // Cleanup
  dds_free_typeinfo (type_info);
  dds_delete_topic_descriptor (descriptor);
  dds_dynamic_type_unref (&dstruct);

  printf ("<press enter to exit>\n");
  (void) getchar ();

  /* Deleting the participant will delete all its children recursively as well. */
  rc = dds_delete (participant);
  if (rc != DDS_RETCODE_OK)
    DDS_FATAL("dds_delete: %s\n", dds_strretcode (-rc));

  return EXIT_SUCCESS;
}
