/*
 * kbdptr.c - deal with keyboard and pointer device over TCP & UDP.
 *
 *
 */

/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
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

#include "X11/X.h"
#define NEED_EVENTS
#include "X11/Xproto.h"
#include "inputstr.h"
#include <X11/keysym.h>
#include <Xatom.h>
#include "rfb.h"

extern WindowPtr *WindowTable; /* Why isn't this in a header file? */

#define KEY_IS_PRESSED(keycode) \
    (kbdDevice->key->down[(keycode) >> 3] & (1 << ((keycode) & 7)))



static DeviceIntPtr kbdDevice;

unsigned char ptrAcceleration = 50;

#define MIN_KEY_CODE		8
#define MAX_KEY_CODE		255
#define NO_OF_KEYS		(MAX_KEY_CODE - MIN_KEY_CODE + 1)
#define GLYPHS_PER_KEY		2

static KeySym kbdMap[] = {

    /* Modifiers */

    XK_Control_L,	NoSymbol,
#define CONTROL_L_KEY_CODE	MIN_KEY_CODE

    XK_Control_R,	NoSymbol,
#define CONTROL_R_KEY_CODE	(MIN_KEY_CODE + 1)

    XK_Shift_L,		NoSymbol,
#define SHIFT_L_KEY_CODE	(MIN_KEY_CODE + 2)

    XK_Shift_R,		NoSymbol,
#define SHIFT_R_KEY_CODE	(MIN_KEY_CODE + 3)

    XK_Meta_L,		NoSymbol,
#define META_L_KEY_CODE		(MIN_KEY_CODE + 4)

    XK_Meta_R,		NoSymbol,
#define META_R_KEY_CODE		(MIN_KEY_CODE + 5)

    XK_Alt_L,		NoSymbol,
#define ALT_L_KEY_CODE		(MIN_KEY_CODE + 6)

    XK_Alt_R,		NoSymbol,
#define ALT_R_KEY_CODE		(MIN_KEY_CODE + 7)

    /* Standard US keyboard */

    XK_space,		NoSymbol,
    XK_0,		XK_parenright,
    XK_1,		XK_exclam,
    XK_2,		XK_at,
    XK_3,		XK_numbersign,
    XK_4,		XK_dollar,
    XK_5,		XK_percent,
    XK_6,		XK_asciicircum,
    XK_7,		XK_ampersand,
    XK_8,		XK_asterisk,
    XK_9,		XK_parenleft,

    XK_minus,		XK_underscore,
    XK_equal,		XK_plus,
    XK_bracketleft,	XK_braceleft,
    XK_bracketright,	XK_braceright,
    XK_semicolon,	XK_colon,
    XK_apostrophe,	XK_quotedbl,
    XK_grave,		XK_asciitilde,
    XK_comma,		XK_less,
    XK_period,		XK_greater,
    XK_slash,		XK_question,
    XK_backslash,	XK_bar,

    XK_a,		XK_A,
    XK_b,		XK_B,
    XK_c,		XK_C,
    XK_d,		XK_D,
    XK_e,		XK_E,
    XK_f,		XK_F,
    XK_g,		XK_G,
    XK_h,		XK_H,
    XK_i,		XK_I,
    XK_j,		XK_J,
    XK_k,		XK_K,
    XK_l,		XK_L,
    XK_m,		XK_M,
    XK_n,		XK_N,
    XK_o,		XK_O,
    XK_p,		XK_P,
    XK_q,		XK_Q,
    XK_r,		XK_R,
    XK_s,		XK_S,
    XK_t,		XK_T,
    XK_u,		XK_U,
    XK_v,		XK_V,
    XK_w,		XK_W,
    XK_x,		XK_X,
    XK_y,		XK_Y,
    XK_z,		XK_Z,

    /* Other useful keys */

    XK_BackSpace,	NoSymbol,
    XK_Return,		NoSymbol,
    XK_Tab,		NoSymbol,
    XK_Escape,		NoSymbol,
    XK_Delete,		NoSymbol,

    XK_Home,		NoSymbol,
    XK_End,		NoSymbol,
    XK_Page_Up,		NoSymbol,
    XK_Page_Down,	NoSymbol,
    XK_Up,		NoSymbol,
    XK_Down,		NoSymbol,
    XK_Left,		NoSymbol,
    XK_Right,		NoSymbol,

    XK_F1,		NoSymbol,
    XK_F2,		NoSymbol,
    XK_F3,		NoSymbol,
    XK_F4,		NoSymbol,
    XK_F5,		NoSymbol,
    XK_F6,		NoSymbol,
    XK_F7,		NoSymbol,
    XK_F8,		NoSymbol,
    XK_F9,		NoSymbol,
    XK_F10,		NoSymbol,
    XK_F11,		NoSymbol,
    XK_F12,		NoSymbol,

    /* Plus blank ones which can be filled in using xmodmap */

};

