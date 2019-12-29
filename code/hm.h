#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

//
// NOTE: Compilers
//

#ifndef COMPILER_MSVC
#define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
#define COMPILER_LLVM 0
#endif

#ifndef COMPILER_GCC
#define COMPILER_GCC  0
#endif

#if !CCOMPILER_MSVC && !COMPILER_LLVM && !COMPILER_GCC
#if __GNUC__
#undef COMPILER_GCC  
#define COMPILER_GCC 1
#else
// TODO: we are assuming that if the
// compiler is not gcc then it is llvm
#undef COMPILER_LLVM 
#define COMPILER_LLVM 1 
#endif
#endif

//
// NOTE: Build options
//

#if HANDMADE_SLOW
#define Assert(Condition) if (!(Condition)) { *(int *)0 = 0;}
#else
#define Assert(Condition)
#endif

#define global_variable static
#define internal_function static
#define local_persist static

#define MAX_CONTROLLER         4
#define MAX_INPUT_DEVICE       5 // MAX_CONTROLLER + keyboard
#define CONTROLLER_DEAD_ZONE   7000
#define CONTROLLER_MAX_BUTTON  12
#define CONTROLLER_MAX_AXIS    2

// 
// NOTE: Constants
//

#define pi32 3.14159265
#define true 1
#define false 0

#define Kilo ((u64)1000)
#define Mega ((u64)1000*1000)
#define Giga ((u64)1000*1000*1000)

#define BKilo ((u64)1024)
#define BMega (1024*BKilo)
#define BGiga (1024*BMega)
#define BTera (1024*BGiga)

#define MAX_U32 (0xFFFFFFFF)
#define MIN_S32 (0x80000000)

//
// NOTE: Types
//

typedef int bool;
typedef float           r32;
typedef double          r64;
typedef uint64_t        u64;
typedef int64_t         s64;
typedef uint32_t        u32;
typedef int32_t         s32;
typedef uint16_t        u16;
typedef int16_t         s16;
typedef unsigned char    u8;
typedef signed char      s8;
typedef uint32_t      bit32;

// Min and Max
#define Min(A,B) ((A < B) ? (A) : (B))
#define Max(A,B) ((A > B) ? (A) : (B))

// Some Common Functions
internal_function s32 CycleMove(s32 Position, s32 Length, s32 Size)
{
    // This function works only when Length <= Size
    Assert(Position < Size);
    Assert(Length <= Size);

    s32 Remaining = Size - Position;
    if (Length < Remaining) {
        return Length + Position;
    }
    else {
        return Length - Remaining;
    }
}

internal_function inline s32 SafeTruncate64(u64 X)
{
    Assert(X <= MAX_U32);
    s32 Y = (s32)X;
    return Y;
}

// types
typedef struct {
    void *Beginning;
    s16 Width;
    s16 Height;
    s16 Pitch; // Pitch = Width * 4. each pixel is 4 bytes
} game_video_buffer;

typedef struct {
    u32 *Beginning;
    u16 Size; // number of samples to output, each sample is 4 bytes  
    u16 Frequency;
    u16 Channels;
    u16 FormatSize;
    u16 BytesPerSample;
} game_audio_buffer;

typedef struct {
    u16 Volume;
    u16 Position; 
    u16 Period;
    r32 (*Value)(r32);
} wave;

typedef struct {
    s16 HalfTransitionCount;
    bool EndedDown;
} game_button_state;

typedef struct {
    bool Analog;
    union {
        r32 Axis[CONTROLLER_MAX_AXIS];
        struct {
            r32 EndX;
            r32 EndY;
        };
    };
    union {
        game_button_state Buttons[CONTROLLER_MAX_BUTTON];
        struct {
            game_button_state ButtonA;
            game_button_state ButtonB;
            game_button_state ButtonX;
            game_button_state ButtonY;
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
            game_button_state Start;
            game_button_state Back;
        };
    };
} game_controller_state;

#define NUM_OF_MOUSE_BUTTONS 3
typedef struct {
    union {
        game_button_state MouseButtons[NUM_OF_MOUSE_BUTTONS];
        struct {
            game_button_state MouseButtonLeft; 
            game_button_state MouseButtonMiddle; 
            game_button_state MouseButtonRight; 
        };
    };

    s32 MouseX;
    s32 MouseY;
    s32 MouseZ;

    union{
        game_controller_state InputDevice[MAX_INPUT_DEVICE]; // 1 is the keyboard
        struct {
            game_controller_state Keyboard;
            game_controller_state Controller[MAX_CONTROLLER];
        };
    };
} game_input;

