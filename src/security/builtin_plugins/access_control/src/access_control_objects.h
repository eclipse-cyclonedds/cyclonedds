// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef ACCESS_CONTROL_OBJECTS_H
#define ACCESS_CONTROL_OBJECTS_H

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/openssl_support.h"
#include "dds/security/core/dds_security_timed_cb.h"

#define ACCESS_CONTROL_OBJECT(o)            ((AccessControlObject *)(o))
#define ACCESS_CONTROL_OBJECT_HANDLE(o)     ((o) ? ACCESS_CONTROL_OBJECT(o)->handle : DDS_SECURITY_HANDLE_NIL)

#define ACCESS_CONTROL_OBJECT_KEEP(o)       access_control_object_keep((AccessControlObject *)(o))
#define ACCESS_CONTROL_OBJECT_RELEASE(o)    access_control_object_release((AccessControlObject *)(o))
#define ACCESS_CONTROL_OBJECT_VALID(o,k)    access_control_object_valid((AccessControlObject *)(o), k)

typedef enum {
    ACCESS_CONTROL_OBJECT_KIND_UNKNOWN,
    ACCESS_CONTROL_OBJECT_KIND_LOCAL_PARTICIPANT,
    ACCESS_CONTROL_OBJECT_KIND_REMOTE_PARTICIPANT,
} AccessControlObjectKind_t;

typedef struct AccessControlObject AccessControlObject;
typedef void (*AccessControlObjectDestructor)(AccessControlObject *obj);

struct AccessControlObject {
    int64_t handle;
    ddsrt_atomic_uint32_t refcount;
    AccessControlObjectKind_t kind;
    AccessControlObjectDestructor destructor;
    dds_time_t permissions_expiry;
    dds_security_time_event_handle_t timer;
};

typedef struct local_participant_access_rights {
    AccessControlObject _parent;
    DDS_Security_ParticipantSecurityAttributes participant_attributes;
    DDS_Security_IdentityHandle local_identity;
    struct governance_parser *governance_tree;
    struct permissions_parser *permissions_tree;
    int domain_id;
    char *identity_subject_name;
    char *permissions_document;
    X509 *permissions_ca;
} local_participant_access_rights;


typedef struct remote_permissions {
    int ref_cnt;
    struct permissions_parser *permissions_tree;
    DDS_Security_string remote_permissions_token_class_id;
} remote_permissions;

typedef struct remote_participant_access_rights {
    AccessControlObject _parent;
    DDS_Security_IdentityHandle remote_identity;
    local_participant_access_rights *local_rights;
    remote_permissions *permissions;
    char *identity_subject_name;
} remote_participant_access_rights;

void access_control_object_init(AccessControlObject *obj, AccessControlObjectKind_t kind, AccessControlObjectDestructor destructor);
AccessControlObject *access_control_object_keep(AccessControlObject *obj);
void access_control_object_release(AccessControlObject *obj);
bool access_control_object_valid(const AccessControlObject *obj, AccessControlObjectKind_t kind);
void access_control_object_free(AccessControlObject *obj);

struct AccessControlTable;
typedef int (*AccessControlTableCallback)(AccessControlObject *obj, void *arg);
struct AccessControlTable *access_control_table_new(void);

void access_control_table_free(struct AccessControlTable *table);
AccessControlObject *access_control_table_insert(struct AccessControlTable *table, AccessControlObject *object);
void access_control_table_remove_object(struct AccessControlTable *table, AccessControlObject *object);
AccessControlObject *access_control_table_remove(struct AccessControlTable *table, int64_t handle);
AccessControlObject *access_control_table_find(struct AccessControlTable *table, int64_t handle);
void access_control_table_walk(struct AccessControlTable *table, AccessControlTableCallback callback, void *arg);

local_participant_access_rights *ac_local_participant_access_rights_new(
    DDS_Security_IdentityHandle local_identity,
    int domain_id,
    char *permissions_document,
    X509 *permissions_ca,
    const char* identity_subject_name,
    struct governance_parser *governance_tree,
    struct permissions_parser *permissions_tree);

remote_participant_access_rights *ac_remote_participant_access_rights_new(
    DDS_Security_IdentityHandle remote_identity,
    const local_participant_access_rights *local_rights,
    remote_permissions *permissions,
    dds_time_t permission_expiry,
    const DDS_Security_PermissionsToken *remote_permissions_token,
    const char *identity_subject);

#endif /* ACCESS_CONTROL_OBJECTS_H */
