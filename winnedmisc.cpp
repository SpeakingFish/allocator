#include "winnedmisc.h"

#include "nedmalloc.h"

struct NedMallInfo winnedinfo(void)
{
	NedMallInfo result;
	struct nedmallinfo info = nedmallinfo();

	result.arena = info.arena;
	result.ordblks = info.ordblks;
	result.smblks = info.smblks;
	result.hblks = info.hblks;
	result.hblkhd = info.hblkhd;
	result.usmblks = info.usmblks;
	result.fsmblks = info.fsmblks;
	result.uordblks = info.uordblks;
	result.fordblks = info.fordblks;
	result.keepcost = info.keepcost;

	return result;
}
