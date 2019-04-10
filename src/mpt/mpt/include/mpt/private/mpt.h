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
#ifndef MPT_PRIVATE_H_INCLUDED
#define MPT_PRIVATE_H_INCLUDED

#include <stdio.h>
#include <string.h>


/*
 * Just some helpful macros.
 */
#define MPT_XSTR(s) MPT_STR(s)
#define MPT_STR(s) #s

#define MPT_STRCMP(value, expected, err) \
  (((value != NULL) && (expected != NULL)) ? strcmp(value, expected) : err)

#define MPT_STRLEN(value, err) \
  ((value != NULL) ? strlen(value) : err)

#define MPT_ProcessArgs     mpt__args__, mpt__retval__
#define MPT_ProcessArgsSyntax \
    const mpt_data_t *mpt__args__, mpt_retval_t *mpt__retval__



/*
 * Name and declaration macros.
 */
#define MPT_ProcessEntryName(process) \
  MPT_ProcessEntry__ ## process

#define MPT_TestInitName(suite, test) \
    MPT_TestInit__##suite##_##test

#define MPT_TestInitDeclaration(suite, test) \
void MPT_TestInitName(suite, test)(void)

#define MPT_TestFiniName(suite, test) \
    MPT_TestFini__##suite##_##test

#define MPT_TestFiniDeclaration(suite, test) \
void MPT_TestFiniName(suite, test)(void)

#define MPT_TestProcessName(suite, test, name) \
    MPT_TestProcess__##suite##_##test##_##name

#define MPT_TestProcessDeclaration(suite, test, name) \
void MPT_TestProcessName(suite, test, name) (MPT_ProcessArgsSyntax)



/*
 * MPT Assert impl.
 */
typedef enum {
  MPT_SUCCESS = 0,
  MPT_FAILURE
} mpt_retval_t;

#define MPT_FATAL_YES 1
#define MPT_FATAL_NO  0

#ifdef _WIN32
/* Microsoft Visual Studio does not expand __VA_ARGS__ correctly. */
#define MPT__ASSERT__(...)     MPT__ASSERT____((__VA_ARGS__))
#define MPT__ASSERT____(tuple) MPT__ASSERT___ tuple
#else
#define MPT__ASSERT__(...) MPT__ASSERT___(__VA_ARGS__)
#endif /* _WIN32 */

#define MPT__ASSERT___(cond, fatal, ...)                  \
  do {                                                    \
    (void)mpt__args__; /* Satisfy compiler. */            \
    if (!(cond)) {                                        \
      if (*mpt__retval__ == MPT_SUCCESS) {                \
        *mpt__retval__ = MPT_FAILURE;                     \
      }                                                   \
      printf("MPT_FAIL(%s, %d):\n", __FILE__, __LINE__);  \
      printf(__VA_ARGS__);                                \
      printf("\n");                                       \
      if (fatal == MPT_FATAL_YES) {                       \
        return;                                           \
      }                                                   \
    }                                                     \
  } while(0)



/*
 * MPT Fixture impl.
 */
struct mpt_env_;

typedef void(*mpt_init_func_t)(void);
typedef void(*mpt_fini_func_t)(void);

typedef struct {
  /* Test and process fixtures. */
  mpt_init_func_t init;
  mpt_fini_func_t fini;
  struct mpt_env_ *environment;
  /* Test fixtures. */
  bool disabled;
  int timeout;
  bool xfail;
  /* IPC information. */
  int todo;
} mpt_data_t;

/* Microsoft Visual Studio does not like empty struct initializers, i.e.
   no fixtures are specified. To work around that issue MPT_Fixture inserts a
   NULL initializer as fall back. */
#define MPT_Comma() ,
#define MPT_Reduce(one, ...) one

#ifdef _WIN32
/* Microsoft Visual Studio does not expand __VA_ARGS__ correctly. */
#define MPT_Fixture__(...) MPT_Fixture____((__VA_ARGS__))
#define MPT_Fixture____(tuple) MPT_Fixture___ tuple
#else
#define MPT_Fixture__(...) MPT_Fixture___(__VA_ARGS__)
#endif /* _WIN32 */

#define MPT_Fixture___(throw, away, value, ...) value

#define MPT_Fixture_(throwaway, ...) \
  MPT_Fixture__(throwaway, ((mpt_data_t){ 0 }), ((mpt_data_t){ __VA_ARGS__ }))

#define MPT_Fixture(...) \
  MPT_Fixture_( MPT_Comma MPT_Reduce(__VA_ARGS__,) (), __VA_ARGS__ )



/*
 * MPT Support functions.
 */
void mpt_export_env(const struct mpt_env_ *env);
void mpt_ipc_send(MPT_ProcessArgsSyntax, const char *str);
void mpt_ipc_wait(MPT_ProcessArgsSyntax, const char *str);


#endif /* MPT_PRIVATE_H_INCLUDED */
