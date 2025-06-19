/*
 * Fake netinet/in.h for Windows PostgreSQL extensions
 * This header provides Windows-compatible definitions for networking
 * constants and types that PostgreSQL expects from netinet/in.h
 */

#ifndef _NETINET_IN_H_
#define _NETINET_IN_H_

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

/* Network address types */
typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

/* Network byte order conversion (already in winsock2.h but ensure they're defined) */
#ifndef htons
#define htons(x) htons(x)
#endif
#ifndef ntohs  
#define ntohs(x) ntohs(x)
#endif
#ifndef htonl
#define htonl(x) htonl(x)
#endif
#ifndef ntohl
#define ntohl(x) ntohl(x)
#endif

/* Standard network addresses */
#ifndef INADDR_ANY
#define INADDR_ANY 0x00000000
#endif
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif
#ifndef INADDR_BROADCAST
#define INADDR_BROADCAST 0xffffffff
#endif
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

/* Address string lengths */
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 46
#endif

/* Protocol numbers (from netinet/in.h on Unix) */
#ifndef IPPROTO_IP
#define IPPROTO_IP 0
#endif
#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP
#define IPPROTO_UDP 17
#endif

/* sockaddr_in is already defined in winsock2.h */

#endif /* _WIN32 */

#endif /* _NETINET_IN_H_ */