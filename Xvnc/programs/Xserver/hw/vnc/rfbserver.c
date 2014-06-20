/*
 * rfbserver.c - deal with server-side of the RFB protocol.
 */

/*
 *  Copyright (C) 1997, 1998 Olivetti & Oracle Research Laboratory
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 */

/* Use ``#define CORBA'' to enable CORBA control interface */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include "windowstr.h"
#include "rfb.h"
#include "input.h"
#include "mipointer.h"
#include "sprite.h"

#ifdef CORBA
#include <vncserverctrl.h>
#endif

char updateBuf[UPDATE_BUF_SIZE];
int ublen;

rfbClientPtr rfbClientHead = NULL;
rfbClientPtr pointerClient = NULL;  /* Mutex for pointer events */

static rfbClientPtr rfbNewClient(int sock);
static void rfbProcessClientProtocolVersion(rfbClientPtr cl);
static void rfbProcessClientNormalMessage(rfbClientPtr cl);
static void rfbProcessClientInitMessage(rfbClientPtr cl);
static Bool rfbSendCopyRegion(rfbClientPtr cl);


/*
 * rfbNewClientConnection is called from sockets.c when a new connection
 * comes in.
 */

void
rfbNewClientConnection(sock)
    int sock;
{
    rfbClientPtr cl;

    cl = rfbNewClient(sock);

#ifdef CORBA
    if (cl != NULL)
	newConnection(cl, (KEYBOARD_DEVICE|POINTER_DEVICE), 1, 1, 1);
#endif
}


/*
 * rfbReverseConnection is called by the CORBA stuff to make an outward
 * connection to a "listening" RFB client.
 */

rfbClientPtr
rfbReverseConnection(host, port)
    char *host;
    int port;
{
    int sock;
    rfbClientPtr cl;

    if ((sock = rfbConnect(host, port)) < 0)
	return (rfbClientPtr)NULL;

    cl = rfbNewClient(sock);

    if (cl) {
	cl->reverseConnection = TRUE;
    }

    return cl;
}


/*
 * rfbNewClient is called when a new connection has been made by whatever
 * means.
 */

static rfbClientPtr
rfbNewClient(sock)
    int sock;
{
    rfbProtocolVersionMsg pv;
    rfbClientPtr cl;
    BoxRec box;

    if (rfbClientHead == NULL) {
	/* no other clients - make sure we don't think any keys are pressed */
	KbdReleaseAllKeys();
    }

    cl = (rfbClientPtr)xalloc(sizeof(rfbClientRec));

    cl->sock = sock;

    cl->state = RFB_PROTOCOL_VERSION;

    cl->reverseConnection = FALSE;
    cl->ready = FALSE;
    cl->readyForSetColourMapEntries = FALSE;
    cl->useCopyRect = FALSE;
    cl->preferredEncoding = rfbEncodingRaw;
    cl->correMaxWidth = 48;
    cl->correMaxHeight = 48;

    REGION_INIT(pScreen,&cl->copyRegion,NullBox,0);
    cl->copyDX = 0;
    cl->copyDY = 0;

    box.x1 = box.y1 = 0;
    box.x2 = rfbScreen.width;
    box.y2 = rfbScreen.height;
    REGION_INIT(pScreen,&cl->modifiedRegion,&box,0);

    cl->format = rfbServerFormat;
    cl->translateFn = rfbTranslateNone;

    cl->next = rfbClientHead;
    rfbClientHead = cl;

    rfbResetStats();

    sprintf(pv,rfbProtocolVersionFormat,rfbProtocolMajorVersion,
	    rfbProtocolMinorVersion);

    if (WriteExact(sock, pv, sz_rfbProtocolVersionMsg) < 0) {
	perror("rfbNewClient: write");
	rfbCloseSock(sock);
	return NULL;
    }

    return cl;
}


/*
 * rfbClientConnectionGone is called from sockets.c just after a connection
 * has gone away.
 */

