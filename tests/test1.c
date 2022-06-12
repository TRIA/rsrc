//
//  main.c
//  port-rsrc
//
//  Created by Steve Bunch on 5/19/22.
//

#include <stdio.h>
#include <assert.h>
#include "rsrc.h"

rsrcPoolP_t p1, p2, p3, p4;

typedef void * RS;

uint8_t *r1, *r2, *r5;

RS r3, r4, r6, r7, r8, r9, r10;

void OOMstub(rsrcPoolP_t pool, int amount)
{
	printf ("User-suppied out-of-memory function was called.  Returning...\n");
	return; // any return at all says to just continue instead of abort
}

int classID_RFU = 0;

int main(int argc, const char * argv[]) {
	p1 = pxRsrcNewPool ("bytepool", 1, 1, 1, 0); // one byte per alloc, 1 init, 1 inc
	assert(p1);
	p2 = pxRsrcNewPool ("longpool", 8, 0, 100, 0); // 8 bytes per, 0 init, 100 inc
	assert(p2);
	p3 = pxRsrcNewVarPool("varpool", 50);
	assert(p3);
	
	printf ("\nShort p1 alone (no allocations done yet):\n");
	vRsrcPrintShort (p1);
	printf ("\nShort all p*:\n");
	vRsrcPrintShort (NULL);
	
	printf ("\nLong all p* (no allocations):\n");
	vRsrcPrintLong(NULL);
	
	vRsrcSetPrintHelper (p1, vRsrcPrintResInHexHelper);
	vRsrcSetAllocHelper (p1, vRsrcSetResToOnesHelper);
	printf ("Helper vRsrcPrintResInHexHelper and vRsrcSetResToOnesHelper set for p1, now allocating r1 and r2 in p1\n");
	r1 = pxRsrcAlloc (p1, "first");
	vRsrcPrintResource ("r1: ", r1);
	r2 = pxRsrcAlloc (p1, "second");
	vRsrcPrintResource ("r2: ", r2);
	printf ("\np1 long, after 2 allocations:\n");
	vRsrcPrintLong(p1);
	
	printf ("\nallocating 2 in p2\n");
	r3 = pxRsrcAlloc (p2, "firstp2");
	vRsrcPrintResource ("this is r3", r3);
	r4 = pxRsrcAlloc (p2, "secondp2");
	vRsrcPrintResource ("this is r4, secondp2", r4);
	printf ("\neverything long, after 2 allocations in p2:\n");
	vRsrcPrintLong (NULL);
	
	vRsrcSetAllocHelper(p3, vRsrcSetResToOnesHelper);
	r5 = pxRsrcVarAlloc(p3, "firstp3", 10000);
	vRsrcPrintResource("variable resource p3", r5);
	r6 = pxRsrcVarAlloc(p3, "secondp3", 1024);
	for (int i = 0; i < 10000; i++) // try to fail...
		r5[i] = 0xef;
	vRsrcSetPrintHelper(p3, vRsrcPrintResInHexHelper);
	
	printf("About to free r1, r2. Clearing eRsrcClearResOnAlloc\n");
	eRsrcClearResOnAlloc = rsrcNOCLEAR;
	vRsrcFree(r1);
	vRsrcSetFreeHelper (p1, vRsrcSetResToZerosHelper);
	vRsrcFree(r2);
	printf ("r1's free helper was unmodified, but before freeing r2, free helper was SetToZeroes.\n");
	printf ("We now use-after-free to check the residue in r1 and r2:\n");
	printf ("r1[0] = 0x%x, r2[0] = 0x%x\n", r1[0], r2[0]);
	vRsrcPrintResource("r3, about to free r3\n", r3);
	vRsrcFree(r3);
	vRsrcRenameRsrc (r4, "renamed r4");

	
	printf ("\nAfter freeing r1, r2, r3, and renaming r4, allocating r5, everything long:\n");
	vRsrcPrintLong (NULL);
	vRsrcFree (r5);
	
#ifdef rsrcTEST_OOM
	printf ("Compiled with rsrcTEST_OOM defined, so we'll try to run out of memory:\n");
	vRsrcOOMfn = OOMstub;
	
	vRsrcFree(r4); // empty out p2 so we know the count accurately when it dies
	for (int i = 0; ; i++) {
		void *ignored = pxRsrcAlloc(p2, "main");
		
		if (!ignored) {
			printf ("Got a NULL back after %d allocations\n", i);
			break;
		}
	}
	vRsrcPrintShort(p2);
	// now, let's go out with a bang
	printf ("That worked.  Now attempting double-free on r1.  This should abort:\n");
	vRsrcFree(r1);
	printf ("TEST FAILED. This should not be printed, we should have aborted before now\n");
#endif /* rsrcTEST_OOM */

	printf ("Finished normally\n");
	return 0;
}
