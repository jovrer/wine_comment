/*
 * based on Windows Sockets 1.1 specs
 * (ftp.microsoft.com:/Advsys/winsock/spec11/WINSOCK.TXT)
 *
 * Copyright (C) 1993,1994,1996,1997 John Brezak, Erik Bos, Alex Korobka.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * NOTE: If you make any changes to fix a particular app, make sure
 * they don't break something else like Netscape or telnet and ftp
 * clients and servers (www.winsite.com got a lot of those).
 */

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_SYS_IPC_H
# include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>
#endif

#if defined(__EMX__)
# include <sys/so_ioctl.h>
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifdef HAVE_SYS_MSG_H
# include <sys/msg.h>
#endif
#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif
#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#ifdef HAVE_SYS_ERRNO_H
#include <sys/errno.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>
#ifdef HAVE_ARPA_NAMESER_H
# include <arpa/nameser.h>
#endif
#ifdef HAVE_RESOLV_H
# include <resolv.h>
#endif
#ifdef HAVE_NET_IF_H
# include <net/if.h>
#endif

#ifdef HAVE_NETIPX_IPX_H
# include <netipx/ipx.h>
# define HAVE_IPX
#elif defined(HAVE_LINUX_IPX_H)
# ifdef HAVE_ASM_TYPES_H
#  include <asm/types.h>
# endif
# include <linux/ipx.h>
# define HAVE_IPX
#endif

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifdef HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#define NONAMELESSUNION
#define NONAMELESSSTRUCT
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "winerror.h"
#include "winnls.h"
#include "winsock2.h"
#include "mswsock.h"
#include "ws2tcpip.h"
#include "ws2spi.h"
#include "wsipx.h"
#include "winnt.h"
#include "iphlpapi.h"
#include "thread.h"
#include "wine/server.h"
#include "wine/debug.h"
#include "ntstatus.h"
#include "wine/unicode.h"

#ifdef HAVE_IPX
# include "wsnwlink.h"
#endif


#ifdef __FreeBSD__
# define sipx_network    sipx_addr.x_net
# define sipx_node       sipx_addr.x_host.c_host
#endif  /* __FreeBSD__ */

#ifndef INADDR_NONE
#define INADDR_NONE ~0UL
#endif

WINE_DEFAULT_DEBUG_CHANNEL(winsock);

/* critical section to protect some non-rentrant net function */
extern CRITICAL_SECTION csWSgetXXXbyYYY;

inline static const char *debugstr_sockaddr( const struct WS_sockaddr *a )
{
    if (!a) return "(nil)";
    return wine_dbg_sprintf("{ family %d, address %s, port %d }",
                            ((const struct sockaddr_in *)a)->sin_family,
                            inet_ntoa(((const struct sockaddr_in *)a)->sin_addr),
                            ntohs(((const struct sockaddr_in *)a)->sin_port));
}

/* HANDLE<->SOCKET conversion (SOCKET is UINT_PTR). */
#define SOCKET2HANDLE(s) ((HANDLE)(s))
#define HANDLE2SOCKET(h) ((SOCKET)(h))

/****************************************************************
 * Async IO declarations
 ****************************************************************/

typedef struct ws2_async
{
    HANDLE                              hSocket;
    enum ws2_mode {ws2m_read, ws2m_write, ws2m_sd_read, ws2m_sd_write} mode;
    LPWSAOVERLAPPED                     user_overlapped;
    LPWSAOVERLAPPED_COMPLETION_ROUTINE  completion_func;
    struct iovec                        *iovec;
    int                                 n_iovecs;
    struct WS_sockaddr                  *addr;
    union
    {
        int val;     /* for send operations */
        int *ptr;    /* for recv operations */
    }                                   addrlen;
    DWORD                               flags;
    int                                 fd;
    HANDLE                              event;
} ws2_async;

/****************************************************************/

/* ----------------------------------- internal data */

/* ws_... struct conversion flags */

typedef struct          /* WSAAsyncSelect() control struct */
{
  HANDLE      service, event, sock;
  HWND        hWnd;
  UINT        uMsg;
  LONG        lEvent;
} ws_select_info;

#define WS_MAX_SOCKETS_PER_PROCESS      128     /* reasonable guess */
#define WS_MAX_UDP_DATAGRAM             1024
static INT WINAPI WSA_DefaultBlockingHook( FARPROC x );

/* hostent's, servent's and protent's are stored in one buffer per thread,
 * as documented on MSDN for the functions that return any of the buffers */
struct per_thread_data
{
    int opentype;
    struct WS_hostent *he_buffer;
    struct WS_servent *se_buffer;
    struct WS_protoent *pe_buffer;
    int he_len;
    int se_len;
    int pe_len;
};

static INT num_startup;          /* reference counter */
static FARPROC blocking_hook = WSA_DefaultBlockingHook;

/* function prototypes */
static struct WS_hostent *WS_dup_he(const struct hostent* p_he);
static struct WS_protoent *WS_dup_pe(const struct protoent* p_pe);
static struct WS_servent *WS_dup_se(const struct servent* p_se);

int WSAIOCTL_GetInterfaceCount(void);
int WSAIOCTL_GetInterfaceName(int intNumber, char *intName);

UINT wsaErrno(void);
UINT wsaHerrno(int errnr);

#define MAP_OPTION(opt) { WS_##opt, opt }

static const int ws_sock_map[][2] =
{
    MAP_OPTION( SO_DEBUG ),
    MAP_OPTION( SO_REUSEADDR ),
    MAP_OPTION( SO_KEEPALIVE ),
    MAP_OPTION( SO_DONTROUTE ),
    MAP_OPTION( SO_BROADCAST ),
    MAP_OPTION( SO_LINGER ),
    MAP_OPTION( SO_OOBINLINE ),
    MAP_OPTION( SO_SNDBUF ),
    MAP_OPTION( SO_RCVBUF ),
    MAP_OPTION( SO_ERROR ),
    MAP_OPTION( SO_TYPE ),
#ifdef SO_RCVTIMEO
    MAP_OPTION( SO_RCVTIMEO ),
#endif
#ifdef SO_SNDTIMEO
    MAP_OPTION( SO_SNDTIMEO ),
#endif
    { 0, 0 }
};

static const int ws_tcp_map[][2] =
{
#ifdef TCP_NODELAY
    MAP_OPTION( TCP_NODELAY ),
#endif
    { 0, 0 }
};

static const int ws_ip_map[][2] =
{
    MAP_OPTION( IP_MULTICAST_IF ),
    MAP_OPTION( IP_MULTICAST_TTL ),
    MAP_OPTION( IP_MULTICAST_LOOP ),
    MAP_OPTION( IP_ADD_MEMBERSHIP ),
    MAP_OPTION( IP_DROP_MEMBERSHIP ),
    MAP_OPTION( IP_OPTIONS ),
#ifdef IP_HDRINCL
    MAP_OPTION( IP_HDRINCL ),
#endif
    MAP_OPTION( IP_TOS ),
    MAP_OPTION( IP_TTL ),
    { 0, 0 }
};

inline static DWORD NtStatusToWSAError( const DWORD status )
{
    /* We only need to cover the status codes set by server async request handling */
    DWORD wserr;
    switch ( status )
    {
    case STATUS_SUCCESS:              wserr = 0;                     break;
    case STATUS_PENDING:              wserr = WSA_IO_PENDING;        break;
    case STATUS_INVALID_HANDLE:       wserr = WSAENOTSOCK;           break;  /* WSAEBADF ? */
    case STATUS_INVALID_PARAMETER:    wserr = WSAEINVAL;             break;
    case STATUS_PIPE_DISCONNECTED:    wserr = WSAESHUTDOWN;          break;
    case STATUS_CANCELLED:            wserr = WSA_OPERATION_ABORTED; break;
    case STATUS_TIMEOUT:              wserr = WSAETIMEDOUT;          break;
    case STATUS_NO_MEMORY:            wserr = WSAEFAULT;             break;
    default:
        if ( status >= WSABASEERR && status <= WSABASEERR+1004 )
            /* It is not a NT status code but a winsock error */
            wserr = status;
        else
        {
            wserr = RtlNtStatusToDosError( status );
            FIXME( "Status code %08lx converted to DOS error code %lx\n", status, wserr );
        }
    }
    return wserr;
}

/* set last error code from NT status without mapping WSA errors */
inline static unsigned int set_error( unsigned int err )
{
    if (err)
    {
        err = NtStatusToWSAError( err );
        SetLastError( err );
    }
    return err;
}

inline static int get_sock_fd( SOCKET s, DWORD access, int *flags )
{
    int fd;
    if (set_error( wine_server_handle_to_fd( SOCKET2HANDLE(s), access, &fd, flags ) ))
        return -1;
    return fd;
}

inline static void release_sock_fd( SOCKET s, int fd )
{
    wine_server_release_fd( SOCKET2HANDLE(s), fd );
}

static void _enable_event( HANDLE s, unsigned int event,
                           unsigned int sstate, unsigned int cstate )
{
    SERVER_START_REQ( enable_socket_event )
    {
        req->handle = s;
        req->mask   = event;
        req->sstate = sstate;
        req->cstate = cstate;
        wine_server_call( req );
    }
    SERVER_END_REQ;
}

static int _is_blocking(SOCKET s)
{
    int ret;
    SERVER_START_REQ( get_socket_event )
    {
        req->handle  = SOCKET2HANDLE(s);
        req->service = FALSE;
        req->c_event = 0;
        wine_server_call( req );
        ret = (reply->state & FD_WINE_NONBLOCKING) == 0;
    }
    SERVER_END_REQ;
    return ret;
}

static unsigned int _get_sock_mask(SOCKET s)
{
    unsigned int ret;
    SERVER_START_REQ( get_socket_event )
    {
        req->handle  = SOCKET2HANDLE(s);
        req->service = FALSE;
        req->c_event = 0;
        wine_server_call( req );
        ret = reply->mask;
    }
    SERVER_END_REQ;
    return ret;
}

static void _sync_sock_state(SOCKET s)
{
    /* do a dummy wineserver request in order to let
       the wineserver run through its select loop once */
    (void)_is_blocking(s);
}

static int _get_sock_error(SOCKET s, unsigned int bit)
{
    int events[FD_MAX_EVENTS];

    SERVER_START_REQ( get_socket_event )
    {
        req->handle  = SOCKET2HANDLE(s);
        req->service = FALSE;
        req->c_event = 0;
        wine_server_set_reply( req, events, sizeof(events) );
        wine_server_call( req );
    }
    SERVER_END_REQ;
    return events[bit];
}

static struct per_thread_data *get_per_thread_data(void)
{
    struct per_thread_data * ptb = NtCurrentTeb()->WinSockData;
    /* lazy initialization */
    if (!ptb)
    {
        ptb = HeapAlloc( GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ptb) );
        NtCurrentTeb()->WinSockData = ptb;
    }
    return ptb;
}

static void free_per_thread_data(void)
{
    struct per_thread_data * ptb = NtCurrentTeb()->WinSockData;

    if (!ptb) return;

    /* delete scratch buffers */
    HeapFree( GetProcessHeap(), 0, ptb->he_buffer );
    HeapFree( GetProcessHeap(), 0, ptb->se_buffer );
    HeapFree( GetProcessHeap(), 0, ptb->pe_buffer );
    ptb->he_buffer = NULL;
    ptb->se_buffer = NULL;
    ptb->pe_buffer = NULL;

    HeapFree( GetProcessHeap(), 0, ptb );
    NtCurrentTeb()->WinSockData = NULL;
}

/***********************************************************************
 *		DllMain (WS2_32.init)
 */
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID fImpLoad)
{
    TRACE("%p 0x%lx %p\n", hInstDLL, fdwReason, fImpLoad);
    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        free_per_thread_data();
        num_startup = 0;
        break;
    case DLL_THREAD_DETACH:
        free_per_thread_data();
        break;
    }
    return TRUE;
}

/***********************************************************************
 *          convert_sockopt()
 *
 * Converts socket flags from Windows format.
 * Return 1 if converted, 0 if not (error).
 */
static int convert_sockopt(INT *level, INT *optname)
{
  int i;
  switch (*level)
  {
     case WS_SOL_SOCKET:
        *level = SOL_SOCKET;
        for(i=0; ws_sock_map[i][0]; i++)
        {
            if( ws_sock_map[i][0] == *optname )
            {
                *optname = ws_sock_map[i][1];
                return 1;
            }
        }
        FIXME("Unknown SOL_SOCKET optname 0x%x\n", *optname);
        break;
     case WS_IPPROTO_TCP:
        *level = IPPROTO_TCP;
        for(i=0; ws_tcp_map[i][0]; i++)
        {
            if ( ws_tcp_map[i][0] == *optname )
            {
                *optname = ws_tcp_map[i][1];
                return 1;
            }
        }
        FIXME("Unknown IPPROTO_TCP optname 0x%x\n", *optname);
	break;
     case WS_IPPROTO_IP:
        *level = IPPROTO_IP;
        for(i=0; ws_ip_map[i][0]; i++)
        {
            if (ws_ip_map[i][0] == *optname )
            {
                *optname = ws_ip_map[i][1];
                return 1;
            }
        }
	FIXME("Unknown IPPROTO_IP optname 0x%x\n", *optname);
	break;
     default: FIXME("Unimplemented or unknown socket level\n");
  }
  return 0;
}

static inline BOOL is_timeout_option( int optname )
{
#ifdef SO_RCVTIMEO
    if (optname == SO_RCVTIMEO) return TRUE;
#endif
#ifdef SO_SNDTIMEO
    if (optname == SO_SNDTIMEO) return TRUE;
#endif
    return FALSE;
}

/* ----------------------------------- Per-thread info (or per-process?) */

static char *strdup_lower(const char *str)
{
    int i;
    char *ret = HeapAlloc( GetProcessHeap(), 0, strlen(str) + 1 );

    if (ret)
    {
        for (i = 0; str[i]; i++) ret[i] = tolower(str[i]);
        ret[i] = 0;
    }
    else SetLastError(WSAENOBUFS);
    return ret;
}

static fd_set* fd_set_import( fd_set* fds, const WS_fd_set* wsfds, int access, int* highfd, int lfd[] )
{
    /* translate Winsock fd set into local fd set */
    if( wsfds )
    {
        unsigned int i;

        FD_ZERO(fds);
        for( i = 0; i < wsfds->fd_count; i++ )
        {
            int s = wsfds->fd_array[i];
            int fd = get_sock_fd( s, access, NULL );
            if (fd != -1)
            {
                lfd[ i ] = fd;
                if( fd > *highfd ) *highfd = fd;
                FD_SET(fd, fds);
            }
            else lfd[ i ] = -1;
        }
        return fds;
    }
    return NULL;
}

inline static int sock_error_p(int s)
{
    unsigned int optval, optlen;

    optlen = sizeof(optval);
    getsockopt(s, SOL_SOCKET, SO_ERROR, (void *) &optval, &optlen);
    if (optval) WARN("\t[%i] error: %d\n", s, optval);
    return optval != 0;
}

static int fd_set_export( const fd_set* fds, fd_set* exceptfds, WS_fd_set* wsfds, int lfd[] )
{
    int num_err = 0;

    /* translate local fd set into Winsock fd set, adding
     * errors to exceptfds (only if app requested it) */

    if( wsfds )
    {
	int i, j, count = wsfds->fd_count;

	for( i = 0, j = 0; i < count; i++ )
	{
            int fd = lfd[i];
            SOCKET s = wsfds->fd_array[i];
            if (fd == -1) continue;
            if( FD_ISSET(fd, fds) )
            {
                if ( exceptfds && sock_error_p(fd) )
                {
                    FD_SET(fd, exceptfds);
                    num_err++;
                }
                else wsfds->fd_array[j++] = s;
            }
            release_sock_fd( s, fd );
	}
	wsfds->fd_count = j;
    }
    return num_err;
}

static void fd_set_unimport( WS_fd_set* wsfds, int lfd[] )
{
    if ( wsfds )
    {
	unsigned int i;

	for ( i = 0; i < wsfds->fd_count; i++ )
	    if ( lfd[i] >= 0 ) release_sock_fd( wsfds->fd_array[i], lfd[i] );
        wsfds->fd_count = 0;
    }
}

/* Utility: get the SO_RCVTIMEO or SO_SNDTIMEO socket option
 * from an fd and return the value converted to milli seconds
 * or -1 if there is an infinite time out */
static inline int get_rcvsnd_timeo( int fd, int optname)
{
  struct timeval tv;
  unsigned int len = sizeof(tv);
  int ret = getsockopt(fd, SOL_SOCKET, optname, &tv, &len);
  if( ret >= 0)
      ret = tv.tv_sec * 1000 + tv.tv_usec / 1000;
  if( ret <= 0 ) /* tv == {0,0} means infinite time out */
      return -1;
  return ret;
}

/* macro wrappers for portability */
#ifdef SO_RCVTIMEO
#define GET_RCVTIMEO(fd) get_rcvsnd_timeo( (fd), SO_RCVTIMEO)
#else
#define GET_RCVTIMEO(fd) (-1)
#endif

#ifdef SO_SNDTIMEO
#define GET_SNDTIMEO(fd) get_rcvsnd_timeo( (fd), SO_SNDTIMEO)
#else
#define GET_SNDTIMEO(fd) (-1)
#endif

