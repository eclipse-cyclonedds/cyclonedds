// Copyright(c) 2021 ZettaScale Technology and others
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
#include "idl/heap.h"
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
    if (!(abspath = idl_malloc(len + 1)))
      goto err_abs;
    memcpy(abspath, dir, dirlen);
    abspath[dirlen] = sep;
    memcpy(abspath + dirlen + 1, path, pathlen);
    abspath[len] = '\0';
err_abs:
    idl_free(dir);
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
  size_t seglen = strlen(segment);

  find = FindFirstFile(path, &find_data);
  if (find == INVALID_HANDLE_VALUE)
    return -1;
  if (strlen(find_data.cFileName) == seglen)
    memcpy(segment, find_data.cFileName, seglen);
  else
    seglen = (size_t)-1;
  FindClose(find);
  return (ssize_t)seglen;
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
  if (!(normpath = idl_malloc((size_t)len + 1)))
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

  idl_free(abspath);
  *normpathp = normpath;
  return IDL_RETCODE_OK;
err_seg:
  idl_free(normpath);
err_norm:
  idl_free(abspath);
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
    if (!(rev = idl_malloc(len+1)))
      return IDL_RETCODE_NO_MEMORY;
    memset(rev, '.', len);
    rev[len] = '\0';
    for (size_t i=0; i < cnt; i++)
      rev[(i*3)+2] = sep;
  }

  (void) idl_asprintf(&rel, "%s%s", rev ? rev : "", fwd);
  if (rev)
    idl_free(rev);
  if (!rel)
    return IDL_RETCODE_NO_MEMORY;
  idl_untaint_path(rel);
  *relpathp = rel;
  return IDL_RETCODE_OK;
}

#if _WIN32
static inline idl_retcode_t idl_mkdir(const char *pathname, int mode) {
  (void) mode;
  return _mkdir(pathname);
}
#else
static inline idl_retcode_t idl_mkdir(const char *pathname, mode_t mode) {
  return mkdir(pathname, mode);
}
#endif

idl_retcode_t idl_mkpath(const char *path)
{
  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  char *full_path;

  assert(path);
  if (!(full_path = absolute_path(path)))
    goto err_full_path;

  if(idl_untaint_path(full_path) < 0)
    goto err_untaint;

  {
    char chr, *ptr = full_path;

    for (; *ptr && *ptr != '/' && *ptr != sep; ptr++)
      /* skip ahead to first separator to skip drive */;

    while (*ptr)
    {
      for (++ptr; *ptr && *ptr != '/' && *ptr != sep; ptr++)
        /* search for next segment */;
      assert(ptr[-1] != '/' && ptr[-1] != sep);
      chr = *ptr;
      *ptr = '\0';
      if ((ret = idl_mkdir(full_path, 0777)) == -1 && errno != EEXIST)
        goto err_mkdir;
      *ptr = chr;
    }

    ret = IDL_RETCODE_OK;
  }

  err_mkdir:
  err_untaint:
  idl_free(full_path);
  err_full_path:
  return ret;
}

/**
 * \verbatim
 *  Given inputs path, output_dir, base_dir, out_ext, constructs the correct output path for the file.
 *  Unless specified otherwise, will also create intermediate directories to allow for immediate access
 *  to the file on return.
 *
 *  If the input file is absolute, the output path will always be in the form of
 *  cwd/file.ext OR output_dir/file.ext
 *  File is derived from the end of `path`, any directories `a, b, c, etc.` are derived from rel. path difference
 *  between `path` and `base_dir`.
 *
 *  If the input path is relative, output path will be in the form of
 *  a/b/c/file.ext OR output_dir/a/b/c/file.ext, given an input path like a/b/c/file.idl
 *
 *  If base_dir is provided, output_path will take the form of
 *  b/c/file.ext OR output_dir/b/c/file.ext given
 *  input path /a/b/c/file.idl and a base path /a
 *  Memory will be allocated to ensure `path` is absolute, to use idl_relative_path and determine the common path.
 *  This will return an error if the path to the base_dir requires us to go up the tree,
 *  which could lead to unwanted behavior.
 *  (e.g. path=/a/b/c/d/file.idl, base_path=/a/b/c/f => ../d/file.idl)
 * \endverbatim
 *
 * @param path          Path (abs / rel) to the input file. Non NULL
 * @param output_dir    Path (abs / rel) to the desired output directory. Can be NULL
 * @param base_dir      Absolute path to the "base directory" of a tree. Can be NULL
 * @param out_ext       Desired output extension. Can be NULL
 * @param out_ptr       Pointer to output str. Non NULL
 * @param skip_mkpath   Allows the function to skip making the output path. Used for testing, recommended False
 * @return retcode
 */
