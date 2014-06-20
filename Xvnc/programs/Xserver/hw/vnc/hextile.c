/*
 * hextile.c
 *
 * Routines to implement Hextile Encoding
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

#include <stdio.h>
#include "rfb.h"

static Bool sendHextiles8 _((rfbClientPtr cl, int x, int y, int w, int h));
static Bool sendHextiles16 _((rfbClientPtr cl, int x, int y, int w, int h));
static Bool sendHextiles32 _((rfbClientPtr cl, int x, int y, int w, int h));


/*
 * rfbSendRectEncodingHextile - send a rectangle using hextile encoding.
 */

Bool
rfbSendRectEncodingHextile(cl, x, y, w, h)
    rfbClientPtr cl;
    int x, y, w, h;
{
    rfbFramebufferUpdateRectHeader rect;

    if (ublen + sz_rfbFramebufferUpdateRectHeader > UPDATE_BUF_SIZE) {
	if (!rfbSendUpdateBuf(cl))
	    return FALSE;
    }

    rect.r.x = Swap16IfLE(x);
    rect.r.y = Swap16IfLE(y);
    rect.r.w = Swap16IfLE(w);
    rect.r.h = Swap16IfLE(h);
    rect.encoding = Swap32IfLE(rfbEncodingHextile);

    memcpy(&updateBuf[ublen], (char *)&rect,
	   sz_rfbFramebufferUpdateRectHeader);
    ublen += sz_rfbFramebufferUpdateRectHeader;

    rfbRectanglesSent[rfbEncodingHextile]++;
    rfbBytesSent[rfbEncodingHextile] += sz_rfbFramebufferUpdateRectHeader;

    switch (cl->format.bitsPerPixel) {
    case 8:
	return sendHextiles8(cl, x, y, w, h);
    case 16:
	return sendHextiles16(cl, x, y, w, h);
    case 32:
	return sendHextiles32(cl, x, y, w, h);
    }

    fprintf(stderr,"rfbSendRectEncodingHextile: bpp %d?\n",
	    cl->format.bitsPerPixel);
    return FALSE;
}


#define PUT_PIXEL8(pix) (updateBuf[ublen++] = (pix))

#define PUT_PIXEL16(pix) (updateBuf[ublen++] = ((char*)&(pix))[0], \
			  updateBuf[ublen++] = ((char*)&(pix))[1])

#define PUT_PIXEL32(pix) (updateBuf[ublen++] = ((char*)&(pix))[0], \
			  updateBuf[ublen++] = ((char*)&(pix))[1], \
			  updateBuf[ublen++] = ((char*)&(pix))[2], \
			  updateBuf[ublen++] = ((char*)&(pix))[3])


#define DEFINE_SEND_HEXTILES(bpp)					      \
									      \
									      \
static Bool subrectEncode##bpp(CARD##bpp *data, int w, int h, CARD##bpp bg,   \
			       CARD##bpp fg, Bool mono);		      \
static void testColours##bpp(CARD##bpp *data, int size, Bool *mono,	      \
			     Bool *solid, CARD##bpp *bg, CARD##bpp *fg);      \
									      \
									      \
/*									      \
 * rfbSendHextiles							      \
 */									      \
									      \