#define N_PREDEFINED_KEYS (sizeof(kbdMap) / (sizeof(KeySym) * GLYPHS_PER_KEY))


void
PtrDeviceInit()
{
}


void
KbdDeviceInit(pDevice, pKeySyms, pModMap)
    DeviceIntPtr pDevice;
    KeySymsPtr pKeySyms;
    CARD8 *pModMap;
{
    int i;

    kbdDevice = pDevice;

    for (i = 0; i < MAP_LENGTH; i++)
	pModMap[i] = NoSymbol;

    pModMap[CONTROL_L_KEY_CODE] = ControlMask;
    pModMap[CONTROL_R_KEY_CODE] = ControlMask;
    pModMap[SHIFT_L_KEY_CODE] = ShiftMask;
    pModMap[SHIFT_R_KEY_CODE] = ShiftMask;
    pModMap[META_L_KEY_CODE] = Mod1Mask;
    pModMap[META_R_KEY_CODE] = Mod1Mask;
    pModMap[ALT_L_KEY_CODE] = Mod1Mask;
    pModMap[ALT_R_KEY_CODE] = Mod1Mask;

    pKeySyms->minKeyCode = MIN_KEY_CODE;
    pKeySyms->maxKeyCode = MAX_KEY_CODE;
    pKeySyms->mapWidth = GLYPHS_PER_KEY;

    pKeySyms->map = (KeySym *)xalloc(sizeof(KeySym)
				     * MAP_LENGTH * GLYPHS_PER_KEY);

    if (!pKeySyms->map) {
	rfbLog("xalloc failed\n");
	exit(1);
    }

    for (i = 0; i < MAP_LENGTH * GLYPHS_PER_KEY; i++)
	pKeySyms->map[i] = NoSymbol;

    for (i = 0; i < N_PREDEFINED_KEYS * GLYPHS_PER_KEY; i++) {
	pKeySyms->map[i] = kbdMap[i];
    }
}



void
KbdDeviceOn()
{
}


void
KbdDeviceOff()
{
}


void
PtrDeviceOn(pDev)
    DeviceIntPtr pDev;
{
    ptrAcceleration = (char)pDev->ptrfeed->ctrl.num;
}


void
PtrDeviceOff()
{
}


void
PtrDeviceControl(dev, ctrl)
    DevicePtr dev;
    PtrCtrl *ctrl;
{
    ptrAcceleration = (char)ctrl->num;

    if (udpSockConnected) {
	if (write(udpSock, &ptrAcceleration, 1) <= 0) {
	    rfbLogPerror("PtrDeviceControl: UDP input: write");
	    rfbDisconnectUDPSock();
	}
    }
}


