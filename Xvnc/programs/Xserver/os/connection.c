/* $XConsortium: connection.c,v 1.190 94/11/08 20:47:43 mor Exp $ */
/***********************************************************

Copyright (c) 1987, 1989  X Consortium

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from the X Consortium.


Copyright 1987, 1989 by Digital Equipment Corporation, Maynard, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Digital not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/
/*****************************************************************
 *  Stuff to create connections --- OS dependent
 *
 *      EstablishNewConnections, CreateWellKnownSockets, ResetWellKnownSockets,
 *      CloseDownConnection, CheckConnections, AddEnabledDevice,
 *	RemoveEnabledDevice, OnlyListToOneClient,
 *      ListenToAllClients,
 *
 *      (WaitForSomething is in its own file)
 *
 *      In this implementation, a client socket table is not kept.
 *      Instead, what would be the index into the table is just the
 *      file descriptor of the socket.  This won't work for if the
 *      socket ids aren't small nums (0 - 2^8)
 *
 *****************************************************************/

#include "X.h"
#include "Xproto.h"
#include <X11/Xtrans.h>
#include <errno.h>
#ifdef X_NOT_STDC_ENV
extern int errno;
#endif
#include <sys/socket.h>

#include <signal.h>
#include <setjmp.h>

#ifdef hpux
#include <sys/utsname.h>
#include <sys/ioctl.h>
#endif

#ifdef AIXV3
#include <sys/ioctl.h>
#endif

#if defined(TCPCONN) || defined(STREAMSCONN)
# include <netinet/in.h>
# ifndef hpux
#  ifdef apollo
#   ifndef NO_TCP_H
#    include <netinet/tcp.h>
#   endif
#  else
#   include <netinet/tcp.h>
#  endif
# endif
#endif

#include <stdio.h>
#include <sys/uio.h>
#include "misc.h"		/* for typedef of pointer */
#include "osdep.h"
#include "opaque.h"
#include "dixstruct.h"

#ifdef LBX
#ifndef X_NOT_POSIX
#include <unistd.h>
#else
extern int read(), writev();
#endif
#endif /* LBX */

#ifdef X_NOT_POSIX
#define Pid_t int
#else
#define Pid_t pid_t
#endif

#ifdef DNETCONN
#include <netdnet/dn.h>
#endif /* DNETCONN */

extern char *display;		/* The display number */
int lastfdesc;			/* maximum file descriptor */

FdMask WellKnownConnections;	/* Listener mask */
FdSet EnabledDevices;		/* mask for input devices that are on */
FdSet AllSockets;		/* select on this */
FdSet AllClients;		/* available clients */
FdSet LastSelectMask;		/* mask returned from last select call */
FdSet ClientsWithInput;		/* clients with FULL requests in buffer */
FdSet ClientsWriteBlocked;	/* clients who cannot receive output */
FdSet OutputPending;		/* clients with reply/event data ready to go */
int MaxClients = MAXSOCKS;
long NConnBitArrays = mskcnt;
Bool NewOutputPending;		/* not yet attempted to write some new output */
Bool AnyClientsWriteBlocked;	/* true if some client blocked on write */

Bool RunFromSmartParent;	/* send SIGUSR1 to parent process */
Bool PartialNetwork;		/* continue even if unable to bind all addrs */
static Pid_t ParentProcess;

static Bool debug_conns = FALSE;

FdSet IgnoredClientsWithInput;
static FdSet GrabImperviousClients;
static FdSet SavedAllClients;
static FdSet SavedAllSockets;
static FdSet SavedClientsWithInput;
int GrabInProgress = 0;

int ConnectionTranslation[MAXSOCKS];
#ifdef LBX
int ConnectionOutputTranslation[MAXSOCKS];
#endif

XtransConnInfo 	*ListenTransConns = NULL;
int	       	*ListenTransFds = NULL;
int		ListenTransCount;

extern int auditTrailLevel;

static void ErrorConnMax(
#if NeedFunctionPrototypes
XtransConnInfo /* trans_conn */
#endif
);

