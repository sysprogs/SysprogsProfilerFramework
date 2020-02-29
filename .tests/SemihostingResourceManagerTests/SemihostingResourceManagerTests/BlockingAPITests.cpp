#include <TinyEmbeddedTest.h>
#include <stdio.h>
#include "TestResourceManager.h"

#include <stm32f4xx_hal.h>

TEST_GROUP(BlockingAPITests)
{
};

TEST(BlockingAPITests, GetSystemTime)
{
	const int Delay = 300;

	auto baseTime = TRMGetHostSystemTime();
	HAL_Delay(Delay);
	auto finalTime = TRMGetHostSystemTime();
	int passedMsec = (finalTime - baseTime) / 10000;
	int error = abs(passedMsec - Delay);
	CHECK(error < 50);
}

template <int _Size> void FillBufferWithRandomBytes(char (&buf)[_Size])
{
	for (int i = 0; i < sizeof(_Size); i++)
		buf[i] = rand();
}

TEST(BlockingAPITests, PortionedWriteTest)
{
	srand(123);
	char testFileContents[4096], tempBuffer[4096];
	FillBufferWithRandomBytes(testFileContents);

	auto hFile = TRMCreateFile("PortionedWriteTest.dat", sfmCreateOrOpenReadWrite);
	CHECK(hFile != 0);
	int done = 0;

	while (done < sizeof(testFileContents))
	{
		int todo = rand() % 128;
		if (todo > (sizeof(testFileContents) - done))
			todo = sizeof(testFileContents) - done;

		int doneNow = TRMWriteFile(hFile, testFileContents + done, todo);
		CHECK_EQUAL(todo, doneNow);

		done += doneNow;
	}

	TRMSeekFile(hFile, SEEK_SET, 0);
	memset(tempBuffer, 0x55, sizeof(tempBuffer));
	done = TRMReadFile(hFile, tempBuffer, sizeof(tempBuffer));
	CHECK_EQUAL(sizeof(tempBuffer), done);
	CHECK_EQUAL(0, memcmp(testFileContents, tempBuffer, sizeof(tempBuffer)));

	TRMCloseFile(hFile);
}

void VerifyFilePosition(TRMFileHandle hFile, const char *pTestFileContents, int totalSize, int expectedOffset, uint64_t reportedPosition)
{
	char tempBuffer[4096];

	CHECK_EQUAL(expectedOffset, reportedPosition);
	int done = TRMReadFile(hFile, tempBuffer, sizeof(tempBuffer));
	CHECK_EQUAL(totalSize - expectedOffset, done);

	int comparisonResult = memcmp(tempBuffer, pTestFileContents + expectedOffset, done);
	CHECK_EQUAL(0, comparisonResult);
}

TEST(BlockingAPITests, SeekAndReadTest)
{
	srand(123);
	char testFileContents[4096];
	FillBufferWithRandomBytes(testFileContents);

	auto hFile = TRMCreateFile("SeekAndReadTest.dat", sfmCreateOrOpenReadWrite);
	CHECK(hFile != 0);

	TRMTruncateFile(hFile);
	CHECK_EQUAL(0, TRMGetFileSize(hFile));

	int done = TRMWriteFile(hFile, testFileContents, sizeof(testFileContents));
	CHECK_EQUAL(sizeof(testFileContents), done);

	uint64_t position = TRMSeekFile(hFile, SEEK_CUR, 0);
	CHECK_EQUAL(sizeof(testFileContents), position);

	//Seek to <end> - 123
	position = TRMSeekFile(hFile, SEEK_CUR, -123);
	VerifyFilePosition(hFile, testFileContents, sizeof(testFileContents), sizeof(testFileContents) - 123, position);

	//Seek to <end> - 123 - 100
	position = TRMSeekFile(hFile, SEEK_END, -123);
	CHECK_EQUAL(sizeof(testFileContents) - 123, position);

	position = TRMSeekFile(hFile, SEEK_CUR, -100);
	VerifyFilePosition(hFile, testFileContents, sizeof(testFileContents), sizeof(testFileContents) - 123 - 100, position);

	//Seek to <end> - 123 + 100
	position = TRMSeekFile(hFile, SEEK_END, -123);
	CHECK_EQUAL(sizeof(testFileContents) - 123, position);

	position = TRMSeekFile(hFile, SEEK_CUR, 100);
	VerifyFilePosition(hFile, testFileContents, sizeof(testFileContents), sizeof(testFileContents) - 123 + 100, position);

	//Seek to <end> - 123, then reset it to <end> - 100
	position = TRMSeekFile(hFile, SEEK_END, -123);
	CHECK_EQUAL(sizeof(testFileContents) - 123, position);

	position = TRMSeekFile(hFile, SEEK_END, -100);
	VerifyFilePosition(hFile, testFileContents, sizeof(testFileContents), sizeof(testFileContents) - 100, position);

	//Seek to <start> + 123
	position = TRMSeekFile(hFile, SEEK_SET, 123);
	VerifyFilePosition(hFile, testFileContents, sizeof(testFileContents), 123, position);

	//Test truncation
	position = TRMSeekFile(hFile, SEEK_SET, 128);
	TRMTruncateFile(hFile);
	position = TRMSeekFile(hFile, SEEK_END, 0);
	CHECK_EQUAL(128, position);

	done = TRMReadFile(hFile, testFileContents, sizeof(testFileContents));
	CHECK_EQUAL(0, done);

	TRMCloseFile(hFile);	
}

TEST(BlockingAPITests, DeleteTest)
{
	const char *pSubdir = "DeleteDirectoryTest";
	const char *pFileName = "DeleteDirectoryTest/test.dat";
	TRMDeleteDirectory(pSubdir, true);

	TRMFileHandle hFile = TRMCreateFile(pFileName, sfmCreateOrTruncateWriteOnly);
	CHECK_EQUAL(0, hFile);

	TRMErrorCode err = TRMCreateDirectory(pSubdir);
	CHECK_EQUAL(trmSuccess, err);

	hFile = TRMCreateFile(pFileName, sfmCreateOrTruncateWriteOnly);
	CHECK(hFile != 0);
	TRMCloseFile(hFile);

	hFile = TRMCreateFile(pFileName, sfmOpenReadOnly);
	CHECK(hFile != 0);
	TRMCloseFile(hFile);

	err = TRMDeleteFile(pFileName);
	CHECK_EQUAL(trmSuccess, err);

	hFile = TRMCreateFile(pFileName, sfmOpenReadOnly);
	CHECK_EQUAL(0, hFile);

	hFile = TRMCreateFile(pFileName, sfmCreateOrTruncateWriteOnly);
	CHECK(hFile != 0);
	TRMCloseFile(hFile);

	err = TRMDeleteDirectory(pSubdir, false);
	CHECK(err != 0);

	err = TRMDeleteDirectory(pSubdir, true);
	CHECK_EQUAL(err, 0);

	hFile = TRMCreateFile(pFileName, sfmCreateOrTruncateWriteOnly);
	CHECK_EQUAL(0, hFile);
}

