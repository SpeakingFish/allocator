#pragma once

#include "winneddefs.h"

WINNED_EXPORT struct NedMallInfo nedMallInfo(void);

#ifdef NEDMALLOC_USE_STATISTICS
	WINNED_EXPORT struct NedStatistics nedStatistics(void);
#endif

#ifdef NEDMALLOC_USE_CALLBACKS
	WINNED_EXPORT void registerCrashCalback(NedCrashCallback callback);
#endif
