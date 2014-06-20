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

/*
 * x.c - functions to deal with X display.
 */

#include <vncviewer.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>


#define SCROLLBAR_SIZE 10
#define SCROLLBAR_BG_SIZE (SCROLLBAR_SIZE + 2)

#define INVALID_PIXEL 0xffffffff
#define COLORMAP_SIZE 256


Display *dpy;
Window canvas = 0;
Colormap cmap;
GC gc;
GC srcGC, dstGC;
unsigned long BGR233ToPixel[COLORMAP_SIZE];

static Window topLevel;
static int topLevelWidth, topLevelHeight;

static Window viewport;
static int viewportX, viewportY;
static int viewportWidth, viewportHeight;

static Window vertScrollbarBg, horizScrollbarBg;
static Window vertScrollbar, horizScrollbar;
static Bool showScrollbars = False;
static int vertScrollbarY, vertScrollbarHeight;
static int horizScrollbarX, horizScrollbarWidth;

static Atom wmProtocols, wmDeleteWindow, wmState;
static Visual *vis;
static unsigned int visdepth;
static int visbpp;
static Bool xloginIconified = False;
static XImage *im = NULL;
static char *screenData = NULL;
static Bool modifierPressed[256];

static Cursor CreateDotCursor();
static void FindBestVisual();
static Bool HandleCanvasEvent(XEvent *ev);
static Bool HandleTopLevelEvent(XEvent *ev);
static Bool HandleVertScrollbarBgEvent(XEvent *ev);
static Bool HandleHorizScrollbarBgEvent(XEvent *ev);
static Bool HandleVertScrollbarEvent(XEvent *ev);
static Bool HandleHorizScrollbarEvent(XEvent *ev);
static Bool HandleRootEvent(XEvent *ev);
static void PositionViewportAndScrollbars();
static void CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width,int height);
static Bool IconifyWindowNamed(Window w, char *name, Bool undo);


/*
 * CreateXWindow.
 */

