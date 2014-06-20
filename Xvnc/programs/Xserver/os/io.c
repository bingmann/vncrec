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
/* $XConsortium: io.c,v 1.91 95/01/25 11:14:28 kaleb Exp $ */
/*****************************************************************
 * i/o functions
 *
 *   WriteToClient, ReadRequestFromClient
 *   InsertFakeRequest, ResetCurrentRequest
 *
 *****************************************************************/

#include <stdio.h>
#include <X11/Xtrans.h>
#ifdef X_NOT_STDC_ENV
extern int errno;
#endif
#include "Xmd.h"
#include <errno.h>
#include <sys/uio.h>
#include "X.h"
#include "Xproto.h"
#include "os.h"
#include "osdep.h"
#include "opaque.h"
#include "dixstruct.h"
#include "misc.h"

/* check for both EAGAIN and EWOULDBLOCK, because some supposedly POSIX
 * systems are broken and return EWOULDBLOCK when they should return EAGAIN
 */
#if defined(EAGAIN) && defined(EWOULDBLOCK)
#define ETEST(err) (err == EAGAIN || err == EWOULDBLOCK)
#else
#ifdef EAGAIN
#define ETEST(err) (err == EAGAIN)
#else
#define ETEST(err) (err == EWOULDBLOCK)
#endif
#endif

extern FdSet ClientsWithInput, IgnoredClientsWithInput, AllClients;
extern FdSet ClientsWriteBlocked;
extern FdSet OutputPending;
extern int ConnectionTranslation[];
#ifdef LBX
extern int ConnectionOutputTranslation[];
#endif
extern Bool NewOutputPending;
extern Bool AnyClientsWriteBlocked;
 Bool CriticalOutputPending;
 int timesThisConnection = 0;
 ConnectionInputPtr FreeInputs = (ConnectionInputPtr)NULL;
 ConnectionOutputPtr FreeOutputs = (ConnectionOutputPtr)NULL;
 OsCommPtr AvailableInput = (OsCommPtr)NULL;

extern ConnectionInputPtr AllocateInputBuffer(
#if NeedFunctionPrototypes
    void
#endif
);

static ConnectionOutputPtr AllocateOutputBuffer(
#if NeedFunctionPrototypes
    void
#endif
);

#ifdef LBX
extern ClientPtr   WritingClient;
#endif

#define get_req_len(req,cli) ((cli)->swapped ? \
			      lswaps((req)->length) : (req)->length)

#ifdef LBX
StandardRequestLength(req,client,got,partp)
    xReq	*req;
    ClientPtr	client;
    int		got;
    Bool    	*partp;
{
    int	    len;
    
    if (!req)
	req = (xReq *) client->requestBuffer;
    if (got < sizeof (xReq))
    {
	*partp = TRUE;
	return sizeof (xReq);
    }
    len = get_req_len(req,client) << 2;
    if (len > MAXBUFSIZE)
    {
	*partp = TRUE;
	return -1;
    }
    *partp = FALSE;
    return len;
}
#endif

#ifdef BIGREQS
typedef struct {
	CARD8 reqType;
	CARD8 data;
	CARD16 zero B16;
        CARD32 length B32;
} xBigReq;

#define get_big_req_len(req,cli) ((cli)->swapped ? \
				  lswapl(((xBigReq *)(req))->length) : \
				  ((xBigReq *)(req))->length)
#endif

#define MAX_TIMES_PER         10