void
rfbClientConnectionGone(sock)
    int sock;
{
    rfbClientPtr cl, prev;

    for (prev = NULL, cl = rfbClientHead; cl; prev = cl, cl = cl->next) {
	if (sock == cl->sock)
	    break;
    }

    if (!cl) {
	fprintf(stderr,"rfbClientConnectionGone: unknown socket %d\n",sock);
	return;
    }

    if (pointerClient == cl)
	pointerClient = NULL;

#ifdef CORBA
    destroyConnection(cl);
#endif

    if (prev)
	prev->next = cl->next;
    else
	rfbClientHead = cl->next;

    REGION_UNINIT(pScreen,&cl->copyRegion);
    REGION_UNINIT(pScreen,&cl->modifiedRegion);

    rfbPrintStats();

    xfree(cl);
}


/*
 * rfbProcessClientMessage is called when there is data to read from a client.
 */

void
rfbProcessClientMessage(sock)
    int sock;
{
    rfbClientPtr cl;

    for (cl = rfbClientHead; cl; cl = cl->next) {
	if (sock == cl->sock)
	    break;
    }

    if (!cl) {
	fprintf(stderr,"rfbProcessClientMessage: unknown socket %d\n",sock);
	rfbCloseSock(sock);
	return;
    }

#ifdef CORBA
    if (isClosePending(cl)) {
	rfbCloseSock(sock);
	return;
    }
#endif

    switch (cl->state) {
    case RFB_PROTOCOL_VERSION:
	rfbProcessClientProtocolVersion(cl);
	return;
    case RFB_AUTHENTICATION:
	rfbAuthProcessClientMessage(cl);
	return;
    case RFB_INITIALISATION:
	rfbProcessClientInitMessage(cl);
	return;
    default:
	rfbProcessClientNormalMessage(cl);
	return;
    }
}


/*
 * rfbProcessClientProtocolVersion is called when the client sends its
 * protocol version.
 */

static void
rfbProcessClientProtocolVersion(cl)
    rfbClientPtr cl;
{
    rfbProtocolVersionMsg pv;
    int n, major, minor;
    char failureReason[256];

    if ((n = ReadExact(cl->sock, pv, sz_rfbProtocolVersionMsg)) <= 0) {
	if (n == 0)
	    fprintf(stderr,"rfbProcessClientProtocolVersion: client gone\n");
	else
	    perror("rfbProcessClientProtocolVersion: read");
	rfbCloseSock(cl->sock);
	return;
    }

    pv[sz_rfbProtocolVersionMsg] = 0;
    if (sscanf(pv,rfbProtocolVersionFormat,&major,&minor) != 2) {
	fprintf(stderr,
		"rfbProcessClientProtocolVersion: not a valid RFB client\n");
	rfbCloseSock(cl->sock);
	return;
    }
    fprintf(stderr,
	    "rfbProcessClientProtocolVersion: client protocol version %d.%d "
	    "(server %d.%d)\n",
	    major, minor, rfbProtocolMajorVersion, rfbProtocolMinorVersion);

    if (major != rfbProtocolMajorVersion) {
	/* Major version mismatch - send a ConnFailed message */

	fprintf(stderr,
		"rfbProcessClientProtocolVersion: major version mismatch\n");
	sprintf(failureReason,
		"RFB protocol version mismatch - server %d.%d, client %d.%d",
		rfbProtocolMajorVersion,rfbProtocolMinorVersion,major,minor);
	rfbClientConnFailed(cl, failureReason);
	return;
    }

    if (minor != rfbProtocolMinorVersion) {
	/* Minor version mismatch - warn but try to continue */
	fprintf(stderr,
		"rfbProcessClientProtocolVersion: ignoring minor version "
		"mismatch\n");
    }

    rfbAuthNewClient(cl);
}


/*
 * rfbClientConnFailed is called when a client connection has failed either
 * because it talks the wrong protocol or it has failed authentication.
 */

void
rfbClientConnFailed(cl, reason)
    rfbClientPtr cl;
    char *reason;
{
    char *buf;
    int len = strlen(reason);

    buf = (char *)xalloc(8 + len);
    ((CARD32 *)buf)[0] = Swap32IfLE(rfbConnFailed);
    ((CARD32 *)buf)[1] = Swap32IfLE(len);
    memcpy(buf + 8, reason, len);

    if (WriteExact(cl->sock, buf, 8 + len) < 0)
	perror("rfbClientConnFailed: write");
    xfree(buf);
    rfbCloseSock(cl->sock);
}


