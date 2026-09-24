// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include "CUnit/Test.h"
#include "dds/ddsrt/filesystem.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/process.h"

#if DDSRT_HAVE_FILESYSTEM && defined _WIN32
#include <direct.h>
#endif

#if DDSRT_HAVE_FILESYSTEM
static void check_abspath (const char *name, const char *expected)
{
  char *path = NULL;
  CU_ASSERT_EQ_FATAL (ddsrt_file_abspath (name, &path), DDS_RETCODE_OK);
  CU_ASSERT_STREQ (path, expected);
  ddsrt_free (path);
}
#endif

CU_Test (ddsrt_filesystem, abspath)
{
#if DDSRT_HAVE_FILESYSTEM
  char *path = NULL;
  CU_ASSERT_EQ_FATAL (ddsrt_file_abspath ("nonexistent-trace-file", &path), DDS_RETCODE_OK);
  check_abspath ("./nonexistent-trace-file", path);
  check_abspath (".//./nonexistent-trace-file", path);
  check_abspath (path, path);
  ddsrt_free (path);
#endif
}

CU_Test (ddsrt_filesystem, abspath_invalid)
{
#if DDSRT_HAVE_FILESYSTEM
  char *path = (char *) 1;
  CU_ASSERT_EQ (ddsrt_file_abspath ("", &path), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (path, NULL);
  path = (char *) 1;
  CU_ASSERT_EQ (ddsrt_file_abspath (NULL, &path), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (path, NULL);
  CU_ASSERT_EQ (ddsrt_file_abspath ("file", NULL), DDS_RETCODE_BAD_PARAMETER);
#endif
}

CU_Test (ddsrt_filesystem, abspath_current_directory)
{
#if DDSRT_HAVE_FILESYSTEM
  char *cwd = NULL, *before = NULL, *after = NULL;
  CU_ASSERT_EQ_FATAL (ddsrt_file_abspath (".", &cwd), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (ddsrt_file_abspath ("trace.log", &before), DDS_RETCODE_OK);
#ifdef _WIN32
  char dirname[64];
#else
  char dirname[192];
#endif
  const int n = snprintf (dirname, sizeof (dirname), "ddsrt-abspath-%"PRIdPID"-", ddsrt_getpid ());
  memset (dirname + n, 'x', sizeof (dirname) - (size_t) n - 1);
  dirname[sizeof (dirname) - 1] = 0;
#ifdef _WIN32
  CU_ASSERT_EQ_FATAL (_mkdir (dirname), 0);
  const int changed = _chdir (dirname);
#else
  CU_ASSERT_EQ_FATAL (mkdir (dirname, 0700), 0);
  const int changed = chdir (dirname);
#endif
  /* The long directory name also exercises growing the POSIX getcwd buffer.
     Restore the working directory before asserting the conversion result. */
  const dds_return_t rc = ddsrt_file_abspath ("trace.log", &after);
#ifdef _WIN32
  const int restored = _chdir (cwd);
  const int removed = _rmdir (dirname);
#else
  const int restored = chdir (cwd);
  const int removed = rmdir (dirname);
#endif
  CU_ASSERT_EQ (changed, 0);
  CU_ASSERT_EQ (restored, 0);
  CU_ASSERT_EQ (removed, 0);
  CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK);
  CU_ASSERT_STRNEQ (before, after);
  CU_ASSERT_NEQ (strstr (after, dirname), NULL);
  check_abspath ("trace.log", before);
  ddsrt_free (cwd);
  ddsrt_free (before);
  ddsrt_free (after);
#endif
}

CU_Test (ddsrt_filesystem, abspath_platform)
{
#if DDSRT_HAVE_FILESYSTEM
#ifdef _WIN32
  check_abspath ("C:/nonexistent/./trace.log", "C:\\nonexistent\\trace.log");
  check_abspath ("\\\\server\\share\\.\\trace.log", "\\\\server\\share\\trace.log");
  char *path = NULL, *relative = NULL;
  CU_ASSERT_EQ_FATAL (ddsrt_file_abspath ("trace.log", &path), DDS_RETCODE_OK);
  if (path[1] == ':')
  {
    char name[] = "C:trace.log";
    name[0] = path[0];
    CU_ASSERT_EQ_FATAL (ddsrt_file_abspath (name, &relative), DDS_RETCODE_OK);
    CU_ASSERT_STREQ (relative, path);
    ddsrt_free (relative);
  }
  ddsrt_free (path);
#else
  check_abspath ("/", "/");
  check_abspath ("//", "//");
  check_abspath ("///", "/");
  check_abspath ("/./trace.log", "/trace.log");
  check_abspath ("/nonexistent//./trace.log", "/nonexistent/trace.log");
  check_abspath ("//nonexistent//trace.log", "//nonexistent/trace.log");
  check_abspath ("/nonexistent/../trace.log", "/nonexistent/../trace.log");
  check_abspath ("/nonexistent/.", "/nonexistent/");
  check_abspath ("/nonexistent///", "/nonexistent/");
  check_abspath ("/a\\b", "/a\\b");

  /* Conversion must not require the path to exist or fit in PATH_MAX. */
  char *longpath = ddsrt_malloc (8194);
  longpath[0] = '/';
  memset (longpath + 1, 'a', 8192);
  longpath[8193] = 0;
  check_abspath (longpath, longpath);
  ddsrt_free (longpath);
#endif
#endif
}
