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
} cu_data_t;

#define CU_InitName(suite) \
  CU_Init_ ## suite
#define CU_CleanName(suite) \
  CU_Fini_ ## suite
#define CU_TestName(suite, test) \
  CU_Test_ ## suite ## _ ## test
#define CU_TestProxyName(suite, test) \
  CU_TestProxy_ ## suite ## _ ## test

#define CU_InitDecl(suite) \
  extern int CU_InitName(suite)(void)
#define CU_Init(suite) \
  CU_InitDecl(suite); \
  int CU_InitName(suite)(void)

#define CU_CleanDecl(suite) \
  extern int CU_CleanName(suite)(void)
#define CU_Clean(suite) \
  CU_CleanDecl(suite); \
  int CU_CleanName(suite)(void)

/* CU_Test generates a wrapper function that takes care of per-test
   initialization and deinitialization, if provided in the CU_Test
   signature. */
#define CU_Test(suite, test, ...)                          \
  static void CU_TestName(suite, test)(void);              \
  void CU_TestProxyName(suite, test)(void);                \
                                                           \
  void CU_TestProxyName(suite, test)(void) {               \
    cu_data_t cu_data = CU_Fixture(__VA_ARGS__);           \
                                                           \
    if (cu_data.init != NULL) {                            \
      cu_data.init();                                      \
    }                                                      \
                                                           \
    CU_TestName(suite, test)();                            \
                                                           \
    if (cu_data.fini != NULL) {                            \
      cu_data.fini();                                      \
    }                                                      \
  }                                                        \
                                                           \
  static void CU_TestName(suite, test)(void)

#define CU_TestDecl(suite, test) \
  extern void CU_TestProxyName(suite, test)(void)

/* Microsoft Visual Studio does not like empty struct initializers, i.e.
   no fixtures are specified. To work around that issue CU_Fixture inserts a
   NULL initializer as fall back. */
#define CU_Comma() ,
#define CU_Reduce(one, ...) one

#ifdef _WIN32
/* Microsoft Visual Studio does not expand __VA_ARGS__ correctly. */
#define CU_Fixture__(...) CU_Fixture____((__VA_ARGS__))
#define CU_Fixture____(tuple) CU_Fixture___ tuple
#else
#define CU_Fixture__(...) CU_Fixture___(__VA_ARGS__)
#endif /* _WIN32 */

#define CU_Fixture___(throw, away, value, ...) value

#define CU_Fixture(...) \
  CU_Fixture_( CU_Comma CU_Reduce(__VA_ARGS__,) (), __VA_ARGS__ )

#define CU_Fixture_(throwaway, ...) \
  CU_Fixture__(throwaway, ((cu_data_t){ 0 }), ((cu_data_t){ __VA_ARGS__ }))

#if defined (__cplusplus)
}
#endif

#endif /* CUNIT_TEST_H */