/*
 * rfbProcessClientInitMessage is called when the client sends its
 * initialisation message.
 */

static void
rfbProcessClientInitMessage(cl)
    rfbClientPtr cl;
{
    rfbClientInitMsg ci;
    char buf[256];
    rfbServerInitMsg *si = (rfbServerInitMsg *)buf;
    char host[256];
    struct passwd *user;
    int len, n;
    rfbClientPtr otherCl, nextCl;

    if ((n = ReadExact(cl->sock, (char *)&ci,sz_rfbClientInitMsg)) <= 0) {
	if (n == 0)
	    fprintf(stderr,"rfbProcessClientInitMessage: client gone\n");
	else
	    perror("rfbProcessClientInitMessage: read");
	rfbCloseSock(cl->sock);
	return;
    }

    si->framebufferWidth = Swap16IfLE(rfbScreen.width);
    si->framebufferHeight = Swap16IfLE(rfbScreen.height);
    si->format = rfbServerFormat;
    si->format.redMax = Swap16IfLE(si->format.redMax);
    si->format.greenMax = Swap16IfLE(si->format.greenMax);
    si->format.blueMax = Swap16IfLE(si->format.blueMax);

    user = getpwuid(getuid());
    gethostname(host, 255);

    if (strlen(desktopName) > 128)	/* sanity check on desktop name len */
	desktopName[128] = 0;

    sprintf(buf + sz_rfbServerInitMsg, "%s's %s desktop (%s:%s)",
	    user->pw_name, desktopName, host, display);
    len = strlen(buf + sz_rfbServerInitMsg);
    si->nameLength = Swap32IfLE(len);

    if (WriteExact(cl->sock, buf, sz_rfbServerInitMsg + len) < 0) {
	perror("rfbProcessClientInitMessage: write");
	rfbCloseSock(cl->sock);
	return;
    }

    cl->state = RFB_NORMAL;

    if (!ci.shared && !cl->reverseConnection) {
	for (otherCl = rfbClientHead; otherCl; otherCl = nextCl) {
	    nextCl = otherCl->next;
	    if ((otherCl != cl) && (otherCl->state == RFB_NORMAL)) {
		rfbCloseSock(otherCl->sock);
	    }
	}
    }
}


/*
 * rfbProcessClientNormalMessage is called when the client has sent a normal
 * protocol message.
 */

static void
rfbProcessClientNormalMessage(cl)
    rfbClientPtr cl;
{
    int n;
    rfbClientToServerMsg msg;
    char *str;

    if ((n = ReadExact(cl->sock, (char *)&msg, 1)) <= 0) {
	if (n == 0)
	    fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	else
	    perror("rfbProcessClientNormalMessage: read");
	rfbCloseSock(cl->sock);
	return;
    }

    switch (msg.type) {

    case rfbSetPixelFormat:

	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbSetPixelFormatMsg - 1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}

	cl->format.bitsPerPixel = msg.spf.format.bitsPerPixel;
	cl->format.depth = msg.spf.format.depth;
	cl->format.bigEndian = (msg.spf.format.bigEndian ? 1 : 0);
	cl->format.trueColour = (msg.spf.format.trueColour ? 1 : 0);
	cl->format.redMax = Swap16IfLE(msg.spf.format.redMax);
	cl->format.greenMax = Swap16IfLE(msg.spf.format.greenMax);
	cl->format.blueMax = Swap16IfLE(msg.spf.format.blueMax);
	cl->format.redShift = msg.spf.format.redShift;
	cl->format.greenShift = msg.spf.format.greenShift;
	cl->format.blueShift = msg.spf.format.blueShift;

	cl->readyForSetColourMapEntries = TRUE;

	rfbSetTranslateFunction(cl);
	return;


    case rfbFixColourMapEntries:
	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbFixColourMapEntriesMsg - 1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}
	fprintf(stderr,"rfbProcessClientNormalMessage: %s",
		"FixColourMapEntries unsupported\n");
	rfbCloseSock(cl->sock);
	return;


    case rfbSetEncodings:
    {
	int i;
	CARD32 enc;

	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbSetEncodingsMsg - 1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}

	msg.se.nEncodings = Swap16IfLE(msg.se.nEncodings);

	cl->preferredEncoding = -1;
	cl->useCopyRect = FALSE;

	for (i = 0; i < msg.se.nEncodings; i++) {
	    if ((n = ReadExact(cl->sock, (char *)&enc, 4)) <= 0) {
		if (n == 0)
		    fprintf(stderr,
			    "rfbProcessClientNormalMessage: client gone\n");
		else
		    perror("rfbProcessClientNormalMessage: read");
		rfbCloseSock(cl->sock);
		return;
	    }
	    enc = Swap32IfLE(enc);

	    switch (enc) {

	    case rfbEncodingCopyRect:
		cl->useCopyRect = TRUE;
		break;
	    case rfbEncodingRaw:
		if (cl->preferredEncoding == -1) {
		    cl->preferredEncoding = enc;
		    fprintf(stderr,"using client's preferred encoding: raw\n");
		}
		break;
	    case rfbEncodingRRE:
		if (cl->preferredEncoding == -1) {
		    cl->preferredEncoding = enc;
		    fprintf(stderr,"using client's preferred encoding: rre\n");
		}
		break;
	    case rfbEncodingCoRRE:
		if (cl->preferredEncoding == -1) {
		    cl->preferredEncoding = enc;
		    fprintf(stderr,"using client's preferred encoding: CoRRE\n");
		}
		break;
	    case rfbEncodingHextile:
		if (cl->preferredEncoding == -1) {
		    cl->preferredEncoding = enc;
		    fprintf(stderr,
			    "using client's preferred encoding: hextile\n");
		}
		break;
	    default:
		fprintf(stderr,"%s: ignoring unknown encoding type %d\n",
			"rfbProcessClientNormalMessage", (int)enc);
	    }
	}

	if (cl->preferredEncoding == -1) {
	    cl->preferredEncoding = rfbEncodingRaw;
	}

	return;
    }


    case rfbFramebufferUpdateRequest:

#ifdef CORBA
	addCapability(cl, DISPLAY_DEVICE);
#endif

	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbFramebufferUpdateRequestMsg-1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}

	msg.fur.x = Swap16IfLE(msg.fur.x);
	msg.fur.y = Swap16IfLE(msg.fur.y);
	msg.fur.w = Swap16IfLE(msg.fur.w);
	msg.fur.h = Swap16IfLE(msg.fur.h);

	cl->ready = TRUE;

	if (!cl->readyForSetColourMapEntries) {
	    /* client hasn't sent a SetPixelFormat so is using server's */
	    cl->readyForSetColourMapEntries = TRUE;
	    if (!cl->format.trueColour) {
		if (!rfbSetClientColourMap(cl, 0, 0))
		    return;
	    }
	}

	if (!msg.fur.incremental) {
	    RegionRec tmpRegion;
	    BoxRec box;

	    box.x1 = msg.fur.x;
	    box.y1 = msg.fur.y;
	    box.x2 = msg.fur.x + msg.fur.w;
	    box.y2 = msg.fur.y + msg.fur.h;
	    REGION_INIT(pScreen,&tmpRegion,&box,0);
	    REGION_UNION(pScreen,&cl->modifiedRegion,&cl->modifiedRegion,
			 &tmpRegion);
	    REGION_SUBTRACT(pScreen,&cl->copyRegion,&cl->copyRegion,
			    &tmpRegion);
	    REGION_UNINIT(pScreen,&tmpRegion);
	}

	if (FB_UPDATE_PENDING(cl)) {
	    rfbSendFramebufferUpdate(cl);
	}
	return;


    case rfbKeyEvent:

	rfbKeyEventsRcvd++;

	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbKeyEventMsg - 1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}

#ifdef CORBA
	addCapability(cl, KEYBOARD_DEVICE);

	if (!isKeyboardEnabled(cl))
	    return;
#endif
	KbdAddEvent(msg.ke.down, (KeySym)Swap32IfLE(msg.ke.key));
	return;


    case rfbPointerEvent:

	rfbPointerEventsRcvd++;

	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbPointerEventMsg - 1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}

#ifdef CORBA
	addCapability(cl, POINTER_DEVICE);

	if (!isPointerEnabled(cl))
	    return;
#endif

	if (pointerClient && (pointerClient != cl))
	    return;

	if (msg.pe.buttonMask == 0)
	    pointerClient = NULL;
	else
	    pointerClient = cl;

	PtrAddEvent(msg.pe.buttonMask,
		    Swap16IfLE(msg.pe.x), Swap16IfLE(msg.pe.y));
	return;


    case rfbClientCutText:

	if ((n = ReadExact(cl->sock, ((char *)&msg) + 1,
			   sz_rfbClientCutTextMsg - 1)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    rfbCloseSock(cl->sock);
	    return;
	}

	msg.cct.length = Swap32IfLE(msg.cct.length);

	str = (char *)xalloc(msg.cct.length);

	if ((n = ReadExact(cl->sock, str, msg.cct.length)) <= 0) {
	    if (n == 0)
		fprintf(stderr,"rfbProcessClientNormalMessage: client gone\n");
	    else
		perror("rfbProcessClientNormalMessage: read");
	    xfree(str);
	    rfbCloseSock(cl->sock);
	    return;
	}

	rfbSetCutText(str, msg.cct.length);

	xfree(str);
	return;


    default:

	fprintf(stderr,
		"rfbProcessClientNormalMessage: unknown message type %d\n",
		msg.type);
	fprintf(stderr,
		"rfbProcessClientNormalMessage: ... closing connection\n");
	rfbCloseSock(cl->sock);
	return;
    }
}



/*
 * rfbSendFramebufferUpdate - send the currently pending framebuffer update to
 * the RFB client.
 */

Bool
rfbSendFramebufferUpdate(cl)
    rfbClientPtr cl;
{
    ScreenPtr pScreen = screenInfo.screens[0];
    int i;
    int nModifiedRegionRects;
    rfbFramebufferUpdateMsg *fu = (rfbFramebufferUpdateMsg *)updateBuf;

    if (!rfbScreen.cursorIsDrawn) {
	rfbSpriteRestoreCursor(pScreen);
    }

    rfbFramebufferUpdateMessagesSent++;

    cl->ready = FALSE;

    if (cl->preferredEncoding == rfbEncodingCoRRE) {
	nModifiedRegionRects = 0;

	for (i = 0; i < REGION_NUM_RECTS(&cl->modifiedRegion); i++) {
	    int x = REGION_RECTS(&cl->modifiedRegion)[i].x1;
	    int y = REGION_RECTS(&cl->modifiedRegion)[i].y1;
	    int w = REGION_RECTS(&cl->modifiedRegion)[i].x2 - x;
	    int h = REGION_RECTS(&cl->modifiedRegion)[i].y2 - y;
	    nModifiedRegionRects += (((w-1) / cl->correMaxWidth + 1)
				     * ((h-1) / cl->correMaxHeight + 1));
	}
    } else {
	nModifiedRegionRects = REGION_NUM_RECTS(&cl->modifiedRegion);
    }

    fu->type = rfbFramebufferUpdate;
    fu->nRects = Swap16IfLE(REGION_NUM_RECTS(&cl->copyRegion)
			    + nModifiedRegionRects);
    ublen = sz_rfbFramebufferUpdateMsg;

    if (REGION_NOTEMPTY(pScreen,&cl->copyRegion)) {
	if (!rfbSendCopyRegion(cl))
	    return FALSE;
    }

    for (i = 0; i < REGION_NUM_RECTS(&cl->modifiedRegion); i++) {
	int x = REGION_RECTS(&cl->modifiedRegion)[i].x1;
	int y = REGION_RECTS(&cl->modifiedRegion)[i].y1;
	int w = REGION_RECTS(&cl->modifiedRegion)[i].x2 - x;
	int h = REGION_RECTS(&cl->modifiedRegion)[i].y2 - y;

	rfbRawBytesEquivalent += (sz_rfbFramebufferUpdateRectHeader
				  + w * (cl->format.bitsPerPixel / 8) * h);

	switch (cl->preferredEncoding) {
	case rfbEncodingRaw:
	    if (!rfbSendRectEncodingRaw(cl, x, y, w, h))
		return FALSE;
	    break;
	case rfbEncodingRRE:
	    if (!rfbSendRectEncodingRRE(cl, x, y, w, h))
		return FALSE;
	    break;
	case rfbEncodingCoRRE:
	    if (!rfbSendRectEncodingCoRRE(cl, x, y, w, h))
		return FALSE;
	    break;
	case rfbEncodingHextile:
	    if (!rfbSendRectEncodingHextile(cl, x, y, w, h))
		return FALSE;
	    break;
	}
    }

    if (!rfbSendUpdateBuf(cl))
	return FALSE;

    REGION_EMPTY(pScreen,&cl->modifiedRegion);
    return TRUE;
}



