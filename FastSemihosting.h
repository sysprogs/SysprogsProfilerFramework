#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
	
void SuspendFastSemihostingPolling();
void ResumeFastSemihostingPolling();

void InitializeFastSemihosting();	//Should not be called explicitly, unless we want VisualGDB to set global variables like ''

#ifdef __cplusplus
}
#endif