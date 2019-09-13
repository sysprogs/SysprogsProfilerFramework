extern "C" {
int _isatty();
int _write(int fd, char *pBuffer, int size);
}

#include <string.h>
#include "DebuggerChecker.h"
#include "FastSemihosting.h"
#include "ProfilerCompatibility.h"

#ifdef __thumb__
#define AngelSWI 0xAB
#else
#define AngelSWI AngelSWI_ARM
#endif

#ifndef FAST_SEMIHOSTING_BUFFER_SIZE
#define FAST_SEMIHOSTING_BUFFER_SIZE 4096
#endif

#ifndef FAST_SEMIHOSTING_BLOCKING_MODE
#define FAST_SEMIHOSTING_BLOCKING_MODE 1
#endif

#ifndef FAST_SEMIHOSTING_STDIO_DRIVER
#define FAST_SEMIHOSTING_STDIO_DRIVER 1
#endif

#ifndef FAST_SEMIHOSTING_PROFILER_DRIVER
#define FAST_SEMIHOSTING_PROFILER_DRIVER 1
#endif

#ifndef FAST_SEMIHOSTING_HOLD_INTERRUPTS
#if defined(USE_FREERTOS) || defined(USE_RTX)
#define FAST_SEMIHOSTING_HOLD_INTERRUPTS 1
#else
#define FAST_SEMIHOSTING_HOLD_INTERRUPTS 0
#endif

#endif

static struct
{
	volatile unsigned ReadOffset;
	volatile unsigned WriteOffset;
	volatile unsigned LastKnownSwitchOffset;
	char Data[FAST_SEMIHOSTING_BUFFER_SIZE];
} s_FastSemihostingState;

static volatile bool s_FastSemihostingInitialized;
static char s_LastKnownChannel;

typedef unsigned ChannelDescriptorType;

static const int SysprogsSemihostingReasonBase = 0x50535953; //'SYSP';

enum
{
	kInitializeFastSemihosting,
	kControlFastSemihostingPolling
};

static void InitializeFastSemihosting()
{
	void *pSemihostingStruct = &s_FastSemihostingState;
	(void)pSemihostingStruct;
	s_FastSemihostingState.ReadOffset = sizeof(s_FastSemihostingState.Data);
	asm volatile("mov r0, %1; mov r1, %0; bkpt %a2" ::"r"(&s_FastSemihostingState), "r"(SysprogsSemihostingReasonBase + kInitializeFastSemihosting), "i"(AngelSWI)
				 : "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc");
	s_FastSemihostingState.ReadOffset = 0;
	s_FastSemihostingState.WriteOffset = 0;
}

static int GetFastSemihostingFreeBufferSize()
{
	unsigned bytesWritten = (s_FastSemihostingState.WriteOffset - s_FastSemihostingState.ReadOffset);
	if (bytesWritten > FAST_SEMIHOSTING_BUFFER_SIZE)
		return 0;

	return FAST_SEMIHOSTING_BUFFER_SIZE - bytesWritten;
}

static int WriteRawFastSemihostingData(const void *pBuffer, int size, int isChannelSwitchRecord)
{
	if (!CanInvokeSemihostingCalls())
		return size; //No debugger connected - ignore all semihosting output.

	if (!s_FastSemihostingInitialized)
	{
		s_FastSemihostingInitialized = true;
		InitializeFastSemihosting();
	}

	unsigned minRequiredSize = (isChannelSwitchRecord ? size : 1) % FAST_SEMIHOSTING_BUFFER_SIZE;
	while ((s_FastSemihostingState.WriteOffset - s_FastSemihostingState.ReadOffset) > (FAST_SEMIHOSTING_BUFFER_SIZE - minRequiredSize))
	{
#if FAST_SEMIHOSTING_BLOCKING_MODE
		continue;
#else
		return 0;
#endif
	}

	int readOff = s_FastSemihostingState.ReadOffset % FAST_SEMIHOSTING_BUFFER_SIZE;
	int writeOff = s_FastSemihostingState.WriteOffset % FAST_SEMIHOSTING_BUFFER_SIZE;
	int todo;
	if (writeOff < readOff)
		todo = readOff - writeOff;
	else
		todo = sizeof(s_FastSemihostingState.Data) - writeOff;

	todo = (size > todo) ? todo : size;
	memcpy(s_FastSemihostingState.Data + writeOff, pBuffer, todo);
	unsigned fullWriteOff = s_FastSemihostingState.WriteOffset;
	if ((writeOff + todo) == FAST_SEMIHOSTING_BUFFER_SIZE && todo != size)
	{
		//We are rolling over the end of the buffer and may be able to write more data at the beginning of it.
		//Normally we could just skip this and let the caller retry, but writing channel switch records requires all-or-nothing semantics, so
		//we have to do it before updating WriteOffset.
		int todo2 = readOff;
		if (todo2 > (size - todo))
			todo2 = size - todo;
		memcpy(s_FastSemihostingState.Data, (char *)pBuffer + todo, todo2);
		if (isChannelSwitchRecord)
			s_FastSemihostingState.LastKnownSwitchOffset = fullWriteOff + todo + todo2;
		s_FastSemihostingState.WriteOffset = fullWriteOff + todo + todo2;
		return todo + todo2;
	}
	else
	{
		if (isChannelSwitchRecord)
			s_FastSemihostingState.LastKnownSwitchOffset = fullWriteOff + todo;
		s_FastSemihostingState.WriteOffset = fullWriteOff + todo;
		return todo;
	}
}

/*
	Multi-channel data format:
	[channel switch record] [data] [data] [data] [channel switch record] [data] ...
	
	Channel switch record DOES NOT specify the length of the remaining data. Instead, it actually points to the previous
	switch record and the global state object points to the last switch record. This has a HUGE advantage of having 0% overhead
	when channel switches are rare (most of the time). One drawback of this approach is that if exactly 2GB of data are passed
	between 2 subsequent channel switches, the client code will ignore the switch and treat it as a part of the data stream.
	This could be fixed by adding an additional counter, but we assume that the probability of this event is extremely low.
	
	The semihosting client code is responsible for 	reconstructing the addresses of the switch records inside the portion of data 
	it reads and in splitting the data between channels.
*/

int g_FastSemihostingCallActive;

#if FAST_SEMIHOSTING_HOLD_INTERRUPTS
__attribute__((always_inline)) static inline unsigned __get_PRIMASK(void)
{
	unsigned result;
	asm volatile("MRS %0, primask"
				 : "=r"(result));
	return (result);
}

__attribute__((always_inline)) static inline void __enable_irq(void)
{
	asm volatile("cpsie i"
				 :
				 :
				 : "memory");
}

__attribute__((always_inline)) static inline void __disable_irq(void)
{
	asm volatile("cpsid i"
				 :
				 :
				 : "memory");
}
#endif

int WriteToFastSemihostingChannel(unsigned char channel, const void *pBuffer, int size, int writeAll)
{
	if (size == 0)
		return 0;

#if FAST_SEMIHOSTING_HOLD_INTERRUPTS
	int interruptsDisabled = __get_PRIMASK();
	__disable_irq();
#endif

	g_FastSemihostingCallActive++;
	channel &= 0x7F;
	if (channel != s_LastKnownChannel)
	{
		unsigned delta = s_FastSemihostingState.WriteOffset - s_FastSemihostingState.LastKnownSwitchOffset;
		int done;
		if (delta < 256)
		{
			unsigned short rec = delta | (channel << 8) | 0x8000; //[delta] [1 || channel]
			done = WriteRawFastSemihostingData(&rec, sizeof(rec), 1);
		}
		else if (delta < 0x007FFFFF)
		{
			unsigned rec = (unsigned)channel << 24 | 0x800000 | delta; //[1 || delta] [0 || channel]
			done = WriteRawFastSemihostingData(&rec, sizeof(rec), 1);
		}
		else
		{
			struct __attribute__((aligned(1), packed))
			{
				unsigned Delta;
				unsigned char Channel;
			} rec = {delta & 0x7FFFFFFF, channel}; //[0 || delta] [0 || channel]
			done = WriteRawFastSemihostingData(&rec, sizeof(rec), 1);
		}

		if (!done)
		{
			g_FastSemihostingCallActive--;
			return 0;
		}

		s_LastKnownChannel = channel;
	}
	int done = 0;
	do
	{
		done += WriteRawFastSemihostingData((const char *)pBuffer + done, size - done, 0);
	} while (writeAll && done != size);

	g_FastSemihostingCallActive--;
#if FAST_SEMIHOSTING_HOLD_INTERRUPTS
	if (!interruptsDisabled)
		__enable_irq();
#endif
	return done;
}

