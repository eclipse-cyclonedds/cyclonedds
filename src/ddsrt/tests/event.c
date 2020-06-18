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

#include <stdio.h>
#include "CUnit/Test.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/event.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/threads.h"

#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>

long int make_pipe(ddsrt_socket_t tomake[2]) {
	struct sockaddr_in addr;
	socklen_t asize = sizeof(addr);
	ddsrt_socket_t listener = socket(AF_INET, SOCK_STREAM, 0);
	ddsrt_socket_t s1 = socket(AF_INET, SOCK_STREAM, 0);
	ddsrt_socket_t s2 = DDSRT_INVALID_SOCKET;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = 0;
	if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
		getsockname(listener, (struct sockaddr*)&addr, &asize) == -1 ||
		listen(listener, 1) == -1 ||
		connect(s1, (struct sockaddr*)&addr, sizeof(addr)) == -1 ||
		(s2 = accept(listener, 0, 0)) == -1) {
		closesocket(listener);
		closesocket(s1);
		closesocket(s2);
		return -1;
	}
	closesocket(listener);
	SetHandleInformation((HANDLE)s1, HANDLE_FLAG_INHERIT, 0);
	SetHandleInformation((HANDLE)s2, HANDLE_FLAG_INHERIT, 0);
	tomake[0] = s1;
	tomake[1] = s2;
	return 0;
}

void close_pipe(ddsrt_socket_t toclose[2]) {
	closesocket(toclose[0]);
	closesocket(toclose[1]);
}

long int push_pipe(ddsrt_socket_t *p) {
	char dummy = 0x0;
	return send(p[1], &dummy, sizeof(dummy), 0);
}

long int pull_pipe(ddsrt_socket_t *p) {
	char buf = 0x0;
	return recv(p[0], &buf, sizeof(buf), 0);
}

void ddsrt_sleep(int microsecs) {
	Sleep(microsecs/1000);
}
#else
#include <sys/select.h>
#include <unistd.h>

long int make_pipe(ddsrt_socket_t tomake[2]) {
	return pipe(tomake);
}

void close_pipe(ddsrt_socket_t toclose[2]) {
	close(toclose[0]);
	close(toclose[1]);
}

long int push_pipe(ddsrt_socket_t *p) {
	char dummy = 0x0;
	return write(p[1], &dummy, sizeof(dummy));
}

long int pull_pipe(ddsrt_socket_t *p) {
	char buf = 0x0;
	return read(p[0], &buf, sizeof(buf));
}

void ddsrt_sleep(int microsecs) {
	usleep(microsecs);
}
#endif


CU_Init(ddsrt_event) {
	ddsrt_init();
	return 0;
}

CU_Clean(ddsrt_event) {
	ddsrt_fini();
	return 0;
}

CU_Test(ddsrt_event, evt_create) {
	int fd = 123456;
	struct ddsrt_event* ptr1 = ddsrt_event_create(MONITORABLE_UNSET, (void*)0x0, sizeof(long int), EVENT_UNSET),
	* ptr2 = ddsrt_event_create(MONITORABLE_FILE, &fd, sizeof(fd), EVENT_CONNECT);

	CU_ASSERT_EQUAL_FATAL(ptr1->mon_type, MONITORABLE_UNSET);
	CU_ASSERT_EQUAL_FATAL(ptr1->mon_sz, sizeof(long int));
	CU_ASSERT_EQUAL_FATAL(ptr1->evt_type, EVENT_UNSET);
	long int t = 0x0;
	CU_ASSERT_EQUAL_FATAL(*(long int*)ptr1->mon_ptr, t);

	CU_ASSERT_EQUAL_FATAL(ptr2->mon_type, MONITORABLE_FILE);
	CU_ASSERT_EQUAL_FATAL(ptr2->mon_sz, sizeof(fd));
	CU_ASSERT_EQUAL_FATAL(ptr2->evt_type, EVENT_CONNECT);
	CU_ASSERT_EQUAL_FATAL(*(int*)ptr2->mon_ptr, fd);

	ddsrt_event_destroy(ptr1);
	ddsrt_event_destroy(ptr2);
	CU_PASS("evt_create");
}

