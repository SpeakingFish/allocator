#include <stdio.h>
#include <windows.h>

#include "winnedmallocleakcheck.h"

#ifdef LEAK_CHECK
#include "stackwalker.h"
#include <map>
#include <list>
#include <set>
#endif

#define WIN32_LEAN_AND_MEAN

#if (_WIN32_WINNT < 0x0500)
#define _WIN32_WINNT 0x0500
#endif

#pragma inline_depth(255)

void (*winnedMemcpyPtr)(void* dest, const void* src, size_t count);
void (*winnedMemsetPtr)(void* dest, int c, size_t count);

#ifdef DEBUG
	LPCWSTR rlsCRTLibraryName[] = {L"MSVCR71D.DLL", L"MSVCR80D.DLL", L"MSVCR90D.DLL", L"MSVCR100D.DLL"};
#else
	LPCWSTR rlsCRTLibraryName[] = {L"MSVCR71.DLL", L"MSVCR80.DLL", L"MSVCR90.DLL", L"MSVCR100.DLL"};
#endif

extern "C"
{
#include "nedmalloc.h"
};

#include "winnedwrappers.h"

struct Patch;
static void patchIt(Patch* patch);

// Intercept the exit functions.

//static const int WINNED_MAX_EXIT_FUNCTIONS = 255;
//static int exitCount = 0;

extern "C" {

	__declspec(dllexport) int ReferenceNMStub;

//	typedef void (*exitFunctionType) (void);
//	exitFunctionType exitFunctionBuffer[255];
//
//	void winned_onexit (void (*function)(void)) {
//		if (exitCount < WINNED_MAX_EXIT_FUNCTIONS) {
//			exitFunctionBuffer[exitCount] = function;
//			exitCount++;
//		}
//	}
//
//	void winned_exit (int code) {
//		while (exitCount > 0) {
//			exitCount--;
//			(exitFunctionBuffer[exitCount])();
//		}
//	}
//
//	void * winned_expand (void * ptr) {
//		return NULL;
//	}

}

#ifdef x64

#pragma pack(push)
#pragma pack(1)
	struct CodeBlock
	{
		const unsigned char block1[6];// = { 0x83, 0xEC, 0x08, 0xC7, 0x04, 0x24 };
		unsigned int addressLow;
		const unsigned char block2[4];// = { 0xC7, 0x44, 0x24, 0x04 };
		unsigned int addressHigh;
		const unsigned char block3;   // = { 0xC3 };
	};
	unsigned char arr[] =
	{ 0x83, 0xEC, 0x08,                               // sub rsp, 8
	  0xC7, 0x04, 0x24, 0xDE, 0xAD, 0xBE, 0xEF,       // mov dword ptr [rsp], 0DEADBEEFh
	  0xC7, 0x44, 0x24, 0x04, 0xDE, 0xAD, 0xBE, 0xEF, // mov dword ptr [rsp + 4], 0DEADBEEFh
	  0xC3                                            // ret
	};
#pragma pack(pop)

struct Patch
{
	const char* import;                         // import name of patch routine
	FARPROC replacement;                        // pointer to replacement function
	FARPROC original;                           // pointer to original function
	unsigned char codebytes[sizeof(CodeBlock)]; // original code storage
};

static void patchIt(Patch* patch)
{
	// Change rights on CRT Library module to execute/read/write.
	MEMORY_BASIC_INFORMATION mbiThunk;
	VirtualQuery((void*)patch->original, &mbiThunk, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbiThunk.BaseAddress, mbiThunk.RegionSize, PAGE_EXECUTE_READWRITE, &mbiThunk.Protect);

	// back up
	(*winnedMemcpyPtr)(patch->codebytes, patch->original, sizeof(patch->codebytes));

	// patch
	unsigned char* patchLocation = (unsigned char*)patch->original;
	CodeBlock* block = (CodeBlock*)arr;
	block->addressLow = (unsigned int)((size_t)patch->replacement & 0xffffffffu);
	block->addressHigh = (unsigned int)(((size_t)patch->replacement & 0xffffffff00000000u) >> 32);
	memcpy(patchLocation, block, sizeof(CodeBlock));

	// Reset CRT library code to original page protection.
	VirtualProtect(mbiThunk.BaseAddress, mbiThunk.RegionSize, mbiThunk.Protect, &mbiThunk.Protect);
}

