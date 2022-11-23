#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds__entity.h"
#include "dds__topic.h"
#include "dds__serdata_default.h"

#include "test_common.h"

#include "DynamicData.h"

static const struct shm_locator {
  unsigned char a[16];
} shm_locators[] = {{{1}}, {{1}}, {{2}}, {{2}}};

#define TRACE_CATEGORY "discovery"

static dds_entity_t create_participant(dds_domainid_t int_dom,
                                       bool shm_enable) {
  const unsigned char *l = shm_locators[int_dom].a;
  char *configstr;
  ddsrt_asprintf(&configstr, "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
  <AllowMulticast>spdp</AllowMulticast>\
</General>\
<Discovery>\
  <ExternalDomainId>0</ExternalDomainId>\
</Discovery>\
<SharedMemory>\
  <Enable>%s</Enable>\
  <Locator>%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x</Locator>\
  <Prefix>DDS_CYCLONE_%d</Prefix>\
</SharedMemory>\
<Tracing>\
  <Category>" TRACE_CATEGORY "</Category>\
  <OutputFile>cdds.log.%d</OutputFile>\
</Tracing>\
",
                 shm_enable ? "true" : "false", l[0], l[1], l[2], l[3], l[4],
                 l[5], l[6], l[7], l[8], l[9], l[10], l[11], l[12], l[13],
                 l[14], l[15], (int)l[0], (int)int_dom);
  char *xconfigstr = ddsrt_expand_envvars(configstr, int_dom);
  const dds_entity_t dom = dds_create_domain(int_dom, xconfigstr);
  CU_ASSERT_FATAL(dom > 0);
  ddsrt_free(xconfigstr);
  ddsrt_free(configstr);
  const dds_entity_t p = dds_create_participant(int_dom, NULL, NULL);
  CU_ASSERT_FATAL(p > 0);
  return p;
}

