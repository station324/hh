#include "hm.h"
#include "int.h"
#include "ran.h"

internal_function vec2 Vec2(r32 X, r32 Y) 
{
	vec2 Result;
	Result.X = X;
	Result.Y = Y;
	return Result;
}

internal_function vec3 Vec3(r32 X, r32 Y, r32 Z) 
{
	vec3 Result;
	Result.X = X;
	Result.Y = Y;
	Result.Z = Z;
	return Result;
}

typedef struct 
{
	loaded_bitmap *Bitmap;
	vec2 Offset;
	r32 Opacity;
} render_piece;

typedef struct 
{
	s32 Count;
#define MAX_RENDER_PIECE_COUNT 8
	render_piece Pieces[MAX_RENDER_PIECE_COUNT];
} render_piece_group;

	internal_function void 
PushRenderPiece(render_piece_group *Group, 
		loaded_bitmap *Bitmap, 
		vec2 Offset, 
		vec2 Alignment, 
		r32 Opacity
	       ) 
{
	Assert(Group->Count < MAX_RENDER_PIECE_COUNT);

	render_piece Piece;
	Piece.Bitmap = Bitmap;
	Piece.Offset = Sum(Offset, Alignment);
	Piece.Opacity = Opacity;

	Group->Pieces[Group->Count++] = Piece;
}

	internal_function world_position 
AddWPos(world_position Pos, vec2 Move)
{
	world_position Result = Pos;

	Assert(Move.X <= CHUNKSIDE_IN_METER);
	Assert(Move.Y <= CHUNKSIDE_IN_METER);

	Result.Offset.X += Move.X;
	Result.Offset.Y += Move.Y;

	if (Result.Offset.X >= CHUNKSIDE_IN_METER)
	{
		Result.Offset.X -= CHUNKSIDE_IN_METER;
		Result.Chunk.X += 1;
	}
	else if (Result.Offset.X < 0)
	{
		Result.Offset.X += CHUNKSIDE_IN_METER;
		Result.Chunk.X -= 1;
	}

	if (Result.Offset.Y >= CHUNKSIDE_IN_METER)
	{
		Result.Offset.Y -= CHUNKSIDE_IN_METER;
		Result.Chunk.Y += 1;
	}
	else if (Result.Offset.Y < 0)
	{
		Result.Offset.Y += CHUNKSIDE_IN_METER;
		Result.Chunk.Y -= 1;
	}

	return Result;
}

	internal_function inline world_position 
AddWPosLarge(world_position Pos, vec2 Move)
{
	world_position Result = Pos;

	Result.Offset.X += Move.X;
	Result.Offset.Y += Move.Y;

	s32 DeltaX = Floor(Result.Offset.X / CHUNKSIDE_IN_METER);
	s32 DeltaY = Floor(Result.Offset.Y / CHUNKSIDE_IN_METER);

	Result.Chunk.X += DeltaX;
	Result.Chunk.Y += DeltaY;

	Result.Offset.X -= DeltaX * CHUNKSIDE_IN_METER;
	Result.Offset.Y -= DeltaY * CHUNKSIDE_IN_METER;

	return Result;
}

	internal_function loaded_bitmap 
DEBUGLoadPNG(thread_context *Thread, 
		debug_platform_read_entire_file *ReadEntireFile, 
		char *FileName
	    )
{
	loaded_bitmap Result = {};
	debug_read_file_result Contents = ReadEntireFile(Thread, FileName);

	if (Contents.Size != 0) {
		BMP_file_header	*FileHeader = (BMP_file_header *)Contents.Beginning; 
		Result.Width = FileHeader->Width;
		Result.Height = FileHeader->Height;
		Result.Beginning = (u32 *)((u8 *)FileHeader + FileHeader->BitmapOffset);


		// Since we are just able to load bmp files with compression mode 3
		Assert(FileHeader->Compression == 3);

		//: TODO:
		// this is to change the channel from RRGGBBAA to AARRGGBB
		// this whole thing works for the bitmap files that we have
		// carefully made. for other bitmap files it won't work
		// so you need to make it general
		// the red green and blue mask are determined by the 
		// header itself???(not really sure this is always the case)

		s32 RedShift = FirstSetBit(FileHeader->RedMask);
		s32 GreenShift = FirstSetBit(FileHeader->GreenMask);
		s32 BlueShift = FirstSetBit(FileHeader->BlueMask);
		s32 AlphaMask = ~(FileHeader->RedMask | FileHeader->GreenMask | FileHeader->BlueMask);
		s32 AlphaShift = FirstSetBit(AlphaMask);

		u32 *Pixel = Result.Beginning;
		for (int Y = 0; Y < Result.Height; ++Y) {
			for (int X = 0; X < Result.Width; ++X) {
				*Pixel = ((*Pixel >> AlphaShift) << 24) |
					((*Pixel >> RedShift)   << 16) |
					((*Pixel >> GreenShift) << 8)  |
					((*Pixel >> BlueShift)  << 0);
				++Pixel;
			}
		}
	}

	return Result;

}

	internal_function void 
OutputWave(u32 *Data, u16 Size, wave *Wave)
{
	u16 *SampleOut = (u16 *)(Data);		
	for (u16 i = 0; i < Size; ++i) {
		r32 t = 2.0f*pi32* (r32)Wave->Position/(r32)Wave->Period;
		s16 SampleValue = Wave->Value(t) * Wave->Volume;
#if 0
		*SampleOut++ = SampleValue;
		*SampleOut++ = SampleValue;
#else
		*SampleOut++ = 0;
		*SampleOut++ = 0;
#endif
		Wave->Position = CycleMove(Wave->Position, 1, Wave->Period);
	}
}



	internal_function void 
