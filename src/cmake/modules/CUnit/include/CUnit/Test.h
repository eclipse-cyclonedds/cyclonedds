#ifndef CUNIT_TEST_H
#define CUNIT_TEST_H

#include <stdbool.h>
#include <CUnit/CUnit.h>
#include <CUnit/CUError.h>

#if defined (__cplusplus)
extern "C" {
#endif

typedef void(*cu_test_init_func_t)(void);
typedef void(*cu_test_fini_func_t)(void);

typedef struct {
  cu_test_init_func_t init;
  cu_test_fini_func_t fini;
  int disabled; /* Parsed by CMake, used at test registration in main. */
  int timeout; /* Parsed by CMake, used at test registration in CMake. */
} cu_test_data_t;

#define CU_InitName(suite) \
  CU_Init_ ## suite
#define CU_CleanName(suite) \
  CU_Fini_ ## suite
#define CU_TestName(suite, test) \
  CU_Test_ ## suite ## _ ## test
#define CU_TestProxyName(suite, test) \
  CU_TestProxy_ ## suite ## _ ## test

#define CU_Init(suite) \
  int CU_InitName(suite)(void)
#define CU_InitDecl(suite) \
  extern CU_Init(suite)

#define CU_Clean(suite) \
  int CU_CleanName(suite)(void)
#define CU_CleanDecl(suite) \
  extern CU_Clean(suite)

/* CU_Test generates a wrapper function that takes care of per-test
   initialization and deinitialization, if provided in the CU_Test
   signature. */
#define CU_Test(suite, test, ...)                       \
  static void CU_TestName(suite, test)(void);           \
                                                        \
  void CU_TestProxyName(suite, test)(void) {            \
    static const cu_test_data_t data = { __VA_ARGS__ }; \
                                                        \
    if (data.init != NULL) {                            \
      data.init();                                      \
    }                                                   \
                                                        \
    CU_TestName(suite, test)();                         \
                                                        \
    if (data.fini != NULL) {                            \
      data.fini();                                      \
    }                                                   \
  }                                                     \
                                                        \
  static void CU_TestName(suite, test)(void)

#define CU_TestDecl(suite, test) \
  extern void CU_TestProxyName(suite, test)(void)

#if defined (__cplusplus)
}
#endif

#endif /* CUNIT_TEST_H */

