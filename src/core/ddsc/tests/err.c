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
#include "ddsc/dds.h"
#include "CUnit/Test.h"

CU_Test(ddsc_err, str)
{
    CU_ASSERT_STRING_EQUAL(dds_err_str(1                                      ), "Success");
    CU_ASSERT_STRING_EQUAL(dds_err_str(-255                                   ), "Unknown");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_OK                     * -1), "Success");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_ERROR                  * -1), "Error");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_UNSUPPORTED            * -1), "Unsupported");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_BAD_PARAMETER          * -1), "Bad Parameter");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_PRECONDITION_NOT_MET   * -1), "Precondition Not Met");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_OUT_OF_RESOURCES       * -1), "Out Of Resources");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_NOT_ENABLED            * -1), "Not Enabled");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_IMMUTABLE_POLICY       * -1), "Immutable Policy");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_INCONSISTENT_POLICY    * -1), "Inconsistent Policy");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_ALREADY_DELETED        * -1), "Already Deleted");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_TIMEOUT                * -1), "Timeout");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_NO_DATA                * -1), "No Data");
    CU_ASSERT_STRING_EQUAL(dds_err_str(DDS_RETCODE_ILLEGAL_OPERATION      * -1), "Illegal Operation");
}
