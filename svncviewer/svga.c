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
 * svga.c - the svgalib interface
 * Basically hacked from x.c by {ganesh,sitaram}@cse.iitb.ernet.in
 */

#include <vncviewer.h>

#include <vga.h>
#include <vgagl.h>
#include <vgakeyboard.h>
#include <vgamouse.h>
#include <fcntl.h>
#include <unistd.h>

#define INVALID_PIXEL 0xffffffff
#define COLORMAP_SIZE 256

unsigned long BGR233ToPixel[COLORMAP_SIZE];

static char *screenData = NULL;

static int Mode = 0;

static void CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width,int height);
static void FindBestMode (void);
static int colorbits (int);
static int hasmode (int, int, int);

/*
 * InitSvga
 */
Bool InitSvga (void)
{
	vga_setmousesupport (1);
	FindBestMode ();

#define NO_ACCEL 1
#ifdef NO_ACCEL
	/* This workaround is to prevent gl from using accelerated functions.
	 * At least on my CLGD 5430/40, svgalib acceleration is buggy.
	 */
	vga_setmode (G320x200x256);
	gl_setcontextvga (Mode);
	vga_setmode (Mode);
#else
	vga_setmode (10);
	gl_setcontextvga (Mode);
#endif
	if (useBGR233) {
		int r, g, b, c;
		for (r = 0; r < 8; r++)
			for (g = 0; g < 8; g++)
				for (b = 0; b < 4; b++) {
					c = (b << 6) | (g << 3) | r;
					gl_setpalettecolor (c, r * 63 / 7, g * 63 / 7, b * 63 / 3);
					BGR233ToPixel[c] = c;
				}
	}

	if (!svncKeyboardInit()) return False;

	fprintf (stderr, "SVGA: using mode %d\n", Mode);

	return True;
}	

/*
 * ShutdownSvga.
 */
void
ShutdownSvga()
{
	keyboard_close ();
	vga_setmode (TEXT);
	mouse_close ();
}

void HandleMouseEvent (void)
{
	int x, y, button;
	
	x = mouse_getx ();
	y = mouse_gety ();
	button = mouse_getbutton ();
	button = ((button & 1) << 2) | ((button >> 2) & 1) | (button & 0xfa);
	SendPointerEvent (x, y, button);
}


static void FindBestMode (void)
{
	int i;
	int mode = -1;
	vga_modeinfo *mi;
	int width, height, depth;

	width = si.framebufferWidth;
	height = si.framebufferHeight;
	depth = si.format.depth;

	if (forceOwnCmap || useBGR233) {
		if ((mode = hasmode (width, height, 8)) < 0)
			goto nomode;
	} else if (forceTruecolour) {
		i = (requestedDepth) ? requestedDepth : depth;
		i = (i == 8) ? 16 : i;
		if ((mode = hasmode (width, height, i)) < 0)
			if ((mode = hasmode (width, height, 16)) < 0)
				goto nomode;
	} else {
		if ((mode = hasmode (width, height, depth)) < 0)
			if ((mode = hasmode (width, height, 16)) < 0)
				if ((mode = hasmode (width, height, 8)) < 0)
					goto nomode;
	}

	Mode = mode;
	mi = vga_getmodeinfo (mode);

	myFormat.bitsPerPixel = mi->bytesperpixel * 8;
	myFormat.depth = colorbits(mi->colors);
	myFormat.bigEndian = 0;
	myFormat.trueColour = (myFormat.depth == 8 && !useBGR233) ? 0 : 1;
	if (myFormat.trueColour) {
		if (myFormat.depth == 8) {
			myFormat.redMax = myFormat.greenMax = 7;
			myFormat.blueMax = 3;
			myFormat.redShift = 0;
			myFormat.greenShift = 3;
			myFormat.blueShift = 6;
		}
		else if (myFormat.depth == 15) {
			myFormat.redMax = myFormat.greenMax = myFormat.blueMax = 31;
			myFormat.redShift = 10;
			myFormat.greenShift = 5;
			myFormat.blueShift = 0;
		}
		else if (myFormat.depth == 16) {
			myFormat.redMax = myFormat.blueMax = 31;
			myFormat.greenMax = 63;
			myFormat.redShift = 11;
			myFormat.greenShift = 5;
			myFormat.blueShift = 0;
		}
		else if (myFormat.depth == 24) {
			myFormat.redMax = myFormat.greenMax = myFormat.blueMax = 255;
			myFormat.redShift = 16;
			myFormat.greenShift = 8;
			myFormat.blueShift = 0;
		}
	}
	return;

nomode:
	fprintf (stderr, "No suitable mode available\n");
	ShutdownSvga ();
	exit (2);
}

/*
 * CopyDataToScreen.
 */

void
CopyDataToScreen(CARD8 *buf, int x, int y, int width, int height)
{
    if (delay != 0) {
#if 0 /* what was this supposed to do ? */
	XFillRectangle(dpy, canvas, DefaultGC(dpy,DefaultScreen(dpy)),
		       x, y, width, height);

	XSync(dpy,False);
#endif
	usleep(delay * 1000);
    }

	gl_putbox (x, y, width, height, buf);
}


/*
 * CopyBGR233ToScreen.
 */

static void
CopyBGR233ToScreen(CARD8 *buf, int x, int y, int width, int height)
{
    int p, q;
    CARD8 *scr8 = ((CARD8 *)screenData) + y * si.framebufferWidth + x;
    CARD16 *scr16 = ((CARD16 *)screenData) + y * si.framebufferWidth + x;
    CARD32 *scr32 = ((CARD32 *)screenData) + y * si.framebufferWidth + x;

    switch (myFormat.depth) {

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

static int 
colorbits (int cols)
{
	switch (cols) {
		case 256: return 8;break;
		case 32768: return 15;break;
		case 65536: return 16;break;
		case 65536*256: return 24;break;
		default: return -1; break;
	}
}

static int
hasmode (int width, int height, int depth)
{
	int i;
	vga_modeinfo *mi;

	for (i = 0; i < 256; i++) {
		if (!vga_hasmode (i))
			continue;
		mi = vga_getmodeinfo (i);
		if (mi->width >= width && mi->height >= height &&
			mi->colors == (1 << depth))
			return i;
	}
	return -1;
}
