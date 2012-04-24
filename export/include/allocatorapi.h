#pragma once

#include "allocatordefs.h"

ALLOCATOR_EXPORT AllocatorMallInfo allocatorMallInfo(void);

#ifdef ALLOCATOR_USE_STATISTICS
	ALLOCATOR_EXPORT AllocatorStatistics allocatorStatistics(void);
#endif

#ifdef ALLOCATOR_USE_CALLBACKS
	ALLOCATOR_EXPORT void registerCrashCalback(AllocatorCrashCallback callback);
#endif
