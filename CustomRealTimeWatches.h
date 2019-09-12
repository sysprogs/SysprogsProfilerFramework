#pragma once
#include "SysprogsProfilerInterface.h"

struct RunTimeWatch;
struct ScalarRealTimeWatch;
struct EventStreamWatch;

#ifdef __cplusplus
extern "C" {
#endif

static void RunTimeWatch_ReportStart(struct RunTimeWatch *pWatch);
static void RunTimeWatch_ReportEnd(struct RunTimeWatch *pWatch);

static void ScalarRealTimeWatch_ReportSignedValue(struct ScalarRealTimeWatch *pWatch, int value);
static void ScalarRealTimeWatch_ReportUnsignedValue(struct ScalarRealTimeWatch *pWatch, unsigned value);
static void ScalarRealTimeWatch_ReportFPValue(struct ScalarRealTimeWatch *pWatch, double value);

static void EventStreamWatch_ReportEvent(struct EventStreamWatch *pWatch, const char *pEvent);
static void EventStreamWatch_ReportEvent_SignedInt(struct EventStreamWatch *pWatch, int arbitraryEventArgument);
static void EventStreamWatch_ReportEvent_UnsignedInt(struct EventStreamWatch *pWatch, unsigned arbitraryEventArgument);
static void EventStreamWatch_ReportEvent_FP(struct EventStreamWatch *pWatch, double arbitraryEventArgument);

void InitializeCustomRealTimeWatch();

#ifdef __cplusplus
}
#endif

struct RunTimeWatch
{
	volatile int Enabled; //Set automatically by the debugger

#ifdef __cplusplus
	void ReportStart()
	{
		RunTimeWatch_ReportStart(this);
	}

	void ReportEnd()
	{
		RunTimeWatch_ReportEnd(this);
	}

#endif
};

struct ScalarRealTimeWatch
{
	volatile int Enabled; //Set automatically by the debugger
#ifdef __cplusplus
	void ReportValue(int value)
	{
		ScalarRealTimeWatch_ReportSignedValue(this, value);
	}

	void ReportValue(unsigned value)
	{
		ScalarRealTimeWatch_ReportUnsignedValue(this, value);
	}

	void ReportValue(double value)
	{
		ScalarRealTimeWatch_ReportFPValue(this, value);
	}
#endif
};

struct EventStreamWatch
{
	volatile int Enabled; //Set automatically by the debugger
#ifdef __cplusplus
	void ReportEvent(const char *pEvent)
	{
		EventStreamWatch_ReportEvent(this, pEvent);
	}

	void ReportEvent(int arbitraryEventArgument)
	{
		EventStreamWatch_ReportEvent_SignedInt(this, arbitraryEventArgument);
	}

	void ReportEvent(unsigned arbitraryEventArgument)
	{
		EventStreamWatch_ReportEvent_UnsignedInt(this, arbitraryEventArgument);
	}

	void ReportEvent(double arbitraryEventArgument)
	{
		EventStreamWatch_ReportEvent_FP(this, arbitraryEventArgument);
	}

#endif
};

#ifdef __cplusplus
class ScopedRunTimeReporter
{
private:
	RunTimeWatch *m_pWatch;

public:
	ScopedRunTimeReporter(RunTimeWatch &watch)
		: m_pWatch(&watch)
	{
		m_pWatch->ReportStart();
	}

	~ScopedRunTimeReporter()
	{
		m_pWatch->ReportEnd();
	}
};
#endif

static void RunTimeWatch_ReportStart(struct RunTimeWatch *pWatch)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportResourceTaken(pWatch, 0, 0);
}

static void RunTimeWatch_ReportEnd(struct RunTimeWatch *pWatch)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportResourceReleased(pWatch, 0, 0);
}

static void ScalarRealTimeWatch_ReportSignedValue(struct ScalarRealTimeWatch *pWatch, int value)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportIntegralValue(pWatch, (unsigned)value, 1);
}

static void ScalarRealTimeWatch_ReportUnsignedValue(struct ScalarRealTimeWatch *pWatch, unsigned value)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportIntegralValue(pWatch, value, 0);
}

static void ScalarRealTimeWatch_ReportFPValue(struct ScalarRealTimeWatch *pWatch, double value)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportFPValue(pWatch, value);
}

static void EventStreamWatch_ReportEvent(struct EventStreamWatch *pWatch, const char *pEvent)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportGenericEvent(pWatch, pEvent);
}

void EventStreamWatch_ReportEvent_SignedInt(struct EventStreamWatch *pWatch, int arbitraryEventArgument)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportGenericEventEx(pWatch, &arbitraryEventArgument, rtaSignedInt, sizeof(arbitraryEventArgument));
}

void EventStreamWatch_ReportEvent_UnsignedInt(struct EventStreamWatch *pWatch, unsigned arbitraryEventArgument)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportGenericEventEx(pWatch, &arbitraryEventArgument, rtaUnsignedInt, sizeof(arbitraryEventArgument));
}

void EventStreamWatch_ReportEvent_FP(struct EventStreamWatch *pWatch, double arbitraryEventArgument)
{
	if (!pWatch->Enabled)
		return;
	SysprogsProfiler_ReportGenericEventEx(pWatch, &arbitraryEventArgument, rtaFloatingPoint, sizeof(arbitraryEventArgument));
}
