
/* based on code found at:
https://github.com/FroggySoft/AlarmClock/blob/master/dcf77.cpp
https://github.com/tobozo/esp32-dcf77-weatherman/blob/master/dcf77.cpp
*/

#include <stdio.h>
#include <stdint.h>
#include <endian.h>
#include "../libdebug/debug.h"
#include "weather.h"

/// Container zum Konvertieren zwischen 4 Bytes und Uint.
union ByteUInt {
	struct {
# if __BYTE_ORDER == __LITTLE_ENDIAN
		uint8_t Byte0;
		uint8_t Byte1;
		uint8_t Byte2;
		uint8_t Byte3;
# elif __BYTE_ORDER == __BIG_ENDIAN
		uint8_t Byte3;
		uint8_t Byte2;
		uint8_t Byte1;
		uint8_t Byte0;
# else
#error unsupported bitorder, please fix
# endif
	} s;
	uint32_t FullUint;
};

/// bit pattern for 0D,0E from 0B-0D
static const uint32_t mUintArrBitPattern12[12] = {
	0x80000, //0b10000000000000000000 / 0D.3
	0x00010, //0b00000000000000010000 / 0B.4
	0x00008, //0b00000000000000001000 / 0B.3
	0x00100, //0b00000000000100000000 / 0C.0
	0x00080, //0b00000000000010000000 / 0B.7
	0x01000, //0b00000001000000000000 / 0C.4
	0x00800, //0b00000000100000000000 / 0C.3
	0x10000, //0b00010000000000000000 / 0D.0
	0x08000, //0b00001000000000000000 / 0C.7
	0x00001, //0b00000000000000000001 / 0B.0
	0x00000, //0b00000000000000000000 / xxxx
	0x00000  //0b00000000000000000000 / xxxx
};

/// 12-15 from 16-19 (time)
static const uint32_t mUintArrBitPattern30_1[30] = {
	0x00000200, //0b00000000000000000000001000000000 / 17.1
	0x00000020, //0b00000000000000000000000000100000 / 16.5
	0x02000000, //0b00000010000000000000000000000000 / 19.1
	0x00000000, //0b00000000000000000000000000000000 / 1A.3
	0x00000000, //0b00000000000000000000000000000000 / 1A.5
	0x00000080, //0b00000000000000000000000010000000 / 16.7
	0x40000000, //0b01000000000000000000000000000000 / 19.6
	0x01000000, //0b00000001000000000000000000000000 / 19.0

	0x04000000, //0b00000100000000000000000000000000 / 19.2
	0x00000000, //0b00000000000000000000000000000000 / 1A.4
	0x00010000, //0b00000000000000010000000000000000 / 18.0
	0x00000000, //0b00000000000000000000000000000000 / 1A.2
	0x00400000, //0b00000000010000000000000000000000 / 18.6
	0x00000010, //0b00000000000000000000000000010000 / 16.4
	0x00200000, //0b00000000001000000000000000000000 / 18.5
	0x00080000, //0b00000000000010000000000000000000 / 18.3

	0x00004000, //0b00000000000000000100000000000000 / 17.6
	0x00000000, //0b00000000000000000000000000000000 / 1A.6
	0x00020000, //0b00000000000000100000000000000000 / 18.1
	0x00100000, //0b00000000000100000000000000000000 / 18.4
	0x00008000, //0b00000000000000001000000000000000 / 17.7
	0x00000040, //0b00000000000000000000000001000000 / 16.6
	0x00001000, //0b00000000000000000001000000000000 / 17.4
	0x00000400, //0b00000000000000000000010000000000 / 17.2

	0x00000001, //0b00000000000000000000000000000001 / 16.0
	0x80000000, //0b10000000000000000000000000000000 / 19.7
	0x00000008, //0b00000000000000000000000000001000 / 16.3
	0x00000002, //0b00000000000000000000000000000010 / 16.1
	0x00040000, //0b00000000000001000000000000000000 / 18.2
	0x10000000  //0b00010000000000000000000000000000 / 19.4
};

