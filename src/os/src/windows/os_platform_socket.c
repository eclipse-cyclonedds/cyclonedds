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
/** \file
 *  \brief WIN32 socket management
 *
 * Implements socket management for WIN32
 */
#include "os/os.h"
#include <Qos2.h>

#include <assert.h>

typedef BOOL (WINAPI *qwaveQOSCreateHandleFuncT) (_In_ PQOS_VERSION Version, _Out_ PHANDLE QOSHandle);
typedef BOOL (WINAPI *qwaveQOSCloseHandleFuncT) (_In_ HANDLE QOSHandle);
typedef BOOL (WINAPI *qwaveQOSAddSocketToFlowFuncT) (
  _In_     HANDLE           QOSHandle,
  _In_     SOCKET           Socket,
  _In_opt_ PSOCKADDR        DestAddr,
  _In_     QOS_TRAFFIC_TYPE TrafficType,
  _In_opt_ DWORD            Flags,
  _Inout_  PQOS_FLOWID      FlowId
);
typedef BOOL (WINAPI *qwaveQOSSetFlowFuncT) (
  _In_       HANDLE       QOSHandle,
  _In_       QOS_FLOWID   FlowId,
  _In_       QOS_SET_FLOW Operation,
  _In_       ULONG        Size,
  _In_       PVOID        Buffer,
  _Reserved_ DWORD        Flags,
  _Out_opt_  LPOVERLAPPED Overlapped
);

static qwaveQOSCreateHandleFuncT qwaveQOSCreateHandleFunc;
static qwaveQOSCloseHandleFuncT qwaveQOSCloseHandleFunc;
static qwaveQOSAddSocketToFlowFuncT qwaveQOSAddSocketToFlowFunc;
static qwaveQOSSetFlowFuncT qwaveQOSSetFlowFunc;

static HANDLE qwaveDLLModuleHandle = NULL;
static HANDLE qwaveDLLModuleLock = NULL;

void
os_socketModuleInit()
{
    WORD wVersionRequested;
    WSADATA wsaData;
    int err;

    wVersionRequested = MAKEWORD (OS_SOCK_VERSION, OS_SOCK_REVISION);

    err = WSAStartup (wVersionRequested, &wsaData);
    if (err != 0) {
        DDS_FATAL("WSAStartup failed, no compatible socket implementation available\n");
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        return;
    }

    /* Confirm that the WinSock DLL supports 2.0.    */
    /* Note that if the DLL supports versions greater    */
    /* than 2.0 in addition to 2.0, it will still return */
    /* 2.0 in wVersion since that is the version we      */
    /* requested.                                        */

    if ((LOBYTE(wsaData.wVersion) != OS_SOCK_VERSION) ||
        (HIBYTE(wsaData.wVersion) != OS_SOCK_REVISION)) {
        /* Tell the user that we could not find a usable */
        /* WinSock DLL.                                  */
        DDS_FATAL("WSAStartup failed, no compatible socket implementation available\n");
        WSACleanup();
        return;
    }

    qwaveDLLModuleLock = CreateMutex(NULL, FALSE, NULL);
    if (qwaveDLLModuleLock == NULL) {
        DDS_ERROR("Failed to create mutex\n");
    }
}

void
os_socketModuleExit(void)
{
    if (qwaveDLLModuleHandle) {
        FreeLibrary(qwaveDLLModuleHandle);
    }

    if (qwaveDLLModuleLock) {
        CloseHandle(qwaveDLLModuleLock);
    }

    WSACleanup();
    return;
}

os_socket
os_sockNew(
    int domain,
    int type)
{
    return socket(domain, type, 0);
}

os_result
os_sockBind(
    os_socket s,
    const struct sockaddr *name,
    uint32_t namelen)
{
    os_result result = os_resultSuccess;

    if (bind(s, (struct sockaddr *)name, namelen) == SOCKET_ERROR) {
        result = os_resultFail;
    }
    return result;
}

os_result
os_sockGetsockname(
    os_socket s,
    const struct sockaddr *name,
        uint32_t namelen)
{
    os_result result = os_resultSuccess;
    int len = namelen;

    if (getsockname(s, (struct sockaddr *)name, &len) == SOCKET_ERROR) {
        result = os_resultFail;
    }
    return result;
}

