/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "CUnit/Runner.h"
#include "os/os.h"
#include "os/os_project.h"

#include <stdio.h>

CUnit_Suite_Initialize(os_report)
{
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_ERRORFILE=vdds_test_error");
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_INFOFILE=vdds_test_info");
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_LOGAPPEND=TRUE");

  return 0;
}

void remove_logs()
{
  const char * error_file_name = os_getenv(OS_PROJECT_NAME_NOSPACE_CAPS"_ERRORFILE");
  const char * info_file_name = os_getenv(OS_PROJECT_NAME_NOSPACE_CAPS"_INFOFILE");

  os_remove(error_file_name);
  os_remove(info_file_name);
}

void check_existence(os_result error_log_existence, os_result info_log_existence)
{
  const char * error_file_name = os_getenv(OS_PROJECT_NAME_NOSPACE_CAPS"_ERRORFILE");
  const char * info_file_name = os_getenv(OS_PROJECT_NAME_NOSPACE_CAPS"_INFOFILE");

  CU_ASSERT(os_access(error_file_name, OS_ROK) == error_log_existence);
  CU_ASSERT(os_access(info_file_name, OS_ROK) == info_log_existence);
}

CUnit_Suite_Cleanup(os_report)
{
  remove_logs();

  return 0;
}


CUnit_Test(os_report, re_init)
{
  os_reportInit(true);

  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);
  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultSuccess);

  os_reportExit();

  os_reportInit(true);
  check_existence(os_resultSuccess, os_resultSuccess);

  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, stack_critical)
{
  os_reportInit(true);

  OS_REPORT_STACK();

  OS_CRITICAL(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);

  check_existence(os_resultFail, os_resultFail);

  OS_REPORT_FLUSH(false);

  // Since a critical is logged, the error log should be created
  check_existence(os_resultSuccess, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, stack_non_critical)
{
  os_reportInit(true);

  OS_REPORT_STACK();

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);

  check_existence(os_resultFail, os_resultFail);

  OS_REPORT_FLUSH(false);

  // Since a non critical is logged, the error log should not be created
  check_existence(os_resultFail, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, error_file_creation_critical)
{
  os_reportInit(true);

  OS_CRITICAL(OS_FUNCTION, 0, "os_report-critical-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, error_file_creation_fatal)
{
  os_reportInit(true);

  OS_FATAL(OS_FUNCTION, 0, "os_report-fatal-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, info_file_creation_warning)
{
  os_reportInit(true);

  OS_WARNING(OS_FUNCTION, 0, "os_report-warning-test %d", __LINE__);

  check_existence(os_resultFail, os_resultSuccess);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, info_file_creation_info)
{
  os_reportInit(true);

  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultFail, os_resultSuccess);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, verbosity_low)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  os_reportVerbosity = OS_REPORT_ERROR;

  OS_WARNING(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultFail, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, verbosity_high)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  os_reportVerbosity = OS_REPORT_DEBUG;

  OS_WARNING(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultFail, os_resultSuccess);

  os_reportExit();

  remove_logs();
}



CUnit_Test(os_report, verbosity_equal)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  os_reportVerbosity = OS_REPORT_WARNING;

  OS_WARNING(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultFail, os_resultSuccess);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, stack_verbosity_low)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  os_reportVerbosity = OS_REPORT_ERROR;

  OS_REPORT_STACK();
  OS_WARNING(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);
  OS_REPORT_FLUSH(true);

  check_existence(os_resultFail, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, stack_verbosity_high)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  os_reportVerbosity = OS_REPORT_DEBUG;

  OS_REPORT_STACK();
  OS_WARNING(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);
  OS_REPORT_FLUSH(true);

  check_existence(os_resultFail, os_resultSuccess);

  os_reportExit();

  remove_logs();
}



CUnit_Test(os_report, stack_verbosity_equal)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  os_reportVerbosity = OS_REPORT_WARNING;

  OS_REPORT_STACK();
  OS_WARNING(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);
  OS_REPORT_FLUSH(true);

  check_existence(os_resultFail, os_resultSuccess);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, no_log_append)
{
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);
  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultSuccess);

  os_reportExit();

  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_LOGAPPEND=FALSE");

  os_reportInit(true);

  // Both logs should be deleted
  check_existence(os_resultFail, os_resultFail);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, log_dir)
{
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_LOGPATH=.");

  os_reportInit(true);

  check_existence(os_resultFail, os_resultFail);

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);
  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultSuccess);

  os_reportExit();

  remove_logs();
}


CUnit_Test(os_report, verbosity_env_value_info)
{
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=0");
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);
  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultSuccess);

  os_reportExit();

  remove_logs();

  //reset for other tests.
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=");
}


CUnit_Test(os_report, verbosity_env_value_error)
{
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=3");
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);
  OS_INFO(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultFail);

  os_reportExit();

  remove_logs();

  //reset for other tests.
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=");
}


CUnit_Test(os_report, verbosity_env_value_error_as_string)
{
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=ERROR");
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);
  OS_DEBUG(OS_FUNCTION, 0, "os_report-info-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultFail);

  os_reportExit();

  remove_logs();

  //reset for other tests.
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=");
}


CUnit_Test(os_report, verbosity_wrong_env_value)
{
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=WRONG");
  os_reportInit(true);
  check_existence(os_resultFail, os_resultFail);

  OS_ERROR(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);
  OS_DEBUG(OS_FUNCTION, 0, "os_report-error-test %d", __LINE__);

  check_existence(os_resultSuccess, os_resultFail);

  os_reportExit();

  remove_logs();

  //reset for other tests.
  os_putenv(OS_PROJECT_NAME_NOSPACE_CAPS"_VERBOSITY=");
}
