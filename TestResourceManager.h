#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#if defined(__ARMCC_VERSION) || defined(__IAR_SYSTEMS_ICC__)
typedef int ssize_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*!	\page TRMAPI Test Resource Manager API
	
	\section Overview
	The Test Resource Manager API allows reading and writing files on the Windows machine (i.e. host) from the code running on the target.
	It is used in unit test applications to access data sets that are too large to fit into the target's memory.
	
	\section TRM_UseCases Typical Uses
	The most typical use of the Test Resource Manager API would involve opening a file on the host, reading the test data from it, and closing it.
	See the code below for a quick example:
	
	\code
		TRMFileHandle hFile = TRMCreateFile("TestData.dat", sfmOpenReadOnly);
		char buffer[512];
		
		if (!hFile)
			FAIL("TestData.dat doesn't exist");
		
		//Read and process data from <Project Dir>\TestResources\TestData.dat
		for (;;)
		{
			size_t done = TRMReadFile(hFile, buffer, sizeof(buffer));
			//TODO: process the data retrieved from the file
			if (done <= 0)
				break;
		}
		
		TRMCloseFile(hFile);
	\endcode
	For performance-critical applications, we recommend using the \ref TRM_ReadBursts and \ref TRM_WriteBursts.
	
	\section TRM_ReadBursts Read Bursts
	Read bursts are the fastest way to sequentially read a file on the host.
	Once a read burst is active, VisualGDB will continuously read data from the file and will keep writing it into the intermediate buffer, as long as it has space.
	This will continue even while the target is running (e.g. processing the previously read data). Use the \ref TRMBeginReadBurst, \ref TRMEndReadBurst and \ref TRMReadFileCached
	(or a more advanced combination of \ref TRMBeginReadFileCached and \ref TRMEndReadFileCached) to read the test resources using the burst API.
	
	Below is a quick example of the read burst API:
	
	\code
		TRMFileHandle hFile = TRMCreateFile("TestData.dat", sfmOpenReadOnly);
		char buffer[512];
		char temporaryBuffer[4096];

		if (!hFile)
			FAIL("TestData.dat doesn't exist");

		TRMReadBurstHandle hBurst = TRMBeginReadBurst(hFile, temporaryBuffer, sizeof(temporaryBuffer));
		
		for (;;)
		{
			size_t done = TRMReadFileCached(hBurst, buffer, sizeof(buffer), 0);
			//TODO: process the data retrieved from the file
			if (done <= 0)
				break;
		}
		
		TRMEndReadBurst(hBurst);
		TRMCloseFile(hFile);	
	\endcode
	
	Note that once a burst is active, calls to the regular \ref TRMReadFile or \ref TRMSeekFile will fail until the burst is ended.
	
	\section TRM_WriteBursts Write Bursts
	Write bursts allow quickly writing data to a file on the host, without stopping the target. They use the regular fast semihosting buffer to queue the written data.
	VisualGDB will process the queued data in the background, without interrupting the target, as long as there is sufficient space in the semihosting buffer.
	
	Below is a quick example of the write burst API:
	\code
		TRMFileHandle hFile = TRMCreateFile("TestData.dat", sfmCreateOrTruncateWriteOnly);
		char buffer[512];
		char temporaryBuffer[4096];

		TRMWriteBurstHandle hBurst = TRMBeginWriteBurst(hFile);

		for (;;)
		{
			//TODO: process the data retrieved from the file
			size_t done = TRMWriteFileCached(hBurst, buffer, sizeof(buffer));
			if (done <= 0)
				break;
		}

		TRMEndWriteBurst(hBurst);
		TRMCloseFile(hFile);	
	\endcode
	
	\section TRM_Functions All Functions
	See the \ref TestResourceManager.h file to a list of all Test Resource Manager functions.
	
	\file TestResourceManager.h
	\brief Contains \ref TRMAPI
*/

