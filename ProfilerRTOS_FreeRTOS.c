#ifdef USE_FREERTOS
#include <FreeRTOS.h>
#include <task.h>
#include "SysprogsProfilerInterface.h"

//Using those macros in conjunction with the <DebugInfoBinding> tag allows accessing fields of private structures not exposed via FreeRTOS headers.
#define DELAYED_STRUCT_MEMBER_OFFSET(struct, member) const volatile int __attribute__((section(".text." #struct "_" #member "_Offset"))) struct##_##member##_Offset
#define STRUCT_MEMBER(ptr, result_type, structType, member) (*((result_type *)((char *)ptr + structType##_##member##_Offset)))

DELAYED_STRUCT_MEMBER_OFFSET(tskTaskControlBlock, pcTaskName);
DELAYED_STRUCT_MEMBER_OFFSET(tskTaskControlBlock, pxStack);
DELAYED_STRUCT_MEMBER_OFFSET(QueueDefinition, uxMessagesWaiting);

extern void *pxCurrentTCB;

#ifndef __countof
#define __countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

static __attribute__((noinline)) void SysprogsRTOSHooks_ReportThreadSwitch()
{
	extern int g_SuppressInstrumentingProfiler;
	g_SuppressInstrumentingProfiler++;
	SysprogsProfiler_RTOSThreadSwitched(pxCurrentTCB,
										&STRUCT_MEMBER(pxCurrentTCB, char, tskTaskControlBlock, pcTaskName),
										STRUCT_MEMBER(pxCurrentTCB, void *, tskTaskControlBlock, pxStack));
	g_SuppressInstrumentingProfiler--;
}

void __attribute__((noinline)) SysprogsRTOSHooks_FreeRTOS_vTaskSwitchContext()
{
	extern int g_SuppressInstrumentingProfiler;
	g_SuppressInstrumentingProfiler++;
	vTaskSwitchContext();
	SysprogsRTOSHooks_ReportThreadSwitch();
	g_SuppressInstrumentingProfiler--;
}

void SVC_Handler();

void __attribute__((noinline, naked)) SysprogsRTOSHooks_FreeRTOS_SVC_Handler()
{
	__asm volatile(".align 4");
	__asm volatile("push {lr}");
	__asm volatile("bl SysprogsRTOSHooks_ReportThreadSwitch");
	__asm volatile("pop {r0}");
	__asm volatile("mov lr, r0");
	__asm volatile("ldr r0, [pc, #4]");
	__asm volatile("mov pc, r0");
	__asm volatile("nop");
	__asm volatile(".word SVC_Handler");
}

void __attribute__((noinline)) SysprogsRTOSHooks_FreeRTOS_vTaskDelete(TaskHandle_t task)
{
	SysprogsProfiler_RTOSThreadDeleted((void *)task);
	vTaskDelete(task);
}

void SysTick_Handler();

void __attribute__((noinline)) SysprogsRTOSHooks_FreeRTOS_SysTick_Handler(void)
{
	extern int g_SuppressInstrumentingProfiler;
	g_SuppressInstrumentingProfiler++;
	SysTick_Handler();
	g_SuppressInstrumentingProfiler--;
}

#ifdef PROFILER_nRF5Xxxxx

void RTC1_IRQHandler();

void __attribute__((noinline)) SysprogsRTOSHooks_FreeRTOS_RTC1_IRQHandler(void)
{
	extern int g_SuppressInstrumentingProfiler;
	g_SuppressInstrumentingProfiler++;
	RTC1_IRQHandler();
	g_SuppressInstrumentingProfiler--;
}

#endif

#ifndef SYSPROGS_PROFILER_MAX_WATCHED_QUEUES
#define SYSPROGS_PROFILER_MAX_WATCHED_QUEUES 16
#endif

struct
{
	unsigned QueueCount;
	void *Queues[SYSPROGS_PROFILER_MAX_WATCHED_QUEUES];
} g_SysprogsProfilerWatchedQueues;

void __attribute__((noinline, optimize("-O0"))) SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND(void *pQueue)
{
	(void)pQueue;
	__asm("nop");
}

void __attribute__((noinline, optimize("-O0"))) SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE(void *pQueue)
{
	(void)pQueue;
	__asm("nop");
}

void __attribute__((noinline, optimize("-O0"))) SysprogsRTOSHooks_FreeRTOS_SchedulerStarting(void)
{
	__asm("bkpt 255"); //When this breakpoint triggers, VisualGDB will automatically reparse real-time watch expressions that could not be parsed before
	vTaskStartScheduler();
}

void __attribute__((noinline)) SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND_Actual(void *pQueue)
{
	unsigned queueCount = g_SysprogsProfilerWatchedQueues.QueueCount;
	if (queueCount > __countof(g_SysprogsProfilerWatchedQueues.Queues))
		queueCount = __countof(g_SysprogsProfilerWatchedQueues.Queues);
	for (unsigned i = 0; i < queueCount; i++)
	{
		if (g_SysprogsProfilerWatchedQueues.Queues[i] == pQueue)
		{
			SysprogsProfiler_ReportResourceTaken(pQueue, pxCurrentTCB, STRUCT_MEMBER(pQueue, unsigned, QueueDefinition, uxMessagesWaiting) + 1);
			return;
		}
	}
}

void __attribute__((noinline)) SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE_Actual(void *pQueue)
{
	unsigned queueCount = g_SysprogsProfilerWatchedQueues.QueueCount;
	if (queueCount > __countof(g_SysprogsProfilerWatchedQueues.Queues))
		queueCount = __countof(g_SysprogsProfilerWatchedQueues.Queues);
	for (unsigned i = 0; i < queueCount; i++)
	{
		if (g_SysprogsProfilerWatchedQueues.Queues[i] == pQueue)
		{
			SysprogsProfiler_ReportResourceReleased(pQueue, pxCurrentTCB, STRUCT_MEMBER(pQueue, unsigned, QueueDefinition, uxMessagesWaiting) - 1);
			return;
		}
	}
}

static void __attribute__((noinline, naked)) ReferenceFreeRTOSSymbols()
{
	//This function should never be called and is only needed to make sure the needed FreeRTOS symbols get included in the final ELF file.
	__asm volatile("bx lr");
	__asm volatile("b xQueueGenericSendFromISR");
	__asm volatile("b xQueueReceiveFromISR");
}

void InitializeProfilerRTOSHooks()
{
	volatile int x = 0;
	if (x)
	{
		SysprogsRTOSHooks_FreeRTOS_vTaskSwitchContext();
		SysprogsRTOSHooks_FreeRTOS_vTaskDelete(0);
		SysprogsRTOSHooks_FreeRTOS_SysTick_Handler();
#ifdef PROFILER_nRF5Xxxxx
		SysprogsRTOSHooks_FreeRTOS_RTC1_IRQHandler();
#endif
		SysprogsRTOSHooks_FreeRTOS_SVC_Handler();
		SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND_Actual(0);
		SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE_Actual(0);
		SysprogsRTOSHooks_FreeRTOS_SchedulerStarting();
		ReferenceFreeRTOSSymbols();
	}
}

#else

void missing_USE_FREERTOS_macro();

void __attribute__((weak)) SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND(void *p)
{
	(void)p;
	//If you encounter a 'undefined reference to missing_USE_FREERTOS_macro' error, this means that you are using FreeRTOS, but
	//the USE_FREERTOS macro is not defined. Please add it to the Preprocessor Macros field in VisualGDB Project Properties.
	missing_USE_FREERTOS_macro();
}

void __attribute__((weak)) SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE(void *p)
{
	(void)p;
	missing_USE_FREERTOS_macro();
}
#endif