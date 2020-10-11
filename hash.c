#include "hash.h"
#include "memory_management.h"

u64 hash_64(u64 val)
{
    u64 hash = val;

    /*  Sigh, gcc can't optimise this alone like it does for 32 bits. */
    u64 n = hash;
    n <<= 18;
    hash -= n;
    n <<= 33;
    hash -= n;
    n <<= 3;
    hash += n;
    n <<= 3;
    hash -= n;
    n <<= 4;
    hash += n;
    n <<= 2;
    hash += n;

    /* High bits are more random, so use them. */
    return hash;
}

void print(HASH *dir)
{
    printf("%d\n", dir->global_depth);
    int i, j;
    for (i = 0; i < (1 << dir->global_depth); ++i)
    {
        printf("(%016llx, %016p)", dir->_->_[i]->pattern, dir->_->_[i]);
    }
    printf("\n");
    for (j = 0; j < kNumSlot; ++j)
    {
        for (i = 0; i < (1 << dir->global_depth); ++i)
        {
            Segment *dir_ = dir->_->_[i];

            printf("(%016llx, %016llx)", dir_->_[j].key, hash_64(dir_->_[j].key));
        }
        printf("\n");
    }
}

int insert_hash(HASH *dir, Key_t new_key, Value_t new_value)
{
    Key_t key_hash = hash_64(new_key);

    Key_t x = (key_hash >> (key_size - dir->global_depth));
    u64 y = (key_hash & kMask) * kNumPairPerCacheLine * kNumCacheLine;
    printf("key = %016x, key_hash = %016llx, x = %016llx, y = %016llx global_depth = %d\n",new_key,key_hash,x,y,dir->global_depth);
    Segment *dir_ = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u64 slot = (y + i) % kNumSlot;
        //printf("slot = %x slot.key = %x\n",slot,dir_->_[slot].key);
        if (dir_->_[slot].key == INVALID ||
            (hash_64(dir_->_[slot].key) >> (key_size - dir_->local_depth)) != dir_->pattern)
        {
            dir_->_[slot].value = new_value;
            dir_->_[slot].key = new_key;
            pmem_persist(&dir_->_[slot], sizeof(Pair));
            mfence();
            print(dir);
            return 1;
        }
    }

    if (dir_->local_depth == dir->global_depth)
    {
        //分配一个新的Segment
        //printf("split 1 begin...\n");
        Segment *new_Segment = getNode(HASH_SEGMENT);
        memset(new_Segment, -1, sizeof(struct Segment));

        new_Segment->pattern = ((dir_->pattern) << 1) + 1;
        new_Segment->local_depth = dir_->local_depth + 1;
        //printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);

        for (i = 0; i < kNumSlot; ++i)
        {
            if (dir_->_[i].key != INVALID)
            {
                Key_t re_hash_key = hash_64(dir_->_[i].key);
                size_t pattern = re_hash_key >> (key_size - new_Segment->local_depth);
                //printf("rehash_key = %x pattern = %x\n",re_hash_key, pattern);
                if (pattern == new_Segment->pattern)
                {
                    //printf("rehash_key = %x pattern = %x", re_hash_key, pattern);
                    u64 Segment_index = (re_hash_key & kMask) * kNumPairPerCacheLine * kNumCacheLine;
                    unsigned j;
                    for (j = 0; j < kNumPairPerCacheLine * kNumCacheLine; ++j)
                    {
                        u64 slot = (Segment_index + j) % kNumSlot;
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
        Directory *new_dir = getNode(HASH_DIRECTORY);
        for (i = 0; i < (1 << (dir->global_depth)); ++i)
        {
            if (i == x)
            {
                new_dir->_[i * 2] = dir->_->_[i];
                new_dir->_[i * 2 + 1] = new_Segment;
            }
            else
            {
                new_dir->_[i * 2] = dir->_->_[i];
                new_dir->_[i * 2 + 1] = dir->_->_[i];
            }
        }
        dir->_ = new_dir;
        pmem_persist(dir->_, sizeof(Directory));
        mfence();
        //printf("New Dir ok\n");

        ++dir_->local_depth;
        pmem_persist(&dir_->local_depth, sizeof(size_t));
        mfence();
        dir_->pattern = dir_->pattern << 1;
        pmem_persist(&dir_->pattern, sizeof(size_t));
        mfence();
        ++dir->global_depth;
        pmem_persist(dir, sizeof(HASH));
        mfence();
        //printf("new_Segment %p\n", new_Segment);
        /*	for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        //printf("insert again\n");
    }
    else
    {
        //printf("split 2 begin...\n");
        /*for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        Segment *new_Segment = getNode(HASH_SEGMENT);
        /*for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        memset(new_Segment, -1, sizeof(struct Segment));
        new_Segment->pattern = ((dir_->pattern) << 1) + 1;
        /*for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        new_Segment->local_depth = dir_->local_depth + 1;
        //printf("new_Segment=%p, local_depth=%d, pattern=%d\n", new_Segment,new_Segment->local_depth,new_Segment->pattern);

        for (i = 0; i < kNumSlot; ++i)
        {
            if (dir_->_[i].key != INVALID)
            {
                Key_t re_hash_key = hash_64(dir_->_[i].key);
                size_t pattern = re_hash_key >> (key_size - new_Segment->local_depth);
                if (pattern == new_Segment->pattern)
                {
                    //printf("rehash_key = %x pattern = %x", re_hash_key, pattern);
                    u64 Segment_index = (re_hash_key & kMask) * kNumPairPerCacheLine * kNumCacheLine;
                    unsigned j;
                    for (j = 0; j < kNumPairPerCacheLine * kNumCacheLine; ++j)
                    {
                        u64 slot = (Segment_index + j) % kNumSlot;
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
        // for (i = 0; i < (1 << dir->global_depth); ++i) {
        // 	printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
        // }

        for (i = 0; i < (1 << (dir->global_depth - new_Segment->local_depth)); ++i)
        {
            dir->_->_[(new_Segment->pattern << (dir->global_depth - new_Segment->local_depth)) + i] = new_Segment;
        }

        pmem_persist(dir->_, sizeof(Directory));
        mfence();
        ++dir_->local_depth;
        //dir_->pattern = dir_->pattern << 1;
        //printf("new_Segment %p\n", new_Segment);
        /*	for (i = 0; i < (1 << dir->global_depth); ++i) {
			printf("%p %x\n", dir->_->_[i],dir->_->_[i]->pattern);
		}*/
        //printf("insert_again\n");
        pmem_persist(dir_, sizeof(Segment));
        mfence();
    }
    insert_hash(dir, new_key, new_value);
    return 1;
}

int delete_hash(HASH *dir, Key_t search_key)
{
    Key_t key_hash = hash_64(search_key);
    Key_t x = (key_hash >> (key_size - dir->global_depth));
    u64 y = (key_hash & kMask) * kNumPairPerCacheLine * kNumCacheLine;

    Segment *dir_ = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u64 slot = (y + i) % kNumSlot;
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
    Key_t key_hash = hash_64(search_key);
    Key_t x = (key_hash >> (key_size - dir->global_depth));
    u64 y = (key_hash & kMask) * kNumPairPerCacheLine * kNumCacheLine;

    Segment *dir_ = dir->_->_[x];
    unsigned i;
    for (i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i)
    {
        u64 slot = (y + i) % kNumSlot;
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
