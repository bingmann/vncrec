/*
 * translate.c - translate between different pixel formats
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
#include "rfb.h"

static void PrintPixelFormat _((rfbPixelFormat *pf));
static Bool rfbSetClientColourMapBGR233();


/*
 * Structure representing pixel format for RFB server (i.e. us).
 */

rfbPixelFormat rfbServerFormat;


/*
 * Some standard pixel formats.
 */

static const rfbPixelFormat BGR233Format = {
    8, 8, 0, 1, 7, 7, 3, 0, 3, 6
};
static const rfbPixelFormat BGR888LittleEndianFormat = {
    32, 24, 0, 1, 255, 255, 255, 0, 8, 16
};
static const rfbPixelFormat BGR888BigEndianFormat = {
    32, 24, 1, 1, 255, 255, 255, 0, 8, 16
};
static const rfbPixelFormat BGR556LittleEndianFormat = {
    16, 16, 0, 1, 63, 31, 31, 0, 6, 11
};
static const rfbPixelFormat BGR556BigEndianFormat = {
    16, 16, 1, 1, 63, 31, 31, 0, 6, 11
};
static const rfbPixelFormat ColourMap8BitFormat = {
    8, 8, 0, 0, 0, 0, 0, 0, 0, 0
};


/*
 * Macro to compare pixel formats.
 */

#define PF_EQ(x,y)							\
	((x.bitsPerPixel == y.bitsPerPixel) &&				\
	 (x.depth == y.depth) &&					\
	 ((x.bigEndian == y.bigEndian) || (x.bitsPerPixel == 8)) &&	\
	 (x.trueColour == y.trueColour) &&				\
	 (!x.trueColour || ((x.redMax == y.redMax) &&			\
			    (x.greenMax == y.greenMax) &&		\
			    (x.blueMax == y.blueMax) &&			\
			    (x.redShift == y.redShift) &&		\
			    (x.greenShift == y.greenShift) &&		\
			    (x.blueShift == y.blueShift))))



/*
 * rfbTranslateNone is used when no translation is required.
 */

void
rfbTranslateNone(cl, iptr, optr, iptradd, optradd, nlines)
    rfbClientPtr cl;
    char *iptr;
    char *optr;
    int iptradd;
    int optradd;
    int nlines;
{
    while (nlines > 0) {
	memcpy(optr, iptr, optradd);
	iptr += iptradd;
	optr += optradd;
	nlines--;
    }
}



/*
 * The macro TRUECOLOUR_TRANS is used to translate a pixel between any two
 * given true colour formats.
 *
 * p is the input pixel value.
 * irShf, igShf, ibShf are the shifts needed to get each of the red, green and
 *       blue values to the least significant bit in the input pixel (e.g.
 *       if the red value is the top three bits of a byte, irShf is 5).
 * irMax, igMax, ibMax are the maximum values for each of red, green and blue.
 *       This value is (2^n - 1) where n is the number of bits used to
 *       represent the colour (e.g. for the above example with three bits to
 *       represent red, irMax is 7).
 * o?Shf and o?Max are the same but for the output pixel value.
 */

#define TRUECOLOUR_TRANS(p,irShf,igShf,ibShf,irMax,igMax,ibMax,orShf,ogShf,obShf,orMax,ogMax,obMax) \
(((((((p) >> (irShf)) & (irMax)) * (orMax) + (irMax)/2) / (irMax)) << orShf) |\
 ((((((p) >> (igShf)) & (igMax)) * (ogMax) + (igMax)/2) / (igMax)) << ogShf) |\
 ((((((p) >> (ibShf)) & (ibMax)) * (obMax) + (ibMax)/2) / (ibMax)) << obShf))



/* For the macro below, Swap8 needs to exist even though it does nothing */

#define Swap8(c) (c)


/*
 * The macro DEFINE_TRANSLATE_TRUECOLOUR is used to define several translation
 * functions.  For a given input and output pixel size, each function copies &
 * translates a rectangle from the framebuffer into an output buffer.  In
 * addition, the output pixels can be endian-swapped if necessary.
 */

