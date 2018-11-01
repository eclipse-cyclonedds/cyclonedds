/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include "CUnit/Test.h"
#include "os/os.h"
#include "assert.h"

static int32_t one = 1;
static int32_t two = 2;
static int32_t three = 3;
static int32_t four = 4;
static int32_t five = 5;

CU_Test(os_iter, create)
{
    os_iter *iter;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    CU_ASSERT_EQUAL(os_iterLength(iter), 0);
    CU_ASSERT_PTR_NULL(os_iterObject(iter, 0));
    CU_ASSERT_PTR_NULL(os_iterTake(iter, 0));
    os_iterFree(iter, NULL);
}

CU_Test(os_iter, prepend)
{
    os_iter *iter;
    int32_t idx;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    idx = os_iterInsert(iter, &one, 0);
    CU_ASSERT_EQUAL(idx, 0);
    idx = os_iterInsert(iter, &two, 0);
    CU_ASSERT_EQUAL(idx, 0);
    idx = os_iterInsert(iter, &three, 0);
    CU_ASSERT_EQUAL(idx, 0);
    CU_ASSERT_EQUAL(os_iterLength(iter), 3);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 0), 3);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 1), 2);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 2), 1);
    os_iterFree(iter, NULL);
}

CU_Test(os_iter, append)
{
    os_iter *iter;
    int32_t idx;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    idx = os_iterInsert(iter, &one, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL(idx, 0);
    idx = os_iterInsert(iter, &two, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL(idx, 1);
    idx = os_iterInsert(iter, &three, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL(idx, 2);
    CU_ASSERT_EQUAL(os_iterLength(iter), 3);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 0), 1);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 1), 2);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 2), 3);
    os_iterFree(iter, NULL);
}

CU_Test(os_iter, insert)
{
    os_iter *iter;
    int32_t idx;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    idx = os_iterInsert(iter, &one, 0);
    CU_ASSERT_EQUAL(idx, 0);
    idx = os_iterInsert(iter, &three, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL(idx, 1);
    idx = os_iterInsert(iter, &two, idx);
    CU_ASSERT_EQUAL(idx, 1);
    idx = os_iterInsert(iter, &four, -2);
    CU_ASSERT_EQUAL(idx, 1);
    idx = os_iterInsert(iter, &five, -2);
    CU_ASSERT_EQUAL(idx, 2);
    CU_ASSERT_EQUAL(os_iterLength(iter), 5);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 0), 1);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 1), 4);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 2), 5);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 3), 2);
    CU_ASSERT_EQUAL(*(int32_t *)os_iterObject(iter, 4), 3);
    os_iterFree(iter, NULL);
}

static void
iter_free_callback(
    void *ptr)
{
    (*(int32_t *)ptr)++;
}

CU_Test(os_iter, free)
{
    os_iter *iter;
    int32_t cnt = 0;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    (void)os_iterInsert(iter, &cnt, OS_ITER_LENGTH);
    (void)os_iterInsert(iter, &cnt, OS_ITER_LENGTH);
    (void)os_iterInsert(iter, &cnt, OS_ITER_LENGTH);
    os_iterFree(iter, &iter_free_callback);
    CU_ASSERT_EQUAL(cnt, 3);
}

static void
iter_walk_callback(
    void *ptr, void *arg)
{
    (*(int32_t *)ptr)++;
    (*(int32_t *)arg)++;
}

CU_Test(os_iter, walk)
{
    os_iter *iter;
    int32_t cnt = 0;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    (void)os_iterInsert(iter, &cnt, OS_ITER_LENGTH);
    (void)os_iterInsert(iter, &cnt, OS_ITER_LENGTH);
    (void)os_iterInsert(iter, &cnt, OS_ITER_LENGTH);
    os_iterWalk(iter, &iter_walk_callback, &cnt);
    CU_ASSERT_EQUAL(cnt, 6);
    os_iterFree(iter, &iter_free_callback);
    CU_ASSERT_EQUAL(cnt, 9);
}

