#include "TestResourceManager.h"
#include <TinyEmbeddedTest.h>
#include <stdio.h>

#include <stm32f4xx_hal.h>

TEST_GROUP(BurstTests)
{
};

TEST(BurstTests, ReadBurstTest)
{
	int blockBuffer[1024];
	char burstBuffer[4096];
	const int blockCount = 64;

	TRMFileHandle hFile = TRMCreateFile("ReadBurstTest.dat", sfmCreateOrTruncateReadWrite);
	CHECK(hFile != 0);
	uint32_t start = HAL_GetTick();

	for (int block = 0; block < blockCount; block++)
	{
		for (int i = 0; i < sizeof(blockBuffer) / sizeof(blockBuffer[0]); i++)
			blockBuffer[i] = (block << 16) | i;

		int done = TRMWriteFile(hFile, blockBuffer, sizeof(blockBuffer));
		CHECK_EQUAL(sizeof(blockBuffer), done);
	}

	printf("Regular write of %d KB completed in %d ticks\r\n", (blockCount * sizeof(blockBuffer)) / 1024, (uint32_t)(HAL_GetTick() - start));
	start = HAL_GetTick();

	uint64_t position = TRMSeekFile(hFile, SEEK_SET, 0);
	CHECK_EQUAL(0, position);

	for (int block = 0; block < blockCount; block++)
	{
		int todo = sizeof(blockBuffer);
		int done = TRMReadFile(hFile, blockBuffer, todo);
		CHECK_EQUAL(todo, done);

		for (int i = 0; i < sizeof(blockBuffer) / sizeof(blockBuffer[0]); i++)
			CHECK_EQUAL((block << 16) | i, blockBuffer[i]);
	}

	printf("Regular read of %d KB completed in %d ticks\r\n", (blockCount * sizeof(blockBuffer)) / 1024, (uint32_t)(HAL_GetTick() - start));
	start = HAL_GetTick();

	position = TRMSeekFile(hFile, SEEK_SET, 0);
	CHECK_EQUAL(0, position);

	auto hBurst = TRMBeginCachedRead(hFile, burstBuffer, sizeof(burstBuffer));
	CHECK(hBurst != 0);

	srand(123);

	for (int block = 0; block < blockCount; block++)
	{
		int done = 0;
		while (done < sizeof(blockBuffer))
		{
			int todo = sizeof(blockBuffer) - done;
			int randomLimit = rand() % sizeof(blockBuffer);
			if (randomLimit && randomLimit < todo)
				todo = randomLimit;

			int doneNow = TRMReadFileCached(hBurst, (char *)blockBuffer + done, todo, false);
			CHECK_EQUAL(todo, doneNow);

			done += todo;
		}

		for (int i = 0; i < sizeof(blockBuffer) / sizeof(blockBuffer[0]); i++)
			CHECK_EQUAL((block << 16) | i, blockBuffer[i]);
	}

	printf("Cached read of %d KB completed in %d ticks\r\n", (blockCount * sizeof(blockBuffer)) / 1024, (uint32_t)(HAL_GetTick() - start));
	TRMErrorCode err = TRMEndCachedRead(hBurst);
	CHECK_EQUAL(trmSuccess, err);
	
	TRMCloseFile(hFile);
}

TEST(BurstTests, WriteBurstTest)
{
	int blockBuffer[1024];
	char burstBuffer[4096];
	const int blockCount = 64;

	TRMFileHandle hFile = TRMCreateFile("WriteBurstTest.dat", sfmCreateOrTruncateReadWrite);
	CHECK(hFile != 0);

	auto hBurst = TRMBeginCachedWrite(hFile);
	CHECK(hBurst != 0);

	uint32_t start = HAL_GetTick();

	for (int block = 0; block < blockCount; block++)
	{
		for (int i = 0; i < sizeof(blockBuffer) / sizeof(blockBuffer[0]); i++)
			blockBuffer[i] = (block << 16) | i;

		int done = TRMWriteFileCached(hBurst, blockBuffer, sizeof(blockBuffer));
		CHECK_EQUAL(sizeof(blockBuffer), done);
	}

	printf("Cached write of %d KB completed in %d ticks\r\n", (blockCount * sizeof(blockBuffer)) / 1024, (uint32_t)(HAL_GetTick() - start));
	start = HAL_GetTick();

	TRMErrorCode err = TRMEndCachedWrite(hBurst);
	CHECK_EQUAL(trmSuccess, err);

	uint64_t position = TRMSeekFile(hFile, SEEK_SET, 0);
	CHECK_EQUAL(0, position);

	for (int block = 0; block < blockCount; block++)
	{
		int todo = sizeof(blockBuffer);
		int done = TRMReadFile(hFile, blockBuffer, todo);
		CHECK_EQUAL(todo, done);

		for (int i = 0; i < sizeof(blockBuffer) / sizeof(blockBuffer[0]); i++)
			CHECK_EQUAL((block << 16) | i, blockBuffer[i]);
	}

	printf("Regular read of %d KB completed in %d ticks\r\n", (blockCount * sizeof(blockBuffer)) / 1024, (uint32_t)(HAL_GetTick() - start));
	start = HAL_GetTick();

	TRMCloseFile(hFile);
}