CU_Test(ddsc_shm_serialization, get_serialized_size) {
  dds_entity_t participant;
  dds_entity_t topic;

  participant = create_participant(0, true);
  CU_ASSERT_FATAL(participant > 0);

  topic = dds_create_topic(participant, &DynamicData_Msg_desc, "DynamicData_Msg",
                           NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);

  dds_topic *tp;
  dds_return_t rc = dds_topic_pin(topic, &tp);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  struct ddsi_sertype *stype = tp->m_stype;

  DynamicData_Msg sample;
  sample.message = "test message";
  sample.scalar = 73;
  int32_t values[] = {11, 13, 17, 19};
  sample.values._buffer = (int32_t*)(&values);
  sample.values._length = 4;
  sample.values._maximum = 5;

  size_t required_size = ddsi_sertype_get_serialized_size(stype, &sample);

  struct ddsi_serdata *serdata = ddsi_serdata_from_sample(stype, SDK_DATA, &sample);
  size_t serialized_size = ddsi_serdata_size(serdata) - sizeof(struct dds_cdr_header);
  ddsi_serdata_unref(serdata);

  printf("required size %zu \n", required_size);
  printf("actual_serialized_size %zu \n", serialized_size);
  // serialized size is always a multiple of 4 (padding is added to ensure this)
  CU_ASSERT(required_size == serialized_size);
  CU_ASSERT(required_size % 4 == 0);

  dds_topic_unpin(tp);

  dds_delete(participant);
  rc = dds_delete(DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL(rc == 0);
}

// TODO: remove or keep? its is useful for checking the buffer contents in case something goes wrong
//       output should be suppressed for successful tests
static void printbuffer(void* buffer, size_t n) {
  char* buf = (char*) buffer;
  for (size_t i = 0; i < n; i++) {
    printf("%02x ", buf[i] & 0xff);
  }
  printf("\n");
}

CU_Test(ddsc_shm_serialization, serialize_into) {
  dds_entity_t participant;
  dds_entity_t topic;

  participant = create_participant(0, true);
  CU_ASSERT_FATAL(participant > 0);

  topic = dds_create_topic(participant, &DynamicData_Msg_desc, "DynamicData_Msg",
                           NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);

  dds_topic *tp;
  dds_return_t rc = dds_topic_pin(topic, &tp);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  struct ddsi_sertype *stype = tp->m_stype;

  DynamicData_Msg sample;
  sample.message = "test message";
  sample.scalar = 73;
  int32_t values[] = {11, 13, 17, 19};
  sample.values._buffer = (int32_t*)(&values);
  sample.values._length = 4;
  sample.values._maximum = 5;

  size_t buffer_size = ddsi_sertype_get_serialized_size(stype, &sample);
  char *buffer = (char *)dds_alloc(buffer_size);
  memset(buffer, 0, buffer_size);

  ddsi_sertype_serialize_into(stype, &sample, buffer, buffer_size);

  struct ddsi_serdata *serdata = ddsi_serdata_from_sample(stype, SDK_DATA, &sample);
  size_t serialized_size = ddsi_serdata_size(serdata) - sizeof(struct dds_cdr_header);

  struct dds_serdata_default *d = (struct dds_serdata_default*) serdata;
  CU_ASSERT(buffer_size >= serialized_size);

  CU_ASSERT(memcmp(d->data, buffer, serialized_size) == 0);

  printf("buffer  ");
  printbuffer(buffer, serialized_size);
  printf("serdata ");
  printbuffer(d->data, serialized_size);

  ddsi_serdata_unref(serdata);
  dds_free(buffer);
  dds_topic_unpin(tp);
  dds_delete(participant);
  rc = dds_delete(DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL(rc == 0);
}

static bool compare_messages(const DynamicData_Msg* s1, const DynamicData_Msg* s2) {
  size_t l = strlen(s1->message);
  if(strlen(s2->message) != l)
    return false;

  bool result = strncmp(s1->message, s2->message, l) == 0;
  result &= s1->scalar == s2->scalar;

  uint32_t n = s1->values._length;
  if(n != s2->values._length)
    return false;

  // no memcmp but logical comparison
  for(uint32_t i=0; i<n; ++i) {
    result &= s1->values._buffer[i] == s2->values._buffer[i];
  }
  return result;
}

CU_Test(ddsc_shm_serialization, transmit_dynamic_type, .timeout = 30) {
  dds_entity_t participant;
  dds_entity_t topic;
  dds_entity_t writer;
  dds_entity_t reader;

  participant = create_participant(0, true);
  CU_ASSERT_FATAL(participant > 0);

  topic = dds_create_topic(participant, &DynamicData_Msg_desc, "DynamicData_Msg",
                           NULL, NULL);
  CU_ASSERT_FATAL(topic > 0);

  dds_qos_t *qos = dds_create_qos();
  CU_ASSERT_FATAL(qos != NULL);

  dds_qset_durability(qos, DDS_DURABILITY_VOLATILE);
  reader = dds_create_reader(participant, topic, qos, NULL);
  CU_ASSERT_FATAL(reader > 0);

  writer = dds_create_writer(participant, topic, qos, NULL);
  CU_ASSERT_FATAL(writer > 0);

  DynamicData_Msg sample;
  sample.message = "test message";
  sample.scalar = 73;
  int32_t values[] = {11, 13, 17, 19};
  sample.values._buffer = (int32_t*)(&values);
  sample.values._length = 4;
  sample.values._maximum = 5;

  dds_return_t rc = dds_set_status_mask(writer, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);

  uint32_t status = 0;
  while (!(status & DDS_PUBLICATION_MATCHED_STATUS)) {
    rc = dds_get_status_changes(writer, &status);
    CU_ASSERT_FATAL(rc == DDS_RETCODE_OK);
    dds_sleepfor(DDS_MSECS(100));
  }

  CU_ASSERT(dds_write(writer, &sample) == DDS_RETCODE_OK);

  void *samples[1];
  samples[0] = NULL;
  dds_sample_info_t infos[1];

  int received = 0;
  while (true) {
    rc = dds_read(reader, samples, infos, 1, 1);
    CU_ASSERT_FATAL(rc >= 0);
    if (rc > 0) {
      received = rc;
      break;
    }
    dds_sleepfor(DDS_MSECS(10));
  }

  if (received != 1 || !infos[0].valid_data) {
    printf("Failure - nothing received\n");
    goto fail;
  }

  DynamicData_Msg *received_sample = (DynamicData_Msg *)samples[0];

  CU_ASSERT(compare_messages(&sample, received_sample));

  dds_delete_qos(qos);
  dds_delete(participant);
  rc = dds_delete(DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_FATAL(rc == 0);
  return;
fail:
  dds_delete_qos(qos);
  dds_delete(participant);
  dds_delete(DDS_CYCLONEDDS_HANDLE);
  CU_FAIL();
}
