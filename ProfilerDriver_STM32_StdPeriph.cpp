#include "ProfilerCompatibility.h"

#ifdef PROFILER_STM32F0
#include <stm32f0xx_tim.h>
#include <stm32f0xx_rcc.h>
#elif defined(PROFILER_STM32F1)
#include <stm32f1xx_tim.h>
#include <stm32f1xx_rcc.h>
#elif defined(PROFILER_STM32F2)
#include <stm32f2xx_tim.h>
#include <stm32f2xx_rcc.h>
#elif defined(PROFILER_STM32F3)
#include <stm32f3xx_tim.h>
#include <stm32f3xx_rcc.h>
#elif defined(PROFILER_STM32F4)
#include <stm32f4xx_tim.h>
#include <stm32f4xx_rcc.h>
#elif defined(PROFILER_STM32F7)
#include <stm32f7xx_tim.h>
#include <stm32f7xx_rcc.h>
#elif defined(PROFILER_STM32L0)
#include <stm32l0xx_tim.h>
#include <stm32l0xx_rcc.h>
#elif defined(PROFILER_STM32L1)
#include <stm32l1xx_tim.h>
#include <stm32l1xx_rcc.h>
#elif defined(PROFILER_STM32L4)
#include <stm32l4xx_tim.h>
#include <stm32l4xx_rcc.h>
#elif defined(PROFILER_STM32W1)
#include <stm32w1xx_tim.h>
#include <stm32w1xx_rcc.h>
#else
#error No STM32 family specified
#endif

#include <misc.h>
#include "SysprogsProfiler.h"
#include "SysprogsProfilerInterface.h"

#ifndef SAMPLING_PROFILER_TIMER_INSTANCE
#define SAMPLING_PROFILER_TIMER_INSTANCE 2
#endif

#pragma region Auto-generated timer detection block
#if SAMPLING_PROFILER_TIMER_INSTANCE == 1
#ifdef RCC_APB1Periph_TIM1
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM1, ENABLE)
#elif defined (RCC_APB2Periph_TIM1)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE)
#else
#error Cannot detect the bus for timer 1. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 2
#ifdef RCC_APB1Periph_TIM2
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE)
#elif defined (RCC_APB2Periph_TIM2)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM2, ENABLE)
#else
#error Cannot detect the bus for timer 2. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 3
#ifdef RCC_APB1Periph_TIM3
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE)
#elif defined (RCC_APB2Periph_TIM3)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM3, ENABLE)
#else
#error Cannot detect the bus for timer 3. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 4
#ifdef RCC_APB1Periph_TIM4
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE)
#elif defined (RCC_APB2Periph_TIM4)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM4, ENABLE)
#else
#error Cannot detect the bus for timer 4. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 5
#ifdef RCC_APB1Periph_TIM5
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE)
#elif defined (RCC_APB2Periph_TIM5)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM5, ENABLE)
#else
#error Cannot detect the bus for timer 5. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 6
#ifdef RCC_APB1Periph_TIM6
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE)
#elif defined (RCC_APB2Periph_TIM6)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM6, ENABLE)
#else
#error Cannot detect the bus for timer 6. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 7
#ifdef RCC_APB1Periph_TIM7
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE)
#elif defined (RCC_APB2Periph_TIM7)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM7, ENABLE)
#else
#error Cannot detect the bus for timer 7. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 8
#ifdef RCC_APB1Periph_TIM8
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM8, ENABLE)
#elif defined (RCC_APB2Periph_TIM8)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE)
#else
#error Cannot detect the bus for timer 8. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 9
#ifdef RCC_APB1Periph_TIM9
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM9, ENABLE)
#elif defined (RCC_APB2Periph_TIM9)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE)
#else
#error Cannot detect the bus for timer 9. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 10
#ifdef RCC_APB1Periph_TIM10
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM10, ENABLE)
#elif defined (RCC_APB2Periph_TIM10)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, ENABLE)
#else
#error Cannot detect the bus for timer 10. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 11
#ifdef RCC_APB1Periph_TIM11
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM11, ENABLE)
#elif defined (RCC_APB2Periph_TIM11)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, ENABLE)
#else
#error Cannot detect the bus for timer 11. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 12
#ifdef RCC_APB1Periph_TIM12
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM12, ENABLE)
#elif defined (RCC_APB2Periph_TIM12)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM12, ENABLE)
#else
#error Cannot detect the bus for timer 12. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 13
#ifdef RCC_APB1Periph_TIM13
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM13, ENABLE)
#elif defined (RCC_APB2Periph_TIM13)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM13, ENABLE)
#else
#error Cannot detect the bus for timer 13. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 14
#ifdef RCC_APB1Periph_TIM14
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, ENABLE)
#elif defined (RCC_APB2Periph_TIM14)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM14, ENABLE)
#else
#error Cannot detect the bus for timer 14. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 15
#ifdef RCC_APB1Periph_TIM15
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM15, ENABLE)
#elif defined (RCC_APB2Periph_TIM15)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM15, ENABLE)
#else
#error Cannot detect the bus for timer 15. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 16
#ifdef RCC_APB1Periph_TIM16
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM16, ENABLE)
#elif defined (RCC_APB2Periph_TIM16)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM16, ENABLE)
#else
#error Cannot detect the bus for timer 16. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 17
#ifdef RCC_APB1Periph_TIM17
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM17, ENABLE)
#elif defined (RCC_APB2Periph_TIM17)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM17, ENABLE)
#else
#error Cannot detect the bus for timer 17. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 18
#ifdef RCC_APB1Periph_TIM18
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM18, ENABLE)
#elif defined (RCC_APB2Periph_TIM18)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM18, ENABLE)
#else
#error Cannot detect the bus for timer 18. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 19
#ifdef RCC_APB1Periph_TIM19
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM19, ENABLE)
#elif defined (RCC_APB2Periph_TIM19)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM19, ENABLE)
#else
#error Cannot detect the bus for timer 19. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 20
#ifdef RCC_APB1Periph_TIM20
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM20, ENABLE)
#elif defined (RCC_APB2Periph_TIM20)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM20, ENABLE)
#else
#error Cannot detect the bus for timer 20. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 21
#ifdef RCC_APB1Periph_TIM21
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM21, ENABLE)
#elif defined (RCC_APB2Periph_TIM21)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM21, ENABLE)
#else
#error Cannot detect the bus for timer 21. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif
#elif SAMPLING_PROFILER_TIMER_INSTANCE == 22
#ifdef RCC_APB1Periph_TIM22
#define SAMPLING_PROFILER_TIMER_APB1
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM22, ENABLE)
#elif defined (RCC_APB2Periph_TIM22)
#define SAMPLING_PROFILER_TIMER_APB2
#define ENABLE_SAMPLING_PROFILER_TIMER() RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM22, ENABLE)
#else
#error Cannot detect the bus for timer 22. Please change SAMPLING_PROFILER_TIMER_INSTANCE.
#endif