/*
 * Send the copy region as a string of CopyRect encoded rectangles.
 * The only slightly tricky thing is that we should send the messages in
 * the correct order so that an earlier CopyRect will not corrupt the source
 * of a later one.
 */

static Bool
rfbSendCopyRegion(cl)
    rfbClientPtr cl;
{
    int nrects, nrectsInBand, x_inc, y_inc, thisRect, firstInNextBand;
    int x, y, w, h;
    rfbFramebufferUpdateRectHeader rect;
    rfbCopyRect cr;

    nrects = REGION_NUM_RECTS(&cl->copyRegion);

    if (cl->copyDX <= 0) {
	x_inc = 1;
    } else {
	x_inc = -1;
    }

    if (cl->copyDY <= 0) {
	thisRect = 0;
	y_inc = 1;
    } else {
	thisRect = nrects - 1;
	y_inc = -1;
    }

    while (nrects > 0) {

	firstInNextBand = thisRect;
	nrectsInBand = 0;

	while ((REGION_RECTS(&cl->copyRegion)[firstInNextBand].y1
		== REGION_RECTS(&cl->copyRegion)[thisRect].y1) &&
	       (nrects > 0))
	{
	    firstInNextBand += y_inc;
	    nrects--;
	    nrectsInBand++;
	}

	if (x_inc != y_inc) {
	    thisRect = firstInNextBand - y_inc;
	}

	while (nrectsInBand > 0) {
	    if ((ublen + sz_rfbFramebufferUpdateRectHeader
		 + sz_rfbCopyRect) > UPDATE_BUF_SIZE)
	    {
		if (!rfbSendUpdateBuf(cl))
		    return FALSE;
	    }

	    x = REGION_RECTS(&cl->copyRegion)[thisRect].x1;
	    y = REGION_RECTS(&cl->copyRegion)[thisRect].y1;
	    w = REGION_RECTS(&cl->copyRegion)[thisRect].x2 - x;
	    h = REGION_RECTS(&cl->copyRegion)[thisRect].y2 - y;

	    rect.r.x = Swap16IfLE(x);
	    rect.r.y = Swap16IfLE(y);
	    rect.r.w = Swap16IfLE(w);
	    rect.r.h = Swap16IfLE(h);
	    rect.encoding = Swap32IfLE(rfbEncodingCopyRect);

	    memcpy(&updateBuf[ublen], (char *)&rect,
		   sz_rfbFramebufferUpdateRectHeader);
	    ublen += sz_rfbFramebufferUpdateRectHeader;

	    cr.srcX = Swap16IfLE(x - cl->copyDX);
	    cr.srcY = Swap16IfLE(y - cl->copyDY);

	    memcpy(&updateBuf[ublen], (char *)&cr, sz_rfbCopyRect);
	    ublen += sz_rfbCopyRect;

	    rfbRectanglesSent[rfbEncodingCopyRect]++;
	    rfbBytesSent[rfbEncodingCopyRect]
		+= sz_rfbFramebufferUpdateRectHeader + sz_rfbCopyRect;

	    thisRect += x_inc;
	    nrectsInBand--;
	}

	thisRect = firstInNextBand;
    }

    REGION_EMPTY(pScreen,&cl->copyRegion);
    cl->copyDX = 0;
    cl->copyDY = 0;

    return TRUE;
}


