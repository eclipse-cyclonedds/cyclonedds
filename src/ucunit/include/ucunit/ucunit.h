// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef UCUNIT_H
#define UCUNIT_H

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "ucunit/export.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef __GNUC__
#define CU_UNREACHABLE __builtin_unreachable ()
#else
#define CU_UNREACHABLE ((void)0)
#endif

#define CU_PASS(msg) \
  CU_assertImplementation (true, __LINE__, ("CU_PASS(" msg ")"), __FILE__, "", false)
#define CU_FAIL(msg) \
  CU_assertImplementation (false, __LINE__, ("CU_FAIL(" msg ")"), __FILE__, "", false)
#define CU_FAIL_FATAL(msg) do { \
    CU_assertImplementation (false, __LINE__, ("CU_FAIL_FATAL(" msg ")"), __FILE__, "", true); \
    CU_UNREACHABLE; \
  } while (0)

#if defined __STDC__ && __STDC_VERSION__ >= 202311L

#if defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#define CU_ASSERT_SUPPRESS_WARNINGS \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wsign-compare\"")
#define CU_ASSERT_RESTORE_WARNINGS \
  _Pragma("GCC diagnostic pop")
#endif

#if !(defined CU_ASSERT_SUPPRESS_WARNINGS && defined CU_ASSERT_RESTORE_WARNINGS)
#define CU_ASSERT_SUPPRESS_WARNINGS
#define CU_ASSERT_RESTORE_WARNINGS
#endif

#define CU_ASSERT_PRINTF_FORMAT(T_)             \
  _Generic ((T_),                               \
    _Bool             : "%d",                   \
    char              : "%c",                   \
    signed char       : "%hhd",                 \
    unsigned char     : "%hhu",                 \
    short             : "%hd",                  \
    int               : "%d",                   \
    long              : "%ld",                  \
    long long         : "%lld",                 \
    unsigned short    : "%hu",                  \
    unsigned int      : "%u",                   \
    unsigned long     : "%lu",                  \
    unsigned long long: "%llu",                 \
    float             : "%f",                   \
    double            : "%f",                   \
    long double       : "%Lf",                  \
    default           : "%p"                    \
  )

