#include "callwrappers.h"

#ifdef BUILD_ALLOCATOR_LIB
#include <memory>
#include "callwrappers-hoard.h"
#include "callwrappers-nedmalloc.h"
#endif

#include "allocatorapi.h"

#pragma warning(disable: 4996) // "This function or variable may be unsafe"

static const size_t s_bufferSize = 32767;

AllocatorMode getAllocatorMode()
{
	static AllocatorMode s_allocatorMode = AllocatorNotInitialized;
	if (s_allocatorMode == AllocatorNotInitialized)
	{
		s_allocatorMode = AllocatorNed;

		const char* allocatorModeEnvName = "ALLOCATOR_MODE";
		char* getenvBuffer = wrapper_getenv(allocatorModeEnvName);

		if (getenvBuffer == NULL || strcmp("ned", getenvBuffer) == 0)
		{
			s_allocatorMode = AllocatorNed;
		}
		else if (strcmp("system", getenvBuffer) == 0)
		{
			s_allocatorMode = NoCustomAllocator;
		}
		else if (strcmp("hoard", getenvBuffer) == 0)
		{
			s_allocatorMode = AllocatorHoard;
		}
	}

	return s_allocatorMode;
}

#ifdef BUILD_ALLOCATOR_LIB

static AllocatorFunctions s_allocatorFunctions = {NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
static AllocatorCrashCallback s_crashCallback = NULL;
static void* s_crashBuffer = NULL;
static const size_t s_crashBufferSize = 4 * 1024 * 1024;
static const size_t s_invalidCrashBufferOffset = (size_t)(-1);
static size_t s_crashBufferOffset = s_invalidCrashBufferOffset;
std::auto_ptr<CRITICAL_SECTION> s_crashCriticalSection;

void initCallWrappers()
{
	if (getAllocatorMode() == AllocatorNed)
	{
		s_allocatorFunctions = nedmallocAllocatorFunctions();
	}
	else if (getAllocatorMode() == AllocatorHoard)
	{
		s_allocatorFunctions = hoardAllocatorFunctions();
	}

#ifdef LEAK_CHECK
	initLeakCheck();
#endif
}

#endif

char* wrapper_getenv(const char* str)
{
	static char buffer[s_bufferSize];
	*buffer = 0;
	if (0 == GetEnvironmentVariableA(str, buffer, _countof(buffer) - 1))
	{
		return NULL;
	}
	return buffer;
}

WCHAR* wrapper_wgetenv(const WCHAR* str)
{
	static WCHAR buffer[s_bufferSize];
	*buffer = 0;
	if (0 == GetEnvironmentVariableW(str, buffer, _countof(buffer) - 1))
	{
		return NULL;
	}
	return buffer;
}

int wrapper_getenv_s(size_t* pReturnValue, char* buffer, size_t numberOfElements, const char* varname)
{
	char buf[s_bufferSize];

	// obtain the required size not including trailing '\0'
	*pReturnValue = GetEnvironmentVariableA(varname, buf, _countof(buf) - 1);

	// return 0 if there is nothing to store
	if (0 == *pReturnValue)
	{
		return 0;
	}

	// now include trailing '\0'
	++(*pReturnValue);

	// ...so in case of size querying the function returns the required buffer size including trailing '\0'
	if (0 == numberOfElements)
	{
		return 0;
	}

	// copy all but not the trailing '\0'
	const size_t count = (*pReturnValue > numberOfElements ? numberOfElements : *pReturnValue) - 1;
	strncpy(buffer, buf, count);

	// set the end of buffer
	buffer[count] = '\0';
	return 0;
}

// see comments for wrapper_getenv_s()
int wrapper_wgetenv_s(size_t* pReturnValue, WCHAR* buffer, size_t numberOfElements, const WCHAR* varname)
{
	WCHAR buf[s_bufferSize];
	*pReturnValue = GetEnvironmentVariableW(varname, buf, _countof(buf) - 1);

	if (0 == *pReturnValue)
	{
		return 0;
	}

	++(*pReturnValue);

	if (0 == numberOfElements)
	{
		return 0;
	}

	const size_t count = (*pReturnValue > numberOfElements ? numberOfElements : *pReturnValue) - 1;
	wcsncpy(buffer, buf, count);
	buffer[count] = '\0';
	return 0;
}

int wrapper_putenv(const char* str)
{
	const char* eqpos = strchr(str, '=');
	if (eqpos != NULL)
	{
		char first[s_bufferSize];
		char second[s_bufferSize];
		const ptrdiff_t namelen = (size_t)eqpos - (size_t)str;
		strncpy(first, str, namelen);
		first[namelen] = '\0';
		const ptrdiff_t valuelen = strlen(eqpos + 1);
		strncpy(second, eqpos + 1, valuelen);
		second[valuelen] = '\0';
		SetEnvironmentVariableA(first, second);
		return 0;
	}
	return -1;
} 

int wrapper_wputenv(WCHAR* str)
{
	WCHAR* eqpos = wcschr(str, '=');
	if (eqpos != NULL)
	{
		WCHAR first[s_bufferSize];
		WCHAR second[s_bufferSize];
		const ptrdiff_t namelen = (size_t)eqpos - (size_t)str;
		wcsncpy(first, str, namelen);
		first[namelen] = '\0';
		const ptrdiff_t valuelen = wcslen(eqpos + 1);
		wcsncpy(second, eqpos + 1, valuelen);
		second[valuelen] = '\0';
		SetEnvironmentVariableW(first, second);
		return 0;
	}
	return -1;
} 

int wrapper_putenv_s(const char* name, const char* value)
{
	SetEnvironmentVariableA(name, value);
	return 0;
} 

int wrapper_wputenv_s(WCHAR* name, WCHAR* value)
{
	SetEnvironmentVariableW(name, value);
	return 0;
} 

#ifdef BUILD_ALLOCATOR_LIB

class CriticalSectionGuard
{
public:
	CriticalSectionGuard(CRITICAL_SECTION* criticalSection)
		: m_criticalSection(criticalSection)
	{
		EnterCriticalSection(m_criticalSection);
	}
	~CriticalSectionGuard()
	{
		LeaveCriticalSection(m_criticalSection);
	}

private:
	CRITICAL_SECTION* m_criticalSection;
};

#ifdef ALLOCATOR_USE_CALLBACKS
void registerCrashCalback(AllocatorCrashCallback callback)
{
	if (getAllocatorMode() != NoCustomAllocator)
	{
		s_crashCriticalSection.reset(new CRITICAL_SECTION);
		InitializeCriticalSection(s_crashCriticalSection.get());
		CriticalSectionGuard guard(s_crashCriticalSection.get());

		s_crashCallback = callback;
		s_crashBuffer = s_allocatorFunctions.malloc(s_crashBufferSize);
	}
}
#endif

namespace
{

inline void handleNoMemory(size_t requstedMemorySize)
{
	AllocatorCrashData data;
	if (s_crashCallback != NULL)
	{
		CriticalSectionGuard guard(s_crashCriticalSection.get());

		data.mallinfo = allocatorMallInfo();
		data.requstedMemorySize = requstedMemorySize;
		s_crashBufferOffset = 0;

		(*s_crashCallback)(data);
	}
}

inline bool isCrash()
{
	return s_crashBufferOffset != s_invalidCrashBufferOffset;
}

inline void* crashAlloc(size_t bytes, size_t align = MALLOC_ALIGNMENT)
{
	CriticalSectionGuard guard(s_crashCriticalSection.get());

	const size_t rest = (s_crashBufferOffset + (size_t)s_crashBuffer) % align;
	size_t offset = (rest == 0) ? 0 : (align - rest);
	if (offset < sizeof(size_t))
	{
		offset += align;
	}
	void* result = (void*)(s_crashBufferOffset + (size_t)s_crashBuffer + offset);
	s_crashBufferOffset += offset + bytes;
	*((size_t*)((size_t)result - sizeof(size_t))) = bytes;
	return result;
}

}

void wrapper_dllProcessAttach() 
{
	if (s_allocatorFunctions.dllProcessAttach != NULL)
	{
		(*s_allocatorFunctions.dllProcessAttach)();
	}
}

void wrapper_dllProcessDetach()
{
	if (s_allocatorFunctions.dllProcessDetach != NULL)
	{
		(*s_allocatorFunctions.dllProcessDetach)();
	}
}

void wrapper_dllThreadAttach()
{
	if (s_allocatorFunctions.dllThreadAttach != NULL)
	{
		(*s_allocatorFunctions.dllThreadAttach)();
	}
}

void wrapper_dllThreadDetach()
{	
	if (s_allocatorFunctions.dllThreadDetach != NULL)
	{
		(*s_allocatorFunctions.dllThreadDetach)();
	}
}

ALLOCATOR_EXPORT AllocatorMallInfo allocatorMallInfo(void)
{
	if (s_allocatorFunctions.allocatorMallInfo != NULL)
	{
		return (*s_allocatorFunctions.allocatorMallInfo)();
	}

	AllocatorMallInfo result;
	memset(&result, 0, sizeof(AllocatorMallInfo));
	return result;
}

#ifdef ALLOCATOR_USE_STATISTICS
ALLOCATOR_EXPORT AllocatorStatistics allocatorStatistics(void)
{
	if (s_allocatorFunctions.allocatorStatistics != NULL)
	{
		return (*s_allocatorFunctions.allocatorStatistics)();
	}

	AllocatorStatistics result;
	memset(&result, 0, sizeof(AllocatorStatistics));
	return result;
}
#endif

void* wrapper_malloc(size_t size)
{
	if (isCrash())
	{
		return crashAlloc(size);
	}

	void* result = s_allocatorFunctions.malloc(size);
	if (size > 0 && NULL == result)
	{
		handleNoMemory(size);
	}
	return result;
}

void wrapper_free(void* mem)
{
	if (isCrash())
	{
		return;
	}

	s_allocatorFunctions.free(mem);
}

void* wrapper_calloc(size_t num, size_t size)
{
	if (isCrash())
	{
		void* result = crashAlloc(num * size);
		memset(result, 0, num * size);
		return result;
	}

	void* result = s_allocatorFunctions.calloc(num, size);
	if (num > 0 && size > 0 && NULL == result)
	{
		handleNoMemory(num * size);
	}
	return result;
}

void* wrapper_realloc(void* mem, size_t size)
{
	if (isCrash())
	{
		const size_t oldMemSize = wrapper_memsize(mem);
		if (oldMemSize < size)
		{
			void* result = crashAlloc(size);
			if (oldMemSize > 0)
			{
				memcpy(result, mem, oldMemSize);
			}
			return result;
		}
		return mem;
	}

	void* result = s_allocatorFunctions.realloc(mem, size);
	if (size > 0 && NULL == result)
	{
		handleNoMemory(size);
	}
	return result;
}

void* wrapper_memalign(size_t bytes, size_t alignment)
{
	if (isCrash())
	{
		return crashAlloc(bytes, alignment);
	}

	// NOTICE: Windows _aligned_malloc and NedMalloc nedmemalign has different function signature
	void* result = s_allocatorFunctions.align(alignment, bytes);
	if (bytes > 0 && NULL == result)
	{
		handleNoMemory(bytes);
	}
	return result;
}

void* wrapper_recalloc(void* mem, size_t num, size_t size)
{
	void* result = mem;
	const size_t bytes = num * size;

	if (isCrash())
	{
		if (wrapper_memsize(mem) < num * size)
		{
			result = crashAlloc(num * size);
		}
	}
	else
	{
		result = s_allocatorFunctions.realloc(mem, bytes);
		if (bytes > 0 && NULL == result)
		{
			handleNoMemory(bytes);
		}
	}
	
	if (NULL != result)
	{
		memset(result, 0, num * size);
	}
	return result;
}

size_t wrapper_memsize(void* mem)
{
	if (isCrash() && s_crashBuffer != NULL && mem >= s_crashBuffer && mem <= (void*)((size_t)s_crashBuffer + s_crashBufferSize))
	{
		return *((size_t*)((size_t)mem - sizeof(size_t)));
	}
	return s_allocatorFunctions.memsize(mem);
}

#endif
