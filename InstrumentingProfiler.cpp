#include "ProfilerCompatibility.h"
#include "SysprogsProfilerInterface.h"
#include <string.h>
#include "DebuggerChecker.h"

#if !defined(__IAR_SYSTEMS_ICC__) && !defined(__CC_ARM)

typedef unsigned long long ProfilerTimeType;

typedef unsigned uintptr_t;

extern int g_FastSemihostingCallActive;

enum RealTimeTracePacketType
{
	rtpInitialization = 1,
	rtpFunctionRuntime = 2,
	rtpOverflow = 3,
	rtpOverheadReport = 4,
	rtpThreadSwitch = 5,
	rtpThreadCreated = 6,
	rtpResourceTaken = 7,
	rtpResourceReleased = 8,
	rtpSignedValueChanged = 9,
	rtpUnsignedValueChanged = 10,
	rtpFPValueChanged = 11,
	rtpCustomEvent = 12,
	rtpCustomEventEx = 13,
	rtpNewTicksPerSecond = 14
};

#ifndef SYSPROGS_PROFILER_MAX_THREADS
#define SYSPROGS_PROFILER_MAX_THREADS 16
#endif

#if (defined(NRF51) || defined(NRF52)) && defined(SOFTDEVICE_PRESENT)
#include <nrf_nvic.h>
#define SYSPROGS_PROFILER_NORDIC_INTERRUPT_WORKAROUND
#endif

#ifndef __countof
#define __countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

int g_SuppressInstrumentingProfiler = 1;
int g_StopOnRealTimeReportingBufferOverflow = 0;

enum InstrumentingProfilerFlags
{
	ipfNone = 0x00,
	ipfProfileFunctionCalls = 0x01,
	ipfRecordFunctionTiming = 0x02,
	ipfVerifyFunctionStacks = 0x04,
	ipfReportThreadCreation = 0x08,
	ipfReportThreadTimes = 0x10
};

InstrumentingProfilerFlags g_InstrumentingProfilerRTOSFlags;

#ifndef SYSPROGS_PROFILER_USE_DWT_CYCLE_COUNTER
#if defined(NRF51) || defined(NRF52)
#define SYSPROGS_PROFILER_USE_DWT_CYCLE_COUNTER 0 //Nordic nRFx devices do not support DWT cycle counter
#endif
#endif

//This is the default implementation of the performance counter querying code.
//It uses the DWT_CYCCNT register that directly counts processor cycles.
//You can add "SYSPROGS_PROFILER_USE_DWT_CYCLE_COUNTER=0" to the preprocessor macros
//and provide your own implementation of this (e.g. using some hardware timer)
#if !defined(SYSPROGS_PROFILER_USE_DWT_CYCLE_COUNTER) || SYSPROGS_PROFILER_USE_DWT_CYCLE_COUNTER
#define SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter_Inline
#ifdef PROFILER_RP2040

#include <hardware/timer.h>

/* RP2040 does not support reading the DWT cycle counter register programmatically, and the hardware PWM units only have 16-bit precision.
 * Hence, we use the system timer hardware to count function run times. It does not offer cycle-level accuracy, however, should be sufficient
 * for most uses.
 * 
 * For cycle-level accuracy, consider creating a custom implementation of this function based on the PWM or PIO hardware.
 */
static unsigned SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter()
{
	uint32_t high = timer_hw->timehr;
	uint32_t low = timer_hw->timelr;
	uint64_t value = (((uint64_t)high) << 32) | low;
	static uint64_t offset = 0;
	uint64_t result = value - offset;
	offset = value;
	return result;
}
#else
static unsigned SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter()
{
#define DWT_CYCCNT (*((unsigned *)0xE0001004))
#define DWT_CTRL (*((unsigned *)0xE0001000))
#define DWT_LAR (*((unsigned *)0xE0001FB0))
#define DWT_LSR (*((unsigned *)0xE0001FB4))
#define COREDEBUG_DEMCR (*((unsigned *)0xe000edfc))

	static int Initialized = 0;
	if (!Initialized)
	{
		COREDEBUG_DEMCR |= 0x01000000;
#ifdef PROFILER_STM32F7
		if (DWT_LSR & 1)
			DWT_LAR = 0xC5ACCE55;
#endif
		
		DWT_CTRL = 1;
		DWT_CYCCNT = 0;
		Initialized = 1;
	}

	unsigned result = DWT_CYCCNT;
	DWT_CYCCNT = 0;
	return result;
}
#endif
#else
extern "C" unsigned SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
#endif

static __attribute__((noinline)) void ReportRealTimeAnalysisBufferOverflow();

namespace SysprogsInstrumentingProfiler
{
#include "SmallNumberCoder.h"

	namespace Chronometer
	{
		ProfilerTimeType ApplicationClockBase;
		ProfilerTimeType ProfilerTimeOverhead;