idl_retcode_t idl_generate_out_file(const char *path, const char *output_dir, const char *base_dir, const char *out_ext, char ** out_ptr, int skip_mkpath) {
  assert(out_ptr);

  idl_retcode_t ret = IDL_RETCODE_NO_MEMORY;
  const char *sepr, *ext, *file;
  char empty[1] = { '\0' };
  char *dir = NULL, *basename = NULL, *abs_file_path = NULL;
  char* rel_path = NULL;
  char *output_path = NULL;

  if(base_dir && !idl_isabsolute(base_dir)) {
    ret = IDL_RETCODE_BAD_PARAMETER;
    goto err_dir;
  }

  sepr = ext = NULL;
  for (const char *ptr = path; ptr[0]; ptr++) {
    if (idl_isseparator((unsigned char)ptr[0]) && ptr[1] != '\0')
      sepr = ptr;
    else if (ptr[0] == '.')
      ext = ptr;
  }

  file = sepr ? sepr + 1 : path;
  if (idl_isabsolute(path) || !sepr)
    dir = empty;
  else if (!(dir = idl_strndup(path, (size_t)(sepr-path))))
    goto err_dir;
  if (!(basename = idl_strndup(file, ext ? (size_t)(ext-file) : strlen(file))))
    goto err_basename;

  /* replace backslashes by forward slashes */
  for (char *ptr = dir; *ptr; ptr++) {
    if (*ptr == '\\')
      *ptr = '/';
  }

  if(base_dir) {
    char* old_dir = dir;

    // We need an absolute path to use idl_relative_path so grab the absolute path of our input file
    // if we were provided relative paths
    if(!idl_isabsolute(path)) {
      if((idl_normalize_path(path, &abs_file_path)) < 0) {
        goto err_rel_path;
      }
    } else {
      if(!(abs_file_path = idl_strdup(path))) {
        goto err_rel_path;
      }
    }

    // If we run into a path error we can still recover and use existing dir
    if(idl_relative_path(base_dir, abs_file_path, &rel_path) == IDL_RETCODE_NO_MEMORY)
      goto err_rel_path;
    // If root is not a parent of file path, or there was an error resolving rel path then return an error
#if _WIN32
    int is_reverse_path = (rel_path && (strncmp(rel_path, "..\\", 3) == 0));
#else
    int is_reverse_path = (rel_path && (strncmp(rel_path, "../", 3) == 0));
#endif
    if(is_reverse_path){
      ret = IDL_RETCODE_BAD_PARAMETER;
      goto err_rel_path;
    }

    // Extract the common directory to dir, idl_free the old_dir pointer
    dir = NULL;
    size_t print_len = rel_path == NULL ? 0 : strlen(rel_path);
    if (file) {
      size_t file_len = sepr == NULL ? 0 : strlen(sepr);
      print_len = print_len >= file_len ? print_len - file_len : 0;
    }
    if (!(dir = idl_strndup(rel_path, print_len))) {
      goto err_rel_path;
    }
    if (old_dir != empty)
      idl_free(old_dir);
  }

  sepr = dir[0] == '\0' ? "" : "/";
  if(output_dir && output_dir[0] != '\0') {
    if(idl_asprintf(&output_path, "%s%s%s", output_dir, sepr, dir) < 0)
      goto err_rel_path;
    sepr = "/";
  } else {
    if(!(output_path = idl_strdup(dir)))
      goto err_rel_path;
  }

  // Allow skipping of path generation for unit testing
  if(!skip_mkpath) {
    if(idl_mkpath(output_path) < 0)
      goto err_mkpath;
  }

  if (idl_asprintf(out_ptr, "%s%s%s%s%s",
                   output_path,
                   sepr,
                   basename,
                   out_ext ? "." : "",
                   out_ext ? out_ext : ""
                   ) < 0) {
    goto err_outpath;
  }
  ret = IDL_RETCODE_OK;

err_outpath:
err_mkpath:
  if(output_path)
    idl_free(output_path);
err_rel_path:
  if(rel_path)
    idl_free(rel_path);
  if(abs_file_path)
    idl_free(abs_file_path);
  idl_free(basename);
err_basename:
  if (dir && dir != empty)
    idl_free(dir);
err_dir:
  return ret;
}
