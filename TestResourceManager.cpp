#include "TestResourceManager.h"
#include "FastSemihosting.h"
#include "ProfilerCompatibility.h"
#include "SysprogsProfilerInterface.h"
#include <string.h>

static volatile bool s_BlockingFastSemihostingInitialized;

enum NonBlockingResourceRequest
{
	nbrrInitialize,			   //Payload: &s_BlockingFastSemihostingInitialized, &s_FastSemihostingStateExtension
	nbrrProcessWriteBurstData, //Payload: <burst handle> <data size> <data>
};

enum BlockingResourceRequest
{
	brrInvalid,
	brrGetSystemTime,
	brrCreateFile,
	brrCloseFile,
	brrReadFile,
	brrWriteFile,
	brrGetFileSize,
	brrSeekFile,
	brrDeleteFile,
	brrTruncateFile,
	brrCreateDirectory,
	brrDeleteDirectory,

	brrBeginCachedRead,
	brrEndCachedRead,
	brrBeginCachedWrite,
	brrEndCachedWrite,
	
	brrReadStdin,
};

static struct
{
	volatile unsigned Command;
	volatile unsigned Arguments[3];
} s_FastSemihostingStateExtension;

void RequestBlockingProcessingViaFastSemihosting();

static void RunBlockingFastSemihostingCall(BlockingResourceRequest request, unsigned arg1 = 0, unsigned arg2 = 0, unsigned arg3 = 0)
{
	if (!s_BlockingFastSemihostingInitialized)
	{
		unsigned request[3] = {nbrrInitialize, (unsigned)&s_BlockingFastSemihostingInitialized, (unsigned)&s_FastSemihostingStateExtension};

		SysprogsProfiler_WriteData(pdcResourceManagementStream, request, sizeof(request), 0, 0);

		while (!s_BlockingFastSemihostingInitialized)
		{
			//The host will acknowledge the request by setting 's_BlockingFastSemihostingInitialized' to 1. If not, ensure you update to VisualGDB 5.5 or later.
		}
	}

	s_FastSemihostingStateExtension.Command = request;
	s_FastSemihostingStateExtension.Arguments[0] = arg1;
	s_FastSemihostingStateExtension.Arguments[1] = arg2;
	s_FastSemihostingStateExtension.Arguments[2] = arg3;
	RequestBlockingProcessingViaFastSemihosting();
}

uint64_t TRMGetHostSystemTime()
{
	RunBlockingFastSemihostingCall(brrGetSystemTime);
	return ((uint64_t)s_FastSemihostingStateExtension.Arguments[1] << 32) | s_FastSemihostingStateExtension.Arguments[0];
}

TRMFileHandle TRMCreateFile(const char *pFileName, SemihostingFileMode mode)
{
	RunBlockingFastSemihostingCall(brrCreateFile, (unsigned)pFileName, strlen(pFileName), mode);
	return (TRMFileHandle)s_FastSemihostingStateExtension.Arguments[0];
}

void TRMCloseFile(TRMFileHandle hFile)
{
	RunBlockingFastSemihostingCall(brrCloseFile, (unsigned)hFile);
}

ssize_t TRMReadFile(TRMFileHandle hFile, void *pBuffer, size_t size)
{
	RunBlockingFastSemihostingCall(brrReadFile, (unsigned)hFile, (unsigned)pBuffer, size);
	return s_FastSemihostingStateExtension.Arguments[0];
}

ssize_t TRMWriteFile(TRMFileHandle hFile, const void *pBuffer, size_t size)
{
	RunBlockingFastSemihostingCall(brrWriteFile, (unsigned)hFile, (unsigned)pBuffer, size);
	return s_FastSemihostingStateExtension.Arguments[0];
}

ssize_t TRMReadStdin(void *pBuffer, size_t size, int blocking)
{
	RunBlockingFastSemihostingCall(brrReadStdin, blocking, (unsigned)pBuffer, size);
	return s_FastSemihostingStateExtension.Arguments[0];
}

uint64_t TRMGetFileSize(TRMFileHandle hFile)
{
	RunBlockingFastSemihostingCall(brrGetFileSize, (unsigned)hFile);
	return ((uint64_t)s_FastSemihostingStateExtension.Arguments[1] << 32) | s_FastSemihostingStateExtension.Arguments[0];
}