Bool
CreateXWindow()
{
    XSetWindowAttributes attr;
    XEvent ev;
    XColor grey;
    char defaultGeometry[256];
    XSizeHints wmHints;
    XGCValues gcv;
    int displayWidth, displayHeight;
    int i;

    if (!(dpy = XOpenDisplay(displayname))) {
	fprintf(stderr,"%s: unable to open display %s\n",
		programName, XDisplayName(displayname));
	return False;
    }

    FindBestVisual();

    for (i = 0; i < 256; i++)
	modifierPressed[i] = False;

    /* Test if the keyboard is grabbed.  If so, it's probably because the
       XDM login window is up, so try iconifying it to release the grab */

    if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), False, GrabModeSync,
		      GrabModeSync, CurrentTime) == GrabSuccess) {
	XUngrabKeyboard(dpy, CurrentTime);
    } else {
	wmState = XInternAtom(dpy, "WM_STATE", False);

	if (IconifyWindowNamed(DefaultRootWindow(dpy), "xlogin", False))
	    xloginIconified = True;
    }

    fprintf(stderr,"Creating window depth %d, visualid 0x%x colormap 0x%x\n",
	    visdepth,(int)vis->visualid,(int)cmap);

    /* Try to work out the geometry of the top-level window */

    displayWidth = WidthOfScreen(DefaultScreenOfDisplay(dpy));
    displayHeight = HeightOfScreen(DefaultScreenOfDisplay(dpy));

    topLevelWidth = si.framebufferWidth;
    topLevelHeight = si.framebufferHeight;

    if ((topLevelWidth + wmDecorationWidth) >= displayWidth)
	topLevelWidth = displayWidth - wmDecorationWidth;

    wmHints.x = (displayWidth - topLevelWidth - wmDecorationWidth) / 2;

    if ((topLevelHeight + wmDecorationHeight) >= displayHeight)
	topLevelHeight = displayHeight - wmDecorationHeight;

    wmHints.y = (displayHeight - topLevelHeight - wmDecorationHeight) / 2;

    wmHints.flags = PMaxSize;
    wmHints.max_width = si.framebufferWidth;
    wmHints.max_height = si.framebufferHeight;

    sprintf(defaultGeometry, "%dx%d+%d+%d", topLevelWidth, topLevelHeight,
	    wmHints.x, wmHints.y);

    XWMGeometry(dpy, DefaultScreen(dpy), geometry, defaultGeometry, 0,
		&wmHints, &wmHints.x, &wmHints.y,
		&topLevelWidth, &topLevelHeight, &wmHints.win_gravity);

    /* Create the top-level window */

    attr.border_pixel = 0; /* needed to allow 8-bit cmap on 24-bit display -
			      otherwise we get a Match error! */
    attr.background_pixel = BlackPixelOfScreen(DefaultScreenOfDisplay(dpy));
    attr.event_mask = LeaveWindowMask|StructureNotifyMask;
    attr.colormap = cmap;

    topLevel = XCreateWindow(dpy, DefaultRootWindow(dpy), wmHints.x, wmHints.y,
			     topLevelWidth, topLevelHeight, 0, visdepth,
			     InputOutput, vis,
			     (CWBorderPixel|CWBackPixel|CWEventMask
			      |CWColormap), &attr);

    wmHints.flags |= USPosition; /* try to force WM to place window */
    XSetWMNormalHints(dpy, topLevel, &wmHints);

    wmProtocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
    wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, topLevel, &wmDeleteWindow, 1);

    XStoreName(dpy, topLevel, desktopName);

    /* Create the scrollbars */

    attr.background_pixel = WhitePixelOfScreen(DefaultScreenOfDisplay(dpy));
    attr.event_mask = ButtonPressMask;

    vertScrollbarBg = XCreateWindow(dpy, topLevel, 0, 0, 1, 1, 0,
				    CopyFromParent, CopyFromParent,
				    CopyFromParent,
				    CWBackPixel|CWEventMask, &attr);

    horizScrollbarBg = XCreateWindow(dpy, topLevel, 0, 0, 1, 1, 0,
				     CopyFromParent, CopyFromParent,
				     CopyFromParent,
				     CWBackPixel|CWEventMask, &attr);

    XAllocNamedColor(dpy, cmap, "grey", &grey, &grey);

    attr.background_pixel = grey.pixel;
    attr.event_mask = ButtonPressMask|ButtonReleaseMask|ButtonMotionMask;
    attr.cursor = XCreateFontCursor(dpy, XC_sb_v_double_arrow);

    vertScrollbar = XCreateWindow(dpy, vertScrollbarBg, 0, 0, 1, 1, 0,
				  CopyFromParent, CopyFromParent,
				  CopyFromParent,
				  (CWBackPixel|CWEventMask|CWCursor),
				  &attr);

    attr.cursor = XCreateFontCursor(dpy, XC_sb_h_double_arrow);
    horizScrollbar = XCreateWindow(dpy, horizScrollbarBg, 0, 0, 1, 1, 0,
				   CopyFromParent, CopyFromParent,
				   CopyFromParent,
				   (CWBackPixel|CWEventMask|CWCursor),
				   &attr);

    /* Create the viewport window and the canvas window */

    viewport = XCreateWindow(dpy, topLevel, 0, 0, 1, 1, 0,
			     CopyFromParent, CopyFromParent, CopyFromParent,
			     0, &attr);
    XMapWindow(dpy, viewport);

    attr.background_pixel = grey.pixel;
    attr.backing_store = WhenMapped;
    attr.event_mask = (ExposureMask|ButtonPressMask|ButtonReleaseMask|
		       PointerMotionMask|KeyPressMask|KeyReleaseMask);
    attr.cursor = CreateDotCursor();

    canvas = XCreateWindow(dpy, viewport, 0, 0,
			   si.framebufferWidth, si.framebufferHeight,
			   0, CopyFromParent, CopyFromParent, CopyFromParent,
			   (CWBackPixel|CWBackingStore|CWEventMask
			    |CWColormap|CWCursor), &attr);
    XMapWindow(dpy, canvas);

    gc = XCreateGC(dpy,canvas,0,NULL);

    /* srcGC and dstGC are used for debugging copyrect */
    gcv.function = GXxor;
    gcv.foreground = 0x0f0f0f0f;
    srcGC = XCreateGC(dpy,canvas,GCFunction|GCForeground,&gcv);
    gcv.foreground = 0xf0f0f0f0;
    dstGC = XCreateGC(dpy,canvas,GCFunction|GCForeground,&gcv);

    viewportX = 0;
    viewportY = 0;
    PositionViewportAndScrollbars();

    XMapWindow(dpy, topLevel);

    XMaskEvent(dpy, ExposureMask, &ev);

    screenData = malloc(si.framebufferWidth * si.framebufferHeight
			* visbpp / 8);

    im = XCreateImage(dpy, vis, visdepth, ZPixmap,
		      0, (char *)screenData,
		      si.framebufferWidth, si.framebufferHeight,
		      BitmapPad(dpy), 0);

    XSelectInput(dpy, DefaultRootWindow(dpy), PropertyChangeMask);

    return True;
}


