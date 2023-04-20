// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"

/* Task list is a buffer used to keep track of blocked tasks. The buffer is
   cyclic to avoid memory (re)allocation as much as possible. To avoid memory
   relocation, the window is allowed to be sparse too.

   Active buckets must always reside in the window denoted by the first active
   bucket and the last active bucket.

   A buffer with 10 buckets will be neatly packed at first.

     X :  Used bucket in window.
     o :  Empty (invalidated) bucket.

   -----------------------
   | X X X X X o o o o o |  length: 10, count: 5
   --^-------^------------
     1st     nth

   As soon as the first task is unblocked.

   -----------------------
   | o X X X X o o o o o |  length: 10, count: 4
   ----^-----^------------
       1st   nth

   After a while the window will wrap around.

   -----------------------
   | X X X o o o o o X X |  length: 10, count: 5
   ------^-----------^----
         nth         1st

   When a task is popped, e.g. a task was not notified in due time.

   -----------------------
   | o X X o X X o o o o |  length: 10, count: 4
   ----^-------^----------
       1st     nth
*/

#ifndef NDEBUG

static void tasklist_assert(ddsrt_tasklist_t *list)
{
  size_t i;

  assert(list != NULL);

  if (list->cnt == 0) {
    assert(list->off == 0);
    assert(list->end == 0);
    assert(list->len == DDSRT_TASKLIST_INITIAL);
    for (i = 0; i < list->len; i++) {
      assert(list->tasks[i] == NULL);
    }
  }

  /* FIXME: add more checks */
}
#else
#define tasklist_assert(...)
#endif /* NDEBUG */

int ddsrt_tasklist_init(ddsrt_tasklist_t *list)
{
  TaskHandle_t *p;

  assert(list != NULL);

  p = ddsrt_malloc(DDSRT_TASKLIST_INITIAL * sizeof(*list->tasks));
  if (p == NULL) {
    return -1;
  }

  memset(list, 0, sizeof(*list));
  memset(p, 0, DDSRT_TASKLIST_INITIAL * sizeof(*list->tasks));
  list->tasks = p;
  list->len = DDSRT_TASKLIST_INITIAL;

  return 0;
}

void ddsrt_tasklist_fini(ddsrt_tasklist_t *list)
{
  ddsrt_free(list->tasks);
  memset(list, 0, sizeof(*list));
}

void ddsrt_tasklist_ltrim(ddsrt_tasklist_t *list)
{
  size_t i;

  assert(list != NULL);
  assert(list->cnt != 0);

  i = list->off;
  for (; i < list->len - 1 && list->tasks[i] == NULL; i++) { }
  /* Take into account wrap around. */
  if (list->tasks[i] == NULL) {
    assert(i == list->len - 1);
    assert(list->off > list->end);
    i = 0;
    /* Trim invalidated buckets from head. */
    for (; i < list->len - 1 && list->tasks[i] == NULL; i++) { }
  }
  list->off = i;
}

void ddsrt_tasklist_rtrim(ddsrt_tasklist_t *list)
{
  size_t i;

  assert(list != NULL);
  assert(list->cnt != 0);

  i = list->end;
  for (; i > 0 && list->tasks[i] == NULL; i--) { }
  /* Take into account wrap around. */
  if (list->tasks[i] == NULL) {
    assert(i == 0);
    assert(list->off > list->end);
    i = list->len - 1;
    /* Trim invalidated buckets from tail. */
    for (; i > 0 && list->tasks[i] == NULL; i--) { }
  }
  list->end = i;
}

void ddsrt_tasklist_pack(ddsrt_tasklist_t *list)
{
  size_t i, j;

  /* Pack operation is trickier on wrap around. */
  if (list->end < list->off) {
    /* Compress tail.
     *
     * -------------------------    -----------------------
     * | c . d . e | . a . b . | >> | c d e . . | . a . b |
     * -------------------------    -----------------------
     */
    for (i = j = 0; i <= list->end; i++) {
      if (list->tasks[i] != NULL) {
        if (i != j) {
          list->tasks[j] = list->tasks[i];
        }
        j++;
      }
    }

    assert(j != 0);
    list->end = (j == 0 ? 0 : j - 1);

    /* Compress head.
     *
     * -------------------------    -------------------------
     * | c d e . . | . a . b . | >> | c d e . . | . . . a b |
     * -------------------------    -------------------------
     */
    for (i = j = list->len - 1; i >= list->off; i--) {
      if (list->tasks[i] != NULL) {
        if (i != j) {
          list->tasks[j] = list->tasks[i];
        }
        j--;
      }
    }

    assert(j != list->len - 1);
    list->off = (j == list->len - 1 ? list->len - 1 : j + 1);
  } else {
    /* Compress.
     *
     * -------------------------    --------------------------
     * | . . a . . | b . c d e | >> | a b c d e | . . . . .  |
     * -------------------------    --------------------------
     */
    for (i = list->off, j = 0; i <= list->end; i++) {
      if (list->tasks[i] != NULL) {
        if (i != j) {
          list->tasks[j] = list->tasks[i];
        }
        j++;
      }
    }
    assert(j != 0);
    list->off = 0;
    list->end = j - 1;
    assert(list->end == list->cnt - 1);
  }
}

