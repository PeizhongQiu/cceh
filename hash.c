#include "hash.h"
#include "memory_management.h"

u32 hash_32(u32 val, unsigned int bits)
{
    u32 hash = val * GOLDEN_RATIO_PRIME_32;
    return hash >> (32 - bits);
}

void print(HASH *dir)
{
    printf("%d\n", dir->global_depth);
    int i, j;
    for (i = 0; i < (1 << dir->global_depth); ++i)
    {
        printf("(%08d, %08p)", dir->_->_[i]->pattern, dir->_->_[i]);
    }
    printf("\n");
    for (j = 0; j < kNumSlot; ++j)
    {
        for (i = 0; i < (1 << dir->global_depth); ++i)
        {
            Segment *dir_ = dir->_->_[i];

            printf("(%08x, %08x)", dir_->_[j].key, hash_32(dir_->_[j].key, 32));
        }
        printf("\n");
    }
}

int insert_hash(HASH *dir, Key_t new_key, Value_t new_value)
{
    u32 key_hash = hash_32(new_key, 32);

    u32 x = (key_hash & ((1 << dir->global_depth) - 1));
    u32 y = (key_hash >> (key_size - kSegmentBits)) * kNumPairPerCacheLine * kNumCacheLine;
    //printf("key = %x, key_hash = %x, x = %x, y = %x global_depth = %d\n",new_key,key_hash,x,y,dir->global_depth);
    Segment *dir_ = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u32 slot = (y + i) % kNumSlot;
        //printf("slot = %x slot.key = %x\n",slot,dir_->_[slot].key);
        if (dir_->_[slot].key == INVALID ||
            (hash_32(dir_->_[slot].key, 32) & ((1 << dir_->local_depth) - 1)) != dir_->pattern)
        {
            dir_->_[slot].value = new_value;
            dir_->_[slot].key = new_key;
            pmem_persist(&dir_->_[slot], sizeof(Pair));
            mfence();
            return 1;
        }
    }

    if (dir_->local_depth == dir->global_depth)
    {
        //分配一个新的Segment
        printf("split 1 begin...\n");
        Segment *new_Segment = getNode(HASH_SEGMENT);
        memset(new_Segment, -1, sizeof(struct Segment));

        new_Segment->pattern = (1 << dir_->local_depth) + dir_->pattern;
        new_Segment->local_depth = dir_->local_depth + 1;
        printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);

        for (i = 0; i < kNumSlot; ++i)
        {
            if (dir_->_[i].key != INVALID)
            {
                u32 re_hash_key = hash_32(dir_->_[i].key, 32);
                u32 pattern = re_hash_key & ((1 << new_Segment->local_depth) - 1);
		        //printf("rehash_key = %x pattern = %x\n",re_hash_key, pattern);
                if (pattern == new_Segment->pattern)
                {
                    //printf("rehash_key = %x pattern = %x", re_hash_key, pattern);
                    u32 Segment_index = (re_hash_key >> (key_size - kSegmentBits)) * kNumPairPerCacheLine * kNumCacheLine;
                    unsigned j;
                    for (j = 0; j < kNumPairPerCacheLine * kNumCacheLine; ++j)
                    {
                        u32 slot = (Segment_index + j) % kNumSlot;
                        if (new_Segment->_[slot].key == INVALID)
                        {
                            new_Segment->_[slot].value = dir_->_[i].value;
                            new_Segment->_[slot].key = dir_->_[i].key;
                            break;
                        }
                    }
                }
            }
        }
        pmem_persist(new_Segment, sizeof(Segment));
        mfence();
        //printf("New Segment ok\n");

        //Directory翻倍
        for (i = (1 << (dir->global_depth)); i < (1 << (dir->global_depth + 1)); ++i)
        {
            dir->_->_[i] = dir->_->_[i - (1 << (dir->global_depth))];
        }
        dir->_->_[(1 << dir->global_depth) + x] = new_Segment;
        pmem_persist(dir->_, sizeof(Directory));
        mfence();
        //printf("New Dir ok\n");
        ++dir_->local_depth;
        pmem_persist(&dir_->local_depth, sizeof(size_t));
        mfence();
        ++dir->global_depth;
        pmem_persist(dir, sizeof(HASH));
        mfence();
        //printf("new_Segment %p\n", new_Segment);
		for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}
        //printf("insert again\n");
    }
    else
    {
        printf("split 2 begin...\n");
        Segment *new_Segment = getNode(HASH_SEGMENT);
        memset(new_Segment, -1, sizeof(struct Segment));
        new_Segment->pattern = (1 << dir_->local_depth) + dir_->pattern;
        new_Segment->local_depth = dir_->local_depth + 1;
        printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);

        for (i = 0; i < kNumSlot; ++i)
        {
            if (dir_->_[i].key != INVALID)
            {
                u32 re_hash_key = hash_32(dir_->_[i].key, 32);
                u32 pattern = re_hash_key & ((1 << new_Segment->local_depth) - 1);
                if (pattern == new_Segment->pattern)
                {
                    //printf("rehash_key = %x pattern = %x", re_hash_key, pattern);
                    u32 Segment_index = (re_hash_key >> (key_size - kSegmentBits)) * kNumPairPerCacheLine * kNumCacheLine;
                    unsigned j;
                    for (j = 0; j < kNumPairPerCacheLine * kNumCacheLine; ++j)
                    {
                        u32 slot = (Segment_index + j) % kNumSlot;
                        if (new_Segment->_[slot].key == INVALID)
                        {
                            new_Segment->_[slot].value = dir_->_[i].value;
                            new_Segment->_[slot].key = dir_->_[i].key;
                            break;
                        }
                    }
                }
            }
        }
        pmem_persist(new_Segment, sizeof(new_Segment));
        mfence();
        for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}

        unsigned int tail = new_Segment->pattern;
        while (tail < (1 << dir->global_depth))
        {
            //printf("%d\n", tail);
            dir->_->_[tail] = new_Segment;
            tail += 1 << (new_Segment->local_depth);
        }

        pmem_persist(dir->_, sizeof(Directory));
        mfence();
        ++dir_->local_depth;
        //dir_->pattern = dir_->pattern << 1;
        //printf("new_Segment %p\n", new_Segment);
		for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}
        //printf("insert_again\n");
        pmem_persist(dir_, sizeof(Segment));
        mfence();
    }
    insert_hash(dir, new_key, new_value);
    return 1;
}

