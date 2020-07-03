#pragma once

/*! \mainpage Fast Semihosting and Profiler Framework
	This framework is used in VisualGDB projects and provides the following functionality:
		- Fast semihosting (printf()-like text output to Visual Studio), much faster than the regular semihosting API. See \ref FastSemihosting.h
		- Support for sampling and instrumenting profiler see \ref SysprogsProfiler.h.
		- Support for real-time watch and visualization of RTOS threads and queues.
		- Test Resource Management. See \ref TestResourceManager.h
		
	\file FastSemihosting.h Provides API used for fast semihosting
		
		Normally, you do not need to include this file, or call any functions from it. 
		Simply enable the <b>VisualGDB Project Properties -> Embedded Frameworks -> Semihosting -> Redirect printf() to fast semihosting</b> option and use the 
		regular <b>printf()</b> function to output data to VisualGDB.
*/

#ifdef __cplusplus
extern "C"
{
#endif
	
//! Temporarily stops VisualGDB from checking whether new semihosting data is available.
/*! This function can be used to stop VisualGDB from reading the target memory in anticipation of
	new semihosting output. Use it to allocate the entire memory reading bandwidth to other tasks (e.g. Live Variables).
*/
void SuspendFastSemihostingPolling();

//! Resumes the semihosting pooling previously suspended by \ref SuspendFastSemihostingPolling.
void ResumeFastSemihostingPolling();

//! Do not call directly. Enable the <b>Redirect printf() to fast semihosting</b> option instead and use regular printf().
int WriteToFastSemihostingChannel(unsigned char channel, const void *pBuffer, int size, int writeAll);

//! Do not call directly.
/*! Do not call this explicitly, unless you want VisualGDB to set global variables like <b>s_FastSemihostingInitialized</b>.
*/
void InitializeFastSemihosting();

#ifdef __cplusplus
}
#endif
