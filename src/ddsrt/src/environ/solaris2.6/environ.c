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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/retcode.h"

extern char **environ;

static int
isenvvar(const char *name)
{
  return (*name == '\0' || strchr(name, '=') != NULL) == 0;
}

dds_return_t
ddsrt_getenv(const char *name, const char **value)
{
  char *env;

  assert(name != NULL);
  assert(value != NULL);

  if (!isenvvar(name))
    return DDS_RETCODE_BAD_PARAMETER;
  if ((env = getenv(name)) != NULL) {
    *value = env;
    return DDS_RETCODE_OK;
  }
  return DDS_RETCODE_NOT_FOUND;
}

dds_return_t
ddsrt_setenv(const char *name, const char *value)
{
  /* Not MT-Safe -- but it is only used in a tests to set
     CYCLONEDDS_URI, so for Solaris 2.6 support it is not worth the
     bother to do a better job.  Same for a bit of leakage. */
  assert(name != NULL);
  assert(value != NULL);

  if (strlen(value) == 0)
    return ddsrt_unsetenv(name);
  if (!isenvvar(name))
    return DDS_RETCODE_BAD_PARAMETER;

  const size_t namelen = strlen (name);
  const size_t entrysize = namelen + 1 + strlen (value) + 1;
  char *entry = malloc (entrysize);
  snprintf (entry, entrysize, "%s=%s", name, value);
  size_t n = 0;
  while (environ[n] != NULL)
  {
    if (strncmp (environ[n], name, namelen) == 0 && environ[n][namelen] == '=')
    {
      environ[n] = entry;
      return DDS_RETCODE_OK;
    }
    n++;
  }
  environ = realloc (environ, (n + 2) * sizeof (*environ));
  environ[n] = entry;
  environ[n+1] = NULL;
  return DDS_RETCODE_OK;
}

dds_return_t
ddsrt_unsetenv(const char *name)
{
  /* Same considerations as setenv. */
  assert(name != NULL);

  if (!isenvvar(name))
    return DDS_RETCODE_BAD_PARAMETER;

  const size_t namelen = strlen (name);
  size_t n = 0, idx = SIZE_MAX;
  while (environ[n] != NULL)
  {
    if (idx > n && strncmp (environ[n], name, namelen) == 0 && environ[n][namelen] == '=')
      idx = n;
    n++;
  }
  if (idx < n)
    memmove (&environ[idx], &environ[idx + 1], (n - idx) * sizeof (*environ));
  return DDS_RETCODE_OK;
}
