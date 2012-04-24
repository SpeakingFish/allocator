#include "allocatorapi.h"

#include "nedmalloc/nedmalloc.h"

ALLOCATOR_EXPORT AllocatorMallInfo allocatorMallInfo(void)
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

#ifdef ALLOCATOR_USE_STATISTICS
ALLOCATOR_EXPORT AllocatorStatistics allocatorStatistics(void)
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
#endif