#elif WIN32

static const int bytesToStore = 1 + 4;
#define IAX86_NEARJMP_OPCODE	  0xe9
#define MakeIAX86Offset(to, from)  ((unsigned)((char*)(to)-(char*)(from)) - bytesToStore)

struct Patch
{
	const char* import;                    // import name of patch routine
	FARPROC replacement;                   // pointer to replacement function
	FARPROC original;                      // pointer to original function
	unsigned char codebytes[bytesToStore]; // original code storage
};

static void patchIt(Patch* patch)
{
	// Change rights on CRT Library module to execute/read/write.
	MEMORY_BASIC_INFORMATION mbiThunk;
	VirtualQuery((void*)patch->original, &mbiThunk, sizeof(MEMORY_BASIC_INFORMATION));
	VirtualProtect(mbiThunk.BaseAddress, mbiThunk.RegionSize, PAGE_EXECUTE_READWRITE, &mbiThunk.Protect);

	// back up
	(*winnedMemcpyPtr)(patch->codebytes, patch->original, sizeof(patch->codebytes));

	// patch
	unsigned char *patchLocation = (unsigned char*)patch->original;
	*patchLocation++ = IAX86_NEARJMP_OPCODE;
	*(unsigned*)patchLocation = MakeIAX86Offset(patch->replacement, patch->original);

	// Reset CRT library code to original page protection.
	VirtualProtect(mbiThunk.BaseAddress, mbiThunk.RegionSize, mbiThunk.Protect, &mbiThunk.Protect);
}

#endif

#ifdef LEAK_CHECK

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

std::set<int> s_leaksIndexCache;

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

static LeakCheck s_leakCheck;

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
	LockHelper lock();
	s_leakCheck.endForCurrentThread();
}

// nedalloc hooks

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

void* nedmemalign_leakcheck_win(size_t size, size_t alignment)
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