#define CU_ASSERT_OP_MAYBE_FATAL(x_, op_, y_, fatal_) do {              \
  CU_ASSERT_SUPPRESS_WARNINGS \
  typeof (x_) xv__ = (x_);                                              \
  typeof (y_) yv__ = (y_);                                              \
  const bool fatal__ = (fatal_);                                        \
  const bool satisfied__ = (xv__) op_ (yv__);                           \
  if (!satisfied__) {                                                   \
    char fmt__[100];                                                    \
    snprintf (fmt__, sizeof (fmt__),                                    \
      "%%s:%%d: not satisfied: (%%s [= %s]) %%s (%%s [= %s])\n",        \
      CU_ASSERT_PRINTF_FORMAT (xv__), CU_ASSERT_PRINTF_FORMAT (yv__));  \
    fprintf (stderr, fmt__, __FILE__, __LINE__,                         \
      #x_, xv__, #op_, #y_, yv__);                                      \
  }                                                                     \
  CU_assertImplementation (satisfied__, __LINE__,                       \
    "(" #x_ ") " #op_ " (" #y_ ")", __FILE__, "", fatal__);             \
  if (!satisfied__ && fatal__)                                          \
    CU_UNREACHABLE;                                                     \
  CU_ASSERT_RESTORE_WARNINGS \
} while (0)

#else

#define CU_ASSERT_OP_MAYBE_FATAL(x_, op_, y_, fatal_) do {      \
  const bool fatal__ = (fatal_);                                \
  const bool satisfied__ = (x_) op_ (y_);                       \
  CU_assertImplementation (satisfied__, __LINE__,               \
    "(" #x_ ") " #op_ " (" #y_ ")", __FILE__, "", fatal__);     \
  if (!satisfied__ && fatal__)                                  \
    CU_UNREACHABLE;                                             \
} while (0)

#endif

#define CU_ASSERT_STRING_OP_MAYBE_FATAL(x_, op_, y_, fatal_) do {       \
  const char *xv__ = (const char *) (x_);                               \
  const char *yv__ = (const char *) (y_);                               \
  const bool fatal__ = (fatal_);                                        \
  const bool satisfied__ = xv__ && yv__ && strcmp (xv__, yv__) op_ 0;   \
  if (!satisfied__) {                                                   \
    fprintf (stderr,                                                    \
      "%s:%d: not satisfied: (%s [= %s]) %s (%s [= %s])\n",             \
      __FILE__, __LINE__, #x_, xv__, #op_, #y_, yv__);                  \
  }                                                                     \
  CU_assertImplementation (satisfied__, __LINE__,                       \
    "(" #x_ ") " #op_ " (" #y_ ")", __FILE__, "", fatal_);              \
  if (!satisfied__ && fatal__)                                          \
    CU_UNREACHABLE;                                                     \
} while (0)

#define CU_ASSERT_MEMEQ_MAYBE_FATAL(x_, xsz_, y_, ysz_, fatal_) do {    \
  const unsigned char *xv__ = (const unsigned char *) (x_);             \
  const unsigned char *yv__ = (const unsigned char *) (y_);             \
  const size_t xszv__ = (size_t) (xsz_);                                \
  const size_t yszv__ = (size_t) (ysz_);                                \
  const bool fatal__ = (fatal_);                                        \
  const bool satisfied__ =                                              \
    (xszv__ == yszv__ &&                                                \
     (xszv__ == 0 || memcmp (xv__, yv__, xszv__) == 0));                \
  if (!satisfied__) {                                                   \
    fprintf (stderr,                                                    \
      "%s:%d: not satisfied: "                                          \
      "(%s,%s [= %p,%zu]) == (%s,%s [= %p,%zu])\n",                     \
      __FILE__, __LINE__,                                               \
      #x_, #xsz_, xv__, xszv__, #y_, #ysz_, yv__, yszv__);              \
    fprintf (stderr, "%s:\n", #x_);                                     \
    CU_hexdump (stderr, xv__, xszv__);                                  \
    fprintf (stderr, "%s:\n", #y_);                                     \
    CU_hexdump (stderr, yv__, yszv__);                                  \
  }                                                                     \
  CU_assertImplementation (satisfied__, __LINE__,                       \
    "(" #x_ ") == (" #y_ ")", __FILE__, "", fatal_);                    \
  if (!satisfied__ && fatal__)                                          \
    CU_UNREACHABLE;                                                     \
} while (0)

#define CU_ASSERT_EQ(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, ==, y_, false)
#define CU_ASSERT_EQ_FATAL(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, ==, y_, true)

#define CU_ASSERT_NEQ(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, !=, y_, false)
#define CU_ASSERT_NEQ_FATAL(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, !=, y_, true)

#define CU_ASSERT_GT(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, >, y_, false)
#define CU_ASSERT_GT_FATAL(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, >, y_, true)

#define CU_ASSERT_LT(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, <, y_, false)
#define CU_ASSERT_LT_FATAL(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, <, y_, true)

#define CU_ASSERT_GEQ(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, >=, y_, false)
#define CU_ASSERT_GEQ_FATAL(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, >=, y_, true)

#define CU_ASSERT_LEQ(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, <=, y_, false)
#define CU_ASSERT_LEQ_FATAL(x_, y_) CU_ASSERT_OP_MAYBE_FATAL (x_, <=, y_, true)

#define CU_ASSERT_STREQ(x_, y_) CU_ASSERT_STRING_OP_MAYBE_FATAL (x_, ==, y_, false)
#define CU_ASSERT_STREQ_FATAL(x_, y_) CU_ASSERT_STRING_OP_MAYBE_FATAL (x_, ==, y_, true)

#define CU_ASSERT_STRNEQ(x_, y_) CU_ASSERT_STRING_OP_MAYBE_FATAL (x_, !=, y_, false)
#define CU_ASSERT_STRNEQ_FATAL(x_, y_) CU_ASSERT_STRING_OP_MAYBE_FATAL (x_, !=, y_, true)

#define CU_ASSERT_MEMEQ(x_, xsz_, y_, ysz_) CU_ASSERT_MEMEQ_MAYBE_FATAL (x_, xsz_, y_, ysz_, false)
#define CU_ASSERT_MEMEQ_FATAL(x_, xsz_, y_, ysz_) CU_ASSERT_MEMEQ_MAYBE_FATAL (x_, xsz_, y_, ysz_, true)

#define CU_ASSERT(x_) CU_ASSERT_OP_MAYBE_FATAL (x_, !=, false, false)
#define CU_ASSERT_FATAL(x_) CU_ASSERT_OP_MAYBE_FATAL (x_, !=, false, true)

typedef void (*CU_TestFunc) (void);
typedef int (*CU_InitializeFunc) (void);
typedef int (*CU_CleanupFunc) (void);

struct CU_FailureRecord {
  struct CU_FailureRecord *next;
  const char *file;
  int line;
  const char *expr;
};

typedef struct CU_Test {
  struct CU_Test *next;
  char *name;
  bool active;
  CU_TestFunc testfunc;
  uint32_t nasserts;
  uint32_t nfailures;
  struct CU_FailureRecord *failures, *latest_failure;
} *CU_pTest;

typedef struct CU_Suite {
  struct CU_Suite *next;
  char *name;
  bool active;
  bool initfailed;
  CU_InitializeFunc init;
  CU_CleanupFunc cleanup;
  struct CU_Test *tests;
  uint32_t ntests;
  uint32_t ntests_pass;
  uint32_t ntests_failed;
} *CU_pSuite;

typedef enum CU_ErrorCode {
  CUE_SUCCESS,
  CUE_ERROR
} CU_ErrorCode;

typedef enum CU_ErrorAction {
  CUEA_FAIL,
  CUEA_ABORT // A failed CU_ASSERT_FATAL cause CU_fatal to abort()
} CU_ErrorAction;

UCUNIT_EXPORT CU_ErrorCode CU_initialize_registry (void);

UCUNIT_EXPORT CU_pSuite CU_add_suite(const char *strName, CU_InitializeFunc pInit, CU_CleanupFunc pClean);

UCUNIT_EXPORT CU_pSuite CU_get_suite (const char *strName);

UCUNIT_EXPORT CU_ErrorCode CU_set_suite_active (CU_pSuite pSuite, bool fNewActive);

UCUNIT_EXPORT CU_pTest CU_add_test(CU_pSuite pSuite, const char *strName, CU_TestFunc pTestFunc);

UCUNIT_EXPORT void CU_set_test_active(CU_pTest pTest, bool fNewActive);

UCUNIT_EXPORT CU_ErrorCode CU_get_error (void);

UCUNIT_EXPORT const char *CU_get_error_msg (void);

UCUNIT_EXPORT CU_ErrorCode CU_basic_run_tests (void);

UCUNIT_EXPORT uint32_t CU_get_number_of_failures (void);

UCUNIT_EXPORT void CU_cleanup_registry (void);

UCUNIT_EXPORT void CU_assertImplementation (bool value, int line, const char *expr, const char *file, const char *something, bool isfatal);

UCUNIT_EXPORT void CU_hexdump (FILE *fp, const unsigned char *msg, const size_t len);

UCUNIT_EXPORT void CU_fatal (void);

UCUNIT_EXPORT void CU_set_error_action (CU_ErrorAction action);

#if defined (__cplusplus)
}
#endif

#endif