static Bool								      \
sendHextiles##bpp(cl, rx, ry, rw, rh)					      \
    rfbClientPtr cl;							      \
    int rx, ry, rw, rh;							      \
{									      \
    int x, y, w, h;							      \
    int startUblen;							      \
    char *fbptr;							      \
    CARD##bpp bg, fg, newBg, newFg;					      \
    Bool mono, solid;							      \
    Bool validBg = FALSE;						      \
    Bool validFg = FALSE;						      \
    CARD##bpp clientPixelData[16*16*(bpp/8)];				      \
									      \
    for (y = ry; y < ry+rh; y += 16) {					      \
	for (x = rx; x < rx+rw; x += 16) {				      \
	    w = h = 16;							      \
	    if (rx+rw - x < 16)						      \
		w = rx+rw - x;						      \
	    if (ry+rh - y < 16)						      \
		h = ry+rh - y;						      \
									      \
	    if ((ublen + 1 + 16*16*(bpp/8)) > UPDATE_BUF_SIZE) {	      \
		if (!rfbSendUpdateBuf(cl))				      \
		    return FALSE;					      \
	    }								      \
									      \
	    fbptr = (rfbScreen.pfbMemory + (rfbScreen.paddedWidthInBytes * y) \
		     + (x * (rfbScreen.bitsPerPixel / 8)));		      \
									      \
	    (*cl->translateFn)(cl, fbptr, (char *)clientPixelData,	      \
			       rfbScreen.paddedWidthInBytes,		      \
			       w * (bpp/8), h);				      \
									      \
	    startUblen = ublen;						      \
	    updateBuf[startUblen] = 0;					      \
	    ublen++;							      \
									      \
	    testColours##bpp(clientPixelData, w * h,			      \
			     &mono, &solid, &newBg, &newFg);		      \
									      \
	    if (!validBg || (newBg != bg)) {				      \
		validBg = TRUE;						      \
		bg = newBg;						      \
		updateBuf[startUblen] |= rfbHextileBackgroundSpecified;	      \
		PUT_PIXEL##bpp(bg);					      \
	    }								      \
									      \
	    if (solid) {						      \
		rfbBytesSent[rfbEncodingHextile] += ublen - startUblen;	      \
		continue;						      \
	    }								      \
									      \
	    updateBuf[startUblen] |= rfbHextileAnySubrects;		      \
									      \
	    if (mono) {							      \
		if (!validFg || (newFg != fg)) {			      \
		    validFg = TRUE;					      \
		    fg = newFg;						      \
		    updateBuf[startUblen] |= rfbHextileForegroundSpecified;   \
		    PUT_PIXEL##bpp(fg);					      \
		}							      \
	    } else {							      \
		validFg = FALSE;					      \
		updateBuf[startUblen] |= rfbHextileSubrectsColoured;	      \
	    }								      \
									      \
	    if (!subrectEncode##bpp(clientPixelData, w, h, bg, fg, mono)) {   \
		/* encoding was too large, use raw */			      \
		validBg = FALSE;					      \
		validFg = FALSE;					      \
		ublen = startUblen;					      \
		updateBuf[ublen++] = rfbHextileRaw;			      \
		(*cl->translateFn)(cl, fbptr, (char *)clientPixelData,	      \
				   rfbScreen.paddedWidthInBytes,	      \
				   w * (bpp/8), h);			      \
									      \
		memcpy(&updateBuf[ublen], (char *)clientPixelData,	      \
		       w * h * (bpp/8));				      \
									      \
		ublen += w * h * (bpp/8);				      \
	    }								      \
									      \
	    rfbBytesSent[rfbEncodingHextile] += ublen - startUblen;	      \
	}								      \
    }									      \
									      \
    return TRUE;							      \
}									      \
									      \
									      \
