#pragma once

// *** Include this file into your main.cpp, link allocator.lib and enjoy!

#include <windows.h>

#if defined(_WIN64)
#pragma comment(linker, "/include:ReferenceNM")
#else
#pragma comment(linker, "/include:_ReferenceNM")
#endif

extern "C" {

	__declspec(dllimport) int ReferenceNMStub;

	void ReferenceNM (void)
	{
		LoadLibraryA("allocator.dll");
		ReferenceNMStub = 1;
	}

} 