/// bit pattern for 12-15 from 1A (time2)
static const uint32_t mUintArrBitPattern30_2[30] = {
	0x00, //0b00000000,  /* 17.1
	0x00, //0b00000000,  /* 16.5
	0x00, //0b00000000,  /* 19.1
	0x08, //0b00001000,  /* 1A.3
	0x20, //0b00100000,  /* 1A.5
	0x00, //0b00000000,  /* 16.7
	0x00, //0b00000000,  /* 19.6
	0x00, //0b00000000,  /* 19.0

	0x00, //0b00000000,  /* 19.2
	0x10, //0b00010000,  /* 1A.4
	0x00, //0b00000000,  /* 18.0
	0x04, //0b00000100,  /* 1A.2
	0x00, //0b00000000,  /* 18.6
	0x00, //0b00000000,  /* 16.4
	0x00, //0b00000000,  /* 18.5
	0x00, //0b00000000,  /* 18.3

	0x00, //0b00000000,  /* 17.6
	0x40, //0b01000000,  /* 1A.6
	0x00, //0b00000000,  /* 18.1
	0x00, //0b00000000,  /* 18.4
	0x00, //0b00000000,  /* 17.7
	0x00, //0b00000000,  /* 16.6
	0x00, //0b00000000,  /* 17.4
	0x00, //0b00000000,  /* 17.2

	0x00, //0b00000000,  /* 16.0
	0x00, //0b00000000,  /* 19.7
	0x00, //0b00000000,  /* 16.3
	0x00, //0b00000000,  /* 16.1
	0x00, //0b00000000,  /* 18.2
	0x00  //0b00000000,  /* 19.4
};

/// 12-14 from 1C-1E (result from F)
static const uint32_t mUintArrBitPattern20[20] = {
	0x000004, //0b000000000000000000000100 / 1C.2
	0x002000, //0b000000000010000000000000 / 1E.5
	0x008000, //0b000000001000000000000000 / 1E.7
	0x400000, //0b010000000000000000000000 / 1D.6
	0x000100, //0b000000000000000100000000 / 1E.0
	0x100000, //0b000100000000000000000000 / 1D.4
	0x000400, //0b000000000000010000000000 / 1E.2
	0x800000, //0b100000000000000000000000 / 1D.7

	0x040000, //0b000001000000000000000000 / 1D.2
	0x020000, //0b000000100000000000000000 / 1D.1
	0x000008, //0b000000000000000000001000 / 1C.3
	0x000200, //0b000000000000001000000000 / 1E.1
	0x004000, //0b000000000100000000000000 / 1E.6
	0x000002, //0b000000000000000000000010 / 1C.1
	0x001000, //0b000000000001000000000000 / 1E.4
	0x080000, //0b000010000000000000000000 / 1D.3

	0x000800, //0b000000000000100000000000 / 1E.3
	0x200000, //0b001000000000000000000000 / 1D.5
	0x010000, //0b000000010000000000000000 / 1D.0
	0x000001  //0b000000000000000000000001 / 1C.0
};

/// bit pattern for 12-15 from 16-19 (1/3)
static const uint64_t mByteArrLookupTable1C_1[8] = {
	0xBB0E22C573DFF76D, 0x90E9A1381C844A56,
	0x648D280BD1BA9352, 0x1CC5A7F0E97F364E,
	0xC1773DB3AAE00C6F, 0x1488F62BD2995E45,
	0x1F7096D3B30BFCEE, 0x8142CA34A5582967
};

/// bit pattern for 12-15 from 16-19 (2/3)
static const uint64_t mByteArrLookupTable1C_2[8] = {
	0xAB3DFC7465E60E4F, 0x9711D85983C2BA20,
	0xC51BD2584937017D, 0x93FAE02F66B4AC8E,
	0xB7CC43FF5866EB35, 0x822A99DD007114AE,
	0x4EB1F7701852AA9F, 0xD56BCC3D0483E926
};

/// bit pattern for 12-15 from 16-19 (3/3)
static const uint64_t mByteArrLookupTable1C_3[8] = {
	0x0A02000F06070D08, 0x030C0B050901040E,
	0x0209050D0C0E0F08, 0x06070B01000A0403,
	0x08000D0F010C0306, 0x0B0409050A07020E,
	0x030D000C09060F0B, 0x010E080A02070405
};

/// Container, which contains all former global vars
typedef struct DataContainer {
	/// Registers R12 to R15
	union ByteUInt mByteUint1;
	/// Registers R08 to R0A
	union ByteUInt mByteUint2;
	/// Registers R0B to R0E
	union ByteUInt mByteUint3;
	/// Registers R1C to R1E
	union ByteUInt mByteUint4;

	uint8_t mByteUpperTime2;//, mByteR1B;
	uint32_t mUintLowerTime;
} DataContainer_t;

