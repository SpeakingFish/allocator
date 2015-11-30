#pragma once

#include <windows.h>

#include "allocatordefs.h"

void wrapper_dllProcessAttach();
void wrapper_dllProcessDetach();
void wrapper_dllThreadAttach();
void wrapper_dllThreadDetach();

char*  wrapper_getenv(const char* str);
WCHAR* wrapper_wgetenv(const WCHAR* str);
int    wrapper_getenv_s(size_t* pReturnValue, char* buffer, size_t numberOfElements, const char* varname);
int    wrapper_wgetenv_s(size_t* pReturnValue, WCHAR* buffer, size_t numberOfElements, const WCHAR* varname);
int    wrapper_putenv(const char * str);
int    wrapper_wputenv(WCHAR * str);
int    wrapper_putenv_s(const char* name, const char* value);
int    wrapper_wputenv_s(WCHAR * name, WCHAR * value);

enum AllocatorMode
{
	AllocatorNotInitialized,
	NoCustomAllocator,
	AllocatorNed,
	AllocatorHoard,
};

AllocatorMode getAllocatorMode();

#ifdef BUILD_ALLOCATOR_LIB

struct AllocatorFunctions
{
	void (*dllProcessAttach)();
	void (*dllProcessDetach)();
	void (*dllThreadAttach)();
	void (*dllThreadDetach)();

	AllocatorMallInfo (*allocatorMallInfo)();
	AllocatorStatistics (*allocatorStatistics)();

	void* (*malloc)(size_t);
	void (*free)(void*);

	void* (*align)(size_t, size_t); // align(alignment, bytes)
	void* (*calloc)(size_t, size_t);
	void* (*realloc)(void*, size_t);

	size_t (*memsize)(void*);
};

void initCallWrappers();

void*  wrapper_malloc(size_t size);
void   wrapper_free(void* mem);
void*  wrapper_calloc(size_t no, size_t size);
void*  wrapper_realloc(void* mem, size_t size);
void*  wrapper_memalign(size_t bytes, size_t alignment);
void*  wrapper_recalloc(void* mem, size_t num, size_t size);
size_t wrapper_memsize(void* mem);

#endif
