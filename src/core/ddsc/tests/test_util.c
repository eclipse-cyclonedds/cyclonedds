// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdarg.h>
#include <string.h>
#include "dds/dds.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds__entity.h"
#include "test_util.h"

void tprintf (const char *msg, ...)
{
  va_list args;
  dds_time_t t = dds_time ();
  printf ("%d.%06d ", (int32_t) (t / DDS_NSECS_IN_SEC), (int32_t) (t % DDS_NSECS_IN_SEC) / 1000);
  va_start (args, msg);
  vprintf (msg, args);
  va_end (args);
  fflush (stdout);
}

char *create_unique_topic_name (const char *prefix, char *name, size_t size)
{
  static ddsrt_atomic_uint32_t count = DDSRT_ATOMIC_UINT64_INIT (0);
  const ddsrt_pid_t pid = ddsrt_getpid();
  const ddsrt_tid_t tid = ddsrt_gettid();
  const uint32_t nr = ddsrt_atomic_inc32_nv (&count);
  (void) snprintf (name, size, "%s%"PRIu32"_pid%" PRIdPID "_tid%" PRIdTID "", prefix, nr, pid, tid);
  return name;
}

char *test_config_from_env (const char *config, dds_domainid_t domain_id)
{
  char *template = NULL;
  char *expanded;
  const char *local_config = config ? config : "";

  (void) ddsrt_asprintf (&template, "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}%s", local_config);
  expanded = ddsrt_expand_envvars (template, domain_id);
  ddsrt_free (template);
  return expanded;
}

dds_entity_t test_create_domain_from_env (dds_domainid_t domain_id, const char *config)
{
  char *expanded = test_config_from_env (config, domain_id);
  dds_entity_t domain = dds_create_domain (domain_id, expanded);
  ddsrt_free (expanded);
  return domain;
}

bool test_config_inherits_fakeudp (void)
{
#ifdef DDS_HAS_FAKEUDP
  const char *env_uri;
  return ddsrt_getenv ("CYCLONEDDS_URI", &env_uri) == DDS_RETCODE_OK &&
         env_uri != NULL && strstr (env_uri, "fakeudp") != NULL;
#else
  return false;
#endif
}

dds_return_t test_save_envvar (struct test_saved_envvar *saved, const char *name)
{
  const char *value;
  const dds_return_t rc = ddsrt_getenv (name, &value);
  saved->name = name;
  saved->value = NULL;
  if (rc == DDS_RETCODE_OK)
  {
    saved->value = ddsrt_strdup (value);
    return saved->value != NULL ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES;
  }
  return rc == DDS_RETCODE_NOT_FOUND ? DDS_RETCODE_OK : rc;
}

dds_return_t test_restore_envvar (struct test_saved_envvar *saved)
{
  const dds_return_t rc = ddsrt_setenv (saved->name, saved->value ? saved->value : "");
  ddsrt_free (saved->value);
  saved->name = NULL;
  saved->value = NULL;
  return rc;
}

struct ddsi_domaingv *get_domaingv (dds_entity_t handle)
{
  struct dds_entity *x;
  dds_return_t ret = dds_entity_pin (handle, &x);
  assert (ret == DDS_RETCODE_OK);
  (void) ret;
  struct ddsi_domaingv * const gv = &x->m_domain->gv;
  dds_entity_unpin (x);
  return gv;
}

void gen_test_guid (struct ddsi_domaingv *gv, ddsi_guid_t *guid, uint32_t entity_id)
{
  union { uint64_t u64; uint32_t u32[2]; } u;
  u.u32[0] = gv->ppguid_base.prefix.u[1];
  u.u32[1] = gv->ppguid_base.prefix.u[2];
  u.u64 += ddsi_iid_gen ();
  guid->prefix.u[0] = gv->ppguid_base.prefix.u[0];
  guid->prefix.u[1] = u.u32[0];
  guid->prefix.u[2] = u.u32[1];
  guid->entityid.u = entity_id;
}