//! Lists the supported file open modes
enum SemihostingFileMode
{
	//! Create an empty file if it doesn't exist. Otherwise, truncate an existing file. Allow writing to the file, but not reading from it.
	sfmCreateOrTruncateWriteOnly,
	//! Create an empty file if it doesn't exist. Otherwise, open the existing file and immediately seek to the end of it. Allow writing to the file, but not reading from it.
	sfmCreateOrAppendWriteOnly,
	//! Open an existing file in read-only mode. If the file doesn't exist, \ref TRMCreateFile will return 0.
	sfmOpenReadOnly,
	//! Open an existing file and allow reading/writing it. If the file doesn't exist, create it.
	sfmCreateOrOpenReadWrite,
	//! Create an empty file if it doesn't exist. Otherwise, truncate an existing file. Allow reading to the file and reading from it.
	sfmCreateOrTruncateReadWrite,
};

//! Specifies the error code returned by common Test Resource Manager functions.
/*!
	\remarks Use View->Other Windows->VisualGDB Diagnostics Console to see details on why a specific Test Resource Manager call failed.
*/
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
/*!
	\return The function returns the number of 100-nanosecond intervals elapsed since January 1, 1601 (UTC).
*/
uint64_t TRMGetHostSystemTime();

//! Creates or opens a file on the host.
/*!
	This function creates or opens a file in the *TestResources* directory of the project on the Windows machine.
	\param pFileName Specifies the relative path to the file (including possible subdirectories). Absolute paths, or paths starting with ".." will be rejected by the host.
	\param mode Specifies the file open mode. See \ref SemihostingFileMode for a list of supported modes.
	\return The function returns a file handle that can be used in calls to \ref TRMReadFile, \ref TRMWriteFile and other functions. If the function fails, the returned value will be 0. Use \ref TRMCloseFile to close the returned handle.
*/
TRMFileHandle TRMCreateFile(const char *pFileName, SemihostingFileMode mode); //Returns 0 on failure.

//! Closes a handle returned by \ref TRMCreateFile.
void TRMCloseFile(TRMFileHandle hFile);

//! Reads the data from a file on the host machine.
/*!
	\param hFile File handle returned by \ref TRMCreateFile.
	\param pBuffer Buffer that will receive the data.
	\param size The maximum number of bytes that should be read from the file.
	\return Normally, the function returns the amount of bytes that was read from the file. Unless an end of file is reached, or an error occurs,
			it will be equivalent to the <b>size</b> parameter. If an end of file is reached, will return 0. If an error occurs, the function will return a negative value.
	\remarks TRMReadFile will not prefetch any data from the host and may take some time to process due to the latency between the host and the target. For a faster prefetching alternative
			 to TRMReadFile, see \ref TRM_ReadBursts.
*/
ssize_t TRMReadFile(TRMFileHandle hFile, void *pBuffer, size_t size);

//! Reads the user input from the Fast Semihosting window.
/*!
	\param pBuffer Buffer that will receive the data.
	\param size The maximum number of bytes that should be read from the file.
	\param blocking Specifies whether the call should wait for at least 1 byte to be available.
	\return The function returns the number of bytes read from the stdin.
*/
ssize_t TRMReadStdin(void *pBuffer, size_t size, int blocking);

//! Write the data to a file on the host machine.
/*!
	\param hFile File handle returned by \ref TRMCreateFile.
	\param pBuffer Buffer that contains the data.
	\param size The maximum number of bytes that should be written to the file.
	\return Normally, the function returns the amount of bytes that was written to the file. Unless an error occurs,
			it will be equivalent to the <b>size</b> parameter. If an error occurs, the function will return a negative value or a zero.
	\remarks TRMWriteFile will not return until the host receives the written data. This may take some time due to the latency between the host and the target. For a faster non-blocking alternative to TRMWriteFile,
			 see \ref TRM_WriteBursts.
*/
ssize_t TRMWriteFile(TRMFileHandle hFile, const void *pBuffer, size_t size);