int ddsrt_tasklist_shrink(ddsrt_tasklist_t *list)
{
  static const size_t x = DDSRT_TASKLIST_CHUNK;
  TaskHandle_t *p;
  size_t mv = 0, n;

  assert(list != NULL);

  /* Shrink by one chunk too, but only if the difference is at least two
     chunks to avoid memory (re)allocation if a task is pushed and popped
     just over the boundary. */
  if (list->cnt > (list->len - (x * 2)) || (list->len - x) < DDSRT_TASKLIST_INITIAL)
  {
    return 0;
  }

  /* List can be sparse. Pack to ensure list can be compacted. */
  ddsrt_tasklist_pack(list);

  /* Pack operation moved head to end of buffer on wrap around. Move head back
     to not discard it on reallocation. */
  if (list->off != 0) {
    assert(list->end < list->off);
    mv = (list->len - list->off) * sizeof(*p);
    memmove(list->tasks + (list->off - x), list->tasks + list->off, mv);
    list->off -= x;
  }

  n = list->len - x;
  if ((p = ddsrt_realloc(list->tasks, n * sizeof(*p))) == NULL) {
    /* Move head back to end of buffer. */
    if (mv != 0) {
      memmove(list->tasks + (list->off + x), list->tasks + list->off, mv);
      list->off += x;
    }
    return -1;
  }

  list->tasks = p;
  list->len = n;

  return 0;
}

int ddsrt_tasklist_grow(ddsrt_tasklist_t *list)
{
  static const size_t x = DDSRT_TASKLIST_CHUNK;
  TaskHandle_t *p;
  size_t n;

  assert(list != NULL);
  /* Should not be called if room is available. */
  assert(list->cnt == list->len);

  n = list->len + x;
  if ((p = ddsrt_realloc(list->tasks, n * sizeof(*p))) == NULL) {
    return -1;
  }

  /* Move head to end of newly allocated memory. */
  if (list->off != 0) {
    assert(list->end < list->off);
    memmove(p + (list->off + x), p + list->off, (list->len - list->off) * sizeof(*p));
    list->off += x;
  }

  /* Zero newly allocated memory. */
  memset(p + (list->end + 1), 0, x * sizeof(*p));

  list->tasks = p;
  list->len = n;

  return 0;
}

ssize_t ddsrt_tasklist_find(ddsrt_tasklist_t *list, TaskHandle_t task)
{
  size_t i, n;

  assert(task != NULL);

  /* No need to check if list is empty. */
  if (list->cnt != 0) {
    /* Task list is circular, so window does not have to be consecutive. */
    n = list->off <= list->end ? list->end : list->len - 1;
    for (i = list->off; i <= n; i++) {
      if (list->tasks[i] == task)
        return (ssize_t)i;
    }

    if (list->off > list->end) {
      n = list->end;
      for (i = 0; i <= n; i++) {
        if (list->tasks[i] == task)
          return (ssize_t)i;
      }
    }
  }

  return -1;
}

TaskHandle_t ddsrt_tasklist_peek(ddsrt_tasklist_t *list, TaskHandle_t task)
{
  tasklist_assert(list);

  if (list->cnt == 0) {
    return NULL;
  } else if (task != NULL) {
    return ddsrt_tasklist_find(list, task) == -1 ? NULL : task;
  }

  return list->tasks[list->off];
}

TaskHandle_t ddsrt_tasklist_pop(ddsrt_tasklist_t *list, TaskHandle_t task)
{
  ssize_t i;

  tasklist_assert(list);

  if (list->cnt == 0) {
    return NULL;
  } else if (task == NULL) {
    i = (ssize_t)list->off;
  } else if ((i = ddsrt_tasklist_find(list, task)) == -1) {
    return NULL;
  }

  task = list->tasks[i];
  if (task != NULL) {
    /* Invalidate bucket. */
    list->tasks[i] = NULL;
    list->cnt--;

    if (list->cnt == 0) {
      list->off = list->end = 0;
    } else if (i == (ssize_t)list->end) {
      /* Trim invalidated buckets from tail of window. */
      ddsrt_tasklist_rtrim(list);
    } else if (i == (ssize_t)list->off) {
      /* Trim invalidated buckets from head of window. */
      ddsrt_tasklist_ltrim(list);
    } else {
      /* Window is now sparse. */
    }

    if (list->cnt <= (list->len - DDSRT_TASKLIST_CHUNK*2)) {
      /* Shrink operation failure can safely be ignored. */
      (void)ddsrt_tasklist_shrink(list);
    }
  }

  return task;
}

int ddsrt_tasklist_push(ddsrt_tasklist_t *list, TaskHandle_t task)
{
  tasklist_assert(list);
  assert(task != NULL);

  /* Ensure task is not listed. */
  if (ddsrt_tasklist_find(list, task) != -1) {
    return 0;
  }
  /* Grow number of buckets if none are available. */
  if (list->cnt == list->len) {
    if (ddsrt_tasklist_grow(list) == -1) {
      return -1;
    }
    list->end++;
  /* Wrap around if there is room at the head. */
  } else if (list->end == list->len - 1 && list->off != 0) {
    list->end = 0;
  } else {
    /* List can be sparse. */
    if (list->end == list->len - 1 || list->end + 1 == list->off) {
      ddsrt_tasklist_pack(list);
    }
    /* Room is guaranteed to be available at the tail. */
    list->end += (list->cnt > 0);
  }

  list->tasks[list->end] = task;
  list->cnt++;

  return 0;
}
