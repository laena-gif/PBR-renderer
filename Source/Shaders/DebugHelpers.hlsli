#ifndef _DEBUG_HELPERS_HLSLI_
#define _DEBUG_HELPERS_HLSLI_

uint GetDigitBits(uint digit)
{
	switch (digit)
	{
		case 0: return 0x7555557;
		case 1: return 0x4444444;
		case 2: return 0x7117447;
		case 3: return 0x7447447;
		case 4: return 0x4447555;
		case 5: return 0x7447117;
		case 6: return 0x7557117;
		case 7: return 0x4444447;
		case 8: return 0x7557557;
		case 9: return 0x7447557;
	}

	return 0;
}

bool SampleDigit(uint digit, uint2 coord)
{
	if (any(coord >= uint2(4, 8)))
	{
		return false;
	}

	uint i = coord.y * 4 + coord.x;
	return (GetDigitBits(digit) & (1 << i)) != 0;
}

#endif