os_result
os_sockSendto(
    os_socket s,
    const void *msg,
    size_t len,
    const struct sockaddr *to,
    size_t tolen,
    size_t *bytesSent)
{
    int res = sendto(s, msg, (int)len, 0, to, (int)tolen); /* The parameter len with type of size_t causes the possible loss of data. So type casting done */
    if (res < 0)
    {
        *bytesSent = 0;
        return os_resultFail;
    }
    else
    {
        *bytesSent = res;
        return os_resultSuccess;
    }
}

os_result
os_sockRecvfrom(
    os_socket s,
    void *buf,
    size_t len,
    struct sockaddr *from,
    size_t *fromlen,
    size_t *bytesRead)
{
    int res;
    res = recvfrom(s, buf, (int)len, 0, from, (int *)fromlen);
    if (res == SOCKET_ERROR)
    {
        *bytesRead = 0;
        return os_resultFail;
    }
    else
    {
        *bytesRead = (size_t)res;
        return os_resultSuccess;
    }
}

os_result
os_sockGetsockopt(
    os_socket s,
        int32_t level,
        int32_t optname,
    void *optval,
    uint32_t *optlen)
{
    os_result result = os_resultSuccess;

    /* On win32 IP_MULTICAST_TTL and IP_MULTICAST_LOOP take DWORD * param
       rather than char * */
    if ( level == IPPROTO_IP
     && ( optname == IP_MULTICAST_TTL
          || optname == IP_MULTICAST_LOOP ) )
    {
       int dwoptlen = sizeof( DWORD );
       DWORD dwoptval = *((unsigned char *)optval);
       if (getsockopt(s, level, optname, (char *)&dwoptval, &dwoptlen) == SOCKET_ERROR)
       {
          result = os_resultFail;
       }

       assert( dwoptlen == sizeof( DWORD ) );
       *((unsigned char *)optval) = (unsigned char)dwoptval;
       *optlen = sizeof( unsigned char );
    }
    else
    {
       if (getsockopt(s, level, optname, optval, (int *)optlen) == SOCKET_ERROR)
       {
          result = os_resultFail;
       }
    }

    return result;
}


static os_result
os_sockSetDscpValueWithTos(
    os_socket sock,
    DWORD value)
{
    os_result result = os_resultSuccess;

    if (setsockopt(sock, IPPROTO_IP, IP_TOS, (char *)&value, (int)sizeof(value)) == SOCKET_ERROR) {
        char errmsg[1024];
        int errNo = os_getErrno();
        (void) os_strerror_r(errNo, errmsg, sizeof errmsg);
        DDS_WARNING("Failed to set diffserv value to %lu: %d %s\n", value, errNo, errmsg);
        result = os_resultFail;
    }

    return result;
}


static os_result
os_sockLoadQwaveLibrary(void)
{
    if (qwaveDLLModuleLock == NULL) {
        DDS_WARNING("Failed to load QWAVE.DLL for using diffserv on outgoing IP packets\n");
        goto err_lock;
    }

    WaitForSingleObject(qwaveDLLModuleLock, INFINITE);
    if (qwaveDLLModuleHandle == NULL) {
        if ((qwaveDLLModuleHandle = LoadLibrary("QWAVE.DLL")) == NULL) {
            DDS_WARNING("Failed to load QWAVE.DLL for using diffserv on outgoing IP packets\n");
            goto err_load_lib;
        }

        qwaveQOSCreateHandleFunc = (qwaveQOSCreateHandleFuncT) GetProcAddress(qwaveDLLModuleHandle, "QOSCreateHandle");
        qwaveQOSCloseHandleFunc = (qwaveQOSCloseHandleFuncT) GetProcAddress(qwaveDLLModuleHandle, "QOSCloseHandle");
        qwaveQOSAddSocketToFlowFunc = (qwaveQOSAddSocketToFlowFuncT) GetProcAddress(qwaveDLLModuleHandle, "QOSAddSocketToFlow");
        qwaveQOSSetFlowFunc = (qwaveQOSSetFlowFuncT) GetProcAddress(qwaveDLLModuleHandle, "QOSSetFlow");

        if ((qwaveQOSCreateHandleFunc == 0) || (qwaveQOSCloseHandleFunc == 0) ||
            (qwaveQOSAddSocketToFlowFunc == 0) || (qwaveQOSSetFlowFunc == 0)) {
            DDS_WARNING("Failed to resolve entry points for using diffserv on outgoing IP packets\n");
            goto err_find_func;
        }
    }
    ReleaseMutex(qwaveDLLModuleLock);

    return os_resultSuccess;

err_find_func:
err_load_lib:
    ReleaseMutex(qwaveDLLModuleLock);
err_lock:
    return os_resultFail;
}

