// 
//
//

typedef struct {
    s32 TileSideInPixels;
    r32 TileSideInMeters;
    r32 MetersToPixels;

    int MapHeightInTiles;
    int MapWidthInTiles;
} world_info;

typedef struct {
    s32 Chunk;
    r32 Offset;
} extended_position;

inline r32
EPosDiff(extended_position W, extended_position V)
{
    r32 Result = (W.Chunk - V.Chunk) + (W.Offset - V.Offset);
}

#define CHUNKSHIFT 4
#define CHUNKMASK 0xf
#define CHUNKSIDE_IN_TILE 16
#define TILESIDE_IN_METER 1.4f
#define CHUNKSIDE_IN_METER (CHUNKSIDE_IN_TILE * TILESIDE_IN_METER)
typedef struct {
    vec3_s32 Chunk;
    vec2 Offset;
    // distance of a point from the center of the world(in meters) is 
    // Chunk * CHUNKSIDE_IN_METER + Offset
    // JUST FOR X AND Y
} world_position;

internal_function inline bool
AreInSameChunk(world_position *OldP, world_position *NewP)
{
    if (Equal(OldP->Chunk, NewP->Chunk))
    {
        return true;
    }

    return false;
}

internal_function inline vec2
WPosDiff(world_position W, world_position V)
{
    vec2 Result;

    Result.X = CHUNKSIDE_IN_METER * (W.Chunk.X - V.Chunk.X) + W.Offset.X - V.Offset.X;
    Result.Y = CHUNKSIDE_IN_METER * (W.Chunk.Y - V.Chunk.Y) + W.Offset.Y - V.Offset.Y;

    return Result;
}

typedef struct entity_block
{
    s32 Count;
#define ENTITY_BLOCK_SIZE 16
    s32 EntityIndex[ENTITY_BLOCK_SIZE];
    struct entity_block *NextBlock;
} entity_block;

typedef struct world_chunk{
    // the coordinates of the tile chunk itself
    s32 X;
    s32 Y;
    s32 Z;

    entity_block First;
    struct world_chunk *NextInHash;
} world_chunk;

typedef struct {
    s32 ChunkRadiusInMeters_;
    entity_block *FirstFreeBlock;
    
#define TILE_CHUNK_HASH_SIZE 4096  // this needs to be a power a 2 because of the way the hashslot works
    world_chunk TileChunkInHash[TILE_CHUNK_HASH_SIZE];
} world;

internal_function inline world_chunk *
GetWorldChunk(world *World, vec3_s32 ChunkIndex, memory_arena *Arena)
{
    s32 HashValue = 19*ChunkIndex.X + 7*ChunkIndex.Y + 3*ChunkIndex.Z;
    s32 HashSlot = HashValue & (TILE_CHUNK_HASH_SIZE - 1); 

    world_chunk *Chunk = World->TileChunkInHash + HashSlot;
    while (true) 
    {
        if ((Chunk->X == ChunkIndex.X) && (Chunk->Y == ChunkIndex.Y) && (Chunk->Z == ChunkIndex.Z)) 
        {
            return Chunk;
        }
        else if (Chunk->NextInHash != 0) 
        {
            Chunk = Chunk->NextInHash;
        }
        else if (Arena)
        {
            if (Chunk->X == MIN_S32)
            {
                    Chunk->X = ChunkIndex.X;
                    Chunk->Y = ChunkIndex.Y;
                    Chunk->Z = ChunkIndex.Z;
                    return Chunk;
            }
            else 
            {
                world_chunk *NewChunk = PushOnArena(Arena, 1, world_chunk);
                Chunk->NextInHash = NewChunk; 
                NewChunk->NextInHash = 0;

                NewChunk->X = ChunkIndex.X;
                NewChunk->Y = ChunkIndex.Y;
                NewChunk->Z = ChunkIndex.Z;
                return Chunk;
            }
        }
        else
        {
            return 0;
        }
    }
}

internal_function inline void 
AddEntityToChunk(world* World, s32 LowIndex, world_position *P, memory_arena *Arena)
{
    // insert entity index in its new location
    world_chunk *Chunk = GetWorldChunk(World, P->Chunk, Arena);
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