//! Returns the size of the file.
/*!	\param hFile File handle returned by \ref TRMCreateFile.
*/
uint64_t TRMGetFileSize(TRMFileHandle hFile);

//! Moves the file pointer
/*!	\param hFile File handle returned by \ref TRMCreateFile.
	\param mode Specifies one of the following values
			- _SEEK_SET_. The <b>distance</b> is an absolute value from the beginning of the file.
			- _SEEK_CUR_. The <b>distance</b> is a relative value (positive of negative), relative to the current position of the file pointer.
			- _SEEK_END_. The <b>distance</b> is a negative value, relative to the end of the file.
	\param distance Specifies the distance to move the file pointer. See the <b>mode</b> parameter for more details.
*/
uint64_t TRMSeekFile(TRMFileHandle hFile, int mode, int64_t distance);

//! Truncates the file.
/*!	This function will set the end-of-file to the current position of the file pointer.
	\param hFile File handle returned by \ref TRMCreateFile.
*/
void TRMTruncateFile(TRMFileHandle hFile);

//! Deletes a file.
/*! This function deletes a file inside the TestResources folder of the current project. See \ref TRMCreateFile for the limitations on file paths.
*/
TRMErrorCode TRMDeleteFile(const char *pFileName);

//! Creates a subdirectory.
/*! This function creates a subdirectory inside the TestResources folder of the current project. See \ref TRMCreateFile for the limitations on file paths.
*/
TRMErrorCode TRMCreateDirectory(const char *pDirName);

//! Deletes a file.
/*! This function deletes a directory inside the TestResources folder of the current project. See \ref TRMCreateFile for the limitations on file paths.
	\param pDirName Relative path to the deleted subdirectory.
	\param recursively Specify a non-zero value to delete the directory recursively. 
	\remarks VisualGDB will automatically restrict all file API (including TRMDeleteDirectory) to the TestResources folder of the debugged project.
*/
TRMErrorCode TRMDeleteDirectory(const char *pDirName, int recursively);

//! Starts a read burst.
/*! This function starts a read burst (i.e. background prefetch) using the supplied temporary buffer. See \ref TRM_ReadBursts for more details.
	Once a burst has been started, use \ref TRMReadFileCached to read the data from the file.
	\param hFile File handle returned by \ref TRMCreateFile.
	\param pWorkArea Temporary buffer that will be used to prefetch the data from the host. The buffer must be at least 16 bytes long (although 4096KB or more is recommended for optimal performance).
	\param workAreaSize Specifies the temporary buffer size, in bytes.
	\return The function returns a read burst handle. If an error occurs, the function returns 0.
	
	\remarks Once the burst has been started, VisualGDB will automatically prefetch the file contents and store it in the buffer without interrupting the target.
			 Use the \ref TRMBeginReadFileCached (or \ref TRMBeginReadFileCached + \ref TRMEndReadFileCached) to access the data. Use \ref TRMEndReadBurst to end an active burst.		 
*/
TRMReadBurstHandle TRMBeginReadBurst(TRMFileHandle hFile, void *pWorkArea, size_t workAreaSize);

//! Ends a read burst.
/*! \param hBurst Burst handle returned by \ref TRMBeginReadBurst */
TRMErrorCode TRMEndReadBurst(TRMReadBurstHandle hBurst);

