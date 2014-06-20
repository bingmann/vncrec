/*
 * tableinitcmtemplate.c - template for initialising lookup tables for
 * translation from a colour map to true colour.
 *
 * This file shouldn't be compiled.  It is included multiple times by
 * translate.c, each time with a different definition of the macro OUT.
 * For each value of OUT, this file defines a function which allocates an
 * appropriately sized lookup table and initialises it.
 *
 * I know this code isn't nice to read because of all the macros, but
 * efficiency is important here.
 */

#if !defined(OUT)
#error "This file shouldn't be compiled."
#error "It is included as part of translate.c"
#endif

#define OUT_T CONCAT2E(CARD,OUT)
#define SwapOUT(x) CONCAT2E(Swap,OUT) (x)
#define rfbInitColourMapSingleTableOUT \
				CONCAT2E(rfbInitColourMapSingleTable,OUT)

static void
rfbInitColourMapSingleTableOUT (char **table, rfbPixelFormat *in,
				rfbPixelFormat *out)
{
    int i, r, g, b;
    OUT_T *t;
    EntryPtr pent;
    int nEntries = 1 << in->bitsPerPixel;

    if (*table) free(*table);
    *table = (char *)malloc(nEntries * sizeof(OUT_T));
    t = (OUT_T *)*table;

    pent = (EntryPtr)&rfbInstalledColormap->red[0];

    for (i = 0; i < nEntries; i++) {
	if (pent->fShared) {
	    r = pent->co.shco.red->color;
	    g = pent->co.shco.green->color;
	    b = pent->co.shco.blue->color;
	} else {
	    r = pent->co.local.red;
	    g = pent->co.local.green;
	    b = pent->co.local.blue;
	}
	t[i] = ((((r * out->redMax + 32767) / 65535) << out->redShift) |
		(((g * out->greenMax + 32767) / 65535) << out->greenShift) |
		(((b * out->blueMax + 32767) / 65535) << out->blueShift));
#if (OUT != 8)
	if (out->bigEndian != in->bigEndian) {
	    t[i] = SwapOUT(t[i]);
	}
#endif
	pent++;
    }
}

#undef OUT_T
#undef SwapOUT
#undef rfbInitColourMapSingleTableOUT
