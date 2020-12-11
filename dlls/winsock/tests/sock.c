/*
 * Unit test suite for winsock functions
 *
 * Copyright 2002 Martin Wilck
 * Copyright 2005 Thomas Kho
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
 */

#include <stdarg.h>

#include <windef.h>
#include <winbase.h>
#include <winsock2.h>
#include <mswsock.h>
#include "wine/test.h"
#include <winnt.h>
#include <winerror.h>

#define MAX_CLIENTS 4      /* Max number of clients */
#define NUM_TESTS   3      /* Number of tests performed */
#define FIRST_CHAR 'A'     /* First character in transferred pattern */
#define BIND_SLEEP 10      /* seconds to wait between attempts to bind() */
#define BIND_TRIES 6       /* Number of bind() attempts */
#define TEST_TIMEOUT 30    /* seconds to wait before killing child threads
                              after server initialization, if something hangs */

#define NUM_UDP_PEERS 3    /* Number of UDP sockets to create and test > 1 */

#define NUM_THREADS 3      /* Number of threads to run getservbyname */
#define NUM_QUERIES 250    /* Number of getservbyname queries per thread */

#define SERVERIP "127.0.0.1"   /* IP to bind to */
#define SERVERPORT 9374        /* Port number to bind to */

#define wsa_ok(op, cond, msg) \
   do { \
        int tmp, err = 0; \
        tmp = op; \
        if ( !(cond tmp) ) err = WSAGetLastError(); \
        ok ( cond tmp, msg, GetCurrentThreadId(), err); \
   } while (0);


/**************** Structs and typedefs ***************/

typedef struct thread_info
{
    HANDLE thread;
    DWORD id;
} thread_info;

/* Information in the server about open client connections */
typedef struct sock_info
{
    SOCKET                 s;
    struct sockaddr_in     addr;
    struct sockaddr_in     peer;
    char                  *buf;
    int                    n_recvd;
    int                    n_sent;
} sock_info;

/* Test parameters for both server & client */
typedef struct test_params
{
    int          sock_type;
    int          sock_prot;
    const char  *inet_addr;
    short        inet_port;
    int          chunk_size;
    int          n_chunks;
    int          n_clients;
} test_params;

/* server-specific test parameters */
typedef struct server_params
{
    test_params   *general;
    DWORD          sock_flags;
    int            buflen;
} server_params;

/* client-specific test parameters */
typedef struct client_params
{
    test_params   *general;
    DWORD          sock_flags;
    int            buflen;
} client_params;

/* This type combines all information for setting up a test scenario */
typedef struct test_setup
{
    test_params              general;
    LPVOID                   srv;
    server_params            srv_params;
    LPVOID                   clt;
    client_params            clt_params;
} test_setup;

/* Thread local storage for server */
typedef struct server_memory
{
    SOCKET                  s;
    struct sockaddr_in      addr;
    sock_info               sock[MAX_CLIENTS];
} server_memory;

/* Thread local storage for client */
typedef struct client_memory
{
    SOCKET s;
    struct sockaddr_in      addr;
    char                   *send_buf;
    char                   *recv_buf;
} client_memory;

/**************** Static variables ***************/

static DWORD      tls;              /* Thread local storage index */
static HANDLE     thread[1+MAX_CLIENTS];
static DWORD      thread_id[1+MAX_CLIENTS];
static HANDLE     server_ready;
static HANDLE     client_ready[MAX_CLIENTS];
static int        client_id;

/**************** General utility functions ***************/

static void set_so_opentype ( BOOL overlapped )
{
    int optval = !overlapped, newval, len = sizeof (int);

    ok ( setsockopt ( INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE,
                      (LPVOID) &optval, sizeof (optval) ) == 0,
         "setting SO_OPENTYPE failed\n" );
    ok ( getsockopt ( INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE,
                      (LPVOID) &newval, &len ) == 0,
         "getting SO_OPENTYPE failed\n" );
    ok ( optval == newval, "failed to set SO_OPENTYPE\n" );
}

static int set_blocking ( SOCKET s, BOOL blocking )
{
    u_long val = !blocking;
    return ioctlsocket ( s, FIONBIO, &val );
}

static void fill_buffer ( char *buf, int chunk_size, int n_chunks )
{
    char c, *p;
    for ( c = FIRST_CHAR, p = buf; c < FIRST_CHAR + n_chunks; c++, p += chunk_size )
        memset ( p, c, chunk_size );
}

static char* test_buffer ( char *buf, int chunk_size, int n_chunks )
{
    char c, *p;
    int i;
    for ( c = FIRST_CHAR, p = buf; c < FIRST_CHAR + n_chunks; c++, p += chunk_size )
    {
        for ( i = 0; i < chunk_size; i++ )
            if ( p[i] != c ) return p + i;
    }
    return NULL;
}

/*
 * This routine is called when a client / server does not expect any more data,
 * but needs to acknowedge the closing of the connection (by reasing 0 bytes).
 */
static void read_zero_bytes ( SOCKET s )
{
    char buf[256];
    int tmp, n = 0;
    while ( ( tmp = recv ( s, buf, 256, 0 ) ) > 0 )
        n += tmp;
    ok ( n <= 0, "garbage data received: %d bytes\n", n );
}

static int do_synchronous_send ( SOCKET s, char *buf, int buflen, int sendlen )
{
    char* last = buf + buflen, *p;
    int n = 1;
    for ( p = buf; n > 0 && p < last; p += n )
        n = send ( s, p, min ( sendlen, last - p ), 0 );
    wsa_ok ( n, 0 <=, "do_synchronous_send (%lx): error %d\n" );
    return p - buf;
}

static int do_synchronous_recv ( SOCKET s, char *buf, int buflen, int recvlen )
{
    char* last = buf + buflen, *p;
    int n = 1;
    for ( p = buf; n > 0 && p < last; p += n )
        n = recv ( s, p, min ( recvlen, last - p ), 0 );
    wsa_ok ( n, 0 <=, "do_synchronous_recv (%lx): error %d:\n" );
    return p - buf;
}

/*
 *  Call this routine right after thread startup.
 *  SO_OPENTYPE must by 0, regardless what the server did.
 */
