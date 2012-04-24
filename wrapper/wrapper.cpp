#include <windows.h>
#include <process.h>

#if (_WIN32_WINNT < 0x0500)
#define _WIN32_WINNT 0x0500
#endif

#pragma inline_depth(255)

void (*winnedMemcpyPtr)(void* dest, const void* src, size_t count);
void (*winnedMemsetPtr)(void* dest, int c, size_t count);

#ifdef DEBUG
	LPCWSTR rlsCRTLibraryName[] = {L"MSVCR71D.DLL", L"MSVCR80D.DLL", L"MSVCR90D.DLL", L"MSVCR100D.DLL", L"KERNEL32.DLL"};
#else
	LPCWSTR rlsCRTLibraryName[] = {L"MSVCR71.DLL", L"MSVCR80.DLL", L"MSVCR90.DLL", L"MSVCR100.DLL", L"KERNEL32.DLL"};
#endif

#include "nedmalloc/nedmalloc.h"

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
	(*winnedMemcpyPtr)(patch->codebytes, patch->original, sizeof(patch->codebytes));

	// patch
	unsigned char* patchLocation = (unsigned char*)patch->original;
	CodeBlock* block = (CodeBlock*)arr;
	block->addressLow = (unsigned int)((size_t)patch->replacement & 0xffffffffu);
	block->addressHigh = (unsigned int)(((size_t)patch->replacement & 0xffffffff00000000u) >> 32);
	(*winnedMemcpyPtr)(patchLocation, block, sizeof(CodeBlock));

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

#ifdef ENABLE_USERMODEPAGEALLOCATOR
#define DebugPrint printf
#define USERPAGE_NOCOMMIT                  (M2_CUSTOM_FLAGS_BEGIN<<1)
extern "C"
{
	extern void *userpage_malloc(size_t toallocate, unsigned flags);
	extern int userpage_free(void *mem, size_t size);
	extern void *userpage_realloc(void *mem, size_t oldsize, size_t newsize, int flags, unsigned flags2);
	extern void *userpage_commit(void *mem, size_t size);
	extern int userpage_release(void *mem, size_t size);
}
static LPVOID WINAPI VirtualAlloc_winned(LPVOID lpAddress, SIZE_T dwSize, DWORD flAllocationType, DWORD flProtect)
{
	LPVOID ret=0;
#if defined(_DEBUG)
	DebugPrint("Winpatcher: VirtualAlloc(%p, %u, %x, %x) intercepted\n", lpAddress, dwSize, flAllocationType, flProtect);
#endif
	if(!lpAddress && flAllocationType&MEM_RESERVE)
	{
		ret=userpage_malloc(dwSize, (flAllocationType&MEM_COMMIT) ? 0 : USERPAGE_NOCOMMIT);
#if defined(_DEBUG)
		DebugPrint("Winpatcher: userpage_malloc returns %p\n", ret);
#endif
	}
	else if(lpAddress && (flAllocationType&(MEM_COMMIT|MEM_RESERVE))==(MEM_COMMIT))
	{
		ret=userpage_commit(lpAddress, dwSize);
#if defined(_DEBUG)
		DebugPrint("Winpatcher: userpage_commit returns %p\n", ret);
#endif
	}
	if(!ret || (void *)-1==ret)
	{
		ret=VirtualAlloc(lpAddress, dwSize, flAllocationType, flProtect);
#if defined(_DEBUG)
		DebugPrint("Winpatcher: VirtualAlloc returns %p\n", ret);
#endif
	}
	return (void *)-1==ret ? 0 : ret;
}
static BOOL WINAPI VirtualFree_winned(LPVOID lpAddress, SIZE_T dwSize, DWORD dwFreeType)
{
#if defined(_DEBUG)
	DebugPrint("Winpatcher: VirtualFree(%p, %u, %x) intercepted\n", lpAddress, dwSize, dwFreeType);
#endif
	if(dwFreeType==MEM_DECOMMIT)
	{
		if(-1!=userpage_release(lpAddress, dwSize)) return 1;
	}
	else if(dwFreeType==MEM_RELEASE)
	{
		if(-1!=userpage_free(lpAddress, dwSize)) return 1;
	}
	return VirtualFree(lpAddress, dwSize, dwFreeType);
}
static SIZE_T WINAPI VirtualQuery_winned(LPVOID lpAddress, PMEMORY_BASIC_INFORMATION lpBuffer, SIZE_T dwSize)
{
#if defined(_DEBUG)
	DebugPrint("Winpatcher: VirtualQuery(%p, %p, %u) intercepted\n", lpAddress, lpBuffer, dwSize);
#endif
	return VirtualQuery(lpAddress, lpBuffer, dwSize);
}

#endif

static Patch rlsPatches[] = 
{
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
// 	{"VirtualAlloc_winned", (FARPROC) VirtualAlloc_winned, 0},
// 	{"VirtualFree_winned",  (FARPROC) VirtualFree_winned,  0},
// 	{"VirtualQuery_winned", (FARPROC) VirtualQuery_winned, 0},
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

#ifdef ENABLE_USERMODEPAGEALLOCATOR
		if(DLL_PROCESS_ATTACH == fdwReason)
		{
			__security_init_cookie();	// For /GS compiler option support
		}
#endif

		switch (fdwReason)
		{
			case DLL_PROCESS_ATTACH:
				initCallWrappers();
				patchMeIn();
				break;

			case DLL_THREAD_DETACH:
				wrapper_process_detach();
				break;
		}
		return TRUE;
	}
}
