#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "mpt/mpt.h"
#include "mpt/resource.h" /* MPT_SOURCE_ROOT_DIR */
#include "dds/ddsrt/environ.h"



/************************************************************
 * Process
 ************************************************************/
MPT_ProcessEntry(proc_environments, MPT_Args(mpt_env_t *exp))
{
  assert(exp);
  while ((exp->name != NULL) && (exp->value != NULL)) {
    /* The environment variable value should match the expected value. */
    char *ptr = NULL;
    ddsrt_getenv(exp->name, &ptr);
    if (ptr) {
      MPT_ASSERT((strcmp(exp->value, ptr) == 0), "%s: found \"%s\", expected \"%s\"", exp->name, ptr, exp->value);
    } else {
      MPT_ASSERT(0, "Expected \"%s\" not found in environment", exp->name);
    }
    exp++;
  }
}



/************************************************************
 * Basic environment tests
 ************************************************************/
static mpt_env_t environ_basic[] = {
    { "MY_ENV_VAR1",  "1"   },
    { "MY_ENV_VAR2",  "2"   },
    { NULL,           NULL  }
};

MPT_TestProcess(environment, proc, id, proc_environments, MPT_ArgValues(environ_basic), .environment=environ_basic);
MPT_Test(environment, proc);

MPT_TestProcess(environment, test, id, proc_environments, MPT_ArgValues(environ_basic));
MPT_Test(environment, test, .environment=environ_basic);



/************************************************************
 * Expanding variables environment tests
 ************************************************************/
static mpt_env_t environ_expand[] = {
    { "1",       "b"                  },
    { "2",       "${1}l"              },
    { "3",       "${2}aat"            },
    { "4",       "bla${1}${2}a${3}"   },
    { NULL,      NULL                 }
};
static mpt_env_t expect_expand[] = {
    { "1",       "b"                  },
    { "2",       "bl"                 },
    { "3",       "blaat"              },
    { "4",       "blabblablaat"       },
    { NULL,      NULL                 }
};

MPT_TestProcess(environment, expand_proc, id, proc_environments, MPT_ArgValues(expect_expand), .environment=environ_expand);
MPT_Test(environment, expand_proc);

MPT_TestProcess(environment, expand_test, id, proc_environments, MPT_ArgValues(expect_expand));
MPT_Test(environment, expand_test, .environment=environ_expand);



/************************************************************
 * Environment inheritance test
 ************************************************************/
static mpt_env_t environ_test[] = {
    { "ETC_DIR",     MPT_SOURCE_ROOT_DIR"/tests/self/etc"                   },
    { "OVERRULE",    "NO"                                                   },
    { NULL,          NULL                                                   }
};
static mpt_env_t environ_proc[] = {
    { "CYCLONE_URI", "file://${ETC_DIR}/ospl.xml"                           },
    { "OVERRULE",    "YES"                                                  },
    { "EXTRA",       "proc"                                                 },
    { NULL,          NULL                                                   }
};
static mpt_env_t environ_test_proc[] = {
    { "ETC_DIR",     MPT_SOURCE_ROOT_DIR"/tests/self/etc"                   },
    { "CYCLONE_URI", "file://"MPT_SOURCE_ROOT_DIR"/tests/self/etc/ospl.xml" },
    { "OVERRULE",    "YES"                                                  },
    { "EXTRA",       "proc"                                                 },
    { NULL,          NULL                                                   }
};
MPT_TestProcess(environment, inheritance, id, proc_environments, MPT_ArgValues(environ_test_proc), .environment=environ_proc);
MPT_Test(environment, inheritance, .environment=environ_test);