OutputGameSound(game_time *GameTime, 
		game_audio_buffer *Buffer, 
		wave *CurrentWave, wave *NextWave)
{
	u16 CurrentWaveRemaining = CurrentWave->Period - CurrentWave->Position;

	Buffer->Size = GameTime->Elapsed * (Buffer->Frequency / 1000); // game time is in miliseconds 
	if (Buffer->Size > CurrentWaveRemaining) {
		OutputWave(Buffer->Beginning, 
				CurrentWaveRemaining, 
				CurrentWave);
		OutputWave(Buffer->Beginning + CurrentWaveRemaining, 
				Buffer->Size - CurrentWaveRemaining, 
				NextWave);
		*CurrentWave = *NextWave;
	}
	else {
		OutputWave(Buffer->Beginning, Buffer->Size, CurrentWave);
	}

	// OutputWave(Buffer->Beginning, Buffer->Size, &CurrentWave);
}


	internal_function void 
DrawGradient(game_video_buffer *Buffer, 
		game_input *Input, s16 XOffset, 
		s16 YOffset, u16 PlayerX, u16 PlayerY) 
{
	s16 Height = Buffer->Height;
	s16 Width = Buffer->Width;
	u32 *Pixel   = (u32 *)Buffer->Beginning;
	for (u16 Y = 0; Y < Height; ++Y) {
		for (u16 X = 0; X < Width; ++X) {
			u8 Green = (u8)(Y + YOffset);
			u8 Blue = (u8)(X + XOffset);
			*Pixel = Green << 8 | Blue;
			++Pixel;
		}
	}
}

	inline internal_function s32 
RoundR32toS32(r32 X)
{
	// just for positive numbers
	return (s32)(X + 0.5f);
}

	internal_function void 
BlitBitmap(
		loaded_bitmap *Bitmap, 
		game_video_buffer *Buffer, 
		vec2 Loc,
		r32 Opacity
	  )
{ 
	s32 BitmapMinX = 0;
	s32 BitmapMaxX = Bitmap->Width - 1;
	s32 BufferMinX = RoundR32toS32(Loc.X);
	s32 BufferMaxX = BufferMinX + Bitmap->Width - 1;
	if (BufferMinX < 0) {
		BitmapMinX = -BufferMinX;
		BufferMinX = 0; 
	}
	if (BufferMaxX >= Buffer->Width) {
		BitmapMaxX -= (BufferMaxX - Buffer->Width);
		BufferMaxX = Buffer->Width;
	}

	s32 BitmapMinY = 0;
	s32 BitmapMaxY = Bitmap->Height - 1;
	s32 BufferMinY = RoundR32toS32(Loc.Y);
	s32 BufferMaxY = BufferMinY + Bitmap->Height - 1;
	if (BufferMinY < 0) {
		BitmapMinY = -BufferMinY;
		BufferMinY = 0; 
	}
	if (BufferMaxY >= Buffer->Height) {
		BitmapMaxY -= (BufferMaxY - Buffer->Height);
		BufferMaxY = Buffer->Height;
	}



	// it starts from the bottom left and goes right
	u32 *Dest = (u32 *)Buffer->Beginning;
	u32 *Source = (u32 *)Bitmap->Beginning;
	for (int Y = BufferMinY; Y <= BufferMaxY; ++Y) {
		for (int X = BufferMinX; X <= BufferMaxX; ++X) {
			int SourceX_tl = X - BufferMinX + BitmapMinX;
			int SourceY_tl = Y - BufferMinY + BitmapMinY;
			int SourcePos_ml = (Bitmap->Height - 1 - SourceY_tl)*Bitmap->Width + SourceX_tl;

			u32 SourceColor = Source[SourcePos_ml];
			u32 DestColor = Dest[X + Y*Buffer->Width];

			r32 Alpha = (r32)((SourceColor >> 24) / 255.0f) * Opacity;

			u8  SourceRed = (SourceColor >> 16) & 0xff;
			u8  DestRed = (DestColor >> 16) & 0xff;
			u8  NewRed = (u8)(Alpha*SourceRed  + (1.0f - Alpha)*DestRed);

			u8  SourceGreen = (SourceColor >> 8) & 0xff;
			u8  DestGreen = (DestColor >> 8) & 0xff;
			u8  NewGreen = (u8)(Alpha*SourceGreen  + (1.0f - Alpha)*DestGreen);

			u8  SourceBlue = SourceColor & 0xff;
			u8  DestBlue = DestColor & 0xff;
			u8  NewBlue = (u8)(Alpha*SourceBlue  + (1.0f - Alpha)*DestBlue);

			Dest[X + Y*Buffer->Width] = (NewRed << 16) | (NewGreen << 8) | (NewBlue);
		}
	}
}


	internal_function void 
DrawRectangle(game_video_buffer *Buffer, 
		vec2 Min, vec2 Max,
		r32 Red, r32 Green, r32 Blue) 
{
	if (Min.X < 0) {
		Min.X = 0;
	}

	if (Min.Y < 0) {
		Min.Y = 0;
	}

	if (Max.X > Buffer->Width) {
		Max.X = Buffer->Width;
	}

	if (Max.Y > Buffer->Height) {
		Max.Y = Buffer->Height;
	}

	s32 RoundMinY = RoundR32toS32(Min.Y);
	s32 RoundMaxY = RoundR32toS32(Max.Y);
	s32 RoundMinX = RoundR32toS32(Min.X);
	s32 RoundMaxX = RoundR32toS32(Max.X);

	u32 Color = RoundR32toS32(Red * 255.0f) << 16 | 
		RoundR32toS32(Green * 255.0f) << 8 | 
		RoundR32toS32(Blue * 255.0f);

	for (int Y = RoundMinY; Y < RoundMaxY; ++Y) {
		u32 *Pixel = (u32 *)Buffer->Beginning + Y*Buffer->Width + RoundMinX;
		for (int X = RoundMinX; X < RoundMaxX; ++X) {
			*Pixel++ = Color;
		}
	}
}

enum tile_content {
	TileContent_Nothing, TileContent_Memory, TileContent_Empty, TileContent_Wall, TileContent_DoorUp, TileContent_DoorDown,
} TileContent;

enum direction {
	Direction_Left, Direction_Back, Direction_Right, Direction_Front, 
} Direction;

