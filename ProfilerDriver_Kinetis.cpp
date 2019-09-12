#include <fsl_hwtimer.h>
#include <fsl_hwtimer_systick.h>
#include <fsl_hwtimer_pit.h>
#include <fsl_pit_driver.h>

#include "SysprogsProfiler.h"
#include "SysprogsProfilerInterface.h"

static hwtimer_t hwtimer;

#ifndef SAMPLING_PROFILER_TIMER_INSTANCE
#define SAMPLING_PROFILER_TIMER_INSTANCE 1
#endif

#define CONCAT2_INNER(a,b) a ## b
#define CONCAT3_INNER(a,b,c) a ## b ## c

#define CONCAT2(a,b) CONCAT2_INNER(a,b)
#define CONCAT3(a,b,c) CONCAT3_INNER(a,b,c)

volatile int g_SamplingProfilerRate = 100;	//Samples/sec
volatile int g_EnableSamplingProfiler = 0;	//Will be set via the debugger interface when starting a profiling session

static void UpdateProfilerTimerFrequency()
{
	if (g_SamplingProfilerRate == 0)
		return;
	SystemCoreClockUpdate();
	
	if (kHwtimerSuccess != HWTIMER_SYS_SetPeriod(&hwtimer, 1000000 / g_SamplingProfilerRate))
	{
		asm("bkpt 255");
	}
}

extern "C" 
{
	void SysprogsProfiler_DoHandleTimerIRQ(void *PC, void *SP, void *FP, void *LR)
	{
		PIT_Type * base = g_pitBase[0];
		if (PIT_HAL_IsIntPending(base, SAMPLING_PROFILER_TIMER_INSTANCE))
		{
			PIT_HAL_ClearIntFlag(base, SAMPLING_PROFILER_TIMER_INSTANCE);
			SysprogsProfiler_ProcessSample(PC, SP, FP, LR);
			if (g_SamplingProfilerRate != 0)
			{
				UpdateProfilerTimerFrequency();
				g_SamplingProfilerRate = 0;
			}
		}
	}

#undef PIT
	void __attribute__((naked)) CONCAT3(PIT, SAMPLING_PROFILER_TIMER_INSTANCE, _IRQHandler)()
	{
		asm volatile("ldr r0, [sp, #0x18]");
		asm volatile("add r1, sp, #0x20");
		asm volatile("mov r2, r7");
		asm volatile("ldr r3, [sp, #0x14]");
		asm volatile("push {lr}");
		asm volatile("bl SysprogsProfiler_DoHandleTimerIRQ");
#ifdef __thumb2__
		asm volatile("pop {lr}");
#else
		asm volatile("pop {r2}");
		asm volatile("mov lr, r2");
#endif
		asm volatile("bx lr");
	}
}

void InitializeSamplingProfiler()
{
	if (!g_EnableSamplingProfiler)
		return;
	
	if (kHwtimerSuccess != HWTIMER_SYS_Init(&hwtimer, &kPitDevif, SAMPLING_PROFILER_TIMER_INSTANCE, NULL))
	{
		asm("bkpt 255");
	}
	
	UpdateProfilerTimerFrequency();

	if (kHwtimerSuccess != HWTIMER_SYS_Start(&hwtimer))
	{
		asm("bkpt 255");
	}
}
