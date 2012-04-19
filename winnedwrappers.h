#pragma once

#include <stdio.h>
#include <windows.h>

char* winned_getenv(const char* str);
WCHAR* winned_wgetenv(const WCHAR* str);
int winned_getenv_s(size_t* pReturnValue, char* buffer, size_t numberOfElements, const char* varname);
int winned_wgetenv_s(size_t* pReturnValue, WCHAR* buffer, size_t numberOfElements, const WCHAR* varname);
int winned_putenv(const char * str);
int winned_wputenv(WCHAR * str);
int winned_putenv_s(const char* name, const char* value);
int winned_wputenv_s(WCHAR * name, WCHAR * value);

#ifdef BUILD_WINNED_LIB

void* winned_malloc(size_t size);
void winned_free(void* mem);
void* winned_calloc(size_t no, size_t size);
void* winned_realloc(void* mem, size_t size);
void* winned_memalign(size_t bytes, size_t alignment);
void* winned_recalloc(void* mem, size_t num, size_t size);
size_t winned_memsize(void* mem);

#endif