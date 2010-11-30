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

// now disabled as unstable
// #define THROWSPEC throw()
// #define ONLY_MSPACES 1
// #define MMAP_CLEARS 0
// #define ENABLE_USERMODEPAGEALLOCATOR 1

/// \brief Returns information about a memory pool
/// it is same as nedmallinfo
struct NedMallInfo { 
	size_t arena;    ///< non-mmapped space allocated from system
	size_t ordblks;  ///< number of free chunks
	size_t smblks;   ///< always 0
	size_t hblks;    ///< always 0
	size_t hblkhd;   ///< space in mmapped regions
	size_t usmblks;  ///< maximum total allocated space
	size_t fsmblks;  ///< always 0
	size_t uordblks; ///< total allocated space
	size_t fordblks; ///< total free space
	size_t keepcost; ///< releasable (via malloc_trim) space
};