static void check_so_opentype (void)
{
    int tmp = 1, len;
    len = sizeof (tmp);
    getsockopt ( INVALID_SOCKET, SOL_SOCKET, SO_OPENTYPE, (LPVOID) &tmp, &len );
    ok ( tmp == 0, "check_so_opentype: wrong startup value of SO_OPENTYPE: %d\n", tmp );
}

/**************** Server utility functions ***************/

/*
 *  Even if we have closed our server socket cleanly,
 *  the OS may mark the address "in use" for some time -
 *  this happens with native Linux apps, too.
 */
static void do_bind ( SOCKET s, struct sockaddr* addr, int addrlen )
{
    int err, wsaerr = 0, n_try = BIND_TRIES;

    while ( ( err = bind ( s, addr, addrlen ) ) != 0 &&
            ( wsaerr = WSAGetLastError () ) == WSAEADDRINUSE &&
            n_try-- >= 0)
    {
        trace ( "address in use, waiting ...\n" );
        Sleep ( 1000 * BIND_SLEEP );
    }
    ok ( err == 0, "failed to bind: %d\n", wsaerr );
}

static void server_start ( server_params *par )
{
    int i;
    test_params *gen = par->general;
    server_memory *mem = (LPVOID) LocalAlloc ( LPTR, sizeof (server_memory));

    TlsSetValue ( tls, mem );
    mem->s = WSASocketA ( AF_INET, gen->sock_type, gen->sock_prot,
                          NULL, 0, par->sock_flags );
    ok ( mem->s != INVALID_SOCKET, "Server: WSASocket failed\n" );

    mem->addr.sin_family = AF_INET;
    mem->addr.sin_addr.s_addr = inet_addr ( gen->inet_addr );
    mem->addr.sin_port = htons ( gen->inet_port );

    for (i = 0; i < MAX_CLIENTS; i++)
    {
        mem->sock[i].s = INVALID_SOCKET;
        mem->sock[i].buf = (LPVOID) LocalAlloc ( LPTR, gen->n_chunks * gen->chunk_size );
        mem->sock[i].n_recvd = 0;
        mem->sock[i].n_sent = 0;
    }

    if ( gen->sock_type == SOCK_STREAM )
        do_bind ( mem->s, (struct sockaddr*) &mem->addr, sizeof (mem->addr) );
}

static void server_stop (void)
{
    int i;
    server_memory *mem = TlsGetValue ( tls );

    for (i = 0; i < MAX_CLIENTS; i++ )
    {
        LocalFree ( (HANDLE) mem->sock[i].buf );
        if ( mem->sock[i].s != INVALID_SOCKET )
            closesocket ( mem->sock[i].s );
    }
    ok ( closesocket ( mem->s ) == 0, "closesocket failed\n" );
    LocalFree ( (HANDLE) mem );
    ExitThread ( GetCurrentThreadId () );
}

/**************** Client utilitiy functions ***************/

static void client_start ( client_params *par )
{
    test_params *gen = par->general;
    client_memory *mem = (LPVOID) LocalAlloc (LPTR, sizeof (client_memory));

    TlsSetValue ( tls, mem );

    WaitForSingleObject ( server_ready, INFINITE );

    mem->s = WSASocketA ( AF_INET, gen->sock_type, gen->sock_prot,
                          NULL, 0, par->sock_flags );

    mem->addr.sin_family = AF_INET;
    mem->addr.sin_addr.s_addr = inet_addr ( gen->inet_addr );
    mem->addr.sin_port = htons ( gen->inet_port );

    ok ( mem->s != INVALID_SOCKET, "Client: WSASocket failed\n" );

    mem->send_buf = (LPVOID) LocalAlloc ( LPTR, 2 * gen->n_chunks * gen->chunk_size );
    mem->recv_buf = mem->send_buf + gen->n_chunks * gen->chunk_size;
    fill_buffer ( mem->send_buf, gen->chunk_size, gen->n_chunks );

    SetEvent ( client_ready[client_id] );
    /* Wait for the other clients to come up */
    WaitForMultipleObjects ( min ( gen->n_clients, MAX_CLIENTS ), client_ready, TRUE, INFINITE );
}

static void client_stop (void)
{
    client_memory *mem = TlsGetValue ( tls );
    wsa_ok ( closesocket ( mem->s ), 0 ==, "closesocket error (%lx): %d\n" );
    LocalFree ( (HANDLE) mem->send_buf );
    LocalFree ( (HANDLE) mem );
    ExitThread(0);
}

/**************** Servers ***************/

/*
 * simple_server: A very basic server doing synchronous IO.
 */
static VOID WINAPI simple_server ( server_params *par )
{
    test_params *gen = par->general;
    server_memory *mem;
    int n_recvd, n_sent, n_expected = gen->n_chunks * gen->chunk_size, tmp, i,
        id = GetCurrentThreadId();
    char *p;

    trace ( "simple_server (%x) starting\n", id );

    set_so_opentype ( FALSE ); /* non-overlapped */
    server_start ( par );
    mem = TlsGetValue ( tls );

    wsa_ok ( set_blocking ( mem->s, TRUE ), 0 ==, "simple_server (%lx): failed to set blocking mode: %d\n");
    wsa_ok ( listen ( mem->s, SOMAXCONN ), 0 ==, "simple_server (%lx): listen failed: %d\n");

    trace ( "simple_server (%x) ready\n", id );
    SetEvent ( server_ready ); /* notify clients */

    for ( i = 0; i < min ( gen->n_clients, MAX_CLIENTS ); i++ )
    {
        trace ( "simple_server (%x): waiting for client\n", id );

        /* accept a single connection */
        tmp = sizeof ( mem->sock[0].peer );
        mem->sock[0].s = accept ( mem->s, (struct sockaddr*) &mem->sock[0].peer, &tmp );
        wsa_ok ( mem->sock[0].s, INVALID_SOCKET !=, "simple_server (%lx): accept failed: %d\n" );

        ok ( mem->sock[0].peer.sin_addr.s_addr == inet_addr ( gen->inet_addr ),
             "simple_server (%x): strange peer address\n", id );

        /* Receive data & check it */
        n_recvd = do_synchronous_recv ( mem->sock[0].s, mem->sock[0].buf, n_expected, par->buflen );
        ok ( n_recvd == n_expected,
             "simple_server (%x): received less data than expected: %d of %d\n", id, n_recvd, n_expected );
        p = test_buffer ( mem->sock[0].buf, gen->chunk_size, gen->n_chunks );
        ok ( p == NULL, "simple_server (%x): test pattern error: %d\n", id, p - mem->sock[0].buf);

        /* Echo data back */
        n_sent = do_synchronous_send ( mem->sock[0].s, mem->sock[0].buf, n_expected, par->buflen );
        ok ( n_sent == n_expected,
             "simple_server (%x): sent less data than expected: %d of %d\n", id, n_sent, n_expected );

        /* cleanup */
        read_zero_bytes ( mem->sock[0].s );
        wsa_ok ( closesocket ( mem->sock[0].s ),  0 ==, "simple_server (%lx): closesocket error: %d\n" );
        mem->sock[0].s = INVALID_SOCKET;
    }

    trace ( "simple_server (%x) exiting\n", id );
    server_stop ();
}

