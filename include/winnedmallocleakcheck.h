#pragma once

#if defined(BUILD_WINNED_LIB)
	#define WINNED_EXPORT __declspec(dllexport)
#else
	#define WINNED_EXPORT __declspec(dllimport)
#endif


struct NMLeakCheckOptions
{
	NMLeakCheckOptions(
		bool collectCallStack = false,
		bool indexBasedCallstack = false, 
		bool useIndex = false,
		size_t sizeGreateThan = 0)
		: collectCallStack(collectCallStack)
		, collectIndexBasedCallstackLeaks(indexBasedCallstack)
		, useCollectedCallstackIndexes(useIndex)
		, sizeGreateThan(sizeGreateThan)
	{
	}

	bool collectCallStack;
	// use global cached leak index info
	bool collectIndexBasedCallstackLeaks;
	bool useCollectedCallstackIndexes;
	size_t sizeGreateThan;
};

struct NMLeakCheckReport
{
	virtual void reportCount(size_t leakCount, size_t maxCount, size_t total){leakCount; maxCount; total;}
	virtual void beginLeakReport(){}
	virtual void reportstackline(char* line){line;}
	virtual void endLeakReport(){}
};

WINNED_EXPORT bool isNMLeakCheck();
WINNED_EXPORT void getNMLeakCheckStat(NMLeakCheckReport* report);
WINNED_EXPORT void enableNMLeakCheckForCurrentThread(NMLeakCheckOptions options);
WINNED_EXPORT void disableNMLeakCheckForCurrentThread();