/*
 * CreateDotCursor.
 */

static Cursor
CreateDotCursor()
{
    Cursor cursor;
    Pixmap src, msk;
    static char srcBits[] = { 0, 14,14,14, 0 };
    static char mskBits[] = { 31,31,31,31,31 };
    XColor fg, bg;

    src = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), srcBits, 5, 5);
    msk = XCreateBitmapFromData(dpy, DefaultRootWindow(dpy), mskBits, 5, 5);
    XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "black",
		     &fg, &fg);
    XAllocNamedColor(dpy, DefaultColormap(dpy,DefaultScreen(dpy)), "white",
		     &bg, &bg);
    cursor = XCreatePixmapCursor(dpy, src, msk, &fg, &bg, 2, 2);
    XFreePixmap(dpy, src);
    XFreePixmap(dpy, msk);

    return cursor;
}


/*
 * ShutdownX.
 */

void
ShutdownX()
{
    if (xloginIconified) {
	IconifyWindowNamed(DefaultRootWindow(dpy), "xlogin", True);
	XFlush(dpy);
    }
    XCloseDisplay(dpy);
}


/*
 * HandleXEvents.
 */

Bool
HandleXEvents()
{
    XEvent ev;

    while (XCheckIfEvent(dpy, &ev, AllXEventsPredicate, NULL)) {

	if (ev.xany.window == canvas) {

	    if (!HandleCanvasEvent(&ev))
		return False;

	} else if (ev.xany.window == topLevel) {

	    if (!HandleTopLevelEvent(&ev))
		return False;

	} else if (ev.xany.window == vertScrollbarBg) {

	    if (!HandleVertScrollbarBgEvent(&ev))
		return False;

	} else if (ev.xany.window == horizScrollbarBg) {

	    if (!HandleHorizScrollbarBgEvent(&ev))
		return False;

	} else if (ev.xany.window == vertScrollbar) {

	    if (!HandleVertScrollbarEvent(&ev))
		return False;

	} else if (ev.xany.window == horizScrollbar) {

	    if (!HandleHorizScrollbarEvent(&ev))
		return False;

	} else if (ev.xany.window == DefaultRootWindow(dpy)) {

	    if (!HandleRootEvent(&ev))
		return False;

	} else if (ev.type == MappingNotify) {

	    XRefreshKeyboardMapping(&ev.xmapping);
	}
    }

    return True;
}


/*
 * HandleCanvasEvent.
 */

static Bool
HandleCanvasEvent(XEvent *ev)
{
    int buttonMask;
    KeySym ks;
    char keyname[256];

    switch (ev->type) {

    case GraphicsExpose:
    case Expose:
	return SendFramebufferUpdateRequest(ev->xexpose.x, ev->xexpose.y,
					    ev->xexpose.width,
					    ev->xexpose.height, False);

    case MotionNotify:
	if (viewOnly) return True;

	while (XCheckTypedWindowEvent(dpy, canvas, MotionNotify, ev))
	    ;	/* discard all queued motion notify events */

	return SendPointerEvent(ev->xmotion.x, ev->xmotion.y,
				(ev->xmotion.state & 0x1f00) >> 8);

    case ButtonPress:
    case ButtonRelease:
	if (viewOnly) return True;

	if (ev->type == ButtonPress) {
	    buttonMask = (((ev->xbutton.state & 0x1f00) >> 8) |
			  (1 << (ev->xbutton.button - 1)));
	} else {
	    buttonMask = (((ev->xbutton.state & 0x1f00) >> 8) &
			  ~(1 << (ev->xbutton.button - 1)));
	}

	return SendPointerEvent(ev->xbutton.x, ev->xbutton.y, buttonMask);

    case KeyPress:
    case KeyRelease:
	if (viewOnly) return True;

	XLookupString(&ev->xkey, keyname, 256, &ks, NULL);

	if (IsModifierKey(ks)) {
	    ks = XKeycodeToKeysym(dpy, ev->xkey.keycode, 0);
	    modifierPressed[ev->xkey.keycode] = (ev->type == KeyPress);
	}

	return SendKeyEvent(ks, (ev->type == KeyPress));
    }

    return True;
}


