// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "dds/ddsrt/time.h"
#include "dds/security/core/dds_security_utils.h"

CU_Test(ddssec_security_utils, parse_xml_date)
{
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date(""), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("abc"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01D01:01:01Z"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2019-02-29T01:01:01Z"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2100-02-29T01:01:01Z"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("1969-01-01T01:01:01Z"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2010-01-01T23:59:60Z"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("1969-01-01T01:01:01+01"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("1969-01-01T01:01:01+0100"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("1969-01-01T01:01:01+0:00"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("1970-01-01T00:00:00+01:00"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.0000000000001+01:00"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.0.1+01:00"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.+01:00"), DDS_TIME_INVALID);

  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("1970-01-01T00:00:00Z"), 0);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2000-02-29T00:00:00Z"), DDS_SECS(951782400));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01Z"), DDS_SECS(1577840461));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01+00:30"), DDS_SECS(1577840461 - 30 * 60));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01+01:00"), DDS_SECS(1577840461 - 60 * 60));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01+12:00"), DDS_SECS(1577840461 - 12 * 60 * 60));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01-01:00"), DDS_SECS(1577840461 + 60 * 60));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-12-31T23:59:59Z"), DDS_SECS(1609459199));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-02-29T01:01:01Z"), DDS_SECS(1582938061));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2038-01-19T03:14:07Z"), DDS_SECS(INT32_MAX));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2038-01-19T03:14:08Z"), DDS_SECS(INT64_C(INT32_MAX + 1)));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2100-01-01T00:00:00Z"), DDS_SECS(4102444800));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2120-01-01T00:00:00Z"), DDS_SECS(4733510400));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2200-01-01T00:00:00Z"), DDS_SECS(7258118400));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2220-01-01T00:00:00Z"), DDS_SECS(7889184000));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2262-04-11T23:47:16.854775807Z"), INT64_MAX);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2262-04-11T23:47:16.854775808Z"), DDS_TIME_INVALID);
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2262-04-11T23:47:16.854775807+00:01"), INT64_MAX - DDS_SECS(60));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2262-04-11T23:47:16.854775807-00:01"), DDS_TIME_INVALID);

  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.000000001+01:00"),  INT64_C(1577836861000000001));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.0000000004+01:00"), INT64_C(1577836861000000000));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.0000000005+01:00"), INT64_C(1577836861000000001));
  CU_ASSERT_EQUAL(DDS_Security_parse_xml_date("2020-01-01T01:01:01.987654321+01:00"),  INT64_C(1577836861987654321));
}
