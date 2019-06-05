/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "CUnit/Test.h"
#include "dds/ddsts/typetree.h"
#include "parser.h"
#include "gen_c99.h"

static bool test_parse_gen_C99(const char *input, const char *str)
{
  ddsts_type_t *root_type = NULL;
  if (ddsts_idl_parse_string(input, &root_type) != DDS_RETCODE_OK) {
    return false;
  }

  char buffer[10000];

  ddsts_generate_C99_to_buffer("test.idl", root_type, buffer, 9999);

  ddsts_free_type(root_type);

  if (strstr(buffer, str) != 0) {
    return true;
  }
  printf("Did not find |%s|\n in |%s|\n", str, buffer);
  return false;
}

static bool test_parse_gen_C99_descr(const char *input, const char *name, const char *align, const char *flags)
{
  ddsts_type_t *root_type = NULL;
  if (ddsts_idl_parse_string(input, &root_type) != DDS_RETCODE_OK) {
    return false;
  }

  char buffer[10000];

  ddsts_generate_C99_to_buffer("test.idl", root_type, buffer, 9999);

  ddsts_free_type(root_type);

  char sizeof_name[40];
  snprintf(sizeof_name, 39, "sizeof (%s),\n  ", name);
  const char *s = strstr(buffer, sizeof_name);
  if (s == NULL) {
    printf("Did not find |%s| in |%s|\n", sizeof_name, buffer);
    return false;
  }
  s += strlen(sizeof_name);
  if (strncmp(s, align, strlen(align)) != 0 || s[strlen(align)] != ',') {
    printf("Did not find alignment |%s| at |%s|\n", align, s);
    return false;
  }
  s += strlen(align) + 4;
  if (strncmp(s, flags, strlen(flags)) != 0 || s[strlen(flags)] != ',') {
    printf("Did not find flags |%s| at |%s|\n", flags, s);
    return false;
  }
  return true;
}

