#include "os/os.h"
#include "CUnit/Theory.h"

typedef enum { eq, lt, gt } eq_t;

CU_TheoryDataPoints(os_strcasecmp, basic) = {
  CU_DataPoints(const char *, "a", "aa", "a",  "a", "A", "a", "b", "a", "B", "A", "", "a"),
  CU_DataPoints(const char *, "a", "a",  "aa", "A", "a", "b", "a", "b", "A", "B", "a", ""),
  CU_DataPoints(eq_t, eq, gt, lt, eq, eq, lt, gt, lt, gt, lt, lt, gt)
};

CU_Theory((const char *s1, const char *s2, eq_t e), os_strcasecmp, basic)
{
  int r = os_strcasecmp(s1, s2);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

CU_TheoryDataPoints(os_strncasecmp, basic) = {
  CU_DataPoints(const char *, "a", "aa", "a",  "A", "a", "b", "a", "B", "A", "", "a"),
  CU_DataPoints(const char *, "a", "a",  "aa", "a", "A", "a", "b", "A", "B", "a", ""),
  CU_DataPoints(size_t, 1, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1),
  CU_DataPoints(eq_t, eq, gt, lt, eq, eq, gt, lt, gt, lt, lt, gt)
};

CU_Theory((const char *s1, const char *s2, size_t n, eq_t e), os_strncasecmp, basic)
{
  int r = os_strncasecmp(s1, s2, n);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

CU_TheoryDataPoints(os_strncasecmp, empty) = {
  CU_DataPoints(const char *, "a", "", "a", "", "a", ""),
  CU_DataPoints(const char *, "", "a", "", "a", "", "a"),
  CU_DataPoints(size_t, 1, 1, 0, 0, 2, 2),
  CU_DataPoints(eq_t, gt, lt, eq, eq, gt, lt)
};

CU_Theory((const char *s1, const char *s2, size_t n, eq_t e), os_strncasecmp, empty)
{
  int r = os_strncasecmp(s1, s2, n);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

CU_TheoryDataPoints(os_strncasecmp, length) = {
  CU_DataPoints(const char *, "aBcD", "AbCX", "aBcD", "AbCX", "aBcD"),
  CU_DataPoints(const char *, "AbCX", "aBcD", "AbCX", "aBcD", "AbCd"),
  CU_DataPoints(size_t, 3, 3, 4, 4, 5, 5),
  CU_DataPoints(eq_t, eq, eq, lt, gt, eq, eq)
};

CU_Theory((const char *s1, const char *s2, size_t n, eq_t e), os_strncasecmp, length)
{
  int r = os_strncasecmp(s1, s2, n);
  CU_ASSERT((e == eq && r == 0) || (e == lt && r < 0) || (e == gt && r > 0));
}