static int32_t GetWeatherFromPlain(uint8_t *PlainBytes)
{
	uint32_t result;
	uint32_t checkSum;

	checkSum = PlainBytes[2] & 0x0f;
	checkSum <<= 8;
	checkSum |= PlainBytes[1];
	checkSum <<= 4;
	checkSum |= PlainBytes[0] >> 4;
	if (checkSum != 0x2501)
		return -1;

	result = PlainBytes[0] & 0x0f;
	result <<= 8;
	result |= PlainBytes[4];
	result <<= 8;
	result |= PlainBytes[3];
	result <<= 4;
	result |= PlainBytes[2] >> 4;

	return result;
}

static uint8_t *GetPlainFromWeather(uint32_t weather)
{
	static uint8_t result[5];
	weather <<= 4;
	result[1] = 0x50;
	result[2] = (weather & 0xf0) | 0x02;
	weather >>= 8;
	result[3] = weather & 0xff;
	weather >>= 8;
	result[4] = weather & 0xff;
	weather >>= 8;
	result[0] = (weather & 0x0f) | 0x10;
	return result;
}

static void CopyTimeToByteUint(uint8_t *data, uint8_t *key, DataContainer_t *container)
{
	int i;

	for (i = 0; i < 4; i++)
	{
		container->mUintLowerTime <<= 8;
		container->mUintLowerTime |= key[3 - i];
	}
	container->mByteUpperTime2 = key[4];

	// copy R
	container->mByteUint3.s.Byte0 = data[2];
	container->mByteUint3.s.Byte1 = data[3];
	container->mByteUint3.s.Byte2 = data[4];
	container->mByteUint3.FullUint >>= 4;

	// copy L
	container->mByteUint2.s.Byte0 = data[0];
	container->mByteUint2.s.Byte1 = data[1];
	container->mByteUint2.s.Byte2 = (uint8_t)(data[2] & 0x0F);
}

static void ShiftTimeRight(int round, DataContainer_t *container)
{
	int count;
	uint8_t tmp;

	if ((round == 16) || (round == 8) || (round == 7) || (round == 3))
		count = 2;
	else
		count = 1;

	while (count-- != 0)
	{
		tmp = 0;
		if ((container->mUintLowerTime & 0x00100000) != 0)					// save time bit 20
			tmp = 1;

		container->mUintLowerTime &= 0xFFEFFFFF;
		if ((container->mUintLowerTime & 1) != 0)
			container->mUintLowerTime |= 0x00100000;				// copy time bit 0 to time bit 19
		container->mUintLowerTime >>= 1;							// time >>= 1

		if ((container->mByteUpperTime2 & 1) != 0)
			container->mUintLowerTime |= 0x80000000;
		container->mByteUpperTime2 >>= 1;
		if (tmp != 0)
			container->mByteUpperTime2 |= 0x80;					// insert time bit 20 to time bit 39
	}

}

static void ShiftTimeLeft(int round, DataContainer_t *container)
{
	int count;
	uint8_t tmp;

	if ((round == 16) || (round == 8) || (round == 7) || (round == 3))
		count = 2;
	else
		count = 1;

	while (count-- != 0)
	{
		tmp = 0;
		if ((container->mByteUpperTime2 & 0x80) != 0)
			tmp = 1;
		container->mByteUpperTime2 <<= 1;

		if ((container->mUintLowerTime & 0x80000000) != 0)
			container->mByteUpperTime2 |= 1;

		container->mUintLowerTime <<= 1;
		if ((container->mUintLowerTime & 0x00100000) != 0)
			container->mUintLowerTime |= 1;

		container->mUintLowerTime &= 0xFFEFFFFF;
		if (tmp != 0)
			container->mUintLowerTime |= 0x00100000;
	}
}

static void ExpandR(DataContainer_t *container)
{
	uint32_t tmp;
	int i;

	container->mByteUint3.FullUint &= 0x000FFFFF;			// clear 0D(4-7),0E
	tmp = 0x00100000;					// and set bits form 0B-0D(0-3)
	for (i = 0; i < 12; i++)
	{
		if ((container->mByteUint3.FullUint & mUintArrBitPattern12[i]) != 0)
			container->mByteUint3.FullUint |= tmp;
		tmp <<= 1;
	}
}

static void ExpandL(DataContainer_t *container)
{
	uint32_t tmp;
	int i;

	container->mByteUint2.FullUint &= 0x000FFFFF;			// clear 0D(4-7),0E
	tmp = 0x00100000;					// and set bits form 0B-0D(0-3)
	for (i = 0; i < 12; i++)
	{
		if ((container->mByteUint2.FullUint & mUintArrBitPattern12[i]) != 0)
			container->mByteUint2.FullUint |= tmp;
		tmp <<= 1;
	}
}