void* nedrecalloc_leakcheck(void* mem, size_t size)
{
	void* result = nedrecalloc(mem, size);
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

#else

bool isNMLeakCheck()
{
	return false;
}

void getNMLeakCheckStat(NMLeakCheckReport*)
{
}

void enableNMLeakCheckForCurrentThread(NMLeakCheckOptions)
{
}

void disableNMLeakCheckForCurrentThread()
{
}

#define nedmalloc_leakcheck nedmalloc
#define nedcalloc_leakcheck nedcalloc
#define nedmemalign_leakcheck_win nedmemalign_win
#define nedfree_leakcheck nedfree
#define nedrealloc_leakcheck nedrealloc
#define nedrecalloc_leakcheck nedrealloc

#endif

static Patch rlsPatches[] = 
{
	// RELEASE CRT library routines supported by this memory manager.

//	{"_expand",		(FARPROC) winned_expand,	   0},
//	{"_onexit",         (FARPROC) winned_onexit,    0},
//	{"_exit",           (FARPROC) winned_exit,      0},

// operator new, new[], delete, delete[].
#ifdef _WIN64

	{"??2@YAPEAX_K@Z",  (FARPROC) nedmalloc,    0},
	{"??_U@YAPEAX_K@Z", (FARPROC) nedmalloc,    0},
	{"??3@YAXPEAX@Z",   (FARPROC) nedfree,      0},
	{"??_V@YAXPEAX@Z",  (FARPROC) nedfree,      0},
	{"_aligned_malloc",  (FARPROC) nedmemalign_win,      0},
	{"_aligned_free",  (FARPROC) nedfree,      0},

#else

	{"??2@YAPAXI@Z",    (FARPROC) nedmalloc_leakcheck,    0},
	{"??_U@YAPAXI@Z",   (FARPROC) nedmalloc_leakcheck,    0},
	{"??3@YAXPAX@Z",    (FARPROC) nedfree_leakcheck,      0},
	{"??_V@YAXPAX@Z",   (FARPROC) nedfree_leakcheck,      0},

#endif

	// the nothrow variants new, new[].
	{"??2@YAPAXIABUnothrow_t@std@@@Z",  (FARPROC) nedmalloc_leakcheck, 0},
	{"??_U@YAPAXIABUnothrow_t@std@@@Z", (FARPROC) nedmalloc_leakcheck, 0},

	// C allocator functions
	{"_msize",      (FARPROC) nedblksize,  0},
	{"calloc",      (FARPROC) nedcalloc_leakcheck,   0},
	{"_calloc_crt", (FARPROC) nedcalloc_leakcheck,   0},
	{"malloc",      (FARPROC) nedmalloc_leakcheck,   0},
	{"realloc",     (FARPROC) nedrealloc_leakcheck,  0},
	{"free",        (FARPROC) nedfree_leakcheck,     0},
	{"_recalloc",   (FARPROC) nedrecalloc_leakcheck, 0},

	// debug allocators
#ifdef DEBUG
	{"_calloc_dbg",   (FARPROC) nedcalloc_leakcheck,   0},
	{"_malloc_dbg",   (FARPROC) nedmalloc_leakcheck,   0},
	{"_realloc_dbg",  (FARPROC) nedrealloc_leakcheck,  0},
	{"_recalloc_dbg", (FARPROC) nedrecalloc_leakcheck, 0},
	{"_free_dbg",     (FARPROC) nedfree_leakcheck,     0},
	{"_msize_dbg",    (FARPROC) nedblksize,  0},
#endif

	// environment getters
	{"_putenv",    (FARPROC) winned_putenv,    0},
	{"_putenv_s",  (FARPROC) winned_putenv_s,  0},
	{"_wputenv",   (FARPROC) winned_wputenv,   0},
	{"_wputenv_s", (FARPROC) winned_wputenv_s, 0},

	// environment setters
	{"getenv",    (FARPROC) winned_getenv,    0},
	{"getenv_s",  (FARPROC) winned_getenv_s,  0},
	{"wgetenv",   (FARPROC) winned_wgetenv,   0},
	{"wgetenv_s", (FARPROC) winned_wgetenv_s, 0},
};

static bool patchMeIn(void)
{
	static const int rlsCRTLibraryNameLength = _countof(rlsCRTLibraryName);
	static const int rlsPatchesCount = _countof(rlsPatches);
	bool patchedIn = false;

	// acquire the module handles for the CRT libraries (release and debug)
	for (int i = 0; i < rlsCRTLibraryNameLength; i++)
	{
		const HMODULE rlsCRTLibrary = GetModuleHandle(rlsCRTLibraryName[i]);
		const HMODULE defCRTLibrary = rlsCRTLibrary;

		// assign function pointers for required CRT support functions
		if (defCRTLibrary)
		{
			winnedMemcpyPtr = (void(*)(void*, const void*, size_t))GetProcAddress(defCRTLibrary, "memcpy");
			winnedMemsetPtr = (void(*)(void*, int, size_t))GetProcAddress(defCRTLibrary, "memset");
		}

		// patch all relevant Release CRT Library entry points
		if (rlsCRTLibrary)
		{
			for (int i = 0; i < rlsPatchesCount; i++)
			{
				rlsPatches[i].original = GetProcAddress(rlsCRTLibrary, rlsPatches[i].import);
				if (NULL != rlsPatches[i].original)
				{
					patchIt(&rlsPatches[i]);
					patchedIn = true;
				}
			}
		}
	}
	return patchedIn;
}

extern "C"
{
	BOOL WINAPI DllMain(HANDLE hinstDLL, DWORD fdwReason, LPVOID lpreserved)
	{
		hinstDLL;
		lpreserved;

		switch (fdwReason)
		{
			case DLL_PROCESS_ATTACH:
				patchMeIn();
#ifdef LEAK_CHECK
				s_leakCheck.init();
#endif
				break;

		}
		return TRUE;
	}
}