CU_Test(ddsrt_event, evt_implicit) {
	long long int fd1 = 123456;
	int fd2 = 654321;

	struct ddsrt_event* ptr1 = ddsrt_event_create_val(MONITORABLE_PIPE, fd1, EVENT_CONNECT),
	* ptr2 = ddsrt_event_create_val(MONITORABLE_SOCKET, fd2, EVENT_DISCONNECT);
	
	CU_ASSERT_EQUAL_FATAL(ptr1->mon_type, MONITORABLE_PIPE);
	CU_ASSERT_EQUAL_FATAL(ptr1->mon_sz, sizeof(long long int));
	CU_ASSERT_EQUAL_FATAL(ptr1->evt_type, EVENT_CONNECT);
	CU_ASSERT_EQUAL_FATAL(*(long int*)ptr1->mon_ptr, fd1);
	

	CU_ASSERT_EQUAL_FATAL(ptr2->mon_type, MONITORABLE_SOCKET);
	CU_ASSERT_EQUAL_FATAL(ptr2->mon_sz, sizeof(int));
	CU_ASSERT_EQUAL_FATAL(ptr2->evt_type, EVENT_DISCONNECT);
	CU_ASSERT_EQUAL_FATAL(*(int*)ptr2->mon_ptr, fd2);

	ddsrt_event_destroy(ptr1);
	ddsrt_event_destroy(ptr2);

	CU_PASS("evt_implicit");
}

CU_Test(ddsrt_event, evt_copy) {
	long long int fd1 = 123456;
	int fd2 = 654321;

	struct ddsrt_event* ptr1 = ddsrt_event_create_val(MONITORABLE_PIPE, fd1, EVENT_CONNECT),
	* ptr2 = ddsrt_event_create_val(MONITORABLE_SOCKET, fd2, EVENT_DISCONNECT);

	ddsrt_event_copy(ptr2, ptr1);

	CU_ASSERT_EQUAL_FATAL(ptr1->mon_type, ptr2->mon_type);
	CU_ASSERT_EQUAL_FATAL(ptr1->mon_sz, ptr2->mon_sz);
	CU_ASSERT_EQUAL_FATAL(ptr1->evt_type, ptr2->evt_type);
	CU_ASSERT_EQUAL_FATAL(memcmp(ptr1->mon_ptr,ptr2->mon_ptr,ptr1->mon_sz), 0);

	ddsrt_event_destroy(ptr1);
	ddsrt_event_destroy(ptr2);

	CU_PASS("evt_copy");
}

CU_Test(ddsrt_event, monitor_register) {
	const int startsize = 8;
	struct ddsrt_monitor* mon_nonfixedsize = ddsrt_monitor_create(startsize, 0), /*monitor of non-fixed size*/
	* mon_fixedsize = ddsrt_monitor_create(startsize, 1); /*monitor of fixed size*/
	for (int i = 0; i < startsize + 1; i++) {
		struct ddsrt_event* evt = ddsrt_event_create_val(MONITORABLE_PIPE, i, EVENT_CONNECT);

		/*writing triggers to monitorables*/
		int nfx = ddsrt_monitor_register_trigger(mon_nonfixedsize, evt),
			fx = ddsrt_monitor_register_trigger(mon_fixedsize, evt);
		CU_ASSERT(nfx == i+1);
		if (i < startsize) {
			CU_ASSERT(fx == i + 1);
		}
		else {
			CU_ASSERT(fx == -1);
		}

		/*adding to existing monitorables*/
		evt->evt_type = EVENT_DISCONNECT;
		nfx = ddsrt_monitor_register_trigger(mon_nonfixedsize, evt);
		fx = ddsrt_monitor_register_trigger(mon_fixedsize, evt);
		CU_ASSERT(nfx == i + 1);
		if (i < startsize) {
			CU_ASSERT(fx == i + 1);
		}
		else {
			CU_ASSERT(fx == -1);
		}

		ddsrt_event_destroy(evt);
	}

	for (int i = 0; i < startsize + 1; i++) {
		struct ddsrt_event* evt = ddsrt_event_create_val(MONITORABLE_PIPE, i, EVENT_DISCONNECT | EVENT_CONNECT);

		/*removing from monitorables*/
		int nfx = ddsrt_monitor_deregister_trigger(mon_nonfixedsize, evt),
			fx = ddsrt_monitor_deregister_trigger(mon_fixedsize, evt);
		CU_ASSERT(nfx == startsize - i);
		if (i < startsize) {
			CU_ASSERT(fx == startsize - i - 1);
		}
		else {
			CU_ASSERT(fx == 0);
		}

		ddsrt_event_destroy(evt);
	}

	ddsrt_monitor_destroy(mon_nonfixedsize);
	ddsrt_monitor_destroy(mon_fixedsize);
	
	CU_PASS("monitor_register");
}

static uint32_t wait_func(void* arg) {
	printf("starting wait for event\n");
	ddsrt_monitor_start_wait((ddsrt_monitor*)arg, 6000);
	printf("done with wait for event\n");
	return 0;
}

static uint32_t write_func(void* arg) {
	ddsrt_socket_t *p = (ddsrt_socket_t*)arg;
	printf("starting wait for send to %d\n",p[1]);
	ddsrt_sleep(250000);
	printf("sending to %d\n", p[1]);
	push_pipe(p);
	printf("done with send\n");
	return 0;
}