/*
 * HandleTopLevelEvent.
 */

static Bool
HandleTopLevelEvent(XEvent *ev)
{
    int i;

    switch (ev->type) {

    case ConfigureNotify:
	topLevelWidth = ev->xconfigure.width;
	topLevelHeight = ev->xconfigure.height;
	PositionViewportAndScrollbars();
	break;

    case LeaveNotify:
	if (viewOnly) return True;

	for (i = 0; i < 256; i++) {
	    if (modifierPressed[i]) {
		if (!SendKeyEvent(XKeycodeToKeysym(dpy, i, 0), False))
		    return False;
	    }
	}
	break;

    case ClientMessage:
	if ((ev->xclient.message_type == wmProtocols) &&
	    (ev->xclient.data.l[0] == wmDeleteWindow))
	{
	    ShutdownX();
	    exit(0);
	}
	break;
    }

    return True;
}


/*
 * HandleVertScrollbarBgEvent.
 */

static Bool
HandleVertScrollbarBgEvent(XEvent *ev)
{
    if (ev->type == ButtonPress) {
	if (ev->xbutton.y > (vertScrollbarY + vertScrollbarHeight)) {
	    viewportY = si.framebufferHeight;
	} else if (ev->xbutton.y < vertScrollbarY) {
	    viewportY = 0;
	}
	PositionViewportAndScrollbars();
    }

    return True;
}


/*
 * HandleHorizScrollbarBgEvent.
 */

static Bool
HandleHorizScrollbarBgEvent(XEvent *ev)
{
    if (ev->type == ButtonPress) {
	if (ev->xbutton.x > (horizScrollbarX + horizScrollbarWidth)) {
	    viewportX = si.framebufferWidth;
	} else if (ev->xbutton.x < horizScrollbarX) {
	    viewportX = 0;
	}
	PositionViewportAndScrollbars();
    }

    return True;
}


/*
 * HandleVertScrollbarEvent.
 */

static Bool
HandleVertScrollbarEvent(XEvent *ev)
{
    static int downY;

    switch (ev->type) {

    case ButtonPress:
	downY = ev->xbutton.y_root;
	break;

    case MotionNotify:
	while (XCheckWindowEvent(dpy,
				 vertScrollbar, PointerMotionMask, ev)) ;

	/* discard any other queued MotionNotify events and drop through... */

    case ButtonRelease:
	viewportY += ((ev->xbutton.y_root - downY) * si.framebufferHeight
		      / viewportHeight);
	downY = ev->xbutton.y_root;

	PositionViewportAndScrollbars();

	break;
    }

    return True;
}


/*
 * HandleHorizScrollbarEvent.
 */

static Bool
HandleHorizScrollbarEvent(XEvent *ev)
{
    static int downX;

    switch (ev->type) {

    case ButtonPress:
	downX = ev->xbutton.x_root;
	break;

    case MotionNotify:
	while (XCheckWindowEvent(dpy,
				 horizScrollbar, PointerMotionMask, ev)) ;

	/* discard any other queued MotionNotify events and drop through... */

    case ButtonRelease:
	viewportX += ((ev->xbutton.x_root - downX) * si.framebufferWidth
		      / viewportWidth);
	downX = ev->xbutton.x_root;

	PositionViewportAndScrollbars();

	break;
    }

    return True;
}


/*
 * PositionViewportAndScrollbars.
 */

