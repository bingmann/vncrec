/* $XConsortium: Xtransint.h,v 1.21 94/05/10 11:08:46 mor Exp $ */
/*

Copyright (c) 1993, 1994  X Consortium

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of the X Consortium shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from the X Consortium.

*/

/* Copyright (c) 1993, 1994 NCR Corporation - Dayton, Ohio, USA
 *
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name NCR not be used in advertising
 * or publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCR makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL NCR BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * DEBUG will enable the PRMSG() macros used in the X Transport Interface code.
 * Each use of the PRMSG macro has a level associated with it. DEBUG is defined
 * to be a level. If the invocation level is =< the value of DEBUG, then the
 * message will be printed out to stderr. Recommended levels are:
 *
 *	DEBUG=1	Error messages
 *	DEBUG=2 API Function Tracing
 *	DEBUG=3 All Function Tracing
 *	DEBUG=4 printing of intermediate values
 *	DEBUG=5 really detailed stuff
#define DEBUG 2
 */

#ifndef _XTRANSINT_H_
#define _XTRANSINT_H_

#ifdef WIN32
#define _WILLWINSOCK_
#endif

#include "Xtrans.h"

#ifdef DEBUG
#include <stdio.h>
#endif /* DEBUG */

#include <errno.h>
#ifdef X_NOT_STDC_ENV
extern int  errno;		/* Internal system error number. */
#endif

#ifndef WIN32
#include <sys/socket.h>

/*
 * makedepend screws up on #undef OPEN_MAX, so we define a new symbol
 */

#ifndef TRANS_OPEN_MAX

#ifndef X_NOT_POSIX
#ifdef _POSIX_SOURCE
#include <limits.h>
#else
#define _POSIX_SOURCE
#include <limits.h>
#undef _POSIX_SOURCE
#endif
#endif
#ifndef OPEN_MAX
#ifdef SVR4
#define OPEN_MAX 256
#else
#include <sys/param.h>
#ifndef OPEN_MAX
#ifdef NOFILE
#define OPEN_MAX NOFILE
#else
#define OPEN_MAX NOFILES_MAX
#endif
#endif
#endif
#endif

#if OPEN_MAX > 256
#define TRANS_OPEN_MAX 256
#else
#define TRANS_OPEN_MAX OPEN_MAX
#endif

#endif /* TRANS_OPEN_MAX */


#define ESET(val) errno = val
#define EGET() errno

#else /* WIN32 */

#define ESET(val) WSASetLastError(val)
#define EGET() WSAGetLastError()

#endif /* WIN32 */

#ifndef NULL
#define NULL 0
#endif

#ifdef X11_t
#define X_TCP_PORT	6000
#endif

struct _XtransConnInfo {
    struct _Xtransport     *transptr;
    int		index;
    char	*priv;
    int		flags;
    int		fd;
    char	*port;
    int		family;
    char	*addr;
    int		addrlen;
    char	*peeraddr;
    int		peeraddrlen;
};

#define XTRANS_OPEN_COTS_CLIENT       1
#define XTRANS_OPEN_COTS_SERVER       2
#define XTRANS_OPEN_CLTS_CLIENT       3
#define XTRANS_OPEN_CLTS_SERVER       4


typedef struct _Xtransport {
    char	*TransName;
    int		flags;

#ifdef TRANS_CLIENT

    XtransConnInfo (*OpenCOTSClient)(
#if NeedNestedPrototypes
	struct _Xtransport *,	/* transport */
	char *,			/* protocol */
	char *,			/* host */
	char *			/* port */
#endif
    );

#endif /* TRANS_CLIENT */

#ifdef TRANS_SERVER

    XtransConnInfo (*OpenCOTSServer)(
#if NeedNestedPrototypes
	struct _Xtransport *,	/* transport */
	char *,			/* protocol */
	char *,			/* host */
	char *			/* port */
#endif
    );

#endif /* TRANS_SERVER */

#ifdef TRANS_CLIENT

    XtransConnInfo (*OpenCLTSClient)(
#if NeedNestedPrototypes
	struct _Xtransport *,	/* transport */
	char *,			/* protocol */
	char *,			/* host */
	char *			/* port */
#endif
    );

#endif /* TRANS_CLIENT */

#ifdef TRANS_SERVER

    XtransConnInfo (*OpenCLTSServer)(
#if NeedNestedPrototypes
	struct _Xtransport *,	/* transport */
	char *,			/* protocol */
	char *,			/* host */
	char *			/* port */
#endif
    );

#endif /* TRANS_SERVER */


#ifdef TRANS_REOPEN

    XtransConnInfo (*ReopenCOTSServer)(
#if NeedNestedPrototypes
	struct _Xtransport *,	/* transport */
        int,			/* fd */
        char *			/* port */
#endif
    );

    XtransConnInfo (*ReopenCLTSServer)(
#if NeedNestedPrototypes
	struct _Xtransport *,	/* transport */
        int,			/* fd */
        char *			/* port */
#endif
    );

#endif /* TRANS_REOPEN */


    int	(*SetOption)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	int,			/* option */
	int			/* arg */
#endif
    );