/* utility: given an fd, will block until one of the events occurs */
static inline int do_block( int fd, int events, int timeout )
{
  struct pollfd pfd;
  int ret;

  pfd.fd = fd;
  pfd.events = events;

  while ((ret = poll(&pfd, 1, timeout)) < 0)
  {
      if (errno != EINTR)
          return -1;
  }
  if( ret == 0 )
      return 0;
  return pfd.revents;
}


/* ----------------------------------- API -----
 *
 * Init / cleanup / error checking.
 */

/***********************************************************************
 *      WSAStartup		(WS2_32.115)
 */
int WINAPI WSAStartup(WORD wVersionRequested, LPWSADATA lpWSAData)
{
    TRACE("verReq=%x\n", wVersionRequested);

    if (LOBYTE(wVersionRequested) < 1)
        return WSAVERNOTSUPPORTED;

    if (!lpWSAData) return WSAEINVAL;

    num_startup++;

    /* that's the whole of the negotiation for now */
    lpWSAData->wVersion = wVersionRequested;
    /* return winsock information */
    lpWSAData->wHighVersion = 0x0202;
    strcpy(lpWSAData->szDescription, "WinSock 2.0" );
    strcpy(lpWSAData->szSystemStatus, "Running" );
    lpWSAData->iMaxSockets = WS_MAX_SOCKETS_PER_PROCESS;
    lpWSAData->iMaxUdpDg = WS_MAX_UDP_DATAGRAM;
    /* don't do anything with lpWSAData->lpVendorInfo */
    /* (some apps don't allocate the space for this field) */

    TRACE("succeeded\n");
    return 0;
}


/***********************************************************************
 *      WSACleanup			(WS2_32.116)
 */
INT WINAPI WSACleanup(void)
{
    if (num_startup)
        return 0;
    SetLastError(WSANOTINITIALISED);
    return SOCKET_ERROR;
}


/***********************************************************************
 *      WSAGetLastError		(WINSOCK.111)
 *      WSAGetLastError		(WS2_32.111)
 */
INT WINAPI WSAGetLastError(void)
{
	return GetLastError();
}

/***********************************************************************
 *      WSASetLastError		(WS2_32.112)
 */
void WINAPI WSASetLastError(INT iError) {
    SetLastError(iError);
}

static struct WS_hostent *check_buffer_he(int size)
{
    struct per_thread_data * ptb = get_per_thread_data();
    if (ptb->he_buffer)
    {
        if (ptb->he_len >= size ) return ptb->he_buffer;
        HeapFree( GetProcessHeap(), 0, ptb->he_buffer );
    }
    ptb->he_buffer = HeapAlloc( GetProcessHeap(), 0, (ptb->he_len = size) );
    if (!ptb->he_buffer) SetLastError(WSAENOBUFS);
    return ptb->he_buffer;
}

static struct WS_servent *check_buffer_se(int size)
{
    struct per_thread_data * ptb = get_per_thread_data();
    if (ptb->se_buffer)
    {
        if (ptb->se_len >= size ) return ptb->se_buffer;
        HeapFree( GetProcessHeap(), 0, ptb->se_buffer );
    }
    ptb->se_buffer = HeapAlloc( GetProcessHeap(), 0, (ptb->se_len = size) );
    if (!ptb->se_buffer) SetLastError(WSAENOBUFS);
    return ptb->se_buffer;
}

static struct WS_protoent *check_buffer_pe(int size)
{
    struct per_thread_data * ptb = get_per_thread_data();
    if (ptb->pe_buffer)
    {
        if (ptb->pe_len >= size ) return ptb->pe_buffer;
        HeapFree( GetProcessHeap(), 0, ptb->pe_buffer );
    }
    ptb->pe_buffer = HeapAlloc( GetProcessHeap(), 0, (ptb->pe_len = size) );
    if (!ptb->pe_buffer) SetLastError(WSAENOBUFS);
    return ptb->pe_buffer;
}

/* ----------------------------------- i/o APIs */

#ifdef HAVE_IPX
#define SUPPORTED_PF(pf) ((pf)==WS_AF_INET || (pf)== WS_AF_IPX)
#else
#define SUPPORTED_PF(pf) ((pf)==WS_AF_INET)
#endif


/**********************************************************************/

/* Returns the converted address if successful, NULL if it was too small to
 * start with. Note that the returned pointer may be the original pointer
 * if no conversion is necessary.
 */
static const struct sockaddr* ws_sockaddr_ws2u(const struct WS_sockaddr* wsaddr, int wsaddrlen, unsigned int *uaddrlen)
{
    switch (wsaddr->sa_family)
    {
#ifdef HAVE_IPX
    case WS_AF_IPX:
        {
            const struct WS_sockaddr_ipx* wsipx=(const struct WS_sockaddr_ipx*)wsaddr;
            struct sockaddr_ipx* uipx;

            if (wsaddrlen<sizeof(struct WS_sockaddr_ipx))
                return NULL;

            *uaddrlen=sizeof(struct sockaddr_ipx);
            uipx=HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, *uaddrlen);
            uipx->sipx_family=AF_IPX;
            uipx->sipx_port=wsipx->sa_socket;
            /* copy sa_netnum and sa_nodenum to sipx_network and sipx_node
             * in one go
             */
            memcpy(&uipx->sipx_network,wsipx->sa_netnum,sizeof(uipx->sipx_network)+sizeof(uipx->sipx_node));
#ifdef IPX_FRAME_NONE
            uipx->sipx_type=IPX_FRAME_NONE;
#endif
            return (const struct sockaddr*)uipx;
        }
#endif

    default:
        if (wsaddrlen<sizeof(struct WS_sockaddr))
            return NULL;

        /* No conversion needed, just return the original address */
        *uaddrlen=wsaddrlen;
        return (const struct sockaddr*)wsaddr;
    }
}

/* Allocates a Unix sockaddr structure to receive the data */
static inline struct sockaddr* ws_sockaddr_alloc(const struct WS_sockaddr* wsaddr, int* wsaddrlen, unsigned int* uaddrlen)
{
    if (wsaddr==NULL)
    {
      ERR( "WINE shouldn't pass a NULL wsaddr! Attempting to continue\n" );

      /* This is not strictly the right thing to do. Hope it works however */
      *uaddrlen=0;

      return NULL;
    }

    if (*wsaddrlen==0)
        *uaddrlen=0;
    else
        *uaddrlen=max(sizeof(struct sockaddr),*wsaddrlen);

    return HeapAlloc(GetProcessHeap(), 0, *uaddrlen);
}

/* Returns 0 if successful, -1 if the buffer is too small */
static int ws_sockaddr_u2ws(const struct sockaddr* uaddr, int uaddrlen, struct WS_sockaddr* wsaddr, int* wsaddrlen)
{
    int res;

    switch(uaddr->sa_family)
    {
#ifdef HAVE_IPX
    case AF_IPX:
        {
            const struct sockaddr_ipx* uipx=(const struct sockaddr_ipx*)uaddr;
            struct WS_sockaddr_ipx* wsipx=(struct WS_sockaddr_ipx*)wsaddr;

            res=-1;
            switch (*wsaddrlen) /* how much can we copy? */
            {
            default:
                res=0; /* enough */
                *wsaddrlen=uaddrlen;
                wsipx->sa_socket=uipx->sipx_port;
                /* fall through */
            case 13:
            case 12:
                memcpy(wsipx->sa_nodenum,uipx->sipx_node,sizeof(wsipx->sa_nodenum));
                /* fall through */
            case 11:
            case 10:
            case 9:
            case 8:
            case 7:
            case 6:
                memcpy(wsipx->sa_netnum,&uipx->sipx_network,sizeof(wsipx->sa_netnum));
                /* fall through */
            case 5:
            case 4:
            case 3:
            case 2:
                wsipx->sa_family=WS_AF_IPX;
                /* fall through */
            case 1:
            case 0:
                /* way too small */
                break;
            }
        }
        break;
#endif

    default:
        /* No conversion needed */
        memcpy(wsaddr,uaddr,*wsaddrlen);
        if (*wsaddrlen<uaddrlen) {
            res=-1;
        } else {
            *wsaddrlen=uaddrlen;
            res=0;
        }
    }
    return res;
}

/* to be called to free the memory allocated by ws_sockaddr_ws2u or
 * ws_sockaddr_alloc
 */
static inline void ws_sockaddr_free(const struct sockaddr* uaddr, const struct WS_sockaddr* wsaddr)
{
    if (uaddr!=(const struct sockaddr*)wsaddr)
        HeapFree(GetProcessHeap(), 0, (void *)uaddr);
}

/**************************************************************************
 * Functions for handling overlapped I/O
 **************************************************************************/

static void CALLBACK ws2_async_terminate(ws2_async* as, IO_STATUS_BLOCK* iosb)
{
    TRACE( "as: %p uovl %p ovl %p\n", as, as->user_overlapped, iosb );

    wine_server_release_fd( as->hSocket, as->fd );
    if ( as->event != INVALID_HANDLE_VALUE )
        NtSetEvent( as->event, NULL );

    if (as->completion_func)
        as->completion_func( NtStatusToWSAError (iosb->u.Status),
                             iosb->Information, as->user_overlapped, as->flags );
    if ( !as->user_overlapped )
    {
#if 0
        /* FIXME: I don't think this is really used */
        if ( as->overlapped->hEvent != INVALID_HANDLE_VALUE )
            WSACloseEvent( as->overlapped->hEvent  );
#endif
        HeapFree( GetProcessHeap(), 0, iosb );
    }

    HeapFree( GetProcessHeap(), 0, as->iovec );
    HeapFree( GetProcessHeap(), 0, as );
}

/***********************************************************************
 *              WS2_make_async          (INTERNAL)
 */

static void WINAPI WS2_async_recv(void*, IO_STATUS_BLOCK*, ULONG);
static void WINAPI WS2_async_send(void*, IO_STATUS_BLOCK*, ULONG);
static void WINAPI WS2_async_shutdown( void*, IO_STATUS_BLOCK*, ULONG);

inline static struct ws2_async*
WS2_make_async(SOCKET s, int fd, enum ws2_mode mode, struct iovec *iovec, DWORD dwBufferCount,
               LPDWORD lpFlags, struct WS_sockaddr *addr,
               LPINT addrlen, LPWSAOVERLAPPED lpOverlapped,
               LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine,
               IO_STATUS_BLOCK **piosb)
{
    struct ws2_async *wsa = HeapAlloc( GetProcessHeap(), 0, sizeof( ws2_async ) );

    TRACE( "wsa %p\n", wsa );

    if (!wsa)
        return NULL;

    wsa->hSocket = (HANDLE) s;
    wsa->mode = mode;
    switch (mode)
    {
    case ws2m_read:
    case ws2m_sd_read:
        wsa->flags = *lpFlags;
        wsa->addrlen.ptr = addrlen;
        break;
    case ws2m_write:
    case ws2m_sd_write:
        wsa->flags = 0;
        wsa->addrlen.val = *addrlen;
        break;
    default:
        ERR("Invalid async mode: %d\n", mode);
    }
    wsa->user_overlapped = lpOverlapped;
    wsa->completion_func = lpCompletionRoutine;
    wsa->iovec = iovec;
    wsa->n_iovecs = dwBufferCount;
    wsa->addr = addr;
    wsa->fd = fd;
    wsa->event = INVALID_HANDLE_VALUE;

    if ( lpOverlapped )
    {
        *piosb = (IO_STATUS_BLOCK*)lpOverlapped;
        if (!lpCompletionRoutine)
        {
            wsa->event = lpOverlapped->hEvent;
            NtResetEvent(wsa->event, NULL);
        }
    }
    else if (!(*piosb = HeapAlloc( GetProcessHeap(), 0, sizeof(IO_STATUS_BLOCK))))
        goto error;

    (*piosb)->Information = 0;
    (*piosb)->u.Status = STATUS_PENDING;
    TRACE( "wsa %p, h %p, ev %p, fd %d, iosb %p, uov %p, cfunc %p\n",
           wsa, wsa->hSocket, wsa->event, wsa->fd,
           *piosb, wsa->user_overlapped, wsa->completion_func );

    return wsa;

error:
    TRACE("Error\n");
    HeapFree( GetProcessHeap(), 0, wsa );
    return NULL;
}

static ULONG ws2_queue_async(struct ws2_async* wsa, IO_STATUS_BLOCK* iosb)
{
    PIO_APC_ROUTINE     apc;
    int                 type;
    NTSTATUS            status;

    switch (wsa->mode)
    {
    case ws2m_read:     apc = WS2_async_recv;     type = ASYNC_TYPE_READ;  break;
    case ws2m_write:    apc = WS2_async_send;     type = ASYNC_TYPE_WRITE; break;
    case ws2m_sd_read:  apc = WS2_async_shutdown; type = ASYNC_TYPE_READ;  break;
    case ws2m_sd_write: apc = WS2_async_shutdown; type = ASYNC_TYPE_WRITE; break;
    default: FIXME("Unknown internal mode (%d)\n", wsa->mode); return STATUS_INVALID_PARAMETER;
    }

    SERVER_START_REQ( register_async )
    {
        req->handle = wsa->hSocket;
        req->io_apc = apc;
        req->io_sb = iosb;
        req->io_user = wsa;
        req->type = type;
        req->count = iosb->Information;
        status = wine_server_call( req );
    }
    SERVER_END_REQ;

    if ( status ) iosb->u.Status = status;
    if ( iosb->u.Status != STATUS_PENDING )
    {
        /* Note: we get here a non zero status when we couldn't queue the async
         * in the server. Therefore, we simply terminate the async.
         */
        status = iosb->u.Status;
        ws2_async_terminate(wsa, iosb);
        return status;
    }
    NtCurrentTeb()->num_async_io++;
    return STATUS_SUCCESS;
}

/***********************************************************************
 *              WS2_recv                (INTERNAL)
 *
 * Workhorse for both synchronous and asynchronous recv() operations.
 */
static int WS2_recv( int fd, struct iovec* iov, int count,
                     struct WS_sockaddr *lpFrom, LPINT lpFromlen,
                      LPDWORD lpFlags )
{
    struct msghdr hdr;
    int n;
    TRACE( "fd %d, iovec %p, count %d addr %s, len %p, flags %lx\n",
           fd, iov, count, debugstr_sockaddr(lpFrom), lpFromlen, *lpFlags);

    hdr.msg_name = NULL;

    if ( lpFrom )
    {
        hdr.msg_namelen = *lpFromlen;
        hdr.msg_name = ws_sockaddr_alloc( lpFrom, lpFromlen, &hdr.msg_namelen );
        if ( !hdr.msg_name )
        {
            WSASetLastError( WSAEFAULT );
            n = -1;
            goto out;
        }
    }
    else
        hdr.msg_namelen = 0;

    hdr.msg_iov = iov;
    hdr.msg_iovlen = count;
#ifdef HAVE_STRUCT_MSGHDR_MSG_ACCRIGHTS
    hdr.msg_accrights = NULL;
    hdr.msg_accrightslen = 0;
#else
    hdr.msg_control = NULL;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
#endif

    if ( (n = recvmsg(fd, &hdr, *lpFlags)) == -1 )
    {
        TRACE( "recvmsg error %d\n", errno);
        goto out;
    }

    if ( lpFrom &&
         ws_sockaddr_u2ws( hdr.msg_name, hdr.msg_namelen,
                           lpFrom, lpFromlen ) != 0 )
    {
        /* The from buffer was too small, but we read the data
         * anyway. Is that really bad?
         */
        WSASetLastError( WSAEFAULT );
        WARN( "Address buffer too small\n" );
    }

out:

    ws_sockaddr_free( hdr.msg_name, lpFrom );
    TRACE("-> %d\n", n);
    return n;
}

/***********************************************************************
 *              WS2_async_recv          (INTERNAL)
 *
 * Handler for overlapped recv() operations.
 */
static void WINAPI WS2_async_recv( void* ovp, IO_STATUS_BLOCK* iosb, ULONG status)
{
    ws2_async* wsa = (ws2_async*) ovp;
    int result, err;

    TRACE( "(%p %p %lx)\n", wsa, iosb, status );

    switch (status)
    {
    case STATUS_ALERTED:
        result = WS2_recv( wsa->fd, wsa->iovec, wsa->n_iovecs,
                           wsa->addr, wsa->addrlen.ptr, &wsa->flags );
        if (result >= 0)
        {
            iosb->u.Status = STATUS_SUCCESS;
            iosb->Information = result;
            TRACE( "received %d bytes\n", result );
            _enable_event( wsa->hSocket, FD_READ, 0, 0 );
        }
        else
        {
            err = wsaErrno();
            if ( err == WSAEINTR || err == WSAEWOULDBLOCK )  /* errno: EINTR / EAGAIN */
            {
                iosb->u.Status = STATUS_PENDING;
                _enable_event( wsa->hSocket, FD_READ, 0, 0 );
                TRACE( "still pending\n" );
            }
            else
            {
                iosb->u.Status = err; /* FIXME: is this correct ???? */
                TRACE( "Error: %x\n", err );
            }
        }
        if (iosb->u.Status == STATUS_PENDING)
            ws2_queue_async(wsa, iosb);
        else
            ws2_async_terminate(wsa, iosb);
        break;
    default:
        FIXME( "status: %ld\n", status );
        iosb->u.Status = status;
        ws2_async_terminate(wsa, iosb);
        return;
    }
}

/***********************************************************************
 *              WS2_send                (INTERNAL)
 *
 * Workhorse for both synchronous and asynchronous send() operations.
 */
