#include <windows.h>
#include "callwrappers.h"

#pragma warning(disable:4996)

extern "C"
{
	void attachNedmalloc()
	{
		char buffer[16 * 1024];
		char* filename;
		char* allowedFilenames;

		if (!initCallWrappers())
		{
			return;
		}

		// get current process file name
		{
			// we may be use buffer[0] for filename comparison later
			GetModuleFileNameA(NULL, buffer + 1, sizeof(buffer) - 1);
			filename = strrchr(buffer + 1, '\\');
			if (NULL == filename)
			{
				filename = buffer + 1;
			}
			else
			{
				filename = filename + 1;
			}
			filename = _strlwr(filename);
		}

		// get allowed exe filenames
		{
			allowedFilenames = wrapper_getenv("NEDMALLOC_HOOKED_EXE_NAMES");
			if (allowedFilenames == NULL)
			{
				return;
			}
			allowedFilenames = _strlwr(allowedFilenames);
			if (0 == strlen(allowedFilenames))
			{
				return;
			}
		}

		// filter
		{
			if (0 != strncmp(allowedFilenames, filename, min(strlen(allowedFilenames), strlen(filename))))
			{
				// (filename - 1) may be buffer[0]
				filename = filename - 1;
				filename[0] = ';';
				if (NULL == strstr(allowedFilenames, filename))
				{
					return;
				}
			}
		}

		LoadLibraryA("nedmalloc.dll");

		return;
	}

	BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpreserved)
	{
		hinstDLL;
		lpreserved;

		if (DLL_PROCESS_ATTACH == fdwReason)
		{
			attachNedmalloc();
			return FALSE;
		}

		return TRUE;
	}
}