#if HANDMADE_INTERNAL
typedef struct {
    void *Beginning;
    s32 Size;
} debug_read_file_result;

typedef struct {
    int PlaceHolder;
} thread_context;

#define DEBUG_PLATFORM_READ_ENTIRE_FILE(name) debug_read_file_result name(thread_context *Thread, char *FileName)
typedef DEBUG_PLATFORM_READ_ENTIRE_FILE(debug_platform_read_entire_file);

#define DEBUG_PLATFORM_FREE_FILE_MEMORY(name) void name(thread_context *Thread, void *Memory)
typedef DEBUG_PLATFORM_FREE_FILE_MEMORY(debug_platform_free_file_memory);

#define DEBUG_PLATFORM_WRITE_ENTIRE_FILE(name) bool name(thread_context *Thread, char *FileName, void *Memory, s32 MemorySize)  
typedef DEBUG_PLATFORM_WRITE_ENTIRE_FILE(debug_platform_write_entire_file);

#endif

typedef struct {
    u64 TotalSize;
    u64 PermanentStorageSize;
    union {	
        void *Beginning;
        void *PermanentStorage;
    };
    u64 TransientStorageSize;
    void *TransientStorage;

    debug_platform_read_entire_file *DEBUGPlatformReadEntireFile;
    debug_platform_free_file_memory *DEBUGPlatformFreeFileMemory;
    debug_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
} game_memory;

#include "mat.h"

typedef struct {
    r32 Elapsed;
} game_time;

typedef struct {
    s32 Size;
    u8 *Base;
    s32 Used;
} memory_arena;

#define PushOnArena(Arena, Count, type) (type *)PushOnArena_(Arena, (Count)*sizeof(type))
internal_function void *
PushOnArena_(memory_arena *Arena, s32 Size)
{
    Assert(Arena->Used + Size <= Arena->Size);

    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return Result;
}

#pragma pack(push, 1)
typedef struct {
    u16 FileType;        /* File type, always 4D42h ("BM") */
    s32 FileSize;        /* Size of the file in bytes */
    u16 Reserved1;       /* Always 0 */
    u16 Reserved2;       /* Always 0 */
    s32 BitmapOffset;    /* Starting position of image data in bytes */
    s32 Size;            /* Size of this header in bytes */
    s32 Width;           /* Image width in pixels */
    s32 Height;          /* Image height in pixels */
    u16  Planes;          /* Number of color planes */
    u16  BitsPerPixel;    /* Number of bits per pixel */
    s32 Compression;     /* Compression methods used */
    s32 SizeOfBitmap;    /* Size of bitmap in bytes */
    s32  HorzResolution;  /* Horizontal resolution in pixels per meter */
    s32  VertResolution;  /* Vertical resolution in pixels per meter */
    s32 ColorsUsed;      /* Number of colors in the image */
    s32 ColorsImportant;
    // from here the bitmap header starts
    s32 RedMask;         /* Mask identifying bits of red component */
    s32 GreenMask;       /* Mask identifying bits of green component */
    s32 BlueMask;        /* Mask identifying bits of blue component */
} BMP_file_header;
#pragma pack(pop)

typedef struct {
    s32 Width;
    s32 Height;
    u32 *Beginning;
} loaded_bitmap;

typedef union {
    struct {
	loaded_bitmap Direction[4];
    };
    struct {
	loaded_bitmap Right;
	loaded_bitmap Back;
	loaded_bitmap Left;
	loaded_bitmap Front;
    };
} directional_bitmap_set;

typedef enum {
    EntityType_Player,
    EntityType_Wall,
    EntityType_Monster,
    EntityType_Familiar,
} entity_type;

typedef struct {
    vec2 P; // is in camera coordinate center of the screen is 0,0. unit is meter.
    s32 Z;
    vec2 dP;
    vec2 ddP;
    u8 Face; // player facing direction can be 0 to 3, they come from an enum

    r32 Zr; // for jump
    r32 dZ; // for jump

    s32 LowIndex;
} high_entity;

#include "wor.h"
typedef struct {
    entity_type Type;
    world_position P;
    vec2 DimInMeters;

    bool Collides;
    s32 dZ; // for "stairs"

    s32 HighIndex;
} low_entity;

