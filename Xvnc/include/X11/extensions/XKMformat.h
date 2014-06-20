/* $XConsortium: XKMformat.h,v 1.4 94/04/08 15:08:20 erik Exp $ */
/************************************************************
 Copyright (c) 1994 by Silicon Graphics Computer Systems, Inc.

 Permission to use, copy, modify, and distribute this
 software and its documentation for any purpose and without
 fee is hereby granted, provided that the above copyright
 notice appear in all copies and that both that copyright
 notice and this permission notice appear in supporting
 documentation, and that the name of Silicon Graphics not be 
 used in advertising or publicity pertaining to distribution 
 of the software without specific prior written permission.
 Silicon Graphics makes no representation about the suitability 
 of this software for any purpose. It is provided "as is"
 without any express or implied warranty.
 
 SILICON GRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS 
 SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY 
 AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL SILICON
 GRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
 DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, 
 DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE 
 OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION  WITH
 THE USE OR PERFORMANCE OF THIS SOFTWARE.

 ********************************************************/
#ifndef XKMFORMAT_H
#define	XKMFORMAT_H 1

#include <X11/extensions/XKBproto.h>
#include "XKM.h"

#define	XkmMSB(s)	((((xkmSectionInfo *)(s))->format)&MSBFirst)

typedef	struct _xkmFileInfo {
	CARD8		type;
	CARD8		min_kc;
	CARD8		max_kc;
	CARD8		num_toc;
} xkmFileInfo;
#define	sz_xkmFileInfo	4

typedef	struct _xkmSectionInfo {
	CARD16		type B16;
	CARD16		format B16;
	CARD16		size B16;
	CARD16		offset B16;
} xkmSectionInfo;
#define	sz_xkmSectionInfo	8

typedef struct _xkmKeyTypeDesc {
	CARD8		realMods;
	CARD8		groupWidth;
	CARD16		virtualMods B16;
	CARD8		nMapEntries;
	CARD8		nLevelNames;
	CARD8		preserve;
	CARD8		pad;
} xkmKeyTypeDesc;
#define	sz_xkmKeyTypeDesc	8

typedef struct _xkmKTMapEntryDesc {
	CARD8		level;
	CARD8		realMods;
	CARD16		virtualMods B16;
} xkmKTMapEntryDesc;
#define	sz_xkmKTMapEntryDesc	4

typedef struct _xkmKTPreserveDesc {
	CARD8		realMods;
	CARD8		pad;
	CARD16		virtualMods B16;
} xkmKTPreserveDesc;
#define	sz_xkmKTPreserveDesc	4

typedef struct _xkmSymInterpretDesc {
	CARD32		sym B32;
	CARD8		mods;
	CARD8		match;
	CARD8		virtualMod;
	CARD8		flags;
	CARD8		actionType;
	CARD8		actionData[7];
} xkmSymInterpretDesc;
#define	sz_xkmSymInterpretDesc	16

typedef struct _xkmBehaviorDesc {
	CARD8		type;
	CARD8		data;
	CARD16		pad;
} xkmBehaviorDesc;
#define	sz_xkmBehaviorDesc	4

typedef struct _xkmActionDesc {
	CARD8		type;
	CARD8		data[7];
} xkmActionDesc;
#define	sz_xkmActionDesc	8

#define	XkmKeyHasType		(1<<3)
#define	XkmKeyHasActions	(1<<4)
#define	XkmKeyHasBehavior	(1<<5)
#define	XkmRepeatingKey		(1<<6)
#define	XkmNonRepeatingKey	(1<<7)

typedef struct _xkmKeySymMapDesc {
	CARD8		group_width;
	CARD8		num_groups;
	CARD8		modifier_map;
	CARD8		flags;
} xkmKeySymMapDesc;
#define sz_xkmKeySymMapDesc	4

#define	XkmIndicatorUnbound	0
typedef struct _xkmIndicatorMapDesc {
	CARD8		indicator;
	CARD8		flags;
	CARD8		which_mods;
	CARD8		real_mods;
	CARD16		vmods;
	CARD8		which_groups;
	CARD8		groups;
	CARD32		ctrls;
} xkmIndicatorMapDesc;
#define sz_xkmIndicatorMapDesc	12

#endif /* XKMFORMAT_H */
