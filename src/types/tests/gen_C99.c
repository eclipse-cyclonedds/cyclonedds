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

#include "parser.h"


bool test_parse_gen_C99(const char *input, const char *str)
{
  char buffer[10000];
  dds_ts_parse_string_gen_C99(input, buffer, 9999);
  if (strstr(buffer, str) != 0) {
    return true;
  }
  printf("Did not find |%s|\n in |%s|\n", str, buffer);
  return false;
}

CU_Test(gen_c99, module1)
{
  CU_ASSERT(test_parse_gen_C99("module a { struct e{char c;};};", "typedef struct a_e\n{\n  char c;\n} a_e;"));
}

CU_Test(gen_c99, module2)
{
  CU_ASSERT(test_parse_gen_C99("module a{struct f{char y;};}; module a { struct e{char x;};};", "typedef struct a_f\n{\n  char y;\n} a_f;"));
  CU_ASSERT(test_parse_gen_C99("module a{struct f{char y;};}; module a { struct e{char x;};};", "typedef struct a_e\n{\n  char x;\n} a_e;"));
}

CU_Test(gen_c99, module3)
{
  CU_ASSERT(test_parse_gen_C99("module x  {module a { struct e{char c;};}; };", "typedef struct x_a_e\n{\n  char c;\n} x_a_e;"));
}

CU_Test(gen_c99, boolean)
{
  CU_ASSERT(test_parse_gen_C99("struct s {boolean c;};","typedef struct s\n{\n  bool c;\n} s;"));
}

