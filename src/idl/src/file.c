/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#if _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "file.h"
#include "idl/string.h"

unsigned int idl_isseparator(int chr)
{
#if _WIN32
  return chr == '/' || chr == '\\';
#else
  return chr == '/';
#endif
}

#define isseparator(chr) idl_isseparator(chr)

unsigned int idl_isabsolute(const char *path)
{
  assert(path);
  if (path[0] == '/')
    return 1;
#if _WIN32
  if (((path[0] >= 'a' && path[0] <= 'z') || (path[0] >= 'A' && path[0] <= 'Z')) &&
       (path[1] == ':') &&
       (path[2] == '/' || path[2] == '\\' || path[2] == '\0'))
    return 3;
#endif
  return 0;
}

#define isabsolute(chr) idl_isabsolute(chr)

#if _WIN32
static const char sep = '\\';
#else
static const char sep = '/';
#endif

idl_retcode_t
idl_current_path(char **abspathp)
{
  char *cwd;
  assert(abspathp);
#if _WIN32
  cwd = _getcwd(NULL, 0);
#else
  cwd = getcwd(NULL, 0);
#endif
  if (!cwd)
    return IDL_RETCODE_NO_MEMORY;
  *abspathp = cwd;
  return IDL_RETCODE_OK;
}

static int isdelimiter(char chr)
{
  return chr == '\0' || isseparator(chr);
}

ssize_t idl_untaint_path(char *path)
{
  size_t abs, len;

  assert(path);

  if ((abs = isabsolute(path)) && path[abs - 1] != '\0')
    path[abs - 1] = sep;

  len = abs;
  for (size_t i=abs,j=i,k,n;;) {
    if (isdelimiter(path[i])) {
      n = i - j;
      if (n == 2 && strncmp(path + j, "..", n) == 0) {
        if (len == abs && abs)
          return -1; /* invalid path */
        /* drop segment, unless segment is ".." */
        for (k=(len ? len - 1 : 0); k > abs && !isseparator(path[k]); k--) ;
        if (strncmp(path + (k + (k != abs)), "..", len - (k + (k != abs))) == 0)
          goto move;
        len = k;
      } else if (n != 0) {
move:
        if (len != abs)
          path[len++] = sep;
        memmove(path + len, path + j, n);
        len += n;
      }
#if defined(_MSC_VER)
__pragma(warning(suppress: 6385))
#endif
      if (path[i++] == '\0')
        break;
      goto mark;
    } else if (i == abs) {
      /* start of segment */
mark:
      n = (path[i] != '.' ? 0 : (path[i+1] != '.' ? 1 : 2));
      if (!isdelimiter(path[i+n]) || n == 2)
        j = i;
      else
        j = i + n;
      i += n ? n : i == abs;
    } else {
      i += 1;
    }
  }

  path[len] = '\0';
  return (ssize_t)len;
}

static char *absolute_path(const char *path)
{
  if (isabsolute(path) == 0) {
    char *abspath = NULL, *dir;
    size_t len, dirlen, pathlen;
    if (idl_current_path(&dir))
      goto err_cwd;
    dirlen = strlen(dir);
    if (dirlen + 0 >= (SIZE_MAX - 2))
      goto err_abs;
    pathlen = strlen(path);
    if (dirlen + 2 >= (SIZE_MAX - pathlen))
      goto err_abs;
    len = dirlen + 1 /* separator */ + pathlen;
    if (!(abspath = malloc(len + 1)))
      goto err_abs;
    memcpy(abspath, dir, dirlen);
    abspath[dirlen] = sep;
    memcpy(abspath + dirlen + 1, path, pathlen);
    abspath[len] = '\0';
err_abs:
    free(dir);
err_cwd:
    return abspath;
  }
  return idl_strdup(path);
}

#if _WIN32
static ssize_t normalize_segment(const char *path, char *segment)
{
  WIN32_FIND_DATA find_data;
  HANDLE find;
  ssize_t seglen = strlen(segment);

  find = FindFirstFile(path, &find_data);
  if (find == INVALID_HANDLE_VALUE)

  if (strlen(find_data.cFileName) == (size_t)seglen)
    memcpy(segment, find_data.cFileName, seglen);
  else
    seglen = -1;
  FindClose(find);
  return seglen;
}
#else
static ssize_t normalize_segment(const char *path, char *segment)
{
  /* FIXME: implement support for case correction on *NIX platforms */
  struct stat buf;
  (void)segment;
  return stat(path, &buf);
}
#endif

