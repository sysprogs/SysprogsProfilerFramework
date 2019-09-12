#pragma once

class SmallNumberCoder
{
private:
	char *m_pBuffer;
	int m_BufferSize;
	int m_Offset;

	void *m_RefPointA, *m_RefPointB;

private:
	static inline int IsSignedIntConstrained(int value, int bits)
	{
		int mask = -1 << bits;
		int refVal = ~(((value >> (bits - 1)) & 1) - 1);
		return (value & mask) == (refVal & mask);
	}

	static inline int IsUnsignedIntConstrained(unsigned value, int bits)
	{
		int mask = -1 << bits;
		return (value & mask) == 0;
	}

	inline bool WriteSmallInteger(int value, int byteCount)
	{
		if (byteCount > (m_BufferSize - m_Offset))
			return false;

#if defined(ARM_MATH_CM3) || defined(ARM_MATH_CM4)
		switch (byteCount)
		{
		case 1:
			*((char *)(m_pBuffer + m_Offset)) = (char)value;
			break;
		case 2:
			*((short *)(m_pBuffer + m_Offset)) = (short)value;
			break;
		case 4:
			*((int *)(m_pBuffer + m_Offset)) = (int)value;
			break;
		default:
			return 0;
		}
#else
		for (int i = 0; i < byteCount; i++)
			*((char *)(m_pBuffer + m_Offset + i)) = ((char *)&value)[i];
#endif

		m_Offset += byteCount;
		return byteCount;
	}

public:
	SmallNumberCoder(void *pBuffer, unsigned size, void *pRefPointA, void *pRefPointB)
		: m_pBuffer((char *)pBuffer), m_BufferSize(size), m_Offset(0), m_RefPointA(pRefPointA), m_RefPointB(pRefPointB)
	{
	}

	inline bool WriteTinySIntWithFlag(int value, bool flag)
	{
		if (IsSignedIntConstrained(value, 6))
		{
			unsigned b = value << 2 | (!!flag << 1);
			return WriteSmallInteger(b, 1);
		}
		else if (IsSignedIntConstrained(value, 12))
		{
			unsigned w = (value << 4) | (!!flag << 1) | (1 << 2) | 1;
			return WriteSmallInteger(w, 2);
		}
		else if (IsSignedIntConstrained(value, 28))
		{
			unsigned dw = (value << 4) | (!!flag << 1) | (2 << 2) | 1;
			return WriteSmallInteger(dw, 4);
		}
		else
		{
			unsigned b = (!!flag << 1) | (3 << 2) | 1;
			if (!WriteSmallInteger(b, 1))
				return false;
			if (!WriteSmallInteger(value, 4))
				return false;
			return true;
		}
	}

	inline bool WriteTinySInt(int value)
	{
		if (IsSignedIntConstrained(value, 7))
		{
			unsigned b = (value << 1);
			return WriteSmallInteger(b, 1);
		}
		else if (IsSignedIntConstrained(value, 14))
		{
			unsigned w = (value << 2) | 1;
			return WriteSmallInteger(w, 2);
		}
		else
		{
			unsigned dw = (value << 2) | 3;
			return WriteSmallInteger(dw, 4);
		}
	}

	inline bool WriteSmallMostLikelyEvenSInt(int value)
	{
		int signBit = (value >> 31) & 1;
		int permutatedValue = (int)((value & 0xffff0000) | ((value & 0xffff) >> 1) | (((value & 1) ^ signBit) << 15));

		if (IsSignedIntConstrained(permutatedValue, 15))
		{
			unsigned w = permutatedValue << 1;
			return WriteSmallInteger(w, 2);
		}
		else if (IsSignedIntConstrained(permutatedValue, 30))
		{
			unsigned dw = (permutatedValue << 2) | 1;
			return WriteSmallInteger(dw, 4);
		}
		else
		{
			unsigned b = 0xff;
			if (!WriteSmallInteger(b, 1))
				return false;
			return WriteSmallInteger(permutatedValue, 4);
		}
	}