/*
 * Send a given rectangle in raw encoding (rfbEncodingRaw).
 */

Bool
rfbSendRectEncodingRaw(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;
    int nlines;
    int bytesPerLine = w * (cl->format.bitsPerPixel / 8);
    char *fbptr = (rfbScreen.pfbMemory + (rfbScreen.paddedWidthInBytes * y)
		   + (x * (rfbScreen.bitsPerPixel / 8)));

    if (ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingRaw);

    memcpy(&updateBuf[ublen], (char *)&rect,sz_rfbFramebufferUpdateRectHeader);
    ublen += sz_rfbFramebufferUpdateRectHeader;

    rfbRectanglesSent[rfbEncodingRaw]++;
    rfbBytesSent[rfbEncodingRaw]
	+= sz_rfbFramebufferUpdateRectHeader + bytesPerLine * h;

    nlines = (UPDATE_BUF_SIZE - ublen) / bytesPerLine;

    while (TRUE) {
	if (nlines > h)
	    nlines = h;

	(*cl->translateFn)(cl, fbptr, &updateBuf[ublen],
			   rfbScreen.paddedWidthInBytes, bytesPerLine, nlines);

	ublen += nlines * bytesPerLine;
	h -= nlines;

	if (h == 0)	/* rect fitted in buffer, do next one */
	    return TRUE;

	/* buffer full - flush partial rect and do another nlines */

	if (!rfbSendUpdateBuf(cl))
	    return FALSE;

	fbptr += (rfbScreen.paddedWidthInBytes * nlines);

	nlines = (UPDATE_BUF_SIZE - ublen) / bytesPerLine;
	if (nlines == 0) {
	    fprintf(stderr,
		    "%s send buffer too small for %d bytes per line\n",
		    "rfbSendRectEncodingRaw:",bytesPerLine);
	    rfbCloseSock(cl->sock);
	    return FALSE;
	}
    }
}



/*
 * Send the contents of updateBuf.  Returns 1 if successful, -1 if
 * not (errno should be set).
 */

Bool
rfbSendUpdateBuf(cl)
    rfbClientPtr cl;
{
    /*
    int i;
    for (i = 0; i < ublen; i++) {
	fprintf(stderr,"%02x ",((unsigned char *)updateBuf)[i]);
    }
    fprintf(stderr,"\n");
    */

    if (WriteExact(cl->sock, updateBuf, ublen) < 0) {
	perror("rfbSendUpdateBuf: write");
	rfbCloseSock(cl->sock);
	return FALSE;
    }

    ublen = 0;
    return TRUE;
}



/*
 * rfbSendSetColourMapEntries sends a SetColourMapEntries message to the
 * client, using values from the currently installed colormap.
 */

Bool
rfbSendSetColourMapEntries(cl, firstColour, nColours)
    rfbClientPtr cl;
    int firstColour;
    int nColours;
{
    char buf[sz_rfbSetColourMapEntriesMsg + 256 * 3 * 2];
    rfbSetColourMapEntriesMsg *scme = (rfbSetColourMapEntriesMsg *)buf;
    CARD16 *rgb = (CARD16 *)(&buf[sz_rfbSetColourMapEntriesMsg]);
    EntryPtr pent;
    int i, len;

    scme->type = rfbSetColourMapEntries;

    scme->firstColour = Swap16IfLE(firstColour);
    scme->nColours = Swap16IfLE(nColours);

    len = sz_rfbSetColourMapEntriesMsg;

    pent = (EntryPtr)&rfbInstalledColormap->red[firstColour];
    for (i = 0; i < nColours; i++) {
	if (pent->fShared) {
	    rgb[i*3] = Swap16IfLE(pent->co.shco.red->color);
	    rgb[i*3+1] = Swap16IfLE(pent->co.shco.green->color);
	    rgb[i*3+2] = Swap16IfLE(pent->co.shco.blue->color);
	} else {
	    rgb[i*3] = Swap16IfLE(pent->co.local.red);
	    rgb[i*3+1] = Swap16IfLE(pent->co.local.green);
	    rgb[i*3+2] = Swap16IfLE(pent->co.local.blue);
	}
	pent++;
    }

    len += nColours * 3 * 2;

    if (WriteExact(cl->sock, buf, len) < 0) {
	perror("rfbSendSetColourMapEntries: write");
	rfbCloseSock(cl->sock);
	return FALSE;
    }
    return TRUE;
}