		unsigned IncludeOverheadTimeInAppTime; //Should be set to -1 to enable

		class ProfilerTimeRegionRAII
		{
		private:
			ProfilerTimeType m_StartTime;

		public:
			ProfilerTimeRegionRAII()
			{
				if (!g_SuppressInstrumentingProfiler++)
					m_StartTime = ApplicationClockBase += SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
				else
					m_StartTime = ApplicationClockBase;
			}

			~ProfilerTimeRegionRAII()
			{
				if (g_SuppressInstrumentingProfiler == 1)
				{
					unsigned thisOperationOverhead = SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
					ProfilerTimeOverhead += thisOperationOverhead;
					ApplicationClockBase += thisOperationOverhead & IncludeOverheadTimeInAppTime;
				}
				g_SuppressInstrumentingProfiler--;
			}

			ProfilerTimeType GetApplicationTime()
			{
				return m_StartTime;
			}

			static ProfilerTimeType GetCurrentTimeForRealTimeWatch()
			{
				return ApplicationClockBase += SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
			}
		};
	} // namespace Chronometer

	namespace VendorSpecificWorkarounds
	{
		class VendorSpecificInterruptHolderRAII
		{
#ifdef SYSPROGS_PROFILER_NORDIC_INTERRUPT_WORKAROUND
		private:
			uint8_t m_Nested;

		public:
			VendorSpecificInterruptHolderRAII()
			{
				sd_nvic_critical_region_enter(&m_Nested);
			}

			~VendorSpecificInterruptHolderRAII()
			{
				sd_nvic_critical_region_exit(m_Nested);
			}
#endif
		};
	} // namespace VendorSpecificWorkarounds

	typedef unsigned ProfilerUIntPtr;

	struct InstrumentedFrame
	{
		ProfilerTimeType StartTime;
		ProfilerTimeType FoldedTime;

		uintptr_t FunctionAndReportFlag;
		void *LR;

		ProfilerUIntPtr SPAndPSPFlag; //PSP flag is stored in the LSB of PC

		InstrumentedFrame *pNextFrame;

		bool IsInterrupt() const
		{
			return ((int)LR) < 0;
		}

		bool IsReported() const
		{
			return FunctionAndReportFlag & (1U << 31);
		}

		void FlagAsReported() const
		{
			const_cast<InstrumentedFrame *>(this)->FunctionAndReportFlag |= (1U << 31);
		}
	};

	struct ProfilerThreadRecord
	{
		void *pOriginalThread;
		InstrumentedFrame *pTopFrame;
	};

	static unsigned ProfilerTimeToUInt32(ProfilerTimeType time)
	{
		if (time > 0xFFFFFFFF)
			return 0xFFFFFFFF;
		else
			return time;
	}

	enum InstrumentingProfilerErrorCode
	{
		ipeNoError = 0,
		ipeFrameBufferOverflow,
		ipeStackPointerMismatch,
		ipeNoFrames,
		ipeScratchBufferOverflow,
		ipeInterfaceError,
		ipeOutOfThreadSlots,
		ipeStackNotAligned
	};

	void __attribute__((noinline)) RaiseError(InstrumentingProfilerErrorCode errorCode, uintptr_t pArg = 0)
	{
		(void)errorCode;
		(void)pArg;
		//If you have stopped here, the profiler has detected an unrecoverable error.
		//Examine the errorCode variable (r0) and pArg (r1) for more details.
		asm("bkpt 255");
	}

	static ProfilerThreadRecord s_MainThreadState;
	static ProfilerThreadRecord *s_pCurrentThreadState = &s_MainThreadState;
	static ProfilerThreadRecord s_AllThreadRecords[SYSPROGS_PROFILER_MAX_THREADS];
	static int s_ThreadIDReportPending;

	class InstrumentedFramePool
	{
	private:
		int m_Initialized;
		InstrumentedFrame m_FramePool[128];
		InstrumentedFrame *m_pFirstAvailableFrame;

	public:
		InstrumentedFrame *AllocateFrame()
		{
			if (!m_Initialized)
			{
				for (int i = 0; i < (int)__countof(m_FramePool) - 1; i++)
					m_FramePool[i].pNextFrame = &m_FramePool[i + 1];
				m_pFirstAvailableFrame = &m_FramePool[0];
				m_Initialized = true;
			}

			if (!m_pFirstAvailableFrame)
				RaiseError(ipeFrameBufferOverflow);

			InstrumentedFrame *pFrame = m_pFirstAvailableFrame;
			m_pFirstAvailableFrame = pFrame->pNextFrame;
			return pFrame;
		}

		void ReleaseFrame(InstrumentedFrame *pFrame)
		{
			pFrame->pNextFrame = m_pFirstAvailableFrame;
			m_pFirstAvailableFrame = pFrame;
		}
	} s_InstrumentedFramePool;

