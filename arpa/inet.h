/*
 * Fake arpa/inet.h for Windows PostgreSQL extensions
 * This header provides Windows-compatible definitions for networking
 * functions that PostgreSQL expects from arpa/inet.h
 */

#ifndef _ARPA_INET_H_
#define _ARPA_INET_H_

#ifdef _WIN32

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>

/* 
 * All the networking functions that would normally be in arpa/inet.h
 * are already provided by winsock2.h and ws2tcpip.h on Windows:
 * - inet_addr()
 * - inet_ntoa() 
 * - inet_aton()
 * - inet_ntop()
 * - inet_pton()
 * - htons(), htonl(), ntohs(), ntohl()
 */

/* Ensure inet_aton is available (sometimes missing on older Windows) */
#ifndef HAVE_INET_ATON
#ifdef inet_aton
/* Already defined as macro */
#else
/* Provide inet_aton implementation using inet_addr */
static inline int inet_aton(const char *cp, struct in_addr *inp)
{
    inp->s_addr = inet_addr(cp);
    return (inp->s_addr != INADDR_NONE) ? 1 : 0;
}
#endif
#endif

#endif /* _WIN32 */

#endif /* _ARPA_INET_H_ */