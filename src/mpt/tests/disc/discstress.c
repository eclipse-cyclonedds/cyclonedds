#include "mpt/mpt.h"
#include "procs/createwriter.h"

#define TEST_PUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "discstress_createwriter")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "discstress_createwriter")
MPT_TestProcess(discstress, createwriter, pub, createwriter_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(discstress, createwriter, sub, createwriter_subscriber, TEST_SUB_ARGS);
MPT_Test(discstress, createwriter, .init=createwriter_init, .fini=createwriter_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS
