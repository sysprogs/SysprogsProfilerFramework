#include "SysprogsProfilerInterface.h"

#ifndef SAMPLING_PROFILER_COMPARE_FRAMES
/*
	If this option is enabled, the profiler will only report that frames that changed since the last invocation.
	E.g. if main() -> outer() -> inner1() changed to main() -> outer() -> inner2(), the profiler will report 1 new frame (inner2) and tell
	that 2 frames were reused. This compresses the output even more, increasing the overall performance at some extra memory costs.
	
	Note that the the actual frame unwinding happens on the client side and from the sampling profiler's point of view a 'frame' is
	any entry on the stack that looks like a code address or a saved frame pointer.
*/
#define SAMPLING_PROFILER_COMPARE_FRAMES 1
#endif

struct SnapshotSummary
{
	void *SP;
	void *FP;
	void *PC;
	void *LR;
	void *RefPointA, *RefPointB;
#if SAMPLING_PROFILER_COMPARE_FRAMES
	void *StartRefPointA, *StartRefPointB;
#endif
	int StackEntryCount;
};

#define MAX_STACK_SNAPSHOT_SIZE 256
#define MAX_STACK_FRAME_SIZE 256
#define MAX_ENTRIES_PER_STACK_SNAPSHOT 128

static struct FilteredStackSnapshot
{
#if SAMPLING_PROFILER_COMPARE_FRAMES
	int LastUsedReportBuffer;
#endif
	int LastUsedReportBufferSize;
	SnapshotSummary LastReportedRegisterSummary;
	char ReportBufferA[MAX_STACK_SNAPSHOT_SIZE];
#if SAMPLING_PROFILER_COMPARE_FRAMES
	char ReportBufferB[MAX_STACK_SNAPSHOT_SIZE];
#endif
} s_FilteredStackSnapshot;


#ifndef SYSPROGS_PROFILER_END_OF_RAM
extern void *_estack;
#define SYSPROGS_PROFILER_END_OF_RAM (&_estack)
#endif

#if defined(SYSPROGS_PROFILER_USE_CUSTOM_ADDRESS_VALIDATORS) && SYSPROGS_PROFILER_USE_CUSTOM_ADDRESS_VALIDATORS
extern "C" {
int IsValidCodeAddress(void *pAddr);
int IsValidStackAddress(void **pStackSlot);
}
#elif defined (__MBED__)
static inline int IsValidCodeAddress(void *pAddr)
{
	extern void *__etext;
	return (pAddr >= (void *)MBED_ROM_START && pAddr <= (void *)&__etext);
}

static inline int IsValidStackAddress(void **pStackSlot)
{
	extern void *_sdata;
	return pStackSlot >= &_sdata && pStackSlot < (void *)(SYSPROGS_PROFILER_END_OF_RAM);
}
#else
static inline int IsValidCodeAddress(void *pAddr)
{
	extern void *_etext, *_stext;
	return (pAddr >= (void *)&_stext && pAddr <= (void *)&_etext);
}

static inline int IsValidStackAddress(void **pStackSlot)
{
	extern void *_sdata;
	return pStackSlot >= &_sdata && pStackSlot < (void *)(SYSPROGS_PROFILER_END_OF_RAM);
}
#endif

static bool IsPotentialSavedFPSlot(void **pStackSlot)
{
	if (!IsValidStackAddress(pStackSlot))
		return false;
	void **potentialFP = (void **)pStackSlot[0];
	if (potentialFP >= (void *)(SYSPROGS_PROFILER_END_OF_RAM) || potentialFP <= pStackSlot)
		return false;

	return true;
}

#define SAMPLES_PER_AUTORATE_CYCLE 1024
#define SAMPLING_PROFILER_COMM_BUFFER_USAGE_EXP 4

volatile int g_PauseSamplingProfiler;
volatile int g_SamplingProfilerDroppedPacketRatio; // x SAMPLES_PER_AUTORATE_CYCLE = 1024
extern volatile int g_SamplingProfilerRate;
volatile int g_SamplingProfilerAutoRate; //0 if disabled, set to non-0 to set the initial autorate

#include "SmallNumberCoder.h"