	static uintptr_t s_FrameAddressBase;

	unsigned FunctionFoldingThreshold;

	void __attribute__((noinline)) ReportFramesToProfiler(const InstrumentedFrame *pTopFrame, ProfilerTimeType runTime)
	{
		static char buffer[32];
		SmallNumberCoder coder(buffer, sizeof(buffer), 0, 0);

		if (s_ThreadIDReportPending)
		{
			s_ThreadIDReportPending = 0;
			if (!coder.WritePackedUIntPair(0x7fff, 0))
				RaiseError(ipeScratchBufferOverflow);
			//TODO: report thread name if pending
			coder.WriteSmallUnsignedInt((unsigned)s_pCurrentThreadState->pOriginalThread);
		}

		unsigned totalFrames = 0;
		unsigned unreportedFrames = -1;
		for (const InstrumentedFrame *pFrame = pTopFrame; pFrame; pFrame = pFrame->pNextFrame)
		{
			if (pFrame->IsReported())
			{
				if ((int)unreportedFrames < 0)
					unreportedFrames = totalFrames;
			}
			totalFrames++;
		}

		if ((int)unreportedFrames < 0)
			unreportedFrames = totalFrames;

		if (!coder.WritePackedUIntPair(totalFrames - unreportedFrames, unreportedFrames))
			RaiseError(ipeScratchBufferOverflow);

		const InstrumentedFrame *pFrame = pTopFrame;
		while (unreportedFrames)
		{
			int addrDelta = ((pFrame->FunctionAndReportFlag & ~(1 << 31)) - s_FrameAddressBase);
			s_FrameAddressBase += addrDelta;
			if (!coder.WriteSmallSignedIntWithFlag(addrDelta, pFrame->IsInterrupt()))
				RaiseError(ipeScratchBufferOverflow);
#if DEBUG
			if (!pFrame || pFrame->IsReported())
				asm("bkpt 255");
#endif
			pFrame->FlagAsReported();

			if (coder.RemainingSize() < sizeof(buffer) / 2)
			{
				while (!SysprogsProfiler_WriteData(pdcInstrumentationProfilerNormalStream,
												   (char *)coder.GetBuffer(),
												   0,
												   coder.GetBuffer(),
												   coder.GetOffset()))
				{
					asm("nop");
				}

				coder.SetOffset(0);
			}
			pFrame = pFrame->pNextFrame;
			unreportedFrames--;
		}

		if (!coder.WriteSmallUnsignedIntWithFlag(ProfilerTimeToUInt32(runTime), pTopFrame->FoldedTime != 0))
			RaiseError(ipeScratchBufferOverflow);

		if (pTopFrame->FoldedTime)
			if (!coder.WriteSmallUnsignedInt(ProfilerTimeToUInt32(pTopFrame->FoldedTime)))
				RaiseError(ipeScratchBufferOverflow);

		while (!SysprogsProfiler_WriteData(pdcInstrumentationProfilerNormalStream,
										   (char *)coder.GetBuffer(),
										   0,
										   coder.GetBuffer(),
										   coder.GetOffset()))
		{
			asm("nop");
		}
	}

	struct RunTimeReportRecord
	{
		unsigned Type;
		unsigned Address;
		int StartTimeDelta;
		unsigned RunTime;
	};

	static ProfilerTimeType LastReportedRealTimeWatchTime = 0;

