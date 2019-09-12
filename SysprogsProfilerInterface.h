#pragma once

typedef enum
{
	pdcSamplingProfilerStream = 0x41,
	pdcInstrumentationProfilerStreamDeprecated = 0x42, //Old protocol that does not support RTOS extensions
	pdcTestReportStream = 0x43,
	pdcRealTimeAnalysisStream = 0x44,
	pdcInstrumentationProfilerNormalStream = 0x45
} ProfilerDataChannel;

typedef enum
{
	rtaSignedInt,
	rtaUnsignedInt,
	rtaFloatingPoint
} RealTimeEventArgType;

#ifdef __cplusplus
extern "C" {
#endif
int SysprogsProfiler_WriteData(ProfilerDataChannel channel, const void *pHeader, unsigned headerSize, const void *pPayload, unsigned payloadSize);
int SysprogsProfiler_GetBufferAvailability(unsigned exp); //Returns (1 << exp) if the buffer is fully available, 0 if fully used, -1 if not supported

unsigned SysprogsInstrumentingProfiler_QueryAndResetPerformanceCounter();

void SysprogsProfiler_RTOSThreadSwitched(void *newThread, const char *pThreadName, void *pStackLimit);
void SysprogsProfiler_RTOSThreadDeleted(void *thread);

void SysprogsProfiler_ReportResourceTaken(void *pResource, void *pOwner, unsigned optional24BitTag);
void SysprogsProfiler_ReportResourceReleased(void *pResource, void *pOwner, unsigned optional24BitTag);

void SysprogsProfiler_ReportIntegralValue(void *pResource, unsigned value, int reportAsSigned);
void SysprogsProfiler_ReportFPValue(void *pResource, double value);
void SysprogsProfiler_ReportGenericEvent(void *pResource, const char *pEvent);
void SysprogsProfiler_ReportGenericEventEx(void *pResource, void *argument, RealTimeEventArgType argType, int argSize);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
namespace SysprogsStackVerifier
{
	extern void *StackLimit;
}
#endif
