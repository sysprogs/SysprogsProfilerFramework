#include "hardware/pio.h"
#include "pico/stdio/driver.h"

#include "FastSemihosting.h"
#include "TestResourceManager.h"

#if FAST_SEMIHOSTING_STDIO_DRIVER

static void stdio_fast_semihosting_out_chars(const char *buf, int length)
{
	WriteToFastSemihostingChannel(0, (const void *)buf, length, FAST_SEMIHOSTING_BLOCKING_MODE);
}

static int stdio_fast_semihosting_in_chars(char *buf, int length)
{
	return TRMReadStdin((void *)buf, length, 1);
}

stdio_driver_t stdio_fast_semihosting = {
	.out_chars = stdio_fast_semihosting_out_chars,
	.in_chars = stdio_fast_semihosting_in_chars,
};

void __attribute__((constructor)) InitializeFastSemihostingStdio()
{
	stdio_set_driver_enabled(&stdio_fast_semihosting, true);
}

#endif