uint64_t TRMSeekFile(TRMFileHandle hFile, int mode /* SEEK_SET/SEEK_CUR/SEEK_END*/, int64_t distance)
{
	RunBlockingFastSemihostingCall(brrSeekFile, (unsigned)hFile, mode, (unsigned)&distance);
	return ((uint64_t)s_FastSemihostingStateExtension.Arguments[1] << 32) | s_FastSemihostingStateExtension.Arguments[0];
}

void TRMTruncateFile(TRMFileHandle hFile)
{
	RunBlockingFastSemihostingCall(brrTruncateFile, (unsigned)hFile);
}

TRMErrorCode TRMDeleteFile(const char *pFileName)
{
	RunBlockingFastSemihostingCall(brrDeleteFile, (unsigned)pFileName, strlen(pFileName));
	return (TRMErrorCode)s_FastSemihostingStateExtension.Arguments[0];
}

TRMErrorCode TRMCreateDirectory(const char *pDirName)
{
	RunBlockingFastSemihostingCall(brrCreateDirectory, (unsigned)pDirName, strlen(pDirName));
	return (TRMErrorCode)s_FastSemihostingStateExtension.Arguments[0];
}

TRMErrorCode TRMDeleteDirectory(const char *pDirName, int recursively)
{
	RunBlockingFastSemihostingCall(brrDeleteDirectory, (unsigned)pDirName, strlen(pDirName), recursively);
	return (TRMErrorCode)s_FastSemihostingStateExtension.Arguments[0];
}

struct ReadBurstWorkArea
{
	volatile uint32_t ReadOffsetWithFlags;
	volatile uint32_t WriteOffsetWithFlags;
	volatile uint32_t BufferSize;
};

TRMReadBurstHandle TRMBeginReadBurst(TRMFileHandle hFile, void *pWorkArea, size_t workAreaSize)
{
	int frontPadding = (unsigned)pWorkArea % sizeof(void *);
	if (frontPadding)
		frontPadding = sizeof(void *) - frontPadding;

	if (workAreaSize <= (size_t)frontPadding + sizeof(ReadBurstWorkArea) + 4)
		return 0;

	ReadBurstWorkArea *pWorkAreaStructure = (ReadBurstWorkArea *)((char *)pWorkArea + frontPadding);
	pWorkAreaStructure->ReadOffsetWithFlags = 0;
	pWorkAreaStructure->WriteOffsetWithFlags = 0;
	pWorkAreaStructure->BufferSize = workAreaSize - frontPadding - sizeof(ReadBurstWorkArea);

	RunBlockingFastSemihostingCall(brrBeginCachedRead, (unsigned)hFile, (unsigned)pWorkAreaStructure);
	if (s_FastSemihostingStateExtension.Arguments[0])
		return 0; //Unknown error on the host side.
	else
		return (TRMReadBurstHandle)pWorkAreaStructure;
}

TRMErrorCode TRMEndReadBurst(TRMReadBurstHandle hBurst)
{
	RunBlockingFastSemihostingCall(brrEndCachedRead, (unsigned)hBurst);
	return (TRMErrorCode)s_FastSemihostingStateExtension.Arguments[0];
}

enum
{
	kReadBurstGenerationFlag = 0x80000000,
	kReadBurstEOFFlag = 0x40000000,
	kReadBurstOffsetMask = 0x3FFFFFFF,
};

void *TRMBeginReadFileCached(TRMReadBurstHandle hBurst, size_t *pSize, int waitForData)
{
	ReadBurstWorkArea *pWorkAreaStructure = (ReadBurstWorkArea *)hBurst;

	unsigned readOffsetWithGenerationBit = pWorkAreaStructure->ReadOffsetWithFlags & ~kReadBurstEOFFlag;

	for (;;)
	{
		unsigned writeOffsetWithFlags = pWorkAreaStructure->WriteOffsetWithFlags;
		if ((writeOffsetWithFlags & ~kReadBurstEOFFlag) != readOffsetWithGenerationBit)
			break; //We have more data to read.

		if (writeOffsetWithFlags & kReadBurstEOFFlag)
		{
			*pSize = 0;
			return 0; //No more data available on host.
		}

		if (!waitForData)
		{
			*pSize = 0;
			return 0; //Non-blocking mode requested.
		}
	}

	unsigned readOffset = readOffsetWithGenerationBit & kReadBurstOffsetMask;
	unsigned writeOffset = pWorkAreaStructure->WriteOffsetWithFlags & kReadBurstOffsetMask;

	size_t availableSize;
	if (writeOffset > readOffset)
		availableSize = writeOffset - readOffset;
	else
		availableSize = pWorkAreaStructure->BufferSize - readOffset;

	*pSize = availableSize;
	return (char *)(pWorkAreaStructure + 1) + readOffset;
}

