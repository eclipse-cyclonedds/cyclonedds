#include <stdio.h>
#include "ucunit/ucunit.h"

static int initflag, testflag;

static int fail_suite_init (void)
{
  return -1;
}

static int success_suite_init (void)
{
  initflag++;
  return 0;
}

static void f_no_assert (void)
{
  testflag++;
}

static void f_pass (void)
{
  testflag++;
  CU_PASS ("this passes");
}

static void f_fail (void)
{
  testflag++;
  CU_ASSERT (0);
}

static void f_fatal (void)
{
  testflag++;
  CU_ASSERT_FATAL (0);
  testflag++; // should not get here
}

static bool test_fail_no_tests_at_all (void)
{
  CU_initialize_registry ();
  CU_basic_run_tests ();
  const bool ok = CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_no_suite_no_test (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_basic_run_tests ();
  const bool ok = CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_suite_no_test (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, false);
  CU_basic_run_tests ();
  const bool ok = CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_active_suite_no_test (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_basic_run_tests ();
  const bool ok = CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_inactive_suite_test (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, false);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 0 && CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_active_suite_inactive_test (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, false);
  CU_basic_run_tests ();
  const bool ok = testflag == 0 && CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_pass (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 1 && CU_get_number_of_failures () == 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_suite_init (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", fail_suite_init, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 0 && CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_suite_init2 (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", fail_suite_init, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, true);
  CU_pSuite suite2 = CU_add_suite ("suite2", NULL, NULL);
  CU_set_suite_active (suite2, true);
  CU_pTest test2 = CU_add_test (suite, "test2", f_pass);
  CU_set_test_active (test2, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 0 && CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_no_assert (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_no_assert);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 1 && CU_get_number_of_failures () > 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_pass_suite_init (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", success_suite_init, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = initflag == 1 && testflag == 1 && CU_get_number_of_failures () == 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_pass_suite_init2 (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", success_suite_init, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_pass);
  CU_set_test_active (test, true);
  CU_pTest test2 = CU_add_test (suite, "test2", f_pass);
  CU_set_test_active (test2, true);
  CU_basic_run_tests ();
  const bool ok = initflag == 1 && testflag == 2 && CU_get_number_of_failures () == 0;
  CU_cleanup_registry ();
  return ok;
}

static bool test_pass_test_fail (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_fail);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 1 && CU_get_number_of_failures () == 1;
  CU_cleanup_registry ();
  return ok;
}

static bool test_pass_test_fatal (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_fatal);
  CU_set_test_active (test, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 1 && CU_get_number_of_failures () == 1;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_pass_after_fail (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_fail);
  CU_set_test_active (test, true);
  CU_pTest test2 = CU_add_test (suite, "test2", f_pass);
  CU_set_test_active (test2, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 2 && CU_get_number_of_failures () == 1;
  CU_cleanup_registry ();
  return ok;
}

static bool test_fail_pass_after_fatal (void)
{
  initflag = testflag = 0;
  CU_initialize_registry ();
  CU_pSuite suite = CU_add_suite ("suite", NULL, NULL);
  CU_set_suite_active (suite, true);
  CU_pTest test = CU_add_test (suite, "test", f_fatal);
  CU_set_test_active (test, true);
  CU_pTest test2 = CU_add_test (suite, "test2", f_pass);
  CU_set_test_active (test2, true);
  CU_basic_run_tests ();
  const bool ok = testflag == 2 && CU_get_number_of_failures () == 1;
  CU_cleanup_registry ();
  return ok;
}

#define TEST(n) { #n, n }
static const struct { const char *name; bool (*f) (void); } tests[] = {
  TEST (test_fail_no_tests_at_all),
  TEST (test_fail_no_suite_no_test),
  TEST (test_fail_suite_no_test),
  TEST (test_fail_active_suite_no_test),
  TEST (test_fail_inactive_suite_test),
  TEST (test_fail_active_suite_inactive_test),
  TEST (test_pass),
  TEST (test_fail_suite_init),
  TEST (test_fail_suite_init2),
  TEST (test_fail_no_assert),
  TEST (test_pass_suite_init),
  TEST (test_pass_suite_init2),
  TEST (test_pass_test_fail),
  TEST (test_pass_test_fatal),
  TEST (test_fail_pass_after_fail),
  TEST (test_fail_pass_after_fatal)
};

int main (int argc, char **argv)
{
  (void) argc; (void) argv;
  bool ok = true;
  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++)
  {
    printf ("%s:\n", tests[i].name);
    fflush (stdout);
    if (tests[i].f ())
      printf ("%s: pass\n", tests[i].name);
    else
    {
      printf ("%s: failed\n", tests[i].name);
      ok = false;
    }
    fflush (stdout);
  }
  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