CU_Test(gen_c99, wchar)
{
  /* wchar not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {wchar c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, short)
{
  CU_ASSERT(test_parse_gen_C99("struct s {short c;};","typedef struct s\n{\n  int16_t c;\n} s;"));
}

CU_Test(gen_c99, int16)
{
  CU_ASSERT(test_parse_gen_C99("struct s {int16 c;};","typedef struct s\n{\n  int16_t c;\n} s;"));
}

CU_Test(gen_c99, long)
{
  CU_ASSERT(test_parse_gen_C99("struct s {long c;};","typedef struct s\n{\n  int32_t c;\n} s;"));
}

CU_Test(gen_c99, int32)
{
  CU_ASSERT(test_parse_gen_C99("struct s {int32 c;};","typedef struct s\n{\n  int32_t c;\n} s;"));
}

CU_Test(gen_c99, longlong)
{
  CU_ASSERT(test_parse_gen_C99("struct s {long long c;};","typedef struct s\n{\n  int64_t c;\n} s;"));
}

CU_Test(gen_c99, int64)
{
  CU_ASSERT(test_parse_gen_C99("struct s {int64 c;};","typedef struct s\n{\n  int64_t c;\n} s;"));
}

CU_Test(gen_c99, ushort)
{
  CU_ASSERT(test_parse_gen_C99("struct s {unsigned short c;};","typedef struct s\n{\n  uint16_t c;\n} s;"));
}

CU_Test(gen_c99, uint16)
{
  CU_ASSERT(test_parse_gen_C99("struct s {uint16 c;};","typedef struct s\n{\n  uint16_t c;\n} s;"));
}

CU_Test(gen_c99, ulong)
{
  CU_ASSERT(test_parse_gen_C99("struct s {unsigned long c;};","typedef struct s\n{\n  uint32_t c;\n} s;"));
}

CU_Test(gen_c99, uint32)
{
  CU_ASSERT(test_parse_gen_C99("struct s {uint32 c;};","typedef struct s\n{\n  uint32_t c;\n} s;"));
}

CU_Test(gen_c99, ulonglong)
{
  CU_ASSERT(test_parse_gen_C99("struct s {unsigned long long c;};","typedef struct s\n{\n  uint64_t c;\n} s;"));
}

CU_Test(gen_c99, uint64)
{
  CU_ASSERT(test_parse_gen_C99("struct s {uint64 c;};","typedef struct s\n{\n  uint64_t c;\n} s;"));
}

CU_Test(gen_c99, octet)
{
  CU_ASSERT(test_parse_gen_C99("struct s {octet c;};","typedef struct s\n{\n  uint8_t c;\n} s;"));
}

CU_Test(gen_c99, int8)
{
  CU_ASSERT(test_parse_gen_C99("struct s {int8 c;};","typedef struct s\n{\n  int8_t c;\n} s;"));
}

CU_Test(gen_c99, uint8)
{
  CU_ASSERT(test_parse_gen_C99("struct s {uint8 c;};","typedef struct s\n{\n  uint8_t c;\n} s;"));
}

CU_Test(gen_c99, float)
{
  CU_ASSERT(test_parse_gen_C99("struct s {float c;};","typedef struct s\n{\n  float c;\n} s;"));
}

CU_Test(gen_c99, double)
{
  CU_ASSERT(test_parse_gen_C99("struct s {double c;};","typedef struct s\n{\n  double c;\n} s;"));
}

CU_Test(gen_c99, longdouble)
{
  /* not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {long double c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, short_sequence)
{
  CU_ASSERT(test_parse_gen_C99("struct s {sequence<short> c;};","typedef struct s\n{\n  dds_sequence_t c;\n} s;"));
}

CU_Test(gen_c99, short_bounded_sequence)
{
  CU_ASSERT(test_parse_gen_C99("struct s {sequence<short,7> c;};","typedef struct s\n{\n  dds_sequence_t c;\n} s;"));
}

CU_Test(gen_c99, string)
{
  CU_ASSERT(test_parse_gen_C99("struct s {string c;};","typedef struct s\n{\n  char * c;\n} s;"));
}

CU_Test(gen_c99, bounded_string)
{
  CU_ASSERT(test_parse_gen_C99("struct s {string<9> c;};","typedef struct s\n{\n  char c[10];\n} s;"));
}

CU_Test(gen_c99, wstring)
{
  /* not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {wstring c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, bounded_wstring)
{
  /* not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {wstring<9> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, fixed)
{
  /* not supported yet */
  CU_ASSERT(test_parse_gen_C99("struct s {fixed<5,3> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, map)
{
  CU_ASSERT(test_parse_gen_C99("struct s {map<short,char> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, bounded_map)
{
  CU_ASSERT(test_parse_gen_C99("struct s {map<short,char,5> c;};","typedef struct s\n{\n  // type not supported: c;\n} s;"));
}

CU_Test(gen_c99, two_members)
{
  CU_ASSERT(test_parse_gen_C99("struct s {char c,b;};","typedef struct s\n{\n  char c;\n  char b;\n} s;"));
}

CU_Test(gen_c99, sequence_struct)
{
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{sequence<x> a;};",
    "typedef struct x\n{\n  char c;\n} x;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{sequence<x> a;};",
    "typedef struct m_a_seq\n{\n  uint32_t _maximum;\n  uint32_t _length;\n  x *_buffer;\n  bool _release;\n} m_a_seq;"));
  CU_ASSERT(test_parse_gen_C99("struct x{char c;};struct m{sequence<x> a;};",
    "typedef struct m\n{\n  m_a_seq a;\n} m;"));
}

CU_Test(gen_c99, inline_struct)
{
  CU_ASSERT(test_parse_gen_C99("struct x{struct y{char c;}a;};",
    "typedef struct x_y\n{\n  char c;\n} x_y;"));
  CU_ASSERT(test_parse_gen_C99("struct x{struct y{char c;}a;};",
    "typedef struct x\n{\n  x_y a;\n} x;"));
}

CU_Test(gen_c99, array)
{
  CU_ASSERT(test_parse_gen_C99("struct x{char a[10][3];};", "typedef struct x\n{\n  char a[10][3];\n} x;"));
}

CU_Test(gen_c99, recursive)
{
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<x> a;};",
    "typedef struct x_a_seq\n{\n  uint32_t _maximum;\n  uint32_t _length;\n  x *_buffer;\n  bool _release;\n} x_a_seq;"));
  CU_ASSERT(test_parse_gen_C99("struct x{sequence<x> a;};",
    "typedef struct x\n{\n  x_a_seq a;\n} x;"));
}


