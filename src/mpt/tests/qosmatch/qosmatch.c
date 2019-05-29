#include "mpt/mpt.h"
#include "procs/rw.h"


/*
 * Tests to check communication between multiple publisher(s) and subscriber(s).
 */


/*
 * The publisher expects 2 publication matched.
 * The subscribers expect 1 sample each.
 */
#define TEST_PUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_qosmatch")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_qosmatch")
MPT_TestProcess(qosmatch, qosmatch, pub, rw_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(qosmatch, qosmatch, sub, rw_subscriber, TEST_SUB_ARGS);
MPT_Test(qosmatch, qosmatch, .init=rw_init, .fini=rw_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS
