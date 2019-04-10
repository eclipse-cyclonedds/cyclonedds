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
#ifndef MPT_H_INCLUDED
#define MPT_H_INCLUDED

#include <stdio.h>
#include <stdbool.h>
#include "mpt/resource.h"
#include "mpt/private/mpt.h"
#include "dds/ddsrt/environ.h"


/* Environment name/value pair. */
typedef struct mpt_env_ {
    const char *name;
    const char *value;
} mpt_env_t;


/* Process entry argument definitions. */
#define MPT_Args(...) MPT_ProcessArgsSyntax, __VA_ARGS__
#define MPT_NoArgs() MPT_ProcessArgsSyntax

#define MPT_ArgValues(...) MPT_ProcessArgs, __VA_ARGS__
#define MPT_NoArgValues() MPT_ProcessArgs


/* Process entry definition. */
#define MPT_ProcessEntry(process, args)\
void MPT_ProcessEntryName(process)(args)


/* IPC functions. */
#define MPT_Send(str) mpt_ipc_send(MPT_ProcessArgs, str);
#define MPT_Wait(str) mpt_ipc_wait(MPT_ProcessArgs, str);


/*
 * MPT_TestProcess generates a wrapper function that takes care of
 * per-process initialization, environment settings,
 * deinitialization and the actual process entry call.
 */
#define MPT_TestProcess(suite, test, name, process, args, ...)    \
MPT_TestInitDeclaration(suite, test);                             \
MPT_TestFiniDeclaration(suite, test);                             \
MPT_TestProcessDeclaration(suite, test, name)                     \
{                                                                 \
  mpt_data_t data = MPT_Fixture(__VA_ARGS__);                     \
                                                                  \
  /* Always export the process name. */                           \
  /* This can be used to generate unique log files fi. */         \
  ddsrt_setenv("MPT_PROCESS_NAME",                                \
               MPT_XSTR(MPT_TestProcessName(suite, test, name))); \
                                                                  \
  /* Initialize test related stuff first. */                      \
  MPT_TestInitName(suite, test)();                                \
                                                                  \
  /* Pre-process initialization. */                               \
  mpt_export_env(data.environment);                               \
  if (data.init != NULL) {                                        \
    data.init();                                                  \
  }                                                               \
                                                                  \
  /* Execute the actual process entry function. */                \
  MPT_ProcessEntryName(process)(args);                            \
                                                                  \
  /* Teardown process and test. */                                \
  if (data.fini != NULL) {                                        \
    data.fini();                                                  \
  }                                                               \
  MPT_TestFiniName(suite, test)();                                \
}


/*
 * MPT_Test generates wrapper functions that take care of
 * per-test initialization, environment settings and
 * deinitialization.
 * This is also used by CMake to determine the ctest timeout
 * and disabled settings.
 */
#define MPT_Test(suite, test, ...)                                \
MPT_TestInitDeclaration(suite, test)                              \
{                                                                 \
  mpt_data_t data = MPT_Fixture(__VA_ARGS__);                     \
  mpt_export_env(data.environment);                               \
  if (data.init != NULL) {                                        \
    data.init();                                                  \
  }                                                               \
}                                                                 \
MPT_TestFiniDeclaration(suite, test)                              \
{                                                                 \
  mpt_data_t data = MPT_Fixture(__VA_ARGS__);                     \
  if (data.fini != NULL) {                                        \
    data.fini();                                                  \
  }                                                               \
}


/*
 * Test asserts.
 * Printing is supported eg MPT_ASSERT_EQ(a, b, "foo: %s", bar")
 */
#define MPT_ASSERT(cond, ...)   MPT__ASSERT__(cond, MPT_FATAL_NO, __VA_ARGS__)

#define MPT_ASSERT_FAIL(...)    MPT_ASSERT(0, __VA_ARGS__)

