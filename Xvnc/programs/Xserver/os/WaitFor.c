/***********************************************************

Copyright (c) 1987  X Consortium

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


Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.

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

/* $XConsortium: WaitFor.c,v 1.68 94/04/17 20:26:52 dpw Exp $ */

/*****************************************************************
 * OS Dependent input routines:
 *
 *  WaitForSomething
 *  TimerForce, TimerSet, TimerCheck, TimerFree
 *
 *****************************************************************/

#include "Xos.h"			/* for strings, fcntl, time */

#include <errno.h>
#ifdef X_NOT_STDC_ENV
extern int errno;
#endif

#include <stdio.h>
#include "X.h"
#include "misc.h"

#include <sys/param.h>
#include "osdep.h"
#include "dixstruct.h"
#include "opaque.h"

extern FdSet AllSockets;
extern FdSet AllClients;
extern FdSet LastSelectMask;
extern FdMask WellKnownConnections;
extern FdSet EnabledDevices;
extern FdSet ClientsWithInput;
extern FdSet ClientsWriteBlocked;
extern FdSet OutputPending;

extern int ConnectionTranslation[];

extern Bool NewOutputPending;
extern Bool AnyClientsWriteBlocked;

extern WorkQueuePtr workQueue;

#ifdef apollo
extern FdSet apInputMask;

static FdSet LastWriteMask;
#endif

#ifdef XTESTEXT1
/*
 * defined in xtestext1dd.c
 */
extern int playback_on;
#endif /* XTESTEXT1 */

struct _OsTimerRec {
    OsTimerPtr		next;
    CARD32		expires;
    OsTimerCallback	callback;
    pointer		arg;
};

static void DoTimer();
static OsTimerPtr timers;

/*****************
 * WaitForSomething:
 *     Make the server suspend until there is
 *	1. data from clients or
 *	2. input events available or
 *	3. ddx notices something of interest (graphics
 *	   queue ready, etc.) or
 *	4. clients that have buffered replies/events are ready
 *
 *     If the time between INPUT events is
 *     greater than ScreenSaverTime, the display is turned off (or
 *     saved, depending on the hardware).  So, WaitForSomething()
 *     has to handle this also (that's why the select() has a timeout.
 *     For more info on ClientsWithInput, see ReadRequestFromClient().
 *     pClientsReady is an array to store ready client->index values into.
 *****************/

static INT32 timeTilFrob = 0;		/* while screen saving */