#define DEFINE_TRANSLATE_TRUECOLOUR(IN,OUT,SWAP)			  \
static void								  \
rfbTranslateTrueColour##IN##to##OUT##swap##SWAP (cl, iptr, optr, iptradd, \
						optradd, nlines)	  \
    rfbClientPtr cl;							  \
    char *iptr;								  \
    char *optr;								  \
    int iptradd;							  \
    int optradd;							  \
    int nlines;								  \
{									  \
    register CARD##IN *ip = (CARD##IN *)iptr;				  \
    register CARD##OUT *op = (CARD##OUT *)optr;				  \
    int opadd = optradd * 8 / OUT;					  \
    int ipadd = (iptradd * 8 / IN) - (optradd * 8 / OUT);		  \
    register CARD##IN i;						  \
    register CARD##OUT o;						  \
    register CARD##OUT *opLineEnd;					  \
									  \
    while (nlines > 0) {						  \
	opLineEnd = op + opadd;						  \
	while (op < opLineEnd) {					  \
	    i = *(ip++);						  \
	    o = TRUECOLOUR_TRANS(i,					  \
				 rfbServerFormat.redShift,		  \
				 rfbServerFormat.greenShift,		  \
				 rfbServerFormat.blueShift,		  \
				 rfbServerFormat.redMax,		  \
				 rfbServerFormat.greenMax,		  \
				 rfbServerFormat.blueMax,		  \
				 cl->format.redShift,			  \
				 cl->format.greenShift,			  \
				 cl->format.blueShift,			  \
				 cl->format.redMax,			  \
				 cl->format.greenMax,			  \
				 cl->format.blueMax);			  \
	    if (SWAP) {							  \
		o = Swap##OUT (o);					  \
	    }								  \
	    *(op++) = o;						  \
	}								  \
									  \
	ip += ipadd;							  \
	nlines--;							  \
    }									  \
}

DEFINE_TRANSLATE_TRUECOLOUR(8,8,0)
DEFINE_TRANSLATE_TRUECOLOUR(8,16,0)
DEFINE_TRANSLATE_TRUECOLOUR(8,16,1)
DEFINE_TRANSLATE_TRUECOLOUR(8,32,0)
DEFINE_TRANSLATE_TRUECOLOUR(8,32,1)
DEFINE_TRANSLATE_TRUECOLOUR(16,8,0)
DEFINE_TRANSLATE_TRUECOLOUR(16,16,0)
DEFINE_TRANSLATE_TRUECOLOUR(16,16,1)
DEFINE_TRANSLATE_TRUECOLOUR(16,32,0)
DEFINE_TRANSLATE_TRUECOLOUR(16,32,1)
DEFINE_TRANSLATE_TRUECOLOUR(32,8,0)
DEFINE_TRANSLATE_TRUECOLOUR(32,16,0)
DEFINE_TRANSLATE_TRUECOLOUR(32,16,1)
DEFINE_TRANSLATE_TRUECOLOUR(32,32,0)
DEFINE_TRANSLATE_TRUECOLOUR(32,32,1)


/*
 * This is an array of the true colour translation functions which are
 * generated using the DEFINE_TRANSLATE_TRUECOLOUR macro.
 */

rfbTranslateFnType rfbTranslateTrueColourFns[3][3][2] = {
    { { rfbTranslateTrueColour8to8swap0,  rfbTranslateTrueColour8to8swap0 },
      { rfbTranslateTrueColour8to16swap0, rfbTranslateTrueColour8to16swap1 },
      { rfbTranslateTrueColour8to32swap0, rfbTranslateTrueColour8to32swap1 } },
    { { rfbTranslateTrueColour16to8swap0, rfbTranslateTrueColour16to8swap0 },
      { rfbTranslateTrueColour16to16swap0,rfbTranslateTrueColour16to16swap1 },
      { rfbTranslateTrueColour16to32swap0,rfbTranslateTrueColour16to32swap1 }},
    { { rfbTranslateTrueColour32to8swap0, rfbTranslateTrueColour32to8swap0 },
      { rfbTranslateTrueColour32to16swap0,rfbTranslateTrueColour32to16swap1 },
      { rfbTranslateTrueColour32to32swap0,rfbTranslateTrueColour32to32swap1 } }
};


/*
 * This is an optimisation of the general 32->8 converter for the
 * BGR888 to BGR233 case.
 */

static void
rfbTranslateBGR888toBGR233(cl, iptr, optr, iptradd, optradd, nlines)
    rfbClientPtr cl;
    char *iptr;
    char *optr;
    int iptradd;
    int optradd;
    int nlines;
{
    register CARD32 *ip = (CARD32 *)iptr;
    register CARD8 *op = (CARD8 *)optr;
    register CARD32 i;
    register CARD8 *opLineEnd;
    int ipadd = (iptradd / 4) - optradd;

    while (nlines > 0) {
	opLineEnd = op + optradd;
	while (op < opLineEnd) {
	    i = *(ip++);
	    *(op++) = (((i & 0xc00000) >> 16) |
		       ((i & 0x00e000) >> 10) |
		       ((i & 0x0000e0) >> 5));
	}

	ip += ipadd;
	nlines--;
    }
}