	void __attribute__((noinline)) ReportFunctionRunTimeToRealTimeWatch(const InstrumentedFrame *pTopFrame, ProfilerTimeType runTime)
	{
		RunTimeReportRecord rec = {.Type = rtpFunctionRuntime, .Address = pTopFrame->FunctionAndReportFlag, .StartTimeDelta = (int)(pTopFrame->StartTime - LastReportedRealTimeWatchTime), .RunTime = ProfilerTimeToUInt32(runTime)};
		LastReportedRealTimeWatchTime = pTopFrame->StartTime;
		while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &rec, sizeof(rec), 0, 0) && !g_FastSemihostingCallActive)
		{
			ReportRealTimeAnalysisBufferOverflow();
		}
	}

	//This function gets invoked when an instrumented function returns. It simply saves the volatile registers to the stack
	//and invokes SysprogsInstrumentingProfilerReturnHookImpl() that does all the actual work.
	static void __attribute__((naked)) ReturnHook()
	{
		asm("push {r0-r3}");
		asm("push {lr}");
#ifndef SYSPROGS_PROFILER_NORDIC_INTERRUPT_WORKAROUND
		asm("mrs r0, faultmask");
		asm("tst r0, r0");
		asm("bne ReturnHook_NoInterrupt");
		asm("ReturnHook_Interrupt:");
		asm("cpsid f");
		asm("mov r0, sp");
		asm("bl SysprogsInstrumentingProfilerReturnHookImpl");
		asm("pop {r0}");
		asm("mov lr, r0");
		asm("pop {r0-r3}");
		asm("cpsie f");
		asm("bx lr");
#endif
		asm("ReturnHook_NoInterrupt:");
		asm("mov r0, sp");
		asm("bl SysprogsInstrumentingProfilerReturnHookImpl");
		asm("pop {r0}");
		asm("mov lr, r0");
		asm("pop {r0-r3}");
		asm("bx lr");
	}

	extern "C" {
	//This function looks up the original value of the 'LR' register for an instrumented frame and restores it.
	//It also reports the performance information to the profiling GUI.
	void SysprogsInstrumentingProfilerReturnHookImpl(void **pStack)
	{
		VendorSpecificWorkarounds::VendorSpecificInterruptHolderRAII holder;
		Chronometer::ProfilerTimeRegionRAII region;
		InstrumentedFrame *pExitingFrame = s_pCurrentThreadState->pTopFrame;
		if (!pExitingFrame)
			RaiseError(ipeNoFrames);

		if ((ProfilerUIntPtr)pStack != ((ProfilerUIntPtr)pExitingFrame->SPAndPSPFlag & ~1) - 2 * sizeof(void *))
			RaiseError(ipeStackPointerMismatch, pExitingFrame->FunctionAndReportFlag & ~(1 << 31));

		pStack[0] = pExitingFrame->LR;
		ProfilerTimeType runTime = region.GetApplicationTime() - pExitingFrame->StartTime;
		if (runTime >= FunctionFoldingThreshold && !g_FastSemihostingCallActive)
		{
			ReportFramesToProfiler(pExitingFrame, runTime);
		}
		else
		{
			if (pExitingFrame->pNextFrame)
				pExitingFrame->pNextFrame += runTime;
		}

		s_pCurrentThreadState->pTopFrame = pExitingFrame->pNextFrame;
		s_InstrumentedFramePool.ReleaseFrame(pExitingFrame);
	}

#if defined (USE_FREERTOS) || defined(USE_RTX)
	__attribute__((always_inline)) static inline bool IsProcessStackMode(void)
	{
		unsigned result;
		asm volatile("MRS %0, control"
					 : "=r"(result));
		return !!(result & 2);
	}
#else
	__attribute__((always_inline)) static inline bool IsProcessStackMode(void)
	{
		return false;
	}
#endif

	//This function gets called when an instrumented function is called. It remembers the time when the function was called
	//and replaces the LR value with the address of ReturnHook. When the function returns, ReturnHook will subtract the
	//function start time from the then-current time and use it to determine how long the instrumented function was running.
	//Note that this function gets called by SysprogsInstrumentingProfilerHook() that prepares a certain stack layout.
	void SysprogsInstrumentingProfilerHookImpl(void **pStack)
	{
		VendorSpecificWorkarounds::VendorSpecificInterruptHolderRAII holder;
		Chronometer::ProfilerTimeRegionRAII region;
		void *OriginalLR = pStack[0];
		bool isPSP = IsProcessStackMode();
		pStack[0] = (void *)&ReturnHook;

		if ((unsigned)pStack & 3)
		{
			RaiseError(SysprogsInstrumentingProfiler::ipeStackNotAligned);
		}

		ProfilerUIntPtr stackWithPSPFlag = (ProfilerUIntPtr)pStack | isPSP;

		ProfilerThreadRecord *pThread = s_pCurrentThreadState;
		while (pThread->pTopFrame && pThread->pTopFrame->SPAndPSPFlag < stackWithPSPFlag && (pThread->pTopFrame->SPAndPSPFlag & 1) == isPSP)
		{
			InstrumentedFrame *pFrame = pThread->pTopFrame;
			//This can be caused either by tail call optimization, or by stack corruption. We assume the first one and
			//treat it like if the original function has exited and the next function was called.

			//As ARM Cortex devices have 2 stacks (MSP/PSP), we only do this if both frames are from the same stack.
			ReportFramesToProfiler(pFrame, region.GetApplicationTime() - pFrame->StartTime);
			pThread->pTopFrame = pFrame->pNextFrame;
			s_InstrumentedFramePool.ReleaseFrame(pFrame);
		}

		InstrumentedFrame *pNewFrame = s_InstrumentedFramePool.AllocateFrame();

		pNewFrame->StartTime = region.GetApplicationTime();
		void *OriginalFunction = pStack[2];
		pNewFrame->FunctionAndReportFlag = (uintptr_t)OriginalFunction;
		pNewFrame->LR = OriginalLR;
		pNewFrame->SPAndPSPFlag = stackWithPSPFlag;
		pNewFrame->FoldedTime = 0;
		pNewFrame->pNextFrame = pThread->pTopFrame;
		pThread->pTopFrame = pNewFrame;
	}
	}

	void ReportThreadCreated(void *newThread, const char *pThreadName)
	{
		if (g_InstrumentingProfilerRTOSFlags & ipfReportThreadCreation)
		{
			unsigned length = strlen(pThreadName);
			unsigned header[2] = {(length << 8) | rtpThreadCreated, (unsigned)newThread};

			while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &header, sizeof(header), pThreadName, length))
			{
				asm("nop");
			}
		}
	}

	static int GetRelativeTimestamp()
	{
		ProfilerTimeType timestamp = SysprogsInstrumentingProfiler::Chronometer::ProfilerTimeRegionRAII::GetCurrentTimeForRealTimeWatch();
		int result = (int)(timestamp - SysprogsInstrumentingProfiler::LastReportedRealTimeWatchTime);
		SysprogsInstrumentingProfiler::LastReportedRealTimeWatchTime = timestamp;
		return result;
	}

	void ReportThreadSwitch(void *newThread)
	{
		if (g_InstrumentingProfilerRTOSFlags & ipfReportThreadTimes)
		{
			unsigned char type = rtpThreadSwitch;
			int payload[2] = {GetRelativeTimestamp(), (int)newThread};
			while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &type, 1, payload, sizeof(payload)))
			{
				asm("nop");
			}
		}
	}

} // namespace SysprogsInstrumentingProfiler

