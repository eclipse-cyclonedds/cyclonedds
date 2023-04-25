// Copyright(c) 2019 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "ddsi__security_msg.h"
#include "mem_ser.h"
#include <assert.h>

static ddsi_participant_generic_message_t test_msg_in =
{
  .message_identity             = { {{.u={1,2,3}},{4}}, 5 },
  .related_message_identity     = { {{.u={5,4,3}},{2}}, 1 },
  .destination_participant_guid = {  {.u={2,3,4}},{5}     },
  .destination_endpoint_guid    = {  {.u={3,4,5}},{6}     },
  .source_endpoint_guid         = {  {.u={4,5,6}},{7}     },
  .message_class_id             = "testing message",
  .message_data                 = {
     .n = 4,
     .tags = (ddsi_dataholder_t[]) {
       {
         .class_id = "holder0",
         .properties = {
             .n = 3,
             .props = (dds_property_t[]) {
                 {
                   .propagate = false,
                   .name  = "holder0::prop0name",
                   .value = "holder0::prop0value",
                 },
                 {
                   .propagate = true,
                   .name  = "holder0::prop1name",
                   .value = "holder0::prop1value",
                 },
                 {
                   .propagate = false,
                   .name  = "holder0::prop2name",
                   .value = "holder0::prop2value",
                 },
             }
         },
         .binary_properties = {
             .n = 1,
             .props = (dds_binaryproperty_t[]) {
                 {
                   .propagate = false,
                   .name  = "holder0::bprop0name",
                   .value = { 2, (unsigned char[]){ 1, 2 } },
                 },
             }
         },
       },
       {
         .class_id = "holder1",
         .properties = {
             .n = 1,
             .props = (dds_property_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder1::prop0name",
                   .value = "holder1::prop0value",
                 },
             }
         },
         .binary_properties = {
             .n = 1,
             .props = (dds_binaryproperty_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder1::bprop0name",
                   .value = { 3, (unsigned char[]){ 1, 2, 3 } },
                 },
             }
         },
       },
       {
         .class_id = "holder2",
         .properties = {
             .n = 1,
             .props = (dds_property_t[]) {
                 {
                   .propagate = false,
                   .name  = "holder2::prop0name",
                   .value = "holder2::prop0value",
                 },
             }
         },
         .binary_properties = {
             .n = 3,
             .props = (dds_binaryproperty_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder2::bprop0name",
                   .value = { 3, (unsigned char[]){ 1, 2, 3 } },
                 },
                 {
                   .propagate = false,
                   .name  = "holder2::bprop1name",
                   .value = { 4, (unsigned char[]){ 1, 2, 3, 4 } },
                 },
                 {
                   .propagate = true,
                   .name  = "holder2::bprop2name",
                   .value = { 5, (unsigned char[]){ 1, 2, 3, 4, 5 } },
                 },
             }
         },
       },
       {
         .class_id = "holder3",
         .properties = {
             .n = 1,
             .props = (dds_property_t[]) {
                 {
                   .propagate = false,
                   .name  = "holder3::prop0name",
                   .value = "holder3::prop0value",
                 },
             }
         },
         .binary_properties = {
             .n = 1,
             .props = (dds_binaryproperty_t[]) {
                 {
                   .propagate = false,
                   .name  = "holder3::bprop0name",
                   .value = { 3, (unsigned char[]){ 1, 2, 3 } },
                 },
             }
         },
       },
     },
  },
};

/* Same as test_msg_in, excluding the non-propagated properties. */
static ddsi_participant_generic_message_t test_msg_out =
{
  .message_identity             = { {{.u={1,2,3}},{4}}, 5 },
  .related_message_identity     = { {{.u={5,4,3}},{2}}, 1 },
  .destination_participant_guid = {  {.u={2,3,4}},{5}     },
  .destination_endpoint_guid    = {  {.u={3,4,5}},{6}     },
  .source_endpoint_guid         = {  {.u={4,5,6}},{7}     },
  .message_class_id             = "testing message",
  .message_data                 = {
     .n = 4,
     .tags = (ddsi_dataholder_t[]) {
       {
         .class_id = "holder0",
         .properties = {
             .n = 1,
             .props = (dds_property_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder0::prop1name",
                   .value = "holder0::prop1value",
                 },
             }
         },
         .binary_properties = {
             .n = 0,
             .props = NULL
         },
       },
       {
         .class_id = "holder1",
         .properties = {
             .n = 1,
             .props = (dds_property_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder1::prop0name",
                   .value = "holder1::prop0value",
                 },
             }
         },
         .binary_properties = {
             .n = 1,
             .props = (dds_binaryproperty_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder1::bprop0name",
                   .value = { 3, (unsigned char[]){ 1, 2, 3 } },
                 },
             }
         },
       },
       {
         .class_id = "holder2",
         .properties = {
             .n = 0,
             .props = NULL,
         },
         .binary_properties = {
             .n = 2,
             .props = (dds_binaryproperty_t[]) {
                 {
                   .propagate = true,
                   .name  = "holder2::bprop0name",
                   .value = { 3, (unsigned char[]){ 1, 2, 3 } },
                 },
                 {
                   .propagate = true,
                   .name  = "holder2::bprop2name",
                   .value = { 5, (unsigned char[]){ 1, 2, 3, 4, 5 } },
                 },
             }
         },
       },
       {
         .class_id = "holder3",
         .properties = {
             .n = 0,
             .props = NULL,
         },
         .binary_properties = {
             .n = 0,
             .props = NULL,
         },
       },
     },
  },
};

