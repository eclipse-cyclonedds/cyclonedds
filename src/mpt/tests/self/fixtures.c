#include <stdio.h>
#include "mpt/mpt.h"
#include "mpt/resource.h" /* MPT_SOURCE_ROOT_DIR */
#include "dds/ddsrt/time.h"



/************************************************************
 * Support functions
 ************************************************************/
static int g_initcnt = 0;
static void init_inc(void)
{
  g_initcnt++;
};



/************************************************************
 * Processes
 ************************************************************/
MPT_ProcessEntry(proc_initcnt, MPT_Args(int init))
{
  MPT_ASSERT((g_initcnt == init), "init count: %d vs %d", g_initcnt, init);
}

MPT_ProcessEntry(proc_sleep, MPT_Args(dds_duration_t delay))
{
  MPT_ASSERT((delay > 0), "basically just to satisfy the compiler");
  dds_sleepfor(delay);
}



/************************************************************
 * Init fixture tests
 ************************************************************/
MPT_TestProcess(init, none, id, proc_initcnt, MPT_ArgValues(0));
MPT_Test(init, none);

MPT_TestProcess(init, null, id, proc_initcnt, MPT_ArgValues(0), .init=NULL);
MPT_Test(init, null, .init=NULL);

MPT_TestProcess(init, proc, id, proc_initcnt, MPT_ArgValues(1), .init=init_inc);
MPT_Test(init, proc);

MPT_TestProcess(init, test, id, proc_initcnt, MPT_ArgValues(1));
MPT_Test(init, test, .init=init_inc);

MPT_TestProcess(init, test_proc, id, proc_initcnt, MPT_ArgValues(2), .init=init_inc);
MPT_Test(init, test_proc, .init=init_inc);



/************************************************************
 * Disable fixture tests
 ************************************************************/
MPT_TestProcess(disabled, _true, id, proc_initcnt, MPT_ArgValues(0));
MPT_Test(disabled, _true, .disabled=true);

MPT_TestProcess(disabled, _false, id, proc_initcnt, MPT_ArgValues(0));
MPT_Test(disabled, _false, .disabled=false);



/************************************************************
 * Timeout fixture tests
 ************************************************************/
/* See if a child process is killed when the parent is killed.
 * This can only really be done manually, unfortunately. */
MPT_TestProcess(timeout, child_culling, id, proc_sleep, MPT_ArgValues(DDS_SECS(120)));
MPT_Test(timeout, child_culling, .timeout=1);