#ifdef __thumb2__
#define CLEAR_R0_BIT_0() \
	asm("bic r0, #1");
#else
#define CLEAR_R0_BIT_0() \
	asm("lsr r0, #1");   \
	asm("lsl r0, #1");
#endif

#define SYSPROGS_THUMB_HOOK_PROLOGUE()                                      \
	/* Old stack layout: [R0] [R1] [LR]. New stack layout: [LR] [R0] [PC]*/ \
	asm("str r0, [sp, #4]"); /* [new R0] = r0 */                            \
	asm("ldr r0, [sp, #8]"); /* r0 = [old LR] */                            \
	asm("str r0, [sp, #0]"); /* [new LR] = r0 */                            \
	asm("mov r0, lr");                                                      \
	CLEAR_R0_BIT_0()                                                        \
	asm("ldr r0, [r0, #0]"); /* r0 = [original PC] */                       \
	asm("str r0, [sp, #8]"); /* [new PC] = r0 */

#define SYSPROGS_THUMB_HOOK_EPILOGUE() \
	asm("pop {r0}");                   \
	asm("mov lr, r0");                 \
	asm("pop {r0, pc}");

//Loads the location of the original function address to r0. Can be used to read extra tags stored after it.
#define SYSPROGS_THUMB_HOOK_PROLOGUE_WITH_TAG_PUSHES_R1()                   \
	/* Old stack layout: [R0] [R1] [LR]. New stack layout: [LR] [R0] [PC]*/ \
	asm("str r0, [sp, #4]"); /* [new R0] = r0 */                            \
	asm("ldr r0, [sp, #8]"); /* r0 = [old LR] */                            \
	asm("str r0, [sp, #0]"); /* [new LR] = r0 */                            \
	asm("mov r0, lr");                                                      \
	CLEAR_R0_BIT_0()                                                        \
	asm("push {r1}");                                                       \
	asm("ldr r1, [r0, #0]");  /* r0 = [original PC] */                      \
	asm("str r1, [sp, #12]"); /* [new PC] = r0 */

#define SYSPROGS_PROFILER_EXIT_IF_SUSPENDED(exitLabelName) \
	asm("ldr r0, =g_SuppressInstrumentingProfiler");       \
	asm("ldr r0, [r0]");                                   \
	asm("tst r0, r0");                                     \
	asm("bne " exitLabelName);

//All instrumented functions will call this function before doing the actual work.
//This function saves all volatile registers to the stack, invokes SysprogsInstrumentingProfilerHookImpl(),
//restores the registers and jumps back to the original function that was instrumented.
//Note that the LR register will be replaced by SysprogsInstrumentingProfilerHookImpl() so that the
//ReturnHook() gets invoked when the instrumented function returns.
extern "C" __attribute__((naked)) void SysprogsInstrumentingProfilerHook()
{
	SYSPROGS_THUMB_HOOK_PROLOGUE();
	asm("mrs r0, faultmask");
	asm("tst r0, r0");
#ifndef SYSPROGS_PROFILER_NORDIC_INTERRUPT_WORKAROUND
	asm("bne ProfilerHook_NoInterrupt");
	asm("ProfilerHook_Interrupt:");
	asm("cpsid f");
	SYSPROGS_PROFILER_EXIT_IF_SUSPENDED("ProfilerHook_Interrupt_Exit");
	asm("mov r0, sp");
	asm("push {r1-r3}");
	asm("bl SysprogsInstrumentingProfilerHookImpl");
	asm("pop {r1-r3}");
	asm("ProfilerHook_Interrupt_Exit:");
	asm("cpsie f");
	SYSPROGS_THUMB_HOOK_EPILOGUE();
#endif
	asm("ProfilerHook_NoInterrupt:");
	SYSPROGS_PROFILER_EXIT_IF_SUSPENDED("ProfilerHook_NoInterrupt_Exit");
	asm("mov r0, sp");
	asm("push {r1-r3}");
	asm("bl SysprogsInstrumentingProfilerHookImpl");
	asm("pop {r1-r3}");
	asm("ProfilerHook_NoInterrupt_Exit:");
	SYSPROGS_THUMB_HOOK_EPILOGUE();
}

