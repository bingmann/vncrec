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
/* $XConsortium: miinitext.c,v 1.32 94/04/17 20:27:38 rws Exp $ */

#include "misc.h"

#ifdef NOPEXEXT /* sleaze for Solaris cpp building XsunMono */
#undef PEXEXT
#endif

extern Bool noTestExtensions;
#ifdef XKB
extern Bool noXkbExtension;
#endif

#ifdef BEZIER
extern void BezierExtensionInit();
#endif
#ifdef XTESTEXT1
extern void XTestExtension1Init();
#endif
#ifdef SHAPE
extern void ShapeExtensionInit();
#endif
#ifdef MITSHM
extern void ShmExtensionInit();
#endif
#ifdef PEXEXT
extern void PexExtensionInit();
#endif
#ifdef MULTIBUFFER
extern void MultibufferExtensionInit();
#endif
#ifdef XINPUT
extern void XInputExtensionInit();
#endif
#ifdef XTEST
extern void XTestExtensionInit();
#endif
#ifdef BIGREQS
extern void BigReqExtensionInit();
#endif
#ifdef MITMISC
extern void MITMiscExtensionInit();
#endif
#ifdef XIDLE
extern void XIdleExtensionInit();
#endif
#ifdef XTRAP
extern void DEC_XTRAPInit();
#endif
#ifdef SCREENSAVER
extern void ScreenSaverExtensionInit ();
#endif
#ifdef XV
extern void XvExtensionInit();
#endif
#ifdef XIE
extern void XieInit();
#endif
#ifdef XSYNC
extern void SyncExtensionInit();
#endif
#ifdef XKB
extern void XkbExtensionInit();
#endif
#ifdef XCMISC
extern void XCMiscExtensionInit();
#endif
#ifdef XRECORD
extern void XRecordExtensionInit();
#endif
#ifdef LBX
extern void     LbxExtensionInit();
#endif

/*ARGSUSED*/
void
InitExtensions(argc, argv)
    int		argc;
    char	*argv[];
{
#ifdef BEZIER
    BezierExtensionInit();
#endif
#ifdef XTESTEXT1
    if (!noTestExtensions) XTestExtension1Init();
#endif
#ifdef SHAPE
    ShapeExtensionInit();
#endif
#ifdef MITSHM
    ShmExtensionInit();
#endif
#ifdef PEXEXT
    PexExtensionInit();
#endif
#ifdef MULTIBUFFER
    MultibufferExtensionInit();
#endif
#ifdef XINPUT
    XInputExtensionInit();
#endif
#ifdef XTEST
    if (!noTestExtensions) XTestExtensionInit();
#endif
#ifdef BIGREQS
    BigReqExtensionInit();
#endif
#ifdef MITMISC
    MITMiscExtensionInit();
#endif
#ifdef XIDLE
    XIdleExtensionInit();
#endif
#ifdef XTRAP
    if (!noTestExtensions) DEC_XTRAPInit();
#endif
#ifdef SCREENSAVER
    ScreenSaverExtensionInit ();
#endif
#ifdef XV
    XvExtensionInit();
#endif
#ifdef XIE
    XieInit();
#endif
#ifdef XSYNC
    SyncExtensionInit();
#endif
#ifdef XKB
    if (!noXkbExtension) XkbExtensionInit();
#endif
#ifdef XCMISC
    XCMiscExtensionInit();
#endif
#ifdef XRECORD
    if (!noTestExtensions) XRecordExtensionInit(); 
#endif
#ifdef LBX
    LbxExtensionInit();
#endif
}
