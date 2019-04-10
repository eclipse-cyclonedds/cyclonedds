#include <stdio.h>
#include <assert.h>
#include "mpt/mpt.h"
#include "dds/ddsrt/time.h"

static int dummy;



/************************************************************
 * Test MPT_ASSERT
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT, MPT_Args(const char *exp, int cond))
{
  assert(exp);
  MPT_ASSERT(cond, "MPT_ASSERT(%d), 1st, expect: %s", cond, exp);
  MPT_ASSERT(cond, "MPT_ASSERT(%d), 2nd, expect: %s", cond, exp);
}
MPT_TestProcess(MPT_ASSERT, pass, id, proc_MPT_ASSERT, MPT_ArgValues("PASS", 1));
MPT_TestProcess(MPT_ASSERT, fail, id, proc_MPT_ASSERT, MPT_ArgValues("FAIL", 0));
MPT_Test(MPT_ASSERT, pass, .xfail=false);
MPT_Test(MPT_ASSERT, fail, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FAIL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FAIL, MPT_NoArgs())
{
  MPT_ASSERT_FAIL("MPT_ASSERT_FAIL(), 1st, expect a fail, always");
  MPT_ASSERT_FAIL("MPT_ASSERT_FAIL(), 2nd, expect a fail, always");
}
MPT_TestProcess(MPT_ASSERT_FAIL, call, id, proc_MPT_ASSERT_FAIL, MPT_NoArgValues());
MPT_Test(MPT_ASSERT_FAIL, call, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_EQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_EQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_EQ(val1, val2, "MPT_ASSERT_EQ(%d, %d), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_EQ(val1, val2, "MPT_ASSERT_EQ(%d, %d), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_EQ_int, eq, id, proc_MPT_ASSERT_EQ_int, MPT_ArgValues("PASS", 1, 1));
MPT_TestProcess(MPT_ASSERT_EQ_int, lt, id, proc_MPT_ASSERT_EQ_int, MPT_ArgValues("FAIL", 1, 2));
MPT_TestProcess(MPT_ASSERT_EQ_int, gt, id, proc_MPT_ASSERT_EQ_int, MPT_ArgValues("FAIL", 3, 2));
MPT_Test(MPT_ASSERT_EQ_int, eq, .xfail=false);
MPT_Test(MPT_ASSERT_EQ_int, lt, .xfail=true);
MPT_Test(MPT_ASSERT_EQ_int, gt, .xfail=true);


MPT_ProcessEntry(proc_MPT_ASSERT_EQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_EQ(val1, val2, "MPT_ASSERT_EQ(%f, %f), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_EQ(val1, val2, "MPT_ASSERT_EQ(%f, %f), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_EQ_double, eq, id, proc_MPT_ASSERT_EQ_double, MPT_ArgValues("PASS", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_EQ_double, lt, id, proc_MPT_ASSERT_EQ_double, MPT_ArgValues("FAIL", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_EQ_double, gt, id, proc_MPT_ASSERT_EQ_double, MPT_ArgValues("FAIL", 1.3, 1.2));
MPT_Test(MPT_ASSERT_EQ_double, eq, .xfail=false);
MPT_Test(MPT_ASSERT_EQ_double, lt, .xfail=true);
MPT_Test(MPT_ASSERT_EQ_double, gt, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_NEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_NEQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_NEQ(val1, val2, "MPT_ASSERT_NEQ(%d, %d), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_NEQ(val1, val2, "MPT_ASSERT_NEQ(%d, %d), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_NEQ_int, eq, id, proc_MPT_ASSERT_NEQ_int, MPT_ArgValues("FAIL", 1, 1));
MPT_TestProcess(MPT_ASSERT_NEQ_int, lt, id, proc_MPT_ASSERT_NEQ_int, MPT_ArgValues("PASS", 1, 2));
MPT_TestProcess(MPT_ASSERT_NEQ_int, gt, id, proc_MPT_ASSERT_NEQ_int, MPT_ArgValues("PASS", 3, 2));
MPT_Test(MPT_ASSERT_NEQ_int, eq, .xfail=true);
MPT_Test(MPT_ASSERT_NEQ_int, lt, .xfail=false);
MPT_Test(MPT_ASSERT_NEQ_int, gt, .xfail=false);


MPT_ProcessEntry(proc_MPT_ASSERT_NEQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_NEQ(val1, val2, "MPT_ASSERT_NEQ(%f, %f), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_NEQ(val1, val2, "MPT_ASSERT_NEQ(%f, %f), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_NEQ_double, eq, id, proc_MPT_ASSERT_NEQ_double, MPT_ArgValues("FAIL", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_NEQ_double, lt, id, proc_MPT_ASSERT_NEQ_double, MPT_ArgValues("PASS", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_NEQ_double, gt, id, proc_MPT_ASSERT_NEQ_double, MPT_ArgValues("PASS", 1.3, 1.2));
MPT_Test(MPT_ASSERT_NEQ_double, eq, .xfail=true);
MPT_Test(MPT_ASSERT_NEQ_double, lt, .xfail=false);
MPT_Test(MPT_ASSERT_NEQ_double, gt, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_LEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_LEQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_LEQ(val1, val2, "MPT_ASSERT_LEQ(%d, %d), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_LEQ(val1, val2, "MPT_ASSERT_LEQ(%d, %d), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_LEQ_int, eq, id, proc_MPT_ASSERT_LEQ_int, MPT_ArgValues("PASS", 1, 1));
MPT_TestProcess(MPT_ASSERT_LEQ_int, lt, id, proc_MPT_ASSERT_LEQ_int, MPT_ArgValues("PASS", 1, 2));
MPT_TestProcess(MPT_ASSERT_LEQ_int, gt, id, proc_MPT_ASSERT_LEQ_int, MPT_ArgValues("FAIL", 3, 2));
MPT_Test(MPT_ASSERT_LEQ_int, eq, .xfail=false);
MPT_Test(MPT_ASSERT_LEQ_int, lt, .xfail=false);
MPT_Test(MPT_ASSERT_LEQ_int, gt, .xfail=true);


MPT_ProcessEntry(proc_MPT_ASSERT_LEQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_LEQ(val1, val2, "MPT_ASSERT_LEQ(%f, %f), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_LEQ(val1, val2, "MPT_ASSERT_LEQ(%f, %f), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_LEQ_double, eq, id, proc_MPT_ASSERT_LEQ_double, MPT_ArgValues("PASS", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_LEQ_double, lt, id, proc_MPT_ASSERT_LEQ_double, MPT_ArgValues("PASS", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_LEQ_double, gt, id, proc_MPT_ASSERT_LEQ_double, MPT_ArgValues("FAIL", 1.3, 1.2));
MPT_Test(MPT_ASSERT_LEQ_double, eq, .xfail=false);
MPT_Test(MPT_ASSERT_LEQ_double, lt, .xfail=false);
MPT_Test(MPT_ASSERT_LEQ_double, gt, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_GEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_GEQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_GEQ(val1, val2, "MPT_ASSERT_GEQ(%d, %d), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_GEQ(val1, val2, "MPT_ASSERT_GEQ(%d, %d), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_GEQ_int, eq, id, proc_MPT_ASSERT_GEQ_int, MPT_ArgValues("PASS", 1, 1));
MPT_TestProcess(MPT_ASSERT_GEQ_int, lt, id, proc_MPT_ASSERT_GEQ_int, MPT_ArgValues("FAIL", 1, 2));
MPT_TestProcess(MPT_ASSERT_GEQ_int, gt, id, proc_MPT_ASSERT_GEQ_int, MPT_ArgValues("PASS", 3, 2));
MPT_Test(MPT_ASSERT_GEQ_int, eq, .xfail=false);
MPT_Test(MPT_ASSERT_GEQ_int, lt, .xfail=true);
MPT_Test(MPT_ASSERT_GEQ_int, gt, .xfail=false);


MPT_ProcessEntry(proc_MPT_ASSERT_GEQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_GEQ(val1, val2, "MPT_ASSERT_GEQ(%f, %f), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_GEQ(val1, val2, "MPT_ASSERT_GEQ(%f, %f), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_GEQ_double, eq, id, proc_MPT_ASSERT_GEQ_double, MPT_ArgValues("PASS", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_GEQ_double, lt, id, proc_MPT_ASSERT_GEQ_double, MPT_ArgValues("FAIL", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_GEQ_double, gt, id, proc_MPT_ASSERT_GEQ_double, MPT_ArgValues("PASS", 1.3, 1.2));
MPT_Test(MPT_ASSERT_GEQ_double, eq, .xfail=false);
MPT_Test(MPT_ASSERT_GEQ_double, lt, .xfail=true);
MPT_Test(MPT_ASSERT_GEQ_double, gt, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_LT
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_LT_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_LT(val1, val2, "MPT_ASSERT_LT(%d, %d), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_LT(val1, val2, "MPT_ASSERT_LT(%d, %d), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_LT_int, eq, id, proc_MPT_ASSERT_LT_int, MPT_ArgValues("FAIL", 1, 1));
MPT_TestProcess(MPT_ASSERT_LT_int, lt, id, proc_MPT_ASSERT_LT_int, MPT_ArgValues("PASS", 1, 2));
MPT_TestProcess(MPT_ASSERT_LT_int, gt, id, proc_MPT_ASSERT_LT_int, MPT_ArgValues("FAIL", 3, 2));
MPT_Test(MPT_ASSERT_LT_int, eq, .xfail=true);
MPT_Test(MPT_ASSERT_LT_int, lt, .xfail=false);
MPT_Test(MPT_ASSERT_LT_int, gt, .xfail=true);


MPT_ProcessEntry(proc_MPT_ASSERT_LT_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_LT(val1, val2, "MPT_ASSERT_LT(%f, %f), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_LT(val1, val2, "MPT_ASSERT_LT(%f, %f), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_LT_double, eq, id, proc_MPT_ASSERT_LT_double, MPT_ArgValues("FAIL", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_LT_double, lt, id, proc_MPT_ASSERT_LT_double, MPT_ArgValues("PASS", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_LT_double, gt, id, proc_MPT_ASSERT_LT_double, MPT_ArgValues("FAIL", 1.3, 1.2));
MPT_Test(MPT_ASSERT_LT_double, eq, .xfail=true);
MPT_Test(MPT_ASSERT_LT_double, lt, .xfail=false);
MPT_Test(MPT_ASSERT_LT_double, gt, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_GT
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_GT_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_GT(val1, val2, "MPT_ASSERT_GT(%d, %d), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_GT(val1, val2, "MPT_ASSERT_GT(%d, %d), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_GT_int, eq, id, proc_MPT_ASSERT_GT_int, MPT_ArgValues("FAIL", 1, 1));
MPT_TestProcess(MPT_ASSERT_GT_int, lt, id, proc_MPT_ASSERT_GT_int, MPT_ArgValues("FAIL", 1, 2));
MPT_TestProcess(MPT_ASSERT_GT_int, gt, id, proc_MPT_ASSERT_GT_int, MPT_ArgValues("PASS", 3, 2));
MPT_Test(MPT_ASSERT_GT_int, eq, .xfail=true);
MPT_Test(MPT_ASSERT_GT_int, lt, .xfail=true);
MPT_Test(MPT_ASSERT_GT_int, gt, .xfail=false);


MPT_ProcessEntry(proc_MPT_ASSERT_GT_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_GT(val1, val2, "MPT_ASSERT_GT(%f, %f), 1st expect: %s", val1, val2, exp);
  MPT_ASSERT_GT(val1, val2, "MPT_ASSERT_GT(%f, %f), 2nd expect: %s", val1, val2, exp);
}
MPT_TestProcess(MPT_ASSERT_GT_double, eq, id, proc_MPT_ASSERT_GT_double, MPT_ArgValues("FAIL", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_GT_double, lt, id, proc_MPT_ASSERT_GT_double, MPT_ArgValues("FAIL", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_GT_double, gt, id, proc_MPT_ASSERT_GT_double, MPT_ArgValues("PASS", 1.3, 1.2));
MPT_Test(MPT_ASSERT_GT_double, eq, .xfail=true);
MPT_Test(MPT_ASSERT_GT_double, lt, .xfail=true);
MPT_Test(MPT_ASSERT_GT_double, gt, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_NULL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_NULL, MPT_Args(const char *exp, const void* ptr))
{
  assert(exp);
  MPT_ASSERT_NULL(ptr, "MPT_ASSERT_NULL(%p), expect: %s", ptr, exp);
}
MPT_TestProcess(MPT_ASSERT_NULL, addr, id, proc_MPT_ASSERT_NULL, MPT_ArgValues("FAIL", &dummy));
MPT_TestProcess(MPT_ASSERT_NULL, null, id, proc_MPT_ASSERT_NULL, MPT_ArgValues("PASS", NULL));
MPT_Test(MPT_ASSERT_NULL, addr, .xfail=true);
MPT_Test(MPT_ASSERT_NULL, null, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_NOT_NULL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_NOT_NULL, MPT_Args(const char *exp, const void* ptr))
{
  assert(exp);
  MPT_ASSERT_NOT_NULL(ptr, "MPT_ASSERT_NOT_NULL(%p), expect: %s", ptr, exp);
}
MPT_TestProcess(MPT_ASSERT_NOT_NULL, addr, id, proc_MPT_ASSERT_NOT_NULL, MPT_ArgValues("PASS", &dummy));
MPT_TestProcess(MPT_ASSERT_NOT_NULL, null, id, proc_MPT_ASSERT_NOT_NULL, MPT_ArgValues("FAIL", NULL));
MPT_Test(MPT_ASSERT_NOT_NULL, addr, .xfail=false);
MPT_Test(MPT_ASSERT_NOT_NULL, null, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_STR_EQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_STR_EQ, MPT_Args(const char *exp, const char* val1, const char* val2))
{
  assert(exp);
  MPT_ASSERT_STR_EQ(val1, val2, "MPT_ASSERT_STR_EQ(%s, %s), expect: %s",
                    val1 ? val1 : "<null>",
                    val2 ? val2 : "<null>",
                    exp);
}
MPT_TestProcess(MPT_ASSERT_STR_EQ,    eq, id, proc_MPT_ASSERT_STR_EQ, MPT_ArgValues("PASS", "foo", "foo"));
MPT_TestProcess(MPT_ASSERT_STR_EQ,   neq, id, proc_MPT_ASSERT_STR_EQ, MPT_ArgValues("FAIL", "foo", "bar"));
MPT_TestProcess(MPT_ASSERT_STR_EQ, null1, id, proc_MPT_ASSERT_STR_EQ, MPT_ArgValues("FAIL",  NULL, "foo"));
MPT_TestProcess(MPT_ASSERT_STR_EQ, null2, id, proc_MPT_ASSERT_STR_EQ, MPT_ArgValues("FAIL", "foo",  NULL));
MPT_Test(MPT_ASSERT_STR_EQ,    eq, .xfail=false);
MPT_Test(MPT_ASSERT_STR_EQ,   neq, .xfail=true);
MPT_Test(MPT_ASSERT_STR_EQ, null1, .xfail=true);
MPT_Test(MPT_ASSERT_STR_EQ, null2, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_STR_NEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_STR_NEQ, MPT_Args(const char *exp, const char* val1, const char* val2))
{
  assert(exp);
  MPT_ASSERT_STR_NEQ(val1, val2, "MPT_ASSERT_STR_NEQ(%s, %s), expect: %s",
                     val1 ? val1 : "<null>",
                     val2 ? val2 : "<null>",
                     exp);
}
MPT_TestProcess(MPT_ASSERT_STR_NEQ,    eq, id, proc_MPT_ASSERT_STR_NEQ, MPT_ArgValues("FAIL", "foo", "foo"));
MPT_TestProcess(MPT_ASSERT_STR_NEQ,   neq, id, proc_MPT_ASSERT_STR_NEQ, MPT_ArgValues("PASS", "foo", "bar"));
MPT_TestProcess(MPT_ASSERT_STR_NEQ, null1, id, proc_MPT_ASSERT_STR_NEQ, MPT_ArgValues("FAIL",  NULL, "foo"));
MPT_TestProcess(MPT_ASSERT_STR_NEQ, null2, id, proc_MPT_ASSERT_STR_NEQ, MPT_ArgValues("FAIL", "foo",  NULL));
MPT_Test(MPT_ASSERT_STR_NEQ,    eq, .xfail=true);
MPT_Test(MPT_ASSERT_STR_NEQ,   neq, .xfail=false);
MPT_Test(MPT_ASSERT_STR_NEQ, null1, .xfail=true);
MPT_Test(MPT_ASSERT_STR_NEQ, null2, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_STR_EMPTY
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_STR_EMPTY, MPT_Args(const char *exp, const char* val))
{
  assert(exp);
  MPT_ASSERT_STR_EMPTY(val, "MPT_ASSERT_STR_EMPTY(%s), expect: %s",
                       val ? val : "<null>",
                       exp);
}
MPT_TestProcess(MPT_ASSERT_STR_EMPTY, nempty, id, proc_MPT_ASSERT_STR_EMPTY, MPT_ArgValues("FAIL", "foo"));
MPT_TestProcess(MPT_ASSERT_STR_EMPTY,  empty, id, proc_MPT_ASSERT_STR_EMPTY, MPT_ArgValues("PASS",    ""));
MPT_TestProcess(MPT_ASSERT_STR_EMPTY,   null, id, proc_MPT_ASSERT_STR_EMPTY, MPT_ArgValues("FAIL",  NULL));
MPT_Test(MPT_ASSERT_STR_EMPTY, nempty, .xfail=true);
MPT_Test(MPT_ASSERT_STR_EMPTY,  empty, .xfail=false);
MPT_Test(MPT_ASSERT_STR_EMPTY,   null, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_STR_NOT_EMPTY
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_STR_NOT_EMPTY, MPT_Args(const char *exp, const char* val))
{
  assert(exp);
  MPT_ASSERT_STR_NOT_EMPTY(val, "MPT_ASSERT_STR_NOT_EMPTY(%s), expect: %s",
                           val ? val : "<null>",
                           exp);
}
MPT_TestProcess(MPT_ASSERT_STR_NOT_EMPTY, nempty, id, proc_MPT_ASSERT_STR_NOT_EMPTY, MPT_ArgValues("PASS", "foo"));
MPT_TestProcess(MPT_ASSERT_STR_NOT_EMPTY,  empty, id, proc_MPT_ASSERT_STR_NOT_EMPTY, MPT_ArgValues("FAIL",    ""));
MPT_TestProcess(MPT_ASSERT_STR_NOT_EMPTY,   null, id, proc_MPT_ASSERT_STR_NOT_EMPTY, MPT_ArgValues("FAIL",  NULL));
MPT_Test(MPT_ASSERT_STR_NOT_EMPTY, nempty, .xfail=false);
MPT_Test(MPT_ASSERT_STR_NOT_EMPTY,  empty, .xfail=true);
MPT_Test(MPT_ASSERT_STR_NOT_EMPTY,   null, .xfail=true);



/*****************************************************************************/



