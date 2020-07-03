#pragma once

#ifndef SYSPROGS_PROFILER_DEBUGGER_CHECK_MODE

static bool CanInvokeSemihostingCalls()
{
	return true;
}

#else
#if SYSPROGS_PROFILER_DEBUGGER_CHECK_MODE

static bool CanInvokeSemihostingCalls()
{
	//Check the C_DEBUGEN bit in DHCSR
	return *((unsigned *)0xE000EDF0) & 1;
}

#else

//Must be user-provided
bool CanInvokeSemihostingCalls();
#endif

#endif