/*****************************************************************
 * ReadRequestFromClient
 *    Returns one request in client->requestBuffer.  Return status is:
 *
 *    > 0  if  successful, specifies length in bytes of the request
 *    = 0  if  entire request is not yet available
 *    < 0  if  client should be terminated
 *
 *    The request returned must be contiguous so that it can be
 *    cast in the dispatcher to the correct request type.  Because requests
 *    are variable length, ReadRequestFromClient() must look at the first 4
 *    bytes of a request to determine the length (the request length is
 *    always the 3rd and 4th bytes of the request).  
 *
 *    Note: in order to make the server scheduler (WaitForSomething())
 *    "fair", the ClientsWithInput mask is used.  This mask tells which
 *    clients have FULL requests left in their buffers.  Clients with
 *    partial requests require a read.  Basically, client buffers
 *    are drained before select() is called again.  But, we can't keep
 *    reading from a client that is sending buckets of data (or has
 *    a partial request) because others clients need to be scheduled.
 *****************************************************************/

#define YieldControl()				\
        { isItTimeToYield = TRUE;		\
	  timesThisConnection = 0; }
#define YieldControlNoInput()			\
        { YieldControl();			\
	  BITCLEAR(ClientsWithInput, fd); }
#define YieldControlDeath()			\
        { timesThisConnection = 0; }

#ifdef LBX
int
StandardReadRequestFromClient(client)
    ClientPtr client;
#else
int
ReadRequestFromClient(client)
    ClientPtr client;
#endif
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    XtransConnInfo trans_conn = oc->trans_conn;
    register int gotnow, needed;
    int result;
    register xReq *request;
    Bool need_header;
#ifdef BIGREQS
    Bool move_header;
#endif

    if (AvailableInput)
    {
	if (AvailableInput != oc)
	{
	    register ConnectionInputPtr aci = AvailableInput->input;
	    if (aci->size > BUFWATERMARK)
	    {
		xfree(aci->buffer);
		xfree(aci);
	    }
	    else
	    {
		aci->next = FreeInputs;
		FreeInputs = aci;
	    }
	    AvailableInput->input = (ConnectionInputPtr)NULL;
	}
	AvailableInput = (OsCommPtr)NULL;
    }
    if (!oci)
    {
	if (oci = FreeInputs)
	{
	    FreeInputs = oci->next;
	}
	else if (!(oci = AllocateInputBuffer()))
	{
	    YieldControlDeath();
	    return -1;
	}
	oc->input = oci;
    }
    oci->bufptr += oci->lenLastReq;

    need_header = FALSE;
#ifdef BIGREQS
    move_header = FALSE;