static uint32_t interrupt_func(void* arg) {
	printf("starting wait for interrupt\n");
	ddsrt_sleep(125000);
	printf("interrupting\n");
	ddsrt_monitor_interrupt_wait((ddsrt_monitor*)arg);
	printf("done with interrupt\n");
	return 0;
}

CU_Test(ddsrt_event, monitor_trigger) {
	ddsrt_socket_t p[2];
	CU_ASSERT_EQUAL_FATAL(make_pipe(p),0);

	ddsrt_monitor *mon = ddsrt_monitor_create(128, 0);

	struct ddsrt_event* evtin = ddsrt_event_create_val(MONITORABLE_SOCKET, p[0], EVENT_DATA_IN);
	ddsrt_monitor_register_trigger(mon, evtin);
	
	dds_return_t ret1 = DDS_RETCODE_OK, ret2 = DDS_RETCODE_OK;
	ddsrt_thread_t thr1, thr2;
	ddsrt_threadattr_t attr;
	uint32_t res1 = 0, res2 = 0;
	
	ddsrt_threadattr_init(&attr);
	attr.schedClass = DDSRT_SCHED_DEFAULT;
	attr.schedPriority = 0;
	
	ret1 = ddsrt_thread_create(&thr1, "reader", &attr, &wait_func, mon);
	ret2 = ddsrt_thread_create(&thr2, "writer", &attr, &write_func, p); 
	
	if (ret1 == DDS_RETCODE_OK) {
		ret1 = ddsrt_thread_join(thr1, &res1);
		CU_ASSERT_EQUAL(ret1, DDS_RETCODE_OK);
	}

	if (ret2 == DDS_RETCODE_OK) {
		ret2 = ddsrt_thread_join(thr2, &res2);
		CU_ASSERT_EQUAL(ret2, DDS_RETCODE_OK);
	}

	/*check for for data_in event*/
	struct ddsrt_event* evtout = ddsrt_monitor_pop_event(mon);
	CU_ASSERT_PTR_NOT_EQUAL_FATAL(evtout, NULL);
	CU_ASSERT_EQUAL(evtout->mon_type, evtin->mon_type);
	CU_ASSERT_EQUAL(evtout->mon_sz, evtin->mon_sz);
	CU_ASSERT(0 == memcmp(evtout->mon_ptr, evtin->mon_ptr, evtin->mon_sz));
	CU_ASSERT_EQUAL(evtout->evt_type, evtin->evt_type);

	evtout = ddsrt_monitor_pop_event(mon);
	CU_ASSERT_EQUAL_FATAL(evtout, NULL);

	ddsrt_monitor_destroy(mon);
	close_pipe(p);

	CU_PASS("monitor_trigger");
}

CU_Test(ddsrt_event, monitor_interrupt) {
	ddsrt_socket_t p[2];
	CU_ASSERT_EQUAL_FATAL(make_pipe(p), 0);

	ddsrt_monitor* mon = ddsrt_monitor_create(128, 0);

	struct ddsrt_event* evtin = ddsrt_event_create_val(MONITORABLE_SOCKET, p[0], EVENT_DATA_IN);
	ddsrt_monitor_register_trigger(mon, evtin);

	dds_return_t ret1 = DDS_RETCODE_OK, ret2 = DDS_RETCODE_OK, ret3 = DDS_RETCODE_OK;
	ddsrt_thread_t thr1, thr2, thr3;
	ddsrt_threadattr_t attr;
	uint32_t res1 = 0, res2 = 0, res3 = 0;

	ddsrt_threadattr_init(&attr);
	attr.schedClass = DDSRT_SCHED_DEFAULT;
	attr.schedPriority = 0;

	ret1 = ddsrt_thread_create(&thr1, "reader", &attr, &wait_func, mon);
	ret2 = ddsrt_thread_create(&thr2, "writer", &attr, &write_func, p);
	ret3 = ddsrt_thread_create(&thr3, "interrupter", &attr, &interrupt_func, mon);

	if (ret1 == DDS_RETCODE_OK) {
		ret1 = ddsrt_thread_join(thr1, &res1);
		CU_ASSERT_EQUAL(ret1, DDS_RETCODE_OK);
	}

	if (ret2 == DDS_RETCODE_OK) {
		ret2 = ddsrt_thread_join(thr2, &res2);
		CU_ASSERT_EQUAL(ret2, DDS_RETCODE_OK);
	}

	if (ret3 == DDS_RETCODE_OK) {
		ret3 = ddsrt_thread_join(thr3, &res3);
		CU_ASSERT_EQUAL(ret3, DDS_RETCODE_OK);
	}

	/*check for for data_in event*/
	struct ddsrt_event* evtout = ddsrt_monitor_pop_event(mon);
	CU_ASSERT_EQUAL_FATAL(evtout, NULL);

	ddsrt_monitor_destroy(mon);
	close_pipe(p);

	CU_PASS("monitor_interrupt");
}