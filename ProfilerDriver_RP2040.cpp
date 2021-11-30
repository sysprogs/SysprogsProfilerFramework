#include "hardware/pio.h"
#include "hardware/structs/iobank0.h"
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
	.out_flush = NULL,
	.in_chars = stdio_fast_semihosting_in_chars,
};

void __attribute__((constructor)) InitializeFastSemihostingStdio()
{
	stdio_set_driver_enabled(&stdio_fast_semihosting, true);
}

#endif

extern "C" {
void SysprogsDebugger_SavePIOOutputs(PIO pio);
void SysprogsDebugger_RestorePIOOutputs(PIO pio);
int SysprogsDebugger_StepPIOs(PIO pio, int mask, int stepCount, int overrideMask);
int SysprogsDebugger_SavePIOStateToBuffer(PIO pio, int mask, int flags, uint32_t inputValues);
}

enum
{
	pioResetStateBuffer = 1,
	pioHasPinOverrides = 2,
};

#define NUM_PIO_INSTRUCTION_SLOTS 32
volatile const uint16_t *SysprogsDebugger_PIOInstructionSources[NUM_PIO_INSTRUCTION_SLOTS * 2];
extern "C" void SysprogsDebugger_PIOProgramLoaded(PIO pio, const pio_program_t *program, uint offset)
{
	for (int i = 0; i < program->length; i++)
	{
		int insnOffset = offset + i;
		if (insnOffset >= 0 && insnOffset < NUM_PIO_INSTRUCTION_SLOTS)
		{
			SysprogsDebugger_PIOInstructionSources[insnOffset] = program->instructions + i;
		}
	}
}

#define GPIO_CHANNELS_TO_SAVE 30

struct DisconnectedPIOPins
{
	unsigned Mask;
	unsigned SavedCTRL[GPIO_CHANNELS_TO_SAVE];
};

struct StateMachineSnapshot
{
	unsigned PC, X, Y, ISR, OSR;
};

struct PIOStateSnapshot
{
	unsigned PADOE, PADOUT, PINS, Interrupts, FLEVEL, FSTAT;
	unsigned StateMachineMask;
};

DisconnectedPIOPins g_DisconnectedPIOPins[2];

char g_PIOStateBuffer[1024];
uint32_t g_PIOInputOverrides[128];
unsigned g_PIOStateBufferOffset;

void SysprogsDebugger_SavePIOOutputs(PIO pio)
{
	int index = pio == pio1;
	gpio_function pioFunc = index ? GPIO_FUNC_PIO1 : GPIO_FUNC_PIO0;
	unsigned mask = 0;

	for (int i = 0; i < GPIO_CHANNELS_TO_SAVE; i++)
	{
		unsigned ctrl = iobank0_hw->io[i].ctrl;
		unsigned status = iobank0_hw->io[i].status;

		if (((ctrl & IO_BANK0_GPIO0_CTRL_FUNCSEL_BITS) >> IO_BANK0_GPIO0_CTRL_FUNCSEL_LSB) == pioFunc)
		{
			g_DisconnectedPIOPins[index].SavedCTRL[i] = ctrl;
			ctrl &= ~(IO_BANK0_GPIO0_CTRL_OEOVER_BITS | IO_BANK0_GPIO0_CTRL_OUTOVER_BITS);

			if (status & IO_BANK0_GPIO0_STATUS_OUTTOPAD_BITS)
				ctrl |= 3 << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB;
			else
				ctrl |= 2 << IO_BANK0_GPIO0_CTRL_OUTOVER_LSB;

			if (status & IO_BANK0_GPIO0_STATUS_OETOPAD_BITS)
				ctrl |= 3 << IO_BANK0_GPIO0_CTRL_OEOVER_LSB;
			else
				ctrl |= 2 << IO_BANK0_GPIO0_CTRL_OEOVER_LSB;

			iobank0_hw->io[i].ctrl = ctrl;
			mask |= 1 << i;
		}
	}

	g_DisconnectedPIOPins[index].Mask = mask;
}