internal_function inline r32 DistanceSq(vec2 A, vec2 B)
{
	r32 Result = Inner(Minus(A,B), Minus(A,B));
	return Result;

}

internal_function bool CollideWithLine(vec2 StartSpeed, r32 *MaxTime, vec2 *EndSpeed, vec2 WallStart, vec2 WallEnd)
{
	// wall tangents
	vec2 WallTangent = Minus(WallEnd, WallStart);
	r32 WallLengthSquared = Inner(WallTangent, WallTangent);
	r32 WallLength = sqrt(WallLengthSquared);
	vec2 UnitaryTangent = Scale(1.0f/WallLength, WallTangent);

	// the wall turns counter clockwise around the blocked area
	vec2 WallNormal;
	WallNormal.Y = -UnitaryTangent.X;
	WallNormal.X = UnitaryTangent.Y;

	r32 SpeedToWall = -Inner(StartSpeed, WallNormal);
	if (SpeedToWall <= 0) {
		return false;
	}
	else {
		r32 Distance = -Inner(WallStart, WallNormal);
		r32 Time = Distance / SpeedToWall;
		if ((Time >= *MaxTime) || (Time < 0)) {
			return false;
		}
		else {
			vec2 CollisionPoint = Scale(Time, StartSpeed);
			vec2 CollisionVector = Minus(CollisionPoint, WallStart);
			r32 CollisionProjection = Inner(CollisionVector, UnitaryTangent); 
			if ((CollisionProjection <= 0) || (CollisionProjection >= WallLength)) {
				return false;
			}
			else {
				r32 MoveTimeEpsilon = 0.0001f;
				if (Time > MoveTimeEpsilon) {
					*MaxTime = Time - MoveTimeEpsilon;
				}
				else {
					*MaxTime = 0;
				}
				*EndSpeed = Scale(Inner(UnitaryTangent, StartSpeed), UnitaryTangent);
				return true;
			}
		}
	}
}

	internal_function void
MakeEntityHighWithPos(game_state *GameState, vec2 Pos, s32 LowIndex)
{
	low_entity *EntityLow = GameState->LowEntities + LowIndex;

	Assert(EntityLow->HighIndex == 0);
	if (EntityLow->HighIndex == 0)
	{
		if (GameState->HighEntityCount < MAX_HIGH_ENTITIES - 1) 
		{ 
			++GameState->HighEntityCount;
			s32 HighIndex = GameState->HighEntityCount;
			EntityLow->HighIndex = HighIndex;
			high_entity *EntityHigh = GameState->HighEntities_ + HighIndex;

			EntityHigh->P = Pos;
			EntityHigh->Z = EntityLow->P.Chunk.Z;

			EntityHigh->dP = Vec2(0,0);
			EntityHigh->Face = Direction_Front;

			EntityHigh->LowIndex = LowIndex;
		}
		else 
		{
			InvalidGamePath("Can't create more high entities\n");
		}
	}
	else
	{
		// do nothing
	}
}

	internal_function void 
MakeEntityHigh(game_state *GameState, s32 LowIndex)
{
	low_entity *EntityLow = GameState->LowEntities + LowIndex;
	if (EntityLow->HighIndex != 0)
	{
		// do nothing
		printf("entity is already high\n");
	}
	else
	{
		vec2 Pos = WPosDiff(EntityLow->P, GameState->HighRectCenter);
		MakeEntityHighWithPos(GameState, Pos, LowIndex);
	}
}

	internal_function void
MakeEntityLow(game_state *GameState, world_info *WorldInfo, 
		s32 LowIndex)
{
	low_entity *EntityLow = &GameState->LowEntities[LowIndex];

	s32 LastHighIndex = GameState->HighEntityCount;
	if (LastHighIndex == EntityLow->HighIndex) {
		// do nothing
	}
	else {
		high_entity *LastEntityHigh = &GameState->HighEntities_[LastHighIndex];
		low_entity *LastEntityLow = &GameState->LowEntities[LastEntityHigh->LowIndex];
		high_entity *DeletedHigh = &GameState->HighEntities_[EntityLow->HighIndex];

		*DeletedHigh = *LastEntityHigh;
		LastEntityLow->HighIndex = EntityLow->HighIndex;
	}

	--GameState->HighEntityCount;
	EntityLow->HighIndex = 0;
}

	inline internal_function low_entity *
GetLowEntity(game_state *GameState, s32 LowIndex)
{
	low_entity *Result = 0;

	if ((LowIndex > 0) && (LowIndex <= GameState->LowEntityCount)) {
		Result = GameState->LowEntities + LowIndex;
	}

	return Result;
}

	inline internal_function high_entity *
GetHighEntity(game_state *GameState, s32 HighIndex)
{
	Assert(HighIndex <= GameState->HighEntityCount);
	high_entity *Result = 0;
	if ((HighIndex > 0) && (HighIndex <= GameState->HighEntityCount)) {
		Result = GameState->HighEntities_ + HighIndex;
	}

	return Result;
}

	inline internal_function entity
GetEntityFromHighIndex(game_state *GameState, s32 I)
{
	entity Result = {};
	if (I != 0) {
		Result.High = GetHighEntity(GameState, I);
		Result.Low = GetLowEntity(GameState, Result.High->LowIndex);
	}
	return Result;
}

	internal_function world_position
TileToWPos(s32 TileX, s32 TileY, s32 TileZ)
{
	world_position Result;

	Result.Chunk.Z = TileZ;

	Result.Chunk.X = TileX >> CHUNKSHIFT;
	Result.Chunk.Y = TileY >> CHUNKSHIFT;

	Result.Offset.X = TILESIDE_IN_METER * (TileX & CHUNKMASK);
	Result.Offset.Y = TILESIDE_IN_METER * (TileY & CHUNKMASK);

	return Result;
}


	internal_function s32