static int WS2_send( int fd, struct iovec* iov, int count,
                     const struct WS_sockaddr *to, INT tolen, DWORD dwFlags )
{
    struct msghdr hdr;
    int n = -1;
    TRACE( "fd %d, iovec %p, count %d addr %s, len %d, flags %lx\n",
           fd, iov, count, debugstr_sockaddr(to), tolen, dwFlags);

    hdr.msg_name = NULL;

    if ( to )
    {
        hdr.msg_name = (struct sockaddr*) ws_sockaddr_ws2u( to, tolen, &hdr.msg_namelen );
        if ( !hdr.msg_name )
        {
            WSASetLastError( WSAEFAULT );
            goto out;
        }

#ifdef HAVE_IPX
        if(to->sa_family == WS_AF_IPX)
        {
#ifdef SOL_IPX
            struct sockaddr_ipx* uipx = (struct sockaddr_ipx*)hdr.msg_name;
            int val=0;
            unsigned int len=sizeof(int);

            /* The packet type is stored at the ipx socket level; At least the linux kernel seems
             *  to do something with it in case hdr.msg_name is NULL. Nonetheless can we use it to store
             *  the packet type and then we can retrieve it using getsockopt. After that we can set the
             *  ipx type in the sockaddr_opx structure with the stored value.
             */
            if(getsockopt(fd, SOL_IPX, IPX_TYPE, &val, &len) != -1)
            {
                TRACE("ptype: %d (fd:%d)\n", val, fd);
                uipx->sipx_type = val;
            }
#endif
        }
#endif

    }
    else
        hdr.msg_namelen = 0;

    hdr.msg_iov = iov;
    hdr.msg_iovlen = count;
#ifdef HAVE_STRUCT_MSGHDR_MSG_ACCRIGHTS
    hdr.msg_accrights = NULL;
    hdr.msg_accrightslen = 0;
#else
    hdr.msg_control = NULL;
    hdr.msg_controllen = 0;
    hdr.msg_flags = 0;
#endif

    n = sendmsg(fd, &hdr, dwFlags);

out:
    ws_sockaddr_free( hdr.msg_name, to );
    return n;
}

/***********************************************************************
 *              WS2_async_send          (INTERNAL)
 *
 * Handler for overlapped send() operations.
 */
static void WINAPI WS2_async_send(void* as, IO_STATUS_BLOCK* iosb, ULONG status)
{
    ws2_async* wsa = (ws2_async*) as;
    int result;

    TRACE( "(%p %p %lx)\n", wsa, iosb, status );

    switch (status)
    {
    case STATUS_ALERTED:
        if (iosb->u.Status != STATUS_PENDING) FIXME("wrong %08lx\n", iosb->u.Status);
        /* check to see if the data is ready (non-blocking) */
        result = WS2_send( wsa->fd, wsa->iovec, wsa->n_iovecs,
                           wsa->addr, wsa->addrlen.val, wsa->flags );

        if (result >= 0)
        {
            iosb->u.Status = STATUS_SUCCESS;
            iosb->Information = result;
            TRACE( "sent %d bytes\n", result );
            _enable_event( wsa->hSocket, FD_WRITE, 0, 0 );
        }
        else
        {
            int err = wsaErrno();
            if ( err == WSAEINTR )
            {
                iosb->u.Status = STATUS_PENDING;
                _enable_event( wsa->hSocket, FD_WRITE, 0, 0 );
                TRACE( "still pending\n" );
            }
            else
            {
                /* We set the status to a winsock error code and check for that
                   later in NtStatusToWSAError () */
                iosb->u.Status = err;
                TRACE( "Error: %x\n", err );
            }
        }
        if (iosb->u.Status == STATUS_PENDING)
            ws2_queue_async(wsa, iosb);
        else
            ws2_async_terminate(wsa, iosb);
        break;
    default:
        FIXME( "status: %ld\n", status );
        iosb->u.Status = status;
        ws2_async_terminate(wsa, iosb);
        return;
    }

}

/***********************************************************************
 *              WS2_async_shutdown      (INTERNAL)
 *
 * Handler for shutdown() operations on overlapped sockets.
 */
static void WINAPI WS2_async_shutdown( void* as, PIO_STATUS_BLOCK iosb, ULONG status )
{
    ws2_async* wsa = (ws2_async*) as;
    int err = 1;

    TRACE( "async %p %d\n", wsa, wsa->mode );
    switch (status)
    {
    case STATUS_ALERTED:
        switch ( wsa->mode )
        {
        case ws2m_sd_read:   err = shutdown( wsa->fd, 0 );  break;
        case ws2m_sd_write:  err = shutdown( wsa->fd, 1 );  break;
        default: ERR("invalid mode: %d\n", wsa->mode );
        }
        iosb->u.Status = err ? wsaErrno() : STATUS_SUCCESS;
        if (iosb->u.Status == STATUS_PENDING)
            ws2_queue_async(wsa, iosb);
        else
            ws2_async_terminate(wsa, iosb);
        break;
    default:
        iosb->u.Status = status;
        ws2_async_terminate(wsa, iosb);
        break;
    }

}

/***********************************************************************
 *  WS2_register_async_shutdown         (INTERNAL)
 *
 * Helper function for WS_shutdown() on overlapped sockets.
 */
static int WS2_register_async_shutdown( SOCKET s, int fd, enum ws2_mode mode )
{
    struct ws2_async *wsa;
    int ret, err = WSAEFAULT;
    DWORD dwflags = 0;
    int len = 0;
    LPWSAOVERLAPPED ovl = HeapAlloc(GetProcessHeap(), 0, sizeof( WSAOVERLAPPED ));
    IO_STATUS_BLOCK *iosb = NULL;

    TRACE("s %d fd %d mode %d\n", s, fd, mode);
    if (!ovl)
        goto out;

    ovl->hEvent = WSACreateEvent();
    if ( ovl->hEvent == WSA_INVALID_EVENT )
        goto out_free;

    wsa = WS2_make_async( s, fd, mode, NULL, 0,
                          &dwflags, NULL, &len, ovl, NULL, &iosb );
    if ( !wsa )
        goto out_close;

    /* Hack: this will cause ws2_async_terminate() to free the overlapped structure */
    wsa->user_overlapped = NULL;
    if ( (ret = ws2_queue_async( wsa, iosb )) )
    {
        err = NtStatusToWSAError( ret );
        goto out;
    }
    /* Try immediate completion */
    while ( WaitForSingleObjectEx( ovl->hEvent, 0, TRUE ) == STATUS_USER_APC );
    return 0;

out_close:
    WSACloseEvent( ovl->hEvent );
out_free:
    HeapFree( GetProcessHeap(), 0, ovl );
out:
    return err;
}

/***********************************************************************
 *		accept		(WS2_32.1)
 */
SOCKET WINAPI WS_accept(SOCKET s, struct WS_sockaddr *addr,
                                 int *addrlen32)
{
    SOCKET as;
    BOOL is_blocking;

    TRACE("socket %04x\n", s );
    is_blocking = _is_blocking(s);

    do {
        if (is_blocking)
        {
            int fd = get_sock_fd( s, GENERIC_READ, NULL );
            if (fd == -1) return INVALID_SOCKET;
            /* block here */
            do_block(fd, POLLIN, -1);
            _sync_sock_state(s); /* let wineserver notice connection */
            release_sock_fd( s, fd );
            /* retrieve any error codes from it */
            SetLastError(_get_sock_error(s, FD_ACCEPT_BIT));
            /* FIXME: care about the error? */
        }
        SERVER_START_REQ( accept_socket )
        {
            req->lhandle = SOCKET2HANDLE(s);
            req->access  = GENERIC_READ|GENERIC_WRITE|SYNCHRONIZE;
            req->inherit = TRUE;
            set_error( wine_server_call( req ) );
            as = HANDLE2SOCKET( reply->handle );
        }
        SERVER_END_REQ;
        if (as)
        {
            if (addr) WS_getpeername(as, addr, addrlen32);
            return as;
        }
    } while (is_blocking);
    return INVALID_SOCKET;
}

/***********************************************************************
 *		bind			(WS2_32.2)
 */
int WINAPI WS_bind(SOCKET s, const struct WS_sockaddr* name, int namelen)
{
    int fd = get_sock_fd( s, 0, NULL );
    int res = SOCKET_ERROR;

    TRACE("socket %04x, ptr %p %s, length %d\n", s, name, debugstr_sockaddr(name), namelen);

    if (fd != -1)
    {
        if (!name || (name->sa_family && !SUPPORTED_PF(name->sa_family)))
        {
            SetLastError(WSAEAFNOSUPPORT);
        }
        else
        {
            const struct sockaddr* uaddr;
            unsigned int uaddrlen;

            uaddr=ws_sockaddr_ws2u(name,namelen,&uaddrlen);
            if (uaddr == NULL)
            {
                SetLastError(WSAEFAULT);
            }
            else
            {
                if (bind(fd, uaddr, uaddrlen) < 0)
                {
                    int loc_errno = errno;
                    WARN("\tfailure - errno = %i\n", errno);
                    errno = loc_errno;
                    switch (errno)
                    {
                    case EBADF:
                        SetLastError(WSAENOTSOCK);
                        break;
                    case EADDRNOTAVAIL:
                        SetLastError(WSAEINVAL);
                        break;
                    default:
                        SetLastError(wsaErrno());
                        break;
                    }
                }
                else
                {
                    res=0; /* success */
                }
                ws_sockaddr_free(uaddr,name);
            }
        }
        release_sock_fd( s, fd );
    }
    return res;
}

/***********************************************************************
 *		closesocket		(WS2_32.3)
 */
int WINAPI WS_closesocket(SOCKET s)
{
    TRACE("socket %04x\n", s);
    if (CloseHandle(SOCKET2HANDLE(s))) return 0;
    return SOCKET_ERROR;
}

/***********************************************************************
 *		connect		(WS2_32.4)
 */
int WINAPI WS_connect(SOCKET s, const struct WS_sockaddr* name, int namelen)
{
    int fd = get_sock_fd( s, GENERIC_READ, NULL );

    TRACE("socket %04x, ptr %p %s, length %d\n", s, name, debugstr_sockaddr(name), namelen);

    if (fd != -1)
    {
        const struct sockaddr* uaddr;
        unsigned int uaddrlen;

        uaddr=ws_sockaddr_ws2u(name,namelen,&uaddrlen);
        if (uaddr == NULL)
        {
            SetLastError(WSAEFAULT);
        }
        else
        {
            int rc;

            rc=connect(fd, uaddr, uaddrlen);
            ws_sockaddr_free(uaddr,name);
            if (rc == 0)
                goto connect_success;
        }

        if (errno == EINPROGRESS)
        {
            /* tell wineserver that a connection is in progress */
            _enable_event(SOCKET2HANDLE(s), FD_CONNECT|FD_READ|FD_WRITE,
                          FD_CONNECT|FD_READ|FD_WRITE,
                          FD_WINE_CONNECTED|FD_WINE_LISTENING);
            if (_is_blocking(s))
            {
                int result;
                /* block here */
                do_block(fd, POLLIN | POLLOUT, -1);
                _sync_sock_state(s); /* let wineserver notice connection */
                /* retrieve any error codes from it */
                result = _get_sock_error(s, FD_CONNECT_BIT);
                if (result)
                    SetLastError(result);
                else
                {
                    goto connect_success;
                }
            }
            else
            {
                SetLastError(WSAEWOULDBLOCK);
            }
        }
        else
        {
            SetLastError(wsaErrno());
        }
        release_sock_fd( s, fd );
    }
    return SOCKET_ERROR;

connect_success:
    release_sock_fd( s, fd );
    _enable_event(SOCKET2HANDLE(s), FD_CONNECT|FD_READ|FD_WRITE,
                  FD_WINE_CONNECTED|FD_READ|FD_WRITE,
                  FD_CONNECT|FD_WINE_LISTENING);
    return 0;
}

/***********************************************************************
 *              WSAConnect             (WS2_32.30)
 */
int WINAPI WSAConnect( SOCKET s, const struct WS_sockaddr* name, int namelen,
                       LPWSABUF lpCallerData, LPWSABUF lpCalleeData,
                       LPQOS lpSQOS, LPQOS lpGQOS )
{
    if ( lpCallerData || lpCalleeData || lpSQOS || lpGQOS )
        FIXME("unsupported parameters!\n");
    return WS_connect( s, name, namelen );
}


/***********************************************************************
 *		getpeername		(WS2_32.5)
 */
int WINAPI WS_getpeername(SOCKET s, struct WS_sockaddr *name, int *namelen)
{
    int fd;
    int res;

    TRACE("socket: %04x, ptr %p, len %08x\n", s, name, *namelen);

    /* Check if what we've received is valid. Should we use IsBadReadPtr? */
    if( (name == NULL) || (namelen == NULL) )
    {
        SetLastError( WSAEFAULT );
        return SOCKET_ERROR;
    }

    fd = get_sock_fd( s, 0, NULL );
    res = SOCKET_ERROR;

    if (fd != -1)
    {
        struct sockaddr* uaddr;
        unsigned int uaddrlen;

        uaddr=ws_sockaddr_alloc(name,namelen,&uaddrlen);
        if (getpeername(fd, uaddr, &uaddrlen) != 0)
        {
            SetLastError(wsaErrno());
        }
        else if (ws_sockaddr_u2ws(uaddr,uaddrlen,name,namelen) != 0)
        {
            /* The buffer was too small */
            SetLastError(WSAEFAULT);
        }
        else
        {
            res=0;
        }
        ws_sockaddr_free(uaddr,name);
        release_sock_fd( s, fd );
    }
    return res;
}

/***********************************************************************
 *		getsockname		(WS2_32.6)
 */
int WINAPI WS_getsockname(SOCKET s, struct WS_sockaddr *name, int *namelen)
{
    int fd;
    int res;

    TRACE("socket: %04x, ptr %p, len %8x\n", s, name, *namelen);

    /* Check if what we've received is valid. Should we use IsBadReadPtr? */
    if( (name == NULL) || (namelen == NULL) )
    {
        SetLastError( WSAEFAULT );
        return SOCKET_ERROR;
    }

    fd = get_sock_fd( s, 0, NULL );
    res = SOCKET_ERROR;

    if (fd != -1)
    {
        struct sockaddr* uaddr;
        unsigned int uaddrlen;

        uaddr=ws_sockaddr_alloc(name,namelen,&uaddrlen);
        if (getsockname(fd, uaddr, &uaddrlen) != 0)
        {
            SetLastError(wsaErrno());
        }
        else if (ws_sockaddr_u2ws(uaddr,uaddrlen,name,namelen) != 0)
        {
            /* The buffer was too small */
            SetLastError(WSAEFAULT);
        }
        else
        {
            res=0;
        }
        ws_sockaddr_free(uaddr,name);
        release_sock_fd( s, fd );
    }
    return res;
}

/***********************************************************************
 *		getsockopt		(WS2_32.7)
 */
INT WINAPI WS_getsockopt(SOCKET s, INT level,
                                  INT optname, char *optval, INT *optlen)
{
    int fd;
    INT ret = 0;

    TRACE("socket: %04x, level 0x%x, name 0x%x, ptr %p, len %d\n",
          s, level, optname, optval, *optlen);
    /* SO_OPENTYPE does not require a valid socket handle. */
    if (level == WS_SOL_SOCKET && optname == WS_SO_OPENTYPE)
    {
        if (!optlen || *optlen < sizeof(int) || !optval)
        {
            SetLastError(WSAEFAULT);
            return SOCKET_ERROR;
        }
        *(int *)optval = get_per_thread_data()->opentype;
        *optlen = sizeof(int);
        TRACE("getting global SO_OPENTYPE = 0x%x\n", *((int*)optval) );
        return 0;
    }

#ifdef HAVE_IPX
    if(level == NSPROTO_IPX)
    {
    	struct WS_sockaddr_ipx addr;
	IPX_ADDRESS_DATA *data;
	int namelen;
	switch(optname)
	{
	    case IPX_PTYPE:
		fd = get_sock_fd( s, 0, NULL );
#ifdef SOL_IPX
		if(getsockopt(fd, SOL_IPX, IPX_TYPE, optval, (unsigned int*)optlen) == -1)
		{
		    return SOCKET_ERROR;
		}
#else
                {
                    struct ipx val;
                    socklen_t len=sizeof(struct ipx);

                    if(getsockopt(fd, 0, SO_DEFAULT_HEADERS, &val, &len) == -1 )
                        return SOCKET_ERROR;
                    *optval = (int)val.ipx_pt;
                }
#endif
		TRACE("ptype: %d (fd: %d)\n", *(int*)optval, fd);
    		release_sock_fd( s, fd );
	
		return 0;
	    case IPX_ADDRESS:
		/*
		*  On a Win2000 system with one network card there are usually three ipx devices one with a speed of 28.8kbps, 10Mbps and 100Mbps.
		*  Using this call you can then retrieve info about this all. In case of Linux it is a bit different. Usually you have
		*  only "one" device active and further it is not possible to query things like the linkspeed.
		*/
		FIXME("IPX_ADDRESS\n");
		namelen = sizeof(struct WS_sockaddr_ipx);
		memset(&addr, 0, sizeof(struct WS_sockaddr_ipx));
		WS_getsockname(s, (struct WS_sockaddr*)&addr, &namelen);

		data = (IPX_ADDRESS_DATA*)optval;
                memcpy(data->nodenum,&addr.sa_nodenum,sizeof(data->nodenum));
                memcpy(data->netnum,&addr.sa_netnum,sizeof(data->netnum));
		data->adapternum = 0;
		data->wan = FALSE; /* We are not on a wan for now .. */
		data->status = FALSE; /* Since we are not on a wan, the wan link isn't up */
		data->maxpkt = 1467; /* This value is the default one, at least on Win2k/WinXP */
		data->linkspeed = 100000; /* Set the line speed in 100bit/s to 10 Mbit; note 1MB = 1000kB in this case */
		return 0;	
	    case IPX_MAX_ADAPTER_NUM:
		FIXME("IPX_MAX_ADAPTER_NUM\n");
    		*(int*)optval = 1; /* As noted under IPX_ADDRESS we have just one card. */

		return 0;
	    default:
		FIXME("IPX optname:%x\n", optname);
		return SOCKET_ERROR;
	}
    }
#endif

    if( (fd = get_sock_fd( s, 0, NULL )) == -1)
        return SOCKET_ERROR;

    if (!convert_sockopt(&level, &optname)) {
        SetLastError(WSAENOPROTOOPT);	/* Unknown option */
        ret = SOCKET_ERROR;
    } else {
        struct timeval tv;
        struct linger lingval;
        unsigned int len, *plen = (unsigned int*)optlen;
        char *pval = optval;
        if(level == SOL_SOCKET && is_timeout_option(optname)) {
            len = sizeof(tv);
            plen = &len;
            pval = (char *) &tv;
        } else if( level == SOL_SOCKET && optname == SO_LINGER) {
            len = sizeof(lingval);
            plen = &len;
            pval = (char *) &lingval;
        }
        if (getsockopt(fd, (int) level, optname, pval, plen) != 0 ) {
            SetLastError((errno == EBADF) ? WSAENOTSOCK : wsaErrno());
            ret = SOCKET_ERROR;
        } else if(level == SOL_SOCKET && is_timeout_option(optname)) {
            if( *optlen >= sizeof(INT) ) {
                *optlen = sizeof(INT);
                *(INT*)optval = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            } else {
                SetLastError(WSAEFAULT);
                ret = SOCKET_ERROR;
            }
        } else if( level == SOL_SOCKET && optname == SO_LINGER) {
            if( *optlen >=  sizeof( LINGER) ) {
                (( LINGER *) optval)->l_onoff = lingval.l_onoff;
                (( LINGER *) optval)->l_linger = lingval.l_linger;
            } else {
                SetLastError(WSAEFAULT);
                ret = SOCKET_ERROR;
            }
        }
    }
    release_sock_fd( s, fd );
    return ret;
}


