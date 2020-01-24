/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdlib.h>

#include "dds/dds.h"
#include "CUnit/Test.h"
#include "config_env.h"
#include "dds/version.h"
#include "dds/ddsrt/environ.h"

CU_Test(ddsc_domain, get_domainid)
{
  dds_entity_t pp, d, x;
  dds_return_t rc;
  uint32_t did;
  pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  d = dds_get_parent (pp);
  CU_ASSERT_FATAL (d > 0);
  x = dds_get_parent (d);
  CU_ASSERT_FATAL (x == DDS_CYCLONEDDS_HANDLE);
  x = dds_get_parent (x);
  CU_ASSERT_FATAL (x == 0);

  rc = dds_get_domainid (pp, &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (did == 0);
  rc = dds_get_domainid (d, &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (did == 0);
  rc = dds_get_domainid (DDS_CYCLONEDDS_HANDLE, &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (did == DDS_DOMAIN_DEFAULT);
  rc = dds_delete (pp);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
}

CU_Test(ddsc_domain, delete_domain0)
{
  dds_entity_t pp[3], d[3];
  dds_return_t rc;
  uint32_t did;
  for (dds_domainid_t i = 0; i < (dds_domainid_t) (sizeof (pp) / sizeof (pp[0])); i++)
  {
    pp[i] = dds_create_participant (0, NULL, NULL);
    CU_ASSERT_FATAL (pp[i] > 0);
    d[i] = dds_get_parent (pp[i]);
    CU_ASSERT_FATAL (d[i] > 0);
    if (i > 0)
      CU_ASSERT_FATAL (d[i] == d[i-1]);
  }
  rc = dds_delete (pp[0]);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_get_domainid (pp[0], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  for (size_t i = 1; i < sizeof (pp) / sizeof (pp[0]); i++)
  {
    rc = dds_get_domainid (pp[i], &did);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    CU_ASSERT_FATAL (did == 0);
  }
  rc = dds_delete (d[1]);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  /* Deleting the domain should delete all participants in it as well,
     and as there is only a single domain in this test, that should
     de-initialize the library.

     A non-initialized library returns PRECONDITION_NOT_MET; an
     initialized one given an invalid handle returns BAD_PARAMETER,
     so we can distinguish the two cases. */
  rc = dds_get_domainid (pp[1], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_Test(ddsc_domain, delete_domainM)
{
  dds_entity_t pp[3], d[3], x;
  dds_return_t rc;
  uint32_t did;
  for (dds_domainid_t i = 0; i < (dds_domainid_t) (sizeof (pp) / sizeof (pp[0])); i++)
  {
    pp[i] = dds_create_participant (i, NULL, NULL);
    CU_ASSERT_FATAL (pp[i] > 0);
    d[i] = dds_get_parent (pp[i]);
    CU_ASSERT_FATAL (d[i] > 0);
    for (dds_domainid_t j = 0; j < i; j++)
      CU_ASSERT_FATAL (d[i] != d[j]);
  }

  /* deleting participant 0 should tear down domain 0, but nothing else */
  rc = dds_delete (pp[0]);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_get_domainid (pp[0], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  rc = dds_get_domainid (d[0], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);

  /* deleting domain should delete participant 1, but leave domain 2 alone */
  rc = dds_delete (d[1]);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_get_domainid (pp[1], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_BAD_PARAMETER);
  x = dds_get_parent (pp[2]);
  CU_ASSERT_FATAL (x == d[2]);

  /* after deleting participant 2, everything should be gone */
  rc = dds_delete (pp[2]);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_get_domainid (pp[1], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_Test(ddsc_domain, delete_cyclonedds)
{
  dds_entity_t pp[3], d[3];
  dds_return_t rc;
  uint32_t did;
  for (dds_domainid_t i = 0; i < (dds_domainid_t) (sizeof (pp) / sizeof (pp[0])); i++)
  {
    pp[i] = dds_create_participant (i, NULL, NULL);
    CU_ASSERT_FATAL (pp[i] > 0);
    d[i] = dds_get_parent (pp[i]);
    CU_ASSERT_FATAL (d[i] > 0);
    for (dds_domainid_t j = 0; j < i; j++)
      CU_ASSERT_FATAL (d[i] != d[j]);
  }

  /* deleting participant 0 should tear down domain 0, but nothing else */
  rc = dds_delete (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_get_domainid (pp[0], &did);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_PRECONDITION_NOT_MET);
}

CU_Test(ddsc_domain_create, valid)
{
  dds_return_t ret;
  dds_domainid_t did;
  dds_entity_t domain;

  domain = dds_create_domain(1, "<"DDS_PROJECT_NAME"><Domain><Id>1</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain > 0);

  ret = dds_get_domainid (domain, &did);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(did == 1);

  ret = dds_delete(domain);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  ret = dds_delete(domain);
  CU_ASSERT_FATAL(ret != DDS_RETCODE_OK);
}

CU_Test(ddsc_domain_create, mismatch)
{
  dds_return_t ret;
  dds_domainid_t did;
  dds_entity_t domain;

  /* The config should have been ignored. */
  domain = dds_create_domain(2, "<"DDS_PROJECT_NAME"><Domain><Id>3</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain > 0);

  ret = dds_get_domainid (domain, &did);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(did == 2);

  ret = dds_delete(domain);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
}

CU_Test(ddsc_domain_create, empty)
{
  dds_return_t ret;
  dds_domainid_t did;
  dds_entity_t domain;

  /* This should create a domain with default settings. */
  domain = dds_create_domain(3, "");
  CU_ASSERT_FATAL(domain > 0);

  ret = dds_get_domainid (domain, &did);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(did == 3);

  ret = dds_delete(domain);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
}

CU_Test(ddsc_domain_create, null)
{
  dds_return_t ret;
  dds_domainid_t did;
  dds_entity_t domain;

  /* This should start create a domain with default settings. */
  domain = dds_create_domain(5, NULL);
  CU_ASSERT_FATAL(domain > 0);

  ret = dds_get_domainid (domain, &did);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(did == 5);

  ret = dds_delete(domain);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
}

CU_Test(ddsc_domain_create, after_domain)
{
  dds_entity_t domain1;
  dds_entity_t domain2;

  domain1 = dds_create_domain(4, "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain1 > 0);

  domain2 = dds_create_domain(4, "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain2 == DDS_RETCODE_PRECONDITION_NOT_MET);

  dds_delete(domain1);
}

CU_Test(ddsc_domain_create, after_participant)
{
  dds_entity_t domain;
  dds_entity_t participant;

  participant = dds_create_participant (5, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);

  domain = dds_create_domain(5, "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain == DDS_RETCODE_PRECONDITION_NOT_MET);

  dds_delete(participant);
}

CU_Test(ddsc_domain_create, diff)
{
  dds_return_t ret;
  dds_domainid_t did;
  dds_entity_t domain1;
  dds_entity_t domain2;

  domain1 = dds_create_domain(1, "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain1 > 0);

  domain2 = dds_create_domain(2, "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain2 > 0);

  ret = dds_get_domainid (domain1, &did);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(did == 1);

  ret = dds_get_domainid (domain2, &did);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  CU_ASSERT_FATAL(did == 2);

  ret = dds_delete(domain1);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);
  ret = dds_delete(domain2);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);

  ret = dds_delete(domain1);
  CU_ASSERT_FATAL(ret != DDS_RETCODE_OK);
  ret = dds_delete(domain2);
  CU_ASSERT_FATAL(ret != DDS_RETCODE_OK);
}

CU_Test(ddsc_domain_create, domain_default)
{
  dds_entity_t domain;
  domain = dds_create_domain(DDS_DOMAIN_DEFAULT, "<"DDS_PROJECT_NAME"><Domain><Id>any</Id></Domain></"DDS_PROJECT_NAME">");
  CU_ASSERT_FATAL(domain == DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_domain_create, invalid_xml)
{
  dds_entity_t domain;
  domain = dds_create_domain(1, "<CycloneDDS incorrect XML");
  CU_ASSERT_FATAL(domain == DDS_RETCODE_ERROR);
}
