/*
 * rfb.h - header file for RFB DDX implementation.
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

#include "scrnintstr.h"
#include "colormapst.h"
#include "gcstruct.h"
#include "osdep.h"
#include <rfbproto.h>
#include <vncauth.h>

#if NeedFunctionPrototypes
#define _(x) x
#else
#define _(x) ()
#endif


extern char *display;


/*
 * Per-screen (framebuffer) structure.  There is only one of these, since we
 * don't allow the X server to have multiple screens.
 */

typedef struct
{
    int width;
    int paddedWidthInBytes;
    int height;
    int depth;
    int bitsPerPixel;
    int sizeInBytes;
    char *pfbMemory;
    Pixel blackPixel;
    Pixel whitePixel;

    /* The following two members are used to minimise the amount of unnecessary
       drawing caused by cursor movement.  Whenever any drawing affects the
       part of the screen where the cursor is, the cursor is removed first and
       then the drawing is done (this is what the sprite routines test for).
       Afterwards, however, we do not replace the cursor, even when the cursor
       is logically being moved across the screen.  We only draw the cursor
       again just as we are about to send the client a framebuffer update.

       We need to be careful when removing and drawing the cursor because of
       their relationship with the normal drawing routines.  The drawing
       routines can invoke the cursor routines, but also the cursor routines
       themselves end up invoking drawing routines.

       Removing the cursor (rfbSpriteRemoveCursor) is eventually achieved by
       doing a CopyArea from a pixmap to the screen, where the pixmap contains
       the saved contents of the screen under the cursor.  Before doing this,
       however, we set cursorIsDrawn to FALSE.  Then, when CopyArea is called,
       it sees that cursorIsDrawn is FALSE and so doesn't feel the need to
       (recursively!) remove the cursor before doing it.

       Putting up the cursor (rfbSpriteRestoreCursor) involves a call to
       PushPixels.  While this is happening, cursorIsDrawn must be FALSE so
       that PushPixels doesn't think it has to remove the cursor first.
       Obviously cursorIsDrawn is set to TRUE afterwards.

       Another problem we face is that drawing routines sometimes cause a
       framebuffer update to be sent to the RFB client.  When the RFB client is
       already waiting for a framebuffer update and some drawing to the
       framebuffer then happens, the drawing routine sees that the client is
       ready, so it calls rfbSendFramebufferUpdate.  If the cursor is not drawn
       at this stage, it must be put up, and so rfbSpriteRestoreCursor is
       called.  However, if the original drawing routine was actually called
       from within rfbSpriteRestoreCursor or rfbSpriteRemoveCursor we don't
       want this to happen.  So both the cursor routines set
       dontSendFramebufferUpdate to TRUE, and all the drawing routines check
       this before calling rfbSendFramebufferUpdate. */

    Bool cursorIsDrawn;		    /* TRUE if the cursor is currently drawn */
    Bool dontSendFramebufferUpdate; /* TRUE while removing or drawing the
				       cursor */

    /* wrapped screen functions */

    CloseScreenProcPtr			CloseScreen;
    CreateGCProcPtr			CreateGC;
    PaintWindowBackgroundProcPtr	PaintWindowBackground;
    PaintWindowBorderProcPtr		PaintWindowBorder;
    CopyWindowProcPtr			CopyWindow;
    ClearToBackgroundProcPtr		ClearToBackground;
    RestoreAreasProcPtr			RestoreAreas;

} rfbScreenInfo, *rfbScreenInfoPtr;


/*
 * rfbTranslateFnType is the type of translation functions.
 */

struct rfbClientRec;
typedef void (*rfbTranslateFnType)(struct rfbClientRec *cl, char *iptr,
				   char *optr, int iptradd, int optradd,
				   int nlines);


/*
 * Per-client structure.
 */