#endif
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    if (gotnow < sizeof(xReq))
    {
	needed = sizeof(xReq);
	need_header = TRUE;
    }
    else
    {
	request = (xReq *)oci->bufptr;
	needed = get_req_len(request, client);
#ifdef BIGREQS
	if (!needed && client->big_requests)
	{
	    move_header = TRUE;
	    if (gotnow < sizeof(xBigReq))
	    {
		needed = sizeof(xBigReq) >> 2;
		need_header = TRUE;
	    }
	    else
		needed = get_big_req_len(request, client);
	}
#endif
	client->req_len = needed;
	needed <<= 2;
    }
    if (gotnow < needed)
    {
	oci->lenLastReq = 0;
	if (needed > MAXBUFSIZE)
	{
	    YieldControlDeath();
	    return -1;
	}
	if ((gotnow == 0) ||
	    ((oci->bufptr - oci->buffer + needed) > oci->size))
	{
	    if ((gotnow > 0) && (oci->bufptr != oci->buffer))
		memmove(oci->buffer, oci->bufptr, gotnow);
	    if (needed > oci->size)
	    {
		char *ibuf;

		ibuf = (char *)xrealloc(oci->buffer, needed);
		if (!ibuf)
		{
		    YieldControlDeath();
		    return -1;
		}
		oci->size = needed;
		oci->buffer = ibuf;
	    }
	    oci->bufptr = oci->buffer;
	    oci->bufcnt = gotnow;
	}
	/*  XXX this is a workaround.  This function is sometimes called
	 *  after the trans_conn has been freed.  In this case trans_conn
	 *  will be null.  Really ought to restructure things so that we
	 *  never get here in those circumstances.
	 */
	if (!trans_conn)
	{
	    /*  treat as if an error occured on the read, which is what
	     *  used to happen
	     */
	    YieldControlDeath();
	    return -1;
	}
	result = _XSERVTransRead(trans_conn, oci->buffer + oci->bufcnt, 
		      oci->size - oci->bufcnt); 
	if (result <= 0)
	{
	    if ((result < 0) && ETEST(errno))
	    {
		YieldControlNoInput();
		return 0;
	    }
	    YieldControlDeath();
	    return -1;
	}
	oci->bufcnt += result;
	gotnow += result;
	/* free up some space after huge requests */
	if ((oci->size > BUFWATERMARK) &&
	    (oci->bufcnt < BUFSIZE) && (needed < BUFSIZE))
	{
	    char *ibuf;

	    ibuf = (char *)xrealloc(oci->buffer, BUFSIZE);
	    if (ibuf)
	    {
		oci->size = BUFSIZE;
		oci->buffer = ibuf;
		oci->bufptr = ibuf + oci->bufcnt - gotnow;
	    }
	}
	if (need_header && gotnow >= needed)
	{
	    request = (xReq *)oci->bufptr;
	    needed = get_req_len(request, client);
#ifdef BIGREQS
	    if (!needed && client->big_requests)
	    {
		move_header = TRUE;
		if (gotnow < sizeof(xBigReq))
		    needed = sizeof(xBigReq) >> 2;
		else
		    needed = get_big_req_len(request, client);
	    }
#endif
	    client->req_len = needed;
	    needed <<= 2;
	}
	if (gotnow < needed)
	{
	    YieldControlNoInput();
	    return 0;
	}
    }
    if (needed == 0)
    {
#ifdef BIGREQS
	if (client->big_requests)
	    needed = sizeof(xBigReq);
	else
#endif
	    needed = sizeof(xReq);
    }
    oci->lenLastReq = needed;

    /*
     *  Check to see if client has at least one whole request in the
     *  buffer.  If there is only a partial request, treat like buffer
     *  is empty so that select() will be called again and other clients
     *  can get into the queue.   
     */

    gotnow -= needed;
    if (gotnow >= sizeof(xReq)) 
    {
	request = (xReq *)(oci->bufptr + needed);
	if (gotnow >= (result = (get_req_len(request, client) << 2))
#ifdef BIGREQS
	    && (result ||
		(client->big_requests &&
		 (gotnow >= sizeof(xBigReq) &&
		  gotnow >= (get_big_req_len(request, client) << 2))))
#endif
	    )
	    BITSET(ClientsWithInput, fd);
	else
	    YieldControlNoInput();
    }
    else
    {
	if (!gotnow)
	    AvailableInput = oc;
	YieldControlNoInput();
    }
    if (++timesThisConnection >= MAX_TIMES_PER)
	YieldControl();
#ifdef BIGREQS
    if (move_header)
    {
	request = (xReq *)oci->bufptr;
	oci->bufptr += (sizeof(xBigReq) - sizeof(xReq));
	*(xReq *)oci->bufptr = *request;
	oci->lenLastReq -= (sizeof(xBigReq) - sizeof(xReq));
	client->req_len -= (sizeof(xBigReq) - sizeof(xReq)) >> 2;
    }
#endif
    client->requestBuffer = (pointer)oci->bufptr;
    return needed;
}

/*****************************************************************
 * InsertFakeRequest
 *    Splice a consed up (possibly partial) request in as the next request.
 *
 **********************/

Bool
InsertFakeRequest(client, data, count)
    ClientPtr client;
    char *data;
    int count;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    register xReq *request;
    register int gotnow, moveup;
#ifdef LBX
    Bool part;
