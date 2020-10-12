#ifndef HASH_CCEH_H
#define HASH_CCEH_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define kCacheLineSize (64)

typedef size_t Key_t;
//typedef const char *Value_t;
typedef size_t Value_t;
typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

extern const Value_t NONE;

typedef struct Pair
{
    Key_t key;
    Value_t value;
} Pair;

#define kSegmentBits 8
#define kMask ((1 << kSegmentBits) - 1)
#define kShift kSegmentBits
#define kSegmentSize ((1 << kSegmentBits) * 16 * 16)
#define kNumPairPerCacheLine 4
#define kNumCacheLine 4
#define kNumSlot ((1 << kSegmentBits) * 16 * 16 / sizeof(Pair))
#define key_size (8 * sizeof(size_t))

typedef struct Segment
{
    Pair _[kNumSlot];
    size_t local_depth;
    //int64_t sema = 0;
    size_t pattern; //Segent对应的匹配
} Segment;

typedef struct Directory
{
    Segment **_;   
} Directory;

typedef struct HASH
{
    size_t global_depth;
    Directory *_;
} HASH;

void init(HASH *o_hash);
Value_t search_hash(HASH *dir, Key_t search_key);
int delete_hash(HASH *dir, Key_t search_key);
int insert_hash(HASH *dir, Key_t new_key, Value_t new_value);
void print(HASH *dir);

#define INVALID -1
#define NONE 0x0
#endif