/* To set the DSCP value on Windows 7 or higher the following functions are used.
 *
 * - BOOL QOSCreateHandle(PQOS_VERSION Version, PHANDLE QOSHandle)
 * - BOOL QOSCloseHandle(HANDLE QOSHandle)
 * - BOOL WINAPI QOSAddSocketToFlow(HANDLE QOSHandle, SOCKET Socket,
 *                                  PSOCKADDR DestAddr, QOS_TRAFFIC_TYPE TrafficType,
 *                                  DWORD Flags, PQOS_FLOWID FlowId)
 * - BOOL WINAPI QOSSetFlow(HANDLE QOSHandle, QOS_FLOWID FlowId,
 *                          QOS_SET_FLOW Operation, ULONG Size, PVOID Buffer,
 *                          DWORD Flags, LPOVERLAPPED Overlapped)
 */


/* QOS_TRAFFIC_TYPE
 *  - QOSTrafficTypeBestEffort      = 0
 *  - QOSTrafficTypeBackground      = 1
 *  - QOSTrafficTypeExcellentEffort = 2
 *  - QOSTrafficTypeAudioVideo      = 3
 *  - QOSTrafficTypeVoice           = 4
 *  - QOSTrafficTypeControl         = 5
 */
#define OS_SOCK_QOS_TRAFFIC_TYPE_BEST_EFFORT      0
#define OS_SOCK_QOS_TRAFFIC_TYPE_BACKGROUND       1
#define OS_SOCK_QOS_TRAFFIC_TYPE_EXCELLENT_EFFORT 2
#define OS_SOCK_QOS_TRAFFIC_TYPE_AUDIO_VIDEO      3
#define OS_SOCK_QOS_TRAFFIC_TYPE_VOICE            4
#define OS_SOCK_QOS_TRAFFIC_TYPE_CONTROL          5

/* Default DSCP values set by QOSAddSocketToFlow
 *  0      : QOSTrafficTypeBestEffort      dscp  0
 *  1 -  8 : QOSTrafficTypeBackground      dscp  8
 *  9 - 40 : QOSTrafficTypeExcellentEffort dscp 40
 * 41 - 55 : QOSTrafficTypeAudioVideo      dscp 40
 * 56      : QOSTrafficTypeVoice           dscp 56
 * 57 - 63 : QOSTrafficTypeControl         dscp 56
 */
#define OS_SOCK_BESTEFFORT_DSCP_VALUE         0
#define OS_SOCK_BACKGROUND_DSCP_VALUE         8
#define OS_SOCK_EXCELLENT_EFFORT_DSCP_VALUE  40
#define OS_SOCK_VOICE_DSCP_VALUE             56

/* QOS_NON_ADAPTIVE_FLOW */
#define OS_SOCK_QOS_NON_ADAPTIVE_FLOW 0x00000002
/* QOSSetOutgoingDSCPValue */
#define OS_SOCK_QOS_SET_OUTGOING_DSCP_VALUE 2

static void
os_sockGetTrafficType(
    DWORD value,
    PLONG trafficType,
    PLONG defaultValue)
{
    if (value == 0) {
        *trafficType = OS_SOCK_QOS_TRAFFIC_TYPE_BEST_EFFORT;
        *defaultValue = OS_SOCK_BESTEFFORT_DSCP_VALUE;
    } else if (value <= OS_SOCK_BACKGROUND_DSCP_VALUE) {
        *trafficType = OS_SOCK_QOS_TRAFFIC_TYPE_BACKGROUND;
        *defaultValue = OS_SOCK_BACKGROUND_DSCP_VALUE;
    } else if (value <= OS_SOCK_EXCELLENT_EFFORT_DSCP_VALUE) {
        *trafficType = OS_SOCK_QOS_TRAFFIC_TYPE_EXCELLENT_EFFORT;
        *defaultValue = OS_SOCK_EXCELLENT_EFFORT_DSCP_VALUE;
    } else if (value < OS_SOCK_VOICE_DSCP_VALUE) {
        *trafficType = OS_SOCK_QOS_TRAFFIC_TYPE_AUDIO_VIDEO;
        *defaultValue = OS_SOCK_EXCELLENT_EFFORT_DSCP_VALUE;
    } else if (value == OS_SOCK_VOICE_DSCP_VALUE) {
        *trafficType = OS_SOCK_QOS_TRAFFIC_TYPE_VOICE;
        *defaultValue = OS_SOCK_VOICE_DSCP_VALUE;
    } else {
        *trafficType = OS_SOCK_QOS_TRAFFIC_TYPE_CONTROL;
        *defaultValue = OS_SOCK_VOICE_DSCP_VALUE;
    }
}


