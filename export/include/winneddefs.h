#pragma once

#if defined(BUILD_WINNED_LIB)
#define WINNED_EXPORT __declspec(dllexport)
#else
#define WINNED_EXPORT __declspec(dllimport)
#endif

#ifdef x64
#define MALLOC_ALIGNMENT ((size_t)16U)
#else
#define MALLOC_ALIGNMENT ((size_t)8U)
#endif

#define DEFAULT_GRANULARITY (64*1024)

#define ENABLE_LARGE_PAGES 1

#define REALLOC_ZERO_BYTES_FREES 1

#define FORCE_OUR_HEAP_DETECTION
#ifdef FORCE_OUR_HEAP_DETECTION
	#define ENABLE_FAST_HEAP_DETECTION
#endif

#define MAXIMUM_THREADS_COUNT 256

// try to optimize
// #define DEFAULTMAXTHREADSINPOOL 1
// #define MAXTHREADSINPOOL 1
// #define THREADCACHEMAXCACHES 2048
// #define THREADCACHEMAXFREESPACE (16 * 1024 * 1024)
// #define THREADCACHEMAX (32 * 1024)
// #define THREADCACHEMAXBINS ((10 + 5) - 4)


// now disabled as unstable
// #define THROWSPEC throw()
// #define ONLY_MSPACES 1
// #define MMAP_CLEARS 0
// #define ENABLE_USERMODEPAGEALLOCATOR 1

/// \brief Returns information about a memory pool
struct NedSummaryInfo
{
	int threadId;
	__int64 totalAllocatedBytes;
	__int64 totalAllocationsCount;
	__int64 totalReallocatedBytesDelta;
	__int64 totalReallocationsCount;
	__int64 totalDeallocatedBytes;
	__int64 totalDeallocationsCount;
};

struct NedStatistics
{
	struct NedSummaryInfo nedInfo;
	struct NedSummaryInfo threadsInfo[MAXIMUM_THREADS_COUNT];
};

struct NedMallInfo
{
	size_t maxAllocated;     ///< maximum total allocated space
	size_t currentAllocated; ///< total allocated space
	size_t currentFree;      ///< total free space
	size_t mayBeRelease;     ///< releasable (via malloc_trim) space
	size_t allocated;        ///< non-mmapped space allocated from system
	size_t mmaped;           ///< space in mmapped regions
	size_t freeChunks;       ///< number of free chunks
};