idl_retcode_t idl_normalize_path(const char *path, char **normpathp)
{
  idl_retcode_t ret;
  size_t abs;
  ssize_t len;
  char *abspath = NULL, *normpath = NULL;

  if (!(abspath = absolute_path(path)))
    { ret = IDL_RETCODE_NO_MEMORY; goto err_abs; }
  if ((len = idl_untaint_path(abspath)) < 0)
    { ret = IDL_RETCODE_BAD_PARAMETER; goto err_norm; }
  if (!(normpath = malloc((size_t)len + 1)))
    { ret = IDL_RETCODE_NO_MEMORY; goto err_norm; }

  /* ensure Windows drive letters are capitals */
  if (idl_islower((unsigned char)abspath[0]))
    abspath[0] = (char)idl_toupper(abspath[0]);

  {
    char *seg, *ptr = NULL;
    char delim[] = { '/', '\0', '\0' };
    size_t pos = 0, seglen;
#if _WIN32
    delim[1] = sep;
#endif
    abs = isabsolute(abspath);
    assert(abs && abs <= (size_t)len);
    seg = idl_strtok_r(abspath, delim, &ptr);
    seglen = strlen(seg);
    if (abs == 1)
      normpath[pos++] = sep;
    memmove(normpath + pos, seg, seglen);
    pos += seglen;
    if (abs != 1)
      normpath[pos++] = sep;
    normpath[pos] = '\0';
    while ((seg = idl_strtok_r(NULL, delim, &ptr))) {
      seglen = strlen(seg);
      if (pos != abs)
        normpath[pos++] = sep;
      memmove(normpath + pos, seg, seglen);
      normpath[pos + seglen] = '\0';
      if (normalize_segment(normpath, normpath + pos) == -1)
        { ret = IDL_RETCODE_NO_ENTRY; goto err_seg; }
      pos += seglen;
    }
    assert(pos == (size_t)len);
  }

  free(abspath);
  *normpathp = normpath;
  return (idl_retcode_t)len;
err_seg:
  free(normpath);
err_norm:
  free(abspath);
err_abs:
  return ret;
}

static unsigned int isresolved(const char *path)
{
  for (size_t i=0,n; path[i];) {
    if (isseparator(path[i])) {
      i++;
      goto segment;
    } else if (i == 0) {
segment:
      n = (path[i] != '.' ? 0 : (path[i+1] != '.' ? 1 : 2));
      if (n && isdelimiter(path[i+n]))
        return 0u;
      i += n ? n : i == 0;
    } else {
      i++;
    }
  }
  return 1u;
}

#if _WIN32
static inline int chrcasecmp(int a, int b)
{
  if (a >= 'A' && a <= 'Z')
    a = 'a' + (a - 'A');
  if (b >= 'A' && b <= 'Z')
    b = 'a' + (b - 'A');
  return a-b;
}
#else
static inline int chrcmp(int a, int b)
{
  return a-b;
}
#endif

idl_retcode_t idl_relative_path(const char *base, const char *path, char **relpathp)
{
  size_t pc, psc = 0;
  size_t bc, bsc = 0;
  char *rel = NULL, *rev = NULL;
  const char *rew, *fwd, *nop = "";

#if _WIN32
  int(*cmp)(int, int) = chrcasecmp;
#else
  int(*cmp)(int, int) = chrcmp;
#endif

  /* reject non-absolute paths and non-resolved paths */
  if (!isabsolute(base) || !isresolved(base))
    return IDL_RETCODE_BAD_PARAMETER;
  if (!isabsolute(path) || !isresolved(path))
    return IDL_RETCODE_BAD_PARAMETER;

  /* find common prefix to strip */
  do {
    pc = psc;
    bc = bsc;
    /* skip matching characters */
    for (; !isdelimiter(path[pc]) && !isdelimiter(base[bc]); pc++, bc++) {
      if (cmp(path[pc], base[bc]) != 0)
        break;
    }
    /* skip separators */
    for (psc=pc; isseparator((unsigned char)path[psc]); psc++) ;
    for (bsc=bc; isseparator((unsigned char)base[bsc]); bsc++) ;
    /* continue on mismatch between separators */
  } while ((pc!=psc) == (bc!=bsc) && path[psc] && !cmp(path[psc], base[bsc]));

  /* matching paths */
  if (!path[psc] && !base[bsc]) {
    rew = nop;
    fwd = nop;
  /* base is subdirectory of path */
  } else if (!path[psc] && isseparator(base[bc])) {
    rew = &base[bsc];
    fwd = nop;
  /* path is subdirectory of base */
  } else if (!base[bsc] && isseparator(path[pc])) {
    rew = nop;
    fwd = &path[psc];
  /* partial match, revert to common prefix */
  } else {
    /* rewind to last separator */
    for (; pc > 0 && !isseparator(path[pc]); pc--) ;
    for (; bc > 0 && !isseparator(base[bc]); bc--) ;
    /* forward to first non-separator */
    for (; isseparator(path[pc]); pc++) ;
    for (; isseparator(base[bc]); bc++) ;
    rew = &base[bc];
    fwd = &path[pc];
  }

  if (rew != nop) {
    size_t len, cnt = 1;
    for (size_t i=0; rew[i]; i++)
      cnt += (isseparator(rew[i]) && !isdelimiter(rew[i+1]));
    len = cnt*3;
    if (!(rev = malloc(len+1)))
      return IDL_RETCODE_NO_MEMORY;
    memset(rev, '.', len);
    rev[len] = '\0';
    for (size_t i=0; i < cnt; i++)
      rev[(i*3)+2] = sep;
  }

  idl_asprintf(&rel, "%s%s", rev ? rev : "", fwd);
  if (rev)
    free(rev);
  if (!rel)
    return IDL_RETCODE_NO_MEMORY;
  idl_untaint_path(rel);
  *relpathp = rel;
  return IDL_RETCODE_OK;
}
