/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *  Copyright (C) 2001 Yoshiki Hayashi <yoshiki@xemacs.org>
 *  Copyright (C) 2006 Karel Kulhavy <clock (at) twibright (dot) com>
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
 * Karel Kulhavy changed the XPM output to yuv4mpeg2 so it can be used easily
 * to encode Theora (with the example libtheora encoder), Xvid and wmv (with
 * transcode). Now it encodes at 85% video playback speed on Pentium M 1.5GHz
 * which is way faster than the original XPM output.
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

void PrintInHex(char *buf, int len);

Bool errorMessageOnReadFailure = True;

#define BUF_SIZE 8192
static char buf[BUF_SIZE];
static char *bufoutptr = buf;
static int buffered = 0;
Bool vncLogTimeStamp = False;

/* Shifts to obtain the red, green, and blue value
 * from a long loaded from the memory. These are initialized once before the
 * first snapshot is taken. */
unsigned red_shift, green_shift, blue_shift;

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

void my_fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
	if (fwrite(ptr, size, nmemb, stream)
		!=nmemb){
		fprintf(stderr,"vncrec: Error occured writing %u bytes"
			"into a file stream: ", (unsigned)(size*nmemb));
		perror(NULL);
		exit(1);
	}
}

static void
writeLogHeader (void)
{
  struct timeval tv;
  static unsigned long frame = 0;

  if (vncLogTimeStamp)
    {
      long tell = ftell(vncLog);

      my_fwrite (&frame, sizeof(frame), 1, vncLog);

      gettimeofday (&tv, NULL);

      if (appData.debugFrames) {
          fprintf(stderr, "write frame %lu at time %.3f @ offset %ld\n",
                  frame, tv.tv_sec + tv.tv_usec / 1e6, tell);
      }

      tv.tv_sec = Swap32IfLE (tv.tv_sec);
      tv.tv_usec = Swap32IfLE (tv.tv_usec);
      my_fwrite (&tv, sizeof (struct timeval), 1, vncLog);

      frame++;
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

void scanline(unsigned char *r, unsigned char *g, unsigned char *b, unsigned
		char *src, unsigned cycles)
{
	unsigned long v; /* This will probably work only on 32-bit
			    architecture */

	for (;cycles;cycles--){
		v=*(unsigned long *)(void *)src;

		*r++=v>>red_shift;
		*g++=v>>green_shift;
		*b++=v>>blue_shift;

		src+=4;
	}
}

/* Use on variable names only */
#define CLIP(x) if (x<0) x=0; else if (x>255) x=255;

/* Converts RGB to the "yuv" that is in yuv4mpeg.
 * It looks like the unspecified "yuv" that is in yuv4mpeg2 is actually
 * Y'CbCr, with all 3 components full range 0-255 as in JPEG. If
 * the reduced range (16-240 etc.) is used here, the result looks washed out. */
void rgb2yuv(unsigned char *d, unsigned plane)
{
	unsigned char *r, *g, *b;
	int y,u,v;

	r=d;
	g=r+plane;
	b=g+plane;
	for (;plane;plane--){
		y=((77**r+150**g+29**b+0x80)>>8);
		u=0x80+((-43**r-85**g+128**b+0x7f)>>8);
		v=0x80+((128**r-107**g-21**b+0x7f)>>8);
		CLIP(y);
		CLIP(u);
		CLIP(v);
		*r++=y;
		*g++=u;
		*b++=v;
	}
}

/* Writes a frame of yuv4mpeg file to the stdout. Repeats writing
 * the identical frame "times" times. */
void dump_image(XImage *i, unsigned times)
{
	unsigned char frame_header[]="FRAME\n";

	unsigned w=si.framebufferWidth; /* Image size */
	unsigned h=si.framebufferHeight;
	unsigned char *d; /* R'G'B' / Y'CbCr data buffer */
	unsigned x, y; /* Counters */
	unsigned char *rptr, *gptr, *bptr; /* R'/Cr, green/get, B'/Cb pointer */
	unsigned char *wptr; /* Write ptr for moving the chroma samples
				together */

	/* Test that the image has 32-bit pixels */
	if (i->bitmap_unit!=32){
		fprintf(stderr,"Sorry, only >= 8 bits per channel colour "
				"is supported. bitmap_unit=%u\n",
				i->bitmap_unit);
		exit(1);
	}

	/* Allocate a R'G'B' buffer (' values mean gamma-corrected so the
	 * numbers are linear to the electrical video signal. RGB would mean
	 * linear photometric space */
	d = malloc(w*h*3);

	/* Read the data in from the image into the RGB buffer */
        rptr = i->data;
        wptr = d;

	for (y=0; y<h; y++){
            for (x=0; x<w; ++x){
                int v;

		 v = *(unsigned long *)(void *)rptr;
                 rptr += 4;

                 *wptr++ = v >> red_shift;
                 *wptr++ = v >> green_shift;
                 *wptr++ = v >> blue_shift;
            }
	}

	/* Write out the frame, "times" times. */
	for (;times;times--){
            //my_fwrite(frame_header, sizeof(frame_header)-1,	1, stdout);
            my_fwrite(d, w*h*3, 1, stdout);
	}

	/* Throw away the formerly R'G'B', now Y'CbCr buffer */
	free(d);
}

/* Returns MSBFirst for big endian and LSBFirst for little */
int test_endian(void)
{
	long a=1;

	*((unsigned char *)(void *)&a)=0;
	return a?MSBFirst:LSBFirst;
}

unsigned mask2shift(unsigned long in)
{
	unsigned shift=0;

	if (!in) return 0;

	while(!(in&1)){
		in>>=1;
		shift++;
	}
	return shift;
}

/* Sets the red_shift, green_shift and blue_shift variables. */
void examine_layout(void)
{
	XWindowAttributes attr;
	Window rootwin;
	int dummy_i;
	unsigned dummy_u;

	XGetGeometry(dpy, desktopWin, &rootwin,
		&dummy_i, &dummy_i, &dummy_u, &dummy_u, &dummy_u, &dummy_u);
	XGetWindowAttributes(dpy, rootwin, &attr);
	fprintf(stderr,"red_mask=%x, green_mask=%x, blue_mask=%x, "
			"CPU endian=%u, dpy endian=%u\n",
			attr.visual->red_mask,
			attr.visual->green_mask,
			attr.visual->blue_mask,
			test_endian(),
			ImageByteOrder(dpy));
	red_shift=mask2shift(attr.visual->red_mask);
	green_shift=mask2shift(attr.visual->green_mask);
	blue_shift=mask2shift(attr.visual->blue_mask);
	if (test_endian()!=ImageByteOrder(dpy)){
		red_shift=24-red_shift;
		green_shift=24-green_shift;
		blue_shift=24-blue_shift;
	}
	fprintf(stderr,"Image dump color channel shifts: R=%u, G=%u, B=%u\n",
			red_shift, green_shift, blue_shift);
}

void print_movie_frames_up_to_time(struct timeval tv)
{
    static double framerate;
    static double start_time = 0;
    double now = tv.tv_sec + tv.tv_usec / 1e6;
    static unsigned last_frame = 0, this_frame = 0;
    static unsigned times;
    XImage *image;

    if (start_time == 0) {  // one-time initialization
        framerate = getenv("VNCREC_MOVIE_FRAMERATE") ? atoi(getenv("VNCREC_MOVIE_FRAMERATE")) : 10;

        fprintf(stderr,"RGB8 W%u H%u F%u:1 Ip A0:0\n",
                si.framebufferWidth,
                si.framebufferHeight,
                (unsigned)framerate);

        examine_layout(); /* Figure out red_shift, green_shift, blue_shift */

        start_time = now;
    }

    this_frame = (unsigned)((now - start_time) * framerate);
    assert(this_frame >= last_frame);

    if (this_frame == last_frame) return; /* not time for the next frame yet. */

    times = this_frame - last_frame;
    last_frame = this_frame;

    image = XGetImage(dpy, desktopWin,0, 0, si.framebufferWidth,
                      si.framebufferHeight, 0xffffffff,
                      ZPixmap);
    assert(image);

    if (0) {
        if (times == 1) {
            fprintf(stderr,"Dumping frame for time %.2f sec - %.6f.\n",
                    times / framerate, now);
        }
        else {
            fprintf(stderr,"Dumping %u frames for time %.2f sec - %.6f.\n",
                    times, times / framerate, now);
        }
    }

    /* Print the frame(s) */
    dump_image(image, times);

    XDestroyImage(image);
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
          long tell = ftell(vncLog);

          static unsigned long rframe_curr = 0; // frame counter for verification
          unsigned long rframe;

	  static struct timeval prev;
	  struct timeval tv;

	  i = fread (&rframe, sizeof (rframe), 1, vncLog);
	  if (i < 1)
	    return False;

          if (rframe != rframe_curr) {
              fprintf(stderr, "Frame number does not match! File desynced or corrupt!.\n");
              abort();
          }
          ++rframe_curr;

	  i = fread (&tv, sizeof (struct timeval), 1, vncLog);
	  if (i < 1)
	    return False;
	  tv.tv_sec = Swap32IfLE (tv.tv_sec);
	  tv.tv_usec = Swap32IfLE (tv.tv_usec);
	  /* This looks like the best way to adjust time at the moment. */
      if (!appData.movie &&  // print the movie frames as fast as possible
	  prev.tv_sec)
	    {
	      struct timeval diff = timeval_subtract (tv, prev);
	      sleep (diff.tv_sec);
	      usleep (diff.tv_usec);
	    }
	  prev = tv;

          if (appData.debugFrames) {
              fprintf(stderr, "read frame %lu at time %.3f @ offset %ld\n",
                      rframe, tv.tv_sec + tv.tv_usec / 1e6, tell);
          }

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
      writeLogHeader (); /* writes the timestamp */
      fwrite (bufoutptr, 1, n, vncLog);
    }
    bufoutptr += n;
    buffered -= n;
    return True;
  }

  memcpy(out, bufoutptr, buffered);
  if (appData.record)
    {
      writeLogHeader (); /* Writes the timestamp */
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
 * FindFreeTcpPort tries to find unused TCP port in the range
 * (TUNNEL_PORT_OFFSET, TUNNEL_PORT_OFFSET + 99]. Returns 0 on failure.
 */

int
FindFreeTcpPort(void)
{
  int sock, port;
  struct sockaddr_in addr;

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    fprintf(stderr,programName);
    perror(": FindFreeTcpPort: socket");
    return 0;
  }

  for (port = TUNNEL_PORT_OFFSET + 99; port > TUNNEL_PORT_OFFSET; port--) {
    addr.sin_port = htons((unsigned short)port);
    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
      close(sock);
      return port;
    }
  }

  close(sock);
  return 0;
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