void SysprogsDebugger_RestorePIOOutputs(PIO pio)
{
	int index = pio == pio1;
	unsigned mask = g_DisconnectedPIOPins[index].Mask;

	for (int i = 0; i < GPIO_CHANNELS_TO_SAVE; i++)
	{
		if (mask & (1 << i))
			iobank0_hw->io[i].ctrl = g_DisconnectedPIOPins[index].SavedCTRL[i];
	}

	g_DisconnectedPIOPins[index].Mask = 0;
}

static __attribute__((always_inline)) inline uint32_t __get_FAULTMASK(void)
{
	uint32_t result;
	asm volatile("MRS %0, faultmask"
				 : "=r"(result));
	return (result);
}

static void SysprogsDebugger_DoStepPIOs(PIO pio, int mask)
{
	int faultmask = __get_FAULTMASK();
	if (!faultmask)
		asm("cpsid f");

	int savedDividers[NUM_PIO_STATE_MACHINES];
	for (int i = 0; i < (sizeof(savedDividers) / sizeof(savedDividers[0])); i++)
	{
		savedDividers[i] = pio->sm[i].clkdiv;
		pio->sm[i].clkdiv = 65535 << PIO_SM0_CLKDIV_INT_LSB;
	}

	pio_enable_sm_mask_in_sync(pio, mask);
	for (volatile int i = 0; i < 500; i++)
	{
		asm("nop");
	}

	pio_set_sm_mask_enabled(pio, mask, false);

	for (int i = 0; i < (sizeof(savedDividers) / sizeof(savedDividers[0])); i++)
		pio->sm[i].clkdiv = savedDividers[i];

	if (!faultmask)
		asm("cpsie f");
}

static int GetPIODumpSize(int mask)
{
	int smCount = 0;
	for (int i = 0; i < NUM_PIO_STATE_MACHINES; i++)
		if (mask & (1 << i))
			smCount++;

	return sizeof(PIOStateSnapshot) + sizeof(StateMachineSnapshot) * smCount;
}

int SysprogsDebugger_StepPIOs(PIO pio, int mask, int stepCount, int overrideMask)
{
	int maxSteps = sizeof(g_PIOStateBuffer) / GetPIODumpSize(mask);
	if (stepCount > maxSteps)
		stepCount = maxSteps;

	g_PIOStateBufferOffset = 0;

	uint32_t originalInputValues = sio_hw->gpio_in;

	for (int i = 0; i < stepCount; i++)
	{
		uint32_t effectiveInputValues = originalInputValues;
		if (overrideMask)
		{
			effectiveInputValues &= ~overrideMask;
			effectiveInputValues |= (overrideMask & g_PIOInputOverrides[i]);
		}

		//If some input pins change value between a PIO step and our sampling of their values, the user would see inconsistent results.
		//To avoid it, we capture the pin values before doing the step, and freeze them via the GPIO override registers until the step is complete.
		for (int pin = 0; pin < 32; pin++)
		{
			bool isHigh = effectiveInputValues & (1 << pin);

			unsigned ctrl = iobank0_hw->io[pin].ctrl & ~IO_BANK0_GPIO0_CTRL_INOVER_BITS;

			if (isHigh)
				ctrl |= 3 << IO_BANK0_GPIO0_CTRL_INOVER_LSB;
			else
				ctrl |= 2 << IO_BANK0_GPIO0_CTRL_INOVER_LSB;

			iobank0_hw->io[pin].ctrl = ctrl;
		}

		SysprogsDebugger_DoStepPIOs(pio, mask);
		SysprogsDebugger_SavePIOStateToBuffer(pio, mask, pioHasPinOverrides, effectiveInputValues);

		for (int pin = 0; pin < 32; pin++)
			iobank0_hw->io[pin].ctrl &= ~IO_BANK0_GPIO0_CTRL_INOVER_BITS;
	}

	return g_PIOStateBufferOffset;
}

static uint ReverseBits(uint value)
{
	uint result = 0;
	for (int i = 0, j = 31; i < 32; i++, j--)
		result |= ((value >> i) & 1) << j;

	return result;
}