#ifdef TRANS_SERVER

    int	(*CreateListener)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	char *			/* port */
#endif
    );

    int	(*ResetListener)(
#if NeedNestedPrototypes
	XtransConnInfo		/* connection */
#endif
    );

    XtransConnInfo (*Accept)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
        int *			/* status */
#endif
    );

#endif /* TRANS_SERVER */

#ifdef TRANS_CLIENT

    int	(*Connect)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	char *,			/* host */
	char *			/* port */
#endif
    );

#endif /* TRANS_CLIENT */

    int	(*BytesReadable)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	BytesReadable_t *	/* pend */
#endif
    );

    int	(*Read)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	char *,			/* buf */
	int			/* size */
#endif
    );

    int	(*Write)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	char *,			/* buf */
	int			/* size */
#endif
    );

    int	(*Readv)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	struct iovec *,		/* buf */
	int			/* size */
#endif
    );

    int	(*Writev)(
#if NeedNestedPrototypes
	XtransConnInfo,		/* connection */
	struct iovec *,		/* buf */
	int			/* size */
#endif
    );

    int	(*Disconnect)(
#if NeedNestedPrototypes
	XtransConnInfo		/* connection */
#endif
    );

    int	(*Close)(
#if NeedNestedPrototypes
	XtransConnInfo		/* connection */
#endif
    );

    int	(*CloseForCloning)(
#if NeedNestedPrototypes
	XtransConnInfo		/* connection */
#endif
    );

} Xtransport;


typedef struct _Xtransport_table {
    Xtransport	*transport;
    int		transport_id;
} Xtransport_table;


/*
 * Flags for the flags member of Xtransport.
 */

#define TRANS_ALIAS	(1<<0)	/* record is an alias, don't create server */
#define TRANS_LOCAL	(1<<1)	/* local transport */


/*
 * readv() and writev() don't exist or don't work correctly on some
 * systems, so they may be emulated.
 */

#if defined(CRAY) || (defined(SYSV) && defined(SYSV386)) || defined(WIN32) || defined(__sxg__) || defined(SCO)

#define READV(ciptr, iov, iovcnt)	TRANS(ReadV)(ciptr, iov, iovcnt)

static	int TRANS(ReadV)(
#if NeedFunctionPrototypes
    XtransConnInfo,	/* ciptr */
    struct iovec *,	/* iov */
    int			/* iovcnt */
#endif
);

#else

#define READV(ciptr, iov, iovcnt)	readv(ciptr->fd, iov, iovcnt)

#endif /* CRAY || (SYSV && SYSV386) || WIN32 || __sxg__ || SCO */


#if defined(CRAY) || defined(WIN32) || defined(__sxg__) || defined(SCO)

#define WRITEV(ciptr, iov, iovcnt)	TRANS(WriteV)(ciptr, iov, iovcnt)

static int TRANS(WriteV)(
#if NeedFunctionPrototypes
    XtransConnInfo,	/* ciptr */
    struct iovec *,	/* iov */
    int 		/* iovcnt */
#endif
);

#else

#define WRITEV(ciptr, iov, iovcnt)	writev(ciptr->fd, iov, iovcnt)

#endif /* CRAY || WIN32 || __sxg__ || SCO */


static int is_numeric (
#if NeedFunctionPrototypes
    char *		/* str */
#endif
);


/*
 * Some DEBUG stuff
 */

#if defined(DEBUG)
#define PRMSG(lvl,x,a,b,c)	if (lvl <= DEBUG){ \
			int saveerrno=errno; \
			fprintf(stderr, x,a,b,c); fflush(stderr); \
			errno=saveerrno; \
			}
#else
#define PRMSG(lvl,x,a,b,c)
#endif /* DEBUG */

#endif /* _XTRANSINT_H_ */
