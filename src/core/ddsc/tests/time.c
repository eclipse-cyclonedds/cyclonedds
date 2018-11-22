#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "ddsc/dds.h"

Test(ddsc_time, request_time)
{
    dds_time_t now, then;
    dds_duration_t pause = 1 * DDS_NSECS_IN_SEC;

    now = dds_time();
    cr_assert_gt(now, 0);
    /* Sleep for 1 second, every platform should (hopefully) support that */
    dds_sleepfor(pause);
    then = dds_time();
    cr_assert_geq(then, now + pause);
}
