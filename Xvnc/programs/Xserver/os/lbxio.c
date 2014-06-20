/***********************************************************
Copyright 1987, 1989 by Digital Equipment Corporation, Maynard, Massachusetts,
and the Massachusetts Institute of Technology, Cambridge, Massachusetts.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Digital or MIT not be
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
/* $XConsortium: lbxio.c,v 1.2 94/12/02 17:33:49 mor Exp $ */
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
#include <sys/param.h>
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
extern int ConnectionOutputTranslation[];
extern Bool NewOutputPending;
extern Bool AnyClientsWriteBlocked;
extern Bool CriticalOutputPending;
extern int timesThisConnection;
extern ConnectionInputPtr FreeInputs;
extern ConnectionOutputPtr FreeOutputs;
extern OsCommPtr AvailableInput;

extern ConnectionInputPtr AllocateInputBuffer(
#if NeedFunctionPrototypes
    void
#endif
);

static ConnectionOutputPtr AllocateUncompBuffer();

ClientPtr   ReadingClient;
ClientPtr   WritingClient;

#define get_req_len(req,cli) (((cli)->swapped ? \
			      lswaps((req)->length) : (req)->length) << 2)

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

int
LBXReadRequestFromClient(client)
    ClientPtr client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    int fd = oc->fd;
    XtransConnInfo trans_conn = oc->trans_conn;
    register int gotnow, needed;
    int result;
    register xReq *request;
    Bool need_header;
    int	nextneeds;
    Bool part;
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
    oci->lenLastReq = 0;

    need_header = FALSE;
#ifdef BIGREQS
    move_header = FALSE;
#endif
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    client->requestBuffer = (pointer)oci->bufptr;
    needed = RequestLength (NULL, client, gotnow, &part);
    client->req_len = needed >> 2;
    if (gotnow < needed || part)
    {
	if (needed == -1)
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
	ReadingClient = client;
#ifndef XTRANS
	result = (*oc->Read)(fd, oci->buffer + oci->bufcnt, 
		      oci->size - oci->bufcnt); 
#else
	result = _XSERVTransRead(trans_conn, oci->buffer + oci->bufcnt, 
		      oci->size - oci->bufcnt); 
#endif
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
	client->requestBuffer = (pointer) oci->bufptr;
	if (part && gotnow >= needed)
	{
	    needed = RequestLength (NULL, client, gotnow, &part);
	    client->req_len = needed >> 2;
	}
	if (gotnow < needed || part)
	{
	    if (needed == -1)
	    {
		YieldControlDeath();
		return -1;
	    }
	    YieldControlNoInput();
	    return 0;
	}
    }
    oci->lenLastReq = needed;

    /*
     *  Check to see if client has at least one whole request in the
     *  buffer.  If there is only a partial request, treat like buffer
     *  is empty so that select() will be called again and other clients
     *  can get into the queue.   
     */

    if (gotnow > needed)
    {
	request = (xReq *)(oci->bufptr + needed);
	nextneeds = RequestLength (request, client, gotnow - needed, &part);
	if (gotnow >= needed + nextneeds && !part)
	    BITSET(ClientsWithInput, fd);
	else
	    YieldControlNoInput();
    }
    else
    {
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
    return needed;
}

Bool
SwitchClientInput (to, check)
    ClientPtr to;
    ClientPtr check;
{
    OsCommPtr ocTo = (OsCommPtr) to->osPrivate;
    ConnectionInputPtr	ociTo = ocTo->input;
    
    ConnectionTranslation[ocTo->fd] = to->index;
    YieldControl();
    CheckPendingClientInput (check);
    return TRUE;
}

SwitchClientOutput (from, to)
    ClientPtr	from, to;
{
    OsCommPtr ocFrom = (OsCommPtr) from->osPrivate;
    OsCommPtr ocTo = (OsCommPtr) to->osPrivate;
    ConnectionOutputPtr	ocoFrom = ocFrom->output;
    ConnectionOutputPtr	ocoTo = ocTo->output;
    
    ConnectionOutputTranslation[ocTo->fd] = to->index;
    if (PendingClientOutput (to))
    {
	NewOutputPending = TRUE;
	BITSET(OutputPending, ocTo->fd);
    }
}

