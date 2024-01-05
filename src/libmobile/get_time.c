#include <time.h>

#include "get_time.h"

double get_time(void)
{
	static struct timespec tv;

	clock_gettime(CLOCK_REALTIME, &tv);

	return (double)tv.tv_sec + (double)tv.tv_nsec / 1000000000.0;
}