static os_result
os_sockSetDscpValueWithQos(
    os_socket sock,
    DWORD value,
    BOOL setDscpSupported)
{
    os_result result = os_resultSuccess;
    QOS_VERSION version;
    HANDLE qosHandle = NULL;
    ULONG qosFlowId = 0; /* Flow Id must be 0. */
    BOOL qosResult;
    LONG trafficType;
    LONG defaultDscp;
    int errNo;
    SOCKADDR_IN sin;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    /* version must be 1.0 */
    version.MajorVersion = 1;
    version.MinorVersion = 0;

    /* Get a handle to the QoS subsystem. */
    qosResult = qwaveQOSCreateHandleFunc(&version, &qosHandle);
    if (!qosResult) {
        char errmsg[1024];
        errNo = os_getErrno();
        (void)os_strerror_r(errNo, errmsg, sizeof errmsg);
        DDS_ERROR("QOSCreateHandle failed: %d %s\n", errNo, errmsg);
        goto err_create_handle;
    }

    os_sockGetTrafficType(value, &trafficType, &defaultDscp);

    /*  Add socket to flow. */
    qosResult = qwaveQOSAddSocketToFlowFunc(qosHandle, sock, (PSOCKADDR)&sin,
            trafficType, OS_SOCK_QOS_NON_ADAPTIVE_FLOW, &qosFlowId);
    if (!qosResult) {
        char errmsg[1024];
        errNo = os_getErrno();
        (void)os_strerror_r(errNo, errmsg, sizeof errmsg);
        DDS_ERROR("QOSAddSocketToFlow failed: %d %s\n", errNo, errmsg);
        qwaveQOSCloseHandleFunc(qosHandle);
        goto err_add_flow;
    }

    if (value != defaultDscp) {

        if (!setDscpSupported) {
            DDS_WARNING("Failed to set diffserv value to %lu value used is %d, not supported on this platform\n",
                        value, defaultDscp);
            goto err_set_flow;
        }

        /* Try to set DSCP value. Requires administrative rights to succeed */
        qosResult = qwaveQOSSetFlowFunc(qosHandle, qosFlowId, OS_SOCK_QOS_SET_OUTGOING_DSCP_VALUE,
                sizeof(value), &value, 0, NULL);
        if (!qosResult) {
            errNo = os_getErrno();
            if ((errNo == ERROR_ACCESS_DENIED) || (errNo == ERROR_ACCESS_DISABLED_BY_POLICY)) {
                DDS_WARNING("Failed to set diffserv value to %lu value used is %d, not enough privileges\n",
                            value, defaultDscp);
            } else {
                char errmsg[1024];
                errNo = os_getErrno();
                (void)os_strerror_r(errNo, errmsg, sizeof errmsg);
                DDS_ERROR("QOSSetFlow failed: %d %s\n", errNo, errmsg);
            }
            goto err_set_flow;
        }
    }

    return result;

err_set_flow:
err_add_flow:
err_create_handle:
    return result;
}


static os_result
os_sockSetDscpValue(
    os_socket sock,
    DWORD value)
{
    os_result result;

    if (IsWindowsVistaOrGreater() && (os_sockLoadQwaveLibrary() == os_resultSuccess)) {
        result = os_sockSetDscpValueWithQos(sock, value, IsWindows7OrGreater());
    } else {
        result = os_sockSetDscpValueWithTos(sock, value);
    }

    return result;
}


os_result
os_sockSetsockopt(
    os_socket s,
    int32_t level,
    int32_t optname,
    const void *optval,
    uint32_t optlen)
{
    os_result result = os_resultSuccess;
    DWORD dwoptval;

    if ((level == IPPROTO_IP) && (optname == IP_TOS)) {
        dwoptval = *((unsigned char *)optval);
        if (dwoptval != 0) {
            result = os_sockSetDscpValue(s, dwoptval);
        }
    } else if ((optname == IP_MULTICAST_TTL) || (optname == IP_MULTICAST_LOOP)) {
        /* On win32 IP_MULTICAST_TTL and IP_MULTICAST_LOOP take DWORD * param
           rather than char * */
        dwoptval = *((unsigned char *)optval);
        optval = &dwoptval;
        optlen = sizeof( DWORD );
        if (setsockopt(s, level, optname, optval, (int)optlen) == SOCKET_ERROR) {
            result = os_resultFail;
        }
    } else {
        if (setsockopt(s, level, optname, optval, (int)optlen) == SOCKET_ERROR) {
            result = os_resultFail;
        }
    }

    return result;
}