/***********************************************************************
 *		htonl			(WINSOCK.8)
 *		htonl			(WS2_32.8)
 */
u_long WINAPI WS_htonl(u_long hostlong)
{
    return htonl(hostlong);
}


/***********************************************************************
 *		htons			(WINSOCK.9)
 *		htons			(WS2_32.9)
 */
u_short WINAPI WS_htons(u_short hostshort)
{
    return htons(hostshort);
}

/***********************************************************************
 *		WSAHtonl		(WS2_32.46)
 *  From MSDN decription of error codes, this function should also
 *  check if WinSock has been initialized and the socket is a valid
 *  socket. But why? This function only translates a host byte order
 *  u_long into a network byte order u_long...
 */
int WINAPI WSAHtonl(SOCKET s, u_long hostlong, u_long *lpnetlong)
{
    if (lpnetlong)
    {
        *lpnetlong = htonl(hostlong);
        return 0;
    }
    WSASetLastError(WSAEFAULT);
    return SOCKET_ERROR;
}

/***********************************************************************
 *		WSAHtons		(WS2_32.47)
 *  From MSDN decription of error codes, this function should also
 *  check if WinSock has been initialized and the socket is a valid
 *  socket. But why? This function only translates a host byte order
 *  u_short into a network byte order u_short...
 */
int WINAPI WSAHtons(SOCKET s, u_short hostshort, u_short *lpnetshort)
{

    if (lpnetshort)
    {
        *lpnetshort = htons(hostshort);
        return 0;
    }
    WSASetLastError(WSAEFAULT);
    return SOCKET_ERROR;
}


/***********************************************************************
 *		inet_addr		(WINSOCK.10)
 *		inet_addr		(WS2_32.11)
 */
u_long WINAPI WS_inet_addr(const char *cp)
{
    return inet_addr(cp);
}


/***********************************************************************
 *		ntohl			(WINSOCK.14)
 *		ntohl			(WS2_32.14)
 */
u_long WINAPI WS_ntohl(u_long netlong)
{
    return ntohl(netlong);
}


/***********************************************************************
 *		ntohs			(WINSOCK.15)
 *		ntohs			(WS2_32.15)
 */
u_short WINAPI WS_ntohs(u_short netshort)
{
    return ntohs(netshort);
}


/***********************************************************************
 *		inet_ntoa		(WS2_32.12)
 */
char* WINAPI WS_inet_ntoa(struct WS_in_addr in)
{
  /* use "buffer for dummies" here because some applications have a
   * propensity to decode addresses in ws_hostent structure without
   * saving them first...
   */
    static char dbuffer[16]; /* Yes, 16: 4*3 digits + 3 '.' + 1 '\0' */

    char* s = inet_ntoa(*((struct in_addr*)&in));
    if( s )
    {
        strcpy(dbuffer, s);
        return dbuffer;
    }
    SetLastError(wsaErrno());
    return NULL;
}

/**********************************************************************
 *              WSAIoctl                (WS2_32.50)
 *
 *
 *   FIXME:  Only SIO_GET_INTERFACE_LIST option implemented.
 */
INT WINAPI WSAIoctl(SOCKET s,
                    DWORD   dwIoControlCode,
                    LPVOID  lpvInBuffer,
                    DWORD   cbInBuffer,
                    LPVOID  lpbOutBuffer,
                    DWORD   cbOutBuffer,
                    LPDWORD lpcbBytesReturned,
                    LPWSAOVERLAPPED lpOverlapped,
                    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
   int fd = get_sock_fd( s, 0, NULL );

   if (fd == -1) return SOCKET_ERROR;

   TRACE("%d, 0x%08lx, %p, %ld, %p, %ld, %p, %p, %p\n", 
       s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpbOutBuffer,
       cbOutBuffer, lpcbBytesReturned, lpOverlapped, lpCompletionRoutine);

   switch( dwIoControlCode )
   {
   case SIO_GET_INTERFACE_LIST:
       {
           INTERFACE_INFO* intArray = (INTERFACE_INFO*)lpbOutBuffer;
           DWORD size, numInt, apiReturn;

           TRACE("-> SIO_GET_INTERFACE_LIST request\n");

           if (!lpbOutBuffer)
           {
               release_sock_fd( s, fd );
               WSASetLastError(WSAEFAULT);
               return SOCKET_ERROR;
           }
           if (!lpcbBytesReturned)
           {
               release_sock_fd( s, fd );
               WSASetLastError(WSAEFAULT);
               return SOCKET_ERROR;
           }

           apiReturn = GetAdaptersInfo(NULL, &size);
           if (apiReturn == ERROR_NO_DATA)
           {
               numInt = 0;
           }
           else if (apiReturn == ERROR_BUFFER_OVERFLOW)
           {
               PIP_ADAPTER_INFO table = HeapAlloc(GetProcessHeap(),0,size);

               if (table)
               {
                  if (GetAdaptersInfo(table, &size) == NO_ERROR)
                  {
                     PIP_ADAPTER_INFO ptr;

                     if (size*sizeof(INTERFACE_INFO)/sizeof(IP_ADAPTER_INFO) > cbOutBuffer)
                     {
                        WARN("Buffer too small = %lu, cbOutBuffer = %lu\n", size, cbOutBuffer);
                        HeapFree(GetProcessHeap(),0,table);
                        release_sock_fd( s, fd );
                        WSASetLastError(WSAEFAULT);
                        return SOCKET_ERROR;
                     }
                     for (ptr = table, numInt = 0; ptr;
                      ptr = ptr->Next, intArray++, numInt++)
                     {
                        unsigned int addr, mask, bcast;
                        struct ifreq ifInfo;

                        /* Socket Status Flags */
                        lstrcpynA(ifInfo.ifr_name, ptr->AdapterName, IFNAMSIZ);
                        if (ioctl(fd, SIOCGIFFLAGS, &ifInfo) < 0)
                        {
                           ERR("Error obtaining status flags for socket!\n");
                           HeapFree(GetProcessHeap(),0,table);
                           release_sock_fd( s, fd );
                           WSASetLastError(WSAEINVAL);
                           return SOCKET_ERROR;
                        }
                        else
                        {
                           /* set flags; the values of IFF_* are not the same
                              under Linux and Windows, therefore must generate
                              new flags */
                           intArray->iiFlags = 0;
                           if (ifInfo.ifr_flags & IFF_BROADCAST)
                              intArray->iiFlags |= WS_IFF_BROADCAST;
#ifdef IFF_POINTOPOINT
                           if (ifInfo.ifr_flags & IFF_POINTOPOINT)
                              intArray->iiFlags |= WS_IFF_POINTTOPOINT;
#endif
                           if (ifInfo.ifr_flags & IFF_LOOPBACK)
                              intArray->iiFlags |= WS_IFF_LOOPBACK;
                           if (ifInfo.ifr_flags & IFF_UP)
                              intArray->iiFlags |= WS_IFF_UP;
                           if (ifInfo.ifr_flags & IFF_MULTICAST)
                              intArray->iiFlags |= WS_IFF_MULTICAST;
                        }

                        addr = inet_addr(ptr->IpAddressList.IpAddress.String);
                        mask = inet_addr(ptr->IpAddressList.IpMask.String);
                        bcast = addr | (addr & !mask);
                        intArray->iiAddress.AddressIn.sin_family = AF_INET;
                        intArray->iiAddress.AddressIn.sin_port = 0;
                        intArray->iiAddress.AddressIn.sin_addr.WS_s_addr =
                         addr;
                        intArray->iiNetmask.AddressIn.sin_family = AF_INET;
                        intArray->iiNetmask.AddressIn.sin_port = 0;
                        intArray->iiNetmask.AddressIn.sin_addr.WS_s_addr =
                         mask;
                        intArray->iiBroadcastAddress.AddressIn.sin_family =
                         AF_INET;
                        intArray->iiBroadcastAddress.AddressIn.sin_port = 0;
                        intArray->iiBroadcastAddress.AddressIn.sin_addr.
                         WS_s_addr = bcast;
                     }
                  }
                  else
                  {
                     ERR("Unable to get interface table!\n");
                     release_sock_fd( s, fd );
                     HeapFree(GetProcessHeap(),0,table);
                     WSASetLastError(WSAEINVAL);
                     return SOCKET_ERROR;
                  }
                  HeapFree(GetProcessHeap(),0,table);
               }
               else
               {
                  release_sock_fd( s, fd );
                  WSASetLastError(WSAEINVAL);
                  return SOCKET_ERROR;
               }
           }
           else
           {
               ERR("Unable to get interface table!\n");
               release_sock_fd( s, fd );
               WSASetLastError(WSAEINVAL);
               return SOCKET_ERROR;
           }
           /* Calculate the size of the array being returned */
           *lpcbBytesReturned = sizeof(INTERFACE_INFO) * numInt;
           break;
       }

   case SIO_ADDRESS_LIST_CHANGE:
       FIXME("-> SIO_ADDRESS_LIST_CHANGE request: stub\n");
       /* FIXME: error and return code depend on whether socket was created
        * with WSA_FLAG_OVERLAPPED, but there is no easy way to get this */
       break;

   default:
       WARN("\tunsupported WS_IOCTL cmd (%08lx)\n", dwIoControlCode);
       release_sock_fd( s, fd );
       WSASetLastError(WSAEOPNOTSUPP);
       return SOCKET_ERROR;
   }

   /* Function executed with no errors */
   release_sock_fd( s, fd );
   return 0;
}


/***********************************************************************
 *		ioctlsocket		(WS2_32.10)
 */
int WINAPI WS_ioctlsocket(SOCKET s, long cmd, u_long *argp)
{
    int fd;
    long newcmd  = cmd;

    TRACE("socket %04x, cmd %08lx, ptr %p\n", s, cmd, argp);

    switch( cmd )
    {
    case WS_FIONREAD:
        newcmd=FIONREAD;
        break;

    case WS_FIONBIO:
        if( _get_sock_mask(s) )
        {
            /* AsyncSelect()'ed sockets are always nonblocking */
            if (*argp) return 0;
            SetLastError(WSAEINVAL);
            return SOCKET_ERROR;
        }
        fd = get_sock_fd( s, 0, NULL );
        if (fd != -1)
        {
            int ret;
            if (*argp)
            {
                _enable_event(SOCKET2HANDLE(s), 0, FD_WINE_NONBLOCKING, 0);
                ret = fcntl( fd, F_SETFL, O_NONBLOCK );
            }
            else
            {
                _enable_event(SOCKET2HANDLE(s), 0, 0, FD_WINE_NONBLOCKING);
                ret = fcntl( fd, F_SETFL, 0 );
            }
            release_sock_fd( s, fd );
            if (!ret) return 0;
            SetLastError((errno == EBADF) ? WSAENOTSOCK : wsaErrno());
        }
        return SOCKET_ERROR;

    case WS_SIOCATMARK:
        newcmd=SIOCATMARK;
        break;

    case WS__IOW('f',125,u_long):
        WARN("Warning: WS1.1 shouldn't be using async I/O\n");
        SetLastError(WSAEINVAL);
        return SOCKET_ERROR;

    case SIOCGIFBRDADDR:
    case SIOCGIFNETMASK:
    case SIOCGIFADDR:
        /* These don't need any special handling.  They are used by
           WsControl, and are here to suppress an unnecessary warning. */
        break;

    default:
        /* Netscape tries hard to use bogus ioctl 0x667e */
        WARN("\tunknown WS_IOCTL cmd (%08lx)\n", cmd);
        break;
    }

    fd = get_sock_fd( s, 0, NULL );
    if (fd != -1)
    {
        if( ioctl(fd, newcmd, (char*)argp ) == 0 )
        {
            release_sock_fd( s, fd );
            return 0;
        }
        SetLastError((errno == EBADF) ? WSAENOTSOCK : wsaErrno());
        release_sock_fd( s, fd );
    }
    return SOCKET_ERROR;
}

/***********************************************************************
 *		listen		(WS2_32.13)
 */
int WINAPI WS_listen(SOCKET s, int backlog)
{
    int fd = get_sock_fd( s, GENERIC_READ, NULL );

    TRACE("socket %04x, backlog %d\n", s, backlog);
    if (fd != -1)
    {
	if (listen(fd, backlog) == 0)
	{
            release_sock_fd( s, fd );
	    _enable_event(SOCKET2HANDLE(s), FD_ACCEPT,
			  FD_WINE_LISTENING,
			  FD_CONNECT|FD_WINE_CONNECTED);
	    return 0;
	}
	SetLastError(wsaErrno());
        release_sock_fd( s, fd );
    }
    return SOCKET_ERROR;
}

/***********************************************************************
 *		recv			(WS2_32.16)
 */
int WINAPI WS_recv(SOCKET s, char *buf, int len, int flags)
{
    DWORD n, dwFlags = flags;
    WSABUF wsabuf;

    wsabuf.len = len;
    wsabuf.buf = buf;

    if ( WSARecvFrom(s, &wsabuf, 1, &n, &dwFlags, NULL, NULL, NULL, NULL) == SOCKET_ERROR )
        return SOCKET_ERROR;
    else
        return n;
}

/***********************************************************************
 *		recvfrom		(WS2_32.17)
 */
int WINAPI WS_recvfrom(SOCKET s, char *buf, INT len, int flags,
                       struct WS_sockaddr *from, int *fromlen)
{
    DWORD n, dwFlags = flags;
    WSABUF wsabuf;

    wsabuf.len = len;
    wsabuf.buf = buf;

    if ( WSARecvFrom(s, &wsabuf, 1, &n, &dwFlags, from, fromlen, NULL, NULL) == SOCKET_ERROR )
        return SOCKET_ERROR;
    else
        return n;
}

/***********************************************************************
 *		select			(WS2_32.18)
 */
int WINAPI WS_select(int nfds, WS_fd_set *ws_readfds,
                     WS_fd_set *ws_writefds, WS_fd_set *ws_exceptfds,
                     const struct WS_timeval* ws_timeout)
{
    int         highfd = 0;
    fd_set      readfds, writefds, exceptfds;
    fd_set     *p_read, *p_write, *p_except;
    int         readfd[FD_SETSIZE], writefd[FD_SETSIZE], exceptfd[FD_SETSIZE];
    struct timeval timeout, *timeoutaddr = NULL;

    TRACE("read %p, write %p, excp %p timeout %p\n",
          ws_readfds, ws_writefds, ws_exceptfds, ws_timeout);

    p_read = fd_set_import(&readfds, ws_readfds, GENERIC_READ, &highfd, readfd);
    p_write = fd_set_import(&writefds, ws_writefds, GENERIC_WRITE, &highfd, writefd);
    p_except = fd_set_import(&exceptfds, ws_exceptfds, 0, &highfd, exceptfd);
    if (ws_timeout)
    {
        timeoutaddr = &timeout;
        timeout.tv_sec=ws_timeout->tv_sec;
        timeout.tv_usec=ws_timeout->tv_usec;
    }

    if( (highfd = select(highfd + 1, p_read, p_write, p_except, timeoutaddr)) > 0 )
    {
        fd_set_export(&readfds, p_except, ws_readfds, readfd);
        fd_set_export(&writefds, p_except, ws_writefds, writefd);

        if (p_except && ws_exceptfds)
        {
            unsigned int i, j;

            for (i = j = 0; i < ws_exceptfds->fd_count; i++)
            {
                int fd = exceptfd[i];
                SOCKET s = ws_exceptfds->fd_array[i];
                if (fd == -1) continue;
                if (FD_ISSET(fd, &exceptfds)) ws_exceptfds->fd_array[j++] = s;
                release_sock_fd( s, fd );
            }
            ws_exceptfds->fd_count = j;
        }
        return highfd;
    }
    fd_set_unimport(ws_readfds, readfd);
    fd_set_unimport(ws_writefds, writefd);
    fd_set_unimport(ws_exceptfds, exceptfd);

    if( highfd == 0 ) return 0;
    SetLastError(wsaErrno());
    return SOCKET_ERROR;
}


