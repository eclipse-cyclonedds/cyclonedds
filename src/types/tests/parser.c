/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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


static bool test_parse_stringify(const char *input, const char *output)
{
  char buffer[1000];
  dds_ts_parse_string_stringify(input, buffer, 500);
  if (strcmp(buffer, output) == 0)
  {
    return true;
  }
  /* In case of a difference, print some information (for debugging) */
  printf("Expect:   |%s|\n"
         "Returned: |%s|\n", buffer, output);
  return false;
}

CU_Test(parser, module2)
{
  CU_ASSERT(test_parse_stringify("module a { struct e{char c;};};", "module a{struct e{char c,;}}"));
}

CU_Test(parser, module3)
{
  CU_ASSERT(test_parse_stringify("module a{struct f{char y;};}; module a { struct e{char x;};};", "module a{struct f{char y,;}}module a{struct e{char x,;}}"));
}

CU_Test(parser, module4)
{
  CU_ASSERT(test_parse_stringify("module x  {module a { struct e{char c;};}; };", "module x{module a{struct e{char c,;}}}"));
}

CU_Test(parser, module5)
{
  CU_ASSERT(test_parse_stringify("struct s {boolean c;};","struct s{bool c,;}"));
}

CU_Test(parser, module6)
{
  CU_ASSERT(test_parse_stringify("struct s {char c;};","struct s{char c,;}"));
}

CU_Test(parser, module7)
{
  CU_ASSERT(test_parse_stringify("struct s {wchar c;};","struct s{wchar c,;}"));
}

CU_Test(parser, module8)
{
  CU_ASSERT(test_parse_stringify("struct s {short c;};","struct s{short c,;}"));
}

CU_Test(parser, module9)
{
  CU_ASSERT(test_parse_stringify("struct s {int16 c;};","struct s{short c,;}"));
}

CU_Test(parser, module10)
{
  CU_ASSERT(test_parse_stringify("struct s {long c;};","struct s{long c,;}"));
}

CU_Test(parser, module11)
{
  CU_ASSERT(test_parse_stringify("struct s {int32 c;};","struct s{long c,;}"));
}

CU_Test(parser, module12)
{
  CU_ASSERT(test_parse_stringify("struct s {long long c;};","struct s{long long c,;}"));
}

CU_Test(parser, module13)
{
  CU_ASSERT(test_parse_stringify("struct s {int64 c;};","struct s{long long c,;}"));
}

CU_Test(parser, module14)
{
  CU_ASSERT(test_parse_stringify("struct s {unsigned short c;};","struct s{unsigned short c,;}"));
}

CU_Test(parser, module15)
{
  CU_ASSERT(test_parse_stringify("struct s {uint16 c;};","struct s{unsigned short c,;}"));
}

CU_Test(parser, module16)
{
  CU_ASSERT(test_parse_stringify("struct s {unsigned long c;};","struct s{unsigned long c,;}"));
}

CU_Test(parser, module17)
{
  CU_ASSERT(test_parse_stringify("struct s {uint32 c;};","struct s{unsigned long c,;}"));
}

CU_Test(parser, module18)
{
  CU_ASSERT(test_parse_stringify("struct s {unsigned long long c;};","struct s{unsigned long long c,;}"));
}

CU_Test(parser, module19)
{
  CU_ASSERT(test_parse_stringify("struct s {uint64 c;};","struct s{unsigned long long c,;}"));
}

CU_Test(parser, module20)
{
  CU_ASSERT(test_parse_stringify("struct s {octet c;};","struct s{octet c,;}"));
}

CU_Test(parser, module21)
{
  CU_ASSERT(test_parse_stringify("struct s {int8 c;};","struct s{int8 c,;}"));
}

CU_Test(parser, module22)
{
  CU_ASSERT(test_parse_stringify("struct s {uint8 c;};","struct s{uint8 c,;}"));
}

CU_Test(parser, module23)
{
  CU_ASSERT(test_parse_stringify("struct s {float c;};","struct s{float c,;}"));
}

CU_Test(parser, module24)
{
  CU_ASSERT(test_parse_stringify("struct s {double c;};","struct s{double c,;}"));
}

CU_Test(parser, module25)
{
  CU_ASSERT(test_parse_stringify("struct s {long double c;};","struct s{long double c,;}"));
}

CU_Test(parser, module26)
{
  CU_ASSERT(test_parse_stringify("struct s {sequence<short> c;};","struct s{sequence<short> c,;}"));
}

CU_Test(parser, module27)
{
  CU_ASSERT(test_parse_stringify("struct s {sequence<short,7> c;};","struct s{sequence<short,7> c,;}"));
}

CU_Test(parser, module28)
{
  CU_ASSERT(test_parse_stringify("struct s {string c;};","struct s{string c,;}"));
}

CU_Test(parser, module29)
{
  CU_ASSERT(test_parse_stringify("struct s {string<9> c;};","struct s{string<9> c,;}"));
}

CU_Test(parser, module30)
{
  CU_ASSERT(test_parse_stringify("struct s {wstring c;};","struct s{wstring c,;}"));
}

CU_Test(parser, module31)
{
  CU_ASSERT(test_parse_stringify("struct s {wstring<9> c;};","struct s{wstring<9> c,;}"));
}

CU_Test(parser, module32)
{
  CU_ASSERT(test_parse_stringify("struct s {fixed<5,3> c;};","struct s{fixed<5,3> c,;}"));
}

CU_Test(parser, module33)
{
  CU_ASSERT(test_parse_stringify("struct s {map<short,char> c;};","struct s{map<short,char> c,;}"));
}

CU_Test(parser, module34)
{
  CU_ASSERT(test_parse_stringify("struct s {map<short,char,5> c;};","struct s{map<short,char,5> c,;}"));
}

CU_Test(parser, module35)
{
  CU_ASSERT(test_parse_stringify("struct s {char c,b;};","struct s{char c,b,;}"));
}

CU_Test(parser, module36)
{
  CU_ASSERT(test_parse_stringify("struct s {char c;wchar d,e;};","struct s{char c,;wchar d,e,;}"));
}

CU_Test(parser, module37)
{
  CU_ASSERT(test_parse_stringify("struct a{char c;};struct b{sequence<a> s;};",
                                 "struct a{char c,;}struct b{sequence<a> s,;}"));
}
