#include "ProfilerCompatibility.h"

#ifdef USE_RTX
#include "rtx_os.h"
#include "SysprogsProfilerInterface.h"

#ifndef __countof
#define __countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

void thread_switch_helper();
extern int g_SuppressInstrumentingProfiler;

static void SysprogsProfiler_DoReportProfilerThreadSwitch(osRtxThread_t *pThread)
{
	extern void *os_idle_thread_cb;
	const char *pThreadName;
	if (!pThread)
		return;

	if (pThread == osRtxInfo.timer.thread || pThread == osRtxInfo.thread.idle)
	{
		//The idle thread consists of an infinite loop constantly calling multiple small functions.
		//Reporting each of those calls via the profiling mechanism results in impractically large overhead.
		g_SuppressInstrumentingProfiler |= 0x80000000;
	}
	else
		g_SuppressInstrumentingProfiler &= ~0x80000000;

	pThreadName = pThread->name;
	if (!pThreadName)
	{
		if (pThread == osRtxInfo.thread.idle)
			pThreadName = "(idle)";
		else
			pThreadName = "";
	}
	SysprogsProfiler_RTOSThreadSwitched(pThread, pThreadName, pThread->stack_mem);
}

void __attribute__((noinline)) SysprogsRTOSHooks_RTX_thread_switch_helper()
{
	g_SuppressInstrumentingProfiler++;
	SysprogsProfiler_DoReportProfilerThreadSwitch(osRtxInfo.thread.run.next);

	/*
	 *	WARNING: if you are getting an "undefined reference to thread_switch_helper" error, please patch the RTX kernel as shown below:
	 *		1. Locate the irq_XXXX.S assembly file (e.g. irq_cm4f.S)
	 *		2. Locate the "SVC_ContextSwitch" label in it 
	 *		3. Add the following code directly after the "STR R2,[R3]" instruction at the "SVC_ContextSwitch" label:
	 *					PUSH {R2,R3}
	 *					bl thread_switch_helper
	 *					POP {R2,R3}
 	 *		4. Add an empty thread_swtich_helper() implementation in your main source file:
 	 *					extern "C" void thread_switch_helper() {}
 	 *		5. Now the profiler will be able to intercept the thread switch events and record call stacks properly.
	 */
	thread_switch_helper();
	g_SuppressInstrumentingProfiler--;
}

void osRtxTick_Handler();

void __attribute__((noinline)) SysprogsRTOSHooks_osRtxTick_Handler()
{
	g_SuppressInstrumentingProfiler++;
	osRtxTick_Handler();
	g_SuppressInstrumentingProfiler--;
}

void __attribute__((noinline)) SysprogsRTOSHooks_RTX_osRtxThreadFree(osRtxThread_t *thread)
{
	//In order for this to work, the osRtxThreadFree() function must be updated to be non-static, so that it won't be inlined by the compiler.
#if 0
	extern void osRtxThreadFree(os_thread_t * thread);
	SysprogsProfiler_RTOSThreadDeleted(thread);
#endif
}

// -------------------------------------------- Logic for visualizing RTOS primitives in Real-time Watch --------------------------------------------

#ifndef SYSPROGS_PROFILER_MAX_WATCHED_MUTEXES
#define SYSPROGS_PROFILER_MAX_WATCHED_MUTEXES 16
#endif

struct
{
	unsigned Count;
	void *Objects[SYSPROGS_PROFILER_MAX_WATCHED_MUTEXES];
} g_SysprogsProfilerWatchedMutexes, g_SysprogsProfilerWatchedSemaphores;

void EvrRtxMutexAcquired(osMutexId_t mutex_id, uint32_t lock)
{
	unsigned queueCount = g_SysprogsProfilerWatchedMutexes.Count;
	if (queueCount > __countof(g_SysprogsProfilerWatchedMutexes.Objects))
		queueCount = __countof(g_SysprogsProfilerWatchedMutexes.Objects);
	
	for (unsigned i = 0; i < queueCount; i++)
	{
		if (g_SysprogsProfilerWatchedMutexes.Objects[i] == mutex_id)
		{
			SysprogsProfiler_ReportResourceTaken(mutex_id, ((osRtxMutex_t *)mutex_id)->owner_thread, 1);
			return;
		}
	}
}

void EvrRtxMutexReleased(osMutexId_t mutex_id, uint32_t lock)
{
	unsigned queueCount = g_SysprogsProfilerWatchedMutexes.Count;
	if (queueCount > __countof(g_SysprogsProfilerWatchedMutexes.Objects))
		queueCount = __countof(g_SysprogsProfilerWatchedMutexes.Objects);

	for (unsigned i = 0; i < queueCount; i++)
	{
		if (g_SysprogsProfilerWatchedMutexes.Objects[i] == mutex_id)
		{
			SysprogsProfiler_ReportResourceReleased(mutex_id, osThreadGetId(), 0);
			return;
		}
	}
}

void EvrRtxSemaphoreAcquired(osSemaphoreId_t semaphore_id)
{
	unsigned queueCount = g_SysprogsProfilerWatchedSemaphores.Count;
	if (queueCount > __countof(g_SysprogsProfilerWatchedSemaphores.Objects))
		queueCount = __countof(g_SysprogsProfilerWatchedSemaphores.Objects);

	for (unsigned i = 0; i < queueCount; i++)
	{
		if (g_SysprogsProfilerWatchedSemaphores.Objects[i] == semaphore_id)
		{
			SysprogsProfiler_ReportResourceTaken(semaphore_id, osThreadGetId(), ((osRtxSemaphore_t *)semaphore_id)->tokens);
			return;
		}
	}
}

void EvrRtxSemaphoreReleased(osSemaphoreId_t semaphore_id)
{
	unsigned queueCount = g_SysprogsProfilerWatchedSemaphores.Count;
	if (queueCount > __countof(g_SysprogsProfilerWatchedSemaphores.Objects))
		queueCount = __countof(g_SysprogsProfilerWatchedSemaphores.Objects);

	for (unsigned i = 0; i < queueCount; i++)
	{
		if (g_SysprogsProfilerWatchedSemaphores.Objects[i] == semaphore_id)
		{
			SysprogsProfiler_ReportResourceReleased(semaphore_id, osThreadGetId(), ((osRtxSemaphore_t *)semaphore_id)->tokens);
			return;
		}
	}
}

void InitializeProfilerRTOSHooks()
{
	volatile int x = 0;
	SysprogsProfiler_DoReportProfilerThreadSwitch(osThreadGetId());

	if (x)
	{
		SysprogsRTOSHooks_RTX_thread_switch_helper();
		SysprogsRTOSHooks_osRtxTick_Handler();
	}
}

void InitializeProfilerRTOSHooksAfterReportingInitialization()
{
	//If we are using Live Variables, but not regular profiling, g_InstrumentingProfilerRTOSFlags will only be set after calling InstrumentingProfilerInitialized(),
	//so we need to report the main thread again in order for it to get tracked.
	SysprogsProfiler_DoReportProfilerThreadSwitch(osThreadGetId());
}

#else

#endif