os_result
os_sockSetNonBlocking(
    os_socket s,
    bool nonblock)
{
    int result;
    u_long mode;
    os_result r;

    /* If mode = 0, blocking is enabled,
     * if mode != 0, non-blocking is enabled. */
    mode = nonblock ? 1 : 0;

    result = ioctlsocket(s, FIONBIO, &mode);
    if (result != SOCKET_ERROR){
        r = os_resultSuccess;
    } else {
        switch(os_getErrno()){
            case WSAEINPROGRESS:
                r = os_resultBusy;
                break;
            case WSAENOTSOCK:
                r = os_resultInvalid;
                break;
            case WSANOTINITIALISED:
                DDS_FATAL("Socket-module not initialised; ensure os_socketModuleInit is performed before using the socket module\n");
            default:
                r = os_resultFail;
                break;
        }
    }

    return r;
}

os_result
os_sockFree(
    os_socket s)
{
    os_result result = os_resultSuccess;

    if (closesocket(s) == -1) {
        result = os_resultFail;
    }
    return result;
}

int32_t
os__sockSelect(
    fd_set *readfds,
    fd_set *writefds,
    fd_set *errorfds,
    os_time *timeout)
{
    struct timeval t;
    int r;

    t.tv_sec = (long)timeout->tv_sec;
    t.tv_usec = (long)(timeout->tv_nsec / 1000);
    r = select(-1, readfds, writefds, errorfds, &t);

    return r;
}

ssize_t recvmsg (os_socket fd, struct msghdr *message, int flags)
{
  ssize_t ret;

  assert (message->msg_iovlen == 1);
  assert (message->msg_controllen == 0);

  message->msg_flags = 0;

  ret = recvfrom (fd, message->msg_iov[0].iov_base, (int)message->msg_iov[0].iov_len, flags,
                  message->msg_name, &message->msg_namelen); /* To fix the warning of conversion from 'size_t' to 'int', which may cause possible loss of data, type casting is done*/

  /* Windows returns an error for too-large messages, Unix expects
     original size and the MSG_TRUNC flag.  MSDN says it is truncated,
     which presumably means it returned as much of the message as it
     could - so we return that the message was 1 byte larger than the
     available space, and set MSG_TRUNC if we can. */
  if (ret == -1 && GetLastError () == WSAEMSGSIZE) {
    ret = message->msg_iov[0].iov_len + 1;
    message->msg_flags |= MSG_TRUNC;
  }

  return ret;
}

#define ASSERT_IOVEC_MATCHES_WSABUF do { \
  struct iovec_matches_WSABUF { \
    char sizeof_matches[sizeof(struct os_iovec) == sizeof(WSABUF) ? 1 : -1]; \
    char base_off_matches[offsetof(struct os_iovec, iov_base) == offsetof(WSABUF, buf) ? 1 : -1]; \
    char base_size_matches[sizeof(((struct os_iovec *)8)->iov_base) == sizeof(((WSABUF *)8)->buf) ? 1 : -1]; \
    char len_off_matches[offsetof(struct os_iovec, iov_len) == offsetof(WSABUF, len) ? 1 : -1]; \
    char len_size_matches[sizeof(((struct os_iovec *)8)->iov_len) == sizeof(((WSABUF *)8)->len) ? 1 : -1]; \
  }; } while (0)

ssize_t sendmsg (os_socket fd, const struct msghdr *message, int flags)
{
  DWORD sent;
  ssize_t ret;

  ASSERT_IOVEC_MATCHES_WSABUF;

  assert(message->msg_controllen == 0);

  if (WSASendTo (fd, (WSABUF *) message->msg_iov, (DWORD)message->msg_iovlen, &sent, flags, (SOCKADDR *) message->msg_name, message->msg_namelen, NULL, NULL) == 0)
    ret = (ssize_t) sent;
  else
    ret = -1;
  return ret;
}
