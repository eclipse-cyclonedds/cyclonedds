// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "CUnit/Test.h"

/* We are deliberately testing some bad arguments that SAL will complain about.
 * So, silence SAL regarding these issues. */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 6387 28020)
#endif

/* Add --verbose command line argument to get the cr_log_info traces (if there are any). */

static dds_entity_t entity = -1;

/* Fixture to create prerequisite entity */
static void create_entity(void)
{
    CU_ASSERT_EQUAL_FATAL(entity, -1);
    entity = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(entity > 0);
}

/* Fixture to delete prerequisite entity */
static void delete_entity(void)
{
    CU_ASSERT_FATAL(entity > 0);
    dds_return_t ret = dds_delete(entity);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    entity = -1;
}

CU_Test(ddsc_entity, create, .fini = delete_entity)
{
    /* Use participant as entity in the tests. */
    entity = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(entity > 0 );
}

CU_Test(ddsc_entity, enable, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;

    /* Check enabling with bad parameters. */
    status = dds_enable(0);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Check actual enabling. */
    /* TODO: CHAM-96: Check enabling.
    status = dds_enable(&entity);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK, "dds_enable (delayed enable)");
    */

    /* Check re-enabling (should be a noop). */
    status = dds_enable(entity);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
}

static void entity_qos_get_set(dds_entity_t e, const char* info)
{
    dds_return_t status;
    dds_qos_t *qos = dds_create_qos();

    (void)info;

    /* Get QoS. */
    status = dds_get_qos (e, qos);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

    status = dds_set_qos (e, qos); /* Doesn't change anything, so no need to forbid. But we return NOT_SUPPORTED anyway for now*/
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);

    dds_delete_qos(qos);
}

