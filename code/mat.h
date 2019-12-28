
typedef union {
	struct {
		r32 X;
		r32 Y;
	};
	r32 E[2];
} vec2;

typedef struct {
	r32 X;
	r32 Y;
	r32 Z;
} vec3;

typedef struct {
	u32 X;
	u32 Y;
	u32 Z;
} vec3_u32;

typedef struct {
	s32 X;
	s32 Y;
	s32 Z;
} vec3_s32;

typedef struct {
	u32 X;
	u32 Y;
} vec2_u32;

typedef struct {
	s32 X;
	s32 Y;
} vec2_s32;

internal_function bool Equal2(vec2 V, vec2 W)
{
	bool Result = (V.X == W.X) && (V.Y == W.Y);

	return Result;
}

internal_function bool Equal3(vec3 V, vec3 W)
{
	bool Result = ((V.X == W.X) &&
			(V.Y == W.Y) &&
			(V.Z == W.Z));

	return Result;
}

internal_function bool Equal_vec3_s32(vec3_s32 V, vec3_s32 W)
{
	bool Result = ((V.X == W.X) &&
			(V.Y == W.Y) &&
			(V.Z == W.Z));

	return Result;
}

#define Equal(V,W) _Generic((V), vec2 : Equal2, vec3 : Equal3, vec3_s32 : Equal_vec3_s32)(V,W)

internal_function vec2 Sum2(vec2 V, vec2 W)
{
	vec2 Result;
	Result.X = V.X + W.X;
	Result.Y = V.Y + W.Y;

	return Result;
}

internal_function vec3 Sum3(vec3 V, vec3 W)
{
	vec3 Result;
	Result.X = V.X + W.X;
	Result.Y = V.Y + W.Y;
	Result.Z = V.Z + W.Z;

	return Result;
}

#define Sum(V,W) _Generic((V), vec2 : Sum2, vec3 : Sum3)(V,W)

internal_function vec2 Scale2(r32 S, vec2 V)
{
	vec2 Result;
	Result.X = S * V.X;
	Result.Y = S * V.Y;

	return Result;
}

internal_function vec3 Scale3(r32 S, vec3 V)
{
	vec3 Result;
	Result.X = S * V.X;
	Result.Y = S * V.Y;
	Result.Z = S * V.Z;

	return Result;
}

#define Scale(s,V) _Generic((V), vec2 : Scale2, vec3 : Scale3)(s,V)

internal_function vec2 Minus2(vec2 V, vec2 W)
{
	vec2 Result;
	Result.X = V.X - W.X;
	Result.Y = V.Y - W.Y;

	return Result;
}


internal_function vec3 Minus3(vec3 V, vec3 W)
{
	vec3 Result;
	Result.X = V.X - W.X;
	Result.Y = V.Y - W.Y;
	Result.Z = V.Z - W.Z;

	return Result;
}

#define Minus(V,W) _Generic((V), vec2 : Minus2, vec3 : Minus3)(V,W)

internal_function r32 Sqr(r32 X)
{
	r32 Result = X*X;
	return Result;
}

internal_function r32 Inner(vec2 V, vec2 W)
{
	r32 Result = V.X * W.X + V.Y * W.Y;
	return Result;
}

#include "math.h"

inline internal_function s32 Ceiling(r32 X) 
{
    s32 Result = ceilf(X);
    return Result;
}

typedef struct {
    vec2 Center;
    vec2 Dim;
} rect_center_dim;

inline internal_function bool
IsInRect(vec2 Point, rect_center_dim Rect)
{
    bool Result = false;

    r32 MinX = Rect.Center.X - Rect.Dim.X/2;
    r32 MaxX = Rect.Center.X + Rect.Dim.X/2;
    r32 MinY = Rect.Center.Y - Rect.Dim.Y/2;
    r32 MaxY = Rect.Center.Y + Rect.Dim.Y/2;
    if ((Point.X >= MinX) &&
         (Point.X <= MaxX) &&   
         (Point.Y >= MinY) &&
         (Point.Y <= MaxY))
    {
        Result = true;
    }

    return Result;

}

