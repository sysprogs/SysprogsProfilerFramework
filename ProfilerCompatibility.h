#pragma once

#ifdef __MBED__

#ifndef USE_RTX
#define USE_RTX
#endif

#ifndef FAST_SEMIHOSTING_STDIO_DRIVER
#define FAST_SEMIHOSTING_STDIO_DRIVER 0
#endif

#ifdef TARGET_STM32F0
#define PROFILER_STM32F0
#endif

#ifdef TARGET_STM32F1
#define PROFILER_STM32F1
#endif

#ifdef TARGET_STM32F2
#define PROFILER_STM32F2
#endif

#ifdef TARGET_STM32F3
#define PROFILER_STM32F3
#endif

#ifdef TARGET_STM32F4
#define PROFILER_STM32F4
#endif

#ifdef TARGET_STM32F7
#define PROFILER_STM32F7
#endif

#ifdef TARGET_STM32L0
#define PROFILER_STM32L0
#endif

#ifdef TARGET_STM32L1
#define PROFILER_STM32L1
#endif

#ifdef TARGET_STM32L4
#define PROFILER_STM32L4
#endif

#ifdef TARGET_STM32W1
#define PROFILER_STM32W1
#endif

#ifdef TARGET_STM32H7
#define PROFILER_STM32H7
#endif

#endif