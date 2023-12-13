#pragma once
#include <string.h>
#include <assert.h>
#define THREAD_TYPE pthread_t

#include <thread>
#include <sys/prctl.h>
	
inline void SetThreadName(std::thread &thread,const char *name)
{
	pthread_setname_np(thread.native_handle(), name);
}
inline void SetThisThreadName(const char *name)
{
	prctl(PR_SET_NAME, (long)name, 0, 0, 0);
}

inline int strcpy_s(char* dst, size_t size, const char* src)
{
	assert(size >= (strlen(src) + 1));
	strncpy(dst, src, size - 1);
	dst[size - 1] = '\0';
	return errno;
}

inline int fopen_s(FILE** pFile, const char *filename, const char *mode)
{
	*pFile = fopen(filename, mode);
	return errno;
}

inline int fdopen_s(FILE** pFile, int fildes, const char *mode)
{
	*pFile = fdopen(fildes, mode);
	return errno;
}


inline void SetThreadPriority(std::thread &thread,int p)
{
	sched_param sch_params;
	switch(p)
	{
		case -2:
		sch_params.sched_priority = 1;
		break;
		case -1:
		sch_params.sched_priority = 25;
		break;
		case 0:
		sch_params.sched_priority = 50;
		break;
		case 1:
		sch_params.sched_priority = 75;
		break;
		case 2:
		sch_params.sched_priority = 99;
		break;
		default:
		sch_params.sched_priority = 50;
		break;
	}
	pthread_setschedparam(thread.native_handle(), SCHED_RR, &sch_params);
}

inline void SetThisThreadPriority( int p)
{
	sched_param sch_params;
	switch (p)
	{
	case -2:
		sch_params.sched_priority = 1;
		break;
	case -1:
		sch_params.sched_priority = 25;
		break;
	case 0:
		sch_params.sched_priority = 50;
		break;
	case 1:
		sch_params.sched_priority = 75;
		break;
	case 2:
		sch_params.sched_priority = 99;
		break;
	default:
		sch_params.sched_priority = 50;
		break;
	}
	pthread_setschedparam(pthread_self(), SCHED_RR, &sch_params);
}