#ifndef LBX
static
#endif
void CloseDownFileDescriptor(
#if NeedFunctionPrototypes
#ifdef LBX
    ClientPtr	client
#else
    register OsCommPtr /*oc*/
#endif
#endif
);

static XtransConnInfo
lookup_trans_conn (fd)

int fd;

{
    if (ListenTransFds)
    {
	int i;
	for (i = 0; i < ListenTransCount; i++)
	    if (ListenTransFds[i] == fd)
		return ListenTransConns[i];
    }

    return (NULL);
}

#ifdef XDMCP
void XdmcpOpenDisplay(), XdmcpInit(), XdmcpReset(), XdmcpCloseDisplay();
#endif

#ifdef LBX
extern int  StandardReadRequestFromClient();
extern int  StandardWriteToClient ();
extern int  UncompressWriteToClient ();
extern unsigned long  StandardRequestLength ();
extern int  StandardFlushClient ();
#endif


/*****************
 * CreateWellKnownSockets
 *    At initialization, create the sockets to listen on for new clients.
 *****************/

void
CreateWellKnownSockets()
{
    int		request, i;
    int		partial;
    char 	port[20];

    CLEARBITS(AllSockets);
    CLEARBITS(AllClients);
    CLEARBITS(LastSelectMask);
    CLEARBITS(ClientsWithInput);

    for (i=0; i<MAXSOCKS; i++) ConnectionTranslation[i] = 0;
#ifdef LBX
    for (i=0; i<MAXSOCKS; i++) ConnectionOutputTranslation[i] = 0;
#endif
#if !defined(X_NOT_POSIX) && !defined(__FreeBSD__) && !defined(__386BSD__) && !defined(__NetBSD__)
    lastfdesc = sysconf(_SC_OPEN_MAX) - 1;
#else
#ifdef hpux
    lastfdesc = _NFILE - 1;
#else
    lastfdesc = getdtablesize() - 1;
#endif
#endif

    if (lastfdesc > MAXSOCKS)
    {
	lastfdesc = MAXSOCKS;
	if (debug_conns)
	    ErrorF( "GOT TO END OF SOCKETS %d\n", MAXSOCKS);
    }

    WellKnownConnections = 0;

    sprintf (port, "%d", atoi (display));

    if ((_XSERVTransMakeAllCOTSServerListeners (port, &partial,
	&ListenTransCount, &ListenTransConns) >= 0) &&
	(ListenTransCount >= 1))
    {
	if (!PartialNetwork && partial)
	{
	    FatalError ("Failed to establish all listening sockets");
	}
	else
	{
	    ListenTransFds = (int *) malloc (ListenTransCount * sizeof (int));

	    for (i = 0; i < ListenTransCount; i++)
	    {
		int fd = _XSERVTransGetConnectionNumber (ListenTransConns[i]);
		
		ListenTransFds[i] = fd;
		WellKnownConnections |= (1L << fd);

		if (!_XSERVTransIsLocal (ListenTransConns[i]))
		{
		    DefineSelf (fd);
		}
	    }
	}
    }

    if (WellKnownConnections == 0)
        FatalError ("Cannot establish any listening sockets - Make sure an X server isn't already running");

    OsSignal (SIGPIPE, SIG_IGN);
    OsSignal (SIGHUP, AutoResetServer);
    OsSignal (SIGINT, GiveUp);
    OsSignal (SIGTERM, GiveUp);
    AllSockets[0] = WellKnownConnections;
    ResetHosts(display);
    /*
     * Magic:  If SIGUSR1 was set to SIG_IGN when
     * the server started, assume that either
     *
     *  a- The parent process is ignoring SIGUSR1
     *
     * or
     *
     *  b- The parent process is expecting a SIGUSR1
     *     when the server is ready to accept connections
     *
     * In the first case, the signal will be harmless,
     * in the second case, the signal will be quite
     * useful
     */
    /*
    if (OsSignal (SIGUSR1, SIG_IGN) == SIG_IGN)
	RunFromSmartParent = TRUE;
    ParentProcess = getppid ();
    if (RunFromSmartParent) {
	if (ParentProcess > 0) {
	    kill (ParentProcess, SIGUSR1);
	}
    }
    */
#ifdef XDMCP
    XdmcpInit ();
#endif
}

