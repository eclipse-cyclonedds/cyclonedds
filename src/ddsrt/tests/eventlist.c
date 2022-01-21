/*
 * Copyright(c) 2022 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <stdio.h>

#include "event.h"

#include "CUnit/Test.h"

static void assert_list(ddsrt_eventlist_t *list)
{
  ddsrt_event_t **events;
  static const size_t embedded =
    sizeof(list->events.embedded)/sizeof(list->events.embedded[0]);

  CU_ASSERT_FATAL(list->count <= list->length);
  CU_ASSERT_FATAL((list->count <= 1) == (list->start == list->end));
  if (list->count <= embedded) {
    CU_ASSERT_EQUAL_FATAL(list->length, embedded);
    events = get_events(list);
    CU_ASSERT_PTR_EQUAL_FATAL(events, list->events.embedded);
  } else {
    CU_ASSERT_EQUAL_FATAL(list->length % embedded, 0);
    events = get_events(list);
    CU_ASSERT_PTR_EQUAL_FATAL(events, list->events.dynamic);
  }
}

static void create_list(ddsrt_eventlist_t *list)
{
  create_eventlist(list);
  assert_list(list);
}

static void destroy_list(ddsrt_eventlist_t *list)
{
  assert_list(list);
  destroy_eventlist(list);
}

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

CU_Test(ddsrt_event, empty_list_noops)
{
  ddsrt_event_t **events;
  ddsrt_eventlist_t list;
  static const size_t embedded = ARRAY_SIZE(list.events.embedded);

  create_list(&list);
  // ensure pack is a noop
  pack_eventlist(&list);
  assert_list(&list);
  // ensure left_trim is a noop
  left_trim(&list);
  assert_list(&list);
  // ensure right_trim is a noop
  right_trim(&list);
  assert_list(&list);

  events = get_events(&list);
  for (size_t i=0; i < embedded; i++) {
    CU_ASSERT_PTR_NULL(events[i]);
  }

  destroy_list(&list);
}

static ddsrt_event_t *fill(ddsrt_eventlist_t *list, int length, int shift)
{
  dds_return_t ret;
  assert(length >= 0);
  ddsrt_event_t *events = ddsrt_malloc((size_t)length * sizeof(*events));

  CU_ASSERT_PTR_NOT_NULL_FATAL(events);
  memset(events, 0, (size_t)length * sizeof(*events));
  for (int i=0; i < length; i++) {
    ret = add_event(list, &events[i], INT_MAX);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    assert_list(list);
  }

  CU_ASSERT_EQUAL_FATAL(list->length, length);
  CU_ASSERT_EQUAL_FATAL(list->count, length);
  CU_ASSERT_EQUAL_FATAL(list->start, 0);
  CU_ASSERT_EQUAL_FATAL(list->end, length - 1);

  if (shift > 0) {
    ddsrt_event_t **buf = ddsrt_malloc((size_t)length * sizeof(*buf));
    ddsrt_event_t **ptr = get_events(list);
    CU_ASSERT_PTR_NOT_NULL_FATAL(buf);
    memmove(buf, ptr + (length - shift), (size_t)shift * sizeof(*buf));
    memmove(buf + shift, ptr, (size_t)(length - shift) * sizeof(*buf));
    memmove(ptr, buf, (size_t)length * sizeof(*buf));
    list->start += (size_t)shift;
    list->end = (size_t)shift - 1;
    ddsrt_free(buf);
  }

  return events;
}

#define EMBEDDED (DDSRT_EMBEDDED_EVENTS)
#define DYNAMIC (EMBEDDED*2)

CU_Test(ddsrt_eventlist, resize)
{
  ddsrt_event_t events[EMBEDDED*4];
  ddsrt_eventlist_t list;
  int count = 0;

  create_list(&list);

  // grow one past embedded. buffer should grow by embedded
  for (; count < EMBEDDED+1; count++)
    add_event(&list, &events[count], INT_MAX);
  assert_list(&list);
  CU_ASSERT_EQUAL_FATAL(list.length, (EMBEDDED*2));
  CU_ASSERT_EQUAL_FATAL(list.count, count);
  CU_ASSERT_EQUAL_FATAL(list.start, 0);
  CU_ASSERT_EQUAL_FATAL(list.end, EMBEDDED);

  // grow one past embedded*3.0 buffer should grow to embedded*4.0
  for (; count < (EMBEDDED*3)+1; count++)
    add_event(&list, &events[count], INT_MAX);
  assert_list(&list);
  CU_ASSERT_EQUAL_FATAL(list.length, (EMBEDDED*4));
  CU_ASSERT_EQUAL_FATAL(list.count, count);
  CU_ASSERT_EQUAL_FATAL(list.start, 0);
  CU_ASSERT_EQUAL_FATAL(list.end, (EMBEDDED*3));

  static const struct {
    int count;
    int expected;
  } shrink[] = {
    // shrink to embedded*3.0. buffer should not shrink
    { (EMBEDDED*3), (EMBEDDED*4) },
    // shrink to embedded*2.0 - 1. buffer should shrink to embedded*2.0
    { (EMBEDDED*2) - 1, (EMBEDDED*2) },
    // shrink to embedded. buffer should shrink to embedded
    { EMBEDDED, EMBEDDED },
    // shrink to embedded - 1. buffer should shrink to embedded
    { EMBEDDED - 1, EMBEDDED }
  };

  for (size_t i=0, n=sizeof(shrink)/sizeof(shrink[0]); i < n; i++) {
    for (; count > shrink[i].count; )
      delete_event(&list, &events[--count]);
    assert_list(&list);
    CU_ASSERT_EQUAL(list.length, shrink[i].expected);
  }

  destroy_list(&list);
}

CU_Test(ddsrt_eventlist, resize_wrapped)
{
  ddsrt_event_t *events;
  ddsrt_eventlist_t list;
  int count = (EMBEDDED*4);

  create_list(&list);
  events = fill(&list, count, 5);
  assert_list(&list);

  // FIXME: the fill function requires us to fill to a multiple of EMBEDDED
  for (int n=(EMBEDDED*3)+1; count > n; )
    delete_event(&list, &events[--count]);

  static const struct {
    int count;
    int expected;
  } shrink[] = {
    // shrink to embedded*3.0. buffer should not shrink
    { (EMBEDDED*3), (EMBEDDED*4) },
    // shrink to embedded*2.0 - 1. buffer should shrink to embedded*2.0
    { (EMBEDDED*2) - 1, (EMBEDDED*2) },
    // shrink to embedded. buffer should shrink to embedded
    { EMBEDDED, EMBEDDED },
    // shrink to embedded - 1. buffer should shrink to embedded
    { EMBEDDED - 1, EMBEDDED }
  };

  for (size_t i=0, n=sizeof(shrink)/sizeof(shrink[0]); i < n; i++) {
    for (; count > shrink[i].count; ) {
      dds_return_t ret = delete_event(&list, &events[--count]);
      CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
    }
    //assert_list(&list);
    CU_ASSERT_EQUAL(list.length, shrink[i].expected);
  }

  ddsrt_free(events);
  destroy_list(&list);
}

CU_Test(ddsrt_eventlist, remove_from_start)
{
  static const struct {
    int length;
    int start;
    int end;
    int remove;
  } tests[] = {
    // embedded list
    { EMBEDDED, 0, EMBEDDED - 1, 1 },
    { EMBEDDED, 0, EMBEDDED - 1, 2 },
    // embedded list with wrap around
    { EMBEDDED, 2, 1, 1 },
    { EMBEDDED, 2, 1, 2 },
    { EMBEDDED, 2, 1, 3 },
    // dynamic list
    { DYNAMIC, 0, DYNAMIC - 1, 1 },
    { DYNAMIC, 0, DYNAMIC - 1, 2 },
    // dynamic list with wrap around
    { DYNAMIC, 2, 1, 1 },
    { DYNAMIC, 2, 1, 2 },
    { DYNAMIC, 2, 1, 3 },
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++) {
    ddsrt_event_t *events;
    ddsrt_eventlist_t list;
    int count;

    create_list(&list);
    events = fill(&list, tests[i].length, tests[i].start);
    CU_ASSERT_PTR_NOT_NULL_FATAL(events);

    CU_ASSERT_EQUAL_FATAL(list.start, tests[i].start);
    CU_ASSERT_EQUAL_FATAL(list.end, tests[i].end);
    count = (int)list.count;
    for (int delete=tests[i].remove; delete > 0; ) {
      count--;
      delete_event(&list, &events[ --delete ]);
      assert_list(&list);
      CU_ASSERT_EQUAL_FATAL(list.length, tests[i].length);
      CU_ASSERT_EQUAL_FATAL(list.count, count);
      if (delete > 0)
        CU_ASSERT_EQUAL_FATAL(list.start, tests[i].start);
    }
    if (tests[i].start >= tests[i].length - tests[i].remove) {
      int start = tests[i].start - (tests[i].length - tests[i].remove);
      CU_ASSERT_EQUAL_FATAL(list.start, start);
    } else {
      int start = tests[i].start + tests[i].remove;
      CU_ASSERT_EQUAL_FATAL(list.start, start);
    }
    for (int add = 0; add < tests[i].remove; add++) {
      add_event(&list, &events[ add ], INT_MAX);
      assert_list(&list);
      CU_ASSERT_EQUAL_FATAL(list.count, (tests[i].length - (tests[i].remove - (add + 1))));
    }
    if (tests[i].end >= tests[i].length - tests[i].remove) {
      int end = tests[i].end - (tests[i].length - tests[i].remove);
      CU_ASSERT_EQUAL(list.end, end);
    } else {
      int end = tests[i].end + tests[i].remove;
      CU_ASSERT_EQUAL(list.end, end);
    }
    ddsrt_free(events);
    destroy_list(&list);
  }
}

CU_Test(ddsrt_eventlist, remove_from_end)
{
  static const struct {
    int length;
    int shift;
    int end;
    int remove;
  } tests[] = {
    // embedded list
    { EMBEDDED, 0, EMBEDDED - 1, -1 },
    { EMBEDDED, 0, EMBEDDED - 1, -2 },
    // embedded list with wrap around
    { EMBEDDED, 2, 1, -1 },
    { EMBEDDED, 2, 1, -2 },
    { EMBEDDED, 2, 1, -3 },
    // dynamic list
    { DYNAMIC, 0, DYNAMIC - 1, -1 },
    { DYNAMIC, 0, DYNAMIC - 1, -2 },
    // dynamic list with wrap around
    { DYNAMIC, 2, 1, -1 },
    { DYNAMIC, 2, 1, -2 },
    { DYNAMIC, 2, 1, -3 },
  };

  for (size_t i=0, n=sizeof(tests)/sizeof(tests[0]); i < n; i++)
  {
    ddsrt_event_t *events;
    ddsrt_eventlist_t list;
    size_t cnt;

    create_list(&list);
    events = fill(&list, tests[i].length, tests[i].shift);
    CU_ASSERT_PTR_NOT_NULL_FATAL(events);
    cnt = list.count;

    CU_ASSERT_EQUAL_FATAL(list.start, tests[i].shift);
    CU_ASSERT_EQUAL_FATAL(list.end, tests[i].end);
    for (int del=tests[i].remove; del < 0; del++) {
      cnt--;
      delete_event(&list, &events[ tests[i].length + del ]);
      assert_list(&list);
      CU_ASSERT_EQUAL_FATAL(list.length, tests[i].length);
      CU_ASSERT_EQUAL_FATAL(list.count, cnt);
      if (del < -1)
        CU_ASSERT_EQUAL_FATAL(list.end, tests[i].end);
    }
    if (tests[i].end < (tests[i].remove * -1)) {
      int del = tests[i].remove + tests[i].end;
      CU_ASSERT_EQUAL_FATAL(list.end, (tests[i].length + del));
    } else {
      int del = tests[i].remove;
      CU_ASSERT_EQUAL_FATAL(list.end, (tests[i].end + del));
    }
    for (int add=tests[i].remove; add < 0; add++) {
      add_event(&list, &events[ tests[i].length + add], INT_MAX);
      assert_list(&list);
      CU_ASSERT_EQUAL_FATAL(list.length, tests[i].length);
    }
    CU_ASSERT_EQUAL_FATAL(list.end, tests[i].end);
    ddsrt_free(events);
    destroy_list(&list);
  }
}