/*
 * This is an optimisation of the general 16->8 converter for the
 * BGR556 to BGR233 case.
 */

static void
rfbTranslateBGR556toBGR233(cl, iptr, optr, iptradd, optradd, nlines)
    rfbClientPtr cl;
    char *iptr;
    char *optr;
    int iptradd;
    int optradd;
    int nlines;
{
    register CARD16 *ip = (CARD16 *)iptr;
    register CARD8 *op = (CARD8 *)optr;
    register CARD16 i;
    register CARD8 *opLineEnd;
    int ipadd = (iptradd / 2) - optradd;

    while (nlines > 0) {
	opLineEnd = op + optradd;
	while (op < opLineEnd) {
	    i = *(ip++);
	    *(op++) = (((i & 0xc000) >> 8) |
		       ((i & 0x0700) >> 5) |
		       ((i & 0x0038) >> 3));
	}

	ip += ipadd;
	nlines--;
    }
}


/*
 * Functions for translating from using a colour map to true colour.
 */

static CARD32 rfbColourMapToTrueColour[256];

#define DEFINE_TRANSLATE_COLOURMAP_TO_TRUECOLOUR(OUT,SWAP)		     \
static void								     \
rfbTranslateColourMapToTrueColour##OUT##swap##SWAP (cl, iptr, optr, iptradd, \
						  optradd, nlines)	     \
    rfbClientPtr cl;							     \
    char *iptr;								     \
    char *optr;								     \
    int iptradd;							     \
    int optradd;							     \
    int nlines;								     \
{									     \
    register CARD8 *ip = (CARD8 *)iptr;					     \
    register CARD##OUT *op = (CARD##OUT *)optr;				     \
    int opadd = optradd * 8 / OUT;					     \
    int ipadd = iptradd - (optradd * 8 / OUT);				     \
    register CARD8 i;							     \
    register CARD##OUT o;						     \
    register CARD##OUT *opLineEnd;					     \
									     \
    while (nlines > 0) {						     \
	opLineEnd = op + opadd;						     \
	while (op < opLineEnd) {					     \
	    i = *(ip++);						     \
	    o = rfbColourMapToTrueColour[i];				     \
	    if (SWAP) {							     \
		o = Swap##OUT (o);					     \
	    }								     \
	    *(op++) = o;						     \
	}								     \
									     \
	ip += ipadd;							     \
	nlines--;							     \
    }									     \
}

DEFINE_TRANSLATE_COLOURMAP_TO_TRUECOLOUR(8,0)
DEFINE_TRANSLATE_COLOURMAP_TO_TRUECOLOUR(16,0)
DEFINE_TRANSLATE_COLOURMAP_TO_TRUECOLOUR(16,1)
DEFINE_TRANSLATE_COLOURMAP_TO_TRUECOLOUR(32,0)
DEFINE_TRANSLATE_COLOURMAP_TO_TRUECOLOUR(32,1)

rfbTranslateFnType rfbTranslateColourMapToTrueColourFns[3][2] = {
    { rfbTranslateColourMapToTrueColour8swap0,
      rfbTranslateColourMapToTrueColour8swap0 },
    { rfbTranslateColourMapToTrueColour16swap0,
      rfbTranslateColourMapToTrueColour16swap1 },
    { rfbTranslateColourMapToTrueColour32swap0,
      rfbTranslateColourMapToTrueColour32swap1 }
};



/*
 * rfbSetTranslateFunction sets the translation function.
 */

