//
//  main.c
//  port-rsrc
//
//  Created by Steve Bunch on 5/19/22.
//

#include <stdio.h>
#include <assert.h>
#include "rsrc.h"

#define TEST_SUCCEEDED -1
#define TEST_FAILED 1
#define TEST_NOT_RUN 0

//#define rsrcDOUBLE_FREE							// if defined, test will try a double-tree to see if it aborts

/*
 * Variables are globals so that the debugger can see them.
 */
rsrcPoolP_t p1, p2, p3, p4, p5;

typedef void * RS;

uint8_t *r1, *r2, *r5;

RS r3, r4, r6, r7, r8, r9, r10;

void OOMstub(rsrcPoolP_t pool, int amount)
{
	printf ("User-suppied out-of-memory function was called.  Returning to continue...\n");
	return; // any return at all says to just continue instead of abort
}

int test_varpool(void)
{
	const int bigsize = 10000;
	
	// create a Var pool, allocate a few variable-sized resources, trying one too many
	p3 = pxRsrcNewVarPool("varpool", 2);
	assert(p3);
	r5 = pxRsrcVarAlloc(p3, "firstp3", bigsize);
	assert(r5);
	r6 = pxRsrcVarAlloc(p3, "secondp3", 1024);
	assert(r6);
	if (p3->uiNumInUse != 2 || p3->uxSizeEach != 0 || p3->uiNumFree)
		return TEST_FAILED;
	for (int i = 0; i < bigsize; i++) // try to touch it all...
		r5[i] = 0xef;
	r7 = pxRsrcVarAlloc(p3, "too many", 1000);  // this should not succeeed
	if (p3->uiNumInUse > 2 || r7) {
		return TEST_FAILED;
	}
	vRsrcFree (r5);
	vRsrcFree (r6);
	return (TEST_SUCCEEDED);  // Failure is a memory fault on any of the above
}

int test_stats (void)
{
	const int poolsize = 100, repeats = 3;
	
	// p2 is a static pool with enough in it to be interesting
	p2 = pxRsrcNewPool ("longpool", 8, 0, poolsize, 0); // static pool: 8 bytes per, 0 init, 100 inc
	assert(p2);

	if (p2->uiNumInUse || p2->uiNumFree || p2->ulTotalAllocs || p2->uiHiWater || p2->uiLowWater)
		return TEST_FAILED;
	
	for (int i = 0; i < repeats; i++) {
		void *results[poolsize/2];
		
		for (int j = 0; j < poolsize/2; j++) {
			results[j] = pxRsrcAlloc(p2, "loop poolsize/2");
			assert(results[j]);
			// have we seen it before?
			for (int k = 0; k < j; k++) {
				if (results[k] == results[j])
					return TEST_FAILED;
			}
		}
		for (int j = 0; j < poolsize/2; j++)  {
			vRsrcFree(results[j]);
		}
	}
	if (p2->uiNumFree != poolsize
		|| p2->uiNumInUse != 0
		|| p2->ulTotalAllocs != repeats * poolsize/2
		|| p2->uiLowWater != poolsize/2
		|| p2->uiHiWater != poolsize/2) {
		return TEST_FAILED;
	}
	return TEST_SUCCEEDED;
}

// Set these to 0 then "print" to verify that they are called the right number of times
int prints = 0;
rsrcPoolP_t pool2Expect = NULL;
// set these as helpers to NOT print, but just to see if you get the expected calls
void fakePoolPrint (rsrcPoolP_t pool, void *res)
{
	prints++;
}

extern struct rsrcPool xRsrcPoolPool;  // not advertised, but we know his name...
int test_print (void)
{
	rsrcPrintHelper_t *ph = xRsrcPoolPool.pxPrintHelper;  // save
	
	// does not check for correct printing, just correct nubmer of calls
	p5 = pxRsrcNewPool("PrintTest", 32, 2, 0, 0); // two 32-byte resources
	assert(p5);
	r5 = pxRsrcAlloc(p5, "r5 in p5");
	assert (r5);
	
	// Set up print helper for Pools and for PrintTest to fakePoolPrint
	vRsrcSetPrintHelper(p5, fakePoolPrint);
	vRsrcSetPrintHelper(&xRsrcPoolPool, fakePoolPrint);
	// print a known number of things, see if they get counted
	prints = 0;
	vRsrcPrintShort(p5); // prints should now be 1 for PrintTest pool
	vRsrcPrintLong(p5); // prints should pick up two more - PrintTest and r5
	// restore main resource pool print routine in case we need it later
	vRsrcSetPrintHelper(&xRsrcPoolPool, ph);
	
	if (prints != 3)
		return TEST_FAILED;
	vRsrcFree(r5);
	return TEST_SUCCEEDED;
}

