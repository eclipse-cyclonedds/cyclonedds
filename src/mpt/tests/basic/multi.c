#include "mpt/mpt.h"
#include "procs/hello.h"


/*
 * Tests to check communication between multiple publisher(s) and subscriber(s).
 */


/*
 * The publisher expects 2 publication matched.
 * The subscribers expect 1 sample each.
 */
#define TEST_PUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_pubsubsub", 2, "pubsubsub")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_pubsubsub", 1, "pubsubsub")
MPT_TestProcess(multi, pubsubsub, pub,  hello_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(multi, pubsubsub, sub1, hello_subscriber, TEST_SUB_ARGS);
MPT_TestProcess(multi, pubsubsub, sub2, hello_subscriber, TEST_SUB_ARGS);
MPT_Test(multi, pubsubsub, .init=hello_init, .fini=hello_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS


/*
 * The publishers expect 1 publication matched each.
 * The subscriber expects 2 samples.
 */
#define TEST_PUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_pubpubsub", 1, "pubpubsub")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_pubpubsub", 2, "pubpubsub")
MPT_TestProcess(multi, pubpubsub, pub1, hello_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(multi, pubpubsub, pub2, hello_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(multi, pubpubsub, sub,  hello_subscriber, TEST_SUB_ARGS);
MPT_Test(multi, pubpubsub, .init=hello_init, .fini=hello_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS


/*
 * The publishers expect 2 publication matched each.
 * The subscribers expect 2 samples each.
 */
#define TEST_PUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_pubpubsubsub", 2, "pubpubsubsub")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "multi_pubpubsubsub", 2, "pubpubsubsub")
MPT_TestProcess(multi, pubpubsubsub, pub1, hello_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(multi, pubpubsubsub, pub2, hello_publisher,  TEST_PUB_ARGS);
MPT_TestProcess(multi, pubpubsubsub, sub1, hello_subscriber, TEST_SUB_ARGS);
MPT_TestProcess(multi, pubpubsubsub, sub2, hello_subscriber, TEST_SUB_ARGS);
MPT_Test(multi, pubpubsubsub, .init=hello_init, .fini=hello_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS
