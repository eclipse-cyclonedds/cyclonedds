#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <criterion/criterion.h>
#include <criterion/logging.h>

#include "os/os.h"
#include "ddsc/dds.h"
#include "RoundTrip.h"

static dds_entity_t participant = DDS_ENTITY_NIL;
static dds_entity_t topic = DDS_ENTITY_NIL;
static dds_entity_t publisher = DDS_ENTITY_NIL;
static dds_entity_t writer = DDS_ENTITY_NIL;

static dds_instance_handle_t handle = DDS_HANDLE_NIL;

static RoundTripModule_Address data;

/* Fixture to create prerequisite entity */
static void setup(void)
{
    participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0);
    topic = dds_create_topic(participant, &RoundTripModule_Address_desc, "ddsc_instance_get_key", NULL, NULL);
    cr_assert_gt(topic, 0);

    publisher = dds_create_publisher(participant, NULL, NULL);
    cr_assert_gt(publisher, 0);

    writer = dds_create_writer(publisher, topic, NULL, NULL);
    cr_assert_gt(writer, 0);

    memset(&data, 0, sizeof(data));
    data.ip = os_strdup("some data");
    cr_assert_not_null(data.ip);
    data.port = 1;
}

/* Fixture to delete prerequisite entity */
static void teardown(void)
{
    RoundTripModule_Address_free(&data, DDS_FREE_CONTENTS);

    dds_delete(writer);
    dds_delete(publisher);
    dds_delete(topic);
    dds_delete(participant);
}

Test(ddsc_instance_get_key, bad_entity, .init=setup, .fini=teardown)
{
    dds_return_t ret;

    ret = dds_instance_get_key(participant, handle, &data);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "returned %d", dds_err_nr(ret));
}

Test(ddsc_instance_get_key, null_data, .init=setup, .fini=teardown)
{
    dds_return_t ret;

    ret = dds_register_instance(writer, &handle, NULL);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "Argument data is NULL");
}

Test(ddsc_instance_get_key, null_handle, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    ret = dds_register_instance(writer, &handle, &data);
    cr_assert_eq(ret, DDS_RETCODE_OK, "dds_register_instance succeeded (ret: %d)", dds_err_nr(ret));

    ret = dds_instance_get_key(writer, DDS_HANDLE_NIL, &data);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_BAD_PARAMETER, "Argument data is not null, but handle is null");
}

Test(ddsc_instance_get_key, registered_instance, .init=setup, .fini=teardown)
{
    dds_return_t ret;
    RoundTripModule_Address key_data;

    ret = dds_register_instance(writer, &handle, &data);
    cr_assert_eq(ret, DDS_RETCODE_OK, "dds_register_instance succeeded (ret: %d)", dds_err_nr(ret));

    memset(&key_data, 0, sizeof(key_data));

    ret = dds_instance_get_key(writer, handle, &key_data);

    cr_assert_not_null(key_data.ip);
    cr_assert_eq(strcmp(key_data.ip, data.ip) , 0);
    cr_assert_eq(key_data.port, data.port);
    cr_assert_eq(dds_err_nr(ret), DDS_RETCODE_OK);

    RoundTripModule_Address_free(&key_data, DDS_FREE_CONTENTS);
}