/*
 * select_server: A non-blocking server.
 */
static VOID WINAPI select_server ( server_params *par )
{
    test_params *gen = par->general;
    server_memory *mem;
    int n_expected = gen->n_chunks * gen->chunk_size, tmp, i,
        id = GetCurrentThreadId(), n_connections = 0, n_sent, n_recvd,
        n_set, delta, n_ready;
    char *p;
    struct timeval timeout = {0,10}; /* wait for 10 milliseconds */
    fd_set fds_recv, fds_send, fds_openrecv, fds_opensend;

    trace ( "select_server (%x) starting\n", id );

    set_so_opentype ( FALSE ); /* non-overlapped */
    server_start ( par );
    mem = TlsGetValue ( tls );

    wsa_ok ( set_blocking ( mem->s, FALSE ), 0 ==, "select_server (%lx): failed to set blocking mode: %d\n");
    wsa_ok ( listen ( mem->s, SOMAXCONN ), 0 ==, "select_server (%lx): listen failed: %d\n");

    trace ( "select_server (%x) ready\n", id );
    SetEvent ( server_ready ); /* notify clients */

    FD_ZERO ( &fds_openrecv );
    FD_ZERO ( &fds_recv );
    FD_ZERO ( &fds_send );
    FD_ZERO ( &fds_opensend );

    FD_SET ( mem->s, &fds_openrecv );

    while(1)
    {
        fds_recv = fds_openrecv;
        fds_send = fds_opensend;

        n_set = 0;

        wsa_ok ( ( n_ready = select ( 0, &fds_recv, &fds_send, NULL, &timeout ) ), SOCKET_ERROR !=, 
            "select_server (%lx): select() failed: %d\n" );

        /* check for incoming requests */
        if ( FD_ISSET ( mem->s, &fds_recv ) ) {
            n_set += 1;

            trace ( "select_server (%x): accepting client connection\n", id );

            /* accept a single connection */
            tmp = sizeof ( mem->sock[n_connections].peer );
            mem->sock[n_connections].s = accept ( mem->s, (struct sockaddr*) &mem->sock[n_connections].peer, &tmp );
            wsa_ok ( mem->sock[n_connections].s, INVALID_SOCKET !=, "select_server (%lx): accept() failed: %d\n" );

            ok ( mem->sock[n_connections].peer.sin_addr.s_addr == inet_addr ( gen->inet_addr ),
                "select_server (%x): strange peer address\n", id );

            /* add to list of open connections */
            FD_SET ( mem->sock[n_connections].s, &fds_openrecv );
            FD_SET ( mem->sock[n_connections].s, &fds_opensend );

            n_connections++;
        }

        /* handle open requests */

        for ( i = 0; i < n_connections; i++ )
        {
            if ( FD_ISSET( mem->sock[i].s, &fds_recv ) ) {
                n_set += 1;

                if ( mem->sock[i].n_recvd < n_expected ) {
                    /* Receive data & check it */
                    n_recvd = recv ( mem->sock[i].s, mem->sock[i].buf + mem->sock[i].n_recvd, min ( n_expected - mem->sock[i].n_recvd, par->buflen ), 0 );
                    ok ( n_recvd != SOCKET_ERROR, "select_server (%x): error in recv(): %d\n", id, WSAGetLastError() );
                    mem->sock[i].n_recvd += n_recvd;

                    if ( mem->sock[i].n_recvd == n_expected ) {
                        p = test_buffer ( mem->sock[i].buf, gen->chunk_size, gen->n_chunks );
                        ok ( p == NULL, "select_server (%x): test pattern error: %d\n", id, p - mem->sock[i].buf );
                        FD_CLR ( mem->sock[i].s, &fds_openrecv );
                    }

                    ok ( mem->sock[i].n_recvd <= n_expected, "select_server (%x): received too many bytes: %d\n", id, mem->sock[i].n_recvd );
                }
            }

            /* only echo back what we've received */
            delta = mem->sock[i].n_recvd - mem->sock[i].n_sent;

            if ( FD_ISSET ( mem->sock[i].s, &fds_send ) ) {
                n_set += 1;

                if ( ( delta > 0 ) && ( mem->sock[i].n_sent < n_expected ) ) {
                    /* Echo data back */
                    n_sent = send ( mem->sock[i].s, mem->sock[i].buf + mem->sock[i].n_sent, min ( delta, par->buflen ), 0 );
                    ok ( n_sent != SOCKET_ERROR, "select_server (%x): error in send(): %d\n", id, WSAGetLastError() );
                    mem->sock[i].n_sent += n_sent;

                    if ( mem->sock[i].n_sent == n_expected ) {
                        FD_CLR ( mem->sock[i].s, &fds_opensend );
                    }

                    ok ( mem->sock[i].n_sent <= n_expected, "select_server (%x): sent too many bytes: %d\n", id, mem->sock[i].n_sent );
                }
            }
        }

        /* check that select returned the correct number of ready sockets */
        ok ( ( n_set == n_ready ), "select_server (%x): select() returns wrong number of ready sockets\n", id );

        /* check if all clients are done */
        if ( ( fds_opensend.fd_count == 0 ) 
            && ( fds_openrecv.fd_count == 1 ) /* initial socket that accepts clients */
            && ( n_connections  == min ( gen->n_clients, MAX_CLIENTS ) ) ) {
            break;
        }
    }

    for ( i = 0; i < min ( gen->n_clients, MAX_CLIENTS ); i++ )
    {
        /* cleanup */
        read_zero_bytes ( mem->sock[i].s );
        wsa_ok ( closesocket ( mem->sock[i].s ),  0 ==, "select_server (%lx): closesocket error: %d\n" );
        mem->sock[i].s = INVALID_SOCKET;
    }

    trace ( "select_server (%x) exiting\n", id );
    server_stop ();
}