#if FAST_SEMIHOSTING_STDIO_DRIVER
int _isatty()
{
	return 1;
}

int _write(int fd, char *pBuffer, int size)
{
	WriteToFastSemihostingChannel(fd & 0x0f, (const void *)pBuffer, size, FAST_SEMIHOSTING_BLOCKING_MODE);
	//If we return less than [size], the newlib will retry the write call for the remaining bytes.
	//However if we are running the the non-blocking mode, we actually want to skip the extra data to avoid slowdown.
	return size;
}
#endif

#if FAST_SEMIHOSTING_PROFILER_DRIVER
#include "SysprogsProfilerInterface.h"

/*
	Every profiler interface driver should implement this function.
	It should send both the header and the payload to the profiler client running on the development machine.
	The function may be blocking, but should have the all-or-nothing semantics. E.g. if it is being invoked while
	another data transfer is being performed, it can return 0 and the profiler will discard the current data sample in a 
	safe way.
*/
int __attribute((weak)) SysprogsProfiler_WriteData(ProfilerDataChannel channel, const void *pHeader, unsigned headerSize, const void *pPayload, unsigned payloadSize)
{
	extern int g_FastSemihostingCallActive;

#if FAST_SEMIHOSTING_HOLD_INTERRUPTS
	int interruptsDisabled = __get_PRIMASK();
	__disable_irq();
#endif

	int result;
	unsigned availableSpace = GetFastSemihostingFreeBufferSize();
	if (availableSpace < (headerSize + payloadSize))
		result = 0;
	else if (g_FastSemihostingCallActive)
		result = 0;
	else
	{
		int done = WriteToFastSemihostingChannel(channel, pHeader, headerSize, 1);
		if (done != (int)headerSize)
			asm("bkpt 255");
		done = WriteToFastSemihostingChannel(channel, pPayload, payloadSize, 1);
		if (done != (int)payloadSize)
			asm("bkpt 255");

		result = headerSize + payloadSize;
	}

#if FAST_SEMIHOSTING_HOLD_INTERRUPTS
	if (!interruptsDisabled)
		__enable_irq();
#endif

	return result;
}

int SysprogsProfiler_GetBufferAvailability(unsigned exp)
{
	return (GetFastSemihostingFreeBufferSize() << exp) / FAST_SEMIHOSTING_BUFFER_SIZE;
}

#endif

void __attribute__((noinline)) SuspendFastSemihostingPolling()
{
	if (!CanInvokeSemihostingCalls())
		return;

	asm volatile("mov r0, %1; mov r1, %0; bkpt %a2" ::"r"(0), "r"(SysprogsSemihostingReasonBase + kControlFastSemihostingPolling), "i"(AngelSWI)
				 : "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc");
}

void __attribute__((noinline)) ResumeFastSemihostingPolling()
{
	if (!CanInvokeSemihostingCalls())
		return;

	asm volatile("mov r0, %1; mov r1, %0; bkpt %a2" ::"r"(1), "r"(SysprogsSemihostingReasonBase + kControlFastSemihostingPolling), "i"(AngelSWI)
				 : "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc");
}
