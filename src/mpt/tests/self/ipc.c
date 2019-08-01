#include <stdio.h>
#include "mpt/mpt.h"



/************************************************************
 * Processes
 ************************************************************/
MPT_ProcessEntry(proc_ipc_send, MPT_NoArgs())
{
  /* This will fail and cause an internal MPT assert as long
   * as IPC is not yet implemented. */
  MPT_Send("todo: implement");
}
MPT_ProcessEntry(proc_ipc_wait, MPT_NoArgs())
{
  /* This will fail and cause an internal MPT assert as long
   * as IPC is not yet implemented. */
  MPT_Wait("todo: implement");
}



/************************************************************
 * Tests
 ************************************************************/
MPT_TestProcess(ipc, TODO, id1, proc_ipc_send, MPT_NoArgValues());
MPT_TestProcess(ipc, TODO, id2, proc_ipc_wait, MPT_NoArgValues());
MPT_Test(ipc, TODO);
