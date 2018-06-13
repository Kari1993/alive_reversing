#include "stdafx.h"
#include "Math.hpp"

#define Math_IMPL true

void Math_ForceLink() { }

ALIVE_ARY(1, 0x546744, unsigned char, 256, sRandomBytes_546744, {});
ALIVE_VAR(1, 0x5D1E10, unsigned char, sRandomSeed_5D1E10, 0);

unsigned int  CC Math_FixedPoint_Multiply_496C50(signed int op1, signed int op2)
{
	unsigned int op1a; // ecx
	unsigned int op2a; // esi
	unsigned int result; // eax

	op1a = op1;
	if (op1 < 0)
		op1a = -op1;
	op2a = op2;
	if (op2 < 0)
		op2a = -op2;
	result = op2a * (op1a >> 16)
		+ (unsigned __int16)op1a * (op2a >> 16)
		+ ((unsigned __int16)op1a * (unsigned int)(unsigned __int16)op2a >> 16);
	if (op1 < 0 != op2 < 0)
		result = -result;
	return result;
}
ALIVE_FUNC_IMPLEX(0x496C50, Math_FixedPoint_Multiply_496C50, Math_IMPL);

unsigned int CC Math_FixedPoint_Divide_496B70(signed int op1, signed int op2)
{
	signed int op1a; // ebx
	unsigned int op1b; // edi
	unsigned int op2a; // ecx
	unsigned int v5; // esi
	int v6; // edi
	unsigned int result; // eax

	op1a = op1;
	op1b = op1;
	if (op1 < 0)
		op1b = -op1;
	op2a = op2;
	if (op2 < 0)
		op2a = -op2;
	v5 = op1b % op2a;
	v6 = op1b / op2a << 16;
	if (op2a >= 0x1000000)
	{
		result = v5 % (op2a >> 8) / (op2a >> 16) + v6 + (v5 / (op2a >> 8) << 8);
		op1a = op1;
	}
	else
	{
		result = ((v5 << 8) % op2a << 8) / op2a + v6 + ((v5 << 8) / op2a << 8);
	}
	if (op1a < 0 != op2 < 0)
		result = -result;
	return result;
}
ALIVE_FUNC_IMPLEX(0x496B70, Math_FixedPoint_Divide_496B70, Math_IMPL);

short CC Math_RandomRange_496AB0(signed short min, signed short max)
{
	short tempMax = max;
    short tempMin = min;

    if (max >= min)
    {
        if (max == min)
        {
            // No need for random if min and max are the same.
            return max;
        }
    }
    else
    {
        // Whoops, silly developer, you've swapped the min and max values! Let me fix that for you!
        tempMax = min;
        tempMin = max;
    }

    short result = tempMin;
    const int rangeSize = tempMax - tempMin;

    if (rangeSize >= 256)
    {
        const int randByte = (257 * sRandomBytes_546744[sRandomSeed_5D1E10]);
        sRandomSeed_5D1E10 += 2;
        result = result + randByte % (rangeSize + 1);
    }
    else
    {
        result += sRandomBytes_546744[sRandomSeed_5D1E10++] % (rangeSize + 1);
    }

    return result;
}
ALIVE_FUNC_IMPLEX(0x496AB0, Math_RandomRange_496AB0, Math_IMPL);