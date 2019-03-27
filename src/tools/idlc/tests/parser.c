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
#include "dds/ddsts/typetree.h"
#include "parser.h"

static bool test_type(ddsts_type_t *type, ddsts_flags_t flags, const char *name, ddsts_type_t *parent, bool next_is_null)
{
  return    type != NULL && DDSTS_IS_TYPE(type, flags) && (name == NULL ? type->type.name == NULL : type->type.name != NULL && strcmp(type->type.name, name) == 0)
         && type->type.parent == parent
         && (next_is_null == (type->type.next == NULL));
}

static void test_basic_type(const char *idl, ddsts_flags_t flags)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string(idl, &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *struct_s = root_type->module.members;
    CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", root_type, true));
      ddsts_type_t *decl_c = struct_s->struct_def.members;
      CU_ASSERT(test_type(decl_c, DDSTS_DECLARATION, "c", struct_s, true));
        ddsts_type_t *char_type = decl_c->declaration.decl_type;
        CU_ASSERT(test_type(char_type, flags, NULL, decl_c, true));
  ddsts_free_type(root_type);
}

CU_Test(parser, basic_types)
{
  test_basic_type("struct s{boolean c;};", DDSTS_BOOLEAN);
  test_basic_type("struct s{char c;};", DDSTS_CHAR);
  test_basic_type("struct s{wchar c;};", DDSTS_WIDE_CHAR);
  test_basic_type("struct s{short c;};", DDSTS_SHORT);
  test_basic_type("struct s{int16 c;};", DDSTS_SHORT);
  test_basic_type("struct s{long c;};", DDSTS_LONG);
  test_basic_type("struct s{int32 c;};", DDSTS_LONG);
  test_basic_type("struct s{long long c;};", DDSTS_LONGLONG);
  test_basic_type("struct s{int64 c;};", DDSTS_LONGLONG);
  test_basic_type("struct s{unsigned short c;};", DDSTS_USHORT);
  test_basic_type("struct s{uint16 c;};", DDSTS_USHORT);
  test_basic_type("struct s{unsigned long c;};", DDSTS_ULONG);
  test_basic_type("struct s{uint32 c;};", DDSTS_ULONG);
  test_basic_type("struct s{unsigned long long c;};", DDSTS_ULONGLONG);
  test_basic_type("struct s{uint64 c;};", DDSTS_ULONGLONG);
  test_basic_type("struct s{octet c;};", DDSTS_OCTET);
  test_basic_type("struct s{int8 c;};", DDSTS_INT8);
  test_basic_type("struct s{uint8 c;};", DDSTS_UINT8);
  test_basic_type("struct s{float c;};", DDSTS_FLOAT);
  test_basic_type("struct s{double c;};", DDSTS_DOUBLE);
  test_basic_type("struct s{long double c;};", DDSTS_LONGDOUBLE);
}

CU_Test(parser, one_module1)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string("module a{ struct s{char c;};};", &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *module_a = root_type->module.members;
    CU_ASSERT(test_type(module_a, DDSTS_MODULE, "a", root_type, true));
    CU_ASSERT(module_a->module.previous == NULL);
      ddsts_type_t *struct_s = module_a->module.members;
      CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", module_a, true));
        ddsts_type_t *decl_c = struct_s->struct_def.members;
        CU_ASSERT(test_type(decl_c, DDSTS_DECLARATION, "c", struct_s, true));
          ddsts_type_t *char_type = decl_c->declaration.decl_type;
          CU_ASSERT(test_type(char_type, DDSTS_CHAR, NULL, decl_c, true));
  ddsts_free_type(root_type);
}

CU_Test(parser, reopen_module)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string("module a{ struct s{char c;};}; module a { struct t{char x;};};", &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *module_a = root_type->module.members;
    CU_ASSERT(test_type(module_a, DDSTS_MODULE, "a", root_type, false));
    CU_ASSERT(module_a->module.previous == NULL);
    {
      ddsts_type_t *struct_s = module_a->module.members;
      CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", module_a, true));
        ddsts_type_t *decl_c = struct_s->struct_def.members;
        CU_ASSERT(test_type(decl_c, DDSTS_DECLARATION, "c", struct_s, true));
          ddsts_type_t *char_type = decl_c->declaration.decl_type;
          CU_ASSERT(test_type(char_type, DDSTS_CHAR, NULL, decl_c, true));
    }
    ddsts_type_t *module_a2 = module_a->type.next;
    CU_ASSERT(test_type(module_a2, DDSTS_MODULE, "a", root_type, true));
    CU_ASSERT(module_a2->module.previous == &module_a->module);
    {
      ddsts_type_t *struct_t = module_a2->module.members;
      CU_ASSERT(test_type(struct_t, DDSTS_STRUCT, "t", module_a2, true));
        ddsts_type_t *decl_x = struct_t->struct_def.members;
        CU_ASSERT(test_type(decl_x, DDSTS_DECLARATION, "x", struct_t, true));
          ddsts_type_t *char_type = decl_x->declaration.decl_type;
          CU_ASSERT(test_type(char_type, DDSTS_CHAR, NULL, decl_x, true));
    }
  ddsts_free_type(root_type);
}