//! Returns the pointer to a block of data that has been prefetched into the read burst buffer.
/*! This function allows directly accessing the data that has been prefetched by VisualGDB into the burst buffer. Use \ref TRMEndReadFileCached to release the returned data
	and let VisualGDB rewrite it with new data. Call \ref TRMEndReadFileCached to release the returned buffer.
	
	\param hBurst Burst handle returned by \ref TRMBeginReadBurst
	\param pSize Receives the amount of bytes available for processing
	\param waitForData Specifies whether to wait for more data, if it hasn't been fetched yet
	\return The function returns the pointer to the first byte of the data fetched from the file. If the end-of-file has been reached, or if <b>waitForData</b> is 0 and no new data has been
			fetched by the host yet, the function returns NULL.
	
	The typical use of the TRMBeginReadFileCached/\ref TRMEndReadFileCached functions is shown below:
	\code
		for (;;)
		{
			size_t size = 0;
			void *pData = TRMBeginReadFileCached(hBurst, &size, 1);
			if (!pData)
				break;	//End-of-file
			ProcessData(pData, size);
			TRMEndReadFileCached(hBurst, pData, size);
		}
	\endcode
	
	\remarks Is is critical to call \ref TRMEndReadFileCached after each succesful call to TRMBeginReadFileCached. If you prefer a simpler API, use the \ref TRMReadFileCached function instead.
*/
void *TRMBeginReadFileCached(TRMReadBurstHandle hBurst, size_t *pSize, int waitForData);

//! Releases the buffer returned by \ref TRMBeginReadFileCached.
/*! \param hBurst Burst handle returned by \ref TRMBeginReadBurst
	\param pBuffer Buffer returned by \ref TRMBeginReadFileCached
	\param size Specifies the amount of bytes to be released. It must be equal or less to the amount of bytes returned by \ref TRMBeginReadBurst via  the <b>pSize</p> parameter.
	
	\remarks After a buffer has been released by a call to TRMEndReadFileCached, you can immediately call \ref TRMBeginReadFileCached to read the next portion of data from the file.
*/
TRMErrorCode TRMEndReadFileCached(TRMReadBurstHandle hBurst, void *pBuffer, size_t size);

//! Reads data within a read burst.
/*! This function reads a portion of data within a read burst into the supplied buffer.
	Because the read bursts involve prefetching the data into a temporary buffer (see \ref TRMBeginReadBurst), calling this function involves an unnecessary copying of data
	between the temporary buffer and the final buffer. If you would like to avoid it, use the \ref TRMBeginReadFileCached/\ref TRMEndReadFileCached instead.
	
	\param hBurst Burst handle returned by \ref TRMBeginReadBurst
	\param pBuffer Buffer that will receive the data
	\param size Maximum amount of bytes to read
	\param allowPartialReads Specifies whether the function can return if some, but not all of the data has been read (similar to the socket API).
	
	\return If the function succeeds, it returns the number of bytes read from the file. Unless <b>allowPartialReads</b> was set to 1, an end-of-file or an error has been encountered,
			it will be equal to the <b>size</b> parameter. If the function fails, the return value is zero or negative.
*/
ssize_t TRMReadFileCached(TRMReadBurstHandle hBurst, void *pBuffer, size_t size, int allowPartialReads);

//! Starts a read burst.
/*! This function starts a write burst (i.e. series of cached non-blocking writes) using the fast semihosting buffer. See \ref TRM_WriteBursts for more details.
	Once a burst has been started, use \ref TRMWriteFileCached to write the data to  the file.
	\param hFile File handle returned by \ref TRMCreateFile.
	
	\remarks Once the burst has been started, regular file API will not work for the file. Use \ref TRMEndWriteBurst to end the burst.
			 Each file can only have one burst operation active at once, however multiple files can have active bursts at the same time.
*/
TRMWriteBurstHandle TRMBeginWriteBurst(TRMFileHandle hFile);

//! Ends a write burst.
/*! \param hBurst Burst handle returned by \ref TRMBeginWriteBurst */
TRMErrorCode TRMEndWriteBurst(TRMWriteBurstHandle hBurst);

//! Writes data within a write burst.
/*! This function writes data to a file on the host within a write burst. As long as the fast semihosting buffer has sufficient space, the function will return immediately,
	letting the target run more code, while VisualGDB is processing the data in the background.
	\param hBurst Burst handle returned by \ref TRMBeginWriteBurst.
	\param pBuffer Buffer that contains the data.
	\param size The maximum number of bytes that should be written to the file.
*/
ssize_t TRMWriteFileCached(TRMWriteBurstHandle hBurst, const void *pData, size_t size);

#ifdef __cplusplus
}
#endif