typedef struct {
    low_entity *Low;
    high_entity *High;
} entity;

typedef struct {
    world_info WorldInfo;
    memory_arena TileArena;

    vec3_s32 CenterScreen;

    s32 PlayerIndexOfController[MAX_INPUT_DEVICE];

    s32 LowEntityCount; 
#define MAX_LOW_ENTITY 100000
    low_entity LowEntities[MAX_LOW_ENTITY]; 

    s32 HighEntityCount;
#define MAX_HIGH_ENTITIES 512
    high_entity HighEntities_[MAX_HIGH_ENTITIES]; 

    world_position CameraUpperLeft;
    s32 CameraFollowingPlayer;
    world_position HighRectCenter; // the world position of the >>center<< of the HighRect 
    vec3_s32 HighRectDimTiles;
    vec2 HighRectDim;

    world World;

    loaded_bitmap BackGroundBitMap;
    loaded_bitmap TreeBitmap;

    union {
        directional_bitmap_set HeroBitmap[3];
        struct {
            directional_bitmap_set HeroHeadBitmap;
            directional_bitmap_set HeroTorsoBitmap;
            directional_bitmap_set HeroLegsBitmap;
        };
    };
    loaded_bitmap HeroShadowBitmap;

    vec2_s32 HeroPosInBitmap; // position is based on the center of pixels he touches the ground
    vec2_s32 TreePosInBitmap; // position is based on the center of pixels he touches the ground
} game_state;

internal_function inline void 
ChangeEntityLocation(game_state *GameState, s32 LowIndex, world_position *NewP, memory_arena *Arena)
{
    world *World = &GameState->World;
    low_entity *EntityLow = GameState->LowEntities + LowIndex;
    world_position *OldP = &EntityLow->P;
    if (AreInSameChunk(OldP, NewP))
    {
        // leave the entity index where it is
    }
    else
    {
        // remove entity index from its old position
        {
            world_chunk *Chunk = GetWorldChunk(World, OldP->Chunk, (memory_arena *)0);
            Assert(Chunk);
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
                        if (Block->EntityIndex[Index] == LowIndex)
                        {
                            Block->EntityIndex[Index] = 
                                Chunk->First.EntityIndex[Chunk->First.Count-- - 1];

                            if (Chunk->First.Count == 0)
                            {
                                // remove the first block if there is another block
                                if (Chunk->First.NextBlock)
                                {
                                    entity_block *BlockToFree = Chunk->First.NextBlock;
                                    Chunk->First = *Chunk->First.NextBlock;

                                    BlockToFree->NextBlock = World->FirstFreeBlock;
                                    World->FirstFreeBlock = BlockToFree;
                                }
                            }

                            goto index_search_end;
                        }
                    }
                }

index_search_end:
                ;
            }
        }

        // insert entity index in its new location
        world_chunk *Chunk = GetWorldChunk(World, NewP->Chunk, Arena);
        entity_block *First = &Chunk->First;

        if (First->Count == ENTITY_BLOCK_SIZE)
        {
            entity_block *NewBlock;
            if (World->FirstFreeBlock)
            {
                NewBlock = World->FirstFreeBlock;
                World->FirstFreeBlock = World->FirstFreeBlock->NextBlock;
            }
            else
            {
                NewBlock = PushOnArena(Arena, 1, entity_block);
            }

            *NewBlock = *First;
            First->Count = 0;
            First->NextBlock = NewBlock;
        }

        First->EntityIndex[First->Count] = LowIndex;
        ++First->Count;
    }
    *OldP = *NewP;

}

/*
TODO: services that the platformlayer provides to the game
 */

/*
TODO: services that the game provides to the platform layer
(this may expand in the future, sound in a different thread etc)
 */

#define GAME_UPDATE_AND_RENDER(name) void name(thread_context *Thread, game_time *GameTime, game_memory *Memory, game_input *Input, game_video_buffer *VideoBuffer, game_audio_buffer *AudioBuffer)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_INITIALIZE_STATE(name) void name(thread_context *Thread, game_memory *Memory)
typedef GAME_INITIALIZE_STATE(game_initialize_state);

global_variable s32 MAX_DIST_U32 = 0x7fffffff;

void InvalidGamePath(char *String)
{
    printf("%s\n", String);
    Assert(1 == 0);
}
