/* $XConsortium: miline.h,v 1.3 94/03/31 14:05:07 dpw Exp $ */

/*

Copyright (c) 1994  X Consortium

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

*/

#ifndef MILINE_H

/* Stuff needed for drawing thin (zero width) lines */

#define X_AXIS	0
#define Y_AXIS	1

#define OUT_LEFT  0x08
#define OUT_RIGHT 0x04
#define OUT_ABOVE 0x02
#define OUT_BELOW 0x01

#define OUTCODES(_result, _x, _y, _pbox) \
    if	    ( (_x) <  (_pbox)->x1) (_result) |= OUT_LEFT; \
    else if ( (_x) >= (_pbox)->x2) (_result) |= OUT_RIGHT; \
    if	    ( (_y) <  (_pbox)->y1) (_result) |= OUT_ABOVE; \
    else if ( (_y) >= (_pbox)->y2) (_result) |= OUT_BELOW;

#define round(dividend, divisor) \
( (((dividend)<<1) + (divisor)) / ((divisor)<<1) )

#define ceiling(m,n)  (((m)-1)/(n) + 1)

#define SignTimes(sign, n) \
    ( ((sign)<0) ? -(n) : (n) )

#define SWAPINT(i, j) \
{  register int _t = i;  i = j;  j = _t; }

#define SWAPPT(i, j) \
{  DDXPointRec _t; _t = i;  i = j; j = _t; }

#define SWAPINT_PAIR(x1, y1, x2, y2)\
{   int t = x1;  x1 = x2;  x2 = t;\
        t = y1;  y1 = y2;  y2 = t;\
}

#define AbsDeltaAndSign(_p2, _p1, _absdelta, _sign) \
    (_sign) = 1; \
    (_absdelta) = (_p2) - (_p1); \
    if ( (_absdelta) < 0) { (_absdelta) = -(_absdelta); (_sign) = -1; }

#ifndef FIXUP_X_MAJOR_ERROR
#define FIXUP_X_MAJOR_ERROR(_e, _signdx, _signdy) \
    (_e) -= ( (_signdx) < 0)
#endif

#ifndef FIXUP_Y_MAJOR_ERROR
#define FIXUP_Y_MAJOR_ERROR(_e, _signdx, _signdy) \
    (_e) -= ( (_signdy) < 0)
#endif

extern int miZeroClipLine(
#if NeedFunctionPrototypes
    int /*xmin*/,
    int /*ymin*/,
    int /*xmax*/,
    int /*ymax*/,
    int * /*new_x1*/,
    int * /*new_y1*/,
    int * /*new_x2*/,
    int * /*new_y2*/,
    unsigned int /*adx*/,
    unsigned int /*ady*/,
    int * /*pt1_clipped*/,
    int * /*pt2_clipped*/,
    int /*axis*/,
    Bool /*signdx_eq_signdy*/,
    int /*oc1*/,
    int /*oc2*/
#endif
);

#endif /* MILINE_H */

