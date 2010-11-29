#include "winnedwrappers.h"

#pragma warning(disable: 4996) // "This function or variable may be unsafe"

static const size_t bufferSize = 32767;

char* winned_getenv(const char* str)
{
	static char buffer[bufferSize];
	*buffer = 0;
	if (0 == GetEnvironmentVariableA(str, buffer, _countof(buffer) - 1))
	{
		return NULL;
	}
	return buffer;
}

WCHAR* winned_wgetenv(const WCHAR* str)
{
	static WCHAR buffer[bufferSize];
	*buffer = 0;
	if (0 == GetEnvironmentVariableW(str, buffer, _countof(buffer) - 1))
	{
		return NULL;
	}
	return buffer;
}

int winned_getenv_s(size_t* pReturnValue, char* buffer, size_t numberOfElements, const char* varname)
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

// see comments for winned_getenv_s()
int winned_wgetenv_s(size_t* pReturnValue, WCHAR* buffer, size_t numberOfElements, const WCHAR* varname)
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

int winned_putenv(const char* str)
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

int winned_wputenv(WCHAR* str)
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

int winned_putenv_s(const char* name, const char* value)
{
	SetEnvironmentVariableA(name, value);
	return 0;
} 

int winned_wputenv_s(WCHAR* name, WCHAR* value)
{
	SetEnvironmentVariableW(name, value);
	return 0;
} 

void* nedmemalign_win(size_t bytes, size_t alignment)
{
	return nedmemalign(alignment, bytes);
}