static void
PositionViewportAndScrollbars()
{
    if ((topLevelWidth >= si.framebufferWidth) &&
	(topLevelHeight >= si.framebufferHeight))
    {
	viewportWidth = si.framebufferWidth;
	viewportHeight = si.framebufferHeight;
	showScrollbars = False;
    } else {
	viewportWidth = topLevelWidth - SCROLLBAR_BG_SIZE - 1;
	viewportHeight = topLevelHeight - SCROLLBAR_BG_SIZE - 1;
	showScrollbars = True;
    }

    if (viewportX < 0)
	viewportX = 0;
    if (viewportX > (si.framebufferWidth - viewportWidth))
	viewportX = si.framebufferWidth - viewportWidth;

    if (viewportY < 0)
	viewportY = 0;
    if (viewportY > (si.framebufferHeight - viewportHeight))
	viewportY = si.framebufferHeight - viewportHeight;

    XMoveWindow(dpy, canvas, -viewportX, -viewportY);
    XResizeWindow(dpy, viewport, viewportWidth, viewportHeight);

    if (showScrollbars) {
	XMoveResizeWindow(dpy, vertScrollbarBg, viewportWidth + 1, 0,
			  SCROLLBAR_BG_SIZE, viewportHeight);
	XMoveResizeWindow(dpy, horizScrollbarBg, 0, viewportHeight + 1,
			  viewportWidth, SCROLLBAR_BG_SIZE);

	vertScrollbarY = viewportHeight * viewportY / si.framebufferHeight;
	vertScrollbarHeight
	    = viewportHeight * viewportHeight / si.framebufferHeight;

	XMoveResizeWindow(dpy, vertScrollbar, 1, vertScrollbarY,
			  SCROLLBAR_SIZE, vertScrollbarHeight);

	horizScrollbarX = viewportWidth * viewportX / si.framebufferWidth;
	horizScrollbarWidth
	    = viewportWidth * viewportWidth / si.framebufferWidth;

	XMoveResizeWindow(dpy, horizScrollbar, horizScrollbarX, 1,
			  horizScrollbarWidth, SCROLLBAR_SIZE);

	XMapWindow(dpy, vertScrollbar);
	XMapWindow(dpy, horizScrollbar);
	XMapWindow(dpy, vertScrollbarBg);
	XMapWindow(dpy, horizScrollbarBg);

    } else {
	XUnmapWindow(dpy, vertScrollbarBg);
	XUnmapWindow(dpy, horizScrollbarBg);
	XUnmapWindow(dpy, vertScrollbar);
	XUnmapWindow(dpy, horizScrollbar);
    }
}


/*
 * HandleRootEvent.
 */

static Bool
HandleRootEvent(XEvent *ev)
{
    char *str;
    int len;

    switch (ev->type) {

    case PropertyNotify:
	if (ev->xproperty.atom == XA_CUT_BUFFER0) {

	    str = XFetchBytes(dpy, &len);
	    if (str) {
		if (!SendClientCutText(str, len))
		    return False;
		XFree(str);
	    }
	}
	break;
    }

    return True;
}


/*
 * AllXEventsPredicate is needed to make XCheckIfEvent return all events.
 */

Bool
AllXEventsPredicate(Display *dpy, XEvent *ev, char *arg)
{
    return True;
}


/*
 * FindBestVisual deals with the wonderful world of X "visuals" (which are
 * equivalent to the RFB protocol's "pixel format").  It takes into account any
 * command line arguments about depth and colour mapping and finds the visual
 * supported by the X server which comes closest.  It sets up the myFormat
 * structure to describe the pixel format in terms that the RFB server will be
 * able to understand.  The catch-all case is to use the BGR233 pixel format
 * and use a lookup table to translate to the nearest colours provided by the X
 * server.
 */

