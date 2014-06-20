/* $XConsortium: extension.h,v 1.8 94/04/17 20:25:41 dpw Exp $ */
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
#ifndef EXTENSION_H
#define EXTENSION_H 

#define GetGCAndDrawableAndValidate(gcID, pGC, drawID, pDraw, client)\
    if ((client->lastDrawableID != drawID) || (client->lastGCID != gcID))\
    {\
        if (client->lastDrawableID != drawID)\
    	    pDraw = (DrawablePtr)LookupIDByClass(drawID, RC_DRAWABLE);\
        else\
	    pDraw = client->lastDrawable;\
        if (client->lastGCID != gcID)\
	    pGC = (GC *)LookupIDByType(gcID, RT_GC);\
        else\
            pGC = client->lastGC;\
	if (pDraw && pGC)\
	{\
	    if ((pDraw->type == UNDRAWABLE_WINDOW) ||\
		(pGC->depth != pDraw->depth) ||\
		(pGC->pScreen != pDraw->pScreen))\
		return (BadMatch);\
	    client->lastDrawable = pDraw;\
	    client->lastDrawableID = drawID;\
            client->lastGC = pGC;\
            client->lastGCID = gcID;\
	}\
    }\
    else\
    {\
        pGC = client->lastGC;\
        pDraw = client->lastDrawable;\
    }\
    if (!pDraw)\
    {\
        client->errorValue = drawID; \
	return (BadDrawable);\
    }\
    if (!pGC)\
    {\
        client->errorValue = gcID;\
        return (BadGC);\
    }\
    if (pGC->serialNumber != pDraw->serialNumber)\
	ValidateGC(pDraw, pGC);

extern unsigned short StandardMinorOpcode(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern unsigned short MinorOpcodeOfRequest(
#if NeedFunctionPrototypes
    ClientPtr /*client*/
#endif
);

extern void InitExtensions(
#if NeedFunctionPrototypes
    int argc,
    char **argv
#endif
);

extern void CloseDownExtensions(
#if NeedFunctionPrototypes
    void
#endif
);

#endif /* EXTENSION_H */
