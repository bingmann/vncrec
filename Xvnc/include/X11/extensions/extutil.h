/*
 * $XConsortium: extutil.h,v 1.13 94/04/17 20:11:19 rws Exp $
 *
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
 *
 * Author:  Jim Fulton, MIT X Consortium
 * 
 *                     Xlib Extension-Writing Utilities
 *
 * This package contains utilities for writing the client API for various
 * protocol extensions.  THESE INTERFACES ARE NOT PART OF THE X STANDARD AND
 * ARE SUBJECT TO CHANGE!
 */

#ifndef _EXTUTIL_H_
#define _EXTUTIL_H_

/*
 * We need to keep a list of open displays since the Xlib display list isn't
 * public.  We also have to per-display info in a separate block since it isn't
 * stored directly in the Display structure.
 */
typedef struct _XExtDisplayInfo {
    struct _XExtDisplayInfo *next;	/* keep a linked list */
    Display *display;			/* which display this is */
    XExtCodes *codes;			/* the extension protocol codes */
    XPointer data;			/* extra data for extension to use */
} XExtDisplayInfo;

typedef struct _XExtensionInfo {
    XExtDisplayInfo *head;		/* start of list */
    XExtDisplayInfo *cur;		/* most recently used */
    int ndisplays;			/* number of displays */
} XExtensionInfo;

typedef struct _XExtensionHooks {
    int (*create_gc)();
    int (*copy_gc)();
    int (*flush_gc)();
    int (*free_gc)();
    int (*create_font)();
    int (*free_font)();
    int (*close_display)();
    Bool (*wire_to_event)();
    Status (*event_to_wire)();
    int (*error)();
    char *(*error_string)();
} XExtensionHooks;

extern XExtensionInfo *XextCreateExtension();
extern void XextDestroyExtension();
extern XExtDisplayInfo *XextAddDisplay();
extern int XextRemoveDisplay();
extern XExtDisplayInfo *XextFindDisplay();

#define XextHasExtension(i) ((i) && ((i)->codes))
#define XextCheckExtension(dpy,i,name,val) \
  if (!XextHasExtension(i)) { XMissingExtension (dpy, name); return val; }
#define XextSimpleCheckExtension(dpy,i,name) \
  if (!XextHasExtension(i)) { XMissingExtension (dpy, name); return; }


/*
 * helper macros to generate code that is common to all extensions; caller
 * should prefix it with static if extension source is in one file; this
 * could be a utility function, but have to stack 6 unused arguments for 
 * something that is called many, many times would be bad.
 */
#define XEXT_GENERATE_FIND_DISPLAY(proc,extinfo,extname,hooks,nev,data) \
XExtDisplayInfo *proc (dpy) \
    register Display *dpy; \
{ \
    XExtDisplayInfo *dpyinfo; \
    if (!extinfo) { if (!(extinfo = XextCreateExtension())) return NULL; } \
    if (!(dpyinfo = XextFindDisplay (extinfo, dpy))) \
      dpyinfo = XextAddDisplay (extinfo,dpy,extname,hooks,nev,data); \
    return dpyinfo; \
}

#define XEXT_GENERATE_CLOSE_DISPLAY(proc,extinfo) \
int proc (dpy, codes) \
    Display *dpy; \
    XExtCodes *codes; \
{ \
    return XextRemoveDisplay (extinfo, dpy); \
}

#define XEXT_GENERATE_ERROR_STRING(proc,extname,nerr,errl) \
char *proc (dpy, code, codes, buf, n) \
    Display  *dpy; \
    int code; \
    XExtCodes *codes; \
    char *buf; \
    int n; \
{  \
    code -= codes->first_error;  \
    if (code >= 0 && code < nerr) { \
	char tmp[256]; \
	sprintf (tmp, "%s.%d", extname, code); \
	XGetErrorDatabaseText (dpy, "XProtoError", tmp, errl[code], buf, n); \
	return buf; \
    } \
    return (char *)0; \
}

#endif
