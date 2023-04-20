// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/types.h"
#include "access_control_objects.h"
#include "access_control_utils.h"
#include "access_control_parser.h"

struct AccessControlTable
{
  struct ddsrt_hh *htab;
  ddsrt_mutex_t lock;
};

bool access_control_object_valid(const AccessControlObject *obj, const AccessControlObjectKind_t kind)
{
  if (!obj)
    return false;
  if (obj->kind != kind)
    return false;
  if (obj->handle != (int64_t)(uintptr_t)obj)
    return false;

  return true;
}

static uint32_t access_control_object_hash(const void *obj)
{
  const AccessControlObject *object = obj;
  const uint64_t c = 0xE21B371BEB9E6C05;
  const uint32_t x = (uint32_t)object->handle;
  return (unsigned)((x * c) >> 32);
}

static int access_control_object_equal(const void *ha, const void *hb)
{
  const AccessControlObject *la = ha;
  const AccessControlObject *lb = hb;
  return la->handle == lb->handle;
}

void access_control_object_init(AccessControlObject *obj, AccessControlObjectKind_t kind, AccessControlObjectDestructor destructor)
{
  assert(obj);
  obj->kind = kind;
  obj->handle = (int64_t)(uintptr_t)obj;
  obj->destructor = destructor;
  ddsrt_atomic_st32(&obj->refcount, 1);
}

static void access_control_object_deinit(AccessControlObject *obj)
{
  assert(obj);
  obj->handle = DDS_SECURITY_HANDLE_NIL;
  obj->kind = ACCESS_CONTROL_OBJECT_KIND_UNKNOWN;
  obj->destructor = NULL;
}

void access_control_object_free(AccessControlObject *obj)
{
  if (obj && obj->destructor)
    obj->destructor(obj);
}

AccessControlObject *access_control_object_keep(AccessControlObject *obj)
{
  if (obj)
    ddsrt_atomic_inc32(&obj->refcount);
  return obj;
}

void access_control_object_release(AccessControlObject *obj)
{
  if (obj)
  {
    if (ddsrt_atomic_dec32_nv(&obj->refcount) == 0)
      access_control_object_free(obj);
  }
}

struct AccessControlTable *access_control_table_new(void)
{
  struct AccessControlTable *table;

  table = ddsrt_malloc(sizeof(*table));
  table->htab = ddsrt_hh_new(32, access_control_object_hash, access_control_object_equal);
  ddsrt_mutex_init(&table->lock);
  return table;
}

void access_control_table_free(struct AccessControlTable *table)
{
  struct ddsrt_hh_iter it;
  AccessControlObject *obj;

  if (!table)
    return;
  for (obj = ddsrt_hh_iter_first(table->htab, &it); obj; obj = ddsrt_hh_iter_next(&it))
  {
    (void)ddsrt_hh_remove(table->htab, obj);
    access_control_object_release(obj);
  }
  ddsrt_hh_free(table->htab);
  ddsrt_mutex_destroy(&table->lock);
  ddsrt_free(table);
}

AccessControlObject *access_control_table_insert(struct AccessControlTable *table, AccessControlObject *object)
{
  AccessControlObject template;
  AccessControlObject *cur;
  assert(table);
  assert(object);
  template.handle = object->handle;
  ddsrt_mutex_lock(&table->lock);
  if (!(cur = access_control_object_keep(ddsrt_hh_lookup(table->htab, &template))))
  {
    cur = access_control_object_keep(object);
    (void)ddsrt_hh_add(table->htab, cur);
  }
  ddsrt_mutex_unlock(&table->lock);
  return cur;
}

void access_control_table_remove_object(struct AccessControlTable *table, AccessControlObject *object)
{
  assert(table);
  assert(object);
  ddsrt_mutex_lock(&table->lock);
  (void)ddsrt_hh_remove(table->htab, object);
  ddsrt_mutex_unlock(&table->lock);
  access_control_object_release(object);
}

AccessControlObject *access_control_table_remove(struct AccessControlTable *table, int64_t handle)
{
  AccessControlObject template;
  AccessControlObject *object;
  assert(table);
  template.handle = handle;
  ddsrt_mutex_lock(&table->lock);
  if ((object = access_control_object_keep(ddsrt_hh_lookup(table->htab, &template))))
  {
    (void)ddsrt_hh_remove(table->htab, object);
    access_control_object_release(object);
  }
  ddsrt_mutex_unlock(&table->lock);
  return object;
}

AccessControlObject *access_control_table_find(struct AccessControlTable *table, int64_t handle)
{
  AccessControlObject template;
  AccessControlObject *object;
  assert(table);
  template.handle = handle;
  ddsrt_mutex_lock(&table->lock);
  object = access_control_object_keep(ddsrt_hh_lookup(table->htab, &template));
  ddsrt_mutex_unlock(&table->lock);
  return object;
}