#endif

    if (AvailableInput)
    {
	if (AvailableInput != oc)
	{
	    register ConnectionInputPtr aci = AvailableInput->input;
	    if (aci->size > BUFWATERMARK)
	    {
		xfree(aci->buffer);
		xfree(aci);
	    }
	    else
	    {
		aci->next = FreeInputs;
		FreeInputs = aci;
	    }
	    AvailableInput->input = (ConnectionInputPtr)NULL;
	}
	AvailableInput = (OsCommPtr)NULL;
    }
    if (!oci)
    {
	if (oci = FreeInputs)
	    FreeInputs = oci->next;
	else if (!(oci = AllocateInputBuffer()))
	    return FALSE;
	oc->input = oci;
    }
    oci->bufptr += oci->lenLastReq;
    oci->lenLastReq = 0;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    if ((gotnow + count) > oci->size)
    {
	char *ibuf;

	ibuf = (char *)xrealloc(oci->buffer, gotnow + count);
	if (!ibuf)
	    return(FALSE);
	oci->size = gotnow + count;
	oci->buffer = ibuf;
	oci->bufptr = ibuf + oci->bufcnt - gotnow;
    }
    moveup = count - (oci->bufptr - oci->buffer);
    if (moveup > 0)
    {
	if (gotnow > 0)
	    memmove(oci->bufptr + moveup, oci->bufptr, gotnow);
	oci->bufptr += moveup;
	oci->bufcnt += moveup;
    }
    memmove(oci->bufptr - count, data, count);
    oci->bufptr -= count;
    request = (xReq *)oci->bufptr;
    gotnow += count;
#ifndef LBX
    if ((gotnow >= sizeof(xReq)) &&
	(gotnow >= (get_req_len(request, client) << 2)))
#else
    if (gotnow >= RequestLength (request, client, gotnow, &part) && !part)
#endif
	BITSET(ClientsWithInput, fd);
    else
	YieldControlNoInput();
    return(TRUE);
}

/*****************************************************************
 * ResetRequestFromClient
 *    Reset to reexecute the current request, and yield.
 *
 **********************/

ResetCurrentRequest(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    register xReq *request;
    int gotnow, needed;
#ifdef LBX
    Bool part;
#endif

    if (AvailableInput == oc)
	AvailableInput = (OsCommPtr)NULL;
    oci->lenLastReq = 0;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
#ifdef LBX
    request = (xReq *)oci->bufptr;
    if (gotnow >= RequestLength (request, client, gotnow, &part) && !part)
    {
	if (GETBIT(AllClients, fd))
	{
	    BITSET(ClientsWithInput, fd);
	}
	else
	{
	    BITSET(IgnoredClientsWithInput, fd);
	}
	YieldControl();
    }
    else
	YieldControlNoInput();
#else
    if (gotnow < sizeof(xReq))
    {
	YieldControlNoInput();
    }
    else
    {
	request = (xReq *)oci->bufptr;
	needed = get_req_len(request, client);
#ifdef BIGREQS
	if (!needed && client->big_requests)
	{
	    oci->bufptr -= sizeof(xBigReq) - sizeof(xReq);
	    *(xReq *)oci->bufptr = *request;
	    ((xBigReq *)oci->bufptr)->length = client->req_len;
	    if (client->swapped)
	    {
		char n;
		swapl(&((xBigReq *)oci->bufptr)->length, n);
	    }
	}
#endif
	if (gotnow >= (needed << 2))
	{
	    if (GETBIT(AllClients, fd))
	    {
		BITSET(ClientsWithInput, fd);
	    }
	    else
	    {
		BITSET(IgnoredClientsWithInput, fd);
	    }
	    YieldControl();
	}
	else
	    YieldControlNoInput();
    }
#endif	/* !LBX */
}

    /* lookup table for adding padding bytes to data that is read from
    	or written to the X socket.  */
static int padlength[4] = {0, 3, 2, 1};

 /********************
 * FlushClient()
 *    If the client isn't keeping up with us, then we try to continue
 *    buffering the data and set the apropriate bit in ClientsWritable
 *    (which is used by WaitFor in the select).  If the connection yields
 *    a permanent error, or we can't allocate any more space, we then
 *    close the connection.
 *
 **********************/