static void
FindBestVisual()
{
    int r, g, b;
    XColor c;
    Bool allocColorFailed = False;
    XVisualInfo tmpl;
    XVisualInfo *vinfo;
    int nvis;
    int nformats;
    XPixmapFormatValues *format;

    if (forceOwnCmap) {

	/* user wants us to use a pseudocolor visual */

	if (!si.format.trueColour) {

	    /* if server uses a colour map, see if we have a pseudocolor visual
	       of the right depth */

	    tmpl.screen = DefaultScreen(dpy);
	    tmpl.depth = si.format.bitsPerPixel;
	    tmpl.class = PseudoColor;
	    tmpl.colormap_size = (1 << si.format.bitsPerPixel);

	    vinfo = XGetVisualInfo(dpy,
				   VisualScreenMask|VisualDepthMask|
				   VisualClassMask|VisualColormapSizeMask,
				   &tmpl, &nvis);

	    if (vinfo) {
		vis = vinfo[0].visual;
		visdepth = vinfo[0].depth;
		visbpp = si.format.bitsPerPixel;
		XFree(vinfo);

		cmap = XCreateColormap(dpy, DefaultRootWindow(dpy), vis,
				       AllocAll);

		fprintf(stderr,"Using pseudocolor visual, depth %d\n",
			visdepth);

		myFormat = si.format;

		if (si.format.bitsPerPixel != 8) {
		    myFormat.bigEndian = (ImageByteOrder(dpy) == MSBFirst);
		}
		return;
	    }
	}

	/* otherwise see if we have an 8-bit pseudocolor visual */

	tmpl.screen = DefaultScreen(dpy);
	tmpl.depth = 8;
	tmpl.class = PseudoColor;
	tmpl.colormap_size = 256;

	vinfo = XGetVisualInfo(dpy,
			       VisualScreenMask|VisualDepthMask|
			       VisualClassMask|VisualColormapSizeMask,
			       &tmpl, &nvis);

	if (vinfo) {
	    vis = vinfo[0].visual;
	    visdepth = vinfo[0].depth;
	    visbpp = 8;
	    XFree(vinfo);

	    cmap = XCreateColormap(dpy, DefaultRootWindow(dpy), vis, AllocAll);

	    fprintf(stderr,"Using pseudocolor visual, depth %d\n",visdepth);

	    myFormat.bitsPerPixel = 8;
	    myFormat.depth = 8;
	    myFormat.trueColour = 0;
	    myFormat.bigEndian = 0;
	    myFormat.redMax = myFormat.greenMax = myFormat.blueMax = 0;
	    myFormat.redShift = myFormat.greenShift
		= myFormat.blueShift = 0;

	    return;
	}

    }

    if (forceTruecolour) {
	int mask = VisualScreenMask|VisualClassMask;

	tmpl.screen = DefaultScreen(dpy);
	tmpl.class = TrueColor;

	if (requestedDepth != 0) {
	    tmpl.depth = requestedDepth;
	    mask |= VisualDepthMask;
	}

	vinfo = XGetVisualInfo(dpy, mask, &tmpl, &nvis);

	if (vinfo) {
	    vis = vinfo[0].visual;
	    visdepth = vinfo[0].depth;
	    XFree(vinfo);

	    cmap = XCreateColormap(dpy, DefaultRootWindow(dpy), vis,AllocNone);

	    fprintf(stderr,"Using truecolor visual, depth %d\n", visdepth);

	    format = XListPixmapFormats(dpy, &nformats);

	    while (format->depth != visdepth) {
		format++;
		if (--nformats <= 0) {
		    fprintf(stderr,"no pixmap format for depth %d???\n",
			    visdepth);
		    exit(1);
		}
	    }

	    visbpp = format->bits_per_pixel;

	    myFormat.bitsPerPixel = format->bits_per_pixel;
	    myFormat.depth = visdepth;
	    myFormat.trueColour = 1;
	    myFormat.bigEndian = (ImageByteOrder(dpy) == MSBFirst);
	    myFormat.redShift = ffs(vis->red_mask) - 1;
	    myFormat.greenShift = ffs(vis->green_mask) - 1;
	    myFormat.blueShift = ffs(vis->blue_mask) - 1;
	    myFormat.redMax = vis->red_mask >> myFormat.redShift;
	    myFormat.greenMax = vis->green_mask >> myFormat.greenShift;
	    myFormat.blueMax = vis->blue_mask >> myFormat.blueShift;

	    return;
	}
	
    }

    if (!useBGR233 && si.format.trueColour &&
	((si.format.bitsPerPixel == 8) ||
	 (si.format.bigEndian == (ImageByteOrder(dpy) == MSBFirst)))) {

	/* if server is truecolour, see if we have exactly matching visual */

	tmpl.screen = DefaultScreen(dpy);
	tmpl.depth = (ffs(si.format.redMax + 1) +
		      ffs(si.format.greenMax + 1) +
		      ffs(si.format.blueMax + 1)) - 3;
	tmpl.class = TrueColor;
	tmpl.red_mask = si.format.redMax << si.format.redShift;
	tmpl.green_mask = si.format.greenMax << si.format.greenShift;
	tmpl.blue_mask = si.format.blueMax << si.format.blueShift;

	vinfo = XGetVisualInfo(dpy,
			       VisualScreenMask|VisualDepthMask|
			       VisualClassMask|VisualRedMaskMask|
			       VisualGreenMaskMask|VisualBlueMaskMask,
			       &tmpl, &nvis);

	if (vinfo) {
	    vis = vinfo[0].visual;
	    visdepth = vinfo[0].depth;
	    visbpp = si.format.bitsPerPixel;
	    XFree(vinfo);

	    cmap = XCreateColormap(dpy, DefaultRootWindow(dpy), vis,AllocNone);

	    fprintf(stderr,"Using perfect match truecolor visual, depth %d\n",
		   visdepth);

	    myFormat = si.format;
	    return;
	}
    }


    /*
     * We don't have an exactly matching visual.  If we're really truecolour
     * then just ask for our natural format, otherwise ask for BGR233 and
     * we'll translate.
     */

    vis = DefaultVisual(dpy,DefaultScreen(dpy));
    visdepth = DefaultDepth(dpy,DefaultScreen(dpy));
    cmap = DefaultColormap(dpy,DefaultScreen(dpy));

    format = XListPixmapFormats(dpy, &nformats);

    while (format->depth != visdepth) {
	format++;
	if (--nformats <= 0) {
	    fprintf(stderr,"no pixmap format for depth %d???\n",visdepth);
	    exit(1);
	}
    }

    visbpp = format->bits_per_pixel;

    if (!useBGR233 && (vis->class == TrueColor)) {

	/* really truecolour */

	fprintf(stderr,"Using default colormap which is truecolor\n");

	myFormat.bitsPerPixel = format->bits_per_pixel;
	myFormat.depth = visdepth;
	myFormat.trueColour = 1;
	myFormat.bigEndian = (ImageByteOrder(dpy) == MSBFirst);
	myFormat.redShift = ffs(vis->red_mask) - 1;
	myFormat.greenShift = ffs(vis->green_mask) - 1;
	myFormat.blueShift = ffs(vis->blue_mask) - 1;
	myFormat.redMax = vis->red_mask >> myFormat.redShift;
	myFormat.greenMax = vis->green_mask >> myFormat.greenShift;
	myFormat.blueMax = vis->blue_mask >> myFormat.blueShift;

	return;
    }

    fprintf(stderr,"Using default colormap and translating to BGR233\n");

    useBGR233 = True;

    myFormat.bitsPerPixel = 8;
    myFormat.depth = 8;
    myFormat.trueColour = 1;
    myFormat.bigEndian = 0;
    myFormat.redMax = 7;
    myFormat.greenMax = 7;
    myFormat.blueMax = 3;
    myFormat.redShift = 0;
    myFormat.greenShift = 3;
    myFormat.blueShift = 6;

    for (r = 0; r < 8; r++) {
	for (g = 0; g < 8; g++) {
	    for (b = 0; b < 4; b++) {
		c.red = r * 65535 / 7;
		c.green = g * 65535 / 7;
		c.blue = b * 65535 / 3;
		if (!XAllocColor(dpy, cmap, &c)) {
		    allocColorFailed = True;
		    c.pixel = INVALID_PIXEL;
		}
		BGR233ToPixel[(b<<6) | (g<<3) | r] = c.pixel;
	    }
	}
    }

    if (allocColorFailed) {
	XColor colorcells[COLORMAP_SIZE];
	unsigned long i, nearestPixel = 0;

	for (i = 0; i < COLORMAP_SIZE; i++) {
	    colorcells[i].pixel = i;
	}

	XQueryColors(dpy, cmap, colorcells, COLORMAP_SIZE);

	for (r = 0; r < 8; r++) {
	    for (g = 0; g < 8; g++) {
		for (b = 0; b < 4; b++) {
		    if (BGR233ToPixel[(b<<6) | (g<<3) | r] == INVALID_PIXEL) {

			unsigned long minDistance = 65536 * 3;

			for (i = 0; i < COLORMAP_SIZE; i++) {
			    unsigned long distance
				= (abs(colorcells[i].red - r * 65535 / 7)
				   + abs(colorcells[i].green - g * 65535 / 7)
				   + abs(colorcells[i].blue - b * 65535 / 3));

			    if (distance < minDistance) {
				minDistance = distance;
				nearestPixel = i;
			    }
			}

			BGR233ToPixel[(b<<6) | (g<<3) | r] = nearestPixel;
		    }
		}
	    }
	}
    }
}


