#pragma once

#include <windows.h>

bool initCallWrappers();

void wrapper_process_attach_before_patch();
void wrapper_process_attach_after_patch();
void wrapper_process_detach();
void wrapper_thread_attach();
void wrapper_thread_detach();

char*  wrapper_getenv(const char* str);
WCHAR* wrapper_wgetenv(const WCHAR* str);
int    wrapper_getenv_s(size_t* pReturnValue, char* buffer, size_t numberOfElements, const char* varname);
int    wrapper_wgetenv_s(size_t* pReturnValue, WCHAR* buffer, size_t numberOfElements, const WCHAR* varname);
int    wrapper_putenv(const char * str);
int    wrapper_wputenv(WCHAR * str);
int    wrapper_putenv_s(const char* name, const char* value);
int    wrapper_wputenv_s(WCHAR * name, WCHAR * value);

#ifdef BUILD_ALLOCATOR_LIB

void*  wrapper_malloc(size_t size);
void   wrapper_free(void* mem);
void*  wrapper_calloc(size_t no, size_t size);
void*  wrapper_realloc(void* mem, size_t size);
void*  wrapper_memalign(size_t bytes, size_t alignment);
void*  wrapper_recalloc(void* mem, size_t num, size_t size);
size_t wrapper_memsize(void* mem);

#endif