Bool
rfbSetTranslateFunction(cl)
    rfbClientPtr cl;
{
    fprintf(stderr,"Client Pixel Format:\n");
    PrintPixelFormat(&cl->format);

    /* first check that bits per pixel values are valid */

    if ((rfbServerFormat.bitsPerPixel != 8) &&
	(rfbServerFormat.bitsPerPixel != 16) &&
	(rfbServerFormat.bitsPerPixel != 32))
    {
	fprintf(stderr,"%s: server bits per pixel not 8, 16 or 32\n",
		"rfbSetTranslateFunction");
	rfbCloseSock(cl->sock);
	return FALSE;
    }

    if ((cl->format.bitsPerPixel != 8) &&
	(cl->format.bitsPerPixel != 16) &&
	(cl->format.bitsPerPixel != 32))
    {
	fprintf(stderr,"%s: client bits per pixel not 8, 16 or 32\n",
		"rfbSetTranslateFunction");
	rfbCloseSock(cl->sock);
	return FALSE;
    }


    if (rfbServerFormat.trueColour) {

	/* server is true colour */

	if (PF_EQ(cl->format,ColourMap8BitFormat)) {

	    /* client has 8-bit colour map, just treat it as BGR233 */

	    if (!rfbSetClientColourMapBGR233(cl))
		return FALSE;

	    /* now cl->format IS true colour so drop through */
	}

	if (!cl->format.trueColour) {

	    /* client has colour map but not 8-bit, can't cope yet */

	    fprintf(stderr,"%s: client has colour map but %d-bit\n",
		    "rfbSetTranslateFunction", cl->format.bitsPerPixel);
	    rfbCloseSock(cl->sock);
	    return FALSE;
	}

	/* true colour -> true colour translation */

	if (PF_EQ(cl->format,rfbServerFormat)) {

	    /* client & server the same */

	    fprintf(stderr,"no translation needed\n");
	    cl->translateFn = rfbTranslateNone;
	    return TRUE;
	}

	if (PF_EQ(cl->format,BGR233Format) &&
	    (PF_EQ(rfbServerFormat,BGR888LittleEndianFormat) ||
	     PF_EQ(rfbServerFormat,BGR888BigEndianFormat)))
	{
	    /* BGR888 -> BGR233 is optimised */

	    fprintf(stderr,"BGR888 to BGR233\n");
	    cl->translateFn = rfbTranslateBGR888toBGR233;
	    return TRUE;
	}

	if (PF_EQ(cl->format,BGR233Format) &&
	    (PF_EQ(rfbServerFormat,BGR556LittleEndianFormat) ||
	     PF_EQ(rfbServerFormat,BGR556BigEndianFormat)))
	{
	    /* BGR556 -> BGR233 is optimised */

	    fprintf(stderr,"BGR556 to BGR233\n");
	    cl->translateFn = rfbTranslateBGR556toBGR233;
	    return TRUE;
	}

	/* else look up standard true colour translation function in array */

	cl->translateFn
	  = rfbTranslateTrueColourFns
	    [rfbServerFormat.bitsPerPixel / 16]
	    [cl->format.bitsPerPixel / 16]
	    [(rfbServerFormat.bigEndian != cl->format.bigEndian)?1:0];

	return TRUE;
    }


    /* server has colour map */

    if (!PF_EQ(rfbServerFormat,ColourMap8BitFormat)) {

	/* server has colour map but not 8-bit, can't cope yet */

	fprintf(stderr,"%s: server has colour map but %d-bit\n",
		"rfbSetTranslateFunction", rfbServerFormat.bitsPerPixel);
	rfbCloseSock(cl->sock);
	return FALSE;
    }

    if (cl->format.trueColour) {

	/* 8-bit colour map -> true colour translation */

	fprintf(stderr,
		"%s: client is %d-bit trueColour, server has colour map\n",
		"rfbSetTranslateFunction",cl->format.bitsPerPixel);

	cl->translateFn
	  = rfbTranslateColourMapToTrueColourFns
	    [cl->format.bitsPerPixel / 16]
	    [(rfbServerFormat.bigEndian != cl->format.bigEndian)?1:0];

	return rfbSetClientColourMap(cl, 0, 0);
    }

    if (!PF_EQ(cl->format,ColourMap8BitFormat)) {

	/* client has colour map but not 8-bit, can't cope yet */

	fprintf(stderr,"%s: client has colour map but %d-bit\n",
		"rfbSetTranslateFunction", cl->format.bitsPerPixel);
	rfbCloseSock(cl->sock);
	return FALSE;
    }

    /* 8-bit colour map -> 8-bit colour map */

    fprintf(stderr,"%s: both 8-bit colour map: no translation needed\n",
	    "rfbSetTranslateFunction");

    cl->translateFn = rfbTranslateNone;
    return rfbSetClientColourMap(cl, 0, 0);
}



/*
 * rfbSetClientColourMapBGR233 sets the client's colour map so that it's
 * just like an 8-bit BGR233 true colour client.
 */