/***********************************************************************
 *		send			(WS2_32.19)
 */
int WINAPI WS_send(SOCKET s, const char *buf, int len, int flags)
{
    DWORD n;
    WSABUF wsabuf;

    wsabuf.len = len;
    wsabuf.buf = (char*) buf;

    if ( WSASendTo( s, &wsabuf, 1, &n, flags, NULL, 0, NULL, NULL) == SOCKET_ERROR )
        return SOCKET_ERROR;
    else
        return n;
}

/***********************************************************************
 *		WSASend			(WS2_32.72)
 */
INT WINAPI WSASend( SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
                    LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
                    LPWSAOVERLAPPED lpOverlapped,
                    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine )
{
    return WSASendTo( s, lpBuffers, dwBufferCount, lpNumberOfBytesSent, dwFlags,
                      NULL, 0, lpOverlapped, lpCompletionRoutine );
}

/***********************************************************************
 *              WSASendDisconnect       (WS2_32.73)
 */
INT WINAPI WSASendDisconnect( SOCKET s, LPWSABUF lpBuffers )
{
    return WS_shutdown( s, SD_SEND );
}


/***********************************************************************
 *		WSASendTo		(WS2_32.74)
 */
INT WINAPI WSASendTo( SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
                      LPDWORD lpNumberOfBytesSent, DWORD dwFlags,
                      const struct WS_sockaddr *to, int tolen,
                      LPWSAOVERLAPPED lpOverlapped,
                      LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine )
{
    unsigned int i;
    int n, fd, err = WSAENOTSOCK, flags, ret;
    struct iovec* iovec;
    struct ws2_async *wsa;
    IO_STATUS_BLOCK* iosb;

    TRACE("socket %04x, wsabuf %p, nbufs %ld, flags %ld, to %p, tolen %d, ovl %p, func %p\n",
          s, lpBuffers, dwBufferCount, dwFlags,
          to, tolen, lpOverlapped, lpCompletionRoutine);

    fd = get_sock_fd( s, GENERIC_WRITE, &flags );
    TRACE( "fd=%d, flags=%x\n", fd, flags );

    if ( fd == -1 ) return SOCKET_ERROR;

    if (flags & FD_FLAG_SEND_SHUTDOWN)
    {
        WSASetLastError( WSAESHUTDOWN );
        goto err_close;
    }

    if ( !lpNumberOfBytesSent )
    {
        err = WSAEFAULT;
        goto err_close;
    }

    iovec = HeapAlloc(GetProcessHeap(), 0, dwBufferCount * sizeof(struct iovec) );

    if ( !iovec )
    {
        err = WSAEFAULT;
        goto err_close;
    }

    for ( i = 0; i < dwBufferCount; i++ )
    {
        iovec[i].iov_base = lpBuffers[i].buf;
        iovec[i].iov_len  = lpBuffers[i].len;
    }

    if ( (lpOverlapped || lpCompletionRoutine) && flags & FD_FLAG_OVERLAPPED )
    {
        wsa = WS2_make_async( s, fd, ws2m_write, iovec, dwBufferCount,
                              &dwFlags, (struct WS_sockaddr*) to, &tolen,
                              lpOverlapped, lpCompletionRoutine, &iosb );
        if ( !wsa )
        {
            err = WSAEFAULT;
            goto err_free;
        }

        if ( ( ret = ws2_queue_async( wsa, iosb ) ) )
        {
            err = NtStatusToWSAError( ret );

            if ( !lpOverlapped )
                HeapFree( GetProcessHeap(), 0, iosb );
            HeapFree( GetProcessHeap(), 0, wsa );
            goto err_free;
        }

        /* Try immediate completion */
        if ( lpOverlapped )
        {
            if  ( WSAGetOverlappedResult( s, lpOverlapped,
                                          lpNumberOfBytesSent, FALSE, &dwFlags) )
                return 0;

            if ( (err = WSAGetLastError()) != WSA_IO_INCOMPLETE )
                goto error;
        }

        WSASetLastError( WSA_IO_PENDING );
        return SOCKET_ERROR;
    }

    if (_is_blocking(s))
    {
        /* FIXME: exceptfds? */
        int timeout = GET_SNDTIMEO(fd);
        if( !do_block(fd, POLLOUT, timeout)) {
            err = WSAETIMEDOUT;
            goto err_free; /* msdn says a timeout in send is fatal */
        }
    }

    n = WS2_send( fd, iovec, dwBufferCount, to, tolen, dwFlags );
    if ( n == -1 )
    {
        err = wsaErrno();
        if ( err == WSAEWOULDBLOCK )
            _enable_event(SOCKET2HANDLE(s), FD_WRITE, 0, 0);
        goto err_free;
    }

    TRACE(" -> %i bytes\n", n);
    *lpNumberOfBytesSent = n;

    HeapFree( GetProcessHeap(), 0, iovec );
    release_sock_fd( s, fd );
    return 0;

err_free:
    HeapFree( GetProcessHeap(), 0, iovec );

err_close:
    release_sock_fd( s, fd );

error:
    WARN(" -> ERROR %d\n", err);
    WSASetLastError(err);
    return SOCKET_ERROR;
}

/***********************************************************************
 *		sendto		(WS2_32.20)
 */
int WINAPI WS_sendto(SOCKET s, const char *buf, int len, int flags,
                              const struct WS_sockaddr *to, int tolen)
{
    DWORD n;
    WSABUF wsabuf;

    wsabuf.len = len;
    wsabuf.buf = (char*) buf;

    if ( WSASendTo(s, &wsabuf, 1, &n, flags, to, tolen, NULL, NULL) == SOCKET_ERROR )
        return SOCKET_ERROR;
    else
        return n;
}

/***********************************************************************
 *		setsockopt		(WS2_32.21)
 */
int WINAPI WS_setsockopt(SOCKET s, int level, int optname,
                                  const char *optval, int optlen)
{
    int fd;
    int woptval;
    struct linger linger;
    struct timeval tval;

    TRACE("socket: %04x, level %d, name %d, ptr %p, len %d\n",
          s, level, optname, optval, optlen);

    /* SO_OPENTYPE does not require a valid socket handle. */
    if (level == WS_SOL_SOCKET && optname == WS_SO_OPENTYPE)
    {
        if (optlen < sizeof(int) || !optval)
        {
            SetLastError(WSAEFAULT);
            return SOCKET_ERROR;
        }
        get_per_thread_data()->opentype = *(int *)optval;
        TRACE("setting global SO_OPENTYPE to 0x%x\n", *(int *)optval );
        return 0;
    }

    /* For some reason the game GrandPrixLegends does set SO_DONTROUTE on its
     * socket. This will either not happen under windows or it is ignored in
     * windows (but it works in linux and therefore prevents the game from
     * finding games outside the current network) */
    if ( level==WS_SOL_SOCKET && optname==WS_SO_DONTROUTE ) 
    {
        FIXME("Does windows ignore SO_DONTROUTE?\n");
        return 0;
    }

#ifdef HAVE_IPX
    if(level == NSPROTO_IPX)
    {
	switch(optname)
	{
	    case IPX_PTYPE:
		fd = get_sock_fd( s, 0, NULL );
		TRACE("trying to set IPX_PTYPE: %d (fd: %d)\n", *(int*)optval, fd);
		
		/* We try to set the ipx type on ipx socket level. */
#ifdef SOL_IPX
		if(setsockopt(fd, SOL_IPX, IPX_TYPE, optval, optlen) == -1)
		{
		    ERR("IPX: could not set ipx option type; expect weird behaviour\n");
		    return SOCKET_ERROR;
		}
#else
                {
                    struct ipx val;
                    /* Should we retrieve val using a getsockopt call and then
                     * set the modified one? */
                    val.ipx_pt = *optval;
                    setsockopt(fd, 0, SO_DEFAULT_HEADERS, &val, sizeof(struct ipx));
                }
#endif
    		release_sock_fd( s, fd );
		return 0;
	    case IPX_FILTERPTYPE:
		/* Sets the receive filter packet type, at the moment we don't support it */
		FIXME("IPX_FILTERPTYPE: %x\n", *optval);
		
		/* Returning 0 is better for now than returning a SOCKET_ERROR */
		return 0;
	    default:
		FIXME("opt_name:%x\n", optname);
		return SOCKET_ERROR;
	}
	return 0;
    }
#endif

    /* Is a privileged and useless operation, so we don't. */
    if ((optname == WS_SO_DEBUG) && (level == WS_SOL_SOCKET))
    {
        FIXME("(%d,SOL_SOCKET,SO_DEBUG,%p(%ld)) attempted (is privileged). Ignoring.\n",s,optval,*(DWORD*)optval);
        return 0;
    }

    if(optname == WS_SO_DONTLINGER && level == WS_SOL_SOCKET) {
        /* This is unique to WinSock and takes special conversion */
        linger.l_onoff  = *((int*)optval) ? 0: 1;
        linger.l_linger = 0;
        optname=SO_LINGER;
        optval = (char*)&linger;
        optlen = sizeof(struct linger);
        level = SOL_SOCKET;
    }
    else
    {
        if (!convert_sockopt(&level, &optname)) {
            ERR("Invalid level (%d) or optname (%d)\n", level, optname);
            SetLastError(WSAENOPROTOOPT);
            return SOCKET_ERROR;
        }
        if (optname == SO_LINGER && optval) {
            linger.l_onoff  = ((LINGER*)optval)->l_onoff;
            linger.l_linger  = ((LINGER*)optval)->l_linger;
            /* FIXME: what is documented behavior if SO_LINGER optval
               is null?? */
            optval = (char*)&linger;
            optlen = sizeof(struct linger);
        }
        else if (optval && optlen < sizeof(int))
        {
            woptval= *((INT16 *) optval);
            optval= (char*) &woptval;
            optlen=sizeof(int);
        }
        if (level == SOL_SOCKET && is_timeout_option(optname))
        {
            if (optlen == sizeof(UINT32)) {
                /* WinSock passes miliseconds instead of struct timeval */
                tval.tv_usec = (*(PUINT32)optval % 1000) * 1000;
                tval.tv_sec = *(PUINT32)optval / 1000;
                /* min of 500 milisec */
                if (tval.tv_sec == 0 && tval.tv_usec < 500000)
                    tval.tv_usec = 500000;
                optlen = sizeof(struct timeval);
                optval = (char*)&tval;
            } else if (optlen == sizeof(struct timeval)) {
                WARN("SO_SND/RCVTIMEO for %d bytes: assuming unixism\n", optlen);
            } else {
                WARN("SO_SND/RCVTIMEO for %d bytes is weird: ignored\n", optlen);
                return 0;
            }
        }
        if (level == SOL_SOCKET && optname == SO_RCVBUF && *(int*)optval < 2048)
        {
            WARN("SO_RCVBF for %d bytes is too small: ignored\n", *(int*)optval );
            return 0;
        }
    }


    fd = get_sock_fd( s, 0, NULL );
    if (fd == -1) return SOCKET_ERROR;

    if (setsockopt(fd, level, optname, optval, optlen) == 0)
    {
        release_sock_fd( s, fd );
        return 0;
    }
    TRACE("Setting socket error, %d\n", wsaErrno());
    SetLastError(wsaErrno());
    release_sock_fd( s, fd );
    return SOCKET_ERROR;
}

/***********************************************************************
 *		shutdown		(WS2_32.22)
 */
int WINAPI WS_shutdown(SOCKET s, int how)
{
    int fd, fd0 = -1, fd1 = -1, flags, err = WSAENOTSOCK;
    unsigned int clear_flags = 0;

    fd = get_sock_fd( s, 0, &flags );
    TRACE("socket %04x, how %i %x\n", s, how, flags );

    if (fd == -1)
        return SOCKET_ERROR;

    switch( how )
    {
    case 0: /* drop receives */
        clear_flags |= FD_READ;
        break;
    case 1: /* drop sends */
        clear_flags |= FD_WRITE;
        break;
    case 2: /* drop all */
        clear_flags |= FD_READ|FD_WRITE;
    default:
        clear_flags |= FD_WINE_LISTENING;
    }

    if ( flags & FD_FLAG_OVERLAPPED ) {

        switch ( how )
        {
        case SD_RECEIVE:
            fd0 = fd;
            break;
        case SD_SEND:
            fd1 = fd;
            break;
        case SD_BOTH:
        default:
            fd0 = fd;
            fd1 = get_sock_fd( s, 0, NULL );
            break;
        }

        if ( fd0 != -1 )
        {
            err = WS2_register_async_shutdown( s, fd0, ws2m_sd_read );
            if ( err )
            {
                release_sock_fd( s, fd0 );
                goto error;
            }
        }
        if ( fd1 != -1 )
        {
            err = WS2_register_async_shutdown( s, fd1, ws2m_sd_write );
            if ( err )
            {
                release_sock_fd( s, fd1 );
                goto error;
            }
        }
    }
    else /* non-overlapped mode */
    {
        if ( shutdown( fd, how ) )
        {
            err = wsaErrno();
            release_sock_fd( s, fd );
            goto error;
        }
        release_sock_fd( s, fd );
    }

    _enable_event( SOCKET2HANDLE(s), 0, 0, clear_flags );
    if ( how > 1) WSAAsyncSelect( s, 0, 0, 0 );
    return 0;

error:
    _enable_event( SOCKET2HANDLE(s), 0, 0, clear_flags );
    WSASetLastError( err );
    return SOCKET_ERROR;
}

/***********************************************************************
 *		socket		(WS2_32.23)
 */
SOCKET WINAPI WS_socket(int af, int type, int protocol)
{
    TRACE("af=%d type=%d protocol=%d\n", af, type, protocol);

    return WSASocketA( af, type, protocol, NULL, 0,
                       get_per_thread_data()->opentype ? 0 : WSA_FLAG_OVERLAPPED );
}


/***********************************************************************
 *		gethostbyaddr		(WS2_32.51)
 */
struct WS_hostent* WINAPI WS_gethostbyaddr(const char *addr, int len, int type)
{
    struct WS_hostent *retval = NULL;
    struct hostent* host;

#ifdef HAVE_LINUX_GETHOSTBYNAME_R_6
    char *extrabuf;
    int ebufsize=1024;
    struct hostent hostentry;
    int locerr=ENOBUFS;
    host = NULL;
    extrabuf=HeapAlloc(GetProcessHeap(),0,ebufsize) ;
    while(extrabuf) {
        int res = gethostbyaddr_r(addr, len, type,
                                  &hostentry, extrabuf, ebufsize, &host, &locerr);
        if( res != ERANGE) break;
        ebufsize *=2;
        extrabuf=HeapReAlloc(GetProcessHeap(),0,extrabuf,ebufsize) ;
    }
    if (!host) SetLastError((locerr < 0) ? wsaErrno() : wsaHerrno(locerr));
#else
    EnterCriticalSection( &csWSgetXXXbyYYY );
    host = gethostbyaddr(addr, len, type);
    if (!host) SetLastError((h_errno < 0) ? wsaErrno() : wsaHerrno(h_errno));
#endif
    if( host != NULL ) retval = WS_dup_he(host);
#ifdef  HAVE_LINUX_GETHOSTBYNAME_R_6
    HeapFree(GetProcessHeap(),0,extrabuf);
#else
    LeaveCriticalSection( &csWSgetXXXbyYYY );
#endif
    TRACE("ptr %p, len %d, type %d ret %p\n", addr, len, type, retval);
    return retval;
}

/***********************************************************************
 *		gethostbyname		(WS2_32.52)
 */
struct WS_hostent* WINAPI WS_gethostbyname(const char* name)
{
    struct WS_hostent *retval = NULL;
    struct hostent*     host;
#ifdef  HAVE_LINUX_GETHOSTBYNAME_R_6
    char *extrabuf;
    int ebufsize=1024;
    struct hostent hostentry;
    int locerr = ENOBUFS;
#endif
    char buf[100];
    if( !name) {
        name = buf;
        if( gethostname( buf, 100) == -1) {
            SetLastError( WSAENOBUFS); /* appropriate ? */
            return retval;
        }
    }
#ifdef  HAVE_LINUX_GETHOSTBYNAME_R_6
    host = NULL;
    extrabuf=HeapAlloc(GetProcessHeap(),0,ebufsize) ;
    while(extrabuf) {
        int res = gethostbyname_r(name, &hostentry, extrabuf, ebufsize, &host, &locerr);
        if( res != ERANGE) break;
        ebufsize *=2;
        extrabuf=HeapReAlloc(GetProcessHeap(),0,extrabuf,ebufsize) ;
    }
    if (!host) SetLastError((locerr < 0) ? wsaErrno() : wsaHerrno(locerr));
#else
    EnterCriticalSection( &csWSgetXXXbyYYY );
    host = gethostbyname(name);
    if (!host) SetLastError((h_errno < 0) ? wsaErrno() : wsaHerrno(h_errno));
#endif
    if (host) retval = WS_dup_he(host);
#ifdef  HAVE_LINUX_GETHOSTBYNAME_R_6
    HeapFree(GetProcessHeap(),0,extrabuf);
#else
    LeaveCriticalSection( &csWSgetXXXbyYYY );
#endif
    TRACE( "%s ret %p\n", debugstr_a(name), retval );
    return retval;
}