void access_control_table_walk(struct AccessControlTable *table, AccessControlTableCallback callback, void *arg)
{
  struct ddsrt_hh_iter it;
  AccessControlObject *obj;
  int r = 1;
  assert(table);
  assert(callback);
  ddsrt_mutex_lock(&table->lock);
  for (obj = ddsrt_hh_iter_first(table->htab, &it); r && obj; obj = ddsrt_hh_iter_next(&it))
    r = callback(obj, arg);
  ddsrt_mutex_unlock(&table->lock);
}

static void local_participant_access_rights_free(AccessControlObject *obj)
{
  local_participant_access_rights *rights = (local_participant_access_rights *)obj;
  if (rights)
  {
    ddsrt_free(rights->permissions_document);
    if (rights->permissions_ca)
      X509_free(rights->permissions_ca);
    access_control_object_deinit((AccessControlObject *)rights);
    if (rights->governance_tree)
      ac_return_governance_tree(rights->governance_tree);
    if (rights->permissions_tree)
      ac_return_permissions_tree(rights->permissions_tree);
    ddsrt_free(rights->identity_subject_name);
    ddsrt_free(rights);
  }
}

local_participant_access_rights *ac_local_participant_access_rights_new(
    DDS_Security_IdentityHandle local_identity,
    int domain_id,
    char *permissions_document,
    X509 *permissions_ca,
    const char *identity_subject_name,
    struct governance_parser *governance_tree,
    struct permissions_parser *permissions_tree)
{
  local_participant_access_rights *rights = ddsrt_malloc(sizeof(local_participant_access_rights));
  memset(rights, 0, sizeof(local_participant_access_rights));
  access_control_object_init((AccessControlObject *)rights, ACCESS_CONTROL_OBJECT_KIND_LOCAL_PARTICIPANT, local_participant_access_rights_free);
  rights->local_identity = local_identity;
  rights->domain_id = domain_id;
  rights->permissions_document = permissions_document;
  rights->permissions_ca = permissions_ca;
  rights->identity_subject_name = ddsrt_strdup(identity_subject_name);
  rights->governance_tree = governance_tree;
  rights->permissions_tree = permissions_tree;
  return rights;
}


static void remote_participant_access_rights_free(AccessControlObject *obj)
{
  remote_participant_access_rights *rights = (remote_participant_access_rights *)obj;
  if (rights)
  {
    if (rights->permissions)
    {
      assert(rights->permissions->ref_cnt > 0);
      rights->permissions->ref_cnt--;
      if (rights->permissions->ref_cnt == 0)
      {
        ac_return_permissions_tree(rights->permissions->permissions_tree);
        ddsrt_free(rights->permissions->remote_permissions_token_class_id);
        ddsrt_free(rights->permissions);
      }
    }
    ddsrt_free(rights->identity_subject_name);
    ACCESS_CONTROL_OBJECT_RELEASE(rights->local_rights);
    access_control_object_deinit((AccessControlObject *)rights);
    ddsrt_free(rights);
  }
}

remote_participant_access_rights *
ac_remote_participant_access_rights_new(
    DDS_Security_IdentityHandle remote_identity,
    const local_participant_access_rights *local_rights,
    remote_permissions *permissions,
    dds_time_t permission_expiry,
    const DDS_Security_PermissionsToken *remote_permissions_token,
    const char *identity_subject)
{
  remote_participant_access_rights *rights = ddsrt_malloc(sizeof(remote_participant_access_rights));
  memset(rights, 0, sizeof(remote_participant_access_rights));
  access_control_object_init((AccessControlObject *)rights, ACCESS_CONTROL_OBJECT_KIND_REMOTE_PARTICIPANT, remote_participant_access_rights_free);
  rights->remote_identity = remote_identity;
  rights->permissions = permissions;
  rights->_parent.permissions_expiry = permission_expiry;
  rights->local_rights = (local_participant_access_rights *)ACCESS_CONTROL_OBJECT_KEEP(local_rights);
  if (rights->permissions)
  {
    rights->permissions->ref_cnt++;
    if (rights->permissions->remote_permissions_token_class_id == NULL)
      rights->permissions->remote_permissions_token_class_id = ddsrt_strdup(remote_permissions_token->class_id);
    else
      assert (strcmp (rights->permissions->remote_permissions_token_class_id, remote_permissions_token->class_id) == 0);
    rights->identity_subject_name = ddsrt_strdup(identity_subject);
  }
  else
  {
    assert(identity_subject == NULL);
    rights->identity_subject_name = NULL;
  }
  return rights;
}