/*
 * CopyDataToScreen.
 */

void
CopyDataToScreen(CARD8 *buf, int x, int y, int width, int height)
{
    if (rawDelay != 0) {
	XFillRectangle(dpy, canvas, DefaultGC(dpy,DefaultScreen(dpy)),
		       x, y, width, height);

	XSync(dpy,False);

	usleep(rawDelay * 1000);
    }

    if (!useBGR233) {
	int h;
	int widthInBytes = width * myFormat.bitsPerPixel / 8;
	int scrWidthInBytes = si.framebufferWidth * myFormat.bitsPerPixel / 8;

	char *scr = (screenData + y * scrWidthInBytes
		     + x * myFormat.bitsPerPixel / 8);

	for (h = 0; h < height; h++) {
	    memcpy(scr, buf, widthInBytes);
	    buf += widthInBytes;
	    scr += scrWidthInBytes;
	}
    } else {
	CopyBGR233ToScreen(buf, x, y, width, height);
    }

    XPutImage(dpy, canvas, gc, im, x, y, x, y,
	      width, height);
}


/*
 * CopyBGR233ToScreen.
 */

static void
CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width, int height)
{
    int p, q;
    int xoff = 7 - x & 7;
    int xcur;
    int fbwb = si.framebufferWidth / 8;
    CARD8 *scr1 = ((CARD8 *)screenData) + y * fbwb + x / 8;
    CARD8 *scrt;
    CARD8 *scr8 = ((CARD8 *)screenData) + y * si.framebufferWidth + x;
    CARD16 *scr16 = ((CARD16 *)screenData) + y * si.framebufferWidth + x;
    CARD32 *scr32 = ((CARD32 *)screenData) + y * si.framebufferWidth + x;

    switch (visdepth) {

	/* thanks to Chris Hooper for single bpp support */

    case 1:
	for (q = 0; q < height; q++) {
             xcur = xoff;
             scrt = scr1;
             for (p = 0; p < width; p++) {
                 *scrt = (*scrt & ~(1 << xcur)
			  | (BGR233ToPixel[*(buf++)] << xcur));

                 if (xcur-- == 0) {
                     xcur = 7;
                     scrt++;
                 }
             }
             scr1 += fbwb;
	}
	break;

    case 8:
	for (q = 0; q < height; q++) {
	    for (p = 0; p < width; p++) {
		*(scr8++) = BGR233ToPixel[*(buf++)];
	    }
	    scr8 += si.framebufferWidth - width;
	}
	break;

    case 16:
	for (q = 0; q < height; q++) {
	    for (p = 0; p < width; p++) {
		*(scr16++) = BGR233ToPixel[*(buf++)];
	    }
	    scr16 += si.framebufferWidth - width;
	}
	break;

    case 24:
	for (q = 0; q < height; q++) {
	    for (p = 0; p < width; p++) {
		*(scr32++) = BGR233ToPixel[*(buf++)];
	    }
	    scr32 += si.framebufferWidth - width;
	}
	break;
    }
}


/*
 * IconifyWindowNamed.
 */

static Bool
IconifyWindowNamed(Window w, char *name, Bool undo)
{
    Window *children, dummy;
    unsigned int nchildren;
    int i;
    char *window_name;
    Atom type = None;
    int format;
    unsigned long nitems, after;
    unsigned char *data;

    if (XFetchName(dpy, w, &window_name)) {
	if (strcmp(window_name, name) == 0) {
	    if (undo) {
		XMapWindow(dpy, w);
	    } else {
		XIconifyWindow(dpy, w, DefaultScreen(dpy));
	    }
	    XFree(window_name);
	    return True;
	}
	XFree(window_name);
    }

    XGetWindowProperty(dpy, w, wmState, 0, 0, False,
		       AnyPropertyType, &type, &format, &nitems,
		       &after, &data);
    if (type != None) {
	XFree(data);
	return False;
    }

    if (!XQueryTree(dpy, w, &dummy, &dummy, &children, &nchildren))
	return False;

    for (i = 0; i < nchildren; i++) {
	if (IconifyWindowNamed(children[i], name, undo)) {
	    XFree ((char *)children);
	    return True;
	}
    }
    if (children) XFree ((char *)children);
    return False;
}