namespace SysprogsStackVerifier
{
	void *StackLimit = 0;
}

//Stack verifier enabled, timing analysis disabled
extern "C" __attribute__((naked)) void SysprogsStackVerifierHook()
{
	SYSPROGS_THUMB_HOOK_PROLOGUE_WITH_TAG_PUSHES_R1();
	asm("add r0, #4");
	asm("ldr r1, [r0]");
	asm("ldr r0, =0xffff");
	asm("and r1, r0");
	asm("ldr r0, =_ZN21SysprogsStackVerifier10StackLimitE");
	asm("ldr r0, [r0]");
	asm("add r0, r1");
	asm("cmp r0, sp");
	asm("bls StackVerifierHook_Exit");
	asm("ldr r0, [sp, #4]");
	asm("mov lr, r0");
	asm("ldr r0, [sp, #12]");
	asm("bkpt 255");
	asm("StackVerifierHook_Exit:");
	asm("pop {r1}");
	SYSPROGS_THUMB_HOOK_EPILOGUE();
}

volatile void *SysprogsProfiler_FunctionHookTable;
extern volatile void *__attribute__((alias("SysprogsProfiler_FunctionHookTable"))) SysprogsProfiler_FunctionHookTableEnd;

//Timing analysis enabled, stack verifier disabled
extern "C" __attribute__((naked)) void SysprogsTimingRecorderHook()
{
	SYSPROGS_THUMB_HOOK_PROLOGUE_WITH_TAG_PUSHES_R1();
	asm("push {r2-r3}");
	asm("add r0, #4");
	asm("ldr r1, [r0]");
	asm("lsr r2, r1, #5");
	asm("mov r3, #31");
	asm("and r1, r3");
	asm("mov r3, #1");
	asm("lsl r3, r1");
	asm("ldr r1, =SysprogsProfiler_FunctionHookTable");
	asm("add r1, r2");
	asm("ldr r1, [r1]");
	asm("tst r1, r3");
	asm("beq TimingRecorderHook_NoInterrupt_Exit");
#ifndef SYSPROGS_PROFILER_NORDIC_INTERRUPT_WORKAROUND
	asm("mrs r0, faultmask");
	asm("tst r0, r0");
	asm("bne TimingRecorderHook_NoInterrupt");
	asm("TimingRecorderHook_Interrupt:");
	asm("cpsid f");
	SYSPROGS_PROFILER_EXIT_IF_SUSPENDED("TimingRecorderHook_Interrupt_Exit");
	asm("mov r0, sp");
	asm("add r0, #12");
	asm("bl SysprogsInstrumentingProfilerHookImpl");
	asm("TimingRecorderHook_Interrupt_Exit:");
	asm("cpsie f");
	asm("pop {r2-r3}");
	asm("pop {r1}");
	SYSPROGS_THUMB_HOOK_EPILOGUE();
#endif
	asm("TimingRecorderHook_NoInterrupt:");
	SYSPROGS_PROFILER_EXIT_IF_SUSPENDED("TimingRecorderHook_NoInterrupt_Exit");
	asm("mov r0, sp");
	asm("add r0, #12");
	asm("bl SysprogsInstrumentingProfilerHookImpl");
	asm("TimingRecorderHook_NoInterrupt_Exit:");
	asm("pop {r2-r3}");
	asm("pop {r1}");
	SYSPROGS_THUMB_HOOK_EPILOGUE();
}

namespace OverheadMeasurementFunctions
{
	__attribute__((noinline, naked, optimize("-O0"))) void NonInstrumented()
	{
		asm("bx lr");
	}

	__attribute__((noinline, naked, optimize("-O0"))) void Instrumented()
	{
		asm("bx lr");
	}

	__attribute__((noinline, naked, optimize("-O0"))) void InstrumentedAndReporting()
	{
		asm("bx lr");
	}
} // namespace OverheadMeasurementFunctions

struct OverheadReportPacket
{
	unsigned TypeAndBaseTime;
	unsigned InstrumentedTime;
	unsigned ReportingTime;
};