#endif
#pragma endregion


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
	
	ENABLE_SAMPLING_PROFILER_TIMER();
	
	unsigned timerClockDividerEncoded;
	//Somewhat dirty trick to determine whether the timer belongs to APB1 or APB2 based on the definition of __TIMxxx_CLK_DISABLE()
#ifdef SAMPLING_PROFILER_TIMER_APB1
	timerClockDividerEncoded = RCC->CFGR & RCC_CFGR_PPRE1;
#elif defined (SAMPLING_PROFILER_TIMER_APB2)
	timerClockDividerEncoded = (RCC->CFGR & RCC_CFGR_PPRE2) >> 3;
#else
#error Unknown bus for the selected timer
#endif
	
	unsigned timerClock = SystemCoreClock;
	
	//Unless the divider is 1, the timer clock is double the APBx clock.
	if (timerClockDividerEncoded == RCC_CFGR_PPRE1_DIV4)
		timerClock /= 2;
	else if (timerClockDividerEncoded == RCC_CFGR_PPRE1_DIV8)
		timerClock /= 4;
	else if (timerClockDividerEncoded == RCC_CFGR_PPRE1_DIV16)
		timerClock /= 8;
	
	unsigned period = timerClock / g_SamplingProfilerRate;
	unsigned prescaler = 1;
	while (period > 0xFFFF)
	{
		prescaler *= 2;
		period /= 2;
	}
	
	TIM_TimeBaseInitTypeDef timerInitStructure; 
	timerInitStructure.TIM_Prescaler = prescaler;
	timerInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	timerInitStructure.TIM_Period = period;
	timerInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	timerInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(CONCAT2(TIM, SAMPLING_PROFILER_TIMER_INSTANCE), &timerInitStructure);
	TIM_Cmd(CONCAT2(TIM, SAMPLING_PROFILER_TIMER_INSTANCE), ENABLE);
}

extern "C" 
{
	void SysprogsProfiler_DoHandleTimerIRQ(void *PC, void *SP, void *FP, void *LR)
	{
		if (TIM_GetITStatus(CONCAT2(TIM, SAMPLING_PROFILER_TIMER_INSTANCE), TIM_IT_Update) != RESET)
		{
			TIM_ClearITPendingBit(CONCAT2(TIM, SAMPLING_PROFILER_TIMER_INSTANCE), TIM_IT_Update);
			SysprogsProfiler_ProcessSample(PC, SP, FP, LR);
			if (g_SamplingProfilerRate != 0)
			{
				UpdateProfilerTimerFrequency();
				g_SamplingProfilerRate = 0;
			}
		}
	}

	void __attribute__((naked)) CONCAT3(TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _IRQHandler)()
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

	NVIC_InitTypeDef nvicStructure;
	nvicStructure.NVIC_IRQChannel = CONCAT3(TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _IRQn);
	nvicStructure.NVIC_IRQChannelPreemptionPriority = 0;
	nvicStructure.NVIC_IRQChannelSubPriority = 1;
	nvicStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvicStructure); 
	TIM_ITConfig(CONCAT2(TIM, SAMPLING_PROFILER_TIMER_INSTANCE), TIM_IT_Update, ENABLE);
}
