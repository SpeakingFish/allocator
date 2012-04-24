#include "callwrappers.h"

#ifdef BUILD_ALLOCATOR_LIB
#include "nedmalloc/nedmalloc.h"
#endif

#include "allocatorapi.h"

enum AllocatorMode
{
	AllocatorNotInitialized,
	NoCustomAllocator,
	AllocatorNed,
	AllocatorHoard,
};

static AllocatorMode s_allocatorMode = AllocatorNotInitialized;
static AllocatorCrashCallback s_crashCallback = NULL;
static void* s_crashBuffer = NULL;
static const size_t s_crashBufferSize = 4 * 1024 * 1024;
static const size_t s_invalidCrashBufferOffset = (size_t)(-1);
static size_t s_crashBufferOffset = s_invalidCrashBufferOffset;
CRITICAL_SECTION s_crashCriticalSection;

#pragma warning(disable: 4996) // "This function or variable may be unsafe"

static const size_t bufferSize = 32767;


bool initCallWrappers()
{
	if (s_allocatorMode == AllocatorNotInitialized)
	{
		s_allocatorMode = AllocatorNed;

		size_t getenvReturnValue;
		const size_t getenvBufferSize = 128;
		char getenvBuffer[getenvBufferSize];
		const char* allocatorModeEnvName = "ALLOCATOR_MODE";
		wrapper_getenv_s(&getenvReturnValue, getenvBuffer, getenvBufferSize - 1, allocatorModeEnvName);

		if (strcmp("system", getenvBuffer))
		{
			s_allocatorMode = NoCustomAllocator;
		}
		else if (strcmp("ned", getenvBuffer))
		{
			s_allocatorMode = AllocatorNed;
		}
		else if (strcmp("hoard", getenvBuffer))
		{
			s_allocatorMode = AllocatorHoard;
		}
	}

#ifdef LEAK_CHECK
	initLeakCheck();
#endif

	return s_allocatorMode == AllocatorNed || s_allocatorMode == AllocatorHoard;
}

char* wrapper_getenv(const char* str)
{
	static char buffer[bufferSize];
	*buffer = 0;
	if (0 == GetEnvironmentVariableA(str, buffer, _countof(buffer) - 1))
	{
		return NULL;
	}
	return buffer;
}

WCHAR* wrapper_wgetenv(const WCHAR* str)
{
	static WCHAR buffer[bufferSize];
	*buffer = 0;
	if (0 == GetEnvironmentVariableW(str, buffer, _countof(buffer) - 1))
	{
		return NULL;
	}
	return buffer;
}

int wrapper_getenv_s(size_t* pReturnValue, char* buffer, size_t numberOfElements, const char* varname)
{
	char buf[bufferSize];

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
	WCHAR buf[bufferSize];
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
		char first[bufferSize];
		char second[bufferSize];
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
		WCHAR first[bufferSize];
		WCHAR second[bufferSize];
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
	InitializeCriticalSection(&s_crashCriticalSection);
	CriticalSectionGuard guard(&s_crashCriticalSection);

	s_crashCallback = callback;
	s_crashBuffer = nedmalloc(s_crashBufferSize);
}
#endif

namespace
{

inline void handleNoMemory(size_t requstedMemorySize)
{
	AllocatorCrashData data;
	if (s_crashCallback != NULL)
	{
		CriticalSectionGuard guard(&s_crashCriticalSection);

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
	CriticalSectionGuard guard(&s_crashCriticalSection);

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

void wrapper_process_detach()
{	
	// Destroy the thread cache for all known pools
	nedpool **pools = nedpoollist();
	if(pools)
	{
		nedpool **pool;
		for(pool = pools; *pool; ++pool)
		{
			neddisablethreadcache(*pool);
		}
		nedfree(pools);
	}
	neddisablethreadcache(0);
}

void* wrapper_malloc(size_t size)
{
	if (isCrash())
	{
		return crashAlloc(size);
	}

	void* result = nedmalloc(size);
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

	nedfree(mem);
}

void* wrapper_calloc(size_t num, size_t size)
{
	if (isCrash())
	{
		void* result = crashAlloc(num * size);
		memset(result, 0, num * size);
		return result;
	}

	void* result = nedcalloc(num, size);
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

	void* result = nedrealloc(mem, size);
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

	// NOTICE: Windows and NedMalloc has different function signature
	void* result = nedmemalign(alignment, bytes);
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
		result = nedrealloc(mem, bytes);
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
	return nedmemsize(mem);
}

#endif
