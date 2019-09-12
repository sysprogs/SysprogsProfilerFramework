#pragma once

/*
	The Sysprogs Profiler framework consists of 3 components:
		* Profiler core
		* Platform driver
		* Interface driver
		
	The profiler core collects the data from the stack and compresses it using the 
	common format.
	
	The platform driver configures one of the hardware timers to trigger periodic interrupts
	and calls the SysprogsProfiler_ProcessSample() function from the ISR.
	
	The interface driver sends arbitrary packets of data to the profiler client running on
	your development machine. The profiler framework includes a universal driver that uses the
	fast semihosting mechanism to transfer the data at approximately 40-50KB/s.
*/

#ifdef __cplusplus
extern "C" {
#endif

//Should be called from your main() function after initializing the system clock.
//If this function is not implemented, you are missing a profiler driver for your platform.
//See the STM32 driver for an implementation example.
void InitializeSamplingProfiler();

//Should be called from the timer ISR implemented by the platform driver.
//The arguments specify the values of the key ARM registers at the time the main
//code was interrupted by the ISR. The profiler will automatically reconstruct the
//stack from those and count it in the final report.
void SysprogsProfiler_ProcessSample(void *PC, void *SP, void *FP, void *LR);

void InitializeInstrumentingProfiler();
void ReportTicksPerSecond(unsigned ticksPerSecond);

#ifdef __cplusplus
}
#endif