int
#ifdef LBX
StandardFlushClient(who, oc, extraBuf, extraCount)
#else
FlushClient(who, oc, extraBuf, extraCount)
#endif
    ClientPtr who;
    OsCommPtr oc;
    char *extraBuf;
    int extraCount; /* do not modify... returned below */
{
    register ConnectionOutputPtr oco = oc->output;
    int connection = oc->fd;
    XtransConnInfo trans_conn = oc->trans_conn;
    struct iovec iov[3];
    static char padBuffer[3];
    long written;
    long padsize;
    long notWritten;
    long todo;

    if (!oco)
	return 0;
    written = 0;
    padsize = padlength[extraCount & 3];
    notWritten = oco->count + extraCount + padsize;
    todo = notWritten;
    while (notWritten) {
	long before = written;	/* amount of whole thing written */
	long remain = todo;	/* amount to try this time, <= notWritten */
	int i = 0;
	long len;

	/* You could be very general here and have "in" and "out" iovecs
	 * and write a loop without using a macro, but what the heck.  This
	 * translates to:
	 *
	 *     how much of this piece is new?
	 *     if more new then we are trying this time, clamp
	 *     if nothing new
	 *         then bump down amount already written, for next piece
	 *         else put new stuff in iovec, will need all of next piece
	 *
	 * Note that todo had better be at least 1 or else we'll end up
	 * writing 0 iovecs.
	 */
#define InsertIOV(pointer, length) \
	len = (length) - before; \
	if (len > remain) \
	    len = remain; \
	if (len <= 0) { \
	    before = (-len); \
	} else { \
	    iov[i].iov_len = len; \
	    iov[i].iov_base = (pointer) + before; \
	    i++; \
	    remain -= len; \
	    before = 0; \
	}

	InsertIOV ((char *)oco->buf, oco->count)
	InsertIOV (extraBuf, extraCount)
	InsertIOV (padBuffer, padsize)

	errno = 0;
#ifdef LBX
	WritingClient = who;
	if ((len = (*oc->Writev) (connection, iov, i)) >= 0)
#else
	if (trans_conn && (len = _XSERVTransWritev(trans_conn, iov, i)) >= 0)
#endif
	{
	    written += len;
	    notWritten -= len;
	    todo = notWritten;
	}
	else if (ETEST(errno)
#ifdef SUNSYSV /* check for another brain-damaged OS bug */
		 || (errno == 0)
#endif
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
		 || ((errno == EMSGSIZE) && (todo == 1))
#endif
		)
	{
	    /* If we've arrived here, then the client is stuffed to the gills
	       and not ready to accept more.  Make a note of it and buffer
	       the rest. */
	    BITSET(ClientsWriteBlocked, connection);
	    AnyClientsWriteBlocked = TRUE;

	    if (written < oco->count)
	    {
		if (written > 0)
		{
		    oco->count -= written;
		    memmove((char *)oco->buf,
			    (char *)oco->buf + written,
			  oco->count);
		    written = 0;
		}
	    }
	    else
	    {
		written -= oco->count;
		oco->count = 0;
	    }

	    if (notWritten > oco->size)
	    {
		unsigned char *obuf;

		obuf = (unsigned char *)xrealloc(oco->buf,
						 notWritten + BUFSIZE);
		if (!obuf)
		{
		    _XSERVTransClose(oc->trans_conn);
		    oc->trans_conn = NULL;
		    MarkClientException(who);
		    oco->count = 0;
		    return(-1);
		}
		oco->size = notWritten + BUFSIZE;
		oco->buf = obuf;
	    }

	    /* If the amount written extended into the padBuffer, then the
	       difference "extraCount - written" may be less than 0 */
	    if ((len = extraCount - written) > 0)
		memmove ((char *)oco->buf + oco->count,
			 extraBuf + written,
		       len);

	    oco->count = notWritten; /* this will include the pad */
	    /* return only the amount explicitly requested */
	    return extraCount;
	}
#ifdef EMSGSIZE /* check for another brain-damaged OS bug */
	else if (errno == EMSGSIZE)
	{
	    todo >>= 1;
	}
#endif
	else
	{
	    if (oc->trans_conn)
	    {
		_XSERVTransClose(oc->trans_conn);
		oc->trans_conn = NULL;
	    }
	    MarkClientException(who);
	    oco->count = 0;
	    return(-1);
	}
    }

    /* everything was flushed out */
    oco->count = 0;
    /* check to see if this client was write blocked */
    if (AnyClientsWriteBlocked)
    {
	BITCLEAR(ClientsWriteBlocked, oc->fd);
 	if (! ANYSET(ClientsWriteBlocked))
	    AnyClientsWriteBlocked = FALSE;
    }
    if (oco->size > BUFWATERMARK)
    {
	xfree(oco->buf);
	xfree(oco);
    }
    else
    {
	oco->next = FreeOutputs;
	FreeOutputs = oco;
    }
    oc->output = (ConnectionOutputPtr)NULL;
    return extraCount; /* return only the amount explicitly requested */
}

 /********************
 * FlushAllOutput()
 *    Flush all clients with output.  However, if some client still
 *    has input in the queue (more requests), then don't flush.  This
 *    will prevent the output queue from being flushed every time around
 *    the round robin queue.  Now, some say that it SHOULD be flushed
 *    every time around, but...
 *
 **********************/