CU_Test(parser, scoped_name)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string("module a{ struct s{char c;};}; module b { struct t{a::s x;};};", &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *module_a = root_type->module.members;
    CU_ASSERT(test_type(module_a, DDSTS_MODULE, "a", root_type, false));
    CU_ASSERT(module_a->module.previous == NULL);
      ddsts_type_t *struct_s = module_a->module.members;
      CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", module_a, true));
        ddsts_type_t *decl_c = struct_s->struct_def.members;
        CU_ASSERT(test_type(decl_c, DDSTS_DECLARATION, "c", struct_s, true));
          ddsts_type_t *char_type = decl_c->declaration.decl_type;
          CU_ASSERT(test_type(char_type, DDSTS_CHAR, NULL, decl_c, true));
    ddsts_type_t *module_b = module_a->type.next;
    CU_ASSERT(test_type(module_b, DDSTS_MODULE, "b", root_type, true));
    CU_ASSERT(module_b->module.previous == NULL);
    {
      ddsts_type_t *struct_t = module_b->module.members;
      CU_ASSERT(test_type(struct_t, DDSTS_STRUCT, "t", module_b, true));
        ddsts_type_t *decl_x = struct_t->struct_def.members;
        CU_ASSERT(test_type(decl_x, DDSTS_DECLARATION, "x", struct_t, true));
          ddsts_type_t *x_type = decl_x->declaration.decl_type;
          CU_ASSERT(x_type == struct_s);
    }
  ddsts_free_type(root_type);
}

CU_Test(parser, comma)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a, b;};", &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *struct_s = root_type->module.members;
    CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", root_type, true));
      ddsts_type_t *decl_a = struct_s->struct_def.members;
      CU_ASSERT(test_type(decl_a, DDSTS_DECLARATION, "a", struct_s, false));
        ddsts_type_t *char_type = decl_a->declaration.decl_type;
        CU_ASSERT(test_type(char_type, DDSTS_CHAR, NULL, decl_a, true));
      ddsts_type_t *decl_b = decl_a->type.next;
      CU_ASSERT(test_type(decl_b, DDSTS_DECLARATION, "b", struct_s, true));
        CU_ASSERT(decl_b->declaration.decl_type == char_type);
  ddsts_free_type(root_type);
}

CU_Test(parser, types)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence<char> us; sequence<char,8> bs; string ust; string<7> bst; wstring uwst; wstring<6> bwst; fixed<5,3> fp; map<short,char> um; map<short,char,5> bm;};", &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *struct_s = root_type->module.members;
    CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", root_type, true));
      ddsts_type_t *decl = struct_s->struct_def.members;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "us", struct_s, false));
        ddsts_type_t *s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_SEQUENCE, NULL, decl, true));
        CU_ASSERT(s_type->sequence.max == 0);
        CU_ASSERT(DDSTS_IS_UNBOUND(s_type));
          ddsts_type_t *elem_type = s_type->sequence.element_type;
          CU_ASSERT(test_type(elem_type, DDSTS_CHAR, NULL, s_type, true));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "bs", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_SEQUENCE, NULL, decl, true));
        CU_ASSERT(s_type->sequence.max == 8);
        CU_ASSERT(!DDSTS_IS_UNBOUND(s_type));
          elem_type = s_type->sequence.element_type;
          CU_ASSERT(test_type(elem_type, DDSTS_CHAR, NULL, s_type, true));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "ust", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_STRING, NULL, decl, true));
        CU_ASSERT(s_type->string.max == 0);
        CU_ASSERT(DDSTS_IS_UNBOUND(s_type));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "bst", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_STRING, NULL, decl, true));
        CU_ASSERT(s_type->string.max == 7);
        CU_ASSERT(!DDSTS_IS_UNBOUND(s_type));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "uwst", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_WIDE_STRING, NULL, decl, true));
        CU_ASSERT(s_type->string.max == 0);
        CU_ASSERT(DDSTS_IS_UNBOUND(s_type));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "bwst", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_WIDE_STRING, NULL, decl, true));
        CU_ASSERT(s_type->string.max == 6);
        CU_ASSERT(!DDSTS_IS_UNBOUND(s_type));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "fp", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_FIXED_PT, NULL, decl, true));
        CU_ASSERT(s_type->fixed_pt.digits == 5);
        CU_ASSERT(s_type->fixed_pt.fraction_digits == 3); 
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "um", struct_s, false));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_MAP, NULL, decl, true));
        CU_ASSERT(s_type->map.max == 0);
        CU_ASSERT(DDSTS_IS_UNBOUND(s_type));
          ddsts_type_t *key_type = s_type->map.key_type;
          CU_ASSERT(test_type(key_type, DDSTS_SHORT, NULL, s_type, true));
          ddsts_type_t *value_type = s_type->map.value_type;
          CU_ASSERT(test_type(value_type, DDSTS_CHAR, NULL, s_type, true));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "bm", struct_s, true));
        s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_MAP, NULL, decl, true));
        CU_ASSERT(s_type->map.max == 5);
        CU_ASSERT(!DDSTS_IS_UNBOUND(s_type));
          key_type = s_type->map.key_type;
          CU_ASSERT(test_type(key_type, DDSTS_SHORT, NULL, s_type, true));
          value_type = s_type->map.value_type;
          CU_ASSERT(test_type(value_type, DDSTS_CHAR, NULL, s_type, true));
  ddsts_free_type(root_type);
}

