// get rid of math.h and write everything using intrinsics

#include "math.h"

internal_function inline u32 Truncate(r32 X)
{
	u32 Result = (u32)X;
	return Result;
}

internal_function inline s32 Round(r32 X)
{
	// read man roundf
	// this gets away from 0
	s32 Result = roundf(X);
	return Result;
}

internal_function inline s32 Floor(r32 X)
{
	s32 Result = floorf(X);
	return Result;
}

internal_function inline r32 Sin(r32 Angle)
{
	r32 Result = sinf(Angle);
	return Result;
}

internal_function inline r32 Cos(r32 Angle)
{
	r32 Result = cosf(Angle);
	return Result;
}

internal_function inline r32 Atan(r32 Angle)
{
	r32 Result = atanf(Angle);
	return Result;
}

internal_function inline r32 Abs(r32 X)
{
	r32 Result = fabsf(X);
	return Result;
}

internal_function inline s32 FirstSetBit(u32 X)
{
#if COMPILER_GCC
	s32	Result = __builtin_ffs((s32)X) - 1;
	return Result;
#else
	for (u8 Index = 0; Index < 32; ++Index) {
		if ((X & 0x1) == 1) {
			return Index;
		}
		else {
			X = X >> 1;
		}
	}
	return 0;
#endif
}