void
ResetWellKnownSockets ()
{
    int i;

    ResetOsBuffers();

    for (i = 0; i < ListenTransCount; i++)
    {
	int status = _XSERVTransResetListener (ListenTransConns[i]);

	if (status != TRANS_RESET_NOOP)
	{
	    if (status == TRANS_RESET_FAILURE)
	    {
		/*
		 * ListenTransConns[i] freed by xtrans.
		 * Remove it from out list.
		 */

		WellKnownConnections &= ~(1L << ListenTransFds[i]);
		ListenTransFds[i] = ListenTransFds[ListenTransCount - 1];
		ListenTransConns[i] = ListenTransConns[ListenTransCount - 1];
		ListenTransCount -= 1;
		i -= 1;
	    }
	    else if (status == TRANS_RESET_NEW_FD)
	    {
		/*
		 * A new file descriptor was allocated (the old one was closed)
		 */

		int newfd = _XSERVTransGetConnectionNumber (ListenTransConns[i]);

		WellKnownConnections &= ~(1L << ListenTransFds[i]);
		ListenTransFds[i] = newfd;
		WellKnownConnections |= (1L << newfd);
	    }
	}
    }

    ResetAuthorization ();
    ResetHosts(display);
    /*
     * See above in CreateWellKnownSockets about SIGUSR1
     */
    /*
    if (RunFromSmartParent) {
	if (ParentProcess > 0) {
	    kill (ParentProcess, SIGUSR1);
	}
    }
    */
    /*
     * restart XDMCP
     */
#ifdef XDMCP
    XdmcpReset ();
#endif
}

static void
AuthAudit (client, letin, saddr, len, proto_n, auth_proto)
    int client;
    Bool letin;
    struct sockaddr *saddr;
    int len;
    unsigned short proto_n;
    char *auth_proto;
{
    char addr[128];

    if (!len)
        strcpy(addr, "local host");
    else
	switch (saddr->sa_family)
	{
	case AF_UNSPEC:
#ifdef UNIXCONN
	case AF_UNIX:
#endif
	    strcpy(addr, "local host");
	    break;
#if defined(TCPCONN) || defined(STREAMSCONN)
	case AF_INET:
	    sprintf(addr, "IP %s port %d",
		    inet_ntoa(((struct sockaddr_in *) saddr)->sin_addr),
		    ((struct sockaddr_in *) saddr)->sin_port);
	    break;
#endif
#ifdef DNETCONN
	case AF_DECnet:
	    sprintf(addr, "DN %s",
		    dnet_ntoa(&((struct sockaddr_dn *) saddr)->sdn_add));
	    break;
#endif
	default:
	    strcpy(addr, "unknown address");
	}
    if (letin)
	AuditF("client %d connected from %s\n", client, addr);
    else
	AuditF("client %d rejected from %s\n", client, addr);
    if (proto_n)
	AuditF("  Auth name: %.*s\n", proto_n, auth_proto);
}

/*****************************************************************
 * ClientAuthorized
 *
 *    Sent by the client at connection setup:
 *                typedef struct _xConnClientPrefix {
 *                   CARD8	byteOrder;
 *                   BYTE	pad;
 *                   CARD16	majorVersion, minorVersion;
 *                   CARD16	nbytesAuthProto;    
 *                   CARD16	nbytesAuthString;   
 *                 } xConnClientPrefix;
 *
 *     	It is hoped that eventually one protocol will be agreed upon.  In the
 *        mean time, a server that implements a different protocol than the
 *        client expects, or a server that only implements the host-based
 *        mechanism, will simply ignore this information.
 *
 *****************************************************************/