typedef struct rfbClientRec {

    int sock;

				/* Possible client states: */
    enum {
	RFB_PROTOCOL_VERSION,	/* establishing protocol version */
	RFB_AUTHENTICATION,	/* authenticating */
	RFB_INITIALISATION,	/* sending initialisation messages */
	RFB_NORMAL		/* normal protocol messages */
    } state;

    Bool reverseConnection;
    Bool ready;
    Bool readyForSetColourMapEntries;

    Bool useCopyRect;
    int preferredEncoding;
    int correMaxWidth, correMaxHeight;

    /* The following member is only used during VNC authentication */

    CARD8 authChallenge[CHALLENGESIZE];

    /* The following members represent the update needed to get the client's
       framebuffer from its present state to the current state of our
       framebuffer.

       If the client does not accept CopyRect encoding then the update is
       simply specified as the region of the screen which has been modified.
       If the client does accept CopyRect encoding, then the update consists of
       two parts.  First we have a (single) copy from one region of the screen
       to another, and second (as before) the region of the screen which has
       been modified in some other way.  Note that although the copy is of a
       single region, this region may have many rectangles.  Note also that the
       rectangles updated using CopyRect are always done before sending the
       "other" modified region, since the modified region may include parts of
       the screen which are in the source of the copy. */

    RegionRec copyRegion;	/* the destination region of the copy */
    int copyDX, copyDY;		/* the translation by which the copy happens */

    RegionRec modifiedRegion;	/* the region of the screen modified in any
				   other way */


    /* This member points to the translation function which is used to copy
       and translate a rectangle from the framebuffer to an output buffer. */

    rfbTranslateFnType translateFn;
    rfbPixelFormat format;

    struct rfbClientRec *next;

} rfbClientRec, *rfbClientPtr;


/*
 * This macro is used to test whether there is a framebuffer update needing to
 * be sent to the client.
 */

#define FB_UPDATE_PENDING(cl)				\
    (!rfbScreen.cursorIsDrawn ||			\
     REGION_NOTEMPTY((pScreen),&(cl)->copyRegion) ||	\
     REGION_NOTEMPTY((pScreen),&(cl)->modifiedRegion))


/*
 * An rfbGCRec is where we store the pointers to the original GC funcs and ops
 * which we wrap (NULL means not wrapped).
 */

typedef struct {
    GCFuncs *wrapFuncs;
    GCOps *wrapOps;
} rfbGCRec, *rfbGCPtr;



/*
 * Macros for endian swapping.
 */

#define Swap16(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))

#define Swap32(l) (((l) >> 24) | \
		   (((l) & 0x00ff0000) >> 8)  | \
		   (((l) & 0x0000ff00) << 8)  | \
		   ((l) << 24))


/* init.c */

static const int rfbEndianTest = 1;

#define Swap16IfLE(s) (*(const char *)&rfbEndianTest ? Swap16(s) : (s))

#define Swap32IfLE(l) (*(const char *)&rfbEndianTest ? Swap32(l) : (l))

extern char *desktopName;

extern rfbScreenInfo rfbScreen;
extern int rfbGCIndex;

extern int rfbBitsPerPixel(int depth);


/* sockets.c */

extern int rfbMaxClientWait;

extern int udpPort;
extern int udpSock;
extern Bool udpSockConnected;

extern int rfbPort;
extern int rfbListenSock;

extern void rfbInitSockets();
extern void rfbDisconnectUDPSock();
extern void rfbCloseSock();
extern void rfbCheckFds();
extern void rfbWaitForClient(int sock);
extern int rfbConnect(char *host, int port);

extern int ReadExact(int sock, char *buf, int len);
extern int WriteExact(int sock, char *buf, int len);
extern int ListenOnTCPPort(int port);
extern int ListenOnUDPPort(int port);
extern int ConnectToTcpAddr(char *host, int port);


/* cmap.c */

extern ColormapPtr rfbInstalledColormap;

extern int rfbListInstalledColormaps(ScreenPtr pScreen, Colormap *pmaps);
extern void rfbInstallColormap(ColormapPtr pmap);
extern void rfbUninstallColormap(ColormapPtr pmap);
extern void rfbStoreColors(ColormapPtr pmap, int ndef, xColorItem *pdefs);


/* draw.c */