/************************************************************
 * Test MPT_ASSERT_FATAL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL, MPT_Args(const char *exp, int cond))
{
  assert(exp);
  MPT_ASSERT_FATAL(cond, "MPT_ASSERT_FATAL(%d), expect: %s", cond, exp);
  MPT_ASSERT(cond, "MPT_ASSERT(%d) after a fatal", cond);
}

MPT_TestProcess(MPT_ASSERT_FATAL, pass, id, proc_MPT_ASSERT_FATAL, MPT_ArgValues("PASS", 1));
MPT_Test(MPT_ASSERT_FATAL, pass, .xfail=false);

MPT_TestProcess(MPT_ASSERT_FATAL, fail, id, proc_MPT_ASSERT_FATAL, MPT_ArgValues("FAIL", 0));
MPT_Test(MPT_ASSERT_FATAL, fail, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_FAIL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_FAIL, MPT_NoArgs())
{
  MPT_ASSERT_FATAL_FAIL("MPT_ASSERT_FATAL_FAIL(), expect a fail, always");
  MPT_ASSERT_FAIL("MPT_ASSERT_FAIL() after a fatal");
}

MPT_TestProcess(MPT_ASSERT_FATAL_FAIL, fail, id, proc_MPT_ASSERT_FATAL_FAIL, MPT_NoArgValues());
MPT_Test(MPT_ASSERT_FATAL_FAIL, fail, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_EQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_EQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_EQ(val1, val2, "MPT_ASSERT_FATAL_EQ(%d, %d), expect: %s", val1, val2, exp);
  MPT_ASSERT_EQ(val1, val2, "MPT_ASSERT_EQ(%d, %d) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_EQ_int, eq, id, proc_MPT_ASSERT_FATAL_EQ_int, MPT_ArgValues("PASS", 1, 1));
MPT_TestProcess(MPT_ASSERT_FATAL_EQ_int, lt, id, proc_MPT_ASSERT_FATAL_EQ_int, MPT_ArgValues("FAIL", 1, 2));
MPT_TestProcess(MPT_ASSERT_FATAL_EQ_int, gt, id, proc_MPT_ASSERT_FATAL_EQ_int, MPT_ArgValues("FAIL", 3, 2));
MPT_Test(MPT_ASSERT_FATAL_EQ_int, eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_EQ_int, lt, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_EQ_int, gt, .xfail=true);


MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_EQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_EQ(val1, val2, "MPT_ASSERT_FATAL_EQ(%f, %f), expect: %s", val1, val2, exp);
  MPT_ASSERT_EQ(val1, val2, "MPT_ASSERT_EQ(%f, %f) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_EQ_double, eq, id, proc_MPT_ASSERT_FATAL_EQ_double, MPT_ArgValues("PASS", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_FATAL_EQ_double, lt, id, proc_MPT_ASSERT_FATAL_EQ_double, MPT_ArgValues("FAIL", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_FATAL_EQ_double, gt, id, proc_MPT_ASSERT_FATAL_EQ_double, MPT_ArgValues("FAIL", 1.3, 1.2));
MPT_Test(MPT_ASSERT_FATAL_EQ_double, eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_EQ_double, lt, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_EQ_double, gt, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_NEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_NEQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_NEQ(val1, val2, "MPT_ASSERT_FATAL_NEQ(%d, %d), expect: %s", val1, val2, exp);
  MPT_ASSERT_NEQ(val1, val2, "MPT_ASSERT_NEQ(%d, %d) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_NEQ_int, eq, id, proc_MPT_ASSERT_FATAL_NEQ_int, MPT_ArgValues("FAIL", 1, 1));
MPT_TestProcess(MPT_ASSERT_FATAL_NEQ_int, lt, id, proc_MPT_ASSERT_FATAL_NEQ_int, MPT_ArgValues("PASS", 1, 2));
MPT_TestProcess(MPT_ASSERT_FATAL_NEQ_int, gt, id, proc_MPT_ASSERT_FATAL_NEQ_int, MPT_ArgValues("PASS", 3, 2));
MPT_Test(MPT_ASSERT_FATAL_NEQ_int, eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_NEQ_int, lt, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_NEQ_int, gt, .xfail=false);


MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_NEQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_NEQ(val1, val2, "MPT_ASSERT_FATAL_NEQ(%f, %f), expect: %s", val1, val2, exp);
  MPT_ASSERT_NEQ(val1, val2, "MPT_ASSERT_NEQ(%f, %f) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_NEQ_double, eq, id, proc_MPT_ASSERT_FATAL_NEQ_double, MPT_ArgValues("FAIL", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_FATAL_NEQ_double, lt, id, proc_MPT_ASSERT_FATAL_NEQ_double, MPT_ArgValues("PASS", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_FATAL_NEQ_double, gt, id, proc_MPT_ASSERT_FATAL_NEQ_double, MPT_ArgValues("PASS", 1.3, 1.2));
MPT_Test(MPT_ASSERT_FATAL_NEQ_double, eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_NEQ_double, lt, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_NEQ_double, gt, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_FATAL_LEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_LEQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_LEQ(val1, val2, "MPT_ASSERT_FATAL_LEQ(%d, %d), expect: %s", val1, val2, exp);
  MPT_ASSERT_LEQ(val1, val2, "MPT_ASSERT_LEQ(%d, %d) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_LEQ_int, eq, id, proc_MPT_ASSERT_FATAL_LEQ_int, MPT_ArgValues("PASS", 1, 1));
MPT_TestProcess(MPT_ASSERT_FATAL_LEQ_int, lt, id, proc_MPT_ASSERT_FATAL_LEQ_int, MPT_ArgValues("PASS", 1, 2));
MPT_TestProcess(MPT_ASSERT_FATAL_LEQ_int, gt, id, proc_MPT_ASSERT_FATAL_LEQ_int, MPT_ArgValues("FAIL", 3, 2));
MPT_Test(MPT_ASSERT_FATAL_LEQ_int, eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_LEQ_int, lt, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_LEQ_int, gt, .xfail=true);


MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_LEQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_LEQ(val1, val2, "MPT_ASSERT_FATAL_LEQ(%f, %f), expect: %s", val1, val2, exp);
  MPT_ASSERT_LEQ(val1, val2, "MPT_ASSERT_LEQ(%f, %f) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_LEQ_double, eq, id, proc_MPT_ASSERT_FATAL_LEQ_double, MPT_ArgValues("PASS", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_FATAL_LEQ_double, lt, id, proc_MPT_ASSERT_FATAL_LEQ_double, MPT_ArgValues("PASS", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_FATAL_LEQ_double, gt, id, proc_MPT_ASSERT_FATAL_LEQ_double, MPT_ArgValues("FAIL", 1.3, 1.2));
MPT_Test(MPT_ASSERT_FATAL_LEQ_double, eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_LEQ_double, lt, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_LEQ_double, gt, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_GEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_GEQ_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_GEQ(val1, val2, "MPT_ASSERT_FATAL_GEQ(%d, %d), expect: %s", val1, val2, exp);
  MPT_ASSERT_GEQ(val1, val2, "MPT_ASSERT_GEQ(%d, %d) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_GEQ_int, eq, id, proc_MPT_ASSERT_FATAL_GEQ_int, MPT_ArgValues("PASS", 1, 1));
MPT_TestProcess(MPT_ASSERT_FATAL_GEQ_int, lt, id, proc_MPT_ASSERT_FATAL_GEQ_int, MPT_ArgValues("FAIL", 1, 2));
MPT_TestProcess(MPT_ASSERT_FATAL_GEQ_int, gt, id, proc_MPT_ASSERT_FATAL_GEQ_int, MPT_ArgValues("PASS", 3, 2));
MPT_Test(MPT_ASSERT_FATAL_GEQ_int, eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_GEQ_int, lt, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_GEQ_int, gt, .xfail=false);


MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_GEQ_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_GEQ(val1, val2, "MPT_ASSERT_FATAL_GEQ(%f, %f), expect: %s", val1, val2, exp);
  MPT_ASSERT_GEQ(val1, val2, "MPT_ASSERT_GEQ(%f, %f) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_GEQ_double, eq, id, proc_MPT_ASSERT_FATAL_GEQ_double, MPT_ArgValues("PASS", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_FATAL_GEQ_double, lt, id, proc_MPT_ASSERT_FATAL_GEQ_double, MPT_ArgValues("FAIL", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_FATAL_GEQ_double, gt, id, proc_MPT_ASSERT_FATAL_GEQ_double, MPT_ArgValues("PASS", 1.3, 1.2));
MPT_Test(MPT_ASSERT_FATAL_GEQ_double, eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_GEQ_double, lt, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_GEQ_double, gt, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_FATAL_LT
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_LT_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_LT(val1, val2, "MPT_ASSERT_FATAL_LT(%d, %d), expect: %s", val1, val2, exp);
  MPT_ASSERT_LT(val1, val2, "MPT_ASSERT_LT(%d, %d) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_LT_int, eq, id, proc_MPT_ASSERT_FATAL_LT_int, MPT_ArgValues("FAIL", 1, 1));
MPT_TestProcess(MPT_ASSERT_FATAL_LT_int, lt, id, proc_MPT_ASSERT_FATAL_LT_int, MPT_ArgValues("PASS", 1, 2));
MPT_TestProcess(MPT_ASSERT_FATAL_LT_int, gt, id, proc_MPT_ASSERT_FATAL_LT_int, MPT_ArgValues("FAIL", 3, 2));
MPT_Test(MPT_ASSERT_FATAL_LT_int, eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_LT_int, lt, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_LT_int, gt, .xfail=true);


MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_LT_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_LT(val1, val2, "MPT_ASSERT_FATAL_LT(%f, %f), expect: %s", val1, val2, exp);
  MPT_ASSERT_LT(val1, val2, "MPT_ASSERT_LT(%f, %f) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_LT_double, eq, id, proc_MPT_ASSERT_FATAL_LT_double, MPT_ArgValues("FAIL", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_FATAL_LT_double, lt, id, proc_MPT_ASSERT_FATAL_LT_double, MPT_ArgValues("PASS", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_FATAL_LT_double, gt, id, proc_MPT_ASSERT_FATAL_LT_double, MPT_ArgValues("FAIL", 1.3, 1.2));
MPT_Test(MPT_ASSERT_FATAL_LT_double, eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_LT_double, lt, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_LT_double, gt, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_GT
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_GT_int, MPT_Args(const char *exp, int val1, int val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_GT(val1, val2, "MPT_ASSERT_FATAL_GT(%d, %d), expect: %s", val1, val2, exp);
  MPT_ASSERT_GT(val1, val2, "MPT_ASSERT_GT(%d, %d) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_GT_int, eq, id, proc_MPT_ASSERT_FATAL_GT_int, MPT_ArgValues("FAIL", 1, 1));
MPT_TestProcess(MPT_ASSERT_FATAL_GT_int, lt, id, proc_MPT_ASSERT_FATAL_GT_int, MPT_ArgValues("FAIL", 1, 2));
MPT_TestProcess(MPT_ASSERT_FATAL_GT_int, gt, id, proc_MPT_ASSERT_FATAL_GT_int, MPT_ArgValues("PASS", 3, 2));
MPT_Test(MPT_ASSERT_FATAL_GT_int, eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_GT_int, lt, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_GT_int, gt, .xfail=false);


MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_GT_double, MPT_Args(const char *exp, double val1, double val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_GT(val1, val2, "MPT_ASSERT_FATAL_GT(%f, %f), expect: %s", val1, val2, exp);
  MPT_ASSERT_GT(val1, val2, "MPT_ASSERT_GT(%f, %f) after a fatal", val1, val2);
}
MPT_TestProcess(MPT_ASSERT_FATAL_GT_double, eq, id, proc_MPT_ASSERT_FATAL_GT_double, MPT_ArgValues("FAIL", 1.1, 1.1));
MPT_TestProcess(MPT_ASSERT_FATAL_GT_double, lt, id, proc_MPT_ASSERT_FATAL_GT_double, MPT_ArgValues("FAIL", 1.1, 1.2));
MPT_TestProcess(MPT_ASSERT_FATAL_GT_double, gt, id, proc_MPT_ASSERT_FATAL_GT_double, MPT_ArgValues("PASS", 1.3, 1.2));
MPT_Test(MPT_ASSERT_FATAL_GT_double, eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_GT_double, lt, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_GT_double, gt, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_FATAL_NULL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_NULL, MPT_Args(const char *exp, const void* ptr))
{
  assert(exp);
  MPT_ASSERT_FATAL_NULL(ptr, "MPT_ASSERT_FATAL_NULL(%p), expect: %s", ptr, exp);
}
MPT_TestProcess(MPT_ASSERT_FATAL_NULL, addr, id, proc_MPT_ASSERT_FATAL_NULL, MPT_ArgValues("FAIL", &dummy));
MPT_TestProcess(MPT_ASSERT_FATAL_NULL, null, id, proc_MPT_ASSERT_FATAL_NULL, MPT_ArgValues("PASS", NULL));
MPT_Test(MPT_ASSERT_FATAL_NULL, addr, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_NULL, null, .xfail=false);



/************************************************************
 * Test MPT_ASSERT_FATAL_NOT_NULL
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_NOT_NULL, MPT_Args(const char *exp, const void* ptr))
{
  assert(exp);
  MPT_ASSERT_FATAL_NOT_NULL(ptr, "MPT_ASSERT_FATAL_NOT_NULL(%p), expect: %s", ptr, exp);
}
MPT_TestProcess(MPT_ASSERT_FATAL_NOT_NULL, addr, id, proc_MPT_ASSERT_FATAL_NOT_NULL, MPT_ArgValues("PASS", &dummy));
MPT_TestProcess(MPT_ASSERT_FATAL_NOT_NULL, null, id, proc_MPT_ASSERT_FATAL_NOT_NULL, MPT_ArgValues("FAIL", NULL));
MPT_Test(MPT_ASSERT_FATAL_NOT_NULL, addr, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_NOT_NULL, null, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_STR_EQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_STR_EQ, MPT_Args(const char *exp, const char* val1, const char* val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_STR_EQ(val1, val2, "MPT_ASSERT_FATAL_STR_EQ(%s, %s), expect: %s",
                          val1 ? val1 : "<null>",
                          val2 ? val2 : "<null>",
                          exp);
}
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EQ,    eq, id, proc_MPT_ASSERT_FATAL_STR_EQ, MPT_ArgValues("PASS", "foo", "foo"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EQ,   neq, id, proc_MPT_ASSERT_FATAL_STR_EQ, MPT_ArgValues("FAIL", "foo", "bar"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EQ, null1, id, proc_MPT_ASSERT_FATAL_STR_EQ, MPT_ArgValues("FAIL",  NULL, "foo"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EQ, null2, id, proc_MPT_ASSERT_FATAL_STR_EQ, MPT_ArgValues("FAIL", "foo",  NULL));
MPT_Test(MPT_ASSERT_FATAL_STR_EQ,    eq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_STR_EQ,   neq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_STR_EQ, null1, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_STR_EQ, null2, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_STR_NEQ
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_STR_NEQ, MPT_Args(const char *exp, const char* val1, const char* val2))
{
  assert(exp);
  MPT_ASSERT_FATAL_STR_NEQ(val1, val2, "MPT_ASSERT_FATAL_STR_NEQ(%s, %s), expect: %s",
                           val1 ? val1 : "<null>",
                           val2 ? val2 : "<null>",
                           exp);
}
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NEQ,    eq, id, proc_MPT_ASSERT_FATAL_STR_NEQ, MPT_ArgValues("FAIL", "foo", "foo"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NEQ,   neq, id, proc_MPT_ASSERT_FATAL_STR_NEQ, MPT_ArgValues("PASS", "foo", "bar"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NEQ, null1, id, proc_MPT_ASSERT_FATAL_STR_NEQ, MPT_ArgValues("FAIL",  NULL, "foo"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NEQ, null2, id, proc_MPT_ASSERT_FATAL_STR_NEQ, MPT_ArgValues("FAIL", "foo",  NULL));
MPT_Test(MPT_ASSERT_FATAL_STR_NEQ,    eq, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_STR_NEQ,   neq, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_STR_NEQ, null1, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_STR_NEQ, null2, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_STR_EMPTY
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_STR_EMPTY, MPT_Args(const char *exp, const char* val))
{
  assert(exp);
  MPT_ASSERT_FATAL_STR_EMPTY(val, "MPT_ASSERT_FATAL_STR_EMPTY(%s), expect: %s",
                             val ? val : "<null>",
                             exp);
}
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EMPTY, nempty, id, proc_MPT_ASSERT_FATAL_STR_EMPTY, MPT_ArgValues("FAIL", "foo"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EMPTY,  empty, id, proc_MPT_ASSERT_FATAL_STR_EMPTY, MPT_ArgValues("PASS",    ""));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_EMPTY,   null, id, proc_MPT_ASSERT_FATAL_STR_EMPTY, MPT_ArgValues("FAIL",  NULL));
MPT_Test(MPT_ASSERT_FATAL_STR_EMPTY, nempty, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_STR_EMPTY,  empty, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_STR_EMPTY,   null, .xfail=true);



/************************************************************
 * Test MPT_ASSERT_FATAL_STR_NOT_EMPTY
 ************************************************************/
