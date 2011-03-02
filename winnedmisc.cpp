#include "winnedmisc.h"

#include "nedmalloc.h"

struct NedMallInfo nedMallInfo(void)
{
	NedMallInfo result;

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

#ifdef NEDMALLOC_USE_STATISTICS
struct NedStatistics nedStatistics(void)
{
	NedStatistics result;

	struct nedstats info = nedstats();

	for (int i = 0; i < MAXIMUM_THREADS_COUNT; ++i)
	{
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
	}
	return result;
}
#endif