CU_Test(parser, array)
{
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string("struct s{short a[3], b[4][5]; sequence<char> s[6];};", &root_type) == DDS_RETCODE_OK);
  CU_ASSERT(test_type(root_type, DDSTS_MODULE, NULL, NULL, true));
  CU_ASSERT(root_type->module.previous == NULL);
    ddsts_type_t *struct_s = root_type->module.members;
    CU_ASSERT(test_type(struct_s, DDSTS_STRUCT, "s", root_type, true));
      ddsts_type_t *decl = struct_s->struct_def.members;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "a", struct_s, false));
        ddsts_type_t *a_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(a_type, DDSTS_ARRAY, NULL, decl, true));
        CU_ASSERT(a_type->array.size == 3);
          ddsts_type_t *elem_type = a_type->array.element_type;
          CU_ASSERT(test_type(elem_type, DDSTS_SHORT, NULL, a_type, true));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "b", struct_s, false));
        ddsts_type_t *b_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(b_type, DDSTS_ARRAY, NULL, decl, true));
        CU_ASSERT(b_type->array.size == 4);
          elem_type = b_type->array.element_type;
          CU_ASSERT(test_type(elem_type, DDSTS_ARRAY, NULL, b_type, true));
          CU_ASSERT(elem_type->array.size == 5);
            ddsts_type_t *elem_type2 = elem_type->array.element_type;
            CU_ASSERT(test_type(elem_type2, DDSTS_SHORT, NULL, a_type, true));
      decl = decl->type.next;
      CU_ASSERT(test_type(decl, DDSTS_DECLARATION, "s", struct_s, true));
        ddsts_type_t *s_type = decl->declaration.decl_type;
        CU_ASSERT(test_type(s_type, DDSTS_ARRAY, NULL, decl, true));
        CU_ASSERT(s_type->array.size == 6);
          elem_type = s_type->array.element_type;
          CU_ASSERT(test_type(elem_type, DDSTS_SEQUENCE, NULL, s_type, true));
            elem_type2 = elem_type->sequence.element_type;
            CU_ASSERT(test_type(elem_type2, DDSTS_CHAR, NULL, elem_type, true));
  ddsts_free_type(root_type);
}

CU_Test(parser, errors)
{
  /* The purpose of these tests is also to verify that all memory is freed correctly */
  ddsts_type_t *root_type = NULL;
  CU_ASSERT(ddsts_idl_parse_string(NULL, &root_type) == DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT(root_type == NULL);
  CU_ASSERT(ddsts_idl_parse_string("xyz", NULL) == DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT(root_type == NULL);
  CU_ASSERT(ddsts_idl_parse_string(NULL, NULL) == DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT(root_type == NULL);
  CU_ASSERT(ddsts_idl_parse_string("xyz", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(root_type == NULL);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3][4];};", &root_type) == DDS_RETCODE_OK);
  ddsts_free_type(root_type);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3][4];}", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(root_type == NULL);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3][4];", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3][4]", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3][4", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3][", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3]", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[3", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a[", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char a;", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{char", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence<char> seqa;}", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence<char> seqa", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence<char>", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence<char", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence<", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{sequence", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m;}", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m;", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> ", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m;};!", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m;}!;", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m;!};", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> m!;};", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char> !m;};", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char>! m;};", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,char!> m;};", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char,!char> m;};", &root_type) == DDS_RETCODE_ERROR);
  CU_ASSERT(ddsts_idl_parse_string("struct s{map<char!,char> m;};", &root_type) == DDS_RETCODE_ERROR);
}