char * 
ClientAuthorized(client, proto_n, auth_proto, string_n, auth_string)
    ClientPtr client;
    char *auth_proto, *auth_string;
    unsigned int proto_n, string_n;
{
    register OsCommPtr 	priv;
    Xtransaddr		*from = NULL;
    int 		family;
    int			fromlen;
    XID	 		auth_id;
    char	 	*reason = NULL;

    auth_id = CheckAuthorization (proto_n, auth_proto,
				  string_n, auth_string, client, &reason);

    priv = (OsCommPtr)client->osPrivate;
    if (auth_id == (XID) ~0L)
    {
	if (_XSERVTransGetPeerAddr (priv->trans_conn,
	    &family, &fromlen, &from) != -1)
	{
	    if (InvalidHost ((struct sockaddr *) from, fromlen))
		AuthAudit(client->index, FALSE,
		    (struct sockaddr *) from, fromlen, proto_n, auth_proto);
	    else
	    {
		auth_id = (XID) 0;
		if (auditTrailLevel > 1)
		    AuthAudit(client->index, TRUE,
			(struct sockaddr *) from, fromlen,
			proto_n, auth_proto);
	    }

	    free ((char *) from);
	}

	if (auth_id == (XID) ~0L)
	    if (reason)
		return reason;
	    else
		return "Client is not authorized to connect to Server";
    }
    else if (auditTrailLevel > 1)
    {
	if (_XSERVTransGetPeerAddr (priv->trans_conn,
	    &family, &fromlen, &from) != -1)
	{
	    AuthAudit(client->index, TRUE, (struct sockaddr *) from, fromlen,
		      proto_n, auth_proto);

	    free ((char *) from);
	}
    }

    priv->auth_id = auth_id;
    priv->conn_time = 0;

#ifdef XDMCP
    /* indicate to Xdmcp protocol that we've opened new client */
    XdmcpOpenDisplay(priv->fd);
#endif /* XDMCP */
    /* At this point, if the client is authorized to change the access control
     * list, we should getpeername() information, and add the client to
     * the selfhosts list.  It's not really the host machine, but the
     * true purpose of the selfhosts list is to see who may change the
     * access control list.
     */
    return((char *)NULL);
}

#ifdef LBX

XtransConnInfo
ClientTransportObject(client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr) client->osPrivate;

    return oc->trans_conn;
}

int
ClientConnectionNumber (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr) client->osPrivate;

    return oc->fd;
}

AvailableClientInput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    if (GETBIT(AllSockets, oc->fd))
	BITSET(ClientsWithInput, oc->fd);
}

ClientPtr
AllocNewConnection (trans_conn, fd, Read, Writev, Close)
    XtransConnInfo trans_conn;
    int	    fd;
    int	    (*Read)();
    int	    (*Writev)();
    void    (*Close)();
{
    OsCommPtr	oc;
    ClientPtr	client;
    
    if (fd >= lastfdesc)
	return NullClient;
    oc = (OsCommPtr)xalloc(sizeof(OsCommRec));
    if (!oc)
	return NullClient;
    oc->trans_conn = trans_conn;
    oc->fd = fd;
    oc->input = (ConnectionInputPtr)NULL;
    oc->output = (ConnectionOutputPtr)NULL;
    oc->conn_time = GetTimeInMillis();
    oc->Read = Read;
    oc->Writev = Writev;
    oc->Close = Close;
    oc->flushClient = StandardFlushClient;
    oc->ofirst = (ConnectionOutputPtr) NULL;
    if (!(client = NextAvailableClient((pointer)oc)))
    {
	xfree (oc);
	return NullClient;
    }
    if (!ConnectionTranslation[fd])
    {
	ConnectionTranslation[fd] = client->index;
	ConnectionOutputTranslation[fd] = client->index;
	if (GrabInProgress)
	{
	    BITSET(SavedAllClients, fd);
	    BITSET(SavedAllSockets, fd);
	}
	else
	{
	    BITSET(AllClients, fd);
	    BITSET(AllSockets, fd);
	}
    }
    client->public.readRequest = StandardReadRequestFromClient;
    client->public.writeToClient = StandardWriteToClient;
    client->public.uncompressedWriteToClient = UncompressWriteToClient;
    client->public.requestLength = StandardRequestLength;
    return client;
}

