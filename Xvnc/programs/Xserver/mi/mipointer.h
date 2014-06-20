/*
 * mipointer.h
 *
 */

/* $XConsortium: mipointer.h,v 5.7 94/04/17 20:27:40 dpw Exp $ */

/*

Copyright (c) 1989  X Consortium

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

typedef struct _miPointerSpriteFuncRec {
    Bool	(*RealizeCursor)();	/* pScreen, pCursor */
    Bool	(*UnrealizeCursor)();	/* pScreen, pCursor */
    void	(*SetCursor)();		/* pScreen, pCursor, x, y */
    void	(*MoveCursor)();	/* pScreen, x, y */
} miPointerSpriteFuncRec, *miPointerSpriteFuncPtr;

typedef struct _miPointerScreenFuncRec {
    Bool	(*CursorOffScreen)();	/* ppScreen, px, py */
    void	(*CrossScreen)();	/* pScreen, entering */
    void	(*WarpCursor)();	/* pScreen, x, y */
    void	(*EnqueueEvent)();	/* xEvent */
    void	(*NewEventScreen)();	/* pScreen */
} miPointerScreenFuncRec, *miPointerScreenFuncPtr;

extern Bool miDCInitialize(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    miPointerScreenFuncPtr /*screenFuncs*/
#endif
);

extern Bool miPointerInitialize(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    miPointerSpriteFuncPtr /*spriteFuncs*/,
    miPointerScreenFuncPtr /*screenFuncs*/,
    Bool /*waitForUpdate*/
#endif
);

extern void miPointerWarpCursor(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    int /*x*/,
    int /*y*/
#endif
);

extern int miPointerGetMotionBufferSize(
#if NeedFunctionPrototypes
    void
#endif
);

extern int miPointerGetMotionEvents(
#if NeedFunctionPrototypes
    DeviceIntPtr /*pPtr*/,
    xTimecoord * /*coords*/,
    unsigned long /*start*/,
    unsigned long /*stop*/,
    ScreenPtr /*pScreen*/
#endif
);

extern void miPointerUpdate(
#if NeedFunctionPrototypes
    void
#endif
);

extern void miPointerDeltaCursor(
#if NeedFunctionPrototypes
    int /*dx*/,
    int /*dy*/,
    unsigned long /*time*/
#endif
);

extern void miPointerAbsoluteCursor(
#if NeedFunctionPrototypes
    int /*x*/,
    int /*y*/,
    unsigned long /*time*/
#endif
);

extern void miPointerPosition(
#if NeedFunctionPrototypes
    int * /*x*/,
    int * /*y*/
#endif
);

extern void miRegisterPointerDevice(
#if NeedFunctionPrototypes
    ScreenPtr /*pScreen*/,
    DevicePtr /*pDevice*/
#endif
);