AddLowEntity(game_state *GameState, entity_type EntityType, 
		s32 TileX, s32 TileY, s32 TileZ, 
		memory_arena *Arena)
{

#define  TILEMAP_SAFE_MARGIN 256

	Assert(TileX > (s32)MIN_S32 + TILEMAP_SAFE_MARGIN);
	Assert(TileY > (s32)MIN_S32 + TILEMAP_SAFE_MARGIN);
	Assert(TileZ > (s32)MIN_S32 + TILEMAP_SAFE_MARGIN);

	Assert(TileX < MAX_U32/2 - TILEMAP_SAFE_MARGIN);
	Assert(TileY < MAX_U32/2 - TILEMAP_SAFE_MARGIN);
	Assert(TileZ < MAX_U32/2 - TILEMAP_SAFE_MARGIN);

	Assert(GameState->LowEntityCount < MAX_LOW_ENTITY - 1);
	++GameState->LowEntityCount;
	s32 Result = GameState->LowEntityCount;

	low_entity *EntityLow = GetLowEntity(GameState, Result);

	EntityLow->Type = EntityType;
	EntityLow->P = TileToWPos(TileX, TileY, TileZ);

	if (EntityType == EntityType_Wall) {
		EntityLow->DimInMeters.X = 1.4f;
		EntityLow->DimInMeters.Y = 1.4f;
		EntityLow->Collides = true;
	}
	else if (EntityType == EntityType_Player) {
		EntityLow->DimInMeters.X = 1.0f;
		EntityLow->DimInMeters.Y = 0.4f;
		EntityLow->Collides = true;
	}
	else if (EntityType == EntityType_Familiar) {
		EntityLow->DimInMeters.X = 1.0f;
		EntityLow->DimInMeters.Y = 0.4f;
		EntityLow->Collides = false;
	}
	else if (EntityType == EntityType_Monster) {
		EntityLow->DimInMeters.X = 1.0f;
		EntityLow->DimInMeters.Y = 0.4f;
		EntityLow->Collides = true;
	}

	// add the index to the chunk
	// that the entity is in
	AddEntityToChunk(&GameState->World, Result, &EntityLow->P, Arena);

	return Result;
}

	internal_function void
UpdateEntityResidenceAndHighRect(game_state *GameState, vec2 EntityOffset)
{
	GameState->HighRectCenter = AddWPosLarge(GameState->HighRectCenter, Scale(-1, EntityOffset));

	// TODO: you can combine the low and high checks to get faster
	// but now i am trying to keep things simple
	rect_center_dim HighRect;
	HighRect.Center = Vec2(0.0f, 0.0f);
	HighRect.Dim = GameState->HighRectDim;
	for (s32 HighIndex = 1; 
			HighIndex <= GameState->HighEntityCount;
			++HighIndex) 
	{
		high_entity *EntityHigh = GameState->HighEntities_ + HighIndex;
		EntityHigh->P = Sum(EntityHigh->P, EntityOffset);

		if ((IsInRect(EntityHigh->P, HighRect)) &&
				(GameState->HighRectCenter.Chunk.Z == EntityHigh->Z)) 
		{
			// do nothing
		}
		else 
		{
			low_entity *EntityLow = GameState->LowEntities + EntityHigh->LowIndex;
			EntityLow->HighIndex = 0;

			s32 LastHighIndex = GameState->HighEntityCount;
			if (LastHighIndex == HighIndex) 
			{
				// do nothing
			}
			else 
			{
				high_entity *LastEntityHigh = GameState->HighEntities_ + LastHighIndex;
				*EntityHigh = *LastEntityHigh;

				low_entity *LastEntityLow = GameState->LowEntities + LastEntityHigh->LowIndex;
				LastEntityLow->HighIndex = HighIndex;
			}

			--GameState->HighEntityCount;
			--HighIndex;
		}
	}

	// move low to high
	world_position MinCorner = AddWPosLarge(GameState->HighRectCenter, 
			Scale(-0.5f, GameState->HighRectDim));
	world_position MaxCorner = AddWPosLarge(GameState->HighRectCenter, 
			Scale(+0.5f, GameState->HighRectDim));

	for (s32 ChunkY = MinCorner.Chunk.Y; 
			ChunkY <= MaxCorner.Chunk.Y;
			++ChunkY) 
	{
		for (s32 ChunkX = MinCorner.Chunk.X; 
				ChunkX <= MaxCorner.Chunk.X;
				++ChunkX) 
		{
			world_chunk *Chunk = GetWorldChunk(&GameState->World, (vec3_s32){ChunkX, ChunkY, GameState->HighRectCenter.Chunk.Z}, (memory_arena *)0);
			if (Chunk)
			{
				for (entity_block *Block = &Chunk->First;
						Block;
						Block = Block->NextBlock)
				{
					for (int Index = 0;
							Index < Block->Count;
							++Index)
					{
						s32 LowIndex = Block->EntityIndex[Index];
						low_entity *EntityLow = GetLowEntity(GameState, LowIndex);
						if (EntityLow->HighIndex != 0) 
						{
							// do nothing
						}
						else 
						{
							rect_center_dim HighRect;
							HighRect.Center = Vec2(0,0);
							HighRect.Dim = GameState->HighRectDim;

							vec2 Diff = WPosDiff(EntityLow->P, GameState->HighRectCenter);
							if (IsInRect(Diff, HighRect)) 
							{
								MakeEntityHighWithPos(GameState, Diff, LowIndex);
							}
							else {
								// do nothing
							}
						}
					}
				}
			}
		}
	}
	printf("Low Entity Count: %d , High Entity Count: %d\n", GameState->LowEntityCount, GameState->HighEntityCount);
}

