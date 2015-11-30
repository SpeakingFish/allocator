#include "callwrappers-hoard.h"

#define CUSTOM_DLLNAME HoardDLLMain
#define CUSTOM_PREFIX(n) hoard_##n
#include "hoard-38/src/heaplayers/util/cpuinfo.h"
#include "hoard-38/src/heaplayers/util/sassert.h"
#include "hoard-38/src/threadpoolheap.h"
#include "hoard-38/src/libhoard.h"
#include "hoard-38/src/wintls.cpp"
#include "hoard-38/src/heaplayers/wrapper.cpp"

static const int s_numberOfProcessors = HL::CPUInfo::computeNumProcessors();

void hoard_dllProcessAttach()
{
	LocalTLABIndex = TlsAlloc();
	if (LocalTLABIndex == TLS_OUT_OF_INDEXES) {
		// Not sure what to do here!
	}
	getCustomHeap();
}

void hoard_dllProcessDetach()
{
	HeapAlloc(GetProcessHeap(), 0, 1);
}

void hoard_dllThreadAttach()
{
	if (s_numberOfProcessors == 1) {
		// We have exactly one processor - just assign the thread to
		// heap 0.
		getMainHoardHeap()->chooseZero();
	} else {
		getMainHoardHeap()->findUnusedHeap();
	}
	getCustomHeap();
}

void hoard_dllThreadDetach()
{
	// Dump the memory from the TLAB.
	getCustomHeap()->clear();

	TheCustomHeapType *heap = (TheCustomHeapType *) TlsGetValue(LocalTLABIndex);

	if (s_numberOfProcessors != 1)
	{
		// If we're on a multiprocessor box, relinquish the heap
		// assigned to this thread.
		getMainHoardHeap()->releaseHeap();
	}

	if (heap != 0)
	{
		TlsSetValue (LocalTLABIndex, 0);
	}
}

AllocatorFunctions hoardAllocatorFunctions()
{
	static const AllocatorFunctions functions =
	{
		hoard_dllProcessAttach,
		hoard_dllProcessDetach,
		hoard_dllThreadAttach,
		hoard_dllThreadDetach,

		NULL,
		NULL,

		hoard_malloc,
		hoard_free,

		hoard_memalign,
		hoard_calloc,
		hoard_realloc,

		hoard_malloc_usable_size
	};

	return functions;
}