/* The cdr of test_msg_out. */
static unsigned char test_msg_ser[] = {
  SER32BE(1), SER32BE(2), SER32BE(3), SER32BE(4), SER64(5),
  SER32BE(5), SER32BE(4), SER32BE(3), SER32BE(2), SER64(1),
  SER32BE(2), SER32BE(3), SER32BE(4), SER32BE(5),
  SER32BE(3), SER32BE(4), SER32BE(5), SER32BE(6),
  SER32BE(4), SER32BE(5), SER32BE(6), SER32BE(7),
  SER32(16), 't','e','s','t','i','n','g',' ','m','e','s','s','a','g','e',0,
    SER32(4),
    /* dataholder 0 */
    SER32(8), 'h','o','l','d','e','r','0',0,
    SER32(1),
      SER32(19), 'h','o','l','d','e','r','0',':',':','p','r','o','p','1','n','a','m','e',0,/* pad */0,
      SER32(20), 'h','o','l','d','e','r','0',':',':','p','r','o','p','1','v','a','l','u','e',0,
    SER32(0),
    /* dataholder 1 */
    SER32(8), 'h','o','l','d','e','r','1',0,
    SER32(1),
      SER32(19), 'h','o','l','d','e','r','1',':',':','p','r','o','p','0','n','a','m','e',0,/* pad */0,
      SER32(20), 'h','o','l','d','e','r','1',':',':','p','r','o','p','0','v','a','l','u','e',0,
    SER32(1),
      SER32(20), 'h','o','l','d','e','r','1',':',':','b','p','r','o','p','0','n','a','m','e',0,
      SER32(3), 1,2,3,  /* pad */0,
    /* dataholder 2 */
    SER32(8), 'h','o','l','d','e','r','2',0,
    SER32(0),
    SER32(2),
      SER32(20), 'h','o','l','d','e','r','2',':',':','b','p','r','o','p','0','n','a','m','e',0,
      SER32(3), 1,2,3,  /* pad */0,
      SER32(20), 'h','o','l','d','e','r','2',':',':','b','p','r','o','p','2','n','a','m','e',0,
      SER32(5), 1,2,3,4,5,  /* pad */0,0,0,
    /* dataholder 2 */
    SER32(8), 'h','o','l','d','e','r','3',0,
    SER32(0),
    SER32(0)
};

CU_Test (ddsi_security_msg, serializer)
{
  ddsi_participant_generic_message_t msg_in;
  ddsi_participant_generic_message_t msg_ser;
  unsigned char *data = NULL;
  dds_return_t ret;
  size_t len;
  bool equal;

  /* Create the message (normally with various arguments). */
  ddsi_participant_generic_message_init(
              &msg_in,
              &test_msg_in.message_identity.source_guid,
               test_msg_in.message_identity.sequence_number,
              &test_msg_in.destination_participant_guid,
              &test_msg_in.destination_endpoint_guid,
              &test_msg_in.source_endpoint_guid,
               test_msg_in.message_class_id,
              &test_msg_in.message_data,
              &test_msg_in.related_message_identity);

  /* Check creation result. */
  equal = ddsi_plist_equal_generic (&msg_in, &test_msg_in, ddsi_pserop_participant_generic_message);
  CU_ASSERT_FATAL(equal == true);

  /* Serialize the message. */
  ret = ddsi_participant_generic_message_serialize(&msg_in, &data, &len);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(data);
  CU_ASSERT(len > 0);

  /* Check serialization result. */
  size_t cmpsize = (len < sizeof(test_msg_ser)) ? len : sizeof(test_msg_ser);
  assert(data != NULL); /* for Clang static analyzer */
  if (memcmp (data, test_msg_ser, cmpsize) != 0)
  {
    printf ("memcmp(%d)\n", (int)cmpsize);
    for (size_t k = 0; k < cmpsize; k++)
      printf ("  %3zu  %02x  %02x (%c) %s\n", k, data[k], test_msg_ser[k],
              ((test_msg_ser[k] >= '0') && (test_msg_ser[k] <= 'z')) ? test_msg_ser[k] : ' ',
              (data[k] == test_msg_ser[k]) ? "" : "<--");
    CU_ASSERT (!(bool)"memcmp");
  }
  CU_ASSERT_FATAL (len == sizeof(test_msg_ser));

  /* Deserialize the message. */
  ret = ddsi_participant_generic_message_deseralize(&msg_ser, data, len, false);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);

  /* Check deserialization result. */
  equal = ddsi_plist_equal_generic (&msg_ser, &test_msg_out, ddsi_pserop_participant_generic_message);
  CU_ASSERT_FATAL(equal == true);

  /* Cleanup. */
  ddsi_participant_generic_message_deinit(&msg_in);
  ddsi_participant_generic_message_deinit(&msg_ser);
  ddsrt_free(data);
}
