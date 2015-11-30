#include <windows.h>
#include <process.h>

#if (_WIN32_WINNT < 0x0500)
#define _WIN32_WINNT 0x0500
#endif

#pragma inline_depth(255)

void (*memcpyPtr)(void* dest, const void* src, size_t count);
void (*memsetPtr)(void* dest, int c, size_t count);

#ifdef DEBUG
	LPCWSTR rlsCRTLibraryName[] = {L"MSVCR71D.DLL", L"MSVCR80D.DLL", L"MSVCR90D.DLL", L"MSVCR100D.DLL", L"KERNEL32.DLL"};
#else
	LPCWSTR rlsCRTLibraryName[] = {L"MSVCR71.DLL", L"MSVCR80.DLL", L"MSVCR90.DLL", L"MSVCR100.DLL", L"KERNEL32.DLL"};
#endif

#include "callwrappers.h"

struct Patch;
static void patchIt(Patch* patch);

extern "C"
{
	__declspec(dllexport) int ReferenceNMStub;
}

#ifdef x64

#pragma warning(disable:4510) // 'CodeBlock' : default constructor could not be generated
#pragma warning(disable:4512) // 'CodeBlock' : assignment operator could not be generated
#pragma warning(disable:4610) // struct 'CodeBlock' can never be instantiated - user defined constructor required
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
	(*memcpyPtr)(patch->codebytes, patch->original, sizeof(patch->codebytes));

	// patch
	unsigned char* patchLocation = (unsigned char*)patch->original;
	CodeBlock* block = (CodeBlock*)arr;
	block->addressLow = (unsigned int)((size_t)patch->replacement & 0xffffffffu);
	block->addressHigh = (unsigned int)(((size_t)patch->replacement & 0xffffffff00000000u) >> 32);
	(*memcpyPtr)(patchLocation, block, sizeof(CodeBlock));

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
	(*memcpyPtr)(patch->codebytes, patch->original, sizeof(patch->codebytes));

	// patch
	unsigned char *patchLocation = (unsigned char*)patch->original;
	*patchLocation++ = IAX86_NEARJMP_OPCODE;
	*(unsigned*)patchLocation = MakeIAX86Offset(patch->replacement, patch->original);

	// Reset CRT library code to original page protection.
	VirtualProtect(mbiThunk.BaseAddress, mbiThunk.RegionSize, mbiThunk.Protect, &mbiThunk.Protect);
}

#endif

extern "C"
{
	static const int MAX_EXIT_FUNCTIONS = 255;
	static int exitCount = 0;
	typedef void (*exitFunctionType) (void);
	exitFunctionType exitFunctionBuffer[MAX_EXIT_FUNCTIONS];

	void allocator_onexit(void (*function)(void))
	{
		if (exitCount < MAX_EXIT_FUNCTIONS)
		{
			exitFunctionBuffer[exitCount] = function;
			exitCount++;
		}
	}

	void allocator_exit(int code)
	{
		code;
		while (exitCount > 0)
		{
			exitCount--;
			(exitFunctionBuffer[exitCount])();
		}
	}

	void* allocator_expand(void * ptr)
	{
		ptr;
		return NULL;
	}
}

static Patch rlsPatches[] = 
{
	// RELEASE CRT library routines supported by this memory manager.

	{"_expand",		(FARPROC) allocator_expand,	   0},
	{"_onexit",     (FARPROC) allocator_onexit,    0},
	{"_exit",       (FARPROC) allocator_exit,      0},

#ifdef DEBUG
	{"_expand_dbg", (FARPROC) allocator_expand,	   0},
#endif

#ifdef DEBUG
	// operator new, delete - in Debug msvcr does not use malloc/free functions
	// for allocations/deallocations, so we have to patch them.
	#ifdef x64
		{"??2@YAPEAX_K@Z",  (FARPROC) wrapper_malloc, 0},
		{"??3@YAXPEAX@Z",   (FARPROC) wrapper_free,   0},
	#else
		{"??2@YAPAXI@Z",    (FARPROC) wrapper_malloc, 0},
		{"??3@YAXPAX@Z",    (FARPROC) wrapper_free,   0},
	#endif
#endif

	// C allocator functions
	{"_msize",      (FARPROC) wrapper_memsize,  0},
	{"calloc",      (FARPROC) wrapper_calloc,   0},
	{"_calloc_crt", (FARPROC) wrapper_calloc,   0},
	{"malloc",      (FARPROC) wrapper_malloc,   0},
	{"realloc",     (FARPROC) wrapper_realloc,  0},
	{"free",        (FARPROC) wrapper_free,     0},
	{"_recalloc",   (FARPROC) wrapper_recalloc, 0},

	// debug allocators
#ifdef DEBUG
	{"_calloc_dbg",   (FARPROC) wrapper_calloc,   0},
	{"_malloc_dbg",   (FARPROC) wrapper_malloc,   0},
	{"_realloc_dbg",  (FARPROC) wrapper_realloc,  0},
	{"_recalloc_dbg", (FARPROC) wrapper_recalloc, 0},
	{"_free_dbg",     (FARPROC) wrapper_free,     0},
	{"_msize_dbg",    (FARPROC) wrapper_memsize,  0},
#endif

	// environment getters
	{"_putenv",    (FARPROC) wrapper_putenv,    0},
	{"_putenv_s",  (FARPROC) wrapper_putenv_s,  0},
	{"_wputenv",   (FARPROC) wrapper_wputenv,   0},
	{"_wputenv_s", (FARPROC) wrapper_wputenv_s, 0},

	// environment setters
	{"getenv",    (FARPROC) wrapper_getenv,    0},
	{"getenv_s",  (FARPROC) wrapper_getenv_s,  0},
	{"wgetenv",   (FARPROC) wrapper_wgetenv,   0},
	{"wgetenv_s", (FARPROC) wrapper_wgetenv_s, 0},

// #ifdef ENABLE_USERMODEPAGEALLOCATOR
// 	{"VirtualAlloc", (FARPROC) VirtualAlloc_winned, 0},
// 	{"VirtualFree",  (FARPROC) VirtualFree_winned,  0},
// 	{"VirtualQuery", (FARPROC) VirtualQuery_winned, 0},
// #endif
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
			memcpyPtr = (void(*)(void*, const void*, size_t))GetProcAddress(defCRTLibrary, "memcpy");
			memsetPtr = (void(*)(void*, int, size_t))GetProcAddress(defCRTLibrary, "memset");
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

		if (getAllocatorMode() == NoCustomAllocator)
		{
			return TRUE;
		}

		if (getAllocatorMode() == AllocatorNotInitialized)
		{
			exit(1);
		}

		switch (fdwReason)
		{
			case DLL_PROCESS_ATTACH:
				initCallWrappers();
				patchMeIn();
				wrapper_dllProcessAttach();
				break;

			case DLL_PROCESS_DETACH:
				wrapper_dllProcessDetach();
				break;

			case DLL_THREAD_ATTACH:
				wrapper_dllThreadAttach();
				break;

			case DLL_THREAD_DETACH:
				wrapper_dllThreadDetach();
				break;
		}
		return TRUE;
	}
}