int
PendingClientOutput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr) client->osPrivate;
    ConnectionOutputPtr	oco = oc->output;
    
    return (oco && oco->count != 0) || oc->ofirst;
}

int
CheckPendingClientInput (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;
    xReq    *request;
    int	    gotnow;
    int	    needed;
    Bool    part;
    
    if (!oci)
	return 0;
    needed = oci->lenLastReq;
    gotnow = oci->bufcnt + oci->buffer - oci->bufptr;
    request = (xReq *) (oci->bufptr + needed);
    if (gotnow >= needed + RequestLength(request, client, gotnow - needed, &part) && !part)
    {
	BITSET(ClientsWithInput, oc->fd);
	return 1;
    }
    return 0;
}

void
MarkConnectionWriteBlocked (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;

    BITSET(ClientsWriteBlocked, oc->fd);
    AnyClientsWriteBlocked = TRUE;
}

int
BytesInClientBuffer (client)
    ClientPtr	client;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;

    if (!oci)
	return 0;
    return oci->bufcnt + oci->buffer - (oci->bufptr + oci->lenLastReq);
}

SkipInClientBuffer (client, nbytes, lenLastReq)
    ClientPtr	client;
    int		nbytes;
    int		lenLastReq;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oci = oc->input;

    if (!oci)
	return;
    oci->bufptr += nbytes;
    oci->lenLastReq = lenLastReq;
}

/*****************************************************************
 * AppendFakeRequest
 *    Append a (possibly partial) request in as the last request.
 *
 **********************/
 
Bool
AppendFakeRequest (client, data, count)
    ClientPtr client;
    char *data;
    int count;
{
    OsCommPtr oc = (OsCommPtr)client->osPrivate;
    register ConnectionInputPtr oco = oc->input;
    int fd = oc->fd;
    register xReq *request;
    register int gotnow, moveup;
    Bool part;

#ifdef NOTDEF
    /* can't do this as the data may actually be coming from this buffer! */
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
#endif
    if (!oco)
    {
	if (oco = FreeInputs)
	    FreeInputs = oco->next;
	else if (!(oco = AllocateInputBuffer()))
	    return FALSE;
	oc->input = oco;
    }
    oco->bufptr += oco->lenLastReq;
    oco->lenLastReq = 0;
    gotnow = oco->bufcnt + oco->buffer - oco->bufptr;
    if ((gotnow + count) > oco->size)
    {
	char *ibuf;

	ibuf = (char *)xrealloc(oco->buffer, gotnow + count);
	if (!ibuf)
	    return(FALSE);
	oco->size = gotnow + count;
	oco->buffer = ibuf;
	oco->bufptr = ibuf;
    }
    bcopy(data, oco->bufptr + gotnow, count);
    oco->bufcnt += count;
    gotnow += count;
    if (gotnow >= RequestLength (oco->bufptr, client, gotnow, &part) && !part)
	BITSET(ClientsWithInput, fd);
    else
	YieldControlNoInput();
    return(TRUE);
}

static int
ExpandOutputBuffer(oco, len)
    ConnectionOutputPtr oco;
    int len;
{
    unsigned char *obuf;

    obuf = (unsigned char *)xrealloc(oco->buf, len + BUFSIZE);
    if (!obuf)
    {
	oco->count = 0;
	return(-1);
    }
    oco->size = len + BUFSIZE;
    oco->buf = obuf;
    return 0;
}

    /* lookup table for adding padding bytes to data that is read from
    	or written to the X socket.  */
static int padlength[4] = {0, 3, 2, 1};

