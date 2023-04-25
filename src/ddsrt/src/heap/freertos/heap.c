// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <FreeRTOS.h>

#if defined(configSUPPORT_DYNAMIC_ALLOCATION) && \
           (configSUPPORT_DYNAMIC_ALLOCATION == 0)
# error Dynamic memory allocation is not supported
#endif

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dds/ddsrt/heap.h"

static const size_t ofst = sizeof(size_t);

void *ddsrt_malloc_s(size_t size)
{
  void *ptr = NULL;

  if (size == 0) {
    size = 1;
  }

  if ((SIZE_MAX - size) < ofst) {
    errno = ERANGE;
  } else {
    ptr = pvPortMalloc(size + ofst);
    if (ptr == NULL) {
      errno = ENOMEM;
    } else {
      *((size_t *)ptr) = size;
      ptr += ofst;
    }
  }

  return ptr;
}

void *ddsrt_malloc(size_t size)
{
  void *ptr;

  if ((ptr = ddsrt_malloc_s(size)) == NULL) {
    abort();
  }

  return ptr;
}

void *ddsrt_calloc_s(size_t nmemb, size_t size)
{
  void *ptr = NULL;

  if (nmemb == 0 || size == 0) {
    nmemb = size = 1;
  }

  if ((SIZE_MAX / nmemb) <= size) {
    errno = ERANGE;
  } else {
    ptr = ddsrt_malloc_s(nmemb * size);
    (void)memset(ptr, 0, nmemb * size);
  }

  return ptr;
}

void *ddsrt_calloc(size_t nmemb, size_t size)
{
  void *ptr = NULL;

  if ((ptr = ddsrt_calloc_s(nmemb, size)) == NULL) {
    abort();
  }

  return ptr;
}

/* pvPortMalloc may be used instead of directly invoking malloc and free as
   offered by the standard C library. Unfortunately FreeRTOS does not offer a
   realloc compatible function and extra information must be embedded in every
   memory block in order to support reallocation of memory (otherwise the
   number of bytes that must be copied is unavailable). */
void *ddsrt_realloc_s(void *memblk, size_t size)
{
  void *ptr = NULL;
  size_t origsize = 0;

  if (memblk != NULL) {
    origsize = *((size_t *)(memblk - ofst));
  }

  if (size != origsize || origsize == 0) {
    if ((ptr = ddsrt_malloc_s(size)) == NULL) {
      return NULL;
    }
    if (memblk != NULL) {
      if (size > 0) {
        (void)memcpy(ptr, memblk, size > origsize ? origsize : size);
      }
      vPortFree(memblk - ofst);
    }
    memblk = ptr;
  }

  return memblk;
}

void *ddsrt_realloc(void *memblk, size_t size)
{
  void *ptr = NULL;

  if ((ptr = ddsrt_realloc_s(memblk, size)) == NULL) {
    abort();
  }

  return ptr;
}

void
ddsrt_free(void *ptr)
{
  if (ptr != NULL) {
    vPortFree(ptr - ofst);
  }
}