void
SwitchConnectionFuncs (client, Read, Writev, Close)
    ClientPtr	client;
    int		(*Read)();
    int		(*Writev)();
    void	(*Close)();
{
    OsCommPtr	oc = (OsCommPtr) client->osPrivate;

    oc->Read = Read;
    oc->Writev = Writev;
    oc->Close = Close;
    oc->conn_time = 0;
}

void
StartOutputCompression(client, CompressOn, CompressOff)
    ClientPtr	client;
    void	(*CompressOn)();
    void	(*CompressOff)();
{
    OsCommPtr	oc = (OsCommPtr) client->osPrivate;
    extern int	LbxFlushClient();

    oc->compressOn = CompressOn;
    oc->compressOff = CompressOff;
    oc->flushClient = LbxFlushClient;
}
#endif

/*****************
 * EstablishNewConnections
 *    If anyone is waiting on listened sockets, accept them.
 *    Returns a mask with indices of new clients.  Updates AllClients
 *    and AllSockets.
 *****************/

/*ARGSUSED*/
Bool
EstablishNewConnections(clientUnused, closure)
    ClientPtr clientUnused;
    pointer closure;
{
    FdMask readyconnections;     /* mask of listeners that are ready */
    int curconn;                  /* fd of listener that's ready */
    register int newconn;         /* fd of new client */
    CARD32 connect_time;
    register int i;
    register ClientPtr client;
    register OsCommPtr oc;
#ifdef LBX
    extern int  writev(), close();
#endif

    readyconnections = (((FdMask)closure) & WellKnownConnections);
    if (!readyconnections)
	return TRUE;
    connect_time = GetTimeInMillis();
    /* kill off stragglers */
    for (i=1; i<currentMaxClients; i++)
    {
	if (client = clients[i])
	{
	    oc = (OsCommPtr)(client->osPrivate);
	    if (oc && (oc->conn_time != 0) &&
		(connect_time - oc->conn_time) >= TimeOutValue)
		CloseDownClient(client);     
	}
    }
    while (readyconnections) 
    {
	XtransConnInfo trans_conn, new_trans_conn;
	int status;

	curconn = ffs (readyconnections) - 1;
	readyconnections &= ~(1 << curconn);

	if ((trans_conn = lookup_trans_conn (curconn)) == NULL)
	    continue;

	if ((new_trans_conn = _XSERVTransAccept (trans_conn, &status)) == NULL)
	    continue;

	newconn = _XSERVTransGetConnectionNumber (new_trans_conn);

	_XSERVTransSetOption(new_trans_conn, TRANS_NONBLOCKING, 1);

#ifdef LBX
	client = AllocNewConnection (new_trans_conn, newconn,
		read, writev, CloseDownFileDescriptor);
	if (!client)
	{
	    ErrorConnMax(new_trans_conn);
	    _XSERVTransClose(new_trans_conn);
	    continue;
	}
#else
	oc = (OsCommPtr)xalloc(sizeof(OsCommRec));
	if (!oc)
	{
	    ErrorConnMax(new_trans_conn);
	    _XSERVTransClose(new_trans_conn);
	    continue;
	}
	if (GrabInProgress)
	{
	    BITSET(SavedAllClients, newconn);
	    BITSET(SavedAllSockets, newconn);
	}
	else
	{
	    BITSET(AllClients, newconn);
	    BITSET(AllSockets, newconn);
	}
	oc->fd = newconn;
	oc->trans_conn = new_trans_conn;
	oc->input = (ConnectionInputPtr)NULL;
	oc->output = (ConnectionOutputPtr)NULL;
	oc->conn_time = connect_time;
	if ((newconn < lastfdesc) &&
	    (client = NextAvailableClient((pointer)oc)))
	{
	    ConnectionTranslation[newconn] = client->index;
	}
	else
	{
	    ErrorConnMax(new_trans_conn);
	    CloseDownFileDescriptor(oc);
	}
#endif	/* LBX */
    }
    return TRUE;
}

#define NOROOM "Maximum number of clients reached"

/************
 *   ErrorConnMax
 *     Fail a connection due to lack of client or file descriptor space
 ************/