static void CompressKey(DataContainer_t *container)
{
	uint32_t tmp;
	int i;

	container->mByteUint1.FullUint = 0;					// clear 12-15
	tmp = 0x00000001;					// and set bits from 16-1A (time)
	for (i = 0; i < 30; i++)
	{
		if ((container->mUintLowerTime & mUintArrBitPattern30_1[i]) != 0 || (container->mByteUpperTime2 & mUintArrBitPattern30_2[i]) != 0)
			container->mByteUint1.FullUint |= tmp;
		tmp <<= 1;
	}
}

static void DoSbox(DataContainer_t *container)
{
	uint8_t tmp, helper; //mByteR1B;
	int i;

	helper = container->mByteUint1.s.Byte3;					      // R1B = R15;
	container->mByteUint1.s.Byte3 = container->mByteUint1.s.Byte2;			// R15 = R14

	// INNER LOOP
	for (i = 5; i > 0; i--)
	{
		if ((i & 1) == 0) // round 4,2
		{
			tmp = (uint8_t)(container->mByteUint1.s.Byte0 >> 4);		// swap R12
			tmp |= (uint8_t)((container->mByteUint1.s.Byte0 & 0x0f) << 4);
			container->mByteUint1.s.Byte0 = tmp;
		}
		container->mByteUint1.s.Byte3 &= 0xF0;						// set R1C
		tmp = (uint8_t)((container->mByteUint1.s.Byte0 & 0x0F) | container->mByteUint1.s.Byte3);

		if ((i & 4) != 0)
			tmp = mByteArrLookupTable1C_1[(tmp & 0x38) >> 3] >> (56 - (tmp & 0x07) * 8);

		if ((i & 2) != 0)
			tmp = mByteArrLookupTable1C_2[(tmp & 0x38) >> 3] >> (56 - (tmp & 0x07) * 8);

		else if (i == 1)
			tmp = mByteArrLookupTable1C_3[(tmp & 0x38) >> 3] >> (56 - (tmp & 0x07) * 8);

		if ((i & 1) != 0)
			container->mByteUint4.s.Byte0 = (uint8_t)(tmp & 0x0F);
		else
			container->mByteUint4.s.Byte0 |= (uint8_t)(tmp & 0xF0);

		if ((i & 1) == 0)							// copy 14->13->12, 1C->1E->1D
		{
			tmp = container->mByteUint1.s.Byte3;
			container->mByteUint1.FullUint >>= 8;
			container->mByteUint1.s.Byte3 = tmp;
			container->mByteUint4.FullUint <<= 8;
		}

		container->mByteUint1.s.Byte3 >>= 1;					// rotate R1B>R15 twice
		if ((helper & 1) != 0)
			container->mByteUint1.s.Byte3 |= 0x80;
		helper >>= 1;

		container->mByteUint1.s.Byte3 >>= 1;
		if ((helper & 1) != 0)
			container->mByteUint1.s.Byte3 |= 0x80;
		helper >>= 1;
	} // end of inner loop
}

static void DoPbox(DataContainer_t *container)
{
	uint32_t tmp;
	int i;

	container->mByteUint1.FullUint = 0xFF000000;			// clear 12-14
	tmp = 0x00000001;					// and set bits from 1C-1E (result from F)
	for (i = 0; i < 20; i++)
	{
		if ((container->mByteUint4.FullUint & mUintArrBitPattern20[i]) != 0)
			container->mByteUint1.FullUint |= tmp;
		tmp <<= 1;
	}
}

/* modified DES decrypt using strings */
static uint8_t *Decrypt(uint8_t *cipher, uint8_t *key)
{
	DataContainer_t container;
	int i;

	static uint8_t plain[5];
	CopyTimeToByteUint(cipher, key, &container);

	// OUTER LOOP 1
	for (i = 16; i > 0; i--)
	{
		ShiftTimeRight(i, &container);
		ExpandR(&container);
		CompressKey(&container);

		// expR XOR compr.Key
		container.mByteUint1.FullUint ^= container.mByteUint3.FullUint;	// 12-15 XOR 0B-0E
		container.mByteUint3.s.Byte2 &= 0x0F;				// clear 0D(4-7)

		DoSbox(&container);
		DoPbox(&container);

		// L XOR P-Boxed Round-Key (L')
		container.mByteUint1.FullUint ^= container.mByteUint2.FullUint;

		// L = R
		container.mByteUint2.FullUint = container.mByteUint3.FullUint & 0x00FFFFFF;

		// R = L'
		container.mByteUint3.FullUint = container.mByteUint1.FullUint & 0x00FFFFFF;
	} // end of outer loop 1

	container.mByteUint3.FullUint <<= 4;
	container.mByteUint2.s.Byte2 &= 0x0F;
	container.mByteUint2.s.Byte2 |= (uint8_t)(container.mByteUint3.s.Byte0 & 0xF0);

	plain[0] = container.mByteUint2.s.Byte0;
	plain[1] = container.mByteUint2.s.Byte1;
	plain[2] = container.mByteUint2.s.Byte2;
	plain[3] = container.mByteUint3.s.Byte1;
	plain[4] = container.mByteUint3.s.Byte2;

	return plain;
}

