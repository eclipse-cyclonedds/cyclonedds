// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "idl/heap.h"
#include "idl/print.h"

#if defined(_MSC_VER)
# define idl_thread_local __declspec(thread)
#elif defined(__GNUC__) || (defined(__clang__) && __clang_major__ >= 2)
# define idl_thread_local __thread
#elif defined(__SUNPROC_C) || defined(__SUNPRO_CC)
# define idl_thread_local __thread
#else
# error "Thread-local storage is not supported"
#endif

struct printa {
  idl_print_t print;
  const void *object;
  void *user_data;
  size_t size;
  char *str, **strp;
};

static idl_thread_local struct printa printa;

int idl_printa_arguments__(
  char **strp, idl_print_t print, const void *object, void *user_data)
{
  int cnt;
  char buf[1];

  assert(strp);
  assert(print);
  assert(object);

  if ((cnt = print(buf, sizeof(buf), object, user_data)) < 0)
    return cnt;

  printa.print = print;
  printa.object = object;
  printa.user_data = user_data;
  printa.size = (size_t)cnt + 1;
  printa.strp = strp;
  printa.str = NULL;

  return cnt;
}

size_t idl_printa_size__(void)
{
  assert(!printa.str);
  return printa.size;
}

char **idl_printa_strp__(void)
{
  return &printa.str;
}

int idl_printa__(void)
{
  int cnt;

  assert(printa.size);
  assert(printa.str);
  assert(printa.strp);

  cnt = printa.print(
    printa.str, printa.size, printa.object, printa.user_data);
  if (cnt >= 0)
    *printa.strp = printa.str;
  printa.print = 0;
  printa.object = NULL;
  printa.user_data = NULL;
  printa.size = 0;
  printa.str = NULL;
  printa.strp = NULL;
  return cnt;
}

int idl_print__(
  char **strp, idl_print_t print, const void *object, void *user_data)
{
  int cnt, len;
  char buf[1], *str = NULL;

  if ((len = print(buf, sizeof(buf), object, user_data)) < 0)
    return len;
  if (!(str = idl_malloc((size_t)len + 1)))
    return -1;
  if ((cnt = print(str, (size_t)len + 1, object, user_data)) >= 0)
    *strp = str;
  else
    idl_free(str);
  return cnt;
}