/***********************************************************************
 *		getprotobyname		(WS2_32.53)
 */
struct WS_protoent* WINAPI WS_getprotobyname(const char* name)
{
    struct WS_protoent* retval = NULL;
#ifdef HAVE_GETPROTOBYNAME
    struct protoent*     proto;
    EnterCriticalSection( &csWSgetXXXbyYYY );
    if( (proto = getprotobyname(name)) != NULL )
    {
        retval = WS_dup_pe(proto);
    }
    else {
        MESSAGE("protocol %s not found; You might want to add "
                "this to /etc/protocols\n", debugstr_a(name) );
        SetLastError(WSANO_DATA);
    }
    LeaveCriticalSection( &csWSgetXXXbyYYY );
#endif
    TRACE( "%s ret %p\n", debugstr_a(name), retval );
    return retval;
}


/***********************************************************************
 *		getprotobynumber	(WS2_32.54)
 */
struct WS_protoent* WINAPI WS_getprotobynumber(int number)
{
    struct WS_protoent* retval = NULL;
#ifdef HAVE_GETPROTOBYNUMBER
    struct protoent*     proto;
    EnterCriticalSection( &csWSgetXXXbyYYY );
    if( (proto = getprotobynumber(number)) != NULL )
    {
        retval = WS_dup_pe(proto);
    }
    else {
        MESSAGE("protocol number %d not found; You might want to add "
                "this to /etc/protocols\n", number );
        SetLastError(WSANO_DATA);
    }
    LeaveCriticalSection( &csWSgetXXXbyYYY );
#endif
    TRACE("%i ret %p\n", number, retval);
    return retval;
}


/***********************************************************************
 *		getservbyname		(WS2_32.55)
 */
struct WS_servent* WINAPI WS_getservbyname(const char *name, const char *proto)
{
    struct WS_servent* retval = NULL;
    struct servent*     serv;
    char *name_str;
    char *proto_str = NULL;

    if (!(name_str = strdup_lower(name))) return NULL;

    if (proto && *proto)
    {
        if (!(proto_str = strdup_lower(proto)))
        {
            HeapFree( GetProcessHeap(), 0, name_str );
            return NULL;
        }
    }

    EnterCriticalSection( &csWSgetXXXbyYYY );
    serv = getservbyname(name_str, proto_str);
    if( serv != NULL )
    {
        retval = WS_dup_se(serv);
    }
    else SetLastError(WSANO_DATA);
    LeaveCriticalSection( &csWSgetXXXbyYYY );
    HeapFree( GetProcessHeap(), 0, proto_str );
    HeapFree( GetProcessHeap(), 0, name_str );
    TRACE( "%s, %s ret %p\n", debugstr_a(name), debugstr_a(proto), retval );
    return retval;
}


/***********************************************************************
 *		getservbyport		(WS2_32.56)
 */
struct WS_servent* WINAPI WS_getservbyport(int port, const char *proto)
{
    struct WS_servent* retval = NULL;
#ifdef HAVE_GETSERVBYPORT
    struct servent*     serv;
    char *proto_str = NULL;

    if (proto && *proto)
    {
        if (!(proto_str = strdup_lower(proto))) return NULL;
    }
    EnterCriticalSection( &csWSgetXXXbyYYY );
    if( (serv = getservbyport(port, proto_str)) != NULL ) {
        retval = WS_dup_se(serv);
    }
    else SetLastError(WSANO_DATA);
    LeaveCriticalSection( &csWSgetXXXbyYYY );
    HeapFree( GetProcessHeap(), 0, proto_str );
#endif
    TRACE("%d (i.e. port %d), %s ret %p\n", port, (int)ntohl(port), debugstr_a(proto), retval);
    return retval;
}


/***********************************************************************
 *              gethostname           (WS2_32.57)
 */
int WINAPI WS_gethostname(char *name, int namelen)
{
    TRACE("name %p, len %d\n", name, namelen);

    if (gethostname(name, namelen) == 0)
    {
        TRACE("<- '%s'\n", name);
        return 0;
    }
    SetLastError((errno == EINVAL) ? WSAEFAULT : wsaErrno());
    TRACE("<- ERROR !\n");
    return SOCKET_ERROR;
}


/* ------------------------------------- Windows sockets extensions -- *
 *								       *
 * ------------------------------------------------------------------- */

/***********************************************************************
 *		WSAEnumNetworkEvents (WS2_32.36)
 */
int WINAPI WSAEnumNetworkEvents(SOCKET s, WSAEVENT hEvent, LPWSANETWORKEVENTS lpEvent)
{
    int ret;

    TRACE("%08x, hEvent %p, lpEvent %p\n", s, hEvent, lpEvent );

    SERVER_START_REQ( get_socket_event )
    {
        req->handle  = SOCKET2HANDLE(s);
        req->service = TRUE;
        req->c_event = hEvent;
        wine_server_set_reply( req, lpEvent->iErrorCode, sizeof(lpEvent->iErrorCode) );
        if (!(ret = wine_server_call(req))) lpEvent->lNetworkEvents = reply->pmask & reply->mask;
    }
    SERVER_END_REQ;
    if (!ret) return 0;
    SetLastError(WSAEINVAL);
    return SOCKET_ERROR;
}

/***********************************************************************
 *		WSAEventSelect (WS2_32.39)
 */
