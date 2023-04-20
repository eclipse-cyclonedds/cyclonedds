// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "dds/ddsrt/bswap.h"

#include "test_common.h"
#include "InstanceHandleTypes.h"

#include "dds/ddsi/ddsi_serdata.h"

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
    c.k = i;
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
       for different topic that have the same key type */
    rc = dds_take_instance (rd[2], &rawC, &siC, 1, 1, siA.instance_handle);
    CU_ASSERT_FATAL (rc == 1);
    CU_ASSERT_FATAL (siC.valid_data);
    CU_ASSERT_FATAL (siC.instance_handle == siA.instance_handle);
    CU_ASSERT_FATAL (c.k == a.k && c.v == 3 * a.k);
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

static const InstanceHandleTypes_MD5 md5xs[] = {
  {{ 0xd1,0x31,0xdd,0x02,0xc5,0xe6,0xee,0xc4,0x69,0x3d,0x9a,0x06,0x98,0xaf,0xf9,0x5c,
     0x2f,0xca,0xb5,0x87,0x12,0x46,0x7e,0xab,0x40,0x04,0x58,0x3e,0xb8,0xfb,0x7f,0x89,
     0x55,0xad,0x34,0x06,0x09,0xf4,0xb3,0x02,0x83,0xe4,0x88,0x83,0x25,0x71,0x41,0x5a,
     0x08,0x51,0x25,0xe8,0xf7,0xcd,0xc9,0x9f,0xd9,0x1d,0xbd,0xf2,0x80,0x37,0x3c,0x5b,
     0xd8,0x82,0x3e,0x31,0x56,0x34,0x8f,0x5b,0xae,0x6d,0xac,0xd4,0x36,0xc9,0x19,0xc6,
     0xdd,0x53,0xe2,0xb4,0x87,0xda,0x03,0xfd,0x02,0x39,0x63,0x06,0xd2,0x48,0xcd,0xa0,
     0xe9,0x9f,0x33,0x42,0x0f,0x57,0x7e,0xe8,0xce,0x54,0xb6,0x70,0x80,0xa8,0x0d,0x1e,
     0xc6,0x98,0x21,0xbc,0xb6,0xa8,0x83,0x93,0x96,0xf9,0x65,0x2b,0x6f,0xf7,0x2a,0x70 }},
  {{ 0xd1,0x31,0xdd,0x02,0xc5,0xe6,0xee,0xc4,0x69,0x3d,0x9a,0x06,0x98,0xaf,0xf9,0x5c,
     0x2f,0xca,0xb5,0x07,0x12,0x46,0x7e,0xab,0x40,0x04,0x58,0x3e,0xb8,0xfb,0x7f,0x89,
     0x55,0xad,0x34,0x06,0x09,0xf4,0xb3,0x02,0x83,0xe4,0x88,0x83,0x25,0xf1,0x41,0x5a,
     0x08,0x51,0x25,0xe8,0xf7,0xcd,0xc9,0x9f,0xd9,0x1d,0xbd,0x72,0x80,0x37,0x3c,0x5b,
     0xd8,0x82,0x3e,0x31,0x56,0x34,0x8f,0x5b,0xae,0x6d,0xac,0xd4,0x36,0xc9,0x19,0xc6,
     0xdd,0x53,0xe2,0x34,0x87,0xda,0x03,0xfd,0x02,0x39,0x63,0x06,0xd2,0x48,0xcd,0xa0,
     0xe9,0x9f,0x33,0x42,0x0f,0x57,0x7e,0xe8,0xce,0x54,0xb6,0x70,0x80,0x28,0x0d,0x1e,
     0xc6,0x98,0x21,0xbc,0xb6,0xa8,0x83,0x93,0x96,0xf9,0x65,0xab,0x6f,0xf7,0x2a,0x70 }}
};
static const unsigned char md5[] = {
  0x79,0x05,0x40,0x25,0x25,0x5f,0xb1,0xa2,0x6e,0x4b,0xc4,0x22,0xae,0xf5,0x4e,0xb4
};

static int cmp_si_ih (const void *va, const void *vb)
{
  const dds_sample_info_t *a = va;
  const dds_sample_info_t *b = vb;
  return (a->instance_handle == b->instance_handle) ? 0 : (a->instance_handle < b->instance_handle) ? -1 : 1;
}

CU_Test (ddsc_instance_handle, md5)
{
#define N (sizeof (md5xs) / sizeof (md5xs[0]))
  char topicname[100];
  dds_qos_t *qos;
  dds_return_t rc;

  dp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);

  /* not strictly necessary to explicitly set KEEP_LAST (it is the default), nor to make
     it reliable (it is only used inside a process without any limits that might cause it
     to drop samples) */
  qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 1);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  create_unique_topic_name ("instance_handle", topicname, sizeof (topicname));
  tp[0] = dds_create_topic (dp, &InstanceHandleTypes_MD5_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL (tp[0] > 0);
  dds_delete_qos (qos);
  wr[0] = dds_create_writer (dp, tp[0], NULL, NULL);
  CU_ASSERT_FATAL (wr[0] > 0);
  rd[0] = dds_create_reader (dp, tp[0], NULL, NULL);
  CU_ASSERT_FATAL (rd[0] > 0);

  for (size_t i = 0; i < N; i++)
  {
    rc = dds_write (wr[0], &md5xs[i]);
    CU_ASSERT_FATAL (rc == 0);
  }

  void *xs[N] = { NULL };
  dds_sample_info_t si[N];
  int32_t n = dds_read (rd[0], xs, si, N, N);
  CU_ASSERT_FATAL (n == (int32_t) N);
  for (int i = 0; i < n; i++)
    CU_ASSERT (memcmp (xs[i], &md5xs[i], sizeof (md5xs[i])) == 0);
  qsort (si, (size_t) n, sizeof (*si), cmp_si_ih);
  for (int i = 1; i < n; i++)
    CU_ASSERT (si[i].instance_handle != si[i-1].instance_handle);
  rc = dds_return_loan (rd[0], xs, n);
  CU_ASSERT_FATAL (rc == 0);

  struct ddsi_serdata *sds[N];
  n = dds_takecdr (rd[0], sds, N, si, DDS_ANY_STATE);
  CU_ASSERT_FATAL (n == (int32_t) N);
  for (int i = 0; i < n; i++)
  {
    ddsi_keyhash_t kh;
    ddsi_serdata_get_keyhash (sds[i], &kh, false);
    CU_ASSERT (memcmp (md5, kh.value, sizeof (md5)) == 0);
    ddsi_serdata_unref (sds[i]);
  }

  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
#undef N
}