/* modified DES encrypt using strings */
static uint8_t *Encrypt(uint8_t *plain, uint8_t *key)
{
	static uint8_t cipher[5];
	DataContainer_t container;
	int i;

	CopyTimeToByteUint(plain, key, &container);

	// OUTER LOOP 1
	for (i = 1; i < 17; i++)
	{
		ExpandL(&container);
		CompressKey(&container);

		// expR XOR compr.Key
		container.mByteUint1.FullUint ^= container.mByteUint2.FullUint;	   // L' XOR compr.Key
		container.mByteUint3.s.Byte2 &= 0x0F;				// clear 0D(4-7)

		DoSbox(&container);
		DoPbox(&container);

		// L XOR P-Boxed Round-Key (L')
		container.mByteUint1.FullUint ^= container.mByteUint3.FullUint;
		// L = R
		container.mByteUint3.FullUint = container.mByteUint2.FullUint & 0x00FFFFFF;
		// R = L'
		container.mByteUint2.FullUint = container.mByteUint1.FullUint & 0x00FFFFFF;

		ShiftTimeLeft(i, &container);
	} // end of outer loop 1

	container.mByteUint3.FullUint <<= 4;
	container.mByteUint2.s.Byte2 &= 0x0F;
	container.mByteUint2.s.Byte2 |= (uint8_t)(container.mByteUint3.s.Byte0 & 0xF0);

	cipher[0] = container.mByteUint2.s.Byte0;
	cipher[1] = container.mByteUint2.s.Byte1;
	cipher[2] = container.mByteUint2.s.Byte2;
	cipher[3] = container.mByteUint3.s.Byte1;
	cipher[4] = container.mByteUint3.s.Byte2;

	return cipher;
}

//#define DEBUG_CIPER

/* decode given crypted frame and key
 * return the weather info or -1 on checksum error
 */
int32_t weather_decode(uint64_t cipher, uint64_t key)
{
	uint8_t CipherBytes[5];
	uint8_t KeyBytes[5];
	uint8_t *PlainBytes;
	int32_t weather;
	int i;

	for (i = 0; i < 5; i++)
		CipherBytes[i] = cipher >> (i * 8);

	for (i = 0; i < 5; i++)
		KeyBytes[i] = key >> (i * 8);

	PlainBytes = Decrypt(CipherBytes, KeyBytes);

	weather = GetWeatherFromPlain(PlainBytes);

#ifdef DEBUG_CIPER
	printf("cipher=%s\n", debug_hex(CipherBytes, 5));
	printf("key   =%s\n", debug_hex(KeyBytes, 5));
	printf("plain =%s\n", debug_hex(PlainBytes, 5));
	if (weather < 0)
		printf("weather=error\n");
	else
		printf("weather=%06x\n", weather);

	weather_encode(weather, key);
#endif

	return weather;
}

/* encode given weather info and key
 * return crypted frame
 */
uint64_t weather_encode(uint32_t weather, uint64_t key)
{
	uint8_t KeyBytes[5];
	uint8_t *PlainBytes;
	uint8_t *CipherBytes;
	uint64_t cipher = 0;
	int i;

	PlainBytes = GetPlainFromWeather(weather);

	for (i = 0; i < 5; i++)
		KeyBytes[i] = key >> (i * 8);

	CipherBytes = Encrypt(PlainBytes, KeyBytes);

#ifdef DEBUG_CIPER
	printf("plain =%s\n", debug_hex(PlainBytes, 5));
	printf("key   =%s\n", debug_hex(KeyBytes, 5));
	printf("cipher=%s\n", debug_hex(CipherBytes, 5));
#endif

	for (i = 0; i < 5; i++)
		cipher |= (uint64_t)(CipherBytes[i]) << (i * 8);

	return cipher;
}