MPT_ProcessEntry(proc_MPT_ASSERT_FATAL_STR_NOT_EMPTY, MPT_Args(const char *exp, const char* val))
{
  assert(exp);
  MPT_ASSERT_FATAL_STR_NOT_EMPTY(val, "MPT_ASSERT_FATAL_STR_NOT_EMPTY(%s), expect: %s",
                                 val ? val : "<null>",
                                 exp);
}
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NOT_EMPTY, nempty, id, proc_MPT_ASSERT_FATAL_STR_NOT_EMPTY, MPT_ArgValues("PASS", "foo"));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NOT_EMPTY,  empty, id, proc_MPT_ASSERT_FATAL_STR_NOT_EMPTY, MPT_ArgValues("FAIL",    ""));
MPT_TestProcess(MPT_ASSERT_FATAL_STR_NOT_EMPTY,   null, id, proc_MPT_ASSERT_FATAL_STR_NOT_EMPTY, MPT_ArgValues("FAIL",  NULL));
MPT_Test(MPT_ASSERT_FATAL_STR_NOT_EMPTY, nempty, .xfail=false);
MPT_Test(MPT_ASSERT_FATAL_STR_NOT_EMPTY,  empty, .xfail=true);
MPT_Test(MPT_ASSERT_FATAL_STR_NOT_EMPTY,   null, .xfail=true);