/**************** Clients ***************/

/*
 * simple_client: A very basic client doing synchronous IO.
 */
static VOID WINAPI simple_client ( client_params *par )
{
    test_params *gen = par->general;
    client_memory *mem;
    int n_sent, n_recvd, n_expected = gen->n_chunks * gen->chunk_size, id;
    char *p;

    id = GetCurrentThreadId();
    trace ( "simple_client (%x): starting\n", id );
    /* wait here because we want to call set_so_opentype before creating a socket */
    WaitForSingleObject ( server_ready, INFINITE );
    trace ( "simple_client (%x): server ready\n", id );

    check_so_opentype ();
    set_so_opentype ( FALSE ); /* non-overlapped */
    client_start ( par );
    mem = TlsGetValue ( tls );

    /* Connect */
    wsa_ok ( connect ( mem->s, (struct sockaddr*) &mem->addr, sizeof ( mem->addr ) ),
             0 ==, "simple_client (%lx): connect error: %d\n" );
    ok ( set_blocking ( mem->s, TRUE ) == 0,
         "simple_client (%x): failed to set blocking mode\n", id );
    trace ( "simple_client (%x) connected\n", id );

    /* send data to server */
    n_sent = do_synchronous_send ( mem->s, mem->send_buf, n_expected, par->buflen );
    ok ( n_sent == n_expected,
         "simple_client (%x): sent less data than expected: %d of %d\n", id, n_sent, n_expected );

    /* shutdown send direction */
    wsa_ok ( shutdown ( mem->s, SD_SEND ), 0 ==, "simple_client (%lx): shutdown failed: %d\n" );

    /* Receive data echoed back & check it */
    n_recvd = do_synchronous_recv ( mem->s, mem->recv_buf, n_expected, par->buflen );
    ok ( n_recvd == n_expected,
         "simple_client (%x): received less data than expected: %d of %d\n", id, n_recvd, n_expected );

    /* check data */
    p = test_buffer ( mem->recv_buf, gen->chunk_size, gen->n_chunks );
    ok ( p == NULL, "simple_client (%x): test pattern error: %d\n", id, p - mem->recv_buf);

    /* cleanup */
    read_zero_bytes ( mem->s );
    trace ( "simple_client (%x) exiting\n", id );
    client_stop ();
}

/*
 * event_client: An event-driven client
 */
static void WINAPI event_client ( client_params *par )
{
    test_params *gen = par->general;
    client_memory *mem;
    int id = GetCurrentThreadId(), n_expected = gen->n_chunks * gen->chunk_size,
        tmp, err, n;
    HANDLE event;
    WSANETWORKEVENTS wsa_events;
    char *send_last, *recv_last, *send_p, *recv_p;
    long mask = FD_READ | FD_WRITE | FD_CLOSE;

    trace ( "event_client (%x): starting\n", id );
    client_start ( par );
    trace ( "event_client (%x): server ready\n", id );

    mem = TlsGetValue ( tls );

    /* Prepare event notification for connect, makes socket nonblocking */
    event = WSACreateEvent ();
    WSAEventSelect ( mem->s, event, FD_CONNECT );
    tmp = connect ( mem->s, (struct sockaddr*) &mem->addr, sizeof ( mem->addr ) );
    if ( tmp != 0 && ( err = WSAGetLastError () ) != WSAEWOULDBLOCK )
        ok ( 0, "event_client (%x): connect error: %d\n", id, err );

    tmp = WaitForSingleObject ( event, INFINITE );
    ok ( tmp == WAIT_OBJECT_0, "event_client (%x): wait for connect event failed: %d\n", id, tmp );
    err = WSAEnumNetworkEvents ( mem->s, event, &wsa_events );
    wsa_ok ( err, 0 ==, "event_client (%lx): WSAEnumNetworkEvents error: %d\n" );

    err = wsa_events.iErrorCode[ FD_CONNECT_BIT ];
    ok ( err == 0, "event_client (%x): connect error: %d\n", id, err );
    if ( err ) goto out;

    trace ( "event_client (%x) connected\n", id );

    WSAEventSelect ( mem->s, event, mask );

    recv_p = mem->recv_buf;
    recv_last = mem->recv_buf + n_expected;
    send_p = mem->send_buf;
    send_last = mem->send_buf + n_expected;

    while ( TRUE )
    {
        err = WaitForSingleObject ( event, INFINITE );
        ok ( err == WAIT_OBJECT_0, "event_client (%x): wait failed\n", id );

        err = WSAEnumNetworkEvents ( mem->s, event, &wsa_events );
        wsa_ok ( err, 0 ==, "event_client (%lx): WSAEnumNetworkEvents error: %d\n" );

        if ( wsa_events.lNetworkEvents & FD_WRITE )
        {
            err = wsa_events.iErrorCode[ FD_WRITE_BIT ];
            ok ( err == 0, "event_client (%x): FD_WRITE error code: %d\n", id, err );

            if ( err== 0 )
                do
                {
                    n = send ( mem->s, send_p, min ( send_last - send_p, par->buflen ), 0 );
                    if ( n < 0 )
                    {
                        err = WSAGetLastError ();
                        ok ( err == WSAEWOULDBLOCK, "event_client (%x): send error: %d\n", id, err );
                    }
                    else
                        send_p += n;
                }
                while ( n >= 0 && send_p < send_last );

            if ( send_p == send_last )
            {
                trace ( "event_client (%x): all data sent - shutdown\n", id );
                shutdown ( mem->s, SD_SEND );
                mask &= ~FD_WRITE;
                WSAEventSelect ( mem->s, event, mask );
            }
        }
        if ( wsa_events.lNetworkEvents & FD_READ )
        {
            err = wsa_events.iErrorCode[ FD_READ_BIT ];
            ok ( err == 0, "event_client (%x): FD_READ error code: %d\n", id, err );
            if ( err != 0 ) break;
            
            /* First read must succeed */
            n = recv ( mem->s, recv_p, min ( recv_last - recv_p, par->buflen ), 0 );
            wsa_ok ( n, 0 <=, "event_client (%lx): recv error: %d\n" );

            while ( n >= 0 ) {
                recv_p += n;
                if ( recv_p == recv_last )
                {
                    mask &= ~FD_READ;
                    trace ( "event_client (%x): all data received\n", id );
                    WSAEventSelect ( mem->s, event, mask );
                    break;
                }
                n = recv ( mem->s, recv_p, min ( recv_last - recv_p, par->buflen ), 0 );
                if ( n < 0 && ( err = WSAGetLastError()) != WSAEWOULDBLOCK )
                    ok ( 0, "event_client (%x): read error: %d\n", id, err );
                
            }
        }   
        if ( wsa_events.lNetworkEvents & FD_CLOSE )
        {
            trace ( "event_client (%x): close event\n", id );
            err = wsa_events.iErrorCode[ FD_CLOSE_BIT ];
            ok ( err == 0, "event_client (%x): FD_CLOSE error code: %d\n", id, err );
            break;
        }
    }

    ok ( send_p == send_last,
         "simple_client (%x): sent less data than expected: %d of %d\n",
         id, send_p - mem->send_buf, n_expected );
    ok ( recv_p == recv_last,
         "simple_client (%x): received less data than expected: %d of %d\n",
         id, recv_p - mem->recv_buf, n_expected );
    recv_p = test_buffer ( mem->recv_buf, gen->chunk_size, gen->n_chunks );
    ok ( recv_p == NULL, "event_client (%x): test pattern error: %d\n", id, recv_p - mem->recv_buf);

out:
    WSACloseEvent ( event );
    trace ( "event_client (%x) exiting\n", id );
    client_stop ();
}