static os_iter *
iter_new(
    void)
{
    os_iter *iter;
    int32_t idx;

    iter = os_iterNew();
    CU_ASSERT_PTR_NOT_NULL_FATAL(iter);
    idx = os_iterInsert(iter, &one, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL_FATAL(idx, 0);
    idx = os_iterInsert(iter, &two, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL_FATAL(idx, 1);
    idx = os_iterInsert(iter, &three, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL_FATAL(idx, 2);
    idx = os_iterInsert(iter, &four, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL_FATAL(idx, 3);
    idx = os_iterInsert(iter, &five, OS_ITER_LENGTH);
    CU_ASSERT_EQUAL_FATAL(idx, 4);
    CU_ASSERT_EQUAL_FATAL(os_iterLength(iter), 5);

    return iter;
}

CU_Test(os_iter, object_indices)
{
    os_iter *iter;
    int32_t *num;

    iter = iter_new();

    /* index out of range on purpose */
    OS_WARNING_MSVC_OFF(28020);
    num = os_iterObject(iter, OS_ITER_LENGTH);
    OS_WARNING_MSVC_ON(28020);

    CU_ASSERT_PTR_NULL(num);
    num = os_iterObject(iter, (int32_t)os_iterLength(iter));
    CU_ASSERT_PTR_NULL(num);
    num = os_iterObject(iter, -6);
    CU_ASSERT_PTR_NULL(num);
    num = os_iterObject(iter, 0);
    CU_ASSERT_PTR_EQUAL(num, &one);
    num = os_iterObject(iter, -5);
    CU_ASSERT_PTR_EQUAL(num, &one);
    num = os_iterObject(iter, (int32_t)os_iterLength(iter) - 1);
    CU_ASSERT_PTR_EQUAL(num, &five);
    num = os_iterObject(iter, -1);
    CU_ASSERT_PTR_EQUAL(num, &five);
    num = os_iterObject(iter, 2);
    CU_ASSERT_PTR_EQUAL(num, &three);
    num = os_iterObject(iter, -3);
    CU_ASSERT_PTR_EQUAL(num, &three);
    os_iterFree(iter, NULL);
}

CU_Test(os_iter, take_indices)
{
    os_iter *iter;
    int32_t *num, cnt = 0;

    iter = iter_new();

    /* index out of range on purpose */
    OS_WARNING_MSVC_OFF(28020);
    num = os_iterTake(iter, OS_ITER_LENGTH);
    OS_WARNING_MSVC_ON(28020);
    CU_ASSERT_PTR_NULL(num);
    num = os_iterTake(iter, (int32_t)os_iterLength(iter));
    CU_ASSERT_PTR_NULL(num);
    num = os_iterTake(iter, -6);
    CU_ASSERT_PTR_NULL(num);
    num = os_iterTake(iter, -5);
    CU_ASSERT_PTR_EQUAL(num, &one);
    CU_ASSERT_EQUAL(os_iterLength(iter), 4);
    num = os_iterTake(iter, -3);
    CU_ASSERT_PTR_EQUAL(num, &three);
    CU_ASSERT_EQUAL(os_iterLength(iter), 3);
    num = os_iterTake(iter, -1);
    CU_ASSERT_PTR_EQUAL(num, &five);
    CU_ASSERT_EQUAL(os_iterLength(iter), 2);
    num = os_iterTake(iter, 1);
    CU_ASSERT_PTR_EQUAL(num, &four);
    CU_ASSERT_EQUAL(os_iterLength(iter), 1);
    num = os_iterTake(iter, 1);
    CU_ASSERT_PTR_NULL(num);
    num = os_iterTake(iter, -2);
    CU_ASSERT_PTR_NULL(num);
    num = os_iterTake(iter, -1);
    CU_ASSERT_PTR_EQUAL(num, &two);
    CU_ASSERT_EQUAL(os_iterLength(iter), 0);
    os_iterWalk(iter, &iter_walk_callback, &cnt);
    CU_ASSERT_EQUAL(cnt, 0);
    os_iterFree(iter, NULL);
}
