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

/*
 * svncviewer.c - VNC viewer for SVGA.
 * derived from vncviewer.c by {ganesh,sitaram}@cse.iitb.ernet.in
 */

#include <vncviewer.h>
#include <vga.h>

void HandleKeyboardEvent (void);

int
main(int argc, char **argv)
{
    fd_set fds;
    struct timeval tv, *tvp;
    int msWait;
	int event;

    vga_init (); /* Give up uid 0 asap */

    processArgs(argc, argv);

    if (listenSpecified) {

/*	listenForIncomingConnections();*/

	/* returns only with a succesful connection */

    } else {
	if (!ConnectToRFBServer(hostname, port)) exit(1);
    }

    if (!InitialiseRFBConnection(rfbsock)) exit(1);

    if (!InitSvga()) exit(1);

    if (!SetFormatAndEncodings()) {
	ShutdownSvga();
	exit(1);
    }

    if (!SendFramebufferUpdateRequest(0, 0, si.framebufferWidth,
				      si.framebufferHeight, False)) {
	ShutdownSvga();
	exit(1);
    }

    while (True) {

	tvp = NULL;

	if (sendUpdateRequest) {
	    gettimeofday(&tv, NULL);

	    msWait = (updateRequestPeriodms +
		      ((updateRequestTime.tv_sec - tv.tv_sec) * 1000) +
		      ((updateRequestTime.tv_usec - tv.tv_usec) / 1000));

	    if (msWait > 0) {
		tv.tv_sec = msWait / 1000;
		tv.tv_usec = (msWait % 1000) * 1000;

		tvp = &tv;
	    } else {
		if (!SendIncrementalFramebufferUpdateRequest()) {
		    ShutdownSvga();
		    exit(1);
		}
	    }
	}

	FD_ZERO(&fds);
	FD_SET(rfbsock,&fds);

	event = vga_waitevent (VGA_MOUSEEVENT|VGA_KEYEVENT, &fds, NULL, NULL, tvp);
	if (event < 0) {
	    perror("select");
	    ShutdownSvga();
	    exit(1);
	}

	if (FD_ISSET(rfbsock, &fds)) {
	    if (!HandleRFBServerMessage()) {
		ShutdownSvga();
		exit(1);
	    }
	}
	if (event & VGA_MOUSEEVENT)
		HandleMouseEvent ();
	if (event & VGA_KEYEVENT)
		HandleKeyboardEvent ();

	}
}
