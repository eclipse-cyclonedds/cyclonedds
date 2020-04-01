/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/bswap.h"

#include "test_common.h"
#include "InstanceHandleTypes.h"

static dds_entity_t dp, tp[3], rd[3], wr[3];

static void instance_handle_init (void)
{
  char topicname[100];
  dds_qos_t *qos;
  dp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);

  /* not strictly necessary to explicitly set KEEP_LAST (it is the default), nor to make
     it reliable (it is only used inside a process without any limits that might cause it
     to drop samples) */
  qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_LAST, 1);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  create_unique_topic_name ("instance_handle", topicname, sizeof (topicname));
  tp[0] = dds_create_topic (dp, &InstanceHandleTypes_A_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp[0] > 0);
  create_unique_topic_name ("instance_handle", topicname, sizeof (topicname));
  tp[1] = dds_create_topic (dp, &InstanceHandleTypes_A_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp[1] > 0);
  create_unique_topic_name ("instance_handle", topicname, sizeof (topicname));
  tp[2] = dds_create_topic (dp, &InstanceHandleTypes_C_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp[2] > 0);
  dds_delete_qos (qos);
  for (size_t i = 0; i < 3; i++)
  {
    rd[i] = dds_create_reader (dp, tp[i], NULL, NULL);
    CU_ASSERT_FATAL (rd[i] > 0);
    wr[i] = dds_create_writer (dp, tp[i], NULL, NULL);
    CU_ASSERT_FATAL (wr[i] > 0);
  }
}

static void instance_handle_fini (void)
{
  dds_return_t rc;
  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test (ddsc_instance_handle, a, .init = instance_handle_init, .fini = instance_handle_fini)
{
  /* By design, Cyclone maintains a global map of (topic class, X, key value) to instance handle,
     where the "topic class" is the implementation of the topic (e.g., "default" or "builtin"),
     "X" is dependent on the topic class but by design is a fixed constant for the default one,
     and key value, again by design, is taken as the serialised representation of the key value.

     The point behind this model is that it allows one to use an instance handle obtained on
     one reader and use it to read the matching instance in another reader.  So that bit of
     behaviour needs to be checked.

     I'm not sure whether the "serialised" part should be included in the test, I don't think
     that's something that should be guaranteed in the API.  However, it is worth verifying that
     it doesn't go off and do something weird. */
  InstanceHandleTypes_A a, b;
  InstanceHandleTypes_C c;
  dds_return_t rc;

  for (uint32_t i = 1; i <= 5; i++)
  {
    a.k = i;
    a.v = i;
    b.k = i;
    b.v = 2 * a.k;
    const uint32_t a_k_be = ddsrt_toBE4u (a.k);
    memcpy (c.k, &a_k_be, sizeof (c.k));
    c.v = 3 * a.k;
    rc = dds_write (wr[0], &a);
    CU_ASSERT_FATAL (rc == 0);
    rc = dds_write (wr[1], &b);
    CU_ASSERT_FATAL (rc == 0);
    rc = dds_write (wr[2], &c);
    CU_ASSERT_FATAL (rc == 0);
  }

  for (uint32_t i = 1; i <= 5; i++)
  {
    dds_sample_info_t siA, siB, siC;
    void *rawA = &a, *rawB = &b, *rawC = &c;

    /* take one sample from A; no guarantee about the order in which the data is returned */
    rc = dds_take (rd[0], &rawA, &siA, 1, 1);
    CU_ASSERT_FATAL (rc == 1);
    CU_ASSERT_FATAL (siA.valid_data);
    CU_ASSERT_FATAL (1 <= a.k && a.k <= 5 && a.v == a.k);

    /* take one sample from B using the instance handle just returned */
    rc = dds_take_instance (rd[1], &rawB, &siB, 1, 1, siA.instance_handle);
    CU_ASSERT_FATAL (rc == 1);
    CU_ASSERT_FATAL (siB.valid_data);
    CU_ASSERT_FATAL (siB.instance_handle == siA.instance_handle);
    CU_ASSERT_FATAL (b.k == a.k && b.v == 2 * a.k);

    /* take one sample from C using the instance handle just returned, this should work
       even though C uses an array of octets as key */
    rc = dds_take_instance (rd[2], &rawC, &siC, 1, 1, siA.instance_handle);
    CU_ASSERT_FATAL (rc == 1);
    CU_ASSERT_FATAL (siC.valid_data);
    CU_ASSERT_FATAL (siC.instance_handle == siA.instance_handle);
    const uint32_t a_k_be = ddsrt_toBE4u (a.k);
    CU_ASSERT_FATAL (memcmp (c.k, &a_k_be, sizeof (c.k)) == 0 && c.v == 3 * a.k);
  }

  /* there should be no data left */
  for (size_t i = 0; i < 3; i++)
  {
    dds_sample_info_t si;
    void *raw = NULL;
    rc = dds_take (rd[0], &raw, &si, 1, 1);
    CU_ASSERT_FATAL (rc == 0);
  }
}
