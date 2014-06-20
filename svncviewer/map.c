#include <X11/X.h>
#include <X11/keysym.h>
#include "xf86Keymap.h"

KeySym scan2x(int scan)
{
  static int shift = 0, caps = 0, numlock = 0;
  switch (scan) {
    case 0x3a: caps ^= 1; break;
    case 0x2a: case 0x36: shift = 1; break;
    case 0xaa: case 0xb6: shift = 0; break;
    case 0x45: numlock ^= 1; break;
  }
  if (numlock && scan>=0x47 && scan<=0x53) {
    return numkeys[scan-0x47];
  } else {
    return map[scan * 4 + caps * 2 + shift];
  }
}

void main()
{
printf("%d\n", scan2x(31));
}