int test_helpers (void)
{
	enum rsrcClearOnAllocSetting_t t = eRsrcClearResOnAlloc; // save so we can restore
	
	eRsrcClearResOnAlloc = rsrcNOCLEAR;	// turn off meddling behavior
	// create a static pool, set the helpers to set them to 1's, 0's (opposite of eRsrcClearResOnAlloc)
	p4 = pxRsrcNewPool ("helperpool", 256, 2, 0, 0);
	assert(p4);
	vRsrcSetAllocHelper(p4, vRsrcSetResToOnesHelper);
	vRsrcSetFreeHelper(p4, vRsrcSetResToZerosHelper);
	
	// allocate a resource, check it for 1's.  Free it.  Check after free to see if it's zeroes.
	r5 = pxRsrcAlloc(p4, "helpertest");
	assert(r5);
	if (r5[0] != 0xff || r5[255] != 0xff)
		return TEST_FAILED + 1;
	vRsrcFree(r5);
	eRsrcClearResOnAlloc = t; // reset global
	
	// cheat -- peek into the freed resource's body and see if it got cleared
	if (r5[0] !=  0 || r5[255] != 0)
		return TEST_FAILED + 1;
	return TEST_SUCCEEDED;
}

int test_dynpool(void)
{
	p1 = pxRsrcNewPool ("bytepool", 1, 1, 1, 0); // dynamic pool: one byte per alloc, 1 init, 1 inc
	assert(p1);
	r1 = pxRsrcAlloc (p1, "r1 in bytepool");
	assert (r1);
	r2 = pxRsrcAlloc(p1, "r2 in bytepool");
	if (p1->uiNumFree || p1->uiNumInUse != 2)
		return TEST_FAILED;
	vRsrcFree(r1);
	if (p1->uiNumFree || p1->uiNumInUse != 1)
		return TEST_FAILED;
	vRsrcFree(r2);
	if (p1->uiNumFree || p1->uiNumInUse != 0)
		return TEST_FAILED;
	return TEST_SUCCEEDED;
}

int test_oom (rsrcPoolP_t pPool)
{
#ifdef rsrcTEST_FORCE_OOM
//	printf ("  Compiled with rsrcTEST_FORCE_OOM defined, so we'll try to run out of memory:\n");
	vRsrcOOMfn = OOMstub;
	
	// usually, rsrcTEST_OOM is a modest number.  We should fail before we hit it.
	for (int i = 0; i < rsrcTEST_FORCE_OOM + 1; i++) {
		void *ignored = pxRsrcAlloc(pPool, "main");
		
		if (!ignored) {
//			printf ("  Got a NULL back after %d allocations\n", i);
			goto success;
		}
	}
	return (TEST_FAILED);
success:
	return (TEST_SUCCEEDED);
#else
	return (TEST_NOT_RUN);
#endif	/* rsrcTEST_FORCE_OOM */
}

int test_doublefree (void *res)
{
#ifdef rsrcDOUBLE_FREE
	// now, let's go out with a bang (eventually, this will catch the signals so it can return)
	printf ("Attempting double-free on res.  If this results in abort(), the test PASSED.\n");
	fflush(stdout); fflush(stderr);
	vRsrcFree(res);
	vRsrcFree(res);
	fprintf (stderr, "DOUBLE-FREE TEST FAILED. This should not be printed, we should have aborted before now\n");
	return TEST_FAILED;
#else
	return (TEST_NOT_RUN);
#endif /* rsrcDOUBLE_FREE */
}

void runtest(const char *testname, int result)
{
	if (result < 0) {
		fprintf (stdout, "Test %s PASSED\n", testname);
	}
	else {
		if (result == 0)
			fprintf (stdout, "Test %s was NOT RUN\n", testname);
		else
			fprintf (stderr, "Test %s FAILED (%d)\n", testname, result);
	}
}

int main(int argc, const char * argv[])
{
#ifndef rsrcTEST_FORCE_OOM
	runtest ("Variable Pool allocation", test_varpool());  // generates p3
	runtest ("Dynamic Pool allocation", test_dynpool());  // generates p1
	runtest ("Pool stats", test_stats());  // generates static pool p2
	runtest ("Printing test", test_print());  // generates p5
	runtest ("Helper alloc/free", test_helpers());  // generates p4
	
	// The following two tests require special compilations of the program, run manually
	r1 = pxRsrcAlloc(p2, "r1 in p2"); // allocate BEFORE we OOM, just in case we run that
	assert(r1);
#else
	p1 = pxRsrcNewPool ("bytepool", 1, 1, 1, 0); // dynamic pool: one byte per alloc, 1 init, 1 inc
	runtest ("OOM test", test_oom(p1));
#endif
	runtest ("Double free (abort == SUCCESS)", test_doublefree(r1)); // rsrc must be from static pool

	printf ("----> '%s' finished normally\n", argv[0]);
	return 0;
}