CU_Test(gen_c99, module)
{
  CU_ASSERT(test_parse_gen_C99("module a { struct e{@key char c;};};",
    "typedef struct a_e\n{\n  char c;\n} a_e;"));
  CU_ASSERT(test_parse_gen_C99("module a { struct e{@key char c;};};",
    "DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (a_e, c),\n"));
  CU_ASSERT(test_parse_gen_C99("module a { struct e{@key char c;};};",
    "<MetaData version=\\\"1.0.0\\\"><Module name=\\\"a\\\"><Struct name=\\\"e\\\"><Member name=\\\"c\\\"><Char/></Member></Struct></Module></MetaData>"));
  CU_ASSERT(test_parse_gen_C99_descr("module a { struct e{@key char c;};};", "a_e", "1u", "DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("module a{struct f{char y;};}; module a { struct e{char x;};};", "typedef struct a_f\n{\n  char y;\n} a_f;"));
  CU_ASSERT(test_parse_gen_C99("module a{struct f{char y;};}; module a { struct e{char x;};};", "typedef struct a_e\n{\n  char x;\n} a_e;"));
  CU_ASSERT(test_parse_gen_C99("module x  {module a { struct e{char c;};}; };", "typedef struct x_a_e\n{\n  char c;\n} x_a_e;"));
}

CU_Test(gen_c99, base_types)
{
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c;};","typedef struct s\n{\n  char c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c;};","DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c;};","<Member name=\\\"c\\\"><Char/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key char c;};","s","1u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key boolean c;};","typedef struct s\n{\n  bool c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key boolean c;};","DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key boolean c;};","<Member name=\\\"c\\\"><Boolean/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key boolean c;};","s","(sizeof(bool)>1u)?sizeof(bool):1u","DDS_TOPIC_FIXED_KEY"));
  /* wchar not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {wchar c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key short c;};","typedef struct s\n{\n  int16_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key short c;};","DDS_OP_ADR | DDS_OP_TYPE_2BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key short c;};","<Member name=\\\"c\\\"><Short/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key short c;};","s","2u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int16 c;};","typedef struct s\n{\n  int16_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int16 c;};","DDS_OP_ADR | DDS_OP_TYPE_2BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int16 c;};","<Member name=\\\"c\\\"><Short/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key int16 c;};","s","2u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key long c;};","typedef struct s\n{\n  int32_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key long c;};","DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key long c;};","<Member name=\\\"c\\\"><Long/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key long c;};","s","4u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int32 c;};","typedef struct s\n{\n  int32_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int32 c;};","DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int32 c;};","<Member name=\\\"c\\\"><Long/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key int32 c;};","s","4u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key long long c;};","typedef struct s\n{\n  int64_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key long long c;};","DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key long long c;};","<Member name=\\\"c\\\"><LongLong/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key long long c;};","s","8u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int64 c;};","typedef struct s\n{\n  int64_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int64 c;};","DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int64 c;};","<Member name=\\\"c\\\"><LongLong/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key int64 c;};","s","8u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned short c;};","typedef struct s\n{\n  uint16_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned short c;};","DDS_OP_ADR | DDS_OP_TYPE_2BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned short c;};","<Member name=\\\"c\\\"><UShort/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key unsigned short c;};","s","2u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint16 c;};","typedef struct s\n{\n  uint16_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint16 c;};","DDS_OP_ADR | DDS_OP_TYPE_2BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint16 c;};","<Member name=\\\"c\\\"><UShort/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key uint16 c;};","s","2u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned long c;};","typedef struct s\n{\n  uint32_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned long c;};","DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned long c;};","<Member name=\\\"c\\\"><ULong/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key unsigned long c;};","s","4u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint32 c;};","typedef struct s\n{\n  uint32_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint32 c;};","DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint32 c;};","<Member name=\\\"c\\\"><ULong/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key uint32 c;};","s","4u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned long long c;};","typedef struct s\n{\n  uint64_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned long long c;};","DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key unsigned long long c;};","<Member name=\\\"c\\\"><ULongLong/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key unsigned long long c;};","s","8u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint64 c;};","typedef struct s\n{\n  uint64_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint64 c;};","DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint64 c;};","<Member name=\\\"c\\\"><ULongLong/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key uint64 c;};","s","8u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key octet c;};","typedef struct s\n{\n  uint8_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key octet c;};","DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key octet c;};","<Member name=\\\"c\\\"><Octet/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key octet c;};","s","1u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int8 c;};","typedef struct s\n{\n  int8_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int8 c;};","DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key int8 c;};","<Member name=\\\"c\\\"><Int8/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key int8 c;};","s","1u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint8 c;};","typedef struct s\n{\n  uint8_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint8 c;};","DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key uint8 c;};","<Member name=\\\"c\\\"><UInt8/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key uint32 c;};","s","4u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key float c;};","typedef struct s\n{\n  float c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key float c;};","DDS_OP_ADR | DDS_OP_TYPE_4BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key float c;};","<Member name=\\\"c\\\"><Float/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key float c;};","s","4u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key double c;};","typedef struct s\n{\n  double c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key double c;};","DDS_OP_ADR | DDS_OP_TYPE_8BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key double c;};","<Member name=\\\"c\\\"><Double/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key double c;};","s","8u","DDS_TOPIC_FIXED_KEY"));
  /* not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {long double c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char x; long double c;};","<Member name=\\\"c\\\"><LongDouble/></Member>"));
  /* not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {fixed<5,3> c; @key char x;};","typedef struct s\n{\n  // type not supported: c;\n  char x;\n} s;"));
}

CU_Test(gen_c99, sequence)
{
  /* sequence of short */
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;sequence<short> c;};","typedef struct s\n{\n  char k;\n  dds_sequence_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;sequence<short> c;};","DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_2BY, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;sequence<short> c;};","<Member name=\\\"c\\\"><Sequence><Short/></Sequence></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key char k;sequence<short> c;};","s","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;sequence<short,7> c;};","typedef struct s\n{\n  char k;\n  dds_sequence_t c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;sequence<short,7> c;};","<Member name=\\\"c\\\"><Sequence size=\\\"7\\\"><Short/></Sequence></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s {@key char k;sequence<short,7> c;};","s","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  /* sequence of struct */
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{@key char k;sequence<x> a;};",
    "typedef struct x\n{\n  char c;\n} x;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{@key char k;sequence<x> a;};",
    "typedef struct m_a_seq\n{\n  uint32_t _maximum;\n  uint32_t _length;\n  x *_buffer;\n  bool _release;\n} m_a_seq;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{@key char k;sequence<x> a;};",
    "typedef struct m\n{\n  char k;\n  m_a_seq a;\n} m;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{@key char k;sequence<x> a;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (m, a),\n"
    "  sizeof (x), (7u << 16u) + 4u,\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (x, c),\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{@key char k;sequence<x> a;};",
    "<Member name=\\\"a\\\"><Sequence><Type name=\\\"x\\\"/></Sequence></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{char c;};struct m{@key char k;sequence<x> a;};","m","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  /* sequence of forward struct */
  CU_ASSERT(test_parse_gen_C99("struct x;struct m{@key char k;sequence<x> a;};struct x{char c;};",
    "typedef struct x\n{\n  char c;\n} x;"));
  CU_ASSERT(test_parse_gen_C99("struct x;struct m{@key char k;sequence<x> a;};struct x{char c;};",
    "typedef struct m_a_seq\n{\n  uint32_t _maximum;\n  uint32_t _length;\n  x *_buffer;\n  bool _release;\n} m_a_seq;"));
  CU_ASSERT(test_parse_gen_C99("struct x;struct m{@key char k;sequence<x> a;};struct x{char c;};",
    "typedef struct m\n{\n  char k;\n  m_a_seq a;\n} m;"));
  CU_ASSERT(test_parse_gen_C99("struct x;struct m{@key char k;sequence<x> a;};struct x{char c;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (m, a),\n"
    "  sizeof (x), (7u << 16u) + 4u,\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (x, c),\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x;struct m{@key char k;sequence<x> a;};struct x{char c;};",
    "<Member name=\\\"a\\\"><Sequence><Type name=\\\"x\\\"/></Sequence></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x;struct m{@key char k;sequence<x> a;};struct x{char c;};","m","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  /* sequence of sequence */
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<sequence<char> > cs;@key char k;};",
    "typedef struct x_cs_seq\n{\n  uint32_t _maximum;\n  uint32_t _length;\n  dds_sequence_t *_buffer;\n  bool _release;\n} x_cs_seq;\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<sequence<char> > cs;@key char k;};",
    "typedef struct x\n{\n  x_cs_seq cs;\n  char k;\n} x;\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<sequence<char> > cs;@key char k;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_SEQ, offsetof (x, cs),\n"
    "  sizeof (dds_sequence_t), (7u << 16u) + 4u,\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY, 0u,\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<sequence<char> > cs;@key char k;};",
    "<Member name=\\\"cs\\\"><Sequence><Sequence><Char/></Sequence></Sequence></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{sequence<sequence<char> > cs;@key char k;};",
    "x","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  /* map is not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {map<short,char> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {map<short,char,5> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, string)
{
  CU_ASSERT(test_parse_gen_C99("struct s {string c;};","typedef struct s\n{\n  char * c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;string c;};","DDS_OP_ADR | DDS_OP_TYPE_STR, offsetof (s, c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;string c;};","</Member><Member name=\\\"c\\\"><String/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s{@key char k;string c;};","s","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s{char k;@key string c;};","s","sizeof (char *)","DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99("struct s {string<9> c;};","typedef struct s\n{\n  char c[10];\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;string<9> c;};","DDS_OP_ADR | DDS_OP_TYPE_BST, offsetof (s, c), 10,\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char k;string<9> c;};","<Member name=\\\"c\\\"><String length=\\\"9\\\"/></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s{@key char k;string<9> c;};","s","1u","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s{char k;@key string<9> c;};","s","1u","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  /* wstring not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {wstring c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {wstring<9> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, structs)
{
  /* struct with two key fields */
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c,b;};","typedef struct s\n{\n  char c;\n  char b;\n} s;"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c,b;};","{ \"c\", 0 },\n  { \"b\", 2 }\n}"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c,b;};","DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, c),\n  DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (s, b),\n"));
  CU_ASSERT(test_parse_gen_C99("struct s {@key char c,b;};","<Struct name=\\\"s\\\"><Member name=\\\"c\\\"><Char/></Member><Member name=\\\"b\\\"><Char/></Member></Struct>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s{@key char c,b;};","s","1u","DDS_TOPIC_FIXED_KEY"));
  /* embedded struct */
  CU_ASSERT(test_parse_gen_C99("struct x{struct y{char c;}a;};",
    "typedef struct x_y\n{\n  char c;\n} x_y;"));
  CU_ASSERT(test_parse_gen_C99("struct x{struct y{char c;}a;};",
    "typedef struct x\n{\n  x_y a;\n} x;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char k;struct y{char c;}a;};\n#pragma keylist x k\n",
    "DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (x, a.c),\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{char k;struct y{char c;}a;};\n#pragma keylist x k\n",
    "</Member><Member name=\\\"a\\\"><Struct name=\\\"y\\\"><Member name=\\\"c\\\"><Char/></Member"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{char k;struct y{char c;}a;};\n#pragma keylist x k\n","x","1u","DDS_TOPIC_FIXED_KEY"));
  /* usage of a (struct) type from a different module */
  CU_ASSERT(test_parse_gen_C99("module a{struct x{char c;};};module b{struct y{a::x d;char k;};\n#pragma keylist y k\n};",
    "typedef struct b_y\n{\n  a_x d;\n  char k;\n} b_y;"));
  CU_ASSERT(test_parse_gen_C99("module a{struct x{char c;};};module b{struct y{a::x d;char k;};\n#pragma keylist y k\n};",
    "DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (b_y, d.c),\n"));
  CU_ASSERT(test_parse_gen_C99("module a{struct x{char c;};};module b{struct y{a::x d;char k;};\n#pragma keylist y k\n};",
    "<Module name=\\\"a\\\"><Struct name=\\\"x\\\"><Member name=\\\"c\\\"><Char/></Member></Struct></Module><Module name=\\\"b\\\"><Struct name=\\\"y\\\"><Member name=\\\"d\\\"><Type name=\\\"::a::x\\\"/></Member><Member name=\\\"k\\\"><Char/></Member></Struct></Module>"));
  CU_ASSERT(test_parse_gen_C99("module a{struct x{char c;};};module b{struct y{a::x d;char k;};\n#pragma keylist y k\n};",
      "DDS_OP_ADR | DDS_OP_TYPE_1BY, offsetof (b_y, d.c),\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_1BY | DDS_OP_FLAG_KEY, offsetof (b_y, k),\n"));
  CU_ASSERT(test_parse_gen_C99_descr("module a{struct x{char c;};};module b{struct y{a::x d;char k;};\n#pragma keylist y k\n};","b_y","1u","DDS_TOPIC_FIXED_KEY"));
}

CU_Test(gen_c99, array)
{
  CU_ASSERT(test_parse_gen_C99("struct x{char a[10][3];};",
    "typedef struct x\n{\n  char a[10][3];\n} x;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char a[10][3];};\n#pragma keylist x a\n",
    "DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_1BY | DDS_OP_FLAG_KEY, offsetof (x, a), 30,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{char a[10][3];};\n#pragma keylist x a\n",
    "<Member name=\\\"a\\\"><Array size=\\\"10\\\"><Array size=\\\"3\\\"><Char/></Array></Array></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{char a[10][3];};\n#pragma keylist x a\n","x","1u","0u"));
  CU_ASSERT(test_parse_gen_C99("struct x{string a[5];@key char k;};",
    "typedef struct x\n{\n  char * a[5];\n  char k;\n} x;\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{string a[5];@key char k;};",
    "DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STR, offsetof (x, a), 5,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{string a[5];@key char k;};",
    "<Member name=\\\"a\\\"><Array size=\\\"5\\\"><String/></Array></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{string a[5];@key char k;};",
    "x","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<char> a[5];char k;};",
    "typedef struct x\n{\n  dds_sequence_t a[5];\n  char k;\n} x;\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<char> a[5];@key char k;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_SEQ, offsetof (x, a), 5,\n"
    "  (8u << 16u) + 5u, sizeof (dds_sequence_t),\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY, 0u,\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<char> a[5];@key char k;};",
    "<Member name=\\\"a\\\"><Array size=\\\"5\\\"><Sequence><Char/></Sequence></Array></Member>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{sequence<char> a[5];@key char k;};",
    "x","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99("struct s{sequence<char> cs;};struct x{s a[5];@key char k;};",
    "typedef struct x\n{\n  s a[5];\n  char k;\n} x;\n"));
  CU_ASSERT(test_parse_gen_C99("struct s{sequence<char> cs;};struct x{s a[5];@key char k;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_ARR | DDS_OP_SUBTYPE_STU, offsetof (x, a), 5,\n"
    "  (8u << 16u) + 5u, sizeof (s),\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_1BY, offsetof (s, cs),\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct s{sequence<char> cs;};struct x{s a[5];@key char k;};",
    "<Member name=\\\"a\\\"><Array size=\\\"5\\\"><Type name=\\\"s\\\"/></Array>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct s{sequence<char> cs;};struct x{s a[5];@key char k;};",
    "x","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  /* fixed topic key when key is at most 16 bytes */
  CU_ASSERT(test_parse_gen_C99_descr("struct x{char a[16];};\n#pragma keylist x a\n","x","1u","DDS_TOPIC_FIXED_KEY"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{char a[17];};\n#pragma keylist x a\n","x","1u","0u"));
}

CU_Test(gen_c99, recursive)
{
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<x> a;};",
    "typedef struct x_a_seq\n{\n  uint32_t _maximum;\n  uint32_t _length;\n  x *_buffer;\n  bool _release;\n} x_a_seq;"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<x> a;};",
    "typedef struct x\n{\n  x_a_seq a;\n} x;"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<x> a;@key char k;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (x, a),\n"
    "  sizeof (x), (7u << 16u) + 4u,\n"
    "  DDS_OP_JSR, (uint32_t)-4,\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<x> a;@key char k;};",
    "<Struct name=\\\"x\\\"><Member name=\\\"a\\\"><Sequence><Type name=\\\"x\\\"/></Sequence></Member><Member name=\\\"k\\\"><Char/></Member></Struct>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct x{sequence<x> a;@key char k;};","x","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
  CU_ASSERT(test_parse_gen_C99("struct a; struct b{sequence<a> as;}; struct a{char k;sequence<b> bs;};",
    "typedef struct b\n{\n  b_as_seq as;\n} b;\n"));
  CU_ASSERT(test_parse_gen_C99("struct a; struct b{sequence<a> as;}; struct a{char k;sequence<b> bs;};",
    "typedef struct a\n{\n  char k;\n  a_bs_seq bs;\n} a;\n"));
  CU_ASSERT(test_parse_gen_C99("struct a; struct b{sequence<a> as;}; struct a{@key char k;sequence<b> bs;};",
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (a, bs),\n"
    "  sizeof (b), (12u << 16u) + 4u,\n"
    "  DDS_OP_ADR | DDS_OP_TYPE_SEQ | DDS_OP_SUBTYPE_STU, offsetof (b, as),\n"
    "  sizeof (a), (7u << 16u) + 4u,\n"
    "  DDS_OP_JSR, (uint32_t)-10,\n"
    "  DDS_OP_RTS,\n"
    "  DDS_OP_RTS,\n"));
  CU_ASSERT(test_parse_gen_C99("struct a; struct b{sequence<a> as;}; struct a{@key char k;sequence<b> bs;};",
    "<Struct name=\\\"b\\\"><Member name=\\\"as\\\"><Sequence><Type name=\\\"a\\\"/></Sequence></Member></Struct><Struct name=\\\"a\\\"><Member name=\\\"k\\\"><Char/></Member><Member name=\\\"bs\\\"><Sequence><Type name=\\\"b\\\"/></Sequence></Member></Struct>"));
  CU_ASSERT(test_parse_gen_C99_descr("struct a; struct b{sequence<a> as;}; struct a{@key char k;sequence<b> bs;};",
    "a","sizeof (char *)","DDS_TOPIC_FIXED_KEY | DDS_TOPIC_NO_OPTIMIZE"));
}