extern "C" void __attribute__((noinline)) InstrumentingProfilerInitialized(int measureOverhead = 0)
{
	if (measureOverhead)
	{
		asm volatile("cpsid f");
		OverheadReportPacket packet;
		SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase = 0;
		SysprogsInstrumentingProfiler::Chronometer::ProfilerTimeOverhead = 0;
		SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
		OverheadMeasurementFunctions::NonInstrumented();
		packet.TypeAndBaseTime = ((SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase + SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter()) << 8) | rtpOverheadReport;
		SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase = 0;
		SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
		OverheadMeasurementFunctions::Instrumented();
		packet.InstrumentedTime = SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase + SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
		SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase = 0;
		SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
		OverheadMeasurementFunctions::InstrumentedAndReporting();
		packet.ReportingTime = SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase + SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();
		SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &packet, sizeof(packet), 0, 0);

		SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase = 0;
		SysprogsInstrumentingProfiler::Chronometer::ProfilerTimeOverhead = 0;
		asm volatile("cpsie f");
	}
}

void SysprogsProfiler_RTOSThreadSwitched(void *newThread, const char *pThreadName, void *pStackLimit)
{
	if (g_InstrumentingProfilerRTOSFlags & (ipfProfileFunctionCalls | ipfVerifyFunctionStacks | ipfRecordFunctionTiming | ipfReportThreadCreation | ipfReportThreadTimes))
	{
		SysprogsStackVerifier::StackLimit = pStackLimit;
		for (int i = 0; i < (int)__countof(SysprogsInstrumentingProfiler::s_AllThreadRecords); i++)
		{
			if (SysprogsInstrumentingProfiler::s_AllThreadRecords[i].pOriginalThread == newThread)
			{
				SysprogsInstrumentingProfiler::s_pCurrentThreadState = &SysprogsInstrumentingProfiler::s_AllThreadRecords[i];
				SysprogsInstrumentingProfiler::s_ThreadIDReportPending = 1;
				SysprogsInstrumentingProfiler::ReportThreadSwitch(newThread);
				return;
			}
		}

		//Current thread is unknown. Create a new record
		for (int i = 0; i < (int)__countof(SysprogsInstrumentingProfiler::s_AllThreadRecords); i++)
		{
			if (!SysprogsInstrumentingProfiler::s_AllThreadRecords[i].pOriginalThread)
			{
				SysprogsInstrumentingProfiler::s_AllThreadRecords[i].pOriginalThread = newThread;
				SysprogsInstrumentingProfiler::s_pCurrentThreadState = &SysprogsInstrumentingProfiler::s_AllThreadRecords[i];
				SysprogsInstrumentingProfiler::s_ThreadIDReportPending = 1;
				SysprogsInstrumentingProfiler::ReportThreadCreated(newThread, pThreadName);
				SysprogsInstrumentingProfiler::ReportThreadSwitch(newThread);
				return;
			}
		}

		//Too many RTOS threads found. Increase SYSPROGS_PROFILER_MAX_THREADS in order to be able to profile them.
		SysprogsInstrumentingProfiler::RaiseError(SysprogsInstrumentingProfiler::ipeOutOfThreadSlots);
	}
}

void SysprogsProfiler_RTOSThreadDeleted(void *thread)
{
	for (int i = 0; i < (int)__countof(SysprogsInstrumentingProfiler::s_AllThreadRecords); i++)
	{
		if (SysprogsInstrumentingProfiler::s_AllThreadRecords[i].pOriginalThread == thread)
		{
			SysprogsInstrumentingProfiler::s_AllThreadRecords[i].pOriginalThread = 0;
			for (SysprogsInstrumentingProfiler::InstrumentedFrame *pFrame = SysprogsInstrumentingProfiler::s_AllThreadRecords[i].pTopFrame, *pNextFrame; pFrame; pFrame = pNextFrame)
			{
				pNextFrame = pFrame->pNextFrame;
				SysprogsInstrumentingProfiler::s_InstrumentedFramePool.ReleaseFrame(pFrame);
			}
		}
	}
}

void SysprogsProfiler_ReportResourceTaken(void *pResource, void *pOwner, unsigned optional24BitTag)
{
	unsigned msg[] = {optional24BitTag << 8 | rtpResourceTaken, (unsigned)SysprogsInstrumentingProfiler::GetRelativeTimestamp(), (unsigned)pResource, (unsigned)pOwner};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), 0, 0))
	{
		ReportRealTimeAnalysisBufferOverflow();
	}
}

void SysprogsProfiler_ReportResourceReleased(void *pResource, void *pOwner, unsigned optional24BitTag)
{
	unsigned msg[] = {optional24BitTag << 8 | rtpResourceReleased, (unsigned)SysprogsInstrumentingProfiler::GetRelativeTimestamp(), (unsigned)pResource, (unsigned)pOwner};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), 0, 0))
	{
		ReportRealTimeAnalysisBufferOverflow();
	}
}