extern "C" void SysprogsProfiler_ProcessSample(void *PC, void *SP, void *FP, void *LR)
{
	static int s_PacketsDropped, s_PacketsInCycle, s_AvailabilitySum;
	if (g_PauseSamplingProfiler)
		return;

	void **lastSavedEntry = (void **)SP;
	unsigned entries = 0;

#if SAMPLING_PROFILER_COMPARE_FRAMES
	void *pCodeBuffer = s_FilteredStackSnapshot.LastUsedReportBuffer ? s_FilteredStackSnapshot.ReportBufferA : s_FilteredStackSnapshot.ReportBufferB;
#else
	void *pCodeBuffer = s_FilteredStackSnapshot.ReportBufferA;
#endif

	SmallNumberCoder coder(pCodeBuffer,
						   MAX_STACK_SNAPSHOT_SIZE,
						   s_FilteredStackSnapshot.LastReportedRegisterSummary.RefPointA,
						   s_FilteredStackSnapshot.LastReportedRegisterSummary.RefPointB);

#if SAMPLING_PROFILER_COMPARE_FRAMES
	void *startA = s_FilteredStackSnapshot.LastReportedRegisterSummary.RefPointA;
	void *startB = s_FilteredStackSnapshot.LastReportedRegisterSummary.RefPointB;

	SmallNumberDecoder decoder(s_FilteredStackSnapshot.LastUsedReportBuffer ? s_FilteredStackSnapshot.ReportBufferB : s_FilteredStackSnapshot.ReportBufferA,
							   MAX_STACK_SNAPSHOT_SIZE,
							   s_FilteredStackSnapshot.LastReportedRegisterSummary.StartRefPointA,
							   s_FilteredStackSnapshot.LastReportedRegisterSummary.StartRefPointB);
	void **readBase = (void **)s_FilteredStackSnapshot.LastReportedRegisterSummary.SP;
	int firstMatchingEntry = -1, firstMatchingEntryOffset = -1;
	int readEntries = 0;

	void *refAAtEndOfMismatchingEntries, *refBAtEndOfMismatchingEntries;
#endif

	//1. Compress stack entries
	for (void **thisSP = (void **)SP; SP < (void *)(SYSPROGS_PROFILER_END_OF_RAM) && ((char *)thisSP - (char *)lastSavedEntry) < MAX_STACK_FRAME_SIZE; thisSP++)
	{
#if SAMPLING_PROFILER_COMPARE_FRAMES
		int entryStartOffset = coder.GetOffset();
#endif
		bool writeRefPointB;
		if (IsPotentialSavedFPSlot(thisSP))
			writeRefPointB = true;
		else if (IsValidStackAddress(thisSP) && IsValidCodeAddress(*thisSP))
			writeRefPointB = false;
		else
			continue;

		int indexDelta = thisSP - lastSavedEntry;
		lastSavedEntry = thisSP;

#if SAMPLING_PROFILER_COMPARE_FRAMES
		void *readValue;
		while (readEntries < s_FilteredStackSnapshot.LastReportedRegisterSummary.StackEntryCount && decoder.ReadStackEntry(&readBase, &readValue))
		{
			readEntries++;
			if (readBase < thisSP)
				continue;
			break;
		}

		if (readBase == thisSP && readValue == *((volatile void **)thisSP) && readEntries)
		{
			if (firstMatchingEntry == -1)
			{
				firstMatchingEntry = entries;
				firstMatchingEntryOffset = entryStartOffset;

				coder.ExportState(&refAAtEndOfMismatchingEntries, &refBAtEndOfMismatchingEntries);
			}
		}
		else
			firstMatchingEntry = firstMatchingEntryOffset = -1;
#endif

		if (!coder.WriteStackEntry(indexDelta, *thisSP, writeRefPointB))
			return;

		if (++entries >= MAX_ENTRIES_PER_STACK_SNAPSHOT)
			break;
	}

	int endOfStackEntries = coder.GetOffset();
	int matchingEntries = 0;
#if SAMPLING_PROFILER_COMPARE_FRAMES
	if (firstMatchingEntry != -1 && firstMatchingEntryOffset != -1)
	{
		matchingEntries = entries - firstMatchingEntry;
		entries = firstMatchingEntry;
		endOfStackEntries = firstMatchingEntryOffset;
	}
	else
		coder.ExportState(&refAAtEndOfMismatchingEntries, &refBAtEndOfMismatchingEntries);

	int headerStartOffset = coder.GetOffset();
#else
	int headerStartOffset = endOfStackEntries;
#endif

	//2. Compress register values
	if (!coder.WriteTinySIntWithFlag((((char *)SP - (char *)s_FilteredStackSnapshot.LastReportedRegisterSummary.SP) / 4), FP == SP))
		return;
	if (FP != SP)
	{
		if (!coder.WriteTinySInt((((char *)FP - (char *)SP) / 4)))
			return;
	}

	if (!coder.WriteSmallMostLikelyEvenSInt(((char *)PC - (char *)s_FilteredStackSnapshot.LastReportedRegisterSummary.PC)))
		return;
	if (!coder.WriteSmallMostLikelyEvenSInt(((char *)LR - (char *)s_FilteredStackSnapshot.LastReportedRegisterSummary.LR)))
		return;

	//3. Write stack entry count
	if (!coder.WritePackedUIntPair(matchingEntries, entries))
		return;

	if (++s_PacketsInCycle >= SAMPLES_PER_AUTORATE_CYCLE)
	{
		g_SamplingProfilerDroppedPacketRatio = s_PacketsDropped;

		if (g_SamplingProfilerAutoRate)
		{
			int newRate = 0;
			if (g_SamplingProfilerDroppedPacketRatio)
				newRate = g_SamplingProfilerAutoRate * (SAMPLES_PER_AUTORATE_CYCLE - g_SamplingProfilerDroppedPacketRatio) / SAMPLES_PER_AUTORATE_CYCLE;
			else if ((s_AvailabilitySum / SAMPLES_PER_AUTORATE_CYCLE) > (1 << SAMPLING_PROFILER_COMM_BUFFER_USAGE_EXP) * 1 / 4)
			{
				if ((s_AvailabilitySum / SAMPLES_PER_AUTORATE_CYCLE) > (1 << SAMPLING_PROFILER_COMM_BUFFER_USAGE_EXP) * 7 / 8)
					newRate = g_SamplingProfilerAutoRate * 4;
				else if ((s_AvailabilitySum / SAMPLES_PER_AUTORATE_CYCLE) > (1 << SAMPLING_PROFILER_COMM_BUFFER_USAGE_EXP) * 3 / 4)
					newRate = g_SamplingProfilerAutoRate * 2;
				else
					newRate = g_SamplingProfilerAutoRate + 200;
			}

			if (newRate > 100 && newRate < 100000)
			{
				g_SamplingProfilerAutoRate = newRate;
				g_SamplingProfilerRate = newRate;
			}
		}
		s_PacketsDropped = 0;
		s_PacketsInCycle = 0;
		s_AvailabilitySum = 0;
	}

	if (!SysprogsProfiler_WriteData(pdcSamplingProfilerStream,
									(char *)coder.GetBuffer() + headerStartOffset,
									coder.GetOffset() - headerStartOffset,
									coder.GetBuffer(),
									endOfStackEntries))
	{
		s_PacketsDropped++;
		return;
	}

	s_AvailabilitySum += SysprogsProfiler_GetBufferAvailability(SAMPLING_PROFILER_COMM_BUFFER_USAGE_EXP);

	s_FilteredStackSnapshot.LastReportedRegisterSummary.FP = FP;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.SP = SP;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.PC = PC;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.LR = LR;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.RefPointA = refAAtEndOfMismatchingEntries;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.RefPointB = refBAtEndOfMismatchingEntries;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.StackEntryCount = entries + matchingEntries;
#if SAMPLING_PROFILER_COMPARE_FRAMES
	s_FilteredStackSnapshot.LastReportedRegisterSummary.StartRefPointA = startA;
	s_FilteredStackSnapshot.LastReportedRegisterSummary.StartRefPointB = startB;
	s_FilteredStackSnapshot.LastUsedReportBuffer = !s_FilteredStackSnapshot.LastUsedReportBuffer;
#endif
}