void
FlushAllOutput()
{
    register int index, base, mask;
    OsCommPtr oc;
    register ClientPtr client;

    if (! NewOutputPending)
	return;

    /*
     * It may be that some client still has critical output pending,
     * but he is not yet ready to receive it anyway, so we will
     * simply wait for the select to tell us when he's ready to receive.
     */
    CriticalOutputPending = FALSE;
    NewOutputPending = FALSE;

    for (base = 0; base < mskcnt; base++)
    {
	mask = OutputPending[ base ];
	OutputPending[ base ] = 0;
	while (mask)
	{
	    index = ffs(mask) - 1;
	    mask &= ~lowbit(mask);
#ifdef LBX
	    if ((index = ConnectionOutputTranslation[(base << 5) + index]) == 0)
#else
	    if ((index = ConnectionTranslation[(base << 5) + index]) == 0)
#endif
		continue;
	    client = clients[index];
	    if (client->clientGone)
		continue;
	    oc = (OsCommPtr)client->osPrivate;
	    if (GETBIT(ClientsWithInput, oc->fd))
	    {
		BITSET(OutputPending, oc->fd); /* set the bit again */
		NewOutputPending = TRUE;
	    }
	    else
		(void)FlushClient(client, oc, (char *)NULL, 0);
	}
    }

}

void
FlushIfCriticalOutputPending()
{
    if (CriticalOutputPending)
	FlushAllOutput();
}

void
SetCriticalOutputPending()
{
    CriticalOutputPending = TRUE;
}

/*****************
 * WriteToClient
 *    Copies buf into ClientPtr.buf if it fits (with padding), else
 *    flushes ClientPtr.buf and buf to client.  As of this writing,
 *    every use of WriteToClient is cast to void, and the result
 *    is ignored.  Potentially, this could be used by requests
 *    that are sending several chunks of data and want to break
 *    out of a loop on error.  Thus, we will leave the type of
 *    this routine as int.
 *****************/

