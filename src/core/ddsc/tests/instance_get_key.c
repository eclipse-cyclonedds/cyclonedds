#include <assert.h>
#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "ddsc/dds.h"
#include "RoundTrip.h"

#include "os/os.h"


static dds_entity_t entity ;

static dds_entity_t participant = 0;
static dds_entity_t topic = 0;
static dds_entity_t publisher = 0;
static dds_entity_t writer = 0;


static dds_instance_handle_t handle= 0;

static const uint32_t payloadSize = 32;
static RoundTripModule_DataType sampleData;

static RoundTripModule_Address data;

/* Fixture to create prerequisite entity */
void initialize_test(void)
{
    entity = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(entity, 0, "create_entity fixture failed");

    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
	cr_assert_gt(participant, 0);
	topic = dds_create_topic(participant, &RoundTripModule_Address_desc, "RoundTripAddress", NULL, NULL);
	cr_assert_gt(topic, 0);

	publisher = dds_create_publisher(participant, NULL, NULL);
	cr_assert_gt(publisher, 0);

	writer = dds_create_writer(publisher, topic, NULL, NULL);
	cr_assert_gt(writer, 0);

	memset(&sampleData, 0, sizeof(sampleData));
	sampleData.payload._length = payloadSize;
	sampleData.payload._buffer = dds_alloc (payloadSize);
	memset(sampleData.payload._buffer, 'a', payloadSize);
	sampleData.payload._release = true;
	sampleData.payload._maximum = 0;

	memset(&data, 0, sizeof(data));
	data.ip = strdup("some data");
	data.port= 1;

	printf("The size of the payload is %d\n", sampleData.payload._length);
	fflush(stdout);
}


/* Fixture to delete prerequisite entity */
void finalize_test(void)
{
    RoundTripModule_DataType_free (&sampleData, DDS_FREE_CONTENTS);
	memset(&sampleData, 0, sizeof(sampleData));

	RoundTripModule_DataType_free (&data, DDS_FREE_CONTENTS);
	memset(&data, 0, sizeof(data));

	cr_assert_gt(entity, 0, "entity not created pre delete_entity fixture");
	dds_return_t ret = dds_delete(entity);
	cr_assert_eq(ret, DDS_RETCODE_OK, "delete_entity fixture failed (ret: %d)", dds_err_nr(ret));
	entity = -1;

    dds_delete(writer);
	dds_delete(publisher);
	dds_delete(topic);
	dds_delete(participant);
}

Test(ddsc_instance_get_key, WillReturnBadParameterErrorWhenDataIsNull, .init=initialize_test, .fini=finalize_test) {
	int result;
	result= dds_instance_get_key(entity, handle, NULL);

	cr_assert_eq(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(result));
}

Test(ddsc_instance_get_key, WillReturnWhenBadParameterErrorEntityIsNotWriterOrReader, .init=initialize_test, .fini=finalize_test) {
	int result;

	result= dds_instance_get_key(entity, handle, &sampleData);

	cr_assert_eq(dds_err_nr(result), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(result));
	//cr_assert_neq(result, DDS_RETCODE_OK);
}


Test(ddsc_instance_get_key, WillReturnWhenEntityIsWriterOrReader, .init=initialize_test, .fini=finalize_test) {

	dds_return_t ret = dds_register_instance(writer, &handle, &data);
	cr_assert_eq(ret, DDS_RETCODE_OK, "dds_register_instance failed (ret: %d)", dds_err_nr(ret));
	int result= dds_instance_get_key(writer, handle, &data);

	cr_assert_eq(dds_err_nr(result), DDS_RETCODE_OK);
}