static Bool
rfbSetClientColourMapBGR233(cl)
    rfbClientPtr cl;
{
    char buf[sz_rfbSetColourMapEntriesMsg + 256 * 3 * 2];
    rfbSetColourMapEntriesMsg *scme = (rfbSetColourMapEntriesMsg *)buf;
    CARD16 *rgb = (CARD16 *)(&buf[sz_rfbSetColourMapEntriesMsg]);
    int i, len;
    int r, g, b;

    if (cl->format.bitsPerPixel != 8) {
	fprintf(stderr,"%s: client not 8 bits per pixel\n",
		"rfbSetClientColourMapBGR233");
	rfbCloseSock(cl->sock);
	return FALSE;
    }

    cl->format = BGR233Format;

    scme->type = rfbSetColourMapEntries;

    scme->firstColour = Swap16IfLE(0);
    scme->nColours = Swap16IfLE(256);

    len = sz_rfbSetColourMapEntriesMsg;

    i = 0;

    for (b = 0; b < 4; b++) {
	for (g = 0; g < 8; g++) {
	    for (r = 0; r < 8; r++) {
		rgb[i++] = Swap16IfLE(r * 65535 / 7);
		rgb[i++] = Swap16IfLE(g * 65535 / 7);
		rgb[i++] = Swap16IfLE(b * 65535 / 3);
	    }
	}
    }

    len += 256 * 3 * 2;

    if (WriteExact(cl->sock, buf, len) < 0) {
	perror("rfbSetClientColourMapBGR233: write");
	rfbCloseSock(cl->sock);
	return FALSE;
    }
    return TRUE;
}


/*
 * rfbSetClientColourMap is called to set the client's colour map.  If the
 * client is a true colour client, we simply update our own translation table
 * and mark the whole screen as having been modified.
 */

Bool
rfbSetClientColourMap(cl, firstColour, nColours)
    rfbClientPtr cl;
    int firstColour;
    int nColours;
{
    EntryPtr pent;
    int i, r, g, b;
    BoxRec box;

    if (nColours == 0) {
	nColours = rfbInstalledColormap->pVisual->ColormapEntries;
    }

    if (rfbServerFormat.trueColour || !cl->readyForSetColourMapEntries) {
	return TRUE;
    }

    if (cl->format.trueColour) {
	pent = (EntryPtr)&rfbInstalledColormap->red[firstColour];
	for (i = 0; i < nColours; i++) {
	    if (pent->fShared) {
		r = pent->co.shco.red->color;
		g = pent->co.shco.green->color;
		b = pent->co.shco.blue->color;
	    } else {
		r = pent->co.local.red;
		g = pent->co.local.green;
		b = pent->co.local.blue;
	    }
	    rfbColourMapToTrueColour[firstColour+i]
		= ((((r * cl->format.redMax + 32767) / 65535)
		    << cl->format.redShift) |
		   (((g * cl->format.greenMax + 32767) / 65535)
		    << cl->format.greenShift) |
		   (((b * cl->format.blueMax + 32767) / 65535)
		    << cl->format.blueShift));

	    pent++;
	}

	REGION_UNINIT(pScreen,&cl->modifiedRegion);
	box.x1 = box.y1 = 0;
	box.x2 = rfbScreen.width;
	box.y2 = rfbScreen.height;
	REGION_INIT(pScreen,&cl->modifiedRegion,&box,0);

	return TRUE;
    }

    return rfbSendSetColourMapEntries(cl, firstColour, nColours);
}


/*
 * rfbSetClientColourMaps sets the colour map for each RFB client.
 */

void
rfbSetClientColourMaps(firstColour, nColours)
    int firstColour;
    int nColours;
{
    rfbClientPtr cl;

    for (cl = rfbClientHead; cl; cl = cl->next) {
	rfbSetClientColourMap(cl, firstColour, nColours);
    }
}


static void
PrintPixelFormat(pf)
    rfbPixelFormat *pf;
{
    if (pf->bitsPerPixel == 1) {
	fprintf(stderr,"Single bit per pixel.\n");
	fprintf(stderr,
		"%s significant bit in each byte is leftmost on the screen.\n",
		(pf->bigEndian ? "Most" : "Least"));
    } else {
	fprintf(stderr,"%d bits per pixel.\n",pf->bitsPerPixel);
	if (pf->bitsPerPixel != 8) {
	    fprintf(stderr,"%s significant byte first in each pixel.\n",
		    (pf->bigEndian ? "Most" : "Least"));
	}
	if (pf->trueColour) {
	    fprintf(stderr,"True colour: max red %d green %d blue %d\n",
		    pf->redMax, pf->greenMax, pf->blueMax);
	    fprintf(stderr,"            shift red %d green %d blue %d\n",
		    pf->redShift, pf->greenShift, pf->blueShift);
	} else {
	    fprintf(stderr,"Uses a colour map (not true colour).\n");
	}
    }
}