/**************** Main program utility functions ***************/

static void Init (void)
{
    WORD ver = MAKEWORD (2, 2);
    WSADATA data;

    ok ( WSAStartup ( ver, &data ) == 0, "WSAStartup failed\n" );
    tls = TlsAlloc();
}

static void Exit (void)
{
    TlsFree ( tls );
    ok ( WSACleanup() == 0, "WSACleanup failed\n" );
}

static void StartServer (LPTHREAD_START_ROUTINE routine,
                         test_params *general, server_params *par)
{
    par->general = general;
    thread[0] = CreateThread ( NULL, 0, routine, par, 0, &thread_id[0] );
    ok ( thread[0] != NULL, "Failed to create server thread\n" );
}

static void StartClients (LPTHREAD_START_ROUTINE routine,
                          test_params *general, client_params *par)
{
    int i;
    par->general = general;
    for ( i = 1; i <= min ( general->n_clients, MAX_CLIENTS ); i++ )
    {
        client_id = i - 1;
        thread[i] = CreateThread ( NULL, 0, routine, par, 0, &thread_id[i] );
        ok ( thread[i] != NULL, "Failed to create client thread\n" );
        /* Make sure the client is up and running */
        WaitForSingleObject ( client_ready[client_id], INFINITE );
    };
}

static void do_test( test_setup *test )
{
    DWORD i, n = min (test->general.n_clients, MAX_CLIENTS);
    DWORD wait;

    server_ready = CreateEventW ( NULL, TRUE, FALSE, NULL );
    for (i = 0; i <= n; i++)
        client_ready[i] = CreateEventW ( NULL, TRUE, FALSE, NULL );

    StartServer ( test->srv, &test->general, &test->srv_params );
    StartClients ( test->clt, &test->general, &test->clt_params );
    WaitForSingleObject ( server_ready, INFINITE );

    wait = WaitForMultipleObjects ( 1 + n, thread, TRUE, 1000 * TEST_TIMEOUT );
    ok ( wait >= WAIT_OBJECT_0 && wait <= WAIT_OBJECT_0 + n , 
         "some threads have not completed: %lx\n", wait );

    if ( ! ( wait >= WAIT_OBJECT_0 && wait <= WAIT_OBJECT_0 + n ) )
    {
        for (i = 0; i <= n; i++)
        {
            trace ("terminating thread %08lx\n", thread_id[i]);
            if ( WaitForSingleObject ( thread[i], 0 ) != WAIT_OBJECT_0 )
                TerminateThread ( thread [i], 0 );
        }
    }
    CloseHandle ( server_ready );
    for (i = 0; i <= n; i++)
        CloseHandle ( client_ready[i] );
}

/********* some tests for getsockopt(setsockopt(X)) == X ***********/
/* optname = SO_LINGER */
LINGER linger_testvals[] = {
    {0,0},
    {0,73}, 
    {1,0},
    {5,189}
};

/* optname = SO_RCVTIMEO, SOSNDTIMEO */
#define SOCKTIMEOUT1 63000 /* 63 seconds. Do not test fractional part because of a
                        bug in the linux kernel (fixed in 2.6.8) */ 
#define SOCKTIMEOUT2 997000 /* 997 seconds */

