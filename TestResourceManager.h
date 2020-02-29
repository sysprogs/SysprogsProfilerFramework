#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <memory.h>

#ifdef __cplusplus
extern "C" {
#endif

//File API

enum SemihostingFileMode
{
	sfmCreateOrTruncateWriteOnly,
	sfmCreateOrAppendWriteOnly,
	sfmOpenReadOnly,
	sfmCreateOrOpenReadWrite,
	sfmCreateOrTruncateReadWrite,
};

enum TRMErrorCode
{
	trmSuccess = 0,
	trmUnknownError = -1,
	trmInvalidArgument = -2,
};

typedef struct tagTRMFileHandle *TRMFileHandle;
typedef struct tagTRMReadBurstHandle *TRMReadBurstHandle;
typedef struct tagTRMWriteBurstHandle *TRMWriteBurstHandle;

//! Returns system time on host in Windows FILETIME format
uint64_t TRMGetHostSystemTime();

TRMFileHandle TRMCreateFile(const char *pFileName, SemihostingFileMode mode); //Returns 0 on failure.
void TRMCloseFile(TRMFileHandle hFile);

size_t TRMReadFile(TRMFileHandle hFile, void *pBuffer, size_t size);
size_t TRMWriteFile(TRMFileHandle hFile, void *pBuffer, size_t size);

uint64_t TRMGetFileSize(TRMFileHandle hFile);
uint64_t TRMSeekFile(TRMFileHandle hFile, int mode /* SEEK_SET/SEEK_CUR/SEEK_END*/, int64_t distance);
void TRMTruncateFile(TRMFileHandle hFile);

TRMErrorCode TRMDeleteFile(const char *pFileName);
TRMErrorCode TRMCreateDirectory(const char *pDirName);
TRMErrorCode TRMDeleteDirectory(const char *pDirName, int recursively);

TRMReadBurstHandle TRMBeginCachedRead(TRMFileHandle hFile, void *pWorkArea, size_t workAreaSize);
TRMErrorCode TRMEndCachedRead(TRMReadBurstHandle hBurst);
void *TRMBeginReadFileCached(TRMReadBurstHandle hBurst, size_t *pSize, int waitForData);
TRMErrorCode TRMEndReadFileCached(TRMReadBurstHandle hBurst, void *pBuffer, size_t size);

size_t TRMReadFileCached(TRMReadBurstHandle hBurst, void *pBuffer, size_t size, int allowPartialReads);

TRMWriteBurstHandle TRMBeginCachedWrite(TRMFileHandle hFile);
TRMErrorCode TRMEndCachedWrite(TRMWriteBurstHandle hBurst);

size_t TRMWriteFileCached(TRMWriteBurstHandle hBurst, const void *pData, size_t size);

#ifdef __cplusplus
}
#endif