GAME_INITIALIZE_STATE(GameInitializeState)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;
	world_info *WorldInfo = &GameState->WorldInfo;

	WorldInfo->TileSideInMeters = 1.4f;
	WorldInfo->TileSideInPixels = 60;
	WorldInfo->MetersToPixels = 
		(r32)GameState->WorldInfo.TileSideInPixels / 
		(r32)GameState->WorldInfo.TileSideInMeters;
	WorldInfo->MapHeightInTiles = 9;
	WorldInfo->MapWidthInTiles = 17;

	GameState->World.FirstFreeBlock = 0;

	GameState->BackGroundBitMap = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/grass.bmp");
	GameState->HeroHeadBitmap.Right = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-right-head.bmp");
	GameState->HeroHeadBitmap.Left = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-left-head.bmp");
	GameState->HeroHeadBitmap.Front = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-front-head.bmp");
	GameState->HeroHeadBitmap.Back = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-back-head.bmp");
	GameState->HeroTorsoBitmap.Right = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-right-torso.bmp");
	GameState->HeroTorsoBitmap.Left = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-left-torso.bmp");
	GameState->HeroTorsoBitmap.Front = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-front-torso.bmp");
	GameState->HeroTorsoBitmap.Back = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-back-torso.bmp");
	GameState->HeroLegsBitmap.Right = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-right-leg.bmp");
	GameState->HeroLegsBitmap.Left = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-left-leg.bmp");
	GameState->HeroLegsBitmap.Front = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-front-leg.bmp");
	GameState->HeroLegsBitmap.Back = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-back-leg.bmp");
	GameState->HeroShadowBitmap = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/hero-shadow.bmp");
	GameState->TreeBitmap = DEBUGLoadPNG(Thread, Memory->DEBUGPlatformReadEntireFile, "../data/tree.bmp");

	// by default all controllers are
	// associated with player 0.
	// it means they are not assigned
	for (s32 i = 0; i < MAX_INPUT_DEVICE; ++i) {
		GameState->PlayerIndexOfController[i] = 0;
	}

	GameState->LowEntityCount = 0;
	GameState->HighEntityCount = 0;
	// Player 0 doesn't exist.

	GameState->HeroPosInBitmap.X = 19;
	GameState->HeroPosInBitmap.Y = 51;

	GameState->TreePosInBitmap.X = 30;
	GameState->TreePosInBitmap.Y = 60;

	GameState->CameraFollowingPlayer = 0;

	GameState->CenterScreen = (vec3_s32){0, 0, 0};

	GameState->HighRectCenter = TileToWPos(
			GameState->CenterScreen.X * WorldInfo->MapWidthInTiles + WorldInfo->MapWidthInTiles / 2, 
			GameState->CenterScreen.Y * WorldInfo->MapHeightInTiles + WorldInfo->MapHeightInTiles / 2, 
			GameState->CenterScreen.Z);

	GameState->HighRectCenter = AddWPosLarge(GameState->HighRectCenter, Vec2(0.5f * TILESIDE_IN_METER , 0.5f * TILESIDE_IN_METER));

	// TODO(ramin): must be 3*... but that makes us crash because
	// we need to change the tiles system to use s32 instead of s32
	GameState->HighRectDimTiles.X = 3*WorldInfo->MapWidthInTiles;
	GameState->HighRectDimTiles.Y = 3*WorldInfo->MapHeightInTiles;
	GameState->HighRectDimTiles.Z = 1;

	GameState->HighRectDim = Vec2( GameState->HighRectDimTiles.X*WorldInfo->TileSideInMeters, GameState->HighRectDimTiles.Y*WorldInfo->TileSideInMeters);

	GameState->TileArena.Size = Memory->PermanentStorageSize - sizeof(game_state);
	GameState->TileArena.Used = 0;
	GameState->TileArena.Base = (u8 *)Memory->PermanentStorage + sizeof(game_state);


	// we need to make chunks zero because we might restart 
	for (int ChunkIndex = 0; 
			ChunkIndex < TILE_CHUNK_HASH_SIZE; 
			++ChunkIndex ) 
	{
		GameState->World.TileChunkInHash[ChunkIndex].X = MIN_S32;
		GameState->World.TileChunkInHash[ChunkIndex].NextInHash = 0;
		GameState->World.TileChunkInHash[ChunkIndex].First.Count = 0;
		GameState->World.TileChunkInHash[ChunkIndex].First.NextBlock = 0;
	}

	s32 ScreenX = GameState->CenterScreen.X;
	s32 ScreenY = GameState->CenterScreen.Y;
	s32 ScreenZ = GameState->CenterScreen.Z;

	bool DoorLeft = false;
	bool DoorRight = false;
	bool DoorTop = false;
	bool DoorBottom = false;
	bool DoorUp = false;
	bool DoorDown = false;
	for (s32 ScreenIndex = 0; ScreenIndex < 2000; ++ScreenIndex) {

		s32 RandomChoice;
		if (DoorUp || DoorDown) {
			RandomChoice = RandomNumberTable[ScreenIndex] % 2;
		}
		else {
			// TODO: make it 3 so that we have up and down doors as well
			RandomChoice = RandomNumberTable[ScreenIndex] % 2;
		}

		if (RandomChoice == 0) {
			DoorRight = true;
		}
		else if (RandomChoice == 1) {
			DoorTop = true;
		}
		else if (RandomChoice == 2) {
			if (ScreenZ == GameState->CenterScreen.Z) {
				DoorUp = true;
			}
			else {
				DoorDown = true;
			}
		}

		for (s32 Row = 0; Row < GameState->WorldInfo.MapHeightInTiles; ++Row) {
			for (s32 Column = 0; Column < GameState->WorldInfo.MapWidthInTiles; ++Column) {
				s32 TileValue = TileContent_Empty;
				if (
						((Row == 0) && (!DoorBottom || (Column != GameState->WorldInfo.MapWidthInTiles/2))) || 
						((Row == GameState->WorldInfo.MapHeightInTiles - 1) && (!DoorTop || (Column != GameState->WorldInfo.MapWidthInTiles/2))) ||
						((Column == 0) && (!DoorLeft || (Row != GameState->WorldInfo.MapHeightInTiles/2))) ||
						((Column == GameState->WorldInfo.MapWidthInTiles - 1) && (!DoorRight || (Row != GameState->WorldInfo.MapHeightInTiles/2)))
				   )
				{
					TileValue = TileContent_Wall;
				}

				if ((Row == 5) && (Column == 5) && (DoorUp)) {
					TileValue = TileContent_DoorUp;
				}

				if ((Row == 5) && (Column == 5) && (DoorDown)) {
					TileValue = TileContent_DoorDown;
				}

				s32 AbsTileX = ScreenX * GameState->WorldInfo.MapWidthInTiles + Column;
				s32 AbsTileY = ScreenY * GameState->WorldInfo.MapHeightInTiles + Row;
				s32 AbsTileZ = ScreenZ;

				if (TileValue == TileContent_Wall) 
				{
					AddLowEntity(GameState, EntityType_Wall,
							AbsTileX, AbsTileY, AbsTileZ,
							&GameState->TileArena);
				}
			}
		}

		DoorLeft = false;
		DoorRight = false;
		DoorTop = false;
		DoorBottom = false;
		DoorUp = false;
		DoorDown = false;

		if (RandomChoice == 0) {
			DoorLeft = true;
			++ScreenX;
		}
		else if (RandomChoice == 1) {
			DoorBottom = true;
			++ScreenY;
		}
		else if (RandomChoice == 2) {
			if (ScreenZ == GameState->CenterScreen.Z) {
				DoorDown = true;
				ScreenZ = GameState->CenterScreen.Z + 1;
			}
			else {
				DoorUp = true;
				ScreenZ = GameState->CenterScreen.Z;
			}
		}
	}

	AddLowEntity(GameState, EntityType_Monster, 
			GameState->CenterScreen.X * GameState->WorldInfo.MapWidthInTiles + 6, 
			GameState->CenterScreen.Y * GameState->WorldInfo.MapHeightInTiles + 3, 
			GameState->CenterScreen.Z,
			&GameState->TileArena);
	AddLowEntity(GameState, EntityType_Familiar, 
			GameState->CenterScreen.X * GameState->WorldInfo.MapWidthInTiles + 2, 
			GameState->CenterScreen.Y * GameState->WorldInfo.MapHeightInTiles + 3, 
			GameState->CenterScreen.Z,
			&GameState->TileArena);

	UpdateEntityResidenceAndHighRect(GameState, Vec2(0.0f, 0.0f));
}