extern Bool rfbCloseScreen(int,ScreenPtr);
extern Bool rfbCreateGC(GCPtr);
extern void rfbPaintWindowBackground(WindowPtr, RegionPtr, int what);
extern void rfbPaintWindowBorder(WindowPtr, RegionPtr, int what);
extern void rfbCopyWindow(WindowPtr, DDXPointRec, RegionPtr);
extern void rfbClearToBackground(WindowPtr, int x, int y, int w,
				 int h, Bool generateExposures);
extern RegionPtr rfbRestoreAreas(WindowPtr, RegionPtr);


/* cutpaste.c */

extern void rfbSetCutText(char *str, int len);


/* kbdptr.c */

extern unsigned char ptrAcceleration;

extern void PtrDeviceInit();
extern void PtrDeviceOn();
extern void PtrDeviceOff();
extern void PtrDeviceControl();
extern void PtrAddEvent(int buttonMask, int x, int y);

extern void KbdDeviceInit();
extern void KbdDeviceOn();
extern void KbdDeviceOff();
extern void KbdAddEvent(Bool down, KeySym keySym);
extern void KbdReleaseAllKeys();


/* rfbserver.c */

/*
 * UPDATE_BUF_SIZE must be big enough to send at least one whole line of the
 * framebuffer.  So for a max screen width of say 2K with 32-bit pixels this
 * means 8K minimum.
 */

#define UPDATE_BUF_SIZE 30000
extern char updateBuf[UPDATE_BUF_SIZE];
extern int ublen;

extern rfbClientPtr rfbClientHead;
extern rfbClientPtr pointerClient;

extern void rfbNewClientConnection(int sock);
extern rfbClientPtr rfbReverseConnection(char *host, int port);
extern void rfbClientConnectionGone(int sock);
extern void rfbProcessClientMessage(int sock);
extern void rfbClientConnFailed(rfbClientPtr cl, char *reason);
extern void rfbNewUDPConnection(int sock);
extern void rfbProcessUDPInput(int sock);
extern Bool rfbSendFramebufferUpdate(rfbClientPtr cl);
extern Bool rfbSendRectEncodingRaw(rfbClientPtr cl, int x,int y,int w,int h);
extern Bool rfbSendUpdateBuf(rfbClientPtr cl);
extern Bool rfbSendSetColourMapEntries(rfbClientPtr cl, int firstColour,
				       int nColours);
extern void rfbSendBell();
extern void rfbSendServerCutText(char *str, int len);


/* translate.c */

extern rfbPixelFormat rfbServerFormat;

extern void rfbTranslateNone(rfbClientPtr cl, char *iptr, char *optr,
			     int iptradd, int optradd, int nlines);
extern Bool rfbSetTranslateFunction(rfbClientPtr cl);
extern void rfbSetClientColourMaps(int firstColour, int nColours);
extern Bool rfbSetClientColourMap(rfbClientPtr cl, int firstColour,
				  int nColours);


/* httpd.c */

extern int httpPort;
extern char *httpDir;

extern void httpInitSockets();
extern void httpCheckFds();



/* auth.c */

extern char *rfbAuthPasswdFile;
extern Bool rfbAuthenticating;

extern void rfbAuthNewClientConnection(rfbClientPtr cl);
extern void rfbAuthProcessClientMessage(rfbClientPtr cl);


/* rre.c */

extern Bool rfbSendRectEncodingRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* corre.c */

extern Bool rfbSendRectEncodingCoRRE(rfbClientPtr cl, int x,int y,int w,int h);


/* hextile.c */

extern Bool rfbSendRectEncodingHextile(rfbClientPtr cl, int x, int y, int w,
				       int h);


/* stats.c */

extern int rfbBytesSent[];
extern int rfbRectanglesSent[];
extern int rfbFramebufferUpdateMessagesSent;
extern int rfbRawBytesEquivalent;
extern int rfbKeyEventsRcvd;
extern int rfbPointerEventsRcvd;

extern void rfbResetStats();
extern void rfbPrintStats();