int WINAPI WSAEventSelect(SOCKET s, WSAEVENT hEvent, long lEvent)
{
    int ret;

    TRACE("%08x, hEvent %p, event %08x\n", s, hEvent, (unsigned)lEvent );

    SERVER_START_REQ( set_socket_event )
    {
        req->handle = SOCKET2HANDLE(s);
        req->mask   = lEvent;
        req->event  = hEvent;
        req->window = 0;
        req->msg    = 0;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (!ret) return 0;
    SetLastError(WSAEINVAL);
    return SOCKET_ERROR;
}

/**********************************************************************
 *      WSAGetOverlappedResult (WS2_32.40)
 */
BOOL WINAPI WSAGetOverlappedResult( SOCKET s, LPWSAOVERLAPPED lpOverlapped,
                                    LPDWORD lpcbTransfer, BOOL fWait,
                                    LPDWORD lpdwFlags )
{
    DWORD r;

    TRACE( "socket %04x ovl %p trans %p, wait %d flags %p\n",
           s, lpOverlapped, lpcbTransfer, fWait, lpdwFlags );

    if ( lpOverlapped == NULL )
    {
        ERR( "Invalid pointer\n" );
        WSASetLastError(WSA_INVALID_PARAMETER);
        return FALSE;
    }

    if ( fWait )
    {
        if (lpOverlapped->hEvent)
            while ( WaitForSingleObjectEx(lpOverlapped->hEvent, 
                                          INFINITE, TRUE) == STATUS_USER_APC );
        else /* busy loop */
            while ( ((volatile OVERLAPPED*)lpOverlapped)->Internal == STATUS_PENDING )
                Sleep( 10 );

    }
    else if ( lpOverlapped->Internal == STATUS_PENDING )
    {
        /* Wait in order to give APCs a chance to run. */
        /* This is cheating, so we must set the event again in case of success -
           it may be a non-manual reset event. */
        while ( (r = WaitForSingleObjectEx(lpOverlapped->hEvent, 0, TRUE)) == STATUS_USER_APC );
        if ( r == WAIT_OBJECT_0 && lpOverlapped->hEvent )
            NtSetEvent( lpOverlapped->hEvent, NULL );
    }

    if ( lpcbTransfer )
        *lpcbTransfer = lpOverlapped->InternalHigh;

    if ( lpdwFlags )
        *lpdwFlags = lpOverlapped->u.s.Offset;

    switch ( lpOverlapped->Internal )
    {
    case STATUS_SUCCESS:
        return TRUE;
    case STATUS_PENDING:
        WSASetLastError( WSA_IO_INCOMPLETE );
        if (fWait) ERR("PENDING status after waiting!\n");
        return FALSE;
    default:
        WSASetLastError( NtStatusToWSAError( lpOverlapped->Internal ));
        return FALSE;
    }
}


/***********************************************************************
 *      WSAAsyncSelect			(WS2_32.101)
 */
INT WINAPI WSAAsyncSelect(SOCKET s, HWND hWnd, UINT uMsg, long lEvent)
{
    int ret;

    TRACE("%x, hWnd %p, uMsg %08x, event %08lx\n", s, hWnd, uMsg, lEvent );

    SERVER_START_REQ( set_socket_event )
    {
        req->handle = SOCKET2HANDLE(s);
        req->mask   = lEvent;
        req->event  = 0;
        req->window = hWnd;
        req->msg    = uMsg;
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (!ret) return 0;
    SetLastError(WSAEINVAL);
    return SOCKET_ERROR;
}

/***********************************************************************
 *      WSACreateEvent          (WS2_32.31)
 *
 */
WSAEVENT WINAPI WSACreateEvent(void)
{
    /* Create a manual-reset event, with initial state: unsignaled */
    TRACE("\n");

    return CreateEventW(NULL, TRUE, FALSE, NULL);
}

/***********************************************************************
 *      WSACloseEvent          (WS2_32.29)
 *
 */
BOOL WINAPI WSACloseEvent(WSAEVENT event)
{
    TRACE ("event=%p\n", event);

    return CloseHandle(event);
}

/***********************************************************************
 *      WSASocketA          (WS2_32.78)
 *
 */
SOCKET WINAPI WSASocketA(int af, int type, int protocol,
                         LPWSAPROTOCOL_INFOA lpProtocolInfo,
                         GROUP g, DWORD dwFlags)
{
    INT len;
    WSAPROTOCOL_INFOW info;

    TRACE("af=%d type=%d protocol=%d protocol_info=%p group=%d flags=0x%lx\n",
          af, type, protocol, lpProtocolInfo, g, dwFlags);

    if (!lpProtocolInfo) return WSASocketW(af, type, protocol, NULL, g, dwFlags);

    memcpy(&info, lpProtocolInfo, FIELD_OFFSET(WSAPROTOCOL_INFOW, szProtocol));
    len = MultiByteToWideChar(CP_ACP, 0, lpProtocolInfo->szProtocol, -1,
                              info.szProtocol, WSAPROTOCOL_LEN + 1);

    if (!len)
    {
        WSASetLastError( WSAEINVAL);
        return SOCKET_ERROR;
    }

    return WSASocketW(af, type, protocol, &info, g, dwFlags);
}

/***********************************************************************
 *      WSASocketW          (WS2_32.79)
 *
 */
SOCKET WINAPI WSASocketW(int af, int type, int protocol,
                         LPWSAPROTOCOL_INFOW lpProtocolInfo,
                         GROUP g, DWORD dwFlags)
{
    SOCKET ret;

   /*
      FIXME: The "advanced" parameters of WSASocketW (lpProtocolInfo,
      g, dwFlags except WSA_FLAG_OVERLAPPED) are ignored.
   */

   TRACE("af=%d type=%d protocol=%d protocol_info=%p group=%d flags=0x%lx\n",
         af, type, protocol, lpProtocolInfo, g, dwFlags );

    /* hack for WSADuplicateSocket */
    if (lpProtocolInfo && lpProtocolInfo->dwServiceFlags4 == 0xff00ff00) {
      ret = lpProtocolInfo->dwCatalogEntryId;
      TRACE("\tgot duplicate %04x\n", ret);
      return ret;
    }

    /* check the socket family */
    switch(af)
    {
#ifdef HAVE_IPX
        case WS_AF_IPX: af = AF_IPX;
#endif
        case AF_INET:
        case AF_UNSPEC:
            break;
        default:
            SetLastError(WSAEAFNOSUPPORT);
            return INVALID_SOCKET;
    }

    /* check the socket type */
    switch(type)
    {
        case WS_SOCK_STREAM:
            type=SOCK_STREAM;
            break;
        case WS_SOCK_DGRAM:
            type=SOCK_DGRAM;
            break;
        case WS_SOCK_RAW:
            type=SOCK_RAW;
            break;
        default:
            SetLastError(WSAESOCKTNOSUPPORT);
            return INVALID_SOCKET;
    }

    /* check the protocol type */
    if ( protocol < 0 )  /* don't support negative values */
    {
        SetLastError(WSAEPROTONOSUPPORT);
        return INVALID_SOCKET;
    }

    if ( af == AF_UNSPEC)  /* did they not specify the address family? */
        switch(protocol)
	{
          case IPPROTO_TCP:
             if (type == SOCK_STREAM) { af = AF_INET; break; }
          case IPPROTO_UDP:
             if (type == SOCK_DGRAM)  { af = AF_INET; break; }
          default: SetLastError(WSAEPROTOTYPE); return INVALID_SOCKET;
        }

    SERVER_START_REQ( create_socket )
    {
        req->family   = af;
        req->type     = type;
        req->protocol = protocol;
        req->access   = GENERIC_READ|GENERIC_WRITE|SYNCHRONIZE;
        req->flags    = dwFlags;
        req->inherit  = TRUE;
        set_error( wine_server_call( req ) );
        ret = HANDLE2SOCKET( reply->handle );
    }
    SERVER_END_REQ;
    if (ret)
    {
        TRACE("\tcreated %04x\n", ret );
        return ret;
    }

    if (GetLastError() == WSAEACCES) /* raw socket denied */
    {
        if (type == SOCK_RAW)
            MESSAGE("WARNING: Trying to create a socket of type SOCK_RAW, will fail unless running as root\n");
        else
            MESSAGE("WS_SOCKET: not enough privileges to create socket, try running as root\n");
        SetLastError(WSAESOCKTNOSUPPORT);
    }

    WARN("\t\tfailed!\n");
    return INVALID_SOCKET;
}

/***********************************************************************
 *      WSAJoinLeaf          (WS2_32.58)
 *
 */
SOCKET WINAPI WSAJoinLeaf(
        SOCKET s,
        const struct WS_sockaddr *addr,
        int addrlen,
        LPWSABUF lpCallerData,
        LPWSABUF lpCalleeData,
        LPQOS lpSQOS,
        LPQOS lpGQOS,
        DWORD dwFlags)
{
    FIXME("stub.\n");
    return INVALID_SOCKET;
}

/***********************************************************************
 *      __WSAFDIsSet			(WS2_32.151)
 */
int WINAPI __WSAFDIsSet(SOCKET s, WS_fd_set *set)
{
  int i = set->fd_count;

  TRACE("(%d,%8lx(%i))\n", s,(unsigned long)set, i);

  while (i--)
      if (set->fd_array[i] == s) return 1;
  return 0;
}

/***********************************************************************
 *      WSAIsBlocking			(WINSOCK.114)
 *      WSAIsBlocking			(WS2_32.114)
 */
BOOL WINAPI WSAIsBlocking(void)
{
  /* By default WinSock should set all its sockets to non-blocking mode
   * and poll in PeekMessage loop when processing "blocking" ones. This
   * function is supposed to tell if the program is in this loop. Our
   * blocking calls are truly blocking so we always return FALSE.
   *
   * Note: It is allowed to call this function without prior WSAStartup().
   */

  TRACE("\n");
  return FALSE;
}

/***********************************************************************
 *      WSACancelBlockingCall		(WINSOCK.113)
 *      WSACancelBlockingCall		(WS2_32.113)
 */
INT WINAPI WSACancelBlockingCall(void)
{
    TRACE("\n");
    return 0;
}

static INT WINAPI WSA_DefaultBlockingHook( FARPROC x )
{
    FIXME("How was this called?\n");
    return x();
}


/***********************************************************************
 *      WSASetBlockingHook (WS2_32.109)
 */
FARPROC WINAPI WSASetBlockingHook(FARPROC lpBlockFunc)
{
  FARPROC prev = blocking_hook;
  blocking_hook = lpBlockFunc;
  TRACE("hook %p\n", lpBlockFunc);
  return prev;
}


/***********************************************************************
 *      WSAUnhookBlockingHook (WS2_32.110)
 */
INT WINAPI WSAUnhookBlockingHook(void)
{
    blocking_hook = WSA_DefaultBlockingHook;
    return 0;
}


/* ----------------------------------- end of API stuff */

/* ----------------------------------- helper functions -
 *
 * TODO: Merge WS_dup_..() stuff into one function that
 * would operate with a generic structure containing internal
 * pointers (via a template of some kind).
 */

static int list_size(char** l, int item_size)
{
  int i,j = 0;
  if(l)
  { for(i=0;l[i];i++)
	j += (item_size) ? item_size : strlen(l[i]) + 1;
    j += (i + 1) * sizeof(char*); }
  return j;
}

static int list_dup(char** l_src, char** l_to, int item_size)
{
   char *p;
   int i;

   for (i = 0; l_src[i]; i++) ;
   p = (char *)(l_to + i + 1);
   for (i = 0; l_src[i]; i++)
   {
       int count = ( item_size ) ? item_size : strlen(l_src[i]) + 1;
       memcpy(p, l_src[i], count);
       l_to[i] = p;
       p += count;
   }
   l_to[i] = NULL;
   return p - (char *)l_to;
}

/* ----- hostent */

/* duplicate hostent entry
 * and handle all Win16/Win32 dependent things (struct size, ...) *correctly*.
 * Dito for protoent and servent.
 */
static struct WS_hostent *WS_dup_he(const struct hostent* p_he)
{
    char *p;
    struct WS_hostent *p_to;

    int size = (sizeof(*p_he) +
                strlen(p_he->h_name) + 1 +
                list_size(p_he->h_aliases, 0) +
                list_size(p_he->h_addr_list, p_he->h_length));

    if (!(p_to = check_buffer_he(size))) return NULL;
    p_to->h_addrtype = p_he->h_addrtype;
    p_to->h_length = p_he->h_length;

    p = (char *)(p_to + 1);
    p_to->h_name = p;
    strcpy(p, p_he->h_name);
    p += strlen(p) + 1;

    p_to->h_aliases = (char **)p;
    p += list_dup(p_he->h_aliases, p_to->h_aliases, 0);

    p_to->h_addr_list = (char **)p;
    list_dup(p_he->h_addr_list, p_to->h_addr_list, p_he->h_length);
    return p_to;
}

/* ----- protoent */

static struct WS_protoent *WS_dup_pe(const struct protoent* p_pe)
{
    char *p;
    struct WS_protoent *p_to;

    int size = (sizeof(*p_pe) +
                strlen(p_pe->p_name) + 1 +
                list_size(p_pe->p_aliases, 0));

    if (!(p_to = check_buffer_pe(size))) return NULL;
    p_to->p_proto = p_pe->p_proto;

    p = (char *)(p_to + 1);
    p_to->p_name = p;
    strcpy(p, p_pe->p_name);
    p += strlen(p) + 1;

    p_to->p_aliases = (char **)p;
    list_dup(p_pe->p_aliases, p_to->p_aliases, 0);
    return p_to;
}

/* ----- servent */

static struct WS_servent *WS_dup_se(const struct servent* p_se)
{
    char *p;
    struct WS_servent *p_to;

    int size = (sizeof(*p_se) +
                strlen(p_se->s_proto) + 1 +
                strlen(p_se->s_name) + 1 +
                list_size(p_se->s_aliases, 0));

    if (!(p_to = check_buffer_se(size))) return NULL;
    p_to->s_port = p_se->s_port;

    p = (char *)(p_to + 1);
    p_to->s_name = p;
    strcpy(p, p_se->s_name);
    p += strlen(p) + 1;

    p_to->s_proto = p;
    strcpy(p, p_se->s_proto);
    p += strlen(p) + 1;

    p_to->s_aliases = (char **)p;
    list_dup(p_se->s_aliases, p_to->s_aliases, 0);
    return p_to;
}

/* ----------------------------------- error handling */

UINT wsaErrno(void)
{
    int	loc_errno = errno;
    WARN("errno %d, (%s).\n", loc_errno, strerror(loc_errno));

    switch(loc_errno)
    {
	case EINTR:		return WSAEINTR;
	case EBADF:		return WSAEBADF;
	case EPERM:
	case EACCES:		return WSAEACCES;
	case EFAULT:		return WSAEFAULT;
	case EINVAL:		return WSAEINVAL;
	case EMFILE:		return WSAEMFILE;
	case EWOULDBLOCK:	return WSAEWOULDBLOCK;
	case EINPROGRESS:	return WSAEINPROGRESS;
	case EALREADY:		return WSAEALREADY;
	case ENOTSOCK:		return WSAENOTSOCK;
	case EDESTADDRREQ:	return WSAEDESTADDRREQ;
	case EMSGSIZE:		return WSAEMSGSIZE;
	case EPROTOTYPE:	return WSAEPROTOTYPE;
	case ENOPROTOOPT:	return WSAENOPROTOOPT;
	case EPROTONOSUPPORT:	return WSAEPROTONOSUPPORT;
	case ESOCKTNOSUPPORT:	return WSAESOCKTNOSUPPORT;
	case EOPNOTSUPP:	return WSAEOPNOTSUPP;
	case EPFNOSUPPORT:	return WSAEPFNOSUPPORT;
	case EAFNOSUPPORT:	return WSAEAFNOSUPPORT;
	case EADDRINUSE:	return WSAEADDRINUSE;
	case EADDRNOTAVAIL:	return WSAEADDRNOTAVAIL;
	case ENETDOWN:		return WSAENETDOWN;
	case ENETUNREACH:	return WSAENETUNREACH;
	case ENETRESET:		return WSAENETRESET;
	case ECONNABORTED:	return WSAECONNABORTED;
	case EPIPE:
	case ECONNRESET:	return WSAECONNRESET;
	case ENOBUFS:		return WSAENOBUFS;
	case EISCONN:		return WSAEISCONN;
	case ENOTCONN:		return WSAENOTCONN;
	case ESHUTDOWN:		return WSAESHUTDOWN;
	case ETOOMANYREFS:	return WSAETOOMANYREFS;
	case ETIMEDOUT:		return WSAETIMEDOUT;
	case ECONNREFUSED:	return WSAECONNREFUSED;
	case ELOOP:		return WSAELOOP;
	case ENAMETOOLONG:	return WSAENAMETOOLONG;
	case EHOSTDOWN:		return WSAEHOSTDOWN;
	case EHOSTUNREACH:	return WSAEHOSTUNREACH;
	case ENOTEMPTY:		return WSAENOTEMPTY;
#ifdef EPROCLIM
	case EPROCLIM:		return WSAEPROCLIM;
#endif
#ifdef EUSERS
	case EUSERS:		return WSAEUSERS;
#endif
#ifdef EDQUOT
	case EDQUOT:		return WSAEDQUOT;
#endif
#ifdef ESTALE
	case ESTALE:		return WSAESTALE;
#endif
#ifdef EREMOTE
	case EREMOTE:		return WSAEREMOTE;
#endif

       /* just in case we ever get here and there are no problems */
	case 0:			return 0;
        default:
		WARN("Unknown errno %d!\n", loc_errno);
		return WSAEOPNOTSUPP;
    }
}

UINT wsaHerrno(int loc_errno)
{

    WARN("h_errno %d.\n", loc_errno);

    switch(loc_errno)
    {
	case HOST_NOT_FOUND:	return WSAHOST_NOT_FOUND;
	case TRY_AGAIN:		return WSATRY_AGAIN;
	case NO_RECOVERY:	return WSANO_RECOVERY;
	case NO_DATA:		return WSANO_DATA;
	case ENOBUFS:		return WSAENOBUFS;

	case 0:			return 0;
        default:
		WARN("Unknown h_errno %d!\n", loc_errno);
		return WSAEOPNOTSUPP;
    }
}


/***********************************************************************
 *		WSARecv			(WS2_32.67)
 */
int WINAPI WSARecv(SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
                   LPDWORD NumberOfBytesReceived, LPDWORD lpFlags,
                   LPWSAOVERLAPPED lpOverlapped,
                   LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
{
    return WSARecvFrom(s, lpBuffers, dwBufferCount, NumberOfBytesReceived, lpFlags,
                       NULL, NULL, lpOverlapped, lpCompletionRoutine);
}

/***********************************************************************
 *              WSARecvFrom             (WS2_32.69)
 */
INT WINAPI WSARecvFrom( SOCKET s, LPWSABUF lpBuffers, DWORD dwBufferCount,
                        LPDWORD lpNumberOfBytesRecvd, LPDWORD lpFlags, struct WS_sockaddr *lpFrom,
                        LPINT lpFromlen, LPWSAOVERLAPPED lpOverlapped,
                        LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine )

{
    unsigned int i;
    int n, fd, err = WSAENOTSOCK, flags, ret;
    struct iovec* iovec;
    struct ws2_async *wsa;
    IO_STATUS_BLOCK* iosb;

    TRACE("socket %04x, wsabuf %p, nbufs %ld, flags %ld, from %p, fromlen %ld, ovl %p, func %p\n",
          s, lpBuffers, dwBufferCount, *lpFlags, lpFrom,
          (lpFromlen ? *lpFromlen : -1L),
          lpOverlapped, lpCompletionRoutine);

    fd = get_sock_fd( s, GENERIC_READ, &flags );
    TRACE( "fd=%d, flags=%x\n", fd, flags );

    if (fd == -1) return SOCKET_ERROR;

    if (flags & FD_FLAG_RECV_SHUTDOWN)
    {
        WSASetLastError( WSAESHUTDOWN );
        goto err_close;
    }

    iovec = HeapAlloc( GetProcessHeap(), 0, dwBufferCount * sizeof (struct iovec) );
    if ( !iovec )
    {
        err = WSAEFAULT;
        goto err_close;
    }

    for (i = 0; i < dwBufferCount; i++)
    {
        iovec[i].iov_base = lpBuffers[i].buf;
        iovec[i].iov_len  = lpBuffers[i].len;
    }

    if ( (lpOverlapped || lpCompletionRoutine) && flags & FD_FLAG_OVERLAPPED )
    {
        wsa = WS2_make_async( s, fd, ws2m_read, iovec, dwBufferCount,
                              lpFlags, lpFrom, lpFromlen,
                              lpOverlapped, lpCompletionRoutine, &iosb );

        if ( !wsa )
        {
            err = WSAEFAULT;
            goto err_free;
        }

        if ( ( ret = ws2_queue_async( wsa, iosb )) )
        {
            err = NtStatusToWSAError( ret );

            if ( !lpOverlapped )
                HeapFree( GetProcessHeap(), 0, iosb );
            HeapFree( GetProcessHeap(), 0, wsa );
            goto err_free;
        }

        /* Try immediate completion */
        if ( lpOverlapped )
        {
            if  ( WSAGetOverlappedResult( s, lpOverlapped,
                                           lpNumberOfBytesRecvd, FALSE, lpFlags) )
                return 0;

            if ( (err = WSAGetLastError()) != WSA_IO_INCOMPLETE )
                goto error;
        }

        WSASetLastError( WSA_IO_PENDING );
        return SOCKET_ERROR;
    }

    if ( _is_blocking(s) )
    {
        /* block here */
        /* FIXME: OOB and exceptfds? */
        int timeout = GET_RCVTIMEO(fd);
        if( !do_block(fd, POLLIN, timeout)) {
            err = WSAETIMEDOUT;
            /* a timeout is not fatal */
            _enable_event(SOCKET2HANDLE(s), FD_READ, 0, 0);
            goto err_free;
        }
    }

    n = WS2_recv( fd, iovec, dwBufferCount, lpFrom, lpFromlen, lpFlags );
    if ( n == -1 )
    {
        err = wsaErrno();
        goto err_free;
    }

    TRACE(" -> %i bytes\n", n);
    *lpNumberOfBytesRecvd = n;

    HeapFree(GetProcessHeap(), 0, iovec);
    release_sock_fd( s, fd );
    _enable_event(SOCKET2HANDLE(s), FD_READ, 0, 0);

    return 0;

err_free:
    HeapFree(GetProcessHeap(), 0, iovec);

err_close:
    release_sock_fd( s, fd );

error:
    WARN(" -> ERROR %d\n", err);
    WSASetLastError( err );
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSCInstallProvider             (WS2_32.88)
 */
INT WINAPI WSCInstallProvider( const LPGUID lpProviderId,
                               LPCWSTR lpszProviderDllPath,
                               const LPWSAPROTOCOL_INFOW lpProtocolInfoList,
                               DWORD dwNumberOfEntries,
                               LPINT lpErrno )
{
    FIXME("(%s, %s, %p, %ld, %p): stub !\n", debugstr_guid(lpProviderId),
          debugstr_w(lpszProviderDllPath), lpProtocolInfoList,
          dwNumberOfEntries, lpErrno);
    *lpErrno = 0;
    return 0;
}


/***********************************************************************
 *              WSCDeinstallProvider             (WS2_32.83)
 */
INT WINAPI WSCDeinstallProvider(LPGUID lpProviderId, LPINT lpErrno)
{
    FIXME("(%s, %p): stub !\n", debugstr_guid(lpProviderId), lpErrno);
    *lpErrno = 0;
    return 0;
}


/***********************************************************************
 *              WSAAccept                        (WS2_32.26)
 */
SOCKET WINAPI WSAAccept( SOCKET s, struct WS_sockaddr *addr, LPINT addrlen,
               LPCONDITIONPROC lpfnCondition, DWORD dwCallbackData)
{

       int ret = 0, size = 0;
       WSABUF CallerId, CallerData, CalleeId, CalleeData;
       /*        QOS SQOS, GQOS; */
       GROUP g;
       SOCKET cs;
       SOCKADDR src_addr, dst_addr;

       TRACE("Socket  %04x, sockaddr %p, addrlen %p, fnCondition %p, dwCallbackData %ld\n",
               s, addr, addrlen, lpfnCondition, dwCallbackData);


       size = sizeof(src_addr);
       cs = WS_accept(s, &src_addr, &size);

       if (cs == SOCKET_ERROR) return SOCKET_ERROR;

       CallerId.buf = (char *)&src_addr;
       CallerId.len = sizeof(src_addr);

       CallerData.buf = NULL;
       CallerData.len = (ULONG)NULL;

       WS_getsockname(cs, &dst_addr, &size);

       CalleeId.buf = (char *)&dst_addr;
       CalleeId.len = sizeof(dst_addr);


       ret = (*lpfnCondition)(&CallerId, &CallerData, NULL, NULL,
                       &CalleeId, &CalleeData, &g, dwCallbackData);

       switch (ret)
       {
               case CF_ACCEPT:
                       if (addr && addrlen)
                               addr = memcpy(addr, &src_addr, (*addrlen > size) ?  size : *addrlen );
                       return cs;
               case CF_DEFER:
                       SERVER_START_REQ( set_socket_deferred )
                       {
                           req->handle = SOCKET2HANDLE(s);
                           req->deferred = SOCKET2HANDLE(cs);
                           if ( !wine_server_call_err ( req ) )
                           {
                               SetLastError( WSATRY_AGAIN );
                               WS_closesocket( cs );
                           }
                       }
                       SERVER_END_REQ;
                       return SOCKET_ERROR;
               case CF_REJECT:
                       WS_closesocket(cs);
                       SetLastError(WSAECONNREFUSED);
                       return SOCKET_ERROR;
               default:
                       FIXME("Unknown return type from Condition function\n");
                       SetLastError(WSAENOTSOCK);
                       return SOCKET_ERROR;
               }
}

/***********************************************************************
 *              WSADuplicateSocketA                      (WS2_32.32)
 */
int WINAPI WSADuplicateSocketA( SOCKET s, DWORD dwProcessId, LPWSAPROTOCOL_INFOA lpProtocolInfo )
{
   HANDLE hProcess;

   TRACE("(%d,%lx,%p)\n", s, dwProcessId, lpProtocolInfo);
   memset(lpProtocolInfo, 0, sizeof(*lpProtocolInfo));
   /* FIXME: WS_getsockopt(s, WS_SOL_SOCKET, SO_PROTOCOL_INFO, lpProtocolInfo, sizeof(*lpProtocolInfo)); */
   /* I don't know what the real Windoze does next, this is a hack */
   /* ...we could duplicate and then use ConvertToGlobalHandle on the duplicate, then let
    * the target use the global duplicate, or we could copy a reference to us to the structure
    * and let the target duplicate it from us, but let's do it as simple as possible */
   hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessId);
   DuplicateHandle(GetCurrentProcess(), SOCKET2HANDLE(s),
                   hProcess, (LPHANDLE)&lpProtocolInfo->dwCatalogEntryId,
                   0, FALSE, DUPLICATE_SAME_ACCESS);
   CloseHandle(hProcess);
   lpProtocolInfo->dwServiceFlags4 = 0xff00ff00; /* magic */
   return 0;
}

/***********************************************************************
 *              WSADuplicateSocketW                      (WS2_32.33)
 */
int WINAPI WSADuplicateSocketW( SOCKET s, DWORD dwProcessId, LPWSAPROTOCOL_INFOW lpProtocolInfo )
{
   HANDLE hProcess;

   TRACE("(%d,%lx,%p)\n", s, dwProcessId, lpProtocolInfo);

   memset(lpProtocolInfo, 0, sizeof(*lpProtocolInfo));
   hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, dwProcessId);
   DuplicateHandle(GetCurrentProcess(), SOCKET2HANDLE(s),
                   hProcess, (LPHANDLE)&lpProtocolInfo->dwCatalogEntryId,
                   0, FALSE, DUPLICATE_SAME_ACCESS);
   CloseHandle(hProcess);
   lpProtocolInfo->dwServiceFlags4 = 0xff00ff00; /* magic */
   return 0;
}

/***********************************************************************
 *              WSAInstallServiceClassA                  (WS2_32.48)
 */
int WINAPI WSAInstallServiceClassA(LPWSASERVICECLASSINFOA info)
{
    FIXME("Request to install service %s\n",debugstr_a(info->lpszServiceClassName));
    WSASetLastError(WSAEACCES);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSAInstallServiceClassW                  (WS2_32.49)
 */
int WINAPI WSAInstallServiceClassW(LPWSASERVICECLASSINFOW info)
{
    FIXME("Request to install service %s\n",debugstr_w(info->lpszServiceClassName));
    WSASetLastError(WSAEACCES);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSARemoveServiceClass                    (WS2_32.70)
 */
int WINAPI WSARemoveServiceClass(LPGUID info)
{
    FIXME("Request to remove service %p\n",info);
    WSASetLastError(WSATYPE_NOT_FOUND);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSAStringToAddressA                      (WS2_32.80)
 */
INT WINAPI WSAStringToAddressA(LPSTR AddressString,
                               INT AddressFamily,
                               LPWSAPROTOCOL_INFOA lpProtocolInfo,
                               LPSOCKADDR lpAddress,
                               LPINT lpAddressLength)
{
    INT res=0;
    struct in_addr inetaddr;
    LPSTR workBuffer=NULL,ptrPort;

    TRACE( "(%s, %x, %p, %p, %p)\n", AddressString, AddressFamily, lpProtocolInfo,
           lpAddress, lpAddressLength );

    if (!lpAddressLength || !lpAddress) return SOCKET_ERROR;

    if (AddressString)
    {
        workBuffer = HeapAlloc( GetProcessHeap(), 0, strlen(AddressString)+1 );
        if (workBuffer)
        {
            strcpy(workBuffer,AddressString);
            switch (AddressFamily)
            {
            case AF_INET:
                /* caller wants to know the size of the socket buffer */
                if (*lpAddressLength < sizeof(SOCKADDR_IN))
                {
                    *lpAddressLength = sizeof(SOCKADDR_IN);
                    res = WSAEFAULT;
                }
                else
                {
                    /* caller wants to translate an AdressString into a SOCKADDR */
                    if (lpAddress)
                    {
                        memset(lpAddress,0,sizeof(SOCKADDR_IN));
                        ((LPSOCKADDR_IN)lpAddress)->sin_family = AF_INET;
                        ptrPort = strchr(workBuffer,':');
                        if (ptrPort)
                        {
                            ((LPSOCKADDR_IN)lpAddress)->sin_port = (u_short)atoi(ptrPort+1);
                            *ptrPort = '\0';
                        }
                        else
                            ((LPSOCKADDR_IN)lpAddress)->sin_port = 0;
                        if (inet_aton(workBuffer, &inetaddr) > 0)
                        {
                            ((LPSOCKADDR_IN)lpAddress)->sin_addr.WS_s_addr = inetaddr.s_addr;
                            res = 0;
                        }
                        else
                            res = WSAEINVAL;
                    }
                }
                if (lpProtocolInfo)
                    FIXME("(%s, %x, %p, %p, %p) - ProtocolInfo not implemented!\n",
                        AddressString, AddressFamily,
                        lpProtocolInfo, lpAddress, lpAddressLength);

                break;
            default:
                FIXME("(%s, %x, %p, %p, %p) - AddressFamiliy not implemented!\n",
                    AddressString, AddressFamily,
                    lpProtocolInfo, lpAddress, lpAddressLength);
            }
            HeapFree( GetProcessHeap(), 0, workBuffer );
        }
        else
            res = WSA_NOT_ENOUGH_MEMORY;
    }
    else
        res = WSAEINVAL;

    if (!res) return 0;
    WSASetLastError(res);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSAStringToAddressW                      (WS2_32.81)
 *
 * Does anybody know if this functions allows to use hebrew/arabic/chinese... digits?
 * If this should be the case, it would be required to map these digits
 * to Unicode digits (0-9) using FoldString first.
 */
INT WINAPI WSAStringToAddressW(LPWSTR AddressString,
                               INT AddressFamily,
                               LPWSAPROTOCOL_INFOW lpProtocolInfo,
                               LPSOCKADDR lpAddress,
                               LPINT lpAddressLength)
{
    INT sBuffer,res=0;
    LPSTR workBuffer=NULL;
    WSAPROTOCOL_INFOA infoA;
    LPWSAPROTOCOL_INFOA lpProtoInfoA = NULL;

    TRACE( "(%s, %x, %p, %p, %p)\n", debugstr_w(AddressString), AddressFamily, lpProtocolInfo,
           lpAddress, lpAddressLength );

    if (!lpAddressLength || !lpAddress) return SOCKET_ERROR;

    /* if ProtocolInfo is available - convert to ANSI variant */
    if (lpProtocolInfo)
    {
        lpProtoInfoA = &infoA;
        memcpy( lpProtoInfoA, lpProtocolInfo, FIELD_OFFSET( WSAPROTOCOL_INFOA, szProtocol ) );

        if (!WideCharToMultiByte( CP_ACP, 0, lpProtocolInfo->szProtocol, -1,
                                  lpProtoInfoA->szProtocol, WSAPROTOCOL_LEN+1, NULL, NULL ))
        {
            WSASetLastError( WSAEINVAL);
            return SOCKET_ERROR;
        }
    }

    if (AddressString)
    {
        /* Translate AddressString to ANSI code page - assumes that only
           standard digits 0-9 are used with this API call */
        sBuffer = WideCharToMultiByte( CP_ACP, 0, AddressString, -1, NULL, 0, NULL, NULL );
        workBuffer = HeapAlloc( GetProcessHeap(), 0, sBuffer );

        if (workBuffer)
        {
            WideCharToMultiByte( CP_ACP, 0, AddressString, -1, workBuffer, sBuffer, NULL, NULL );
            res = WSAStringToAddressA(workBuffer,AddressFamily,lpProtoInfoA,
                                      lpAddress,lpAddressLength);
            HeapFree( GetProcessHeap(), 0, workBuffer );
            return res;
        }
        else
            res = WSA_NOT_ENOUGH_MEMORY;
    }
    else
        res = WSAEINVAL;

    WSASetLastError(res);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSAAddressToStringA                      (WS2_32.27)
 *
 *  See WSAAddressToStringW
 */
INT WINAPI WSAAddressToStringA( LPSOCKADDR sockaddr, DWORD len,
                                LPWSAPROTOCOL_INFOA info, LPSTR string,
                                LPDWORD lenstr )
{
    INT size;
    CHAR buffer[22]; /* 12 digits + 3 dots + ':' + 5 digits + '\0' */
    CHAR *p;

    TRACE( "(%p, %ld, %p, %p, %p)\n", sockaddr, len, info, string, lenstr );

    if (!sockaddr || len < sizeof(SOCKADDR_IN)) return SOCKET_ERROR;
    if (!string || !lenstr) return SOCKET_ERROR;

    /* sin_family is garanteed to be the first u_short */
    if (((SOCKADDR_IN *)sockaddr)->sin_family != AF_INET) return SOCKET_ERROR;

    sprintf( buffer, "%u.%u.%u.%u:%u",
             (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) >> 24 & 0xff),
             (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) >> 16 & 0xff),
             (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) >> 8 & 0xff),
             (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) & 0xff),
             ntohs( ((SOCKADDR_IN *)sockaddr)->sin_port ) );

    p = strchr( buffer, ':' );
    if (!((SOCKADDR_IN *)sockaddr)->sin_port) *p = 0;

    size = strlen( buffer );

    if (*lenstr <  size)
    {
        *lenstr = size;
        WSASetLastError(WSAEFAULT);
        return SOCKET_ERROR;
    }

    strcpy( string, buffer );
    return 0;
}

/***********************************************************************
 *              WSAAddressToStringW                      (WS2_32.28)
 *
 * Convert a sockaddr address into a readable address string. 
 *
 * PARAMS
 *  sockaddr [I]    Pointer to a sockaddr structure.
 *  len      [I]    Size of the sockaddr structure.
 *  info     [I]    Pointer to a WSAPROTOCOL_INFOW structure (optional).
 *  string   [I/O]  Pointer to a buffer to receive the address string.
 *  lenstr   [I/O]  Size of the receive buffer in WCHARs.
 *
 * RETURNS
 *  Success: 0
 *  Failure: SOCKET_ERROR
 *
 * NOTES
 *  The 'info' parameter is ignored.
 *
 * BUGS
 *  Only supports AF_INET addresses.
 */
INT WINAPI WSAAddressToStringW( LPSOCKADDR sockaddr, DWORD len,
                                LPWSAPROTOCOL_INFOW info, LPWSTR string,
                                LPDWORD lenstr )
{
    INT size;
    WCHAR buffer[22]; /* 12 digits + 3 dots + ':' + 5 digits + '\0' */
    static const WCHAR format[] = { '%','u','.','%','u','.','%','u','.','%','u',':','%','u',0 };
    WCHAR *p;

    TRACE( "(%p, %lx, %p, %p, %p)\n", sockaddr, len, info, string, lenstr );

    if (!sockaddr || len < sizeof(SOCKADDR_IN)) return SOCKET_ERROR;
    if (!string || !lenstr) return SOCKET_ERROR;

    /* sin_family is garanteed to be the first u_short */
    if (((SOCKADDR_IN *)sockaddr)->sin_family != AF_INET) return SOCKET_ERROR;

    sprintfW( buffer, format,
              (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) >> 24 & 0xff),
              (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) >> 16 & 0xff),
              (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) >> 8 & 0xff),
              (unsigned int)(ntohl( ((SOCKADDR_IN *)sockaddr)->sin_addr.WS_s_addr ) & 0xff),
              ntohs( ((SOCKADDR_IN *)sockaddr)->sin_port ) );

    p = strchrW( buffer, ':' );
    if (!((SOCKADDR_IN *)sockaddr)->sin_port) *p = 0;

    size = lstrlenW( buffer );

    if (*lenstr <  size)
    {
        *lenstr = size;
        return SOCKET_ERROR;
    }

    lstrcpyW( string, buffer );
    return 0;
}

