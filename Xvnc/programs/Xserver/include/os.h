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

/* $XConsortium: os.h,v 1.64 95/01/05 19:50:01 kaleb Exp $ */

#ifndef OS_H
#define OS_H
#include "misc.h"

#ifdef INCLUDE_ALLOCA_H
#include <alloca.h>
#endif

#define NullFID ((FID) 0)

#define SCREEN_SAVER_ON   0
#define SCREEN_SAVER_OFF  1
#define SCREEN_SAVER_FORCER 2
#define SCREEN_SAVER_CYCLE  3

#ifndef MAX_REQUEST_SIZE
#define MAX_REQUEST_SIZE 65535
#endif
#ifndef MAX_BIG_REQUEST_SIZE
#define MAX_BIG_REQUEST_SIZE 1048575
#endif

typedef pointer	FID;
typedef struct _FontPathRec *FontPathPtr;
typedef struct _NewClientRec *NewClientPtr;

#ifndef NO_ALLOCA
/*
 * os-dependent definition of local allocation and deallocation
 * If you want something other than Xalloc/Xfree for ALLOCATE/DEALLOCATE
 * LOCAL then you add that in here.
 */
#ifdef __HIGHC__

#ifndef NCR
extern char *alloca();

#if HCVERSION < 21003
#define ALLOCATE_LOCAL(size)	alloca((int)(size))
pragma on(alloca);
#else /* HCVERSION >= 21003 */
#define	ALLOCATE_LOCAL(size)	_Alloca((int)(size))
#endif /* HCVERSION < 21003 */
#else /* NCR */
#define ALLOCATE_LOCAL(size)	alloca(size)
#endif

#define DEALLOCATE_LOCAL(ptr)  /* as nothing */

#endif /* defined(__HIGHC__) */


#if defined(__GNUC__) && !defined(alloca)
#define alloca __builtin_alloca
#else

/*
 * warning: old mips alloca (pre 2.10) is unusable, new one is builtin
 * Test is easy, the new one is named __builtin_alloca and comes
 * from alloca.h which #defines alloca.
 */
#ifndef NCR
#if defined(vax) || defined(sun) || defined(apollo) || defined(stellar) || defined(alloca)
/*
 * Some System V boxes extract alloca.o from /lib/libPW.a; if you
 * decide that you don't want to use alloca, you might want to fix 
 * ../os/4.2bsd/Imakefile
 */
#ifndef alloca
char *alloca();
#endif
#define ALLOCATE_LOCAL(size) alloca((int)(size))
#define DEALLOCATE_LOCAL(ptr)  /* as nothing */
#endif /* who does alloca */
#endif /* NCR */
#endif /* ! __GNUC__ */

#endif /* NO_ALLOCA */

#ifndef ALLOCATE_LOCAL
#define ALLOCATE_LOCAL(size) Xalloc((unsigned long)(size))
#define DEALLOCATE_LOCAL(ptr) Xfree((pointer)(ptr))
#endif /* ALLOCATE_LOCAL */

#define xnfalloc(size) XNFalloc((unsigned long)(size))
#define xnfrealloc(ptr, size) XNFrealloc((pointer)(ptr), (unsigned long)(size))

#define xalloc(size) Xalloc((unsigned long)(size))
#define xrealloc(ptr, size) Xrealloc((pointer)(ptr), (unsigned long)(size))
#define xfree(ptr) Xfree((pointer)(ptr))

#ifndef X_NOT_STDC_ENV
#include <string.h>
#else
#ifdef SYSV
#include <string.h>
#else
#include <strings.h>
#endif
#endif

/* have to put $(SIGNAL_DEFINES) in DEFINES in Imakefile to get this right */
#ifdef SIGNALRETURNSINT
#define SIGVAL int
#else
#define SIGVAL void
#endif

extern int WaitForSomething(
#if NeedFunctionPrototypes
    int* /*pClientsReady*/
#endif
);

extern int ReadRequestFromClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern Bool InsertFakeRequest(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    char* /*data*/,
    int /*count*/
#endif
);