int
WaitForSomething(pClientsReady)
    int *pClientsReady;
{
    int i;
    struct timeval waittime, *wt;
    INT32 timeout;
    FdSet clientsReadable;
    FdSet clientsWritable;
    int curclient;
    int selecterr;
    int nready;
    FdSet devicesReadable;
    CARD32 now;

    CLEARBITS(clientsReadable);

    /* We need a while loop here to handle 
       crashed connections and the screen saver timeout */
    while (1)
    {
	/* deal with any blocked jobs */
	if (workQueue)
	    ProcessWorkQueue();

	if (ANYSET(ClientsWithInput))
	{
	    COPYBITS(ClientsWithInput, clientsReadable);
	    break;
	}
	if (ScreenSaverTime || timers)
	    now = GetTimeInMillis();
	wt = NULL;
	if (timers)
	{
	    while (timers && timers->expires <= now)
		DoTimer(timers, now, &timers);
	    if (timers)
	    {
		timeout = timers->expires - now;
		waittime.tv_sec = timeout / MILLI_PER_SECOND;
		waittime.tv_usec = (timeout % MILLI_PER_SECOND) *
		    (1000000 / MILLI_PER_SECOND);
		wt = &waittime;
	    }
	}
	if (ScreenSaverTime)
	{
	    timeout = (ScreenSaverTime -
		       (now - lastDeviceEventTime.milliseconds));
	    if (timeout <= 0) /* may be forced by AutoResetServer() */
	    {
		INT32 timeSinceSave;

		timeSinceSave = -timeout;
		if ((timeSinceSave >= timeTilFrob) && (timeTilFrob >= 0))
		{
		    ResetOsBuffers(); /* not ideal, but better than nothing */
		    SaveScreens(SCREEN_SAVER_ON, ScreenSaverActive);
		    if (ScreenSaverInterval)
			/* round up to the next ScreenSaverInterval */
			timeTilFrob = ScreenSaverInterval *
				((timeSinceSave + ScreenSaverInterval) /
					ScreenSaverInterval);
		    else
			timeTilFrob = -1;
		}
		timeout = timeTilFrob - timeSinceSave;
	    }
	    else
	    {
		if (timeout > ScreenSaverTime)
		    timeout = ScreenSaverTime;
		timeTilFrob = 0;
	    }
	    if (timeTilFrob >= 0 && (!wt || timeout < (timers->expires - now)))
	    {
		waittime.tv_sec = timeout / MILLI_PER_SECOND;
		waittime.tv_usec = (timeout % MILLI_PER_SECOND) *
					(1000000 / MILLI_PER_SECOND);
		wt = &waittime;
	    }
	}
	COPYBITS(AllSockets, LastSelectMask);
#ifdef apollo
        COPYBITS(apInputMask, LastWriteMask);
#endif
	BlockHandler((pointer)&wt, (pointer)LastSelectMask);
	if (NewOutputPending)
	    FlushAllOutput();
#ifdef XTESTEXT1
	/* XXX how does this interact with new write block handling? */
	if (playback_on) {
	    wt = &waittime;
	    XTestComputeWaitTime (&waittime);
	}
#endif /* XTESTEXT1 */
	/* keep this check close to select() call to minimize race */
	if (dispatchException)
	    i = -1;
	else if (AnyClientsWriteBlocked)
	{
	    COPYBITS(ClientsWriteBlocked, clientsWritable);
	    i = select (MAXSOCKS, (int *)LastSelectMask,
			(int *)clientsWritable, (int *) NULL, wt);
	}
	else
#ifdef apollo
	    i = select (MAXSOCKS, (int *)LastSelectMask,
			(int *)LastWriteMask, (int *) NULL, wt);
#else
	    i = select (MAXSOCKS, (int *)LastSelectMask,
			(int *) NULL, (int *) NULL, wt);
#endif
	selecterr = errno;
	WakeupHandler(i, (pointer)LastSelectMask);
#ifdef XTESTEXT1
	if (playback_on) {
	    i = XTestProcessInputAction (i, &waittime);
	}
#endif /* XTESTEXT1 */
	if (i <= 0) /* An error or timeout occurred */
	{

	    if (dispatchException)
		return 0;
	    CLEARBITS(clientsWritable);
	    if (i < 0) 
		if (selecterr == EBADF)    /* Some client disconnected */
		{
		    CheckConnections ();
		    if (! ANYSET (AllClients))
			return 0;
		}
		else if (selecterr != EINTR)
		    ErrorF("WaitForSomething(): select: errno=%d\n",
			selecterr);
	    if (timers)
	    {
		now = GetTimeInMillis();
		while (timers && timers->expires <= now)
		    DoTimer(timers, now, &timers);
	    }
	    if (*checkForInput[0] != *checkForInput[1])
		return 0;
	}
	else
	{
	    if (AnyClientsWriteBlocked && ANYSET (clientsWritable))
	    {
		NewOutputPending = TRUE;
		ORBITS(OutputPending, clientsWritable, OutputPending);
		UNSETBITS(ClientsWriteBlocked, clientsWritable);
		if (! ANYSET(ClientsWriteBlocked))
		    AnyClientsWriteBlocked = FALSE;
	    }

	    MASKANDSETBITS(devicesReadable, LastSelectMask, EnabledDevices);
	    MASKANDSETBITS(clientsReadable, LastSelectMask, AllClients); 
	    if (LastSelectMask[0] & WellKnownConnections) 
		QueueWorkProc(EstablishNewConnections, NULL,
			      (pointer)LastSelectMask[0]);
	    if (ANYSET (devicesReadable) || ANYSET (clientsReadable))
		break;
	}
    }

    nready = 0;
    if (ANYSET(clientsReadable))
    {
	for (i=0; i<mskcnt; i++)
	{
	    int highest_priority;

	    while (clientsReadable[i])
	    {
	        int client_priority, client_index;

		curclient = ffs (clientsReadable[i]) - 1;
		client_index = ConnectionTranslation[curclient + (i << 5)];
#ifdef XSYNC
		/*  We implement "strict" priorities.
		 *  Only the highest priority client is returned to
		 *  dix.  If multiple clients at the same priority are
		 *  ready, they are all returned.  This means that an
		 *  aggressive client could take over the server.
		 *  This was not considered a big problem because
		 *  aggressive clients can hose the server in so many 
		 *  other ways :)
		 */
		client_priority = clients[client_index]->priority;
		if (nready == 0 || client_priority > highest_priority)
		{
		    /*  Either we found the first client, or we found
		     *  a client whose priority is greater than all others
		     *  that have been found so far.  Either way, we want 
		     *  to initialize the list of clients to contain just
		     *  this client.
		     */
		    pClientsReady[0] = client_index;
		    highest_priority = client_priority;
		    nready = 1;
		}
		/*  the following if makes sure that multiple same-priority 
		 *  clients get batched together
		 */
		else if (client_priority == highest_priority)
#endif
		{
		    pClientsReady[nready++] = client_index;
		}
		clientsReadable[i] &= ~(((FdMask)1) << curclient);
	    }
	}	
    }
    return nready;
}