internal_function void 
MoveEntity(game_state *GameState, s32 idx, vec2 ddP, r32 dT) // idx the entity index
{
	entity Ent = GetEntityFromHighIndex(GameState, idx);
	// speed
	Ent.High->dP = Sum(Scale(0.8f, Ent.High->dP), 
			Scale(dT, ddP));  


	// determine the direction the player is facing
	// based on his velocity
	// TODO: make this based on acceleration
	if (Abs(Ent.High->dP.X) > Abs(Ent.High->dP.Y)) {
		if (Ent.High->dP.X > 0) {
			Ent.High->Face = Direction_Left;
		}
		else {
			Ent.High->Face = Direction_Right;
		}
	}
	else {
		if (Ent.High->dP.Y > 0) {
			Ent.High->Face = Direction_Back;
		}
		else {
			Ent.High->Face = Direction_Front;
		}
	}


	// collision detection
	r32 RemainingT = dT;
	r32 BestT = dT;
	vec2 nV = Ent.High->dP; // next velocity
	s32 S = 0; // selected entity index
	for (int i = 0; i < 4; ++i) { // i = iteration
		BestT = RemainingT;
		if (Ent.Low->Collides) {
			for (int j = 1; j <= GameState->HighEntityCount; ++j) {
				if (j != idx) {
					entity T = GetEntityFromHighIndex(GameState, j);
					if (T.Low->Collides) {

						// corners are chosen to include the minkowski sum
						r32 MinX = T.High->P.X - Ent.High->P.X - T.Low->DimInMeters.X/2 - Ent.Low->DimInMeters.X/2;
						r32 MaxX = T.High->P.X - Ent.High->P.X + T.Low->DimInMeters.X/2 + Ent.Low->DimInMeters.X/2;
						r32 MinY = T.High->P.Y - Ent.High->P.Y - T.Low->DimInMeters.Y/2 - Ent.Low->DimInMeters.Y/2;
						r32 MaxY = T.High->P.Y - Ent.High->P.Y + T.Low->DimInMeters.Y/2 + Ent.Low->DimInMeters.Y/2;

						// top wall
						if (CollideWithLine(Ent.High->dP, &BestT, &nV, Vec2(MaxX, MaxY), Vec2(MinX, MaxY))) {
							S = j; 
						}

						// bottom wall
						if (CollideWithLine(Ent.High->dP, &BestT, &nV, Vec2(MinX, MinY), Vec2(MaxX, MinY))) {
							S = j; 
						}

						// left wall
						if (CollideWithLine(Ent.High->dP, &BestT, &nV, Vec2(MinX, MaxY), Vec2(MinX, MinY))) {
							S = j; 
						}

						// right wall
						if (CollideWithLine(Ent.High->dP, &BestT, &nV, Vec2(MaxX, MinY), Vec2(MaxX, MaxY))) {
							S = j; 
						}

					}
				}
			}
		}

		// update position based on BestT
		vec2 PartialMovement; 
		PartialMovement = Scale(BestT , Ent.High->dP);

		Ent.High->P = Sum(Ent.High->P, PartialMovement);
		RemainingT -= BestT;
		Ent.High->dP = nV;

		if (S != 0) {
			// stairs
			entity E = GetEntityFromHighIndex(GameState, S);
			Ent.High->Z += E.Low->dZ;
		}

		else {
			break;
		}
	}
}

