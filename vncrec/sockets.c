/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *  Copyright (C) 2001 Yoshiki Hayashi <yoshiki@xemacs.org>
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
 * sockets.c - functions to deal with sockets.
 */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include <vncviewer.h>
#include <X11/xpm.h>

void PrintInHex(char *buf, int len);

Bool errorMessageOnReadFailure = True;

#define BUF_SIZE 8192
static char buf[BUF_SIZE];
static char *bufoutptr = buf;
static int buffered = 0;
Bool vncLogTimeStamp = False;

/*
 * ReadFromRFBServer is called whenever we want to read some data from the RFB
 * server.  It is non-trivial for two reasons:
 *
 * 1. For efficiency it performs some intelligent buffering, avoiding invoking
 *    the read() system call too often.  For small chunks of data, it simply
 *    copies the data out of an internal buffer.  For large amounts of data it
 *    reads directly into the buffer provided by the caller.
 *
 * 2. Whenever read() would block, it invokes the Xt event dispatching
 *    mechanism to process X events.  In fact, this is the only place these
 *    events are processed, as there is no XtAppMainLoop in the program.
 */

static Bool rfbsockReady = False;
static void
rfbsockReadyCallback(XtPointer clientData, int *fd, XtInputId *id)
{
  rfbsockReady = True;
  XtRemoveInput(*id);
}

static void
ProcessXtEvents()
{
  rfbsockReady = False;
  XtAppAddInput(appContext, rfbsock, (XtPointer)XtInputReadMask,
		rfbsockReadyCallback, NULL);
  while (!rfbsockReady) {
    XtAppProcessEvent(appContext, XtIMAll);
  }
}

static void
writeLogHeader (void)
{
  struct timeval tv;

  if (vncLogTimeStamp)
    {
      gettimeofday (&tv, NULL);
      tv.tv_sec = Swap32IfLE (tv.tv_sec);
      tv.tv_usec = Swap32IfLE (tv.tv_usec);
      fwrite (&tv, sizeof (struct timeval), 1, vncLog);
    }
}

static struct timeval
timeval_subtract (struct timeval x,
		  struct timeval y)
{
  struct timeval result;

  result.tv_sec = x.tv_sec - y.tv_sec;
  result.tv_usec = x.tv_usec - y.tv_usec;
  if (result.tv_usec < 0)
    {
      result.tv_sec--;
      result.tv_usec += 1000000;
    }
  assert (result.tv_usec >= 0);
  return result;
}

void print_movie_frames_up_to_time(struct timeval tv)
{
  static int num_frames=0;
  static double framerate;
  static double start_time = 0;
  static const char * cmd;
  double now = tv.tv_sec + tv.tv_usec / 1e6;
  char ** data;
  int success;
  int num_frames_this_time;
  static int num_parallel;

  if(start_time == 0) {  // one-time initialization
    const char * usr_cmd = getenv("VNCREC_MOVIE_CMD");
    cmd = usr_cmd ? usr_cmd : "cat > img_%05d.xpm";
    framerate = getenv("VNCREC_MOVIE_FRAMERATE") ? atoi(getenv("VNCREC_MOVIE_FRAMERATE")) : 10;
    num_parallel = getenv("VNCREC_MOVIE_PARALLEL") ? atoi(getenv("VNCREC_MOVIE_PARALLEL")) : 1;
    start_time = now;
  }

  num_frames_this_time = (int)((now - start_time) * framerate - num_frames);

  if(num_frames_this_time <= 0)
    return; ///// not time for the next frame yet.

  success = XpmCreateDataFromPixmap(dpy, &data, desktopWin, 0, 0);
  assert(success == XpmSuccess);

  for(; num_frames_this_time-- > 0; ++num_frames) {

    printf("printing frame for time = %f seconds\n", num_frames/framerate);

    if(fork()) {
      int status;
      if(num_frames >= num_parallel-1) {
	wait(&status);
	assert(status == 0);
      }
    }
    else {
      int i;
      FILE * file;
      char cmd_buf[1000];

      // since XpmCreateDataFromPixmap didn't return error, we'll assume the first line is valid.
      char * endptr;
      const int width = strtol(data[0], &endptr, 10);
      const int height = strtol(endptr, &endptr, 10);
      const int num_colors = strtol(endptr, &endptr, 10);

      assert(width ==  si.framebufferWidth);
      assert(height ==  si.framebufferHeight);

      snprintf(cmd_buf, sizeof(cmd_buf)-1, cmd, num_frames);
      file = popen(cmd_buf, "w");
      assert(file);

      fprintf(file, "/* XPM */\nstatic char * x[] = {\n");
      for(i=0; i < 1+num_colors+height; ++i)
	fprintf(file, "\"%s\",\n", data[i]);
      fprintf(file, "};");
      pclose(file);
      exit(0);
    }
  }
  XpmFree(data);
//  printf("all done up to next time\n");
}