/*
 * rfbSendBell sends a Bell message to all the clients.
 */

void
rfbSendBell()
{
    rfbClientPtr cl;
    rfbBellMsg b;

    for (cl = rfbClientHead; cl; cl = cl->next) {
	b.type = rfbBell;
	if (WriteExact(cl->sock, (char *)&b, sz_rfbBellMsg) < 0) {
	    perror("rfbSendBell: write");
	    rfbCloseSock(cl->sock);
	}
    }
}


/*
 * rfbSendServerCutText sends a ServerCutText message to all the clients.
 */

void
rfbSendServerCutText(char *str, int len)
{
    rfbClientPtr cl;
    rfbServerCutTextMsg sct;

    for (cl = rfbClientHead; cl; cl = cl->next) {
	sct.type = rfbServerCutText;
	sct.length = Swap32IfLE(len);
	if (WriteExact(cl->sock, (char *)&sct,
		       sz_rfbServerCutTextMsg) < 0) {
	    perror("rfbSendServerCutText: write");
	    rfbCloseSock(cl->sock);
	    continue;
	}
	if (WriteExact(cl->sock, str, len) < 0) {
	    perror("rfbSendServerCutText: write");
	    rfbCloseSock(cl->sock);
	}
    }
}




/*****************************************************************************
 *
 * UDP can be used for keyboard and pointer events when the underlying
 * network is highly reliable.  This is really here to support ORL's
 * videotile, whose TCP implementation doesn't like sending lots of small
 * packets (such as 100s of pen readings per second!).
 */

void
rfbNewUDPConnection(sock)
    int sock;
{
    if (write(sock, &ptrAcceleration, 1) < 0) {
	perror("rfbNewUDPConnection: write");
    }
}

/*
 * Because UDP is a message based service, we can't read the first byte and
 * then the rest of the packet separately like we do with TCP.  We will always
 * get a whole packet delivered in one go, so we ask read() for the maximum
 * number of bytes we can possibly get.
 */

void
rfbProcessUDPInput(sock)
    int sock;
{
    int n;
    rfbClientToServerMsg msg;

    if ((n = read(sock, (char *)&msg, sizeof(msg))) <= 0) {
	if (n < 0) {
	    perror("rfbProcessUDPInput: read");
	}
	rfbDisconnectUDPSock();
	return;
    }

    switch (msg.type) {

    case rfbKeyEvent:
	if (n != sz_rfbKeyEventMsg) {
	    fprintf(stderr,"rfbProcessUDPInput: key event incorrect length\n");
	    rfbDisconnectUDPSock();
	    return;
	}
	KbdAddEvent(msg.ke.down, (KeySym)Swap32IfLE(msg.ke.key));
	break;

    case rfbPointerEvent:
	if (n != sz_rfbPointerEventMsg) {
	    fprintf(stderr,"rfbProcessUDPInput: ptr event incorrect length\n");
	    rfbDisconnectUDPSock();
	    return;
	}
	PtrAddEvent(msg.pe.buttonMask,
		    Swap16IfLE(msg.pe.x), Swap16IfLE(msg.pe.y));
	break;

    default:
	fprintf(stderr,"rfbProcessUDPInput: unknown message type %d\n",
		msg.type);
	rfbDisconnectUDPSock();
    }
}
