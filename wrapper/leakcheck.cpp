#include "leakcheck.h"

#ifdef LEAK_CHECK

#include "nedmalloc/nedmalloc.h"
#include "stackwalker.h"
#include "callwrappers.h"

#include <map>
#include <list>
#include <set>
#include <list>

#define MLOCK_T               CRITICAL_SECTION
#define INITIAL_LOCK(s)       (!InitializeCriticalSectionAndSpinCount((s), 0x80000000|4000))
#define ACQUIRE_LOCK(s)       (EnterCriticalSection(s), 0)
#define RELEASE_LOCK(s)       LeaveCriticalSection(s)

#define TLS_UNINITIALIZED 0x0 // Thread local storage for the current thread is uninitialized.
#define TLS_DISABLED      0x1 // If set, memory leak detection is disabled for the current thread.
#define TLS_ENABLED       0x2 // If set, memory leak detection is enabled for the current thread.

typedef std::list<DWORD_PTR> CallStack;

struct LeakInfo
{
	LeakInfo() : index(0)
	{
	}

	CallStack callstack;
	size_t index;
};

typedef std::map<void*, LeakInfo> AllocationMap;

std::set<ptrdiff_t> s_leaksIndexCache;

struct LeakCheck
{
	LeakCheck() 
		: leakCheckTlsIndex(0)
		, internalAllocation(false)
		, stackWalker(NULL)
		, totalAllocations(0)
		, maxAllocations(0)
	{

	}

	~LeakCheck()
	{      
		TlsFree(leakCheckTlsIndex);
	}

	void init()
	{
		INITIAL_LOCK(&mutex);
		leakCheckTlsIndex = TlsAlloc();
		stackWalker = new StackWalker();
	}

	void beginForCurrentThread(NMLeakCheckOptions options);

	void endForCurrentThread();

	bool isEnabledForCurrentThread()
	{
		LPVOID state = TlsGetValue(leakCheckTlsIndex);

		if (state == TLS_UNINITIALIZED)
		{
			disableNMLeakCheckForCurrentThread();
			return false;
		}

		return state == (LPVOID)TLS_ENABLED;
	}

	void addAllocation(void* mem, size_t size);

	void removeAllocation(void* mem);

	void getReport(NMLeakCheckReport* report);

	void leaksStackVerbose(NMLeakCheckReport* report);

	AllocationMap allocations;
	size_t totalAllocations;
	size_t maxAllocations;

	DWORD leakCheckTlsIndex;
	bool internalAllocation;
	MLOCK_T mutex;
	NMLeakCheckOptions options;
	StackWalker* stackWalker;
};

LeakCheck s_leakCheck;

// ----------------------------------------------------

struct InternalAllocation
{
	InternalAllocation(LeakCheck* leakCheck) : leakCheck(leakCheck)
	{
		internalAllocation = leakCheck->internalAllocation;
		leakCheck->internalAllocation = true;
	}

	~InternalAllocation()
	{
		leakCheck->internalAllocation = internalAllocation;
	}

	LeakCheck* leakCheck;
	bool internalAllocation;
};


struct LockHelper
{
	LockHelper(LeakCheck* leakCheck) : leakCheck(leakCheck)
	{
		ACQUIRE_LOCK(&leakCheck->mutex);
	}

	~LockHelper()
	{
		RELEASE_LOCK(&leakCheck->mutex);
	}

	LeakCheck* leakCheck;
};

// ----------------------------------------------------

void LeakCheck::beginForCurrentThread(NMLeakCheckOptions options)
{
	TlsSetValue(leakCheckTlsIndex, (LPVOID)TLS_ENABLED);
	InternalAllocation internalAllocation(this);
	if (options.collectCallStack || options.useCollectedCallstackIndexes)
	{
		stackWalker->LoadModules();
	}
	this->options = options;
	LockHelper lock(this);
	allocations.clear();
	maxAllocations = 0;
	totalAllocations = 0;
}

void LeakCheck::endForCurrentThread()
{
	TlsSetValue(leakCheckTlsIndex, (LPVOID)TLS_DISABLED);
	LockHelper lock(this);
	InternalAllocation internalAllocation(this);
	allocations.clear();
	if (options.useCollectedCallstackIndexes)
	{
		s_leaksIndexCache.clear();
	}
}