int
LbxFlushClient(who, oc, extraBuf, extraCount)
    ClientPtr who;
    OsCommPtr oc;
    char *extraBuf;
    int extraCount; /* do not modify... returned below */
{
    ConnectionOutputPtr nextbuf;
    register ConnectionOutputPtr oco;
    int retval = extraCount;

    if (!oc->ofirst) {
	return StandardFlushClient(who, oc, extraBuf, extraCount);
    }

    if (oco = oc->output) {
	oc->olast->next = oco;
	oc->olast = oco;
    }

    oco = oc->ofirst;
    do {
	Bool nocomp = oco->nocompress;
	nextbuf = (oco != oc->olast) ? oco->next : NULL;
	oc->output = oco;
	if (nocomp)
	    (*oc->compressOff)(oc->fd);
	if (oc->olast == oco) {
	    StandardFlushClient(who, oc, extraBuf, extraCount);
	    extraCount = 0;
	}
	else
	    StandardFlushClient(who, oc, (char *)NULL, 0);
	if (nocomp)
	    (*oc->compressOn)(oc->fd);
	if (oc->output != (ConnectionOutputPtr) NULL) {
	    oc->output = (ConnectionOutputPtr) NULL;
	    break;
	}
    } while (oco = nextbuf);
    oc->ofirst = oco;

    /*
     * If we didn't get a chance to flush the extraBuf above, then
     * we need to buffer it here.
     */
    if (extraCount) {
	int newlen = oco->count + extraCount + padlength[extraCount & 3];
	oco = oc->olast;
	if (ExpandOutputBuffer(oco, newlen) < 0) {
	    _XSERVTransClose(oc->trans_conn);
	    oc->trans_conn = NULL;
	    MarkClientException(who);
	    return(-1);
	}
	bcopy(extraBuf, (char *)oco->buf + oco->count, extraCount);
	oco->count = newlen;
    }

    return retval;
}

int
UncompressWriteToClient (who, count, buf)
    ClientPtr who;
    char *buf;
    int count;
{
    OsCommPtr oc = (OsCommPtr)who->osPrivate;
    register ConnectionOutputPtr oco;
    int paddedLen = count + padlength[count & 3];

    if (!count)
	return(0);

    if (oco = oc->output) {
	/*
	 * we're currently filling a buffer, and it must be compressible,
	 * so put it on the queue
	 */
	if (oc->ofirst) {
	    oc->olast->next = oco;
	    oc->olast = oco;
	}
	else {
	    oc->ofirst = oc->olast = oco;
	}
	oco = oc->output = (ConnectionOutputPtr)NULL;
    }
    else if (oc->ofirst) {
	oco = oc->olast;
	if (!oco->nocompress || ((oco->count + paddedLen) > oco->size))
	    oco = (ConnectionOutputPtr)NULL;
    }

    if (!oco) {
	if (!(oco = AllocateUncompBuffer(paddedLen))) {
	    _XSERVTransClose(oc->trans_conn);
	    oc->trans_conn = NULL;
	    MarkClientException(who);
	    return -1;
	}
    }
    bcopy(buf, (char *)oco->buf + oco->count, count);
    oco->count += paddedLen;

    if (oc->ofirst) {
	oc->olast->next = oco;
	oc->olast = oco;
    }
    else {
	oc->ofirst = oc->olast = oco;
    }

    NewOutputPending = TRUE;
    BITSET(OutputPending, oc->fd);
    return(count);
}

static ConnectionOutputPtr
AllocateUncompBuffer(count)
    int count;
{
    register ConnectionOutputPtr oco;
    int len = (count > BUFSIZE) ? count : BUFSIZE;

    oco = (ConnectionOutputPtr)xalloc(sizeof(ConnectionOutput));
    if (!oco)
	return (ConnectionOutputPtr)NULL;
    oco->buf = (unsigned char *) xalloc(len);
    if (!oco->buf)
    {
	xfree(oco);
	return (ConnectionOutputPtr)NULL;
    }
    oco->size = len;
    oco->count = 0;
    oco->nocompress = TRUE;
    return oco;
}
