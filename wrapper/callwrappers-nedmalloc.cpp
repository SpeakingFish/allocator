#include "callwrappers-nedmalloc.h"

#include "nedmalloc/nedmalloc.h"

void nedmalloc_dllThreadDetach()
{
	// Destroy the thread cache for all known pools
	nedpool **pools = nedpoollist();
	if(pools)
	{
		nedpool **pool;
		for(pool = pools; *pool; ++pool)
		{
			neddisablethreadcache(*pool);
		}
		nedfree(pools);
	}
	neddisablethreadcache(0);
}

AllocatorMallInfo nedmalloc_allocatorMallInfo(void)
{
	AllocatorMallInfo result;

	struct nedmallinfo info = nedmallinfo();

	result.allocated = info.arena;
	result.freeChunks = info.ordblks;
	result.mmaped = info.hblkhd;
	result.maxAllocated = info.usmblks;
	result.currentAllocated = info.uordblks;
	result.currentFree = info.fordblks;
	result.mayBeRelease = info.keepcost;

	return result;
}

AllocatorStatistics nedmalloc_allocatorStatistics(void)
{
	AllocatorStatistics result;

	nedstats_t info = {0};
	nedstats(&info);

	memset(&result.allocatorInfo, 0, sizeof(AllocatorSummaryInfo));

	for (int i = 0; i < MAXIMUM_THREADS_COUNT; ++i)
	{
		result.threadsInfo[i] = info.info[i];
		if (result.threadsInfo[i].threadId <= 0)
		{
			continue;
		}
		result.allocatorInfo.totalAllocatedBytes += result.threadsInfo[i].totalAllocatedBytes;
		result.allocatorInfo.totalAllocationsCount += result.threadsInfo[i].totalAllocationsCount;
		result.allocatorInfo.totalReallocatedBytesDelta += result.threadsInfo[i].totalReallocatedBytesDelta;
		result.allocatorInfo.totalReallocationsCount += result.threadsInfo[i].totalReallocationsCount;
		result.allocatorInfo.totalDeallocatedBytes += result.threadsInfo[i].totalDeallocatedBytes;
		result.allocatorInfo.totalDeallocationsCount += result.threadsInfo[i].totalDeallocationsCount;
	}
	return result;
}

AllocatorFunctions nedmallocAllocatorFunctions()
{
	static const AllocatorFunctions functions =
	{
		NULL,
		NULL,
		NULL,
		nedmalloc_dllThreadDetach,

		nedmalloc_allocatorMallInfo,
		nedmalloc_allocatorStatistics,

		nedmalloc,
		nedfree,

		nedmemalign,
		nedcalloc,
		nedrealloc,

		nedmemsize
	};

	return functions;
}

// NOTICE: this is user mode page allocator for nedmalloc. This code not stabilized with this wrapper.
// But this code must give >10% boost on large allocations for nedmalloc

// #ifdef ENABLE_USERMODEPAGEALLOCATOR
// #define DebugPrint printf
// #define USERPAGE_NOCOMMIT                  (M2_CUSTOM_FLAGS_BEGIN<<1)
// extern "C"
// {
// 	extern void *userpage_malloc(size_t toallocate, unsigned flags);
// 	extern int userpage_free(void *mem, size_t size);
// 	extern void *userpage_realloc(void *mem, size_t oldsize, size_t newsize, int flags, unsigned flags2);
// 	extern void *userpage_commit(void *mem, size_t size);
// 	extern int userpage_release(void *mem, size_t size);
// }
// static LPVOID WINAPI VirtualAlloc_winned(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
// {
// 	LPVOID ret=0;
// #if defined(_DEBUG)
// 	DebugPrint("Winpatcher: VirtualAlloc(%p, %u, %x, %x) intercepted\n", lpAddress, dwSize, flAllocationType, flProtect);
// #endif
// 	if(!lpAddress && flAllocationType&MEM_RESERVE)
// 	{
// 		ret=userpage_malloc(dwSize, (flAllocationType&MEM_COMMIT) ? 0 : USERPAGE_NOCOMMIT);
// #if defined(_DEBUG)
// 		DebugPrint("Winpatcher: userpage_malloc returns %p\n", ret);
// #endif
// 	}
// 	else if(lpAddress && (flAllocationType&(MEM_COMMIT|MEM_RESERVE))==(MEM_COMMIT))
// 	{
// 		ret=userpage_commit(lpAddress, dwSize);
// #if defined(_DEBUG)
// 		DebugPrint("Winpatcher: userpage_commit returns %p\n", ret);
// #endif
// 	}
// 	if(!ret || (void *)-1==ret)
// 	{
// 		ret=VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
// #if defined(_DEBUG)
// 		DebugPrint("Winpatcher: VirtualAlloc returns %p\n", ret);
// #endif
// 	}
// 	return (void *)-1==ret ? 0 : ret;
// }
// static BOOL WINAPI VirtualFree_winned(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
// {
// #if defined(_DEBUG)
// 	DebugPrint("Winpatcher: VirtualFree(%p, %u, %x) intercepted\n", lpAddress, dwSize, dwFreeType);
// #endif
// 	if(dwFreeType==MEM_DECOMMIT)
// 	{
// 		if(-1!=userpage_release(lpAddress, dwSize)) return 1;
// 	}
// 	else if(dwFreeType==MEM_RELEASE)
// 	{
// 		if(-1!=userpage_free(lpAddress, dwSize)) return 1;
// 	}
// 	return VirtualFree(lpAddress, dwSize, dwFreeType);
// }
// static SIZE_T WINAPI VirtualQuery_winned(LPVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwSize)
// {
// #if defined(_DEBUG)
// 	DebugPrint("Winpatcher: VirtualQuery(%p, %p, %u) intercepted\n", lpAddress, lpBuffer, dwSize);
// #endif
// 	return VirtualQuery(lpAddress, lpBuffer, dwSize);
// }
// 
// #endif