Bool
ReadFromRFBServer(char *out, unsigned int n)
{
  if (appData.play || appData.movie)
    {
      int i;
      char buf[1];

      if (vncLogTimeStamp)
	{
	  static struct timeval prev;
	  struct timeval tv;
	  i = fread (&tv, sizeof (struct timeval), 1, vncLog);
	  tv.tv_sec = Swap32IfLE (tv.tv_sec);
	  tv.tv_usec = Swap32IfLE (tv.tv_usec);
	  if (i < 0)
	    return False;
	  /* This looks like the best way to adjust time at the moment. */
      if (!appData.movie &&  // print the movie frames as fast as possible
	  prev.tv_sec)
	    {
	      struct timeval diff = timeval_subtract (tv, prev);
	      sleep (diff.tv_sec);
	      usleep (diff.tv_usec);
	    }
	  prev = tv;

      if(appData.movie)
	print_movie_frames_up_to_time(tv);
	}
      i = fread (out, 1, n, vncLog);
      if (i < 0)
	return False;
      else
	return True;
    }
      
  if (n <= buffered) {
    memcpy(out, bufoutptr, n);
    if (appData.record)
    {
      writeLogHeader ();
      fwrite (bufoutptr, 1, n, vncLog);
    }
    bufoutptr += n;
    buffered -= n;
    return True;
  }

  memcpy(out, bufoutptr, buffered);
  if (appData.record)
    {
      writeLogHeader ();
      fwrite (bufoutptr, 1, buffered, vncLog);
    }

  out += buffered;
  n -= buffered;

  bufoutptr = buf;
  buffered = 0;

  if (n <= BUF_SIZE) {

    while (buffered < n) {
      int i = read(rfbsock, buf + buffered, BUF_SIZE - buffered);
      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    ProcessXtEvents();
	    i = 0;
	  } else {
	    fprintf(stderr,programName);
	    perror(": read");
	    return False;
	  }
	} else {
	  if (errorMessageOnReadFailure) {
	    fprintf(stderr,"%s: VNC server closed connection\n",programName);
	  }
	  return False;
	}
      }
      buffered += i;
    }

    memcpy(out, bufoutptr, n);
    if (appData.record)
      fwrite (bufoutptr, 1, n, vncLog);
    bufoutptr += n;
    buffered -= n;
    return True;

  } else {

    while (n > 0) {
      int i = read(rfbsock, out, n);
      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    ProcessXtEvents();
	    i = 0;
	  } else {
	    fprintf(stderr,programName);
	    perror(": read");
	    return False;
	  }
	} else {
	  if (errorMessageOnReadFailure) {
	    fprintf(stderr,"%s: VNC server closed connection\n",programName);
	  }
	  return False;
	}
      }
      else
	{
	  if (appData.record)
	    fwrite (out, 1, i, vncLog);
	}
      out += i;
      n -= i;
    }

    return True;
  }
}


/*
 * Write an exact number of bytes, and don't return until you've sent them.
 */

Bool
WriteExact(int sock, char *buf, int n)
{
  fd_set fds;
  int i = 0;
  int j;

  if (appData.play || appData.movie)
    return True;

  while (i < n) {
    j = write(sock, buf + i, (n - i));
    if (j <= 0) {
      if (j < 0) {
	if (errno == EWOULDBLOCK || errno == EAGAIN) {
	  FD_ZERO(&fds);
	  FD_SET(rfbsock,&fds);

	  if (select(rfbsock+1, NULL, &fds, NULL, NULL) <= 0) {
	    fprintf(stderr,programName);
	    perror(": select");
	    return False;
	  }
	  j = 0;
	} else {
	  fprintf(stderr,programName);
	  perror(": write");
	  return False;
	}
      } else {
	fprintf(stderr,"%s: write failed\n",programName);
	return False;
      }
    }
    i += j;
  }
  return True;
}