extern int ResetCurrentRequest(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void FlushAllOutput(
#if NeedFunctionPrototypes
    void
#endif
);

extern void FlushIfCriticalOutputPending(
#if NeedFunctionPrototypes
    void
#endif
);

extern void SetCriticalOutputPending(
#if NeedFunctionPrototypes
    void
#endif
);

extern int WriteToClient(
#if NeedFunctionPrototypes
    ClientPtr /*who*/,
    int /*count*/,
    char* /*buf*/
#endif
);

extern void ResetOsBuffers(
#if NeedFunctionPrototypes
    void
#endif
);

extern void CreateWellKnownSockets(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ResetWellKnownSockets(
#if NeedFunctionPrototypes
    void
#endif
);

extern char *ClientAuthorized(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    unsigned int /*proto_n*/,
    char* /*auth_proto*/,
    unsigned int /*string_n*/,
    char* /*auth_string*/
#endif
);

extern Bool EstablishNewConnections(
#if NeedFunctionPrototypes
    ClientPtr /*clientUnused*/,
    pointer /*closure*/
#endif
);

extern void CheckConnections(
#if NeedFunctionPrototypes
    void
#endif
);

extern void CloseDownConnection(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int AddEnabledDevice(
#if NeedFunctionPrototypes
    int /*fd*/
#endif
);

extern int RemoveEnabledDevice(
#if NeedFunctionPrototypes
    int /*fd*/
#endif
);

extern int OnlyListenToOneClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int ListenToAllClients(
#if NeedFunctionPrototypes
    void
#endif
);

extern int IgnoreClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int AttendClient(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int MakeClientGrabImpervious(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern int MakeClientGrabPervious(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void Error(
#if NeedFunctionPrototypes
    char* /*str*/
#endif
);

extern CARD32 GetTimeInMillis(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AdjustWaitForDelay(
#if NeedFunctionPrototypes
    pointer /*waitTime*/,
    unsigned long /*newdelay*/
#endif
);

typedef	struct _OsTimerRec *OsTimerPtr;

typedef CARD32 (*OsTimerCallback)(
#if NeedFunctionPrototypes
    OsTimerPtr /* timer */,
    CARD32 /* time */,
    pointer /* arg */
#endif
);

extern void TimerInit(
#if NeedFunctionPrototypes
    void
#endif
);

extern Bool TimerForce(
#if NeedFunctionPrototypes
    OsTimerPtr /* timer */
#endif
);

#define TimerAbsolute (1<<0)
#define TimerForceOld (1<<1)

extern OsTimerPtr TimerSet(
#if NeedFunctionPrototypes
    OsTimerPtr /* timer */,
    int /* flags */,
    CARD32 /* millis */,
    OsTimerCallback /* func */,
    pointer /* arg */
#endif
);

extern void TimerCheck(
#if NeedFunctionPrototypes
    void
#endif
);

extern void TimerFree(
#if NeedFunctionPrototypes
    OsTimerPtr /* pTimer */
#endif
);

extern SIGVAL AutoResetServer(
#if NeedFunctionPrototypes
    int /*sig*/
#endif
);

extern SIGVAL GiveUp(
#if NeedFunctionPrototypes
    int /*sig*/
#endif
);

extern void UseMsg(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ProcessCommandLine(
#if NeedFunctionPrototypes
    int /*argc*/,
    char* /*argv*/[]
#endif
);

extern unsigned long *Xalloc(
#if NeedFunctionPrototypes
    unsigned long /*amount*/
#endif
);

extern unsigned long *XNFalloc(
#if NeedFunctionPrototypes
    unsigned long /*amount*/
#endif
);

extern unsigned long *Xcalloc(
#if NeedFunctionPrototypes
    unsigned long /*amount*/
#endif
);

extern unsigned long *Xrealloc(
#if NeedFunctionPrototypes
    pointer /*ptr*/,
    unsigned long /*amount*/
#endif
);

extern unsigned long *XNFrealloc(
#if NeedFunctionPrototypes
    pointer /*ptr*/,
    unsigned long /*amount*/
#endif
);

extern void Xfree(
#if NeedFunctionPrototypes
    pointer /*ptr*/
#endif
);

extern int OsInitAllocator(
#if NeedFunctionPrototypes
    void
#endif
);

typedef SIGVAL (*OsSigHandlerPtr)(
#if NeedFunctionPrototypes
    int /* sig */
#endif
);

extern OsSigHandlerPtr OsSignal(
#if NeedFunctionPrototypes
    int /* sig */,
    OsSigHandlerPtr /* handler */
#endif
);

extern void AuditF(
#if NeedVarargsPrototypes
    char* /*f*/,
    ...
#endif
);

extern void FatalError(
#if NeedVarargsPrototypes
    char* /*f*/,
    ...
#endif
);

extern void ErrorF(
#if NeedVarargsPrototypes
    char* /*f*/,
    ...
#endif
);

extern int OsLookupColor(
#if NeedFunctionPrototypes
    int	/*screen*/,
    char * /*name*/,
    unsigned /*len*/,
    unsigned short * /*pred*/,
    unsigned short * /*pgreen*/,
    unsigned short * /*pblue*/
#endif
);

extern void OsInit(
#if NeedFunctionPrototypes
    void
#endif
);

extern void OsVendorInit(
#if NeedFunctionPrototypes
    void
#endif
);

extern int OsInitColors(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AddHost(
#if NeedFunctionPrototypes
    ClientPtr	/*client*/,
    int         /*family*/,
    unsigned    /*length*/,
    pointer     /*pAddr*/
#endif
);

extern Bool ForEachHostInFamily (
#if NeedFunctionPrototypes
    int	    /*family*/,
    Bool    (* /*func*/ )(),
    pointer /*closure*/
#endif
);

extern int RemoveHost(
#if NeedFunctionPrototypes
    ClientPtr	/*client*/,
    int         /*family*/,
    unsigned    /*length*/,
    pointer     /*pAddr*/
#endif
);

extern int GetHosts(
#if NeedFunctionPrototypes
    pointer * /*data*/,
    int	    * /*pnHosts*/,
    int	    * /*pLen*/,
    BOOL    * /*pEnabled*/
#endif
);

typedef struct sockaddr * sockaddrPtr;

extern int InvalidHost(
#if NeedFunctionPrototypes
    sockaddrPtr /*saddr*/,
    int		/*len*/
#endif
);

extern int ChangeAccessControl(
#if NeedFunctionPrototypes
    ClientPtr /*client*/,
    int /*fEnabled*/
#endif
);

extern void AddLocalHosts(
#if NeedFunctionPrototypes
    void
#endif
);

extern void ResetHosts(
#if NeedFunctionPrototypes
    char *display
#endif
);

extern void EnableLocalHost(
#if NeedFunctionPrototypes
    void
#endif
);

extern void DisableLocalHost(
#if NeedFunctionPrototypes
    void
#endif
);

extern void AccessUsingXdmcp(
#if NeedFunctionPrototypes
    void
#endif
);

extern void DefineSelf(
#if NeedFunctionPrototypes
    int /*fd*/
#endif
);

extern void AugmentSelf(
#if NeedFunctionPrototypes
    pointer /*from*/,
    int /*len*/
#endif
);

extern void InitAuthorization(
#if NeedFunctionPrototypes
    char * /*filename*/
#endif
);

extern int LoadAuthorization(
#if NeedFunctionPrototypes
    void
#endif
);

extern void RegisterAuthorizations(
#if NeedFunctionPrototypes
    void
#endif
);

extern XID CheckAuthorization(
#if NeedFunctionPrototypes
    unsigned int /*namelength*/,
    char * /*name*/,
    unsigned int /*datalength*/,
    char * /*data*/,
    ClientPtr /*client*/,
    char ** /*reason*/
#endif
);

extern void ResetAuthorization(
#if NeedFunctionPrototypes
    void
#endif
);

extern int AddAuthorization(
#if NeedFunctionPrototypes
    unsigned int /*name_length*/,
    char * /*name*/,
    unsigned int /*data_length*/,
    char * /*data*/
#endif
);

extern void ExpandCommandLine(
#if NeedFunctionPrototypes
    int * /*pargc*/,
    char *** /*pargv*/
#endif
);

extern int ddxProcessArgument(
#if NeedFunctionPrototypes
    int /*argc*/,
    char * /*argv*/ [],
    int /*i*/
#endif
);

#endif /* OS_H */