static void test_set_getsockopt(void)
{
    SOCKET s;
    int i, err;
    int timeout;
    LINGER lingval;
    int size;

    s = socket(AF_INET, SOCK_STREAM, 0);
    ok(s!=INVALID_SOCKET, "socket() failed error: %d\n", WSAGetLastError());
    if( s == INVALID_SOCKET) return;
    /* SO_RCVTIMEO */
    timeout = SOCKTIMEOUT1;
    size = sizeof(timeout);
    err = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, size); 
    if( !err)
        err = getsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, &size); 
    ok( !err, "get/setsockopt(SO_RCVTIMEO) failed error: %d\n", WSAGetLastError());
    ok( timeout == SOCKTIMEOUT1, "getsockopt(SO_RCVTIMEO) returned wrong value %d\n", timeout);
    /* SO_SNDTIMEO */
    timeout = SOCKTIMEOUT2; /* 54 seconds. See remark above */ 
    size = sizeof(timeout);
    err = setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, size); 
    if( !err)
        err = getsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, &size); 
    ok( !err, "get/setsockopt(SO_SNDTIMEO) failed error: %d\n", WSAGetLastError());
    ok( timeout == SOCKTIMEOUT2, "getsockopt(SO_SNDTIMEO) returned wrong value %d\n", timeout);
    /* SO_LINGER */
    for( i = 0; i < sizeof(linger_testvals)/sizeof(LINGER);i++) {
        size =  sizeof(lingval);
        lingval = linger_testvals[i];
        err = setsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &lingval, size); 
        if( !err)
            err = getsockopt(s, SOL_SOCKET, SO_LINGER, (char *) &lingval, &size); 
        ok( !err, "get/setsockopt(SO_LINGER) failed error: %d\n", WSAGetLastError());
        ok( !lingval.l_onoff == !linger_testvals[i].l_onoff &&
                (lingval.l_linger == linger_testvals[i].l_linger ||
                 (!lingval.l_linger && !linger_testvals[i].l_onoff))
                , "getsockopt(SO_LINGER #%d) returned wrong value %d,%d not %d,%d \n", i, 
                 lingval.l_onoff, lingval.l_linger,
                 linger_testvals[i].l_onoff, linger_testvals[i].l_linger);
    }
    closesocket(s);
}

static void test_so_reuseaddr(void)
{
    struct sockaddr_in saddr;
    SOCKET s1,s2;
    unsigned int rc,reuse;
    int size;

    saddr.sin_family      = AF_INET;
    saddr.sin_port        = htons(9375);
    saddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    s1=socket(AF_INET, SOCK_STREAM, 0);
    ok(s1!=INVALID_SOCKET, "socket() failed error: %d\n", WSAGetLastError());
    rc = bind(s1, (struct sockaddr*)&saddr, sizeof(saddr));
    ok(rc!=SOCKET_ERROR, "bind(s1) failed error: %d\n", WSAGetLastError());

    s2=socket(AF_INET, SOCK_STREAM, 0);
    ok(s2!=INVALID_SOCKET, "socket() failed error: %d\n", WSAGetLastError());

    reuse=0x1234;
    size=sizeof(reuse);
    rc=getsockopt(s2, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, &size );
    ok(rc==0 && reuse==0,"wrong result in getsockopt(SO_REUSEADDR): rc=%d reuse=%d\n",rc,reuse);

    rc = bind(s2, (struct sockaddr*)&saddr, sizeof(saddr));
    ok(rc==SOCKET_ERROR, "bind() succeeded\n");

    reuse = 1;
    rc = setsockopt(s2, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse, sizeof(reuse));
    ok(rc==0, "setsockopt() failed error: %d\n", WSAGetLastError());

    todo_wine {
    rc = bind(s2, (struct sockaddr*)&saddr, sizeof(saddr));
    ok(rc==0, "bind() failed error: %d\n", WSAGetLastError());
    }

    closesocket(s2);
    closesocket(s1);
}

/************* Array containing the tests to run **********/

#define STD_STREAM_SOCKET \
            SOCK_STREAM, \
            0, \
            SERVERIP, \
            SERVERPORT

static test_setup tests [NUM_TESTS] =
{
    /* Test 0: synchronous client and server */
    {
        {
            STD_STREAM_SOCKET,
            2048,
            16,
            2
        },
        simple_server,
        {
            NULL,
            0,
            64
        },
        simple_client,
        {
            NULL,
            0,
            128
        }
    },
    /* Test 1: event-driven client, synchronous server */
    {
        {
            STD_STREAM_SOCKET,
            2048,
            16,
            2
        },
        simple_server,
        {
            NULL,
            0,
            64
        },
        event_client,
        {
            NULL,
            WSA_FLAG_OVERLAPPED,
            128
        }
    },
    /* Test 2: synchronous client, non-blocking server via select() */
    {
        {
            STD_STREAM_SOCKET,
            2048,
            16,
            2
        },
        select_server,
        {
            NULL,
            0,
            64
        },
        simple_client,
        {
            NULL,
            0,
            128
        }
    }
};

static void test_UDP(void)
{
    /* This function tests UDP sendto() and recvfrom(). UDP is unreliable, so it is
       possible that this test fails due to dropped packets. */

    /* peer 0 receives data from all other peers */
    struct sock_info peer[NUM_UDP_PEERS];
    char buf[16];
    int ss, i, n_recv, n_sent;

    for ( i = NUM_UDP_PEERS - 1; i >= 0; i-- ) {
        ok ( ( peer[i].s = socket ( AF_INET, SOCK_DGRAM, 0 ) ) != INVALID_SOCKET, "UDP: socket failed\n" );

        peer[i].addr.sin_family         = AF_INET;
        peer[i].addr.sin_addr.s_addr    = inet_addr ( SERVERIP );

        if ( i == 0 ) {
            peer[i].addr.sin_port       = htons ( SERVERPORT );
        } else {
            peer[i].addr.sin_port       = htons ( 0 );
        }

        do_bind ( peer[i].s, (struct sockaddr *) &peer[i].addr, sizeof( peer[i].addr ) );

        /* test getsockname() to get peer's port */
        ss = sizeof ( peer[i].addr );
        ok ( getsockname ( peer[i].s, (struct sockaddr *) &peer[i].addr, &ss ) != SOCKET_ERROR, "UDP: could not getsockname()\n" );
        ok ( peer[i].addr.sin_port != htons ( 0 ), "UDP: bind() did not associate port\n" );
    }

    /* test getsockname() */
    ok ( peer[0].addr.sin_port == htons ( SERVERPORT ), "UDP: getsockname returned incorrect peer port\n" );

    for ( i = 1; i < NUM_UDP_PEERS; i++ ) {
        /* send client's ip */
        memcpy( buf, &peer[i].addr.sin_port, sizeof(peer[i].addr.sin_port) );
        n_sent = sendto ( peer[i].s, buf, sizeof(buf), 0, (struct sockaddr*) &peer[0].addr, sizeof(peer[0].addr) );
        ok ( n_sent == sizeof(buf), "UDP: sendto() sent wrong amount of data or socket error: %d\n", n_sent );
    }

    for ( i = 1; i < NUM_UDP_PEERS; i++ ) {
        n_recv = recvfrom ( peer[0].s, buf, sizeof(buf), 0,(struct sockaddr *) &peer[0].peer, &ss );
        ok ( n_recv == sizeof(buf), "UDP: recvfrom() received wrong amount of data or socket error: %d\n", n_recv );
        ok ( memcmp ( &peer[0].peer.sin_port, buf, sizeof(peer[0].addr.sin_port) ) == 0, "UDP: port numbers do not match\n" );
    }
}

