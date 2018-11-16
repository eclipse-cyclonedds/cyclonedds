#include <stdio.h>
#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "ddsc/dds.h"
#include "os/os.h"
#include <stdlib.h>


Test(dds_time, createTime)
{
	dds_time_t resultTime;
	resultTime= dds_time();

	cr_assert_gt(resultTime, 0);
}
