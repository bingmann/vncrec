/*
 * Keyboard stuff
 */

#include <vncviewer.h>
#include <vgakeyboard.h>
#include <X11/keysym.h>

#define NR_KEYS 128
static int KeyState [NR_KEYS]; 

enum { mod_shift, mod_lock, mod_control, mod_mod1, mod_mod2, mod_mod3,
       mod_mod4, mod_mod5, num_modifiers };

static int state[num_modifiers] = {0,};

static KeySym keynorm[NR_KEYS], keyshift[NR_KEYS];
static KeySym keycode2keysym (int);
static void keyboard_handler (int, int);

/*
 * for debugging; this indicates whether the normal or the shifted
 * keysym is selected by keycode2keysym. check keytest.c.
 */
static int shifted;

Bool
svncKeyboardInit(void)
{
    int i;

    if (keyboard_init ()) {
	fprintf (stderr, "Couldn't open keyboard !\n");
	return False;
    }
    keyboard_translatekeys (DONT_CATCH_CTRLC);
    keyboard_seteventhandler (keyboard_handler);

    for (i = 0; i < NR_KEYS; i++)
        KeyState[i] = -1;

/*
 * initialize keynorm[] and keyshift[] (keycode -> keysym) here.
 */
#   include "keys.h"

    return True;
}

static void keyboard_handler (int, int);


/*
 * HandleKeyboardEvent 
 */
void HandleKeyboardEvent (void)
{
    int key;
    KeySym ks;

    for (key = 0; key < NR_KEYS; key++) {
        if (KeyState[key] != -1) {
            ks = keycode2keysym (key);
            if (ks == XK_BackSpace && state[mod_control] && state[mod_mod1]) {
                ShutdownSvga();
                exit (0);
            }
            SendKeyEvent (keycode2keysym(key), (KeyState[key] == KEY_EVENTPRESS));
            KeyState[key] = -1;
        }
    }
}

static KeySym 
keycode2keysym(int code)
{
    int m;

    /* This is by executing xmodmap -pm and -pke */
    switch(keynorm[code]) {
        case XK_Shift_L:	m = mod_shift;		break;
        case XK_Shift_R:	m = mod_shift;		break;
        case XK_Caps_Lock:	m = mod_lock;		break;
        case XK_Control_L:	m = mod_control;	break;
        case XK_Control_R:	m = mod_control;	break;
        case XK_Alt_L:		m = mod_mod1;		break;
        case XK_Alt_R:		m = mod_mod1;		break;
        case XK_Num_Lock:	m = mod_mod2;		break;
        case XK_Scroll_Lock:	m = mod_mod5;		break;
        default:		m = -1;			break;
    }

    if (KeyState[code] == KEY_EVENTPRESS) {
        switch (m) {
            case mod_lock:              /* caps */
            case mod_mod2:              /* num */
                if (!(state[m] & 2)) { state[m] ^= 1 ; state[m] |= 2; }
                break;
            case mod_control:           /* control */
            case mod_shift:             /* shift */  
            case mod_mod1:              /* alt */    
                state[m] = 1;
                break;
        }
    } else {
        switch (m) {
            case mod_lock:              /* caps */
            case mod_mod2:              /* num */
                state[m] &= ~2;
                break;
            case mod_control:           /* control */
            case mod_shift:             /* shift */
            case mod_mod1:              /* alt */
                state[m] = 0;
                break;
        }
    }
    /*
     * WOW! I LIKE THIS CODE!!!
     *
     * funda: keynorm[] and keyshift[] give the normal and shifted
     * KeySyms of a keycode respectively. This shifted sense is reversed
     * by caps lock for a-z and numlock for keypad keys 0-9 and dot.
     *
     * Hence  ((numlock & keypad) OR (caps & a-z)) XOR shift
     * means you select the shifted KeySym, else select the normal one.
     *
     * All in one expression!
     */
    return 
        (shifted = ((((state[mod_mod2] & 1)
            & (((keyshift[code] >= XK_KP_0) & (keyshift[code] <= XK_KP_9))
		| (keyshift[code] == XK_KP_Decimal)))
        | ((state[mod_lock] & 1)
            & (keynorm[code] >= XK_a) & (keynorm[code] <= XK_z)))
        ^ state[mod_shift])) ? keyshift[code] : keynorm[code];
}

static void 
keyboard_handler (int keycode, int newstate)
{
    KeyState[keycode] = newstate;
}
