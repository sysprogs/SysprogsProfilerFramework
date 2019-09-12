#ifdef __cplusplus
extern "C" {
#endif
	
#include <nrf_delay.h>
#include <nrf_timer.h>
#include <nrf_gpio.h>
	
#ifdef __cplusplus
}
#endif

#include "SysprogsProfiler.h"
#include "SysprogsProfilerInterface.h"

#ifndef PROFILER_TIMER_INSTANCE
#define PROFILER_TIMER_INSTANCE 2
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
	int divider = SystemCoreClock / g_SamplingProfilerRate;
	int prescaler = 0;
	while (divider > 65535)
	{
		prescaler++;
		divider /= 2;
	}
	
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->CC[0] = divider;
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->PRESCALER = prescaler;		  
}

extern "C" 
{
	void SysprogsProfiler_DoHandleTimerIRQ(void *PC, void *SP, void *FP, void *LR)
	{
		if ((CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->EVENTS_COMPARE[0] != 0) && ((CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->INTENSET & TIMER_INTENSET_COMPARE0_Msk) != 0))
		{
			CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->EVENTS_COMPARE[0] = 0;	      //Clear compare register 0 event	
			CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_CLEAR = 1;		//Reset the timer
			
			SysprogsProfiler_ProcessSample(PC, SP, FP, LR);
			if (g_SamplingProfilerRate != 0)
			{
				UpdateProfilerTimerFrequency();
				g_SamplingProfilerRate = 0;
			}
		}
	}

	void __attribute__((naked)) CONCAT3(TIMER, PROFILER_TIMER_INSTANCE, _IRQHandler)()
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
	
	UpdateProfilerTimerFrequency();
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->MODE = TIMER_MODE_MODE_Timer;
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_CLEAR = 1;                      
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
		
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->INTENSET = (TIMER_INTENSET_COMPARE0_Enabled << TIMER_INTENSET_COMPARE0_Pos);
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_START = 1;    

	//As of SDK 14.2, the softdevice will fail to initialize if the timer IRQ priority is 1.
	NVIC_SetPriority(CONCAT3(TIMER, PROFILER_TIMER_INSTANCE, _IRQn), 2);
	NVIC_EnableIRQ(CONCAT3(TIMER, PROFILER_TIMER_INSTANCE, _IRQn));
}


unsigned SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter()
{
	static int Initialized = 0;
	
	if (!Initialized)
	{
		CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->CC[0] = 10;
		CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->PRESCALER = 1;		  

		CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->MODE = TIMER_MODE_MODE_Timer;
		CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_CLEAR = 1;                      
		CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
		
		CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_START = 1;    
		Initialized = 1;
	}
	
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_CAPTURE[0] = 1;    
	unsigned value = CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->CC[0];
	CONCAT2(NRF_TIMER, PROFILER_TIMER_INSTANCE)->TASKS_CLEAR = 1;                      
	
	return value;
}