static void
ErrorConnMax(trans_conn)
XtransConnInfo trans_conn;
{
    register int fd = _XSERVTransGetConnectionNumber (trans_conn);
    xConnSetupPrefix csp;
    char pad[3];
    struct iovec iov[3];
    char byteOrder = 0;
    int whichbyte = 1;
    struct timeval waittime;
    FdSet mask;

    /* if these seems like a lot of trouble to go to, it probably is */
    waittime.tv_sec = BOTIMEOUT / MILLI_PER_SECOND;
    waittime.tv_usec = (BOTIMEOUT % MILLI_PER_SECOND) *
		       (1000000 / MILLI_PER_SECOND);
    CLEARBITS(mask);
    BITSET(mask, fd);
    (void)select(fd + 1, (int *) mask, (int *) NULL, (int *) NULL, &waittime);
    /* try to read the byte-order of the connection */
    (void)_XSERVTransRead(trans_conn, &byteOrder, 1);
    if ((byteOrder == 'l') || (byteOrder == 'B'))
    {
	csp.success = xFalse;
	csp.lengthReason = sizeof(NOROOM) - 1;
	csp.length = (sizeof(NOROOM) + 2) >> 2;
	csp.majorVersion = X_PROTOCOL;
	csp.minorVersion = X_PROTOCOL_REVISION;
	if (((*(char *) &whichbyte) && (byteOrder == 'B')) ||
	    (!(*(char *) &whichbyte) && (byteOrder == 'l')))
	{
	    swaps(&csp.majorVersion, whichbyte);
	    swaps(&csp.minorVersion, whichbyte);
	    swaps(&csp.length, whichbyte);
	}
	iov[0].iov_len = sz_xConnSetupPrefix;
	iov[0].iov_base = (char *) &csp;
	iov[1].iov_len = csp.lengthReason;
	iov[1].iov_base = NOROOM;
	iov[2].iov_len = (4 - (csp.lengthReason & 3)) & 3;
	iov[2].iov_base = pad;
	(void)_XSERVTransWritev(trans_conn, iov, 3);
    }
}

/************
 *   CloseDownFileDescriptor:
 *     Remove this file descriptor and it's I/O buffers, etc.
 ************/

#ifdef LBX
void
CloseDownFileDescriptor(client)
    ClientPtr	client;
#else
static void
CloseDownFileDescriptor(oc)
    register OsCommPtr oc;
#endif
{
#ifdef LBX
    register OsCommPtr oc = (OsCommPtr) client->osPrivate;
#endif
    int connection = oc->fd;

    if (oc->trans_conn)
	_XSERVTransClose(oc->trans_conn);
#ifdef LBX
    ConnectionTranslation[connection] = 0;
    ConnectionOutputTranslation[connection] = 0;
#else
    FreeOsBuffers(oc);
#endif
    BITCLEAR(AllSockets, connection);
    BITCLEAR(AllClients, connection);
    BITCLEAR(ClientsWithInput, connection);
    BITCLEAR(GrabImperviousClients, connection);
    if (GrabInProgress)
    {
	BITCLEAR(SavedAllSockets, connection);
	BITCLEAR(SavedAllClients, connection);
	BITCLEAR(SavedClientsWithInput, connection);
    }
    BITCLEAR(ClientsWriteBlocked, connection);
    if (!ANYSET(ClientsWriteBlocked))
    	AnyClientsWriteBlocked = FALSE;
    BITCLEAR(OutputPending, connection);
#ifndef LBX
    xfree(oc);
#endif
}

/*****************
 * CheckConections
 *    Some connection has died, go find which one and shut it down 
 *    The file descriptor has been closed, but is still in AllClients.
 *    If would truly be wonderful if select() would put the bogus
 *    file descriptors in the exception mask, but nooooo.  So we have
 *    to check each and every socket individually.
 *****************/

