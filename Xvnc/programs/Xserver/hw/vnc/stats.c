/*
 * stats.c
 */

/*
 *  Copyright (C) 1997, 1998 Olivetti & Oracle Research Laboratory
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
#include <stdlib.h>
#include "rfb.h"

#define MAX_ENCODINGS 10

int rfbBytesSent[MAX_ENCODINGS];
int rfbRectanglesSent[MAX_ENCODINGS];

int rfbFramebufferUpdateMessagesSent;
int rfbRawBytesEquivalent;
int rfbKeyEventsRcvd;
int rfbPointerEventsRcvd;

static char* encNames[] = {
    "raw", "copyRect", "RRE", "[encoding 3]", "CoRRE", "hextile",
    "[encoding 6]", "[encoding 7]", "[encoding 8]", "[encoding 9]"
};


void
rfbResetStats()
{
    int i;
    for (i = 0; i < MAX_ENCODINGS; i++) {
	rfbBytesSent[i] = 0;
	rfbRectanglesSent[i] = 0;
    }
    rfbFramebufferUpdateMessagesSent = 0;
    rfbRawBytesEquivalent = 0;
    rfbKeyEventsRcvd = 0;
    rfbPointerEventsRcvd = 0;
}

void
rfbPrintStats()
{
    int i;
    int totalRectanglesSent = 0;
    int totalBytesSent = 0;

    fprintf(stderr,"Statistics\n");
    fprintf(stderr,"----------\n");

    if ((rfbKeyEventsRcvd != 0) || (rfbPointerEventsRcvd != 0))
	fprintf(stderr,"Key events received %d, pointer events %d\n",
		rfbKeyEventsRcvd, rfbPointerEventsRcvd);

    for (i = 0; i < MAX_ENCODINGS; i++) {
	totalRectanglesSent += rfbRectanglesSent[i];
	totalBytesSent += rfbBytesSent[i];
    }

    fprintf(stderr,"Framebuffer updates %d, rectangles %d, bytes %d\n",
	    rfbFramebufferUpdateMessagesSent, totalRectanglesSent,
	    totalBytesSent);

    for (i = 0; i < MAX_ENCODINGS; i++) {
	if (rfbRectanglesSent[i] != 0)
	    fprintf(stderr,"   %s rectangles %d, bytes %d\n",
		    encNames[i], rfbRectanglesSent[i], rfbBytesSent[i]);
    }

    if ((totalBytesSent-rfbBytesSent[rfbEncodingCopyRect]) != 0) {
	fprintf(stderr,"Raw bytes equivalent %d, compression ratio %f\n",
		rfbRawBytesEquivalent,
		(double)rfbRawBytesEquivalent
		/ (double)(totalBytesSent-rfbBytesSent[rfbEncodingCopyRect]));
    }
    /*
    if (rfbFramebufferUpdateMessagesSent != 0)
	fprintf(stderr,"Average rectangles per framebuffer update %d\n",
		totalRectanglesSent / rfbFramebufferUpdateMessagesSent);

    if (totalRectanglesSent != 0)
	fprintf(stderr,"Average bytes per rectangle %d\n",
		totalBytesSent / totalRectanglesSent);

    for (i = 0; i < MAX_ENCODINGS; i++) {
	if (rfbRectanglesSent[i] != 0)
	    fprintf(stderr,"   %s %d\n",
		    encNames[i], rfbBytesSent[i] / rfbRectanglesSent[i]);
    }
    */
    fprintf(stderr,"\n");
}