TRMErrorCode TRMEndReadFileCached(TRMReadBurstHandle hBurst, void *pBuffer, size_t size)
{
	ReadBurstWorkArea *pWorkAreaStructure = (ReadBurstWorkArea *)hBurst;

	unsigned readOffsetWithGenerationBit = pWorkAreaStructure->ReadOffsetWithFlags;
	unsigned readOffset = readOffsetWithGenerationBit & kReadBurstOffsetMask;
	void *pExpectedBuffer = (char *)(pWorkAreaStructure + 1) + readOffset;
	//unsigned writeOffset = pWorkAreaStructure->WriteOffsetWithFlags & kReadBurstOffsetMask;

	if (pBuffer != pExpectedBuffer)
		return trmInvalidArgument; //We are trying to release a wrong buffer. Do not update anything.

	readOffset += size;

	size_t bufferSize = pWorkAreaStructure->BufferSize;
	if (readOffset >= bufferSize)
	{
		readOffset -= bufferSize;
		readOffsetWithGenerationBit ^= kReadBurstGenerationFlag;
	}

	if (readOffset > bufferSize)
		return trmInvalidArgument; //Trying to release too much data.

	pWorkAreaStructure->ReadOffsetWithFlags = (readOffsetWithGenerationBit & kReadBurstGenerationFlag) | (readOffset & ~kReadBurstGenerationFlag);
	return trmSuccess;
}

ssize_t TRMReadFileCached(TRMReadBurstHandle hBurst, void *pBuffer, size_t size, int allowPartialReads)
{
	char *pAdjustedBuffer = (char *)pBuffer;
	size_t done = 0;

	while (done < size)
	{
		size_t maxTodo = size - done, todo = 0;
		void *pData = TRMBeginReadFileCached(hBurst, &todo, 1);
		if (!pData || !todo)
			break;

		if (todo > maxTodo)
			todo = maxTodo;

		memcpy(pAdjustedBuffer, pData, todo);
		TRMErrorCode err = TRMEndReadFileCached(hBurst, pData, todo);
		if (err != trmSuccess)
			break;

		pAdjustedBuffer += todo;
		done += todo;

		if (allowPartialReads)
			break;
	}

	return done;
}

TRMWriteBurstHandle TRMBeginWriteBurst(TRMFileHandle hFile)
{
	RunBlockingFastSemihostingCall(brrBeginCachedWrite, (unsigned)hFile);
	return (TRMWriteBurstHandle)s_FastSemihostingStateExtension.Arguments[0];
}

TRMErrorCode TRMEndWriteBurst(TRMWriteBurstHandle hBurst)
{
	RunBlockingFastSemihostingCall(brrEndCachedWrite, (unsigned)hBurst);
	return (TRMErrorCode)s_FastSemihostingStateExtension.Arguments[0];
}

ssize_t TRMWriteFileCached(TRMWriteBurstHandle hBurst, const void *pData, size_t size)
{
	unsigned request[3] = {nbrrProcessWriteBurstData, (unsigned)hBurst, size};
	if (WriteToFastSemihostingChannel(pdcResourceManagementStream, &request, sizeof(request), 1) != sizeof(request))
		return 0;

	return WriteToFastSemihostingChannel(pdcResourceManagementStream, pData, size, 1);
}

#if FAST_SEMIHOSTING_STDIO_DRIVER && !defined(PROFILER_RP2040)

#ifdef __IAR_SYSTEMS_ICC__
extern "C" size_t __read(int fd, const unsigned char *pBuffer, size_t size)
#else
extern "C" int _read(int fd, char *pBuffer, int size)
#endif
{
	(void)fd;
	return TRMReadStdin((void *)pBuffer, size, 1);
}
#endif