/***********************************************************************
 *              WSAEnumNameSpaceProvidersA                  (WS2_32.34)
 */
INT WINAPI WSAEnumNameSpaceProvidersA( LPDWORD len, LPWSANAMESPACE_INFOA buffer )
{
    FIXME( "(%p %p) Stub!\n", len, buffer );
    return 0;
}

/***********************************************************************
 *              WSAEnumNameSpaceProvidersW                  (WS2_32.35)
 */
INT WINAPI WSAEnumNameSpaceProvidersW( LPDWORD len, LPWSANAMESPACE_INFOW buffer )
{
    FIXME( "(%p %p) Stub!\n", len, buffer );
    return 0;
}

/***********************************************************************
 *              WSAGetQOSByName                             (WS2_32.41)
 */
BOOL WINAPI WSAGetQOSByName( SOCKET s, LPWSABUF lpQOSName, LPQOS lpQOS )
{
    FIXME( "(0x%04x %p %p) Stub!\n", s, lpQOSName, lpQOS );
    return FALSE;
}

/***********************************************************************
 *              WSAGetServiceClassInfoA                     (WS2_32.42)
 */
INT WINAPI WSAGetServiceClassInfoA( LPGUID provider, LPGUID service, LPDWORD len,
                                    LPWSASERVICECLASSINFOA info )
{
    FIXME( "(%s %s %p %p) Stub!\n", debugstr_guid(provider), debugstr_guid(service),
           len, info );
    WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
    return SOCKET_ERROR; 
}

/***********************************************************************
 *              WSAGetServiceClassInfoW                     (WS2_32.43)
 */
INT WINAPI WSAGetServiceClassInfoW( LPGUID provider, LPGUID service, LPDWORD len,
                                    LPWSASERVICECLASSINFOW info )
{
    FIXME( "(%s %s %p %p) Stub!\n", debugstr_guid(provider), debugstr_guid(service),
           len, info );
    WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSAGetServiceClassNameByClassIdA            (WS2_32.44)
 */
INT WINAPI WSAGetServiceClassNameByClassIdA( LPGUID class, LPSTR service, LPDWORD len )
{
    FIXME( "(%s %p %p) Stub!\n", debugstr_guid(class), service, len );
    WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSAGetServiceClassNameByClassIdW            (WS2_32.45)
 */
INT WINAPI WSAGetServiceClassNameByClassIdW( LPGUID class, LPWSTR service, LPDWORD len )
{
    FIXME( "(%s %p %p) Stub!\n", debugstr_guid(class), service, len );
    WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSALookupServiceBeginA                       (WS2_32.59)
 */
INT WINAPI WSALookupServiceBeginA( LPWSAQUERYSETA lpqsRestrictions,
                                   DWORD dwControlFlags,
                                   LPHANDLE lphLookup)
{
    FIXME("(%p 0x%08lx %p) Stub!\n", lpqsRestrictions, dwControlFlags,
            lphLookup);
    WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSALookupServiceBeginW                       (WS2_32.60)
 */
INT WINAPI WSALookupServiceBeginW( LPWSAQUERYSETW lpqsRestrictions,
                                   DWORD dwControlFlags,
                                   LPHANDLE lphLookup)
{
    FIXME("(%p 0x%08lx %p) Stub!\n", lpqsRestrictions, dwControlFlags,
            lphLookup);
    WSASetLastError(WSA_NOT_ENOUGH_MEMORY);
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSALookupServiceBeginW                       (WS2_32.61)
 */
INT WINAPI WSALookupServiceEnd( HANDLE lookup )
{
    FIXME("(%p) Stub!\n", lookup );
    return 0;
}

/***********************************************************************
 *              WSALookupServiceNextA                       (WS2_32.62)
 */
INT WINAPI WSALookupServiceNextA( HANDLE lookup, DWORD flags, LPDWORD len, LPWSAQUERYSETA results )
{
    FIXME( "(%p 0x%08lx %p %p) Stub!\n", lookup, flags, len, results );
    return 0;
}

/***********************************************************************
 *              WSALookupServiceNextW                       (WS2_32.63)
 */
INT WINAPI WSALookupServiceNextW( HANDLE lookup, DWORD flags, LPDWORD len, LPWSAQUERYSETW results )
{
    FIXME( "(%p 0x%08lx %p %p) Stub!\n", lookup, flags, len, results );
    return 0;
}

/***********************************************************************
 *              WSANtohl                                   (WS2_32.64)
 */
INT WINAPI WSANtohl( SOCKET s, u_long netlong, u_long* lphostlong )
{
    TRACE( "(0x%04x 0x%08lx %p)\n", s, netlong, lphostlong );

    if (!lphostlong) return WSAEFAULT;

    *lphostlong = ntohl( netlong );
    return 0;
}

/***********************************************************************
 *              WSANtohs                                   (WS2_32.65)
 */
INT WINAPI WSANtohs( SOCKET s, u_short netshort, u_short* lphostshort )
{
    TRACE( "(0x%04x 0x%08x %p)\n", s, netshort, lphostshort );

    if (!lphostshort) return WSAEFAULT;

    *lphostshort = ntohs( netshort );
    return 0;
}

/***********************************************************************
 *              WSAProviderConfigChange                     (WS2_32.66)
 */
INT WINAPI WSAProviderConfigChange( LPHANDLE handle, LPWSAOVERLAPPED overlapped,
                                    LPWSAOVERLAPPED_COMPLETION_ROUTINE completion )
{
    FIXME( "(%p %p %p) Stub!\n", handle, overlapped, completion );
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSARecvDisconnect                           (WS2_32.68)
 */
INT WINAPI WSARecvDisconnect( SOCKET s, LPWSABUF disconnectdata )
{
    TRACE( "(0x%04x %p)\n", s, disconnectdata ); 

    return WS_shutdown( s, 0 );
}

/***********************************************************************
 *              WSASetServiceA                              (WS2_32.76)
 */
INT WINAPI WSASetServiceA( LPWSAQUERYSETA query, WSAESETSERVICEOP operation, DWORD flags )
{
    FIXME( "(%p 0x%08x 0x%08lx) Stub!\n", query, operation, flags );
    return 0;
}

/***********************************************************************
 *              WSASetServiceW                              (WS2_32.77)
 */
INT WINAPI WSASetServiceW( LPWSAQUERYSETW query, WSAESETSERVICEOP operation, DWORD flags )
{
    FIXME( "(%p 0x%08x 0x%08lx) Stub!\n", query, operation, flags );
    return 0;
}

/***********************************************************************
 *              WSCEnableNSProvider                         (WS2_32.84)
 */
INT WINAPI WSCEnableNSProvider( LPGUID provider, BOOL enable )
{
    FIXME( "(%s 0x%08x) Stub!\n", debugstr_guid(provider), enable );
    return 0;
}

/***********************************************************************
 *              WSCGetProviderPath                          (WS2_32.86)
 */
INT WINAPI WSCGetProviderPath( LPGUID provider, LPWSTR path, LPINT len, LPINT errcode )
{
    FIXME( "(%s %p %p %p) Stub!\n", debugstr_guid(provider), path, len, errcode );

    if (!errcode || !provider || !len) return WSAEFAULT;

    *errcode = WSAEINVAL;
    return SOCKET_ERROR;
}

/***********************************************************************
 *              WSCInstallNameSpace                         (WS2_32.87)
 */
INT WINAPI WSCInstallNameSpace( LPWSTR identifier, LPWSTR path, DWORD namespace,
                                DWORD version, LPGUID provider )
{
    FIXME( "(%s %s 0x%08lx 0x%08lx %s) Stub!\n", debugstr_w(identifier), debugstr_w(path),
           namespace, version, debugstr_guid(provider) );
    return 0;
}

/***********************************************************************
 *              WSCUnInstallNameSpace                       (WS2_32.89)
 */
INT WINAPI WSCUnInstallNameSpace( LPGUID lpProviderId )
{
    FIXME("(%p) Stub!\n", lpProviderId);
    return NO_ERROR;
}

/***********************************************************************
 *              WSCWriteProviderOrder                       (WS2_32.91)
 */
INT WINAPI WSCWriteProviderOrder( LPDWORD entry, DWORD number )
{
    FIXME("(%p 0x%08lx) Stub!\n", entry, number);
    return 0;
}
