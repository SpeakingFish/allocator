#include "winnedmisc.h"

#include "nedmalloc.h"

struct NedStatistics nedStatistics(void)
{
	NedStatistics result;

	struct nedmallinfo info = nedmallinfo();

	result.allocated = info.arena;
	result.freeChunks = info.ordblks;
	result.mmaped = info.hblkhd;
	result.maxAllocated = info.usmblks;
	result.currentAllocated = info.uordblks;
	result.currentFree = info.fordblks;
	result.mayBeRelease = info.keepcost;

	result.nedInfo.threadId = -1;
	result.nedInfo.totalAllocatedBytes = 0;
	result.nedInfo.totalAllocationsCount = 0;
	result.nedInfo.totalReallocatedBytesDelta = 0;
	result.nedInfo.totalReallocationsCount = 0;
	result.nedInfo.totalDeallocatedBytes = 0;
	result.nedInfo.totalDeallocationsCount = 0;
	for (int i = 0; i < MAXIMUM_THREADS_COUNT; ++i)
	{
#ifdef NEDMALLOC_USE_STATISTICS
		result.threadsInfo[i] = info.info[i];
		if (result.threadsInfo[i].threadId <= 0)
		{
			continue;
		}
		result.nedInfo.totalAllocatedBytes += result.threadsInfo[i].totalAllocatedBytes;
		result.nedInfo.totalAllocationsCount += result.threadsInfo[i].totalAllocationsCount;
		result.nedInfo.totalReallocatedBytesDelta += result.threadsInfo[i].totalReallocatedBytesDelta;
		result.nedInfo.totalReallocationsCount += result.threadsInfo[i].totalReallocationsCount;
		result.nedInfo.totalDeallocatedBytes += result.threadsInfo[i].totalDeallocatedBytes;
		result.nedInfo.totalDeallocationsCount += result.threadsInfo[i].totalDeallocationsCount;
#else
		result.threadsInfo[i] = result.nedInfo;
#endif
	}

	return result;
}