	inline bool WriteSmallUnsignedInt(unsigned value)
	{
		if (IsUnsignedIntConstrained(value, 15))
		{
			unsigned w = value << 1;
			return WriteSmallInteger(w, 2);
		}
		else if (IsUnsignedIntConstrained(value, 30))
		{
			unsigned dw = (value << 2) | 1;
			return WriteSmallInteger(dw, 4);
		}
		else
		{
			unsigned b = 0xff;
			if (!WriteSmallInteger(b, 1))
				return false;
			return WriteSmallInteger(value, 4);
		}
	}

	inline bool WriteSmallUnsignedIntWithFlag(unsigned value, bool flag)
	{
		if (IsUnsignedIntConstrained(value, 14))
		{
			unsigned w = value << 2 | (!!flag << 1);
			return WriteSmallInteger(w, 2);
		}
		else if (IsUnsignedIntConstrained(value, 29))
		{
			unsigned dw = (value << 3) | 1 | (!!flag << 2);
			return WriteSmallInteger(dw, 4);
		}
		else
		{
			unsigned b = flag ? 0xff : 0x7f;
			if (!WriteSmallInteger(b, 1))
				return false;
			return WriteSmallInteger(value, 4);
		}
	}

	inline bool WriteSmallSignedIntWithFlag(int value, bool flag)
	{
		if (IsSignedIntConstrained(value, 14))
		{
			unsigned w = value << 2 | (!!flag << 1);
			return WriteSmallInteger(w, 2);
		}
		else if (IsSignedIntConstrained(value, 29))
		{
			unsigned dw = (value << 3) | 1 | (!!flag << 2);
			return WriteSmallInteger(dw, 4);
		}
		else
		{
			unsigned b = flag ? 0xff : 0x7f;
			if (!WriteSmallInteger(b, 1))
				return false;
			return WriteSmallInteger(value, 4);
		}
	}

	inline bool WritePackedUIntPair(unsigned short largerInt, unsigned short smallerInt)
	{
		if (largerInt < 16 && smallerInt < 14)
		{
			unsigned b = largerInt << 4 | smallerInt;
			return WriteSmallInteger(b, 1);
		}
		else if (largerInt < 256 && smallerInt < 256)
		{
			if (!WriteSmallInteger(0xFE, 1))
				return false;
			if (!WriteSmallInteger(largerInt, 1))
				return false;
			if (!WriteSmallInteger(smallerInt, 1))
				return false;
			return true;
		}
		else
		{
			if (!WriteSmallInteger(0xFF, 1))
				return false;
			if (!WriteSmallInteger(largerInt, 2))
				return false;
			if (!WriteSmallInteger(smallerInt, 2))
				return false;
			return true;
		}
	}

	inline bool WriteStackEntry(int indexDelta, void *value, bool useRefPointB)
	{
		int dist;
		if (useRefPointB)
		{
			dist = (int)value - (int)m_RefPointB;
			m_RefPointB = value;
		}
		else
		{
			dist = (int)value - (int)m_RefPointA;
			m_RefPointA = value;
		}

		if (!WriteTinySIntWithFlag(indexDelta, useRefPointB))
			return false;
		return WriteSmallMostLikelyEvenSInt(dist);
	}

	int GetOffset()
	{
		return m_Offset;
	}

	void *GetBuffer()
	{
		return m_pBuffer;
	}

	void ExportState(void **pRefPointA, void **pRefPointB)
	{
		*pRefPointA = m_RefPointA;
		*pRefPointB = m_RefPointB;
	}

	void SetOffset(int offset)
	{
		m_Offset = offset;
	}

	unsigned RemainingSize()
	{
		return m_BufferSize - m_Offset;
	}
};

class SmallNumberDecoder
{
private:
	const char *m_pBuffer;
	int m_BufferSize;
	int m_Offset;

	char *m_RefPointA, *m_RefPointB;

private:
	static int SignExtend(int value, int bits)
	{
		return ((~(((value >> (bits - 1)) & 1) - 1)) << bits) | value;
	}