static void WINAPI do_getservbyname( HANDLE *starttest )
{
    struct {
        const char *name;
        const char *proto;
        int port;
    } serv[2] = { {"domain", "udp", 53}, {"telnet", "tcp", 23} };

    int i, j;
    struct servent *pserv[2];

    ok ( WaitForSingleObject ( *starttest, TEST_TIMEOUT * 1000 ) != WAIT_TIMEOUT, "test_getservbyname: timeout waiting for start signal\n");

    /* ensure that necessary buffer resizes are completed */
    for ( j = 0; j < 2; j++) {
        pserv[j] = getservbyname ( serv[j].name, serv[j].proto );
    }

    for ( i = 0; i < NUM_QUERIES / 2; i++ ) {
        for ( j = 0; j < 2; j++ ) {
            pserv[j] = getservbyname ( serv[j].name, serv[j].proto );
            ok ( pserv[j] != NULL, "getservbyname could not retrieve information for %s: %d\n", serv[j].name, WSAGetLastError() );
            ok ( pserv[j]->s_port == htons(serv[j].port), "getservbyname returned the wrong port for %s: %d\n", serv[j].name, ntohs(pserv[j]->s_port) );
            ok ( !strcmp ( pserv[j]->s_proto, serv[j].proto ), "getservbyname returned the wrong protocol for %s: %s\n", serv[j].name, pserv[j]->s_proto );
            ok ( !strcmp ( pserv[j]->s_name, serv[j].name ), "getservbyname returned the wrong name for %s: %s\n", serv[j].name, pserv[j]->s_name );
        }

        ok ( pserv[0] == pserv[1], "getservbyname: winsock resized servent buffer when not necessary\n" );
    }
}

static void test_getservbyname(void)
{
    int i;
    HANDLE starttest, thread[NUM_THREADS];
    DWORD thread_id[NUM_THREADS];

    starttest = CreateEvent ( NULL, 1, 0, "test_getservbyname_starttest" );

    /* create threads */
    for ( i = 0; i < NUM_THREADS; i++ ) {
        thread[i] = CreateThread ( NULL, 0, (LPTHREAD_START_ROUTINE) &do_getservbyname, &starttest, 0, &thread_id[i] );
    }

    /* signal threads to start */
    SetEvent ( starttest );

    for ( i = 0; i < NUM_THREADS; i++) {
        WaitForSingleObject ( thread[i], TEST_TIMEOUT * 1000 );
    }
}