int
#ifdef LBX
StandardWriteToClient (who, count, buf)
#else
WriteToClient (who, count, buf)
#endif
    ClientPtr who;
    char *buf;
    int count;
{
    OsCommPtr oc = (OsCommPtr)who->osPrivate;
    register ConnectionOutputPtr oco = oc->output;
    int padBytes;

    if (!count)
	return(0);

    if (!oco)
    {
	if (oco = FreeOutputs)
	{
	    FreeOutputs = oco->next;
	}
	else if (!(oco = AllocateOutputBuffer()))
	{
	    _XSERVTransClose(oc->trans_conn);
	    oc->trans_conn = NULL;
	    MarkClientException(who);
	    return -1;
	}
	oc->output = oco;
    }

    padBytes =  padlength[count & 3];

    if (oco->count + count + padBytes > oco->size)
    {
	BITCLEAR(OutputPending, oc->fd);
	CriticalOutputPending = FALSE;
	NewOutputPending = FALSE;
	return FlushClient(who, oc, buf, count);
    }

    NewOutputPending = TRUE;
    BITSET(OutputPending, oc->fd);
    memmove((char *)oco->buf + oco->count, buf, count);
    oco->count += count + padBytes;
    
    return(count);
}

ConnectionInputPtr
AllocateInputBuffer()
{
    register ConnectionInputPtr oci;

    oci = (ConnectionInputPtr)xalloc(sizeof(ConnectionInput));
    if (!oci)
	return (ConnectionInputPtr)NULL;
    oci->buffer = (char *)xalloc(BUFSIZE);
    if (!oci->buffer)
    {
	xfree(oci);
	return (ConnectionInputPtr)NULL;
    }
    oci->size = BUFSIZE;
    oci->bufptr = oci->buffer;
    oci->bufcnt = 0;
    oci->lenLastReq = 0;
    return oci;
}

static ConnectionOutputPtr
AllocateOutputBuffer()
{
    register ConnectionOutputPtr oco;

    oco = (ConnectionOutputPtr)xalloc(sizeof(ConnectionOutput));
    if (!oco)
	return (ConnectionOutputPtr)NULL;
    oco->buf = (unsigned char *) xalloc(BUFSIZE);
    if (!oco->buf)
    {
	xfree(oco);
	return (ConnectionOutputPtr)NULL;
    }
    oco->size = BUFSIZE;
    oco->count = 0;
#ifdef LBX
    oco->nocompress = FALSE;
#endif
    return oco;
}

void
FreeOsBuffers(oc)
    OsCommPtr oc;
{
    register ConnectionInputPtr oci;
    register ConnectionOutputPtr oco;

    if (AvailableInput == oc)
	AvailableInput = (OsCommPtr)NULL;
    if (oci = oc->input)
    {
	if (FreeInputs)
	{
	    xfree(oci->buffer);
	    xfree(oci);
	}
	else
	{
	    FreeInputs = oci;
	    oci->next = (ConnectionInputPtr)NULL;
	    oci->bufptr = oci->buffer;
	    oci->bufcnt = 0;
	    oci->lenLastReq = 0;
	}
    }
    if (oco = oc->output)
    {
	if (FreeOutputs)
	{
	    xfree(oco->buf);
	    xfree(oco);
	}
	else
	{
	    FreeOutputs = oco;
	    oco->next = (ConnectionOutputPtr)NULL;
	    oco->count = 0;
	}
    }
#ifdef LBX
    if (oco = oc->ofirst) {
	ConnectionOutputPtr nextoco;
	do {
	    nextoco = oco->next;
	    xfree(oco->buf);
	    xfree(oco);
	    if (oco == oc->olast)
		break;
	    oco = nextoco;
	} while (1);
    }
#endif
}

void
ResetOsBuffers()
{
    register ConnectionInputPtr oci;
    register ConnectionOutputPtr oco;

    while (oci = FreeInputs)
    {
	FreeInputs = oci->next;
	xfree(oci->buffer);
	xfree(oci);
    }
    while (oco = FreeOutputs)
    {
	FreeOutputs = oco->next;
	xfree(oco->buf);
	xfree(oco);
    }
}
