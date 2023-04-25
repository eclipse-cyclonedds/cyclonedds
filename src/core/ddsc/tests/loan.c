// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include "dds/dds.h"
#include "test_common.h"

static dds_entity_t participant, topic, reader, writer, read_condition, read_condition_unread;

static void create_entities (void)
{
  char topicname[100];
  struct dds_qos *qos;

  create_unique_topic_name ("ddsc_return_loan_test", topicname, sizeof topicname);
  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (participant > 0);

  qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, 0);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 1);
  topic = dds_create_topic (participant, &RoundTripModule_DataType_desc, topicname, qos, NULL);
  CU_ASSERT_FATAL (topic > 0);
  dds_delete_qos (qos);

  writer = dds_create_writer (participant, topic, NULL, NULL);
  CU_ASSERT_FATAL (writer > 0);
  reader = dds_create_reader (participant, topic, NULL, NULL);
  CU_ASSERT_FATAL (reader > 0);
  read_condition = dds_create_readcondition (reader, DDS_ANY_STATE);
  CU_ASSERT_FATAL (read_condition > 0);
  read_condition_unread = dds_create_readcondition (reader, DDS_ANY_INSTANCE_STATE | DDS_ANY_VIEW_STATE | DDS_NOT_READ_SAMPLE_STATE);
  CU_ASSERT_FATAL (read_condition > 0);
}

static void delete_entities (void)
{
  dds_return_t result;
  result = dds_delete (participant);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
}

CU_Test (ddsc_loan, bad_params, .init = create_entities, .fini = delete_entities)
{
  dds_return_t result;

  /* buf = NULL */
  result = dds_return_loan (reader, NULL, -1);
  CU_ASSERT (result == DDS_RETCODE_BAD_PARAMETER);
  result = dds_return_loan (reader, NULL, 0);
  CU_ASSERT (result == DDS_RETCODE_BAD_PARAMETER);
  result = dds_return_loan (reader, NULL, 1);
  CU_ASSERT (result == DDS_RETCODE_BAD_PARAMETER);

  /* buf[0] = NULL, size > 0 */
  void *buf = NULL;
  result = dds_return_loan (reader, &buf, 1);
  CU_ASSERT (result == DDS_RETCODE_BAD_PARAMETER);

  /* not a reader or condition (checking only the ones we have at hand) */
  /* buf[0] != NULL, size <= 0 */
  char dummy = 0;
  buf = &dummy;
  result = dds_return_loan (participant, &buf, 1);
  CU_ASSERT (result == DDS_RETCODE_ILLEGAL_OPERATION);
  result = dds_return_loan (topic, &buf, 1);
  CU_ASSERT (result == DDS_RETCODE_ILLEGAL_OPERATION);
}

CU_Test (ddsc_loan, success, .init = create_entities, .fini = delete_entities)
{
  const RoundTripModule_DataType s = {
    .payload = {
      ._length = 1,
      ._buffer = (uint8_t[]) { 'a' }
    }
  };
  const unsigned char zeros[3 * sizeof (s)] = { 0 };
  dds_return_t result;
  for (size_t i = 0; i < 3; i++)
  {
    result = dds_write (writer, &s);
    CU_ASSERT_FATAL (result == 0);
  }

  /* rely on things like address sanitizer, valgrind for detecting double frees and leaks */
  int32_t n;
  void *ptrs[3] = { NULL };
  void *ptr0copy, *ptr1copy;
  dds_sample_info_t si[3];

  /* read 1, return: this should cause memory to be allocated for 1 sample only */
  n = dds_read (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] != NULL && ptrs[1] == NULL);
  ptr0copy = ptrs[0];
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
  /* return resets buf[0] (so that it picks up the loan the next time) and zeros the data */
  CU_ASSERT_FATAL (ptrs[0] == NULL);
  assert (ptr0copy != NULL); /* clang static analyzer */
  CU_ASSERT_FATAL (memcmp (ptr0copy, zeros, sizeof (s)) == 0);

  /* read 3, return: should work fine, causes realloc */
  n = dds_read (reader, ptrs, si, 3, 3);
  CU_ASSERT_FATAL (n == 3);
  CU_ASSERT_FATAL (ptrs[0] != NULL && ptrs[1] != NULL && ptrs[2] != NULL);
  ptr0copy = ptrs[0];
  ptr1copy = ptrs[1];
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ptrs[0] == NULL);
  assert (ptr0copy != NULL); /* clang static analyzer */
  CU_ASSERT_FATAL (memcmp (ptr0copy, zeros, 3 * sizeof (s)) == 0);

  /* read 1 using loan, expecting to get the same address (no realloc needed), defer return.
     Expect ptrs[1] to remain unchanged, although that probably is really an implementation
     detail rather than something one might want to rely on */
  n = dds_read (read_condition, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == ptr1copy);

  /* read 3, letting read allocate */
  int32_t n2;
  void *ptrs2[3] = { NULL };
  n2 = dds_read (read_condition, ptrs2, si, 3, 3);
  CU_ASSERT_FATAL (n2 == 3);
  CU_ASSERT_FATAL (ptrs2[0] != NULL && ptrs2[1] != NULL && ptrs2[2] != NULL);
  CU_ASSERT_FATAL (ptrs2[0] != ptrs[0]);

  /* contents of first sample should be the same; the point of comparing them
     is that valgrind/address sanitizer will get angry with us if one of them
     has been freed; can't use memcmp because the sequence buffers should be
     at different addresses */
  {
    const struct RoundTripModule_DataType *a = ptrs[0];
    const struct RoundTripModule_DataType *b = ptrs2[0];
    assert (a != NULL && b != NULL); /* clang static analyzer */
    CU_ASSERT_FATAL (a->payload._length == b->payload._length);
    CU_ASSERT_FATAL (a->payload._buffer != b->payload._buffer);
    CU_ASSERT_FATAL (a->payload._buffer[0] == b->payload._buffer[0]);
  }

  /* return loan -- to be freed when we delete the reader */
  result = dds_return_loan (read_condition, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ptrs[0] == NULL);

  /* use "dds_return_loan" to free the second result immediately, there's no
     easy way to check this happens short of using a custom sertype */
  ptr0copy = ptrs2[0];
  result = dds_return_loan (read_condition, ptrs2, n2);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (ptrs2[0] == NULL);

  //This should be a use-after-free
  //CU_ASSERT_FATAL (memcmp (ptr0copy, zeros, sizeof (s)) == 0);
  (void) ptr0copy;
}