inline internal_function void
UpdateFamiliar(game_state *GameState, s32 idx, r32 dT)
{
	entity Fml = GetEntityFromHighIndex(GameState, idx);
	entity S = {}; // the closest player to the familiar
	r32 Sd = 10.0 * 10.0; // we use square of the distance 
	for (int I = 1; I <= GameState->HighEntityCount; ++I) {
		entity T = GetEntityFromHighIndex(GameState, I);
		if (T.Low->Type == EntityType_Player) {
			r32 Td =  DistanceSq(T.High->P, Fml.High->P);
			if (Td < Sd) {
				S = T;
				Sd = Td;
			}
		}
	}
	vec2 Acc = {};
	if ((S.High != 0) && (Sd > 2.0 * 2.0)) {
		// move it
		r32 AccMag = 70.0;
		vec2 AccDir = Minus(S.High->P, Fml.High->P);
		vec2 AccUdr = Scale( 1.0 / sqrt(Sd), AccDir); 
		Acc = Scale(AccMag, AccUdr);
	}
	// TODO(RAMIN): wrong, we don't check if the familiar collides or not
	MoveEntity(GameState, idx, Acc, dT);
}

GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	game_state *GameState = (game_state *)Memory->PermanentStorage;

	world_info *WorldInfo = &GameState->WorldInfo;

	// get input and update state
	for (int ControllerIndex = 0; ControllerIndex < MAX_INPUT_DEVICE; ++ControllerIndex) 
	{
		game_controller_state Controller = Input->InputDevice[ControllerIndex];
		s32 PlayerIndex = GameState->PlayerIndexOfController[ControllerIndex];
		if (PlayerIndex != 0) 
		{

			low_entity *PlayerLow = GetLowEntity(GameState, PlayerIndex);
			high_entity *PlayerHigh = GetHighEntity(GameState, PlayerLow->HighIndex);

			r32 AccMagnitude = 100; // Acceleration magnitude m / s^2
			if (Controller.ButtonA.EndedDown) {
				AccMagnitude = 200;
			}

			// jump
			r32 ddZ = -9.8;
			if (Controller.ButtonX.EndedDown) {
				PlayerHigh->dZ = 100;
			}
			PlayerHigh->dZ += ddZ;
			PlayerHigh->Zr += GameTime->Elapsed*PlayerHigh->dZ;
			if (PlayerHigh->Zr < 0) {
				PlayerHigh->Zr = 0;
			}

			// determine player acceleration for players using the joystick
			// or directional buttons
			vec2 DirectionInput;
			if (Controller.Analog && ((Controller.EndX != 0) || (Controller.EndY != 0))) {
				DirectionInput.X = Controller.EndX; 
				DirectionInput.Y = -Controller.EndY;
			}
			else {
				DirectionInput.X = Controller.Right.EndedDown - Controller.Left.EndedDown; 
				DirectionInput.Y = Controller.Up.EndedDown - Controller.Down.EndedDown;
			}

			// normalize AccDirection
			r32 InputMagnitude = sqrt(Inner(DirectionInput, DirectionInput));
			vec2 Acc = {};
			if (InputMagnitude <= 1) {
				Acc = Scale(AccMagnitude, DirectionInput);
			}
			else {
				Acc = Scale(AccMagnitude/InputMagnitude, DirectionInput);
			}


			// TODO(ramin): make it automatic so that 
			// we don't need to remember everytime we change a high
			// entity to update its low part as well

			MoveEntity(GameState, PlayerLow->HighIndex, Acc, GameTime->Elapsed);

			// TODO: bundle these together
			world_position NewP = AddWPos(GameState->HighRectCenter, PlayerHigh->P);
			ChangeEntityLocation(GameState, PlayerIndex, &NewP, &GameState->TileArena);
		}
		else 
		{
			if (Controller.ButtonB.EndedDown) {
				// Assert(); // we have players left
				PlayerIndex = AddLowEntity(GameState, 
						EntityType_Player, 
						GameState->CenterScreen.X * GameState->WorldInfo.MapWidthInTiles + 3, 
						GameState->CenterScreen.Y * GameState->WorldInfo.MapHeightInTiles + 3, 
						GameState->CenterScreen.Z,
						&GameState->TileArena);
				MakeEntityHigh(GameState, PlayerIndex);
				GameState->CameraFollowingPlayer = PlayerIndex;
				GameState->PlayerIndexOfController[ControllerIndex] = PlayerIndex;

			}
		}

	} // end of controllers loop

	{ // update HighRectCenter
		vec2 EntityOffset = Vec2(0.0f, 0.0f);
		if (GameState->CameraFollowingPlayer != 0) {
			low_entity *PlayerLow = GetLowEntity(GameState, GameState->CameraFollowingPlayer);
			// high_entity *PlayerHigh = GetHighEntity(GameState, PlayerLow->HighIndex);

			vec2 PlayerPosInHighRect = WPosDiff(PlayerLow->P, GameState->HighRectCenter);


			if (PlayerPosInHighRect.X > GameState->WorldInfo.MapWidthInTiles*GameState->WorldInfo.TileSideInMeters/2) 
			{
				EntityOffset.X = -GameState->WorldInfo.MapWidthInTiles * WorldInfo->TileSideInMeters;
			}
			else if (PlayerPosInHighRect.X < -GameState->WorldInfo.MapWidthInTiles*GameState->WorldInfo.TileSideInMeters/2)
			{
				EntityOffset.X = GameState->WorldInfo.MapWidthInTiles * WorldInfo->TileSideInMeters;
			}

			if (PlayerPosInHighRect.Y > GameState->WorldInfo.MapHeightInTiles*GameState->WorldInfo.TileSideInMeters/2) 
			{
				EntityOffset.Y = -GameState->WorldInfo.MapHeightInTiles * WorldInfo->TileSideInMeters;
			}
			else if (PlayerPosInHighRect.Y < -GameState->WorldInfo.MapHeightInTiles*GameState->WorldInfo.TileSideInMeters/2)
			{
				EntityOffset.Y = GameState->WorldInfo.MapHeightInTiles * WorldInfo->TileSideInMeters;
			}


			if ((EntityOffset.X != 0.0f) || (EntityOffset.Y != 0.0f))
			{
				UpdateEntityResidenceAndHighRect(GameState, EntityOffset);
			}
		}
	}


	// draw the pink background
	vec2 UpperLeft = {{0, 0}};
	vec2 LowerRight = {{(r32)VideoBuffer->Width, (r32)VideoBuffer->Height}};
	DrawRectangle(VideoBuffer, UpperLeft, LowerRight, 0.7f, 0.7f, 0.7f);

	// BlitBitmap(&GameState->BackGroundBitMap, VideoBuffer, UpperLeft);

	{  // draw entities
		vec2 HighRectInCameraInTiles = Vec2((r32)WorldInfo->MapWidthInTiles/2, (r32)WorldInfo->MapHeightInTiles/2 - 0.5f);
		vec2 O = Scale(WorldInfo->TileSideInPixels, HighRectInCameraInTiles);
		vec2 I = {{WorldInfo->MetersToPixels, 0}}; // vector-i of CS in Pixel coordinate
		vec2 J = {{0, -WorldInfo->MetersToPixels}}; // vector-j of CS in Pixel coordinate

		render_piece_group RenderPieceGroup;
		for (s32 i = 1; i <= GameState->HighEntityCount; ++i) {
			RenderPieceGroup.Count = 0;
			high_entity *EntityHigh = GetHighEntity(GameState, i);
			low_entity *EntityLow = GetLowEntity(GameState, EntityHigh->LowIndex);

			vec2 V = EntityHigh->P; // V is the coordinates of the vector in the OIJ system
			vec2 D = EntityLow->DimInMeters; // the height and width of the entity

			// NOTE: DON'T CHANGE THESE, CHANGE ABOVE
			vec2 EntityPos = Sum(O, Sum(Scale(V.X, I), Scale(V.Y, J)));
			vec2 EntityDim = Scale(WorldInfo->MetersToPixels, D);
			vec2 PosToMin = Scale(-0.5f, EntityDim);
			vec2 EntityMin = Sum(EntityPos, PosToMin);
			vec2 EntityMax = Sum(EntityMin, EntityDim);

			switch (EntityLow->Type) {
				case EntityType_Player: {
					//DrawRectangle(VideoBuffer, EntityMin, EntityMax, 1.0f, 1.0f, 0.0f);
					r32 ShadowOpacity = 1.0 - 0.02*EntityHigh->Zr*WorldInfo->TileSideInMeters;
					if (ShadowOpacity < 0) {
						ShadowOpacity  = 0;
					}

					PushRenderPiece(&RenderPieceGroup, 
							&GameState->HeroShadowBitmap, 
							Vec2(0.0, 0.0), 
							Vec2(EntityPos.X-GameState->HeroPosInBitmap.X ,
								EntityPos.Y -GameState->HeroPosInBitmap.Y), 
							ShadowOpacity);
					PushRenderPiece(&RenderPieceGroup, 
							&GameState->HeroLegsBitmap.Direction[EntityHigh->Face],
							Vec2(0.0, 0.0), 
							Vec2(EntityPos.X-GameState->HeroPosInBitmap.X , 
								EntityPos.Y 
								- EntityHigh->Zr*WorldInfo->TileSideInMeters 
								- GameState->HeroPosInBitmap.Y), 
							1.0);
					PushRenderPiece(&RenderPieceGroup, 
							&GameState->HeroTorsoBitmap.Direction[EntityHigh->Face],
							Vec2(0.0, 0.0), 
							Vec2(EntityPos.X-GameState->HeroPosInBitmap.X ,
								EntityPos.Y - EntityHigh->Zr*WorldInfo->TileSideInMeters -GameState->HeroPosInBitmap.Y), 
							1.0);
					PushRenderPiece(&RenderPieceGroup, &GameState->HeroHeadBitmap.Direction[EntityHigh->Face], Vec2(0.0, 0.0), Vec2(EntityPos.X-GameState->HeroPosInBitmap.X , EntityPos.Y - EntityHigh->Zr*WorldInfo->TileSideInMeters -GameState->HeroPosInBitmap.Y), 1.0);
				} break;
				case EntityType_Wall: {
					// DrawRectangle(VideoBuffer, EntityMin, EntityMax, 1.0f, 1.0f, 1.0f);
					PushRenderPiece(&RenderPieceGroup, &GameState->TreeBitmap, Vec2(0.0, 0.0), Vec2(EntityPos.X-GameState->TreePosInBitmap.X , EntityPos.Y - EntityHigh->Zr*WorldInfo->TileSideInMeters -GameState->TreePosInBitmap.Y), 1.0);
				} break;
				case EntityType_Monster: {
					DrawRectangle(VideoBuffer, EntityMin, EntityMax, 1.0f, 1.0f, 1.0f);
					PushRenderPiece(&RenderPieceGroup, &GameState->HeroTorsoBitmap.Direction[EntityHigh->Face], Vec2(0.0, 0.0), Vec2(EntityPos.X-GameState->HeroPosInBitmap.X , EntityPos.Y - EntityHigh->Zr*WorldInfo->TileSideInMeters -GameState->HeroPosInBitmap.Y), 1.0);
				} break;
				case EntityType_Familiar: {
					UpdateFamiliar(GameState, i, GameTime->Elapsed);
					PushRenderPiece(&RenderPieceGroup, 
							&GameState->HeroHeadBitmap.Direction[EntityHigh->Face], 
							Vec2(0.0, 0.0), 
							Vec2(EntityPos.X-GameState->HeroPosInBitmap.X , 
								EntityPos.Y - EntityHigh->Zr*WorldInfo->TileSideInMeters -GameState->HeroPosInBitmap.Y), 
							1.0);
					// DrawRectangle(VideoBuffer, EntityMin, EntityMax, 1.0f, 1.0f, 1.0f);
				} break;
				default: {
					InvalidGamePath("some entities are not being drawn\n");
				}
			}

			// draw the pieces
			for (s32 PieceIndex=0; PieceIndex < RenderPieceGroup.Count; ++PieceIndex) {
				render_piece Piece = RenderPieceGroup.Pieces[PieceIndex];
				BlitBitmap(Piece.Bitmap, VideoBuffer, Piece.Offset, Piece.Opacity);
			}
		}
	}
}