/*****************************************************************************/



/************************************************************
 * Test propagation,
 * Check if failure/success is actually propagated to ctest.
 ************************************************************/
MPT_ProcessEntry(proc_propagation, MPT_Args(const char *exp, int cond, dds_duration_t delay))
{
  assert(exp);
  if (delay > 0) {
    dds_sleepfor(delay);
  }
  MPT_ASSERT(cond, "MPT_ASSERT(%d), expect: %s", cond, exp);
}
/* This should pass in the ctest results. */
MPT_TestProcess(propagation, pass, id1, proc_propagation, MPT_ArgValues("PASS", 1, 0));
MPT_TestProcess(propagation, pass, id2, proc_propagation, MPT_ArgValues("PASS", 1, 0));
MPT_Test(propagation, pass);
/* This should fail in the ctest results. */
MPT_TestProcess(propagation, fail_1st, id1, proc_propagation, MPT_ArgValues("FAIL", 0, 0));
MPT_TestProcess(propagation, fail_1st, id2, proc_propagation, MPT_ArgValues("PASS", 1, DDS_SECS(1)));
MPT_Test(propagation, fail_1st);
/* This should fail in the ctest results. */
MPT_TestProcess(propagation, fail_2nd, id1, proc_propagation, MPT_ArgValues("PASS", 1, 0));
MPT_TestProcess(propagation, fail_2nd, id2, proc_propagation, MPT_ArgValues("FAIL", 0, DDS_SECS(1)));
MPT_Test(propagation, fail_2nd);

