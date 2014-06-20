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
/* $XConsortium: mizerline.c,v 5.7 94/04/17 20:28:05 dpw Exp $ */
#include "X.h"

#include "misc.h"
#include "scrnintstr.h"
#include "gcstruct.h"
#include "windowstr.h"
#include "pixmap.h"
#include "mi.h"
#include "miline.h"

#define MIOUTCODES(outcode, x, y, xmin, ymin, xmax, ymax) \
{\
     if (x < xmin) outcode |= OUT_LEFT;\
     if (x > xmax) outcode |= OUT_RIGHT;\
     if (y < ymin) outcode |= OUT_ABOVE;\
     if (y > ymax) outcode |= OUT_BELOW;\
}

/* round, but maps x/y == z.5 to z.0 instead of (z+1).0 */
/* note that "ceiling" breaks for numerator < 1, so special-case it */
#define round_down(x, y)   ((int)(2*(x)-(y)) <= 0 ? 0 :\
			                (ceiling((2*(x)-(y)), (2*(y)))))

/* miZeroClipLine
 *
 * returns:  1 for partially clipped line
 *          -1 for completely clipped line
 *
 */
int
miZeroClipLine(xmin, ymin, xmax, ymax,
	       new_x1, new_y1, new_x2, new_y2,
	       adx, ady,
	       pt1_clipped, pt2_clipped, axis, signdx_eq_signdy, oc1, oc2)
    int xmin, ymin, xmax, ymax;
    int *new_x1, *new_y1, *new_x2, *new_y2;
    int *pt1_clipped, *pt2_clipped;
    unsigned int adx, ady;
    int axis, oc1, oc2;
    Bool signdx_eq_signdy;
{
    int swapped = 0;
    int clipDone = 0;
    CARD32 utmp;
    int clip1, clip2;
    int x1, y1, x2, y2;
    int x1_orig, y1_orig, x2_orig, y2_orig;

    x1 = x1_orig = *new_x1;
    y1 = y1_orig = *new_y1;
    x2 = x2_orig = *new_x2;
    y2 = y2_orig = *new_y2;
    
    clip1 = 0;
    clip2 = 0;

    do
    {
        if ((oc1 & oc2) != 0)
	{
	    clipDone = -1;
	    clip1 = oc1;
	    clip2 = oc2;
	}
        else if ((oc1 | oc2) == 0) 	   /* trivial accept */
        {
	    clipDone = 1;
	    if (swapped)
	    {
	        SWAPINT_PAIR(x1, y1, x2, y2);
	        SWAPINT(oc1, oc2);
	        SWAPINT(clip1, clip2);
	    }
        }
        else			/* have to clip */
        {
	    /* only clip one point at a time */
	    if (oc1 == 0)
	    {
	        SWAPINT_PAIR(x1, y1, x2, y2);
	        SWAPINT_PAIR(x1_orig, y1_orig, x2_orig, y2_orig);
	        SWAPINT(oc1, oc2);
	        SWAPINT(clip1, clip2);
	        swapped = !swapped;
	    }
    
	    clip1 |= oc1;
	    if (oc1 & OUT_LEFT)
	    {
		if (axis == X_AXIS)
		{
		    utmp = xmin - x1_orig;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp *= ady;
			if (signdx_eq_signdy)
			    y1 = y1_orig + round(utmp, adx);
			else
			    y1 = y1_orig - round(utmp, adx);
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (x2_orig - xmin) * ady;
			if (signdx_eq_signdy)
			    y1 = y2_orig - round_down(utmp, adx);
			else
			    y1 = y2_orig + round_down(utmp, adx);
		    }
		}
		else	/* Y_AXIS */
		{
		    utmp = xmin - x1_orig;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp = ((utmp * ady) << 1) - ady;
			if (signdx_eq_signdy)
			    y1 = y1_orig + ceiling(utmp, 2*adx);
			else
			    y1 = y1_orig - (utmp / (2*adx)) - 1;
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (((x2_orig - xmin) * ady) << 1) + ady;
			if (signdx_eq_signdy)
			    y1 = y2_orig - (utmp / (2*adx));
			else
			    y1 = y2_orig + ceiling(utmp, 2*adx) - 1;
		    }
		}
		x1 = xmin;
	    }
	    else if (oc1 & OUT_ABOVE)
	    {
		if (axis == Y_AXIS)
		{
		    utmp = ymin - y1_orig;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp *= adx;
			if (signdx_eq_signdy)
			    x1 = x1_orig + round(utmp, ady);
			else
			    x1 = x1_orig - round(utmp, ady);
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (y2_orig - ymin) * adx;
			if (signdx_eq_signdy)
			    x1 = x2_orig - round_down(utmp, ady);
			else
			    x1 = x2_orig + round_down(utmp, ady);
		    }
		}
		else	/* X_AXIS */
		{
		    utmp = ymin - y1_orig;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp = ((utmp * adx) << 1) - adx;
			if (signdx_eq_signdy)
			    x1 = x1_orig + ceiling(utmp, 2*ady);
			else
			    x1 = x1_orig - (utmp / (2*ady)) - 1;
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (((y2_orig - ymin) * adx) << 1) + adx;
			if (signdx_eq_signdy)
			    x1 = x2_orig - (utmp / (2*ady));
			else
			    x1 = x2_orig + ceiling(utmp, 2*ady) - 1;
		    }
		}
		y1 = ymin;
	    }
	    else if (oc1 & OUT_RIGHT)
	    {
		if (axis == X_AXIS)
		{
		    utmp = x1_orig - xmax;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp *= ady;
			if (signdx_eq_signdy)
			    y1 = y1_orig - round_down(utmp, adx);
			else
			    y1 = y1_orig + round_down(utmp, adx);
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (xmax - x2_orig) * ady;
			if (signdx_eq_signdy)
			    y1 = y2_orig + round(utmp, adx);
			else
			    y1 = y2_orig - round(utmp, adx);
		    }
		}
		else	/* Y_AXIS */
		{
		    utmp = x1_orig - xmax;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp = ((utmp * ady) << 1) - ady;
			if (signdx_eq_signdy)
			    y1 = y1_orig - (utmp / (2*adx)) - 1;
			else
			    y1 = y1_orig + ceiling(utmp, 2*adx);
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (((xmax - x2_orig) * ady) << 1) + ady;
			if (signdx_eq_signdy)
			    y1 = y2_orig + ceiling(utmp, 2*adx) - 1;
			else
			    y1 = y2_orig - (utmp / (2*adx));
		    }
		}
		x1 = xmax;		
	    }
	    else if (oc1 & OUT_BELOW)
	    {
		if (axis == Y_AXIS)
		{
		    utmp = y1_orig - ymax;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp *= adx;
			if (signdx_eq_signdy)
			    x1 = x1_orig - round_down(utmp, ady);
			else
			    x1 = x1_orig + round_down(utmp, ady);
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (ymax - y2_orig) * adx;
			if (signdx_eq_signdy)
			    x1 = x2_orig + round(utmp, ady);
			else
			    x1 = x2_orig - round(utmp, ady);
		    }
		}
		else	/* X_AXIS */
		{
		    utmp = y1_orig - ymax;
		    if (utmp <= 32767)
		    {		/* clip using x1,y1 as a starting point */
			utmp = ((utmp * adx) << 1) - adx;
			if (signdx_eq_signdy)
			    x1 = x1_orig - (utmp / (2*ady)) - 1;
			else
			    x1 = x1_orig + ceiling(utmp, 2*ady);
		    }
		    else	/* clip using x2,y2 as a starting point */
		    {
			utmp = (((ymax - y2_orig) * adx) << 1) + adx;
			if (signdx_eq_signdy)
			    x1 = x2_orig + ceiling(utmp, 2*ady) - 1;
			else
			    x1 = x2_orig - (utmp / (2*ady));
		    }
		}
		y1 = ymax;
	    }
        }			/* else have to clip */

        oc1 = 0;
        oc2 = 0;
        MIOUTCODES(oc1, x1, y1, xmin, ymin, xmax, ymax);
        MIOUTCODES(oc2, x2, y2, xmin, ymin, xmax, ymax);

    } while (!clipDone);

    *new_x1 = x1;
    *new_y1 = y1;
    *new_x2 = x2;
    *new_y2 = y2;
    
    *pt1_clipped = clip1;
    *pt2_clipped = clip2;

    return clipDone;
}