	inline unsigned PeekInt32(int delta = 0)
	{
		int offset = m_Offset + delta;
#if defined(ARM_MATH_CM3) || defined(ARM_MATH_CM4)
		return *((unsigned *)(m_pBuffer + offset));
#else
		return (((unsigned char *)(m_pBuffer + offset))[0] << 0) | (((unsigned char *)(m_pBuffer + offset))[1] << 8) | (((unsigned char *)(m_pBuffer + offset))[2] << 16) | (((unsigned char *)(m_pBuffer + offset))[3] << 24);
#endif
	}

	inline unsigned PeekInt16(int delta = 0)
	{
		int offset = m_Offset + delta;
#if defined(ARM_MATH_CM3) || defined(ARM_MATH_CM4)
		return *((unsigned short *)(m_pBuffer + offset));
#else
		return (((unsigned char *)(m_pBuffer + offset))[0] << 0) | (((unsigned char *)(m_pBuffer + offset))[1] << 8);
#endif
	}

public:
	SmallNumberDecoder(const void *pBuffer, unsigned size, void *pRefPointA, void *pRefPointB)
		: m_pBuffer((char *)pBuffer), m_BufferSize(size), m_Offset(0), m_RefPointA((char *)pRefPointA), m_RefPointB((char *)pRefPointB)
	{
	}

	inline bool ReadTinySIntWithFlag(int *pValue, bool *pFlag)
	{
		if ((m_Offset + 1) > m_BufferSize)
			return false;

		unsigned char b = (unsigned char)m_pBuffer[m_Offset];

		if ((b & 0x01) == 0)
		{
			*pValue = SignExtend(b >> 2, 6);
			*pFlag = (b & 0x02) != 0;
			m_Offset += 1;
			return true;
		}
		else if (((b >> 2) & 0x03) == 1)
		{
			if ((m_Offset + 2) > m_BufferSize)
				return false;
			unsigned w = PeekInt16();

			*pValue = SignExtend(w >> 4, 12);
			*pFlag = (w & 0x02) != 0;
			m_Offset += 2;
			return true;
		}
		else if (((b >> 2) & 0x03) == 2)
		{
			if ((m_Offset + 4) > m_BufferSize)
				return false;
			unsigned dw = PeekInt32();

			*pValue = SignExtend(dw >> 4, 28);
			*pFlag = (dw & 0x02) != 0;
			m_Offset += 4;
			return true;
		}
		else
		{
			if ((m_Offset + 5) > m_BufferSize)
				return false;
			*pValue = PeekInt32(1);
			*pFlag = (b & 0x02) != 0;
			m_Offset += 5;
			return true;
		}
	}

	bool ReadSmallMostlyEvenSInt(int *pValue)
	{
		if ((m_Offset + 1) > m_BufferSize)
			return false;

		unsigned char b = (unsigned char)m_pBuffer[m_Offset];

		int permutatedValue;
		if ((b & 0x01) == 0)
		{
			if ((m_Offset + 2) > m_BufferSize)
				return false;
			short w = PeekInt16();
			permutatedValue = w >> 1;
			m_Offset += 2;
		}
		else if ((b & 0x03) == 0x01)
		{
			if ((m_Offset + 4) > m_BufferSize)
				return false;
			int dw = PeekInt32();
			permutatedValue = dw >> 2;
			m_Offset += 4;
		}
		else
		{
			if ((m_Offset + 5) > m_BufferSize)
				return false;
			int dw = PeekInt32(1);
			permutatedValue = dw >> 2;
			m_Offset += 5;
		}

		int signBit = (permutatedValue >> 31) & 1;
		*pValue = (int)(((unsigned)permutatedValue & 0xffff0000) | ((((unsigned)permutatedValue & 0x8000) >> 15) ^ signBit) | (((unsigned)permutatedValue & 0x7fff) << 1));
		return true;
	}

	inline bool ReadStackEntry(void ***pInOutStackAddr, void **pValue)
	{
		int dist;
		bool useRefPointB;
		int indexDelta = 0;

		if (!ReadTinySIntWithFlag(&indexDelta, &useRefPointB))
			return false;
		if (!ReadSmallMostlyEvenSInt(&dist))
			return false;

		if (useRefPointB)
			*pValue = (m_RefPointB += dist);
		else
			*pValue = (m_RefPointA += dist);

		*pInOutStackAddr += indexDelta;

		return true;
	}
};