void
KbdAddEvent(down, keySym, cl)
    Bool down;
    KeySym keySym;
    rfbClientPtr cl;
{
    xEvent ev, fake;
    KeySymsPtr keySyms = &kbdDevice->key->curKeySyms;
    int i;
    Bool foundKey = FALSE;
    int freeKey = -1;
    unsigned long time;
    Bool fakeShiftPress = FALSE;
    Bool fakeShiftLRelease = FALSE;
    Bool fakeShiftRRelease = FALSE;
    Bool shiftedKey;

#ifdef CORBA
    if (cl) {
	CARD32 clientId = cl->sock;
	ChangeWindowProperty(WindowTable[0], VNC_LAST_CLIENT_ID, XA_INTEGER,
			     32, PropModeReplace, 1, (pointer)&clientId, TRUE);
    }
#endif

    if (down) {
	ev.u.u.type = KeyPress;
    } else {
	ev.u.u.type = KeyRelease;
    }

    for (i = 0; i < N_PREDEFINED_KEYS * GLYPHS_PER_KEY; i++) {
	if (keySym == kbdMap[i]) {
	    foundKey = TRUE;
	    break;
	}
    }

    if (!foundKey) {
	for (i = 0; i < NO_OF_KEYS * keySyms->mapWidth; i++) {
	    if (keySym == keySyms->map[i]) {
		foundKey = TRUE;
		break;
	    }
	    if ((freeKey == -1) && (keySyms->map[i] == NoSymbol)
		&& (i % keySyms->mapWidth) == 0)
	    {
		freeKey = i;
	    }
	}
    }

    if (!foundKey) {
	if (freeKey == -1) {
	    rfbLog("KbdAddEvent: ignoring KeySym 0x%x - no free KeyCodes\n",
		   keySym);
	    return;
	}

	i = freeKey;
	keySyms->map[i] = keySym;
	SendMappingNotify(MappingKeyboard,
			  MIN_KEY_CODE + (i / keySyms->mapWidth), 1,
			  serverClient);

	rfbLog("KbdAddEvent: unknown KeySym 0x%x - allocating KeyCode %d\n",
	       keySym, MIN_KEY_CODE + (i / keySyms->mapWidth));
    }

    time = GetTimeInMillis();

    shiftedKey = ((i % keySyms->mapWidth) == 1);

    if (down && (keySym > 32) && (keySym < 127)) {
	if (shiftedKey && !(kbdDevice->key->state & ShiftMask)) {
	    fakeShiftPress = TRUE;
	    fake.u.u.type = KeyPress;
	    fake.u.u.detail = SHIFT_L_KEY_CODE;
	    fake.u.keyButtonPointer.time = time;
	    mieqEnqueue(&fake);
	}
	if (!shiftedKey && (kbdDevice->key->state & ShiftMask)) {
	    if (KEY_IS_PRESSED(SHIFT_L_KEY_CODE)) {
		fakeShiftLRelease = TRUE;
		fake.u.u.type = KeyRelease;
		fake.u.u.detail = SHIFT_L_KEY_CODE;
		fake.u.keyButtonPointer.time = time;
		mieqEnqueue(&fake);
	    }
	    if (KEY_IS_PRESSED(SHIFT_R_KEY_CODE)) {
		fakeShiftRRelease = TRUE;
		fake.u.u.type = KeyRelease;
		fake.u.u.detail = SHIFT_R_KEY_CODE;
		fake.u.keyButtonPointer.time = time;
		mieqEnqueue(&fake);
	    }
	}
    }

    ev.u.u.detail = MIN_KEY_CODE + (i / keySyms->mapWidth);
    ev.u.keyButtonPointer.time = time;
    mieqEnqueue(&ev);

    if (fakeShiftPress) {
	fake.u.u.type = KeyRelease;
	fake.u.u.detail = SHIFT_L_KEY_CODE;
	fake.u.keyButtonPointer.time = time;
	mieqEnqueue(&fake);
    }
    if (fakeShiftLRelease) {
	fake.u.u.type = KeyPress;
	fake.u.u.detail = SHIFT_L_KEY_CODE;
	fake.u.keyButtonPointer.time = time;
	mieqEnqueue(&fake);
    }
    if (fakeShiftRRelease) {
	fake.u.u.type = KeyPress;
	fake.u.u.detail = SHIFT_R_KEY_CODE;
	fake.u.keyButtonPointer.time = time;
	mieqEnqueue(&fake);
    }
}

void
PtrAddEvent(buttonMask, x, y, cl)
    int buttonMask;
    int x;
    int y;
    rfbClientPtr cl;
{
    xEvent ev;
    int i;
    unsigned long time;
    static int oldButtonMask = 0;

#ifdef CORBA
    if (cl) {
	CARD32 clientId = cl->sock;
	ChangeWindowProperty(WindowTable[0], VNC_LAST_CLIENT_ID, XA_INTEGER,
			     32, PropModeReplace, 1, (pointer)&clientId, TRUE);
    }
#endif

    time = GetTimeInMillis();

    miPointerAbsoluteCursor(x, y, time);

    for (i = 0; i < 5; i++) {
	if ((buttonMask ^ oldButtonMask) & (1<<i)) {
	    if (buttonMask & (1<<i)) {
		ev.u.u.type = ButtonPress;
		ev.u.u.detail = i + 1;
		ev.u.keyButtonPointer.time = time;
		mieqEnqueue(&ev);
	    } else {
		ev.u.u.type = ButtonRelease;
		ev.u.u.detail = i + 1;
		ev.u.keyButtonPointer.time = time;
		mieqEnqueue(&ev);
	    }
	}
    }

    oldButtonMask = buttonMask;
}

void
KbdReleaseAllKeys()
{
    int i, j;
    xEvent ev;
    unsigned long time = GetTimeInMillis();

    for (i = 0; i < DOWN_LENGTH; i++) {
	if (kbdDevice->key->down[i] != 0) {
	    for (j = 0; j < 8; j++) {
		if (kbdDevice->key->down[i] & (1 << j)) {
		    ev.u.u.type = KeyRelease;
		    ev.u.u.detail = (i << 3) | j;
		    ev.u.keyButtonPointer.time = time;
		    mieqEnqueue(&ev);
		}
	    }
	}
    }
}