/* Draw lineSolid, fillStyle-independent zero width lines.
 *
 * Must keep X and Y coordinates in "ints" at least until after they're
 * translated and clipped to accomodate CoordModePrevious lines with very
 * large coordinates.
 *
 * Draws the same pixels regardless of sign(dx) or sign(dy).
 *
 * Ken Whaley
 *
 */

/* largest positive value that can fit into a component of a point.
 * Assumes that the point structure is {type x, y;} where type is
 * a signed type.
 */
#define MAX_COORDINATE ((1 << (((sizeof(DDXPointRec) >> 1) << 3) - 1)) - 1)

#define MI_OUTPUT_POINT(xx, yy)\
{\
    if ( !new_span && yy == current_y)\
    {\
        if (xx < spans->x)\
	    spans->x = xx;\
	++*widths;\
    }\
    else\
    {\
        ++Nspans;\
	++spans;\
	++widths;\
	spans->x = xx;\
	spans->y = yy;\
	*widths = 1;\
	current_y = yy;\
        new_span = FALSE;\
    }\
}

void
miZeroLine(pDraw, pGC, mode, npt, pptInit)
    DrawablePtr pDraw;
    GCPtr	pGC;
    int		mode;		/* Origin or Previous */
    int		npt;		/* number of points */
    DDXPointPtr pptInit;
{
    int Nspans, current_y;
    DDXPointPtr ppt; 
    DDXPointPtr pspanInit, spans;
    int *pwidthInit, *widths, list_len;
    int xleft, ytop, xright, ybottom;
    int new_x1, new_y1, new_x2, new_y2;
    int x, y, x1, y1, x2, y2, xstart, ystart;
    int oc1, oc2;
    int result;
    int pt1_clipped, pt2_clipped = 0;
    Bool new_span;
    int signdx, signdy;
    int clipdx, clipdy;
    int width, height;
    int adx, ady;
    int e, e1, e2, e3;	/* Bresenham error terms */
    int length;		/* length of lines == # of pixels on major axis */

    xleft   = pDraw->x;
    ytop    = pDraw->y;
    xright  = pDraw->x + pDraw->width - 1;
    ybottom = pDraw->y + pDraw->height - 1;

    if (!pGC->miTranslate)
    {
	/* do everything in drawable-relative coordinates */
	xleft    = 0;
	ytop     = 0;
	xright  -= pDraw->x;
	ybottom -= pDraw->y;
    }

    /* it doesn't matter whether we're in drawable or screen coordinates,
     * FillSpans simply cannot take starting coordinates outside of the
     * range of a DDXPointRec component.
     */
    if (xright > MAX_COORDINATE)
	xright = MAX_COORDINATE;
    if (ybottom > MAX_COORDINATE)
	ybottom = MAX_COORDINATE;

    /* since we're clipping to the drawable's boundaries & coordinate
     * space boundaries, we're guaranteed that the larger of width/height
     * is the longest span we'll need to output
     */
    width = xright - xleft + 1;
    height = ybottom - ytop + 1;
    list_len = (height >= width) ? height : width;
    pspanInit = (DDXPointPtr)ALLOCATE_LOCAL(list_len * sizeof(DDXPointRec));
    pwidthInit = (int *)ALLOCATE_LOCAL(list_len * sizeof(int));
    if (!pspanInit || !pwidthInit)
	return;

    Nspans = 0;
    new_span = TRUE;
    spans  = pspanInit - 1;
    widths = pwidthInit - 1;
    ppt = pptInit;

    xstart = ppt->x;
    ystart = ppt->y;
    if (pGC->miTranslate)
    {
	xstart += pDraw->x;
	ystart += pDraw->y;
    }
    
    /* x2, y2, oc2 copied to x1, y1, oc1 at top of loop to simplify
     * iteration logic
     */
    x2 = xstart;
    y2 = ystart;
    oc2 = 0;
    MIOUTCODES(oc2, x2, y2, xleft, ytop, xright, ybottom);

    while (--npt > 0)
    {
	if (Nspans > 0)
	    (*pGC->ops->FillSpans)(pDraw, pGC, Nspans, pspanInit,
				   pwidthInit, FALSE);
	Nspans = 0;
	new_span = TRUE;
	spans  = pspanInit - 1;
	widths = pwidthInit - 1;

	x1  = x2;
	y1  = y2;
	oc1 = oc2;
	++ppt;

	x2 = ppt->x;
	y2 = ppt->y;
	if (pGC->miTranslate && (mode != CoordModePrevious))
	{
	    x2 += pDraw->x;
	    y2 += pDraw->y;
	}
	else if (mode == CoordModePrevious)
	{
	    x2 += x1;
	    y2 += y1;
	}

	oc2 = 0;
	MIOUTCODES(oc2, x2, y2, xleft, ytop, xright, ybottom);

	AbsDeltaAndSign(x2, x1, adx, signdx);
	AbsDeltaAndSign(y2, y1, ady, signdy);

	if (adx > ady)
	{
	    e1 = ady << 1;
	    e2 = e1 - (adx << 1);
	    e  = e1 - adx;
	    FIXUP_X_MAJOR_ERROR(e, signdx, signdy);
	    length  = adx;	/* don't draw endpoint in main loop */

	    new_x1 = x1;
	    new_y1 = y1;
	    new_x2 = x2;
	    new_y2 = y2;
	    pt1_clipped = 0;
	    pt2_clipped = 0;

	    if ((oc1 | oc2) != 0)
	    {
		result = miZeroClipLine(xleft, ytop, xright, ybottom,
					&new_x1, &new_y1, &new_x2, &new_y2,
					adx, ady,
					&pt1_clipped, &pt2_clipped, X_AXIS,
					signdx == signdy, oc1, oc2);
		if (result == -1)
		    continue;

		length = abs(new_x2 - new_x1);

		/* if we've clipped the endpoint, always draw the full length
		 * of the segment, because then the capstyle doesn't matter 
		 */
		if (pt2_clipped)
		    length++;

		if (pt1_clipped)
		{
		    /* must calculate new error terms */
		    clipdx = abs(new_x1 - x1);
		    clipdy = abs(new_y1 - y1);
		    e += (clipdy * e2) + ((clipdx - clipdy) * e1);
		}
	    }

	    /* draw the segment */

	    x = new_x1;
	    y = new_y1;
	    
	    e3 = e2 - e1;
	    e  = e - e1;

	    while (length--)
	    {
		MI_OUTPUT_POINT(x, y);
		e += e1;
		if (e >= 0)
		{
		    y += signdy;
		    e += e3;
		}
		x += signdx;
	    }
	}
	else    /* Y major line */
	{
	    e1 = adx << 1;
	    e2 = e1 - (ady << 1);
	    e  = e1 - ady;
	    FIXUP_Y_MAJOR_ERROR(e, signdx, signdy);
	    length  = ady;	/* don't draw endpoint in main loop */

	    new_x1 = x1;
	    new_y1 = y1;
	    new_x2 = x2;
	    new_y2 = y2;
	    pt1_clipped = 0;
	    pt2_clipped = 0;

	    if ((oc1 | oc2) != 0)
	    {
		result = miZeroClipLine(xleft, ytop, xright, ybottom,
					&new_x1, &new_y1, &new_x2, &new_y2,
					adx, ady,
					&pt1_clipped, &pt2_clipped, Y_AXIS,
					signdx == signdy, oc1, oc2);
		if (result == -1)
		    continue;

		length = abs(new_y2 - new_y1);

		/* if we've clipped the endpoint, always draw the full length
		 * of the segment, because then the capstyle doesn't matter 
		 */
		if (pt2_clipped)
		    length++;

		if (pt1_clipped)
		{
		    /* must calculate new error terms */
		    clipdx = abs(new_x1 - x1);
		    clipdy = abs(new_y1 - y1);
		    e += (clipdx * e2) + ((clipdy - clipdx) * e1);
		}
	    }

	    /* draw the segment */

	    x = new_x1;
	    y = new_y1;

	    e3 = e2 - e1;
	    e  = e - e1;

	    while (length--)
	    {
		MI_OUTPUT_POINT(x, y);
		e += e1;
		if (e >= 0)
		{
		    x += signdx;
		    e += e3;
		}
		y += signdy;
	    }
	}
    }

    /* only do the capnotlast check on the last segment
     * and only if the endpoint wasn't clipped.  And then, if the last
     * point is the same as the first point, do not draw it, unless the
     * line is degenerate
     */
    if ( (! pt2_clipped) && (pGC->capStyle != CapNotLast) &&
		(((xstart != x2) || (ystart != y2)) || (ppt == pptInit + 1)))
    {
	MI_OUTPUT_POINT(x, y);
    }    

    if (Nspans > 0)
	(*pGC->ops->FillSpans)(pDraw, pGC, Nspans, pspanInit,
			       pwidthInit, FALSE);

    DEALLOCATE_LOCAL(pwidthInit);
    DEALLOCATE_LOCAL(pspanInit);
}

void
miZeroDashLine(dst, pgc, mode, nptInit, pptInit)
DrawablePtr dst;
GCPtr pgc;
int mode;
int nptInit;		/* number of points in polyline */
DDXPointRec *pptInit;	/* points in the polyline */
{
    /* XXX kludge until real zero-width dash code is written */
    pgc->lineWidth = 1;
    miWideDash (dst, pgc, mode, nptInit, pptInit);
    pgc->lineWidth = 0;
}