#ifndef ANYSET
/*
 * This is not always a macro.
 */
ANYSET(src)
    FdMask	*src;
{
    int i;

    for (i=0; i<mskcnt; i++)
	if (src[ i ])
	    return (TRUE);
    return (FALSE);
}
#endif

static void
DoTimer(timer, now, prev)
    register OsTimerPtr timer;
    CARD32 now;
    OsTimerPtr *prev;
{
    CARD32 newTime;

    *prev = timer->next;
    timer->next = NULL;
    newTime = (*timer->callback)(timer, now, timer->arg);
    if (newTime)
	TimerSet(timer, 0, newTime, timer->callback, timer->arg);
}

OsTimerPtr
TimerSet(timer, flags, millis, func, arg)
    register OsTimerPtr timer;
    int flags;
    CARD32 millis;
    OsTimerCallback func;
    pointer arg;
{
    register OsTimerPtr *prev;
    CARD32 now = GetTimeInMillis();

    if (!timer)
    {
	timer = (OsTimerPtr)xalloc(sizeof(struct _OsTimerRec));
	if (!timer)
	    return NULL;
    }
    else
    {
	for (prev = &timers; *prev; prev = &(*prev)->next)
	{
	    if (*prev == timer)
	    {
		*prev = timer->next;
		if (flags & TimerForceOld)
		    (void)(*timer->callback)(timer, now, timer->arg);
		break;
	    }
	}
    }
    if (!millis)
	return timer;
    if (!(flags & TimerAbsolute))
	millis += now;
    timer->expires = millis;
    timer->callback = func;
    timer->arg = arg;
    if (millis <= now)
    {
	timer->next = NULL;
	millis = (*timer->callback)(timer, now, timer->arg);
	if (!millis)
	    return timer;
    }
    for (prev = &timers;
	 *prev && millis > (*prev)->expires;
	 prev = &(*prev)->next)
	;
    timer->next = *prev;
    *prev = timer;
    return timer;
}

Bool
TimerForce(timer)
    register OsTimerPtr timer;
{
    register OsTimerPtr *prev;
    register CARD32 newTime;

    for (prev = &timers; *prev; prev = &(*prev)->next)
    {
	if (*prev == timer)
	{
	    DoTimer(timer, GetTimeInMillis(), prev);
	    return TRUE;
	}
    }
    return FALSE;
}

void
TimerFree(timer)
    register OsTimerPtr timer;
{
    register OsTimerPtr *prev;

    if (!timer)
	return;
    for (prev = &timers; *prev; prev = &(*prev)->next)
    {
	if (*prev == timer)
	{
	    *prev = timer->next;
	    break;
	}
    }
    xfree(timer);
}

void
TimerCheck()
{
    register CARD32 now = GetTimeInMillis();

    while (timers && timers->expires <= now)
	DoTimer(timers, now, &timers);
}

void
TimerInit()
{
    OsTimerPtr timer;

    while (timer = timers)
    {
	timers = timer->next;
	xfree(timer);
    }
}
