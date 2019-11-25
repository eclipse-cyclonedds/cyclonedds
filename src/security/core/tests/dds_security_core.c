/*
 * Copyright(c) 2019 ADLINK Technology Limited and others
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
#include "CUnit/Test.h"

#include "dds/security/core/dds_security_timed_cb.h"
#include "dds/ddsrt/misc.h"

#define SEQ_SIZE (16)

typedef struct {
    struct dds_security_timed_dispatcher_t *d;
    dds_security_timed_cb_kind kind;
    void *listener;
    void *arg;
    dds_time_t time;
} tc__sequence_data;

static int g_sequence_idx = 0;
static tc__sequence_data g_sequence_array[SEQ_SIZE];

static void simple_callback(struct dds_security_timed_dispatcher_t *d,
        dds_security_timed_cb_kind kind,
        void *listener,
        void *arg)
{
    DDSRT_UNUSED_ARG(d);
    DDSRT_UNUSED_ARG(kind);
    DDSRT_UNUSED_ARG(listener);

    if (*((bool *)arg) == false)
    {
        *((bool *)arg) = true;
    }
    else
    {
        *((bool *)arg) = false;
    }
}

static int g_order_callback_idx = 0;
static void* g_order_callback[2] = {(void*)NULL, (void*)NULL};
static void order_callback(struct dds_security_timed_dispatcher_t *d,
        dds_security_timed_cb_kind kind,
        void *listener,
        void *arg)
{
    DDSRT_UNUSED_ARG(d);
    DDSRT_UNUSED_ARG(kind);
    DDSRT_UNUSED_ARG(listener);

    g_order_callback[g_order_callback_idx] = arg;
    g_order_callback_idx++;
}

static void
tc__callback(
        struct dds_security_timed_dispatcher_t *d,
        dds_security_timed_cb_kind kind,
        void *listener,
        void *arg)
{
    if (g_sequence_idx < SEQ_SIZE) {
        g_sequence_array[g_sequence_idx].d = d;
        g_sequence_array[g_sequence_idx].arg = arg;
        g_sequence_array[g_sequence_idx].kind = kind;
        g_sequence_array[g_sequence_idx].listener = listener;
        g_sequence_array[g_sequence_idx].time = dds_time();
    }
    g_sequence_idx++;
}

CU_Test(dds_security_timed_cb, simple_test)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    static bool test_var = false;

    dds_time_t now     = dds_time();
    dds_time_t future  = now + DDS_SECS(2);

    d1 = dds_security_timed_dispatcher_new(tcb);

    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);

    dds_security_timed_dispatcher_add(tcb, d1, simple_callback, future, (void*)&test_var);

    dds_security_timed_dispatcher_enable(tcb, d1, (void*)NULL);

    CU_ASSERT_FALSE_FATAL(test_var);

    dds_sleepfor(DDS_MSECS(500));

    CU_ASSERT_FALSE_FATAL(test_var);

    dds_sleepfor(DDS_SECS(2));

    CU_ASSERT_TRUE_FATAL(test_var);

    dds_security_timed_dispatcher_free(tcb, d1);

    dds_security_timed_cb_free(tcb);
}

CU_Test(dds_security_timed_cb, simple_order)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    dds_time_t future;
    dds_time_t future2;

    d1 = dds_security_timed_dispatcher_new(tcb);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
    future  = dds_time() + DDS_MSECS(20);
    future2  = future;
    dds_security_timed_dispatcher_add(tcb, d1, order_callback, future, (void*)1);
    dds_security_timed_dispatcher_add(tcb, d1, order_callback, future2, (void*)2);
    dds_security_timed_dispatcher_enable(tcb, d1, (void*)&g_order_callback);
    dds_sleepfor(DDS_MSECS(10));

    dds_security_timed_dispatcher_free(tcb, d1);

    CU_ASSERT_EQUAL_FATAL(g_order_callback[0], (void*)1);
    CU_ASSERT_EQUAL_FATAL(g_order_callback[1], (void*)2);

    dds_security_timed_cb_free(tcb);
}

CU_Test(dds_security_timed_cb, test_enabled_and_disabled)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    static bool test_var = false;

    dds_time_t now     = dds_time();
    dds_time_t future  = now + DDS_SECS(2);

    d1 = dds_security_timed_dispatcher_new(tcb);

    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);

    dds_security_timed_dispatcher_add(tcb, d1, simple_callback, future, (void*)&test_var);

    dds_security_timed_dispatcher_enable(tcb, d1, (void*)NULL);

    CU_ASSERT_FALSE(test_var);

    dds_security_timed_dispatcher_disable(tcb, d1);

    dds_sleepfor(DDS_MSECS(500));

    CU_ASSERT_FALSE(test_var);

    dds_sleepfor(DDS_SECS(2));

    CU_ASSERT_FALSE(test_var);

    dds_security_timed_dispatcher_free(tcb, d1);

    dds_security_timed_cb_free(tcb);
}

CU_Test(dds_security_timed_cb, simple_test_with_future)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    static bool test_var = false;

    dds_time_t now     = dds_time();
    dds_time_t future  = now + DDS_SECS(2);
    dds_time_t far_future  = now + DDS_SECS(10);

    d1 = dds_security_timed_dispatcher_new(tcb);

    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);

    dds_security_timed_dispatcher_enable(tcb, d1, (void*)NULL);

    dds_security_timed_dispatcher_add(tcb, d1, simple_callback, future, (void*)&test_var);
    dds_security_timed_dispatcher_add(tcb, d1, simple_callback, far_future, (void*)&test_var);

    CU_ASSERT_FALSE_FATAL(test_var);

    dds_sleepfor(DDS_MSECS(500));

    CU_ASSERT_FALSE_FATAL(test_var);

    dds_sleepfor(DDS_SECS(2));

    CU_ASSERT_TRUE_FATAL(test_var);

    dds_security_timed_dispatcher_free(tcb, d1);

    dds_security_timed_cb_free(tcb);
}

CU_Test(dds_security_timed_cb, test_multiple_dispatchers)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    struct dds_security_timed_dispatcher_t *d2 = NULL;
    static bool test_var = false;

    dds_time_t now     = dds_time();
    dds_time_t future  = now + DDS_SECS(2);
    dds_time_t far_future  = now + DDS_SECS(10);

    d1 = dds_security_timed_dispatcher_new(tcb);
    d2 = dds_security_timed_dispatcher_new(tcb);

    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);

    dds_security_timed_dispatcher_enable(tcb, d1, (void*)NULL);
    dds_security_timed_dispatcher_enable(tcb, d2, (void*)NULL);

    dds_security_timed_dispatcher_free(tcb, d2);

    dds_security_timed_dispatcher_add(tcb, d1, simple_callback, future, (void*)&test_var);
    dds_security_timed_dispatcher_add(tcb, d1, simple_callback, far_future, (void*)&test_var);

    CU_ASSERT_FALSE_FATAL(test_var);

    dds_sleepfor(DDS_MSECS(500));

    CU_ASSERT_FALSE_FATAL(test_var);

    dds_sleepfor(DDS_SECS(2));

    CU_ASSERT_TRUE_FATAL(test_var);

    dds_security_timed_dispatcher_free(tcb, d1);

    dds_security_timed_cb_free(tcb);
}

CU_Test(dds_security_timed_cb, test_not_enabled_multiple_dispatchers)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    struct dds_security_timed_dispatcher_t *d2 = NULL;

    d1 = dds_security_timed_dispatcher_new(tcb);
    d2 = dds_security_timed_dispatcher_new(tcb);

    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d2);

    dds_security_timed_dispatcher_free(tcb, d2);

    dds_security_timed_dispatcher_free(tcb, d1);

    dds_security_timed_cb_free(tcb);

    CU_PASS("Timed callbacks enabled and disabled without add");
}

CU_Test(dds_security_timed_cb, test_create_dispatcher)
{
    struct dds_security_timed_cb_data *tcb = dds_security_timed_cb_new();
    struct dds_security_timed_dispatcher_t *d1 = NULL;
    struct dds_security_timed_dispatcher_t *d2 = NULL;
    struct dds_security_timed_dispatcher_t *d3 = NULL;
    struct dds_security_timed_dispatcher_t *d4 = NULL;
    struct dds_security_timed_dispatcher_t *d5 = NULL;
    bool ok = false;

    dds_time_t now     = dds_time();
    dds_time_t past    = now - DDS_SECS(1);
    dds_time_t present = now + DDS_SECS(1);
    dds_time_t future  = present + DDS_SECS(1);
    dds_time_t future2 = future + DDS_SECS(10);

    /*************************************************************************
     * Check if dispatchers can be created
     *************************************************************************/
    d1 = dds_security_timed_dispatcher_new(tcb);
    d2 = dds_security_timed_dispatcher_new(tcb);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d1);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d2);

    /*************************************************************************
     * Check if adding callbacks succeeds
     *************************************************************************/
    
    /* The last argument is a sequence number in which
     * the callbacks are expected to be called. */
    /* We can only really check if it crashes or not... */
    dds_security_timed_dispatcher_add(tcb, d1, tc__callback, present, (void*)1);
    dds_security_timed_dispatcher_add(tcb, d2, tc__callback, past,    (void*)0);
    dds_security_timed_dispatcher_add(tcb, d2, tc__callback, present, (void*)2);
    dds_security_timed_dispatcher_add(tcb, d1, tc__callback, future,  (void*)7);
        
    CU_PASS("Added callbacks")

    /*************************************************************************
     * Check if dispatchers can be created
     *************************************************************************/
    d3 = dds_security_timed_dispatcher_new(tcb);
    d4 = dds_security_timed_dispatcher_new(tcb);
    d5 = dds_security_timed_dispatcher_new(tcb);
    
    CU_ASSERT_PTR_NOT_NULL_FATAL(d3);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d4);
    CU_ASSERT_PTR_NOT_NULL_FATAL(d5);


    /*************************************************************************
     * Check if enabling dispatchers succeeds
     *************************************************************************/

    /* The sleeps are added to get the timing between
        * 'present' and 'past' callbacks right. */
    /* We can only really check if it crashes or not... */
    dds_sleepfor(DDS_MSECS(600));
    dds_security_timed_dispatcher_enable(tcb, d1, (void*)NULL);
    dds_security_timed_dispatcher_enable(tcb, d2, (void*)  d2);
    dds_security_timed_dispatcher_enable(tcb, d3, (void*)NULL);
    /* Specifically not enabling d4 and d5. */
    dds_sleepfor(DDS_MSECS(600));
    CU_PASS("Enabled dds_security_timed_dispatchers.");

    /*************************************************************************
     * Check if adding callbacks succeeds
     *************************************************************************/

    /* The last argument is a sequence number in which
        * the callbacks are expected to be called. */
    /* We can only really check if it crashes or not... */
    dds_security_timed_dispatcher_add(tcb, d4, tc__callback, past,    (void*)99);
    dds_security_timed_dispatcher_add(tcb, d2, tc__callback, future,  (void*) 8);
    dds_security_timed_dispatcher_add(tcb, d3, tc__callback, future2, (void*) 9);
    dds_security_timed_dispatcher_add(tcb, d1, tc__callback, past,    (void*) 3);
    dds_security_timed_dispatcher_add(tcb, d1, tc__callback, future2, (void*)10);
    dds_security_timed_dispatcher_add(tcb, d1, tc__callback, present, (void*) 4);
    dds_security_timed_dispatcher_add(tcb, d2, tc__callback, present, (void*) 5);
    dds_security_timed_dispatcher_add(tcb, d1, tc__callback, future,  (void*) 6);
    dds_security_timed_dispatcher_add(tcb, d3, tc__callback, future2, (void*)11);
    CU_PASS("Added callbacks.");


    /*************************************************************************
     * Check if timeout callbacks are triggered in the right sequence
     *************************************************************************/
    
    int idx;
    int timeout = 200; /* 2 seconds */

    /* Wait for the callbacks to have been triggered.
        * Ignore the ones in the far future. */
    while ((g_sequence_idx < 8) && (timeout > 0)) {
        dds_sleepfor(DDS_MSECS(10));
        timeout--;
    }

    /* Print and check sequence of triggered callbacks. */
    for (idx = 0; (idx < g_sequence_idx) && (idx < SEQ_SIZE); idx++) {
        int seq = (int)(long long)(g_sequence_array[idx].arg);
        struct dds_security_timed_dispatcher_t *expected_d;
        void *expected_l;

        /*
            * Sequence checks.
            */
        if ((seq == 1) || (seq == 6) || (seq == 3) || (seq == 10) || (seq == 4) || (seq == 7)) {
            expected_d = d1;
            expected_l = NULL;
        } else if ((seq == 0) || (seq == 2) || (seq == 8) || (seq == 5)) {
            expected_d = d2;
            expected_l = d2;
        } else if (seq == 9) {
            expected_d = d3;
            expected_l = NULL;
        } else if (seq == 99) {
            expected_d = d4;
            expected_l = NULL;
            CU_FAIL("Unexpected callback on a disabled dispatcher");
            ok = false;
        } else {
            expected_d = NULL;
            expected_l = NULL;
            CU_FAIL(sprintf("Unknown sequence idx received %d", seq));
            ok = false;
        }

        if (seq != idx) {
            /* 6 and 7 order may be mixed since the order is not defined for same time stamp */
            if (!((seq == 6 && idx == 7) || (seq == 7 && idx == 6)))
            {
                printf("Unexpected sequence ordering %d vs %d\n", seq, idx);
                CU_FAIL("Unexpected sequence ordering");
                ok = false;
            }
        }
        if (seq > 8) {
            CU_FAIL(sprintf("Unexpected sequence idx %d of the far future", seq));
            ok = false;
        }
        if (idx > 8) {
            CU_FAIL(sprintf("Too many callbacks %d", idx));
            ok = false;
        }

        /*
         * Callback contents checks.
         */
        if (expected_d != NULL) {
            if (g_sequence_array[idx].d != expected_d) {
                printf("Unexpected dispatcher %p vs %p\n", g_sequence_array[idx].d, expected_d);
                CU_FAIL("Unexpected dispatcher");
                ok = false;
            }
            if (g_sequence_array[idx].listener != expected_l) {
                CU_FAIL(sprintf("Unexpected listener %p vs %p", g_sequence_array[idx].listener, expected_l));
                ok = false;
            }
        }

        /*
         * Callback kind check.
         */
        if (g_sequence_array[idx].kind != DDS_SECURITY_TIMED_CB_KIND_TIMEOUT) {
            CU_FAIL(sprintf("Unexpected kind %d vs %d", (int)g_sequence_array[idx].kind, (int)DDS_SECURITY_TIMED_CB_KIND_TIMEOUT));
            ok = false;
        }
    }
    if (g_sequence_idx < 8) {
        CU_FAIL(sprintf("Received %d callbacks, while 9 are expected",
                                g_sequence_idx + 1));
        ok = false;
    }
    if (ok) {
        CU_FAIL(sprintf("Received timeout callbacks."));
    }

    /* Reset callback index to catch the deletion ones. */
    g_sequence_idx = 0;


    /*************************************************************************
     * Check if deleting succeeds with dispatchers in different states
     *************************************************************************/
    /* We can only really check if it crashes or not... */
    if (d1) {
        dds_security_timed_dispatcher_free(tcb, d1);
    }
    if (d2) {
        dds_security_timed_dispatcher_free(tcb, d2);
    }
    if (d3) {
        dds_security_timed_dispatcher_free(tcb, d3);
    }
    if (d4) {
        dds_security_timed_dispatcher_free(tcb, d4);
    }
    if (d5) {
        dds_security_timed_dispatcher_free(tcb, d5);
    }
    CU_PASS("Deleted dispatchers.");


    /*************************************************************************
     * Check if deletion callbacks are triggered
     *************************************************************************/
    if (ok) {
        timeout = 200; /* 2 seconds */

        /* Wait for the callbacks to have been triggered.
         * Ignore the ones in the far future. */
        while ((g_sequence_idx < 4) && (timeout > 0)) {
            dds_sleepfor(DDS_MSECS(10));
            timeout--;
        }

        /* Print and check sequence of triggered callbacks. */
        for (idx = 0; (idx < g_sequence_idx) && (idx < SEQ_SIZE); idx++) {
            int seq = (int)(long long)(g_sequence_array[idx].arg);
            struct dds_security_timed_dispatcher_t *expected_d;

            /*
             * Human (somewhat) readable format.
             */
            // tc__sequence_data_print(&(g_sequence_array[idx]));

            /*
             * Sequence checks.
             */
            if (seq == 99) {
                expected_d = d4;
            } else if ((seq == 9) || (seq == 11)) {
                expected_d = d3;
            } else if (seq == 10) {
                expected_d = d1;
            } else {
                expected_d = NULL;
                CU_FAIL(sprintf("Unexpected sequence idx received %d", seq));
                ok = false;
            }
            if (idx > 4) {
                CU_FAIL(sprintf("Too many callbacks %d", idx));
                ok = false;
            }

            /*
             * Callback contents checks.
             */
            if (expected_d != NULL) {
                if (g_sequence_array[idx].d != expected_d) {
                    CU_FAIL(sprintf("Unexpected dispatcher %p vs %p", g_sequence_array[idx].d, expected_d));
                    ok = false;
                }
                if (g_sequence_array[idx].listener != NULL) {
                    CU_FAIL(sprintf("Unexpected listener %p vs NULL", g_sequence_array[idx].listener));
                    ok = false;
                }
            }

            /*
             * Callback kind check.
             */
            if (g_sequence_array[idx].kind != DDS_SECURITY_TIMED_CB_KIND_DELETE) {
                CU_FAIL(sprintf("Unexpected kind %d vs %d", (int)g_sequence_array[idx].kind, (int)DDS_SECURITY_TIMED_CB_KIND_TIMEOUT));
                ok = false;
            }
        }
        if (g_sequence_idx < 4) {
            CU_FAIL(sprintf("Received %d callbacks, while 3 are expected",
                                    g_sequence_idx + 1));
            ok = false;
        }
        if (ok) {
            CU_PASS("Received deletion callbacks.");
        }
    }

    dds_security_timed_cb_free(tcb);
}