int delete_hash(HASH *dir, Key_t search_key)
{
    u32 key_hash = hash_32(search_key, 32);
    u32 x = (key_hash & ((1 << dir->global_depth) - 1));
    u32 y = (key_hash >> (key_size - kSegmentBits)) * kNumPairPerCacheLine * kNumCacheLine;

    Segment *dir_ = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u32 slot = (y + i) % kNumSlot;
        if (dir_->_[slot].key == search_key)
        {
            dir_->_[slot].key = INVALID;
            pmem_persist(&dir_->_[slot].key, sizeof(Key_t));
            mfence();
            //            free(dir_->_[slot].value);
            return 1;
        }
    }
    return NONE;
}

Value_t search_hash(HASH *dir, Key_t search_key)
{
    u32 key_hash = hash_32(search_key, 32);
    u32 x = (key_hash & ((1 << dir->global_depth) - 1));
    u32 y = (key_hash >> (key_size - kSegmentBits)) * kNumPairPerCacheLine * kNumCacheLine;

    Segment *dir_ = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u32 slot = (y + i) % kNumSlot;
        if (dir_->_[slot].key == search_key)
        {
            return dir_->_[slot].value;
        }
    }
    return NONE;
}

// int update_hash(HASH *dir, Key_t search_key, Value_t new_value)
// {
//     u32 key_hash = hash_32(search_key, 32);
//     u32 x = (key_hash >> (key_size - dir->global_depth));
//     u32 y = (key_hash & kMask) * kNumPairPerCacheLine * kNumCacheLine;

//     Segment *dir_ = dir->_->_[x];
//     unsigned i;
//     for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
//     {
//         u32 slot = (y + i) % kNumSlot;
//         if (dir_->_[slot].key == search_key)
//         {
//             dir_->_[slot].value = new_value;
//             return 1;
//         }
//     }
//     return NONE;
// }

void init(HASH *o_hash)
{
    o_hash->global_depth = 1;
    o_hash->_ = getNode(HASH_DIRECTORY);
    Directory *init_dir = o_hash->_;
    init_dir->_[0] = getNode(HASH_SEGMENT);
    memset(init_dir->_[0], -1, sizeof(Segment));

    init_dir->_[0]->local_depth = 1;
    init_dir->_[0]->pattern = 0;

    init_dir->_[1] = getNode(HASH_SEGMENT);
    memset(init_dir->_[1], -1, sizeof(Segment));
    init_dir->_[1]->local_depth = 1;
    init_dir->_[1]->pattern = 1;
}