#define MPT_ASSERT_EQ(value, expected, ...)  MPT_ASSERT((value == expected), __VA_ARGS__)
#define MPT_ASSERT_NEQ(value, expected, ...) MPT_ASSERT((value != expected), __VA_ARGS__)
#define MPT_ASSERT_LEQ(value, expected, ...) MPT_ASSERT((value <= expected), __VA_ARGS__)
#define MPT_ASSERT_GEQ(value, expected, ...) MPT_ASSERT((value >= expected), __VA_ARGS__)
#define MPT_ASSERT_LT(value, expected, ...)  MPT_ASSERT((value  < expected), __VA_ARGS__)
#define MPT_ASSERT_GT(value, expected, ...)  MPT_ASSERT((value  > expected), __VA_ARGS__)

#define MPT_ASSERT_NULL(value, ...)     MPT_ASSERT((value == NULL), __VA_ARGS__)
#define MPT_ASSERT_NOT_NULL(value, ...) MPT_ASSERT((value != NULL), __VA_ARGS__)

#define MPT_ASSERT_STR_EQ(value, expected, ...)  MPT_ASSERT((MPT_STRCMP(value, expected, 1) == 0), __VA_ARGS__)
#define MPT_ASSERT_STR_NEQ(value, expected, ...) MPT_ASSERT((MPT_STRCMP(value, expected, 0) != 0), __VA_ARGS__)
#define MPT_ASSERT_STR_EMPTY(value, ...)         MPT_ASSERT((MPT_STRLEN(value, 1) == 0), __VA_ARGS__)
#define MPT_ASSERT_STR_NOT_EMPTY(value, ...)     MPT_ASSERT((MPT_STRLEN(value, 0)  > 0), __VA_ARGS__)


/* Fatal just means that control is returned to the parent function. */
#define MPT_ASSERT_FATAL(cond, ...)   MPT__ASSERT__(cond, MPT_FATAL_YES, __VA_ARGS__)

#define MPT_ASSERT_FATAL_FAIL(...)    MPT_ASSERT_FATAL(0, __VA_ARGS__)

#define MPT_ASSERT_FATAL_EQ(value, expected, ...)  MPT_ASSERT_FATAL((value == expected), __VA_ARGS__)
#define MPT_ASSERT_FATAL_NEQ(value, expected, ...) MPT_ASSERT_FATAL((value != expected), __VA_ARGS__)
#define MPT_ASSERT_FATAL_LEQ(value, expected, ...) MPT_ASSERT_FATAL((value <= expected), __VA_ARGS__)
#define MPT_ASSERT_FATAL_GEQ(value, expected, ...) MPT_ASSERT_FATAL((value >= expected), __VA_ARGS__)
#define MPT_ASSERT_FATAL_LT(value, expected, ...)  MPT_ASSERT_FATAL((value  < expected), __VA_ARGS__)
#define MPT_ASSERT_FATAL_GT(value, expected, ...)  MPT_ASSERT_FATAL((value  > expected), __VA_ARGS__)

#define MPT_ASSERT_FATAL_NULL(value, ...)     MPT_ASSERT_FATAL((value == NULL), __VA_ARGS__)
#define MPT_ASSERT_FATAL_NOT_NULL(value, ...) MPT_ASSERT_FATAL((value != NULL), __VA_ARGS__)

#define MPT_ASSERT_FATAL_STR_EQ(value, expected, ...)  MPT_ASSERT_FATAL((MPT_STRCMP(value, expected, 1) == 0), __VA_ARGS__)
#define MPT_ASSERT_FATAL_STR_NEQ(value, expected, ...) MPT_ASSERT_FATAL((MPT_STRCMP(value, expected, 0) != 0), __VA_ARGS__)
#define MPT_ASSERT_FATAL_STR_EMPTY(value, ...)         MPT_ASSERT_FATAL((MPT_STRLEN(value, 1) == 0), __VA_ARGS__)
#define MPT_ASSERT_FATAL_STR_NOT_EMPTY(value, ...)     MPT_ASSERT_FATAL((MPT_STRLEN(value, 0)  > 0), __VA_ARGS__)


/* Helpful function to check for patterns in log callbacks. */
int mpt_patmatch(const char *pat, const char *str);


#endif /* MPT_H_INCLUDED */
