#ifndef CUNIT_THEORY_H
#define CUNIT_THEORY_H

/* Function-style macros cannot be defined on the command line. */
#ifdef CU_THEORY_INCLUDE_FILE
#include CU_THEORY_INCLUDE_FILE
#endif

#include "CUnit/Test.h"


#if defined (__cplusplus)
extern "C" {
#endif

#define CU_TheoryDataPointsName(suite, test) \
  CU_TheoryDataPoints_ ## suite ## _ ## test

#define CU_TheoryDataPointsTypeName(suite, test) \
  CU_TheoryDataPointsType_ ## suite ## _ ## test

#define CU_TheoryDataPointsSize(suite, test) \
  CU_TheoryDataPointsSize_ ## suite ## _ ## test ( \
    CU_TheoryDataPointsName(suite, test))

#define CU_TheoryDataPointsSlice(suite, test, index) \
  CU_TheoryDataPointsSlice_ ## suite ## _ ## test ( \
    CU_TheoryDataPointsName(suite, test), index)

#define CU_TheoryDataPointsTypedef(suite, test) \
  CU_TheoryDataPointsTypedef_ ## suite ## _ ## test()

#define CU_TheoryDataPoints(suite, test) \
  struct CU_TheoryDataPointsTypeName(suite, test) \
    CU_TheoryDataPointsTypedef(suite, test) ; \
   \
  static struct CU_TheoryDataPointsTypeName(suite, test) \
    CU_TheoryDataPointsName(suite, test)

#define CU_DataPoints(type, ...) { \
    .p = (type[]) { __VA_ARGS__ }, \
    .n = (sizeof((type[]) { __VA_ARGS__ }) / sizeof(type)) \
  }

#define CU_Theory(signature, suite, test, ...)                            \
  static void CU_TestName(suite, test) signature;                         \
  void CU_TestProxyName(suite, test)(void);                               \
                                                                          \
  void CU_TestProxyName(suite, test)(void) {                              \
    cu_data_t cu_data = CU_Fixture(__VA_ARGS__);                          \
    size_t i, n;                                                          \
                                                                          \
    if (cu_data.init != NULL) {                                           \
      cu_data.init();                                                     \
    }                                                                     \
                                                                          \
    for (i = 0, n = CU_TheoryDataPointsSize(suite, test); i < n; i++) {   \
      CU_TestName(suite, test) CU_TheoryDataPointsSlice(suite, test, i) ; \
    }                                                                     \
                                                                          \
    if (cu_data.fini != NULL) {                                           \
      cu_data.fini();                                                     \
    }                                                                     \
  }                                                                       \
                                                                          \
  static void CU_TestName(suite, test) signature

#if defined (__cplusplus)
}
#endif

#endif /* CUNIT_THEORY_H */