CU_Test(ddsc_entity, qos, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;
    dds_qos_t *qos = dds_create_qos();

    /* Don't check inconsistent and immutable policies. That's a job
     * for the specific entity children, not for the generic part. */

    /* Check getting QoS with bad parameters. */
    status = dds_get_qos (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_qos (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_qos (0, qos);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Check setting QoS with bad parameters. */
    status = dds_set_qos (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_set_qos (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_set_qos (0, qos);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Check set/get with entity without initial qos. */
    entity_qos_get_set(entity, "{without initial qos}");

    /* Check set/get with entity with initial qos. */
    {
        dds_entity_t par = dds_create_participant (DDS_DOMAIN_DEFAULT, qos, NULL);
        entity_qos_get_set(par, "{with initial qos}");
        dds_delete(par);
    }

    /* Delete qos. */
    dds_delete_qos(qos);
}

CU_Test(ddsc_entity, listener, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;
    dds_listener_t *l1 = dds_create_listener(NULL);
    dds_listener_t *l2 = dds_create_listener(NULL);
    void *cb1;
    void *cb2;

    /* Don't check actual workings of the listeners. That's a job
     * for the specific entity children, not for the generic part. */

    /* Set some random values for the l2 listener callbacks.
     * I know for sure that these will not be called within this test.
     * Otherwise, the following would let everything crash.
     * We just set them to know for sure that we got what we set. */
    dds_lset_liveliness_changed(l2,         (dds_on_liveliness_changed_fn)          1234);
    dds_lset_requested_deadline_missed(l2,  (dds_on_requested_deadline_missed_fn)   5678);
    dds_lset_requested_incompatible_qos(l2, (dds_on_requested_incompatible_qos_fn)  8765);
    dds_lset_publication_matched(l2,        (dds_on_publication_matched_fn)         4321);

    /* Check getting Listener with bad parameters. */
    status = dds_get_listener (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_listener (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_listener (0, l1);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Get Listener, which should be unset. */
    status = dds_get_listener (entity, l1);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    dds_lget_liveliness_changed (l1, (dds_on_liveliness_changed_fn*)&cb1);
    CU_ASSERT_EQUAL_FATAL(cb1, DDS_LUNSET);
    dds_lget_requested_deadline_missed (l1, (dds_on_requested_deadline_missed_fn*)&cb1);
    CU_ASSERT_EQUAL_FATAL(cb1, DDS_LUNSET);
    dds_lget_requested_incompatible_qos (l1, (dds_on_requested_incompatible_qos_fn*)&cb1);
    CU_ASSERT_EQUAL_FATAL(cb1, DDS_LUNSET);
    dds_lget_publication_matched (l1, (dds_on_publication_matched_fn*)&cb1);
    CU_ASSERT_EQUAL_FATAL(cb1, DDS_LUNSET);

    /* Check setting Listener with bad parameters. */
    status = dds_set_listener (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_set_listener (0, l2);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Getting after setting should return set listener. */
    status = dds_set_listener (entity, l2);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    status = dds_get_listener (entity, l1);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    dds_lget_liveliness_changed (l1, (dds_on_liveliness_changed_fn*)&cb1);
    dds_lget_liveliness_changed (l2, (dds_on_liveliness_changed_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb1, cb2);
    dds_lget_requested_deadline_missed (l1, (dds_on_requested_deadline_missed_fn*)&cb1);
    dds_lget_requested_deadline_missed (l2, (dds_on_requested_deadline_missed_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb1, cb2);
    dds_lget_requested_incompatible_qos (l1, (dds_on_requested_incompatible_qos_fn*)&cb1);
    dds_lget_requested_incompatible_qos (l2, (dds_on_requested_incompatible_qos_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb1, cb2);
    dds_lget_publication_matched (l1, (dds_on_publication_matched_fn*)&cb1);
    dds_lget_publication_matched (l2, (dds_on_publication_matched_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb1, cb2);

    /* Reset listener. */
    status = dds_set_listener (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    status = dds_get_listener (entity, l2);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    dds_lget_liveliness_changed (l2, (dds_on_liveliness_changed_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb2, DDS_LUNSET);
    dds_lget_requested_deadline_missed (l2, (dds_on_requested_deadline_missed_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb2, DDS_LUNSET);
    dds_lget_requested_incompatible_qos (l2, (dds_on_requested_incompatible_qos_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb2, DDS_LUNSET);
    dds_lget_publication_matched (l2, (dds_on_publication_matched_fn*)&cb2);
    CU_ASSERT_EQUAL_FATAL(cb2, DDS_LUNSET);

    dds_free(l2);
    dds_free(l1);
}

CU_Test(ddsc_entity, status, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status1;
    uint32_t s1 = 0;

    /* Don't check actual bad statuses. That's a job
     * for the specific entity children, not for the generic part. */

    /* Check getting Status with bad parameters. */
    status1 = dds_get_status_mask (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_get_status_mask (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_get_status_mask (0, &s1);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);

    /* Get Status, which should be 0 for a participant. */
    status1 = dds_get_status_mask (entity, &s1);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL_FATAL(s1, 0);

    /* Check setting Status with bad parameters. */
    status1 = dds_set_status_mask (0, 0);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);

    /* I shouldn't be able to set statuses on a participant. */
    status1 = dds_set_status_mask (entity, 0);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);
    status1 = dds_set_status_mask (entity, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);

    /* Check getting Status changes with bad parameters. */
    status1 = dds_get_status_changes (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_get_status_changes (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_get_status_changes (0, &s1);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_get_status_changes (entity, &s1);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);

    /* Status read and take shouldn't work either. */
    status1 = dds_read_status (0, &s1, 0);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_read_status (entity, &s1, 0);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);
    status1 = dds_take_status (0, &s1, 0);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_BAD_PARAMETER);
    status1 = dds_take_status (entity, &s1, 0);
    CU_ASSERT_EQUAL_FATAL(status1, DDS_RETCODE_OK);
}

CU_Test(ddsc_entity, guid, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;
    dds_guid_t guid, zero;
    memset(&zero, 0, sizeof(zero));

    /* Don't check actual handle contents. That's a job
     * for the specific entity children, not for the generic part. */

    /* Check getting Handle with bad parameters. */
    status = dds_get_guid (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_guid (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_guid (0, &guid);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Get Instance Handle, which should not be 0 for a participant. */
    status = dds_get_guid (entity, &guid);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    CU_ASSERT_FATAL(memcmp(&guid, &zero, sizeof(guid)) != 0);
}

CU_Test(ddsc_entity, instance_handle, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;
    dds_instance_handle_t hdl;

    /* Don't check actual handle contents. That's a job
     * for the specific entity children, not for the generic part. */

    /* Check getting Handle with bad parameters. */
    status = dds_get_instance_handle (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_instance_handle (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_instance_handle (0, &hdl);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Get Instance Handle, which should not be 0 for a participant. */
    status = dds_get_instance_handle (entity, &hdl);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    CU_ASSERT_NOT_EQUAL_FATAL(hdl, 0);
}

CU_Test(ddsc_entity, get_entities, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;
    dds_entity_t par;
    dds_entity_t child;

    /* ---------- Get Parent ------------ */

    /* Check getting Parent with bad parameters. */
    par = dds_get_parent (0);
    CU_ASSERT_EQUAL_FATAL(par, DDS_RETCODE_BAD_PARAMETER);

    /* Get Parent, a participant always has a parent (the domain). */
    par = dds_get_parent (entity);
    CU_ASSERT_NOT_EQUAL_FATAL(par, DDS_HANDLE_NIL);
    /* The domain has a parent: the pseudo-entity for the library */
    par = dds_get_parent (par);
    CU_ASSERT_EQUAL_FATAL(par, DDS_CYCLONEDDS_HANDLE);

    /* ---------- Get Participant ------------ */

    /* Check getting Participant with bad parameters. */
    par = dds_get_participant (0);
    CU_ASSERT_EQUAL_FATAL(par, DDS_RETCODE_BAD_PARAMETER);

    /* Get Participant, a participants' participant is itself. */
    par = dds_get_participant (entity);
    CU_ASSERT_EQUAL_FATAL(par, entity);

    /* ---------- Get Children ------------ */

    /* Check getting Children with bad parameters. */
    status = dds_get_children (0, &child, 1);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_children (entity, NULL, 1);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_children (entity, &child, 0);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_children (0, NULL, 1);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_children (0, &child, 0);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Get Children, of which there are currently none. */
    status = dds_get_children (entity, NULL, 0);
    if (status > 0) {
        CU_ASSERT_FATAL(false);
    } else {
        CU_ASSERT_EQUAL_FATAL(status, 0);
    }
    status = dds_get_children (entity, &child, 1);
    if (status > 0) {
        CU_ASSERT_FATAL(false);
    } else {
        CU_ASSERT_EQUAL_FATAL(status, 0);
    }
}

CU_Test(ddsc_entity, get_domainid, .init = create_entity, .fini = delete_entity)
{
    dds_return_t status;
    dds_domainid_t id = DDS_DOMAIN_DEFAULT;

    /* Check getting ID with bad parameters. */
    status = dds_get_domainid (0, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_domainid (entity, NULL);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);
    status = dds_get_domainid (0, &id);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    /* Get and check the domain id. */
    status = dds_get_domainid (entity, &id);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    CU_ASSERT_FATAL(id != DDS_DOMAIN_DEFAULT);
}

CU_Test(ddsc_entity, delete, .init = create_entity)
{
    dds_return_t status;
    status = dds_delete(0);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_BAD_PARAMETER);

    status = dds_delete(entity);
    CU_ASSERT_EQUAL_FATAL(status, DDS_RETCODE_OK);
    entity = 0;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif
