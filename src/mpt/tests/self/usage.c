#include <stdio.h>
#include "mpt/mpt.h"
#include "mpt/resource.h" /* MPT_SOURCE_ROOT_DIR */


/******************************************************************************
 * First, we need a process entry-point that can be used in tests.
 *****************************************************************************/
/*              |  name     | arguments   |    */
MPT_ProcessEntry(proc_noargs, MPT_NoArgs())
{
    // Do stuff

    // The test processes will use asserts to indicate success/failures.
    MPT_ASSERT(1, "The problem is: %s", "existential crisis");

    // No need to return anything, that's handled by the assert calls.
}

/******************************************************************************
 * A process entry-point can have arguments.
 *****************************************************************************/
/*              |  name   | arguments                            |   */
MPT_ProcessEntry(proc_args, MPT_Args(int domain, const char* text))
{
  int expected = 1;
  MPT_ASSERT(expected == domain, "proc_args(%d, %s)", domain, text);
}

/******************************************************************************
 * Process entry-points can communicate to be able to sync fi.
 *****************************************************************************/
MPT_ProcessEntry(proc_recv, MPT_NoArgs())
{
  /* This will wait until another process sends the same string. */
  MPT_Wait("some state reached");
}
MPT_ProcessEntry(proc_send, MPT_NoArgs())
{
  /* If this fails, an internal MPT_ASSERT will be triggered.
   * The same is true for MPT_Wait(). */
  MPT_Send("some state reached");
}



/******************************************************************************
 * Test: suitename_testA
 ******************************************************************************
 * A simple test that starts two processes. Because a test can use the same
 * process entry-point to start multiple processes, each process has to have
 * its own unique id within the test.
 */
/*             | process identification | entry-point | arguments        |   */
MPT_TestProcess(suitename, testA, id1,    proc_noargs,  MPT_NoArgValues());
MPT_TestProcess(suitename, testA, id2,    proc_noargs,  MPT_NoArgValues());
MPT_Test(suitename, testA);




/******************************************************************************
 * Test: suitename_testB
 ******************************************************************************
 * Of course, different processes can be started as well.
 * Argument values are provided per test process.
 */
MPT_TestProcess(suitename, testB, id1, proc_noargs, MPT_NoArgValues(    ));
MPT_TestProcess(suitename, testB, id2, proc_args,   MPT_ArgValues(1, "2"));
MPT_TestProcess(suitename, testB, id3, proc_args,   MPT_ArgValues(1, "3"));
MPT_Test(suitename, testB);




/******************************************************************************
 * Test: suitename_testC
 ******************************************************************************
 * The processes can have different or equal 'system environments'.
 */
mpt_env_t environment_C1[] = {
  { "CYCLONEDDS_URI", "file://config1.xml"     },
  { "PERMISSIONS",    "file://permissions.p7s" },
  { "GOVERNANCE",     "file://governance.p7s"  },
  { NULL,             NULL                     }
};
mpt_env_t environment_C2[] = {
  { "CYCLONEDDS_URI", "file://config2.xml"     },
  { "PERMISSIONS",    "file://permissions.p7s" },
  { "GOVERNANCE",     "file://governance.p7s"  },
  { NULL,             NULL                     }
};
MPT_TestProcess(suitename, testC, id1, proc_noargs, MPT_NoArgValues(), .environment=environment_C1);
MPT_TestProcess(suitename, testC, id2, proc_noargs, MPT_NoArgValues(), .environment=environment_C1);
MPT_TestProcess(suitename, testC, id3, proc_noargs, MPT_NoArgValues(), .environment=environment_C2);
MPT_Test(suitename, testC);




/******************************************************************************
 * Test: suitename_testD
 ******************************************************************************
 * The two environments in the previous example are partly the same.
 * It's possible set the environment on test level. The environment variables
 * related to the test are set before the ones related to a process. This
 * means that a process can overrule variables.
 *
 * The following test is the same as the previous one.
 */
mpt_env_t environment_D1[] = {
  { "CYCLONEDDS_URI", "file://config1.xml"     },
  { "PERMISSIONS",    "file://permissions.p7s" },
  { "GOVERNANCE",     "file://governance.p7s"  },
  { NULL,             NULL                     }
};
mpt_env_t environment_D2[] = {
  { "CYCLONEDDS_URI", "file://config2.xml"     },
  { NULL,             NULL                     }
};
MPT_TestProcess(suitename, testD, id1, proc_noargs, MPT_NoArgValues());
MPT_TestProcess(suitename, testD, id2, proc_noargs, MPT_NoArgValues());
MPT_TestProcess(suitename, testD, id3, proc_noargs, MPT_NoArgValues(), .environment=environment_D2);
MPT_Test(suitename, testD, .environment=environment_D1);




/******************************************************************************
 * Test: suitename_testE
 ******************************************************************************
 * Environment variables will be expanded.
 * Also, the MPT_SOURCE_ROOT_DIR define contains a string to that particular
 * directory.
 * This can be combined to easily point to files.
 */
mpt_env_t environment_E[] = {
  { "ETC_DIR",        MPT_SOURCE_ROOT_DIR"/tests/self/etc"  },
  { "CYCLONEDDS_URI", "file://${ETC_DIR}/config.xml"        },
  { NULL,             NULL                                  }
};
MPT_TestProcess(suitename, testE, id, proc_noargs, MPT_NoArgValues(), .environment=environment_E);
MPT_Test(suitename, testE);




/******************************************************************************
 * Test: suitename_testF
 ******************************************************************************
 * The processes and tests can use init/fini fixtures.
 * The test init is executed before the process init.
 * The process fini is executed before the test fini.
 */
void proc_setup(void)    { /* do stuff */ }
void proc_teardown(void) { /* do stuff */ }
void test_setup(void)    { /* do stuff */ }
void test_teardown(void) { /* do stuff */ }
MPT_TestProcess(suitename, testF, id1, proc_noargs, MPT_NoArgValues(), .init=proc_setup);
MPT_TestProcess(suitename, testF, id2, proc_noargs, MPT_NoArgValues(), .fini=proc_teardown);
MPT_TestProcess(suitename, testF, id3, proc_noargs, MPT_NoArgValues(), .init=proc_setup, .fini=proc_teardown);
MPT_Test(suitename, testF, .init=test_setup, .fini=test_teardown);




/******************************************************************************
 * Test: suitename_testG
 ******************************************************************************
 * The timeout and disable options are handled by test fixtures.
 */
MPT_TestProcess(suitename, testG, id1, proc_noargs,  MPT_NoArgValues());
MPT_TestProcess(suitename, testG, id2, proc_noargs,  MPT_NoArgValues());
MPT_Test(suitename, testG, .timeout=10, .disabled=true);




/******************************************************************************
 * Test: suitename_testH
 ******************************************************************************
 * See the process entries to notice the MPT Send/Wait IPC.
 */
MPT_TestProcess(suitename, testH, id1, proc_recv,  MPT_NoArgValues());
MPT_TestProcess(suitename, testH, id2, proc_send,  MPT_NoArgValues());
MPT_Test(suitename, testH);
