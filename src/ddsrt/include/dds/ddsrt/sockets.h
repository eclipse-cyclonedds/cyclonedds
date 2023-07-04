#ifndef DDSRT_SOCKETS_H
#define DDSRT_SOCKETS_H

#include <stdbool.h>

#include "dds/export.h"
#include "dds/config.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/misc.h"
#if _WIN32
#include "dds/ddsrt/sockets/windows.h"
#else
#include "dds/ddsrt/sockets/posix.h"
#endif

/** @file sockets.h
 *
 * This provides the interface for sockets. Most of the functions declared here, map directly onto the corresponding
 * OS functions. Therefore the nitty gritty details are omitted as they should be available in the OS documentation.
 */

#if defined (__cplusplus)
extern "C" {
#endif

#define INET_ADDRSTRLEN_EXTENDED (INET_ADDRSTRLEN + 6) /* ":12345" */

#if DDSRT_HAVE_IPV6
#define INET6_ADDRSTRLEN_EXTENDED (INET6_ADDRSTRLEN + 8) /* "[]:12345" */
extern const struct in6_addr ddsrt_in6addr_any;
extern const struct in6_addr ddsrt_in6addr_loopback;
#endif /* DDSRT_HAVE_IPV6 */

#define DDSRT_AF_TERM (-1)

#if DDSRT_HAVE_GETHOSTNAME
/**
 * @brief Get the hostname
 *
 * The buffer needs to be large enough to hold the name including null terminator.
 * On success, the buffer will include the null terminator.
 *
 * @param[out] hostname a buffer to copy the name into
 * @param[in] buffersize the space (in bytes) that may be used
 * @return a DDS_RETCODE (OK, ERROR, NOT_ENOUGH_SPACE)
 */
DDS_EXPORT dds_return_t
ddsrt_gethostname(
  char *hostname,
  size_t buffersize);
#endif

/**
 * @brief Creates a socket
 *
 * @param[out] sockptr a pointer to the socket (file descriptor) created
 * @param[in] domain the communication domain, selects the protocol family e.g. PF_INET, PF_PACKET
 * @param[in] type specifies communication semantics e.g. SOCK_STREAM, SOCK_RAW
 * @param[in] protocol specifies a particular protocol to be used with the socket e.g. IPPROTO_TCP, IPPROTO_UDP.
 *            Normally only a single protocol exists to support a particular socket type within a given protocol family,
 *            in which case protocol can be specified as 0.
 * @return a DDS_RETCODE (OK, ERROR, NOT_ALLOWED, BAD_PARAMETER, OUT_OF_RESOURCES)
 *
 * See @ddsrt_bind, @ddsrt_close
 */
dds_return_t
ddsrt_socket(
  ddsrt_socket_t *sockptr,
  int domain,
  int type,
  int protocol);

/**
 * @brief Initialize an "extended socket" based on a normal socket
 *
 * @note The "extended socket" currently only serves as a holder for the WSARecvMsg
 * pointer on Windows.
 *
 * @param[out] sockext the extended socket
 * @param[in] sock the socket (file descriptor) created by @ref ddsrt_socket
 */
void
ddsrt_socket_ext_init(
  ddsrt_socket_ext_t *sockext,
  ddsrt_socket_t sock);

/**
 * @brief Undo the work of @ref ddsrt_socket_ext_init
 *
 * @note Does not close the socket (it is not opened by @ref ddsrt_socket_ext_init, so it
 * is not closed here)
 *
 * @param[in] sockext the extended socket
 */
void
ddsrt_socket_ext_fini(
  ddsrt_socket_ext_t *sockext);

/**
 * @brief Close the socket
 *
 * @param[in,out] sock the socket (file descriptor) created by @ref ddsrt_socket
 * @return a DDS_RETCODE (OK, ERROR, INTERRUPTED, BAD_PARAMETER)
 */
dds_return_t
ddsrt_close(
  ddsrt_socket_t sock);

/**
 * @brief Connects the socket to the address specified by 'addr'.
 *
 * @param[in,out] sock the socket
 * @param[in] addr a socket address
 * @param[in] addrlen the size of 'addr'
 * @return a DDS_RETCODE (OK, ERROR, TIMEOUT, and more)
 *
 * See @ref ddsrt_accept
 */
dds_return_t
ddsrt_connect(
  ddsrt_socket_t sock,
  const struct sockaddr *addr,
  socklen_t addrlen);

/**
 * @brief Accept a connect (@ref ddsrt_connect) request and create a new connected socket for it.
 *
 * Is used with connection-based socket types (SOCK_STREAM, SOCK_SEQPACKET).
 * - 'addrlen' must contain the size of the structure pointed to by 'addr' before the operation,
 *   and afterwards will contain the the size of the peer address.
 * - when not used, 'addr' and 'addrlen' must be NULL
 *
 * @param[in,out] sock socket with which to wait for a connection
 * @param[out] addr address of the connecting peer
 * @param[in,out] addrlen the size (in bytes) of the structure pointed to by 'addr'
 * @param[out] connptr pointer to the new connected socket.
 * @return a DDS_RETCODE (OK, ERROR, and more)
 *
 * See @ref ddsrt_bind, ddsrt_listen
 */
dds_return_t
ddsrt_accept(
  ddsrt_socket_t sock,
  struct sockaddr *addr,
  socklen_t *addrlen,
  ddsrt_socket_t *connptr);

/**
 * @brief Marks the socket referred to by 'sock' as a passive socket.
 *
 * A passive socket will be used to accept incoming connection requests using @ref ddsrt_accept.
 *
 * @param[in,out] sock file descriptor of the socket
 * @param[in] backlog maximum number of pending connections
 * @return a DDS_RETCODE (OK, ERROR, PRECONDITION_NOT_MET, BAD_PARAMETER, ILLEGAL_OPERATION)
 */
dds_return_t
ddsrt_listen(
  ddsrt_socket_t sock,
  int backlog);

/**
 * @brief Assign an address to the socket.
 *
 * When a socket is created with @ref ddsrt_socket, it exists in a name space (address family) but has no address assigned to it.
 * @ref ddsrt_bind assigns the address specified by 'addr' to the socket referred to by the file descriptor 'sock'.
 *
 * @param[in,out] sock the socket
 * @param[in] addr address to assign to the socket
 * @param[in] addrlen specifies the size, in bytes, of the address structure pointed to by 'addr'
 * @return a DDS_RETCODE (OK, ERROR, NOT_ALLOWED, PRECONDITION_NOT_MET, BAD_PARAMETER)
 */
dds_return_t
ddsrt_bind(
  ddsrt_socket_t sock,
  const struct sockaddr *addr,
  socklen_t addrlen);

/**
 * @brief Get the current address to which the socket is bound (@ref ddsrt_bind).
 *
 * The 'addrlen' argument should be initialized to indicate the amount of space (in bytes)
 * pointed to by 'addr'. On return it contains the actual size of the socket address.
 * The returned address is truncated if the buffer provided is too small; in this case, 'addrlen' will return a value greater than was supplied to the call.
 *
 * @param[in] sock the socket
 * @param[out] addr the address of the socket
 * @param[in,out] addrlen specifies the size, in bytes, of the address structure pointed to by 'addr'
 * @return a DDS_RETCODE (OK, ERROR, BAD_PARAMETER, OUT_OF_RESOURCES)
 */
dds_return_t
ddsrt_getsockname(
  ddsrt_socket_t sock,
  struct sockaddr *addr,
  socklen_t *addrlen);

/**
 * @brief Send data from a buffer
 *
 * - Sends 'len' bytes of 'buf'
 * - The 'flags' can be 0, or the bitwise OR of one or more of:
 *   {MSG_CONFIRM, MSG_DONTROUTE, MSG_DONTWAIT, MSG_EOR, MSG_MORE, MSG_NOSIGNAL, MSG_OOB}
 *
 * @param[in] sock the socket
 * @param[in] buf buffer containing the data to send
 * @param[in] len size (in bytes) of 'buf'
 * @param[in] flags flags for special options
 * @param[out] sent the number of bytes sent
 * @return a DDS_RETCODE (OK, ERROR, and more)
 *
 * See @ref ddsrt_recv
 */
dds_return_t
ddsrt_send(
  ddsrt_socket_t sock,
  const void *buf,
  size_t len,
  int flags,
  ssize_t *sent);

/**
 * @brief Send a message
 *
 * - The 'flags' can be 0, or the bitwise OR of one or more of:
 *   {MSG_CONFIRM, MSG_DONTROUTE, MSG_DONTWAIT, MSG_EOR, MSG_MORE, MSG_NOSIGNAL, MSG_OOB}
 *
 * @param[in] sock the socket
 * @param[in] msg the message to send
 * @param[in] flags flags for special options
 * @param[out] sent the number of bytes sent
 * @return a DDS_RETCODE (OK, ERROR, and more)
 *
 * See @ref ddsrt_recvmsg
 */
dds_return_t
ddsrt_sendmsg(
  ddsrt_socket_t sock,
  const ddsrt_msghdr_t *msg,
  int flags,
  ssize_t *sent);

/**
 * @brief Receive data into a buffer
 *
 * - If a message is too long to fit in the supplied buffer, excess bytes may be discarded depending on
 *   the type of socket the message is received from.
 * - If no data is available at the socket, the call waits for a message to arrive, unless the socket is nonblocking (MSG_DONTWAIT).
 * - The 'flags' can be 0, or the bitwise OR of one or more of:
 *   {MSG_CMSG_CLOEXEC, MSG_DONTWAIT, MSG_ERRQUEUE, MSG_OOB, MSG_PEEK, MSG_TRUNC, MSG_WAITALL}
 *
 * @param[in] sock the socket
 * @param[out] buf buffer in which to receive the data
 * @param[in] len the size available in the buffer
 * @param[in] flags flags for special options
 * @param[out] rcvd number of bytes received
 * @return a DDS_RETCODE (OK, ERROR, TRY_AGAIN, BAD_PARAMETER, NO_CONNECTION, INTERRUPTED, OUT_OF_RESOURCES, ILLEGAL_OPERATION)
 *
 * See @ref ddsrt_send
 */
dds_return_t
ddsrt_recv(
  ddsrt_socket_t sock,
  void *buf,
  size_t len,
  int flags,
  ssize_t *rcvd);

/**
 * @brief Receive a message
 *
 * - If a message is too long to fit in the supplied buffer, excess bytes may be discarded depending on
 *   the type of socket the message is received from.
 * - If no data is available at the socket, the call waits for a message to arrive, unless the socket is nonblocking (MSG_DONTWAIT).
 * - The 'flags' can be 0, or the bitwise OR of one or more of:
 *   {MSG_CMSG_CLOEXEC, MSG_DONTWAIT, MSG_ERRQUEUE, MSG_OOB, MSG_PEEK, MSG_TRUNC, MSG_WAITALL}
 *
 * @param[in] sockext the socket
 * @param[out] msg the message received
 * @param[in] flags flags for special options
 * @param[out] rcvd number of bytes received
 * @return a DDS_RETCODE (OK, ERROR, TRY_AGAIN, BAD_PARAMETER, NO_CONNECTION, INTERRUPTED, OUT_OF_RESOURCES, ILLEGAL_OPERATION)
 *
 * See @ref ddsrt_sendmsg
 */
dds_return_t
ddsrt_recvmsg(
  const ddsrt_socket_ext_t *sockext,
  ddsrt_msghdr_t *msg,
  int flags,
  ssize_t *rcvd);

/**
 * @brief Get options from the socket.
 *
 * Argument 'optlen' is a value-result argument, initially containing the size
 * of the buffer pointed to by 'optval', and modified on return to indicate the
 * actual size of the value returned.
 *
 * @param[in] sock the socket
 * @param[in] level the level at which the option resides. For socket API use SOL_SOCKET
 * @param[in] optname the name of the option (SO_REUSEADDR, SO_DONTROUTE, SO_BROADCAST, SO_SNDBUF, SO_RCVBUF, ...)
 * @param[out] optval buffer into which to receive the option value
 * @param[in,out] optlen size of buffer 'optval'
 * @return a DDS_RETCODE (OK, ERROR, BAD_PARAMETER, UNSUPPORTED)
 *
 * See @ref ddsrt_setsockopt
 */
dds_return_t
ddsrt_getsockopt(
  ddsrt_socket_t sock,
  int32_t level,
  int32_t optname,
  void *optval,
  socklen_t *optlen);

/**
 * @brief Set options on the socket
 *
 * Most socket-level options utilize an int argument for 'optval'.
 * The argument should be nonzero to enable a boolean option,
 * or zero if the option is to be disabled.
 *
 * @param[in,out] sock the socket
 * @param[in] level the level at which the option resides. For socket API use SOL_SOCKET
 * @param[in] optname the name of the option (SO_REUSEADDR, SO_DONTROUTE, SO_BROADCAST, SO_SNDBUF, SO_RCVBUF, ...)
 * @param[in] optval buffer containing the option value
 * @param[in] optlen size of buffer 'optval'
 * @return a DDS_RETCODE (OK, ERROR, BAD_PARAMETER, UNSUPPORTED)
 *
 * See @ref ddsrt_getsockopt
 */
dds_return_t
ddsrt_setsockopt(
  ddsrt_socket_t sock,
  int32_t level,
  int32_t optname,
  const void *optval,
  socklen_t optlen);

/**
 * @brief Set the I/O on the socket to blocking or non-nonblocking.
 *
 * @param[in]  sock      Socket to set I/O mode for.
 * @param[in]  nonblock  true for nonblocking, or false for blocking I/O.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             I/O mode successfully set to (non)blocking.
 * @retval DDS_RETCODE_TRY_AGAIN
 *             A blocking call is in progress.
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             @sock is not a valid socket.
 * @retval DDS_RETCODE_ERROR
 *             An unknown error error occurred.
 */
dds_return_t
ddsrt_setsocknonblocking(
  ddsrt_socket_t sock,
  bool nonblock);

/**
 * @brief Set whether a port may be shared with other sockets
 *
 * Maps to SO_REUSEPORT (if defined) followed by SO_REUSEADDR
 *
 * @param[in]   sock  Socket to set reusability for.
 * @param[in]   reuse Whether to allow sharing the address/port.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             SO_REUSEPORT successfully set, or not defined,
 *             or returned ENOPROTOOPT
 *             SO_REUSEADDR successfully set
 * @retval DDS_RETCODE_UNSUPPORTED
 *             Network stack doesn't support SO_REUSEADDR and
 *             returned ENOPROTOOPT
 * @retval DDS_RETCODE_BAD_PARAMETER
 *             Bad things happened (e.g., not a socket)
 * @retval DDS_RETCODE_ERROR
 *             An unknown error occurred.
 */
dds_return_t
ddsrt_setsockreuse(
  ddsrt_socket_t sock,
  bool reuse);

/**
 * @brief Monitor multiple sockets, waiting until one or more become ready.
 *
 * @param[in]  nfds      Highest-numbered file descriptor in any of the sets.
 * @param[in]  readfds   Set of sockets to monitor for read ready status.
 * @param[in]  writefds  Set of sockets to monitor for write ready status.
 * @param[in]  errorfds  Set of sockets to monitor for exceptional conditions.
 * @param[in]  reltime   Interval to block for sockets to become ready.
 *
 * @returns The number of sockets ready in the sets or a return code.
 */
dds_return_t
ddsrt_select(
  int32_t nfds,
  fd_set *readfds,
  fd_set *writefds,
  fd_set *errorfds,
  dds_duration_t reltime);

#if _WIN32
/* SOCKETs on Windows are NOT integers. The nfds parameter is only there for
   compatibility, the implementation ignores it. Implicit casts will generate
   warnings though, therefore ddsrt_select is redefined to discard the
   parameter on Windows. */
#define ddsrt_select(nfds, readfds, writefds, errorfds, timeout) \
    ddsrt_select(-1, readfds, writefds, errorfds, timeout)
#endif /* _WIN32 */

/**
 * @brief Get the size of a socket address.
 *
 * @param[in]  sa  Socket address to return the size for.
 *
 * @returns Size of the socket address based on the address family, or 0 if
 *          the address family is unknown.
 */
socklen_t
ddsrt_sockaddr_get_size(
  const struct sockaddr *const sa) ddsrt_nonnull_all;

/**
 * @brief Get the port number from a socket address.
 *
 * @param[in]  sa  Socket address to retrieve the port from.
 *
 * @return Port number in host order.
 */
uint16_t
ddsrt_sockaddr_get_port(
  const struct sockaddr *const sa) ddsrt_nonnull_all;

/**
 * @brief Check if the given address is unspecified.
 *
 * @param[in]  sa  Socket address to check.
 *
 * @return true if the address is unspecified, false otherwise.
 */
bool
ddsrt_sockaddr_isunspecified(
  const struct sockaddr *__restrict sa) ddsrt_nonnull_all;

/**
 * @brief Check if the given address is a loopback address.
 *
 * @param[in]  sa  Socket address to check.
 *
 * @return true if the address is a loopback address, false otherwise.
 */
bool
ddsrt_sockaddr_isloopback(
  const struct sockaddr *__restrict sa) ddsrt_nonnull_all;

/**
 * @brief Check if given socket IP addresses reside in the same subnet.
 *
 * Checks if two socket IP addresses reside in the same subnet, considering the
 * given subnetmask. IPv6 mapped IPv4 addresses are not taken in account.
 *
 * @param[in]  sa1   First address
 * @param[in]  sa2   Second address.
 * @param[in]  mask  Subnetmask.
 *
 * @returns true if both addresses reside in the same subnet, false otherwise.
 */
bool
ddsrt_sockaddr_insamesubnet(
  const struct sockaddr *sa1,
  const struct sockaddr *sa2,
  const struct sockaddr *mask)
ddsrt_nonnull_all;

/**
 * @brief Convert a string to a socket address
 *
 * The socket address 'sa' can be any of type:
 * (struct sockaddr_in*, struct sockaddr_in6*, struct sockaddr_storage*)
 * Note that the data is copied into the existing socket address (does not allocate memory).
 *
 * @param[in] af the address family (AF_INET, AF_INET6)
 * @param[in] str the string input e.g. "192.0.2.0"
 * @param[out] sa the socket address to overwrite
 * @return a DDS_RETCODE (OK, BAD_PARAMETER)
 *
 * See @ref ddsrt_sockaddrtostr
 */
dds_return_t
ddsrt_sockaddrfromstr(
  int af, const char *str, void *sa);

/**
 * @brief Convert a socket address to a string
 *
 * The socket address 'sa' can be any of type:
 * (const struct sockaddr_in*, const struct sockaddr_in6*, const struct sockaddr_storage*)
 *
 * @param[in] sa the socket address to convert into a string
 * @param[out] buf a string buffer for the output
 * @param[in] size the size (in bytes) of 'buf'
 * @return a DDS_RETCODE (OK, BAD_PARAMETER, NOT_ENOUGH_SPACE)
 *
 * See @ref ddsrt_sockaddrfromstr
 */
dds_return_t
ddsrt_sockaddrtostr(
  const void *sa, char *buf, size_t size);

#if DDSRT_HAVE_DNS
DDSRT_WARNING_MSVC_OFF(4200)
/**
 * @brief A vector of socket addresses
 */
typedef struct {
  size_t naddrs; ///< Number of addresses
  struct sockaddr_storage addrs[]; ///< Array containing socket addresses
} ddsrt_hostent_t;
DDSRT_WARNING_MSVC_ON(4200)

/**
 * @brief Lookup addresses for given host name.
 *
 * @param[in]   name  Host name to resolve.
 * @param[in]   af    Address family, either AF_INET, AF_INET6 or AF_UNSPEC.
 * @param[out]  hentp Structure of type ddsrt_hostent_t.
 *
 * @returns A dds_return_t indicating success or failure.
 *
 * @retval DDS_RETCODE_OK
 *             Host name successfully resolved to address(es).
 * @retval DDS_RETCODE_HOST_NOT_FOUND
 *             Host not found.
 * @retval DDS_RETCODE_NO_DATA
 *             Valid name, no data record of requested type.
 * @retval DDS_RETCODE_ERROR
 *             Nonrecoverable error.
 * @retval DDS_RETCODE_TRY_AGAIN
 *             Nonauthoratitative host not found.
 */
dds_return_t
ddsrt_gethostbyname(
  const char *name,
  int af,
  ddsrt_hostent_t **hentp);
#endif

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_SOCKETS_H */