static void test_WSAAddressToStringA(void)
{
    INT ret;
    DWORD len;
    int GLE;
    SOCKADDR_IN sockaddr;
    CHAR address[22]; /* 12 digits + 3 dots + ':' + 5 digits + '\0' */

    CHAR expect1[] = "0.0.0.0";
    CHAR expect2[] = "255.255.255.255";
    CHAR expect3[] = "0.0.0.0:65535";
    CHAR expect4[] = "255.255.255.255:65535";

    len = 0;

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAAddressToStringA( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    GLE = WSAGetLastError();
    ok( (ret == SOCKET_ERROR && GLE == WSAEFAULT) || (ret == 0), 
        "WSAAddressToStringA() failed unexpectedly: WSAGetLastError()=%d, ret=%d\n",
        GLE, ret );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAAddressToStringA( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringA() failed unexpectedly: %d\n", WSAGetLastError() );

    ok( !strcmp( address, expect1 ), "Expected: %s, got: %s\n", expect1, address );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0xffffffff;

    ret = WSAAddressToStringA( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringA() failed unexpectedly: %d\n", WSAGetLastError() );

    ok( !strcmp( address, expect2 ), "Expected: %s, got: %s\n", expect2, address );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0xffff;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAAddressToStringA( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringA() failed unexpectedly: %d\n", WSAGetLastError() );

    ok( !strcmp( address, expect3 ), "Expected: %s, got: %s\n", expect3, address );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0xffff;
    sockaddr.sin_addr.s_addr = 0xffffffff;

    ret = WSAAddressToStringA( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringA() failed unexpectedly: %d\n", WSAGetLastError() );

    ok( !strcmp( address, expect4 ), "Expected: %s, got: %s\n", expect4, address );
}

static void test_WSAAddressToStringW(void)
{
    INT ret;
    DWORD len;
    int GLE;
    SOCKADDR_IN sockaddr;
    WCHAR address[22]; /* 12 digits + 3 dots + ':' + 5 digits + '\0' */

    WCHAR expect1[] = { '0','.','0','.','0','.','0', 0 };
    WCHAR expect2[] = { '2','5','5','.','2','5','5','.','2','5','5','.','2','5','5', 0 };
    WCHAR expect3[] = { '0','.','0','.','0','.','0', ':', '6', '5', '5', '3', '5', 0 };
    WCHAR expect4[] = { '2','5','5','.','2','5','5','.','2','5','5','.','2','5','5', ':',
                        '6', '5', '5', '3', '5', 0 };

    len = 0;

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAAddressToStringW( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    GLE = WSAGetLastError();
    ok( (ret == SOCKET_ERROR && GLE == WSAEFAULT) || (ret == 0), 
        "WSAAddressToStringW() failed unexpectedly: WSAGetLastError()=%d, ret=%d\n",
        GLE, ret );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAAddressToStringW( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringW() failed unexpectedly: %x\n", WSAGetLastError() );

    ok( !lstrcmpW( address, expect1 ), "Expected different address string\n" );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0xffffffff;

    ret = WSAAddressToStringW( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringW() failed unexpectedly: %x\n", WSAGetLastError() );

    ok( !lstrcmpW( address, expect2 ), "Expected different address string\n" );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0xffff;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAAddressToStringW( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringW() failed unexpectedly: %x\n", WSAGetLastError() );

    ok( !lstrcmpW( address, expect3 ), "Expected different address string\n" );

    len = sizeof(address);

    sockaddr.sin_family = AF_INET;
    sockaddr.sin_port = 0xffff;
    sockaddr.sin_addr.s_addr = 0xffffffff;

    ret = WSAAddressToStringW( (SOCKADDR*)&sockaddr, sizeof(sockaddr), NULL, address, &len );
    ok( !ret, "WSAAddressToStringW() failed unexpectedly: %x\n", WSAGetLastError() );

    ok( !lstrcmpW( address, expect4 ), "Expected different address string\n" );
}

static void test_WSAStringToAddressA(void)
{
    INT ret, len;
    SOCKADDR_IN sockaddr;
    int GLE;

    CHAR address1[] = "0.0.0.0";
    CHAR address2[] = "127.127.127.127";
    CHAR address3[] = "255.255.255.255";
    CHAR address4[] = "127.127.127.127:65535";
    CHAR address5[] = "255.255.255.255:65535";

    len = 0;
    sockaddr.sin_family = AF_INET;

    ret = WSAStringToAddressA( address1, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( ret == SOCKET_ERROR, "WSAStringToAddressA() succeeded unexpectedly: %x\n",
        WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressA( address1, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( !ret && sockaddr.sin_addr.s_addr == 0,
        "WSAStringToAddressA() failed unexpectedly: %d\n", WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressA( address2, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( !ret && sockaddr.sin_addr.s_addr == 0x7f7f7f7f,
        "WSAStringToAddressA() failed unexpectedly: %d\n", WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressA( address3, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    GLE = WSAGetLastError();
    ok( (ret == 0 && sockaddr.sin_addr.s_addr == 0xffffffff) || 
        (ret == SOCKET_ERROR && (GLE == ERROR_INVALID_PARAMETER || GLE == WSAEINVAL)),
        "WSAStringToAddressA() failed unexpectedly: %d\n", GLE );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressA( address4, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( !ret && sockaddr.sin_addr.s_addr == 0x7f7f7f7f && sockaddr.sin_port == 0xffff,
        "WSAStringToAddressA() failed unexpectedly: %d\n", WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressA( address5, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    GLE = WSAGetLastError();
    ok( (ret == 0 && sockaddr.sin_addr.s_addr == 0xffffffff && sockaddr.sin_port == 0xffff) || 
        (ret == SOCKET_ERROR && (GLE == ERROR_INVALID_PARAMETER || GLE == WSAEINVAL)),
        "WSAStringToAddressA() failed unexpectedly: %d\n", GLE );
}

static void test_WSAStringToAddressW(void)
{
    INT ret, len;
    SOCKADDR_IN sockaddr;
    int GLE;

    WCHAR address1[] = { '0','.','0','.','0','.','0', 0 };
    WCHAR address2[] = { '1','2','7','.','1','2','7','.','1','2','7','.','1','2','7', 0 };
    WCHAR address3[] = { '2','5','5','.','2','5','5','.','2','5','5','.','2','5','5', 0 };
    WCHAR address4[] = { '1','2','7','.','1','2','7','.','1','2','7','.','1','2','7',
                         ':', '6', '5', '5', '3', '5', 0 };
    WCHAR address5[] = { '2','5','5','.','2','5','5','.','2','5','5','.','2','5','5', ':',
                         '6', '5', '5', '3', '5', 0 };

    len = 0;
    sockaddr.sin_family = AF_INET;

    ret = WSAStringToAddressW( address1, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( ret == SOCKET_ERROR, "WSAStringToAddressW() failed unexpectedly: %d\n",
        WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressW( address1, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( !ret && sockaddr.sin_addr.s_addr == 0,
        "WSAStringToAddressW() failed unexpectedly: %d\n", WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressW( address2, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( !ret && sockaddr.sin_addr.s_addr == 0x7f7f7f7f,
        "WSAStringToAddressW() failed unexpectedly: %d\n", WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressW( address3, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    GLE = WSAGetLastError();
    ok( (ret == 0 && sockaddr.sin_addr.s_addr == 0xffffffff) || 
        (ret == SOCKET_ERROR && (GLE == ERROR_INVALID_PARAMETER || GLE == WSAEINVAL)),
        "WSAStringToAddressW() failed unexpectedly: %d\n", GLE );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressW( address4, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( !ret && sockaddr.sin_addr.s_addr == 0x7f7f7f7f && sockaddr.sin_port == 0xffff,
        "WSAStringToAddressW() failed unexpectedly: %d\n", WSAGetLastError() );

    len = sizeof(sockaddr);
    sockaddr.sin_port = 0;
    sockaddr.sin_addr.s_addr = 0;

    ret = WSAStringToAddressW( address5, AF_INET, NULL, (SOCKADDR*)&sockaddr, &len );
    ok( (ret == 0 && sockaddr.sin_addr.s_addr == 0xffffffff && sockaddr.sin_port == 0xffff) || 
        (ret == SOCKET_ERROR && (GLE == ERROR_INVALID_PARAMETER || GLE == WSAEINVAL)),
        "WSAStringToAddressW() failed unexpectedly: %d\n", GLE );
}

/**************** Main program  ***************/

START_TEST( sock )
{
    int i;
    Init();

    test_set_getsockopt();
    test_so_reuseaddr();

    for (i = 0; i < NUM_TESTS; i++)
    {
        trace ( " **** STARTING TEST %d **** \n", i );
        do_test (  &tests[i] );
        trace ( " **** TEST %d COMPLETE **** \n", i );
    }

    test_UDP();

    test_getservbyname();

    test_WSAAddressToStringA();
    test_WSAAddressToStringW();

    test_WSAStringToAddressA();
    test_WSAStringToAddressW();

    Exit();
}
