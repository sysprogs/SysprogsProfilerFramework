#include "ProfilerCompatibility.h"
#if defined(PROFILER_STM32F0) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32F0_Series)
#include <stm32f0xx_hal.h>
#include <stm32f0xx_hal_tim.h>
#elif defined(PROFILER_STM32F1) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32F1_Series)
#include <stm32f1xx_hal.h>
#include <stm32f1xx_hal_tim.h>
#elif defined(PROFILER_STM32F2) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32F2_Series)
#include <stm32f2xx_hal.h>
#include <stm32f2xx_hal_tim.h>
#elif defined(PROFILER_STM32F3) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32F3_Series)
#include <stm32f3xx_hal.h>
#include <stm32f3xx_hal_tim.h>
#elif defined(PROFILER_STM32F4) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32F4_Series)
#include <stm32f4xx_hal.h>
#include <stm32f4xx_hal_tim.h>
#elif defined(PROFILER_STM32F7) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32F7_Series)
#include <stm32f7xx_hal.h>
#include <stm32f7xx_hal_tim.h>
#elif defined(PROFILER_STM32L0) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32L0_Series)
#include <stm32l0xx_hal.h>
#include <stm32l0xx_hal_tim.h>
#elif defined(PROFILER_STM32L1) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32L1_Series)
#include <stm32l1xx_hal.h>
#include <stm32l1xx_hal_tim.h>
#elif defined(PROFILER_STM32L4) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32L4_Series)
#include <stm32l4xx_hal.h>
#include <stm32l4xx_hal_tim.h>
#elif defined(PROFILER_STM32W1) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32W1_Series)
#include <stm32w1xx_hal.h>
#include <stm32w1xx_hal_tim.h>
#elif defined(PROFILER_STM32H7) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32H7_Series)
#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_tim.h>
#elif defined(PROFILER_STM32H7_M4) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32H7_M4_Series)
#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_tim.h>
#elif defined(PROFILER_STM32G0) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32G0_Series)
#include <stm32g0xx_hal.h>
#include <stm32g0xx_hal_tim.h>
#elif defined(PROFILER_STM32G4) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32G4_Series)
#include <stm32g4xx_hal.h>
#include <stm32g4xx_hal_tim.h>
#elif defined(PROFILER_STM32U5) || defined(PROFILER_com_sysprogs_generated_keil_family_STM32U5_Series)
#include <stm32u5xx_hal.h>
#include <stm32u5xx_hal_tim.h>
#else
#error No STM32 family specified
#endif

#include "SysprogsProfiler.h"
#include "SysprogsProfilerInterface.h"

#ifndef SAMPLING_PROFILER_TIMER_INSTANCE
#define SAMPLING_PROFILER_TIMER_INSTANCE 2
#endif

#define CONCAT2_INNER(a, b) a##b
#define CONCAT3_INNER(a, b, c) a##b##c

#define CONCAT2(a, b) CONCAT2_INNER(a, b)
#define CONCAT3(a, b, c) CONCAT3_INNER(a, b, c)

static TIM_HandleTypeDef s_SamplingProfilerTimer = {
	.Instance = CONCAT2(TIM, SAMPLING_PROFILER_TIMER_INSTANCE),
};

volatile int g_SamplingProfilerRate = 100; //Samples/sec
volatile int g_EnableSamplingProfiler = 0; //Will be set via the debugger interface when starting a profiling session

static void UpdateProfilerTimerFrequency()
{
	if (g_SamplingProfilerRate == 0)
		return;
	SystemCoreClockUpdate();

	unsigned timerClockDividerEncoded;
	//Somewhat dirty trick to determine whether the timer belongs to APB1 or APB2 based on the definition of __TIMxxx_CLK_DISABLE()
#if defined(RCC_CFGR_PPRE1) && defined(RCC_CFGR_PPRE2)
#if defined(PROFILER_STM32L4) || defined(PROFILER_STM32G4)
	if (&CONCAT3(__TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _CLK_DISABLE)() == &RCC->APB1ENR1)
#else
	if (&CONCAT3(__TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _CLK_DISABLE)() == &RCC->APB1ENR)
#endif
		timerClockDividerEncoded = RCC->CFGR & RCC_CFGR_PPRE1;
	else
		timerClockDividerEncoded = (RCC->CFGR & RCC_CFGR_PPRE2) >> 3;
#elif defined(RCC_CFGR_PPRE)
	timerClockDividerEncoded = RCC->CFGR & RCC_CFGR_PPRE;
#elif defined(RCC_D1CFGR_HPRE)
	timerClockDividerEncoded = RCC->D1CFGR & RCC_D1CFGR_HPRE;
#else
#error Unable to determine timer clock divider for this device. Please remove sampling profiler code from the project or contact support for a hotfix.
#endif

	unsigned timerClock = SystemCoreClock;

	//Unless the divider is 1, the timer clock is double the APBx clock.
	if (timerClockDividerEncoded == RCC_HCLK_DIV4)
		timerClock /= 2;
	else if (timerClockDividerEncoded == RCC_HCLK_DIV8)
		timerClock /= 4;
	else if (timerClockDividerEncoded == RCC_HCLK_DIV16)
		timerClock /= 8;

	CONCAT3(__TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _CLK_ENABLE)
	();

	unsigned period = timerClock / g_SamplingProfilerRate;
	unsigned prescaler = 1;
	while (period > 0xFFFF)
	{
		prescaler *= 2;
		period /= 2;
	}

	s_SamplingProfilerTimer.Init.Prescaler = prescaler - 1;
	s_SamplingProfilerTimer.Init.CounterMode = TIM_COUNTERMODE_UP;
	s_SamplingProfilerTimer.Init.Period = period;
	s_SamplingProfilerTimer.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
#ifndef PROFILER_STM32L0
	s_SamplingProfilerTimer.Init.RepetitionCounter = 0;
#endif
	HAL_TIM_Base_Init(&s_SamplingProfilerTimer);
}

extern "C" {
void SysprogsProfiler_DoHandleTimerIRQ(void *PC, void *SP, void *FP, void *LR)
{
	if (__HAL_TIM_GET_FLAG(&s_SamplingProfilerTimer, TIM_FLAG_UPDATE) != RESET)
	{
		if (__HAL_TIM_GET_IT_SOURCE(&s_SamplingProfilerTimer, TIM_IT_UPDATE) != RESET)
		{
			__HAL_TIM_CLEAR_IT(&s_SamplingProfilerTimer, TIM_IT_UPDATE);
			SysprogsProfiler_ProcessSample(PC, SP, FP, LR);
			if (g_SamplingProfilerRate != 0)
			{
				UpdateProfilerTimerFrequency();
				g_SamplingProfilerRate = 0;
			}
		}
	}
}

void __attribute__((naked)) CONCAT3(TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _IRQHandler)()
{
#ifdef USE_RTX
	asm volatile("mrs r3, psp");
#else
	asm volatile("mov r3, sp");
#endif
	asm volatile("ldr r0, [r3, #0x18]");
#ifdef __thumb2__
	asm volatile("add r1, r3, #0x20");
#else
	asm volatile("mov r1, r3");
	asm volatile("add r1, #0x20");
#endif
	asm volatile("mov r2, r7");
	asm volatile("ldr r3, [r3, #0x14]");
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

	HAL_TIM_Base_Start_IT(&s_SamplingProfilerTimer);
	HAL_NVIC_SetPriority(CONCAT3(TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _IRQn), 0, 0);
	HAL_NVIC_EnableIRQ(CONCAT3(TIM, SAMPLING_PROFILER_TIMER_INSTANCE, _IRQn));
}
