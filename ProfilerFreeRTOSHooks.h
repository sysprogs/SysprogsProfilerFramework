#pragma once

#ifndef __ASSEMBLER__
#ifndef DISABLE_SYSPROGS_FREERTOS_HOOKS

#ifdef __cplusplus
extern "C" {
#endif

//The normal hook functions are empty and won't cause any significant overhead.
//When real-time analysis is enabled, they will be replaced with the actual ones
//that report the queue events to the Real-time watch.
void SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND(void *pQueue);
void SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE(void *pQueue);

#define traceQUEUE_SEND SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND
#define traceQUEUE_RECEIVE SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE
#define traceQUEUE_SEND_FROM_ISR SysprogsRTOSHooks_FreeRTOS_traceQUEUE_SEND
#define traceQUEUE_RECEIVE_FROM_ISR SysprogsRTOSHooks_FreeRTOS_traceQUEUE_RECEIVE

#ifdef __cplusplus
}
#endif

#endif
#endif