CU_Test (ddsc_loan, take_cleanup, .init = create_entities, .fini = delete_entities)
{
  const RoundTripModule_DataType s = {
    .payload = {
      ._length = 1,
      ._buffer = (uint8_t[]) { 'a' }
    }
  };
  dds_return_t result;

  /* rely on things like address sanitizer, valgrind for detecting double frees and leaks */
  int32_t n;
  void *ptrs[3] = { NULL };
  void *ptr0copy;
  dds_sample_info_t si[3];

  /* take 1 from an empty reader: this should cause memory to be allocated for
     1 sample only, be stored as the loan, but not become visisble to the
     application */
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 0);
  CU_ASSERT_FATAL (ptrs[0] == NULL && ptrs[1] == NULL);

  /* take 1 that's present: allocates a loan  */
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] != NULL && ptrs[1] == NULL);
  ptr0copy = ptrs[0];
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);

  /* if it really got handled as a loan, the same address must come out again
     (rely on address sanitizer allocating at a different address each time) */
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == NULL);
  ptr0copy = ptrs[0];
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);

  /* take that fails (for lack of data in this case) must reuse the loan, but
     hand it back and restore the null pointer */
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 0);
  CU_ASSERT_FATAL (ptrs[0] == NULL && ptrs[1] == NULL);

  /* take that succeeds again must therefore still be using the same address */
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == NULL);

  /* take that fails (with the loan still out) must allocate new memory and
     free it */
  int32_t n2;
  void *ptrs2[3] = { NULL };
  n2 = dds_take (reader, ptrs2, si, 1, 1);
  CU_ASSERT_FATAL (n2 == 0);
  CU_ASSERT_FATAL (ptrs2[0] == NULL && ptrs2[1] == NULL);

  /* return the loan and the next take should reuse the memory */
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == NULL);
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
}

CU_Test (ddsc_loan, read_cleanup, .init = create_entities, .fini = delete_entities)
{
  const RoundTripModule_DataType s = {
    .payload = {
      ._length = 1,
      ._buffer = (uint8_t[]) { 'a' }
    }
  };
  dds_return_t result;

  /* rely on things like address sanitizer, valgrind for detecting double frees and leaks */
  int32_t n;
  void *ptrs[3] = { NULL };
  void *ptr0copy;
  dds_sample_info_t si[3];

  /* read 1 from an empty reader: this should cause memory to be allocated for
     1 sample only, be stored as the loan, but not become visisble to the
     application */
  n = dds_read (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 0);
  CU_ASSERT_FATAL (ptrs[0] == NULL && ptrs[1] == NULL);

  /* read 1 that's present: allocates a loan  */
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_take (reader, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] != NULL && ptrs[1] == NULL);
  ptr0copy = ptrs[0];
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);

  /* if it really got handled as a loan, the same address must come out again
     (rely on address sanitizer allocating at a different address each time) */
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_read (read_condition_unread, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == NULL);
  ptr0copy = ptrs[0];
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);

  /* take that fails (for lack of data in this case) must reuse the loan, but
     hand it back and restore the null pointer */
  n = dds_read (read_condition_unread, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 0);
  CU_ASSERT_FATAL (ptrs[0] == NULL && ptrs[1] == NULL);

  /* take that succeeds again must therefore still be using the same address */
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_read (read_condition_unread, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == NULL);

  /* take that fails (with the loan still out) must allocate new memory and
     free it */
  int32_t n2;
  void *ptrs2[3] = { NULL };
  n2 = dds_read (read_condition_unread, ptrs2, si, 1, 1);
  CU_ASSERT_FATAL (n2 == 0);
  CU_ASSERT_FATAL (ptrs2[0] == NULL && ptrs2[1] == NULL);

  /* return the loan and the next take should reuse the memory */
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
  result = dds_write (writer, &s);
  CU_ASSERT_FATAL (result == 0);
  n = dds_read (read_condition_unread, ptrs, si, 1, 1);
  CU_ASSERT_FATAL (n == 1);
  CU_ASSERT_FATAL (ptrs[0] == ptr0copy && ptrs[1] == NULL);
  result = dds_return_loan (reader, ptrs, n);
  CU_ASSERT_FATAL (result == DDS_RETCODE_OK);
}