static Bool								      \
subrectEncode##bpp(CARD##bpp *data, int w, int h, CARD##bpp bg,		      \
		   CARD##bpp fg, Bool mono)				      \
{									      \
    CARD##bpp cl;							      \
    int x,y;								      \
    int i,j;								      \
    int hx=0,hy,vx=0,vy;						      \
    int hyflag;								      \
    CARD##bpp *seg;							      \
    CARD##bpp *line;							      \
    int hw,hh,vw,vh;							      \
    int thex,they,thew,theh;						      \
    int numsubs = 0;							      \
    int newLen;								      \
    int nSubrectsUblen;							      \
									      \
    nSubrectsUblen = ublen;						      \
    ublen++;								      \
									      \
    for (y=0; y<h; y++) {						      \
	line = data+(y*w);						      \
	for (x=0; x<w; x++) {						      \
	    if (line[x] != bg) {					      \
		cl = line[x];						      \
		hy = y-1;						      \
		hyflag = 1;						      \
		for (j=y; j<h; j++) {					      \
		    seg = data+(j*w);					      \
		    if (seg[x] != cl) {break;}				      \
		    i = x;						      \
		    while ((seg[i] == cl) && (i < w)) i += 1;		      \
		    i -= 1;						      \
		    if (j == y) vx = hx = i;				      \
		    if (i < vx) vx = i;					      \
		    if ((hyflag > 0) && (i >= hx)) {			      \
			hy += 1;					      \
		    } else {						      \
			hyflag = 0;					      \
		    }							      \
		}							      \
		vy = j-1;						      \
									      \
		/* We now have two possible subrects: (x,y,hx,hy) and	      \
		 * (x,y,vx,vy).  We'll choose the bigger of the two.	      \
		 */							      \
		hw = hx-x+1;						      \
		hh = hy-y+1;						      \
		vw = vx-x+1;						      \
		vh = vy-y+1;						      \
									      \
		thex = x;						      \
		they = y;						      \
									      \
		if ((hw*hh) > (vw*vh)) {				      \
		    thew = hw;						      \
		    theh = hh;						      \
		} else {						      \
		    thew = vw;						      \
		    theh = vh;						      \
		}							      \
									      \
		if (mono) {						      \
		    newLen = ublen - nSubrectsUblen + 2;		      \
		} else {						      \
		    newLen = ublen - nSubrectsUblen + bpp/8 + 2;	      \
		}							      \
									      \
		if (newLen > (w * h * (bpp/8)))				      \
		    return FALSE;					      \
									      \
		numsubs += 1;						      \
									      \
		if (!mono) PUT_PIXEL##bpp(cl);				      \
									      \
		updateBuf[ublen++] = rfbHextilePackXY(thex,they);	      \
		updateBuf[ublen++] = rfbHextilePackWH(thew,theh);	      \
									      \
		/*							      \
		 * Now mark the subrect as done.			      \
		 */							      \
		for (j=they; j < (they+theh); j++) {			      \
		    for (i=thex; i < (thex+thew); i++) {		      \
			data[j*w+i] = bg;				      \
		    }							      \
		}							      \
	    }								      \
	}								      \
    }									      \
									      \
    updateBuf[nSubrectsUblen] = numsubs;				      \
									      \
    return TRUE;							      \
}									      \
									      \
									      \
/*									      \
 * testColours() tests if there are one (solid), two (mono) or more	      \
 * colours in a tile and gets a reasonable guess at the best background	      \
 * pixel, and the foreground pixel for mono.				      \
 */									      \
									      \
static void								      \
testColours##bpp(data,size,mono,solid,bg,fg)				      \
    CARD##bpp *data;							      \
    int size;								      \
    Bool *mono;								      \
    Bool *solid;							      \
    CARD##bpp *bg;							      \
    CARD##bpp *fg;							      \
{									      \
    CARD##bpp colour1, colour2;						      \
    int n1 = 0, n2 = 0;							      \
    *mono = TRUE;							      \
    *solid = TRUE;							      \
									      \
    for (; size > 0; size--, data++) {					      \
									      \
	if (n1 == 0)							      \
	    colour1 = *data;						      \
									      \
	if (*data == colour1) {						      \
	    n1++;							      \
	    continue;							      \
	}								      \
									      \
	if (n2 == 0) {							      \
	    *solid = FALSE;						      \
	    colour2 = *data;						      \
	}								      \
									      \
	if (*data == colour2) {						      \
	    n2++;							      \
	    continue;							      \
	}								      \
									      \
	*mono = FALSE;							      \
	break;								      \
    }									      \
									      \
    if (n1 > n2) {							      \
	*bg = colour1;							      \
	*fg = colour2;							      \
    } else {								      \
	*bg = colour2;							      \
	*fg = colour1;							      \
    }									      \
}

DEFINE_SEND_HEXTILES(8)
DEFINE_SEND_HEXTILES(16)
DEFINE_SEND_HEXTILES(32)