void
CheckConnections()
{
    FdMask		mask;
    FdSet		tmask; 
    register int	curclient, curoff;
    int			i;
    struct timeval	notime;
    int r;

    notime.tv_sec = 0;
    notime.tv_usec = 0;

    for (i=0; i<mskcnt; i++)
    {
	mask = AllClients[i];
        while (mask)
    	{
	    curoff = ffs (mask) - 1;
 	    curclient = curoff + (i << 5);
            CLEARBITS(tmask);
            BITSET(tmask, curclient);
            r = select (curclient + 1, (int *)tmask, (int *)NULL, (int *)NULL, 
			&notime);
            if (r < 0)
		CloseDownClient(clients[ConnectionTranslation[curclient]]);
	    mask &= ~(1 << curoff);
	}
    }	
}


/*****************
 * CloseDownConnection
 *    Delete client from AllClients and free resources 
 *****************/

void
CloseDownConnection(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    if (oc->output && oc->output->count)
	FlushClient(client, oc, (char *)NULL, 0);
#ifdef XDMCP
    XdmcpCloseDisplay(oc->fd);
#endif
#ifndef LBX
    CloseDownFileDescriptor(oc);
#else
    (*oc->Close) (client);
    FreeOsBuffers(oc);
    xfree(oc);
#endif
    client->osPrivate = (pointer)NULL;
    if (auditTrailLevel > 1)
	AuditF("client %d disconnected\n", client->index);
}


AddEnabledDevice(fd)
    int fd;
{
    BITSET(EnabledDevices, fd);
    BITSET(AllSockets, fd);
}


RemoveEnabledDevice(fd)
    int fd;
{
    BITCLEAR(EnabledDevices, fd);
    BITCLEAR(AllSockets, fd);
}

/*****************
 * OnlyListenToOneClient:
 *    Only accept requests from  one client.  Continue to handle new
 *    connections, but don't take any protocol requests from the new
 *    ones.  Note that if GrabInProgress is set, EstablishNewConnections
 *    needs to put new clients into SavedAllSockets and SavedAllClients.
 *    Note also that there is no timeout for this in the protocol.
 *    This routine is "undone" by ListenToAllClients()
 *****************/

OnlyListenToOneClient(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    if (! GrabInProgress)
    {
	COPYBITS(ClientsWithInput, SavedClientsWithInput);
	MASKANDSETBITS(ClientsWithInput,
		       ClientsWithInput, GrabImperviousClients);
	if (GETBIT(SavedClientsWithInput, connection))
	{
	    BITCLEAR(SavedClientsWithInput, connection);
	    BITSET(ClientsWithInput, connection);
	}
	UNSETBITS(SavedClientsWithInput, GrabImperviousClients);
	COPYBITS(AllSockets, SavedAllSockets);
	COPYBITS(AllClients, SavedAllClients);
	UNSETBITS(AllSockets, AllClients);
	MASKANDSETBITS(AllClients, AllClients, GrabImperviousClients);
	BITSET(AllClients, connection);
	ORBITS(AllSockets, AllSockets, AllClients);
	GrabInProgress = client->index;
    }
}

/****************
 * ListenToAllClients:
 *    Undoes OnlyListentToOneClient()
 ****************/

ListenToAllClients()
{
    if (GrabInProgress)
    {
	ORBITS(AllSockets, AllSockets, SavedAllSockets);
	ORBITS(AllClients, AllClients, SavedAllClients);
	ORBITS(ClientsWithInput, ClientsWithInput, SavedClientsWithInput);
	GrabInProgress = 0;
    }	
}

/****************
 * IgnoreClient
 *    Removes one client from input masks.
 *    Must have cooresponding call to AttendClient.
 ****************/

IgnoreClient (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    if (!GrabInProgress || GETBIT(AllClients, connection))
    {
    	if (GETBIT (ClientsWithInput, connection))
	    BITSET(IgnoredClientsWithInput, connection);
    	else
	    BITCLEAR(IgnoredClientsWithInput, connection);
    	BITCLEAR(ClientsWithInput, connection);
    	BITCLEAR(AllSockets, connection);
    	BITCLEAR(AllClients, connection);
	BITCLEAR(LastSelectMask, connection);
    }
    else
    {
    	if (GETBIT (SavedClientsWithInput, connection))
	    BITSET(IgnoredClientsWithInput, connection);
    	else
	    BITCLEAR(IgnoredClientsWithInput, connection);
	BITCLEAR(SavedClientsWithInput, connection);
	BITCLEAR(SavedAllSockets, connection);
	BITCLEAR(SavedAllClients, connection);
    }
    isItTimeToYield = TRUE;
}

