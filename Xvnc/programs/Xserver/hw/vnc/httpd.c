/*
 * httpd.c - a simple HTTP server
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
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <pwd.h>

#include "rfb.h"

#define NOT_FOUND_STR "HTTP/1.0 404 Not found\n\n" \
    "<HEAD><TITLE>File Not Found</TITLE></HEAD>\n" \
    "<BODY><H1>File Not Found</H1></BODY>\n"

#define OK_STR "HTTP/1.0 200 OK\n\n"

#define GEN_HTML_STR "<HTML><TITLE>%s's VNC desktop</TITLE>\n" \
   "<APPLET CODE=vncviewer.class ARCHIVE=vncviewer.jar WIDTH=%d HEIGHT=%d>\n" \
   "<param name=PORT value=%d></APPLET></HTML>\n"

static void httpProcessInput();

int httpPort = 0;
char *httpDir = NULL;

int httpListenSock = -1;
int httpSock = -1;
FILE* httpFP = NULL;

#define BUF_SIZE 32768

static char buf[BUF_SIZE];


/*
 * httpInitSockets sets up the TCP socket to listen for HTTP connections.
 */

void
httpInitSockets()
{
    static Bool done = FALSE;

    if (done)
	return;

    done = TRUE;

    if (!httpDir)
	return;

    if (httpPort == 0) {
	httpPort = 5800 + atoi(display);
    }

    fprintf(stderr,"httpInitSockets: listening on TCP port %d\n", httpPort);

    if ((httpListenSock = ListenOnTCPPort(httpPort)) < 0) {
	perror("ListenOnTCPPort");
	exit(1);
    }

    AddEnabledDevice(httpListenSock);
}


/*
 * httpCheckFds is called from ProcessInputEvents to check for input on the
 * HTTP socket(s).  If there is input to process, httpProcessInput is called.
 */

void
httpCheckFds()
{
    int nfds, n;
    fd_set fds;
    struct timeval tv;
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);

    if (!httpDir)
	return;

    FD_ZERO(&fds);
    FD_SET(httpListenSock, &fds);
    if (httpSock >= 0) {
	FD_SET(httpSock, &fds);
    }
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    nfds = select(max(httpSock,httpListenSock) + 1, &fds, NULL, NULL, &tv);
    if (nfds == 0) {
	return;
    }
    if (nfds < 0) {
	perror("httpCheckFds: select");
	return;
    }

    if ((httpSock >= 0) && FD_ISSET(httpSock, &fds)) {
	httpProcessInput();
    }

    if (FD_ISSET(httpListenSock, &fds)) {

	if (httpSock >= 0) close(httpSock);

	if ((httpSock = accept(httpListenSock,
			       (struct sockaddr *)&addr, &addrlen)) < 0) {
	    perror("httpCheckFds: accept");
	    return;
	}
	if ((httpFP = fdopen(httpSock, "r+")) == NULL) {
	    perror("httpCheckFds: fdopen");
	    close(httpSock);
	    httpSock = -1;
	    return;
	}

	/*fprintf(stderr,"httpCheckFds: got connection\n");*/
	AddEnabledDevice(httpSock);
    }
}


static void
httpCloseSock()
{
    fclose(httpFP);
    httpFP = NULL;
    RemoveEnabledDevice(httpSock);
    httpSock = -1;
}


/*
 * httpProcessInput is called when input is received on the HTTP socket.
 */

static void
httpProcessInput()
{
    char *fname = NULL;
    int fd;

    buf[0] = '\0';

    while (1) {
	if (!fgets(buf, BUF_SIZE, httpFP)) {
	    perror("httpProcessInput: fgets");
	    httpCloseSock();
	    return;
	}

	if ((strcmp(buf,"\n") == 0) || (strcmp(buf,"\r\n") == 0)
	    || (strcmp(buf,"\r") == 0) || (strcmp(buf,"\n\r") == 0))
	    /* end of client request */
	    break;

	if (strncmp(buf, "GET ", 4) == 0) {
	    char *base;

	    fname = (char *)xalloc(strlen(httpDir) + strlen(buf) + 2);
	    sprintf(fname, "%s", httpDir);
	    base = &fname[strlen(fname)];

	    if (sscanf(buf, "GET %s HTTP/1.0", base) != 1) {
		fprintf(stderr,"couldn't parse GET line\n");
		httpCloseSock();
		free(fname);
		return;
	    }

	    if (base[0] != '/') {
		fprintf(stderr,"filename didn't begin with '/'\n");
		WriteExact(httpSock, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
		httpCloseSock();
		free(fname);
		return;
	    }

	    if (strchr(base+1, '/') != NULL) {
		fprintf(stderr,"asking for file in other directory\n");
		WriteExact(httpSock, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
		httpCloseSock();
		free(fname);
		return;
	    }

	    fprintf(stderr,"httpd: get '%s'\n", base+1);
	    continue;
	}
    }

    if (fname == NULL) {
	fprintf(stderr,"no GET line\n");
	httpCloseSock();
	return;
    }


    if (fname[strlen(fname)-1] == '/') {
	struct passwd *user = getpwuid(getuid());

	WriteExact(httpSock, OK_STR, strlen(OK_STR));
	sprintf(buf, GEN_HTML_STR, user->pw_name, rfbScreen.width,
		rfbScreen.height + 32, rfbPort);
	WriteExact(httpSock, buf, strlen(buf));
	httpCloseSock();
	free(fname);
	return;
    }

    if ((fd = open(fname, O_RDONLY)) < 0) {
	perror("httpProcessInput: open");
	WriteExact(httpSock, NOT_FOUND_STR, strlen(NOT_FOUND_STR));
	httpCloseSock();
	free(fname);
	return;
    }

    WriteExact(httpSock, OK_STR, strlen(OK_STR));

    while (1) {
	int n = read(fd, buf, BUFSIZE);
	if (n < 0) {
	    perror("httpProcessInput: read");
	    close(fd);
	    httpCloseSock();
	    free(fname);
	    return;
	}

	if (n == 0)
	    break;

	if (WriteExact(httpSock, buf, n) < 0)
	    break;
    }

    close(fd);
    httpCloseSock();
    free(fname);
}