static unsigned SysprogsDebugger_ReadPIORegisterDestroyingPinState(PIO pio, unsigned sm, pio_src_dest reg)
{
	pio_sm_exec_wait_blocking(pio, sm, pio_encode_mov(pio_pins, reg));
	unsigned normalValues = pio->dbg_padout;
	pio_sm_exec_wait_blocking(pio, sm, pio_encode_mov_reverse(pio_pins, reg));
	unsigned reversedValues = pio->dbg_padout;

	uint reconstructedFromReversing = ReverseBits(reversedValues);

	uint intersectionMask = 0x3FFFFFFC; //These bits should be accessible by both normal and reverse read operations
	if (((normalValues ^ reconstructedFromReversing) & intersectionMask) == 0)
		return normalValues | reconstructedFromReversing; //Normal value has 2 MSBs at 0, reversed one has 2 LSBs at 0. ORing both values gets the actual register value.

	return 0;
}

static unsigned SysprogsDebugger_ReadPIORegister(PIO pio, unsigned sm, pio_src_dest reg)
{
	uint32_t outputValues = pio->dbg_padout;
	SysprogsDebugger_SavePIOOutputs(pio);
	unsigned result = SysprogsDebugger_ReadPIORegisterDestroyingPinState(pio, sm, reg);
	pio_sm_set_pins(pio, sm, outputValues);
	SysprogsDebugger_RestorePIOOutputs(pio);
	return result;
}

int SysprogsDebugger_SavePIOStateToBuffer(PIO pio, int mask, int flags, uint32_t inputValues)
{
	int requiredSize = GetPIODumpSize(mask);
	if (g_PIOStateBufferOffset > (sizeof(g_PIOStateBuffer) - requiredSize))
		return 0;

	if (flags & pioResetStateBuffer)
		g_PIOStateBufferOffset = 0;

	PIOStateSnapshot *pState = (PIOStateSnapshot *)&g_PIOStateBuffer[g_PIOStateBufferOffset];
	StateMachineSnapshot *pSMState = (StateMachineSnapshot *)&g_PIOStateBuffer[g_PIOStateBufferOffset + sizeof(PIOStateSnapshot)];

	pState->PADOUT = pio->dbg_padout;
	pState->PADOE = pio->dbg_padoe;
	pState->Interrupts = (pio->irq << 24) | pio->intr;
	pState->FLEVEL = pio->flevel;
	pState->FSTAT = pio->fstat;
	if (flags & pioHasPinOverrides)
		pState->PINS = inputValues;
	else
		pState->PINS = sio_hw->gpio_in;

	pState->StateMachineMask = mask;

	uint32_t outputValues = pio->dbg_padout;
	SysprogsDebugger_SavePIOOutputs(pio);

	for (int i = 0; i < NUM_PIO_STATE_MACHINES; i++)
	{
		if (!(mask & (1 << i)))
			continue;

		uint32_t pinctrl = pio->sm[i].pinctrl;
		pio->sm[i].pinctrl = (pio->sm[i].pinctrl & ~(PIO_SM0_PINCTRL_OUT_BASE_BITS | PIO_SM0_PINCTRL_OUT_COUNT_BITS)) |
							 (0 << PIO_SM0_PINCTRL_OUT_BASE_LSB) |
							 (32 << PIO_SM0_PINCTRL_OUT_COUNT_LSB);

		pSMState->PC = pio->sm[i].addr;
		pSMState->X = SysprogsDebugger_ReadPIORegisterDestroyingPinState(pio, i, pio_x);
		pSMState->Y = SysprogsDebugger_ReadPIORegisterDestroyingPinState(pio, i, pio_y);
		pSMState->ISR = SysprogsDebugger_ReadPIORegisterDestroyingPinState(pio, i, pio_isr);
		pSMState->OSR = SysprogsDebugger_ReadPIORegisterDestroyingPinState(pio, i, pio_osr);
		pSMState++;

		pio->sm[i].pinctrl = pinctrl;
	}

	g_PIOStateBufferOffset += requiredSize;
	pio_sm_set_pins(pio, 0, outputValues);
	SysprogsDebugger_RestorePIOOutputs(pio);

	return g_PIOStateBufferOffset;
}

void __attribute__((constructor)) InitializePIODebugger()
{
	volatile int unused = 0;
	if (unused)
	{
		SysprogsDebugger_StepPIOs(0, 0, 0, 0);
		SysprogsDebugger_ReadPIORegister(0, 0, pio_pins);
	}
}