/****************
 * AttendClient
 *    Adds one client back into the input masks.
 ****************/

AttendClient (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    if (!GrabInProgress || GrabInProgress == client->index ||
	GETBIT(GrabImperviousClients, connection))
    {
    	BITSET(AllClients, connection);
    	BITSET(AllSockets, connection);
	BITSET(LastSelectMask, connection);
    	if (GETBIT (IgnoredClientsWithInput, connection))
	    BITSET(ClientsWithInput, connection);
    }
    else
    {
	BITSET(SavedAllClients, connection);
	BITSET(SavedAllSockets, connection);
	if (GETBIT(IgnoredClientsWithInput, connection))
	    BITSET(SavedClientsWithInput, connection);
    }
}

/* make client impervious to grabs; assume only executing client calls this */

MakeClientGrabImpervious(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    BITSET(GrabImperviousClients, connection);

    if (ServerGrabCallback)
    {
	ServerGrabInfoRec grabinfo;
	grabinfo.client = client;
	grabinfo.grabstate  = CLIENT_IMPERVIOUS;
	CallCallbacks(&ServerGrabCallback, &grabinfo);
    }
}

/* make client pervious to grabs; assume only executing client calls this */

MakeClientGrabPervious(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    int connection = oc->fd;

    BITCLEAR(GrabImperviousClients, connection);
    if (GrabInProgress && (GrabInProgress != client->index))
    {
	if (GETBIT(ClientsWithInput, connection))
	{
	    BITSET(SavedClientsWithInput, connection);
	    BITCLEAR(ClientsWithInput, connection);
	}
	BITCLEAR(AllSockets, connection);
	BITCLEAR(AllClients, connection);
	isItTimeToYield = TRUE;
    }

    if (ServerGrabCallback)
    {
	ServerGrabInfoRec grabinfo;
	grabinfo.client = client;
	grabinfo.grabstate  = CLIENT_PERVIOUS;
	CallCallbacks(&ServerGrabCallback, &grabinfo);
    }
}

#ifdef AIXV3

static FdSet pendingActiveClients;
static BOOL reallyGrabbed;

/****************
* DontListenToAnybody:
*   Don't listen to requests from any clients. Continue to handle new
*   connections, but don't take any protocol requests from anybody.
*   We have to take care if there is already a grab in progress, though.
*   Undone by PayAttentionToClientsAgain. We also have to be careful
*   not to accept any more input from the currently dispatched client.
*   we do this be telling dispatch it is time to yield.

*   We call this when the server loses access to the glass
*   (user hot-keys away).  This looks like a grab by the 
*   server itself, but gets a little tricky if there is already
*   a grab in progress.
******************/

void
DontListenToAnybody()
{
    if (!GrabInProgress)
    {
	COPYBITS(ClientsWithInput, SavedClientsWithInput);
	COPYBITS(AllSockets, SavedAllSockets);
	COPYBITS(AllClients, SavedAllClients);
	GrabInProgress = TRUE;
	reallyGrabbed = FALSE;
    }
    else
    {
	COPYBITS(AllClients, pendingActiveClients);
	reallyGrabbed = TRUE;
    }
    CLEARBITS(ClientsWithInput);
    UNSETBITS(AllSockets, AllClients);
    CLEARBITS(AllClients);
    isItTimeToYield = TRUE;
}

void
PayAttentionToClientsAgain()
{
    if (reallyGrabbed)
    {
	ORBITS(AllSockets, AllSockets, pendingActiveClients);
	ORBITS(AllClients, AllClients, pendingActiveClients);
    }
    else
    {
	ListenToAllClients();
    }
    reallyGrabbed = FALSE;
}

#endif
