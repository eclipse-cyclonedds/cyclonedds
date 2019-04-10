#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "mpt/mpt.h"
#include "mpt/resource.h" /* MPT_SOURCE_ROOT_DIR */

/************************************************************
 * Processes
 ************************************************************/
MPT_ProcessEntry(proc_file, MPT_NoArgs())
{
  const char *test_file = MPT_SOURCE_ROOT_DIR"/tests/self/etc/file";
#if _WIN32
  struct _stat buffer;
  int ret = _stat(test_file,&buffer);
#else
  struct stat buffer;
  int ret = stat(test_file,&buffer);
#endif
  MPT_ASSERT((ret == 0), "%s", test_file);
}



/************************************************************
 * Test if MPT_SOURCE_ROOT_DIR is a valid location.
 ************************************************************/
MPT_TestProcess(resources, root_dir, id, proc_file, MPT_NoArgValues());
MPT_Test(resources, root_dir);