void SysprogsProfiler_ReportIntegralValue(void *pResource, unsigned value, int reportAsSigned)
{
	unsigned msg[] = {(reportAsSigned ? rtpSignedValueChanged : rtpUnsignedValueChanged), (unsigned)SysprogsInstrumentingProfiler::GetRelativeTimestamp(), (unsigned)pResource, value};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), 0, 0))
	{
		ReportRealTimeAnalysisBufferOverflow();
	}
}

void SysprogsProfiler_ReportFPValue(void *pResource, double value)
{
	unsigned msg[] = {rtpFPValueChanged, (unsigned)SysprogsInstrumentingProfiler::GetRelativeTimestamp(), (unsigned)pResource};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), &value, sizeof(value)))
	{
		ReportRealTimeAnalysisBufferOverflow();
	}
}

void SysprogsProfiler_ReportGenericEvent(void *pResource, const char *pEvent)
{
	unsigned length = pEvent ? strlen(pEvent) : 0;
	unsigned msg[] = {length << 8 | rtpCustomEvent, (unsigned)SysprogsInstrumentingProfiler::GetRelativeTimestamp(), (unsigned)pResource};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), pEvent, length))
	{
		ReportRealTimeAnalysisBufferOverflow();
	}
}

void SysprogsProfiler_ReportGenericEventEx(void *pResource, void *argument, RealTimeEventArgType argType, int argSize)
{
	unsigned msg[] = {(unsigned)rtpCustomEventEx | (unsigned)argType << 8, (unsigned)SysprogsInstrumentingProfiler::GetRelativeTimestamp(), (unsigned)pResource};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), argument, argSize))
	{
		ReportRealTimeAnalysisBufferOverflow();
	}
}

extern "C" void __attribute__((weak)) InitializeProfilerRTOSHooks()
{
}

extern "C" void __attribute__((weak)) InitializeProfilerRTOSHooksAfterReportingInitialization()
{
}
extern "C" void __attribute__((noinline)) InitializeInstrumentingProfiler()
{
	if (!CanInvokeSemihostingCalls())
		return;

	bool functionHookTablePresent = false;
	for (volatile void **p = &SysprogsProfiler_FunctionHookTable; p < &SysprogsProfiler_FunctionHookTableEnd; p++)
	{
		functionHookTablePresent = true;
		*p = 0;
	}

	if (functionHookTablePresent)
	{
		SysprogsInstrumentingProfiler::Chronometer::IncludeOverheadTimeInAppTime = -1;

		char rec = (char)rtpInitialization;
		//This ensures that the profiler is initialized and won't require stopping the application when it gets actual data to send.
		SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &rec, 1, 0, 0);
	}

	volatile int x = 0;
	if (x)
	{
		//This code is only needed to reference the functions so that they won't be remoevd by the linker.
		SysprogsInstrumentingProfilerHook();
		SysprogsStackVerifierHook();
		SysprogsTimingRecorderHook();
		SysprogsInstrumentingProfiler::SysprogsInstrumentingProfilerHookImpl(0);
		SysprogsInstrumentingProfiler::SysprogsInstrumentingProfilerReturnHookImpl(0);
		SysprogsInstrumentingProfiler::ReportFunctionRunTimeToRealTimeWatch(0, 0);
	}
	InitializeProfilerRTOSHooks();

	g_SuppressInstrumentingProfiler = 0;
	InstrumentingProfilerInitialized(x);
	InitializeProfilerRTOSHooksAfterReportingInitialization();

#ifdef SYSPROGS_PROFILER_NORDIC_INTERRUPT_WORKAROUND
	{
		//This ensures that the profiler is initialized and won't require stopping the application when it gets actual data to send.
		char rec = (char)rtpInitialization;
		SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &rec, 1, 0, 0);
	}
#endif
}

extern "C" void ReportTicksPerSecond(unsigned ticksPerSecond)
{
	unsigned msg[] = {rtpNewTicksPerSecond, ticksPerSecond};
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, msg, sizeof(msg), 0, 0))
	{
	}
}

static __attribute__((noinline)) void ReportRealTimeAnalysisBufferOverflow()
{
	if (g_StopOnRealTimeReportingBufferOverflow)
		asm("bkpt 255");

	//Wait for the buffer to empty up
	while (SysprogsProfiler_GetBufferAvailability(4) <= 2)
	{
	}

	ProfilerTimeType overflowRec = SysprogsInstrumentingProfiler::Chronometer::ApplicationClockBase << 8 | rtpOverflow;
	while (!SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &overflowRec, sizeof(overflowRec), 0, 0))
	{
	}
}

extern "C" void InitializeCustomRealTimeWatch()
{
	char rec = (char)rtpInitialization;
	SysprogsProfiler_WriteData(pdcRealTimeAnalysisStream, &rec, 1, 0, 0);
}

#endif