/*
 * ConnectToTcpAddr connects to the given TCP port.
 */

int
ConnectToTcpAddr(unsigned int host, int port)
{
  int sock;
  struct sockaddr_in addr;
  int one = 1;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = host;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": ConnectToTcpAddr: socket");
    return -1;
  }

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr,programName);
    perror(": ConnectToTcpAddr: connect");
    close(sock);
    return -1;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		 (char *)&one, sizeof(one)) < 0) {
    fprintf(stderr,programName);
    perror(": ConnectToTcpAddr: setsockopt");
    close(sock);
    return -1;
  }

  return sock;
}



/*
 * ListenAtTcpPort starts listening at the given TCP port.
 */

int
ListenAtTcpPort(int port)
{
  int sock;
  struct sockaddr_in addr;
  int one = 1;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: socket");
    return -1;
  }

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		 (const char *)&one, sizeof(one)) < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: setsockopt");
    close(sock);
    return -1;
  }

  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: bind");
    close(sock);
    return -1;
  }

  if (listen(sock, 5) < 0) {
    fprintf(stderr,programName);
    perror(": ListenAtTcpPort: listen");
    close(sock);
    return -1;
  }

  return sock;
}


/*
 * AcceptTcpConnection accepts a TCP connection.
 */

int
AcceptTcpConnection(int listenSock)
{
  int sock;
  struct sockaddr_in addr;
  int addrlen = sizeof(addr);
  int one = 1;

  sock = accept(listenSock, (struct sockaddr *) &addr, &addrlen);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": AcceptTcpConnection: accept");
    return -1;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		 (char *)&one, sizeof(one)) < 0) {
    fprintf(stderr,programName);
    perror(": AcceptTcpConnection: setsockopt");
    close(sock);
    return -1;
  }

  return sock;
}


/*
 * SetNonBlocking sets a socket into non-blocking mode.
 */

Bool
SetNonBlocking(int sock)
{
  if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
    fprintf(stderr,programName);
    perror(": AcceptTcpConnection: fcntl");
    return False;
  }
  return True;
}


/*
 * StringToIPAddr - convert a host string to an IP address.
 */

Bool
StringToIPAddr(const char *str, unsigned int *addr)
{
  struct hostent *hp;

  if (strcmp(str,"") == 0) {
    *addr = 0; /* local */
    return True;
  }

  *addr = inet_addr(str);

  if (*addr != -1)
    return True;

  hp = gethostbyname(str);

  if (hp) {
    *addr = *(unsigned int *)hp->h_addr;
    return True;
  }

  return False;
}


/*
 * Test if the other end of a socket is on the same machine.
 */

Bool
SameMachine(int sock)
{
  struct sockaddr_in peeraddr, myaddr;
  int addrlen = sizeof(struct sockaddr_in);

  if (appData.play || appData.record || appData.movie)
    return False;

  getpeername(sock, (struct sockaddr *)&peeraddr, &addrlen);
  getsockname(sock, (struct sockaddr *)&myaddr, &addrlen);

  return (peeraddr.sin_addr.s_addr == myaddr.sin_addr.s_addr);
}


/*
 * Print out the contents of a packet for debugging.
 */

void
PrintInHex(char *buf, int len)
{
  int i, j;
  char c, str[17];

  str[16] = 0;

  fprintf(stderr,"ReadExact: ");

  for (i = 0; i < len; i++)
    {
      if ((i % 16 == 0) && (i != 0)) {
	fprintf(stderr,"           ");
      }
      c = buf[i];
      str[i % 16] = (((c > 31) && (c < 127)) ? c : '.');
      fprintf(stderr,"%02x ",(unsigned char)c);
      if ((i % 4) == 3)
	fprintf(stderr," ");
      if ((i % 16) == 15)
	{
	  fprintf(stderr,"%s\n",str);
	}
    }
  if ((i % 16) != 0)
    {
      for (j = i % 16; j < 16; j++)
	{
	  fprintf(stderr,"   ");
	  if ((j % 4) == 3) fprintf(stderr," ");
	}
      str[i % 16] = 0;
      fprintf(stderr,"%s\n",str);
    }

  fflush(stderr);
}