void LeakCheck::addAllocation(void* mem, size_t size)
{
	if (internalAllocation || size < options.sizeGreateThan)
	{
		return;
	}

	InternalAllocation internalAllocation(this);
	LeakInfo info;
	if (options.collectCallStack)
	{
		stackWalker->GetCallStack(&info.callstack);
	}
	info.index = totalAllocations;
	if (options.useCollectedCallstackIndexes)
	{
		if (s_leaksIndexCache.find(info.index) != s_leaksIndexCache.end())
		{
			stackWalker->GetCallStack(&info.callstack);
		}
	}
	allocations[mem] = info;
	size_t currentSize = allocations.size();
	if (currentSize > maxAllocations)
	{
		maxAllocations = currentSize;
	}
	++totalAllocations;
}

void LeakCheck::removeAllocation(void* mem)
{
	AllocationMap::iterator iter = allocations.find(mem);
	if (iter != allocations.end())
	{
		InternalAllocation internalAllocation(this);
		allocations.erase(iter);
	}
}

void LeakCheck::getReport(NMLeakCheckReport* report)
{
	InternalAllocation internalAllocation(this);
	report->reportCount(allocations.size(), maxAllocations, totalAllocations);
	if (options.collectCallStack || options.useCollectedCallstackIndexes)
	{
		leaksStackVerbose(report);
	}
	if (options.collectIndexBasedCallstackLeaks)
	{
		for (AllocationMap::iterator iter = allocations.begin(); iter != allocations.end(); ++iter)
		{
			s_leaksIndexCache.insert(iter->second.index);
		}
	}
}

void LeakCheck::leaksStackVerbose(NMLeakCheckReport* report)
{
	InternalAllocation internalAllocation(this);
	for (AllocationMap::iterator iter = allocations.begin(); iter != allocations.end(); ++iter)
	{
		report->beginLeakReport();
		stackWalker->PrintCallStack(&iter->second.callstack, report);
		report->endLeakReport();
	}
}

// static interface

bool isNMLeakCheck()
{
	return true;
}

void getNMLeakCheckStat(NMLeakCheckReport* report)
{
	s_leakCheck.getReport(report);
}

bool isLeakCheckEnabledForCurrentThread()
{
	return s_leakCheck.isEnabledForCurrentThread();
}

void enableNMLeakCheckForCurrentThread(NMLeakCheckOptions options)
{
	s_leakCheck.beginForCurrentThread(options);
}

void disableNMLeakCheckForCurrentThread()
{
	LockHelper lock(&s_leakCheck);
	s_leakCheck.endForCurrentThread();
}

void* nedmalloc_leakcheck(size_t size)
{
	if (!isLeakCheckEnabledForCurrentThread())
	{
		return nedmalloc(size);
	}

	LockHelper lock(&s_leakCheck);

	void* result = nedmalloc(size);
	s_leakCheck.addAllocation(result, size);
	return result;
}

void* nedcalloc_leakcheck(size_t no, size_t size)
{
	if (!isLeakCheckEnabledForCurrentThread())
	{
		return nedcalloc(no, size);
	}

	LockHelper lock(&s_leakCheck);

	void* result = nedcalloc(no, size);
	s_leakCheck.addAllocation(result, no*size);
	return result;
}

void* nedmemalign_win_leakcheck(size_t size, size_t alignment)
{
	if (!isLeakCheckEnabledForCurrentThread())
	{
		return nedmemalign_win(size, alignment);
	}

	LockHelper lock(&s_leakCheck);

	void* result = nedmemalign_win(size, alignment);
	s_leakCheck.addAllocation(result, size);
	return result;
}

void nedfree_leakcheck(void *mem)
{
	if (s_leakCheck.internalAllocation)
	{
		nedfree(mem);
		return;
	}

	LockHelper lock(&s_leakCheck);
	s_leakCheck.removeAllocation(mem);
	nedfree(mem);
}

void* nedrealloc_leakcheck(void* mem, size_t size)
{
	void* result = nedrealloc(mem, size);
	if (result != mem)
	{
		LockHelper lock(&s_leakCheck);
		s_leakCheck.removeAllocation(mem);
		if (isLeakCheckEnabledForCurrentThread())
		{
			s_leakCheck.addAllocation(result, size);
		}
	}
	return result;
}

void* nedrecalloc_winned_leakcheck(void* mem, size_t num, size_t size)
{
	void* result = nedrecalloc_winned(mem, num, size);
	if (result != mem)
	{
		LockHelper lock(&s_leakCheck);
		s_leakCheck.removeAllocation(mem);
		if (isLeakCheckEnabledForCurrentThread())
		{
			s_leakCheck.addAllocation(result, num * size);
		}

	}
	return result;
}

void initLeakCheck()
{
	s_leakCheck.init();
}

#endif
