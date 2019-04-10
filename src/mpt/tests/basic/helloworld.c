#include "dds/dds.h"
#include "mpt/mpt.h"
#include "mpt/resource.h" /* MPT_SOURCE_ROOT_DIR */
#include "procs/hello.h" /* publisher and subscriber entry points. */


/*
 * Tests to check simple communication between a publisher and subscriber.
 * The published text should be received by the subscriber.
 */


/* Environments */
static mpt_env_t environment_any[] = {
    { "ETC_DIR",        MPT_SOURCE_ROOT_DIR"/tests/basic/etc"   },
    { "CYCLONEDDS_URI", "file://${ETC_DIR}/config_any.xml"      },
    { NULL,             NULL                                    }
};
static mpt_env_t environment_42[] = {
    { "ETC_DIR",        MPT_SOURCE_ROOT_DIR"/tests/basic/etc"   },
    { "DOMAIN_ID",      "42"                                    },
    { "CYCLONEDDS_URI", "file://${ETC_DIR}/config_specific.xml" },
    { NULL,             NULL                                    }
};



/**********************************************************************
 * No CYCLONEDDS_URI set.
 **********************************************************************/
#define TEST_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "hello_def", 1, "No environment set")
MPT_TestProcess(helloworld, default, pub, hello_publisher,  TEST_ARGS);
MPT_TestProcess(helloworld, default, sub, hello_subscriber, TEST_ARGS);
MPT_Test(helloworld, default, .init=hello_init, .fini=hello_fini);
#undef TEST_ARGS



/**********************************************************************
 * Config domain is any. Test domain is default.
 **********************************************************************/
#define TEST_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "hello_any", 1, "Some nice text over any domain")
MPT_TestProcess(helloworld, domain_any, pub, hello_publisher,  TEST_ARGS);
MPT_TestProcess(helloworld, domain_any, sub, hello_subscriber, TEST_ARGS);
MPT_Test(helloworld, domain_any, .init=hello_init, .fini=hello_fini, .environment=environment_any);
#undef TEST_ARGS



/**********************************************************************
 * Pub: Config domain is any. Test domain is 42.
 * Sub: Config domain is 42 (through DOMAIN_ID env). Test domain is default.
 **********************************************************************/
#define TEST_PUB_ARGS MPT_ArgValues(42,                 "hello_42", 1, "Now domain 42 is used")
#define TEST_SUB_ARGS MPT_ArgValues(DDS_DOMAIN_DEFAULT, "hello_42", 1, "Now domain 42 is used")
MPT_TestProcess(helloworld, domain_42, pub, hello_publisher,  TEST_PUB_ARGS, .environment=environment_any);
MPT_TestProcess(helloworld, domain_42, sub, hello_subscriber, TEST_SUB_ARGS, .environment=environment_42);
MPT_Test(helloworld, domain_42, .init=hello_init, .fini=hello_fini);
#undef TEST_SUB_ARGS
#undef TEST_PUB_ARGS



